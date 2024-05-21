static const char *nk_theme_strs[] = {
        [THEME_DEFAULT] = "Nuklear Default",
        [THEME_BLACK] = "Black",
        [THEME_WHITE] = "White",
        [THEME_RED] = "Red",
        [THEME_BLUE] = "Blue",
        [THEME_GREEN] = "Green",
        [THEME_PURPLE] = "Purple",
        [THEME_BROWN] = "Brown",
        [THEME_DRACULA] = "Dracula",
        [THEME_DARK] = "Dark",
        [THEME_GRUVBOX] = "Gruvbox",
        [THEME_SOLARIZED_LIGHT] = "Solarized Light",
        [THEME_SOLARIZED_DARK] = "Solarized Dark",
};

int settings_wnd_draw(struct nkgdi_window *wnd, struct nk_context *ctx)
{
        int *last_auto_brightness = nkgdi_window_userdata_get(wnd);

        if (is_gonna_exit())
                return 0;

        {
                nk_layout_row_dynamic(ctx, widget_h, 1);
                nk_label(ctx, "General", NK_TEXT_LEFT);
        }

        {
                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_widget_tooltip(ctx, "Some monitors support brightness control but failed to detect");
                nk_label(ctx, "Force brightness control", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                nk_checkbox_label(ctx, "", (int *)&g_config.force_brightness_control);
                nk_layout_row_end(ctx);
        }

        {
                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_widget_tooltip(ctx, "Enable auto brightness control");
                nk_label(ctx, "Auto brightness", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                nk_checkbox_label(ctx, "", last_auto_brightness);
                nk_layout_row_end(ctx);
        }

        {
                static char buf[64] = { };
                int len = strlen(buf);
                int state;
                uint32_t t;

                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_label(ctx, "Auto brightness update interval", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                nk_widget_tooltip(ctx, "In seconds");
                state = nk_edit_string(ctx, NK_EDIT_FIELD, buf, &len, sizeof(buf), nk_filter_decimal);
                nk_layout_row_end(ctx);

                buf[sizeof(buf) - 1] = '\0';
                if (len >= 0 && (size_t)len < sizeof(buf))
                        buf[len] = '\0';

                if ((state & NK_EDIT_DEACTIVATED) && (sscanf(buf, "%u", &t) == 1))
                        g_config.auto_brightness_update_interval_sec = t;

                if (state & NK_EDIT_INACTIVE)
                        snprintf(buf, sizeof(buf), "%u", g_config.auto_brightness_update_interval_sec);
        }

        {
                static char buf[64] = { };
                int len = strlen(buf);
                int state;
                uint32_t t;

                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_label(ctx, "Brightness update interval in manual", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                nk_widget_tooltip(ctx, "In milliseconds");
                state = nk_edit_string(ctx, NK_EDIT_FIELD, buf, &len, sizeof(buf), nk_filter_decimal);
                nk_layout_row_end(ctx);

                buf[sizeof(buf) - 1] = '\0';
                if (len >= 0 && (size_t)len < sizeof(buf))
                        buf[len] = '\0';

                if ((state & NK_EDIT_DEACTIVATED) && (sscanf(buf, "%u", &t) == 1))
                        g_config.brightness_update_interval_msec = t;

                if (state & NK_EDIT_INACTIVE)
                        snprintf(buf, sizeof(buf), "%u", g_config.brightness_update_interval_msec);
        }

        {
                nk_layout_row_dynamic(ctx, widget_h, 1);
                nk_label(ctx, "", NK_TEXT_LEFT);
        }

        {
                nk_layout_row_dynamic(ctx, widget_h, 1);
                nk_label(ctx, "Look", NK_TEXT_LEFT);
        }

        {
                int t;

                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_label(ctx, "Theme", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                t = nk_combo(ctx, nk_theme_strs, NUM_NK_THEMES, nk_theme, widget_h, nk_vec2(nk_widget_width(ctx), 400));

                if (t != nk_theme) {
                        nk_theme = t;
                        nk_set_style(ctx, nk_theme);
                }
        }

        {
                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_label(ctx, "Widget height", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                // nk_slider_float(ctx, 10.0f, &widget_h, 100.0f, 0.5f);
                nk_property_float_nolabel(ctx, "#widget_height", 10.0f, &widget_h, 100.0f, 0.5f, 0.5f);
                nk_layout_row_end(ctx);
        }

        {
                nk_layout_row_dynamic(ctx, widget_h, 1);
                nk_label(ctx, "", NK_TEXT_LEFT);
        }

        {
                nk_layout_row_dynamic(ctx, widget_h, 1);
                nk_label(ctx, "Lux Range", NK_TEXT_LEFT);
        }

        {
                static char buf[64] = { };
                int len = strlen(buf);
                int state;
                uint32_t t;

                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_label(ctx, "Min", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                state = nk_edit_string(ctx, NK_EDIT_FIELD, buf, &len, sizeof(buf), nk_filter_decimal);
                nk_layout_row_end(ctx);

                buf[sizeof(buf) - 1] = '\0';
                if (len >= 0 && (size_t)len < sizeof(buf))
                        buf[len] = '\0';

                if ((state & NK_EDIT_DEACTIVATED) && (sscanf(buf, "%u", &t) == 1))
                        g_config.lux_range.min = t;

                if (state & NK_EDIT_INACTIVE)
                        snprintf(buf, sizeof(buf), "%u", g_config.lux_range.min);
        }

        {
                static char buf[64] = { };
                int len = strlen(buf);
                int state;
                uint32_t t;

                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_label(ctx, "Max", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                state = nk_edit_string(ctx, NK_EDIT_FIELD, buf, &len, sizeof(buf), nk_filter_decimal);
                nk_layout_row_end(ctx);

                buf[sizeof(buf) - 1] = '\0';
                if (len >= 0 && (size_t)len < sizeof(buf))
                        buf[len] = '\0';

                if ((state & NK_EDIT_DEACTIVATED) && (sscanf(buf, "%u", &t) == 1))
                        g_config.lux_range.max = t;

                if (state & NK_EDIT_INACTIVE)
                        snprintf(buf, sizeof(buf), "%u", g_config.lux_range.max);
        }

        {
                nk_layout_row_dynamic(ctx, widget_h, 1);
                nk_label(ctx, "", NK_TEXT_LEFT);
        }

        {
                nk_layout_row_dynamic(ctx, widget_h, 1);
                nk_label(ctx, "Sensor Hub", NK_TEXT_LEFT);
        }

        {
                static char buf[256] = { };
                int len = strlen(buf);
                int state;
                uint32_t t;

                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_label(ctx, "Host", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                state = nk_edit_string(ctx, NK_EDIT_FIELD, buf, &len, sizeof(buf), nk_filter_ascii);
                nk_layout_row_end(ctx);

                buf[sizeof(buf) - 1] = '\0';
                if (len >= 0 && (size_t)len < sizeof(buf))
                        buf[len] = '\0';

                if ((state & NK_EDIT_DEACTIVATED) && (sscanf(buf, "%u", &t) == 1)) {
                        strncpy(g_config.sensor_hub.host, buf, sizeof(g_config.sensor_hub.host));
                }

                if (state & NK_EDIT_INACTIVE)
                        strncpy(buf, g_config.sensor_hub.host, sizeof(buf));
        }

        {
                static char buf[64] = { };
                int len = strlen(buf);
                int state;
                uint32_t t;

                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_label(ctx, "Port", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                state = nk_edit_string(ctx, NK_EDIT_FIELD, buf, &len, sizeof(buf), nk_filter_decimal);
                nk_layout_row_end(ctx);

                buf[sizeof(buf) - 1] = '\0';
                if (len >= 0 && (size_t)len < sizeof(buf))
                        buf[len] = '\0';

                if ((state & NK_EDIT_DEACTIVATED) && (sscanf(buf, "%u", &t) == 1))
                        g_config.sensor_hub.port = t;

                if (state & NK_EDIT_INACTIVE)
                        snprintf(buf, sizeof(buf), "%u", g_config.sensor_hub.port);
        }

        {
                static char buf[64] = { };
                int len = strlen(buf);
                int state;
                uint32_t t;

                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_label(ctx, "Query interval", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                nk_widget_tooltip(ctx, "In seconds");
                state = nk_edit_string(ctx, NK_EDIT_FIELD, buf, &len, sizeof(buf), nk_filter_decimal);
                nk_layout_row_end(ctx);

                buf[sizeof(buf) - 1] = '\0';
                if (len >= 0 && (size_t)len < sizeof(buf))
                        buf[len] = '\0';

                if ((state & NK_EDIT_DEACTIVATED) && (sscanf(buf, "%u", &t) == 1))
                        g_config.sensor_hub.query_interval_sec = t;

                if (state & NK_EDIT_INACTIVE)
                        snprintf(buf, sizeof(buf), "%u", g_config.sensor_hub.query_interval_sec);
        }

        {
                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_label(ctx, " ", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                if (nk_button_label(ctx, "Test")) {
                        ReleaseSemaphore(sem_sensor_wake, 1, 0);
                }
                nk_layout_row_end(ctx);
        }

        {
                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_label(ctx, "Connection state", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                nk_label(ctx, is_sensorhub_connected() ? "Connected" : "Unavailable", NK_TEXT_LEFT);
                nk_layout_row_end(ctx);
        }

        {
                static char buf[32] = { };

                snprintf(buf, sizeof(buf), "%.1f", lux_get());

                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.6f);
                nk_label(ctx, "Current lux", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.4f);
                nk_label(ctx, buf, NK_TEXT_LEFT);
                nk_layout_row_end(ctx);
        }

        {
                nk_layout_row_dynamic(ctx, widget_h, 1);
                nk_label(ctx, "", NK_TEXT_LEFT);
        }

        {
                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);
                nk_layout_row_push(ctx, 0.8f);
                nk_label(ctx, "", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.2f);
                if (nk_button_label(ctx, "OK"))
                        return 0;
                nk_layout_row_end(ctx);
        }

        return 1;
}

void settings_wnd_create(void)
{
        static int running = 0;
        struct nkgdi_window wnd = { };
        int wnd_height = (int)(widget_h * 30.0f);
        int wnd_width = 700;
        int err;

        if (__sync_bool_compare_and_swap(&running, 0, 1) == 0)
                return;

        if ((err = usrcfg_save_trylock())) {
                pr_mb_err("failed to grab save lock: %s", strerror(err));
                goto out;
        }

        auto_brightness_suspend();

        wnd.allow_sizing = 1;
        wnd.allow_maximize = 1;
        wnd.allow_move = 1;
        wnd.has_titlebar = 1;
        wnd.always_on_top = 0;
        wnd.close_on_focus_lose = 0;
        wnd.font_name = "ubuntu mono mod";
        wnd.cb_on_draw = settings_wnd_draw;
        wnd.cb_on_close = NULL;

        nkgdi_window_create(&wnd, wnd_width, wnd_height, "Settings", 0, 0);
        nkgdi_window_icon_set(&wnd, LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APP_ICON)));
        nkgdi_window_userdata_set(&wnd, &last_auto_brightness);
        nkgdi_window_set_center(&wnd);
        nk_set_style(nkgdi_window_nkctx_get(&wnd), nk_theme);

        nkgdi_window_blocking_update(&wnd);

        nkgdi_window_destroy(&wnd);

        auto_brightness_resume();

        usrcfg_save_unlock();

        auto_brightness_tray_update();

out:
        running = 0;
}

void *settings_wnd_thread_worker(void *arg)
{
        settings_wnd_create();
        pthread_exit(NULL);
        return NULL;
}

void settings_wnd_thread_create(void)
{
        pthread_t tid;
        pthread_create(&tid, NULL, settings_wnd_thread_worker, NULL);
}