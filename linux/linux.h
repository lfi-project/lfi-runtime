#pragma once

#include "arch_types.h"
#include "lfi_linux.h"
#include "log.h"

#define EXPORT __attribute__((visibility("default")))

struct LFILinuxEngine {
    struct LFIEngine *engine;
    struct LFILinuxOptions opts;
};

struct Dirent {
    linux_ino_t d_ino;
    linux_off_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

struct SysInfo {
    unsigned long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs, pad;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned mem_unit;
    char __reserved[256];
};

typedef int linux_clockid_t;
typedef int64_t linux_time_t;

#define LINUX_EPERM                1
#define LINUX_ENOENT               2
#define LINUX_EBADF                9
#define LINUX_EAGAIN               11
#define LINUX_ENOMEM               12
#define LINUX_EACCES               13
#define LINUX_EFAULT               14
#define LINUX_ENOTDIR              20
#define LINUX_EISDIR               21
#define LINUX_EINVAL               22
#define LINUX_EMFILE               24
#define LINUX_ENOSYS               38

#define LINUX_MAP_SHARED           1
#define LINUX_MAP_PRIVATE          2
#define LINUX_MAP_FIXED            16
#define LINUX_MAP_ANONYMOUS        32
#define LINUX_MAP_DENYWRITE        2048
#define LINUX_MAP_NORESERVE        16384

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

#define LINUX_F_OK                 0
#define LINUX_R_OK                 4
#define LINUX_W_OK                 2
#define LINUX_X_OK                 1

#define LINUX_AT_NULL              0
#define LINUX_AT_IGNORE            1
#define LINUX_AT_EXECFD            2
#define LINUX_AT_PHDR              3
#define LINUX_AT_PHENT             4
#define LINUX_AT_PHNUM             5
#define LINUX_AT_PAGESZ            6
#define LINUX_AT_BASE              7
#define LINUX_AT_FLAGS             8
#define LINUX_AT_ENTRY             9
#define LINUX_AT_NOTELF            10
#define LINUX_AT_UID               11
#define LINUX_AT_EUID              12
#define LINUX_AT_GID               13
#define LINUX_AT_EGID              14
#define LINUX_AT_CLKTCK            17
#define LINUX_AT_PLATFORM          15
#define LINUX_AT_HWCAP             16
#define LINUX_AT_FPUCW             18
#define LINUX_AT_DCACHEBSIZE       19
#define LINUX_AT_ICACHEBSIZE       20
#define LINUX_AT_UCACHEBSIZE       21
#define LINUX_AT_IGNOREPPC         22
#define LINUX_AT_SECURE            23
#define LINUX_AT_BASE_PLATFORM     24
#define LINUX_AT_RANDOM            25
#define LINUX_AT_HWCAP2            26
#define LINUX_AT_EXECFN            31
#define LINUX_AT_SYSINFO           32
#define LINUX_AT_SYSINFO_EHDR      33
#define LINUX_AT_L1I_CACHESHAPE    34
#define LINUX_AT_L1D_CACHESHAPE    35
#define LINUX_AT_L2_CACHESHAPE     36
#define LINUX_AT_L3_CACHESHAPE     37
#define LINUX_AT_L1I_CACHESIZE     40
#define LINUX_AT_L1I_CACHEGEOMETRY 41
#define LINUX_AT_L1D_CACHESIZE     42
#define LINUX_AT_L1D_CACHEGEOMETRY 43
#define LINUX_AT_L2_CACHESIZE      44
#define LINUX_AT_L2_CACHEGEOMETRY  45
#define LINUX_AT_L3_CACHESIZE      46
#define LINUX_AT_L3_CACHEGEOMETRY  47
#define LINUX_AT_MINSIGSTKSZ       51
