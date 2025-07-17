#include "sys/sys.h"

#define UTSNAME_LENGTH 65
#define LINUX_VERSION "4.5.0"

struct UTSName {
    char sysname[UTSNAME_LENGTH];
    char nodename[UTSNAME_LENGTH];
    char release[UTSNAME_LENGTH];
    char version[UTSNAME_LENGTH];
    char machine[UTSNAME_LENGTH];
};

int
sys_uname(struct LFILinuxThread *t, lfiptr bufp)
{
    struct UTSName *uts = bufhost(t, bufp, sizeof(struct UTSName), alignof(struct UTSName));
    if (!uts)
        return -LINUX_EINVAL;
    strcpy(uts->sysname, "Linux LFI");
    strcpy(uts->nodename, "");
    strcpy(uts->release, LINUX_VERSION "-lfi");
    strcpy(uts->version, "0.0.0-unknown");
#if defined(LFI_ARCH_X64)
    strcpy(uts->machine, "x86_64");
#elif defined(LFI_ARCH_ARM64)
    strcpy(uts->machine, "aarch64");
#elif defined(LFI_ARCH_RISCV64)
    strcpy(uts->machine, "riscv64");
#else
    strcpy(uts->machine, "unknown");
#endif
    return 0;
}
