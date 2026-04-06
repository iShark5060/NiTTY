/*
 * NiTTY: RuTTY-style session script playback (auto-login scripts).
 */

#ifndef NITTY_SCRIPT_H
#define NITTY_SCRIPT_H

#include "putty.h"

#include <windows.h>

#define NITTY_SCRIPT_LINE_SIZE 4096
#define NITTY_SCRIPT_COND_SIZE 256

typedef struct NittyScriptData {
    Ldisc *ldisc;
    LogContext *logctx;
    HWND hwnd;

    int line_delay;
    int char_delay;
    char cond_char;
    bool cond_use;
    bool enable;
    bool except;
    int timeout;
    int crlf;

    char waitfor[NITTY_SCRIPT_COND_SIZE];
    int waitfor_c;
    char halton[NITTY_SCRIPT_COND_SIZE];
    int halton_c;

    char waitfor2[NITTY_SCRIPT_COND_SIZE];
    int waitfor2_c;

    bool runs;
    bool send;

    char *filebuffer;
    char *nextnextline;
    char *filebuffer_end;
    unsigned long latest;

    char *nextline;
    int nextline_c;
    int nextline_cc;

    char remotedata[NITTY_SCRIPT_LINE_SIZE];
    int remotedata_c;
} NittyScriptData;

void nitty_script_prepare_session(NittyScriptData *ns, Conf *conf);
void nitty_script_arm(NittyScriptData *ns, Ldisc *ldisc,
                      LogContext *logctx, HWND hwnd);
void nitty_script_cleanup(NittyScriptData *ns);
bool nitty_script_sendfile(NittyScriptData *ns, const Filename *scriptfile);
void nitty_script_on_remote_data(NittyScriptData *ns,
                                 const void *data, size_t len);

#endif /* NITTY_SCRIPT_H */
