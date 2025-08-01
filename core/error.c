#include "core.h"
#include "lfi_core.h"

#include <stdlib.h>

thread_local int lfi_error;
thread_local char *lfi_error_desc;

EXPORT int
lfi_errno(void)
{
    return lfi_error;
}

static const char *
name(int err)
{
    switch (err) {
    case LFI_ERR_NONE:
        return "no error";
    case LFI_ERR_ALLOC:
        return "allocation failed";
    case LFI_ERR_BOXMAP:
        return "boxmap failed";
    case LFI_ERR_RESERVE:
        return "memory reservation failed";
    case LFI_ERR_MMAP:
        return "libmmap failed";
    case LFI_ERR_PKU:
        return "pku operation failed";
    case LFI_ERR_SIGALTSTACK:
        return "sigaltstack failed";
    default:
        return "unknown error";
    }
}

EXPORT const char *
lfi_errmsg(void)
{
    const char *err = name(lfi_error);
    if (lfi_error_desc) {
        const char *full = xasprintf("%s: %s", err, lfi_error_desc);
        free(lfi_error_desc);
        lfi_error_desc = NULL;
        return full;
    }
    return err;
}
