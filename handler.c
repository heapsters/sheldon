#include "header.h"

extern sig_atomic_t atomic_fggpid;

/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell
 *    whenever a child job terminates (becomes a zombie), or
 *    stops because it received a SIGSTOP or SIGTSTP signal.
 *    The handler reaps all available zombie children, but
 *    doesn't wait for any other currently running children
 *    to terminate.
 */
void sigchld_handler(int sig)
{
    int status, olderrno = errno;
    sigset_t mask_all, prev_all;
    pid_t pid;
    struct job_t *job;

    Log("REAP [0]\n", 9);

    Sigfillset(&mask_all);

    while (TRUE) {

        pid = waitpid(-1, &status, WNOHANG | WUNTRACED);

        /* `waitpid` returns 0 when no children are left
         * to be reaped, this accounts for stopped processes
         * when error checking at the bottom of
         * the function.
         */
        if (pid == 0) {
            errno = ECHILD;
            break;
        }
        /* `waitpid` returns -1 in the case when
         * there is some other possible error
         * associated when calling it.
         */
        else if (pid < 0) {
            break;
        }

        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

        job = getjobpid(jobs, pid);

        /* If process terminated because of a signal
         *      that was not caught, print out a
         *      status message
         */
        if (WIFSIGNALED(status)) {
            printf("Job [%d] (%d) terminated by signal %d\n",
                job->jid, pid, WTERMSIG(status));
        }

        /*
         * If process was stopped we print out
         *      a message notifying this and continue
         *      before deleting the job.
         */
        if (WIFSTOPPED(status)) {
            printf("Job [%d] (%d) stopped by signal %d\n",
                job->jid, pid, WSTOPSIG(status));
            job->state = ST;
            atomic_fggpid = 0;
            /* Skips remaining portion of loop,
             * including unblocking blocked
             * signals so do it here
             */
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
            continue;
        }

        /* Closes foreground processes which
         * exit without interruption
         * from user sent signals
         */
        if (pid == atomic_fggpid) {
            atomic_fggpid = 0;
        }
        deletejob(jobs, pid);

        Log("REAP [1]\n", 9);

        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }

    if (errno != ECHILD) {
        Sio_error("waitpid error\n", 14);
    }

    Log("REAP [2]\n", 9);

    errno = olderrno;
}

/*
 * sigquit_handler - The kernel sends a SIGINT to the shell
 *    whenever the user types ctrl-c at the keyboard. Catch it
 *    and send it along the foreground job.
 */
void sigint_handler(int sig)
{
    int olderrno = errno;
    sigset_t mask, prev;

    Log("TERM [0]\n", 9);

    Sigfillset(&mask);
    Sigprocmask(SIG_BLOCK, &mask, &prev);

    /* There a no currently running foreground jobs to terminate */
    if (!atomic_fggpid) {
        return;
    }

    Log("TERM [1]\n", 9);

    Kill(-atomic_fggpid, SIGINT);

    Log("TERM [2]\n", 9);

    Sigprocmask(SIG_SETMASK, &prev, NULL);

    errno = olderrno;
}

/*
 * sigtstp_handler - The kernel sends a SIGSTP to the shell
 *    whenever the user types ctrl-z at the keyboard. Catch
 *    it and suspend the foreground job by sending it a
 *    SIGSTP.
 */
void sigtstp_handler(int sig)
{
    int olderrno = errno;
    sigset_t mask, prev;

    Log("STOP [0]\n", 9);

    Sigfillset(&mask);
    Sigprocmask(SIG_BLOCK, &mask, &prev);

    /* There are no currently running foreground jobs to stop */
    if (!atomic_fggpid) {
        return;
    }

    Log("STOP [1]\n", 9);

    Kill(-atomic_fggpid, SIGTSTP);

    Log("STOP [2]\n", 9);

    Sigprocmask(SIG_SETMASK, &prev, NULL);

    errno = olderrno;
}

/*
 * sigquit_handler - The driver program can gracefully terminate
 *    the child shell by sending it a SIGQUIT signal
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}
