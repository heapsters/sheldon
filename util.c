#include "header.h"

extern volatile int logger;
extern char *environ[];
extern volatile sig_atomic_t atomic_fggpid;

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Log - emits I/O safe logs whether global
 *      macro LOG is on or off
 */
void Log(char *msg, int len)
{
    sio_puts(msg, len & logger);
}

/*
 * eval - Evaluate the command line that the user has
 *    just typed in
 *
 * If the user has requested a built-in command (quit, jobs,
 *    bg or fg) then execute it immediately. Otherwise, fork a
 *    child process and run the job in the context of the child.
 *    If the job is running in the foreground, wait for it to
 *    terminate and then return.
 *
 * Note: each child process must have a unique process
 *    group ID so that our background children don't receive
 *    SIGINT (SIGSTP) from the kernel
 *    when we type ctrl-c (ctrl-z) at the keyboard.
 */
void eval(char *cmdline)
{
    char *argv[MAXARGS], buf[MAXLINE];
    int bg, status, state;
    volatile pid_t pid;
    int jid;
    sigset_t mask_all, mask_one, prev_one;

    Log("EVAL [0]\n", 9);

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);

    if (argv[0] == NULL) {
        return;
    }

    Log("EVAL [1]\n", 9);

    if (builtin_cmd(argv)) {
        return;
    }

    Log("EVAL [2]\n", 9);

    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);

    Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);

    pid = Fork();

    if (pid == CHILD) {
        Sigprocmask(SIG_SETMASK, &prev_one, NULL);
        Setpgid(0, 0);
        Log("EVAL [3]\n", 9);
        Execve(argv[0], argv, environ);
    }

    Sigprocmask(SIG_BLOCK, &mask_all, NULL);

    state = bg ? BG : FG;

    Log("EVAL [4]\n", 9);

    status = addjob(jobs, pid, state, cmdline);

    Log("EVAL [5]\n", 9);

    /* Stores jid while process has not been removed */
    jid = getjobpid(jobs, pid)->jid;

    atomic_fggpid = bg ? 0 : pid;

    Log("EVAL [5a]\n", 10);

    Sigprocmask(SIG_SETMASK, &prev_one, NULL);

    Log("EVAL [5b]\n", 10);

    /* Handle errors when generating a new job */
    if (!status) {
        return;
    }

    /* Parent waits for foreground job to terminate */
    if (!bg) {
        Log("EVAL [6]\n", 9);
        waitfg(pid);
    }
    else {
        Log("EVAL [7]\n", 9);
        printf("[%d] (%d) %s", jid, pid, cmdline);
    }
}

/*
 * parseline - Parse the command line and build the argv
 *    array.
 *
 * Characters enclosed in single quotes are treated as a
 *    single argument. Return true if the user has requested
 *    a BG job, false if the user has requested a
 *    FG job.
 */
int parseline(const char *cmdline, char **argv)
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line  */
    char *delim;                /* points to first space delimiter  */
    int argc;
    int bg;

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';   /* replace trailing '\n' with space */

    /* ignore leading spaces */
    while (*buf && (*buf == ' ')) {
        buf++;
    }

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
        buf++;
        delim = strchr(buf, '\'');
    }
    else {
        delim = strchr(buf, ' ');
    }

    while (delim) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;

        /* ignore spaces */
        while (*buf && (*buf == ' ')) {
            buf++;
        }

        if (*buf == '\'') {
            buf++;
            delim = strchr(buf, '\'');
        }
        else {
            delim = strchr(buf, ' ');
        }
    }

    argv[argc] = NULL;

    /* ignore blank line */
    if (argc == 0) {
        return 0;
    }

    /* should the job run in the background? */
    bg = (*argv[argc-1] == '&');
    if (bg != 0) {
        argv[--argc] = NULL;
    }
    return bg;
}

/*
 * builtin_cmd - If the user has typed a built-int
 *    command then execute it immediately.
 *    quit, fg, bg, jobs
 */
int builtin_cmd(char **argv)
{
    char *cmd = argv[0];
    sigset_t mask, prev;

    /* Blocks all signals while determining
     * whether command is built in
     */
     Sigfillset(&mask);
     Sigprocmask(SIG_BLOCK, &mask, &prev);

    if (!strcmp(cmd, "quit")) {
        Sigprocmask(SIG_SETMASK, &prev, NULL);
        exit(0);
    }
    if (!strcmp(cmd, "jobs")) {
        listjobs(jobs);
        Sigprocmask(SIG_SETMASK, &prev, NULL);
        return 1;
    }
    if (!strcmp(cmd, "bg") || !strcmp(cmd, "fg")) {
        Sigprocmask(SIG_SETMASK, &prev, NULL);
        do_bgfg(argv);
        return 1;
    }

    Sigprocmask(SIG_SETMASK, &prev, NULL);
    return 0;
}

/*
 * waitfg - Block until process pid is no longer the
 *    foreground process
 */
void waitfg(pid_t pid)
{
    sigset_t mask, prev;

    Log("WAITFG [0]\n", 11);

    Sigemptyset(&mask);
    Sigaddset(&mask, SIGCHLD);

    Sigprocmask(SIG_BLOCK, &mask, &prev);

    Log("WAITFG [1]\n", 11);

    while (atomic_fggpid == pid) {
        Log("WAITFG [2]\n", 11);
        Sigsuspend(&prev);
    }

    Log("WAITFG [3]\n", 11);

    Sigprocmask(SIG_SETMASK, &prev, NULL);
}
