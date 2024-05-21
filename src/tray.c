#include <windows.h>
#include <asset/resource.h>

#include <libjj/utils.h>
#include <libjj/tray.h>
#include <libjj/logging.h>

#include "monitor_auto_brightness.h"
#include "auto_brightness.h"
#include "usrcfg.h"
#include "monitor.h"
#include "gui.h"

static void tray_lbtn_on_click(struct tray *tray, void *data)
{
        static int running = 0;

        if (__sync_bool_compare_and_swap(&running, 0, 1) == 0)
                return;

        brightness_adjust_wnd_create();

        running = 0;
}

static void console_show_click(struct tray_menu *m)
{
        if (!g_console_alloc)
                return;

        m->checked = !m->checked;

        if (m->checked) {
                g_console_show = 1;
                console_show(1);

                pr_raw("====================================================================\n");
                pr_raw("=== CLOSE THIS LOGGING WINDOW WILL TERMINATE PROGRAM, ^C TO HIDE ===\n");
                pr_raw("====================================================================\n");

                return;
        }

        g_console_show = 0;
        console_hide();
}

static void console_show_update(struct tray_menu *m)
{
        if (!g_console_alloc) {
                m->checked = 0;
                m->disabled = 1;
                return;
        }

        if (is_console_hid())
                m->checked = 0;
        else
                m->checked = 1;
}

static void loglvl_click(struct tray_menu *m)
{
        uint32_t level = (size_t)m->userdata;

        m->checked = !m->checked;

        if (m->checked) {
                g_logprint_level |= level;
        } else {
                g_logprint_level &= ~level;
        }
}

static void loglvl_update(struct tray_menu *m)
{
        uint32_t level = (size_t)m->userdata;

        if (g_logprint_level & level)
                m->checked = 1;
        else
                m->checked = 0;
}

static void quit_on_click(struct tray_menu *m)
{
        auto_brightness_quit();
}

static void save_on_click(struct tray_menu *m)
{
        usrcfg_save();
}

static void auto_brightness_pre_show(struct tray_menu *m)
{
        if (auto_brightness_suspend_cnt)
                m->disabled = 1;
        else
                m->disabled = 0;

        if (g_config.auto_brightness)
                m->checked = 1;
        else
                m->checked = 0;
}

static void auto_brightness_on_click(struct tray_menu *m)
{
        g_config.auto_brightness = !g_config.auto_brightness;

        if (!g_config.auto_brightness && is_auto_brightness_running())
                auto_brightness_stop();
        else
                auto_brightness_start();
}

static void edit_lux_map_on_click(struct tray_menu *m)
{
        lux_map_wnd_thread_create();
}

static void settings_on_click(struct tray_menu *m)
{
        settings_wnd_thread_create();
}

static struct tray auto_brightness_tray = {
        .icon = {
                .path = NULL,
                .id = IDI_APP_ICON,
        },
        .menu = (struct tray_menu[]) {
                { .name = L"Auto brightness", .pre_show = auto_brightness_pre_show, .on_click = auto_brightness_on_click },
                { .name = L"Edit lux map", .on_click = edit_lux_map_on_click },
                { .is_separator = 1 },
                { .name = L"Settings", .on_click = settings_on_click },
                { .name = L"Save config", .on_click = save_on_click },
                { .is_separator = 1 },
                {
                        .name = L"Logging",
                        .submenu = (struct tray_menu[]) {
                                { .name = L"Show", .pre_show = console_show_update, .on_click = console_show_click },
                                { .is_separator = 1 },
                                { .name = L"Verbose", .pre_show = loglvl_update, .on_click = loglvl_click, .userdata = (void *)LOG_LEVEL_VERBOSE },
                                { .name = L"Debug",   .pre_show = loglvl_update, .on_click = loglvl_click, .userdata = (void *)LOG_LEVEL_DEBUG   },
                                { .name = L"Info",    .pre_show = loglvl_update, .on_click = loglvl_click, .userdata = (void *)LOG_LEVEL_INFO    },
                                { .name = L"Notice",  .pre_show = loglvl_update, .on_click = loglvl_click, .userdata = (void *)LOG_LEVEL_NOTICE  },
                                { .name = L"Warning", .pre_show = loglvl_update, .on_click = loglvl_click, .userdata = (void *)LOG_LEVEL_WARN    },
                                { .name = L"Error",   .pre_show = loglvl_update, .on_click = loglvl_click, .userdata = (void *)LOG_LEVEL_ERROR   },
                                { .name = L"Fatal",   .pre_show = loglvl_update, .on_click = loglvl_click, .userdata = (void *)LOG_LEVEL_FATAL   },
                                { .is_end = 1 },
                        },
                },
                { .is_separator = 1 },
                { .name = L"Quit", .on_click = quit_on_click },
                { .is_end = 1 },
        },
        .lbtn_click = tray_lbtn_on_click,
        .lbtn_dblclick = NULL,
};

void auto_brightness_tray_update(void)
{
        tray_update_post(&auto_brightness_tray);
}

int auto_brightness_tray_init(HINSTANCE ins)
{
        int err;

        if ((err = tray_init(&auto_brightness_tray, ins)))
                return err;

        tray_update_post(&auto_brightness_tray);

        return 0;
}

void auto_brightness_tray_exit(void)
{
        tray_exit(&auto_brightness_tray);
}