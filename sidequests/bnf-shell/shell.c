#define _POSIX_C_SOURCE 200809L

/* BNFC generates these headers from Grammar.cf. */
#include "Absyn.h"   /* Command, ListArg, Pipeline, Input, free_Input, */
                    /* is_Single, is_Pipe, is_LastFG, is_LastBG, */
                    /* is_ThenFG, is_ThenBG, is_Assign, is_Literal, */
                    /* is_NoRedir */
#include "Parser.h"  /* psInput */
#include <ctype.h>   /* isspace */
#include <errno.h>   /* errno, EINTR, ENOENT */
#include <pthread.h> /* pthread_t, pthread_create, pthread_join */
#include <stdio.h>   /* getline, fprintf, perror, ferror, fflush, fputs, */
                    /* fputc, stdin, stdout, stderr, EOF */
#include <stdlib.h>  /* calloc, free, exit, EXIT_SUCCESS, EXIT_FAILURE */
#include <string.h>  /* strcmp, strerror */
#include <sys/types.h> /* pid_t */
#include <sys/wait.h> /* waitpid, WIFEXITED, WEXITSTATUS, WTERMSIG */
#include <unistd.h>  /* fork, execvp, _exit */

/* Share arguments and the result between the main thread and the worker. */
struct InternalTask {
    int (*run)(ListArg);
    ListArg arguments;
    int status;
};

/*
 * Purpose: Print echo arguments with spaces and a final newline.
 * Args: arguments is the argument list from the AST.
 * Rets: Zero on success, or nonzero on an output failure.
 * Notes: Borrow the AST strings. Options and escapes are not supported.
 */
static int run_echo(ListArg arguments)
{
    /* Follow the BNFC argument list. NULL marks the end of the list. */
    for (ListArg arg = arguments; arg != NULL;
         arg = arg->listarg_
    ) {
        /* Put one space before each argument except the first. */
        if (arg != arguments && fputc(' ', stdout) == EOF) {
            return EXIT_FAILURE;
        }
        if (fputs(arg->arg_->u.literal_.word_, stdout) == EOF) {
            return EXIT_FAILURE;
        }
    }
    /* Even an empty argument list produces a newline. Send buffered output. */
    if (fputc('\n', stdout) == EOF || fflush(stdout) == EOF) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/*
 * Purpose: Call the selected internal command from a worker thread.
 * Args: data points to an InternalTask that remains valid until the join.
 * Rets: NULL. Store the command status in the task.
 * Notes: The caller owns the task and the AST strings.
 */
static void *run_internal(void *data)
{
    /* Recover the task type, then call its selected command function. */
    struct InternalTask *task = data;
    task->status = task->run(task->arguments);
    /* Return from this worker only. Do not call exit from the worker. */
    return NULL;
}

/*
 * Purpose: Run a selected internal command in a thread and wait for it.
 * Args: command supplies the argument list from the AST.
 *       run is the function that implements the selected command.
 * Rets: Zero on success, or nonzero on a thread or output failure.
 * Notes: Join before the task leaves scope or the AST is freed.
 */
static int execute_internal(Command command, int (*run)(ListArg))
{
    /* The worker borrows the AST list. This task stays valid until the join. */
    struct InternalTask task = {
        run, command->u.cmd_.listarg_, EXIT_FAILURE
    };
    pthread_t worker;
    /* Start the generic worker. The task selects the command function. */
    int error = pthread_create(&worker, NULL, run_internal, &task);
    if (error != 0) {
        /* pthread functions return an error code. They do not set errno. */
        fprintf(stderr, "pthread_create: %s\n", strerror(error));
        return EXIT_FAILURE;
    }

    /*
     * Wait for the worker to finish. No other command runs during this wait.
     * A successful join makes the worker's task updates visible here.
     * NULL means that we do not need the worker's pointer return value.
     */
    error = pthread_join(worker, NULL);
    if (error != 0) {
        fprintf(stderr, "pthread_join: %s\n", strerror(error));
        /* Stop: the worker could still use the task and the AST. */
        exit(EXIT_FAILURE);
    }
    if (task.status != EXIT_SUCCESS) {
        fprintf(stderr, "%s: internal command failed\n",
                command->u.cmd_.word_);
    }
    /* Read the result only after the worker has finished. */
    return task.status;
}

/*
 * Purpose: Run one external program in a child process.
 * Args: command is the AST from the generated parser.
 * Rets: The command exit status, or a nonzero status on failure.
 * Notes: The argument strings belong to the AST. Do not free them here.
 */
static int execute_external(Command command)
{
    /* Count the command name first, then count each argument list node. */
    size_t count = 1;
    for (ListArg arg = command->u.cmd_.listarg_; arg != NULL;
         arg = arg->listarg_
    ) {
        count++;
    }

    /* Reserve one extra pointer. calloc sets the final pointer to NULL. */
    char **argv = calloc(count + 1, sizeof(*argv));
    if (argv == NULL) {
        perror("allocate arguments");
        return EXIT_FAILURE;
    }
    /*
     * execvp requires the command name at index zero.
     * Copy string pointers from the AST, not the strings themselves.
     * This array is for an external program.
     */
    argv[0] = command->u.cmd_.word_;
    size_t index = 1;
    for (ListArg arg = command->u.cmd_.listarg_; arg != NULL;
         arg = arg->listarg_) {
        argv[index++] = arg->arg_->u.literal_.word_;
    }

    /*
     * fork creates a child with a separate copy of the process memory.
     * It returns zero in the child and the child process ID in the parent.
     * A return value of -1 means that no child was created.
     */
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        free(argv);
        return EXIT_FAILURE;
    }
    if (pid == 0) {
        /*
         * Search PATH for the command and replace this child with it.
         * A successful execvp does not return. The parent remains the shell.
         */
        execvp(argv[0], argv);
        /* execvp returns only on failure. Do not flush copied shell buffers. */
        int code = errno == ENOENT ? 127 : 126;
        perror(argv[0]);
        _exit(code);
    }

    /*
     * Only the parent reaches this point. The child has its own memory.
     * Free the pointer array here. The AST still owns the argument strings.
     */
    free(argv);
    int child_status;
    /* Wait for this child. Retry if a signal interrupts the wait. */
    while (waitpid(pid, &child_status, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return EXIT_FAILURE;
        }
    }
    /* The wait status is encoded. Extract the program's exit status. */
    if (WIFEXITED(child_status)) {
        return WEXITSTATUS(child_status);
    }
    /* A signal terminated the program. Use 128 plus the signal number. */
    return 128 + WTERMSIG(child_status);
}

/*
 * Purpose: Select an internal command or fall back to an external program.
 * Args: command is the AST from the generated parser.
 * Rets: The selected runner's command status.
 * Notes: Keep the AST valid until the runner returns.
 */
static int execute_command(Command command)
{
    /* Reject unsupported forms before a worker or external program starts. */
    if (command->u.cmd_.optredir_->kind != is_NoRedir) {
        fprintf(stderr, "Redirection execution not supported yet\n");
        return EXIT_FAILURE;
    }
    for (ListArg arg = command->u.cmd_.listarg_; arg != NULL;
         arg = arg->listarg_) {
        if (arg->arg_->kind != is_Literal) {
            fprintf(stderr, "Variable expansion not supported yet\n");
            return EXIT_FAILURE;
        }
    }

    /* strcmp returns zero when the command name equals echo. */
    if (strcmp(command->u.cmd_.word_, "echo") == 0) {
        return execute_internal(command, run_echo);
    }
    return execute_external(command);
}

/*
 * Purpose: Execute a single command or reject an unsupported pipeline.
 * Args: pipeline is the AST from the generated parser.
 * Rets: The command status, or failure if execution is not supported.
 * Notes: Parsing supports pipes. Execution does not support pipes yet.
 */
static int execute_pipeline(Pipeline pipeline)
{
    /* Check the constructor before reading its union fields. */
    switch (pipeline->kind) {
    case is_Single:
        return execute_command(pipeline->u.single_.command_);
    case is_Pipe:
        fprintf(stderr, "Pipeline execution not supported yet\n");
        return EXIT_FAILURE;
    }
    fprintf(stderr, "Unknown pipeline constructor\n");
    return EXIT_FAILURE;
}

/*
 * Purpose: Evaluate one foreground job or reject unsupported job forms.
 * Args: input is the complete AST for one input line.
 * Rets: The pipeline status, or failure for an unsupported job form.
 * Notes: Do not start any job from a sequence or a background request yet.
 */
static int execute_input(Input input)
{
    switch (input->kind) {
    case is_Assign:
        fprintf(stderr, "Variable assignment not supported yet\n");
        return EXIT_FAILURE;
    case is_LastFG:
        return execute_pipeline(input->u.lastFG_.pipeline_);
    case is_LastBG:
    case is_ThenFG:
    case is_ThenBG:
        fprintf(stderr,
                "Job sequence or background execution not supported yet\n");
        return EXIT_FAILURE;
    }
    fprintf(stderr, "Unknown input constructor\n");
    return EXIT_FAILURE;
}

/*
 * Purpose: Parse each input line and evaluate its jobs.
 * Args: none
 * Rets: Zero if all commands succeed, or a nonzero status on failure.
 * Notes: Keep a failure status if a later command succeeds.
 *        Release each AST and the input buffer.
 */
int main(void)
{
    char *line = NULL;
    size_t capacity = 0;
    int status = EXIT_SUCCESS;

    while (getline(&line, &capacity, stdin) != -1) {
        /* Skip lines that contain only whitespace. */
        const char *text = line;
        while (*text != '\0' && isspace((unsigned char)*text)) {
            text++;
        }
        if (*text == '\0') {
            continue;
        }

        /*
         * The grammar selects Input as its only entry point.
         * BNFC generates psInput to return an Input AST.
         * Parse this line, not the remaining input stream.
         * A single foreground job has a LastFG node around its pipeline.
         */
        Input tree = psInput(line);
        /* A NULL result means that the input does not match the grammar. */
        if (tree == NULL) {
            fprintf(stderr, "Parse error\n");
            status = EXIT_FAILURE;
            continue;
        }

        /* Wait for the command before reading and running the next line. */
        int command_status = execute_input(tree);
        if (command_status != EXIT_SUCCESS) {
            status = command_status;
        }
        /* BNFC generates this function to free the AST and its contents. */
        free_Input(tree);
    }

    if (ferror(stdin)) {
        perror("read input");
        status = EXIT_FAILURE;
    }
    if (fflush(stdout) == EOF) {
        perror("flush output");
        status = EXIT_FAILURE;
    }
    free(line);
    return status;
}

