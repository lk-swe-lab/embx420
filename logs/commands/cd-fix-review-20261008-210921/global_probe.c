#define _POSIX_C_SOURCE 200809L
#include "cmd_cd.h"
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char sentinel[] = "caller optarg";
    char *success[] = {"cd", ".", NULL};
    char *error[] = {"cd", "--bad", NULL};
    char *help[] = {"cd", "--help", NULL};
    char **calls[] = {success, error, help};
    int mismatches = 0;
    for (int i = 0; i < 3; ++i) {
        optind = 41;
        opterr = 73;
        optopt = 89;
        optarg = sentinel;
        int status = cmd_cd_spec.run(2, calls[i]);
        int same = optind == 41 && opterr == 73 && optopt == 89 &&
                   optarg == sentinel;
        printf("case=%d status=%d optind=%d opterr=%d optopt=%d "
               "optarg_preserved=%d all_preserved=%d\n", i, status,
               optind, opterr, optopt, optarg == sentinel, same);
        mismatches += !same;
    }
    return mismatches != 0;
}
