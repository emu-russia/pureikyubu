#include "Recompiler/a_tables.h"
#include "Recompiler/a_integer.h"
#include "Recompiler/a_logical.h"
#include "Recompiler/a_compare.h"
#include "Recompiler/a_rotate.h"
#include "Recompiler/a_shift.h"
#include "Recompiler/a_loadstore.h"
#include "Recompiler/a_floatingpoint.h"
#include "Recompiler/a_fploadstore.h"
#include "Recompiler/a_pairedsingle.h"
#include "Recompiler/a_psloadstore.h"
#include "Recompiler/a_branch.h"
#include "Recompiler/a_condition.h"
#include "Recompiler/a_system.h"

#define RECBUFSIZE      1024*1024   // length of temporary recompilation buffer
#define CPU_MAX_GROUP   10000       // max size of recompiler group (in opcodes)

// recompiler core API
void    RECStart();         // we can run only
void    RECFlushRange(u32 addr=0x80000000, u32 size=RAMSIZE);
void    RECException(u32 code);

// some externals to debugger. you should implement them too, 
// if you gonna modify recompiler core.
u32     RECGroupSize(u32 start);
void*   __fastcall RECCompileGroup(u32 start);
void    __fastcall RECDefaultGroup(u32 addr);
