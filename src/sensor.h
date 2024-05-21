#ifndef __AUTO_BRIGHTNESS_SENSOR_H__
#define __AUTO_BRIGHTNESS_SENSOR_H__

#include <windows.h>

extern HANDLE sem_sensor_wake;

// need to check isnan()
float lux_get(void);
time_t last_fetch_timestamp_get(void);
int is_sensorhub_connected(void);

int sensorhub_init(void);
int sensorhub_exit(void);

#endif // __AUTO_BRIGHTNESS_SENSOR_H__