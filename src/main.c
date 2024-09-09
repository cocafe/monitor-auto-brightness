#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <uchar.h>
#include <signal.h>

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
#include "power_event.h"
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

static void sigint_handler(int dummy) {
        UNUSED_PARAM(dummy);

        pr_info("receive SIGINT\n");

        // auto_brightness_quit();
}

static BOOL HandlerRoutine(DWORD dwCtrlType)
{
        switch (dwCtrlType) {
        case CTRL_C_EVENT: // ^C event
                console_hide();
                auto_brightness_tray_update();
                break;

        case CTRL_CLOSE_EVENT: // console is being closed
                // auto_brightness_quit();
                break;

        case CTRL_LOGOFF_EVENT: // user is logging off
        case CTRL_SHUTDOWN_EVENT: // system is shutting down
        case CTRL_BREAK_EVENT: // ^break
        default:
                break;
        }

        return TRUE; // FALSE will pass event to next signal handler
};

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

int wmain(int argc, wchar_t *argv[])
{
        int err = 0;

        // this equals "System(enhanced)" in compatibility setting
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED);

        setbuf(stdout, NULL);
        logging_colored_set(1);

        if ((err = lopts_parse(__argc, __wargv, NULL))) {
                if (err != -EAGAIN)
                        pr_mb_err("invalid arguments\n");

                return err;
        }

        if ((err = logging_init()))
                return err;

        if (g_console_alloc) {
                while (!IsWindowEnabled(g_console_hwnd))
                        Sleep(1);

                // if HANDLER is NULL, and TRUE is set, console will ignore ^C
                // TRUE: add handler
                // FALSE: remove handler
                SetConsoleCtrlHandler(HandlerRoutine, TRUE);

                // XXX: racing with console window initialization
                console_title_set(L"Auto Brightness Log (^C to hide console)");
        } else {
                signal(SIGINT, sigint_handler);
        }

        if (!g_console_show)
                console_hide();

        if ((err = usrcfg_init())) {
                goto out_logging;
        }

        if ((err = monitors_init())) {
                goto out_usrcfg;
        }

        if ((err = sensorhub_init())) {
                goto out_monitor;
        }

        if ((err = auto_brightness_tray_init(GetModuleHandle(NULL)))) {
                goto out_sensorhub;
        }

        if ((err = power_event_init()))
                goto out_tray;

        if (g_config.auto_brightness)
                auto_brightness_start();

        gui_init();

        wnd_msg_process(1);

        gui_exit();

        auto_brightness_stop();

        power_event_exit();

out_tray:
        auto_brightness_tray_exit();

out_sensorhub:
        sensorhub_exit();

out_monitor:
        monitors_exit();

out_usrcfg:
        usrcfg_exit();

out_logging:
        logging_exit();

        return err;
}