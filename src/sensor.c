#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

#include <curl/curl.h>

#include <libjj/utils.h>
#include <libjj/opts.h>
#include <libjj/logging.h>

#include "monitor_auto_brightness.h"
#include "usrcfg.h"
#include "sensor.h"

struct chunk {
        uint8_t *buf;
        size_t size;
};

static CURL *curl;
static pthread_t tid_sensorhub;
static float lux = NAN;
static time_t last_fetch;
static int last_success;

HANDLE sem_sensor_wake;

float lux_get(void)
{
        return lux;
}

int is_sensorhub_connected(void)
{
        return last_success;
}

time_t last_fetch_timestamp_get(void)
{
        return last_fetch;
}

static size_t curl_write_cb(void *contents, size_t size, size_t nmemb, void *arg)
{
        struct chunk *chunk = arg;
        size_t realsize = size * nmemb;
        uint8_t *ptr;

        pr_verbose("%s(): total size: %zu\n", __func__, size * nmemb);

        ptr = realloc(chunk->buf, chunk->size + realsize + 2);
        if (!ptr) {
                pr_err("failed to realloc() %zu bytes\n", realsize);
                return 0;
        }

        memcpy(&ptr[chunk->size], contents, realsize);

        chunk->buf = ptr;
        chunk->size += realsize;
        chunk->buf[chunk->size] = 0;

        return realsize;
}

static int sensorhub_buf_parse(char *buf)
{
        char *str = strstr(buf, "\nsensorhub_lux");

        if (!str)
                return -ENODATA;

        char *line_end = strstr(&str[1], "\n");

        if (!line_end)
                return -EINVAL;

        char b[256] = { };
        size_t cnt = ((intptr_t)line_end - (intptr_t)str);

        strncpy(b, str, cnt);

        pr_verbose("%s\n", &b[1]);

        if (1 != sscanf(&b[1], "sensorhub_lux %f", &lux)) {
                pr_warn("failed to parse data string \"%s\"\n", b);
                return -EINVAL;
        }

        last_fetch = time(NULL);

        return 0;
}

int sensorhub_query(void)
{
        struct chunk chunk = { };
        char url[256] = { };
        CURLcode res;
        int err = 0;

        if (is_strptr_not_set(g_config.sensor_hub.host))
                return -ENODATA;

        snprintf(url, sizeof(url), "http://%s:%u/metrics", g_config.sensor_hub.host, g_config.sensor_hub.port);

        chunk.buf = malloc(1);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
                pr_warn("curl_easy_perform(): %s\n", curl_easy_strerror(res));
                err = -EFAULT;
                goto out_free;
        }

        pr_verbose("received %zu bytes\n", chunk.size);

        if (sensorhub_buf_parse((char *)chunk.buf)) {
                lux = NAN;
                err = -EINVAL;
        }

out_free:
        free(chunk.buf);

        return err;
}

void *sensorhub_worker(void *arg)
{
        last_success = 0;

        while (!is_gonna_exit()) {
                DWORD ret;

                if (sensorhub_query())
                        last_success = 0;
                else
                        last_success = 1;

                ret = WaitForSingleObject(sem_sensor_wake, g_config.sensor_hub.query_interval_sec * 1000);
                if (ret == WAIT_TIMEOUT)
                        pr_verbose("semaphore timed out, %s\n", is_gonna_exit() ? "gonna exit" : "continue");
        }

        last_success = 0;

        pthread_exit(NULL);

        return NULL;
}

int sensorhub_init(void)
{
        pthread_t tid;
        int err;

        sem_sensor_wake = CreateSemaphore(NULL, 0, 1, NULL);
        if (!sem_sensor_wake) {
                pr_getlasterr("CreateSemaphore");
                return -EINVAL;
        }

        curl_global_init(CURL_GLOBAL_ALL);

        curl = curl_easy_init();
        if (!curl) {
                pr_mb_err("curl_easy_init()\n");
                return -EFAULT;
        }

        if ((err = pthread_create(&tid, NULL, sensorhub_worker, NULL))) {
                pr_mb_err("pthread_create(): %d\n", err);
                return err;
        }

        tid_sensorhub = tid;

        return 0;
}

int sensorhub_exit(void)
{
        pthread_join(tid_sensorhub, NULL);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        CloseHandle(sem_sensor_wake);

        return 0;
}
