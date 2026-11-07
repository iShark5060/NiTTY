/*
 * Client for the Windows OpenSSH agent pipe.
 *
 * connect_to_named_pipe() rejects any pipe whose owner is not the
 * current user. The Windows OpenSSH Authentication Agent service owns
 * its pipe as SYSTEM, so that check cannot be reused here.
 *
 * ponytail: no owner SID check on this well-known name. ssh.exe does
 * not check either. A different user can squat \\.\pipe\openssh-ssh-agent
 * and see signing requests. Upgrade path: accept only the current
 * user, SYSTEM, and Administrators once GetSecurityInfo is known to
 * succeed against Bitwarden's pipe.
 */

#include <stdlib.h>

#include "putty.h"
#include "nitty_openssh_agent.h"

static bool pipe_present(const char *name)
{
    WIN32_FIND_DATA data;
    HANDLE ffh;

    if (!name)
        return false;
    ffh = FindFirstFile(name, &data);
    if (ffh == INVALID_HANDLE_VALUE)
        return false;
    FindClose(ffh);
    return true;
}

static const char *selected_pipe(void)
{
    const char *env = getenv("SSH_AUTH_SOCK");

    return nitty_openssh_agent_choose(
        env,
        nitty_openssh_agent_path_ok(env) && pipe_present(env),
        pipe_present(NITTY_OPENSSH_AGENT_PIPE));
}

bool nitty_openssh_agent_exists(void)
{
    return selected_pipe() != NULL;
}

static HANDLE open_pipe(const char *pipename, char **err)
{
    HANDLE pipehandle;

    while (1) {
        pipehandle = CreateFile(pipename, GENERIC_READ | GENERIC_WRITE,
                                0, NULL, OPEN_EXISTING,
                                FILE_FLAG_OVERLAPPED, NULL);
        if (pipehandle != INVALID_HANDLE_VALUE)
            return pipehandle;

        if (GetLastError() != ERROR_PIPE_BUSY) {
            *err = dupprintf(
                "Unable to open named pipe '%s': %s",
                pipename, win_strerror(GetLastError()));
            return INVALID_HANDLE_VALUE;
        }

        if (!WaitNamedPipe(pipename, NMPWAIT_USE_DEFAULT_WAIT)) {
            *err = dupprintf(
                "Error waiting for named pipe '%s': %s",
                pipename, win_strerror(GetLastError()));
            return INVALID_HANDLE_VALUE;
        }
    }
}

HANDLE nitty_openssh_agent_open(char **err)
{
    const char *name = selected_pipe();

    if (!name) {
        *err = dupprintf("Windows OpenSSH agent is not running");
        return INVALID_HANDLE_VALUE;
    }
    return open_pipe(name, err);
}

Socket *nitty_openssh_agent_connect(Plug *plug)
{
    char *err = NULL;
    HANDLE pipehandle;

    if (!nitty_openssh_agent_exists())
        return NULL;

    pipehandle = nitty_openssh_agent_open(&err);
    if (pipehandle == INVALID_HANDLE_VALUE) {
        if (!err)
            err = dupprintf("Unable to open Windows OpenSSH agent pipe");
        return new_error_socket_consume_string(plug, err);
    }
    return make_handle_socket(pipehandle, pipehandle, NULL, NULL, 0,
                              plug, true);
}
