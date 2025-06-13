#ifdef HAVE_MEMFD_CREATE
#define _GNU_SOURCE
#endif

#include "core.h"

#include <assert.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>

#if defined(HAVE_MEMFD_CREATE)
#include <sys/mman.h>
#elif defined(HAVE_SYS_MEMFD_CREATE)
#include <sys/syscall.h>
int
memfd_create(const char *name, unsigned flags)
{
    return syscall(SYS_memfd_create, name, flags);
}
#endif

extern void
lfi_callback();

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
// branch target out of the sandbox) goes in x18.
static uint32_t cbtrampoline[4] = {
    LDRLIT(16, MAXCALLBACKS * sizeof(struct CallbackEntry)), // ldr x16, .+X
    LDRLIT(18,
        4 + MAXCALLBACKS * sizeof(struct CallbackEntry)), // ldr x18, .+X+4
    0xd61f0240,                                           // br x18
    0xd503201f,                                           // nop
};

_Static_assert(sizeof(struct CallbackDataEntry) == sizeof(struct CallbackEntry),
    "invalid CallbackEntry size");
_Static_assert(MAXCALLBACKS * sizeof(struct CallbackEntry) % 16384 == 0,
    "invalid MAXCALLBACKS");

bool
lfi_box_cbinit(struct LFIBox *box)
{
    int fd = memfd_create("", 0);
    if (fd < 0)
        return false;
    size_t size = 2 * MAXCALLBACKS * sizeof(struct CallbackEntry);
    int r = ftruncate(fd, size);
    if (r < 0)
        goto err;
    // Map callback entries outside the sandbox as read/write.
    void *aliasmap = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
        0);
    if (aliasmap == (void *) -1)
        goto err;
    box->cbinfo.cbentries_alias = (struct CallbackEntry *) aliasmap;
    box->cbinfo.dataentries_alias = (struct CallbackDataEntry *) (aliasmap +
        size / 2);
    // Fill in the code for each entry.
    for (size_t i = 0; i < MAXCALLBACKS; i++) {
        memcpy(&box->cbinfo.cbentries_alias[i].code, &cbtrampoline[0],
            sizeof(box->cbinfo.cbentries_alias[i].code));
    }
    // Share the mapping inside the sandbox as read/exec. We disable
    // verification because the code entries are hand-crafted and the verifier
    // will not be able to validate their targets, which are inserted
    // dynamically when creating callbacks.
    lfiptr boxmap = lfi_box_mapany_noverify(box, size,
        LFI_PROT_READ | LFI_PROT_EXEC, LFI_MAP_SHARED, fd, 0);
    if (boxmap == (lfiptr) -1)
        goto err1;
    // Mark the data region as non-executable.
    r = lfi_box_mprotect(box, boxmap + size / 2, size / 2, LFI_PROT_READ);
    if (r != 0)
        goto err2;
    assert(boxmap + size / 2 + size / 2 == boxmap + size);
    box->cbinfo.cbentries_box = (struct CallbackEntry *) boxmap;
    box->cbinfo.dataentries_box = (struct CallbackDataEntry *) (boxmap +
        size / 2);

    close(fd);
    return true;
err2:
    lfi_box_munmap(box, boxmap, size);
err1:
    munmap(aliasmap, size);
err:
    close(fd);
    return false;
}

void *
lfi_box_register_cb(struct LFIBox *box, void *fn)
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
        (uint64_t) lfi_callback, memory_order_seq_cst);

    // Mark the slot as allocated.
    box->callbacks[slot] = fn;

    return &box->cbinfo.cbentries_box[slot].code[0];
}

void
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
