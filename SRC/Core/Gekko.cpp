// CPU controls 
#include "pch.h"

namespace Gekko
{
    GekkoCore* Gekko;

    void GekkoCore::GekkoThreadProc(void* Parameter)
    {
        GekkoCore* core = (GekkoCore*)Parameter;

        while (true)
        {
            core->interp->ExecuteOpcode();
            //core->jitc->Execute();
        }
    }

    GekkoCore::GekkoCore()
    {
        interp = new Interpreter(this);
        assert(interp);

        jitc = new Jitc(this);
        assert(jitc);

        Debug::Hub.AddNode(GEKKO_CORE_JDI_JSON, gekko_init_handlers);

        gekkoThread = new Thread(GekkoThreadProc, true, this, "GekkoCore");
        assert(gekkoThread);

        Reset();

        msec = OneSecond() / 1000;
    }

    GekkoCore::~GekkoCore()
    {
        for (int i = 0; i < (int)GekkoWaiter::Max; i++)
        {
            if (waitQueue[i].thread)
            {
                waitQueue[i].thread->Suspend();
            }
        }

        Debug::Hub.RemoveNode(GEKKO_CORE_JDI_JSON);

        delete gekkoThread;
        delete interp;
        delete jitc;
    }

    // Reset processor
    void GekkoCore::Reset()
    {
        gekkoThread->Suspend();

        one_second = CPU_TIMER_CLOCK;
        intFlag = false;
        ops = 0;
        dispatchQueueCounter = 0;

        // TODO: Make switchable
        //EffectiveToPhysical = &GekkoCore::EffectiveToPhysicalNoMmu;

        // BAT registers are scattered across the SPR address space. This is not very convenient, we will make it convenient.

        dbatu[0] = &regs.spr[(int)SPR::DBAT0U];
        dbatu[1] = &regs.spr[(int)SPR::DBAT1U];
        dbatu[2] = &regs.spr[(int)SPR::DBAT2U];
        dbatu[3] = &regs.spr[(int)SPR::DBAT3U];

        dbatl[0] = &regs.spr[(int)SPR::DBAT0L];
        dbatl[1] = &regs.spr[(int)SPR::DBAT1L];
        dbatl[2] = &regs.spr[(int)SPR::DBAT2L];
        dbatl[3] = &regs.spr[(int)SPR::DBAT3L];

        ibatu[0] = &regs.spr[(int)SPR::IBAT0U];
        ibatu[1] = &regs.spr[(int)SPR::IBAT1U];
        ibatu[2] = &regs.spr[(int)SPR::IBAT2U];
        ibatu[3] = &regs.spr[(int)SPR::IBAT3U];

        ibatl[0] = &regs.spr[(int)SPR::IBAT0L];
        ibatl[1] = &regs.spr[(int)SPR::IBAT1L];
        ibatl[2] = &regs.spr[(int)SPR::IBAT2L];
        ibatl[3] = &regs.spr[(int)SPR::IBAT3L];

        // Registers

        memset(&regs, 0, sizeof(regs));

        // Disable translation for now
        regs.msr &= ~(MSR_DR | MSR_IR);

        regs.tb.uval = 0;
        regs.spr[22] = 0;   // DEC
        regs.spr[9] = 0;    // CTR

        gatherBuffer.Reset();

        jitc->Reset();
        segmentsExecuted = 0;

        tlb.InvalidateAll();
    }

    // Modify CPU counters
    void GekkoCore::Tick()
    {
        regs.tb.uval += CounterStep;         // timer

        uint32_t old = regs.spr[(int)SPR::DEC];
        regs.spr[(int)SPR::DEC]--;          // decrementer
        if ((old ^ regs.spr[(int)SPR::DEC]) & 0x80000000)
        {
            if (regs.msr & MSR_EE)
            {
                decreq = 1;
                DBReport2(DbgChannel::CPU, "decrementer exception (OS alarm), pc:%08X\n", regs.pc);
            }
        }

        if (waitQueueEnabled)
        {
            dispatchQueueCounter++;
            if (dispatchQueueCounter >= dispatchQueuePeriod)
            {
                dispatchQueueCounter = 0;
                DispatchWaitQueue();
            }
        }
    }

    int64_t GekkoCore::GetTicks()
    {
        return regs.tb.sval;
    }

    // 1 second of emulated CPU time.
    int64_t GekkoCore::OneSecond()
    {
        return CPU_TIMER_CLOCK;
    }

    // Swap longs (no need in assembly, used by tools)
    void GekkoCore::SwapArea(uint32_t* addr, int count)
    {
        uint32_t* until = addr + count / sizeof(uint32_t);

        while (addr != until)
        {
            *addr = _byteswap_ulong(*addr);
            addr++;
        }
    }

    // Swap shorts (no need in assembly, used by tools)
    void GekkoCore::SwapAreaHalf(uint16_t* addr, int count)
    {
        uint16_t* until = addr + count / sizeof(uint16_t);

        while (addr != until)
        {
            *addr = _byteswap_ushort(*addr);
            addr++;
        }
    }

    void GekkoCore::Step()
    {
        interp->ExecuteOpcode();
    }

    void GekkoCore::AssertInterrupt()
    {
        intFlag = true;
    }

    void GekkoCore::ClearInterrupt()
    {
        intFlag = false;
    }

    void GekkoCore::Exception(Gekko::Exception code)
    {
        interp->Exception(code);
    }

    void GekkoCore::DispatchWaitQueue()
    {
        int i;

        for (i = 0; i < (int)GekkoWaiter::Max; i++)
        {
            if (waitQueue[i].requireSuspend)
            {
                waitQueue[i].requireSuspend = false;
                if (!waitQueue[i].suspended)
                {
                    waitQueue[i].thread->Suspend();
                    waitQueue[i].suspended = true;
                }
            }
            else
            {
                if (regs.tb.uval >= waitQueue[i].tbrValue)
                {
                    waitQueue[i].tbrValue = -1;
                    if (waitQueue[i].thread)
                    {
                        waitQueue[i].thread->Resume();
                    }
                }
            }
        }
    }

    void GekkoCore::WakeMeUp(GekkoWaiter disignation, uint64_t gekkoTicks, Thread* thread)
    {
        if (!waitQueueEnabled)
            return;

        waitQueue[(int)disignation].thread = thread;

        // If the tick value is too small, we will not sleep the thread, because suspend/resume will take more resources than just sleep.
        if (gekkoTicks < msec)
        {
            waitQueue[(int)disignation].tbrValue = -1;
        }
        else
        {
            waitQueue[(int)disignation].tbrValue = (uint64_t)GetTicks() + gekkoTicks;
            waitQueue[(int)disignation].requireSuspend = true;
        }
    }

    void GekkoCore::ExecuteOpcodeDebug(uint32_t pc, uint32_t instr)
    {
        interp->ExecuteOpcodeDirect(pc, instr);
    }

    bool GekkoCore::ExecuteInterpeterFallback()
    {
        return Gekko->interp->ExecuteInterpeterFallback();
    }

    uint32_t GekkoCore::EffectiveToPhysical(uint32_t ea, MmuAccess type)
    {
        //return EffectiveToPhysicalNoMmu(ea, type);
        return EffectiveToPhysicalMmu(ea, type);
    }

}
