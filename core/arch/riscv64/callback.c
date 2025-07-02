// Define _GNU_SOURCE unconditionally at the very top
#define _GNU_SOURCE

#include "core.h"

#include <assert.h>
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
#endif

EXPORT bool
lfi_box_cbinit(struct LFIBox *box)
{
    assert(!"unimplemented");
}

EXPORT void *
lfi_box_register_cb(struct LFIBox *box, void *fn)
{
    assert(!"unimplemented");
}

EXPORT void
lfi_box_unregister_cb(struct LFIBox *box, void *fn)
{
    assert(!"unimplemented");
}
