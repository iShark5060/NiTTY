/*
 * Windows 11–style colors for the NiTTY configuration dialogs, following
 * the user's light/dark preference (Settings > Personalization > Colors).
 */

#ifndef NITTY_CONFIG_THEME_H
#define NITTY_CONFIG_THEME_H

#include <windows.h>

/* Settings → Personalization → Colors → Apps (same as immersive dark chrome). */
bool nitty_config_theme_apps_use_dark(void);

typedef struct nitty_config_theme {
    bool inited;
    bool dark;
    HBRUSH br_dialog;
    HBRUSH br_edit;
    HBRUSH br_list;
    HBRUSH br_btn;
    COLORREF clr_text;
    COLORREF clr_text_dim;
    COLORREF clr_edit_text;
    COLORREF clr_accent;
} nitty_config_theme;

void nitty_config_theme_init(nitty_config_theme *t);
void nitty_config_theme_free(nitty_config_theme *t);
void nitty_config_theme_refresh(nitty_config_theme *t, HWND treeview, HWND dlg);

void nitty_config_theme_apply_tree(HWND treeview, const nitty_config_theme *t);
void nitty_config_theme_apply_children(HWND dlg, const nitty_config_theme *t);

/*
 * Handle WM_CTLCOLOR* for the config dialog. Returns TRUE if *out is set.
 */
bool nitty_config_theme_ctlcolor(nitty_config_theme *t, HWND dlg, UINT msg,
                                 WPARAM wParam, LPARAM lParam, LRESULT *out);

/*
 * Config dialog: TreeView NM_CUSTOMDRAW (selection/hover) for dark mode.
 * Returns true if *out should be returned from the dialog proc.
 */
bool nitty_config_theme_tree_notify(nitty_config_theme *t, LPARAM lParam,
                                    LRESULT *out);

#endif
