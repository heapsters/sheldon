#include "header.h"

extern volatile sig_atomic_t atomic_fggpid;

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv)
{
    char *cmd = argv[0], *opt = argv[1];
    int tofg, pid, jid, restart;
    struct job_t *job;

    tofg = !strcmp(cmd, "fg");

    /* Asserts whether passed arguements
     * follow fg/bg %[JID] or fg/bg PID
     * and that PID is a valid
     * numerical value. JID is asserted
     * later in evaluation.
     */
    if (*opt) {
        pid = atoi(opt);
        jid = (*opt == '%');
        if (!jid && !pid) {
            printf("%s: argument must be a PID or jobid\n",
                (tofg ? "fg" : "bg"));
            return;
        }
    }
    else {
        printf("%s command requires PID or jobid argument\n",
            (tofg ? "fg" : "bg"));
        return;
    }

    /* By default, there is no valid job found */
    job = NULL;

    /* Asserts whether it is a valid PID */
    if (pid) {
        job = getjobpid(jobs, pid);
        if (!job) {
            printf("(%d): No such process\n", pid);
        }
    }
    /* Asserts whether it is a valid JID */
    else if (jid) {
        jid = atoi(opt+1);
        if (!jid) {
            printf("(%s): Invalid JID\n", opt+1);
        }
        job = getjobjid(jobs, jid);
        if (!job) {
            printf("(%d): No such job\n", jid);
        }
    }

    if (!job) {
        return;
    }

    /* Checks whether any additonal values were
     * passed, if so prints an error message and
     * returns.
     */
    if (argv[2] != NULL) {
        printf("%s: Invalid option %s\n",
            (tofg ? "fg" : "bg"), argv[2]);
        return;
    }

    /* Discovers whether the process needs
     * to be awakended through sending it a
     * SIGCONT signal.
     */
    restart = 1;
    if (job->state != ST) {
        restart = 0;
    }

    /* Update the state of the job */
    job->state = (tofg ? FG : BG);

    if (tofg) {
        atomic_fggpid = job->pid;

        if (restart) {
            Kill(job->pid, SIGCONT);
        }
        waitfg(job->pid);
    }
    else {
        if (restart) {
            Kill(job->pid, SIGCONT);
        }
    }
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid != 0) {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state) {
                case BG:
                    printf("Running ");
                    break;
                case FG:
                    printf("Foreground ");
                    break;
                case ST:
                    printf("Stopped ");
                    break;
                default:
                    printf("listjobs: Internal error: job[%d].state=%d ",
                        i, jobs[i].state);
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}

/*
 * usage - print a help message
 */
void usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h  print this message\n");
    printf("   -v  print additional diagnostic information\n");
    printf("   -p  do not emit a command prompt\n");
    printf("   -l  emit logging statements to console\n");
    exit(1);
}
