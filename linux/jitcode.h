#pragma once

#include "proc.h"

// Maps a new JIT code region into the sandbox. exec_size bytes will be
// executable, data_size bytes (immediately after) will be read-only data.
// Returns the sandbox address of the region via o_addr if it succeeds.
int
proc_jit_map(struct LFILinuxProc *p, size_t exec_size, size_t data_size,
    lfiptr *o_addr);

// Unmaps the JIT code region. Arguments must exactly match the values
// given to proc_jit_map.
int
proc_jit_unmap(struct LFILinuxProc *p, lfiptr addrp, size_t exec_size,
    size_t data_size);

int
proc_jit_create(struct LFILinuxProc *p, lfiptr dst, uint8_t *src, size_t length);

int
proc_jit_create2(struct LFILinuxProc *p, lfiptr dst, uint8_t *header,
    size_t header_len, uint8_t *body, size_t body_len);

int
proc_jit_modify(struct LFILinuxProc *p, lfiptr src, size_t value,
    size_t patch_len);

int
proc_jit_delete(struct LFILinuxProc *p, lfiptr addrp, size_t length);

int
proc_jit_commit(struct LFILinuxProc *p, lfiptr addrp, size_t length);

int
proc_jit_decommit(struct LFILinuxProc *p, lfiptr addrp, size_t length);
