#ifdef HAVE_MEMFD_CREATE
#define _GNU_SOURCE
#endif

#include "core.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#if !defined(HAVE_MEMFD_CREATE) && defined(HAVE_SYS_MEMFD_CREATE)
#include <sys/syscall.h>
int
memfd_create(const char *name, unsigned flags)
{
    return syscall(SYS_memfd_create, name, flags);
}
#elif defined(__APPLE__)
int
memfd_create(const char *name, unsigned int flags)
{
    char template[] = "/tmp/memfd-XXXXXX";
    int fd = mkstemp(template);
    if (fd == -1)
        return -1;
    if (unlink(template) == -1) {
        close(fd);
        return -1;
    }
    return fd;
}
#endif

extern void
lfi_callback() asm("lfi_callback");

extern void
lfi_callback_struct() asm("lfi_callback_struct");

static ssize_t
cbfreeslot(struct LFIBox *box)
{
    for (ssize_t i = 0; i < MAXCALLBACKS; i++) {
        if (!box->callbacks[i])
            return i;
    }
    return -1;
}

static ssize_t
cbfind(struct LFIBox *box, void *fn)
{
    for (size_t i = 0; i < MAXCALLBACKS; i++) {
        if (box->callbacks[i] == fn)
            return i;
    }
    return -1;
}

#define LDRLIT(reg, lit) ((0b01011000 << 24) | (((lit) >> 2) << 5) | (reg))

// Code entry for a callback. The target goes in x16 and the trampoline (actual
// branch target out of the sandbox) goes in x28.
static uint32_t cbtrampoline[4] = {
    LDRLIT(16, MAXCALLBACKS * sizeof(struct CallbackEntry)), // ldr x16, .+X
    LDRLIT(28,
        4 + MAXCALLBACKS * sizeof(struct CallbackEntry)), // ldr x28, .+X+4
    0xd61f0380,                                           // br x28
    0xd503201f,                                           // nop
};

_Static_assert(sizeof(struct CallbackDataEntry) == sizeof(struct CallbackEntry),
    "invalid CallbackEntry size");
_Static_assert(MAXCALLBACKS * sizeof(struct CallbackEntry) % 16384 == 0,
    "invalid MAXCALLBACKS");

EXPORT bool
lfi_box_cbinit(struct LFIBox *box)
{
    int fd = memfd_create("", 0);
    if (fd < 0)
        return false;
    size_t size = 2 * MAXCALLBACKS * sizeof(struct CallbackEntry);
    int r = ftruncate(fd, size);
    if (r < 0)
        goto err;
    // Map callback data entries outside the sandbox as read/write.
    void *aliasmap = mmap(NULL, size / 2, PROT_READ | PROT_WRITE, MAP_SHARED,
        fd, 0);
    if (aliasmap == (void *) -1)
        goto err;
    box->cbinfo.dataentries_alias = (struct CallbackDataEntry *) aliasmap;
    // Map the code entry region.
    lfiptr codemap = lfi_box_mapany(box, size / 2,
        LFI_PROT_READ | LFI_PROT_WRITE, LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1,
        0);
    if (codemap == (lfiptr) -1)
        goto err1;
    // Fill in the code for each entry.
    for (size_t i = 0; i < MAXCALLBACKS; i++) {
        lfi_box_copyto(box, codemap + sizeof(struct CallbackEntry) * i,
            &cbtrampoline[0], sizeof(struct CallbackEntry));
    }
    // Mark the code region as R/X. This is unverified because the code region
    // entries are manually constructed by the runtime.
    r = lfi_box_mprotect_noverify(box, codemap, size / 2,
        LFI_PROT_READ | LFI_PROT_EXEC);
    if (r == -1)
        goto err1;
    lfiptr boxmap = lfi_box_mapat(box, codemap + size / 2, size / 2,
        LFI_PROT_READ, LFI_MAP_SHARED, fd, 0);
    if (boxmap == (lfiptr) -1)
        goto err2;
    box->cbinfo.cbentries = (struct CallbackEntry *) codemap;
    box->cbinfo.dataentries_box = (struct CallbackDataEntry *) boxmap;

    close(fd);
    return true;
err2:
    lfi_box_munmap(box, codemap, size / 2);
err1:
    munmap(aliasmap, size / 2);
err:
    close(fd);
    return false;
}

static void *
register_cb(struct LFIBox *box, void *fn, uint64_t lfi_callback_fn)
{
    assert(fn);
    assert(cbfind(box, fn) == -1 && "fn is already registered as a callback");

    ssize_t slot = cbfreeslot(box);
    if (slot == -1)
        return NULL;

    // write 'fn' into the 'target' field for the chosen slot.
    atomic_store_explicit(&box->cbinfo.dataentries_alias[slot].target,
        (uint64_t) fn, memory_order_seq_cst);
    // write the trampoline into the 'trampoline' field for the chosen slot
    atomic_store_explicit(&box->cbinfo.dataentries_alias[slot].trampoline,
        lfi_callback_fn, memory_order_seq_cst);

    // Mark the slot as allocated.
    box->callbacks[slot] = fn;

    return &box->cbinfo.cbentries[slot].code[0];
}

EXPORT void *
lfi_box_register_cb_struct(struct LFIBox *box, void *fn)
{
    return register_cb(box, fn, (uint64_t) lfi_callback_struct);
}

EXPORT void *
lfi_box_register_cb(struct LFIBox *box, void *fn)
{
    return register_cb(box, fn, (uint64_t) lfi_callback);
}

EXPORT void
lfi_box_unregister_cb(struct LFIBox *box, void *fn)
{
    ssize_t slot = cbfind(box, fn);
    if (slot == -1)
        return;
    box->callbacks[slot] = NULL;
    atomic_store_explicit(&box->cbinfo.dataentries_alias[slot].target, 0,
        memory_order_seq_cst);
    atomic_store_explicit(&box->cbinfo.dataentries_alias[slot].trampoline, 0,
        memory_order_seq_cst);
}
