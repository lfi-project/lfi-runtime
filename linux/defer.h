#pragma once

#include <stdlib.h>

static inline void
defer_free(char **pp)
{
    free(*pp);
}

#define FREE_DEFER(p) p __attribute__((cleanup(defer_free)))
