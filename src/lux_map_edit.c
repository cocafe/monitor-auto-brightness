struct luxmap_wnd_data {
        struct lux_map *luxmap_sel;
        struct lux_map *luxmap_new;
        char **strs_sel;
        int monitor_cnt;
        int sel;
};

int lux_mpa_sel_draw(struct nk_context *ctx, struct luxmap_wnd_data *data)
{
        int new_sel;

        nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 1);

        nk_layout_row_push(ctx, 1.0f);
        new_sel = nk_combo(ctx, (const char **)data->strs_sel, data->monitor_cnt, data->sel, widget_h, nk_vec2(nk_widget_width(ctx), 400));

        nk_layout_row_end(ctx);

        if (new_sel >= 0 && new_sel != data->sel) {
                data->sel = new_sel;
                return 1;
        }

        return 0;
}

void lux_map_val_draw(struct nk_context *ctx, struct luxmap_wnd_data *data)
{
        struct monitor_info *m = &minfo[data->sel];
        float lux = lux_get();

        nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 2);

        {
                char buf[128] = { };

                snprintf(buf, sizeof(buf), "current lux: %.1f", lux);

                nk_layout_row_push(ctx, 0.5f);

                nk_label(ctx, buf, NK_TEXT_LEFT);
        }

        {
                char buf[128] = { };
                int bl = monitor_brightness_compute(m, lux);

                if (bl >= 0)
                        snprintf(buf, sizeof(buf), "target brightness: %d", bl);
                else
                        snprintf(buf, sizeof(buf), "invalid brightness result %d", bl);

                nk_layout_row_push(ctx, 0.5f);

                nk_label(ctx, buf, NK_TEXT_LEFT);
        }

        nk_layout_row_end(ctx);
}

void lux_map_val_test_draw(struct nk_context *ctx, struct luxmap_wnd_data *data)
{
        struct monitor_info *m = &minfo[data->sel];
        static float lux = 0;

        nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 4);

        {
                static char buf[64] = { };
                int len = strlen(buf);
                int state;

                nk_layout_row_push(ctx, 0.10f);
                nk_label(ctx, "lux: ", NK_TEXT_LEFT);

                nk_layout_row_push(ctx, 0.20f);
                state = nk_edit_string(ctx, NK_EDIT_FIELD, buf, &len, sizeof(buf), nk_filter_float);

                nk_layout_row_push(ctx, 0.20f);
                nk_label(ctx, "", NK_TEXT_LEFT);

                buf[sizeof(buf) - 1] = '\0';
                if (len >= 0 && (size_t)len < sizeof(buf))
                        buf[len] = '\0';

                if (state & NK_EDIT_INACTIVE)
                        snprintf(buf, sizeof(buf), "%.0f", lux);

                if (sscanf(buf, "%f", &lux) != 1)
                        lux = 0;
        }

        {
                char buf[128] = { };
                int bl = monitor_brightness_compute(m, lux);

                if (bl >= 0)
                        snprintf(buf, sizeof(buf), "computed brightness: %d", bl);
                else
                        snprintf(buf, sizeof(buf), "invalid brightness: %d", bl);

                nk_layout_row_push(ctx, 0.5f);
                nk_label(ctx, buf, NK_TEXT_LEFT);
        }

        nk_layout_row_end(ctx);
}

void lux_map_chart_draw(struct nk_context *ctx, struct luxmap_wnd_data *data)
{
        struct monitor_info *mon = &minfo[data->sel];
        struct lux_map *hovered = NULL, *selected = NULL;
        struct list_head *pos;
        int cnt;

        if (list_empty(&mon->monitor_save->lux_map))
                cnt = 0;

        list_for_each(pos, &mon->monitor_save->lux_map) {
                cnt++;
        }

        nk_layout_row_dynamic(ctx, widget_h * 10, 1);

        if (nk_chart_begin(ctx, NK_CHART_COLUMN, cnt, mon->brightness.min, mon->brightness.max)) {
                nk_chart_add_slot(ctx, NK_CHART_LINES, cnt, mon->brightness.min, mon->brightness.max);

                list_for_each(pos, &mon->monitor_save->lux_map) {
                        struct lux_map *d = container_of(pos, struct lux_map, node);
                        nk_flags res;

                        res = nk_chart_push_slot(ctx, d->bl, 0);
                        res |= nk_chart_push_slot(ctx, d->bl, 1);

                        if (res & NK_CHART_HOVERING)
                                hovered = d;
                        if (res & NK_CHART_CLICKED)
                                selected = d;
                }
        }

        nk_chart_end(ctx);

        if (hovered)
                nk_tooltipf(ctx, "lux: %u bl: %u", hovered->lux, hovered->bl);

        if (selected) {
                data->luxmap_sel = selected;
        }
}

void lux_map_edit_draw(struct nk_context *ctx, struct luxmap_wnd_data *data)
{
        struct monitor_info *mon = &minfo[data->sel];
        struct list_head *head = &mon->monitor_save->lux_map;
        struct lux_map *map = data->luxmap_sel;
        struct lux_map *new_map = data->luxmap_new;

        nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 5);

        nk_layout_row_push(ctx, 0.1f);
        if (new_map) {
                nk_button_label_disabled(ctx, "+", 1);
        } else if (nk_button_label(ctx, "+")) {
                new_map = calloc(1, sizeof(struct lux_map));
                INIT_LIST_HEAD(&new_map->node);
                data->luxmap_new = new_map;
        }

        if (new_map)
                map = new_map;

        nk_layout_row_push(ctx, 0.1f);
        if (map != data->luxmap_sel) {
                nk_button_label_disabled(ctx, "-", 1);
        } else if (nk_button_label(ctx, "-") && map) {
                list_del(&map->node);
                free(map);
                map = NULL;
                data->luxmap_sel = NULL;
        }

        nk_layout_row_push(ctx, 0.1f);
        if (map != data->luxmap_sel) {
                nk_button_label_disabled(ctx, "<", 1);
        } else if (nk_button_label(ctx, "<")) {
                do {
                        if (list_empty(head)) {
                                break;
                        }

                        if (!map) {
                                map = container_of(head->prev, struct lux_map, node);
                        } else {
                                if (map->node.prev == head)
                                        map = container_of(head->prev, struct lux_map, node);
                                else
                                        map = list_prev_entry(map, node);
                        }

                        data->luxmap_sel = map;
                } while (0);
        }

        nk_layout_row_push(ctx, 0.1f);
        if (map != data->luxmap_sel) {
                nk_button_label_disabled(ctx, ">", 1);
        } else if (nk_button_label(ctx, ">")) {
                do {
                        if (list_empty(head)) {
                                break;
                        }

                        if (!map) {
                                map = container_of(head->next, struct lux_map, node);
                        } else {
                                if (map->node.next == head)
                                        map = container_of(head->next, struct lux_map, node);
                                else
                                        map = list_next_entry(map, node);
                        }

                        data->luxmap_sel = map;
                } while (0);
        }

        nk_layout_row_end(ctx);

        if (!map) {
                return;
        }

        nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 3);

        nk_layout_row_push(ctx, 0.10f);
        nk_label(ctx, "lux:", NK_TEXT_LEFT);

        nk_layout_row_push(ctx, 0.70f);
        nk_slider_int(ctx, (int)g_config.lux_range.min, (int *)&map->lux, (int)g_config.lux_range.max, 1);

        nk_layout_row_push(ctx, 0.20f);
        nk_property_int_nolabel(ctx, "#prop_lux", (int)g_config.lux_range.min, (int *)&map->lux, (int)g_config.lux_range.max, 1, 1);

        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 3);

        nk_layout_row_push(ctx, 0.10f);
        nk_label(ctx, "bl:", NK_TEXT_LEFT);

        nk_layout_row_push(ctx, 0.70f);
        nk_slider_int(ctx, mon->brightness.min, (int *)&map->bl, mon->brightness.max, 1);

        nk_layout_row_push(ctx, 0.20f);
        nk_property_int_nolabel(ctx, "#prop_bl", mon->brightness.min, (int *)&map->bl, mon->brightness.max, 1, 1);

        nk_layout_row_end(ctx);

        if (data->luxmap_new) {
                nk_layout_row_begin(ctx, NK_DYNAMIC, widget_h, 3);

                nk_layout_row_push(ctx, 0.80f);
                nk_label(ctx, "", NK_TEXT_LEFT);

                nk_layout_row_push(ctx, 0.10f);
                if (nk_button_label(ctx, "Add")) {
                        list_add(&data->luxmap_new->node, head);
                        data->luxmap_new = NULL;
                        data->luxmap_sel = NULL;
                }

                nk_layout_row_push(ctx, 0.10f);
                if (nk_button_label(ctx, "Cancel")) {
                        free(data->luxmap_new);
                        data->luxmap_new = NULL;
                        data->luxmap_sel = NULL;
                }

                nk_layout_row_end(ctx);
        }
}

int lux_map_wnd_draw(struct nkgdi_window *wnd, struct nk_context *ctx)
{
        struct luxmap_wnd_data *data = nkgdi_window_userdata_get(wnd);

        if (is_gonna_exit())
                return 0;

        if (lux_mpa_sel_draw(ctx, data)) {
                data->luxmap_sel = NULL;
                return 1;
        }

        lux_map_val_draw(ctx, data);
        lux_map_val_test_draw(ctx, data);
        lux_map_chart_draw(ctx, data);
        lux_map_edit_draw(ctx, data);

        for_each_monitor(i) {
                struct monitor_info *m = &minfo[i];
                struct monitor_cfg *cfg = m->monitor_save;

                if (!m->active)
                        continue;

                lux_map_fix(cfg);
        }

        return 1;
}

void lux_map_wnd_free(struct luxmap_wnd_data *data)
{
        if (data->strs_sel) {
                for (int i = 0; i < data->monitor_cnt; i++)
                        if (data->strs_sel[i])
                                free(data->strs_sel[i]);

                free(data->strs_sel);
        }

        if (data->luxmap_new)
                free(data->luxmap_new);
}

int lux_map_wnd_prepare(struct luxmap_wnd_data *data)
{
        memset(data, 0, sizeof(struct luxmap_wnd_data));

        data->sel = -1;

        for_each_monitor(i) {
                struct monitor_info *m = &minfo[i];

                if (!m->active)
                        continue;

                if (data->sel < 0)
                        data->sel = (int)i;

                data->monitor_cnt++;
        }

        if (data->sel < 0) {
                pr_mb_err("No active monitor");
                return -ENODEV;
        }

        data->strs_sel = calloc(data->monitor_cnt, sizeof(char *));
        if (!data->strs_sel)
                return -ENOMEM;

        for_each_monitor(i) {
                struct monitor_info *m = &minfo[i];
                char name[200] = { };
                char *buf;

                if (!m->active)
                        continue;

                buf = calloc(256, sizeof(char));
                if (!buf)
                        goto out_free;

                iconv_wc2utf8(m->str.name, sizeof(m->str.name), name, sizeof(name));
                snprintf(buf, 256, "%zu %s", i, name);

                data->strs_sel[i] = buf;
        }

        return 0;

out_free:
        lux_map_wnd_free(data);

        return -ENOMEM;
}

void lux_map_wnd_create(void)
{
        static int running = 0;
        struct nkgdi_window wnd = { };
        struct luxmap_wnd_data wnd_data = { };
        int wnd_height = (int)(widget_h * 20.0f);
        int wnd_width = 600;
        int err;

        if (__sync_bool_compare_and_swap(&running, 0, 1) == 0)
                return;

        // TODO: grab monitor lock
        // TODO: check monitor needs update flag

        if ((err = lux_map_wnd_prepare(&wnd_data))) {
                pr_mb_err("lux_map_wnd_prepare(): %s", strerror(err));
                goto out;
        }

        if ((err = usrcfg_save_trylock())) {
                pr_mb_err("failed to grab save lock: %s", strerror(err));
                goto out_free;
        }

        auto_brightness_suspend();

        wnd.allow_sizing = 1;
        wnd.allow_maximize = 1;
        wnd.allow_move = 1;
        wnd.has_titlebar = 1;
        wnd.always_on_top = 0;
        wnd.close_on_focus_lose = 0;
        wnd.font_name = "ubuntu mono mod";
        wnd.cb_on_draw = lux_map_wnd_draw;
        wnd.cb_on_close = NULL;

        nkgdi_window_create(&wnd, wnd_width, wnd_height, "Lux Map", 0, 0);
        nkgdi_window_icon_set(&wnd, LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APP_ICON)));
        nkgdi_window_userdata_set(&wnd, &wnd_data);
        nkgdi_window_set_center(&wnd);
        nk_set_style(nkgdi_window_nkctx_get(&wnd), nk_theme);

        nkgdi_window_blocking_update(&wnd);

        nkgdi_window_destroy(&wnd);

        auto_brightness_resume();

        usrcfg_save_unlock();

        auto_brightness_tray_update();

out_free:
        lux_map_wnd_free(&wnd_data);

out:
        running = 0;
}

void *lux_map_wnd_thread_worker(void *arg)
{
        lux_map_wnd_create();
        pthread_exit(NULL);
        return NULL;
}

void lux_map_wnd_thread_create(void)
{
        pthread_t tid;
        pthread_create(&tid, NULL, lux_map_wnd_thread_worker, NULL);
}