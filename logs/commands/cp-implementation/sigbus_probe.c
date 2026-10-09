#define _XOPEN_SOURCE 700
#include "cmd_cp.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
void *__real_mmap(void *, size_t, int, int, int, off_t);
void *__wrap_mmap(void *p, size_t n, int prot, int flags, int fd, off_t off)
{
    void *result = __real_mmap(p, n, prot, flags, fd, off);
    if (result != MAP_FAILED && prot == PROT_READ) {
        int change = open("source", O_WRONLY);
        assert(change >= 0 && ftruncate(change, 0) == 0);
        assert(close(change) == 0);
    }
    return result;
}
int main(int argc, char **argv) { return cmd_cp_spec.run(argc, argv); }
