#ifndef CMD_SPEC_H
#define CMD_SPEC_H

#include <stdio.h>  // FILE *


/* A command (module) specification struct.
 * Single source of truth for 
 * - CLI options and arguments
 * - Help / documentation text
 * - Shell built-in integration
 * - Package metadata (pkg.json, docs in the package)
 */
typedef struct cmd_spec {
    const char *name;       // command name, e.g., "ls"
    const char *summary;    // one-line description
    const char *long_help;  // longer description / Markdown (may be NULL)

    // Main entrypoint: parses args with argtable3 and runs the command.
    int (*run)(int argc, char **argv);

    // Prints usage and option help (using argtable3) to the given stream.
    void (*print_usage)(FILE *out);
} cmd_spec_t;

#endif  // CMD_SPEC_H

