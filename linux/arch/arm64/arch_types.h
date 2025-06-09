#pragma once

#include <stdint.h>

typedef uint64_t linux_dev_t;
typedef uint64_t linux_ino_t;
typedef uint32_t linux_nlink_t;
typedef uint32_t linux_mode_t;
typedef uint32_t linux_uid_t;
typedef uint32_t linux_gid_t;
typedef uint64_t linux_off_t;
typedef uint32_t linux_blksize_t;
typedef uint64_t linux_blkcnt_t;

struct TimeSpec {
    uint64_t sec;
    uint64_t nsec;
};

struct Stat {
    linux_dev_t st_dev;
    linux_ino_t st_ino;
    linux_mode_t st_mode;
    linux_nlink_t st_nlink;
    linux_uid_t st_uid;
    linux_gid_t st_gid;
    linux_dev_t st_rdev;
    uint64_t _pad0;
    linux_off_t st_size;
    linux_blksize_t st_blksize;
    uint32_t _pad2;
    linux_blkcnt_t st_blocks;

    struct TimeSpec st_atim;
    struct TimeSpec st_mtim;
    struct TimeSpec st_ctim;
    uint32_t _unused[2];
};
