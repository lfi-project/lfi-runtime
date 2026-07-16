#pragma once

#include <stdint.h>

typedef uint64_t linux_dev_t;
typedef uint64_t linux_ino_t;
typedef uint32_t linux_nlink_t;
typedef uint32_t linux_mode_t;
typedef uint32_t linux_uid_t;
typedef uint32_t linux_gid_t;
typedef uint64_t linux_off_t;
typedef uint32_t linux_blksize_t;
typedef uint64_t linux_blkcnt_t;

struct TimeSpec {
    uint64_t sec;
    uint64_t nsec;
};

struct Stat {
    linux_dev_t st_dev;
    linux_ino_t st_ino;
    linux_mode_t st_mode;
    linux_nlink_t st_nlink;
    linux_uid_t st_uid;
    linux_gid_t st_gid;
    linux_dev_t st_rdev;
    uint64_t _pad0;
    linux_off_t st_size;
    linux_blksize_t st_blksize;
    uint32_t _pad2;
    linux_blkcnt_t st_blocks;

    struct TimeSpec st_atim;
    struct TimeSpec st_mtim;
    struct TimeSpec st_ctim;
    uint32_t _unused[2];
};

struct SigAction {
    uintptr_t handler;
    unsigned long flags;
    uintptr_t restorer;
    uint64_t mask;
};

#define SA_RESTORER 0x04000000

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

    // stack_t
    uint8_t ss__sp[8];
    uint8_t ss__flags[4];
    uint8_t pad0_[4];
    uint8_t ss_size[8];

    // glibc uses a 1024-bit sigset
    uint8_t sigmask[8];
    uint8_t __sigmask[120];

    uint8_t pad2_[8];
    // sigcontext
    uint8_t fault_address[8];
    uint8_t regs[31][8];
    uint8_t sp[8];
    uint8_t pc[8];
    uint8_t pstate[8];

    uint8_t pad3_[8];
    uint8_t __reserved[4096]; // TODO aligned 16 static assert
};

// TODO: FPSIMD context

struct SigFrame {
    struct SigInfo si;
    struct UContext uc;
};
