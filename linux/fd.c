#include "fd.h"

#include "lock.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void
fdassign(struct FDTable *t, int fd, int host_fd)
{
    LOCK_WITH_DEFER(&t->lk, t_lk);
    assert(t->fds[fd] == -1);
    t->fds[fd] = host_fd;
}

int
fdget(struct FDTable *t, int fd)
{
    LOCK_WITH_DEFER(&t->lk, lk);
    return t->fds[fd];
}

void
fdclose(struct FDTable *t, int fd)
{
    fdassign(t, fd, -1);
}

void
fdinit(struct LFILinuxEngine *engine, struct FDTable *t)
{
    pthread_mutex_init(&t->lk, NULL);

    t->fds[0] = dup(STDIN_FILENO);
    t->fds[1] = dup(STDOUT_FILENO);
    t->fds[2] = dup(STDERR_FILENO);
}
