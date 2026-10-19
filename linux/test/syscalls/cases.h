#pragma once

// Differential syscall test cases. Every case is a function
//
//     long case_<name>(void);
//
// that issues raw system calls and returns 0 when Linux's behavior was
// observed, -errno when a syscall failed, or a small positive code
// identifying which property check failed. cases.c is compiled twice: natively
// into the test driver (giving the real Linux result) and with the LFI
// toolchain into a sandboxed library (giving the runtime's result).
//
// The second, third, and fourth columns say what the minimal runtime, the
// full runtime, and the full runtime in passthrough mode
// (LFILinuxOptions.sys_passthrough) are expected to return. LINUX means "the
// same as native Linux"; anything else is a documented deviation and is
// compared literally.
//
// Column key: name, minimal expectation, full expectation, passthrough
// expectation.
#define SYSCALL_CASES(X)                                       \
    /* brk */                                                  \
    X(brk_query, LINUX, LINUX, LINUX)                          \
    X(brk_grow_shrink, LINUX, LINUX, LINUX)                    \
    X(brk_regrow_zeroed, LINUX, LINUX, LINUX)                  \
    X(brk_below_start, LINUX, LINUX, LINUX)                    \
    X(brk_absurd, LINUX, LINUX, LINUX)                         \
    X(brk_shrink_unmaps, -ENOSYS, LINUX, LINUX)                \
    X(brk_grow_blocked, -ENOSYS, LINUX, LINUX)                 \
    X(brk_grow_over_unmapped, -ENOSYS, LINUX, LINUX)           \
    /* mmap: the minimal runtime has no mmap at all */         \
    X(mmap_anon, -ENOSYS, LINUX, LINUX)                        \
    X(mmap_len0, -ENOSYS, LINUX, LINUX)                        \
    X(mmap_len_overflow, -ENOSYS, LINUX, LINUX)                \
    X(mmap_no_type, -ENOSYS, LINUX, LINUX)                     \
    X(mmap_unknown_prot, -ENOSYS, LINUX, LINUX)                \
    X(mmap_fixed, -ENOSYS, LINUX, LINUX)                       \
    X(mmap_fixed_unaligned, -ENOSYS, LINUX, LINUX)             \
    X(mmap_offset_unaligned, -ENOSYS, LINUX, LINUX)            \
    X(mmap_anon_ignores_fd, -ENOSYS, LINUX, LINUX)             \
    X(mmap_prot_none_then_rw, -ENOSYS, LINUX, LINUX)           \
    /* shared and executable mappings are refused by design */ \
    X(mmap_shared_anon, -ENOSYS, -EINVAL, -EINVAL)             \
    X(mmap_exec_anon, -ENOSYS, -EACCES, -EACCES)               \
    /* file mappings: only passthrough has file descriptors */ \
    X(mmap_file_badfd, -ENOSYS, LINUX, LINUX)                  \
    X(mmap_file_private, -ENOSYS, -ENOSYS, LINUX)              \
    X(mmap_file_shared, -ENOSYS, -ENOSYS, LINUX)               \
    X(mmap_file_wronly, -ENOSYS, -ENOSYS, LINUX)               \
    X(mmap_file_dir, -ENOSYS, -ENOSYS, LINUX)                  \
    X(mmap_file_opath, -ENOSYS, -ENOSYS, LINUX)                \
    X(mmap_file_bad_offset, -ENOSYS, -ENOSYS, LINUX)           \
    X(mmap_file_fixed_keeps, -ENOSYS, -ENOSYS, LINUX)          \
    /* munmap */                                               \
    X(munmap_len0, -ENOSYS, LINUX, LINUX)                      \
    X(munmap_unaligned, -ENOSYS, LINUX, LINUX)                 \
    X(munmap_unmapped, -ENOSYS, LINUX, LINUX)                  \
    /* mprotect */                                             \
    X(mprotect_ro_rw, -ENOSYS, LINUX, LINUX)                   \
    X(mprotect_len0, -ENOSYS, LINUX, LINUX)                    \
    X(mprotect_unaligned, -ENOSYS, LINUX, LINUX)               \
    X(mprotect_unmapped, -ENOSYS, LINUX, LINUX)                \
    X(mprotect_unknown_prot, -ENOSYS, LINUX, LINUX)            \
    X(mprotect_sem, -ENOSYS, LINUX, LINUX)                     \
    X(mprotect_above_brk, -ENOSYS, LINUX, LINUX)               \
    X(mprotect_exec, -ENOSYS, -EACCES, -EACCES)                \
    /* mremap is rejected in every configuration */            \
    X(mremap_grow, -ENOSYS, -ENOSYS, -ENOSYS)                  \
    /* passthrough: file descriptors and paths */              \
    X(file_rw, -ENOSYS, -ENOSYS, LINUX)                        \
    X(file_readv_writev, -ENOSYS, -ENOSYS, LINUX)              \
    X(file_stat, -ENOSYS, -ENOSYS, LINUX)                      \
    X(file_rename, -ENOSYS, -ENOSYS, LINUX)                    \
    X(file_readlink_regular, -ENOSYS, -ENOSYS, LINUX)          \
    X(file_dup_fcntl, -ENOSYS, -ENOSYS, LINUX)                 \
    X(file_ioctl_notty, -ENOSYS, -ENOSYS, LINUX)               \
    X(cwd_chdir, -ENOSYS, -ENOSYS, LINUX)                      \
    X(getdents64, -ENOSYS, -ENOSYS, LINUX)                     \
    X(umask, -ENOSYS, -ENOSYS, LINUX)                          \
    /* passthrough: time and resources */                      \
    X(clock_gettime, -ENOSYS, -ENOSYS, LINUX)                  \
    X(gettimeofday, -ENOSYS, -ENOSYS, LINUX)                   \
    X(resources, -ENOSYS, -ENOSYS, LINUX)                      \
    /* signal stubs and identity (rt_sigprocmask is needed for \
       threads, so the full runtime always has it) */          \
    X(rt_sigaction, -ENOSYS, -ENOSYS, LINUX)                   \
    X(rt_sigaction_invalid, -ENOSYS, -ENOSYS, LINUX)           \
    X(rt_sigprocmask, -ENOSYS, LINUX, LINUX)                   \
    X(getpid, -ENOSYS, -ENOSYS, LINUX)                         \
    /* threads: the minimal runtime has no clone (or mmap) */  \
    X(gettid, -ENOSYS, LINUX, LINUX)                           \
    X(set_tid_address, 1, LINUX, LINUX)                        \
    X(set_tid_address_null, 1, LINUX, LINUX)                   \
    X(thread_create_join, -EAGAIN, LINUX, LINUX)               \
    X(thread_sequential, -EAGAIN, LINUX, LINUX)                \
    X(thread_tid, -EAGAIN, LINUX, LINUX)                       \
    X(thread_tls, -EAGAIN, LINUX, LINUX)                       \
    X(thread_condvar, -EAGAIN, LINUX, LINUX)                   \
    X(clone_tids, -ENOSYS, LINUX, LINUX)                       \
    X(clone_newns_fs, -ENOSYS, LINUX, LINUX)                   \
    /* only threads can be created, on a sandbox stack */      \
    X(clone_fork, -ENOSYS, -ENOSYS, -ENOSYS)                   \
    X(clone_no_thread, -ENOSYS, -EINVAL, -EINVAL)              \
    X(clone_bad_stack, -ENOSYS, -EFAULT, -EFAULT)              \
    X(futex_wait_mismatch, -ENOSYS, LINUX, LINUX)              \
    X(futex_wait_timeout, -ENOSYS, LINUX, LINUX)               \
    X(futex_wake_none, -ENOSYS, LINUX, LINUX)                  \
    X(futex_invalid, -ENOSYS, LINUX, LINUX)                    \
    /* misc */                                                 \
    X(bogus_sysno, LINUX, LINUX, LINUX)

#if defined(__x86_64__)
#define SYSCALL_CASES_ARCH(X)                                       \
    X(arch_prctl_set_fs, LINUX, LINUX, LINUX)                       \
    X(arch_prctl_bad_code, LINUX, LINUX, LINUX)                     \
    X(arch_prctl_get_fs, LINUX, LINUX, LINUX)                       \
    /* passthrough: the legacy path syscalls musl uses on x86-64 */ \
    X(legacy_paths, -ENOSYS, -ENOSYS, LINUX)
#else
#define SYSCALL_CASES_ARCH(X)
#endif

#define DECLARE_CASE(name, min, full, pass) long case_##name(void);
SYSCALL_CASES(DECLARE_CASE)
SYSCALL_CASES_ARCH(DECLARE_CASE)
#undef DECLARE_CASE
