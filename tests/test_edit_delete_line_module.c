#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_edit_delete_line.h" /* cmd_spec_t. */
#include <argtable3.h>                   /* Constructor types, parser. */
#include <assert.h>                      /* assert. */
#include <dirent.h>                      /* opendir, readdir, closedir. */
#include <errno.h>    /* errno, ENOMEM, EIO, EINTR, ENOSPC. */
#include <fcntl.h>    /* open, fcntl, descriptor flags. */
#include <getopt.h>   /* Exported getopt state. */
#include <signal.h>   /* sigaction, SIGPIPE. */
#include <stdio.h>    /* fdopen, puts, streams, snprintf, dprintf. */
#include <stdlib.h>   /* malloc, calloc, realloc, free, mkdtemp, getenv. */
#include <string.h>   /* memcmp, memcpy, strcmp, strlen. */
#include <sys/stat.h> /* stat, fstat, lstat, modes. */
#include <sys/wait.h> /* waitpid, WIFEXITED, WEXITSTATUS. */
#include <unistd.h>   /* Descriptors, read/write, fork, _exit, cwd, uid. */

enum fault_kind {
    NONE,
    CONSTRUCTOR,
    ALLOCATE,
    OPEN_FAIL,
    OPEN_INTR,
    LSTAT_FAIL,
    FSTAT_FAIL,
    TEMP_FAIL,
    READ_FAIL,
    READ_INTR,
    READ_SHORT,
    WRITE_FAIL,
    WRITE_INTR,
    WRITE_SHORT,
    WRITE_ZERO,
    CHOWN_FAIL,
    CHMOD_FAIL,
    SYNC_FAIL,
    SYNC_INTR,
    FD_FLAG_FAIL,
    CLOSE_FAIL,
    RENAME_FAIL,
    CHANGE_CONTENT,
    CHANGE_PATH,
    CHANGE_MODE,
    CHANGE_LINKS,
    CHANGE_OWNER,
    CHANGE_SIZE,
    CHANGE_MTIME,
    CHANGE_CTIME,
    OUTPUT_FAIL,
    OUTPUT_INTR,
    OUTPUT_SHORT,
    OUTPUT_ZERO,
    CLEANUP_FAIL,
    FOREIGN_OWNER
};
static enum fault_kind fault;
static int countdown, active, allocation_count, descriptors;
static int source_fd = -1, stage_fd = -1, stage_synced, stage_closed;
static int internal_oom, inside_parse, inside_constructor, reorder;
static int rename_calls, case_count, diagnostic_fd = -1;
static void *allocations[1024];
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);
int __real_open(const char *, int, ...);
int __real_close(int);
int __real_mkstemp(char *);
int __real_lstat(const char *, struct stat *);
int __real_fstat(int, struct stat *);
int __real_fcntl(int, int, ...);
int __real_fchown(int, uid_t, gid_t);
int __real_fchmod(int, mode_t);
int __real_fsync(int);
int __real_rename(const char *, const char *);
int __real_unlink(const char *);
ssize_t __real_read(int, void *, size_t);
ssize_t __real_write(int, const void *, size_t);
struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);
struct arg_file *__real_arg_file0(const char *, const char *, const char *,
                                  const char *);
struct arg_str *__real_arg_str0(const char *, const char *, const char *,
                                const char *);
struct arg_end *__real_arg_end(int);
int __real_arg_parse(int, char **, void **);

/* Purpose: Fire a selected deterministic fault exactly once.
 * Args: kind identifies a fault seam.
 * Rets: Nonzero on the selected invocation.
 */
static int hit(enum fault_kind kind)
{
    if (!active || fault != kind || --countdown != 0)
        return 0;
    fault = NONE;
    errno = EIO;
    return 1;
}

/* Purpose: Track module and static-parser allocations.
 * Args: pointer is an allocated object.
 * Rets: The same pointer.
 */
static void *track(void *pointer)
{
    if (active && pointer != NULL) {
        assert(allocation_count < 1024);
        allocations[allocation_count++] = pointer;
    }
    return pointer;
}

/* Purpose: Forget a tracked allocation without freeing it.
 * Args: pointer identifies an allocation.
 * Rets: void.
 */
static void forget(void *pointer)
{
    for (int i = 0; i < allocation_count; ++i) {
        if (allocations[i] == pointer) {
            allocations[i] = allocations[--allocation_count];
            return;
        }
    }
}

/* Purpose: Inject module-owned OOM or characterize internal parser OOM.
 * Args: size is requested bytes.
 * Rets: Allocated pointer or NULL.
 */
void *__wrap_malloc(size_t size)
{
    if ((internal_oom && inside_parse) ||
        (!inside_parse && !inside_constructor && hit(ALLOCATE))) {
        errno = ENOMEM;
        return NULL;
    }
    return track(__real_malloc(size));
}

/* Purpose: Track zero-initialized parser allocations.
 * Args: count/size identify allocation size.
 * Rets: Real allocation.
 */
void *__wrap_calloc(size_t count, size_t size)
{
    return track(__real_calloc(count, size));
}

/* Purpose: Track resized parser allocations.
 * Args: pointer/size identify resizing.
 * Rets: Real resize result.
 */
void *__wrap_realloc(void *pointer, size_t size)
{
    void *result = __real_realloc(pointer, size);
    if (result != NULL || size == 0)
        forget(pointer);
    return track(result);
}

/* Purpose: Track release of module/parser resources.
 * Args: pointer is an owned object.
 * Rets: void.
 */
void __wrap_free(void *pointer)
{
    forget(pointer);
    __real_free(pointer);
}

/* Purpose: Inject a literal constructor NULL without library panic.
 * Args: short/long/glossary define syntax.
 * Rets: Entry or NULL.
 */
struct arg_lit *__wrap_arg_lit0(const char *shortopt, const char *longopt,
                                const char *glossary)
{
    if (hit(CONSTRUCTOR))
        return NULL;
    inside_constructor = 1;
    struct arg_lit *result = __real_arg_lit0(shortopt, longopt, glossary);
    inside_constructor = 0;
    return result;
}

/* Purpose: Inject a file constructor NULL.
 * Args: short/long/type/glossary define syntax.
 * Rets: Entry or NULL.
 */
struct arg_file *__wrap_arg_file0(const char *shortopt, const char *longopt,
                                  const char *type, const char *glossary)
{
    if (hit(CONSTRUCTOR))
        return NULL;
    inside_constructor = 1;
    struct arg_file *result =
        __real_arg_file0(shortopt, longopt, type, glossary);
    inside_constructor = 0;
    return result;
}

/* Purpose: Inject a string constructor NULL.
 * Args: short/long/type/glossary define syntax.
 * Rets: Entry or NULL.
 */
struct arg_str *__wrap_arg_str0(const char *shortopt, const char *longopt,
                                const char *type, const char *glossary)
{
    if (hit(CONSTRUCTOR))
        return NULL;
    inside_constructor = 1;
    struct arg_str *result = __real_arg_str0(shortopt, longopt, type, glossary);
    inside_constructor = 0;
    return result;
}

/* Purpose: Inject an error-table constructor NULL.
 * Args: maximum bounds parse errors.
 * Rets: Entry or NULL.
 */
struct arg_end *__wrap_arg_end(int maximum)
{
    if (hit(CONSTRUCTOR))
        return NULL;
    inside_constructor = 1;
    struct arg_end *result = __real_arg_end(maximum);
    inside_constructor = 0;
    return result;
}

/* Purpose: Exercise real parsing and pointer-copy sensitivity.
 * Args: argc/argv/table are live parser arguments.
 * Rets: Real parse status.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    inside_parse = 1;
    int result = __real_arg_parse(argc, argv, table);
    inside_parse = 0;
    if (reorder && argc > 2) {
        char *swap = argv[1];
        argv[1] = argv[2];
        argv[2] = swap;
    }
    return result;
}

/* Purpose: Ensure opening never truncates/creates source or reads stdin.
 * Args: path/flags describe the source request.
 * Rets: Source fd or deterministic failure/interruption.
 */
int __wrap_open(const char *path, int flags, ...)
{
    if (active) {
        assert((flags & (O_TRUNC | O_CREAT)) == 0);
        assert((flags & O_NOFOLLOW) != 0 && (flags & O_NONBLOCK) != 0);
        assert(strcmp(path, "file") == 0);
    }
    if (hit(OPEN_FAIL))
        return -1;
    if (hit(OPEN_INTR)) {
        errno = EINTR;
        return -1;
    }
    int result = __real_open(path, flags);
    if (active && result >= 0) {
        source_fd = result;
        ++descriptors;
    }
    return result;
}

/* Purpose: Inject preflight failure or final pathname replacement detection.
 * Args: path/info describe pathname metadata.
 * Rets: Real status or failure.
 */
int __wrap_lstat(const char *path, struct stat *info)
{
    if (hit(LSTAT_FAIL))
        return -1;
    int result = __real_lstat(path, info);
    if (result == 0 && hit(FOREIGN_OWNER))
        ++info->st_uid;
    return result;
}

/* Purpose: Inject source metadata/read consistency failures.
 * Args: fd/info describe descriptor metadata.
 * Rets: Real or injected status.
 */
int __wrap_fstat(int fd, struct stat *info)
{
    if (hit(FSTAT_FAIL))
        return -1;
    int result = __real_fstat(fd, info);
    if (result == 0 && stage_synced) {
        if (hit(CHANGE_MODE))
            info->st_mode ^= 0010;
        if (hit(CHANGE_LINKS))
            ++info->st_nlink;
        if (hit(CHANGE_OWNER))
            ++info->st_uid;
        if (hit(CHANGE_SIZE))
            ++info->st_size;
        if (hit(CHANGE_MTIME))
            ++info->st_mtim.tv_nsec;
        if (hit(CHANGE_CTIME))
            ++info->st_ctim.tv_nsec;
    }
    return result;
}

/* Purpose: Verify exclusive temporary creation and track its descriptor.
 * Args: name is the mutable staging template.
 * Rets: Stage fd or failure.
 */
int __wrap_mkstemp(char *name)
{
    assert(strstr(name, ".edit-delete-line-") != NULL);
    if (hit(TEMP_FAIL))
        return -1;
    int result = __real_mkstemp(name);
    if (active && result >= 0) {
        stage_fd = result;
        ++descriptors;
        struct stat info;
        assert(__real_fstat(result, &info) == 0);
        assert((info.st_mode & 0777) == 0600 && info.st_nlink == 1);
    }
    return result;
}

/* Purpose: Inject the module's three-argument descriptor flag request.
 * Args: fd/command/flags are the single supported fcntl call shape.
 * Rets: Real result or failure.
 * Notes: The module only calls F_SETFD; fixtures use __real_fcntl directly.
 */
int __wrap_fcntl(int fd, int command, int flags)
{
    assert(command == F_SETFD && flags == FD_CLOEXEC);
    if (hit(FD_FLAG_FAIL))
        return -1;
    return __real_fcntl(fd, command, flags);
}

/* Purpose: Inject stage group restoration failure.
 * Args: fd/owner/group are the metadata request.
 * Rets: Real result or failure.
 */
int __wrap_fchown(int fd, uid_t owner, gid_t group)
{
    assert(!active || fd == stage_fd);
    return hit(CHOWN_FAIL) ? -1 : __real_fchown(fd, owner, group);
}

/* Purpose: Inject stage mode restoration failure.
 * Args: fd/mode are the metadata request.
 * Rets: Real result or failure.
 */
int __wrap_fchmod(int fd, mode_t mode)
{
    assert(!active || fd == stage_fd);
    return hit(CHMOD_FAIL) ? -1 : __real_fchmod(fd, mode);
}

/* Purpose: Inject sync faults and real concurrent source/path changes.
 * Args: fd is the stage descriptor.
 * Rets: Real result or failure/interruption.
 */
int __wrap_fsync(int fd)
{
    assert(!active || fd == stage_fd);
    if (hit(SYNC_FAIL))
        return -1;
    if (hit(SYNC_INTR)) {
        errno = EINTR;
        return -1;
    }
    int result = __real_fsync(fd);
    if (result == 0)
        stage_synced = 1;
    if (hit(CHANGE_CONTENT))
        assert(pwrite(source_fd, "race", 4, 0) == 4);
    if (hit(CHANGE_PATH)) {
        int other = __real_open("other", O_WRONLY | O_CREAT | O_EXCL, 0600);
        assert(other >= 0 && __real_write(other, "other", 5) == 5);
        assert(__real_close(other) == 0);
        assert(__real_rename("other", "file") == 0);
    }
    return result;
}

/* Purpose: Enforce owned descriptor cleanup even on Linux close errors.
 * Args: fd identifies the descriptor to release.
 * Rets: Real result or deterministic EIO after release.
 */
int __wrap_close(int fd)
{
    if (active) {
        assert(fd == source_fd || fd == stage_fd);
        --descriptors;
        if (fd == stage_fd)
            stage_closed = 1;
    }
    int result = __real_close(fd);
    return hit(CLOSE_FAIL) ? -1 : result;
}

/* Purpose: Exercise interrupted and short reads and forbid borrowed stdin.
 * Args: fd/data/size identify read request.
 * Rets: Real bytes or error.
 */
ssize_t __wrap_read(int fd, void *data, size_t size)
{
    assert(!active || fd == source_fd);
    if (hit(READ_FAIL))
        return -1;
    if (hit(READ_INTR)) {
        errno = EINTR;
        return -1;
    }
    if (hit(READ_SHORT) && size > 1)
        size = 1;
    return __real_read(fd, data, size);
}

/* Purpose: Exercise stage/result transfer errors without mocking copy logic.
 * Args: fd/data/size identify write request.
 * Rets: Real progress or injected result.
 */
ssize_t __wrap_write(int fd, const void *data, size_t size)
{
    if (active) {
        assert(fd != source_fd);
        assert(fd == stage_fd || fd == STDOUT_FILENO);
        int output = fd == STDOUT_FILENO;
        if (hit(output ? OUTPUT_FAIL : WRITE_FAIL))
            return -1;
        if (hit(output ? OUTPUT_INTR : WRITE_INTR)) {
            errno = EINTR;
            return -1;
        }
        if (hit(output ? OUTPUT_SHORT : WRITE_SHORT) && size > 1)
            size = 1;
        if (hit(output ? OUTPUT_ZERO : WRITE_ZERO))
            return 0;
    }
    return __real_write(fd, data, size);
}

/* Purpose: Verify rename occurs only after synced, complete, closed staging.
 * Args: source/target are the rename paths.
 * Rets: Real result or failure.
 */
int __wrap_rename(const char *source, const char *target)
{
    if (active) {
        ++rename_calls;
        assert(stage_synced && stage_closed && descriptors == 0);
        assert(strcmp(target, "file") == 0);
        int check = __real_open(source, O_RDONLY);
        char bytes[64];
        assert(check >= 0 && pread(check, bytes, sizeof(bytes), 0) == 5);
        assert(memcmp(bytes, "keep\n", 5) == 0);
        assert(__real_close(check) == 0);
    }
    return hit(RENAME_FAIL) ? -1 : __real_rename(source, target);
}

/* Purpose: Inject cleanup failure after a staging read failure.
 * Args: path names owned staging.
 * Rets: Real result or failure.
 */
int __wrap_unlink(const char *path)
{
    if (hit(CLEANUP_FAIL))
        return -1;
    return __real_unlink(path);
}

/* Purpose: Create an unlinked capture file inside the scratch fixture.
 * Args: none.
 * Rets: Owned stream; assertions detect fixture failures.
 */
static FILE *capture_open(void)
{
    char name[] = ".capture-XXXXXX";
    int fd = __real_mkstemp(name);
    assert(fd >= 0 && __real_unlink(name) == 0);
    FILE *stream = fdopen(fd, "w+");
    assert(stream != NULL);
    return stream;
}

/* Purpose: Reset capture streams without closing borrowed descriptors.
 * Args: stream is test-owned.
 * Rets: void.
 */
static void reset(FILE *stream)
{
    assert(fflush(stream) == 0);
    assert(ftruncate(fileno(stream), 0) == 0);
    assert(lseek(fileno(stream), 0, SEEK_SET) == 0);
}

/* Purpose: Compare entire captured bytes to an independent expectation.
 * Args: stream is test-owned, expected identifies exact bytes.
 * Rets: void.
 */
static void expect(FILE *stream, const char *expected)
{
    char buffer[8192];
    assert(fflush(stream) == 0);
    ssize_t size = pread(fileno(stream), buffer, sizeof(buffer), 0);
    assert(size >= 0 && (size_t)size == strlen(expected));
    assert(memcmp(buffer, expected, (size_t)size) == 0);
}

/* Purpose: Install original fixture independently of the module.
 * Args: none.
 * Rets: void.
 */
static void fixture(void)
{
    int fd = __real_open("file", O_WRONLY | O_CREAT | O_TRUNC, 0640);
    assert(fd >= 0);
    assert(__real_write(fd, "old\r\nkeep\n", 10) == 10);
    assert(__real_close(fd) == 0);
}

/* Purpose: Verify all owned resources and exported getopt state on return.
 * Args: argc/argv are borrowed invocation arguments.
 * Rets: Module status.
 */
static int invoke(int argc, char **argv)
{
    assert(allocation_count == 0 && descriptors == 0);
    source_fd = stage_fd = -1;
    stage_synced = stage_closed = rename_calls = 0;
    char sentinel[] = "getopt sentinel";
    optarg = sentinel;
    optind = 7;
    opterr = 4;
    optopt = 'Q';
    active = 1;
    int result = cmd_edit_delete_line_spec.run(argc, argv);
    active = 0;
    assert(allocation_count == 0 && descriptors == 0);
    if (optarg != sentinel || optind != 7 || opterr != 4 || optopt != 'Q')
        dprintf(diagnostic_fd, "getopt preservation failed\n");
    assert(optarg == sentinel && optind == 7 && opterr == 4 && optopt == 'Q');
    optarg = NULL;
    ++case_count;
    return result;
}

/* Purpose: Check no staging leftovers and exact source content.
 * Args: expected describes independently required fixture bytes.
 * Rets: void.
 */
static void verify_file(const char *expected)
{
    int fd = __real_open("file", O_RDONLY);
    char buffer[64];
    assert(fd >= 0);
    ssize_t size = pread(fd, buffer, sizeof(buffer), 0);
    if (size < 0 || (size_t)size != strlen(expected))
        dprintf(diagnostic_fd, "source content mismatch\n");
    assert(size >= 0 && (size_t)size == strlen(expected));
    if (memcmp(buffer, expected, (size_t)size) != 0)
        dprintf(diagnostic_fd, "source content mismatch\n");
    assert(memcmp(buffer, expected, (size_t)size) == 0);
    assert(__real_close(fd) == 0);
    DIR *directory = opendir(".");
    assert(directory != NULL);
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL)
        assert(strncmp(entry->d_name, ".edit-delete-line-", 18) != 0);
    assert(closedir(directory) == 0);
}

/* Purpose: Exercise repeated calls, failures, global state and staging safety.
 * Args: none.
 * Rets: Zero after every assertion passes.
 * Notes: Fixture-owned cwd/descriptors/signals are restored at completion.
 */
int main(void)
{
    char *cwd = getcwd(NULL, 0);
    assert(cwd != NULL);
    char directory[4096];
    assert(snprintf(directory, sizeof(directory), "%s/edit-module-XXXXXX",
                    getenv("TMPDIR")) > 0);
    assert(mkdtemp(directory) != NULL && chdir(directory) == 0);
    FILE *out = capture_open(), *err = capture_open(), *input = capture_open();
    FILE *usage = capture_open();
    assert(out && err && input && usage);
    int saved[3] = {dup(0), dup(1), dup(2)};
    assert(saved[0] >= 0 && saved[1] >= 0 && saved[2] >= 0);
    diagnostic_fd = saved[2];
    assert(fputs("untouched stdin", input) >= 0 && fflush(input) == 0);
    assert(dup2(fileno(input), 0) == 0 && lseek(0, 3, SEEK_SET) == 3);
    assert(dup2(fileno(out), 1) == 1 && dup2(fileno(err), 2) == 2);
    mode_t mask = umask(0027);
    struct sigaction before, after;
    assert(sigaction(SIGPIPE, NULL, &before) == 0);
    const char *home = getenv("HOME");
    char *json[] = {"edit-delete-line", "file", "1", "--json", NULL};
    char *help[] = {"edit-delete-line", "--help", NULL};
    char *bad[] = {"edit-delete-line", "file", "0", NULL};
    const char *expected = "{\"path\":\"file\",\"line\":1,\"status\":\"ok\","
                           "\"message\":null}\n";
    reorder = 1;
    for (int repeat = 0; repeat < 100; ++repeat) {
        fixture();
        char *pointers[5];
        memcpy(pointers, json, sizeof(pointers));
        assert(invoke(4, json) == 0);
        if (memcmp(pointers, json, sizeof(pointers)) != 0)
            dprintf(diagnostic_fd, "argv pointer preservation failed\n");
        assert(memcmp(pointers, json, sizeof(pointers)) == 0);
        assert(strcmp(json[0], "edit-delete-line") == 0);
        assert(strcmp(json[1], "file") == 0 && strcmp(json[3], "--json") == 0);
        assert(strcmp(json[2], "1") == 0);
        expect(out, expected);
        verify_file("keep\n");
        reset(out);
        assert(invoke(3, bad) == 2);
        expect(out, "");
        reset(err);
        assert(invoke(2, help) == 0);
        reset(out);
        assert(lseek(0, 0, SEEK_CUR) == 3);
        for (int fd = 0; fd < 3; ++fd)
            assert(__real_fcntl(fd, F_GETFD) >= 0);
        assert(umask(0027) == 0027 && getenv("HOME") == home);
        char *here = getcwd(NULL, 0);
        assert(here != NULL && strcmp(here, directory) == 0);
        free(here);
        assert(sigaction(SIGPIPE, NULL, &after) == 0);
        assert(before.sa_handler == after.sa_handler);
        assert(before.sa_flags == after.sa_flags);
    }
    reorder = 0;
    for (int at = 1; at <= 5; ++at) {
        fixture();
        fault = CONSTRUCTOR;
        countdown = at;
        assert(invoke(4, json) == 1 && fault == NONE);
        expect(out, "");
        verify_file("old\r\nkeep\n");
        reset(err);
    }
    for (int at = 1; at <= 2; ++at) {
        fixture();
        fault = ALLOCATE;
        countdown = at;
        assert(invoke(4, json) == 1 && fault == NONE);
        verify_file("old\r\nkeep\n");
        if (at == 1)
            expect(out, "");
        else {
            expect(out, "{\"path\":\"file\",\"line\":1,\"status\":\"error\","
                        "\"message\":\"Cannot allocate memory\"}\n");
        }
        reset(out);
        reset(err);
    }
    enum fault_kind cases[] = {
        OPEN_FAIL,    OPEN_INTR,    LSTAT_FAIL,   FSTAT_FAIL,   TEMP_FAIL,
        READ_FAIL,    READ_INTR,    READ_SHORT,   WRITE_FAIL,   WRITE_INTR,
        WRITE_SHORT,  WRITE_ZERO,   CHOWN_FAIL,   CHMOD_FAIL,   SYNC_FAIL,
        SYNC_INTR,    FD_FLAG_FAIL, CLOSE_FAIL,   RENAME_FAIL,  CHANGE_CONTENT,
        CHANGE_PATH,  CHANGE_MODE,  CHANGE_LINKS, CHANGE_OWNER, CHANGE_SIZE,
        CHANGE_MTIME, CHANGE_CTIME, OUTPUT_FAIL,  OUTPUT_INTR,  OUTPUT_SHORT,
        OUTPUT_ZERO,  FOREIGN_OWNER};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        fixture();
        fault = cases[i];
        countdown = 1;
        int success = fault == OPEN_INTR || fault == READ_INTR ||
                      fault == READ_SHORT || fault == WRITE_INTR ||
                      fault == WRITE_SHORT || fault == SYNC_INTR ||
                      fault == OUTPUT_INTR || fault == OUTPUT_SHORT;
        int committed = success || fault == OUTPUT_FAIL || fault == OUTPUT_ZERO;
        struct stat original;
        assert(stat("file", &original) == 0);
        int result = invoke(4, json);
        if (!committed && cases[i] != CHANGE_CONTENT &&
            cases[i] != CHANGE_PATH) {
            struct stat current;
            assert(stat("file", &current) == 0);
            assert(original.st_ino == current.st_ino);
            assert(original.st_mode == current.st_mode);
            assert(original.st_uid == current.st_uid);
            assert(original.st_gid == current.st_gid);
            assert(original.st_nlink == current.st_nlink);
            assert(original.st_mtim.tv_sec == current.st_mtim.tv_sec);
            assert(original.st_mtim.tv_nsec == current.st_mtim.tv_nsec);
        }
        if (result != (success ? 0 : 1) || fault != NONE) {
            if (cases[i] >= CHANGE_CONTENT && cases[i] <= CHANGE_CTIME)
                dprintf(saved[2], "concurrency guard failed\n");
            dprintf(saved[2], "fault %d result %d remaining %d\n", cases[i],
                    result, fault);
        }
        assert(result == (success ? 0 : 1) && fault == NONE);
        const char *bytes = committed                    ? "keep\n"
                            : cases[i] == CHANGE_CONTENT ? "race\nkeep\n"
                            : cases[i] == CHANGE_PATH    ? "other"
                                                         : "old\r\nkeep\n";
        verify_file(bytes);
        if (success)
            expect(out, expected);
        reset(out);
        reset(err);
    }
    for (int at = 1; at <= 2; ++at) {
        enum fault_kind late[] = {FSTAT_FAIL, LSTAT_FAIL, CLOSE_FAIL};
        for (size_t i = 0; i < 3; ++i) {
            fixture();
            fault = late[i];
            countdown = at;
            assert(invoke(4, json) == 1 && fault == NONE);
            verify_file("old\r\nkeep\n");
            reset(out);
            reset(err);
        }
    }
    fixture();
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        internal_oom = 1;
        _exit(invoke(4, json));
    }
    int child_status;
    assert(waitpid(child, &child_status, 0) == child);
    assert(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 1);
    dprintf(saved[2], "installed argtable internal parser OOM exits host: %d\n",
            WEXITSTATUS(child_status));
    verify_file("old\r\nkeep\n");
    reset(out);
    reset(err);
    for (int at = 1; at <= 5; ++at) {
        fault = CONSTRUCTOR;
        countdown = at;
        active = 1;
        cmd_edit_delete_line_spec.print_usage(usage);
        active = 0;
        assert(allocation_count == 0 && fault == NONE);
        expect(usage, "");
    }
    active = 1;
    cmd_edit_delete_line_spec.print_usage(usage);
    active = 0;
    assert(allocation_count == 0);
    assert(invoke(2, help) == 0);
    char left[8192], right[8192];
    assert(fflush(usage) == 0 && fflush(out) == 0);
    ssize_t a = pread(fileno(usage), left, sizeof(left), 0);
    ssize_t b = pread(fileno(out), right, sizeof(right), 0);
    assert(a > 0 && a == b && memcmp(left, right, (size_t)a) == 0);
    reset(out);
    fixture();
    char *absent_line[] = {"edit-delete-line", "file", "20", NULL};
    fault = CLEANUP_FAIL;
    countdown = 1;
    assert(invoke(3, absent_line) == 1 && fault == NONE);
    assert(fflush(err) == 0);
    char cleanup_message[8192];
    ssize_t cleanup_length =
        pread(fileno(err), cleanup_message, sizeof(cleanup_message) - 1, 0);
    assert(cleanup_length > 0);
    cleanup_message[cleanup_length] = '\0';
    assert(strstr(cleanup_message, "staging cleanup") != NULL);
    DIR *leftovers = opendir(".");
    assert(leftovers != NULL);
    struct dirent *entry;
    int leftover_count = 0;
    while ((entry = readdir(leftovers)) != NULL) {
        if (strncmp(entry->d_name, ".edit-delete-line-", 18) == 0) {
            ++leftover_count;
            assert(__real_unlink(entry->d_name) == 0);
        }
    }
    assert(closedir(leftovers) == 0 && leftover_count == 1);
    verify_file("old\r\nkeep\n");
    reset(err);
    int full = __real_open("/dev/full", O_WRONLY);
    assert(full >= 0 && dup2(full, 1) == 1);
    assert(invoke(2, help) == 1);
    assert(invoke(4, json) == 1);
    assert(dup2(fileno(out), 1) == 1 && __real_close(full) == 0);
    verify_file("keep\n");
    int pipefds[2];
    assert(pipe(pipefds) == 0 && __real_close(pipefds[0]) == 0);
    struct sigaction ignored = {0};
    ignored.sa_handler = SIG_IGN;
    assert(sigemptyset(&ignored.sa_mask) == 0);
    assert(sigaction(SIGPIPE, &ignored, NULL) == 0);
    assert(dup2(pipefds[1], 1) == 1);
    fixture();
    assert(invoke(4, json) == 1);
    assert(sigaction(SIGPIPE, NULL, &after) == 0);
    assert(after.sa_handler == SIG_IGN);
    assert(sigaction(SIGPIPE, &before, NULL) == 0);
    assert(dup2(fileno(out), 1) == 1 && __real_close(pipefds[1]) == 0);
    verify_file("keep\n");
    reset(out);
    reset(err);
    char *invalid[] = {"edit-delete-line", "file", NULL, NULL};
    assert(invoke(0, NULL) == 2 && invoke(-1, NULL) == 2);
    assert(invoke(1, NULL) == 2 && invoke(3, invalid) == 2);
    fixture();
    verify_file("old\r\nkeep\n");
    for (int fd = 0; fd < 3; ++fd)
        assert(dup2(saved[fd], fd) == fd && __real_close(saved[fd]) == 0);
    assert(fclose(out) == 0 && fclose(err) == 0 && fclose(input) == 0);
    assert(fclose(usage) == 0);
    assert(__real_unlink("file") == 0);
    assert(chdir(cwd) == 0 && rmdir(directory) == 0);
    (void)umask(mask);
    free(cwd);
    printf("edit-delete-line module: %d invocations passed\n", case_count);
    return 0;
}
