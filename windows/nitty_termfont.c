/*
 * NiTTY: em-based cell size (Windows Terminal multipliers) and
 * cell-filling glyphs that GDI cannot scale from the session font.
 *
 * U+2591–U+2593: 8×8 dither. U+E0B0 / U+E0B2: filled triangles.
 * Outline U+E0B1 and other PUA stay in the session font.
 */

#include <stdio.h>

#include "putty.h"
#include "dialog.h"
#include "nitty_termfont.h"

#define NITTY_CELL_SCALE_MIN 50
#define NITTY_CELL_SCALE_MAX 300

static int nitty_scale_font_px(int px, int hundredths)
{
    int v;

    if (hundredths < NITTY_CELL_SCALE_MIN)
        hundredths = NITTY_CELL_SCALE_MIN;
    if (hundredths > NITTY_CELL_SCALE_MAX)
        hundredths = NITTY_CELL_SCALE_MAX;
    v = (px * hundredths + 50) / 100;
    return v < 1 ? 1 : v;
}

void nitty_apply_font_cell_scale(Conf *conf, int em, int natural_h,
                                 int *cell_w, int *cell_h, int *text_yoff)
{
    int cw = conf_get_int(conf, CONF_nitty_cell_width);
    int lh = conf_get_int(conf, CONF_nitty_line_height);

    /*
     * 1.00 / 1.00 keeps GDI tmAveCharWidth × tmHeight (stock PuTTY).
     * Any other pair is an em multiplier, Windows Terminal style.
     * Courier New at 10px is about 0.6em wide; forcing a 1.00em cell
     * is what made MOTD text look letter-spaced.
     */
    if (cw == 100 && lh == 100) {
        *text_yoff = 0;
        return;
    }

    if (em < 1)
        em = natural_h > 0 ? natural_h : 1;
    *cell_w = nitty_scale_font_px(em, cw);
    *cell_h = nitty_scale_font_px(em, lh);
    *text_yoff = (*cell_h - natural_h) / 2;
}

static bool nitty_is_wedge(unsigned uc)
{
    return uc == 0xE0B0 || uc == 0xE0B2;
}

static bool nitty_is_shade(unsigned uc)
{
    return uc >= 0x2591 && uc <= 0x2593;
}

int nitty_glyph_draw_class(unsigned uc)
{
    if (nitty_is_wedge(uc))
        return 1;
    if (nitty_is_shade(uc))
        return 2;
    return 0;
}

int nitty_next_glyph_run(const wchar_t *text, int len, int start)
{
    int j, cls;

    if (start >= len)
        return start;
    if (start + 1 < len && IS_SURROGATE_PAIR(text[start], text[start + 1]))
        return start + 2;
    cls = nitty_glyph_draw_class((unsigned)text[start]);
    j = start + 1;
    while (j < len) {
        if (j + 1 < len && IS_SURROGATE_PAIR(text[j], text[j + 1]))
            break;
        if (nitty_glyph_draw_class((unsigned)text[j]) != cls)
            break;
        j++;
    }
    return j;
}

static void nitty_fill_shade_cells(
    HDC hdc, RECT box, int cell_w, const wchar_t *text, int len,
    COLORREF fg, COLORREF bg)
{
    static const WORD pat25[8] = {
        0x00EE, 0x00BB, 0x00EE, 0x00BB, 0x00EE, 0x00BB, 0x00EE, 0x00BB
    };
    static const WORD pat50[8] = {
        0x00AA, 0x0055, 0x00AA, 0x0055, 0x00AA, 0x0055, 0x00AA, 0x0055
    };
    static const WORD pat75[8] = {
        0x0011, 0x0044, 0x0011, 0x0044, 0x0011, 0x0044, 0x0011, 0x0044
    };
    int i;
    COLORREF oldfg, oldbg;
    POINT oldorg;

    oldfg = SetTextColor(hdc, fg);
    oldbg = SetBkColor(hdc, bg);
    SetBrushOrgEx(hdc, box.left, box.top, &oldorg);

    for (i = 0; i < len; i++) {
        const WORD *pat;
        HBITMAP bm;
        HBRUSH br, oldbr;
        RECT cell = box;

        cell.left = box.left + i * cell_w;
        cell.right = cell.left + cell_w;
        if (text[i] == 0x2591)
            pat = pat25;
        else if (text[i] == 0x2592)
            pat = pat50;
        else
            pat = pat75;

        bm = CreateBitmap(8, 8, 1, 1, pat);
        br = CreatePatternBrush(bm);
        UnrealizeObject(br);
        SetBrushOrgEx(hdc, cell.left, cell.top, NULL);
        oldbr = SelectObject(hdc, br);
        FillRect(hdc, &cell, br);
        SelectObject(hdc, oldbr);
        DeleteObject(br);
        DeleteObject(bm);
    }

    SetBrushOrgEx(hdc, oldorg.x, oldorg.y, NULL);
    SetTextColor(hdc, oldfg);
    SetBkColor(hdc, oldbg);
}

static void nitty_fill_wedge_cells(
    HDC hdc, RECT box, int cell_w, const wchar_t *text, int len,
    COLORREF fg, COLORREF bg)
{
    HBRUSH bgbr, fgbr, oldbr;
    HPEN oldpen;
    int i;

    bgbr = CreateSolidBrush(bg);
    fgbr = CreateSolidBrush(fg);
    oldpen = SelectObject(hdc, GetStockObject(NULL_PEN));
    oldbr = SelectObject(hdc, fgbr);

    for (i = 0; i < len; i++) {
        RECT cell = box;
        POINT pt[3];
        int mid;

        cell.left = box.left + i * cell_w;
        cell.right = cell.left + cell_w;
        mid = (cell.top + cell.bottom - 1) / 2;
        FillRect(hdc, &cell, bgbr);
        if (text[i] == 0xE0B2) {
            pt[0].x = cell.right - 1;
            pt[0].y = cell.top;
            pt[1].x = cell.right - 1;
            pt[1].y = cell.bottom - 1;
            pt[2].x = cell.left;
            pt[2].y = mid;
        } else {
            pt[0].x = cell.left;
            pt[0].y = cell.top;
            pt[1].x = cell.left;
            pt[1].y = cell.bottom - 1;
            pt[2].x = cell.right - 1;
            pt[2].y = mid;
        }
        SelectObject(hdc, fgbr);
        Polygon(hdc, pt, 3);
    }

    SelectObject(hdc, oldbr);
    SelectObject(hdc, oldpen);
    DeleteObject(fgbr);
    DeleteObject(bgbr);
}

bool nitty_draw_special_run(HDC hdc, RECT box, int cell_w,
                            const wchar_t *text, int len,
                            COLORREF fg, COLORREF bg)
{
    int i;

    if (len <= 0)
        return false;
    if (nitty_is_shade((unsigned)text[0])) {
        for (i = 0; i < len; i++)
            if (!nitty_is_shade((unsigned)text[i]))
                return false;
        nitty_fill_shade_cells(hdc, box, cell_w, text, len, fg, bg);
        return true;
    }
    if (nitty_is_wedge((unsigned)text[0])) {
        for (i = 0; i < len; i++)
            if (!nitty_is_wedge((unsigned)text[i]))
                return false;
        nitty_fill_wedge_cells(hdc, box, cell_w, text, len, fg, bg);
        return true;
    }
    return false;
}

static int nitty_parse_cell_scale(const char *s)
{
    double f;
    int h;

    if (sscanf(s, "%lf", &f) != 1 || f <= 0)
        return -1;
    h = (int)(f * 100.0 + 0.5);
    if (h < NITTY_CELL_SCALE_MIN)
        h = NITTY_CELL_SCALE_MIN;
    if (h > NITTY_CELL_SCALE_MAX)
        h = NITTY_CELL_SCALE_MAX;
    return h;
}

void nitty_cell_scale_handler(dlgcontrol *ctrl, dlgparam *dlg,
                              void *data, int event)
{
    Conf *conf = (Conf *)data;
    int key = ctrl->context.i;

    if (event == EVENT_REFRESH) {
        char str[16];
        sprintf(str, "%.2f", conf_get_int(conf, key) / 100.0);
        dlg_editbox_set(ctrl, dlg, str);
    } else if (event == EVENT_VALCHANGE) {
        char *s = dlg_editbox_get(ctrl, dlg);
        int h = nitty_parse_cell_scale(s);
        sfree(s);
        if (h < 0)
            return;
        conf_set_int(conf, key, h);
    }
}
