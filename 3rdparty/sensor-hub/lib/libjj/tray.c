#include <stdio.h>
#include <errno.h>

#include <windows.h>
#include <winuser.h>
#include <shellapi.h>

#include "logging.h"
#include "utils.h"
#include "tray.h"

void tray_icon_taskbar_add(struct tray *tray);
void tray_update(struct tray *tray);

static UINT msg_taskbar_created;

static LRESULT CALLBACK tray_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{

        struct tray *tray = (void *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        struct tray_data *data = &tray->data;

        switch (msg) {
        case WM_CREATE:
                break;

        case WM_CLOSE:
                DestroyWindow(hwnd);
                goto out;

        case WM_DESTROY:
                PostQuitMessage(0);
                goto out;

        case WM_TRAY_CALLBACK_MSG:
                if (lparam == WM_LBUTTONDBLCLK) {
                        if (tray->lbtn_dblclick)
                                tray->lbtn_dblclick(tray, data->userdata);

                        goto out;
                } else if (lparam == WM_LBUTTONUP) {
                        if (tray->lbtn_click)
                                tray->lbtn_click(tray, data->userdata);

                        goto out;
                }
                else if (lparam == WM_RBUTTONUP) {
                        HMENU hmenu = data->hmenu;
                        POINT p;
                        WORD item;

                        GetCursorPos(&p);
                        SetForegroundWindow(hwnd);

                        item = TrackPopupMenu(hmenu,
                                             TPM_LEFTALIGN |
                                             TPM_RIGHTBUTTON |
                                             TPM_RETURNCMD |
                                             TPM_NONOTIFY,
                                              p.x, p.y,
                                              0,
                                              hwnd,
                                              NULL);
                        SendMessage(hwnd, WM_COMMAND, item, 0);

                        goto out;
                }

                break;

        case WM_COMMAND:
                if (wparam >= MENU_ITEM_ID_BEGIN) {
                        struct tray_menu *menu = NULL;
                        HMENU hmenu = data->hmenu;
                        MENUITEMINFO item = {
                                .cbSize = sizeof(MENUITEMINFO),
                                .fMask = MIIM_ID | MIIM_DATA,
                        };

                        if (GetMenuItemInfo(hmenu, wparam, FALSE, &item)) {
                                menu = (void *)item.dwItemData;
                                if (menu && menu->on_click && !menu->disabled) {
                                        menu->on_click(menu);
                                        tray_update_post(tray);
                                }
                        }

                        goto out;
                }

                break;

        case WM_TRAY_UPDATE_MSG:
                if (wparam == TRAY_UPDATE_MAGIC) {
                        tray_update(tray);
                        goto out;
                }

                break;

        default:
                if (msg == msg_taskbar_created) {
                        tray_icon_taskbar_add(tray);
                        tray_update_post(tray);
                }

                break;
        }

        return DefWindowProc(hwnd, msg, wparam, lparam);

out:
        return 1;
}

HMENU tray_menu_do_update(struct tray_menu *m, UINT *id)
{
        HMENU hmenu = CreatePopupMenu();
        MENUITEMINFO item;

        for (; m != NULL; m++, (*id)++) {
                if (m->is_end)
                        break;

                if (m->is_separator) {
                        InsertMenu(hmenu, *id, MF_SEPARATOR, FALSE, L"");
                        m->id = *id;
                        continue;
                }

                if (!m->name)
                        continue;

                if (m->pre_show) {
                        m->pre_show(m);
                }

                memset(&item, 0x00, sizeof(item));
                item.cbSize = sizeof(MENUITEMINFO);
                item.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE | MIIM_DATA;
                item.fType = 0;
                item.fState = 0;

                if (m->disabled)
                        item.fState |= MFS_DISABLED;

                if (m->checked)
                        item.fState |= MFS_CHECKED;

                if (m->highlighted)
                        item.fState |= MFS_HILITE;

                if (m->submenu != NULL) {
                        item.fMask |= MIIM_SUBMENU;
                        item.hSubMenu = tray_menu_do_update(m->submenu, id);
                }

                // @id may change after creating sub menu
                item.wID = *id;
                item.dwTypeData = m->name;
                item.dwItemData = (ULONG_PTR)m;

                if (0 == InsertMenuItem(hmenu, *id, FALSE, &item))
                        pr_err("failed to insert menu item: \"%ls\"\n", m->name);

                m->id = *id;
        }

        return hmenu;
}

void tray_icon_init(struct tray *tray)
{
        NOTIFYICONDATA *nid = &tray->data.nid;

        memset(nid, 0, sizeof(NOTIFYICONDATA));
        nid->cbSize             = sizeof(NOTIFYICONDATA);
        nid->hWnd               = tray->data.hwnd;
        nid->uID                = 0;
        nid->uFlags             = NIF_ICON | NIF_MESSAGE;
        nid->uCallbackMessage   = WM_TRAY_CALLBACK_MSG;
}

void tray_icon_taskbar_add(struct tray *tray)
{
        if (Shell_NotifyIcon(NIM_ADD, &tray->data.nid) != TRUE) {
                pr_err("failed to create tray icon");
        }
}

void tray_icon_update(struct tray *tray)
{
        NOTIFYICONDATA *nid = &tray->data.nid;
        HICON hicon = NULL;

        if (tray->icon.id > 0) {
                hicon = LoadIcon(tray->data.ins, MAKEINTRESOURCE(tray->icon.id));
                if (hicon == NULL) {
                        pr_err("LoadIcon() failed, err=%lu\n", GetLastError());
                        goto update_icon;
                }
        } else if (tray->icon.path != NULL) {
                int ret;

                if (!wcsncmp(tray->icon.path, tray->icon.path_last, wcslen(tray->icon.path_last)))
                        return;

                ret = ExtractIconEx(tray->icon.path, 0, NULL, &hicon, 0);
                if (ret == -1) {
                        pr_err("ExtractIconEx() failed for path: %ls, err=%lu\n", tray->icon.path, GetLastError());
                        goto update_icon;
                }

                wcsncpy(tray->icon.path_last, tray->icon.path, WCBUF_LEN(tray->icon.path_last));
        } else {
                pr_verbose("neither icon path or id are defined\n");
        }

update_icon:
        if (nid->hIcon) {
                DestroyIcon(nid->hIcon);
                nid->hIcon = NULL;
        }

        nid->hIcon = hicon;
        Shell_NotifyIcon(NIM_MODIFY, nid);
}

void tray_tooltip_set(struct tray *tray, wchar_t *tip)
{
        NOTIFYICONDATA *nid = &tray->data.nid;

        if (!tip)
                return;

        wcsncpy(nid->szTip, tip, ARRAY_SIZE(nid->szTip));
        nid->szTip[ARRAY_SIZE(nid->szTip) - 1] = L'\0';
}

void tray_tooltip_update(struct tray *tray)
{
        NOTIFYICONDATA *nid = &tray->data.nid;

        if (is_strptr_not_set(nid->szTip)) {
                nid->uFlags &= ~NIF_TIP;
        } else {
                nid->uFlags |= NIF_TIP;
        }

        Shell_NotifyIcon(NIM_MODIFY, nid);
}

void tray_menu_update(struct tray *tray)
{
        HMENU prevmenu = tray->data.hmenu;
        HMENU hmenu = prevmenu;
        UINT id = MENU_ITEM_ID_BEGIN;

        pthread_mutex_lock(&tray->data.update_lck);

        // tray is about to exit
        if (hmenu == NULL)
                goto out;

        tray->data.hmenu = tray_menu_do_update(tray->menu, &id);
        tray->data.max_menu_id = id;

        SendMessage(tray->data.hwnd, WM_INITMENUPOPUP, (WPARAM)hmenu, 0);

        if (prevmenu != NULL && prevmenu != INVALID_HANDLE_VALUE) {
                DestroyMenu(prevmenu);
        }

out:
        pthread_mutex_unlock(&tray->data.update_lck);
}

void tray_update_post(struct tray *tray)
{
        PostMessage(tray->data.hwnd, WM_TRAY_UPDATE_MSG, TRAY_UPDATE_MAGIC, (LPARAM)tray);
}

void tray_update(struct tray *tray)
{
        tray_menu_update(tray);
        tray_icon_update(tray);
        tray_tooltip_update(tray);
}

int tray_init(struct tray *tray, HINSTANCE ins)
{
        WNDCLASSEX *wc;
        HWND hwnd;

        if (!tray)
                return -EINVAL;

        tray->data.nid_created = 0;
        memset(&tray->data.nid, 0, sizeof(NOTIFYICONDATA));

        wc = &tray->data.wc;
        memset(wc, 0x00, sizeof(WNDCLASSEX));
        wc->cbSize              = sizeof(WNDCLASSEX);
        wc->lpfnWndProc         = tray_wnd_proc;
        wc->hInstance           = GetModuleHandle(NULL);
        wc->lpszClassName       = WC_TRAY_CLASS_NAME;
        if (!RegisterClassEx(wc)) {
                pr_err("RegisterClassEx() failed\n");
                return -EFAULT;
        }

        hwnd = CreateWindowEx(0, WC_TRAY_CLASS_NAME, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        if (hwnd == NULL) {
                pr_err("CreateWindowEx() failed\n");
                return -EFAULT;
        }

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)tray);
        UpdateWindow(hwnd);

        // initial value
        tray->data.hmenu = INVALID_HANDLE_VALUE;
        tray->data.ins = ins;
        tray->data.hwnd = hwnd;

        pthread_mutex_init(&tray->data.update_lck, NULL);

        msg_taskbar_created = RegisterWindowMessage(L"TaskbarCreated");

        // by default, elevated privileges program
        // will not be received messages from a lower-privileged process
        ChangeWindowMessageFilterEx(hwnd, WM_COMMAND, MSGFLT_ALLOW, NULL);
        ChangeWindowMessageFilterEx(hwnd, msg_taskbar_created, MSGFLT_ALLOW, NULL);

        tray_icon_init(tray);
        tray_tooltip_set(tray, tray->tip);
        tray_icon_taskbar_add(tray);
        tray_update(tray);

        return 0;
}

void tray_exit(struct tray *tray)
{
        NOTIFYICONDATA *nid;
        HMENU hmenu;

        if (!tray)
                return;

        nid = &tray->data.nid;
        hmenu = tray->data.hmenu;

        Shell_NotifyIcon(NIM_DELETE, nid);

        if (nid->hIcon) {
                DestroyIcon(nid->hIcon);
                nid->hIcon = NULL;
        }

        pthread_mutex_lock(&tray->data.update_lck);
        if (hmenu != 0 && hmenu != INVALID_HANDLE_VALUE) {
                DestroyMenu(hmenu);
                tray->data.hmenu = NULL;
        }
        pthread_mutex_unlock(&tray->data.update_lck);

        pthread_mutex_destroy(&tray->data.update_lck);

        PostQuitMessage(0);
        UnregisterClass(WC_TRAY_CLASS_NAME, GetModuleHandle(NULL));
}

int tray_loop(int blocking) {
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

        return 0;
}

int tray_click_cb_set(struct tray *tray, void *userdata,
                      tray_click_cb lbtn_click, tray_click_cb lbtn_dblclick)
{
        if (!tray)
                return -EINVAL;

        tray->lbtn_click = lbtn_click;
        tray->lbtn_dblclick = lbtn_dblclick;
        tray->data.userdata = userdata;

        return 0;
}

struct tray_menu *tray_menu_alloc_copy(struct tray_menu *src)
{
        struct tray_menu *dst = NULL;
        struct tray_menu *m;
        size_t item_cnt = 0;

        if (!src)
                return NULL;

        for (m = src; m != NULL; m++) {
                item_cnt++;

                if (m->is_end)
                        break;
        }

        dst = calloc(item_cnt, sizeof(struct tray_menu));
        if (!dst)
                return NULL;

        memcpy(dst, src, item_cnt * sizeof(struct tray_menu));

        for (size_t i = 0; i < item_cnt; i++) {
                struct tray_menu *m_dst = &dst[i];
                struct tray_menu *m_src = &src[i];

                if (m_src->submenu != NULL) {
                        m_dst->submenu = tray_menu_alloc_copy(m_src->submenu);
                        if (m_dst->submenu == NULL)
                                goto err;
                }
        }

        return dst;

err:
        free(dst);

        return NULL;
}

void tray_menu_recursive_free(struct tray_menu *m)
{
        if (!m)
                return;

        for (struct tray_menu *i = m; i != NULL; i++) {
                tray_menu_recursive_free(i->submenu);

                if (i->is_end)
                        break;
        }

        free(m);
}

/*
 * if same (struct tray_menu *) is added multiple times to menu,
 * while getting (struct tray_menu *) by MENU ID via GetMenuItemInfo()
 * will always return the last (struct tray_menu *) added to menu,
 * this behavior is unexpected!
 */
int tray_item_id_sanity_check(struct tray *tray)
{
        struct tray_menu *menu = NULL;
        HMENU hmenu = tray->data.hmenu;
        MENUITEMINFO item = {
                .cbSize = sizeof(MENUITEMINFO),
                .fMask = MIIM_ID | MIIM_DATA,
        };

        for (size_t id = MENU_ITEM_ID_BEGIN; id < tray->data.max_menu_id; id++) {
                // no such item
                if (0 == GetMenuItemInfo(hmenu, id, FALSE, &item))
                        return -EINVAL;

                // id mismatch
                menu = (void *)item.dwItemData;
                if (menu->id != id)
                        return -EFAULT;
        }

        return 0;
}
