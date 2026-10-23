#define _POSIX_C_SOURCE 200809L
#include "cmd_rg.h"    /* cmd_spec_t. */
#include "cmd_lines.h" /* lines_write, lines_quote, lines_utf8. */
#include <argtable3.h> /* Argument constructors, parse, help, cleanup. */
#include <dirent.h>    /* DIR, fdopendir, readdir, closedir. */
#include <errno.h>     /* errno, EINTR, ENOMEM, EIO. */
#include <fcntl.h>     /* open, O_RDONLY, O_CLOEXEC, O_NONBLOCK, O_NOFOLLOW,
                       * O_DIRECTORY. */
#include <getopt.h>    /* optarg, optind, opterr, optopt. */
#include <limits.h>    /* INT_MAX. */
#include <stdint.h>    /* SIZE_MAX. */
#include <locale.h>    /* newlocale, uselocale, freelocale, LC_ALL_MASK. */
#include <regex.h>     /* regex_t, regmatch_t, regcomp, regexec, regfree,
                       * REG_EXTENDED, REG_ICASE, REG_NOTBOL, REG_NOMATCH. */
#include <stdio.h>     /* fprintf, fputs, snprintf, fflush, ferror, clearerr. */
#include <stdlib.h>    /* malloc, calloc, realloc, strdup, qsort, free. */
#include <string.h>    /* memcpy, memchr, strcmp, strlen, strchr, strerror,
                       * strdup. */
#include <sys/stat.h>  /* fstat, lstat, struct stat, S_ISREG, S_ISDIR,
                       * S_ISLNK. */
#include <unistd.h>    /* read, close. */

enum {
    HELP,
    NUMBER,
    IGNORE_CASE,
    WORD,
    FIXED,
    JSON,
    CONTEXT,
    OPERANDS,
    END,
    ARG_COUNT
};
enum { FILE_LIMIT = 64 * 1024 * 1024 };

struct search {
    regex_t regex;
    int number;
    int ignore_case;
    int word;
    int fixed;
    int json;
    size_t context;
    const char *pattern;
    int found;
    int failed;
    int output_failed;
};

/* Purpose: Define invocation-local arguments for both parsing and help.
 * Args: table receives entries, maximum bounds positional storage.
 * Rets: Nonzero on constructor failure.
 */
static int arguments_build(void **table, int maximum)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[NUMBER] = arg_lit0("n", NULL, "Show 1-based line numbers.");
    table[IGNORE_CASE] = arg_lit0("i", NULL, "Ignore ASCII case.");
    table[WORD] = arg_lit0("w", NULL, "Require ASCII word boundaries.");
    table[FIXED] = arg_lit0(NULL, "fixed-strings", "Match literal bytes.");
    table[JSON] = arg_lit0(NULL, "json", "Emit one JSON object per match.");
    table[CONTEXT] = arg_str0("C", NULL, "N", "Show N lines either side.");
    table[OPERANDS] = arg_strn(NULL, NULL, "PATTERN PATH", 0, maximum,
                               "Search explicit read-only files/directories.");
    table[END] = arg_end(20);
    return arg_nullcheck(table);
}

/* Purpose: Render stable logical syntax from invocation-owned definitions.
 * Args: out is borrowed; table holds parser objects.
 * Rets: void; stream state reports failures.
 */
static void help_print(FILE *out, void **table)
{
    struct arg_str *operands = table[OPERANDS];
    int maximum = operands->hdr.maxcount;
    operands->hdr.mincount = 1;
    operands->hdr.maxcount = 1;
    fputs("Usage: rg", out);
    arg_print_syntax(out, table, "...\n\n");
    operands->hdr.mincount = 0;
    operands->hdr.maxcount = maximum;
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nLine-local POSIX ERE; explicit paths required; no stdin.\n"
          "ASCII case/word rules. Includes hidden names; no ignore files.\n"
          "Reject explicit links; skip discovered links and NUL files.\n"
          "64 MiB/file, 64 directory levels; no input modifications.\n"
          "JSON: file, line, column, index, text (full line without LF).\n"
          "Line/column are 1-based; index is a 0-based file byte offset.\n"
          "JSON requires UTF-8 and ignores -n/-C.\n"
          "Exit status: 0 matches/help, 1 no matches, 2 error.\n",
          out);
}

/* Purpose: Record an operational error without changing caller state.
 * Args: s is invocation state; path labels the failing operation.
 * Rets: void.
 */
static void failure(struct search *s, const char *path)
{
    fprintf(stderr, "rg: %s: %s\n", path, strerror(errno));
    s->failed = 1;
}

/* Purpose: Emit one textual matching line.
 * Args: s is state; path/line are borrowed; n/length describe the line.
 * Rets: Zero on success, -1 on output failure.
 */
static int text_print(struct search *s, const char *path, const char *line,
                      size_t n, size_t length, int matched)
{
    char number[48];
    const char *separator = matched ? ":" : "-";
    int count =
        snprintf(number, sizeof(number), "%s%zu%s", separator, n, separator);
    if (lines_write((const unsigned char *)path, strlen(path)) != 0 ||
        lines_write((const unsigned char *)(s->number ? number : separator),
                    s->number ? (size_t)count : 1) != 0 ||
        lines_write((const unsigned char *)line, length) != 0 ||
        lines_write((const unsigned char *)"\n", 1) != 0) {
        s->output_failed = 1;
        return -1;
    }
    return 0;
}

/* Purpose: Read a bounded regular file completely before matching.
 * Args: path labels input; length receives byte count.
 * Rets: Owned terminated buffer, or NULL with errno on failure.
 * Notes: Final links/special files refused, only owned fd is closed.
 */
static char *file_read(const char *path, size_t *length)
{
    int fd;
    do {
        fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOFOLLOW);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0)
        return NULL;
    struct stat st;
    char *data = NULL;
    size_t capacity = 4096;
    *length = 0;
    if (fstat(fd, &st) != 0)
        goto done;
    if (!S_ISREG(st.st_mode)) {
        errno = EINVAL;
        goto done;
    }
    if (st.st_size > FILE_LIMIT) {
        errno = EFBIG;
        goto done;
    }
    data = malloc(capacity + 1);
    if (data == NULL)
        goto done;
    for (;;) {
        if (*length == capacity) {
            if (capacity == FILE_LIMIT) {
                char extra;
                ssize_t got;
                do {
                    got = read(fd, &extra, 1);
                } while (got < 0 && errno == EINTR);
                if (got == 0)
                    break;
                if (got > 0)
                    errno = EFBIG;
                goto fail;
            }
            capacity *= 2;
            char *grown = realloc(data, capacity + 1);
            if (grown == NULL)
                goto fail;
            data = grown;
        }
        ssize_t got = read(fd, data + *length, capacity - *length);
        if (got < 0) {
            if (errno == EINTR)
                continue;
            goto fail;
        }
        if (got == 0)
            break;
        *length += (size_t)got;
    }
    data[*length] = '\0';
    goto done;
fail:
    free(data);
    data = NULL;
done:;
    int error = errno;
    if (close(fd) != 0 && data != NULL) {
        error = errno;
        free(data);
        data = NULL;
    }
    errno = error;
    return data;
}

/* Purpose: Classify ASCII word bytes independently of caller locale.
 * Args: ch is one unsigned input byte.
 * Rets: Nonzero for letters, digits and underscore.
 */
static int word_byte(unsigned char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_';
}

/* Purpose: Fold ASCII case without changing non-ASCII bytes.
 * Args: ch is an unsigned byte.
 * Rets: Folded byte.
 */
static unsigned char fold(unsigned char ch)
{
    return ch >= 'A' && ch <= 'Z' ? (unsigned char)(ch + ('a' - 'A')) : ch;
}

/* Purpose: Find the next accepted literal or ERE match.
 * Args: s is state; line is terminated; length/pos bound bytes; a/b receive
 * inclusive start and exclusive end byte offsets.
 * Rets: 1 match, 0 none, -1 regex execution failure.
 * Notes: Rejected word candidates resume one byte after candidate start.
 */
static int match_next(struct search *s, const char *line, size_t length,
                      size_t pos, size_t *a, size_t *b)
{
    while (pos <= length) {
        if (s->fixed) {
            size_t n = strlen(s->pattern);
            int found = 0;
            for (; pos <= length && n <= length - pos; ++pos) {
                size_t j = 0;
                while (j < n) {
                    unsigned char x = (unsigned char)line[pos + j];
                    unsigned char y = (unsigned char)s->pattern[j];
                    if (s->ignore_case) {
                        x = fold(x);
                        y = fold(y);
                    }
                    if (x != y)
                        break;
                    ++j;
                }
                if (j == n) {
                    found = 1;
                    break;
                }
            }
            if (!found)
                return 0;
            *a = pos;
            *b = pos + n;
        } else {
            regmatch_t match;
            int result = regexec(&s->regex, line + pos, 1, &match,
                                 pos == 0 ? 0 : REG_NOTBOL);
            if (result == REG_NOMATCH)
                return 0;
            if (result != 0) {
                errno = EIO;
                return -1;
            }
            *a = pos + (size_t)match.rm_so;
            *b = pos + (size_t)match.rm_eo;
        }
        if (!s->word || ((*a == 0 || !word_byte((unsigned char)line[*a - 1])) &&
                         (*b == length || !word_byte((unsigned char)line[*b]))))
            return 1;
        pos = *a + 1;
    }
    return 0;
}

/* Purpose: Emit the course-oriented JSON record for one individual match.
 * Args: path/line are borrowed UTF-8; length bounds text; n/column/index
 * locate this match using line and byte coordinates.
 * Rets: Zero success, -1 on output failure.
 */
static int json_print(const char *path, const char *line, size_t length,
                      size_t n, size_t column, size_t index)
{
    char fields[160];
    int count =
        snprintf(fields, sizeof(fields),
                 ",\"line\":%zu,\"column\":%zu,\"index\":%zu,\"text\":", n,
                 column, index);
    if (lines_write((const unsigned char *)"{\"file\":", 8) != 0 ||
        lines_quote((const unsigned char *)path, strlen(path)) != 0 ||
        lines_write((const unsigned char *)fields, (size_t)count) != 0 ||
        lines_quote((const unsigned char *)line, length) != 0)
        return -1;
    return lines_write((const unsigned char *)"}\n", 2);
}

/* Purpose: Search one explicit input and print each matching line once.
 * Args: s is invocation state; path is borrowed.
 * Rets: void; state records matching/errors.
 */
static void search_file(struct search *s, const char *path)
{
    size_t length;
    char *data = file_read(path, &length);
    if (data == NULL) {
        failure(s, path);
        return;
    }
    if (memchr(data, '\0', length) != NULL) {
        free(data);
        return;
    }
    if (s->json && !lines_utf8((const unsigned char *)data, length)) {
        errno = EILSEQ;
        failure(s, path);
        free(data);
        return;
    }
    struct line {
        size_t start;
        size_t length;
        int matched;
    };
    size_t count = 0;
    for (size_t i = 0; i < length; ++i)
        if (data[i] == '\n')
            ++count;
    if (length && data[length - 1] != '\n')
        ++count;
    struct line *lines = calloc(count ? count : 1, sizeof(*lines));
    if (lines == NULL) {
        failure(s, path);
        free(data);
        return;
    }
    size_t start = 0;
    for (size_t i = 0; i < count && !s->output_failed; ++i) {
        size_t end = start;
        while (end < length && data[end] != '\n')
            ++end;
        data[end] = '\0';
        lines[i].start = start;
        lines[i].length = end - start;
        size_t a, b, pos = 0;
        int result;
        while ((result = match_next(s, data + start, end - start, pos, &a,
                                    &b)) == 1) {
            s->found = 1;
            lines[i].matched = 1;
            if (!s->json)
                break;
            if (json_print(path, data + start, end - start, i + 1, a + 1,
                           start + a) != 0) {
                s->output_failed = 1;
                break;
            }
            pos = b > a ? b : a + 1;
        }
        if (result < 0)
            failure(s, path);
        start = end + 1;
    }
    size_t printed = 0;
    int any = 0;
    for (size_t i = 0; !s->json && i < count && !s->output_failed; ++i) {
        if (!lines[i].matched)
            continue;
        size_t first = i > s->context ? i - s->context : 0;
        size_t last = s->context < count - 1 - i ? i + s->context : count - 1;
        if (any && first > printed && s->context &&
            lines_write((const unsigned char *)"--\n", 3) != 0) {
            s->output_failed = 1;
            break;
        }
        if (first < printed)
            first = printed;
        for (size_t j = first; j <= last && !s->output_failed; ++j)
            (void)text_print(s, path, data + lines[j].start, j + 1,
                             lines[j].length, lines[j].matched);
        if (last + 1 > printed)
            printed = last + 1;
        any = 1;
    }
    free(lines);
    free(data);
}

/* Purpose: Sort names bytewise for deterministic depth-first traversal.
 * Args: a/b point to stored name pointers.
 * Rets: strcmp ordering.
 */
static int name_compare(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Purpose: Collect a directory completely before searching its children.
 * Args: path is borrowed; count receives the number of owned names.
 * Rets: Owned name vector, or NULL with errno on failure.
 * Notes: Reject final links and detect enumeration/close errors.
 */
static char **directory_read(const char *path, size_t *count)
{
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0)
        return NULL;
    DIR *dir = fdopendir(fd);
    if (dir == NULL) {
        int error = errno;
        (void)close(fd);
        errno = error;
        return NULL;
    }
    size_t capacity = 16;
    char **names = malloc(capacity * sizeof(*names));
    int error = names == NULL ? ENOMEM : 0;
    *count = 0;
    while (!error) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (entry == NULL) {
            error = errno;
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (*count == capacity) {
            if (capacity > SIZE_MAX / sizeof(*names) / 2) {
                error = ENOMEM;
                break;
            }
            capacity *= 2;
            char **grown = realloc(names, capacity * sizeof(*names));
            if (grown == NULL) {
                error = ENOMEM;
                break;
            }
            names = grown;
        }
        names[*count] = strdup(entry->d_name);
        if (names[*count] == NULL) {
            error = ENOMEM;
            break;
        }
        ++*count;
    }
    if (closedir(dir) != 0 && !error)
        error = errno;
    if (error) {
        for (size_t i = 0; i < *count; ++i)
            free(names[i]);
        free(names);
        errno = error;
        return NULL;
    }
    if (*count > 1)
        qsort(names, *count, sizeof(*names), name_compare);
    return names;
}

/* Purpose: Search explicit roots without following final symlinks.
 * Args: s is state; path is borrowed; depth limits directory recursion;
 * explicit distinguishes rejected operands from silently skipped entries.
 * Rets: void; invocation state records failures.
 * Notes: Ancestor link resolution/concurrent changes are not sandboxed.
 */
static void search_path(struct search *s, const char *path, int depth,
                        int explicit)
{
    size_t length = strlen(path);
    if (s->json && !lines_utf8((const unsigned char *)path, length)) {
        errno = EILSEQ;
        failure(s, path);
        return;
    }
    char *trimmed = strdup(path);
    if (trimmed == NULL) {
        failure(s, path);
        return;
    }
    while (length > 1 && trimmed[length - 1] == '/')
        trimmed[--length] = '\0';
    struct stat st;
    int result = lstat(trimmed, &st);
    free(trimmed);
    if (result != 0) {
        failure(s, path);
        return;
    }
    if (S_ISREG(st.st_mode)) {
        search_file(s, path);
        return;
    }
    if (!S_ISDIR(st.st_mode)) {
        if (explicit) {
            errno = S_ISLNK(st.st_mode) ? ELOOP : EINVAL;
            failure(s, path);
        }
        return;
    }
    if (depth >= 64) {
        errno = ELOOP;
        failure(s, path);
        return;
    }
    size_t count;
    char **names = directory_read(path, &count);
    if (names == NULL) {
        failure(s, path);
        return;
    }
    size_t base = strlen(path);
    for (size_t i = 0; i < count; ++i) {
        if (!s->output_failed) {
            size_t n = strlen(names[i]);
            char *child =
                base <= SIZE_MAX - n - 2 ? malloc(base + n + 2) : NULL;
            if (child == NULL) {
                errno = ENOMEM;
                failure(s, path);
            } else {
                memcpy(child, path, base);
                child[base] = '/';
                memcpy(child + base + 1, names[i], n + 1);
                search_path(s, child, depth + 1, 0);
                free(child);
            }
        }
        free(names[i]);
    }
    free(names);
}

/* Purpose: Parse/compile completely before read-only search.
 * Args: argc/argv are borrowed, including pointer array.
 * Rets: 0 matches/help, 1 no matches, 2 errors.
 * Notes: Preserve exported getopt values and caller thread locale.
 */
static int rg_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL)
        return 2;
    for (int i = 0; i < argc; ++i)
        if (argv[i] == NULL)
            return 2;
    void *table[ARG_COUNT] = {0};
    char **copy = NULL;
    locale_t locale = (locale_t)0;
    locale_t previous = (locale_t)0;
    struct search s = {0};
    int compiled = 0;
    int status = 2;
    if (arguments_build(table, argc) != 0) {
        errno = ENOMEM;
        goto error;
    }
    copy = malloc(((size_t)argc + 1) * sizeof(*copy));
    if (copy == NULL)
        goto error;
    memcpy(copy, argv, (size_t)argc * sizeof(*copy));
    copy[argc] = NULL;
    char *saved_optarg = optarg;
    int saved_optind = optind, saved_opterr = opterr, saved_optopt = optopt;
    int errors = arg_parse(argc, copy, table);
    optarg = saved_optarg;
    optind = saved_optind;
    opterr = saved_opterr;
    optopt = saved_optopt;
    if (errors != 0) {
        arg_print_errors(stderr, table[END], "rg");
        goto done;
    }
    if (((struct arg_lit *)table[HELP])->count) {
        help_print(stdout, table);
        status = fflush(stdout) == 0 && !ferror(stdout) ? 0 : 2;
        clearerr(stdout);
        if (status != 0)
            fputs("rg: help output failed\n", stderr);
        goto done;
    }
    struct arg_str *operands = table[OPERANDS];
    if (operands->count < 2 || strchr(operands->sval[0], '\n') != NULL) {
        fputs("rg: require a line-local PATTERN and explicit PATH\n", stderr);
        goto done;
    }
    struct arg_str *context = table[CONTEXT];
    if (context->count) {
        const char *value = context->sval[0];
        if (*value == '\0') {
            fputs("rg: context must be decimal 0..INT_MAX\n", stderr);
            goto done;
        }
        for (; *value != '\0'; ++value) {
            if (*value < '0' || *value > '9' ||
                s.context > ((size_t)INT_MAX - (size_t)(*value - '0')) / 10) {
                fputs("rg: context must be decimal 0..INT_MAX\n", stderr);
                goto done;
            }
            s.context = s.context * 10 + (size_t)(*value - '0');
        }
    }
    locale = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    if (locale == (locale_t)0)
        goto error;
    previous = uselocale(locale);
    if (previous == (locale_t)0)
        goto error;
    s.ignore_case = ((struct arg_lit *)table[IGNORE_CASE])->count;
    s.word = ((struct arg_lit *)table[WORD])->count;
    s.fixed = ((struct arg_lit *)table[FIXED])->count;
    s.json = ((struct arg_lit *)table[JSON])->count;
    s.pattern = operands->sval[0];
    for (int i = 1; i < operands->count; ++i) {
        const char *path = operands->sval[i];
        if (*path == '\0' || (s.json && !lines_utf8((const unsigned char *)path,
                                                    strlen(path)))) {
            fputs("rg: empty or non-UTF-8 JSON PATH\n", stderr);
            goto done;
        }
    }
    if (!s.fixed &&
        regcomp(&s.regex, s.pattern,
                REG_EXTENDED | (s.ignore_case ? REG_ICASE : 0)) != 0) {
        fputs("rg: invalid POSIX extended regex\n", stderr);
        goto done;
    }
    compiled = !s.fixed;
    s.number = ((struct arg_lit *)table[NUMBER])->count;
    for (int i = 1; i < operands->count && !s.output_failed; ++i)
        search_path(&s, operands->sval[i], 0, 1);
    if (s.output_failed)
        failure(&s, "output");
    status = s.failed ? 2 : (s.found ? 0 : 1);
    goto done;
error:
    fprintf(stderr, "rg: %s\n", strerror(errno));
done:
    if (compiled)
        regfree(&s.regex);
    if (previous != (locale_t)0)
        (void)uselocale(previous);
    if (locale != (locale_t)0)
        freelocale(locale);
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}

/* Purpose: Expose parser-derived usage to an embedding caller.
 * Args: out is a borrowed stream.
 * Rets: void; output errors remain on the stream.
 */
static void rg_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table, 1) == 0)
        help_print(out, table);
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_rg_spec = {
    "rg",
    "Search explicit files and directories.",
    "Search read-only inputs using POSIX ERE or literals.",
    rg_run,
    rg_usage,
    NULL};
