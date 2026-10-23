#define _POSIX_C_SOURCE 200809L
#include "pkg_fs.h"   /* Safe directory and root API. */
#include <errno.h>    /* errno, ENOENT, EINVAL. */
#include <stdint.h>   /* uintmax_t. */
#include <fcntl.h>    /* openat, open, fcntl, O_*, struct flock. */
#include <stdio.h>    /* snprintf. */
#include <stdlib.h>   /* malloc, free, getenv. */
#include <string.h>   /* strlen, strcmp, strtok_r, strdup, strrchr. */
#include <sys/stat.h> /* mkdirat, fstat, fstatat, fchmod, fchmodat. */
#include <unistd.h>   /* close, geteuid, read, write. */

/* Purpose: Open a directory component without following a symbolic link.
 * Args: parent is borrowed; create makes a missing private directory;
 *       managed requires private ownership and write permissions.
 * Rets: Owned descriptor, or -1 with errno.
 */
int pkg_child_dir(int parent, const char *name, int create, int managed)
{
    if (create) {
        int made = mkdirat(parent, name, 0700);
        if (made != 0 && errno != EEXIST)
            return -1;
        if (made == 0 && fchmodat(parent, name, 0700, AT_SYMLINK_NOFOLLOW) != 0)
            return -1;
    }
    int fd =
        openat(parent, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        return -1;
    struct stat st;
    if (fstat(fd, &st) != 0 ||
        (managed && (st.st_uid != geteuid() || (st.st_mode & 0022) != 0))) {
        close(fd);
        errno = EINVAL;
        return -1;
    }
    return fd;
}

/* Purpose: Resolve every directory path component without symlinks.
 * Args: path is an absolute or relative borrowed path.
 * Rets: Owned descriptor, or -1.
 */
int pkg_dir_open(const char *path)
{
    if (path == NULL || *path == '\0') {
        errno = EINVAL;
        return -1;
    }
    char *copy = strdup(path);
    if (copy == NULL)
        return -1;
    int fd =
        open(path[0] == '/' ? "/" : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    char *save = NULL;
    for (char *part = strtok_r(copy, "/", &save); part != NULL && fd >= 0;
         part = strtok_r(NULL, "/", &save)) {
        int next = pkg_child_dir(fd, part, 0, 0);
        int saved = errno;
        if (close(fd) != 0 && next >= 0) {
            close(next);
            next = -1;
        }
        fd = next;
        errno = saved;
    }
    free(copy);
    return fd;
}

/* Purpose: Test physical ancestry using directory identities, not spelling.
 * Args: ancestor and child borrow directory descriptors.
 * Rets: 1 ancestor or equal, 0 disjoint, -1 on observation/close failure.
 */
static int ancestor_of(int ancestor, int child)
{
    struct stat wanted, current, parent;
    if (fstat(ancestor, &wanted) != 0)
        return -1;
    int fd = pkg_child_dir(child, ".", 0, 0);
    if (fd < 0)
        return -1;
    int result = -1;
    for (;;) {
        if (fstat(fd, &current) != 0)
            break;
        if (wanted.st_dev == current.st_dev &&
            wanted.st_ino == current.st_ino) {
            result = 1;
            break;
        }
        int next = pkg_child_dir(fd, "..", 0, 0);
        if (next < 0)
            break;
        int observed = fstat(next, &parent);
        int closed = close(fd);
        fd = next;
        if (observed != 0 || closed != 0)
            break;
        if (parent.st_dev == current.st_dev &&
            parent.st_ino == current.st_ino) {
            result = 0;
            break;
        }
    }
    if (close(fd) != 0)
        result = -1;
    return result;
}

/* Purpose: Reject source/managed-root overlap before any state mutation.
 * Args: source borrows an already no-follow opened source directory.
 * Rets: 0 disjoint, -1 overlap, unsafe HOME/parents or observation error.
 * Notes: Missing root suffixes are checked from their existing ancestors.
 */
int pkg_install_disjoint(int source)
{
    const char *home = getenv("HOME");
    if (home == NULL || home[0] != '/')
        return -1;
    int fd = pkg_dir_open(home);
    if (fd < 0)
        return -1;
    int result = -1;
    const char *parts[] = {".mysh", "pkgs"};
    for (size_t i = 0;; ++i) {
        if (ancestor_of(source, fd) != 0)
            break;
        if (i == 2) {
            result = ancestor_of(fd, source) == 0 ? 0 : -1;
            break;
        }
        int next = pkg_child_dir(fd, parts[i], 0, 1);
        if (next < 0) {
            result = errno == ENOENT ? 0 : -1;
            break;
        }
        int closed = close(fd);
        fd = next;
        if (closed != 0)
            break;
    }
    if (close(fd) != 0)
        result = -1;
    return result;
}

/* Purpose: Close the persistent lock and root descriptors.
 * Args: root owns its descriptors and path.
 * Rets: 0 success, -1 on any close failure.
 */
int pkg_root_close(pkg_root *root)
{
    int status = 0;
    if (root->lock >= 0 && close(root->lock) != 0)
        status = -1;
    if (root->root >= 0 && close(root->root) != 0)
        status = -1;
    free(root->path);
    *root = (pkg_root){-1, -1, NULL};
    return status;
}

/* Purpose: Open or create managed state and take its exclusive record lock.
 * Args: root receives ownership; create permits creation of missing roots.
 * Rets: 0 success; 1 absent read-only root; -1 invalid, busy or I/O failure.
 */
int pkg_root_open(pkg_root *root, int create)
{
    *root = (pkg_root){-1, -1, NULL};
    const char *home = getenv("HOME");
    if (home == NULL || home[0] != '/') {
        errno = EINVAL;
        return -1;
    }
    int fd = pkg_dir_open(home);
    if (fd < 0)
        return -1;
    const char *parts[] = {".mysh", "pkgs"};
    for (size_t i = 0; i < 2; ++i) {
        int next = pkg_child_dir(fd, parts[i], create, 1);
        int saved = errno;
        int closed = close(fd);
        if (next < 0) {
            errno = saved;
            return !create && saved == ENOENT ? 1 : -1;
        }
        if (closed != 0) {
            close(next);
            return -1;
        }
        fd = next;
    }
    root->root = fd;
    size_t n = strlen(home);
    if (n > (size_t)-1 - 13)
        goto fail;
    root->path = malloc(n + 13);
    if (root->path == NULL)
        goto fail;
    snprintf(root->path, n + 13, "%s/.mysh/pkgs", home);
    root->lock = openat(
        fd, ".lock",
        O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600);
    if (root->lock >= 0) {
        if (fchmod(root->lock, 0600) != 0)
            goto fail;
    } else if (errno == EEXIST) {
        root->lock =
            openat(fd, ".lock", O_RDWR | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    }
    struct stat st;
    if (root->lock < 0 || fstat(root->lock, &st) != 0 || !S_ISREG(st.st_mode) ||
        st.st_nlink != 1 || st.st_uid != geteuid() || (st.st_mode & 0022) != 0)
        goto fail;
    struct flock lock = {
        .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
    if (fcntl(root->lock, F_SETLK, &lock) != 0)
        goto fail;
    return 0;
fail:
    pkg_root_close(root);
    return -1;
}

/* Purpose: Open a safe relative payload parent, optionally creating it.
 * Args: dir is borrowed; path is already validated; leaf receives ownership.
 * Rets: Owned parent descriptor, or -1.
 */
int pkg_parent_open(int dir, const char *path, int create, char **leaf)
{
    *leaf = NULL;
    char *copy = strdup(path);
    if (copy == NULL)
        return -1;
    char *last = strrchr(copy, '/');
    int fd = openat(dir, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (last != NULL) {
        *last = '\0';
        char *save = NULL;
        for (char *p = strtok_r(copy, "/", &save); p && fd >= 0;
             p = strtok_r(NULL, "/", &save)) {
            int next = pkg_child_dir(fd, p, create, create);
            int saved = errno;
            if (close(fd) != 0 && next >= 0) {
                close(next);
                next = -1;
            }
            fd = next;
            errno = saved;
        }
    }
    if (fd >= 0) {
        *leaf = strdup(last == NULL ? copy : last + 1);
        if (*leaf == NULL) {
            close(fd);
            fd = -1;
        }
    }
    free(copy);
    return fd;
}

/* Purpose: Compare file observations including change timestamps.
 * Args: a and b are borrowed observations.
 * Rets: 1 same, 0 changed.
 */
int pkg_same_file(const struct stat *a, const struct stat *b)
{
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino &&
           a->st_mode == b->st_mode && a->st_nlink == b->st_nlink &&
           a->st_size == b->st_size && a->st_mtim.tv_sec == b->st_mtim.tv_sec &&
           a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec &&
           a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
}

/* Purpose: Read a bounded single-link regular file with no-follow traversal.
 * Args: dir/path select input; limit bounds bytes; length/info receive results.
 * Rets: Owned bytes with extra NUL terminator, or NULL with errno.
 */
char *pkg_read_file(int dir, const char *path, size_t limit, size_t *length,
                    struct stat *info)
{
    char *leaf;
    int parent = pkg_parent_open(dir, path, 0, &leaf);
    if (parent < 0)
        return NULL;
    int fd =
        openat(parent, leaf, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    int saved = errno;
    free(leaf);
    if (close(parent) != 0 && fd >= 0) {
        close(fd);
        fd = -1;
    }
    errno = saved;
    if (fd < 0)
        return NULL;
    struct stat before, after;
    char *data = NULL;
    if (fstat(fd, &before) != 0 || !S_ISREG(before.st_mode) ||
        before.st_nlink != 1 || before.st_size < 0 ||
        (uintmax_t)before.st_size > limit) {
        errno = EINVAL;
        goto done;
    }
    size_t size = (size_t)before.st_size;
    data = malloc(size + 1);
    if (data == NULL)
        goto done;
    size_t used = 0;
    while (used < size) {
        ssize_t n = read(fd, data + used, size - used);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            goto fail;
        used += (size_t)n;
    }
    char extra;
    ssize_t n;
    do {
        n = read(fd, &extra, 1);
    } while (n < 0 && errno == EINTR);
    if (n != 0 || fstat(fd, &after) != 0 || !pkg_same_file(&before, &after))
        goto fail;
    data[size] = '\0';
    *length = size;
    if (info != NULL)
        *info = before;
    goto done;
fail:
    free(data);
    data = NULL;
    errno = EINVAL;
done:
    if (close(fd) != 0) {
        free(data);
        data = NULL;
    }
    return data;
}

/* Purpose: Create one staged file exclusively with normalized permissions.
 * Args: dir/path select output; data/length are borrowed bytes; mode is exact.
 * Rets: 0 success, -1 allocation/open/write/chmod/close failure.
 */
int pkg_write_file(int dir, const char *path, const char *data, size_t length,
                   mode_t mode)
{
    char *leaf;
    int parent = pkg_parent_open(dir, path, 1, &leaf);
    if (parent < 0)
        return -1;
    int fd = openat(parent, leaf,
                    O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    free(leaf);
    int status = close(parent);
    if (fd < 0)
        return -1;
    size_t used = 0;
    while (status == 0 && used < length) {
        ssize_t n = write(fd, data + used, length - used);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            status = -1;
            break;
        }
        used += (size_t)n;
    }
    if (fchmod(fd, mode) != 0)
        status = -1;
    if (close(fd) != 0)
        status = -1;
    return status;
}
