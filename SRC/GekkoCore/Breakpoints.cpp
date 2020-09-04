// Support for breakpoints.

// After switching the Gekko emulation as a separate thread, the implementation of breakpoints is trivial. 
// When a breakpoint occurs, we just do Suspend of the Gekko thread. 
// And since all the other subsystems are tied to the Gekko thread (except for the UI and Debugger, it has an independent thread)
// the whole system stops with the processor.

#include "pch.h"

using namespace Debug;

namespace Gekko
{
    void GekkoCore::AddBreakpoint(uint32_t addr)
    {
        breakPointsLock.Lock();
        bool exists = false;
        for (auto it = breakPointsExecute.begin(); it != breakPointsExecute.end(); ++it)
        {
            if (*it == addr)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
        {
            Report(Channel::CPU, "Breakpoint added: 0x%08X\n", addr);
            breakPointsExecute.push_back(addr);
            jitc->Invalidate(addr, 4);
            EnableTestBreakpoints = true;
        }
        breakPointsLock.Unlock();
    }

    void GekkoCore::RemoveBreakpoint(uint32_t addr)
    {
        breakPointsLock.Lock();
        bool exists = false;
        for (auto it = breakPointsExecute.begin(); it != breakPointsExecute.end(); ++it)
        {
            if (*it == addr)
            {
                exists = true;
                break;
            }
        }
        if (exists)
        {
            Report(Channel::CPU, "Breakpoint removed: 0x%08X\n", addr);
            breakPointsExecute.remove(addr);
            jitc->Invalidate(addr, 4);
        }
        if (breakPointsExecute.size() == 0)
        {
            EnableTestBreakpoints = false;
        }
        breakPointsLock.Unlock();
    }

    void GekkoCore::AddReadBreak(uint32_t addr)
    {
        breakPointsLock.Lock();
        breakPointsRead.push_back(addr);
        breakPointsLock.Unlock();
        EnableTestReadBreakpoints = true;
    }

    void GekkoCore::AddWriteBreak(uint32_t addr)
    {
        breakPointsLock.Lock();
        breakPointsWrite.push_back(addr);
        breakPointsLock.Unlock();
        EnableTestWriteBreakpoints = true;
    }

    void GekkoCore::ClearBreakpoints()
    {
        breakPointsLock.Lock();
        breakPointsExecute.clear();
        breakPointsRead.clear();
        breakPointsWrite.clear();
        breakPointsLock.Unlock();
        EnableTestBreakpoints = false;
        EnableTestReadBreakpoints = false;
        EnableTestWriteBreakpoints = false;
    }

    bool GekkoCore::TestBreakpointForJitc(uint32_t addr)
    {
        if (!EnableTestBreakpoints)
            return false;

        if (oneShotBreakpoint != BadAddress && regs.pc == oneShotBreakpoint)
        {
            oneShotBreakpoint = BadAddress;
            return true;
        }

        bool exists = false;

        breakPointsLock.Lock();
        for (auto it = breakPointsExecute.begin(); it != breakPointsExecute.end(); ++it)
        {
            if (*it == addr)
            {
                exists = true;
                break;
            }
        }
        breakPointsLock.Unlock();

        return exists;
    }

    void GekkoCore::TestBreakpoints()
    {
        if (oneShotBreakpoint != BadAddress && regs.pc == oneShotBreakpoint)
        {
            oneShotBreakpoint = BadAddress;
            Halt("One shot breakpoint\n");
        }

        uint32_t addr = BadAddress;

        breakPointsLock.Lock();
        for (auto it = breakPointsExecute.begin(); it != breakPointsExecute.end(); ++it)
        {
            if (*it == regs.pc)
            {
                addr = *it;
                break;
            }
        }
        breakPointsLock.Unlock();

        if (addr != BadAddress)
        {
            Halt("Gekko suspended at addr: 0x%08X\n", addr);
        }
    }

    void GekkoCore::TestReadBreakpoints(uint32_t accessAddress)
    {
        uint32_t addr = BadAddress;

        breakPointsLock.Lock();
        for (auto it = breakPointsRead.begin(); it != breakPointsRead.end(); ++it)
        {
            if (*it == accessAddress)
            {
                addr = *it;
                break;
            }
        }
        breakPointsLock.Unlock();

        if (addr != BadAddress)
        {
            Halt("Gekko suspended trying to read: 0x%08X\n", addr);
        }
    }

    void GekkoCore::TestWriteBreakpoints(uint32_t accessAddress)
    {
        uint32_t addr = BadAddress;

        breakPointsLock.Lock();
        for (auto it = breakPointsWrite.begin(); it != breakPointsWrite.end(); ++it)
        {
            if (*it == accessAddress)
            {
                addr = *it;
                break;
            }
        }
        breakPointsLock.Unlock();

        if (addr != BadAddress)
        {
            Halt("Gekko suspended trying to write: 0x%08X\n", addr);
        }
    }

    void GekkoCore::AddOneShotBreakpoint(uint32_t addr)
    {
        oneShotBreakpoint = addr;
        EnableTestBreakpoints = true;
    }

    void GekkoCore::ToggleBreakpoint(uint32_t addr)
    {
        if (IsBreakpoint(addr))
            RemoveBreakpoint(addr);
        else
            AddBreakpoint(addr);
    }

    bool GekkoCore::IsBreakpoint(uint32_t addr)
    {
        bool exists = false;
        breakPointsLock.Lock();
        for (auto it = breakPointsExecute.begin(); it != breakPointsExecute.end(); ++it)
        {
            if (*it == addr)
            {
                exists = true;
                break;
            }
        }
        breakPointsLock.Unlock();
        return exists;
    }
}
