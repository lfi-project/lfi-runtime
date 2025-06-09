#pragma once

#include "proc.h"

struct File {
    int host_fd;
    int flags;
};

// Create a new FDFile from a host fd.
struct FDFile *
filefdnew(int host_fd, int flags);

// Create a new FDFile from a host path.
struct FDFile *
filenew(struct LFILinuxEngine *lengine, int host_dir, const char *name,
    int flags, int mode);
