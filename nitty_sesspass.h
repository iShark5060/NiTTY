/*
 * Obfuscated storage for saved SSH passwords (session registry / files).
 * XOR + Base64 keyed by session name — not a strong secret; deters casual
 * copy-paste of config. Legacy plain-text "Password" values still load.
 */

#ifndef NITTY_SESSPASS_H
#define NITTY_SESSPASS_H

char *nitty_sesspass_encrypt_for_save(const char *sessionname,
                                      const char *plaintext);
char *nitty_sesspass_decrypt_after_load(const char *sessionname,
                                        const char *stored);

#endif
