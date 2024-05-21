#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <uchar.h>

#include <windows.h>
#include <winuser.h>
#include <wingdi.h>

#include <libjj/utils.h>
#include <libjj/logging.h>
#include <libjj/opts.h>
#include <libjj/jkey.h>

#include "monitor_auto_brightness.h"
#include "monitor.h"
#include "auto_brightness.h"
#include "usrcfg.h"
#include "sensor.h"
#include "tray.h"
#include "gui.h"


int g_should_exit = 0;

int is_gonna_exit(void)
{
        return g_should_exit;
}

void auto_brightness_quit(void)
{
        if (__sync_bool_compare_and_swap(&g_should_exit, 0, 1) == 0)
                return;

        ReleaseSemaphore(sem_sensor_wake, 1, 0);
        PostQuitMessage(0);
}

void wnd_msg_process(int blocking)
{
        MSG msg;

        while (1) {
                if (blocking) {
                        GetMessage(&msg, NULL, 0, 0);
                } else {
                        PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
                }

                if (msg.message == WM_QUIT)
                        break;

                TranslateMessage(&msg);
                DispatchMessage(&msg);
        }
}

int WINAPI wWinMain(HINSTANCE ins, HINSTANCE prev_ins,
                    LPWSTR cmdline, int cmdshow)
{
        int err = 0;

        // this equals "System(enhanced)" in compatibility setting
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED);

        setbuf(stdout, NULL);
        logging_colored_set(1);

        if ((err = logging_init()))
                return err;

        if ((err = lopts_parse(__argc, __wargv, NULL))) {
                if (err != -EAGAIN)
                        pr_mb_err("invalid arguments\n");

                goto out_logging;
        }

        console_init();

        if (!g_console_show)
                console_hide();

        if ((err = usrcfg_init())) {
                goto out_logging;
        }

        if ((err = monitor_init())) {
                goto out_usrcfg;
        }

        if ((err = sensorhub_init())) {
                goto out_monitor;
        }

        if ((err = auto_brightness_tray_init(ins))) {
                goto out_sensorhub;
        }

        if (g_config.auto_brightness)
                auto_brightness_start();

        gui_init();

        wnd_msg_process(1);

        gui_exit();

        auto_brightness_stop();

        auto_brightness_tray_exit();

out_sensorhub:
        sensorhub_exit();

out_monitor:
        monitor_exit();

out_usrcfg:
        usrcfg_exit();

out_logging:
        logging_exit();

        return err;
}