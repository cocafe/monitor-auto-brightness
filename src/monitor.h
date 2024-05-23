#ifndef __AUTO_BRIGHTNESS_MONITOR_H__
#define __AUTO_BRIGHTNESS_MONITOR_H__

#include <highlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>

#define MONITOR_MAX                     (8)

struct monitor_info {
        uint8_t active;
        uint8_t is_primary;
        struct {
                uint8_t support_brightness;
                uint8_t support_contrast;
        } cap;
        struct {
                int32_t x;
                int32_t y;
                uint32_t w;
                uint32_t h;
                uint32_t orientation;
        } desktop;
        struct {
                uint32_t min;
                uint32_t max;
                uint32_t curr;
                uint32_t set;
        } brightness;
        struct {
                wchar_t name[256];
                wchar_t dev_path[256];
                wchar_t reg_path[256];
        } str;
        struct {
                size_t mcnt;
                PHYSICAL_MONITOR *monitor_list;
                HMONITOR enum_monitor;
                HMONITOR phy_monitor;
        } handle;
        struct monitor_cfg *monitor_save;
};

#define for_each_monitor(i) for (size_t (i) = 0; (i) < ARRAY_SIZE(minfo); (i)++)

extern struct monitor_info minfo[MONITOR_MAX];
extern int monitor_info_update_required;

int monitors_brightness_update(void);

int monitor_brightness_get(struct monitor_info *m);
int monitor_brightness_update(struct monitor_info *m);
int monitor_brightness_set(struct monitor_info *m, uint32_t bl);

int monitor_info_update(void);
int monitor_info_free(void);
int monitor_init(void);
int monitor_exit(void);

#endif // __AUTO_BRIGHTNESS_MONITOR_H__