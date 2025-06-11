#pragma once

#include "proc.h"

#include <stdbool.h>
#include <stddef.h>

bool
path_resolve(struct LFILinuxProc *proc, const char *path, char *buffer,
    size_t buffer_size);
