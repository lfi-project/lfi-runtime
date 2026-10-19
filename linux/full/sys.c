#include "full/sys/sys.h"

#include "sys.h"

// Forwards a syscall to the host as is (see full_sys_passthrough).
#define PASS(SYSNAME)                                                    \
    case LINUX_SYS_##SYSNAME:                                            \
        *o_ret = full_sys_passthrough(t, sysno, a0, a1, a2, a3, a4, a5); \
        return true;

// Syscalls that are only available in passthrough mode. Everything that
// reaches the host here is UNSAFE and only meant for testing and
// benchmarking. The list covers what the LLVM test suite and SPEC CPU 2017
// need: none of these return a pointer into the sandbox, so passing them
// through cannot hand the sandbox memory outside of it (memory management
// stays with the handlers above).
static bool
passthrough_syshandle(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0,
    uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5,
    uintptr_t *o_ret)
{
    // clang-format off
    switch (sysno) {
    // I/O on file descriptors.
    PASS(read)
    PASS(write)
    PASS(readv)
    PASS(writev)
    PASS(lseek)
    PASS(close)
    PASS(ioctl)
    PASS(fcntl)
    PASS(dup3)
#ifdef LINUX_SYS_dup2
    PASS(dup2)
#endif

    // Filesystem and path operations. Paths are host paths, relative to the
    // host working directory.
    PASS(openat)
    PASS(fstat)
    PASS(newfstatat)
    PASS(faccessat)
    PASS(unlinkat)
#ifdef LINUX_SYS_renameat
    PASS(renameat)
#endif
    PASS(renameat2)
    PASS(readlinkat)
    PASS(getcwd)
    PASS(chdir)
    PASS(getdents64)
    PASS(umask)
#ifdef LINUX_SYS_open
    PASS(open)
#endif
#ifdef LINUX_SYS_stat
    PASS(stat)
#endif
#ifdef LINUX_SYS_lstat
    PASS(lstat)
#endif
#ifdef LINUX_SYS_access
    PASS(access)
#endif
#ifdef LINUX_SYS_unlink
    PASS(unlink)
#endif
#ifdef LINUX_SYS_rename
    PASS(rename)
#endif
#ifdef LINUX_SYS_readlink
    PASS(readlink)
#endif

    // Time, resources, and identity. There is no vDSO in the sandbox, so the
    // time calls always reach the runtime.
    PASS(clock_gettime)
    PASS(gettimeofday)
#ifdef LINUX_SYS_time
    PASS(time)
#endif
    PASS(getrusage)
    PASS(prlimit64)
    PASS(sysinfo)
    PASS(getpid)
    PASS(gettid)

    // Signals are never delivered to the sandbox, so these are stubs.
    FULL_SYS(rt_sigaction,
            full_sys_rt_sigaction(t, a0, a1, a2, a3))
    FULL_SYS(rt_sigprocmask,
            full_sys_rt_sigprocmask(t, a0, a1, a2, a3))
    }
    // clang-format on

    return false;
}

bool
full_syshandle(struct LFILinuxThread *t, uintptr_t sysno, uintptr_t a0,
    uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5,
    uintptr_t *o_ret)
{
    // clang-format off
    switch (sysno) {
    FULL_SYS(mmap,
            full_sys_mmap(t, a0, a1, a2, a3, a4, a5))
    FULL_SYS(mprotect,
            full_sys_mprotect(t, a0, a1, a2))
    FULL_SYS(munmap,
            full_sys_munmap(t, a0, a1))
    }
    // clang-format on

    if (t->proc->engine->opts.sys_passthrough)
        return passthrough_syshandle(t, sysno, a0, a1, a2, a3, a4, a5, o_ret);

    return false;
}
