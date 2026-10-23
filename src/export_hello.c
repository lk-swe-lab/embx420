#include "cmd_hello.h"    /* cmd_hello_spec. */
#include "cmd_metadata.h" /* cmd_metadata_write. */
#include <stdio.h>        /* stdout, stderr, fputs. */

/* Purpose: Export the package-only command without parsing terminal help.
 * Args: none.
 * Rets: 0 success, 1 on export failure.
 */
int main(void)
{
    if (cmd_metadata_write(stdout, &cmd_hello_spec) != 0) {
        fputs("export-hello: metadata failure\n", stderr);
        return 1;
    }
    return 0;
}
