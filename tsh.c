/*
 * shelldon - A tiny shell program with job control
 *
 * Richard Protasov
 * Eric Voorhis
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE   1024        /* max line size                 */
#define MAXARGS   128         /* max args on a command line    */
#define MAXJOBS   16          /* max jobs at any point in time */
#define MAXID     1<<16       /* max job ID                    */

/* Job states */
#define UNDEF 0     /* undefined             */
#define FG    1     /* running in foreground */
#define BG    2     /* running in background */
#define ST    3     /* stopped               */

/* Helpers */
#define TRUE  1
#define CHILD 0

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling action:
 *    FG -> ST  : ctrl-z
 *    ST -> FG  : fg command
 *    ST -> BG  : bg command
 *    BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Gloabal variables */
extern char **environ;              /* defined in libc                     */
char promt[] = "mpsh> ";            /* command line prompt (DO NOT CHANGE) */
int verbose = 0;                    /* if true, print additional output    */
int nextjid = 1;                    /* next job ID to allocate             */
char sbuf[MAXLINE];                 /* for composing sprintf messages      */
volatile int logger = 0;            /* if true, print logging messages     */

struct job_t {                      /* The job struct       */
    pid_t pid;                      /* job PID              */
    int jid;                        /* job ID [1, 2, .. ]   */
    int state;                      /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];          /* command line         */
};
struct job_t jobs[MAXJOBS];

volatile sig_atomic_t atomic_pid;

/* Function Prototypes */

void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Helper routines provided */
int parseline(const char *cmdline, char **argv);
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

void Sigemptyset(sigset_t *set);
void Sigfillset(sigset_t *set);
void Sigaddset(sigset_t *set, int signum);
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
pid_t Fork(void);
ssize_t sio_puts(char *msg, int len);
void Sio_error(char *msg, int len);
void Execve(const char *filename, char *argv[],
            char *envp[]);

void Log(char msg[], int len);

/*
 * main - The shell's main routine
 */

int main(int argc, char **argv)
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1;    /* emit promt (default) */

    /* Redirect stderr to stdout (so that the driver will)
     * get all output on the pipe connected
     * to stdout)
     */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvpl")) != EOF) {
        switch (c) {
            case 'h':             /* print help message */
                usage();
                break;
            case 'v':             /* emit additional diagnostic info */
                verbose = 1;
                break;
            case 'p':             /* don't print a promt */
                emit_prompt = 0;  /* handy for automatic testing */
                break;
            case 'l':
                logger = ~0;
                break;
            default:
                usage();
        }
    }

    /* Install the signal handlers */

    Signal(SIGINT, sigint_handler);     /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);   /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);   /* Terminated or stopped child */

    /* Provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (TRUE) {
        /* Read command line */
        if (emit_prompt) {
            printf("%s", promt);
            fflush(stdout);
        }

        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
            app_error("fgets error");
        }

        if (feof(stdin)) {    /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    }

    exit(0);    /* control never reaches here */
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
    pid_t pid;
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

        Log("EVAL [3]\n", 9);

        Execve(argv[0], argv, environ);
    }

    Sigprocmask(SIG_BLOCK, &mask_all, NULL);

    state = bg ? BG : FG;

    Log("EVAL [4]\n", 9);

    status = addjob(jobs, pid, state, cmdline);

    Log("EVAL [5]\n", 9);

    jid = getjobpid(jobs, pid)->jid;

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

    if (!strcmp(cmd, "quit")) {
        exit(0);
    }
    if (!strcmp(cmd, "jobs")) {
        listjobs(jobs);
        return 1;
    }
    if (!strcmp(cmd, "fg")) {

        return 1;
    }
    if (!strcmp(cmd, "bg")) {

        return 1;
    }
    if (!strcmp(cmd, "kill")) {

        return 1;
    }

    return 0;
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv)
{
    return;
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

    atomic_pid = 0;
    while (atomic_pid != pid) {
        Log("WAITFG [2]\n", 11);
        sigsuspend(&prev);
    }

    Log("WAITFG [3]\n", 11);

    Sigprocmask(SIG_SETMASK, &prev, NULL);
}

/*****************
 * Signal handlers
 *****************/

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
    int olderrno = errno;
    sigset_t mask_all, prev_all;
    pid_t pid;

    Log("DELETED [0]\n", 12);

    Sigfillset(&mask_all);
    while ((pid = wait(NULL)) > 0) {
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        atomic_pid = pid;
        deletejob(jobs, pid);

        Log("DELETED [1]\n", 12);

        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }

    if (errno != ECHILD) {
        Sio_error("waitpid error", 13);
    }

    errno = olderrno;
}

/*
 * sigquit_handler - The kernel sends a SIGINT to the shell
 *    whenever the user types ctrl-c at the keyboard. Catch it
 *    and send it along the foreground job.
 */
void sigint_handler(int sig)
{
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGSTP to the shell
 *    whenever the user types ctrl-z at the keyboard. Catch
 *    it and suspend the foreground job by sending it a
 *    SIGSTP.
 */
void sigtstp_handler(int sig)
{
    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job)
{
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++) {
        clearjob(&jobs[i]);
    }
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)
{
    int i, max = 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].jid > max) {
            max = jobs[i].jid;
        }
    }
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs,
           pid_t pid, int state,
           char *cmdline)
{
    int i;

    if (pid < 1) {
        return 0;
    }

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == 0) {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = nextjid;
            nextjid++;
            if (nextjid > MAXJOBS) {
                nextjid = 1;
            }
            strcpy(jobs[i].cmdline, cmdline);
            if (verbose) {
                printf("Added job [%d] %d %s",
                    jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1) {
        return 0;
    }

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs)+1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].state == FG) {
            return jobs[i].pid;
        }
    }
    return 0;
}

/* getjobpid - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1) {
        return NULL;
    }

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            return &jobs[i];
        }
    }
    return NULL;
}

/* getjobjid - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid)
{
    int i;

    if (jid < 1) {
        return NULL;
    }

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].jid == jid) {
            return &jobs[i];
        }
    }
    return NULL;
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

/******************************
 * end job list helper routines
 ******************************/


/***********************
* Other helper routines
***********************/

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
 * sigquit_handler - The driver program can gracefully terminate
 *    the child shell by sending it a SIGQUIT signal
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
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
 * Log - emits I/O safe logs whether global
 *      macro LOG is on or off
 */
void Log(char *msg, int len)
{
    sio_puts(msg, len & logger);
}
