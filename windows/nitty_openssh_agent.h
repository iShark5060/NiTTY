/*
 * Windows OpenSSH agent client (\\.\pipe\openssh-ssh-agent).
 *
 * Pageant's named pipe and this pipe speak the same SSH agent
 * protocol. Bitwarden's desktop SSH agent binds the OpenSSH pipe.
 * The vault server is not involved.
 *
 * Pageant still wins when it is running. This header's path helpers
 * are pure so a tiny test can call them without linking PuTTY.
 */

#ifndef NITTY_OPENSSH_AGENT_H
#define NITTY_OPENSSH_AGENT_H

#include <stdbool.h>
#include <string.h>

#define NITTY_OPENSSH_AGENT_PIPE "\\\\.\\pipe\\openssh-ssh-agent"

/* True for \\.\pipe\<single component>. Rejects Unix socket paths. */
static inline bool nitty_openssh_agent_path_ok(const char *path)
{
    const char prefix[] = "\\\\.\\pipe\\";
    const char *leaf;

    if (!path)
        return false;
    if (strncmp(path, prefix, sizeof(prefix) - 1) != 0)
        return false;
    leaf = path + (sizeof(prefix) - 1);
    if (*leaf == '\0' || strchr(leaf, '\\') != NULL)
        return false;
    return true;
}

/*
 * ssh_auth_sock is used only when it is a pipe path and that pipe
 * exists. Otherwise the well-known OpenSSH pipe is used if it exists.
 * Returns NULL when neither is available. The returned pointer is
 * ssh_auth_sock or a string literal.
 */
static inline const char *nitty_openssh_agent_choose(
    const char *ssh_auth_sock, bool env_pipe_exists, bool default_pipe_exists)
{
    if (nitty_openssh_agent_path_ok(ssh_auth_sock) && env_pipe_exists)
        return ssh_auth_sock;
    if (default_pipe_exists)
        return NITTY_OPENSSH_AGENT_PIPE;
    return NULL;
}

#ifndef NITTY_OPENSSH_AGENT_PURE
#include "putty.h"

bool nitty_openssh_agent_exists(void);
/* Opens the selected pipe. *err is set on failure (caller frees it). */
HANDLE nitty_openssh_agent_open(char **err);
/* NULL if no OpenSSH agent pipe is present. */
Socket *nitty_openssh_agent_connect(Plug *plug);
#endif

#endif
