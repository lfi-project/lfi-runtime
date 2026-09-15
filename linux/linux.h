#pragma once

#include "lfi_linux.h"
#include "log.h"

#define EXPORT __attribute__((visibility("default")))

struct LFILinuxEngine {
    struct LFIEngine *engine;
    struct LFILinuxOptions opts;
};

#define LINUX_EPERM                1
#define LINUX_ENOENT               2
#define LINUX_ENXIO                6
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
#define LINUX_ETIMEDOUT            110

#define LINUX_GRND_NONBLOCK        1
#define LINUX_GRND_RANDOM          2

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
