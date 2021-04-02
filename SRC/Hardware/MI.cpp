// MI - memory interface.
//
// MI is used in __OSInitMemoryProtection and by few games for debug.
#include "pch.h"

using namespace Debug;

// stubs for MI registers
static void no_write(uint32_t addr, uint32_t data) {}
static void no_read(uint32_t addr, uint32_t *reg)  { *reg = 0; }

MIControl mi;

// Load and descramble bootrom.
// This implementation makes working with Bootrom easier, since we do not need to monitor cache transactions ("bursts") from the processor.

void LoadBootrom(HWConfig* config)
{
    mi.BootromPresent = false;
    mi.bootromSize = BOOTROM_SIZE;

    // Load bootrom image

    if (wcslen(config->BootromFilename) == 0)
    {
        Report(Channel::MI, "Bootrom not loaded (not specified)\n");
        return;
    }

    auto bootrom = Util::FileLoad(config->BootromFilename);
    if (bootrom.empty())
    {
        Report(Channel::MI, "Cannot load Bootrom: %s\n", Util::WstringToString(config->BootromFilename).c_str());
        return;
    }

    mi.bootrom = new uint8_t[mi.bootromSize];

    if (bootrom.size() != mi.bootromSize)
    {
        delete [] mi.bootrom;
        mi.bootrom = nullptr;
        return;
    }

    memcpy(mi.bootrom, bootrom.data(), bootrom.size());

    // Determine size of encrypted data (find first empty cache burst line)

    const size_t strideSize = 0x20;
    uint8_t zeroStride[strideSize] = { 0 };

    size_t beginOffset = 0x100;
    size_t endOffset = mi.bootromSize - strideSize;
    size_t offset = beginOffset;

    while (offset < endOffset)
    {
        if (!memcmp(&mi.bootrom[offset], zeroStride, sizeof(zeroStride)))
        {
            break;
        }

        offset += strideSize;
    }

    if (offset == endOffset)
    {
        // Empty cacheline not found, something wrong with the image

        delete[] mi.bootrom;
        mi.bootrom = nullptr;
        return;
    }

    // Descramble

    IPLDescrambler(&mi.bootrom[beginOffset], (offset - beginOffset));
    mi.BootromPresent = true;

    // Show version

    Report(Channel::MI, "Loaded and descrambled valid Bootrom\n");
    Report(Channel::Norm, "%s\n", (char*)mi.bootrom);
}

void MIOpen(HWConfig * config)
{
    Report(Channel::MI, "Flipper memory interface\n");

    mi.ramSize = config->ramsize;
    mi.ram = new uint8_t[mi.ramSize];

    memset(mi.ram, 0, mi.ramSize);

    for(uint32_t ofs=0; ofs<=0x28; ofs+=2)
    {
        PISetTrap(16, 0x0C004000 | ofs, no_read, no_write);
    }

    LoadBootrom(config);
}

void MIClose()
{
    if (mi.ram)
    {
        delete [] mi.ram;
        mi.ram = nullptr;
    }

    if (mi.bootrom)
    {
        delete[] mi.bootrom;
        mi.bootrom = nullptr;
    }
}

uint8_t* MITranslatePhysicalAddress(uint32_t physAddr, size_t bytes)
{
    if (!mi.ram || bytes == 0)
        return nullptr;

    if (physAddr < (RAMSIZE - bytes))
    {
        return &mi.ram[physAddr];
    }

    if (physAddr >= BOOTROM_START_ADDRESS && mi.BootromPresent)
    {
        return &mi.bootrom[physAddr - BOOTROM_START_ADDRESS];
    }
    
    return nullptr;
}
