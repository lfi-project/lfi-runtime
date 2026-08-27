#pragma once

// This header is included from hand-written assembly, so it must only contain
// preprocessor definitions.

// Offset of the runtime's context pointer.
#ifndef CTXREG_CTX_OFFSET
#define CTXREG_CTX_OFFSET 8
#endif

// Offset of the sandbox's thread pointer.
#ifndef CTXREG_TP_OFFSET
#define CTXREG_TP_OFFSET 16
#endif
