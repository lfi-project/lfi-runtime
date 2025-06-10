#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>

int
main(void)
{
    size_t pagesize = getpagesize();
    char *p = mmap(NULL, pagesize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    assert(p != (char *) -1);
    assert(p[0] == 0);
    p[0] = 42;

    int r = mprotect(p, pagesize, PROT_READ);
    assert(r == 0 && p[0] == 42);

    r = munmap(p, pagesize);
    assert(r == 0);

    char *newp = mmap(p, pagesize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    assert(newp == p);
    assert(newp[0] == 0);

    printf("success\n");

    return 0;
}
