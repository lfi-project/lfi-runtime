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

// Initialize a sandbox with the Linux library engine. The library engine is
// used for sandboxes that are loaded by the dynamic loader rather than by
// liblfi itself. In this model, a sandbox region exists as a set of PT_LOAD
// segments inside a .so file. When that .so is loaded, the dynamic loader
// automatically maps the sandbox and then runs the .so's constructor. The
// constructor should call this function to register the sandbox with the
// 'library engine.'
bool
lfi_linux_lib_init(void *base, void *end, void *entry, void *phdrs,
    struct LFIOptions opts, struct LFILinuxOptions linux_opts);

// Creates a new LFILinuxProc for the given LFIBox.
struct LFILinuxProc *
lfi_proc_new(struct LFILinuxEngine *engine, struct LFIBox *box);

// Returns the LFIBox associated with the given proc.
struct LFIBox *
lfi_proc_box(struct LFILinuxProc *proc);

// Loads an ELF program in the proc's address space.
bool
lfi_proc_load(struct LFILinuxProc *proc, uint8_t *prog, size_t prog_size);

// Frees an LFILinuxProc.
void
lfi_proc_free(struct LFILinuxProc *proc);

// Creates a new LFILinuxThread from an ELF file and a command-line.
struct LFILinuxThread *
lfi_thread_new(struct LFILinuxProc *proc, int argc, char **argv, char **envp);

// Begins executed an LFILinuxThread.
int
lfi_thread_run(struct LFILinuxThread *p);

// Frees an LFILinuxThread.
void
lfi_thread_free(struct LFILinuxThread *t);

#ifdef __cplusplus
}
#endif
