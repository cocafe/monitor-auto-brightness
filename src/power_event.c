#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <winuser.h>
#include <guiddef.h>

#include <libjj/uuid.h>
#include <libjj/logging.h>
#include <libjj/utils.h>

#include "auto_brightness.h"
#include "usrcfg.h"

enum {
        DISP_OFF = 0,
        DISP_ON,
        DISP_DIMMED,
        NUM_DISP_EVENTS,
};

enum {
        ON_PWRSRC_AC,
        ON_PWRSRC_DC,
        ON_PWRSRC_HOT,
        NUM_ACDC_EVENTS,
};

enum {
        USER_PRESENT,
        USER_INACTIVE,
};

enum {
        DISP_POWER,
        ON_RESUME,
        ON_SUSPEND,
        ACDC_PWR_SRC,
        BATT_PERCENT,
        NUM_POWER_EVENTS,
};

typedef int (*notifier_cb_t)(POWERBROADCAST_SETTING *settings);

struct notifier {
        HANDLE hdl;
        LPCGUID GUID;
        notifier_cb_t cb;
};

static int display_power_notify_cb(POWERBROADCAST_SETTING *);
static int acdc_power_notify_cb(POWERBROADCAST_SETTING *);
static int battery_percent_notify_cb(POWERBROADCAST_SETTING *);

static struct notifier notifiers[] = {
        [DISP_POWER]    = { .GUID = &GUID_SESSION_DISPLAY_STATUS,       .cb = display_power_notify_cb },
        [ACDC_PWR_SRC]  = { .GUID = &GUID_ACDC_POWER_SOURCE,            .cb = acdc_power_notify_cb },
        [BATT_PERCENT]  = { .GUID = &GUID_BATTERY_PERCENTAGE_REMAINING, .cb = battery_percent_notify_cb },
};

static char *disp_evt_strs[] = {
        [DISP_OFF] = "DISPLAY_OFF",
        [DISP_ON] = "DISPLAY_ON",
        [DISP_DIMMED] = "DISPLAY_DIMMED",
};

// static char *acdc_evt_strs[] = {
//         [ON_PWRSRC_AC] = "POWERSRC_AC",
//         [ON_PWRSRC_DC] = "POWERSRC_DC",
//         [ON_PWRSRC_HOT] = "POWERSRC_UPS",
// };

static HWND notify_wnd;

static int display_power_notify_cb(POWERBROADCAST_SETTING *settings)
{
        if (settings->DataLength != sizeof(DWORD)) {
                pr_err("Invalid data length\n");
                return -EINVAL;
        }

        DWORD event = *(DWORD *)settings->Data;

        if (event >= NUM_DISP_EVENTS) {
                pr_err("Unknown event: %lu\n", event);
                return -EINVAL;
        }

        pr_dbg("event: %s\n", disp_evt_strs[event]);

        switch (event) {
        case DISP_OFF:
                auto_brightness_suspend();
                break;

        case DISP_ON:
                auto_brightness_resume();
                break;

        case DISP_DIMMED:
                break;
        }

        return 0;
}

// static int acdc_power_notify_cb(POWERBROADCAST_SETTING *settings)
// {
//         static struct list_head *task_list[] = {
//                 [ON_PWRSRC_AC] = &tasks_on_ac,
//                 [ON_PWRSRC_DC] = &tasks_on_dc,
//                 [ON_PWRSRC_HOT] = NULL,
//         };
//
//         if (settings->DataLength != sizeof(DWORD)) {
//                 pr_err("Invalid data length\n");
//                 return -EINVAL;
//         }
//
//         DWORD event = *(DWORD *)settings->Data;
//
//         if (event > NUM_ACDC_EVENTS) {
//                 pr_err("Unknown event: %lu\n", event);
//                 return -EINVAL;
//         }
//
//         pr_dbg("event: %s\n", acdc_evt_strs[event]);
//
//         event_execute(task_list[event]);
//
//         return 0;
// }

// static int battery_percent_notify_cb(POWERBROADCAST_SETTING *settings)
// {
//         if (settings->DataLength != sizeof(DWORD)) {
//                 pr_err("Invalid data length\n");
//                 return -EINVAL;
//         }
//
//         DWORD percent = *(DWORD *)settings->Data;
//
//         pr_dbg("event: battery percent on %lu\n", percent);
//
//         return 0;
// }

static inline int acdc_power_notify_cb(POWERBROADCAST_SETTING *settings)
{
        return 0;
}

static inline int battery_percent_notify_cb(POWERBROADCAST_SETTING *settings)
{
        return 0;
}

static int event_power_settings_handle(POWERBROADCAST_SETTING *settings)
{
        int err = -ENOENT;

        for (size_t i = 0; i < ARRAY_SIZE(notifiers); i++) {
                struct notifier *n = &notifiers[i];

                if (!n->GUID)
                        continue;

                if (IsEqualGUID(&settings->PowerSetting, n->GUID)) {
                        if (n->cb)
                                err = n->cb(settings);

                        break;
                }
        }

        if (err == -ENOENT) {
                pr_err("no handler for unknown GUID " UUID_FMT "\n",
                       UUID_ANY_PTR(&settings->PowerSetting));
        }

        return err;
}

static LRESULT CALLBACK powernotify_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
        if (msg != WM_POWERBROADCAST)
                goto def_proc;

        if (wparam == PBT_POWERSETTINGCHANGE) {
                event_power_settings_handle((void *)lparam);
                return TRUE;
        }

        if (wparam == PBT_APMRESUMEAUTOMATIC) {
                pr_dbg("power event: ON_RESUME\n");
                auto_brightness_trigger();
        }

        if (wparam == PBT_APMSUSPEND) {
                pr_dbg("power event: ON_SUSPEND\n");
        }

        return TRUE;

def_proc:
        return DefWindowProc(hwnd, msg, wparam, lparam);
}

static HANDLE power_notify_wnd_create(void)
{
        HWND wnd = NULL;
        WNDCLASSEX wc = { 0 };

        wc.cbSize              = sizeof(WNDCLASSEX);
        wc.lpfnWndProc         = powernotify_proc;
        wc.hInstance           = GetModuleHandle(NULL);
        wc.lpszClassName       = L"PowerNotifyWnd";
        if (!RegisterClassEx(&wc)) {
                pr_err("RegisterClassEx() failed\n");
                return NULL;
        }

        wnd = CreateWindowEx(0, L"PowerNotifyWnd",
                             NULL, 0, 0, 0, 0, 0, 0, 0,
                             GetModuleHandle(NULL), 0);
        if (wnd == NULL) {
                pr_err("CreateWindowEx() failed\n");
                return NULL;
        }

        ShowWindow(wnd, SW_HIDE);
        UpdateWindow(wnd);

        return wnd;
}

static int power_notify_register(HWND notify_wnd)
{
        for (size_t i = 0; i < ARRAY_SIZE(notifiers); i++) {
                struct notifier *n = &notifiers[i];

                if (!n->GUID)
                        continue;

                n->hdl = RegisterPowerSettingNotification(notify_wnd, n->GUID, DEVICE_NOTIFY_WINDOW_HANDLE);
                if (!n->hdl) {
                        pr_mb_err("RegisterPowerSettingNotification(): 0x%08lx\n", GetLastError());
                        return -EFAULT;
                }
        }

        return 0;
}

static void power_notify_unregister(void)
{
        for (size_t i = 0; i < ARRAY_SIZE(notifiers); i++) {
                struct notifier *n = &notifiers[i];

                if (!n->hdl)
                        continue;

                UnregisterPowerSettingNotification(n->hdl);
                n->hdl = NULL;
        }
}

int power_event_init(void)
{
        int err;

        notify_wnd = power_notify_wnd_create();
        if (!notify_wnd) {
                return -ENOENT;
        }

        if ((err = power_notify_register(notify_wnd)))
                return err;

        return 0;
}

int power_event_exit(void)
{
        power_notify_unregister();
        DestroyWindow(notify_wnd);

        return 0;
}
