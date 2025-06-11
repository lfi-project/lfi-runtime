#include "sys/sys.h"

#include <errno.h>
#include <sys/ioctl.h>

#define LINUX_TIOCGWINSZ 0x5413

struct WinSize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

int
sys_ioctl(struct LFILinuxThread *t, int fd, unsigned long request,
    uintptr_t va0, uintptr_t va1, uintptr_t va2, uintptr_t va3)
{
    if (request != LINUX_TIOCGWINSZ) {
        LOG(t->proc->engine, "unknown ioctl request: %ld", request);
        return -LINUX_EINVAL;
    }

    lfiptr box_wsp = va0;
    if (!bufcheck(t, box_wsp, sizeof(struct WinSize), alignof(struct WinSize)))
        return -LINUX_EINVAL;

    struct winsize ws;
    int kfd = fdget(&t->proc->fdtable, fd);
    if (kfd == -1)
        return -LINUX_EBADF;
    int e = ioctl(kfd, TIOCGWINSZ, &ws);
    if (e != 0)
        return -errno;
    struct WinSize box_ws = (struct WinSize) {
        .ws_row = ws.ws_row,
        .ws_col = ws.ws_col,
        .ws_xpixel = ws.ws_xpixel,
        .ws_ypixel = ws.ws_ypixel,
    };
    lfi_box_copyto(t->proc->box, box_wsp, &box_ws, sizeof(box_ws));
    return 0;
}
