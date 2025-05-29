#include "lfi_core.h"
#include "core.h"

#include <stdlib.h>

EXPORT struct LFIContext *
lfi_ctx_new(struct LFIBox *box, void *ctxp)
{
    struct LFIContext *ctx = malloc(sizeof(struct LFIContext));
    if (!ctx) {
        lfi_error = LFI_ERR_ALLOC;
        return NULL;
    }


}
