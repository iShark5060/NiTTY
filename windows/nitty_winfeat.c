/*
 * NiTTY: layered-window transparency and minimize-to-tray.
 */

#include "nitty_winfeat.h"

#include "win-gui-seat.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <string.h>
#include <wchar.h>

#include "putty-rc.h"

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")

extern HINSTANCE hinst;

static HMODULE nitty_dwmapi_module;
DECL_WINDOWS_FUNCTION(static, HRESULT, DwmSetWindowAttribute,
                      (HWND, DWORD, LPCVOID, DWORD));

/* dwmapi.h values (avoid SDK version assumptions) */
#define NITTY_DWMWA_USE_IMMERSIVE_DARK_MODE 20
#define NITTY_DWMWA_SYSTEMBACKDROP_TYPE 38
#define NITTY_DWMSBT_MAINWINDOW 2 /* Mica for top-level windows (Windows 11+) */

static bool winfeat_inited;
static UINT msg_taskbar_created;

/*
 * Undocumented uxtheme exports (widely used; safe if unavailable):
 *   104 — RefreshImmersiveColorPolicyState (after app mode change)
 *   133 — AllowDarkModeForWindow (NC incl. WS_VSCROLL)
 *   134 / 136 — FlushMenuThemes (ordinal varies by Windows build)
 *   135 — SetPreferredAppMode (menu bar band + popups)
 */
typedef BOOL (WINAPI *nitty_AllowDarkModeForWindow_fn)(HWND, BOOL);
typedef int (WINAPI *nitty_SetPreferredAppMode_fn)(int);
typedef void (WINAPI *nitty_FlushMenuThemes_fn)(void);
typedef void (WINAPI *nitty_RefreshImmersiveColorPolicyState_fn)(void);

static nitty_AllowDarkModeForWindow_fn nitty_pAllowDarkModeForWindow;
static nitty_SetPreferredAppMode_fn nitty_pSetPreferredAppMode;
static nitty_FlushMenuThemes_fn nitty_pFlushMenuThemes;
static nitty_RefreshImmersiveColorPolicyState_fn
    nitty_pRefreshImmersiveColorPolicyState;

#ifndef NITTY_DWMWA_NCRENDERING_POLICY
#define NITTY_DWMWA_NCRENDERING_POLICY 2
#endif
#ifndef DWMNCRP_USEWINDOWSTYLE
#define DWMNCRP_USEWINDOWSTYLE 0
#endif
#ifndef DWMNCRP_ENABLED
#define DWMNCRP_ENABLED 2
#endif

/*
 * "Apps" light/dark preference from Settings > Personalization > Colors
 * (same signal most Win32 apps use for dark title bars).
 */
static bool nitty_windows_apps_prefer_dark(void)
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

static void nitty_load_uxtheme_dark_helpers(void)
{
    static bool tried;
    HMODULE ux;

    if (tried)
        return;
    tried = true;
    ux = GetModuleHandleW(L"uxtheme.dll");
    if (!ux)
        return;
    nitty_pAllowDarkModeForWindow = (nitty_AllowDarkModeForWindow_fn)(void *)
        GetProcAddress(ux, (LPCSTR)(UINT_PTR)133);
    nitty_pSetPreferredAppMode = (nitty_SetPreferredAppMode_fn)(void *)
        GetProcAddress(ux, (LPCSTR)(UINT_PTR)135);
    nitty_pFlushMenuThemes = (nitty_FlushMenuThemes_fn)(void *)
        GetProcAddress(ux, (LPCSTR)(UINT_PTR)134);
    if (!nitty_pFlushMenuThemes)
        nitty_pFlushMenuThemes = (nitty_FlushMenuThemes_fn)(void *)
            GetProcAddress(ux, (LPCSTR)(UINT_PTR)136);
    nitty_pRefreshImmersiveColorPolicyState =
        (nitty_RefreshImmersiveColorPolicyState_fn)(void *)
            GetProcAddress(ux, (LPCSTR)(UINT_PTR)104);
}

/*
 * UAH (undocumented) menu-bar painting — same approach as win32-darkmodelib /
 * UAHMenuBar. Only used when the window has an HMENU (e.g. NiTTYgen).
 */
#define WM_UAHDRAWMENU     0x0091
#define WM_UAHDRAWMENUITEM 0x0092

#define NITTY_UAH_SUBCLASS_ID 42007

/* vsstyle.h MENU parts / BARITEM states (avoid SDK header coupling) */
#define NITTY_MENU_BARITEM 7
#define NITTY_MBI_NORMAL          1
#define NITTY_MBI_HOT             2
#define NITTY_MBI_PUSHED          3
#define NITTY_MBI_DISABLED        4
#define NITTY_MBI_DISABLEDHOT     5
#define NITTY_MBI_DISABLEDPUSHED  6

typedef union tag_UAHMENUITEMMETRICS {
    struct {
        DWORD cx;
        DWORD cy;
    } rgsizeBar[2];
    struct {
        DWORD cx;
        DWORD cy;
    } rgsizePopup[4];
} UAHMENUITEMMETRICS;

typedef struct tag_UAHMENUPOPUPMETRICS {
    DWORD rgcx[4];
    DWORD fUpdateMaxWidths : 2;
} UAHMENUPOPUPMETRICS;

typedef struct tag_UAHMENU {
    HMENU hmenu;
    HDC hdc;
    DWORD dwFlags;
} UAHMENU;

typedef struct tag_UAHMENUITEM {
    int iPosition;
    UAHMENUITEMMETRICS umim;
    UAHMENUPOPUPMETRICS umpm;
} UAHMENUITEM;

typedef struct tag_UAHDRAWMENUITEM {
    DRAWITEMSTRUCT dis;
    UAHMENU um;
    UAHMENUITEM umi;
} UAHDRAWMENUITEM;

struct nitty_uah_data {
    HTHEME theme;
    HBRUSH br_menu;
    HBRUSH br_hot;
    HBRUSH br_sel;
};

static void nitty_uah_free_data(struct nitty_uah_data *d)
{
    if (!d)
        return;
    if (d->theme)
        CloseThemeData(d->theme);
    if (d->br_menu)
        DeleteObject(d->br_menu);
    if (d->br_hot)
        DeleteObject(d->br_hot);
    if (d->br_sel)
        DeleteObject(d->br_sel);
    sfree(d);
}

static void nitty_uah_paint_menubar_bg(HWND hwnd, HDC hdc, HBRUSH br)
{
    MENUBARINFO mbi;
    RECT rcWindow, rcBar;

    memset(&mbi, 0, sizeof(mbi));
    mbi.cbSize = sizeof(mbi);
    if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
        return;
    GetWindowRect(hwnd, &rcWindow);
    rcBar = mbi.rcBar;
    OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top);
    rcBar.top -= 1;
    FillRect(hdc, &rcBar, br);
}

static void nitty_uah_draw_menu_bottom_line(HWND hwnd, HBRUSH br)
{
    MENUBARINFO mbi;
    RECT rcClient, rcWindow, rcLine;
    HDC hdc;

    memset(&mbi, 0, sizeof(mbi));
    mbi.cbSize = sizeof(mbi);
    if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
        return;
    GetClientRect(hwnd, &rcClient);
    MapWindowPoints(hwnd, NULL, (POINT *)&rcClient, 2);
    GetWindowRect(hwnd, &rcWindow);
    OffsetRect(&rcClient, -rcWindow.left, -rcWindow.top);
    rcLine = rcClient;
    rcLine.bottom = rcLine.top;
    rcLine.top--;
    hdc = GetWindowDC(hwnd);
    FillRect(hdc, &rcLine, br);
    ReleaseDC(hwnd, hdc);
}

static void nitty_uah_paint_menu_item(UAHDRAWMENUITEM *ud,
                                      struct nitty_uah_data *d)
{
    wchar_t buf[MAX_PATH];
    MENUITEMINFOW mii;
    int bgst = NITTY_MBI_NORMAL;
    int txst = NITTY_MBI_NORMAL;
    RECT *rc = &ud->dis.rcItem;
    HDC hdc = ud->um.hdc;
    DWORD st = ud->dis.itemState;

    memset(&mii, 0, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STRING;
    mii.dwTypeData = buf;
    mii.cch = MAX_PATH - 1;
    if (!GetMenuItemInfoW(ud->um.hmenu, (UINT)ud->umi.iPosition, TRUE, &mii))
        return;

    if ((st & ODS_SELECTED))
        bgst = txst = NITTY_MBI_PUSHED;
    else if ((st & ODS_HOTLIGHT))
        bgst = txst = ((st & ODS_INACTIVE) ? NITTY_MBI_DISABLEDHOT : NITTY_MBI_HOT);
    else if ((st & ODS_GRAYED) || (st & ODS_DISABLED) || (st & ODS_INACTIVE))
        bgst = txst = NITTY_MBI_DISABLED;

    switch (bgst) {
      case NITTY_MBI_NORMAL:
      case NITTY_MBI_DISABLED:
        FillRect(hdc, rc, d->br_menu);
        break;
      case NITTY_MBI_HOT:
      case NITTY_MBI_DISABLEDHOT:
        FillRect(hdc, rc, d->br_hot);
        break;
      case NITTY_MBI_PUSHED:
      case NITTY_MBI_DISABLEDPUSHED:
        FillRect(hdc, rc, d->br_sel);
        break;
      default:
        if (d->theme)
            DrawThemeBackground(d->theme, hdc, NITTY_MENU_BARITEM, bgst, rc,
                                NULL);
        break;
    }

    {
        DTTOPTS opt;
        COLORREF txc = RGB(255, 255, 255);

        if (txst == NITTY_MBI_DISABLED || txst == NITTY_MBI_DISABLEDHOT ||
            txst == NITTY_MBI_DISABLEDPUSHED)
            txc = RGB(160, 160, 160);

        if (d->theme) {
            memset(&opt, 0, sizeof(opt));
            opt.dwSize = sizeof(opt);
            opt.dwFlags = DTT_TEXTCOLOR;
            opt.crText = txc;
            DrawThemeTextEx(d->theme, hdc, NITTY_MENU_BARITEM, txst, buf,
                            (int)wcslen(buf),
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE |
                                ((st & ODS_NOACCEL) ? DT_HIDEPREFIX : 0),
                            rc, &opt);
        } else {
            UINT dt = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, txc);
            if (st & ODS_NOACCEL)
                dt |= DT_HIDEPREFIX;
            DrawTextW(hdc, buf, (int)wcslen(buf), rc, dt);
        }
    }
}

static LRESULT CALLBACK nitty_uah_menubar_subclass(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subId, DWORD_PTR ref)
{
    struct nitty_uah_data *d = (struct nitty_uah_data *)ref;

    (void)subId;

    if (msg != WM_NCDESTROY && (!d || !GetMenu(hwnd)))
        return DefSubclassProc(hwnd, msg, wParam, lParam);

    switch (msg) {
      case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, nitty_uah_menubar_subclass,
                             NITTY_UAH_SUBCLASS_ID);
        nitty_uah_free_data(d);
        break;
      case WM_UAHDRAWMENU: {
        UAHMENU *p = (UAHMENU *)lParam;

        nitty_uah_paint_menubar_bg(hwnd, p->hdc, d->br_menu);
        return 0;
      }
      case WM_UAHDRAWMENUITEM: {
        UAHDRAWMENUITEM *p = (UAHDRAWMENUITEM *)lParam;

        if (!d->theme)
            d->theme = OpenThemeData(hwnd, L"Menu");
        nitty_uah_paint_menu_item(p, d);
        return 0;
      }
      case WM_THEMECHANGED:
      case WM_SETTINGCHANGE:
        if (d->theme) {
            CloseThemeData(d->theme);
            d->theme = NULL;
        }
        break;
      case WM_NCACTIVATE:
      case WM_NCPAINT: {
        LRESULT lr = DefSubclassProc(hwnd, msg, wParam, lParam);

        nitty_uah_draw_menu_bottom_line(hwnd, d->br_menu);
        return lr;
      }
      default:
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static void nitty_uah_menubar_detach(HWND hwnd)
{
    if (hwnd)
        RemoveWindowSubclass(hwnd, nitty_uah_menubar_subclass,
                             NITTY_UAH_SUBCLASS_ID);
}

static void nitty_uah_menubar_attach(HWND hwnd)
{
    struct nitty_uah_data *d;

    if (!hwnd || !GetMenu(hwnd) || !nitty_windows_apps_prefer_dark())
        return;

    nitty_uah_menubar_detach(hwnd);

    d = snew(struct nitty_uah_data);
    memset(d, 0, sizeof(*d));
    d->br_menu = CreateSolidBrush(RGB(32, 32, 32));
    d->br_hot = CreateSolidBrush(RGB(60, 60, 60));
    d->br_sel = CreateSolidBrush(RGB(45, 45, 45));
    if (!d->br_menu || !d->br_hot || !d->br_sel) {
        nitty_uah_free_data(d);
        return;
    }
    SetWindowSubclass(hwnd, nitty_uah_menubar_subclass, NITTY_UAH_SUBCLASS_ID,
                      (DWORD_PTR)d);
}

void nitty_sync_preferred_app_mode(void)
{
    bool dark = nitty_windows_apps_prefer_dark();

    nitty_load_uxtheme_dark_helpers();
    if (nitty_pSetPreferredAppMode) {
        /*
         * PreferredAppMode: Default=0, AllowDark=1, ForceDark=2, ForceLight=3.
         * On many Win10/11 builds AllowDark darkens popup menus but leaves the
         * horizontal menu bar (“menu band”) light; ForceDark matches the dark
         * title bar + menu strip when Apps use dark mode. We only force when
         * the user already chose dark apps (same registry as elsewhere).
         */
        nitty_pSetPreferredAppMode(dark ? 2 : 0);
    }
    if (nitty_pFlushMenuThemes)
        nitty_pFlushMenuThemes();
    if (dark && nitty_pRefreshImmersiveColorPolicyState)
        nitty_pRefreshImmersiveColorPolicyState();
}

void nitty_allow_dark_mode_for_window(HWND hwnd, bool enable)
{
    if (!hwnd)
        return;
    nitty_load_uxtheme_dark_helpers();
    if (nitty_pAllowDarkModeForWindow)
        nitty_pAllowDarkModeForWindow(hwnd, enable ? TRUE : FALSE);
}

void nitty_apply_win11_window_chrome(HWND hwnd)
{
    BOOL dark_flag;
    DWORD backdrop;
    bool dark;

    if (!hwnd)
        return;

    nitty_uah_menubar_detach(hwnd);

    dark = nitty_windows_apps_prefer_dark();

    nitty_sync_preferred_app_mode();

    if (!nitty_dwmapi_module) {
        nitty_dwmapi_module = load_system32_dll("dwmapi.dll");
        if (nitty_dwmapi_module)
            GET_WINDOWS_FUNCTION(nitty_dwmapi_module, DwmSetWindowAttribute);
    }
    if (p_DwmSetWindowAttribute) {
        dark_flag = dark ? TRUE : FALSE;
        p_DwmSetWindowAttribute(hwnd, NITTY_DWMWA_USE_IMMERSIVE_DARK_MODE,
                                &dark_flag, sizeof(dark_flag));

        /*
         * Ask DWM to render non-client chrome (incl. menu bar strip) so it can
         * follow immersive dark mode; restore default when light.
         */
        {
            int ncrp = dark ? DWMNCRP_ENABLED : DWMNCRP_USEWINDOWSTYLE;
            p_DwmSetWindowAttribute(hwnd, NITTY_DWMWA_NCRENDERING_POLICY,
                                    &ncrp, sizeof(ncrp));
        }

        /*
         * Mica-style system backdrop (Windows 11). Silently ignored on older
         * releases that do not support this attribute.
         */
        backdrop = NITTY_DWMSBT_MAINWINDOW;
        p_DwmSetWindowAttribute(hwnd, NITTY_DWMWA_SYSTEMBACKDROP_TYPE,
                                &backdrop, sizeof(backdrop));
    }

    /*
     * Theming for the non-client vertical scrollbar (WS_VSCROLL): match
     * Explorer / Settings (rounded Win11 thumb, dark track when Apps theme
     * is dark). Configuration dialogs use the same DarkMode_Explorer hook.
     */
    nitty_load_uxtheme_dark_helpers();
    if (nitty_pAllowDarkModeForWindow)
        nitty_pAllowDarkModeForWindow(hwnd, dark ? TRUE : FALSE);
    SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : L"Explorer", NULL);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    if (dark && GetMenu(hwnd))
        nitty_uah_menubar_attach(hwnd);
    DrawMenuBar(hwnd);
    RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
}

void nitty_winfeat_init(void)
{
    nitty_sync_preferred_app_mode();
    if (winfeat_inited)
        return;
    winfeat_inited = true;
    msg_taskbar_created = RegisterWindowMessage("TaskbarCreated");
}

UINT nitty_taskbar_created_message(void)
{
    nitty_winfeat_init();
    return msg_taskbar_created;
}

int nitty_transparency_pct_to_alpha(int transparency_pct)
{
    if (transparency_pct <= 0)
        return 0;
    if (transparency_pct >= 100)
        return 1;
    return (255 * (100 - transparency_pct) + 50) / 100;
}

int nitty_alpha_to_transparency_pct(int alpha)
{
    int t;

    if (alpha <= 0)
        return 0;
    if (alpha >= 255)
        return 0;
    t = ((255 - alpha) * 100 + 127) / 255;
    if (t < 1)
        t = 1;
    if (t > 100)
        t = 100;
    return t;
}

void nitty_apply_transparency(HWND hwnd, Conf *conf)
{
    int alpha = conf_get_int(conf, CONF_nitty_window_alpha);
    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

    if (alpha <= 0 || alpha > 255) {
        if (ex & WS_EX_LAYERED) {
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex & ~(LONG_PTR)WS_EX_LAYERED);
            RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME);
        }
        return;
    }

    if (!(ex & WS_EX_LAYERED))
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, (BYTE)alpha, LWA_ALPHA);
}

bool nitty_handle_minimize_to_tray(WinGuiSeat *wgs, WPARAM wParam)
{
    HWND hwnd = wgs->term_hwnd;

    if ((wParam & 0xFFF0) != SC_MINIMIZE)
        return false;
    if (!conf_get_bool(wgs->conf, CONF_nitty_minimize_to_tray))
        return false;

    if (!wgs->nitty_tray_added) {
        NOTIFYICONDATAW *nid = &wgs->nitty_tray_nid;

        memset(nid, 0, sizeof(*nid));
        nid->cbSize = sizeof(*nid);
        nid->hWnd = hwnd;
        nid->uID = 1;
        nid->uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        nid->uCallbackMessage = NITTY_WM_TRAY;
        nid->hIcon = LoadIcon(hinst, MAKEINTRESOURCE(IDI_MAINICON));
        {
            wchar_t *tip = dup_mb_to_wc(DEFAULT_CODEPAGE, appname);

            wcsncpy(nid->szTip, tip, lenof(nid->szTip) - 1);
            nid->szTip[lenof(nid->szTip) - 1] = L'\0';
            sfree(tip);
        }
        Shell_NotifyIconW(NIM_ADD, nid);
        wgs->nitty_tray_added = true;
    }

    wgs->nitty_tray_hidden = true;
    ShowWindow(hwnd, SW_HIDE);
    return true;
}

void nitty_tray_notify(WinGuiSeat *wgs, LPARAM lParam)
{
    HWND hwnd = wgs->term_hwnd;

    if (lParam == WM_LBUTTONDBLCLK) {
        wgs->nitty_tray_hidden = false;
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
        if (wgs->nitty_tray_added) {
            Shell_NotifyIconW(NIM_DELETE, &wgs->nitty_tray_nid);
            wgs->nitty_tray_added = false;
        }
    }
}

void nitty_tray_on_taskbar_created(WinGuiSeat *wgs)
{
    HWND hwnd = wgs->term_hwnd;

    if (!wgs->nitty_tray_hidden || !wgs->nitty_tray_added)
        return;
    /* Explorer restarted: re-add icon; window stays hidden */
    Shell_NotifyIconW(NIM_DELETE, &wgs->nitty_tray_nid);
    wgs->nitty_tray_added = false;

    memset(&wgs->nitty_tray_nid, 0, sizeof(wgs->nitty_tray_nid));
    wgs->nitty_tray_nid.cbSize = sizeof(wgs->nitty_tray_nid);
    wgs->nitty_tray_nid.hWnd = hwnd;
    wgs->nitty_tray_nid.uID = 1;
    wgs->nitty_tray_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    wgs->nitty_tray_nid.uCallbackMessage = NITTY_WM_TRAY;
    wgs->nitty_tray_nid.hIcon = LoadIcon(hinst, MAKEINTRESOURCE(IDI_MAINICON));
    {
        wchar_t *tip = dup_mb_to_wc(DEFAULT_CODEPAGE, appname);

        wcsncpy(wgs->nitty_tray_nid.szTip, tip,
                lenof(wgs->nitty_tray_nid.szTip) - 1);
        wgs->nitty_tray_nid.szTip[lenof(wgs->nitty_tray_nid.szTip) - 1] = L'\0';
        sfree(tip);
    }
    Shell_NotifyIconW(NIM_ADD, &wgs->nitty_tray_nid);
    wgs->nitty_tray_added = true;
}

void nitty_tray_cleanup(WinGuiSeat *wgs)
{
    if (wgs->nitty_tray_added) {
        Shell_NotifyIconW(NIM_DELETE, &wgs->nitty_tray_nid);
        wgs->nitty_tray_added = false;
    }
    wgs->nitty_tray_hidden = false;
}
