#include "header.h"

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);   /* block sigs of type being handled */
    action.sa_flags = SA_RESTART;   /* restart syscalls if possible     */

    if (sigaction(signum, &action, &old_action) < 0) {
        unix_error("Signal error");
    }

    return (old_action.sa_handler);
}

/*
 * Sigemptyset - wrapper for sigemptyset function
 */
void Sigemptyset(sigset_t *set)
{
    if (sigemptyset(set) < 0) {
        unix_error("Sigemptyset error");
    }
}

/*
 * Sigfillset - wrapper for sigfillset function
 */
void Sigfillset(sigset_t *set)
{
    if (sigfillset(set) < 0) {
        unix_error("Sigfillset error");
    }
}

/*
 * sigaddset - wrapper for sigaddset function
 */
void Sigaddset(sigset_t *set, int signum)
{
    if (sigaddset(set, signum) < 0) {
        unix_error("Sigaddset error");
    }
}

/*
 * Sigprocmask - wrapper for sigprocmask function
 */
void Sigprocmask(int how,
                const sigset_t *set,
                sigset_t *oldset)
{
    if (sigprocmask(how, set, oldset) < 0) {
        unix_error("Sigprocmask error");
    }
}

/*
 * Fork - wrapper for fork function
 */
pid_t Fork(void)
{
    pid_t pid;

    if ((pid = fork()) < 0) {
        unix_error("Fork error");
    }
    return pid;
}

/*
 * sio_puts - I/O ansynchronous safe function
 */
ssize_t sio_puts(char *msg, int len)
{
    return write(STDOUT_FILENO, msg, len);
}

/*
 * sio_error - I/O ansynchronous safe function prints error
 *      message and exits
 */
void Sio_error(char *msg, int len)
{
    sio_puts(msg, len);
    _exit(1);
}

/*
 * Execve - wrapper for execve function
 */
void Execve(const char *filename,
            char *argv[],
            char *envp[])
{
    if (execve(filename, argv, envp) < 0) {
        printf("%s: Command not found.\n", argv[0]);
        exit(0);
    }
}

/*
 * Setpgid - wrapper for setpgid function
 */
void Setpgid(pid_t pid, pid_t group)
{
    if (setpgid(pid, group) < 0) {
        unix_error("Setpgid error");
    }
}

/*
 * Kill - wrapper for kill function
 */
void Kill(pid_t pid, int sig)
{
    if (kill(pid, sig) < 0) {
        unix_error("Kill error");
    }
}

/*
 * Sigsuspend - wrapper for sigsuspend function
 */
void Sigsuspend(sigset_t const *mask)
{
    int olderrno = errno;
    sigsuspend(mask);
    if (errno != EINTR) {
        unix_error("Sigsuspend error");
    }
    errno = olderrno;
}

/*
 * Sigdelset - wrapper for sigdelset function
 */
void Sigdelset(sigset_t *mask, int sig)
{
    if (sigdelset(mask, sig) < 0) {
        unix_error("Sigdelset error");
    }
}
