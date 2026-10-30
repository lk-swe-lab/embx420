#include "lean.h"           /* Parse and execute. */
#include "module_runtime.h" /* Module discovery. */
#include <errno.h>          /* errno, EINTR. */
#include <fcntl.h>          /* fcntl, F_GETFD. */
#include <signal.h>         /* sigaction, sig_atomic_t, signal constants. */
#include <stdio.h>          /* fflush, fputs, stdout, stderr. */
#include <unistd.h>         /* read, isatty. */

static volatile sig_atomic_t interrupted;

/* Purpose: Mark INT and retain blocked QUIT without I/O in the handler.
 * Args: number is the received signal.
 * Rets: void.
 */
static void interrupt(int number)
{
    if (number == SIGINT)
        interrupted = 1;
}

/* Purpose: Set signal rules and check standard descriptors.
 * Args: none.
 * Rets: 0 ready, -1 system error.
 */
static int prepare(void)
{
    for (int fd = 0; fd < 3; ++fd)
        if (fcntl(fd, F_GETFD) < 0)
            return -1;
    struct sigaction action = {0};
    sigemptyset(&action.sa_mask);
    action.sa_handler = interrupt;
    if (sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGQUIT, &action, NULL) < 0)
        return -1;
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGTSTP, &action, NULL) < 0 ||
        sigaction(SIGPIPE, &action, NULL) < 0)
        return -1;
    return 0;
}

/* Purpose: Read one line without read-ahead.
 * Args: line receives bytes and a final NUL.
 * Rets: 1 line, 0 EOF, -1 I/O error, -2 invalid line, -3 interrupt.
 */
static int read_line(char line[LEAN_LINE + 1])
{
    size_t used = 0;
    int invalid = 0;
    for (;;) {
        if (interrupted)
            return -3;
        char byte;
        ssize_t got = read(0, &byte, 1);
        if (got < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (got == 0 || byte == '\n') {
            line[used] = '\0';
            return invalid ? -2 : (got == 0 && used == 0 ? 0 : 1);
        }
        if (byte == '\0' || used == LEAN_LINE)
            invalid = 1;
        if (!invalid)
            line[used++] = byte;
    }
}

/* Purpose: Run the foreground shell loop.
 * Args: none.
 * Rets: Last command status.
 */
int main(void)
{
    if (prepare() < 0 || module_runtime_start() != 0) {
        perror("lean: startup");
        return 1;
    }
    int status = 0;
    for (;;) {
        interrupted = 0;
        if (isatty(0) &&
            (fputs("lean> ", stderr) == EOF || fflush(stderr) != 0))
            return 1;
        char line[LEAN_LINE + 1];
        int got = read_line(line);
        if (got == -3) {
            status = 130;
            if (isatty(0))
                fputc('\n', stderr);
            continue;
        }
        if (got == 0)
            return status;
        if (got == -1) {
            perror("lean: input");
            return 1;
        }
        lean_pipeline pipeline;
        if (got == -2 || lean_parse(line, &pipeline) != 0) {
            fputs("lean: syntax error or input limit\n", stderr);
            status = 2;
            continue;
        }
        if (pipeline.count == 0)
            continue;
        int stop;
        status = lean_run(&pipeline, status, &stop);
        if (stop)
            return status;
    }
}
