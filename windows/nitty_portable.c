/*
 * Portable mode bootstrap for NiTTY.
 *
 * Place nitty.ini next to the executable:
 *   [NiTTY]
 *   savemode=dir
 * Optional:
 *   configdir=subfolder   (relative to exe dir, or absolute path)
 *   fileextension=ktx     (session files become name.ktx; leading dot optional)
 *
 * With savemode=dir, sessions, host keys, random seed, jumplist data, and
 * SSH host CA definitions are stored under the config directory (see code:
 * Sessions, SshHostKeys, SshHostCAs, Jumplist, putty.rnd).
 *
 * Optional [Pageant] savemode=dir + PersistKeys=1 enables Pageant to load
 * and save a list of private key file paths (Pageant\\pageant-keys.txt).
 */

#include <stdlib.h>
#include <string.h>

#include "putty.h"
#include "misc.h"
#include "nitty_portable.h"

#include <windows.h>

#define NITTY_INI "nitty.ini"
#define NITTY_SECTION "NiTTY"
#define PAGEANT_SECTION "Pageant"

static bool init_done;
static bool dir_mode;

static char *config_dir_override;

static char nitty_ini_path[MAX_PATH];
static char exe_dir[MAX_PATH];
static char base_dir[MAX_PATH];
static char pageant_subdir[MAX_PATH];
static char pageant_keysfile[MAX_PATH];
static char sessdir[MAX_PATH];
static char sshkeysdir[MAX_PATH];
static char jumplistdir[MAX_PATH];
static char cadir[MAX_PATH];
static char seedpath[MAX_PATH];
static char session_suffix[32];

static void trim_end(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                     s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = '\0';
}

void nitty_portable_set_config_directory(const char *dir)
{
    sfree(config_dir_override);
    config_dir_override = (dir && *dir) ? dupstr(dir) : NULL;
}

void nitty_portable_init(void)
{
    char inipath[MAX_PATH + 16];
    char mode[64];
    char cfg[MAX_PATH];

    if (init_done)
        return;
    init_done = true;
    dir_mode = false;
    exe_dir[0] = base_dir[0] = '\0';
    nitty_ini_path[0] = '\0';
    pageant_subdir[0] = '\0';
    pageant_keysfile[0] = '\0';
    session_suffix[0] = '\0';

    if (config_dir_override) {
        strncpy(exe_dir, config_dir_override, sizeof exe_dir - 1);
        exe_dir[sizeof exe_dir - 1] = '\0';
    } else if (!GetModuleFileName(NULL, exe_dir, lenof(exe_dir))) {
        return;
    } else {
        char *p = strrchr(exe_dir, '\\');
        if (p)
            *p = '\0';
    }

    {
        char *t = dupcat(exe_dir, "\\", NITTY_INI, NULL);
        strncpy(inipath, t, sizeof inipath - 1);
        inipath[sizeof inipath - 1] = '\0';
        sfree(t);
    }

    strncpy(nitty_ini_path, inipath, sizeof nitty_ini_path - 1);
    nitty_ini_path[sizeof nitty_ini_path - 1] = '\0';

    GetPrivateProfileStringA(NITTY_SECTION, "savemode", "", mode, sizeof mode, inipath);
    trim_end(mode);
    if (stricmp(mode, "dir") != 0)
        return;

    dir_mode = true;
    strcpy(base_dir, exe_dir);

    GetPrivateProfileStringA(NITTY_SECTION, "configdir", "", cfg, sizeof cfg, inipath);
    trim_end(cfg);
    if (*cfg) {
        if (cfg[0] == '\\' || (strlen(cfg) > 2 && cfg[1] == ':')) {
            strncpy(base_dir, cfg, sizeof base_dir - 1);
            base_dir[sizeof base_dir - 1] = '\0';
        } else {
            char *t = dupcat(exe_dir, "\\", cfg, NULL);
            strncpy(base_dir, t, sizeof base_dir - 1);
            base_dir[sizeof base_dir - 1] = '\0';
            sfree(t);
        }
    }

    GetPrivateProfileStringA(NITTY_SECTION, "fileextension", "", session_suffix,
                             sizeof session_suffix, inipath);
    trim_end(session_suffix);
    if (*session_suffix && session_suffix[0] != '.') {
        char *t = dupcat(".", session_suffix, NULL);
        strncpy(session_suffix, t, sizeof session_suffix - 1);
        session_suffix[sizeof session_suffix - 1] = '\0';
        sfree(t);
    }

    {
        char *t;

        t = dupcat(base_dir, "\\Sessions", NULL);
        strncpy(sessdir, t, sizeof sessdir - 1);
        sessdir[sizeof sessdir - 1] = '\0';
        sfree(t);

        t = dupcat(base_dir, "\\SshHostKeys", NULL);
        strncpy(sshkeysdir, t, sizeof sshkeysdir - 1);
        sshkeysdir[sizeof sshkeysdir - 1] = '\0';
        sfree(t);

        t = dupcat(base_dir, "\\Jumplist", NULL);
        strncpy(jumplistdir, t, sizeof jumplistdir - 1);
        jumplistdir[sizeof jumplistdir - 1] = '\0';
        sfree(t);

        t = dupcat(base_dir, "\\SshHostCAs", NULL);
        strncpy(cadir, t, sizeof cadir - 1);
        cadir[sizeof cadir - 1] = '\0';
        sfree(t);

        t = dupcat(base_dir, "\\putty.rnd", NULL);
        strncpy(seedpath, t, sizeof seedpath - 1);
        seedpath[sizeof seedpath - 1] = '\0';
        sfree(t);

        t = dupcat(base_dir, "\\Pageant", NULL);
        strncpy(pageant_subdir, t, sizeof pageant_subdir - 1);
        pageant_subdir[sizeof pageant_subdir - 1] = '\0';
        sfree(t);

        t = dupcat(pageant_subdir, "\\pageant-keys.txt", NULL);
        strncpy(pageant_keysfile, t, sizeof pageant_keysfile - 1);
        pageant_keysfile[sizeof pageant_keysfile - 1] = '\0';
        sfree(t);
    }

    nitty_portable_ensure_dir(sessdir);
    nitty_portable_ensure_dir(sshkeysdir);
    nitty_portable_ensure_dir(jumplistdir);
    nitty_portable_ensure_dir(cadir);
    if (dir_mode && *pageant_subdir)
        nitty_portable_ensure_dir(pageant_subdir);
}

static bool profile_truthy(const char *s)
{
    return stricmp(s, "1") == 0 || stricmp(s, "yes") == 0 ||
           stricmp(s, "true") == 0 || stricmp(s, "on") == 0;
}

bool nitty_portable_pageant_persist_keys(void)
{
    char sm[64], pk[64];

    nitty_portable_init();
    if (!dir_mode || !*nitty_ini_path)
        return false;

    GetPrivateProfileStringA(PAGEANT_SECTION, "savemode", "", sm, sizeof sm,
                             nitty_ini_path);
    trim_end(sm);
    if (stricmp(sm, "dir") != 0)
        return false;

    GetPrivateProfileStringA(PAGEANT_SECTION, "PersistKeys", "0", pk, sizeof pk,
                             nitty_ini_path);
    trim_end(pk);
    return profile_truthy(pk);
}

const char *nitty_portable_pageant_keys_path(void)
{
    nitty_portable_init();
    if (!dir_mode || !*pageant_keysfile)
        return NULL;
    return pageant_keysfile;
}

bool nitty_portable_dir_mode(void)
{
    nitty_portable_init();
    return dir_mode;
}

const char *nitty_portable_sessdir(void)
{
    return sessdir;
}

const char *nitty_portable_sshkeysdir(void)
{
    return sshkeysdir;
}

const char *nitty_portable_jumplistdir(void)
{
    return jumplistdir;
}

const char *nitty_portable_cadir(void)
{
    return cadir;
}

const char *nitty_portable_seedpath(void)
{
    return seedpath;
}

const char *nitty_portable_session_suffix(void)
{
    return session_suffix;
}

bool nitty_portable_ensure_dir(const char *dirpath)
{
    char buf[MAX_PATH];
    size_t i, len;

    if (!dirpath || !*dirpath)
        return false;
    if (GetFileAttributesA(dirpath) != INVALID_FILE_ATTRIBUTES)
        return true;

    len = strlen(dirpath);
    if (len >= sizeof buf)
        return false;
    strcpy(buf, dirpath);
    for (i = 0; i < len; i++) {
        if (buf[i] == '\\' && i > 2) {
            buf[i] = '\0';
            CreateDirectoryA(buf, NULL);
            buf[i] = '\\';
        }
    }
    return CreateDirectoryA(buf, NULL) != 0 ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}
