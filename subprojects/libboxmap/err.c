#include <string.h>

#include "err.h"

_Thread_local char* g_err;

char* boxmap_strerror(void) {
    if (g_err != ERR_OK) {
        return g_err;
    }
    return NULL;
}
