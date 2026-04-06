/*
 * NiTTY: URL detection without POSIX regex (portable MSVC/MinGW).
 */

#include "nitty_url.h"

#include <ctype.h>
#include <wchar.h>

#include <shellapi.h>

#include "terminal.h"

static bool url_char(wchar_t c)
{
    if (c < 32)
        return false;
    if (c < 128)
        return (isalnum((unsigned char)c) ||
                wcschr(L"-._~:/?#[]@!$&'()*+,;=%", c) != NULL);
    return true;
}

static bool line_char_at(termline *ld, int termcols, int j, wchar_t *out)
{
    unsigned long c;

    if (j < 0 || j >= termcols || j >= ld->cols)
        return false;
    c = ld->chars[j].chr;
    if (c == UCSWIDE)
        return false;
    if (c >= 0x10000)
        *out = L'?';
    else
        *out = (wchar_t)c;
    return true;
}

static bool find_url_containing(termline *ld, int termcols, int cols,
                                int vx, wchar_t *url, size_t urllen)
{
    wchar_t line[512];
    int i, len = 0;

    for (i = 0; i < cols && len < (int)lenof(line) - 1; i++) {
        if (!line_char_at(ld, termcols, i, line + len))
            line[len++] = L' ';
        else
            len++;
    }
    line[len] = L'\0';

    for (i = 0; i < len; i++) {
        const wchar_t *schemes[] = {
            L"https://", L"http://", L"ftp://", L"mailto:", L"www."
        };
        size_t si;
        int start = -1, end = -1;
        bool iswww = false;

        for (si = 0; si < lenof(schemes); si++) {
            size_t sl = wcslen(schemes[si]);

            if ((size_t)(len - i) >= sl &&
                !wmemcmp(line + i, schemes[si], sl * sizeof(wchar_t))) {
                start = i;
                end = (int)(i + sl);
                if (si == lenof(schemes) - 1)
                    iswww = true;
                break;
            }
        }
        if (start < 0)
            continue;

        while (end < len && url_char(line[end]))
            end++;

        if (vx >= start && vx < end) {
            int k, p = 0;

            for (k = start; k < end && p < (int)urllen - 1; k++)
                url[p++] = line[k];
            url[p] = L'\0';
            if (iswww) {
                wchar_t tmp[512];
                swprintf(tmp, lenof(tmp), L"http://%ls", url);
                wcsncpy(url, tmp, urllen - 1);
                url[urllen - 1] = L'\0';
            }
            return true;
        }
        i = end - 1;
    }
    return false;
}

bool nitty_url_open_at(Terminal *term, Conf *conf, int vx, int vy)
{
    termline *ld;
    int absy;
    wchar_t url[512];

    if (!conf_get_bool(conf, CONF_nitty_url_enable))
        return false;

    if (conf_get_bool(conf, CONF_nitty_url_ctrl_click))
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0)
            return false;

    if (vy < 0 || vy >= term->rows || vx < 0 || vx >= term->cols)
        return false;

    absy = vy + term->disptop;
    ld = term_get_line(term, absy);
    if (!ld)
        return false;

    if (!find_url_containing(ld, term->cols, term->cols, vx, url, lenof(url))) {
        term_release_line(ld);
        return false;
    }
    term_release_line(ld);

    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
    return true;
}

bool nitty_url_set_cursor(Terminal *term, Conf *conf, int vx, int vy,
                          bool ctrl_pressed)
{
    termline *ld;
    int absy;
    wchar_t url[512];

    if (!conf_get_bool(conf, CONF_nitty_url_enable))
        return false;
    if (conf_get_bool(conf, CONF_nitty_url_ctrl_click) && !ctrl_pressed)
        return false;
    if (vy < 0 || vy >= term->rows || vx < 0 || vx >= term->cols)
        return false;

    absy = vy + term->disptop;
    ld = term_get_line(term, absy);
    if (!ld)
        return false;

    if (!find_url_containing(ld, term->cols, term->cols, vx, url, lenof(url))) {
        term_release_line(ld);
        return false;
    }
    term_release_line(ld);

    SetCursor(LoadCursor(NULL, IDC_HAND));
    return true;
}
