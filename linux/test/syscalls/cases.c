// See cases.h. Everything here must be valid both natively (glibc/musl on the
// host) and inside an LFI sandbox (musl compiled with the LFI toolchain), so
// only syscall(2) and plain memory accesses are used.

#define _GNU_SOURCE

#include "cases.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#ifndef PROT_SEM
#define PROT_SEM 0x8
#endif

// The host page size; inside the sandbox this comes from AT_PAGESZ, which the
// runtime sets to the page size it was configured with.
static size_t
page_size(void)
{
    static size_t pagesize;
    if (pagesize == 0)
        pagesize = (size_t) sysconf(_SC_PAGESIZE);
    return pagesize;
}

#define PAGE page_size()

// Raw syscall: returns the result, or -errno on failure.
#define SYSCALL(...)                                          \
    __extension__({                                           \
        long _r = syscall(__VA_ARGS__);                       \
        _r == -1 ? -(long) errno : _r;                        \
    })

static long
brk_now(void)
{
    return SYSCALL(SYS_brk, 0);
}

static int
region_is_zero(const volatile unsigned char *p, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (p[i] != 0)
            return 0;
    return 1;
}

static void
region_fill(volatile unsigned char *p, size_t n, unsigned char v)
{
    for (size_t i = 0; i < n; i++)
        p[i] = v;
}

// brk(0) reports the current break: non-zero and word aligned.
long
case_brk_query(void)
{
    long b = brk_now();
    if (b < 0)
        return b;
    if (b == 0)
        return 1;
    if (b % 8 != 0)
        return 2;
    return 0;
}

// Growing the break returns the new break, the new memory is zero-filled and
// writable, and shrinking back returns the original break.
long
case_brk_grow_shrink(void)
{
    long cur = brk_now();
    if (cur < 0)
        return cur;
    long want = cur + 16 * PAGE;
    long got = SYSCALL(SYS_brk, want);
    if (got < 0)
        return got;
    if (got != want)
        return 1;
    volatile unsigned char *p = (volatile unsigned char *) cur;
    if (!region_is_zero(p, 16 * PAGE))
        return 2;
    region_fill(p, 16 * PAGE, 0xab);
    long back = SYSCALL(SYS_brk, cur);
    if (back < 0)
        return back;
    if (back != cur)
        return 3;
    return 0;
}

// Pages released by shrinking the break come back zero-filled when the break
// grows over them again.
long
case_brk_regrow_zeroed(void)
{
    long cur = brk_now();
    if (cur < 0)
        return cur;
    // Work from a page-aligned break so that the whole range is made of pages
    // that shrinking actually releases.
    long base = (cur + PAGE - 1) & ~(PAGE - 1);
    if (SYSCALL(SYS_brk, base) != base)
        return 1;
    long top = base + 4 * PAGE;
    if (SYSCALL(SYS_brk, top) != top)
        return 2;
    volatile unsigned char *p = (volatile unsigned char *) base;
    region_fill(p, 4 * PAGE, 0xcd);
    if (SYSCALL(SYS_brk, base) != base)
        return 3;
    if (SYSCALL(SYS_brk, top) != top)
        return 4;
    int zero = region_is_zero(p, 4 * PAGE);
    if (SYSCALL(SYS_brk, cur) != cur)
        return 5;
    return zero ? 0 : 6;
}

// A request below the start of the heap is ignored: brk returns the current
// break unchanged and does not fail.
long
case_brk_below_start(void)
{
    long cur = brk_now();
    if (cur < 0)
        return cur;
    long r = SYSCALL(SYS_brk, 1);
    if (r < 0)
        return r;
    if (r != cur)
        return 1;
    if (brk_now() != cur)
        return 2;
    return 0;
}

// A request that cannot possibly be satisfied is ignored the same way.
long
case_brk_absurd(void)
{
    long cur = brk_now();
    if (cur < 0)
        return cur;
    long r = SYSCALL(SYS_brk, cur + (1L << 46));
    if (r < 0)
        return r;
    if (r != cur)
        return 1;
    if (brk_now() != cur)
        return 2;
    return 0;
}

static long
map_anon(size_t len, int prot, int extra_flags, int fd, long off)
{
    return SYSCALL(SYS_mmap, 0, len, prot, MAP_PRIVATE | MAP_ANONYMOUS | extra_flags, fd, off);
}

static long
unmap(long p, size_t len)
{
    return SYSCALL(SYS_munmap, p, len);
}

// Releasing pages by shrinking the break unmaps them, so mprotect on them
// fails with ENOMEM.
long
case_brk_shrink_unmaps(void)
{
    long cur = brk_now();
    if (cur < 0)
        return cur;
    long base = (cur + PAGE - 1) & ~(PAGE - 1);
    long top = base + 4 * PAGE;
    if (SYSCALL(SYS_brk, top) != top)
        return 1;
    long r = SYSCALL(SYS_brk, base);
    if (r != base)
        return 2;
    r = SYSCALL(SYS_mprotect, base, 4 * PAGE, PROT_READ);
    if (SYSCALL(SYS_brk, cur) != cur)
        return 3;
    if (r < 0)
        return r;
    return 4;
}

// brk refuses to grow over another mapping: the break stays unchanged and the
// mapping is left intact.
long
case_brk_grow_blocked(void)
{
    long cur = brk_now();
    if (cur < 0)
        return cur;
    long base = (cur + PAGE - 1) & ~(PAGE - 1);
    long block = base + 2 * PAGE;
    long p = SYSCALL(SYS_mmap, block, PAGE, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (p < 0)
        return p;
    if (p != block) {
        unmap(p, PAGE);
        return 1;
    }
    volatile unsigned char *b = (volatile unsigned char *) p;
    b[0] = 0x77;
    long r = SYSCALL(SYS_brk, base + 4 * PAGE);
    long ret;
    if (r != cur)
        ret = 2;
    else if (brk_now() != cur)
        ret = 3;
    else if (b[0] != 0x77)
        ret = 4;
    else
        ret = 0;
    unmap(p, PAGE);
    if (brk_now() != cur)
        SYSCALL(SYS_brk, cur);
    return ret;
}

// brk grows over pages above the break that were unmapped, since they are
// free.
long
case_brk_grow_over_unmapped(void)
{
    long cur = brk_now();
    if (cur < 0)
        return cur;
    long base = (cur + PAGE - 1) & ~(PAGE - 1);
    long top = base + 4 * PAGE;
    long r = unmap(base, 4 * PAGE);
    if (r < 0)
        return r;
    if (SYSCALL(SYS_brk, top) != top)
        return 1;
    volatile unsigned char *b = (volatile unsigned char *) base;
    int zero = region_is_zero(b, 4 * PAGE);
    region_fill(b, 4 * PAGE, 0x31);
    if (SYSCALL(SYS_brk, cur) != cur)
        return 2;
    return zero ? 0 : 3;
}

// Anonymous private mapping: page aligned, zero-filled, writable, unmappable.
long
case_mmap_anon(void)
{
    long p = map_anon(2 * PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    if (p == 0 || p % PAGE != 0)
        return 1;
    volatile unsigned char *b = (volatile unsigned char *) p;
    if (!region_is_zero(b, 2 * PAGE))
        return 2;
    region_fill(b, 2 * PAGE, 0x5a);
    if (b[0] != 0x5a || b[2 * PAGE - 1] != 0x5a)
        return 3;
    long r = unmap(p, 2 * PAGE);
    if (r < 0)
        return r;
    return 0;
}

long
case_mmap_len0(void)
{
    long p = map_anon(0, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    unmap(p, PAGE);
    return 1;
}

// A length that overflows when rounded up to a page is ENOMEM.
long
case_mmap_len_overflow(void)
{
    long p = map_anon((size_t) -1, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    unmap(p, PAGE);
    return 1;
}

// A mapping that is neither MAP_PRIVATE nor MAP_SHARED is EINVAL.
long
case_mmap_no_type(void)
{
    long p = SYSCALL(SYS_mmap, 0, PAGE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS,
        -1, 0);
    if (p < 0)
        return p;
    unmap(p, PAGE);
    return 1;
}

// mmap ignores unknown protection bits.
long
case_mmap_unknown_prot(void)
{
    long p = map_anon(PAGE, PROT_READ | PROT_WRITE | 0x100, 0, -1, 0);
    if (p < 0)
        return p;
    volatile unsigned char *b = (volatile unsigned char *) p;
    b[0] = 3;
    long ret = b[0] == 3 ? 0 : 1;
    long r = unmap(p, PAGE);
    if (r < 0)
        return r;
    return ret;
}

// MAP_FIXED over an existing mapping replaces it at exactly that address.
long
case_mmap_fixed(void)
{
    long p = map_anon(2 * PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    region_fill((volatile unsigned char *) p, 2 * PAGE, 0x11);
    long q = SYSCALL(SYS_mmap, p, PAGE, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    long ret;
    if (q < 0)
        ret = q;
    else if (q != p)
        ret = 1;
    else if (!region_is_zero((volatile unsigned char *) p, PAGE))
        ret = 2;
    else if (((volatile unsigned char *) p)[PAGE] != 0x11)
        ret = 3;
    else
        ret = 0;
    unmap(p, 2 * PAGE);
    return ret;
}

long
case_mmap_fixed_unaligned(void)
{
    long p = map_anon(2 * PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    long q = SYSCALL(SYS_mmap, p + 1, PAGE, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    unmap(p, 2 * PAGE);
    if (q < 0)
        return q;
    return 1;
}

long
case_mmap_offset_unaligned(void)
{
    long p = map_anon(PAGE, PROT_READ | PROT_WRITE, 0, -1, 123);
    if (p < 0)
        return p;
    unmap(p, PAGE);
    return 1;
}

// With MAP_ANONYMOUS the file descriptor argument is ignored.
long
case_mmap_anon_ignores_fd(void)
{
    long p = map_anon(PAGE, PROT_READ | PROT_WRITE, 0, 5, 0);
    if (p < 0)
        return p;
    long r = unmap(p, PAGE);
    if (r < 0)
        return r;
    return 0;
}

long
case_mmap_prot_none_then_rw(void)
{
    long p = map_anon(PAGE, PROT_NONE, 0, -1, 0);
    if (p < 0)
        return p;
    long r = SYSCALL(SYS_mprotect, p, PAGE, PROT_READ | PROT_WRITE);
    long ret;
    if (r < 0) {
        ret = r;
    } else {
        volatile unsigned char *b = (volatile unsigned char *) p;
        b[0] = 7;
        ret = b[0] == 7 ? 0 : 1;
    }
    unmap(p, PAGE);
    return ret;
}

long
case_mmap_shared_anon(void)
{
    long p = SYSCALL(SYS_mmap, 0, PAGE, PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (p < 0)
        return p;
    long r = unmap(p, PAGE);
    if (r < 0)
        return r;
    return 0;
}

long
case_mmap_exec_anon(void)
{
    long p = map_anon(PAGE, PROT_READ | PROT_EXEC, 0, -1, 0);
    if (p < 0)
        return p;
    long r = unmap(p, PAGE);
    if (r < 0)
        return r;
    return 0;
}

long
case_munmap_len0(void)
{
    long p = map_anon(PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    long r = unmap(p, 0);
    unmap(p, PAGE);
    if (r < 0)
        return r;
    return 1;
}

long
case_munmap_unaligned(void)
{
    long p = map_anon(2 * PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    long r = unmap(p + 1, PAGE);
    unmap(p, 2 * PAGE);
    if (r < 0)
        return r;
    return 1;
}

// Unmapping a range that is not mapped succeeds.
long
case_munmap_unmapped(void)
{
    long p = map_anon(2 * PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    long r = unmap(p, 2 * PAGE);
    if (r < 0)
        return r;
    r = unmap(p, 2 * PAGE);
    if (r < 0)
        return r;
    return 0;
}

long
case_mprotect_ro_rw(void)
{
    long p = map_anon(PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    volatile unsigned char *b = (volatile unsigned char *) p;
    b[0] = 9;
    long ret = 0;
    long r = SYSCALL(SYS_mprotect, p, PAGE, PROT_READ);
    if (r < 0)
        ret = r;
    else if (b[0] != 9)
        ret = 1;
    else if ((r = SYSCALL(SYS_mprotect, p, PAGE, PROT_READ | PROT_WRITE)) < 0)
        ret = r;
    else {
        b[0] = 10;
        if (b[0] != 10)
            ret = 2;
    }
    unmap(p, PAGE);
    return ret;
}

long
case_mprotect_len0(void)
{
    long p = map_anon(PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    long r = SYSCALL(SYS_mprotect, p, 0, PROT_READ);
    unmap(p, PAGE);
    if (r < 0)
        return r;
    return 0;
}

long
case_mprotect_unaligned(void)
{
    long p = map_anon(2 * PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    long r = SYSCALL(SYS_mprotect, p + 1, PAGE, PROT_READ);
    unmap(p, 2 * PAGE);
    if (r < 0)
        return r;
    return 1;
}

long
case_mprotect_unmapped(void)
{
    long p = map_anon(2 * PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    long r = unmap(p, 2 * PAGE);
    if (r < 0)
        return r;
    r = SYSCALL(SYS_mprotect, p, 2 * PAGE, PROT_READ);
    if (r < 0)
        return r;
    return 1;
}

// mprotect rejects unknown protection bits.
long
case_mprotect_unknown_prot(void)
{
    long p = map_anon(PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    long r = SYSCALL(SYS_mprotect, p, PAGE, PROT_READ | 0x100);
    unmap(p, PAGE);
    if (r < 0)
        return r;
    return 1;
}

// mprotect accepts and ignores PROT_SEM.
long
case_mprotect_sem(void)
{
    long p = map_anon(PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    long r = SYSCALL(SYS_mprotect, p, PAGE, PROT_READ | PROT_WRITE | PROT_SEM);
    long ret = r;
    if (r == 0) {
        volatile unsigned char *b = (volatile unsigned char *) p;
        b[0] = 4;
        ret = b[0] == 4 ? 0 : 1;
    }
    unmap(p, PAGE);
    return ret;
}

// The heap above the break is not mapped, so mprotect on it is ENOMEM.
long
case_mprotect_above_brk(void)
{
    long cur = brk_now();
    if (cur < 0)
        return cur;
    long base = (cur + PAGE - 1) & ~(PAGE - 1);
    long r = SYSCALL(SYS_mprotect, base + 4 * PAGE, PAGE, PROT_READ);
    if (r < 0)
        return r;
    return 1;
}

long
case_mprotect_exec(void)
{
    long p = map_anon(PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    long r = SYSCALL(SYS_mprotect, p, PAGE, PROT_READ | PROT_EXEC);
    unmap(p, PAGE);
    if (r < 0)
        return r;
    return 0;
}

long
case_mremap_grow(void)
{
    long p = map_anon(PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0)
        return p;
    long q = SYSCALL(SYS_mremap, p, PAGE, 2 * PAGE, MREMAP_MAYMOVE);
    if (q < 0) {
        unmap(p, PAGE);
        return q;
    }
    long r = unmap(q, 2 * PAGE);
    if (r < 0)
        return r;
    return 0;
}

// The passthrough cases below make their scratch files in /tmp. The native
// run and the sandboxed run are different processes (the sandbox runs in the
// test driver), so a name that includes the pid is unique to each run.
static void
tmp_path(char *buf, const char *tag)
{
    static const char prefix[] = "/tmp/lfi-syscall-test-";
    size_t n = 0;
    for (size_t i = 0; prefix[i]; i++)
        buf[n++] = prefix[i];
    long pid = SYSCALL(SYS_getpid);
    char digits[24];
    size_t nd = 0;
    do {
        digits[nd++] = '0' + (char) (pid % 10);
        pid /= 10;
    } while (pid > 0);
    while (nd > 0)
        buf[n++] = digits[--nd];
    buf[n++] = '-';
    for (size_t i = 0; tag[i]; i++)
        buf[n++] = tag[i];
    buf[n] = '\0';
}

#define TMP_PATH_MAX 64

static long
tmp_create(char *path, const char *tag, int flags)
{
    tmp_path(path, tag);
    return SYSCALL(SYS_openat, AT_FDCWD, path, flags | O_CREAT | O_TRUNC,
        0600);
}

static void
tmp_remove(long fd, const char *path)
{
    SYSCALL(SYS_close, fd);
    SYSCALL(SYS_unlinkat, AT_FDCWD, path, 0);
}

static int
bytes_equal(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (size_t i = 0; i < n; i++)
        if (x[i] != y[i])
            return 0;
    return 1;
}

static const char file_msg[] = "hello from the sandbox";

// Fills a file with one page of a byte pattern.
static long
fill_page(long fd, unsigned char v)
{
    unsigned char buf[256];
    for (size_t i = 0; i < sizeof(buf); i++)
        buf[i] = v;
    for (size_t off = 0; off < PAGE; off += sizeof(buf)) {
        long r = SYSCALL(SYS_write, fd, buf, sizeof(buf));
        if (r != (long) sizeof(buf))
            return r < 0 ? r : -EIO;
    }
    return 0;
}

long
case_mmap_file_badfd(void)
{
    long p = SYSCALL(SYS_mmap, 0, PAGE, PROT_READ, MAP_PRIVATE, 1 << 30, 0L);
    if (p < 0)
        return p;
    unmap(p, PAGE);
    return 1;
}

// A private file mapping shows the file's contents.
long
case_mmap_file_private(void)
{
    char path[TMP_PATH_MAX];
    long fd = tmp_create(path, "mmap-private", O_RDWR);
    if (fd < 0)
        return fd;
    long ret = fill_page(fd, 0x5a);
    if (ret == 0) {
        long p = SYSCALL(SYS_mmap, 0, PAGE, PROT_READ, MAP_PRIVATE, fd, 0L);
        if (p < 0) {
            ret = p;
        } else {
            const volatile unsigned char *q = (const volatile unsigned char *) p;
            for (size_t i = 0; i < PAGE; i++)
                if (q[i] != 0x5a)
                    ret = 1;
            unmap(p, PAGE);
        }
    }
    tmp_remove(fd, path);
    return ret;
}

// Stores through a shared file mapping reach the file.
long
case_mmap_file_shared(void)
{
    char path[TMP_PATH_MAX];
    long fd = tmp_create(path, "mmap-shared", O_RDWR);
    if (fd < 0)
        return fd;
    long ret = fill_page(fd, 0);
    if (ret == 0) {
        long p = SYSCALL(SYS_mmap, 0, PAGE, PROT_READ | PROT_WRITE, MAP_SHARED,
            fd, 0L);
        if (p < 0) {
            ret = p;
        } else {
            *(volatile unsigned char *) p = 0x77;
            unmap(p, PAGE);
            unsigned char c = 0;
            if (SYSCALL(SYS_lseek, fd, 0, SEEK_SET) != 0)
                ret = 1;
            else if (SYSCALL(SYS_read, fd, &c, 1) != 1)
                ret = 2;
            else if (c != 0x77)
                ret = 3;
        }
    }
    tmp_remove(fd, path);
    return ret;
}

// Mapping a file that is not open for reading fails.
long
case_mmap_file_wronly(void)
{
    char path[TMP_PATH_MAX];
    long fd = tmp_create(path, "mmap-wronly", O_WRONLY);
    if (fd < 0)
        return fd;
    long p = SYSCALL(SYS_mmap, 0, PAGE, PROT_READ, MAP_PRIVATE, fd, 0L);
    tmp_remove(fd, path);
    if (p < 0)
        return p;
    unmap(p, PAGE);
    return 1;
}

// A directory cannot be mapped.
long
case_mmap_file_dir(void)
{
    long fd = SYSCALL(SYS_openat, AT_FDCWD, "/", O_RDONLY | O_DIRECTORY);
    if (fd < 0)
        return fd;
    long p = SYSCALL(SYS_mmap, 0, PAGE, PROT_READ, MAP_PRIVATE, fd, 0L);
    SYSCALL(SYS_close, fd);
    if (p < 0)
        return p;
    unmap(p, PAGE);
    return 1;
}

// An O_PATH descriptor is not a valid descriptor for mmap.
long
case_mmap_file_opath(void)
{
    char path[TMP_PATH_MAX];
    long fd = tmp_create(path, "mmap-opath", O_RDWR);
    if (fd < 0)
        return fd;
    long ret = fill_page(fd, 0);
    if (ret == 0) {
        long pfd = SYSCALL(SYS_openat, AT_FDCWD, path, O_PATH);
        if (pfd < 0) {
            ret = pfd;
        } else {
            ret = SYSCALL(SYS_mmap, 0, PAGE, PROT_READ, MAP_PRIVATE, pfd, 0L);
            if (ret >= 0) {
                unmap(ret, PAGE);
                ret = 1;
            }
            SYSCALL(SYS_close, pfd);
        }
    }
    tmp_remove(fd, path);
    return ret;
}

// A negative offset is past the largest file offset.
long
case_mmap_file_bad_offset(void)
{
    char path[TMP_PATH_MAX];
    long fd = tmp_create(path, "mmap-offset", O_RDWR);
    if (fd < 0)
        return fd;
    long ret = fill_page(fd, 0);
    if (ret == 0) {
        ret = SYSCALL(SYS_mmap, 0, PAGE, PROT_READ, MAP_PRIVATE, fd,
            -(long) PAGE);
        if (ret >= 0) {
            unmap(ret, PAGE);
            ret = 1;
        }
    }
    tmp_remove(fd, path);
    return ret;
}

// A MAP_FIXED file mapping that fails leaves the old mapping in place.
long
case_mmap_file_fixed_keeps(void)
{
    long fd = SYSCALL(SYS_openat, AT_FDCWD, "/", O_RDONLY | O_DIRECTORY);
    if (fd < 0)
        return fd;
    long p = map_anon(PAGE, PROT_READ | PROT_WRITE, 0, -1, 0);
    if (p < 0) {
        SYSCALL(SYS_close, fd);
        return p;
    }
    *(volatile unsigned char *) p = 0x3c;
    long ret = SYSCALL(SYS_mmap, p, PAGE, PROT_READ, MAP_PRIVATE | MAP_FIXED,
        fd, 0L);
    SYSCALL(SYS_close, fd);
    if (ret >= 0) {
        ret = 1;
    } else if (SYSCALL(SYS_mprotect, p, PAGE, PROT_READ | PROT_WRITE) != 0) {
        // Checked first so a removed mapping is reported, not faulted on.
        ret = 2;
    } else if (*(volatile unsigned char *) p != 0x3c) {
        ret = 3;
    }
    unmap(p, PAGE);
    return ret;
}

// Data written to a file reads back, and the offset moves as expected.
long
case_file_rw(void)
{
    char path[TMP_PATH_MAX];
    long fd = tmp_create(path, "rw", O_RDWR);
    if (fd < 0)
        return fd;
    char buf[sizeof(file_msg)] = { 0 };
    long ret = 0;
    if (SYSCALL(SYS_write, fd, file_msg, sizeof(file_msg)) != sizeof(file_msg))
        ret = 1;
    else if (SYSCALL(SYS_lseek, fd, 0, SEEK_SET) != 0)
        ret = 2;
    else if (SYSCALL(SYS_read, fd, buf, sizeof(buf)) != sizeof(buf))
        ret = 3;
    else if (!bytes_equal(buf, file_msg, sizeof(file_msg)))
        ret = 4;
    else if (SYSCALL(SYS_lseek, fd, 0, SEEK_END) != sizeof(file_msg))
        ret = 5;
    else if (SYSCALL(SYS_read, fd, buf, sizeof(buf)) != 0)
        ret = 6;
    tmp_remove(fd, path);
    return ret;
}

// Vectored I/O, which musl uses for all stdio.
long
case_file_readv_writev(void)
{
    char path[TMP_PATH_MAX];
    long fd = tmp_create(path, "iov", O_RDWR);
    if (fd < 0)
        return fd;
    struct iovec out[2] = {
        { (void *) file_msg, 5 },
        { (void *) (file_msg + 5), sizeof(file_msg) - 5 },
    };
    char a[3], b[sizeof(file_msg) - 3];
    struct iovec in[2] = { { a, sizeof(a) }, { b, sizeof(b) } };
    long ret = 0;
    if (SYSCALL(SYS_writev, fd, out, 2) != sizeof(file_msg))
        ret = 1;
    else if (SYSCALL(SYS_lseek, fd, 0, SEEK_SET) != 0)
        ret = 2;
    else if (SYSCALL(SYS_readv, fd, in, 2) != sizeof(file_msg))
        ret = 3;
    else if (!bytes_equal(a, file_msg, sizeof(a)) ||
        !bytes_equal(b, file_msg + sizeof(a), sizeof(b)))
        ret = 4;
    tmp_remove(fd, path);
    return ret;
}

long
case_file_stat(void)
{
    char path[TMP_PATH_MAX];
    long fd = tmp_create(path, "stat", O_RDWR);
    if (fd < 0)
        return fd;
    struct stat st;
    long ret = 0;
    if (SYSCALL(SYS_write, fd, file_msg, sizeof(file_msg)) != sizeof(file_msg))
        ret = 1;
    else if (SYSCALL(SYS_fstat, fd, &st) != 0)
        ret = 2;
    else if (st.st_size != sizeof(file_msg) || !S_ISREG(st.st_mode))
        ret = 3;
    else if (SYSCALL(SYS_newfstatat, AT_FDCWD, path, &st, 0) != 0)
        ret = 4;
    else if (st.st_size != sizeof(file_msg))
        ret = 5;
    else if (SYSCALL(SYS_faccessat, AT_FDCWD, path, R_OK | W_OK) != 0)
        ret = 6;
    tmp_remove(fd, path);
    if (ret != 0)
        return ret;
    // Existence checks on a file that is gone.
    if (SYSCALL(SYS_faccessat, AT_FDCWD, path, F_OK) != -ENOENT)
        return 7;
    if (SYSCALL(SYS_newfstatat, AT_FDCWD, path, &st, 0) != -ENOENT)
        return 8;
    return 0;
}

static long
rename_moves(const char *from, const char *to, long r)
{
    if (r != 0)
        return r < 0 ? r : 1;
    if (SYSCALL(SYS_faccessat, AT_FDCWD, from, F_OK) != -ENOENT)
        return 2;
    if (SYSCALL(SYS_faccessat, AT_FDCWD, to, F_OK) != 0)
        return 3;
    return 0;
}

long
case_file_rename(void)
{
    char a[TMP_PATH_MAX], b[TMP_PATH_MAX];
    long fd = tmp_create(a, "rename-a", O_RDWR);
    if (fd < 0)
        return fd;
    tmp_path(b, "rename-b");
    long ret = 0;
#ifdef SYS_renameat
    ret = rename_moves(a, b,
        SYSCALL(SYS_renameat, AT_FDCWD, a, AT_FDCWD, b));
#endif
    if (ret == 0)
        ret = rename_moves(b, a,
            SYSCALL(SYS_renameat2, AT_FDCWD, b, AT_FDCWD, a, 0));
    tmp_remove(fd, a);
    SYSCALL(SYS_unlinkat, AT_FDCWD, b, 0);
    return ret;
}

// readlink on a regular file fails with EINVAL (xalancbmk relies on this).
long
case_file_readlink_regular(void)
{
    char path[TMP_PATH_MAX];
    long fd = tmp_create(path, "readlink", O_RDWR);
    if (fd < 0)
        return fd;
    char buf[64];
    long r = SYSCALL(SYS_readlinkat, AT_FDCWD, path, buf, sizeof(buf));
    tmp_remove(fd, path);
    return r;
}

long
case_file_dup_fcntl(void)
{
    char path[TMP_PATH_MAX];
    long fd = tmp_create(path, "dup", O_RDWR);
    if (fd < 0)
        return fd;
    const long nfd = 200;
    long ret = 0;
    if (SYSCALL(SYS_dup3, fd, nfd, 0) != nfd)
        ret = 1;
    else if ((SYSCALL(SYS_fcntl, nfd, F_GETFL) & O_ACCMODE) != O_RDWR)
        ret = 2;
    else if (SYSCALL(SYS_fcntl, nfd, F_SETFD, FD_CLOEXEC) != 0)
        ret = 3;
    else if (SYSCALL(SYS_fcntl, nfd, F_GETFD) != FD_CLOEXEC)
        ret = 4;
    else if (SYSCALL(SYS_dup3, fd, fd, 0) != -EINVAL)
        ret = 5;
    SYSCALL(SYS_close, nfd);
    tmp_remove(fd, path);
    return ret;
}

// musl checks whether stdout is a tty with TIOCGWINSZ.
long
case_file_ioctl_notty(void)
{
    char path[TMP_PATH_MAX];
    long fd = tmp_create(path, "ioctl", O_RDWR);
    if (fd < 0)
        return fd;
    struct winsize ws;
    long r = SYSCALL(SYS_ioctl, fd, TIOCGWINSZ, &ws);
    tmp_remove(fd, path);
    return r;
}

static long
cwd_is(const char *want)
{
    char buf[4096];
    long n = SYSCALL(SYS_getcwd, buf, sizeof(buf));
    if (n < 0)
        return n;
    size_t len = 0;
    while (want[len])
        len++;
    return n == (long) len + 1 && bytes_equal(buf, want, len + 1) ? 0 : 1;
}

long
case_cwd_chdir(void)
{
    char old[4096];
    long n = SYSCALL(SYS_getcwd, old, sizeof(old));
    if (n < 0)
        return n;
    long r = SYSCALL(SYS_chdir, "/");
    if (r < 0)
        return r;
    long ret = cwd_is("/") == 0 ? 0 : 1;
    if (SYSCALL(SYS_chdir, old) != 0)
        return 2;
    if (ret == 0 && cwd_is(old) != 0)
        ret = 3;
    return ret;
}

long
case_getdents64(void)
{
    long fd = SYSCALL(SYS_openat, AT_FDCWD, "/", O_RDONLY | O_DIRECTORY);
    if (fd < 0)
        return fd;
    _Alignas(8) char buf[4096];
    long n = SYSCALL(SYS_getdents64, fd, buf, sizeof(buf));
    SYSCALL(SYS_close, fd);
    if (n < 0)
        return n;
    // The first record is at least a header and a one byte name.
    unsigned short reclen = *(unsigned short *) (buf + 16);
    if (n < 20 || reclen < 20 || reclen > n)
        return 1;
    return 0;
}

long
case_umask(void)
{
    long old = SYSCALL(SYS_umask, 027);
    if (old < 0)
        return old;
    long r = SYSCALL(SYS_umask, old);
    return r == 027 ? 0 : 1;
}

static long
clock_ok(clockid_t id)
{
    struct timespec ts = { -1, -1 };
    long r = SYSCALL(SYS_clock_gettime, id, &ts);
    if (r != 0)
        return r < 0 ? r : 1;
    return ts.tv_sec >= 0 && ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000 ? 0 :
                                                                       2;
}

// There is no vDSO in the sandbox, so every clock is a syscall.
long
case_clock_gettime(void)
{
    long r;
    if ((r = clock_ok(CLOCK_REALTIME)) != 0)
        return r;
    if ((r = clock_ok(CLOCK_MONOTONIC)) != 0)
        return r;
    if ((r = clock_ok(CLOCK_PROCESS_CPUTIME_ID)) != 0)
        return r;
    struct timespec ts;
    if (SYSCALL(SYS_clock_gettime, 1000, &ts) != -EINVAL)
        return 3;
    return 0;
}

long
case_gettimeofday(void)
{
    struct timeval tv = { 0, -1 };
    long r = SYSCALL(SYS_gettimeofday, &tv, 0);
    if (r != 0)
        return r < 0 ? r : 1;
    return tv.tv_sec > 0 && tv.tv_usec >= 0 && tv.tv_usec < 1000000 ? 0 : 2;
}

long
case_resources(void)
{
    struct rusage ru;
    long r = SYSCALL(SYS_getrusage, RUSAGE_SELF, &ru);
    if (r != 0)
        return r < 0 ? r : 1;
    // 502.gcc_r only reads limits.
    uint64_t lim[2] = { 0, 0 };
    r = SYSCALL(SYS_prlimit64, 0, RLIMIT_AS, 0, lim);
    if (r != 0)
        return r < 0 ? r : 2;
    if (lim[0] > lim[1])
        return 3;
    struct sysinfo si;
    r = SYSCALL(SYS_sysinfo, &si);
    if (r != 0)
        return r < 0 ? r : 4;
    return si.totalram > 0 ? 0 : 5;
}

// The kernel's struct sigaction with a 64-signal mask.
struct ksigaction {
    uintptr_t handler;
    unsigned long flags;
    uintptr_t restorer;
    uint64_t mask;
};

// Querying an untouched signal reports SIG_DFL, and installing and resetting
// a disposition succeeds. The runtime's stub does not remember dispositions,
// so the case does not read one back after setting it.
long
case_rt_sigaction(void)
{
    struct ksigaction old;
    for (size_t i = 0; i < sizeof(old); i++)
        ((volatile char *) &old)[i] = 0x55;
    long r = SYSCALL(SYS_rt_sigaction, SIGUSR2, 0, &old, 8);
    if (r != 0)
        return r < 0 ? r : 1;
    if (old.handler != (uintptr_t) SIG_DFL || old.mask != 0)
        return 2;
    struct ksigaction act = { (uintptr_t) SIG_IGN, 0, 0, 0 };
    if (SYSCALL(SYS_rt_sigaction, SIGUSR2, &act, 0, 8) != 0)
        return 3;
    act.handler = (uintptr_t) SIG_DFL;
    if (SYSCALL(SYS_rt_sigaction, SIGUSR2, &act, 0, 8) != 0)
        return 4;
    return 0;
}

long
case_rt_sigaction_invalid(void)
{
    struct ksigaction act = { (uintptr_t) SIG_IGN, 0, 0, 0 };
    struct ksigaction old;
    long r = SYSCALL(SYS_rt_sigaction, SIGUSR2, 0, &old, 4);
    if (r != -EINVAL)
        return r < 0 ? r : 1;
    if (SYSCALL(SYS_rt_sigaction, 0, 0, &old, 8) != -EINVAL)
        return 2;
    if (SYSCALL(SYS_rt_sigaction, 65, 0, &old, 8) != -EINVAL)
        return 3;
    if (SYSCALL(SYS_rt_sigaction, SIGKILL, &act, 0, 8) != -EINVAL)
        return 4;
    // Querying SIGKILL is allowed.
    if (SYSCALL(SYS_rt_sigaction, SIGKILL, 0, &old, 8) != 0)
        return 5;
    return 0;
}

// Blocking and restoring a signal succeeds and invalid arguments are refused.
// The runtime's stub does not track the mask, so its contents are not
// checked.
long
case_rt_sigprocmask(void)
{
    uint64_t set = 1ULL << (SIGUSR2 - 1), old = 0;
    long r = SYSCALL(SYS_rt_sigprocmask, SIG_BLOCK, &set, &old, 8);
    if (r != 0)
        return r < 0 ? r : 1;
    if (SYSCALL(SYS_rt_sigprocmask, SIG_SETMASK, &old, 0, 8) != 0)
        return 2;
    if (SYSCALL(SYS_rt_sigprocmask, SIG_BLOCK, 0, &old, 8) != 0)
        return 3;
    if (SYSCALL(SYS_rt_sigprocmask, 99, &set, 0, 8) != -EINVAL)
        return 4;
    if (SYSCALL(SYS_rt_sigprocmask, SIG_BLOCK, 0, &old, 4) != -EINVAL)
        return 5;
    return 0;
}

// The caller's tid is positive.
long
case_gettid(void)
{
    long tid = SYSCALL(SYS_gettid);
    if (tid < 0)
        return tid;
    return tid > 0 ? 0 : 1;
}

// Linux returns the caller's tid, which is positive. A runtime that ignores
// the call returns 0, which this case reports as 1.
long
case_set_tid_address(void)
{
    static int tidptr;
    long r = SYSCALL(SYS_set_tid_address, &tidptr);
    if (r < 0)
        return r;
    return r > 0 ? 0 : 1;
}

long
case_getpid(void)
{
    long r = SYSCALL(SYS_getpid);
    if (r < 0)
        return r;
    return r > 0 ? 0 : 1;
}

long
case_bogus_sysno(void)
{
    long r = SYSCALL(9999L);
    if (r < 0)
        return r;
    return 1;
}

#if defined(__x86_64__)

#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003

// Reads the current thread pointer. Compilers treat the thread pointer as
// invariant within a function and will happily hoist a single read above the
// arch_prctl calls, so the read must be opaque to the optimizer.
#if defined(HOST_TEST)
// Natively: inline assembly, which is never hoisted and calls nothing. This
// matters because the driver may be sanitizer-instrumented, and instrumented
// code cannot run while %fs points at the test block.
static inline uintptr_t
thread_pointer(void)
{
    uintptr_t tp;
    __asm__ volatile("movq %%fs:0, %0" : "=r"(tp));
    return tp;
}
#else
// In the sandbox: a separate function called through a volatile pointer. The
// volatile local keeps the read a plain load, since the thread pointer lives
// in the context register's memory, which the LFI verifier only allows to be
// read with mov.
static uintptr_t
thread_pointer_read(void)
{
    volatile uintptr_t tp = (uintptr_t) __builtin_thread_pointer();
    return tp;
}

static uintptr_t (*volatile thread_pointer)(void) = thread_pointer_read;
#endif

#if defined(HOST_TEST) && defined(__has_attribute)
#if __has_attribute(no_sanitize)
#define NO_SANITIZE __attribute__((no_sanitize("address", "undefined")))
#endif
#endif
#ifndef NO_SANITIZE
#define NO_SANITIZE
#endif

// Point %fs at a private block and check the thread pointer follows, then
// restore it. The block copies the stack-protector canary slot (offset 0x28
// in both glibc's and musl's TLS layout) so an instrumented prologue or
// epilogue still sees the right value while %fs is switched.
NO_SANITIZE long
case_arch_prctl_set_fs(void)
{
    static uint64_t block[64];
    uintptr_t old = thread_pointer();
    block[0] = (uint64_t) (uintptr_t) block;
    block[5] = ((uint64_t *) old)[5];

    long r = SYSCALL(SYS_arch_prctl, ARCH_SET_FS, block);
    if (r < 0)
        return r;
    uintptr_t now = thread_pointer();
    long back = SYSCALL(SYS_arch_prctl, ARCH_SET_FS, old);
    if (back < 0)
        return back;
    if (now != (uintptr_t) block)
        return 1;
    if (thread_pointer() != old)
        return 2;
    return 0;
}

long
case_arch_prctl_bad_code(void)
{
    long r = SYSCALL(SYS_arch_prctl, 0x9999, 0);
    if (r < 0)
        return r;
    return 1;
}

long
case_arch_prctl_get_fs(void)
{
    uintptr_t fs = 0;
    long r = SYSCALL(SYS_arch_prctl, ARCH_GET_FS, &fs);
    if (r < 0)
        return r;
    return fs == thread_pointer() ? 0 : 1;
}

// The non-*at path syscalls, which musl uses on x86-64.
long
case_legacy_paths(void)
{
    char a[TMP_PATH_MAX], b[TMP_PATH_MAX];
    tmp_path(a, "legacy-a");
    tmp_path(b, "legacy-b");
    long fd = SYSCALL(SYS_open, a, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
        return fd;
    struct stat st;
    char buf[64];
    long ret = 0;
    if (SYSCALL(SYS_stat, a, &st) != 0 || !S_ISREG(st.st_mode))
        ret = 1;
    else if (SYSCALL(SYS_lstat, a, &st) != 0 || !S_ISREG(st.st_mode))
        ret = 2;
    else if (SYSCALL(SYS_access, a, R_OK | W_OK) != 0)
        ret = 3;
    else if (SYSCALL(SYS_readlink, a, buf, sizeof(buf)) != -EINVAL)
        ret = 4;
    else if (SYSCALL(SYS_dup2, fd, 201) != 201)
        ret = 5;
    else if (SYSCALL(SYS_rename, a, b) != 0)
        ret = 6;
    else if (SYSCALL(SYS_access, a, F_OK) != -ENOENT)
        ret = 7;
    else if (SYSCALL(SYS_unlink, b) != 0)
        ret = 8;
    else if (SYSCALL(SYS_stat, b, &st) != -ENOENT)
        ret = 9;
    SYSCALL(SYS_close, 201);
    SYSCALL(SYS_close, fd);
    SYSCALL(SYS_unlink, a);
    SYSCALL(SYS_unlink, b);
    return ret;
}

#endif
