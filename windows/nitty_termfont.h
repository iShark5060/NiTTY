/*
 * NiTTY: terminal cell metrics and Powerline/shade drawing.
 */

#ifndef NITTY_TERMFONT_H
#define NITTY_TERMFONT_H

#include <windows.h>
#include "putty.h"

struct dlgcontrol;
struct dlgparam;

void nitty_apply_font_cell_scale(Conf *conf, int em, int natural_h,
                                 int *cell_w, int *cell_h, int *text_yoff);
int nitty_glyph_draw_class(unsigned uc);
int nitty_next_glyph_run(const wchar_t *text, int len, int start);
bool nitty_draw_special_run(HDC hdc, RECT box, int cell_w,
                            const wchar_t *text, int len,
                            COLORREF fg, COLORREF bg);
void nitty_cell_scale_handler(struct dlgcontrol *ctrl, struct dlgparam *dlg,
                              void *data, int event);

#endif
