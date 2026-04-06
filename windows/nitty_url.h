/*
 * NiTTY: detect URLs on a terminal line and open them (KiTTY-style).
 */

#ifndef NITTY_URL_H
#define NITTY_URL_H

#include "putty.h"

struct Terminal;
struct Conf;

/*
 * If the cell (vx, vy) lies inside a detected URL, open it and return true.
 * vy/vx are viewport coordinates (same as term_mouse).
 */
bool nitty_url_open_at(Terminal *term, Conf *conf, int vx, int vy);

/*
 * If the pointer is over a URL (and ctrl is held when required), use the
 * hand cursor; otherwise return false so the caller can set the default
 * arrow.
 */
bool nitty_url_set_cursor(Terminal *term, Conf *conf, int vx, int vy,
                          bool ctrl_pressed);

#endif
