#include "cmd_printf.h" /* cmd_spec_t. */
#include "cmd_lines.h"  /* lines_write. */
#include <argtable3.h>  /* Argument definitions, parser, help, cleanup. */
#include <errno.h>      /* errno, ENOMEM, EIO. */
#include <getopt.h>     /* optarg, optind, opterr, optopt. */
#include <stdint.h>     /* SIZE_MAX, uintmax_t, INTMAX_MAX, UINTMAX_MAX. */
#include <limits.h>     /* CHAR_BIT. */
#include <stdio.h>      /* FILE, fputs, fprintf, fflush, ferror, clearerr. */
#include <stdlib.h>     /* malloc, free. */
#include <string.h>     /* strlen, strcmp, strchr, strerror, memset. */

enum { HELP, FORMAT, ARGUMENTS, END, ARG_COUNT };

/* Purpose: Build invocation-local syntax used by parser and usage.
 * Args: table initially NULL; capacity bounds operand storage.
 * Rets: Zero success, -1/ENOMEM on constructor failure.
 */
static int arguments_build(void **table, int capacity)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[FORMAT] = arg_str0(NULL, NULL, "FORMAT", "Byte output format.");
    table[ARGUMENTS] = arg_strn(NULL, NULL, "ARGUMENT", 0, capacity,
                                "Literal conversion operands.");
    table[END] = arg_end(20);
    for (size_t i = 0; i < ARG_COUNT; ++i) {
        if (table[i] == NULL) {
            errno = ENOMEM;
            return -1;
        }
    }
    return 0;
}
/* Purpose: Render stable logical grammar independent of capacity.
 * Args: out and table borrowed.
 * Rets: void; stream error state reports output failure.
 */
static void help_print(FILE *out, void **table)
{
    struct arg_str *format = table[FORMAT], *operands = table[ARGUMENTS];
    int capacity = operands->hdr.maxcount;
    format->hdr.mincount = 1;
    operands->hdr.maxcount = 3;
    fputs("Usage: printf", out);
    arg_print_syntax(out, table, "\n\n");
    arg_print_glossary(out, table, "  %-20s %s\n");
    format->hdr.mincount = 0;
    operands->hdr.maxcount = capacity;
    fputs("\nPrint formatted bytes without an automatic newline.\n"
          "Options precede FORMAT; later arguments are literal data.\n"
          "Use -- before an option-like FORMAT.\n"
          "Conversions: %s %c %d %i %u %o %x %X %%.\n"
          "Literal width up to 4096; - alignment, integer 0 padding;\n"
          "string precision up to 4096. No floats, *, %b, %q or %n.\n"
          "Byte escapes only; total output limit 16 MiB.\n"
          "Exit status: 0 success, 1 operational failure, 2 argument error.\n",
          out);
}
/* Purpose: Map an ASCII hex digit without locale-sensitive classification.
 * Args: byte input character.
 * Rets: 0..15 digit value or -1 if not a hex digit.
 */
static int digit_value(unsigned char byte)
{
    if (byte >= '0' && byte <= '9')
        return byte - '0';
    if (byte >= 'a' && byte <= 'f')
        return byte - 'a' + 10;
    if (byte >= 'A' && byte <= 'F')
        return byte - 'A' + 10;
    return -1;
}
/* Purpose: Decode one bounded FORMAT escape, including embedded NUL.
 * Args: cursor points to backslash; byte receives decoded low byte.
 * Rets: Zero success, -1 malformed/unsupported escape.
 */
static int escape_byte(const char **cursor, unsigned char *byte)
{
    const char *p = *cursor + 1;
    const char *names = "\"\\abefnrtv";
    const unsigned char values[] = {'"', '\\', 7, 8, 27, 12, 10, 13, 9, 11};
    if (*p == '\0')
        return -1;
    const char *named = strchr(names, *p);
    if (named != NULL) {
        *byte = values[(size_t)(named - names)];
        *cursor = p;
        return 0;
    }
    unsigned int value = 0;
    int count = 0, base = 8, maximum = 3;
    if (*p == 'x') {
        ++p;
        base = 16;
        maximum = 2;
    }
    while (count < maximum) {
        int digit = digit_value((unsigned char)*p);
        if (digit < 0 || digit >= base)
            break;
        value = value * (unsigned int)base + (unsigned int)digit;
        ++p;
        ++count;
    }
    if (count == 0)
        return -1;
    *byte = (unsigned char)(value & 255U);
    *cursor = p - 1;
    return 0;
}
/* Purpose: Parse a bounded decimal field size.
 * Args: cursor starts at digits; value receives 0..4096.
 * Rets: Zero success, -1 if the field bound is exceeded.
 */
static int field_size(const char **cursor, size_t *value)
{
    *value = 0;
    while (**cursor >= '0' && **cursor <= '9') {
        *value = *value * 10 + (size_t)(**cursor - '0');
        if (*value > 4096)
            return -1;
        ++*cursor;
    }
    return 0;
}
typedef struct {
    char type;
    int left, zero, precise;
    size_t width, precision;
} conversion_t;
/* Purpose: Validate a conversion before selecting its typed renderer.
 * Args: cursor points after percent; spec receives selected fields.
 * Rets: Zero success, -1 unsupported or malformed conversion.
 */
static int conversion_parse(const char **cursor, conversion_t *spec)
{
    const char *p = *cursor;
    *spec = (conversion_t){0};
    while (*p == '-' || *p == '0') {
        int *flag = *p == '-' ? &spec->left : &spec->zero;
        if (*flag)
            return -1;
        *flag = 1;
        ++p;
    }
    if (field_size(&p, &spec->width))
        return -1;
    if (*p == '.') {
        spec->precise = 1;
        ++p;
        if (field_size(&p, &spec->precision))
            return -1;
    }
    if (*p == '\0' || strchr("scdiuoxX", *p) == NULL)
        return -1;
    spec->type = *p;
    if ((spec->type == 's' || spec->type == 'c') && spec->zero)
        return -1;
    if (spec->precise && spec->type != 's')
        return -1;
    *cursor = p;
    return 0;
}
/* Purpose: Convert strict ASCII integer text without locale or overflow.
 * Args: text borrowed; signed_type selects bounds; outputs sign/magnitude.
 * Rets: Zero success, -1 invalid or out-of-range operand.
 */
static int integer_parse(const char *text, int signed_type, int *negative,
                         uintmax_t *magnitude)
{
    *negative = 0;
    *magnitude = 0;
    if (*text == '\0')
        return 0;
    if (*text == '+' || *text == '-') {
        *negative = *text == '-';
        ++text;
        if (*text == '\0' || (*negative && !signed_type))
            return -1;
    }
    unsigned int base = 10;
    if (text[0] == '0') {
        base = 8;
        if (text[1] == 'x' || text[1] == 'X') {
            base = 16;
            text += 2;
            if (*text == '\0')
                return -1;
        }
    }
    uintmax_t limit = signed_type ? (uintmax_t)INTMAX_MAX : UINTMAX_MAX;
    if (signed_type && *negative)
        ++limit;
    for (; *text; ++text) {
        int digit = digit_value((unsigned char)*text);
        if (digit < 0 || (unsigned int)digit >= base ||
            *magnitude > (limit - (unsigned int)digit) / base)
            return -1;
        *magnitude = *magnitude * base + (unsigned int)digit;
    }
    if (*magnitude == 0)
        *negative = 0;
    return 0;
}
/* Purpose: Encode a magnitude with an optional sign safely.
 * Args: buffer capacity CHAR_BIT*sizeof(uintmax_t)+2; base 8, 10 or 16.
 * Rets: Generated length; no terminator needed.
 */
static size_t integer_bytes(unsigned char *buffer, uintmax_t magnitude,
                            unsigned int base, int negative, int uppercase)
{
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t length = 0;
    do {
        buffer[length++] = (unsigned char)digits[magnitude % base];
        magnitude /= base;
    } while (magnitude);
    if (negative)
        buffer[length++] = '-';
    for (size_t i = 0; i < length / 2; ++i) {
        unsigned char byte = buffer[i];
        buffer[i] = buffer[length - i - 1];
        buffer[length - i - 1] = byte;
    }
    return length;
}
enum { OUTPUT_LIMIT = 16 * 1024 * 1024 };
typedef struct {
    size_t total;
    int writing;
} output_t;
/* Purpose: Count bytes during preflight or write them after validation.
 * Args: output invocation state; bytes borrowed; length exact byte count.
 * Rets: 0 success, 1 I/O failure, 2 output size bound exceeded.
 */
static int output_write(output_t *output, const unsigned char *bytes,
                        size_t length)
{
    if (length > OUTPUT_LIMIT - output->total)
        return 2;
    output->total += length;
    if (output->writing && lines_write(bytes, length))
        return 1;
    return 0;
}
/* Purpose: Write a bounded run of one padding byte.
 * Args: output invocation state; byte padding character; count length.
 * Rets: 0 success, 1 output failure, 2 output bound exceeded.
 */
static int padding_write(output_t *output, unsigned char byte, size_t count)
{
    unsigned char padding[64];
    memset(padding, byte, sizeof(padding));
    while (count) {
        size_t amount = count < sizeof(padding) ? count : sizeof(padding);
        int status = output_write(output, padding, amount);
        if (status != 0)
            return status;
        count -= amount;
    }
    return 0;
}
/* Purpose: Emit a typed field, never an untrusted variadic format.
 * Args: output invocation state; spec validated; argument borrowed.
 * Rets: 0 success, 1 output failure, 2 invalid numeric operand.
 */
static int conversion_write(output_t *output, const conversion_t *spec,
                            const char *argument)
{
    unsigned char buffer[CHAR_BIT * sizeof(uintmax_t) + 2];
    const unsigned char *text = (const unsigned char *)argument;
    size_t length;
    int negative = 0;
    if (spec->type == 's') {
        length = strlen(argument);
        if (spec->precise && length > spec->precision)
            length = spec->precision;
    } else if (spec->type == 'c') {
        length = 1;
    } else {
        uintmax_t magnitude;
        int signed_type = spec->type == 'd' || spec->type == 'i';
        if (integer_parse(argument, signed_type, &negative, &magnitude))
            return 2;
        unsigned int base = spec->type == 'o'                        ? 8U
                            : spec->type == 'x' || spec->type == 'X' ? 16U
                                                                     : 10U;
        length =
            integer_bytes(buffer, magnitude, base, negative, spec->type == 'X');
        text = buffer;
    }
    size_t padding = spec->width > length ? spec->width - length : 0;
    int zero = spec->zero && !spec->left;
    int status = 0;
    if (negative && zero) {
        status = output_write(output, text++, 1);
        if (status != 0)
            return status;
        --length;
    }
    if (!spec->left)
        status = padding_write(output, zero ? '0' : ' ', padding);
    if (status == 0)
        status = output_write(output, text, length);
    if (status == 0 && spec->left)
        status = padding_write(output, ' ', padding);
    return status;
}
/* Purpose: Emit the selected byte format using literal operands.
 * Args: format/operands borrowed; writing selects output or preflight.
 * Rets: 0 success, 1 output failure, 2 format/operand/size error.
 */
static int render_pass(const char *format, const struct arg_str *operands,
                       int writing)
{
    output_t output = {0, writing};
    int next = 0, previous;
    do {
        previous = next;
        for (const char *p = format; *p; ++p) {
            unsigned char byte = (unsigned char)*p;
            if (*p == '\\') {
                if (escape_byte(&p, &byte))
                    return 2;
            } else if (*p == '%') {
                ++p;
                if (*p == '%') {
                    byte = '%';
                } else {
                    conversion_t spec;
                    if (conversion_parse(&p, &spec))
                        return 2;
                    const char *argument =
                        next < operands->count ? operands->sval[next++] : "";
                    int status = conversion_write(&output, &spec, argument);
                    if (status != 0)
                        return status;
                    continue;
                }
            }
            int status = output_write(&output, &byte, 1);
            if (status != 0)
                return status;
        }
    } while (next < operands->count && next != previous);
    return 0;
}
/* Purpose: Validate all output before beginning descriptor writes.
 * Args: format and operands borrowed and stable throughout invocation.
 * Rets: 0 success, 1 output failure, 2 invalid format/operand/output size.
 */
static int render(const char *format, const struct arg_str *operands)
{
    int status = render_pass(format, operands, 0);
    return status != 0 ? status : render_pass(format, operands, 1);
}
/* Purpose: Parse private argv and print bytes or help without host mutation.
 * Args: argc/argv borrowed; every argc element must be a valid C string.
 * Rets: 0 success, 1 operational failure, 2 argument error.
 * Notes: No stdin reads, descriptor ownership, locale or signal changes.
 */
static int printf_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("printf: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            fputs("printf: invalid module arguments\n", stderr);
            return 2;
        }
    }
    void *table[ARG_COUNT] = {0};
    char **copy = NULL;
    int status = 1;
    if (arguments_build(table, argc) != 0)
        goto failure;
    if ((size_t)argc + 2 > SIZE_MAX / sizeof(*copy)) {
        errno = ENOMEM;
        goto failure;
    }
    copy = malloc(((size_t)argc + 2) * sizeof(*copy));
    if (copy == NULL) {
        errno = ENOMEM;
        goto failure;
    }
    /* Preserve leading options; insert -- before the first format operand.
     * Never classify later operands as options, independent of host getopt.
     */
    int next = 0, i = 0;
    copy[next++] = argv[i++];
    while (i < argc && argv[i][0] == '-' && argv[i][1] != '\0' &&
           strcmp(argv[i], "--") != 0)
        copy[next++] = argv[i++];
    copy[next++] = "--";
    if (i < argc && strcmp(argv[i], "--") == 0)
        ++i;
    while (i < argc)
        copy[next++] = argv[i++];
    copy[next] = NULL;
    char *saved_optarg = optarg;
    int saved_optind = optind, saved_opterr = opterr, saved_optopt = optopt;
    int errors = arg_parse(next, copy, table);
    optarg = saved_optarg;
    optind = saved_optind;
    opterr = saved_opterr;
    optopt = saved_optopt;
    if (errors != 0) {
        arg_print_errors(stderr, table[END], "printf");
        status = 2;
        goto done;
    }
    if (((struct arg_lit *)table[HELP])->count != 0) {
        help_print(stdout, table);
        if (fflush(stdout) != 0 || ferror(stdout)) {
            int error = errno == 0 ? EIO : errno;
            clearerr(stdout);
            errno = error;
            goto failure;
        }
        status = 0;
    } else if (((struct arg_str *)table[FORMAT])->count == 0) {
        fputs("printf: missing FORMAT\n", stderr);
        status = 2;
    } else {
        status = render(((struct arg_str *)table[FORMAT])->sval[0],
                        table[ARGUMENTS]);
        if (status == 1)
            goto failure;
        if (status == 2)
            fputs("printf: invalid format or argument\n", stderr);
    }
    goto done;
failure:
    fprintf(stderr, "printf: %s\n", strerror(errno));
done:
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}
/* Purpose: Expose parser-derived usage without persistent parser state.
 * Args: out borrowed valid stream.
 * Rets: void; stream state reports output failure.
 */
static void printf_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table, 3) == 0)
        help_print(out, table);
    else
        fputs("printf: cannot allocate help\n", out);
    arg_freetable(table, ARG_COUNT);
}
cmd_spec_t cmd_printf_spec = {"printf", "Print formatted bytes.",
                              "Print bounded typed byte formats safely.",
                              printf_run, printf_usage};
