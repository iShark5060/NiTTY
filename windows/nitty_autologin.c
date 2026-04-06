/*
 * KiTTY-style saved SSH password (session setting, plain text).
 */

#include "putty.h"

SeatPromptResult nitty_conf_get_passwd_input(
    prompts_t *p, Conf *conf, cmdline_get_passwd_input_state *state,
    bool restartable)
{
    const char *pw;

    if (p->n_prompts != 1 || p->prompts[0]->echo || !p->to_server)
        return SPR_INCOMPLETE;

    if (state->tried)
        return SPR_SW_ABORT("Saved password was not accepted");

    pw = conf_get_str(conf, CONF_nitty_autologin_password);
    if (!pw || !*pw)
        return SPR_INCOMPLETE;

    prompt_set_result(p->prompts[0], pw);
    state->tried = true;

    (void)restartable; /* password remains in conf for the next session */

    return SPR_OK;
}
