#include <windows.h>
#include <pthread.h>

#include <libjj/utils.h>
#include <libjj/jkey.h>
#include <libjj/opts.h>
#include <libjj/logging.h>
#include <libjj/list_sort.h>

#include "gui.h"
#include "monitor.h"
#include "usrcfg.h"

#define DEFAULT_JSON_PATH                       "config.json"

struct config g_config = {
        .json_path = DEFAULT_JSON_PATH,
        .auto_brightness = 1,
        .auto_brightness_update_interval_sec = 10,
        .brightness_update_interval_msec = 1,
        .force_brightness_control = 0,
        .sensor_hub = {
                .host = "192.168.77.123",
                .port = 5959,
                .query_interval_sec = 10,
        },
        .lux_range = {
                .min = 0,
                .max = 1000,
        },
        .monitor_list = LIST_HEAD_INIT(g_config.monitor_list),
};

lsopt_strbuf(c, json_path, g_config.json_path, sizeof(g_config.json_path), "JSON config path");

static jbuf_t jbuf_usrcfg;
static pthread_mutex_t lck_save = PTHREAD_MUTEX_INITIALIZER;

size_t lux_map_count(struct list_head *head)
{
        struct list_head *p;
        size_t c = 0;

        if (!head)
                return 0;

        if (list_empty(head))
                return 0;

        list_for_each(p, head) {
                c++;
        }

        return c;
}

int lux_map_cmp(void *arg, const struct list_head *_a, const struct list_head *_b)
{
        struct lux_map *a = container_of(_a, struct lux_map, node);
        struct lux_map *b = container_of(_b, struct lux_map, node);

        if (a->lux < b->lux)
                return -1;

        if (a->lux > b->lux)
                return 1;

        return 0;
}

void lux_map_sort(struct list_head *head)
{
        if (list_empty(head))
                return;

        list_sort(NULL, head, lux_map_cmp);
}

void lux_map_deduplicate(struct list_head *head)
{
        struct list_head *pos, *n;

        if (list_empty(head))
                return;

        for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next) {
                struct lux_map *curr = container_of(pos, struct lux_map, node);
                struct lux_map *next = container_of(n, struct lux_map, node);

                if (curr->lux != next->lux)
                        continue;

                list_del(&next->node);
                free(next);
                n = pos;
        }
}

void lux_map_invalid_remove(struct monitor_cfg *cfg, struct list_head *head)
{
        struct lux_map *curr, *next;
        uint32_t min = cfg->brightness.min;
        uint32_t max = cfg->brightness.max;

        list_for_each_entry_safe(curr, next, head, node) {
                if (curr->bl < min || curr->bl > max) {
                        list_del(&curr->node);
                        free(curr);
                }
        }
}

void lux_map_fix(struct monitor_cfg *cfg)
{
        struct list_head *head = &cfg->lux_map;

        lux_map_invalid_remove(cfg, head);
        lux_map_sort(head);
        lux_map_deduplicate(head);
}

struct monitor_cfg *monitor_cfg_alloc_init(void)
{
        struct monitor_cfg *c = calloc(1, sizeof(struct monitor_cfg));
        if (!c)
                return NULL;

        INIT_LIST_HEAD(&c->node);
        INIT_LIST_HEAD(&c->lux_map);

        return c;
}

void monitor_cfg_merge(struct monitor_cfg *c, struct monitor_info *m)
{
        c->brightness.min = m->brightness.min;
        c->brightness.max = m->brightness.max;
        c->brightness.set = m->brightness.set;
        __wstr_ncpy(c->name, m->str.name, WCBUF_LEN(c->name));
        __wstr_ncpy(c->dev_path, m->str.reg_path, WCBUF_LEN(c->dev_path));
}

void monitor_cfg_add(struct monitor_cfg *c)
{
        list_add(&c->node, &g_config.monitor_list);
}

extern int usrcfg_monitor_info_merge(void)
{
        for_each_monitor(i) {
                struct monitor_info *m = &minfo[i];
                struct list_head *p;
                int found = 0;

                if (!m->active)
                        continue;

                if (m->monitor_save)
                        continue;

                list_for_each(p, &g_config.monitor_list) {
                        struct monitor_cfg *c = container_of(p, struct monitor_cfg, node);

                        if (is_wstr_equal(c->dev_path, m->str.reg_path)) {
                                m->monitor_save = c;
                                found = 1;

                                break;
                        }
                }

                if (!found) {
                        struct monitor_cfg *c = monitor_cfg_alloc_init();

                        monitor_cfg_merge(c, m);
                        monitor_cfg_add(c);

                        pr_info("monitor \"%ls\" is not in config file\n", m->str.name);

                        m->monitor_save = c;
                }
        }

        return 0;
}

int usrcfg_save_lock(void)
{
        return pthread_mutex_lock(&lck_save);
}

int usrcfg_save_trylock(void)
{
        return pthread_mutex_trylock(&lck_save);
}

int usrcfg_save_unlock(void)
{
        return pthread_mutex_unlock(&lck_save);
}

int usrcfg_save(void)
{
        char *path = g_config.json_path;
        int err = 0;

        if ((err = pthread_mutex_trylock(&lck_save))) {
                if (err == EBUSY)
                        pr_mb_warn("Can't save config now\n");
                else
                        pr_err("failed to grab save_lck, err = %d %s\n", err, strerror(err));

                return err;
        }

        for_each_monitor(i) {
                struct monitor_info *m = &minfo[i];
                struct monitor_cfg *c = m->monitor_save;

                if (!m->active)
                        continue;

                if (!m->monitor_save)
                        continue;

                monitor_cfg_merge(c, m);
        }

        if ((err = jbuf_save(&jbuf_usrcfg, path))) {
                mb_err("failed to save config to \"%s\", err: %d", path, err);
                goto unlock;
        }

        pr_dbg("saved config to \"%s\"\n", path);

unlock:
        pthread_mutex_unlock(&lck_save);

        return err;
}

static int usrcfg_root_key_create(jbuf_t *b)
{
        void *root;
        int err;

        if ((err = jbuf_init(b, JBUF_INIT_ALLOC_KEYS))) {
                pr_err("jbuf_init(): %d\n", err);
                return err;
        }

        root = jbuf_obj_open(b, NULL);

        {
                void *obj_settings = jbuf_obj_open(b, "settings");

                jbuf_bool_add(b, "force_brightness_control", &g_config.force_brightness_control);
                jbuf_bool_add(b, "auto_brightness", &g_config.auto_brightness);
                jbuf_u32_add(b, "auto_brightness_update_interval_sec", &g_config.auto_brightness_update_interval_sec);
                jbuf_u32_add(b, "manual_brightness_update_interval_msec", &g_config.brightness_update_interval_msec);

                {
                        void *obj_lux_range = jbuf_obj_open(b, "lux_range");

                        jbuf_u32_add(b, "min", &g_config.lux_range.min);
                        jbuf_u32_add(b, "max", &g_config.lux_range.max);

                        jbuf_obj_close(b, obj_lux_range);
                }

                {
                        void *obj_sensorhub = jbuf_obj_open(b, "sensor_hub");

                        jbuf_strbuf_add(b, "host", g_config.sensor_hub.host, sizeof(g_config.sensor_hub.host));
                        jbuf_u32_add(b, "port", &g_config.sensor_hub.port);
                        jbuf_u32_add(b, "query_interval_sec", &g_config.sensor_hub.query_interval_sec);

                        jbuf_obj_close(b, obj_sensorhub);
                }

                {
                        void *obj_nuklear = jbuf_obj_open(b, "nuklear");

                        jbuf_u32_add(b, "theme", (uint32_t *)&nk_theme);
                        jbuf_float_add(b, "widget_height", &widget_h);

                        jbuf_obj_close(b, obj_nuklear);
                }

                jbuf_obj_close(b, obj_settings);
        }

        {
                void *arr_monitors = jbuf_list_arr_open(b, "monitors");

                jbuf_list_arr_setup(b,
                                    arr_monitors,
                                    &g_config.monitor_list,
                                    sizeof(struct monitor_cfg),
                                    offsetof(struct monitor_cfg, node),
                                    0,
                                0);

                void *obj_monitors = jbuf_offset_obj_open(b, NULL, 0);

                jbuf_offset_wstrbuf_add(b, "name", offsetof(struct monitor_cfg, name), sizeof(((struct monitor_cfg *)(0))->name));
                jbuf_offset_wstrbuf_add(b, "dev_path", offsetof(struct monitor_cfg, dev_path), sizeof(((struct monitor_cfg *)(0))->dev_path));

                {
                        void *obj_bl = jbuf_offset_obj_open(b, "brightness", 0);

                        jbuf_offset_add(b, u32, "min", offsetof(struct monitor_cfg, brightness.min));
                        jbuf_offset_add(b, u32, "max", offsetof(struct monitor_cfg, brightness.max));
                        jbuf_offset_add(b, u32, "set", offsetof(struct monitor_cfg, brightness.set));

                        jbuf_obj_close(b, obj_bl);
                }

                {
                        void *arr_luxmap = jbuf_list_arr_open(b, "lux_map");

                        jbuf_offset_list_arr_setup(b,
                                                   arr_luxmap,
                                                   offsetof(struct monitor_cfg, lux_map),
                                                   sizeof(struct monitor_cfg),
                                                   offsetof(struct lux_map, node),
                                                   0, 0);

                        void *obj_luxmap = jbuf_offset_obj_open(b, NULL, 0);

                        jbuf_offset_add(b, u32, "lux", offsetof(struct lux_map, lux));
                        jbuf_offset_add(b, u32, "bl", offsetof(struct lux_map, bl));

                        jbuf_obj_close(b, obj_luxmap);

                        jbuf_arr_close(b, arr_luxmap);
                }

                jbuf_obj_close(b, obj_monitors);
                jbuf_arr_close(b, arr_monitors);
        }

        jbuf_obj_close(b, root);

        return 0;
}

int usrcfg_lux_map_fix(void)
{
        struct list_head *pos;

        list_for_each(pos, &g_config.monitor_list) {
                struct monitor_cfg *cfg = container_of(pos, struct monitor_cfg, node);

                if (list_empty(&cfg->lux_map))
                        continue;

                lux_map_fix(cfg);
        }

        return 0;
}

int usrcfg_init(void)
{
        jbuf_t *jbuf = &jbuf_usrcfg;
        int err;

        if ((err = usrcfg_root_key_create(jbuf)))
                return err;

        pr_info("json config: %s\n", g_config.json_path);

        if (jbuf_load(jbuf, g_config.json_path)) {
                pr_mb_err("failed to load config");
                return -EINVAL;
        }

        usrcfg_lux_map_fix();

        pr_info("loaded config:\n");
        jbuf_traverse_print(jbuf);

        return 0;
}

int usrcfg_exit(void)
{
        pthread_mutex_lock(&lck_save);
        pthread_mutex_unlock(&lck_save);

        jbuf_deinit(&jbuf_usrcfg);

        return 0;
}
