// EXI - expansion (or extension) interface.
#include "pch.h"

// IMPORTANT : all EXI transfers are completed instantly in emu (mean no DMA chain).

/* ---------------------------------------------------------------------------

    Purpose :
    ---------

    EXI is used for reading of SRAM, RTC, IPL font (from MX chip)
    EXI used for memory cards and broad band adapter.
    bootrom using EXI for AD16 writes ('AD' - what does it mean ?)

    SRAM : little piece of battery-backed data. 64 bytes or so.
    RTC  : 32-bit counter of seconds, since current millenium
    AD16 : debugging 32-bit register

    memcard should be in another module (see MC.cpp)
    broad band adapter should be in another module (see BBA.cpp)

    EXI Device Map of dedicated MX Chip :
    -------------------------------------

      --------------------------------------------
     |chan | dev |  addr  |     description       |
     |============================================|
     |  0  |  0  |        | Memory Card (Slot A)  |
     |--------------------------------------------|
     |  0  |  1  |00000000| Mask ROM              | *
     |--------------------------------------------|
     |  0  |  1  |20000000| Real-Time Clock (RTC) |
     |--------------------------------------------|
     |  0  |  1  |20000100| SRAM                  |
     |--------------------------------------------|
     |  0  |  1  |20010000| UART                  |
     |--------------------------------------------|
     |  0  |  1  |20010100| GPIO (Panasonic Q)    |
     |--------------------------------------------|
     |  1  |  0  |        | Memory Card (Slot B)  |
     |--------------------------------------------|
     |  2  |  0  |        | AD16 (trace step)     |
      --------------------------------------------

        * - not actually address, but command (addr << 6) | 0x20000000,
            used as is for emulation, though.

    Summary : EXI is sort of USB architecture.

    Tip from monk :
    ---------------

    If You wan't to support the OSReport on a lower level, You just have
    to handle the immediate writes to UART (0x20010000, when read, should
    return a value between 0 and 4 inclusive) and set the console type to
    one of the devkits (I use 0x10000006).

--------------------------------------------------------------------------- */

using namespace Debug;

// SI state (registers and other data)
EIControl exi;

// bootrom copyright message (at offset 0). PAL only, NTSC has garbage.
// can be used by apps to detect PAL/NTSC cube.
static  char palver[0x100] = "(C) 1999-2001 Nintendo.  All rights reserved."
                             "(C) 1999 ArtX Inc.  All rights reserved."
                             "PAL  Revision 1.0 ";
//                            ^^^
static  char ntscver[0x100]= "ABRACADABRA";

// forward refs on EXI transfers
void    UnknownTransfer();  // ???
void    MXTransfer();       // bootrom, RTC and SRAM
void    ADTransfer();       // AD16

// EXI transfer bindings
static void (*EXITransfer[3][3])() = {
    { MCTransfer     , MXTransfer     , UnknownTransfer }, 
    { MCTransfer     , UnknownTransfer, UnknownTransfer }, 
    { ADTransfer     , UnknownTransfer, UnknownTransfer }
};

// ---------------------------------------------------------------------------
// EXI utilities

//
// update EXI interrupt status
//

void EXIUpdateInterrupts()
{
    if( // match interrupt with its mask
        (
         (exi.regs[0].csr & (exi.regs[0].csr << 1) & EXI_CSR_INTERRUPTS) ||
         (exi.regs[1].csr & (exi.regs[1].csr << 1) & EXI_CSR_INTERRUPTS) ||
         (exi.regs[2].csr & (exi.regs[2].csr << 1) & EXI_CSR_INTERRUPTS)
        )
    )
    {
        // assert CPU interrupt
        PIAssertInt(PI_INTERRUPT_EXI);
    }
    else
    {
        // clear cpu interrupt
        PIClearInt(PI_INTERRUPT_EXI);
    }
}

//
// attach / detach device on EXI channel
//

void EXIAttach(int chan)
{
    if(exi.log) Report(Channel::EXI, "attaching device at channel %i\n", chan);

    // set attach flag
    exi.regs[chan].csr |= EXI_CSR_EXT;

    // assert attach interrupt
    exi.regs[chan].csr |= EXI_CSR_EXTINT;
    EXIUpdateInterrupts();
}

void EXIDetach(int chan)
{
    if(exi.log) Report(Channel::EXI, "detaching device at channel %i\n", chan);

    // clear attach flag
    exi.regs[chan].csr &= ~EXI_CSR_EXT;

    // assert detach interrupt
    exi.regs[chan].csr |= EXI_CSR_EXTINT;
    EXIUpdateInterrupts();
}

//
// SRAM saving/loading, using external binary file
//

static void SRAMLoad(SRAM *s)
{
    /* Load data from file in temporary buffe. */
    auto buffer = Util::FileLoad(SRAM_FILE);
    memset(s, 0, sizeof(SRAM));

    /* Copy less or equal bytes from buffer to SRAM. */
    if (!buffer.empty())
    {
        auto load_size = (buffer.size() > sizeof(SRAM) ? sizeof(SRAM) : buffer.size());
        memcpy(s, buffer.data(), load_size);
    }
    else
    {
        Report(Channel::EXI, "SRAM loading failed from %s\n\n", SRAM_FILE);
    }
}

static void SRAMSave(SRAM *s)
{
    auto ptr = (uint8_t*)s;

    auto buffer = std::vector<uint8_t>(ptr, ptr + sizeof(SRAM));
    Util::FileSave(SRAM_FILE, buffer);
}

//
// update real-time clock register
// bootrom is updating time-base registers, using RTC value
//

#define MAGIC_VALUE 0x386d4380  // seconds between 1970 and 2000

// use to get updated RTC
void RTCUpdate()
{
    exi.rtcVal = 0;// (uint32_t)time(NULL) - MAGIC_VALUE;
}

//
// load ANSI and SJIS fonts
//

static void FontLoad(uint8_t **font, uint32_t fontsize, wchar_t *filename)
{
    do
    {
        /* Allocate memory for font data. */
        *font = (uint8_t*)malloc(fontsize);
        if (*font == NULL)
        {
            break;
        }

        memset(*font, 0, fontsize); /* Clear */

        /* Load data from file in temporary buffer. */
        auto buffer = Util::FileLoad(filename);
        if (!buffer.empty())
        {
            auto load_size = (buffer.size() > fontsize ? fontsize : buffer.size());
            memcpy(*font, buffer.data(), load_size);
        }
        else
        {
            break;
        }

        return;
    } while (false);

    /* Loading failed. */
    Halt("EXI: Cannot load bootrom font: %s\n", filename);
}

static void FontUnload(uint8_t **font)
{
    if(*font)
    {
        free(*font);
        *font = 0;
    }
}

// format UART string (covert ESC-codes, to debugger color-codes)
static char * uartf(char *buf)
{
    static char str[300];
    char *ptr = str;
    size_t len = strlen(buf);
    for(int n=0; n<len; n++)
    {
        if(buf[n] == 13) buf[n] = '\n';
        *ptr++ = buf[n];
    } *ptr = 0;
    return str;
}

// ---------------------------------------------------------------------------
// basic transfers (memcard and BBA transfers are too big to put them here)

// undefined transfer
void UnknownTransfer()
{
    // dont do nothing on the exi transfer
    if(exi.log)
    {
        Report(Channel::EXI, "unknown transfer (channel:%i, device:%i)\n", exi.chan, exi.sel);
    }
}

// MX chip transfers (EXI device 0:1)
void MXTransfer()
{
    uint32_t ofs;
    bool dma = (exi.regs[0].cr & EXI_CR_DMA) ? (true) : (false);

    // read or write ?
    switch(EXI_CR_RW(exi.regs[0].cr))
    {
        case 0:                 // read
        {
            if(dma)             // dma
            {
                ofs = exi.mxaddr & 0x7fffffff;
                if(ofs == 0x20000100)
                {
                    if(exi.regs[0].len > sizeof(SRAM))
                    {
                        Report(Channel::EXI, "wrong input buffer size for SRAM read dma\n");
                        return;
                    }
                    memcpy(&mi.ram[exi.regs[0].madr & RAMMASK], &exi.sram, sizeof(SRAM));
                    return;
                }
                if((ofs >= 0x001fcf00) && (ofs < (0x001fcf00 + ANSI_SIZE)))
                {
                    if (mi.BootromPresent)
                    {
                        memcpy(
                            &mi.ram[exi.regs[0].madr & RAMMASK],
                            &mi.bootrom[ofs],
                            exi.regs[0].len
                        );
                    }
                    else
                    {
                        assert(exi.ansiFont);
                        memcpy(
                            &mi.ram[exi.regs[0].madr & RAMMASK],
                            &exi.ansiFont[ofs - 0x001fcf00],
                            exi.regs[0].len
                        );
                    }
                    if(exi.log) Report ( Channel::EXI, "ansi font copy to %08X (%i)\n",
                                         exi.regs[0].madr | 0x80000000, exi.regs[0].len );
                    return;
                }
                if((ofs >= 0x001aff00) && (ofs < (0x001aff00 + SJIS_SIZE)))
                {
                    if (mi.BootromPresent)
                    {
                        memcpy(
                            &mi.ram[exi.regs[0].madr & RAMMASK],
                            &mi.bootrom[ofs],
                            exi.regs[0].len
                        );
                    }
                    else
                    {
                        assert(exi.sjisFont);
                        memcpy(
                            &mi.ram[exi.regs[0].madr & RAMMASK],
                            &exi.sjisFont[ofs - 0x001aff00],
                            exi.regs[0].len
                        );
                    }
                    if(exi.log) Report ( Channel::EXI, "sjis font copy to %08X (%i)\n",
                                         exi.regs[0].madr | 0x80000000, exi.regs[0].len );
                    return;
                }

                // Bootrom reads

                if (ofs < mi.bootromSize && mi.BootromPresent)
                {
                    memcpy(
                        &mi.ram[exi.regs[0].madr & RAMMASK],
                        &mi.bootrom[ofs],
                        exi.regs[0].len
                    );
                    if(exi.log) Report ( Channel::EXI, "bootrom copy to %08X (%i)\n",
                                         exi.regs[0].madr | 0x80000000, exi.regs[0].len );
                    return;
                }

                if(ofs)
                {
                    if(exi.log) Report ( Channel::EXI, "unknown MX chip dma read\n");
                }
            }
            else                // immediate access
            {
                ofs = exi.mxaddr & 0x7fffffff;
                if(ofs == 0x20000000)
                {
                    RTCUpdate();
                    exi.regs[0].data = exi.rtcVal;
                    return;
                }
                else if((ofs >= 0x20000100) && (ofs < (0x20000100 + (sizeof(SRAM) << 6))))
                {
                    int len = EXI_CR_TLEN(exi.regs[0].cr);
                    uint8_t * sofs = (uint8_t *)&exi.sram + ((ofs >> 6) & 0xff) - 4;
                    uint8_t * rofs = (uint8_t *)&exi.regs[0].data;
                    switch(len)
                    {
                        case 0:         // byte
                            rofs[0] =
                            rofs[1] = 
                            rofs[2] = 0;
                            rofs[3] = sofs[0];
                            exi.mxaddr += 1 << 6;
                            break;
                        case 1:         // hword
                            rofs[0] =
                            rofs[1] = 0;
                            rofs[2] = sofs[1];
                            rofs[3] = sofs[0];
                            exi.mxaddr += 2 << 6;
                            break;
                        case 2:         // triplet
                            rofs[0] = 0;
                            rofs[1] = sofs[2];
                            rofs[2] = sofs[1];
                            rofs[3] = sofs[0];
                            exi.mxaddr += 3 << 6;
                            break;
                        case 3:         // word
                            rofs[0] = sofs[3];
                            rofs[1] = sofs[2];
                            rofs[2] = sofs[1];
                            rofs[3] = sofs[0];
                            exi.mxaddr += 4 << 6;
                            break;
                    }
                    if(exi.log) Report(Channel::EXI, "immediate read SRAM (ofs:%i, len:%i)\n", ((ofs >> 6) & 0xff) - 4, len+1);
                    return;
                }
                else if(ofs == 0x20010000)
                {
                    exi.regs[0].data = 0x03000000;
                    return;
                }
                else
                {
                    Halt("EXI: Unknown MX chip read immediate from %08X", ofs);
                }
            }
            return;
        }

        case 1:                 // write
        {
            if(dma)             // dma
            {
                Halt("EXI: unknown MX chip write dma\n");
                return;
            }
            else                // immediate access
            {
                if(exi.firstImm)
                {
                    exi.firstImm = false;
                    exi.mxaddr = exi.regs[0].data;
                    if(exi.mxaddr < 0x20000000) exi.mxaddr >>= 6;
                }
                else
                {
                    uint32_t bytes = (EXI_CR_TLEN(exi.regs[0].cr) + 1);
                    uint32_t data = _BYTESWAP_UINT32(exi.regs[0].data);

                    ofs = exi.mxaddr & 0x7fffffff;
                    if((ofs >= 0x20000100) && (ofs <= 0x20001000))
                    {
                        // SRAM immediate writes
                        uint32_t pos = (((ofs - 256) >> 6) & 0x3F);

                        if(exi.log) Report(Channel::EXI, "SRAM write immediate pos %d data %08x bytes %08x\n",
                                              pos, exi.regs[0].data, bytes );

                        memcpy(((uint8_t *)&exi.sram) + pos, &data, bytes);
                        exi.mxaddr += (bytes << 6);
                    }
                    else if((ofs >= 0x20010000) && (ofs < 0x20010100))
                    {
                        // UART I/O
                        uint8_t *buf = (uint8_t *)&data;
                        for(uint32_t n=0; n<bytes; n++)
                        {
                            exi.uart[exi.upos++] = buf[n];

                            // output UART buffer after de-select
                            if(buf[n] == 13)
                            {
                                exi.uart[exi.upos] = 0;
                                exi.upos = 0;
                                if(exi.osReport) Report(Channel::Info, "%s", uartf(exi.uart));
                            }
                        }
                    }
                    else Report(Channel::EXI, "Unknown MX chip write immediate to %08X", ofs);
                }
            }
            return;
        }

        default:
        {
            if(EXI_CR_RW(exi.regs[0].cr))
            {
                Report(Channel::EXI, "unknown EXI transfer mode for MX chip\n");
            }
        }
    }
}

// AD chip? transfer (EXI device 2:0)
void ADTransfer()
{
    // read or write ?
    switch(EXI_CR_RW(exi.regs[2].cr))
    {
        case 0:                 // read
        {
            if(exi.ad16_cmd == 0) exi.regs[2].data = 0x04120000;
            else if(exi.ad16_cmd == 0xa2000000) exi.regs[2].data = exi.ad16 << 16;
            else Report(Channel::EXI, "unknown AD16 command\n");
            return;
        }

        case 1:                 // write
        {
            if(exi.firstImm)
            {
                exi.firstImm = false;
                exi.ad16_cmd = exi.regs[2].data;
            }
            else
            {
                if(exi.ad16_cmd != 0xa0000000)
                {
                    Report(Channel::EXI, "unknown AD command (%08X)\n", exi.ad16_cmd);
                    return;
                }
                exi.ad16 = exi.regs[2].data >> 16;
                if(exi.log) Report(Channel::EXI, "AD16 set to %04X\n", exi.ad16);
            }
            return;
        }

        default:
        {
            if(EXI_CR_RW(exi.regs[2].cr))
            {
                Report(Channel::EXI, "unknown EXI transfer mode for AD16\n");
            }
        }
    }
}

// ---------------------------------------------------------------------------
// registers

//
// communication control
//

static void exi_select(int chan)
{
    // set flag
    exi.firstImm = true;

    if(exi.regs[chan].csr & EXI_CSR_CS0B)
    {
        exi.sel = 0;
        return;
    }
    if(exi.regs[chan].csr & EXI_CSR_CS1B)
    {
        exi.sel = 1;
        return;
    }
    if(exi.regs[chan].csr & EXI_CSR_CS2B)
    {
        exi.sel = 2;
        return;
    }

    // no device selected
    exi.sel = -1;
}

static void write_csr(int chan, uint32_t data)
{
    // clear interrupts 
    exi.regs[chan].csr &= ~(data & EXI_CSR_INTERRUPTS); 

    // update register and do select
    exi.regs[chan].csr = (exi.regs[chan].csr & EXI_CSR_READONLY) | (data & ~EXI_CSR_READONLY);
    exi_select(chan);
    EXIUpdateInterrupts();
}

static void exi0_read_csr(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[0].csr; }
static void exi1_read_csr(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[1].csr; }
static void exi2_read_csr(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[2].csr; }
static void exi0_write_csr(uint32_t addr, uint32_t data) { write_csr(0, data); }
static void exi1_write_csr(uint32_t addr, uint32_t data) { write_csr(1, data); }
static void exi2_write_csr(uint32_t addr, uint32_t data) { write_csr(2, data); }

//
// memory address for EXI DMA
//

static void exi0_read_madr(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[0].madr; }
static void exi1_read_madr(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[1].madr; }
static void exi2_read_madr(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[2].madr; }
static void exi0_write_madr(uint32_t addr, uint32_t data) { exi.regs[0].madr = data; }
static void exi1_write_madr(uint32_t addr, uint32_t data) { exi.regs[1].madr = data; }
static void exi2_write_madr(uint32_t addr, uint32_t data) { exi.regs[2].madr = data; }

//
// data length for DMA
//

static void exi0_read_len(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[0].len; }
static void exi1_read_len(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[1].len; }
static void exi2_read_len(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[2].len; }
static void exi0_write_len(uint32_t addr, uint32_t data) { exi.regs[0].len = data; }
static void exi1_write_len(uint32_t addr, uint32_t data) { exi.regs[1].len = data; }
static void exi2_write_len(uint32_t addr, uint32_t data) { exi.regs[2].len = data; }

//
// EXI control 
//

static void exi_write_cr(int chan, uint32_t data)
{
    EXIRegs *regs = &exi.regs[chan];
    regs->cr = data;

    if(regs->cr & EXI_CR_TSTART)
    {
        if(exi.sel == -1)
        {
            Report(Channel::EXI, "device should be selected before transfer\n");
            return;
        }

        // start transfer
        EXITransfer[exi.chan = chan][exi.sel]();

        // complete transfer
        regs->cr &= ~EXI_CR_TSTART;

        // assert transfer complete interrupt
        regs->csr |= EXI_CSR_TCINT;
        EXIUpdateInterrupts();
    }
}

static void exi0_read_cr(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[0].cr; }
static void exi1_read_cr(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[1].cr; }
static void exi2_read_cr(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[2].cr; }
static void exi0_write_cr(uint32_t addr, uint32_t data) { exi_write_cr(0, data); }
static void exi1_write_cr(uint32_t addr, uint32_t data) { exi_write_cr(1, data); }
static void exi2_write_cr(uint32_t addr, uint32_t data) { exi_write_cr(2, data); }

//
// EXI immediate data
//

static void exi0_read_data(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[0].data; }
static void exi1_read_data(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[1].data; }
static void exi2_read_data(uint32_t addr, uint32_t *reg)  { *reg = exi.regs[2].data; }
static void exi0_write_data(uint32_t addr, uint32_t data) { exi.regs[0].data = data; }
static void exi1_write_data(uint32_t addr, uint32_t data) { exi.regs[1].data = data; }
static void exi2_write_data(uint32_t addr, uint32_t data) { exi.regs[2].data = data; }

// ---------------------------------------------------------------------------
// init

void EIOpen(HWConfig * config)
{
    Report(Channel::EXI, "External devices interface bus\n");

    // clear registers
    memset(&exi, 0, sizeof(EIControl));

    // load user variables
    exi.log = config->exi_log;
    exi.osReport = config->exi_osReport;
    
    // reset devices
    exi.sel = -1;           // deselect MX device
    SRAMLoad(&exi.sram);    // load sram
    RTCUpdate();
    if (!mi.BootromPresent)
    {
        FontLoad(&exi.ansiFont, ANSI_SIZE, config->ansiFilename);
        FontLoad(&exi.sjisFont, SJIS_SIZE, config->sjisFilename);
    }

    // set traps for EXI channel 0 registers
    PISetTrap(32, EXI0_CSR , exi0_read_csr , exi0_write_csr) ;
    PISetTrap(32, EXI0_MADR, exi0_read_madr, exi0_write_madr);
    PISetTrap(32, EXI0_LEN , exi0_read_len , exi0_write_len) ;
    PISetTrap(32, EXI0_CR  , exi0_read_cr  , exi0_write_cr)  ;
    PISetTrap(32, EXI0_DATA, exi0_read_data, exi0_write_data);

    // set traps for EXI channel 1 registers
    PISetTrap(32, EXI1_CSR , exi1_read_csr , exi1_write_csr) ;
    PISetTrap(32, EXI1_MADR, exi1_read_madr, exi1_write_madr);
    PISetTrap(32, EXI1_LEN , exi1_read_len , exi1_write_len) ;
    PISetTrap(32, EXI1_CR  , exi1_read_cr  , exi1_write_cr)  ;
    PISetTrap(32, EXI1_DATA, exi1_read_data, exi1_write_data);

    // set traps for EXI channel 2 registers
    PISetTrap(32, EXI2_CSR , exi2_read_csr , exi2_write_csr) ;
    PISetTrap(32, EXI2_MADR, exi2_read_madr, exi2_write_madr);
    PISetTrap(32, EXI2_LEN , exi2_read_len , exi2_write_len) ;
    PISetTrap(32, EXI2_CR  , exi2_read_cr  , exi2_write_cr)  ;
    PISetTrap(32, EXI2_DATA, exi2_read_data, exi2_write_data);

    // open memory cards
    MCOpen(config);

    // open broad band adapter
}

void EIClose()
{
    // close memory cards
    MCClose();

    // close broad band adapter

    // unload fonts
    FontUnload(&exi.ansiFont);
    FontUnload(&exi.sjisFont);

    // sync sram
    SRAMSave(&exi.sram);
}
