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

#include "Memory.h"
#include "Gekko.h"
#include "Interpreter.h"

#include "Interpreter/c_tables.h"
#include "Interpreter/c_integer.h"
#include "Interpreter/c_logical.h"
#include "Interpreter/c_compare.h"
#include "Interpreter/c_rotate.h"
#include "Interpreter/c_shift.h"
#include "Interpreter/c_loadstore.h"
#include "Interpreter/c_floatingpoint.h"
#include "Interpreter/c_fploadstore.h"
#include "Interpreter/c_pairedsingle.h"
#include "Interpreter/c_psloadstore.h"
#include "Interpreter/c_branch.h"
#include "Interpreter/c_condition.h"
#include "Interpreter/c_system.h"

#include "../Hardware/Hardware.h"
#include "../Debugger/Debugger.h"
