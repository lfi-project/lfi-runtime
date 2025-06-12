#pragma once

#include "linux.h"
#include "proc.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

// Assign the host file descriptor to fd.
bool
fdassign(struct FDTable *t, int fd, int host_fd);

// Returns the host file descriptor associated with fd.
int
fdget(struct FDTable *t, int fd);

// Close the host file descriptor associated with fd and remove the
// slot for fd in the table.
bool
fdclose(struct FDTable *t, int fd);

// Initialize the file descriptor table.
void
fdinit(struct LFILinuxEngine *engine, struct FDTable *t);
