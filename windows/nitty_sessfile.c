/*
 * Session file storage (KiTTY-compatible line format: name\\value\\ per record).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "putty.h"
#include "nitty_sessfile.h"

#include <windows.h>

struct nitty_settings_item {
    char *name;
    char *value;
    NittySettingsItem *next;
    NittySettingsItem *prev;
};

struct nitty_settings_list {
    NittySettingsItem *first;
    NittySettingsItem *last;
};

static NittySettingsItem *nitty_item_new(const char *name, const char *value)
{
    NittySettingsItem *item = snew(NittySettingsItem);
    item->name = dupstr(name);
    item->value = value ? dupstr(value) : NULL;
    item->next = item->prev = NULL;
    return item;
}

static void nitty_item_free(NittySettingsItem *item)
{
    if (!item)
        return;
    sfree(item->name);
    sfree(item->value);
    sfree(item);
}

NittySettingsList nitty_settings_list_new(void)
{
    NittySettingsList list = snew(struct nitty_settings_list);
    list->first = list->last = NULL;
    return list;
}

void nitty_settings_list_free(NittySettingsList list)
{
    NittySettingsItem *cur, *next;

    if (!list)
        return;
    for (cur = list->first; cur; cur = next) {
        next = cur->next;
        nitty_item_free(cur);
    }
    sfree(list);
}

void nitty_settings_del(NittySettingsList list, const char *name)
{
    NittySettingsItem *cur;

    if (!list || !name)
        return;
    for (cur = list->first; cur; cur = cur->next) {
        if (cur->name && !strcmp(cur->name, name)) {
            if (cur->prev)
                cur->prev->next = cur->next;
            else
                list->first = cur->next;
            if (cur->next)
                cur->next->prev = cur->prev;
            else
                list->last = cur->prev;
            nitty_item_free(cur);
            return;
        }
    }
}

void nitty_settings_add(NittySettingsList list, const char *name, const char *value)
{
    NittySettingsItem *item;

    if (!list || !name)
        return;
    nitty_settings_del(list, name);
    item = nitty_item_new(name, value);
    if (!list->last) {
        list->first = list->last = item;
    } else {
        list->last->next = item;
        item->prev = list->last;
        list->last = item;
    }
}

char *nitty_settings_get_str_dup(NittySettingsList list, const char *key)
{
    NittySettingsItem *cur;

    if (!list)
        return NULL;
    for (cur = list->first; cur; cur = cur->next) {
        if (cur->name && !strcmp(cur->name, key))
            return cur->value ? dupstr(cur->value) : NULL;
    }
    return NULL;
}

int nitty_settings_get_int(NittySettingsList list, const char *key, int def)
{
    char *s = nitty_settings_get_str_dup(list, key);
    int v;

    if (!s)
        return def;
    v = atoi(s);
    sfree(s);
    return v;
}

void nitty_mungestr(const char *in, char *out)
{
    bool candot = false;
    static const char hex[16] = "0123456789ABCDEF";

    while (*in) {
        if (*in == ' ' || *in == '\\' || *in == '*' || *in == '?' ||
            *in == ':' || *in == '/' || *in == '"' || *in == '<' ||
            *in == '>' || *in == '|' || *in == '%' || *in < ' ' ||
            *in > '~' || (*in == '.' && !candot)) {
            *out++ = '%';
            *out++ = hex[((unsigned char)*in) >> 4];
            *out++ = hex[((unsigned char)*in) & 15];
        } else {
            *out++ = *in;
        }
        in++;
        candot = true;
    }
    *out = '\0';
}

void nitty_unmungestr(const char *in, char *out, int outlen)
{
    while (*in && outlen > 1) {
        if (*in == '%' && in[1] && in[2]) {
            int hi = in[1] - '0';
            int lo = in[2] - '0';

            hi -= (hi > 9 ? 7 : 0);
            lo -= (lo > 9 ? 7 : 0);
            *out++ = (char)((hi << 4) + lo);
            in += 3;
        } else {
            *out++ = *in++;
        }
        outlen--;
    }
    *out = '\0';
}

void nitty_packstr(const char *in, char *out)
{
    static const char hex[16] = "0123456789ABCDEF";

    while (*in) {
        if (*in == '<' || *in == '>' || *in == ':' || *in == '"' ||
            *in == '/' || *in == '\\' || *in == '|') {
            *out++ = '%';
            *out++ = hex[((unsigned char)*in) >> 4];
            *out++ = hex[((unsigned char)*in) & 15];
        } else {
            *out++ = *in;
        }
        in++;
    }
    *out = '\0';
}

void nitty_settings_load_file(NittySettingsList list, const char *filepath)
{
    FILE *fp;
    char line[65536];

    if (!list || !filepath)
        return;
    fp = fopen(filepath, "rb");
    if (!fp)
        return;

    while (fgets(line, sizeof line, fp)) {
        char *r = line + strcspn(line, "\r\n");
        char *dup, *sep, *end, *val;
        size_t outlen;
        char *out;

        *r = '\0';
        if (!*line)
            continue;

        dup = dupstr(line);
        sep = strchr(dup, '\\');
        end = strrchr(dup, '\\');
        if (!sep || !end || end < sep) {
            sfree(dup);
            continue;
        }
        *end = '\0';
        *sep = '\0';
        val = sep + 1;

        outlen = strlen(val) * 4 + 16;
        out = snewn(outlen, char);
        nitty_unmungestr(val, out, outlen);
        nitty_settings_add(list, dup, out);
        sfree(out);
        sfree(dup);
    }
    fclose(fp);
}

void nitty_settings_save_file(NittySettingsList list, const char *filepath)
{
    FILE *fp;
    NittySettingsItem *cur;

    if (!list || !filepath)
        return;
    fp = fopen(filepath, "wb");
    if (!fp) {
        nonfatal("NiTTY: unable to write session file '%s'", filepath);
        return;
    }

    for (cur = list->first; cur; cur = cur->next) {
        if (cur->name) {
            if (!cur->value) {
                fprintf(fp, "%s\\\\\n", cur->name);
            } else {
                char *packed = snewn(strlen(cur->value) * 3 + 4, char);

                nitty_mungestr(cur->value, packed);
                fprintf(fp, "%s\\%s\\\n", cur->name, packed);
                sfree(packed);
            }
        }
    }
    fclose(fp);
}
