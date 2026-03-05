#pragma once

#include <stdint.h>

typedef uint64_t linux_dev_t;
typedef uint64_t linux_ino_t;
typedef uint64_t linux_nlink_t;
typedef uint32_t linux_mode_t;
typedef uint32_t linux_uid_t;
typedef uint32_t linux_gid_t;
typedef uint64_t linux_off_t;
typedef uint64_t linux_blksize_t;
typedef uint64_t linux_blkcnt_t;

struct TimeSpec {
    uint64_t sec;
    uint64_t nsec;
};

struct Stat {
    linux_dev_t st_dev;
    linux_ino_t st_ino;
    linux_nlink_t st_nlink;

    linux_mode_t st_mode;
    linux_uid_t st_uid;
    linux_gid_t st_gid;
    uint32_t _pad0;
    linux_dev_t st_rdev;
    linux_off_t st_size;
    linux_blksize_t st_blksize;
    linux_blkcnt_t st_blocks;

    struct TimeSpec st_atim;
    struct TimeSpec st_mtim;
    struct TimeSpec st_ctim;
    uint64_t _unused[3];
};

struct SigAction {
    uintptr_t handler;
    unsigned long flags;
    uintptr_t restorer;
    uint64_t mask;
};

// These definitions are from jart/blink.

struct SigInfo {
    uint8_t signo[4];
    uint8_t errno_[4];
    uint8_t code[4];
    uint8_t pad1_[4];
    union {
        struct {
            union {
                struct {
                    // signals sent by kill() and sigqueue() set these
                    uint8_t pid[4];
                    uint8_t uid[4];
                };
                struct {
                    // SIGALRM sets these
                    uint8_t timerid[4];
                    uint8_t overrun[4];
                };
            };
            union {
                uint8_t value[8]; // provided by third arg of sigqueue(2)
                struct {
                    uint8_t status[4];
                    uint8_t pad2_[4];
                    uint8_t utime[8];
                    uint8_t stime[8];
                };
            };
        };
        struct {
            // SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGTRAP
            uint8_t addr[8];
            uint8_t addr_lsb[2];
            uint8_t pad3_[6];
            union {
                struct {
                    uint8_t lower[8];
                    uint8_t upper[8];
                };
                uint8_t pkey[4];
            };
        };
        struct {
            uint8_t band[8]; // SIGPOLL
            uint8_t fd[4];
        };
        struct {
            uint8_t call_addr[8];
            uint8_t syscall[4];
            uint8_t arch[4];
        };
        uint8_t payload[112];
    };
};

struct UContext {
    uint8_t uc__flags[8];
    uint8_t uc__link[8];

    // stack_t (sigaltstack)
    uint8_t ss__sp[8];
    uint8_t ss__flags[4];
    uint8_t pad0_[4];
    uint8_t ss_size[8];

    // sigcontext
    uint8_t r8[8];
    uint8_t r9[8];
    uint8_t r10[8];
    uint8_t r11[8];
    uint8_t r12[8];
    uint8_t r13[8];
    uint8_t r14[8];
    uint8_t r15[8];
    uint8_t rdi[8];
    uint8_t rsi[8];
    uint8_t rbp[8];
    uint8_t rbx[8];
    uint8_t rdx[8];
    uint8_t rax[8];
    uint8_t rcx[8];
    uint8_t rsp[8];
    uint8_t rip[8];
    uint8_t eflags[8];
    uint8_t cs[2];
    uint8_t gs[2];
    uint8_t fs[2];
    uint8_t ss[2];
    uint8_t err[8];
    uint8_t trapno[8];
    uint8_t oldmask[8];
    uint8_t cr2[8];
    uint8_t fpstate[8];
    uint8_t pad1_[64];

    uint8_t sigmask[8];
};

struct FPState {
    uint8_t cwd[2];
    uint8_t swd[2];
    uint8_t ftw[2];
    uint8_t fop[2];
    uint8_t rip[8];
    uint8_t rdp[8];
    uint8_t mxcsr[4];
    uint8_t mxcr_mask[4];
    uint8_t st[8][16];
    uint8_t xmm[16][16];
    uint8_t padding_[96];
};

#define SA_RESTORER 0x04000000

struct SigFrame {
    uint8_t ret[8];
    struct SigInfo si;
    struct UContext uc;
    struct FPState fp;
};
