// GC hardware includes

#ifndef __HARDWARE_H__
#define __HARDWARE_H__

// Dolwin plugins
#include "Plugins.h"

// hardware controls and register traps
#include "HW.h"

// GC hardware set (in register addressing order, see Memory.h)
// *_OLD hardware modules also should work (not recommended) :
// replace ANY.cpp by ANY_OLD.cpp to use instead, and change
// from #include "ANY.h" to #include "ANY_OLD.h" below.
#include "EFB.h"
#include "AI.h"
#include "GDI.h"
#include "CP.h"
#include "VI.h"
#include "PI.h"
#include "MI.h"
#include "AR.h"
#include "DI.h"
#include "SI.h"
#include "EI.h"
#include "MC.h"

#endif  // __HARDWARE_H__
