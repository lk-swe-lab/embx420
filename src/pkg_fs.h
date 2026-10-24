#ifndef PKG_FS_H
#define PKG_FS_H
#include <stddef.h>   /* size_t. */
#include <sys/stat.h> /* struct stat, mode_t. */

typedef struct {
    int root;
    int lock;
    char *path;
} pkg_root;
int pkg_dir_open(const char *path);
int pkg_install_disjoint(int source);
int pkg_child_dir(int parent, const char *name, int create, int managed);
int pkg_root_open(pkg_root *root, int create);
int pkg_root_close(pkg_root *root);
char *pkg_read_file(int dir, const char *path, size_t limit, size_t *length,
                    struct stat *info);
int pkg_parent_open(int dir, const char *path, int create, char **leaf);
int pkg_same_file(const struct stat *a, const struct stat *b);
int pkg_write_file(int dir, const char *path, const char *data, size_t length,
                   mode_t mode);
#endif
