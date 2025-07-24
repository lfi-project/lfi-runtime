#pragma once

#include "proc.h"

#include <stddef.h>
#include <stdint.h>

void
db_register_load(struct LFILinuxProc *proc, const char *filename,
    uint8_t *prog_data, size_t prog_size, uintptr_t load_addr);
