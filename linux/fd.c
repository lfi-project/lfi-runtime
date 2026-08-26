#include "fd.h"

#include "lock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool
fdassign(struct FDTable *t, int fd, int host_fd, char *dir, int flags)
{
    if (t->passthrough)
        return true;
    if (fd < 0 || fd >= LINUX_NOFILE)
        return false;
    LOCK_WITH_DEFER(&t->lk, t_lk);
    assert(t->fds[fd] == -1);
    t->fds[fd] = host_fd;
    t->dirs[fd] = dir;
    t->flags[fd] = flags;
    return true;
}

int
fdacquire(struct FDTable *t, int fd, int *o_flags, bool *o_isdir)
{
    if (o_flags)
        *o_flags = 0;
    if (o_isdir)
        *o_isdir = false;
    if (t->passthrough)
        return fd;
    if (fd < 0 || fd >= LINUX_NOFILE)
        return -1;
    LOCK_WITH_DEFER(&t->lk, lk);
    if (t->fds[fd] == -1)
        return -1;
    if (o_flags)
        *o_flags = t->flags[fd];
    if (o_isdir)
        *o_isdir = t->dirs[fd] != NULL;
    // Return a private duplicate rather than the raw host fd.
    return dup(t->fds[fd]);
}

void
fdrelease(struct FDTable *t, int hostfd)
{
    if (t->passthrough)
        return;
    if (hostfd >= 0)
        close(hostfd);
}

int
fddup2(struct FDTable *t, int oldfd, int newfd)
{
    if (oldfd < 0 || oldfd >= LINUX_NOFILE || newfd < -1 || newfd >= LINUX_NOFILE)
        return -LINUX_EBADF;
    LOCK_WITH_DEFER(&t->lk, lk);
    int koldfd = t->fds[oldfd];
    if (koldfd == -1)
        return -LINUX_EBADF;
    char *dir = NULL;
    if (t->dirs[oldfd]) {
        dir = malloc(FILENAME_MAX);
        if (!dir)
            return -LINUX_ENOMEM;
    }
    int knewfd;
    if (newfd == -1) {
        knewfd = dup(koldfd);
        if (knewfd == -1)
            goto err;
        newfd = knewfd;
        t->fds[newfd] = knewfd;
    } else {
        knewfd = t->fds[newfd];
        if (knewfd == -1) {
            // Slot is unoccupied: allocate a new kernel fd rather than
            // using the raw sandbox fd number, which could collide with
            // monitor-internal file descriptors.
            knewfd = dup(koldfd);
            if (knewfd == -1)
                goto err;
        } else {
            if (dup2(koldfd, knewfd) < 0)
                goto err;
        }
        t->fds[newfd] = knewfd;
        if (t->dirs[newfd]) {
            free(t->dirs[newfd]);
            t->dirs[newfd] = NULL;
        }
    }

    if (t->dirs[oldfd]) {
        assert(t->dirs[newfd] == NULL && dir != NULL);
        t->dirs[newfd] = dir;
        strncpy(t->dirs[newfd], t->dirs[oldfd], FILENAME_MAX - 1);
        t->dirs[newfd][FILENAME_MAX - 1] = 0;
    }
    t->flags[newfd] = t->flags[oldfd];
    return newfd;

err:
    if (dir)
        free(dir);
    return -LINUX_EINVAL;
}

bool
fdclose(struct FDTable *t, int fd)
{
    if (t->passthrough)
        return true;
    if (fd < 0 || fd >= LINUX_NOFILE)
        return false;
    LOCK_WITH_DEFER(&t->lk, lk);
    if (t->fds[fd] == -1)
        return false;
    close(t->fds[fd]);
    t->fds[fd] = -1;
    t->flags[fd] = 0;
    if (t->dirs[fd]) {
        free(t->dirs[fd]);
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

#ifndef SYS_MINIMAL
    t->fds[0] = dup(STDIN_FILENO);
    t->fds[1] = dup(STDOUT_FILENO);
    t->fds[2] = dup(STDERR_FILENO);
#endif

    t->passthrough = engine->opts.sys_passthrough;
}

void
fdfree(struct FDTable *t)
{
    for (size_t i = 0; i < LINUX_NOFILE; i++) {
        if (t->fds[i] == -1)
            continue;
        close(t->fds[i]);
        t->fds[i] = -1;
        if (t->dirs[i]) {
            free(t->dirs[i]);
            t->dirs[i] = NULL;
        }
    }
}
