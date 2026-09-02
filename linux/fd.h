#pragma once

#include "linux.h"
#include "proc.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

// Assign host_fd to the lowest-numbered free sandbox descriptor and return
// that descriptor. Takes ownership of dir: it is stored in the table on
// success and freed otherwise.
int
fdalloc(struct FDTable *t, int host_fd, char *dir, int flags);

// Borrow the host file descriptor for `fd` for the duration of a single
// operation. Every successful acquire must be paired with an fdrelease.
int
fdacquire(struct FDTable *t, int fd, int *o_flags, bool *o_isdir);

// Release a host file descriptor borrowed with fdacquire.
void
fdrelease(struct FDTable *t, int hostfd);

// Scoped wrapper for fdacquire/fdrelease.
struct FDRef {
    struct FDTable *t;
    int kfd;
};

static inline void
fd_unref(struct FDRef *r)
{
    fdrelease(r->t, r->kfd);
}

#define FD_ACQUIRE(name, table, fd, o_flags, o_isdir)              \
    struct FDRef name __attribute__((cleanup(fd_unref))) = {       \
        (table),                                                   \
        fdacquire((table), (fd), (o_flags), (o_isdir)),            \
    }

// Adjust newfd so that it now points to oldfd. If newfd is -1, allocates a new
// file descriptor for newfd automatically (same behavior as dup).
int
fddup2(struct FDTable *t, int oldfd, int newfd);

// Close the host file descriptor associated with fd and remove the
// slot for fd in the table.
bool
fdclose(struct FDTable *t, int fd);

// Initialize the file descriptor table.
void
fdinit(struct LFILinuxEngine *engine, struct FDTable *t);

// Closes all FDs in the table and frees associated memory.
void
fdfree(struct FDTable *t);
