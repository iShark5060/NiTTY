/*
 * windows/utils/defaults.c: default settings that are specific to Windows.
 */

#include "putty.h"
#include "nitty_portable.h"

#include <commctrl.h>

FontSpec *platform_default_fontspec(const char *name)
{
    if (!strcmp(name, "Font"))
        return fontspec_new("Courier New", false, 10, ANSI_CHARSET);
    else
        return fontspec_new_default();
}

Filename *platform_default_filename(const char *name)
{
    if (!strcmp(name, "LogFileName")) {
        if (nitty_portable_dir_mode()) {
            const char *logs = nitty_portable_logsdir();
            char *path;
            Filename *fn;

            nitty_portable_ensure_dir(logs);
            path = dupcat(logs, "\\nitty-&H-&P.log", NULL);
            fn = filename_from_str(path);
            sfree(path);
            return fn;
        }
        return filename_from_str("putty.log");
    } else
        return filename_from_str("");
}

char *platform_default_s(const char *name)
{
    if (!strcmp(name, "SerialLine"))
        return dupstr("COM1");
    return NULL;
}

bool platform_default_b(const char *name, bool def)
{
    return def;
}

int platform_default_i(const char *name, int def)
{
    return def;
}
