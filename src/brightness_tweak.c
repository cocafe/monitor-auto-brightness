void *monitor_brightness_set_worker(void *arg)
{
        int *should_stop = arg;

        while (!*should_stop && !is_gonna_exit()) {
                for_each_monitor(i) {
                        struct monitor_info *m = &minfo[i];

                        if (!m->active)
                                continue;

                        if (!m->cap.support_brightness)
                                continue;

                        monitor_brightness_update(m);

                        if (m->brightness.curr == m->brightness.set)
                                continue;

                        if (monitor_brightness_set(m, m->brightness.set) == 0) {
                                monitor_brightness_update(m);

                                // if auto brightness is enabled
                                if (!is_auto_brightness_suspended())
                                        m->monitor_save->brightness.set = m->brightness.set;
                        }
                }

                usleep(g_config.brightness_update_interval_msec * 1000);
        }

        pthread_exit(NULL);

        return NULL;
}

void brightness_adjust_row_draw(struct nk_context *ctx,
                                size_t idx,
                                struct monitor_info *m)
{
        char str_monitor[64] = { };
        char property_name[16] = { };
        int min = (int)m->brightness.min;
        int max = (int)m->brightness.max;

        snprintf(str_monitor, sizeof(str_monitor), "%zu %ls", idx, m->str.name);
        snprintf(property_name, sizeof(property_name), "brightness%zu", idx);

        nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 1);

        nk_layout_row_push(ctx, 1.0f);
        nk_label(ctx, str_monitor, NK_TEXT_LEFT);

        nk_layout_row_end(ctx);


        nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);

        nk_layout_row_push(ctx, 0.85f);
        nk_slider_int(ctx, min, (int *)&m->brightness.set, max, 1);

        nk_layout_row_push(ctx, 0.15f);
        nk_property_int_nolabel(ctx, property_name, min, (int *)&m->brightness.set, max, 1, 1);

        nk_layout_row_end(ctx);
}

int brightness_adjust_wnd_draw(struct nkgdi_window *wnd, struct nk_context *ctx)
{
        if (is_gonna_exit() || READ_ONCE(monitor_info_update_required))
                return 0;

        for_each_monitor(i) {
                struct monitor_info *m = &minfo[i];

                if (!m->active)
                        continue;

                if (!m->cap.support_brightness)
                        continue;

                brightness_adjust_row_draw(ctx, i, m);
        }

        return 1;
}

void brightness_wnd_position_set(struct nkgdi_window *wnd)
{
        HWND hwnd = nkgdi_window_hwnd_get(wnd);
        HMONITOR monitor;
        MONITORINFO monitorInfo;
        POINT p = { };

        GetCursorPos(&p);

        int x = p.x, y = p.y;

        monitor = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);

        monitorInfo.cbSize = sizeof(MONITORINFO);
        if (!GetMonitorInfoW(monitor, &monitorInfo)) {
                pr_getlasterr("GetMonitorInfo");
                return;
        }

        if (x + wnd->_internal.width > monitorInfo.rcMonitor.right)
                x = monitorInfo.rcMonitor.right - wnd->_internal.width;

        y = y - wnd->_internal.height - 50;
        if (y < monitorInfo.rcMonitor.top)
                y = monitorInfo.rcMonitor.top + wnd->_internal.height;

        SetWindowPos(hwnd,HWND_TOPMOST,
                     x, y,0, 0,
                     SWP_NOSIZE | SWP_NOZORDER);
}

void brightness_adjust_wnd_create(void)
{
        static int running = 0;
        struct nkgdi_window wnd = { 0 };
        pthread_t worker_tid = 0;
        int should_stop = 0;
        int wnd_height = (int)(widget_h * 3.0f);
        int wnd_width = 500;

        if (__sync_bool_compare_and_swap(&running, 0, 1) == 0)
                return;

        monitors_use_count_inc();

        for_each_monitor(i) {
                struct monitor_info *m = &minfo[i];

                if (!m->active)
                        continue;

                if (!m->cap.support_brightness)
                        continue;

                if (g_config.auto_brightness)
                        m->brightness.set = m->brightness.curr;
                else
                        m->brightness.set = m->monitor_save->brightness.set;

                wnd_height += (int)(widget_h * 2.0f);
        }

        auto_brightness_suspend();

        if (pthread_create(&worker_tid, NULL, monitor_brightness_set_worker, &should_stop)) {
                pr_mb_err("Failed to create brightness control thread\n");
                goto out;
        }

        monitors_brightness_update();

        wnd.allow_sizing = 1;
        wnd.allow_maximize = 0;
        wnd.allow_move = 1;
        wnd.has_titlebar = 1;
        wnd.always_on_top = 1;
        wnd.close_on_focus_lose = 1;
        wnd.font_name = "ubuntu mono mod";
        wnd.cb_on_draw = brightness_adjust_wnd_draw;
        wnd.cb_on_close = NULL;

        nkgdi_window_create(&wnd, wnd_width, wnd_height, "Brightness Adjustment", 0, 0);
        nkgdi_window_icon_set(&wnd, LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APP_ICON)));
        nk_set_style(nkgdi_window_nkctx_get(&wnd), nk_theme);
        brightness_wnd_position_set(&wnd);

        nkgdi_window_blocking_update(&wnd);

        nkgdi_window_destroy(&wnd);

        should_stop = 1;
        pthread_join(worker_tid, NULL);

out:
        auto_brightness_resume();
        monitors_use_count_dec();
        running = 0;
}
