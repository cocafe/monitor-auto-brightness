#ifndef __AUTO_BRIGHTNESS_H__
#define __AUTO_BRIGHTNESS_H__

#include "monitor.h"

extern int last_auto_brightness;
extern int auto_brightness_suspend_cnt;

int monitor_brightness_compute(struct monitor_info *mon, float lux);

int is_auto_brightness_suspended(void);
int is_last_auto_brightness_enabled(void);
int is_auto_brightness_running(void);

void auto_brightness_trigger(void);

void auto_brightness_suspend(void);
void auto_brightness_resume(void);

int auto_brightness_start(void);
void auto_brightness_stop(void);

#endif // __AUTO_BRIGHTNESS_H__