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

int
proc_jit_map(struct LFILinuxProc *p, size_t exec_size, size_t data_size,
    lfiptr *o_addr)
{
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    if (p->jit_base != 0)
        return -LINUX_EINVAL;

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

    // TODO: this will raise two mprotects NONE->READ-(verifier)->READ/EXEC
    if (lfi_box_mprotect(p->box, base, len, LFI_PROT_READ | LFI_PROT_EXEC) < 0) {
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
    size_t patch_len, int halt_pad)
{
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    if (p->jit_base == 0)
        return -LINUX_EINVAL;

    size_t bundle_mask = 0xffffffffffffffe0UL;
    size_t patch_offset = src & ~bundle_mask;
    lfiptr dst = src & bundle_mask;

    // Patch must not span two bundles.
    if (dst != ((src + patch_len - 1) & bundle_mask))
        return -LINUX_EINVAL;

    size_t size = 32;

    if (!jit_codebufvalid(p, dst, size))
        return -LINUX_EINVAL;

    struct LFIEngine *engine = p->engine->engine;

    size_t pagesize = lfi_opts(engine).pagesize;
    lfiptr base = truncp(dst, pagesize);
    size_t len = ceilp(dst + size - base, pagesize);

    if (lfi_box_mprotect_noverify(p->box, base, len, LFI_PROT_NONE) < 0)
        return -1;

    uint8_t *jit_addr = jit_get_aliasptr(p, dst);
    if (halt_pad)
        memset(jit_addr, 0xf4, patch_offset);
    memcpy(jit_addr + patch_offset, &value, patch_len);

    // TODO: this will raise two mprotects NONE->READ-(verifier)->READ/EXEC
    if (lfi_box_mprotect(p->box, base, len, LFI_PROT_READ | LFI_PROT_EXEC) < 0) {
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
    return 0;
}

int
proc_jit_commit(struct LFILinuxProc *p, lfiptr addrp, size_t length)
{
    LOCK_WITH_DEFER(&p->lk_box, lk_box);
    if (!jit_codebufvalid(p, addrp, length))
      return -1;
    memset(jit_get_aliasptr(p, addrp), 0xcc, length);
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
