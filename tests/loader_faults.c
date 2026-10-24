#define _GNU_SOURCE
#include <dirent.h> /* DIR, dirent, dirfd, readdir. */
#include <dlfcn.h>  /* dlsym, RTLD_NEXT. */
#include <errno.h>  /* errno, EIO. */
#include <stdio.h>  /* snprintf. */
#include <stdlib.h> /* getenv. */
#include <string.h> /* memcpy, strlen, strcmp. */
#include <unistd.h> /* readlink. */

/* Purpose: Inject libc-directory enumeration failure at one store path.
 * Args: directory borrows the real enumeration stream.
 * Rets: Real entry, or NULL/EIO for the selected directory suffix.
 */
struct dirent *readdir(DIR *directory)
{
    static struct dirent *(*real_read)(DIR *);
    if (real_read == NULL) {
        void *symbol = dlsym(RTLD_NEXT, "readdir");
        if (symbol == NULL) {
            errno = EIO;
            return NULL;
        }
        memcpy(&real_read, &symbol, sizeof(real_read));
    }
    const char *suffix = getenv("AISHELL_FAIL_READDIR");
    if (suffix != NULL) {
        char link[64], path[4096];
        snprintf(link, sizeof(link), "/proc/self/fd/%d", dirfd(directory));
        ssize_t size = readlink(link, path, sizeof(path) - 1);
        if (size > 0) {
            path[size] = 0;
            size_t n = strlen(suffix), length = (size_t)size;
            if (length >= n && !strcmp(path + length - n, suffix)) {
                errno = EIO;
                return NULL;
            }
        }
    }
    return real_read(directory);
}
