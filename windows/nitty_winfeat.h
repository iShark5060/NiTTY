/*
 * NiTTY: layered-window transparency and minimize-to-tray.
 */

#ifndef NITTY_WINFEAT_H
#define NITTY_WINFEAT_H

#include "putty.h"

#define NITTY_WM_TRAY (WM_APP + 25)

struct WinGuiSeat;

void nitty_winfeat_init(void);
UINT nitty_taskbar_created_message(void);

/*
 * Process-wide dark/light for Win32 menus (menu bar + dropdowns): must run
 * before the first top-level window/menu is created (e.g. from WinMain after
 * init_common_controls). Safe to call again on theme change; also invoked
 * from nitty_apply_win11_window_chrome.
 */
void nitty_sync_preferred_app_mode(void);

/*
 * Per-window hook (uxtheme #133): use dark-mode scrollbars / NC chrome for this
 * HWND. Call on multiline edits etc. when stripping visual styles but still
 * using the system scrollbar (NiTTYgen public key, About readonly text).
 */
void nitty_allow_dark_mode_for_window(HWND hwnd, bool enable);

/*
 * Windows 10+ / 11: title bar dark/light to match Settings, Mica-style backdrop
 * when supported, and Explorer-themed non-client scrollbar (WS_VSCROLL) to
 * match Win11 / Settings. Safe no-ops on older OS or if APIs are unavailable.
 * Idempotent; call after HWND exists and on theme changes.
 */
void nitty_apply_win11_window_chrome(HWND hwnd);

void nitty_apply_transparency(HWND hwnd, Conf *conf);

/*
 * Shared mapping for CONF_nitty_window_alpha (0 = layered off, 1-255 = LWA alpha)
 * and UI "transparency %" (0 = off, 1-100 = more transparent). Used by the
 * session config editbox and the Transparency context submenu.
 */
int nitty_transparency_pct_to_alpha(int transparency_pct);
int nitty_alpha_to_transparency_pct(int alpha);

bool nitty_handle_minimize_to_tray(struct WinGuiSeat *wgs, WPARAM wParam);
void nitty_tray_notify(struct WinGuiSeat *wgs, LPARAM lParam);
void nitty_tray_on_taskbar_created(struct WinGuiSeat *wgs);
void nitty_tray_cleanup(struct WinGuiSeat *wgs);

#endif
