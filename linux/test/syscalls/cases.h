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
// The second and third columns say what the minimal and full runtime
// configurations are expected to return. LINUX means "the same as native
// Linux"; anything else is a documented deviation and is compared literally.
//
// Column key: name, minimal expectation, full expectation.
#define SYSCALL_CASES(X)                                       \
    /* brk */                                                  \
    X(brk_query, LINUX, LINUX)                                 \
    X(brk_grow_shrink, LINUX, LINUX)                           \
    X(brk_regrow_zeroed, LINUX, LINUX)                         \
    X(brk_below_start, LINUX, LINUX)                           \
    X(brk_absurd, LINUX, LINUX)                                \
    X(brk_shrink_unmaps, -ENOSYS, LINUX)                       \
    X(brk_grow_blocked, -ENOSYS, LINUX)                        \
    X(brk_grow_over_unmapped, -ENOSYS, LINUX)                  \
    /* mmap: the minimal runtime has no mmap at all */         \
    X(mmap_anon, -ENOSYS, LINUX)                               \
    X(mmap_len0, -ENOSYS, LINUX)                               \
    X(mmap_len_overflow, -ENOSYS, LINUX)                       \
    X(mmap_no_type, -ENOSYS, LINUX)                            \
    X(mmap_unknown_prot, -ENOSYS, LINUX)                       \
    X(mmap_fixed, -ENOSYS, LINUX)                              \
    X(mmap_fixed_unaligned, -ENOSYS, LINUX)                    \
    X(mmap_offset_unaligned, -ENOSYS, LINUX)                   \
    X(mmap_anon_ignores_fd, -ENOSYS, LINUX)                    \
    X(mmap_prot_none_then_rw, -ENOSYS, LINUX)                  \
    /* shared and executable mappings are refused by design */ \
    X(mmap_shared_anon, -ENOSYS, -EINVAL)                      \
    X(mmap_exec_anon, -ENOSYS, -EACCES)                        \
    /* munmap */                                               \
    X(munmap_len0, -ENOSYS, LINUX)                             \
    X(munmap_unaligned, -ENOSYS, LINUX)                        \
    X(munmap_unmapped, -ENOSYS, LINUX)                         \
    /* mprotect */                                             \
    X(mprotect_ro_rw, -ENOSYS, LINUX)                          \
    X(mprotect_len0, -ENOSYS, LINUX)                           \
    X(mprotect_unaligned, -ENOSYS, LINUX)                      \
    X(mprotect_unmapped, -ENOSYS, LINUX)                       \
    X(mprotect_unknown_prot, -ENOSYS, LINUX)                   \
    X(mprotect_sem, -ENOSYS, LINUX)                            \
    X(mprotect_above_brk, -ENOSYS, LINUX)                      \
    X(mprotect_exec, -ENOSYS, -EACCES)                         \
    /* mremap is rejected in both configurations */            \
    X(mremap_grow, -ENOSYS, -ENOSYS)                           \
    /* misc */                                                 \
    X(set_tid_address, 1, 1)                                   \
    X(getpid, -ENOSYS, -ENOSYS)                                \
    X(bogus_sysno, LINUX, LINUX)

#if defined(__x86_64__)
#define SYSCALL_CASES_ARCH(X)                            \
    X(arch_prctl_set_fs, LINUX, LINUX)                   \
    X(arch_prctl_bad_code, LINUX, LINUX)                 \
    X(arch_prctl_get_fs, LINUX, LINUX)
#else
#define SYSCALL_CASES_ARCH(X)
#endif

#define DECLARE_CASE(name, min, full) long case_##name(void);
SYSCALL_CASES(DECLARE_CASE)
SYSCALL_CASES_ARCH(DECLARE_CASE)
#undef DECLARE_CASE
