/*
 * XOR + Base64 obfuscation keyed by session name (NiTTY-specific).
 */

#include "putty.h"
#include "misc.h"
#include "nitty_sesspass.h"

#include <string.h>
#include <stddef.h>

#define NITTY_SESSPASS_PREFIX "N1:"

static void derive_xor_key(const char *sessionname, unsigned char key[32])
{
    unsigned char state[96];
    size_t i, n;
    const char *pepper = "NiTTY saved-session password v1";
    n = strlen(pepper);
    memcpy(state, pepper, n);
    if (sessionname) {
        size_t sl = strlen(sessionname);
        if (sl > sizeof(state) - n)
            sl = sizeof(state) - n;
        memcpy(state + n, sessionname, sl);
        n += sl;
    }
    memset(state + n, 0, sizeof(state) - n);

    for (i = 0; i < 32; i++) {
        unsigned j;
        uint32_t acc = (uint32_t)(i + 1) * 0x9E3779B1U;
        for (j = 0; j < 96; j++)
            acc = acc * 1103515245U + 12345U + (uint32_t)state[j];
        key[i] = (unsigned char)(acc ^ state[i % n] ^ state[(i * 7) % n]);
    }
}

char *nitty_sesspass_encrypt_for_save(const char *sessionname,
                                      const char *plaintext)
{
    unsigned char key[32];
    strbuf *raw;
    strbuf *b64;
    char *ret;

    if (!plaintext || !*plaintext)
        return dupstr("");

    derive_xor_key(sessionname, key);

    raw = strbuf_new_nm();
    put_datapl(raw, ptrlen_from_asciz(plaintext));
    {
        size_t i;
        for (i = 0; i < raw->len; i++)
            raw->s[i] ^= key[i % 32];
    }

    b64 = base64_encode_sb(ptrlen_from_strbuf(raw), 0);
    strbuf_free(raw);

    ret = dupcat(NITTY_SESSPASS_PREFIX, b64->s, NULL);
    strbuf_free(b64);
    return ret;
}

char *nitty_sesspass_decrypt_after_load(const char *sessionname,
                                        const char *stored)
{
    unsigned char key[32];
    strbuf *dec;
    char *ret;
    const char *b64data;

    if (!stored || !*stored)
        return dupstr("");

    if (strncmp(stored, NITTY_SESSPASS_PREFIX, strlen(NITTY_SESSPASS_PREFIX)))
        return dupstr(stored);

    b64data = stored + strlen(NITTY_SESSPASS_PREFIX);

    dec = base64_decode_sb(ptrlen_from_asciz(b64data));
    if (!dec || dec->len == 0) {
        if (dec)
            strbuf_free(dec);
        return dupstr("");
    }

    derive_xor_key(sessionname, key);
    {
        size_t i;
        for (i = 0; i < dec->len; i++)
            dec->s[i] ^= key[i % 32];
    }

    ret = snewn(dec->len + 1, char);
    memcpy(ret, dec->s, dec->len);
    ret[dec->len] = '\0';
    strbuf_free(dec);
    return ret;
}
