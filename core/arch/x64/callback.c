#if defined(HAVE_MEMFD_CREATE)
#define _GNU_SOURCE
#endif

#include "core.h"

#include <assert.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#if !defined(HAVE_MEMFD_CREATE) && defined(HAVE_SYS_MEMFD_CREATE)
#include <sys/syscall.h>
int
memfd_create(const char *name, unsigned flags)
{
    return syscall(SYS_memfd_create, name, flags);
}
#endif

// The callback entry code loads the target into %r10 and then jumps to the
// trampoline address stored in the callback.
static uint8_t cbentry_code[16] = {
    // clang-format off
    0x4c, 0x8b, 0x15, 0x09, 0x00, 0x00, 0x00, // mov    0x9(%rip),%r10
    0xff, 0x25, 0x0b, 0x00, 0x00, 0x00,       // jmp    *0xb(%rip)
    0x0f, 0x01f, 0x00,                        // nop
    // clang-format on
};

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

EXPORT bool
lfi_box_cbinit(struct LFIBox *box)
{
    int fd = memfd_create("", 0);
    if (fd < 0)
        return false;
    size_t size = MAXCALLBACKS * sizeof(struct CallbackEntry);
    int r = ftruncate(fd, size);
    if (r < 0)
        goto err;
    // Map callback entries outside the sandbox as read/write.
    void *aliasmap = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
        0);
    if (aliasmap == (void *) -1)
        goto err;
    box->cbinfo.cbentries_alias = (struct CallbackEntry *) aliasmap;
    // Fill in the code for each entry.
    for (size_t i = 0; i < MAXCALLBACKS; i++) {
        memcpy(&box->cbinfo.cbentries_alias[i].code, &cbentry_code[0],
            sizeof(box->cbinfo.cbentries_alias[i].code));
    }
    // Share the mapping inside the sandbox as read/exec. This mapping is
    // unverified because it contains specific trampoline sequences that the
    // verifier cannot validate.
    lfiptr boxmap = lfi_box_mapany_noverify(box, size,
        LFI_PROT_READ | LFI_PROT_EXEC, LFI_MAP_SHARED, fd, 0);
    if (boxmap == (lfiptr) -1)
        goto err1;
    box->cbinfo.cbentries_box = (struct CallbackEntry *) boxmap;

    close(fd);
    return true;
err1:
    munmap(aliasmap, size);
err:
    close(fd);
    return false;
}

EXPORT void *
lfi_box_register_cb(struct LFIBox *box, void *fn)
{
    assert(fn);
    assert(cbfind(box, fn) == -1 && "fn is already registered as a callback");

    ssize_t slot = cbfreeslot(box);
    if (slot == -1)
        return NULL;

    // Write 'fn' into the 'target' field for the chosen slot.
    atomic_store_explicit(&box->cbinfo.cbentries_alias[slot].target,
        (uint64_t) fn, memory_order_seq_cst);
    // Write the trampoline into the 'trampoline' field for the chosen slot
    atomic_store_explicit(&box->cbinfo.cbentries_alias[slot].trampoline,
        (uint64_t) lfi_callback, memory_order_seq_cst);

    // Mark the slot as allocated.
    box->callbacks[slot] = fn;

    return &box->cbinfo.cbentries_box[slot].code[0];
}

EXPORT void
lfi_box_unregister_cb(struct LFIBox *box, void *fn)
{
    ssize_t slot = cbfind(box, fn);
    if (slot == -1)
        return;
    box->callbacks[slot] = NULL;
    atomic_store_explicit(&box->cbinfo.cbentries_alias[slot].target, 0,
        memory_order_seq_cst);
    atomic_store_explicit(&box->cbinfo.cbentries_alias[slot].trampoline, 0,
        memory_order_seq_cst);
}
