// CPU controls 
#include "pch.h"

// CPU control/state block (all important data is here)
CPUControl cpu;

// memory operations (using MEM* or DB* read/write operations)
void (__fastcall *CPUReadByte)(uint32_t addr, uint32_t *reg);
void (__fastcall *CPUWriteByte)(uint32_t addr, uint32_t data);
void (__fastcall *CPUReadHalf)(uint32_t addr, uint32_t *reg);
void (__fastcall *CPUReadHalfS)(uint32_t addr, uint32_t *reg);
void (__fastcall *CPUWriteHalf)(uint32_t addr, uint32_t data);
void (__fastcall *CPUReadWord)(uint32_t addr, uint32_t *reg);
void (__fastcall *CPUWriteWord)(uint32_t addr, uint32_t data);
void (__fastcall *CPUReadDouble)(uint32_t addr, uint64_t *reg);
void (__fastcall *CPUWriteDouble)(uint32_t addr, uint64_t *data);

// run from PC (using interpreter/debugger/recompiler)
void (*CPUStart)();

namespace Gekko
{
    GekkoCore* Gekko;

    void GekkoCore::GekkoThreadProc(void* Parameter)
    {
        GekkoCore* core = (GekkoCore*)Parameter;

        while (true)
        {
            core->interp->ExecuteOpcode();
        }
    }

    GekkoCore::GekkoCore()
    {
        interp = new Interpreter(this);
        assert(interp);

        Debug::Hub.AddNode(GEKKO_CORE_JDI_JSON, gekko_init_handlers);

        gekkoThread = new Thread(GekkoThreadProc, true, this, "GekkoCore");
        assert(gekkoThread);

        Reset();
    }

    GekkoCore::~GekkoCore()
    {
        Debug::Hub.RemoveNode(GEKKO_CORE_JDI_JSON);

        delete gekkoThread;
        delete interp;
    }

    // Reset processor
    void GekkoCore::Reset()
    {
        gekkoThread->Suspend();

        cpu.one_second = CPU_TIMER_CLOCK;
        cpu.decreq = 0;
        intFlag = false;

        // set CPU memory operations to default (using MEM*);
        // debugger will override them by DB* calls after start, if need.
        CPUReadByte = MEMReadByte;
        CPUWriteByte = MEMWriteByte;
        CPUReadHalf = MEMReadHalf;
        CPUReadHalfS = MEMReadHalfS;
        CPUWriteHalf = MEMWriteHalf;
        CPUReadWord = MEMReadWord;
        CPUWriteWord = MEMWriteWord;
        CPUReadDouble = MEMReadDouble;
        CPUWriteDouble = MEMWriteDouble;

        // clear opcode counter (used for MIPS calculaion)
        cpu.ops = 0;

        // we dont care about CPU registers, because they are 
        // undefined, since any GC program is virtually loaded
        // after BOOTROM. although, we should set some system
        // registers for correct MMU emulation (see Bootrom.cpp).

        // Disable translation for now
        MSR &= ~(MSR_DR | MSR_IR);
    }

    // Modify CPU counters
    void GekkoCore::Tick()
    {
        UTBR += CounterStep;         // timer

        uint32_t old = PPC_DEC;
        PPC_DEC--;          // decrementer
        if ((old ^ PPC_DEC) & 0x80000000)
        {
            if (MSR & MSR_EE)
            {
                cpu.decreq = 1;
                DBReport2(DbgChannel::CPU, "decrementer exception (OS alarm), pc:%08X\n", PC);
            }
        }

        DispatchWaitQueue();
    }

    int64_t GekkoCore::GetTicks()
    {
        return cpu.tb.sval;
    }

    // 1 second of emulated CPU time.
    int64_t GekkoCore::OneSecond()
    {
        return CPU_TIMER_CLOCK;
    }

    uint32_t GekkoCore::EffectiveToPhysical(uint32_t ea, bool IR)
    {
        return GCEffectiveToPhysical(ea, IR);
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

    void GekkoCore::Exception(uint32_t code)
    {
        interp->Exception(code);
    }

    void GekkoCore::DispatchWaitQueue()
    {
        for (int i = 0; i < (int)GekkoWaiter::Max; i++)
        {
            if (cpu.tb.uval >= waitQueue[i].tbrValue)
            {
                waitQueue[i].tbrValue = -1;
                if (waitQueue[i].thread)
                {
                    waitQueue[i].thread->Resume();
                }
            }
        }
    }

    void GekkoCore::WakeMeUp(GekkoWaiter disignation, uint64_t gekkoTicks, Thread* thread)
    {
        waitQueue[(int)disignation].tbrValue = GetTicks() + gekkoTicks;
        waitQueue[(int)disignation].thread = thread;
        thread->Suspend();
    }

}
