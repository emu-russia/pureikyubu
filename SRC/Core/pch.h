// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#pragma once

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <intrin.h>

#include "../Common/Spinlock.h"
#include "../Common/Jdi.h"

#include "Mmu.h"
#include "Gekko.h"
#include "Interpreter.h"
#include "GekkoCommands.h"
#include "GekkoDisasm.h"

#include "../Hardware/Hardware.h"
#include "../Debugger/Debugger.h"
