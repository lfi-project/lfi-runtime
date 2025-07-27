#include "fd.h"

#include "lock.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

bool
fdassign(struct FDTable *t, int fd, int host_fd, char *dir)
{
    if (t->passthrough)
        return true;
    if (fd >= LINUX_NOFILE)
        return false;
    LOCK_WITH_DEFER(&t->lk, t_lk);
    assert(t->fds[fd] == -1);
    t->fds[fd] = host_fd;
    t->dirs[fd] = dir;
    return true;
}

int
fdget(struct FDTable *t, int fd)
{
    if (t->passthrough)
        return fd;
    LOCK_WITH_DEFER(&t->lk, lk);
    return t->fds[fd];
}

bool
fdclose(struct FDTable *t, int fd)
{
    if (t->passthrough)
        return true;
    LOCK_WITH_DEFER(&t->lk, lk);
    if (t->fds[fd] == -1)
        return false;
    close(t->fds[fd]);
    t->fds[fd] = -1;
    if (t->dirs[fd]) {
        free((void *) t->dirs[fd]);
        t->dirs[fd] = NULL;
    }
    return true;
}

void
fdinit(struct LFILinuxEngine *engine, struct FDTable *t)
{
    pthread_mutex_init(&t->lk, NULL);

    for (size_t i = 0; i < LINUX_NOFILE; i++) {
        t->fds[i] = -1;
    }

    t->fds[0] = dup(STDIN_FILENO);
    t->fds[1] = dup(STDOUT_FILENO);
    t->fds[2] = dup(STDERR_FILENO);

    t->passthrough = engine->opts.sys_passthrough;
}

void
fdfree(struct FDTable *t)
{
    for (size_t i = 0; i < LINUX_NOFILE; i++) {
        if (t->fds[i] == -1)
            continue;
        close(t->fds[i]);
        free((char *) t->dirs[i]);
    }
}
