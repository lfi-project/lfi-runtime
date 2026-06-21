#if defined(HAVE_MEMFD_CREATE)
#define _GNU_SOURCE
#endif

#include "jitcode.h"

#include "align.h"
#include "lfi_core.h"
#include "linux.h"
#include "lock.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#if !defined(HAVE_MEMFD_CREATE) && defined(HAVE_SYS_MEMFD_CREATE)
static int
memfd_create(const char *name, unsigned flags)
{
    return syscall(SYS_memfd_create, name, flags);
}
#endif

static bool
jit_ptrvalid(struct LFILinuxProc *p, lfiptr ptr)
{
    return p->jit_base != 0 &&
           ptr >= p->jit_base &&
           ptr < p->jit_base + p->jit_exec_size;
}

static bool
jit_codebufvalid(struct LFILinuxProc *p, lfiptr buf_start, size_t length)
{
    if (length == 0)
        return jit_ptrvalid(p, buf_start);

    lfiptr buf_end = buf_start + length;
    assert(buf_start < buf_end);

    return p->jit_base != 0 &&
           buf_start >= p->jit_base &&
           buf_end <= p->jit_base + p->jit_exec_size;
}

static uint8_t *
jit_get_aliasptr(struct LFILinuxProc *p, lfiptr addr)
{
    assert(jit_ptrvalid(p, addr));
    return p->jit_alias + (addr - p->jit_base);
}

static unsigned dcache_line_size_ = 1;
static unsigned icache_line_size_ = 1;

uint32_t get_cache_type() {
#if defined(__aarch64__)
  uint64_t cache_type_register;
  // Copy the content of the cache type register to a core register.
  __asm__ __volatile__ ("mrs %[ctr], ctr_el0"  // NOLINT
                        : [ctr] "=r" (cache_type_register));
  // The top bits are reserved, or report information about MTE which currently
  // we discard. This will likely need to be changed to just return
  // cache_type_register when we update VIXL next.
  uint64_t mask = 0xffffffffull;
  return (uint32_t)(cache_type_register & mask);
#else
    return 0;
#endif
}

static void
setup_cache()
{
    uint32_t cache_type_register = get_cache_type();

    // The cache type register holds information about the caches, including I
    // D caches line size.
    static const int kDCacheLineSizeShift = 16;
    static const int kICacheLineSizeShift = 0;
    static const uint32_t kDCacheLineSizeMask = 0xf << kDCacheLineSizeShift;
    static const uint32_t kICacheLineSizeMask = 0xf << kICacheLineSizeShift;

    // The cache type register holds the size of the I and D caches in words as
    // a power of two.
    uint32_t dcache_line_size_power_of_two = (cache_type_register &
                                                 kDCacheLineSizeMask) >>
        kDCacheLineSizeShift;
    uint32_t icache_line_size_power_of_two = (cache_type_register &
                                                 kICacheLineSizeMask) >>
        kICacheLineSizeShift;

    dcache_line_size_ = 4 << dcache_line_size_power_of_two;
    icache_line_size_ = 4 << icache_line_size_power_of_two;

    const uint32_t conservative_line_size = 32;
    dcache_line_size_ = MIN(dcache_line_size_, conservative_line_size);
    icache_line_size_ = MIN(icache_line_size_, conservative_line_size);
}

static inline void flush_icache(void* address, size_t length) {
#if defined(__aarch64__)
    if (length == 0) {
        return;
    }

    // The code below assumes user space cache operations are allowed.

    // Work out the line sizes for each cache, and use them to determine the
    // start addresses.
    uintptr_t start = (uintptr_t)address;
    uintptr_t dsize = (uintptr_t)dcache_line_size_;
    uintptr_t isize = (uintptr_t)icache_line_size_;
    uintptr_t dline = start & ~(dsize - 1);
    uintptr_t iline = start & ~(isize - 1);

    uintptr_t end = start + length;

    do {
        __asm__ __volatile__(
            // Clean each line of the D cache containing the target data.
            //
            // dc       : Data Cache maintenance
            //     c    : Clean
            //      i   : Invalidate
            //      va  : by (Virtual) Address
            //        c : to the point of Coherency
            // Original implementation used cvau, but changed to civac due to
            // errata on Cortex-A53 819472, 826319, 827319 and 824069.
            // See ARM DDI 0406B page B2-12 for more information.
            //
            "   dc    civac, %[dline]\n"
            :
            : [dline] "r"(dline)
            // This code does not write to memory, but the "memory" dependency
            // prevents GCC from reordering the code.
            : "memory");
        dline += dsize;
    } while (dline < end);

    __asm__ __volatile__(
        // Make sure that the data cache operations (above) complete before the
        // instruction cache operations (below).
        //
        // dsb      : Data Synchronisation Barrier
        //      ish : Inner SHareable domain
        //
        // The point of unification for an Inner Shareable shareability domain
        // is the point by which the instruction and data caches of all the
        // processors in that Inner Shareable shareability domain are guaranteed
        // to see the same copy of a memory location.  See ARM DDI 0406B page
        // B2-12 for more information.
        "   dsb   ish\n"
        :
        :
        : "memory");

    do {
        __asm__ __volatile__(
            // Invalidate each line of the I cache containing the target data.
            //
            // ic      : Instruction Cache maintenance
            //    i    : Invalidate
            //     va  : by Address
            //       u : to the point of Unification
            "   ic   ivau, %[iline]\n"
            :
            : [iline] "r"(iline)
            : "memory");
        iline += isize;
    } while (iline < end);

    __asm__ __volatile__(
        // Make sure that the instruction cache operations (above) take effect
        // before the isb (below).
        "   dsb  ish\n"

        // Ensure that any instructions already in the pipeline are discarded
        // and reloaded from the new data. isb : Instruction Synchronisation
        // Barrier
        "   isb\n"
        :
        :
        : "memory");
#endif
}

int
proc_jit_map(struct LFILinuxProc *p, size_t exec_size, size_t data_size,
    lfiptr *o_addr)
{
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    if (p->jit_base != 0)
        return -LINUX_EINVAL;

    setup_cache();

    int fd = memfd_create("", 0);
    if (fd < 0)
        return -LINUX_EINVAL;

    size_t size = exec_size + data_size;
    // overflow check
    if (exec_size > SIZE_MAX - data_size) {
        close(fd);
        return -LINUX_EINVAL;
    }
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -LINUX_EINVAL;
    }

    void *alias = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (alias == (void *) -1) {
        close(fd);
        return -LINUX_EINVAL;
    }

    lfiptr addr = lfi_box_mapany(p->box, size, LFI_PROT_NONE, LFI_MAP_SHARED, fd, 0);
    if (addr == (lfiptr) -1) {
        munmap(alias, size);
        close(fd);
        return -LINUX_EINVAL;
    }

    p->jit_base = addr;
    p->jit_exec_size = exec_size;
    p->jit_data_size = data_size;
    p->jit_alias = alias;
    p->jit_fd = fd;

    *o_addr = addr;
    return 0;
}

int
proc_jit_unmap(struct LFILinuxProc *p, lfiptr addrp, size_t exec_size,
    size_t data_size)
{
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    if (p->jit_base == 0)
        return -LINUX_EINVAL;

    if (p->jit_base != addrp || p->jit_exec_size != exec_size ||
        p->jit_data_size != data_size)
        return -LINUX_EINVAL;

    // overflow has been checked at *jit_map
    size_t size = exec_size + data_size;
    lfiptr base = p->jit_base;
    uint8_t *alias = p->jit_alias;
    int fd = p->jit_fd;

    p->jit_base = 0;
    p->jit_exec_size = 0;
    p->jit_data_size = 0;
    p->jit_alias = NULL;
    p->jit_fd = -1;

    if (lfi_box_munmap(p->box, base, size) < 0)
        return -LINUX_EINVAL;
    munmap(alias, size);
    close(fd);
    return 0;
}

// Shared core for proc_jit_create and proc_jit_create2: assumes src is a
// contiguous host buffer of length bytes to install at dst.
static int
jit_create_buf(struct LFILinuxProc *p, lfiptr dst, uint8_t *src, size_t length)
{
    if (p->jit_base == 0)
        return -LINUX_EINVAL;
    if (!jit_codebufvalid(p, dst, length))
        return -LINUX_EINVAL;

    struct LFIEngine *engine = p->engine->engine;
    size_t pagesize = lfi_opts(engine).pagesize;
    lfiptr base = truncp(dst, pagesize);
    size_t len = ceilp(dst + length - base, pagesize);

    if (lfi_box_mprotect_noverify(p->box, base, len, LFI_PROT_NONE) < 0)
        return -1;

    uint8_t *alias_dst = jit_get_aliasptr(p, dst);
    memcpy(alias_dst, src, length);

    // Verify only the jitcode compilation unit
    if (!lfi_jitcode_verify(p->box, (uintptr_t)alias_dst, length)) {
        lfi_box_munmap(p->box, base, len);
        return -1;
    }

    flush_icache(alias_dst, length);

    if (lfi_box_mprotect_noverify(p->box, base, len, LFI_PROT_READ | LFI_PROT_EXEC) < 0) {
        return -1;
    }

    return 0;
}

int
proc_jit_create(struct LFILinuxProc *p, lfiptr dst, uint8_t *src, size_t length)
{
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    return jit_create_buf(p, dst, src, length);
}

int
proc_jit_create2(struct LFILinuxProc *p, lfiptr dst, uint8_t *header,
    size_t header_len, uint8_t *body, size_t body_len)
{
    size_t total = header_len + body_len;
    uint8_t *buf = malloc(total);
    if (!buf)
        return -LINUX_ENOMEM;

    memcpy(buf, header, header_len);
    if (body_len > 0)
      memcpy(buf + header_len, body, body_len);

    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    int r = jit_create_buf(p, dst, buf, total);
    free(buf);
    return r;
}

int
proc_jit_modify(struct LFILinuxProc *p, lfiptr src, size_t value,
    size_t patch_len)
{
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    if (p->jit_base == 0)
        return -LINUX_EINVAL;

    // Patch must be instruction aligned
    if (src % 4 != 0 || patch_len % 4 != 0)
        return -LINUX_EINVAL;

    lfiptr dst = src;

    if (!jit_codebufvalid(p, dst, patch_len))
        return -LINUX_EINVAL;

    struct LFIEngine *engine = p->engine->engine;

    size_t pagesize = lfi_opts(engine).pagesize;
    lfiptr base = truncp(dst, pagesize);
    size_t len = ceilp(dst + patch_len - base, pagesize);

    if (lfi_box_mprotect_noverify(p->box, base, len, LFI_PROT_NONE) < 0)
        return -1;

    uint8_t *jit_addr = jit_get_aliasptr(p, dst);
    memcpy(jit_addr, &value, patch_len);

    // Verify only the jitcode compilation unit
    if (!lfi_jitcode_verify(p->box, (uintptr_t)jit_addr, patch_len)) {
        lfi_box_munmap(p->box, base, len);
        return -1;
    }

    flush_icache(jit_addr, patch_len);

    if (lfi_box_mprotect_noverify(p->box, base, len, LFI_PROT_READ | LFI_PROT_EXEC) < 0) {
        return -1;
    }

    return 0;
}

int
proc_jit_delete(struct LFILinuxProc *p, lfiptr addrp, size_t length)
{
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    if (p->jit_base == 0)
        return -LINUX_EINVAL;
    if (!jit_codebufvalid(p, addrp, length))
      return -1;
    memset(jit_get_aliasptr(p, addrp), 0x00, length);
    return 0;
}

int
proc_jit_commit(struct LFILinuxProc *p, lfiptr addrp, size_t length)
{
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    if (!jit_codebufvalid(p, addrp, length))
      return -1;
    memset(jit_get_aliasptr(p, addrp), 0x00, length);
    return lfi_box_mprotect_noverify(p->box, addrp, length,
        LFI_PROT_READ | LFI_PROT_EXEC);
}

int
proc_jit_decommit(struct LFILinuxProc *p, lfiptr addrp, size_t length)
{
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    if (!jit_codebufvalid(p, addrp, length))
      return -1;
    return lfi_box_mprotect_noverify(p->box, addrp, length, LFI_PROT_NONE);
}
