/*
 * NiTTY: layered-window transparency and minimize-to-tray.
 */

#include "nitty_winfeat.h"

#include "win-gui-seat.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>

#include "putty-rc.h"

#pragma comment(lib, "uxtheme.lib")

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

void nitty_apply_win11_window_chrome(HWND hwnd)
{
    BOOL dark_flag;
    DWORD backdrop;
    bool dark;

    if (!hwnd)
        return;

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
