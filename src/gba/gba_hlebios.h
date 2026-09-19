// The high level BIOS calls, implemented in the host (HLE).
//
// The GBA BIOS is copyrighted and cannot be shipped with the emulator, and a large part of it is
// ordinary computation rather than hardware behaviour (division, square root, arctangent, the
// decompressors). Those calls are reimplemented here from GBATEK "GBA BIOS" and the ARM Architecture Reference Manual, and
// the emulator uses them whenever the user has not supplied a real BIOS image.
//
// The names below are the official entry points; the number in the SWI comment field (bits
// 16-23 of the SWI instruction) selects one. The custom boot ROM does not call any of them.
//
// Covered: SoftReset, RegisterRamReset, Halt, Stop, IntrWait, VBlankIntrWait, Div, DivArm,
// Sqrt, ArcTan, ArcTan2, CpuSet, CpuFastSet, BgAffineSet, ObjAffineSet, the four decompressors
// (LZ77, run-length, Huffman, the two difference filters), SoundBias, the BIOS's sound driver
// (SoundDriverInit/Mode/Main/VSync/ChannelClear/VSyncOff/VSyncOn, which is the mixer M4A games
// use) and its MidiKey2Freq, MultiBoot and the two "custom" hooks the hardware leaves to the game
// (CustomHalt). The maths helpers keep the BIOS's exact results, including Div's remainder
// convention and ArcTan's table.
//
// The SWI numbers are the official ones (GBATEK "GBA BIOS Functions"): the sound driver is
// 1Ah..1Fh and 28h/29h, *not* the 28h..2Fh block a first version of this file used - 1Ah was even
// labelled "DivArm2" and divided, so a game's SoundDriverInit returned nonsense and its music
// never started.

#pragma once

#include "gba_types.h"

namespace GBA
{
	class GbaBus;

	// The SWI comment field values (the official function numbers).
	enum SwiNumber : uint32_t
	{
		SwiSoftReset = 0x00,
		SwiRegisterRamReset = 0x01,
		SwiHalt = 0x02,
		SwiStop = 0x03,
		SwiIntrWait = 0x04,
		SwiVBlankIntrWait = 0x05,
		SwiDiv = 0x06,
		SwiDivArm = 0x07,
		SwiSqrt = 0x08,
		SwiArcTan = 0x09,
		SwiArcTan2 = 0x0A,
		SwiCpuSet = 0x0B,
		SwiCpuFastSet = 0x0C,
		SwiGetBiosChecksum = 0x0D,
		SwiBgAffineSet = 0x0E,
		SwiObjAffineSet = 0x0F,
		SwiBitUnPack = 0x10,
		SwiLz77UnCompWram = 0x11,
		SwiLz77UnCompVram = 0x12,
		SwiHuffUnComp = 0x13,
		SwiRlUnCompWram = 0x14,
		SwiRlUnCompVram = 0x15,
		SwiDiff8bitUnFilterWram = 0x16,
		SwiDiff8bitUnFilterVram = 0x17,
		SwiDiff16bitUnFilter = 0x18,
		SwiSoundBias = 0x19,
		SwiSoundDriverInit = 0x1A,
		SwiSoundDriverMode = 0x1B,
		SwiSoundDriverMain = 0x1C,
		SwiSoundDriverVSync = 0x1D,
		SwiSoundChannelClear = 0x1E,
		SwiMidiKey2Freq = 0x1F,
		SwiSoundWhatever0 = 0x20,
		SwiSoundWhatever1 = 0x21,
		SwiSoundWhatever2 = 0x22,
		SwiSoundWhatever3 = 0x23,
		SwiSoundWhatever4 = 0x24,
		SwiMultiBoot = 0x25,
		SwiHardReset = 0x26,
		SwiCustomHalt = 0x27,
		SwiSoundDriverVSyncOff = 0x28,
		SwiSoundDriverVSyncOn = 0x29,
		SwiSoundGetJumpList = 0x2A,
	};

	namespace HleBios
	{
		/// <summary>Forget the per-call statistics (the tests use them).</summary>
		void Reset();

		/// <summary>
		/// Handle one SWI. The CPU has already decoded the instruction and has put the return
		/// address in LR; the handler reads and writes the CPU registers through the bus and is
		/// responsible for the return (it sets PC to LR, or leaves the CPU halted for
		/// Halt/IntrWait). The CPU does not bank the mode or the CPSR for an HLE call: the games
		/// see the service functions as ordinary calls.
		/// </summary>
		/// <returns>true when the call was handled here.</returns>
		bool Swi(GbaBus& bus, uint32_t comment);

		/// <summary>
		/// Poll the wait state IntrWait/VBlankIntrWait left behind. The bus calls this from its
		/// clock: when the requested interrupt shows up in IF the wait is over (the CPU has
		/// already been woken by the interrupt request itself).
		/// </summary>
		void Tick(GbaBus& bus);

		/// <summary>How many times a SWI number has been handled (a test and harness aid).</summary>
		uint64_t CallCount(uint32_t comment);

		/// <summary>True when a SWI number is implemented here.</summary>
		bool Implemented(uint32_t comment);
	}
}
