/*
 * shelldon - A tiny shell program with job control
 *
 * Richard Protasov
 * Eric Voorhis
 */

#include "header.h"

/*
 * main - The shell's main routine
 */

/* Global variables */
extern char **environ;              /* defined in libc                     */
char promt[] = "mpsh> ";            /* command line prompt (DO NOT CHANGE) */
char verbose = 0;                   /* if true, print additional output    */
jid_t nextjid = 1;                  /* next job ID to allocate             */
char sbuf[MAXLINE];                 /* for composing sprintf messages      */
volatile int logger = 0;            /* if true, print logging messages     */

volatile sig_atomic_t atomic_fggpid = 0;

int main(int argc, char **argv)
{
    char c;
    char cmdline[MAXLINE];
    char emit_prompt = 1;    /* emit promt (default) */

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
