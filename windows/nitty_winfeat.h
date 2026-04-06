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
 * Windows 10+ / 11: title bar dark/light to match Settings, Mica-style backdrop
 * when supported, and Explorer-themed non-client scrollbar (WS_VSCROLL) to
 * match Win11 / Settings. Safe no-ops on older OS or if APIs are unavailable.
 * Idempotent; call after HWND exists and on theme changes.
 */
void nitty_apply_win11_window_chrome(HWND hwnd);

void nitty_apply_transparency(HWND hwnd, Conf *conf);

bool nitty_handle_minimize_to_tray(struct WinGuiSeat *wgs, WPARAM wParam);
void nitty_tray_notify(struct WinGuiSeat *wgs, LPARAM lParam);
void nitty_tray_on_taskbar_created(struct WinGuiSeat *wgs);
void nitty_tray_cleanup(struct WinGuiSeat *wgs);

#endif
