#pragma once

#include "lfi_linux.h"
#include "log.h"

#define EXPORT __attribute__((visibility("default")))

struct LFILinuxEngine {
    struct LFIEngine *engine;
    struct LFILinuxOptions opts;
};
