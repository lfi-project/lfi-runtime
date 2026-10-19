// See cases.h. Everything here must be valid both natively (glibc/musl on the
// host) and inside an LFI sandbox (musl compiled with the LFI toolchain), so
// only syscall(2) and plain memory accesses are used.

#define _GNU_SOURCE

#include "cases.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/syscall.h>
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

#endif
