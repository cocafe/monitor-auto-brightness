#ifndef __AUTO_BRIGHTNESS_USRCFG_H__
#define __AUTO_BRIGHTNESS_USRCFG_H__

#include <stdint.h>
#include <limits.h>

#include <libjj/list.h>

struct config {
        char json_path[PATH_MAX];
        uint32_t auto_save;
        uint32_t force_brightness_control;
        uint32_t auto_brightness;
        uint32_t auto_brightness_update_interval_sec;
        uint32_t brightness_update_interval_msec;
        struct list_head monitor_list;
        struct {
                char host[256];
                uint32_t port;
                uint32_t query_interval_sec;
        } sensor_hub;
        struct {
                uint32_t min;
                uint32_t max;
        } lux_range;
};

struct monitor_cfg {
        struct list_head node;
        wchar_t name[256];
        wchar_t dev_path[512];
        struct {
                uint32_t min;
                uint32_t max;
                uint32_t set;
        } brightness;
        struct list_head lux_map;
};

struct lux_map {
        struct list_head node;
        uint32_t lux;
        uint32_t bl;
};

extern struct config g_config;

size_t lux_map_count(struct list_head *head);
void lux_map_fix(struct monitor_cfg *cfg);

int usrcfg_monitor_info_merge(void);

int usrcfg_save_lock(void);
int usrcfg_save_trylock(void);
int usrcfg_save_unlock(void);
int usrcfg_save(void);

int usrcfg_init(void);
int usrcfg_exit(void);

#endif // __AUTO_BRIGHTNESS_USRCFG_H__
