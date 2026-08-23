/*
 * Theming for NiTTY configuration dialogs (Win11 Settings-inspired palette).
 *
 * Win32 dark-mode support is complicated because visual styles (uxtheme)
 * paint over or ignore WM_CTLCOLOR* brushes for many control types.
 * See the long comment in theme_child_cb for details.
 *
 * Strategy:
 *   - Strip visual styles (SetWindowTheme("","")) on controls where we need
 *     full GDI control: Static labels, radio/check buttons.
 *   - Subclass group boxes and push buttons to owner-paint frame/border/text
 *     in the correct colours.
 *   - Keep DarkMode_Explorer on ALL Edit/ComboBox/ListBox controls so that
 *     scrollbars are themed dark.  Read-only edits send WM_CTLCOLORSTATIC
 *     (not WM_CTLCOLOREDIT); the ctlcolor handler paints them with the
 *     edit brush.
 */

#include "putty.h"
#include "nitty_config_theme.h"
#include "nitty_winfeat.h"

#include <commctrl.h>
#include <string.h>
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

#ifndef SS_ETCHEDHORZ
#define SS_ETCHEDHORZ 0x00000010L
#endif
#ifndef SS_ETCHEDVERT
#define SS_ETCHEDVERT 0x00000011L
#endif

/* ---- registry helpers ---- */

bool nitty_config_theme_apps_use_dark(void)
{
    HKEY key;
    DWORD value = 1;
    DWORD size = sizeof(value);
    LONG r;

    r = RegOpenKeyExA(HKEY_CURRENT_USER,
                      "Software\\Microsoft\\Windows\\CurrentVersion\\"
                      "Themes\\Personalize",
                      0, KEY_READ, &key);
    if (r != ERROR_SUCCESS)
        return false;
    r = RegQueryValueExA(key, "AppsUseLightTheme", NULL, NULL,
                         (BYTE *)&value, &size);
    RegCloseKey(key);
    if (r != ERROR_SUCCESS || size != sizeof(value))
        return false;
    return value == 0;
}

static COLORREF read_dwm_accent(void)
{
    HKEY key;
    DWORD val = 0;
    DWORD size = sizeof(val);
    LONG r;

    r = RegOpenKeyExA(HKEY_CURRENT_USER,
                      "Software\\Microsoft\\Windows\\DWM",
                      0, KEY_READ, &key);
    if (r != ERROR_SUCCESS)
        return RGB(0, 103, 192);
    r = RegQueryValueExA(key, "ColorizationColor", NULL, NULL,
                         (BYTE *)&val, &size);
    RegCloseKey(key);
    if (r != ERROR_SUCCESS || size != sizeof(val) || val == 0)
        return RGB(0, 103, 192);
    return RGB((val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF);
}

/* ---- theme init / free ---- */

void nitty_config_theme_init(nitty_config_theme *t)
{
    COLORREF bg, ed, lst, btnf;
    COLORREF tx, txdim, etx;

    nitty_config_theme_free(t);
    memset(t, 0, sizeof(*t));

    t->dark = nitty_config_theme_apps_use_dark();
    t->clr_accent = read_dwm_accent();

    if (t->dark) {
        bg = RGB(32, 32, 32);
        ed = RGB(45, 45, 45);
        lst = RGB(40, 40, 40);
        btnf = RGB(55, 55, 55);
        tx = RGB(255, 255, 255);
        txdim = RGB(200, 200, 200);
        etx = RGB(240, 240, 240);
    } else {
        bg = RGB(243, 243, 243);
        ed = RGB(255, 255, 255);
        lst = RGB(255, 255, 255);
        btnf = RGB(251, 251, 251);
        tx = RGB(0, 0, 0);
        txdim = RGB(90, 90, 90);
        etx = RGB(0, 0, 0);
    }

    t->clr_text = tx;
    t->clr_text_dim = txdim;
    t->clr_edit_text = etx;
    t->br_dialog = CreateSolidBrush(bg);
    t->br_edit = CreateSolidBrush(ed);
    t->br_list = CreateSolidBrush(lst);
    t->br_btn = CreateSolidBrush(btnf);
    t->inited = true;
}

void nitty_config_theme_free(nitty_config_theme *t)
{
    if (!t)
        return;
    if (t->br_dialog)
        DeleteObject(t->br_dialog);
    if (t->br_edit)
        DeleteObject(t->br_edit);
    if (t->br_list)
        DeleteObject(t->br_list);
    if (t->br_btn)
        DeleteObject(t->br_btn);
    memset(t, 0, sizeof(*t));
}

/* ---- tree view colours ---- */

void nitty_config_theme_apply_tree(HWND treeview, const nitty_config_theme *t)
{
    COLORREF bg, fg;

    if (!treeview || !t || !t->inited)
        return;
    if (t->dark) {
        bg = RGB(38, 38, 38);
        fg = RGB(255, 255, 255);
    } else {
        bg = RGB(255, 255, 255);
        fg = RGB(0, 0, 0);
    }
    TreeView_SetBkColor(treeview, bg);
    TreeView_SetTextColor(treeview, fg);
    /* Plus/minus and connector lines (default was grey-blue in dark theme) */
    TreeView_SetLineColor(treeview, fg);
}

/* ---- button subclass (group boxes + push buttons) ---- */

#define NITTY_BTN_SUBCLASS_ID 42001

/*
 * Subclass for group boxes and push buttons in dark mode.
 *
 * Group boxes: the classic (unthemed) group box paints its etched frame via
 * DrawEdge() which uses COLOR_3DHILIGHT / COLOR_3DSHADOW — those are system-
 * wide and light. We draw a subtle dark rounded-rect frame and the title text
 * ourselves.
 *
 * Push buttons: DarkMode_Explorer produces a bright white outline. We draw
 * a filled rounded rect with a subtle border, accent-coloured for the
 * default/focused button.
 */
static LRESULT CALLBACK nitty_btn_subclass(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    const nitty_config_theme *t = (const nitty_config_theme *)dwRefData;

    if (!t || !t->inited || !t->dark)
        goto passthrough;

    if (msg == WM_ERASEBKGND) {
        LONG s = GetWindowLong(hwnd, GWL_STYLE);
        int bt = s & BS_TYPEMASK;
        if (bt == BS_PUSHBUTTON || bt == BS_DEFPUSHBUTTON)
            return 1;
        /*
         * Without this, themed WM_ERASEBKGND runs first for radios/checks
         * and leaves a grey-blue flash under our owner-draw (notably in
         * some builds such as NiTTYtel).
         */
        if (bt == BS_AUTORADIOBUTTON || bt == BS_RADIOBUTTON ||
            bt == BS_AUTOCHECKBOX || bt == BS_CHECKBOX ||
            bt == BS_AUTO3STATE || bt == BS_3STATE) {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, t->br_dialog);
            return 1;
        }
    }

    if (msg == WM_NCPAINT) {
        return 0;
    }

    if (msg == WM_PAINT) {
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        int btype = style & BS_TYPEMASK;

        if (btype == BS_GROUPBOX) {
            /*
             * PuTTY creates group boxes AFTER their children (endbox),
             * so the group box is above them in Z-order. We must NOT
             * FillRect the interior — that would cover every label,
             * edit, and radio inside. Draw a single subtle stroke
             * (same RGB as owner-draw panel titles in controls.c) plus
             * title text with a notch over the top edge.
             */
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);

            HDC hdc = GetDC(hwnd);
            RECT rc;
            GetClientRect(hwnd, &rc);

            HFONT hf = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
            if (!hf)
                hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT oldf = (HFONT)SelectObject(hdc, hf);

            char text[256] = "";
            GetWindowTextA(hwnd, text, sizeof(text));

            SIZE sz = {0, 8};
            if (text[0])
                GetTextExtentPoint32A(hdc, text, (int)strlen(text), &sz);

            {
                const COLORREF stroke = RGB(76, 76, 76);
                int yt = sz.cy / 2;
                HPEN pen = CreatePen(PS_SOLID, 1, stroke);
                HPEN oldp = (HPEN)SelectObject(hdc, pen);
                HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

                RoundRect(hdc, rc.left, yt, rc.right - 1, rc.bottom - 1, 6, 6);
                SelectObject(hdc, oldp);
                SelectObject(hdc, oldb);
                DeleteObject(pen);
            }

            if (text[0]) {
                int tx = rc.left + 9;
                RECT tr = {tx - 2, 0, tx + sz.cx + 2, sz.cy};
                FillRect(hdc, &tr, t->br_dialog);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, t->clr_text);
                TextOutA(hdc, tx, 0, text, (int)strlen(text));
            }

            SelectObject(hdc, oldf);
            ReleaseDC(hwnd, hdc);
            return 0;
        }

        if (btype == BS_AUTORADIOBUTTON || btype == BS_RADIOBUTTON ||
            btype == BS_AUTOCHECKBOX || btype == BS_CHECKBOX ||
            btype == BS_AUTO3STATE || btype == BS_3STATE) {
            bool isradio = (btype == BS_AUTORADIOBUTTON ||
                            btype == BS_RADIOBUTTON);
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            FillRect(hdc, &rc, t->br_dialog);

            LRESULT bstate = SendMessage(hwnd, BM_GETSTATE, 0, 0);
            bool checked = (bstate & BST_CHECKED) != 0;
            bool focused = (bstate & BST_FOCUS) != 0;

            int gsz = 13;
            int gy = rc.top + (rc.bottom - rc.top - gsz) / 2;
            int gx = rc.left + 1;
            RECT gr = {gx, gy, gx + gsz, gy + gsz};

            HPEN pen = CreatePen(PS_SOLID, 1, RGB(140, 140, 140));
            HPEN oldp = (HPEN)SelectObject(hdc, pen);
            HBRUSH fillBr = CreateSolidBrush(RGB(50, 50, 50));
            HBRUSH oldb = (HBRUSH)SelectObject(hdc, fillBr);

            if (isradio)
                Ellipse(hdc, gr.left, gr.top, gr.right, gr.bottom);
            else
                RoundRect(hdc, gr.left, gr.top, gr.right, gr.bottom,
                          3, 3);

            SelectObject(hdc, oldp);
            SelectObject(hdc, oldb);
            DeleteObject(pen);
            DeleteObject(fillBr);

            if (checked) {
                COLORREF glyphClr = RGB(255, 255, 255);
                if (isradio) {
                    HBRUSH dot = CreateSolidBrush(glyphClr);
                    HBRUSH odot = (HBRUSH)SelectObject(hdc, dot);
                    HPEN np = CreatePen(PS_SOLID, 1, glyphClr);
                    HPEN onp = (HPEN)SelectObject(hdc, np);
                    Ellipse(hdc, gr.left + 3, gr.top + 3,
                            gr.right - 3, gr.bottom - 3);
                    SelectObject(hdc, onp);
                    SelectObject(hdc, odot);
                    DeleteObject(np);
                    DeleteObject(dot);
                } else {
                    HPEN cp = CreatePen(PS_SOLID, 2, glyphClr);
                    HPEN ocp = (HPEN)SelectObject(hdc, cp);
                    MoveToEx(hdc, gr.left + 3, gy + gsz/2, NULL);
                    LineTo(hdc, gr.left + 5, gr.bottom - 3);
                    LineTo(hdc, gr.right - 3, gr.top + 3);
                    SelectObject(hdc, ocp);
                    DeleteObject(cp);
                }
            }

            /* Text label */
            int gw = gsz + 5;
            RECT textrc = {rc.left + gw, rc.top, rc.right, rc.bottom};
            char text[256] = "";
            GetWindowTextA(hwnd, text, sizeof(text));
            if (text[0]) {
                HFONT hf = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
                HFONT oldf = NULL;
                if (hf)
                    oldf = (HFONT)SelectObject(hdc, hf);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, t->clr_text);
                DrawTextA(hdc, text, -1, &textrc,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                if (oldf)
                    SelectObject(hdc, oldf);
            }

            if (focused) {
                RECT fr = {textrc.left - 1, rc.top, rc.right, rc.bottom};
                DrawFocusRect(hdc, &fr);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        if (btype == BS_PUSHBUTTON || btype == BS_DEFPUSHBUTTON) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            LRESULT bstate = SendMessage(hwnd, BM_GETSTATE, 0, 0);
            bool pushed = (bstate & BST_PUSHED) != 0;
            bool focused = (bstate & BST_FOCUS) != 0;
            bool isdef = (btype == BS_DEFPUSHBUTTON);

            COLORREF face = pushed ? RGB(70, 70, 70) : RGB(55, 55, 55);
            COLORREF border = (focused || isdef)
                              ? t->clr_accent : RGB(100, 100, 100);

            HBRUSH faceBr = CreateSolidBrush(face);
            FillRect(hdc, &rc, faceBr);
            DeleteObject(faceBr);

            HPEN pen = CreatePen(PS_SOLID, 1, border);
            HPEN oldp = (HPEN)SelectObject(hdc, pen);
            HBRUSH oldb = (HBRUSH)SelectObject(hdc,
                                               GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, oldp);
            SelectObject(hdc, oldb);
            DeleteObject(pen);

            char text[256] = "";
            GetWindowTextA(hwnd, text, sizeof(text));

            HFONT hf = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
            HFONT oldf = NULL;
            if (hf)
                oldf = (HFONT)SelectObject(hdc, hf);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            DrawTextA(hdc, text, -1, &rc,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (oldf)
                SelectObject(hdc, oldf);

            EndPaint(hwnd, &ps);
            return 0;
        }
    }

passthrough:
    if (msg == WM_NCDESTROY)
        RemoveWindowSubclass(hwnd, nitty_btn_subclass, uIdSubclass);
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

/* ---- edit / combobox / listbox border + combo display subclass ---- */

#define NITTY_EDIT_SUBCLASS_ID 42002

/*
 * Paint a dark 1px border over the themed 3D border from within WM_PAINT.
 * We use GetWindowDC (which covers both NC and client areas) so we can
 * draw on top of whatever the theme engine rendered.
 */
static void nitty_overpaint_border(HWND hwnd)
{
    HDC hdc = GetWindowDC(hwnd);
    RECT wr;
    GetWindowRect(hwnd, &wr);
    int w = wr.right - wr.left;
    int h = wr.bottom - wr.top;
    int bw = GetSystemMetrics(SM_CXEDGE);

    /* Fill all NC border pixels with the edit background colour */
    HBRUSH inner = CreateSolidBrush(RGB(45, 45, 45));
    for (int i = 0; i < bw; i++) {
        RECT fr = {i, i, w - i, h - i};
        FrameRect(hdc, &fr, inner);
    }
    DeleteObject(inner);

    /* Draw 1px grey border on the outermost edge */
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
    HPEN oldp = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 0, 0, w, h);
    SelectObject(hdc, oldp);
    SelectObject(hdc, oldb);
    DeleteObject(pen);

    ReleaseDC(hwnd, hdc);
}

static LRESULT CALLBACK nitty_edit_subclass(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    const nitty_config_theme *t = (const nitty_config_theme *)dwRefData;

    if (!t || !t->inited || !t->dark)
        goto ep_passthrough;

    if (msg == WM_PAINT) {
        LRESULT lr = DefSubclassProc(hwnd, msg, wParam, lParam);
        char cls[64];
        bool isCombo = GetClassNameA(hwnd, cls, sizeof(cls)) &&
            !_stricmp(cls, "ComboBox");

        /*
         * Combo: uxtheme draws light inner rules between the fake "edit",
         * button, and outer border. Painting over the NC after WM_PAINT
         * stacks an extra frame (see nitty_overpaint_border). Wipe the
         * full client with the edit brush, then redraw field + chevron only
         * — same idea as win32-darkmodelib owner-draw combo, without the
         * GetWindowDC frame pass on top of the themed control.
         *
         * CBS_DROPDOWN (Translation charset, etc.) owns a child Edit.
         * Filling the whole client after DefSubclassProc paints over that
         * child, so the selection blinks on hover and vanishes on killfocus.
         * CBS_DROPDOWNLIST (Proxy type, etc.) has no child; we DrawText.
         */
        if (!isCombo)
            nitty_overpaint_border(hwnd);

        if (isCombo) {
            LONG style = GetWindowLong(hwnd, GWL_STYLE) & 0x0F;
            COMBOBOXINFO cbi;

            memset(&cbi, 0, sizeof(cbi));
            cbi.cbSize = sizeof(cbi);
            if (GetComboBoxInfo(hwnd, &cbi)) {
                HDC hdc = GetDC(hwnd);
                RECT cr;
                bool editable = (style == CBS_DROPDOWN && cbi.hwndItem);

                GetClientRect(hwnd, &cr);
                if (editable)
                    ExcludeClipRect(hdc, cbi.rcItem.left, cbi.rcItem.top,
                                    cbi.rcItem.right, cbi.rcItem.bottom);
                FillRect(hdc, &cr, t->br_edit);

                if (style == CBS_DROPDOWNLIST) {
                    int idx = (int)SendMessage(hwnd, CB_GETCURSEL, 0, 0);

                    if (idx != CB_ERR) {
                        int len = (int)SendMessage(
                            hwnd, CB_GETLBTEXTLEN, idx, 0);
                        if (len > 0 && len < 256) {
                            char buf[256];
                            HFONT hf = (HFONT)SendMessage(
                                hwnd, WM_GETFONT, 0, 0);
                            HFONT oldf = NULL;

                            SendMessageA(hwnd, CB_GETLBTEXT, idx,
                                         (LPARAM)buf);
                            if (hf)
                                oldf = (HFONT)SelectObject(hdc, hf);
                            SetBkMode(hdc, TRANSPARENT);
                            SetTextColor(hdc, t->clr_edit_text);
                            {
                                RECT tr = cbi.rcItem;

                                tr.left += 3;
                                DrawTextA(hdc, buf, -1, &tr,
                                          DT_LEFT | DT_VCENTER |
                                          DT_SINGLELINE | DT_NOPREFIX);
                            }
                            if (oldf)
                                SelectObject(hdc, oldf);
                        }
                    }
                }

                if (style == CBS_DROPDOWNLIST || style == CBS_DROPDOWN) {
                    FillRect(hdc, &cbi.rcButton, t->br_btn);
                    {
                        int cx = (cbi.rcButton.left + cbi.rcButton.right) / 2;
                        int cy = (cbi.rcButton.top + cbi.rcButton.bottom) / 2;
                        HPEN pen = CreatePen(PS_SOLID, 1, t->clr_text_dim);
                        HPEN op = (HPEN)SelectObject(hdc, pen);

                        MoveToEx(hdc, cx - 3, cy - 1, NULL);
                        LineTo(hdc, cx, cy + 2);
                        LineTo(hdc, cx + 3, cy - 1);
                        SelectObject(hdc, op);
                        DeleteObject(pen);
                    }
                }

                ReleaseDC(hwnd, hdc);
                if (editable)
                    InvalidateRect(cbi.hwndItem, NULL, FALSE);
            }
        }

        return lr;
    }

    if (msg == WM_NCPAINT) {
        LRESULT lr = DefSubclassProc(hwnd, msg, wParam, lParam);
        char cls[64];

        if (!GetClassNameA(hwnd, cls, sizeof(cls)) || _stricmp(cls, "ComboBox"))
            nitty_overpaint_border(hwnd);
        return lr;
    }

ep_passthrough:
    if (msg == WM_NCDESTROY)
        RemoveWindowSubclass(hwnd, nitty_edit_subclass, uIdSubclass);
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

/* ---- per-child theming callback ---- */

static BOOL CALLBACK theme_child_cb(HWND hwnd, LPARAM lp)
{
    const nitty_config_theme *t = (const nitty_config_theme *)lp;
    char cls[64];
    LONG bstyle;

    if (!GetClassNameA(hwnd, cls, lenof(cls)))
        return TRUE;

    if (!t->dark) {
        SetWindowTheme(hwnd, L"Explorer", NULL);
        if (!_stricmp(cls, "Edit") || !_stricmp(cls, "ComboBox") ||
            !_stricmp(cls, "ListBox"))
            nitty_allow_dark_mode_for_window(hwnd, false);
        if (!_stricmp(cls, "Button"))
            RemoveWindowSubclass(hwnd, nitty_btn_subclass,
                                 NITTY_BTN_SUBCLASS_ID);
        if (!_stricmp(cls, "Edit") || !_stricmp(cls, "ComboBox") ||
            !_stricmp(cls, "ListBox"))
            RemoveWindowSubclass(hwnd, nitty_edit_subclass,
                                 NITTY_EDIT_SUBCLASS_ID);
        return TRUE;
    }

    if (!_stricmp(cls, "Static")) {
        SetWindowTheme(hwnd, L"", L"");
    } else if (!_stricmp(cls, "Button")) {
        bstyle = GetWindowLong(hwnd, GWL_STYLE) & BS_TYPEMASK;
        SetWindowTheme(hwnd, L"", L"");
        if (bstyle == BS_PUSHBUTTON || bstyle == BS_DEFPUSHBUTTON ||
            bstyle == BS_GROUPBOX ||
            bstyle == BS_AUTORADIOBUTTON || bstyle == BS_RADIOBUTTON ||
            bstyle == BS_AUTOCHECKBOX || bstyle == BS_CHECKBOX ||
            bstyle == BS_AUTO3STATE || bstyle == BS_3STATE) {
            SetWindowSubclass(hwnd, nitty_btn_subclass,
                              NITTY_BTN_SUBCLASS_ID, (DWORD_PTR)t);
        }
    } else if (!_stricmp(cls, "Edit") || !_stricmp(cls, "ComboBox") ||
               !_stricmp(cls, "ListBox")) {
        HWND parent;
        char pcls[64];
        bool combo_edit = false;

        /*
         * DarkMode_Explorer gives dark scrollbars on all edits (including
         * readonly multiline ones like the NiTTYgen public-key box).
         * Read-only edits send WM_CTLCOLORSTATIC (not WM_CTLCOLOREDIT);
         * nitty_config_theme_ctlcolor handles that with the edit brush,
         * so the client area stays dark while Explorer theming skins the
         * scrollbar.
         *
         * The inner Edit of a CBS_DROPDOWN must not get nitty_edit_subclass:
         * that handler's GetWindowDC frame pass eats the visible text.
         * Theme + WM_CTLCOLOREDIT is enough for that child.
         */
        parent = GetParent(hwnd);
        if (!_stricmp(cls, "Edit") && parent &&
            GetClassNameA(parent, pcls, sizeof(pcls)) &&
            !_stricmp(pcls, "ComboBox"))
            combo_edit = true;

        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
        nitty_allow_dark_mode_for_window(hwnd, true);
        if (!combo_edit)
            SetWindowSubclass(hwnd, nitty_edit_subclass,
                              NITTY_EDIT_SUBCLASS_ID, (DWORD_PTR)t);
    } else if (!_stricmp(cls, "SysTreeView32") ||
               !_stricmp(cls, "SysListView32")) {
        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
        nitty_allow_dark_mode_for_window(hwnd, true);
        SetWindowSubclass(hwnd, nitty_edit_subclass,
                          NITTY_EDIT_SUBCLASS_ID, (DWORD_PTR)t);
    }
    return TRUE;
}

void nitty_config_theme_apply_children(HWND dlg, const nitty_config_theme *t)
{
    if (!dlg || !t || !t->inited)
        return;
    if (t->dark)
        SetWindowTheme(dlg, L"DarkMode_Explorer", NULL);
    else
        SetWindowTheme(dlg, L"Explorer", NULL);
    EnumChildWindows(dlg, theme_child_cb, (LPARAM)t);
}

void nitty_config_theme_refresh(nitty_config_theme *t, HWND treeview, HWND dlg)
{
    nitty_config_theme_init(t);
    if (treeview)
        nitty_config_theme_apply_tree(treeview, t);
    if (dlg) {
        nitty_config_theme_apply_children(dlg, t);
        InvalidateRect(dlg, NULL, TRUE);
    }
}

bool nitty_config_theme_tree_notify(nitty_config_theme *t, LPARAM lParam,
                                    LRESULT *out)
{
    NMTVCUSTOMDRAW *tvcd = (NMTVCUSTOMDRAW *)lParam;

    if (!t || !t->inited || !t->dark || !out)
        return false;

    switch (tvcd->nmcd.dwDrawStage) {
      case CDDS_PREPAINT:
        *out = CDRF_NOTIFYITEMDRAW;
        return true;
      case CDDS_ITEMPREPAINT:
        if (tvcd->nmcd.uItemState & CDIS_SELECTED) {
            tvcd->clrText = t->clr_text;
            tvcd->clrTextBk = RGB(45, 45, 45);
            FillRect(tvcd->nmcd.hdc, &tvcd->nmcd.rc, t->br_edit);
            *out = CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT;
            return true;
        }
        if (tvcd->nmcd.uItemState & CDIS_HOT) {
            tvcd->clrText = t->clr_text;
            tvcd->clrTextBk = RGB(55, 55, 55);
            FillRect(tvcd->nmcd.hdc, &tvcd->nmcd.rc, t->br_btn);
            *out = CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT;
            return true;
        }
        *out = CDRF_DODEFAULT;
        return true;
      case CDDS_ITEMPOSTPAINT:
        if (tvcd->nmcd.uItemState & (CDIS_SELECTED | CDIS_HOT)) {
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
            HPEN old = (HPEN)SelectObject(tvcd->nmcd.hdc, pen);
            HBRUSH ob = (HBRUSH)SelectObject(tvcd->nmcd.hdc, GetStockObject(NULL_BRUSH));
            Rectangle(tvcd->nmcd.hdc, tvcd->nmcd.rc.left, tvcd->nmcd.rc.top,
                      tvcd->nmcd.rc.right - 1, tvcd->nmcd.rc.bottom - 1);
            SelectObject(tvcd->nmcd.hdc, old);
            SelectObject(tvcd->nmcd.hdc, ob);
            DeleteObject(pen);
        }
        *out = CDRF_DODEFAULT;
        return true;
      default:
        break;
    }
    return false;
}

/* ---- WM_CTLCOLOR* handler ---- */

bool nitty_config_theme_ctlcolor(nitty_config_theme *t, HWND dlg, UINT msg,
                                 WPARAM wParam, LPARAM lParam, LRESULT *out)
{
    HDC hdc = (HDC)wParam;
    HWND ctl = (HWND)lParam;
    char cls[64];
    LONG style;
    int stype;

    if (!t || !t->inited || !out)
        return false;

    switch (msg) {
      case WM_CTLCOLORDLG:
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, t->dark ? RGB(32, 32, 32) : RGB(243, 243, 243));
        SetTextColor(hdc, t->clr_text);
        *out = (LRESULT)t->br_dialog;
        return true;

      case WM_CTLCOLORSTATIC:
        if (!GetClassNameA(ctl, cls, lenof(cls)))
            return false;
        /*
         * Read-only Edit controls send WM_CTLCOLORSTATIC, not
         * WM_CTLCOLOREDIT.  Paint them with the edit brush so
         * About/Licence text and NiTTYgen readonly fields go dark.
         */
        if (!_stricmp(cls, "Edit")) {
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, t->dark ? RGB(45, 45, 45) : RGB(255, 255, 255));
            SetTextColor(hdc, t->clr_edit_text);
            *out = (LRESULT)t->br_edit;
            return true;
        }
        if (_stricmp(cls, "Static"))
            return false;
        style = GetWindowLong(ctl, GWL_STYLE);
        stype = style & 0x1F;
        if (stype == SS_ICON || stype == SS_BITMAP || stype == SS_ENHMETAFILE ||
            stype == SS_ETCHEDHORZ || stype == SS_ETCHEDVERT)
            return false;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, t->dark ? RGB(32, 32, 32) : RGB(243, 243, 243));
        SetTextColor(hdc, t->clr_text);
        *out = (LRESULT)t->br_dialog;
        return true;

      case WM_CTLCOLOREDIT:
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, t->dark ? RGB(45, 45, 45) : RGB(255, 255, 255));
        SetTextColor(hdc, t->clr_edit_text);
        *out = (LRESULT)t->br_edit;
        return true;

      case WM_CTLCOLORLISTBOX:
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, t->dark ? RGB(40, 40, 40) : RGB(255, 255, 255));
        SetTextColor(hdc, t->clr_edit_text);
        *out = (LRESULT)t->br_list;
        return true;

      case WM_CTLCOLORBTN:
        if (!GetClassNameA(ctl, cls, lenof(cls)) || _stricmp(cls, "Button"))
            return false;
        style = GetWindowLong(ctl, GWL_STYLE);
        switch (style & BS_TYPEMASK) {
          case BS_GROUPBOX:
          case BS_PUSHBUTTON:
          case BS_DEFPUSHBUTTON:
            /* Subclass handles painting; just return the dialog brush
             * so any residual erase uses the right colour. */
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, t->dark ? RGB(32, 32, 32) : RGB(243, 243, 243));
            SetTextColor(hdc, t->clr_text);
            *out = (LRESULT)t->br_dialog;
            return true;
          case BS_AUTOCHECKBOX:
          case BS_AUTORADIOBUTTON:
          case BS_RADIOBUTTON:
          case BS_CHECKBOX:
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, t->dark ? RGB(32, 32, 32) : RGB(243, 243, 243));
            SetTextColor(hdc, t->clr_text);
            *out = (LRESULT)t->br_dialog;
            return true;
          default:
            break;
        }
        return false;

      default:
        return false;
    }
}
