/*
 * NiTTY: RuTTY-style session script playback (adapted from KiTTY/RuTTY rutty/script.c).
 */

#include "nitty_script.h"

#include <stdlib.h>
#include <string.h>

static int nitty_findline(NittyScriptData *ns);
static void nitty_getline(NittyScriptData *ns);
static int nitty_chkline(NittyScriptData *ns);
static void nitty_cond_set(char *cond, int *p, const char *in, int sz);
static int nitty_cond_chk(char *ref, int rc, char *data, int dc);
static void nitty_setsend(NittyScriptData *ns);
static void nitty_sendline(void *ctx, unsigned long now);
static void nitty_sendchar(void *ctx, unsigned long now);
static void nitty_timeout(void *ctx, unsigned long now);

static void nitty_fail(NittyScriptData *ns, const char *msg)
{
    logevent(ns->logctx, msg);
    MessageBoxA(ns->hwnd, msg, appname, MB_OK | MB_ICONEXCLAMATION);
}

void nitty_script_prepare_session(NittyScriptData *ns, Conf *conf)
{
    nitty_script_cleanup(ns);

    ns->line_delay = conf_get_int(conf, CONF_nitty_script_line_delay);
    if (ns->line_delay < 5)
        ns->line_delay = 5;
    ns->line_delay = ns->line_delay * TICKSPERSEC / 1000;

    ns->char_delay = conf_get_int(conf, CONF_nitty_script_char_delay) * TICKSPERSEC / 1000;

    {
        const char *cl = conf_get_str(conf, CONF_nitty_script_cond_line);
        ns->cond_char = (cl && cl[0]) ? cl[0] : ':';
    }

    ns->enable = conf_get_bool(conf, CONF_nitty_script_enable);
    ns->cond_use = ns->enable && conf_get_bool(conf, CONF_nitty_script_cond_use);
    ns->except = conf_get_bool(conf, CONF_nitty_script_except);
    ns->timeout = conf_get_int(conf, CONF_nitty_script_timeout) * TICKSPERSEC;

    nitty_cond_set(ns->waitfor, &ns->waitfor_c,
                   conf_get_str(conf, CONF_nitty_script_waitfor),
                   (int)strlen(conf_get_str(conf, CONF_nitty_script_waitfor)));
    nitty_cond_set(ns->halton, &ns->halton_c,
                   conf_get_str(conf, CONF_nitty_script_halton),
                   (int)strlen(conf_get_str(conf, CONF_nitty_script_halton)));

    ns->crlf = conf_get_int(conf, CONF_nitty_script_crlf);

    ns->waitfor2[0] = '\0';
    ns->waitfor2_c = -1;

    ns->runs = false;
    ns->send = false;
    ns->filebuffer = NULL;

    ns->latest = 0;

    ns->remotedata_c = NITTY_SCRIPT_COND_SIZE;
    ns->remotedata[0] = '\0';
}

void nitty_script_arm(NittyScriptData *ns, Ldisc *ldisc,
                      LogContext *logctx, HWND hwnd)
{
    ns->ldisc = ldisc;
    ns->logctx = logctx;
    ns->hwnd = hwnd;
}

void nitty_script_cleanup(NittyScriptData *ns)
{
    ns->runs = false;
    expire_timer_context(ns);
    ns->latest = 0;

    if (ns->filebuffer) {
        sfree(ns->filebuffer);
        ns->filebuffer = NULL;
    }
    ns->nextnextline = NULL;
    ns->filebuffer_end = NULL;
    ns->nextline = NULL;

    ns->ldisc = NULL;
    ns->logctx = NULL;
}

bool nitty_script_sendfile(NittyScriptData *ns, const Filename *scriptfile)
{
    FILE *fp;
    long fsize;

    if (ns->runs)
        return false;
    if (!ns->ldisc)
        return false;

    fp = f_open(scriptfile, "rb", false);
    if (!fp) {
        logevent(ns->logctx, "script file not found");
        return false;
    }

    ns->runs = true;

    fseek(fp, 0L, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    ns->nextnextline = ns->filebuffer = smalloc(fsize);
    ns->filebuffer_end = &ns->filebuffer[fsize];

    if (fread(ns->filebuffer, sizeof(char), fsize, fp) != (size_t)fsize) {
        logevent(ns->logctx, "script file read failed");
        fclose(fp);
        nitty_script_cleanup(ns);
        return false;
    }

    fclose(fp);

    logevent(ns->logctx, "sending script to host ...");

    nitty_getline(ns);
    nitty_chkline(ns);

    if (ns->enable && !ns->except) {
        ns->send = false;
        ns->latest = schedule_timer(ns->timeout, nitty_timeout, ns);
    } else {
        ns->send = true;
        schedule_timer(ns->line_delay, nitty_sendline, ns);
    }
    return true;
}

static int nitty_findline(NittyScriptData *ns)
{
    if (!ns->filebuffer)
        return false;

    if (ns->nextnextline >= ns->filebuffer_end)
        return false;

    ns->nextline = ns->nextnextline;
    ns->nextline_c = 0;
    while (ns->nextnextline < ns->filebuffer_end && ns->nextnextline[0] != '\n') {
        ns->nextnextline++;
        ns->nextline_c++;
    }

    if (ns->nextnextline < ns->filebuffer_end) {
        ns->nextnextline++;
        ns->nextline_c++;

        if (ns->crlf == NITTY_SCRIPT_CRLF_REC) {
            if (ns->nextnextline < ns->filebuffer_end && ns->nextnextline[0] == '\n') {
                ns->nextnextline++;
                ns->nextline_c++;

                if (&ns->nextnextline[-3] >= ns->filebuffer &&
                    ns->nextnextline[-3] == '\r' &&
                    ns->nextnextline < ns->filebuffer_end &&
                    ns->nextnextline[0] == '\n') {
                    ns->nextnextline++;
                }
            }
            ns->nextline_c--;
        }
    }
    return true;
}

static void nitty_getline(NittyScriptData *ns)
{
    int neof;
    int i;

    if (!ns->runs || !ns->filebuffer)
        return;

    do {
        do
            neof = nitty_findline(ns);
        while (neof &&
               ((!ns->cond_use && ns->nextline[0] == ns->cond_char) ||
                (ns->cond_use && ns->nextline[0] == ns->cond_char &&
                 ns->nextline[1] == ns->cond_char)));
        if (!neof) {
            ns->nextline_c = 0;
            ns->nextline_cc = 0;
            return;
        }

        i = ns->nextline_c;
        switch (ns->crlf) {
          case NITTY_SCRIPT_CRLF_OFF:
            break;
          case NITTY_SCRIPT_CRLF_NOLF:
            if (ns->nextline[i - 1] == '\n')
                i--;
            break;
          case NITTY_SCRIPT_CRLF_CR:
            if (ns->nextline[i - 1] == '\n')
                i--;
            if (i > 0 && ns->nextline[i - 1] == '\r')
                i--;
            ns->nextline[i++] = '\r';
            break;
          default:
            break;
        }
    } while (i == 0);
    ns->nextline_c = i;
    ns->nextline_cc = 0;
}

static void nitty_sendline(void *ctx, unsigned long now)
{
    NittyScriptData *ns = (NittyScriptData *)ctx;

    (void)now;

    if (!ns->runs)
        return;
    if (!ns->ldisc)
        return;

    if (ns->nextline_c == 0) {
        nitty_script_cleanup(ns);
        logevent(ns->logctx, " ...finished sending script");
        return;
    }

    if (ns->char_delay > 1) {
        schedule_timer(ns->char_delay, nitty_sendchar, ns);
        return;
    }

    if (ns->char_delay == 0)
        ldisc_send(ns->ldisc, ns->nextline, ns->nextline_c, 0);
    else {
        int j;
        for (j = 0; j < ns->nextline_c; j++)
            ldisc_send(ns->ldisc, &ns->nextline[j], 1, 0);
    }

    nitty_getline(ns);
    nitty_chkline(ns);

    if (ns->enable) {
        ns->send = false;
        ns->latest = schedule_timer(ns->timeout, nitty_timeout, ns);
    } else {
        schedule_timer(ns->line_delay, nitty_sendline, ns);
    }
}

static void nitty_sendchar(void *ctx, unsigned long now)
{
    NittyScriptData *ns = (NittyScriptData *)ctx;

    (void)now;

    if (!ns->runs)
        return;
    if (!ns->ldisc)
        return;

    if (ns->nextline_c == 0) {
        nitty_script_cleanup(ns);
        logevent(ns->logctx, "....finished sending script");
        return;
    }

    if (ns->nextline_cc < ns->nextline_c)
        ldisc_send(ns->ldisc, &ns->nextline[ns->nextline_cc++], 1, 0);

    if (ns->nextline_cc < ns->nextline_c) {
        schedule_timer(ns->char_delay, nitty_sendchar, ns);
        return;
    }

    nitty_getline(ns);
    nitty_chkline(ns);

    if (ns->enable) {
        ns->send = false;
        ns->latest = schedule_timer(ns->timeout, nitty_timeout, ns);
    } else {
        schedule_timer(ns->line_delay, nitty_sendline, ns);
    }
}

static void nitty_timeout(void *ctx, unsigned long now)
{
    NittyScriptData *ns = (NittyScriptData *)ctx;
    unsigned long diff = now > ns->latest ? now - ns->latest : ns->latest - now;

    if (diff < 50) {
        nitty_script_cleanup(ns);
        logevent(ns->logctx, "script timeout !");
        nitty_fail(ns, "script timeout !");
    }
}

static int nitty_chkline(NittyScriptData *ns)
{
    if (ns->nextline_c > 0 && ns->nextline[0] == ns->cond_char) {
        nitty_cond_set(ns->waitfor2, &ns->waitfor2_c,
                       &ns->nextline[1], ns->nextline_c - 1);
        nitty_getline(ns);
        return true;
    } else {
        ns->waitfor2_c = -1;
        ns->waitfor2[0] = '\0';
    }
    return false;
}

static void nitty_cond_set(char *cond, int *p, const char *in, int sz)
{
    int i = 0;
    (*p) = 0;

    while (sz > 0 && (in[sz - 1] == '\n' || in[sz - 1] == '\r'))
        sz--;

    if (sz == 0) {
        cond[*p] = '\0';
    } else if (in[0] != '"') {
        if (sz > (NITTY_SCRIPT_COND_SIZE - 1))
            i = sz - (NITTY_SCRIPT_COND_SIZE - 1);
        cond[(*p)++] = '\0';
        while (i < sz)
            cond[(*p)++] = in[i++];
    } else {
        if (sz > NITTY_SCRIPT_COND_SIZE)
            sz = NITTY_SCRIPT_COND_SIZE;
        i++;
        while (i < sz) {
            cond[(*p)++] = '\0';
            while (i < sz && in[i] != '"')
                cond[(*p)++] = in[i++];
            i++;
            while (i < sz && in[i] == ' ')
                i++;
            while (i < sz && in[i] == '"')
                i++;
        }
    }
}

static int nitty_cond_chk(char *ref, int rc, char *data, int dc)
{
    int rcc = rc;
    int dcc = dc;

    while (rcc > 0 && dcc > 0) {
        do {
            rcc--;
            dcc--;
        } while (rcc >= 0 && dcc >= 0 && ref[rcc] != '\0' && ref[rcc] == data[dcc]);

        if (ref[rcc] == '\0')
            return true;

        dcc = dc;
        while (rcc > 0 && ref[--rcc] != '\0')
            ;
    }
    return false;
}

static void nitty_setsend(NittyScriptData *ns)
{
    ns->latest = 0;

    if (ns->nextline_c == 0) {
        nitty_script_cleanup(ns);
        logevent(ns->logctx, "... finished sending script");
        return;
    }

    if (nitty_chkline(ns)) {
        ns->send = false;
        ns->latest = schedule_timer(ns->timeout, nitty_timeout, ns);
    } else {
        ns->send = true;
        schedule_timer(ns->line_delay, nitty_sendline, ns);
    }
}

void nitty_script_on_remote_data(NittyScriptData *ns, const void *vdata, size_t len)
{
    const char *data = (const char *)vdata;
    size_t k;

    for (k = 0; k < len; k++) {
        if (data[k] == '\n' || data[k] == '\r' || data[k] == '\0') {
            if (ns->remotedata_c > NITTY_SCRIPT_COND_SIZE)
                ; /* RuTTY recorded line here for RECORD mode */

            ns->remotedata_c = NITTY_SCRIPT_COND_SIZE;
            ns->remotedata[ns->remotedata_c] = '\0';
        } else {
            if (ns->remotedata_c >= NITTY_SCRIPT_LINE_SIZE) {
                int j = NITTY_SCRIPT_LINE_SIZE - NITTY_SCRIPT_COND_SIZE;
                ns->remotedata_c = 0;
                while (ns->remotedata_c < NITTY_SCRIPT_COND_SIZE)
                    ns->remotedata[ns->remotedata_c++] = ns->remotedata[j++];
            }
            ns->remotedata[ns->remotedata_c++] = data[k];
        }

        if (ns->runs) {
            if (ns->halton_c > 0 &&
                nitty_cond_chk(ns->halton, ns->halton_c,
                               ns->remotedata, ns->remotedata_c)) {
                nitty_script_cleanup(ns);
                logevent(ns->logctx, "script halted");
                nitty_fail(ns, "script halted");
                return;
            }

            if (ns->enable && !ns->send) {
                if (ns->waitfor2_c >= 0) {
                    if (ns->waitfor2_c == 0 ||
                        nitty_cond_chk(ns->waitfor2, ns->waitfor2_c,
                                       ns->remotedata, ns->remotedata_c))
                        nitty_setsend(ns);
                } else if (ns->waitfor_c == 0 ||
                           nitty_cond_chk(ns->waitfor, ns->waitfor_c,
                                           ns->remotedata, ns->remotedata_c)) {
                    nitty_setsend(ns);
                }
            }
        }
    }
}
