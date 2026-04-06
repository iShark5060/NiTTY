#ifndef NITTY_SESSFILE_H
#define NITTY_SESSFILE_H

#include "defs.h"

typedef struct nitty_settings_item NittySettingsItem;
/* Opaque handle: implementation uses pointers; do not typedef the struct itself. */
struct nitty_settings_list;
typedef struct nitty_settings_list *NittySettingsList;

NittySettingsList nitty_settings_list_new(void);
void nitty_settings_list_free(NittySettingsList list);
void nitty_settings_add(NittySettingsList list, const char *name, const char *value);
void nitty_settings_del(NittySettingsList list, const char *name);
char *nitty_settings_get_str_dup(NittySettingsList list, const char *key);
int nitty_settings_get_int(NittySettingsList list, const char *key, int def);
void nitty_settings_load_file(NittySettingsList list, const char *filepath);
void nitty_settings_save_file(NittySettingsList list, const char *filepath);

void nitty_mungestr(const char *in, char *out);
void nitty_unmungestr(const char *in, char *out, int outlen);
void nitty_packstr(const char *in, char *out);

#endif
