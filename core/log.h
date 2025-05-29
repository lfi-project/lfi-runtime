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

#ifdef __ANDROID__

#include <android/log.h>

#define LOG_(...) \
    ((void) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))

#else

#define LOG_(fmt, ...) \
    fprintf(stderr, "[" LOG_TAG "] " fmt "\n", ##__VA_ARGS__);

#endif
