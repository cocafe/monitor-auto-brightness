#include <stdio.h>

#ifdef __WINNT__
#include <fcntl.h>
#include <windows.h>
#include <windowsx.h>
#include <iconv.h>
#endif // __WINNT__

#include "iconv.h"
#include "logging.h"
#include "opts.h"

uint32_t g_logprint_level = LOG_LEVEL_DEFAULT;
uint32_t g_logprint_colored = 1;

#ifdef __WINNT__

uint32_t g_console_alloc = 0;
uint32_t g_console_show = 1;
HWND g_console_hwnd = NULL;
static uint32_t g_console_is_hide;
static uint32_t g_console_is_allocated;

#ifdef SUBSYS_WINDOW
lopt_noarg(alloc_console, &g_console_alloc, sizeof(g_console_alloc), &(uint32_t){ 1 }, "Alloc console window");
lopt_noarg(no_console, &g_console_alloc, sizeof(g_console_alloc), &(uint32_t){ 0 }, "Not to alloc console window");
lopt_noarg(hide_console, &g_console_show, sizeof(g_console_show), &(uint32_t){ 0 }, "Hide console window on startup");
#endif

void console_show(int set_focus)
{
        if (!g_console_hwnd)
                return;

        ShowWindow(g_console_hwnd, SW_NORMAL); // SW_RESTORE
        g_console_is_hide = 0;

        if (set_focus) {
                SetFocus(g_console_hwnd);
                SetForegroundWindow(g_console_hwnd);
        }
}

void console_hide(void)
{
        if (!g_console_hwnd)
                return;

        ShowWindow(g_console_hwnd, SW_HIDE);
        g_console_is_hide = 1;
}

int is_console_hid(void)
{
        return g_console_is_hide ? 1 : 0;
}

int is_console_allocated(void)
{
        return g_console_is_allocated ? 1 : 0;
}

void console_alloc_set(int enabled)
{
        g_console_alloc = enabled ? 1 : 0;
}

static void console_stdio_redirect(void)
{
        HANDLE ConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        int SystemOutput = _open_osfhandle((intptr_t)ConsoleOutput, _O_TEXT);
        FILE *COutputHandle = _fdopen(SystemOutput, "w");

        HANDLE ConsoleError = GetStdHandle(STD_ERROR_HANDLE);
        int SystemError = _open_osfhandle((intptr_t)ConsoleError, _O_TEXT);
        FILE *CErrorHandle = _fdopen(SystemError, "w");

        HANDLE ConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
        int SystemInput = _open_osfhandle((intptr_t)ConsoleInput, _O_TEXT);
        FILE *CInputHandle = _fdopen(SystemInput, "r");

        freopen_s(&CInputHandle, "CONIN$", "r", stdin);
        freopen_s(&COutputHandle, "CONOUT$", "w", stdout);
        freopen_s(&CErrorHandle, "CONOUT$", "w", stderr);
}

int __console_init(void)
{
        if (g_console_is_allocated)
                return -EALREADY;

        if (AllocConsole() == 0) {
                pr_err("AllocConsole(), err = %lu\n", GetLastError());
                return -1;
        }

        console_stdio_redirect();

        g_console_hwnd = GetConsoleWindow();
        if (!g_console_hwnd) {
                pr_err("GetConsoleWindow() failed\n");
                return -1;
        }

        g_console_is_hide = 0;
        g_console_is_allocated = 1;

        if (!g_console_show)
                console_hide();

        return 0;
}

int console_init(void)
{
        int err;

        if (!g_console_alloc)
                return 0;

        if ((err = __console_init()))
                return err;

        return 0;
}

int console_title_set(wchar_t *title)
{
        if (!g_console_is_allocated || !g_console_hwnd)
                return -ENOENT;

        if (SetWindowTextW(g_console_hwnd, title) == 0) {
                pr_err("SetWindowText(), err=%lu\n", GetLastError());
                return -EFAULT;
        }

        return 0;
}

int console_deinit(void)
{
        if (!is_console_allocated())
                return -ENOENT;

        if (FreeConsole() == 0) {
                pr_err("FreeConsole(), err = %lu\n", GetLastError());
                return -EFAULT;
        }

        g_console_is_allocated = 0;

        return 0;
}

void mb_wchar_show(char *title, char *content, size_t len, uint32_t flags)
{
        wchar_t wc_title[32] = { 0 };
        wchar_t *wc_buf;

        wc_buf = calloc(len + 4, sizeof(wchar_t));
        if (!wc_buf) {
                MessageBoxW(NULL, L"ERROR", L"failed to allocate buffer for unicode string", MB_OK);
                return;
        }

        if (iconv_utf82wc(title, strlen(title), wc_title, sizeof(wc_title) - sizeof(wchar_t))) {
                MessageBoxW(NULL, L"ERROR", L"iconv_convert() failed", MB_OK);
                goto out;
        }

        if (iconv_utf82wc(content, len * sizeof(char), wc_buf, len * sizeof(wchar_t))) {
                MessageBoxW(NULL, L"ERROR", L"iconv_convert() failed", MB_OK);
                goto out;
        }

        MessageBoxW(NULL, wc_buf, wc_title, flags);

out:
        free(wc_buf);
}

int mb_vprintf(const char *title, uint32_t flags, const char *fmt, va_list arg)
{
        va_list arg2;
        char cbuf;
        char *sbuf;
        int len, ret;

        va_copy(arg2, arg);
        len = vsnprintf(&cbuf, sizeof(cbuf), fmt, arg2);
        va_end(arg2);

        if (len < 0)
                return len;

        sbuf = calloc(len + 2, sizeof(char));
        if (!sbuf)
                return -ENOMEM;

        ret = vsnprintf(sbuf, len + 1, fmt, arg);
        if (ret < 0) {
                MessageBoxA(NULL, NULL, "vsnprintf() failed", MB_OK);
                goto out;
        }

#ifdef UNICODE
        mb_wchar_show((char *)title, sbuf, ret, flags);
#else
        MessageBox(NULL, sbuf, title, flags);
#endif

out:
        free(sbuf);

        return ret;
}

int mb_vwprintf(const wchar_t *title, uint32_t flags, const wchar_t *fmt, va_list arg)
{
        va_list arg2;
        wchar_t cbuf;
        wchar_t *sbuf;
        int len, ret;

        va_copy(arg2, arg);
        len = vsnwprintf(&cbuf, sizeof(cbuf), fmt, arg2);
        va_end(arg2);

        if (len < 0)
                return len;

        sbuf = calloc(len + 2, sizeof(wchar_t));
        if (!sbuf)
                return -ENOMEM;

        ret = vsnwprintf(sbuf, len + 1, fmt, arg);
        if (ret < 0) {
                MessageBoxW(NULL, NULL, L"vsnwprintf() failed", MB_OK);
                goto out;
        }

        MessageBoxW(NULL, sbuf, title, flags);

out:
        free(sbuf);

        return ret;
}

int mb_wprintf(const wchar_t *title, uint32_t flags, const wchar_t *fmt, ...)
{
        va_list ap;
        int ret;

        va_start(ap, fmt);
        ret = mb_vwprintf(title, flags, fmt, ap);
        va_end(ap);

        return ret;
}

int mb_printf(const char *title, uint32_t flags, const char *fmt, ...)
{
        va_list ap;
        int ret;

        va_start(ap, fmt);
        ret = mb_vprintf(title, flags, fmt, ap);
        va_end(ap);

        return ret;
}

#endif // __WINNT__

int is_logging_colored(void)
{
        return g_logprint_colored ? 1 : 0;
}

void logging_colored_set(int enabled)
{
        g_logprint_colored = enabled ? 1 : 0;
}

int logging_init(void)
{
        int err = 0;

#ifdef __WINNT__
        char *alloc_con = getenv("ALLOC_CONSOLE");
        if (alloc_con && alloc_con[0] == '1')
                g_console_alloc = 1;

        if ((err = console_init()))
                return err;

        if (is_console_allocated())
                logging_colored_set(0);
#endif

        return err;
}

int logging_exit(void)
{
        int err = 0;

#ifdef __WINNT__
        if (console_deinit())
                return err;
#endif

        return err;
}