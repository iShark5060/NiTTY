/*
 * Pipe-selection rules for the Windows OpenSSH agent client.
 * Build with NITTY_OPENSSH_AGENT_PURE so this file does not pull in PuTTY.
 */

#include <stdio.h>

#include "nitty_openssh_agent.h"

static int fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int eq(const char *got, const char *want)
{
    if (got == NULL || want == NULL)
        return got == want;
    return strcmp(got, want) == 0;
}

int main(void)
{
    const char *def = NITTY_OPENSSH_AGENT_PIPE;
    const char *custom = "\\\\.\\pipe\\my-agent";

    if (!nitty_openssh_agent_path_ok(def))
        return fail("default pipe rejected");
    if (!nitty_openssh_agent_path_ok(custom))
        return fail("custom pipe rejected");
    if (nitty_openssh_agent_path_ok(NULL))
        return fail("NULL accepted");
    if (nitty_openssh_agent_path_ok(""))
        return fail("empty accepted");
    if (nitty_openssh_agent_path_ok("\\\\.\\pipe\\"))
        return fail("empty leaf accepted");
    if (nitty_openssh_agent_path_ok("\\\\.\\pipe\\a\\b"))
        return fail("nested pipe accepted");
    if (nitty_openssh_agent_path_ok("/tmp/ssh-agent.sock"))
        return fail("unix socket accepted");

    if (!eq(nitty_openssh_agent_choose(NULL, false, true), def))
        return fail("missing env should use the default pipe");
    if (!eq(nitty_openssh_agent_choose("/tmp/ssh-agent.sock", true, true), def))
        return fail("unix SSH_AUTH_SOCK should be ignored");
    if (!eq(nitty_openssh_agent_choose(custom, true, true), custom))
        return fail("existing env pipe should win");
    if (!eq(nitty_openssh_agent_choose(custom, false, true), def))
        return fail("missing env pipe should fall back");
    if (nitty_openssh_agent_choose(custom, false, false) != NULL)
        return fail("nothing present should be NULL");
    if (nitty_openssh_agent_choose(NULL, false, false) != NULL)
        return fail("no default should be NULL");
    if (!eq(nitty_openssh_agent_choose("\\\\.\\pipe\\a\\b", true, true), def))
        return fail("illegal env pipe should fall back");

    return 0;
}
