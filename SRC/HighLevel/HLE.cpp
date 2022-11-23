// high level initialization code
#include "pch.h"

using namespace Debug;

HLEControl hle;

// ---------------------------------------------------------------------------

void os_ignore() { Report(Channel::HLE, "High level ignore (pc: %08X, %s)\n", Core->regs.pc, SYMName(Core->regs.pc)); }
void os_ret0()   { Core->regs.gpr[3] = 0; }
void os_ret1()   { Core->regs.gpr[3] = 1; }
void os_trap()   { Core->regs.pc = Core->regs.spr[(int)Gekko::SPR::LR] - 4; Halt("High level trap (pc: %08X)!\n", Core->regs.pc); }

// HLE Ignore (you know what are you doing!)
static const char *osignore[] = {
    // video
    //"VIWaitForRetrace"          ,

    // Terminator
    "HLE_IGNORE",
    NULL
};

// HLE which return 0 as result
static const char *osret0[] = {

    // Terminator
    "HLE_RETURN0",
    NULL
};

// HLE which return 1 as result
static const char *osret1[] = {

    // Terminator
    "HLE_RETURN1",
    NULL
};

// HLE Traps (calls, which can cause unpredictable situation)
static const char *ostraps[] = {

    // Terminator
    "HLE_TRAP",
    NULL
};

// HLE Calls
static struct OSCalls
{
    char    *name;
    void    (*call)();
} oscalls[] = {

/*/

    // Interrupt handling
    { "OSDisableInterrupts"     , OSDisableInterrupts       },
    { "OSEnableInterrupts"      , OSEnableInterrupts        },
    { "OSRestoreInterrupts"     , OSRestoreInterrupts       },

    // Context API
    // its working, but we need better recognition for OSLoadContext
    // minimal set: OSSaveContext, OSLoadContext, __OSContextInit.
    { "OSSetCurrentContext"     , OSSetCurrentContext       },
    { "OSGetCurrentContext"     , OSGetCurrentContext       },
    { "OSSaveContext"           , OSSaveContext             },
    //{ "OSLoadContext"           , OSLoadContext             },
    { "OSClearContext"          , OSClearContext            },
    { "OSInitContext"           , OSInitContext             },
    { "OSLoadFPUContext"        , OSLoadFPUContext          },
    { "OSSaveFPUContext"        , OSSaveFPUContext          },
    { "OSFillFPUContext"        , OSFillFPUContext          },
    { "__OSContextInit"         , __OSContextInit           },

    // Std C
    { "memset"                  , HLE_memset                },
    { "memcpy"                  , HLE_memcpy                },

    { "cos"                     , HLE_cos                   },
    { "sin"                     , HLE_sin                   },
    { "modf"                    , HLE_modf                  },
    { "frexp"                   , HLE_frexp                 },
    { "ldexp"                   , HLE_ldexp                 },
    { "floor"                   , HLE_floor                 },
    { "ceil"                    , HLE_ceil                  },
/*/

    // Terminator
    { NULL                      , NULL                      }
};

// ---------------------------------------------------------------------------

// wrapper
void HLESetCall(const char * name, void (*call)())
{
    SYMSetHighlevel(name, call);
}

void HLEInit()
{
    JDI::Hub.AddNode(HLE_JDI_JSON, HLE::JdiReflector);
}

void HLEShutdown()
{
    JDI::Hub.RemoveNode(HLE_JDI_JSON);
}

void HLEOpen()
{
    Report(Channel::Info,
        "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n"
        "Highlevel Initialization.\n"
        "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n"
    );

    // set high level calls
    int32_t n = 0;
    while(osignore[n])
    {
        HLESetCall(osignore[n++], os_ignore);
    } n = 0;
    while(osret0[n])
    {
        HLESetCall(osret0[n++], os_ret0);
    } n = 0;
    while(osret1[n])
    {
        HLESetCall(osret1[n++], os_ret1);
    } n = 0;
    while(ostraps[n])
    {
        HLESetCall(ostraps[n++], os_trap);
    } n = 0;
    while(oscalls[n].name)
    {
        HLESetCall(oscalls[n].name, oscalls[n].call);
        n++;
    }

    // Geometry library
    //MTXOpen();
}

void HLEClose()
{
    SYMKill();
}

void HLEExecuteCallback(uint32_t entryPoint)
{
    uint32_t old = Core->regs.spr[(int)Gekko::SPR::LR];
    Core->regs.pc = entryPoint;
    Core->regs.spr[(int)Gekko::SPR::LR] = 0;
    while (Core->regs.pc) Core->Step();
    Core->regs.pc = Core->regs.spr[(int)Gekko::SPR::LR] = old;
}
