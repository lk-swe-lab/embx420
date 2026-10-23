#define _GNU_SOURCE
#include <assert.h>   /* assert. */
#include <fcntl.h>    /* openat, O_*, AT_FDCWD. */
#include <stdio.h>    /* fprintf, stderr. */
#include <sys/stat.h> /* fstat, mode_t. */
#include <unistd.h>   /* close, unlinkat. */

/* Purpose: Verify real variadic open forwarding with and without a mode.
 * Args: argv[1] is a private empty test directory.
 * Rets: 0 success; assertions identify forwarding defects.
 */
int main(int argc, char **argv)
{
    assert(argc == 2);
    int dir = openat(AT_FDCWD, argv[1], O_RDONLY | O_DIRECTORY);
    assert(dir >= 0);
    struct stat st;
    for (int i = 0; i < 2; ++i) {
        int fd =
            i ? openat(dir, "created", O_RDWR | O_CREAT | O_EXCL, (mode_t)0600)
              : openat(dir, "literal", O_RDWR | O_CREAT | O_EXCL, 0600);
        assert(fd >= 0 && fstat(fd, &st) == 0);
        assert((st.st_mode & 0777) == 0600);
        assert(close(fd) == 0);
        fd = openat(dir, i ? "created" : "literal", O_RDONLY);
        assert(fd >= 0 && close(fd) == 0);
    }
    int fd = openat(dir, ".", O_TMPFILE | O_RDWR, (mode_t)0600);
    assert(fd >= 0 && fstat(fd, &st) == 0);
    if ((st.st_mode & 0777) != 0600)
        fprintf(stderr, "openat O_TMPFILE lost variadic mode\n");
    assert((st.st_mode & 0777) == 0600);
    assert(close(fd) == 0);
    assert(unlinkat(dir, "created", 0) == 0);
    assert(unlinkat(dir, "literal", 0) == 0);
    assert(close(dir) == 0);
    return 0;
}
