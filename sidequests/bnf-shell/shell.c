#define _POSIX_C_SOURCE 200809L

#include "Absyn.h"     /* AST nodes, free_Input */
#include "Parser.h"    /* psInput */
#include "nl_input.h"  /* nl_input */
#include <ctype.h>     /* isspace, isalpha, isalnum */
#include <errno.h>     /* errno, EINTR, ENOENT */
#include <fcntl.h>     /* open, fcntl, O_*, F_* */
#include <pthread.h>   /* pthread_create, pthread_join, pthread_t */
#include <signal.h>    /* signal, SIGPIPE, SIG_IGN */
#include <spawn.h>     /* posix_spawnp, spawn file actions and attributes */
#include <stdatomic.h> /* atomic_int, atomic_init, atomic_load, atomic_store */
#include <stdio.h>     /* fprintf, perror, stderr */
#include <stdlib.h>    /* calloc, malloc, free, getenv, setenv, strtol */
#include <string.h>    /* strcmp, strdup, strlen, memcpy, memchr, strerror */
#include <sys/stat.h>  /* fstat, struct stat, S_ISFIFO */
#include <sys/wait.h>  /* waitpid, WIFEXITED, WEXITSTATUS, WTERMSIG */
#include <unistd.h>    /* read, write, close, pipe, chdir, _exit, STDIN_* */

#define MAX_STAGES 64
#define MAX_ARGS 256
#define MAX_LINE 65536
#define MAX_VARS 128
#define MAX_JOBS 32
#define MAX_CAPTURE 65536

extern char **environ;

struct Variable {
    char *name;
    char *value;
    int exported;
};

static struct Variable variables[MAX_VARS];
static size_t variable_count;
static int exit_requested;
static int last_status;

struct Stage {
    char *argv[MAX_ARGS + 1];
    int in;
    int out;
    int status;
    int threaded;
    int started;
    atomic_int done;
    pthread_t thread;
    pid_t pid;
    int (*run)(struct Stage *);
};

struct Job {
    struct Stage stages[MAX_STAGES];
    size_t count;
};

static struct Job *background_jobs[MAX_JOBS];
static char *capture_pipeline(InnerPipeline inner);

/*
 * Purpose: Write all bytes to a descriptor.
 * Args: fd is the output; text is a string.
 * Rets: Zero on success, one on an output error.
 * Notes: SIGPIPE is ignored in the shell. EPIPE is an output error.
 */
static int put_text(int fd, const char *text)
{
    size_t left = strlen(text);
    while (left != 0) {
        ssize_t n = write(fd, text, left);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return 1;
        }
        text += n;
        left -= (size_t)n;
    }
    return 0;
}

/*
 * Purpose: Run the small echo command.
 * Args: s supplies owned argument strings and output descriptor.
 * Rets: Zero on success, one on an output error.
 */
static int run_echo(struct Stage *s)
{
    for (size_t i = 1; s->argv[i] != NULL; i++) {
        if ((i > 1 && put_text(s->out, " ")) ||
            put_text(s->out, s->argv[i])) {
            return 1;
        }
    }
    return put_text(s->out, "\n");
}

/*
 * Purpose: Print the registered example command's greeting.
 * Args: s supplies its explicit output descriptor.
 * Rets: Zero on success, one on an output error.
 */
static int run_hello(struct Stage *s)
{
    return put_text(s->out, "Hello from bnf-shell\n");
}

struct InternalCommand {
    const char *name;
    int (*run)(struct Stage *);
};

static const struct InternalCommand commands[] = {
    {"echo", run_echo},
    {"hello", run_hello},
    {NULL, NULL}
};

/*
 * Purpose: Check a variable name before it changes shell state.
 * Args: name is the proposed name.
 * Rets: One for a valid name, zero otherwise.
 */
static int valid_name(const char *name)
{
    if (!isalpha((unsigned char)*name) && *name != '_') {
        return 0;
    }
    for (name++; *name; name++) {
        if (!isalnum((unsigned char)*name) && *name != '_') {
            return 0;
        }
    }
    return 1;
}

/*
 * Purpose: Find a shell variable.
 * Args: name is the name to find.
 * Rets: A borrowed entry, or NULL if no local entry exists.
 */
static struct Variable *find_variable(const char *name)
{
    for (size_t i = 0; i < variable_count; i++) {
        if (strcmp(name, variables[i].name) == 0) {
            return &variables[i];
        }
    }
    return NULL;
}

/*
 * Purpose: Get a local value, an inherited value, or an empty value.
 * Args: name is the variable name.
 * Rets: A borrowed string.
 * Notes: Only the main thread accesses shell variables and environment.
 */
static const char *get_variable(const char *name)
{
    struct Variable *v = find_variable(name);
    const char *value = v ? v->value : getenv(name);
    return value ? value : "";
}

/*
 * Purpose: Set or export one shell variable.
 * Args: name and value are borrowed strings; exporting marks an export.
 * Rets: Zero on success, one on invalid input or a resource error.
 */
static int set_variable(const char *name, const char *value, int exporting)
{
    if (!valid_name(name)) {
        fprintf(stderr, "Invalid variable name: %s\n", name);
        return 1;
    }
    struct Variable *v = find_variable(name);
    char *copy = strdup(value);
    if (!copy) {
        return 1;
    }
    if (!v) {
        if (variable_count == MAX_VARS) {
            free(copy);
            fprintf(stderr, "Variable limit exceeded\n");
            return 1;
        }
        v = &variables[variable_count];
        v->name = strdup(name);
        if (!v->name) {
            free(copy);
            return 1;
        }
        v->exported = getenv(name) != NULL;
        variable_count++;
    }
    if ((exporting || v->exported) && setenv(name, copy, 1) != 0) {
        free(copy);
        return 1;
    }
    free(v->value);
    v->value = copy;
    v->exported |= exporting;
    return 0;
}

/*
 * Purpose: Check whether a command must run in the main thread.
 * Args: name is the command name.
 * Rets: One for a state command, zero otherwise.
 */
static int state_command(const char *name)
{
    return strcmp(name, "cd") == 0 || strcmp(name, "exit") == 0 ||
           strcmp(name, "export") == 0;
}

/*
 * Purpose: Run a state command in the main thread.
 * Args: s supplies already expanded arguments.
 * Rets: Zero on success, or a command error/status.
 */
static int run_state(struct Stage *s)
{
    char **a = s->argv;
    if (strcmp(a[0], "cd") == 0) {
        const char *path = a[1] ? a[1] : getenv("HOME");
        if ((a[1] && a[2]) || !path || chdir(path) != 0) {
            fprintf(stderr, "cd: invalid path or arguments\n");
            return 1;
        }
        return 0;
    }
    if (strcmp(a[0], "export") == 0) {
        if (!a[1] || a[2]) {
            fprintf(stderr, "export: supply one variable name\n");
            return 1;
        }
        return set_variable(a[1], get_variable(a[1]), 1);
    }
    int status = last_status;
    if (a[1]) {
        char *end;
        errno = 0;
        long code = strtol(a[1], &end, 10);
        if (a[2] || errno || *end || !*a[1] || code < 0 || code > 255) {
            fprintf(stderr, "exit: supply a status from 0 to 255\n");
            return 1;
        }
        status = (int)code;
    }
    exit_requested = 1;
    return status;
}

/*
 * Purpose: Call the selected internal command in a worker thread.
 * Args: data is a stage that stays valid until the join.
 * Rets: NULL; store the command status in the stage.
 * Notes: No worker changes the process standard descriptors.
 */
static void *run_internal(void *data)
{
    struct Stage *s = data;
    s->status = s->run(s);
    close(s->in);
    close(s->out);
    atomic_store(&s->done, 1);
    return NULL;
}

/*
 * Purpose: Copy a descriptor and keep it out of spawned programs.
 * Args: fd is the source descriptor.
 * Rets: An owned descriptor, or -1 on error.
 */
static int copy_fd(int fd)
{
    return fcntl(fd, F_DUPFD_CLOEXEC, 3);
}

/*
 * Purpose: Release an unstarted job or a joined job.
 * Args: job owns all stage arguments and unstarted descriptors.
 * Rets: void.
 */
static void free_job(struct Job *job)
{
    for (size_t i = 0; i < job->count; i++) {
        struct Stage *s = &job->stages[i];
        if (s->in >= 0) {
            close(s->in);
        }
        if (s->out >= 0) {
            close(s->out);
        }
        for (size_t a = 0; s->argv[a] != NULL; a++) {
            free(s->argv[a]);
        }
    }
    free(job);
}

/*
 * Purpose: Open a redirection file and reject named pipes without a wait.
 * Args: path is the file name; output selects write and truncation.
 * Rets: An owned blocking descriptor, or -1 on error.
 * Notes: Open without blocking, inspect its type, then clear O_NONBLOCK.
 */
static int open_file(const char *path, int output)
{
    int flags = output ? O_WRONLY | O_CREAT | O_TRUNC : O_RDONLY;
    int fd = open(path, flags | O_CLOEXEC | O_NONBLOCK, 0666);
    if (fd < 0) {
        perror(path);
        return -1;
    }
    struct stat info;
    if (fstat(fd, &info) != 0) {
        perror(path);
        close(fd);
        return -1;
    }
    if (S_ISFIFO(info.st_mode)) {
        fprintf(stderr, "%s: FIFO redirection is not supported\n", path);
        close(fd);
        return -1;
    }
    int mode = fcntl(fd, F_GETFL);
    if (mode < 0 || fcntl(fd, F_SETFL, mode & ~O_NONBLOCK) < 0) {
        perror(path);
        close(fd);
        return -1;
    }
    return fd;
}

/*
 * Purpose: Apply one command's file redirections before any stage starts.
 * Args: s owns the current descriptors; r is the redirection AST.
 * Rets: Zero on success, one on an open failure.
 */
static int redirect_stage(struct Stage *s, OptRedir r)
{
    const char *input = NULL;
    const char *output = NULL;
    switch (r->kind) {
    case is_NoRedir: break;
    case is_InRedir: input = r->u.inRedir_.word_; break;
    case is_OutRedir: output = r->u.outRedir_.word_; break;
    case is_InOutRedir:
        input = r->u.inOutRedir_.word_1;
        output = r->u.inOutRedir_.word_2;
        break;
    case is_OutInRedir:
        output = r->u.outInRedir_.word_1;
        input = r->u.outInRedir_.word_2;
        break;
    }
    if (input != NULL) {
        close(s->in);
        s->in = open_file(input, 0);
        if (s->in < 0) {
            return 1;
        }
    }
    if (output != NULL) {
        close(s->out);
        s->out = open_file(output, 1);
        if (s->out < 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * Purpose: Copy a parsed pipeline into a job and prepare its descriptors.
 * Args: p or inner is the borrowed AST; input/output are end descriptors.
 * Rets: An owned job, or NULL on a limit, allocation, or file error.
 * Notes: Finish all preparation before a thread or program starts.
 */
static struct Job *prepare_pipeline(Pipeline p, InnerPipeline inner,
                                    int input, int output)
{
    struct Job *job = calloc(1, sizeof(*job));
    if (job == NULL) {
        return NULL;
    }
    int previous = copy_fd(input);
    while (p != NULL || inner != NULL) {
        if (job->count == MAX_STAGES || previous < 0) {
            goto fail;
        }
        Command c = p ? (p->kind == is_Single ? p->u.single_.command_ :
                                              p->u.pipe_.command_) : NULL;
        InnerCommand ic = inner ? (inner->kind == is_InnerSingle ?
            inner->u.innerSingle_.innercommand_ :
            inner->u.innerPipe_.innercommand_) : NULL;
        Pipeline next = p && p->kind == is_Pipe ?
            p->u.pipe_.pipeline_ : NULL;
        InnerPipeline inner_next = inner && inner->kind == is_InnerPipe ?
            inner->u.innerPipe_.innerpipeline_ : NULL;
        const char *name = c ? c->u.cmd_.word_ : ic->u.innerCmd_.word_;
        if (state_command(name) && (inner || input != STDIN_FILENO ||
                                   next || job->count != 0)) {
            fprintf(stderr, "State command is not allowed in this job\n");
            goto fail;
        }
        struct Stage *s = &job->stages[job->count++];
        atomic_init(&s->done, 0);
        s->in = previous;
        previous = -1;
        s->out = -1;
        s->argv[0] = strdup(name);
        if (s->argv[0] == NULL) {
            goto fail;
        }
        size_t a = 1;
        for (ListArg arg = c ? c->u.cmd_.listarg_ : NULL; arg;
             arg = arg->listarg_) {
            if (a == MAX_ARGS) {
                goto fail;
            }
            Arg value = arg->arg_;
            s->argv[a] = value->kind == is_Subcommand ?
                capture_pipeline(value->u.subcommand_.innerpipeline_) :
                strdup(value->kind == is_Literal ? value->u.literal_.word_ :
                       get_variable(value->u.variable_.word_));
            if (s->argv[a++] == NULL) {
                goto fail;
            }
        }
        for (ListInnerArg arg = ic ? ic->u.innerCmd_.listinnerarg_ : NULL;
             arg; arg = arg->listinnerarg_) {
            if (a == MAX_ARGS) {
                goto fail;
            }
            InnerArg value = arg->innerarg_;
            s->argv[a] = strdup(value->kind == is_InnerLiteral ?
                value->u.innerLiteral_.word_ :
                get_variable(value->u.innerVariable_.word_));
            if (s->argv[a++] == NULL) {
                goto fail;
            }
        }
        if (next != NULL || inner_next != NULL) {
            int ends[2];
            if (pipe(ends) != 0) {
                goto fail;
            }
            s->out = ends[1];
            previous = ends[0];
            if (fcntl(ends[0], F_SETFD, FD_CLOEXEC) < 0 ||
                fcntl(ends[1], F_SETFD, FD_CLOEXEC) < 0) {
                goto fail;
            }
        } else {
            s->out = copy_fd(output);
        }
        OptRedir r = c ? c->u.cmd_.optredir_ : ic->u.innerCmd_.optredir_;
        if (s->out < 0 || redirect_stage(s, r)) {
            goto fail;
        }
        p = next;
        inner = inner_next;
    }
    return job;
fail:
    if (previous >= 0) {
        close(previous);
    }
    fprintf(stderr, "Cannot prepare pipeline (limit or I/O error)\n");
    free_job(job);
    return NULL;
}

/*
 * Purpose: Start an external stage without an unsafe fork child path.
 * Args: s supplies arguments and descriptors.
 * Rets: Zero on success, or a spawn error number.
 * Notes: Child file actions do not change any shell thread's descriptors.
 */
static int spawn_stage(struct Stage *s)
{
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    int error = posix_spawn_file_actions_init(&actions);
    if (error != 0) {
        return error;
    }
    error = posix_spawnattr_init(&attr);
    if (error != 0) {
        posix_spawn_file_actions_destroy(&actions);
        return error;
    }
    sigset_t defaults;
    sigemptyset(&defaults);
    sigaddset(&defaults, SIGPIPE);
    error = posix_spawnattr_setsigdefault(&attr, &defaults);
    if (!error) {
        error = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF);
    }
    if (!error) {
        error = posix_spawn_file_actions_adddup2(&actions, s->in, 0);
    }
    if (!error) {
        error = posix_spawn_file_actions_adddup2(&actions, s->out, 1);
    }
    if (!error) {
        error = posix_spawnp(&s->pid, s->argv[0], &actions, &attr,
                             s->argv, environ);
    }
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    return error;
}

/*
 * Purpose: Start every stage before waiting for any stage.
 * Args: job is fully prepared and owns each descriptor exactly once.
 * Rets: void; record start failures as stage statuses.
 */
static void start_job(struct Job *job)
{
    for (size_t i = 0; i < job->count; i++) {
        struct Stage *s = &job->stages[i];
        for (size_t c = 0; commands[c].name; c++) {
            if (strcmp(s->argv[0], commands[c].name) == 0) {
                s->run = commands[c].run;
                break;
            }
        }
        s->threaded = s->run != NULL;
        int error = s->threaded ?
            pthread_create(&s->thread, NULL, run_internal, s) :
            spawn_stage(s);
        if (error) {
            fprintf(stderr, "%s: %s\n", s->argv[0], strerror(error));
            s->status = error == ENOENT ? 127 : 126;
            atomic_store(&s->done, 1);
        } else {
            s->started = 1;
        }
        if (!s->threaded || error) {
            close(s->in);
            close(s->out);
        }
        /* A worker owns its copies until return; main must not close them. */
        if (!s->threaded || error) {
            s->in = -1;
            s->out = -1;
        }
    }
}

/*
 * Purpose: Join workers and reap the exact child IDs of one job.
 * Args: job is a started job; blocking selects a full wait or a poll.
 * Rets: Last stage status, or -1 if the job is not yet complete.
 */
static int wait_job(struct Job *job, int blocking)
{
    int pending = 0;
    for (size_t i = 0; i < job->count; i++) {
        struct Stage *s = &job->stages[i];
        if (!s->started) {
            continue;
        }
        if (s->threaded) {
            if (!blocking && !atomic_load(&s->done)) {
                pending = 1;
                continue;
            }
            int error = pthread_join(s->thread, NULL);
            if (error) {
                fprintf(stderr, "pthread_join: %s\n", strerror(error));
                _exit(1);
            }
            s->in = -1;
            s->out = -1;
        } else {
            int status;
            pid_t result;
            do {
                result = waitpid(s->pid, &status, blocking ? 0 : WNOHANG);
            } while (result < 0 && errno == EINTR);
            if (result == 0) {
                pending = 1;
                continue;
            }
            s->status = result < 0 ? 1 : WIFEXITED(status) ?
                WEXITSTATUS(status) : 128 + WTERMSIG(status);
        }
        s->started = 0;
    }
    return pending ? -1 : job->stages[job->count - 1].status;
}

/*
 * Purpose: Reap background stages and release complete jobs.
 * Args: blocking selects shutdown waits instead of nonblocking polls.
 * Rets: void.
 * Notes: Poll between jobs and lines. Wait for all jobs at EOF or exit.
 */
static void reap_jobs(int blocking)
{
    for (size_t i = 0; i < MAX_JOBS; i++) {
        if (background_jobs[i] && wait_job(background_jobs[i], blocking) >= 0) {
            free_job(background_jobs[i]);
            background_jobs[i] = NULL;
        }
    }
}

/*
 * Purpose: Evaluate a backquote pipeline with bounded output capture.
 * Args: inner is a borrowed inner pipeline AST.
 * Rets: An owned string, or NULL for a failed command or capture error.
 * Notes: Read before waiting. No system shell evaluates the text.
 */
static char *capture_pipeline(InnerPipeline inner)
{
    int ends[2];
    if (pipe(ends) != 0) {
        return NULL;
    }
    int input = open_file("/dev/null", 0);
    if (input < 0 || fcntl(ends[0], F_SETFD, FD_CLOEXEC) < 0 ||
        fcntl(ends[1], F_SETFD, FD_CLOEXEC) < 0) {
        if (input >= 0) {
            close(input);
        }
        close(ends[0]);
        close(ends[1]);
        return NULL;
    }
    struct Job *job = prepare_pipeline(NULL, inner, input, ends[1]);
    close(input);
    close(ends[1]);
    if (!job) {
        close(ends[0]);
        return NULL;
    }
    char *text = malloc(MAX_CAPTURE + 1);
    if (!text) {
        close(ends[0]);
        free_job(job);
        return NULL;
    }
    start_job(job);
    size_t count = 0;
    int failed = 0;
    for (;;) {
        char buffer[4096];
        ssize_t n = read(ends[0], buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            failed |= n < 0;
            break;
        }
        if (count + (size_t)n > MAX_CAPTURE ||
            memchr(buffer, '\0', (size_t)n)) {
            failed = 1;
            break;
        }
        memcpy(text + count, buffer, (size_t)n);
        count += (size_t)n;
    }
    close(ends[0]);
    failed |= wait_job(job, 1) != 0;
    free_job(job);
    if (failed) {
        fprintf(stderr,
                "Command substitution failed or output limit exceeded\n");
        free(text);
        return NULL;
    }
    while (count && text[count - 1] == '\n') {
        count--;
    }
    text[count] = '\0';
    return text;
}

/*
 * Purpose: Start one owned background job without waiting for its stages.
 * Args: p is a pipeline AST that can be freed after this call.
 * Rets: Zero on successful preparation, one on a limit or input error.
 */
static int execute_background(Pipeline p)
{
    reap_jobs(0);
    size_t slot = 0;
    while (slot < MAX_JOBS && background_jobs[slot]) {
        slot++;
    }
    if (slot == MAX_JOBS) {
        fprintf(stderr, "Background job limit exceeded\n");
        return 1;
    }
    int input = open_file("/dev/null", 0);
    if (input < 0) {
        return 1;
    }
    struct Job *job = prepare_pipeline(p, NULL, input, STDOUT_FILENO);
    close(input);
    if (!job) {
        return 1;
    }
    start_job(job);
    background_jobs[slot] = job;
    return 0;
}

/*
 * Purpose: Prepare, start, and wait for a foreground pipeline.
 * Args: p is the borrowed pipeline AST.
 * Rets: The last stage status, or one on a preparation error.
 */
static int execute_pipeline(Pipeline p)
{
    /* Reject state commands in pipes before opening files or starting work. */
    for (Pipeline q = p; q != NULL;) {
        Command c = q->kind == is_Single ? q->u.single_.command_ :
                                         q->u.pipe_.command_;
        if (state_command(c->u.cmd_.word_) && p->kind != is_Single) {
            fprintf(stderr, "State command is not allowed in a pipeline\n");
            return 1;
        }
        q = q->kind == is_Single ? NULL : q->u.pipe_.pipeline_;
    }
    reap_jobs(0);
    struct Job *job = prepare_pipeline(p, NULL, STDIN_FILENO, STDOUT_FILENO);
    if (job == NULL) {
        return 1;
    }
    int status;
    if (job->count == 1 && state_command(job->stages[0].argv[0])) {
        status = run_state(&job->stages[0]);
    } else {
        start_job(job);
        status = wait_job(job, 1);
    }
    free_job(job);
    return status;
}

/*
 * Purpose: Evaluate foreground jobs in source order.
 * Args: input is the borrowed input AST.
 * Rets: The final command status, or one for an unsupported form.
 */
static int execute_input(Input input)
{
    switch (input->kind) {
    case is_Assign:
        return set_variable(input->u.assign_.word_1,
                            input->u.assign_.word_2, 0);
    case is_ThenAssign:
        last_status = set_variable(input->u.thenAssign_.word_1,
                                   input->u.thenAssign_.word_2, 0);
        if (last_status) {
            return last_status;
        }
        return execute_input(input->u.thenAssign_.input_);
    case is_If:
        last_status = execute_pipeline(input->u.if_.pipeline_1);
        if (exit_requested || last_status != 0) {
            return exit_requested ? last_status : 0;
        }
        return execute_pipeline(input->u.if_.pipeline_2);
    case is_LastFG:
        return execute_pipeline(input->u.lastFG_.pipeline_);
    case is_LastBG:
        return execute_background(input->u.lastBG_.pipeline_);
    case is_ThenBG:
        last_status = execute_background(input->u.thenBG_.pipeline_);
        return execute_input(input->u.thenBG_.input_);
    case is_ThenFG:
        last_status = execute_pipeline(input->u.thenFG_.pipeline_);
        if (exit_requested) {
            return last_status;
        }
        return execute_input(input->u.thenFG_.input_);
    default:
        fprintf(stderr, "Input form not supported yet\n");
        return 1;
    }
}

/*
 * Purpose: Read one line without stealing bytes from an external command.
 * Args: line has size bytes, including space for the string terminator.
 * Rets: One for a line, zero for EOF, or -1 for a read/limit error.
 * Notes: Discard the rest of a line that exceeds the limit.
 */
static int read_line(char *line, size_t size)
{
    line[0] = '\0';
    size_t count = 0;
    int overflow = 0;
    for (;;) {
        char ch;
        ssize_t n = read(STDIN_FILENO, &ch, 1);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0) {
            return -1;
        }
        if (n == 0 || ch == '\n') {
            line[count] = '\0';
            return overflow ? -1 : n != 0 || count != 0;
        }
        if (count == size - 1 || ch == '\0') {
            overflow = 1;
        } else if (!overflow) {
            line[count++] = ch;
        }
    }
}

/*
 * Purpose: Parse and execute one line at a time.
 * Args: none.
 * Rets: Last command status; syntax or input errors remain nonzero.
 */
int main(void)
{
    char line[MAX_LINE + 1];
    int status = 0;
    int input_error = 0;
    signal(SIGPIPE, SIG_IGN);
    while (!exit_requested) {
        reap_jobs(0);
        errno = 0;
        int result = read_line(line, sizeof(line));
        if (result == 0) {
            break;
        }
        if (result < 0) {
            fprintf(stderr, "Input error or line limit exceeded\n");
            input_error = 1;
            if (errno != 0 || line[0] == '@') {
                break;
            }
            continue;
        }
        if (line[0] == '@') {
            result = nl_input(line, sizeof(line), read_line);
            if (result < 0) {
                input_error = 1;
                break;
            }
            if (result == 0) {
                continue;
            }
        }
        const char *text = line;
        while (isspace((unsigned char)*text)) {
            text++;
        }
        if (*text == '\0') {
            continue;
        }
        Input tree = psInput(line);
        if (tree == NULL) {
            input_error = 1;
            continue;
        }
        status = execute_input(tree);
        last_status = status;
        free_Input(tree);
    }
    reap_jobs(1);
    for (size_t i = 0; i < variable_count; i++) {
        free(variables[i].name);
        free(variables[i].value);
    }
    return input_error ? 1 : status;
}