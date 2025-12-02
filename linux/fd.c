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
fdget(struct FDTable *t, int fd)
{
    if (t->passthrough)
        return fd;
    if (fd < 0 || fd >= LINUX_NOFILE)
        return -1;
    LOCK_WITH_DEFER(&t->lk, lk);
    return t->fds[fd];
}

int
fdgetflags(struct FDTable *t, int fd)
{
    if (t->passthrough)
        return 0;
    if (fd < 0 || fd >= LINUX_NOFILE)
        return 0;
    LOCK_WITH_DEFER(&t->lk, lk);
    return t->flags[fd];
}

bool
fdisdir(struct FDTable *t, int fd)
{
    if (t->passthrough)
        return false;
    if (fd < 0 || fd >= LINUX_NOFILE)
        return false;
    LOCK_WITH_DEFER(&t->lk, lk);
    return t->dirs[fd] != NULL;
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
            return false;
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
        if (knewfd == -1)
            knewfd = newfd;
        if (dup2(koldfd, knewfd) < 0)
            goto err;
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
        t->fds[i] = -1;
        if (t->dirs[i]) {
            free(t->dirs[i]);
            t->dirs[i] = NULL;
        }
    }
}
