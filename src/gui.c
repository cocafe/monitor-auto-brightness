#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>

#include <windows.h>
#include <wingdi.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_GDI_IMPLEMENTATION
#define NK_BUTTON_TRIGGER_ON_RELEASE // to fix touch input

#include <nuklear/nuklear.h>
#include <nuklear/nuklear_gdi.h>

// #define NKGDI_UPDATE_FOREGROUND_ONLY
#define NKGDI_IMPLEMENT_WINDOW
#include <nuklear/nuklear_gdiwnd.h>

#include <nuklear/nuklear_jj.h>

#include <asset/resource.h>
#include <libjj/iconv.h>
#include <libjj/utils.h>
#include <libjj/logging.h>
#include <libjj/opts.h>
#include <libjj/tray.h>

#include "monitor_auto_brightness.h"
#include "auto_brightness.h"
#include "sensor.h"
#include "tray.h"
#include "gui.h"
#include "usrcfg.h"
#include "monitor.h"

float widget_h = 30.0f;
int nk_theme = THEME_SOLARIZED_LIGHT;

#include "lux_map_edit.c"
#include "brightness_tweak.c"
#include "settings.c"

struct font_rsc {
        int id;
        HFONT font;
};

struct font_rsc g_font_rscs[] = {
        { .id = IDI_FONT_BUILTIN },
};

void font_rsc_load(void)
{
        HINSTANCE hInstance = GetModuleHandle(NULL);

        for (size_t i = 0; i < ARRAY_SIZE(g_font_rscs); i++) {
                struct font_rsc *rsc = &g_font_rscs[i];
                HRSRC hFntRes = FindResource(hInstance, MAKEINTRESOURCE(rsc->id), L"BINARY");

                if (hFntRes) {
                        HGLOBAL hFntMem = LoadResource(hInstance, hFntRes);
                        if (hFntMem) {
                                void *FntData = LockResource(hFntMem); // Will be released by windows
                                DWORD nFonts = 0, len = SizeofResource(hInstance, hFntRes);

                                rsc->font = AddFontMemResourceEx(FntData, len, NULL, &nFonts);
                        } else {
                                pr_err("failed to load font resource, id = %d\n", rsc->id);
                        }
                } else {
                        pr_err("failed to find font resource, id = %d\n", rsc->id);
                }
        }
}

void font_rsc_free(void)
{
        for (size_t i = 0; i < ARRAY_SIZE(g_font_rscs); i++) {
                struct font_rsc *rsc = &g_font_rscs[i];

                if (rsc->font)
                        RemoveFontMemResourceEx(rsc->font);
        }
}

void gui_init(void)
{
        font_rsc_load();
        nkgdi_window_init();
}

void gui_exit(void)
{
        nkgdi_window_shutdown();
        font_rsc_free();
}
