#pragma once

#include "arch_types.h"
#include "lfi_linux.h"
#include "log.h"

#define EXPORT __attribute__((visibility("default")))

struct LFILinuxEngine {
    struct LFIEngine *engine;
    struct LFILinuxOptions opts;

    struct FDFile *fstdin;
    struct FDFile *fstdout;
    struct FDFile *fstderr;
};

#define LINUX_EPERM                1
#define LINUX_ENOENT               2
#define LINUX_EBADF                9
#define LINUX_EAGAIN               11
#define LINUX_ENOMEM               12
#define LINUX_EACCES               13
#define LINUX_EFAULT               14
#define LINUX_ENOTDIR              20
#define LINUX_EINVAL               22
#define LINUX_EMFILE               24
#define LINUX_ENOSYS               38

#define LINUX_SEEK_SET             0
#define LINUX_SEEK_CUR             1
#define LINUX_SEEK_END             2

#define LINUX_O_RDONLY             0x0
#define LINUX_O_WRONLY             0x1
#define LINUX_O_RDWR               0x2
#define LINUX_O_CREAT              0x40
#define LINUX_O_TRUNC              0x200
#define LINUX_O_APPEND             0x400
#define LINUX_O_NONBLOCK           0x800
#define LINUX_O_DIRECTORY          0x10000
#define LINUX_O_CLOEXEC            0x80000
#define LINUX_O_PATH               0x200000

#define LINUX_AT_FDCWD             -100
#define LINUX_AT_REMOVEDIR         0x200
#define LINUX_AT_EMPTY_PATH        0x1000

#define LINUX_GRND_NONBLOCK        1
#define LINUX_GRND_RANDOM          2

#define LINUX_FUTEX_WAIT           0
#define LINUX_FUTEX_WAKE           1
#define LINUX_FUTEX_REQUEUE        3
#define LINUX_FUTEX_WAIT_BITSET    9
#define LINUX_FUTEX_PRIVATE_FLAG   128
#define LINUX_FUTEX_CLOCK_REALTIME 256

#define LINUX_CLONE_VM             0x100
#define LINUX_CLONE_FS             0x200
#define LINUX_CLONE_FILES          0x400
#define LINUX_CLONE_SIGHAND        0x800
#define LINUX_CLONE_VFORK          0x4000
#define LINUX_CLONE_THREAD         0x10000
#define LINUX_CLONE_SYSVSEM        0x40000
#define LINUX_CLONE_SETTLS         0x80000
#define LINUX_CLONE_PARENT_SETTID  0x100000
#define LINUX_CLONE_CHILD_CLEARTID 0x200000
#define LINUX_CLONE_DETACHED       0x400000
#define LINUX_CLONE_CHILD_SETTID   0x1000000
#define LINUX_CLONE_IO             0x80000000UL

#define LINUX_SIGCHLD              0x11

#define LINUX_PR_SET_NAME          15
