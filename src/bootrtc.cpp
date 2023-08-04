// This component deals with emulation of the external Macronix chip where Bootrom and RTC are located.
// https://www.icreversing.com/chips/rtc_p-dol_a
#include "pch.h"

using namespace Debug;

//
// SRAM saving/loading, using external binary file
//

void SRAMLoad(SRAM* s)
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

void SRAMSave(SRAM* s)
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

void FontLoad(uint8_t** font, uint32_t fontsize, wchar_t* filename)
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

void FontUnload(uint8_t** font)
{
	if (*font)
	{
		free(*font);
		*font = 0;
	}
}

// format UART string (covert ESC-codes, to debugger color-codes)
static char* uartf(char* buf)
{
	static char str[300];
	char* ptr = str;
	size_t len = strlen(buf);
	for (int n = 0; n < len; n++)
	{
		if (buf[n] == 13) buf[n] = '\n';
		*ptr++ = buf[n];
	} *ptr = 0;
	return str;
}

// MX chip transfers (EXI device 0:1)
void MXTransfer()
{
	uint32_t ofs;
	bool dma = (exi.regs[0].cr & EXI_CR_DMA) ? (true) : (false);

	// read or write ?
	switch (EXI_CR_RW(exi.regs[0].cr))
	{
		case 0:                 // read
		{
			if (dma)             // dma
			{
				ofs = exi.mxaddr & 0x7fffffff;
				if (ofs == 0x20000100)
				{
					if (exi.regs[0].len > sizeof(SRAM))
					{
						Report(Channel::EXI, "wrong input buffer size for SRAM read dma\n");
						return;
					}
					memcpy(&mi.ram[exi.regs[0].madr & RAMMASK], &exi.sram, sizeof(SRAM));
					return;
				}
				if ((ofs >= 0x001fcf00) && (ofs < (0x001fcf00 + ANSI_SIZE)))
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
					if (exi.log) Report(Channel::EXI, "ansi font copy to %08X (%i)\n",
						exi.regs[0].madr | 0x80000000, exi.regs[0].len);
					return;
				}
				if ((ofs >= 0x001aff00) && (ofs < (0x001aff00 + SJIS_SIZE)))
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
					if (exi.log) Report(Channel::EXI, "sjis font copy to %08X (%i)\n",
						exi.regs[0].madr | 0x80000000, exi.regs[0].len);
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
					if (exi.log) Report(Channel::EXI, "bootrom copy to %08X (%i)\n",
						exi.regs[0].madr | 0x80000000, exi.regs[0].len);
					return;
				}

				if (ofs)
				{
					if (exi.log) Report(Channel::EXI, "unknown MX chip dma read\n");
				}
			}
			else                // immediate access
			{
				ofs = exi.mxaddr & 0x7fffffff;
				if (ofs == 0x20000000)
				{
					RTCUpdate();
					exi.regs[0].data = exi.rtcVal;
					return;
				}
				else if ((ofs >= 0x20000100) && (ofs < (0x20000100 + (sizeof(SRAM) << 6))))
				{
					int len = EXI_CR_TLEN(exi.regs[0].cr);
					uint8_t* sofs = (uint8_t*)&exi.sram + ((ofs >> 6) & 0xff) - 4;
					uint8_t* rofs = (uint8_t*)&exi.regs[0].data;
					switch (len)
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
					if (exi.log) Report(Channel::EXI, "immediate read SRAM (ofs:%i, len:%i)\n", ((ofs >> 6) & 0xff) - 4, len + 1);
					return;
				}
				else if (ofs == 0x20010000)
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
			if (dma)             // dma
			{
				Halt("EXI: unknown MX chip write dma\n");
				return;
			}
			else                // immediate access
			{
				if (exi.firstImm)
				{
					exi.firstImm = false;
					exi.mxaddr = exi.regs[0].data;
					if (exi.mxaddr < 0x20000000) exi.mxaddr >>= 6;
				}
				else
				{
					uint32_t bytes = (EXI_CR_TLEN(exi.regs[0].cr) + 1);
					uint32_t data = _BYTESWAP_UINT32(exi.regs[0].data);

					ofs = exi.mxaddr & 0x7fffffff;
					if ((ofs >= 0x20000100) && (ofs <= 0x20001000))
					{
						// SRAM immediate writes
						uint32_t pos = (((ofs - 256) >> 6) & 0x3F);

						if (exi.log) Report(Channel::EXI, "SRAM write immediate pos %d data %08x bytes %08x\n",
							pos, exi.regs[0].data, bytes);

						memcpy(((uint8_t*)&exi.sram) + pos, &data, bytes);
						exi.mxaddr += (bytes << 6);
					}
					else if ((ofs >= 0x20010000) && (ofs < 0x20010100))
					{
						// UART I/O
						uint8_t* buf = (uint8_t*)&data;
						for (uint32_t n = 0; n < bytes; n++)
						{
							exi.uart[exi.upos++] = buf[n];

							// output UART buffer after de-select
							if (buf[n] == 13)
							{
								exi.uart[exi.upos] = 0;
								exi.upos = 0;
								if (exi.osReport) Report(Channel::Info, "%s", uartf(exi.uart));
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
			if (EXI_CR_RW(exi.regs[0].cr))
			{
				Report(Channel::EXI, "unknown EXI transfer mode for MX chip\n");
			}
		}
	}
}


// The descrambling circuit is actually inside Flipper, but we'll put it here for convenience.

// bootrom descrambler reversed by segher
// Copyright 2008 Segher Boessenkool <segher@kernel.crashing.org>
void IPLDescrambler(uint8_t* data, size_t size)
{
	uint8_t acc = 0;
	uint8_t nacc = 0;

	uint16_t t = 0x2953;
	uint16_t u = 0xd9c2;
	uint16_t v = 0x3ff1;

	uint8_t x = 1;

	for (size_t it = 0; it < size;)
	{
		int t0 = t & 1;
		int t1 = (t >> 1) & 1;
		int u0 = u & 1;
		int u1 = (u >> 1) & 1;
		int v0 = v & 1;

		x ^= t1 ^ v0;
		x ^= (u0 | u1);
		x ^= (t0 ^ u1 ^ v0) & (t0 ^ u0);

		if (t0 == u0)
		{
			v >>= 1;
			if (v0)
			{
				v ^= 0xb3d0;
			}
		}

		if (t0 == 0)
		{
			u >>= 1;
			if (u0)
			{
				u ^= 0xfb10;
			}
		}

		t >>= 1;
		if (t0)
		{
			t ^= 0xa740;
		}

		nacc++;
		acc = 2 * acc + x;
		if (nacc == 8)
		{
			data[it++] ^= acc;
			nacc = 0;
		}
	}
}


// BS and BS2 (IPL) simulation.
// The simulation of BS and BS2 is performed with the cache turned off virtually.

// This piece of code is activated if GCM disk image was run for emulation.
// TODO: Implement as a patch at address 0xfff00000 to unify HLE and regular bootrom startup

static uint32_t default_syscall[] = {    // default exception handler
	0x2c01004c,     // isync
	0xac04007c,     // sync
	0x6400004c,     // rfi
};

// load FST
static void ReadFST()
{
	#define DOL_LIMIT   (4*1024*1024)
	#define ROUND32(x)  (((uint32_t)(x)+32-1)&~(32-1))

	uint32_t     bb2[8];         // space for BB2
	uint32_t     fstAddr, fstOffs, fstSize, fstMaxSize;

	// read BB2
	DVD::Seek(DVD_BB2_OFFSET);
	DVD::Read((uint8_t*)bb2, 32);

	// rounding is not important, but present in new apploaders.
	// FST memory address is calculated, by adjusting bb[4] with "DOL LIMIT";
	// DOL limit is fixed to 4 mb, for most apploaders (in release date range
	// from AnimalCrossing to Zelda: Wind Waker).
	fstOffs = _BYTESWAP_UINT32(bb2[1]);
	fstSize = ROUND32(_BYTESWAP_UINT32(bb2[2]));
	fstMaxSize = ROUND32(_BYTESWAP_UINT32(bb2[3]));
	fstAddr = _BYTESWAP_UINT32(bb2[4]);      // Ignore this

	uint32_t ArenaHi = 0;
	Core->ReadWord(0x80000034, &ArenaHi);
	ArenaHi -= fstSize;
	Core->WriteWord(0x80000034, ArenaHi);

	// load FST into memory
	DVD::Seek(fstOffs);
	DVD::Read(&mi.ram[ArenaHi & RAMMASK], fstSize);

	// save fst configuration in lomem
	Core->WriteWord(0x80000038, ArenaHi);
	Core->WriteWord(0x8000003c, fstMaxSize);

	// adjust arenaHi (OSInit will override it anyway, but not home demos)
	// arenaLo set to 0
	//CPUWriteWord(0x80000030, 0);
	//CPUWriteWord(0x80000034, fstAddr);
}

// execute apploader (apploader base is 0x81200000)
// this is exact apploader emulation. it is safe and checked.
static void BootApploader()
{
	uint32_t     appHeader[8];           // apploader header information
	uint32_t     appSize;                // size of apploader image
	uint32_t     appEntryPoint;
	uint32_t     _prolog, _main, _epilog;
	uint32_t     offs, size, addr;       // return of apploader main

	// I use prolog/epilog terms here, but Nintendo is using 
	// something weird, like : appLoaderFunc1 (see Zelda dump - it 
	// has some compilation garbage parts from bootrom, hehe).

	Report(Channel::HLE, "booting apploader..\n");

	// set OSReport dummy
	Core->WriteWord(0x81300000, 0x4e800020 /* blr opcode */);

	DVD::Seek(DVD_APPLDR_OFFSET);                // apploader offset
	DVD::Read((uint8_t*)appHeader, 32);   // read apploader header
	Gekko::GekkoCore::SwapArea(appHeader, 32);     // and swap it

	// save apploader info
	appEntryPoint = appHeader[4];
	appSize = appHeader[5];

	// load apploader image
	DVD::Seek(0x2460);
	DVD::Read(&mi.ram[0x81200000 & RAMMASK], appSize);

	// set parameters for apploader entrypoint
	Core->regs.gpr[3] = 0x81300004;            // save apploader _prolog offset
	Core->regs.gpr[4] = 0x81300008;            // main
	Core->regs.gpr[5] = 0x8130000c;            // _epilog

	// execute entrypoint
	Core->regs.pc = appEntryPoint;
	Core->regs.spr[(int)Gekko::SPR::LR] = 0;
	while (Core->regs.pc)
	{
		Core->Step();
	}

	// get apploader interface offsets
	Core->ReadWord(0x81300004, &_prolog);
	Core->ReadWord(0x81300008, &_main);
	Core->ReadWord(0x8130000c, &_epilog);

	Report(Channel::HLE, "apploader interface : init : %08X main : %08X close : %08X\n",
		_prolog, _main, _epilog);

	// execute apploader prolog
	Core->regs.gpr[3] = 0x81300000;            // OSReport callback as parameter
	Core->regs.pc = _prolog;
	Core->regs.spr[(int)Gekko::SPR::LR] = 0;
	while (Core->regs.pc)
	{
		Core->Step();
	}

	// execute apploader main
	do
	{
		// apploader main parameters
		Core->regs.gpr[3] = 0x81300004;        // memory address
		Core->regs.gpr[4] = 0x81300008;        // size
		Core->regs.gpr[5] = 0x8130000c;        // disk offset

		Core->regs.pc = _main;
		Core->regs.spr[(int)Gekko::SPR::LR] = 0;
		while (Core->regs.pc)
		{
			Core->Step();
		}

		Core->ReadWord(0x81300004, &addr);
		Core->ReadWord(0x81300008, &size);
		Core->ReadWord(0x8130000c, &offs);

		if (size)
		{
			DVD::Seek(offs);
			DVD::Read(&mi.ram[addr & RAMMASK], size);

			Report(Channel::HLE, "apploader read : offs : %08X size : %08X addr : %08X\n",
				offs, size, addr);
		}

	} while (Core->regs.gpr[3] != 0);

	// execute apploader epilog
	Core->regs.pc = _epilog;
	Core->regs.spr[(int)Gekko::SPR::LR] = 0;
	while (Core->regs.pc)
	{
		Core->Step();
	}

	Core->regs.pc = Core->regs.gpr[3];
	Report(Channel::Norm, "\n");
}

// RTC -> TBR
static void SyncTime(bool rtc)
{
	if (!rtc)
	{
		Core->regs.tb.uval = 0;
		return;
	}

	RTCUpdate();

	Report(Channel::HLE, "updating timer value..\n");

	int32_t counterBias = (int32_t)_BYTESWAP_UINT32(exi.sram.counterBias);
	int32_t rtcValue = exi.rtcVal + counterBias;
	Report(Channel::HLE, "counter bias: %i, real-time clock: %i\n", counterBias, exi.rtcVal);

	int64_t newTime = (int64_t)rtcValue * CPU_TIMER_CLOCK;
	int64_t systemTime;
	Core->ReadDouble(0x800030d8, (uint64_t*)&systemTime);
	systemTime += newTime - Core->regs.tb.sval;
	Core->WriteDouble(0x800030d8, (uint64_t*)&systemTime);
	Core->regs.tb.sval = newTime;
	Report(Channel::HLE, "new timer: 0x%llx\n\n", Core->GetTicks());
}

void BootROM(bool dvd, bool rtc, uint32_t consoleVer)
{
	// set initial MMU state, according with BS2/Dolphin OS
	for (int sr = 0; sr < 16; sr++)
	{
		Core->regs.sr[sr] = 0x80000000;
	}
	// DBATs
	Core->regs.spr[(int)Gekko::SPR::DBAT0U] = 0x80001fff; Core->regs.spr[(int)Gekko::SPR::DBAT0L] = 0x00000002;   // 0x80000000, 256mb, Write-back cached
	Core->regs.spr[(int)Gekko::SPR::DBAT1U] = 0xc0001fff; Core->regs.spr[(int)Gekko::SPR::DBAT1L] = 0x0000002a;   // 0xC0000000, 256mb, Cache inhibited, Guarded
	Core->regs.spr[(int)Gekko::SPR::DBAT2U] = 0x00000000; Core->regs.spr[(int)Gekko::SPR::DBAT2L] = 0x00000000;   // undefined
	Core->regs.spr[(int)Gekko::SPR::DBAT3U] = 0x00000000; Core->regs.spr[(int)Gekko::SPR::DBAT3L] = 0x00000000;   // undefined
	// IBATs
	Core->regs.spr[(int)Gekko::SPR::IBAT0U] = Core->regs.spr[(int)Gekko::SPR::DBAT0U];
	Core->regs.spr[(int)Gekko::SPR::IBAT0L] = Core->regs.spr[(int)Gekko::SPR::DBAT0L];
	Core->regs.spr[(int)Gekko::SPR::IBAT1U] = Core->regs.spr[(int)Gekko::SPR::DBAT1U];
	Core->regs.spr[(int)Gekko::SPR::IBAT1L] = Core->regs.spr[(int)Gekko::SPR::DBAT1L];
	Core->regs.spr[(int)Gekko::SPR::IBAT2U] = Core->regs.spr[(int)Gekko::SPR::DBAT2U];
	Core->regs.spr[(int)Gekko::SPR::IBAT2L] = Core->regs.spr[(int)Gekko::SPR::DBAT2L];
	Core->regs.spr[(int)Gekko::SPR::IBAT3U] = Core->regs.spr[(int)Gekko::SPR::DBAT3U];
	Core->regs.spr[(int)Gekko::SPR::IBAT3L] = Core->regs.spr[(int)Gekko::SPR::DBAT3L];
	// MSR MMU bits
	Core->regs.msr |= (MSR_IR | MSR_DR);               // enable translation
	// page table
	Core->regs.spr[(int)Gekko::SPR::SDR1] = 0;

	Core->regs.msr &= ~MSR_EE;                         // disable interrupts/DEC
	Core->regs.msr |= MSR_FP;                          // enable FP

	// from gc-linux dev mailing list
	Core->regs.spr[(int)Gekko::SPR::PVR] = 0x00083214;

	// RTC -> TBR
	SyncTime(rtc);

	// modify important OS low memory variables (lomem) (BS)
	Core->WriteWord(0x8000002c, consoleVer);   // console type
	Core->WriteWord(0x80000028, RAMSIZE);      // memsize
	Core->WriteWord(0x800000f0, RAMSIZE);      // simmemsize
	Core->WriteWord(0x800000f8, CPU_BUS_CLOCK);
	Core->WriteWord(0x800000fc, CPU_CORE_CLOCK);

	// install default syscall. not important for Dolphin OS,
	// but should be installed to avoid crash on SC opcode.
	memcpy(&mi.ram[(int)Gekko::Exception::EXCEPTION_SYSTEM_CALL],
		default_syscall,
		sizeof(default_syscall));

	// set stack
	Core->regs.gpr[1] = 0x816ffffc;
	Core->regs.gpr[13] = 0x81100000;      // Fake sda1

	// simulate or boot apploader, if dvd
	if (dvd)
	{
		// read disk ID information to 0x80000000
		DVD::Seek(0);
		DVD::Read(mi.ram, 32);

		// additional PAL/NTSC selection hack for old VIConfigure()
		char* id = (char*)mi.ram;
		if (id[3] == 'P') Core->WriteWord(0x800000CC, 1);   // set to PAL
		else Core->WriteWord(0x800000CC, 0);

		BootApploader();
	}
	else
	{
		Core->WriteWord(0x80000034, Core->regs.gpr[1] - 0x10000);

		ReadFST(); // load FST, for demos
	}
}
