#pragma once

char *
xasprintf(const char *fmt, ...);

#define LOG_TAG "lfi-core"

#ifdef LOG_DISABLE

#define LOG(engine, ...)

#else

#define LOG(engine, ...)            \
    do {                            \
        if ((*engine).opts.verbose) \
            LOG_(__VA_ARGS__);      \
    } while (0)

#endif

#if defined(__ANDROID__)

#include <android/log.h>
#include <stdio.h>

#define LOG_(fmt, ...) \
    do { \
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__); \
        fprintf(stderr, "[" LOG_TAG "] " fmt "\n", ##__VA_ARGS__); \
    } while (0)

#define ERROR(fmt, ...) \
    do { \
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__); \
        fprintf(stderr, "[" LOG_TAG "] " fmt "\n", ##__VA_ARGS__); \
    } while (0)

#else

#define LOG_(fmt, ...) \
    fprintf(stderr, "[" LOG_TAG "] " fmt "\n", ##__VA_ARGS__);

#define ERROR(fmt, ...) \
    fprintf(stderr, "[" LOG_TAG "] " fmt "\n", ##__VA_ARGS__)

#endif
