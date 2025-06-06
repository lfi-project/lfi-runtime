#pragma once

#include "lfi_core.h"

#ifdef __cplusplus
extern "C" {
#endif

struct LFILinuxOptions {
    // Stack size to use for processes.
    size_t stacksize;

    // Enable verbose debug information.
    bool verbose;

    // Enable generation of a perf.map file containing symbol mappings.
    bool perf;
};

// An LFILinuxEngine tracks a set of LFILinuxProcs and LFILinuxThreads.
struct LFILinuxEngine;

// An LFILinuxProc represents an LFI sandbox region for a sandbox that is being
// run in Linux emulation mode.
struct LFILinuxProc;

// An LFILinuxThread stores the state associated with a single execution
// context of an LFILinuxProc. There can be multiple LFILinuxThreads associated
// with a single LFILinuxProc.
struct LFILinuxThread;

// Creates a new LFILinuxEngine from an LFIEngine.
struct LFILinuxEngine *
lfi_linux_new(struct LFIEngine *engine, struct LFILinuxOptions opts);

// Creates a new LFILinuxThread from an ELF file and a command-line.
struct LFILinuxThread *
lfi_linux_thread_new(struct LFILinuxEngine *engine, uint8_t *prog,
    size_t progsize, int argc, char **argv, char **envp);

// Begins executed an LFILinuxThread.
int
lfi_linux_thread_run(struct LFILinuxThread *p);

// Frees an LFILinuxThread.
void
lfi_linux_thread_free(struct LFILinuxThread *t);

#ifdef __cplusplus
}
#endif
