#include "sys/sys.h"

#include "align.h"
#include "jitcode.h"
#include "linux.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

uintptr_t
sys_jitcode_mmap(struct LFILinuxThread *t, lfiptr addrp, size_t exec_length,
    size_t data_length)
{
    size_t pagesize = lfi_opts(t->proc->engine->engine).pagesize;
    if (exec_length == 0 || exec_length != ceilp(exec_length, pagesize))
        return -LINUX_EINVAL;
    if (data_length != 0 && data_length != ceilp(data_length, pagesize))
        return -LINUX_EINVAL;

    lfiptr i_addrp = addrp;
    int r = proc_jit_map(t->proc, exec_length, data_length, &addrp);
    if (r < 0) {
        LOG(t->proc->engine, "sys_jitcode_mmap((%lx), %zu, %zu) = %d",
            i_addrp, exec_length, data_length, r);
        return r;
    }
    LOG(t->proc->engine, "sys_jitcode_mmap(%lx (%lx), %zu, %zu) = %lx",
        addrp, i_addrp, exec_length, data_length, addrp);
    return addrp;
}

int
sys_jitcode_munmap(struct LFILinuxThread *t, lfiptr addrp, size_t exec_length,
    size_t data_length)
{
    size_t pagesize = lfi_opts(t->proc->engine->engine).pagesize;
    if (exec_length == 0 || exec_length != ceilp(exec_length, pagesize))
        return -LINUX_EINVAL;
    if (data_length != 0 && data_length != ceilp(data_length, pagesize))
        return -LINUX_EINVAL;
    int r = proc_jit_unmap(t->proc, addrp, exec_length, data_length);
    LOG(t->proc->engine, "sys_jitcode_unmap((%lx), %zu, %zu)",
        addrp, exec_length, data_length);
    return r;
}

int
sys_jitcode_create(struct LFILinuxThread *t, lfiptr addrp, lfiptr bufp,
    size_t length)
{
    int bundle_size = 32;
    if (addrp % bundle_size != 0 || length % bundle_size != 0)
        return -LINUX_EINVAL;

    if (!bufcheck(t, bufp, length, 1))
        return -LINUX_EINVAL;
    uint8_t *buf = copyout(t, bufp, length);
    if (!buf)
        return -LINUX_ENOMEM;

    int r = proc_jit_create(t->proc, addrp, buf, length);
    LOG(t->proc->engine, "sys_jitcode_create((%lx), (%lx), %zu) = %d",
        addrp, bufp, length, r);
    free(buf);
    return r;
}

int
sys_jitcode_create2(struct LFILinuxThread *t, lfiptr addrp, lfiptr headerp,
    size_t header_length, lfiptr bufp, size_t total_length)
{
    if (total_length < header_length)
        return -LINUX_EINVAL;

    int bundle_size = 32;
    if (addrp % bundle_size != 0 || total_length % bundle_size != 0 ||
        header_length % bundle_size != 0)
        return -LINUX_EINVAL;

    size_t body_length = total_length - header_length;

    if (!bufcheck(t, headerp, header_length, 1))
        return -LINUX_EINVAL;
    if (body_length > 0 && !bufcheck(t, bufp, body_length, 1))
        return -LINUX_EINVAL;

    uint8_t *header = copyout(t, headerp, header_length);
    if (!header)
        return -LINUX_ENOMEM;

    uint8_t *body = NULL;
    if (body_length > 0) {
        body = copyout(t, bufp, body_length);
        if (!body) {
            free(header);
            return -LINUX_ENOMEM;
        }
    }

    int r = proc_jit_create2(t->proc, addrp, header, header_length,
        body, body_length);
    LOG(t->proc->engine, "sys_jitcode_create2((%lx), (%p), %zu, (%p), %zu) = %d",
        addrp, header, header_length, body, body_length, r);
    free(header);
    free(body);
    return r;
}

int
sys_jitcode_modify(struct LFILinuxThread *t, lfiptr addrp, size_t valp,
    size_t length, int halt_pad)
{
    if (length > 5)
        return -LINUX_EINVAL;
    int r = proc_jit_modify(t->proc, addrp, valp, length, halt_pad);
    LOG(t->proc->engine, "sys_jitcode_modify((%lx), %zu, %zu, %d) = %d",
        addrp, valp, length, halt_pad, r);
    return r;
}

int
sys_jitcode_delete(struct LFILinuxThread *t, lfiptr addrp, size_t length)
{
    LOG(t->proc->engine, "sys_jitcode_delete((%lx), %zu)",
        addrp, length);
    return proc_jit_delete(t->proc, addrp, length);
}

int
sys_jitcode_commit(struct LFILinuxThread *t, lfiptr addrp, size_t length)
{
    LOG(t->proc->engine, "sys_jitcode_commit((%lx), %zu)",
        addrp, length);
    return proc_jit_commit(t->proc, addrp, length);
}

int
sys_jitcode_decommit(struct LFILinuxThread *t, lfiptr addrp, size_t length)
{
    LOG(t->proc->engine, "sys_jitcode_decommit((%lx), %zu)",
        addrp, length);
    return proc_jit_decommit(t->proc, addrp, length);
}
