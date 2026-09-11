#pragma once

// This header is included from hand-written assembly, so it must only contain
// preprocessor definitions.

// Offset of the sandbox's thread pointer.
#ifndef CTXREG_TP_OFFSET
#define CTXREG_TP_OFFSET 16
#endif

// Offsets of scratch slots.
#define CTXREG_SCRATCH0_OFFSET 64
#define CTXREG_SCRATCH1_OFFSET 72
