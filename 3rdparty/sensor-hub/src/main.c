#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>

#include <libjj/utils.h>
#include <libjj/opts.h>
#include <libjj/logging.h>
#include <simple_uart/simple_uart.h>

#include "prometheus.h"

static uint32_t verbose = 0;
static uint32_t listen_port = 5959;
static uint32_t uart_baudrate = 115200;
static char uart_port[PATH_MAX] = "/dev/ttyUSB0";

lopt_noarg_simple(verbose, 1, "Enabled verbose logging");
lopt_uint_simple(listen_port, "Port to listen to");
lopt_uint_simple(uart_baudrate, "UART baud rate");
lopt_strbuf_simple(uart_port, "UART port connected to MCU");

static int g_should_exit = 0;
static prom_metric_set metrics;
static struct simple_uart *uart;
static pthread_t uart_reader_tid;

static double tempC_bmp;
static double tempC_sht;
static double rH;
static double pressure;
static double lux;
static double TVOC_ppb;
static double eCO2_ppm;
static double h2;
static double ethanol;

struct metric {
        prom_metric_def def;
        double *value;
};

static struct metric metric_list[] = {
        { { "sensorhub_tempC_bmp", "Temperature read by bmp280", PROM_METRIC_TYPE_GAUGE }, &tempC_bmp },
        { { "sensorhub_tempC_sht", "Temperature read by sht31", PROM_METRIC_TYPE_GAUGE },  &tempC_sht },
        { { "sensorhub_rH", "Relative Humidity read by sht31", PROM_METRIC_TYPE_GAUGE }, &rH },
        { { "sensorhub_pressure", "Atmospheric pressure read by bmp280", PROM_METRIC_TYPE_GAUGE }, &pressure },
        { { "sensorhub_lux", "Lux value read by bh1750", PROM_METRIC_TYPE_GAUGE }, &lux },
        { { "sensorhub_TVOC_ppb", "TVOC ppb read by SGP30", PROM_METRIC_TYPE_GAUGE }, &TVOC_ppb },
        { { "sensorhub_eCO2_ppm", "eCO2 ppm read by SGP30", PROM_METRIC_TYPE_GAUGE }, &eCO2_ppm },
        { { "sensorhub_h2", "H2 value read by SGP30", PROM_METRIC_TYPE_GAUGE }, &h2 },
        { { "sensorhub_ethanol", "Ethanol value read by SGP30", PROM_METRIC_TYPE_GAUGE }, &ethanol },
};

static int is_gonna_exit(void)
{
        return g_should_exit;
}

static void go_exit(void)
{
        // prevent enter again
        if (__sync_bool_compare_and_swap(&g_should_exit, 0, 1) == 0)
                return;

        alarm(1);

        prom_cleanup(&metrics);

        pr_err("%s()\n", __func__);
}

static void prometheus_metric_register(void)
{
        for (size_t i = 0; i < ARRAY_SIZE(metric_list); i++) {
                prom_register(&metrics, &metric_list[i].def);
        }
}

static void prometheus_metric_update(void)
{
        for (size_t i = 0; i < ARRAY_SIZE(metric_list); i++) {
                prom_metric *m = prom_get(&metrics, &metric_list[i].def, 0);
                m->value = *(metric_list[i].value);
        }

        prom_flush(&metrics);
}

static void *uart_reader(void *arg)
{
        while (!is_gonna_exit()) {
                static char buf[1024] = { 0 };
                ssize_t len = simple_uart_read_line(uart, buf, sizeof(buf), 1000);

                if (len <= 0)
                        continue;

                if (verbose)
                        pr_dbg("read: \"%s\"\n", buf);

                if (9 != sscanf(buf, "data: %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                       &tempC_bmp, &tempC_sht, &rH, &pressure, &lux, &TVOC_ppb, &eCO2_ppm, &h2, &ethanol))
                        continue;

                prometheus_metric_update();
        }

        pthread_exit(NULL);
}

static void signal_handler(int sig_no)
{
        switch (sig_no) {
        case SIGALRM:
                break;

        case SIGTERM:
        case SIGKILL:
        case SIGINT:
                go_exit();
                break;

        case SIGUSR1:
                break;

        case SIGUSR2:
                break;

        default:
                break;
        }
}

int main(int argc, char *argv[])
{
        int pid;
        int err;

        setbuf(stdout, NULL);
        setbuf(stderr, NULL);

        signal(SIGALRM, signal_handler);
        signal(SIGKILL, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGINT,  signal_handler);

        if ((err = lopts_parse(argc, argv, NULL)))
                return err;

        if (listen_port >= 65535)
                return -EINVAL;

        prom_init(&metrics);

        prometheus_metric_register();

        uart = simple_uart_open(uart_port, uart_baudrate, "8N1");
        if (!uart) {
                pr_err("failed to open uart port\n");
                return -EIO;
        }

        if ((err = pthread_create(&uart_reader_tid, NULL, uart_reader, NULL))) {
                pr_err("failed to create uart reader thread\n");
                goto exit_uart;
        }

        pid = fork();
        if (pid != 0) {
                if ((err = prom_start_server(&metrics, (int)listen_port)) < 0) {
                        pr_err("failed to start prometheus metric server: %d\n", err);
                        exit(err);
                }

                return 0;
        }

        pthread_join(uart_reader_tid, NULL);

exit_uart:
        simple_uart_close(uart);

        return err;
}