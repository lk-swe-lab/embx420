#include "command.h"
#include <stddef.h>
#include <string.h>

/* Keep the skeleton's command-building steps; argv borrows token storage. */
void fillCommandStructure(Command *cp, int first, int last, char *sep)
{
    cp->first = first;
    cp->last = last - 1;
    cp->sep = sep;
}

void buildCommandArgumentArray(char *token[], Command *cp)
{
    cp->argv = token + cp->first;
}

int separateCommands(char *token[], Command command[])
{
    int count = 0;
    while (token[count] != NULL) {
        /* Reject metacharacters even when attached to a word. No shell lexer. */
        if (strpbrk(token[count], "|&;<>*?[]\"'`$\\(){}~#") != NULL)
            return -1;
        ++count;
    }
    if (count == 0)
        return 0;
    fillCommandStructure(&command[0], 0, count, NULL);
    command[0].stdin_file = NULL;
    command[0].stdout_file = NULL;
    buildCommandArgumentArray(token, &command[0]);
    return 1;
}
