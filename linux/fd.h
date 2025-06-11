#pragma once

#include "linux.h"
#include "proc.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

void
fdassign(struct FDTable *t, int fd, int host_fd);

int
fdget(struct FDTable *t, int fd);

void
fdclose(struct FDTable *t, int fd);

// Initialize the file descriptor table.
void
fdinit(struct LFILinuxEngine *engine, struct FDTable *t);
