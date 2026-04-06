/*
 * NiTTY portable mode: optional file-based config layout (KiTTY-style).
 * Enabled when nitty.ini beside the executable contains [NiTTY] savemode=dir.
 */

#ifndef NITTY_PORTABLE_H
#define NITTY_PORTABLE_H

#include "defs.h"

void nitty_portable_init(void);
/*
 * If non-NULL, portable bootstrap reads nitty.ini from this directory
 * (the folder containing nitty.ini), instead of the current executable's
 * directory. Call before the first nitty_portable_init() (e.g. Pageant
 * passes the directory of nitty.exe).
 */
void nitty_portable_set_config_directory(const char *dir);
bool nitty_portable_dir_mode(void);

const char *nitty_portable_sessdir(void);
const char *nitty_portable_sshkeysdir(void);
const char *nitty_portable_jumplistdir(void);
const char *nitty_portable_cadir(void);
const char *nitty_portable_seedpath(void);
const char *nitty_portable_session_suffix(void);

bool nitty_portable_ensure_dir(const char *dirpath);

/*
 * Pageant portable key list (requires [NiTTY] savemode=dir already).
 * In nitty.ini, add:
 *   [Pageant]
 *   savemode=dir
 *   PersistKeys=1
 * Key paths are stored as UTF-8 under <configdir>\\Pageant\\pageant-keys.txt
 */
bool nitty_portable_pageant_persist_keys(void);
const char *nitty_portable_pageant_keys_path(void);

#endif
