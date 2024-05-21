#include <math.h>
#include <float.h>
#include <time.h>
#include <pthread.h>

#include <windows.h>

#include <libjj/logging.h>
#include <libjj/utils.h>

#include "monitor_auto_brightness.h"
#include "auto_brightness.h"
#include "monitor.h"
#include "sensor.h"
#include "usrcfg.h"

// TODO: change brightness smoothly?

HANDLE sem_autobl_stop;
int last_auto_brightness = -1;
int auto_brightness_suspend_cnt = 0;

static int is_running = 0;
static pthread_t tid_worker = -1;
static pthread_mutex_t lck_autobl = PTHREAD_MUTEX_INITIALIZER;

static int linear_resolve(struct lux_map *a, struct lux_map *b, int x)
{
        double x1, x2, y1, y2, slope, y_intercept;

        x1 = a->lux;
        x2 = b->lux;
        y1 = a->bl;
        y2 = b->bl;
        slope = (y2 - y1) / (x2 - x1);
        y_intercept = y1 - (slope * x1);

        return (int)(slope * (double)(x) + y_intercept);
}

static int __monitor_brightness_compute(struct monitor_info *mon, int lux)
{
        struct list_head *p, *n, *head = &mon->monitor_save->lux_map;
        struct lux_map *first = container_of(head->next, struct lux_map, node);
        struct lux_map *last = container_of(head->prev, struct lux_map, node);

        if (lux <= (int)first->lux)
                return (int)first->bl;

        if (lux >= (int)last->lux)
                return (int)last->bl;

        list_for_each_safe(p, n, head) {
                struct lux_map *curr = container_of(p, struct lux_map, node);
                struct lux_map *next = container_of(n, struct lux_map, node);

                if (n == head) {
                        return (int)last->bl;
                }

                if (lux == (int)curr->lux)
                        return (int)curr->bl;

                if ((int)curr->lux < lux && lux < (int)next->lux)
                        return linear_resolve(curr, next, lux);
        }

        return -EINVAL;
}

int _monitor_brightness_compute(struct monitor_info *mon, float lux)
{
        struct monitor_cfg *cfg = mon->monitor_save;
        size_t cnt;

        if (!cfg)
                return -EINVAL;

        if (isnan(lux))
                return -EINVAL;

        cnt = lux_map_count(&cfg->lux_map);

        if (cnt == 0) {
                return (int)mon->brightness.set;;
        }

        if (cnt == 1) {
                struct lux_map *map = list_first_entry(&cfg->lux_map, struct lux_map, node);
                return (int)map->bl;
        }

        return __monitor_brightness_compute(mon, lux);
}

int monitor_brightness_compute(struct monitor_info *mon, float lux)
{
        int ret = _monitor_brightness_compute(mon, lux);

        // on error
        if (ret < 0)
                return ret;

        if (ret < (int)mon->brightness.min)
                return (int)mon->brightness.min;

        if (ret > (int)mon->brightness.max)
                return (int)mon->brightness.max;

        return ret;
}

int is_auto_brightness_running(void)
{
        return is_running;
}

void auto_brightness_update(void)
{
        float lux = lux_get();
        struct tm *local_time;
        time_t ts;

        time(&ts);
        local_time = localtime(&ts);

        if (isnan(lux)) {
                pr_warn("invalid lux value, check sensor hub connection?\n");
                return;
        }

        pr_rawlvl(DEBUG, "%s", asctime(local_time));
        pr_rawlvl(DEBUG, "lux: %.1f\n", lux);

        for_each_monitor(i) {
                struct monitor_info *m = &minfo[i];
                int bl;

                if (!m->active)
                        continue;

                if (!m->cap.support_brightness)
                        continue;

                bl = monitor_brightness_compute(m, lux);
                if (bl < 0) {
                        pr_dbg("monitor_brightness_compute(): %d\n", bl);
                        continue;
                }

                pr_rawlvl(DEBUG, "monitor \"%ls\" brightness current: %d target: %d\n", m->str.name, m->brightness.curr, bl);

                if ((int)m->brightness.curr == bl)
                        continue;

                if (0 == monitor_brightness_set(i, bl))
                        m->brightness.curr = bl;
        }
}

void *auto_brightness_worker(void *arg)
{
        is_running = 1;

        while (!is_gonna_exit() && g_config.auto_brightness) {
                DWORD ret;

                auto_brightness_update();

                ret = WaitForSingleObject(sem_autobl_stop, g_config.auto_brightness_update_interval_sec * 1000);
                if (ret == WAIT_TIMEOUT)
                        pr_verbose("semaphore timed out, %s\n", is_gonna_exit() ? "gonna exit" : "continue");
        }

        is_running = 0;

        pr_info("stopped\n");

        pthread_exit(NULL);

        return NULL;
}

int _auto_brightness_start(void)
{
        pthread_t tid;

        if (!g_config.auto_brightness)
                return -EPERM;

        if (is_running) {
                pr_warn("worker is still running\n");
                return -EALREADY;
        }

        monitor_brightness_update();

        sem_autobl_stop = CreateSemaphore(NULL, 0, 1, NULL);
        if (!sem_autobl_stop) {
                pr_getlasterr("CreateSemaphore");
                return -EINVAL;
        }

        if (pthread_create(&tid, NULL, auto_brightness_worker, NULL)) {
                pr_err("pthread_create(): %d\n", -errno);
                return -errno;
        }

        tid_worker = tid;

        return 0;
}

void auto_brightness_suspend(void)
{
        if (last_auto_brightness == -1)
                last_auto_brightness = g_config.auto_brightness;

        if (g_config.auto_brightness && is_auto_brightness_running()) {
                g_config.auto_brightness = 0;
                auto_brightness_stop();
        }

        __sync_fetch_and_add(&auto_brightness_suspend_cnt, 1);
}

void auto_brightness_resume(void)
{
        if (__sync_sub_and_fetch(&auto_brightness_suspend_cnt, 1) == 0) {
                if (last_auto_brightness == 1) {
                        g_config.auto_brightness = 1;
                        auto_brightness_start();
                }

                last_auto_brightness = -1;
        }
}

int auto_brightness_start(void)
{
        int err;

        pthread_mutex_lock(&lck_autobl);
        err = _auto_brightness_start();
        pthread_mutex_unlock(&lck_autobl);

        if (!err)
                pr_info("started\n");

        return err;
}

void auto_brightness_stop(void)
{
        pthread_mutex_lock(&lck_autobl);

        if (!is_running) {
                pthread_mutex_unlock(&lck_autobl);
                return;
        }

        ReleaseSemaphore(sem_autobl_stop, 1, 0);
        pthread_join(tid_worker, NULL);
        CloseHandle(sem_autobl_stop);
        sem_autobl_stop = NULL;

        pthread_mutex_unlock(&lck_autobl);

        pr_info("stopped\n");
}
