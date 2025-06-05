#pragma once

#if defined(__aarch64__) || defined(_M_ARM64)

#define REG_BASE x21
#define REG_ADDR x18

#elif defined(__x86_64__) || defined(_M_X64)

#define REG_BASE r14

#endif
