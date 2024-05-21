#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <windows.h>
#include <winuser.h>
#include <wingdi.h>
#include <highlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>

#include <libjj/utils.h>
#include <libjj/logging.h>
#include <libjj/opts.h>

#include "usrcfg.h"
#include "monitor.h"

static HWND notify_wnd;

struct monitor_info minfo[MONITOR_MAX];

// TODO: lock
// TODO: idle brightness

int monitor_brightness_set(size_t idx, uint32_t brightness)
{
        struct monitor_info *m;

        if (idx >= ARRAY_SIZE(minfo))
                return -EINVAL;

        m = &minfo[idx];

        if (!m->active)
                return -ENODEV;

        if (brightness > m->brightness.max)
                brightness = m->brightness.max;

        if (brightness < m->brightness.min)
                brightness = m->brightness.min;

        if (SetMonitorBrightness(m->handle.phy_monitor, brightness) == FALSE) {
                pr_getlasterr("SetMonitorBrightness()");
                return -EIO;
        }

        return 0;
}

int monitor_target_get(wchar_t *device, DISPLAYCONFIG_TARGET_DEVICE_NAME *target)
{
        DISPLAYCONFIG_PATH_INFO *paths = NULL;
        DISPLAYCONFIG_MODE_INFO *modes = NULL;
        uint32_t n_path, n_mode;
        int err = -ENOENT;

        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,
                                        &n_path,
                                        &n_mode)
                                        != ERROR_SUCCESS) {
                pr_getlasterr("GetDisplayConfigBufferSizes");
                return -EINVAL;
        }

        paths = calloc(n_path, sizeof(DISPLAYCONFIG_PATH_INFO));
        modes = calloc(n_mode, sizeof(DISPLAYCONFIG_MODE_INFO));

        if (!paths || !modes) {
                err = -ENOMEM;
                goto out_free;
        }

        if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,
                               &n_path,
                               paths,
                               &n_mode,
                               modes,
                               NULL) != ERROR_SUCCESS) {
                pr_getlasterr("QueryDisplayConfig");
                err = -EINVAL;

                goto out_free;
        }

        for (size_t i = 0; i < n_path; i++) {
                const DISPLAYCONFIG_PATH_INFO *path = &paths[i];
                DISPLAYCONFIG_SOURCE_DEVICE_NAME source = { };

                source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
                source.header.size = sizeof(source);
                source.header.adapterId = path->sourceInfo.adapterId;
                source.header.id = path->sourceInfo.id;

                if ((DisplayConfigGetDeviceInfo(&source.header) == ERROR_SUCCESS) &&
                        (0 == wcscmp(device, source.viewGdiDeviceName))) {
                        target->header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
                        target->header.size = sizeof(*target);
                        target->header.adapterId = path->sourceInfo.adapterId;
                        target->header.id = path->targetInfo.id;

                        if (DisplayConfigGetDeviceInfo(&target->header) != ERROR_SUCCESS) {
                                pr_getlasterr("DisplayConfigGetDeviceInfo");
                                err = -EIO;
                                break;
                        }

                        err = 0;
                        break;
                }

        }

out_free:
        if (paths)
                free(paths);

        if (modes)
                free(modes);

        return err;
}

int phy_monitor_desc_get(struct monitor_info *m, HMONITOR monitor)
{
        MONITORINFOEXW info = { };
        DISPLAYCONFIG_TARGET_DEVICE_NAME target = {};
        info.cbSize = sizeof(info);
        int err;

        if (GetMonitorInfoW(monitor, (MONITORINFO *)&info) == FALSE) {
                pr_getlasterr("GetMonitorInfo");
                return -1;
        }

        if ((err = monitor_target_get(info.szDevice, &target))) {
                return err;
        }

        __wstr_ncpy(m->str.name, target.monitorFriendlyDeviceName, WCBUF_LEN(m->str.name));
        __wstr_ncpy(m->str.reg_path, target.monitorDevicePath, WCBUF_LEN(m->str.reg_path));
        __wstr_ncpy(m->str.dev_path, info.szDevice, WCBUF_LEN(m->str.dev_path));

        if (info.dwFlags == MONITORINFOF_PRIMARY) {
                pr_rawlvl(DEBUG, "monitor is primary\n");
                m->is_primary = 1;
        }

        pr_rawlvl(DEBUG, "monitor name: \"%ls\"\n", target.monitorFriendlyDeviceName);
        pr_rawlvl(DEBUG, "manufacture id: %x\n", target.edidManufactureId);
        pr_rawlvl(DEBUG, "product id: %x\n", target.edidProductCodeId);
        pr_rawlvl(DEBUG, "device path: \"%ls\"\n", info.szDevice);
        pr_rawlvl(DEBUG, "device reg path: \"%ls\"\n", target.monitorDevicePath);

        return 0;
}

int phy_monitor_enum_cb(HMONITOR monitor, HDC hdc, RECT *rect, LPARAM param)
{
        int *reset_idx = (void *)param;
        static size_t i = 0;

        if (*reset_idx) {
                i = 0;
                *reset_idx = 0;
        }

        struct monitor_info *m = &minfo[i];

        pr_rawlvl(DEBUG, "monitor %zu:\n", i);

        if (phy_monitor_desc_get(m, monitor))
                return FALSE;

        {
                DWORD cnt = 0;

                GetNumberOfPhysicalMonitorsFromHMONITOR(monitor, &cnt);

                if (cnt != 1)
                        pr_warn("associated monitor cnt = %lu\n", cnt);

                LPPHYSICAL_MONITOR monitor_list = calloc(1, cnt * sizeof(PHYSICAL_MONITOR));

                if (GetPhysicalMonitorsFromHMONITOR(monitor, cnt, monitor_list) == FALSE) {
                        pr_getlasterr("GetPhysicalMonitorsFromHMONITOR");
                        free(monitor_list);

                        return FALSE;
                }

                // pr_rawlvl(DEBUG, "desc: %ls\n", monitor_list[0].szPhysicalMonitorDescription);

                m->handle.mcnt = cnt;
                m->handle.enum_monitor = monitor;
                m->handle.monitor_list = monitor_list;
                m->handle.phy_monitor = monitor_list[0].hPhysicalMonitor;
        }

        m->active = 1;

        i++;

        if (i >= ARRAY_SIZE(minfo)) {
                pr_err("monitor idx is overflow\n");
                return FALSE;
        }

        return TRUE;
}

int __phy_monitor_info_update(struct monitor_info *m)
{
        HMONITOR hmonitor = m->handle.monitor_list[0].hPhysicalMonitor;
        unsigned long cap = 0, color_cap = 0;

        if (GetMonitorCapabilities(hmonitor, &cap, &color_cap) == FALSE) {
                pr_warn("monitor \"%ls\" seems like does not support DDC/CI\n", m->str.name);
        }

        if (g_config.force_brightness_control || (cap & MC_CAPS_BRIGHTNESS)) {
                DWORD min = 0, max = 0, curr;

                if (cap & MC_CAPS_BRIGHTNESS)
                        pr_rawlvl(DEBUG, "monitor \"%ls\" supports brightness control\n", m->str.name);

                if (GetMonitorBrightness(hmonitor, &min, &curr, &max) == FALSE) {
                        pr_getlasterr("GetMonitorBrightness");
                } else {
                        if (min == 0 && max == 0) {
                                pr_warn("monitor \"%ls\" invalid brightness value\n", m->str.name);
                        } else {
                                pr_rawlvl(DEBUG, "monitor \"%ls\" brightness: min: %lu max: %lu curr: %lu\n", m->str.name, min, max, curr);

                                m->brightness.min = min;
                                m->brightness.max = max;
                                m->brightness.curr = curr;
                                m->brightness.set = curr;
                                m->cap.support_brightness = 1;
                        }
                }
        }

        if (cap & MC_CAPS_CONTRAST) {
                pr_rawlvl(DEBUG, "monitor \"%ls\" supports contrast control\n", m->str.name);
                m->cap.support_contrast = 1;
        }

        return 0;
}

int phy_monitor_info_update(void)
{
        for_each_monitor(i) {
                struct monitor_info *m = &minfo[i];
                int err;

                if (!m->active)
                        continue;

                if ((err = __phy_monitor_info_update(m)))
                        return err;
        }

        return 0;
}

int __monitor_brightness_update(struct monitor_info *m)
{
        HMONITOR hmonitor = m->handle.monitor_list[0].hPhysicalMonitor;
        DWORD min = 0, max = 0, curr;

        if (GetMonitorBrightness(hmonitor, &min, &curr, &max) == FALSE) {
                pr_getlasterr("GetMonitorBrightness");
                return -EFAULT;
        }

        m->brightness.curr = curr;

        return 0;
}

int monitor_brightness_update(void)
{
        for_each_monitor(i) {
                struct monitor_info *m = &minfo[i];
                int err;

                if (!m->active)
                        continue;

                if (!m->cap.support_brightness)
                        continue;

                if ((err = __monitor_brightness_update(m)))
                        return err;
        }

        return 0;
}

void __monitor_info_free(struct monitor_info *m)
{
        if (!m->active)
                return;

        if (m->handle.monitor_list) {
                DestroyPhysicalMonitors(m->handle.mcnt, m->handle.monitor_list);
                free(m->handle.monitor_list);
        }

        memset(m, 0, sizeof(struct monitor_info));
}

int monitor_info_free(void)
{
        for_each_monitor(i) {
                __monitor_info_free(&minfo[i]);
        }

        return 0;
}

int __monitor_desktop_info_update(struct monitor_info *m, DISPLAY_DEVICE *dev, DEVMODE *mode)
{
        m->desktop.x = mode->dmPosition.x;
        m->desktop.y = mode->dmPosition.y;
        m->desktop.w = mode->dmPelsWidth;
        m->desktop.h = mode->dmPelsHeight;
        m->desktop.orientation = mode->dmDisplayOrientation;

        return 0;
}

int monitor_desktop_info_update(DISPLAY_DEVICE *dev, DEVMODE *mode)
{
        for_each_monitor(i) {
                struct monitor_info *m = &minfo[i];

                if (!m->active)
                        continue;

                if (!is_wstr_equal(dev->DeviceName, m->str.dev_path))
                        continue;

                return __monitor_desktop_info_update(m, dev, mode);
        }

        return -ENODEV;
}

int virtual_desktop_info_update(void)
{
        for (DWORD i = 0; ; i++) {
                DISPLAY_DEVICE dev = { .cb = sizeof(DISPLAY_DEVICE) };
                DEVMODE mode = { .dmSize = sizeof(DEVMODE) };

                if (FALSE == EnumDisplayDevices(NULL, i, &dev, 0)) {
                        if (i == 0) {
                                pr_getlasterr("EnumDisplayDevices");
                                return -EIO;
                        }

                        break;
                }

                if (0 == (dev.StateFlags & DISPLAY_DEVICE_ACTIVE)) {
                        pr_rawlvl(DEBUG, "Display #%lu (not active)\n", i);
                        continue;
                }

                if ((dev.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)) {
                        pr_rawlvl(DEBUG, "Display #%lu (mirroring)\n", i);
                        continue;
                }

                pr_rawlvl(DEBUG, "Display #%lu\n", i);
                pr_rawlvl(DEBUG, "       Name:   %ls\n", dev.DeviceName);
                pr_rawlvl(DEBUG, "       String: %ls\n", dev.DeviceString);
                pr_rawlvl(DEBUG, "       Flags:  0x%08lx\n", dev.StateFlags);
                pr_rawlvl(DEBUG, "       RegKey: %ls\n", dev.DeviceKey);

                if (!EnumDisplaySettings(dev.DeviceName, ENUM_CURRENT_SETTINGS, &mode)) {
                        pr_getlasterr("EnumDisplaySettings\n");
                        continue;
                }

                //
                // the primary display is always located at 0,0
                //

                pr_rawlvl(DEBUG, "       Mode: %lux%lu @ %lu Hz %lu bpp\n", mode.dmPelsWidth, mode.dmPelsHeight, mode.dmDisplayFrequency, mode.dmBitsPerPel);
                pr_rawlvl(DEBUG, "       Orientation: %lu\n", mode.dmDisplayOrientation);
                pr_rawlvl(DEBUG, "       Desktop position: ( %ld, %ld )\n", mode.dmPosition.x, mode.dmPosition.y);

                monitor_desktop_info_update(&dev, &mode);
        }

        return 0;
}

int monitor_info_update(void)
{
        int reset_idx = 1;
        int err = 0;

        monitor_info_free();

        if (EnumDisplayMonitors(NULL, NULL, phy_monitor_enum_cb, (intptr_t)&reset_idx) == FALSE) {
                pr_getlasterr("EnumDisplayMonitors\n");
                return -EIO;
        }

        if ((err = phy_monitor_info_update()))
                return err;

        if ((err = virtual_desktop_info_update()))
                return err;

        if ((err = usrcfg_monitor_info_merge()))
                return err;

        return err;
}

static LRESULT CALLBACK notify_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
        if (msg != WM_DISPLAYCHANGE)
                goto def_proc;

        // pr_info("display changed: bit: %lld %ux%u\n", wparam, LOWORD(lparam), HIWORD(lparam));
        pr_dbg("display mode changed\n");

        monitor_info_update();

        return TRUE;

def_proc:
        return DefWindowProc(hwnd, msg, wparam, lparam);
}

static HANDLE notify_wnd_create(void)
{
        HWND wnd = NULL;
        WNDCLASSEX wc = { 0 };

        wc.cbSize              = sizeof(WNDCLASSEX);
        wc.lpfnWndProc         = notify_wnd_proc;
        wc.hInstance           = GetModuleHandle(NULL);
        wc.lpszClassName       = L"NotifyWnd";
        if (!RegisterClassEx(&wc)) {
                return NULL;
        }

        wnd = CreateWindowEx(0, L"NotifyWnd",
                             NULL, 0, 0, 0, 0, 0, 0, 0,
                             GetModuleHandle(NULL), 0);
        if (wnd == NULL) {
                return NULL;
        }

        ShowWindow(wnd, SW_HIDE);
        UpdateWindow(wnd);

        return wnd;
}

int monitor_init(void)
{
        int err;

        if ((err = monitor_info_update())) {
                pr_mb_err("failed to read monitor info\n");
                return err;
        }

        notify_wnd = notify_wnd_create();
        if (!notify_wnd) {
                pr_mb_err("failed to register display change notification\n");
                return -EFAULT;
        }

        return 0;
}

int monitor_exit(void)
{
        DestroyWindow(notify_wnd);
        monitor_info_free();

        return 0;
}
