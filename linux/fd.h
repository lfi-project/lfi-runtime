#pragma once

#include "linux.h"
#include "proc.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

// Assign the slot for 'fd' to the file 'ff'.
void
fdassign(struct FDTable *t, int fd, struct FDFile *ff);

// Allocate a new fd in the table and return it.
int
fdalloc(struct FDTable *t);

// Return the FDFile associated with 'fd'.
struct FDFile *
fdget(struct FDTable *t, int fd);

// Release the given FDFile.
void
fdrelease(struct FDFile *f);

// Free the slot for 'fd' in the table.
bool
fdremove(struct FDTable *t, int fd);

// Return true if the table has an allocated file for 'fd'.
bool
fdhas(struct FDTable *t, int fd);

// Remove all file descriptors in the table.
void
fdclear(struct FDTable *t);

// Initialize the file descriptor table.
void
fdinit(struct LFILinuxEngine *engine, struct FDTable *t);

static inline void
fdprelease(struct FDFile **fp)
{
    fdrelease(*fp);
}

#define FD_DEFER(f) f __attribute__((cleanup(fdprelease)))
