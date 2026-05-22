#pragma once

#include "linux.h"

#ifndef thread_local
#define thread_local _Thread_local
#endif

extern thread_local struct LFIContext *new_ctx;
