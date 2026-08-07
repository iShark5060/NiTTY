/*
 * storage.c: Windows-specific implementation of the interface
 * defined in storage.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include "putty.h"
#include "storage.h"
#include "nitty_portable.h"
#include "nitty_sessfile.h"
#include "nitty_sesspass.h"

#include <shlobj.h>
#ifndef CSIDL_APPDATA
#define CSIDL_APPDATA 0x001a
#endif
#ifndef CSIDL_LOCAL_APPDATA
#define CSIDL_LOCAL_APPDATA 0x001c
#endif

#include <windows.h>

static bool nitty_file_exists(const char *path)
{
    return path && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static const char *const reg_jumplist_key = PUTTY_REG_POS "\\Jumplist";
static const char *const reg_jumplist_value = "Recent sessions";
static const char *const puttystr = PUTTY_REG_POS "\\Sessions";
static const char *const host_ca_key = PUTTY_REG_POS "\\SshHostCAs";

static bool tried_shgetfolderpath = false;
static HMODULE shell32_module = NULL;
DECL_WINDOWS_FUNCTION(static, HRESULT, SHGetFolderPathA,
                      (HWND, int, HANDLE, DWORD, LPSTR));

struct settings_w {
    HKEY sesskey;
    NittySettingsList nitty_list;
    char *nitty_basefile;
    char *nitty_sessname;
};

settings_w *open_settings_w(const char *sessionname, char **errmsg)
{
    *errmsg = NULL;

    if (!sessionname || !*sessionname)
        sessionname = "Default Settings";

    nitty_portable_init();
    if (nitty_portable_dir_mode()) {
        char munged[4096];
        char *basefile, *sessdir;
        settings_w *handle = snew(settings_w);

        handle->sesskey = NULL;
        handle->nitty_list = nitty_settings_list_new();
        handle->nitty_sessname = dupstr(sessionname);
        nitty_mungestr(sessionname, munged);
        basefile = dupcat(munged, nitty_portable_session_suffix(), NULL);
        handle->nitty_basefile = basefile;
        sessdir = dupstr(nitty_portable_sessdir());
        nitty_portable_ensure_dir(sessdir);
        sfree(sessdir);
        return handle;
    }

    {
        strbuf *sb = strbuf_new();
        HKEY sesskey;

        escape_registry_key(sessionname, sb);

        sesskey = create_regkey(HKEY_CURRENT_USER, puttystr, sb->s);
        if (!sesskey) {
            *errmsg = dupprintf("Unable to create registry key\n"
                                "HKEY_CURRENT_USER\\%s\\%s", puttystr, sb->s);
            strbuf_free(sb);
            return NULL;
        }
        strbuf_free(sb);

        {
            settings_w *handle = snew(settings_w);

            handle->sesskey = sesskey;
            handle->nitty_list = NULL;
            handle->nitty_basefile = NULL;
            handle->nitty_sessname = dupstr(sessionname);
            return handle;
        }
    }
}

void write_setting_s(settings_w *handle, const char *key, const char *value)
{
    char *enc = NULL;
    const char *wval = value;

    if (!handle)
        return;
    if (handle->nitty_sessname && key && !strcmp(key, "Password") &&
        value && *value) {
        enc = nitty_sesspass_encrypt_for_save(handle->nitty_sessname, value);
        wval = enc;
    }
    if (handle->nitty_list) {
        nitty_settings_add(handle->nitty_list, key, wval);
        sfree(enc);
        return;
    }
    put_reg_sz(handle->sesskey, key, wval);
    sfree(enc);
}

void write_setting_i(settings_w *handle, const char *key, int value)
{
    char buf[32];

    if (!handle)
        return;
    if (handle->nitty_list) {
        sprintf(buf, "%d", value);
        nitty_settings_add(handle->nitty_list, key, buf);
        return;
    }
    put_reg_dword(handle->sesskey, key, value);
}

void close_settings_w(settings_w *handle)
{
    if (!handle)
        return;
    if (handle->nitty_list) {
        char *path;

        nitty_portable_init();
        path = dupcat(nitty_portable_sessdir(), "\\", handle->nitty_basefile, NULL);
        nitty_settings_save_file(handle->nitty_list, path);
        sfree(path);
        nitty_settings_list_free(handle->nitty_list);
        sfree(handle->nitty_basefile);
        sfree(handle->nitty_sessname);
        sfree(handle);
        return;
    }
    close_regkey(handle->sesskey);
    sfree(handle->nitty_sessname);
    sfree(handle);
}

struct settings_r {
    HKEY sesskey;
    NittySettingsList nitty_list;
    char *nitty_sessname;
};

settings_r *open_settings_r(const char *sessionname)
{
    if (!sessionname || !*sessionname)
        sessionname = "Default Settings";

    nitty_portable_init();
    if (nitty_portable_dir_mode()) {
        char munged[4096];
        char *path;
        bool exists;
        NittySettingsList list = nitty_settings_list_new();
        settings_r *handle;

        nitty_mungestr(sessionname, munged);
        path = dupcat(nitty_portable_sessdir(), "\\", munged,
                      nitty_portable_session_suffix(), NULL);
        exists = nitty_file_exists(path);
        if (exists)
            nitty_settings_load_file(list, path);

        if (!exists && strcmp(sessionname, "Default Settings")) {
            nitty_settings_list_free(list);
            sfree(path);
            return NULL;
        }
        sfree(path);

        handle = snew(settings_r);
        handle->sesskey = NULL;
        handle->nitty_list = list;
        handle->nitty_sessname = dupstr(sessionname);
        return handle;
    }

    {
        strbuf *sb = strbuf_new();
        HKEY sesskey;

        escape_registry_key(sessionname, sb);
        sesskey = open_regkey_ro(HKEY_CURRENT_USER, puttystr, sb->s);
        strbuf_free(sb);

        if (!sesskey)
            return NULL;

        {
            settings_r *handle = snew(settings_r);

            handle->sesskey = sesskey;
            handle->nitty_list = NULL;
            handle->nitty_sessname = dupstr(sessionname);
            return handle;
        }
    }
}

char *read_setting_s(settings_r *handle, const char *key)
{
    char *raw, *out;

    if (!handle)
        return NULL;
    if (handle->nitty_list)
        raw = nitty_settings_get_str_dup(handle->nitty_list, key);
    else
        raw = get_reg_sz(handle->sesskey, key);

    if (!raw)
        return NULL;
    if (handle->nitty_sessname && key && !strcmp(key, "Password") && *raw) {
        out = nitty_sesspass_decrypt_after_load(handle->nitty_sessname, raw);
        sfree(raw);
        return out;
    }
    return raw;
}

int read_setting_i(settings_r *handle, const char *key, int defvalue)
{
    DWORD val;

    if (!handle)
        return defvalue;
    if (handle->nitty_list)
        return nitty_settings_get_int(handle->nitty_list, key, defvalue);
    if (!get_reg_dword(handle->sesskey, key, &val))
        return defvalue;
    return val;
}

FontSpec *read_setting_fontspec(settings_r *handle, const char *name)
{
    char *settingname;
    char *fontname;
    FontSpec *ret;
    int isbold, height, charset;

    fontname = read_setting_s(handle, name);
    if (!fontname)
        return NULL;

    settingname = dupcat(name, "IsBold");
    isbold = read_setting_i(handle, settingname, -1);
    sfree(settingname);
    if (isbold == -1) {
        sfree(fontname);
        return NULL;
    }

    settingname = dupcat(name, "CharSet");
    charset = read_setting_i(handle, settingname, -1);
    sfree(settingname);
    if (charset == -1) {
        sfree(fontname);
        return NULL;
    }

    settingname = dupcat(name, "Height");
    height = read_setting_i(handle, settingname, INT_MIN);
    sfree(settingname);
    if (height == INT_MIN) {
        sfree(fontname);
        return NULL;
    }

    ret = fontspec_new(fontname, isbold, height, charset);
    sfree(fontname);
    return ret;
}

void write_setting_fontspec(settings_w *handle,
                            const char *name, FontSpec *font)
{
    char *settingname;

    write_setting_s(handle, name, font->name);
    settingname = dupcat(name, "IsBold");
    write_setting_i(handle, settingname, font->isbold);
    sfree(settingname);
    settingname = dupcat(name, "CharSet");
    write_setting_i(handle, settingname, font->charset);
    sfree(settingname);
    settingname = dupcat(name, "Height");
    write_setting_i(handle, settingname, font->height);
    sfree(settingname);
}

Filename *read_setting_filename(settings_r *handle, const char *name)
{
    char *tmp = read_setting_s(handle, name);
    if (tmp) {
        Filename *ret;
        if (!strcmp(name, "LogFileName")) {
            char *resolved = nitty_portable_resolve_log_filename(tmp);
            sfree(tmp);
            tmp = resolved;
        }
        ret = filename_from_str(tmp);
        sfree(tmp);
        return ret;
    } else
        return NULL;
}

void write_setting_filename(settings_w *handle,
                            const char *name, Filename *result)
{
    /*
     * When saving a session involving a Filename, we use the 'cpath'
     * member of the Filename structure, because otherwise we break
     * backwards compatibility with existing saved sessions.
     *
     * This means that 'exotic' filenames - those including Unicode
     * characters outside the host system's CP_ACP default code page -
     * cannot be represented faithfully, and saving and reloading a
     * Conf including one will break it.
     *
     * This can't be fixed without breaking backwards compatibility,
     * and if we're going to break compatibility then we should break
     * it good and hard (the Nanny Ogg principle), and devise a
     * completely fresh storage representation that fixes as many
     * other legacy problems as possible at the same time.
     */
    write_setting_s(handle, name, result->cpath); /* FIXME */
}

void close_settings_r(settings_r *handle)
{
    if (!handle)
        return;
    if (handle->nitty_list) {
        nitty_settings_list_free(handle->nitty_list);
        sfree(handle->nitty_sessname);
        sfree(handle);
        return;
    }
    close_regkey(handle->sesskey);
    sfree(handle->nitty_sessname);
    sfree(handle);
}

void del_settings(const char *sessionname)
{
    nitty_portable_init();
    if (nitty_portable_dir_mode()) {
        char munged[4096];
        char *path;

        nitty_mungestr(sessionname, munged);
        path = dupcat(nitty_portable_sessdir(), "\\", munged,
                      nitty_portable_session_suffix(), NULL);
        DeleteFileA(path);
        sfree(path);
        remove_session_from_jumplist(sessionname);
        return;
    }

    {
        HKEY rkey = open_regkey_rw(HKEY_CURRENT_USER, puttystr);

        if (!rkey)
            return;

        {
            strbuf *sb = strbuf_new();

            escape_registry_key(sessionname, sb);
            del_regkey(rkey, sb->s);
            strbuf_free(sb);
        }

        close_regkey(rkey);

        remove_session_from_jumplist(sessionname);
    }
}

struct settings_e {
    HKEY key;
    int i;
    bool nitty;
    HANDLE nitty_find;
    WIN32_FIND_DATAA nitty_fd;
    bool nitty_first;
};

settings_e *enum_settings_start(void)
{
    nitty_portable_init();
    if (nitty_portable_dir_mode()) {
        char *pattern;
        settings_e *e = snew(settings_e);

        nitty_portable_ensure_dir(nitty_portable_sessdir());
        pattern = dupcat(nitty_portable_sessdir(), "\\*", NULL);
        e->nitty = true;
        e->nitty_first = true;
        e->key = NULL;
        e->i = 0;
        e->nitty_find = FindFirstFileA(pattern, &e->nitty_fd);
        sfree(pattern);
        if (e->nitty_find == INVALID_HANDLE_VALUE) {
            sfree(e);
            return NULL;
        }
        return e;
    }

    {
        HKEY key = open_regkey_ro(HKEY_CURRENT_USER, puttystr);
        settings_e *e;

        if (!key)
            return NULL;

        e = snew(settings_e);
        e->key = key;
        e->i = 0;
        e->nitty = false;
        e->nitty_find = INVALID_HANDLE_VALUE;
        return e;
    }
}

bool enum_settings_next(settings_e *e, strbuf *sb)
{
    if (e->nitty) {
        const char *suf = nitty_portable_session_suffix();
        size_t suflen = strlen(suf);

        while (1) {
            BOOL ok;
            char dec[512];
            char *namebuf;

            if (!e->nitty_first) {
                ok = FindNextFileA(e->nitty_find, &e->nitty_fd);
                if (!ok)
                    return false;
            } else {
                e->nitty_first = false;
            }

            if (e->nitty_fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;
            if (!strcmp(e->nitty_fd.cFileName, ".") ||
                !strcmp(e->nitty_fd.cFileName, ".."))
                continue;

            namebuf = dupstr(e->nitty_fd.cFileName);
            if (suflen > 0 && strlen(namebuf) > suflen &&
                !strcmp(namebuf + strlen(namebuf) - suflen, suf))
                namebuf[strlen(namebuf) - suflen] = '\0';

            nitty_unmungestr(namebuf, dec, sizeof dec);
            sfree(namebuf);

            /*
             * Append this session name like registry and Unix dir enumeration:
             * get_sesslist() chains enum_settings_next + put_byte('\\0').
             */
            put_datapl(sb, ptrlen_from_asciz(dec));
            return true;
        }
    }

    {
        char *name = enum_regkey(e->key, e->i);

        if (!name)
            return false;

        unescape_registry_key(name, sb);
        sfree(name);
        e->i++;
        return true;
    }
}

void enum_settings_finish(settings_e *e)
{
    if (e->nitty) {
        FindClose(e->nitty_find);
        sfree(e);
        return;
    }
    close_regkey(e->key);
    sfree(e);
}

static void hostkey_regname(strbuf *sb, const char *hostname,
                            int port, const char *keytype)
{
    put_fmt(sb, "%s@%d:", keytype, port);
    escape_registry_key(hostname, sb);
}

static int nitty_check_host_key_file(strbuf *regname, const char *key)
{
    char *packed;
    char *path;
    FILE *fp;
    char *buf;
    long sz;
    int cmp;

    packed = snewn(regname->len * 3 + 16, char);
    nitty_packstr(regname->s, packed);
    path = dupcat(nitty_portable_sshkeysdir(), "\\", packed, NULL);
    sfree(packed);
    if (!nitty_file_exists(path)) {
        sfree(path);
        return 1;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        sfree(path);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        sfree(path);
        return 1;
    }
    fseek(fp, 0, SEEK_SET);
    buf = snewn(sz + 1, char);
    fread(buf, 1, (size_t)sz, fp);
    buf[sz] = '\0';
    fclose(fp);
    sfree(path);

    cmp = strcmp(buf, key);
    sfree(buf);
    if (cmp)
        return 2;
    return 0;
}

static void nitty_store_host_key_file(strbuf *regname, const char *key)
{
    char *packed;
    char *path;
    FILE *fp;

    packed = snewn(regname->len * 3 + 16, char);
    nitty_packstr(regname->s, packed);
    nitty_portable_ensure_dir(nitty_portable_sshkeysdir());
    path = dupcat(nitty_portable_sshkeysdir(), "\\", packed, NULL);
    sfree(packed);
    fp = fopen(path, "wb");
    if (fp) {
        fwrite(key, 1, strlen(key), fp);
        fclose(fp);
    }
    sfree(path);
}

int check_stored_host_key(const char *hostname, int port,
                          const char *keytype, const char *key)
{
    /*
     * Read a saved key in from the registry and see what it says.
     */
    strbuf *regname = strbuf_new();
    hostkey_regname(regname, hostname, port, keytype);

    nitty_portable_init();
    if (nitty_portable_dir_mode()) {
        int r = nitty_check_host_key_file(regname, key);

        strbuf_free(regname);
        return r;
    }

    {
        HKEY rkey = open_regkey_ro(HKEY_CURRENT_USER,
                                   PUTTY_REG_POS "\\SshHostKeys");
    if (!rkey) {
        strbuf_free(regname);
        return 1;                      /* key does not exist in registry */
    }

    char *otherstr = get_reg_sz(rkey, regname->s);
    if (!otherstr && !strcmp(keytype, "rsa")) {
        /*
         * Key didn't exist. If the key type is RSA, we'll try
         * another trick, which is to look up the _old_ key format
         * under just the hostname and translate that.
         */
        char *justhost = regname->s + 1 + strcspn(regname->s, ":");
        char *oldstyle = get_reg_sz(rkey, justhost);

        if (oldstyle) {
            /*
             * The old format is two old-style bignums separated by
             * a slash. An old-style bignum is made of groups of
             * four hex digits: digits are ordered in sensible
             * (most to least significant) order within each group,
             * but groups are ordered in silly (least to most)
             * order within the bignum. The new format is two
             * ordinary C-format hex numbers (0xABCDEFG...XYZ, with
             * A nonzero except in the special case 0x0, which
             * doesn't appear anyway in RSA keys) separated by a
             * comma. All hex digits are lowercase in both formats.
             */
            strbuf *new = strbuf_new();
            const char *q = oldstyle;
            int i, j;

            for (i = 0; i < 2; i++) {
                int ndigits, nwords;
                put_datapl(new, PTRLEN_LITERAL("0x"));
                ndigits = strcspn(q, "/");      /* find / or end of string */
                nwords = ndigits / 4;
                /* now trim ndigits to remove leading zeros */
                while (q[(ndigits - 1) ^ 3] == '0' && ndigits > 1)
                    ndigits--;
                /* now move digits over to new string */
                for (j = ndigits; j-- > 0 ;)
                    put_byte(new, q[j ^ 3]);
                q += nwords * 4;
                if (*q) {
                    q++;                 /* eat the slash */
                    put_byte(new, ',');  /* add a comma */
                }
            }

            /*
             * Now _if_ this key matches, we'll enter it in the new
             * format. If not, we'll assume something odd went
             * wrong, and hyper-cautiously do nothing.
             */
            if (!strcmp(new->s, key)) {
                put_reg_sz(rkey, regname->s, new->s);
                otherstr = strbuf_to_str(new);
            } else {
                strbuf_free(new);
            }
        }

        sfree(oldstyle);
    }

        close_regkey(rkey);

        {
            bool key_absent = !otherstr;
            int compare = otherstr ? strcmp(otherstr, key) : -1;

            sfree(otherstr);
            strbuf_free(regname);

            if (key_absent)
                return 1;              /* key does not exist in registry */
            else if (compare)
                return 2;              /* key is different in registry */
            else
                return 0;              /* key matched OK in registry */
        }
    }
}

bool have_ssh_host_key(const char *hostname, int port,
                       const char *keytype)
{
    /*
     * If we have a host key, check_stored_host_key will return 0 or 2.
     * If we don't have one, it'll return 1.
     */
    return check_stored_host_key(hostname, port, keytype, "") != 1;
}

void store_host_key(Seat *seat, const char *hostname, int port,
                    const char *keytype, const char *key)
{
    strbuf *regname = strbuf_new();
    hostkey_regname(regname, hostname, port, keytype);

    nitty_portable_init();
    if (nitty_portable_dir_mode()) {
        nitty_store_host_key_file(regname, key);
        strbuf_free(regname);
        return;
    }

    {
        HKEY rkey = create_regkey(HKEY_CURRENT_USER,
                                  PUTTY_REG_POS "\\SshHostKeys");

        if (rkey) {
            put_reg_sz(rkey, regname->s, key);
            close_regkey(rkey);
        } /* else key does not exist in registry */

        strbuf_free(regname);
    }
    (void)seat;
}

/*
 * Filename encoding matches Unix PuTTY (~/.config/putty/sshhostcas): safe
 * characters opt-in, others %-encoded (see unix/storage.c make_session_filename).
 */
static void nitty_hostca_make_filename(const char *in, strbuf *out)
{
    static const char hex[16] = "0123456789ABCDEF";

    if (!in || !*in)
        in = "unnamed";

    while (*in) {
        if (*in!='+' && *in!='-' && *in!='.' && *in!='@' && *in!='_' &&
            !(*in >= '0' && *in <= '9') &&
            !(*in >= 'A' && *in <= 'Z') &&
            !(*in >= 'a' && *in <= 'z')) {
            put_byte(out, '%');
            put_byte(out, hex[((unsigned char) *in) >> 4]);
            put_byte(out, hex[((unsigned char) *in) & 15]);
        } else
            put_byte(out, *in);
        in++;
    }
}

static void nitty_hostca_decode_filename(const char *in, strbuf *out)
{
    while (*in) {
        if (*in == '%' && in[1] && in[2]) {
            int i, j;

            i = in[1] - '0';
            i -= (i > 9 ? 7 : 0);
            j = in[2] - '0';
            j -= (j > 9 ? 7 : 0);

            put_byte(out, (i << 4) + j);
            in += 3;
        } else {
            put_byte(out, *in++);
        }
    }
}

static char *nitty_hostca_filepath(const char *name)
{
    strbuf *sb = strbuf_new();

    put_fmt(sb, "%s\\", nitty_portable_cadir());
    nitty_hostca_make_filename(name, sb);
    return strbuf_to_str(sb);
}

static host_ca *host_ca_load_fp(FILE *fp, const char *name)
{
    host_ca *hca = host_ca_new();
    hca->name = dupstr(name);

    char *line;
    CertExprBuilder *eb = NULL;

    while ((line = fgetline(fp))) {
        char *value = strchr(line, '=');

        if (!value) {
            sfree(line);
            continue;
        }
        *value++ = '\0';
        value[strcspn(value, "\r\n")] = '\0';

        if (!strcmp(line, "PublicKey")) {
            hca->ca_public_key = base64_decode_sb(ptrlen_from_asciz(value));
        } else if (!strcmp(line, "MatchHosts")) {
            if (!eb)
                eb = cert_expr_builder_new();
            cert_expr_builder_add(eb, value);
        } else if (!strcmp(line, "Validity")) {
            hca->validity_expression = strbuf_to_str(
                percent_decode_sb(ptrlen_from_asciz(value)));
        } else if (!strcmp(line, "PermitRSASHA1")) {
            hca->opts.permit_rsa_sha1 = atoi(value);
        } else if (!strcmp(line, "PermitRSASHA256")) {
            hca->opts.permit_rsa_sha256 = atoi(value);
        } else if (!strcmp(line, "PermitRSASHA512")) {
            hca->opts.permit_rsa_sha512 = atoi(value);
        }

        sfree(line);
    }

    if (eb) {
        if (!hca->validity_expression) {
            hca->validity_expression = cert_expr_expression(eb);
        }
        cert_expr_builder_free(eb);
    }

    return hca;
}

struct host_ca_enum {
    bool nitty_file;
    HKEY key;
    int i;
    HANDLE find_handle;
    WIN32_FIND_DATAA find_data;
    bool find_first;
};

host_ca_enum *enum_host_ca_start(void)
{
    nitty_portable_init();
    if (nitty_portable_dir_mode()) {
        char *pattern;
        host_ca_enum *e = snew(host_ca_enum);

        nitty_portable_ensure_dir(nitty_portable_cadir());
        pattern = dupcat(nitty_portable_cadir(), "\\*", NULL);
        e->nitty_file = true;
        e->key = NULL;
        e->i = 0;
        e->find_first = true;
        e->find_handle = FindFirstFileA(pattern, &e->find_data);
        sfree(pattern);
        if (e->find_handle == INVALID_HANDLE_VALUE) {
            sfree(e);
            return NULL;
        }
        return e;
    }

    {
        host_ca_enum *e;
        HKEY key;

        if (!(key = open_regkey_ro(HKEY_CURRENT_USER, host_ca_key)))
            return NULL;

        e = snew(host_ca_enum);
        e->nitty_file = false;
        e->key = key;
        e->i = 0;
        e->find_handle = INVALID_HANDLE_VALUE;
        return e;
    }
}

bool enum_host_ca_next(host_ca_enum *e, strbuf *sb)
{
    if (e->nitty_file) {
        while (1) {
            BOOL ok;

            if (!e->find_first) {
                ok = FindNextFileA(e->find_handle, &e->find_data);
                if (!ok)
                    return false;
            } else {
                e->find_first = false;
            }

            if (e->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;
            if (!strcmp(e->find_data.cFileName, ".") ||
                !strcmp(e->find_data.cFileName, ".."))
                continue;

            strbuf_clear(sb);
            nitty_hostca_decode_filename(e->find_data.cFileName, sb);
            return true;
        }
    }

    {
        char *regbuf = enum_regkey(e->key, e->i);

        if (!regbuf)
            return false;

        unescape_registry_key(regbuf, sb);
        sfree(regbuf);
        e->i++;
        return true;
    }
}

void enum_host_ca_finish(host_ca_enum *e)
{
    if (e->nitty_file)
        FindClose(e->find_handle);
    else
        close_regkey(e->key);
    sfree(e);
}

host_ca *host_ca_load(const char *name)
{
    nitty_portable_init();
    if (nitty_portable_dir_mode()) {
        char *path = nitty_hostca_filepath(name);
        FILE *fp = fopen(path, "r");
        host_ca *hca;

        sfree(path);
        if (!fp)
            return NULL;

        hca = host_ca_load_fp(fp, name);
        fclose(fp);
        return hca;
    }

    {
        strbuf *sb;
        const char *s;

        sb = strbuf_new();
        escape_registry_key(name, sb);
        HKEY rkey = open_regkey_ro(HKEY_CURRENT_USER, host_ca_key, sb->s);
        strbuf_free(sb);

        if (!rkey)
            return NULL;

        {
            host_ca *hca = host_ca_new();
            hca->name = dupstr(name);

            DWORD val;

            if ((s = get_reg_sz(rkey, "PublicKey")) != NULL)
                hca->ca_public_key = base64_decode_sb(ptrlen_from_asciz(s));

            if ((s = get_reg_sz(rkey, "Validity")) != NULL) {
                hca->validity_expression = strbuf_to_str(
                    percent_decode_sb(ptrlen_from_asciz(s)));
            } else if ((sb = get_reg_multi_sz(rkey, "MatchHosts")) != NULL) {
                BinarySource src[1];
                BinarySource_BARE_INIT_PL(src, ptrlen_from_strbuf(sb));
                CertExprBuilder *eb = cert_expr_builder_new();

                const char *wc;
                while (wc = get_asciz(src), !get_err(src))
                    cert_expr_builder_add(eb, wc);

                hca->validity_expression = cert_expr_expression(eb);
                cert_expr_builder_free(eb);
                strbuf_free(sb);
            }

            if (get_reg_dword(rkey, "PermitRSASHA1", &val))
                hca->opts.permit_rsa_sha1 = val;
            if (get_reg_dword(rkey, "PermitRSASHA256", &val))
                hca->opts.permit_rsa_sha256 = val;
            if (get_reg_dword(rkey, "PermitRSASHA512", &val))
                hca->opts.permit_rsa_sha512 = val;

            close_regkey(rkey);
            return hca;
        }
    }
}

char *host_ca_save(host_ca *hca)
{
    if (!*hca->name)
        return dupstr("CA record must have a name");

    nitty_portable_init();
    if (nitty_portable_dir_mode()) {
        char *path, *err;
        FILE *fp;
        bool bad;

        nitty_portable_ensure_dir(nitty_portable_cadir());
        path = nitty_hostca_filepath(hca->name);
        fp = fopen(path, "wb");
        if (!fp) {
            err = dupprintf("Unable to save CA record: opening file '%s': %s",
                            path, strerror(errno));
            sfree(path);
            return err;
        }

        fprintf(fp, "PublicKey=");
        base64_encode_fp(fp, ptrlen_from_strbuf(hca->ca_public_key), 0);
        fprintf(fp, "\n");

        fprintf(fp, "Validity=");
        percent_encode_fp(fp, ptrlen_from_asciz(hca->validity_expression), NULL);
        fprintf(fp, "\n");

        fprintf(fp, "PermitRSASHA1=%d\n", (int)hca->opts.permit_rsa_sha1);
        fprintf(fp, "PermitRSASHA256=%d\n", (int)hca->opts.permit_rsa_sha256);
        fprintf(fp, "PermitRSASHA512=%d\n", (int)hca->opts.permit_rsa_sha512);

        bad = ferror(fp);
        if (fclose(fp) < 0)
            bad = true;

        err = NULL;
        if (bad)
            err = dupprintf("Unable to save CA record: writing file '%s': %s",
                            path, strerror(errno));

        sfree(path);
        return err;
    }

    {
        strbuf *sb = strbuf_new();
        escape_registry_key(hca->name, sb);
        HKEY rkey = create_regkey(HKEY_CURRENT_USER, host_ca_key, sb->s);
        if (!rkey) {
            char *err = dupprintf("Unable to create registry key\n"
                                  "HKEY_CURRENT_USER\\%s\\%s", host_ca_key,
                                  sb->s);
            strbuf_free(sb);
            return err;
        }
        strbuf_free(sb);

        {
            strbuf *base64_pubkey = base64_encode_sb(
                ptrlen_from_strbuf(hca->ca_public_key), 0);
            put_reg_sz(rkey, "PublicKey", base64_pubkey->s);
            strbuf_free(base64_pubkey);

            {
                strbuf *validity = percent_encode_sb(
                    ptrlen_from_asciz(hca->validity_expression), NULL);
                put_reg_sz(rkey, "Validity", validity->s);
                strbuf_free(validity);
            }

            put_reg_dword(rkey, "PermitRSASHA1", hca->opts.permit_rsa_sha1);
            put_reg_dword(rkey, "PermitRSASHA256", hca->opts.permit_rsa_sha256);
            put_reg_dword(rkey, "PermitRSASHA512", hca->opts.permit_rsa_sha512);

            close_regkey(rkey);
            return NULL;
        }
    }
}

char *host_ca_delete(const char *name)
{
    nitty_portable_init();
    if (nitty_portable_dir_mode()) {
        char *path;
        bool bad;

        if (!*name)
            return dupstr("CA record must have a name");
        path = nitty_hostca_filepath(name);
        bad = DeleteFileA(path) == 0;
        if (bad && GetLastError() != ERROR_FILE_NOT_FOUND) {
            char *err = dupprintf("Unable to delete file '%s': %s", path,
                                  win_strerror(GetLastError()));
            sfree(path);
            return err;
        }
        sfree(path);
        return NULL;
    }

    {
        HKEY rkey = open_regkey_rw(HKEY_CURRENT_USER, host_ca_key);
        if (!rkey)
            return NULL;

        {
            strbuf *sb = strbuf_new();
            escape_registry_key(name, sb);
            del_regkey(rkey, sb->s);
            strbuf_free(sb);
        }

        return NULL;
    }
}

/*
 * Open (or delete) the random seed file.
 */
enum { DEL, OPEN_R, OPEN_W };
static bool try_random_seed(char const *path, int action, HANDLE *ret)
{
    if (action == DEL) {
        if (!DeleteFile(path) && GetLastError() != ERROR_FILE_NOT_FOUND) {
            nonfatal("Unable to delete '%s': %s", path,
                     win_strerror(GetLastError()));
        }
        *ret = INVALID_HANDLE_VALUE;
        return false;                  /* so we'll do the next ones too */
    }

    *ret = CreateFile(path,
                      action == OPEN_W ? GENERIC_WRITE : GENERIC_READ,
                      action == OPEN_W ? 0 : (FILE_SHARE_READ |
                                              FILE_SHARE_WRITE),
                      NULL,
                      action == OPEN_W ? CREATE_ALWAYS : OPEN_EXISTING,
                      action == OPEN_W ? FILE_ATTRIBUTE_NORMAL : 0,
                      NULL);

    return (*ret != INVALID_HANDLE_VALUE);
}

static bool try_random_seed_and_free(char *path, int action, HANDLE *hout)
{
    bool retd = try_random_seed(path, action, hout);
    sfree(path);
    return retd;
}

static HANDLE access_random_seed(int action)
{
    HANDLE rethandle;

    nitty_portable_init();
    if (nitty_portable_dir_mode()) {
        if (try_random_seed(nitty_portable_seedpath(), action, &rethandle))
            return rethandle;
        return INVALID_HANDLE_VALUE;
    }

    /*
     * Iterate over a selection of possible random seed paths until
     * we find one that works.
     *
     * We do this iteration separately for reading and writing,
     * meaning that we will automatically migrate random seed files
     * if a better location becomes available (by reading from the
     * best location in which we actually find one, and then
     * writing to the best location in which we can _create_ one).
     */

    /*
     * First, try the location specified by the user in the
     * Registry, if any.
     */
    {
        HKEY rkey = open_regkey_ro(HKEY_CURRENT_USER, PUTTY_REG_POS);
        if (rkey) {
            char *regpath = get_reg_sz(rkey, "RandSeedFile");
            close_regkey(rkey);
            if (regpath) {
                bool success = try_random_seed(regpath, action, &rethandle);
                sfree(regpath);
                if (success)
                    return rethandle;
            }
        }
    }

    /*
     * Next, try the user's local Application Data directory,
     * followed by their non-local one. This is found using the
     * SHGetFolderPath function, which won't be present on all
     * versions of Windows.
     */
    if (!tried_shgetfolderpath) {
        /* This is likely only to bear fruit on systems with IE5+
         * installed, or WinMe/2K+. There is some faffing with
         * SHFOLDER.DLL we could do to try to find an equivalent
         * on older versions of Windows if we cared enough.
         * However, the invocation below requires IE5+ anyway,
         * so stuff that. */
        shell32_module = load_system32_dll("shell32.dll");
        GET_WINDOWS_FUNCTION(shell32_module, SHGetFolderPathA);
        tried_shgetfolderpath = true;
    }
    if (p_SHGetFolderPathA) {
        char profile[MAX_PATH + 1];
        if (SUCCEEDED(p_SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA,
                                         NULL, SHGFP_TYPE_CURRENT, profile)) &&
            try_random_seed_and_free(dupcat(profile, "\\PUTTY.RND"),
                                     action, &rethandle))
            return rethandle;

        if (SUCCEEDED(p_SHGetFolderPathA(NULL, CSIDL_APPDATA,
                                         NULL, SHGFP_TYPE_CURRENT, profile)) &&
            try_random_seed_and_free(dupcat(profile, "\\PUTTY.RND"),
                                     action, &rethandle))
            return rethandle;
    }

    /*
     * Failing that, try %HOMEDRIVE%%HOMEPATH% as a guess at the
     * user's home directory.
     */
    {
        char drv[MAX_PATH], path[MAX_PATH];

        DWORD drvlen = GetEnvironmentVariable("HOMEDRIVE", drv, sizeof(drv));
        DWORD pathlen = GetEnvironmentVariable("HOMEPATH", path, sizeof(path));

        /* We permit %HOMEDRIVE% to expand to an empty string, but if
         * %HOMEPATH% does that, we abort the attempt. Same if either
         * variable overflows its buffer. */
        if (drvlen == 0)
            drv[0] = '\0';

        if (drvlen < lenof(drv) && pathlen < lenof(path) && pathlen > 0 &&
            try_random_seed_and_free(
                dupcat(drv, path, "\\PUTTY.RND"), action, &rethandle))
            return rethandle;
    }

    /*
     * And finally, fall back to C:\WINDOWS.
     */
    {
        char windir[MAX_PATH];
        DWORD len = GetWindowsDirectory(windir, sizeof(windir));
        if (len < lenof(windir) &&
            try_random_seed_and_free(
                dupcat(windir, "\\PUTTY.RND"), action, &rethandle))
            return rethandle;
    }

    /*
     * If even that failed, give up.
     */
    return INVALID_HANDLE_VALUE;
}

void read_random_seed(noise_consumer_t consumer)
{
    HANDLE seedf = access_random_seed(OPEN_R);

    if (seedf != INVALID_HANDLE_VALUE) {
        while (1) {
            char buf[1024];
            DWORD len;

            if (ReadFile(seedf, buf, sizeof(buf), &len, NULL) && len)
                consumer(buf, len);
            else
                break;
        }
        CloseHandle(seedf);
    }
}

void write_random_seed(void *data, int len)
{
    HANDLE seedf = access_random_seed(OPEN_W);

    if (seedf != INVALID_HANDLE_VALUE) {
        DWORD lenwritten;

        WriteFile(seedf, data, len, &lenwritten, NULL);
        CloseHandle(seedf);
    }
}

static strbuf *jumplist_merge_multi_sz(strbuf *oldlist, const char *add,
                                       const char *rem)
{
    BinarySource src[1];
    strbuf *newlist = strbuf_new();

    BinarySource_BARE_INIT_PL(src, ptrlen_from_strbuf(oldlist));

    if (add)
        put_asciz(newlist, add);

    while (true) {
        const char *olditem = get_asciz(src);
        if (get_err(src))
            break;

        if (!rem || strcmp(olditem, rem) != 0) {
            settings_r *psettings_tmp = open_settings_r(olditem);

            if (psettings_tmp != NULL) {
                close_settings_r(psettings_tmp);
                put_asciz(newlist, olditem);
            }
        }
    }

    return newlist;
}

/*
 * Internal function supporting the jump list registry code. All the
 * functions to add, remove and read the list have substantially
 * similar content, so this is a generalisation of all of them which
 * transforms the list in the registry by prepending 'add' (if
 * non-null), removing 'rem' from what's left (if non-null), and
 * returning the resulting concatenated list of strings in 'out' (if
 * non-null).
 */
static int transform_jumplist_registry(
    const char *add, const char *rem, char **out)
{
    strbuf *oldlist;
    bool write_failure = false;

    nitty_portable_init();
    if (nitty_portable_dir_mode()) {
        char *recent_path = dupcat(nitty_portable_jumplistdir(),
                                    "\\RecentSessions", NULL);
        FILE *fp;
        long sz;
        char *raw;

        nitty_portable_ensure_dir(nitty_portable_jumplistdir());
        fp = fopen(recent_path, "rb");
        if (!fp) {
            oldlist = strbuf_new();
            put_data(oldlist, "\0\0", 2);
        } else {
            fseek(fp, 0, SEEK_END);
            sz = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            raw = snewn(sz > 0 ? sz + 2 : 2, char);
            if (sz > 0)
                fread(raw, 1, sz, fp);
            raw[sz] = '\0';
            raw[sz + 1] = '\0';
            fclose(fp);
            oldlist = strbuf_new();
            put_datapl(oldlist, make_ptrlen(raw, (size_t)sz));
            if (sz < 2)
                put_data(oldlist, "\0\0", 2);
            sfree(raw);
        }

        if (add || rem) {
            strbuf *newlist = jumplist_merge_multi_sz(oldlist, add, rem);

            strbuf_free(oldlist);
            oldlist = newlist;
            fp = fopen(recent_path, "wb");
            if (fp) {
                fwrite(oldlist->s, 1, oldlist->len, fp);
                fclose(fp);
            } else {
                write_failure = true;
            }
        }

        sfree(recent_path);

        if (out && !write_failure)
            *out = strbuf_to_str(oldlist);
        else
            strbuf_free(oldlist);

        if (write_failure)
            return JUMPLISTREG_ERROR_VALUEWRITE_FAILURE;
        return JUMPLISTREG_OK;
    }

    {
        HKEY rkey = create_regkey(HKEY_CURRENT_USER, reg_jumplist_key);

        if (!rkey)
            return JUMPLISTREG_ERROR_KEYOPENCREATE_FAILURE;

        oldlist = get_reg_multi_sz(rkey, reg_jumplist_value);
        if (!oldlist) {
            oldlist = strbuf_new();
            put_data(oldlist, "\0\0", 2);
        }

        if (add || rem) {
            strbuf *newlist = jumplist_merge_multi_sz(oldlist, add, rem);

            strbuf_free(oldlist);
            oldlist = newlist;
            write_failure = !put_reg_multi_sz(rkey, reg_jumplist_value, oldlist);
        }

        close_regkey(rkey);

        if (out && !write_failure)
            *out = strbuf_to_str(oldlist);
        else
            strbuf_free(oldlist);

        if (write_failure)
            return JUMPLISTREG_ERROR_VALUEWRITE_FAILURE;
        return JUMPLISTREG_OK;
    }
}

/* Adds a new entry to the jumplist entries in the registry. */
int add_to_jumplist_registry(const char *item)
{
    return transform_jumplist_registry(item, item, NULL);
}

/* Removes an item from the jumplist entries in the registry. */
int remove_from_jumplist_registry(const char *item)
{
    return transform_jumplist_registry(NULL, item, NULL);
}

/* Returns the jumplist entries from the registry. Caller must free
 * the returned pointer. */
char *get_jumplist_registry_entries (void)
{
    char *list_value;

    if (transform_jumplist_registry(NULL,NULL,&list_value) != JUMPLISTREG_OK) {
        list_value = snewn(2, char);
        *list_value = '\0';
        *(list_value + 1) = '\0';
    }
    return list_value;
}

static void nitty_delete_all_files_in_dir(const char *dir)
{
    char *pat;
    WIN32_FIND_DATAA fd;
    HANDLE h;

    if (!dir || !*dir)
        return;
    pat = dupcat(dir, "\\*", NULL);
    h = FindFirstFileA(pat, &fd);
    sfree(pat);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char *fp = dupcat(dir, "\\", fd.cFileName, NULL);

            DeleteFileA(fp);
            sfree(fp);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/*
 * Recursively delete a registry key and everything under it.
 */
static void registry_recursive_remove(HKEY key)
{
    char *name;

    DWORD i = 0;
    while ((name = enum_regkey(key, i)) != NULL) {
        HKEY subkey = open_regkey_rw(key, name);
        if (subkey) {
            registry_recursive_remove(subkey);
            close_regkey(subkey);
        }
        del_regkey(key, name);
        sfree(name);
    }
}

void cleanup_all(void)
{
    /* ------------------------------------------------------------
     * Wipe out the random seed file, in all of its possible
     * locations.
     */
    access_random_seed(DEL);

    /* ------------------------------------------------------------
     * Ask Windows to delete any jump list information associated
     * with this installation of PuTTY.
     */
    clear_jumplist();

    nitty_portable_init();
    if (nitty_portable_dir_mode()) {
        char *jp = dupcat(nitty_portable_jumplistdir(), "\\RecentSessions", NULL);

        DeleteFileA(jp);
        sfree(jp);
        nitty_delete_all_files_in_dir(nitty_portable_sessdir());
        nitty_delete_all_files_in_dir(nitty_portable_sshkeysdir());
        nitty_delete_all_files_in_dir(nitty_portable_cadir());
        return;
    }

    /* ------------------------------------------------------------
     * Destroy all registry information associated with PuTTY.
     */

    /*
     * Open the main PuTTY registry key and remove everything in it.
     */
    HKEY key = open_regkey_rw(HKEY_CURRENT_USER, PUTTY_REG_POS);
    if (key) {
        registry_recursive_remove(key);
        close_regkey(key);
    }
    /*
     * Now open the parent key and remove the PuTTY main key. Once
     * we've done that, see if the parent key has any other
     * children.
     */
    if ((key = open_regkey_rw(HKEY_CURRENT_USER, PUTTY_REG_PARENT)) != NULL) {
        del_regkey(key, PUTTY_REG_PARENT_CHILD);
        char *name = enum_regkey(key, 0);
        close_regkey(key);

        /*
         * If the parent key had no other children, we must delete
         * it in its turn. That means opening the _grandparent_
         * key.
         */
        if (name) {
            sfree(name);
        } else {
            if ((key = open_regkey_rw(HKEY_CURRENT_USER,
                                      PUTTY_REG_GPARENT)) != NULL) {
                del_regkey(key, PUTTY_REG_GPARENT_CHILD);
                close_regkey(key);
            }
        }
    }
    /*
     * Now we're done.
     */
}
