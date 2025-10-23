#pragma once

#include "lfi_core.h"

#ifdef __cplusplus
extern "C" {
#endif

struct LFILinuxOptions {
    // Stack size to use for processes (2mb recommended).
    size_t stacksize;

    // Enable verbose debug information.
    bool verbose;

    // Enable generation of a perf.map file containing symbol mappings.
    bool perf;

    // Null-terminated list of directory mappings from the sandbox FS to the
    // host. Each directory mapping is expressed as sandbox_path=host_path. For
    // example, you might have /=/path/to/lfi-sysroot along with /work=$PWD.
    const char **dir_maps;

    // Working directory that the sandbox starts in, within the sandbox FS. For
    // example, you might set wd to /work if you have mapped $PWD=/work.
    const char *wd;

    // Exit the entire process when an unknown syscall is executed, rather than
    // returning ENOSYS. This is useful for debugging when we want programs
    // that use unsupported syscalls to crash.
    bool exit_unknown_syscalls;

    // Pass most system calls through to the host without checking them
    // (UNSAFE). Useful for testing and benchmarking for system calls that the
    // runtime doesn't support. Only works on Linux.
    bool sys_passthrough;

    // Enable debugger support by hooking into the _r_debug symbol.
    bool debug;

    // Use brk_size for the the brk heap size, instead of the system default.
    bool brk_control;

    // Size to use for the default brk heap (512mb recommended).
    size_t brk_size;
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

// Initialize a global engine for use with libraries.
bool
lfi_linux_lib_init(struct LFIOptions opts, struct LFILinuxOptions linux_opts);

// Return a pointer to the library engine.
struct LFILinuxEngine *
lfi_linux_lib_engine(void);

// Frees the resources allocated for the linux engine (but not the underlying
// LFIEngine).
void
lfi_linux_free(struct LFILinuxEngine *engine);

// Creates a new LFILinuxProc.
struct LFILinuxProc *
lfi_proc_new(struct LFILinuxEngine *engine);

// Returns the LFIBox associated with the given proc.
struct LFIBox *
lfi_proc_box(struct LFILinuxProc *proc);

// Loads an ELF program in the proc's address space. The path is used to set up
// debugging, and can be NULL if unknown. The buffer is not modified by the
// loader and can be freed after loading is complete.
bool
lfi_proc_load(struct LFILinuxProc *proc, uint8_t *prog, size_t prog_size,
    const char *prog_path);

// Load an ELF program directly from a readable file descriptor. The path is
// used to set up debugging, and can be NULL if unknown. The file descriptor
// may be closed after loading is complete.
bool
lfi_proc_load_fd(struct LFILinuxProc *proc, int fd, const char *prog_path);

// Load an ELF program directly from a file.
bool
lfi_proc_load_file(struct LFILinuxProc *proc, const char *prog_path);

// Reload the same ELF file that this proc was initially loaded with. The
// reload will reset all LFI process state to its initial values.
bool
lfi_proc_reload(struct LFILinuxProc *proc, uint8_t *prog, size_t prog_size);

// Look up the address of the given symbol name. Must be called after an ELF
// image has been loaded in proc. Returns 0 if not found.
uint64_t
lfi_proc_sym(struct LFILinuxProc *proc, const char *symname);

// Frees an LFILinuxProc.
void
lfi_proc_free(struct LFILinuxProc *proc);

// Creates a new LFILinuxThread from an ELF file and a command-line.
struct LFILinuxThread *
lfi_thread_new(struct LFILinuxProc *proc, int argc, const char **argv,
    const char **envp);

// Reload a LFILinuxThread by resetting it back to an initial state.
bool
lfi_thread_reload(struct LFILinuxThread *t, int argc, const char **argv,
    const char **envp);

// Begins executed an LFILinuxThread.
int
lfi_thread_run(struct LFILinuxThread *t);

// Frees an LFILinuxThread.
void
lfi_thread_free(struct LFILinuxThread *t);

struct LFIContext **
lfi_thread_ctxp(struct LFILinuxThread *t);

// Initialize the ability to dynamically spawn new sandbox threads when
// invoking from new host threads.
void
lfi_linux_init_clone(struct LFILinuxThread *main_thread);

// Memory management functions for sandboxed libraries.
void *
lfi_lib_malloc(struct LFIBox *box, struct LFIContext **ctxp, size_t size);

void *
lfi_lib_realloc(struct LFIBox *box, struct LFIContext **ctxp, size_t size);

void *
lfi_lib_calloc(struct LFIBox *box, struct LFIContext **ctxp, size_t count,
    size_t size);

void
lfi_lib_free(struct LFIBox *box, struct LFIContext **ctxp, void *p);

#ifdef __cplusplus
}
#endif
