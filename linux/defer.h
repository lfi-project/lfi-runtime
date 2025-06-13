#pragma once

#include <stdlib.h>

static inline void
free_defer(char **pp)
{
    free(*pp);
}

#define FREE_DEFER(p) p __attribute__((cleanup(free_defer)))
