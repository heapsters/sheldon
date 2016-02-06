#ifndef header_h
#define header_h

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

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling action:
 *    FG -> ST  : ctrl-z
 *    ST -> FG  : fg command
 *    ST -> BG  : bg command
 *    BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Helpers */
#define TRUE  1
#define CHILD 0

typedef void handler_t(int);
typedef int jid_t;

struct job_t {                    /* The job struct       */
  pid_t pid;                      /* job PID              */
  jid_t jid;                      /* job ID [1, 2, .. ]   */
  int state;                      /* UNDEF, BG, FG, or ST */
  char cmdline[MAXLINE];          /* command line         */
};

struct job_t jobs[MAXJOBS];

/* Function Prototypes */

/* util.h    */
void unix_error(char *msg);
void app_error(char *msg);
void Log(char *msg, int len);
void eval(char *cmdline);
int parseline(const char *cmdline, char *argv[]);
int builtin_cmd(char *argv[]);
void waitfg(pid_t pid);

/* handler.h */
void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void sigquit_handler(int sig);

/* cmd.h     */
void do_bgfg(char **argv);
void listjobs(struct job_t *jobs);
void usage(void);

/* job.h     */
void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, jid_t jid);

/* wrapper.h */
handler_t *Signal(int signum, handler_t *handler);
void Sigemptyset(sigset_t *set);
void Sigfillset(sigset_t *set);
void Sigaddset(sigset_t *set, int signum);
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
pid_t Fork(void);
ssize_t sio_puts(char *msg, int len);
void Sio_error(char *msg, int len);
void Execve(const char *filename, char *argv[], char *envp[]);
void Setpgid(pid_t, pid_t group);
void Kill(pid_t, int sig);
void Sigsuspend(sigset_t const *mask);
void Sigdelset(sigset_t *mask, int sig);

#endif /* header */
