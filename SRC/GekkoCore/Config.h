// Compile time macros for GekkoCore.

#pragma once

// For debugging purposes, Jitc is not yet turned on when the code is uploaded to master.

#ifndef GEKKOCORE_USE_JITC
#define GEKKOCORE_USE_JITC 0	//!< Use the recompiler during main code execution (in runtime). Debugging (step-by-step execution) is always done using the interpreter.
#endif

#ifndef GEKKOCORE_SIMPLE_MMU
#define GEKKOCORE_SIMPLE_MMU 0	//!< Use the simple MMU translation used in Dolphin OS (until games start using ARAM mapping, also not suitable for GC-Linux).
#endif
