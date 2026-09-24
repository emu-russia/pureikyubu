// The BIOS service calls, implemented in the host (HLE).
//
// Written from GBATEK "GBA BIOS Functions" and the ARM Architecture Reference Manual's division/sqrt semantics. The GBA
// BIOS is copyrighted, so instead of shipping it the emulator implements the calls that are
// ordinary computation here and executes everything else from the BIOS image the user provides
// (or from the emulator's own boot ROM, which does not use these functions at all).
//
// The SWI call convention on the GBA: the function number is the comment field of the SWI
// instruction; the arguments are in r0, r1, r2... and the results come back in r0, r1, r3. The
// CPU calls us with LR already holding the return address and *without* switching modes - the
// games only see the call and the return, which is what a BIOS service function looks like from
// the outside. Every handler therefore ends by framing the return itself.

#include "gba_hlebios.h"
#include "gba_bus.h"
#include "gba_savestate.h"

#include <cmath>

namespace GBA
{
	namespace HleBios
	{
		static uint64_t callCounts[0x100];
		static bool waiting = false;
		static uint16_t waitMask = 0;

		// The sound driver's state (its work area is the game's, so only what the HLE has to
		// remember between calls lives here - see the sound driver section below).
		static uint32_t soundArea = 0;
		static uint32_t soundMode = 0;
		static bool soundReady = false;
		static bool soundDmaOn = false;

		// How fast the driver mixes: the playback frequency the mode selected, in Hz. The mode's
		// index picks a timer 0 reload, and the frequency is the machine's clock over the period.
		// It is defined up here (and not next to the driver's own tables) because it is part of the
		// state a save state carries.
		static uint32_t soundRate = 13379;

		static uint32_t Reg(GbaBus& bus, int index) { return bus.cpu.Reg(index); }
		static void SetReg(GbaBus& bus, int index, uint32_t value) { bus.cpu.SetReg(index, value); }

		/// <summary>Return from the service call. The CPU's PC already holds the return address
		/// (the SWI decoder left it there), and the caller's LR must be left alone, so the branch
		/// reads the PC rather than r14.</summary>
		static void Return(GbaBus& bus)
		{
			bus.cpu.BranchTo(bus.cpu.CurrentPC());
		}

		void Reset()
		{
			memset(callCounts, 0, sizeof(callCounts));
			waiting = false;
			waitMask = 0;

			// The sound driver's state is the game's work area, which a reset throws away.
			soundArea = 0;
			soundMode = 0;
			soundReady = false;
			soundDmaOn = false;
		}

		void SaveState(StateWriter& writer)
		{
			writer.Fields(waiting, waitMask);
			writer.Fields(soundArea, soundMode, soundReady, soundDmaOn, soundRate);
			writer.Array(callCounts);
		}

		void LoadState(StateReader& reader)
		{
			reader.Fields(waiting, waitMask);
			reader.Fields(soundArea, soundMode, soundReady, soundDmaOn, soundRate);
			reader.Array(callCounts);
		}

		uint64_t CallCount(uint32_t comment)
		{
			return callCounts[comment & 0xFF];
		}

		bool Implemented(uint32_t comment)
		{
			switch (comment)
			{
				case SwiSoftReset:
				case SwiRegisterRamReset:
				case SwiHalt:
				case SwiStop:
				case SwiIntrWait:
				case SwiVBlankIntrWait:
				case SwiDiv:
				case SwiDivArm:
				case SwiSqrt:
				case SwiArcTan:
				case SwiArcTan2:
				case SwiCpuSet:
				case SwiCpuFastSet:
				case SwiGetBiosChecksum:
				case SwiBgAffineSet:
				case SwiObjAffineSet:
				case SwiBitUnPack:
				case SwiLz77UnCompWram:
				case SwiLz77UnCompVram:
				case SwiHuffUnComp:
				case SwiRlUnCompWram:
				case SwiRlUnCompVram:
				case SwiDiff8bitUnFilterWram:
				case SwiDiff8bitUnFilterVram:
				case SwiDiff16bitUnFilter:
				case SwiSoundBias:
				case SwiMidiKey2Freq:
				case SwiSoundDriverInit:
				case SwiSoundDriverMode:
				case SwiSoundDriverMain:
				case SwiSoundDriverVSync:
				case SwiSoundChannelClear:
				case SwiSoundDriverVSyncOff:
				case SwiSoundDriverVSyncOn:
				case SwiHardReset:
				case SwiCustomHalt:
					return true;

				default:
					return false;
			}
		}

		void Tick(GbaBus& bus)
		{
			if (waiting && (bus.irq.ReadIF() & waitMask) != 0)
			{
				// The interrupt the caller asked for arrived. The CPU was woken by the request
				// itself (HALT wakes on IE&IF regardless of IME), and the IRQ handler - the
				// BIOS's or the game's - takes it from here.
				waiting = false;
				waitMask = 0;
			}
		}

		// -----------------------------------------------------------------------------------
		// Memory helpers (the GBA's VRAM is a 16-bit bus, the work RAM accepts bytes)
		// -----------------------------------------------------------------------------------

		static void WriteMemory8(GbaBus& bus, uint32_t address, uint8_t value)
		{
			bus.Write8(address, value);
		}

		static void WriteMemory16(GbaBus& bus, uint32_t address, uint16_t value)
		{
			bus.Write16(address, value);
		}

		static uint8_t ReadMemory8(GbaBus& bus, uint32_t address)
		{
			return bus.Read8(address);
		}

		// -----------------------------------------------------------------------------------
		// SoftReset / RegisterRamReset / HardReset
		// -----------------------------------------------------------------------------------

		static void SoftResetInternal(GbaBus& bus)
		{
			// GBATEK "SoftReset": the RAM is cleared, the BIOS's own state is reset and the
			// cartridge (or the multiboot image at 0x02000000) is started.
			bus.ewram.Fill(0);

			for (uint32_t i = 0; i < IwramSize - 0x200; i++)
			{
				bus.iwram.Write8(i, 0);
			}

			bus.cpu.SwitchMode(ModeSystem);
			SetReg(bus, 13, 0x03007F00);
			SetReg(bus, 12, 0);
			bus.cpu.WriteCPSR(ModeSystem);

			// The multiboot flag lives in the reset-flag byte; without a multiboot image the
			// cartridge is started.
			bus.cpu.BranchTo(MemRom1);
		}

		static void RegisterRamReset(GbaBus& bus, uint32_t flags)
		{
			if (flags & 0x01)
			{
				bus.ewram.Fill(0);
			}

			if (flags & 0x02)
			{
				// The last 0x200 bytes hold the BIOS's own variables and are preserved.
				for (uint32_t i = 0; i < IwramSize - 0x200; i++)
				{
					bus.iwram.Write8(i, 0);
				}
			}

			if (flags & 0x04)
			{
				for (uint32_t i = 0; i < PaletteSize; i++)
				{
					bus.ppu.WritePalette(i, 0);
				}
			}

			if (flags & 0x08)
			{
				for (uint32_t i = 0; i < VramSize; i++)
				{
					bus.ppu.WriteVram(i, 0);
				}
			}

			if (flags & 0x10)
			{
				for (uint32_t i = 0; i < OamSize; i++)
				{
					bus.ppu.WriteOam(i, 0);
				}
			}

			if (flags & 0x20)
			{
				// The serial registers go back to their reset values.
				bus.sio.Write16(bus, 0x120, 0);
				bus.sio.Write16(bus, 0x122, 0);
				bus.sio.Write16(bus, 0x124, 0);
				bus.sio.Write16(bus, 0x128, 0);
				bus.sio.Write16(bus, 0x134, 0);
			}

			if (flags & 0x40)
			{
				// The sound registers, except SOUNDBIAS.
				for (uint32_t offset = 0x060; offset <= 0x0A7; offset++)
				{
					bus.apu.Write8(offset, 0);
				}
			}

			if (flags & 0x80)
			{
				// "Other I/O": everything up to 0x04000060 except DISPCNT, plus the timers, the
				// DMA channels and the interrupt registers.
				uint16_t dispcnt = bus.ppu.DispCnt();
				for (uint32_t offset = 0x000; offset < 0x060; offset += 2)
				{
					bus.ppu.Write16(bus, offset, 0, (int)bus.TotalCycles());
				}
				bus.ppu.Write16(bus, 0x000, dispcnt, (int)bus.TotalCycles());

				for (uint32_t offset = 0x0B0; offset <= 0x0DE; offset += 2)
				{
					bus.dma.Write16(bus, offset, 0);
				}

				for (uint32_t offset = 0x100; offset <= 0x10E; offset += 2)
				{
					bus.timers.Write16(bus, offset, 0);
				}

				bus.irq.WriteIE(0);
				bus.irq.WriteIF(0xFFFF);
				bus.irq.WriteIME(false);
				bus.Write16(0x04000204, 0);		// WAITCNT
			}
		}

		// -----------------------------------------------------------------------------------
		// Integer maths
		// -----------------------------------------------------------------------------------

		static void Divide(GbaBus& bus, uint32_t numerator, uint32_t denominator)
		{
			int32_t num = (int32_t)numerator;
			int32_t den = (int32_t)denominator;

			if (den == 0)
			{
				// GBATEK: dividing by zero leaves the numerator in r1 and a zero quotient.
				SetReg(bus, 0, 0);
				SetReg(bus, 1, (uint32_t)num);
				SetReg(bus, 3, 0);
				return;
			}

			int32_t quotient = num / den;
			int32_t remainder = num % den;

			SetReg(bus, 0, (uint32_t)quotient);
			SetReg(bus, 1, (uint32_t)remainder);
			SetReg(bus, 3, (uint32_t)(quotient < 0 ? -quotient : quotient));
		}

		/// <summary>The angle unit the BIOS uses: a full circle is 0x10000.</summary>
		static uint32_t AngleFromRadians(double radians)
		{
			double units = radians * 65536.0 / (2.0 * 3.14159265358979323846);
			return (uint32_t)(int32_t)lround(units);
		}

		// -----------------------------------------------------------------------------------
		// Blocks of memory
		// -----------------------------------------------------------------------------------

		static void CpuSet(GbaBus& bus, uint32_t source, uint32_t dest, uint32_t control)
		{
			bool fill = (control & 0x01000000) != 0;
			bool word = (control & 0x04000000) != 0;
			uint32_t count = control & 0x001FFFFF;

			if (count == 0)
			{
				count = 0x200000;
			}

			if (word)
			{
				for (uint32_t i = 0; i < count; i++)
				{
					uint32_t value = fill ? bus.Read32(source) : bus.Read32(source + i * 4);
					bus.Write32(dest + i * 4, value);
				}
			}
			else
			{
				for (uint32_t i = 0; i < count; i++)
				{
					uint16_t value = fill ? bus.Read16(source) : bus.Read16(source + i * 2);
					bus.Write16(dest + i * 2, value);
				}
			}
		}

		static void CpuFastSet(GbaBus& bus, uint32_t source, uint32_t dest, uint32_t control)
		{
			bool fill = (control & 0x01000000) != 0;
			uint32_t count = control & 0x001FFFFF;

			if (count == 0)
			{
				count = 0x200000;
			}

			// The BIOS rounds the length up to a multiple of 8 words.
			count = (count + 7) & ~7u;

			for (uint32_t i = 0; i < count; i++)
			{
				uint32_t value = fill ? bus.Read32(source) : bus.Read32(source + i * 4);
				bus.Write32(dest + i * 4, value);
			}
		}

		static void BgAffineSet(GbaBus& bus, uint32_t source, uint32_t dest, uint32_t count)
		{
			for (uint32_t i = 0; i < count; i++)
			{
				uint32_t src = source + i * 20;
				uint32_t dst = dest + i * 16;

				int32_t bgx = (int32_t)bus.Read32(src + 0);			// 1.19.12
				int32_t bgy = (int32_t)bus.Read32(src + 4);
				int16_t dispX = (int16_t)bus.Read16(src + 8);
				int16_t dispY = (int16_t)bus.Read16(src + 10);
				int16_t scaleX = (int16_t)bus.Read16(src + 12);		// 1.7.8
				int16_t scaleY = (int16_t)bus.Read16(src + 14);
				uint16_t angle = bus.Read16(src + 16);			// 0..0xFFFF, 0x10000 = 360 degrees

				double radians = angle * (2.0 * 3.14159265358979323846 / 65536.0);
				double cosA = cos(radians);
				double sinA = sin(radians);

				double sx = scaleX / 256.0;
				double sy = scaleY / 256.0;

				// The reference point is the screen position mapped back through the rotation.
				double dx = bgx / 4096.0 - ((dispX * sx * cosA) - (dispY * sx * sinA));
				double dy = bgy / 4096.0 - ((dispX * sy * sinA) + (dispY * sy * cosA));

				int16_t pa = (int16_t)lround(sx * cosA * 256.0);
				int16_t pb = (int16_t)lround(-sx * sinA * 256.0);
				int16_t pc = (int16_t)lround(sy * sinA * 256.0);
				int16_t pd = (int16_t)lround(sy * cosA * 256.0);

				bus.Write16(dst + 0, (uint16_t)pa);
				bus.Write16(dst + 2, (uint16_t)pb);
				bus.Write16(dst + 4, (uint16_t)pc);
				bus.Write16(dst + 6, (uint16_t)pd);
				bus.Write32(dst + 8, (uint32_t)(int32_t)lround(dx * 256.0));
				bus.Write32(dst + 12, (uint32_t)(int32_t)lround(dy * 256.0));
			}
		}

		static void ObjAffineSet(GbaBus& bus, uint32_t source, uint32_t dest, uint32_t count, uint32_t stride)
		{
			for (uint32_t i = 0; i < count; i++)
			{
				uint32_t src = source + i * 8;
				uint32_t dst = dest + i * stride;

				int16_t scaleX = (int16_t)bus.Read16(src + 0);
				int16_t scaleY = (int16_t)bus.Read16(src + 2);
				uint16_t angle = bus.Read16(src + 4);

				double radians = angle * (2.0 * 3.14159265358979323846 / 65536.0);
				double sx = scaleX / 256.0;
				double sy = scaleY / 256.0;

				int16_t pa = (int16_t)lround(sx * cos(radians) * 256.0);
				int16_t pb = (int16_t)lround(-sx * sin(radians) * 256.0);
				int16_t pc = (int16_t)lround(sy * sin(radians) * 256.0);
				int16_t pd = (int16_t)lround(sy * cos(radians) * 256.0);

				bus.Write16(dst + 0, (uint16_t)pa);
				bus.Write16(dst + 2, (uint16_t)pb);
				bus.Write16(dst + 4, (uint16_t)pc);
				bus.Write16(dst + 6, (uint16_t)pd);
			}
		}

		/// <summary>
		/// BitUnPack (GBATEK "SWI 10h"): widen every source unit to the destination width. The
		/// data offset is added to units that are not zero; its top bit, not a byte of its own, is
		/// the "add it to zero units too" flag (the official BIOS reads it as `mov r8, r11, lsr
		/// #31` and clears it with `lsl #1 / lsr #1`). The units come out of each source byte
		/// least significant first and the result is accumulated into 32bit words - a trailing
		/// partial word is dropped, because the BIOS only ever stores whole ones.
		/// </summary>
		static void BitUnPack(GbaBus& bus, uint32_t source, uint32_t dest, uint32_t info)
		{
			uint32_t sourceLength = bus.Read16(info + 0);
			uint32_t sourceWidth = bus.Read8(info + 2);
			uint32_t destWidth = bus.Read8(info + 3);
			uint32_t offsetField = bus.Read32(info + 4);
			bool zeroIsOffset = (offsetField & 0x80000000u) != 0;
			uint32_t dataOffset = offsetField & 0x7FFFFFFFu;

			const bool validSource = sourceWidth == 1 || sourceWidth == 2 || sourceWidth == 4 || sourceWidth == 8;
			const bool validDest = destWidth == 1 || destWidth == 2 || destWidth == 4 || destWidth == 8 ||
				destWidth == 16 || destWidth == 32;

			if (!validSource || !validDest)
			{
				Log(LogLevel::Warn, "BitUnPack: source/dest width %i/%i is not supported", sourceWidth,
					destWidth);
				return;
			}

			uint32_t unitMask = (sourceWidth == 8) ? 0xFF : ((1u << sourceWidth) - 1);
			uint32_t accumulator = 0;
			uint32_t bits = 0;

			for (uint32_t i = 0; i < sourceLength; i++)
			{
				uint8_t byte = bus.Read8(source + i);

				for (uint32_t shift = 0; shift < 8; shift += sourceWidth)
				{
					uint32_t value = (byte >> shift) & unitMask;

					if (value != 0 || zeroIsOffset)
						value += dataOffset;

					accumulator |= value << bits;
					bits += destWidth;

					if (bits >= 32)
					{
						bus.Write32(dest, accumulator);
						dest += 4;
						accumulator = 0;
						bits = 0;
					}
				}
			}
		}
		// -----------------------------------------------------------------------------------
		// The sound driver (SWI 1Ah..1Fh, 28h and 29h)
		//
		// This is the "M4A" driver the BIOS carries: the game's own MPlay code writes the virtual
		// channel structures of the SoundArea and calls SoundDriverMain once a frame and
		// SoundDriverVSync in the VBlank handler, and the driver mixes those channels into the two
		// direct sound FIFOs.
		//
		// The facts here were read out of the official BIOS image (its SWI entry points called with
		// a probe and the results dumped): the work area is 0xFB0 bytes and starts with the
		// identifier 68736D53h, the mixed buffer (`pcmbuf`) is two 0x630 byte halves at +0x350
		// (FIFO A, right) and +0x980 (FIFO B, left), the two FIFO DMAs are armed with SAD pointing
		// at those halves, DAD at 40000A0h/40000A4h and CNT = B600h (enable, special/FIFO timing,
		// 32bit, repeat, incrementing source - the source *must* keep running for the music to
		// stream, which is what GbaDma now does), SOUNDCNT_H is 210Eh (PSG at 50 %, both FIFOs at
		// 100 %, A to the right, B to the left, both on timer 0) and SOUNDCNT_X gets the master
		// enable bit. The playback frequency index selects the timer 0 reload from the table
		// below, which is the BIOS's own.
		//
		// The mixer itself is implemented below from the same probe work: the driver's own channel
		// array (16 entries of 30h bytes at +50h), the envelope state machine, the volume scaling
		// ((master+1) * volume >> 4, then * rv/lv >> 8), the sample stepping and the ring of
		// sub-buffers (the stride at +10h, the count at +0Bh and the down counter at +4 that
		// SoundDriverVSync moves). HleBios.TheMixerMatchesTheOfficialBios runs the same synthetic
		// channel through both mixers and compares the mixed output byte for byte.
		//
		// What is *not* modelled: the reverb (the mode's reverb bits are stored but the driver's
		// delay line at 01E20h is not), and the pitch stepping is a 16.16 accumulator of the
		// channel's frequency over the playback frequency - exact when a channel plays its wave at
		// its own rate (which the differential test pins) and musically right otherwise, but not
		// bit for bit the BIOS's own fixed point.		// -----------------------------------------------------------------------------------

		const uint32_t SoundAreaSize = 0xFB0;
		const uint32_t SoundIdent = 0x68736D53;
		const uint32_t SoundPcmA = 0x350;			// the right channel's mixed buffer
		const uint32_t SoundPcmB = 0x980;			// the left one
		const uint32_t SoundPcmHalf = 0x630;		// PCM_BF: bytes per half, and the stride

		// The driver's own channel array: 12 entries of 40h bytes each, right after the header,
		// ending exactly at `pcmbuf` (50h + 12 * 40h = 350h). GBATEK's documented SoundArea layout
		// puts `vchn` right after the header, but the real header is a table of the driver's own
		// code pointers, so the channels are further along - which the disassembly showed as
		// `add r4, #50h` before the per-channel loop. The count and the stride were both read out
		// of the official driver: GBATEK "SoundDriverMode" bits 8-11 give "1-12 channels", and
		// putting a channel at 50h + n * 30h against 50h + n * 40h and running the BIOS's own mixer
		// shows it reads the 40h one (the 16 x 30h array spans the same 300h bytes, which is how
		// the wrong stride hid: only channel 0 landed where the official driver looks).
		const uint32_t SoundChannels = 12;
		const uint32_t SoundChannelBase = 0x50;
		const uint32_t SoundChannelSize = 0x40;

		// How fast the driver mixes: the playback frequency the mode selected, in Hz (`soundRate`
		// is declared with the rest of the state at the top of this file).
		const uint16_t SoundTimerReload[13] =
		{
			// Read out of the real BIOS with SoundDriverMode(index) and a timer read that stops the
			// counter first (TMxCNT_L reads back as the *counter*, so the first measurements were
			// off by however long the call took). Index 0 is the default, 13379 Hz, like index 4;
			// the frequencies match GBATEK "SoundDriverMode" bits 16-19: 13379, 5734, 7884, 10512,
			// 13379, 15768, 18157, 21024, 26758, 31536, 36314, 40137, 42048 Hz.
			0xFB1A, 0xF492, 0xF7B0, 0xF9C4, 0xFB1A, 0xFBD8, 0xFC64,
			0xFCE2, 0xFD8D, 0xFDEC, 0xFE32, 0xFE5E, 0xFE71,
		};

		// The default mode: 8 simultaneous channels, master volume 15, frequency index 4
		// (13379 Hz), 8 bit DAC (GBATEK "SoundDriverMode").
		const uint32_t SoundDefaultMode = (8u << 8) | (15u << 12) | (4u << 16) | (9u << 20);

		static void SoundWriteDma(GbaBus& bus)
		{
			if (soundArea == 0)
				return;

			// DMA1 -> FIFO A, DMA2 -> FIFO B (GBATEK "Sound DMA (FIFO Timing Mode)"): 4 units of
			// 32 bits per request, repeat, 32bit, special timing, incrementing source.
			bus.Write32(0x040000BC, soundArea + SoundPcmA);
			bus.Write32(0x040000C0, 0x040000A0);
			bus.Write16(0x040000C6, (uint16_t)(soundDmaOn ? 0xB600 : 0x0000));

			bus.Write32(0x040000C8, soundArea + SoundPcmB);
			bus.Write32(0x040000CC, 0x040000A4);
			bus.Write16(0x040000D2, (uint16_t)(soundDmaOn ? 0xB600 : 0x0000));
		}

		static void SoundSilence(GbaBus& bus, uint32_t half)
		{
			for (uint32_t i = 0; i < SoundPcmHalf; i++)
				bus.Write8(soundArea + half + i, 0);
		}

		static void SoundApplyMode(GbaBus& bus)
		{
			if (!soundReady)
				return;

			uint32_t index = (soundMode >> 16) & 0xF;
			if (index > 12)
				index = 12;

			// SOUNDCNT_H: PSG volume 1 (50 %), FIFO A and B at 100 %, A to the right, B to the
			// left, both clocked by timer 0. SOUNDCNT_X: master enable.
			bus.Write16(0x04000082, 0x210E);
			bus.Write16(0x04000084, (uint16_t)(bus.Read16(0x04000084) | 0x0080));

			// The mode's other fields land in the work area's header, as the real driver leaves
			// them (measured): +4 the DMA count, +5 the reverb, +6 the simultaneous channels and
			// +7 the master volume.
			bus.Write8(soundArea + 4, 0);
			bus.Write8(soundArea + 5, (uint8_t)(((soundMode & 0x80) != 0) ? (soundMode & 0x7F) : 0));
			bus.Write8(soundArea + 6, (uint8_t)((soundMode >> 8) & 0x0F));
			bus.Write8(soundArea + 7, (uint8_t)((soundMode >> 12) & 0x0F));

			// The playback frequency decides the mixer's own clock: one frame is
			// `frequency / 60` output samples, which the driver rounds up to 20h samples (the
			// measured stride for the default 13379 Hz is 0E0h = 224, and 224 divides the 0630h
			// byte half into 7 sub-buffers - the count the driver keeps at +0Bh).
			soundRate = 16777216u / (uint32_t)(0x10000 - SoundTimerReload[index]);

			uint32_t stride = ((soundRate / 60) + 31) & ~31u;
			if (stride == 0)
				stride = 32;

			uint32_t count = SoundPcmHalf / stride;
			if (count == 0)
				count = 1;
			if (count > 0xFF)
				count = 0xFF;

			bus.Write32(soundArea + 0x10, stride);
			bus.Write8(soundArea + 0x0B, (uint8_t)count);

			// Timer 0 is the FIFO's byte clock: prescaler 1, enabled, reloaded with the BIOS's
			// value for this playback frequency.
			bus.Write16(0x04000100, SoundTimerReload[index]);
			bus.Write16(0x04000102, 0x0080);

			SoundWriteDma(bus);
		}

		static void SoundDriverInit(GbaBus& bus, uint32_t area)
		{
			area &= ~3u;

			if (area == 0)
			{
				Log(LogLevel::Warn, "HLE BIOS: SoundDriverInit with a null work area");
				return;
			}

			// The driver clears its whole work area and then identifies it (the identifier is what
			// a game's library checks before it uses the driver).
			for (uint32_t i = 0; i < SoundAreaSize; i++)
				bus.Write8(area + i, 0);

			bus.Write32(area, SoundIdent);

			soundArea = area;
			soundMode = SoundDefaultMode;
			soundReady = true;
			soundDmaOn = true;

			SetReg(bus, 0, SoundIdent);		// the BIOS returns the identifier in r0
			SoundSilence(bus, SoundPcmA);
			SoundSilence(bus, SoundPcmB);
			SoundApplyMode(bus);
		}

		static void SoundDriverMode(GbaBus& bus, uint32_t mode)
		{
			if (!soundReady)
				return;

			soundMode = mode;
			SoundApplyMode(bus);
		}

		static void SoundChannelClear(GbaBus& bus)
		{
			if (!soundReady)
				return;

			// "Clears all direct sound channels and stops the sound": every virtual channel is
			// stopped (its status byte zeroed) and the mixed buffers are silenced.
			for (uint32_t c = 0; c < SoundChannels; c++)
				bus.Write8(soundArea + SoundChannelBase + c * SoundChannelSize, 0);

			SoundSilence(bus, SoundPcmA);
			SoundSilence(bus, SoundPcmB);
		}

		static void SoundDriverMain(GbaBus& bus)
		{
			if (!soundReady)
				return;

			// The driver fills one sub-buffer of a ring per call: the work area names the stride
			// (one frame's worth of output samples at the mode's playback frequency), the number
			// of sub-buffers per half and which one is next (its "DmaCount", a *down* counter that
			// SoundDriverVSync moves). All of that was read out of the official driver: with
			// 13379 Hz the stride is 0E0h = 224 samples, there are 7 sub-buffers in each 0630h
			// byte half, and the sub-buffer filled is `count - DmaCount + 1`.
			uint32_t count = bus.Read8(soundArea + 0x0B);
			uint32_t stride = bus.Read32(soundArea + 0x10);
			uint32_t dmaCount = bus.Read8(soundArea + 4);

			if (dmaCount == 0)
				dmaCount = count;						// 0 stands for "the last one"

			if (count == 0 || stride == 0 || stride > SoundPcmHalf)
			{
				SoundSilence(bus, SoundPcmA);
				SoundSilence(bus, SoundPcmB);
				return;
			}

			uint32_t index = (count + 1 - dmaCount) % count;
			uint32_t right = soundArea + SoundPcmA + index * stride;
			uint32_t left = soundArea + SoundPcmB + index * stride;

			for (uint32_t i = 0; i < stride; i++)
			{
				bus.Write8(right + i, 0);
				bus.Write8(left + i, 0);
			}

			uint32_t master = (uint32_t)bus.Read8(soundArea + 7) + 1;	// the volume is 1..16
			uint32_t channels = bus.Read8(soundArea + 6);

			if (channels > SoundChannels)
				channels = SoundChannels;

			for (uint32_t c = 0; c < channels; c++)
			{
				uint32_t channel = soundArea + SoundChannelBase + c * SoundChannelSize;
				uint32_t sf = bus.Read8(channel + 0);

				// 0C7h is the mask of "this channel is doing something" the driver tests.
				if ((sf & 0xC7) == 0)
					continue;

				uint32_t volume = bus.Read8(channel + 9);

				if (sf & 0x80)
				{
					// A start: the envelope begins at zero, the sample pointer goes to the wave
					// data and the phase to 0.
					if (sf & 0x40)
					{
						// Started and stopped in the same frame, which the driver reads as off.
						bus.Write8(channel + 0, 0);
						continue;
					}

					uint32_t wave = bus.Read32(channel + 0x24);
					uint32_t loop = bus.Read32(wave + 8);
					uint32_t size = bus.Read32(wave + 12);
					uint32_t start = (uint32_t)bus.Read8(wave + 3) & 0x40;	// stat: 4000h = looping

					sf = 3;									// the attack stage
					if (start != 0)
						sf |= 0x10;							// looping

					bus.Write8(channel + 0, (uint8_t)sf);
					bus.Write32(channel + 0x18, size);
					bus.Write32(channel + 0x1C, 0);			// the phase
					bus.Write32(channel + 0x20, bus.Read32(channel + 0x20));	// fr is the game's
					bus.Write32(channel + 0x28, wave + 0x10);	// the sample pointer
					bus.Write8(channel + 9, 0);
					bus.Write8(channel + 0x0C, 0);			// the release floor

					// A sample that has already run out stops the channel without mixing.
					if (size == 0)
					{
						bus.Write8(channel + 0, 0);
						continue;
					}

					// The driver's start path runs straight into the attack, so a channel that
					// starts is already at its attack volume for this frame's samples.
					volume = bus.Read8(channel + 4);
					if (volume >= 0xFF)
					{
						volume = 0xFF;
						bus.Write8(channel + 0, (uint8_t)(sf - 1));
					}
				}
				else if (sf & 4)
				{
					// In the release: a counter runs out and the channel stops.
					uint32_t timer = bus.Read8(channel + 0x0D);
					if (timer == 0)
					{
						bus.Write8(channel + 0, 0);
						continue;
					}

					bus.Write8(channel + 0x0D, (uint8_t)(timer - 1));
				}
				else if (sf & 0x40)
				{
					// Key off: the volume decays by the release rate, and the channel stops when
					// it reaches the floor (which a game can set to shorten the release).
					volume = (volume * bus.Read8(channel + 7)) >> 8;

					uint32_t floor_ = bus.Read8(channel + 0x0C);
					if (volume > floor_)
					{
						// still audible, keep releasing
					}
					else
					{
						volume = floor_;
						if (volume == 0)
						{
							bus.Write8(channel + 0, 0);
							continue;
						}

						bus.Write8(channel + 0, (uint8_t)(sf | 4));
					}
				}
				else if ((sf & 3) == 2)
				{
					// Decay: the volume is multiplied by the decay rate until the sustain level.
					volume = (volume * bus.Read8(channel + 5)) >> 8;

					uint32_t sustain = bus.Read8(channel + 6);
					if (volume > sustain)
					{
						// still decaying
					}
					else
					{
						volume = sustain;
						if (sustain == 0)
						{
							bus.Write8(channel + 0, 0);
							continue;
						}

						bus.Write8(channel + 0, (uint8_t)(sf - 1));		// the sustain stage
					}
				}
				else if ((sf & 3) == 3)
				{
					// Attack: the volume rises by the attack rate every frame.
					volume += bus.Read8(channel + 4);
					if (volume >= 0xFF)
					{
						volume = 0xFF;
						bus.Write8(channel + 0, (uint8_t)(sf - 1));		// on to the decay
					}
				}

				bus.Write8(channel + 9, (uint8_t)volume);

				// The levels the mixer scales the samples with: the master volume (1..16), the
				// channel's volume and its two side volumes, in the driver's own fixed point.
				uint32_t level = (master * volume) >> 4;
				uint32_t rightLevel = (level * bus.Read8(channel + 2)) >> 8;
				uint32_t leftLevel = (level * bus.Read8(channel + 3)) >> 8;

				bus.Write8(channel + 0x0A, (uint8_t)rightLevel);
				bus.Write8(channel + 0x0B, (uint8_t)leftLevel);

				if (rightLevel == 0 && leftLevel == 0)
					continue;

				// Mix the channel's samples into the two halves. The phase is a 16.16 accumulator
				// that advances by the channel's frequency over the mode's playback frequency, so
				// a channel playing its wave data at its own rate steps one sample per output
				// sample (which is what the official driver does with fr = the playback rate).
				uint32_t wave = bus.Read32(channel + 0x24);
				uint32_t data = wave + 0x10;
				uint32_t size = bus.Read32(wave + 12);
				uint32_t loop = (bus.Read8(wave + 3) & 0x40) ? bus.Read32(wave + 8) : size;
				uint32_t frequency = bus.Read32(channel + 0x20);
				uint64_t phase = bus.Read32(channel + 0x1C);
				uint64_t step = ((uint64_t)frequency << 16) / (soundRate ? soundRate : 1);

				for (uint32_t i = 0; i < stride; i++)
				{
					uint32_t position = (uint32_t)(phase >> 16);

					while (position >= size)
					{
						// The wave data has run out: a looping sample goes back to its loop point,
						// a one shot stops the channel (its last sample stays latched).
						if (loop < size)
						{
							phase -= (uint64_t)(size - loop) << 16;
							position = (uint32_t)(phase >> 16);
						}
						else
						{
							bus.Write8(channel + 0, 0);
							position = size - 1;
							phase = (uint64_t)position << 16;
							break;
						}
					}

					int sample = (int)(int8_t)bus.Read8(data + position);

					if (rightLevel != 0)
					{
						int mixed = (int)bus.Read8(right + i) + ((sample * (int)rightLevel) >> 8);
						if (mixed > 127) mixed = 127;
						if (mixed < -128) mixed = -128;
						bus.Write8(right + i, (uint8_t)(int8_t)mixed);
					}

					if (leftLevel != 0)
					{
						int mixed = (int)bus.Read8(left + i) + ((sample * (int)leftLevel) >> 8);
						if (mixed > 127) mixed = 127;
						if (mixed < -128) mixed = -128;
						bus.Write8(left + i, (uint8_t)(int8_t)mixed);
					}

					phase += step;

					if ((bus.Read8(channel + 0) & 0xC7) == 0)
						break;								// the channel stopped mid-buffer
				}

				bus.Write32(channel + 0x1C, (uint32_t)phase);
			}
		}

		static void SoundDriverVSync(GbaBus& bus)
		{
			// "An extremely short system call that resets the sound DMA" (GBATEK). It also moves
			// the driver's buffer counter one step down (0 wrapping to the number of sub-buffers),
			// which is what decides the sub-buffer the next SoundDriverMain fills - measured
			// against the official driver, whose counter went 0 -> 7 -> 6 -> 5 with 7 sub-buffers.
			if (soundReady)
			{
				uint32_t count = bus.Read8(soundArea + 0x0B);
				uint32_t dmaCount = bus.Read8(soundArea + 4);

				if (count != 0)
				{
					dmaCount = (dmaCount == 0) ? count : dmaCount - 1;
					bus.Write8(soundArea + 4, (uint8_t)dmaCount);
				}
			}

			if (soundReady && soundDmaOn)
				SoundWriteDma(bus);
		}

		static void SoundDriverVSyncOff(GbaBus& bus)
		{
			if (!soundReady)
				return;

			// Stop the sound DMA (the two channel control registers) but leave the mixer's state
			// alone, so VSyncOn can start it again.
			soundDmaOn = false;
			bus.Write16(0x040000C6, 0x0000);
			bus.Write16(0x040000D2, 0x0000);
		}

		static void SoundDriverVSyncOn(GbaBus& bus)
		{
			if (!soundReady)
				return;

			soundDmaOn = true;
			SoundWriteDma(bus);
		}

		/// <summary>
		/// MidiKey2Freq(wa, mk, fp): the frequency a virtual channel has to be given to play the
		/// wave data `wa` at MIDI key `mk` with the fine adjustment `fp` (1/256 of a halftone).
		///
		/// GBATEK 1Fh: the WaveData's own `freq` is "sampling rate * 2^((180 - original key)/12)",
		/// so the value for a key is that frequency scaled by 2^((mk - 180)/12) - one octave per
		/// twelve keys, and the sample plays at its own rate at key 180. The official BIOS computes
		/// it in fixed point: `2^(n/12)` for the twelve semitones comes from the table below, in
		/// 16.16, the octave is a shift and the product is truncated. The table is the BIOS's own
		/// (read out of it with a probe: its entries are the truncated powers, e.g. 69376 for
		/// 2^(1/12) where the exact value is 69433), which is what makes the results agree exactly
		/// for a whole key and to within a unit with a fine adjustment.
		/// </summary>
		static uint32_t MidiKey2FreqValue(GbaBus& bus, uint32_t waveData, uint32_t key, uint32_t fine)
		{
			// 2^(n/12) in 16.16, n = 0..11 (the BIOS's semitone table).
			static const uint32_t Semitones[12] =
			{
				65536, 69376, 73472, 77824, 82560, 87424, 92672, 98176, 103936, 110208, 116736, 123648,
			};

			uint32_t frequency = bus.Read32(waveData + 4);
			int relative = (int)(key & 0x7F) - 180;			// the reference key is 180
			int semitone = relative % 12;
			int octave = relative / 12;

			if (semitone < 0)
			{
				// C++ truncates towards zero, the note has to be split into a non-negative
				// semitone and the octave it belongs to.
				semitone += 12;
				octave -= 1;
			}

			// frequency * 2^(semitone/12) * 2^octave, in the same fixed point the BIOS uses.
			uint64_t scaled = (uint64_t)frequency * Semitones[semitone];
			int shift = 16 - octave;

			if (shift >= 0)
				scaled >>= shift;
			else
				scaled <<= -shift;

			// The fine value is a fraction of a halftone: the BIOS scales by roughly
			// 2^(fp/256/12), which its own slope puts at 3896/65536 per unit.
			scaled = (scaled * (uint64_t)(65536 + ((fine & 0xFF) * 3896) / 256)) >> 16;

			if (scaled > 0xFFFFFFFFu)
				return 0xFFFFFFFFu;

			return (uint32_t)scaled;
		}

		// -----------------------------------------------------------------------------------
		// The decompressors
		// -----------------------------------------------------------------------------------

		/// <summary>
		/// A decompressor's destination as the BIOS writes it. The Wram variants write single
		/// bytes; the Vram ones write 16bit units, because VRAM has no byte writes. The Vram form
		/// keeps one byte pending until its pair is complete, exactly like the halfword
		/// accumulator the BIOS uses - so a byte that has been produced but not yet paired is not
		/// in memory yet, and an odd final byte is never written at all. That is not cosmetic: a
		/// back reference that reaches the pending byte reads the old memory instead (GBATEK's
		/// caution that the LZ77 Vram function works with disp 1..FFFh but not 0).
		/// </summary>
		class DecompressedOutput
		{
		public:
			DecompressedOutput(GbaBus& bus, uint32_t address, bool wide)
				: bus(bus), address(address), wide(wide) {}

			void Put(uint8_t value)
			{
				if (!wide)
				{
					bus.Write8(address++, value);
					return;
				}

				if (!pending)
				{
					low = value;
					pending = true;
					address++;
					return;
				}

				bus.Write16(address & ~1u, (uint16_t)(low | (value << 8)));
				address++;
				pending = false;
			}

			/// <summary>Where the next produced byte lands - a back reference's base address.</summary>
			uint32_t Position() const { return address; }

		private:
			GbaBus& bus;
			uint32_t address;
			bool wide;
			bool pending = false;
			uint8_t low = 0;
		};

		/// <summary>
		/// The GBA's LZ77 variant: a flag byte (MSB first) and eight blocks, each block either a
		/// literal byte or "copy N+3 bytes from dest-disp-1" (GBATEK "LZ77UnComp").
		///
		/// The byte count in the header decides where the BIOS stops reading blocks; it is not a
		/// limit on how much a block writes, so it copies the whole of the block that crosses the
		/// end. (Found with the official BIOS: a stream whose last block reaches one byte past its
		/// size has that byte written.)
		/// </summary>
		static void Lz77UnComp(GbaBus& bus, uint32_t source, uint32_t dest, bool vram)
		{
			uint32_t header = (bus.Read8(source + 1) | (bus.Read8(source + 2) << 8) | (bus.Read8(source + 3) << 16));
			uint32_t src = source + 4;
			int32_t remaining = (int32_t)header;
			DecompressedOutput out(bus, dest, vram);

			while (remaining > 0)
			{
				uint8_t flags = bus.Read8(src++);

				for (int bit = 7; bit >= 0 && remaining > 0; bit--)
				{
					if (flags & (1u << bit))
					{
						uint8_t b0 = bus.Read8(src++);
						uint8_t b1 = bus.Read8(src++);
						uint32_t length = (b0 >> 4) + 3;
						uint32_t offset = (((b0 & 0xF) << 8) | b1) + 1;

						remaining -= (int32_t)length;

						for (uint32_t i = 0; i < length; i++)
							out.Put(bus.Read8(out.Position() - offset));
					}
					else
					{
						out.Put(bus.Read8(src++));
						remaining--;
					}
				}
			}
		}

		/// <summary>
		/// The GBA's Huffman decompressor (GBATEK "SWI 13h - HuffUnComp"). The stream starts with a
		/// 32bit header - bits 0-3 the size of one data unit in bits (4 or 8; a 4 bit stream packs
		/// its symbols into bytes, low nibble first), bits 4-7 the compression type (2), bits 8-31
		/// the size of the decompressed data in bytes - then an 8bit tree size ((size of the tree
		/// table / 2) - 1), the tree table itself and the bitstream.
		///
		/// A tree node is 8 bits: bits 0-5 the offset to its children, bit 6 "the child taken when
		/// the bit is 1 is data", bit 7 "the child taken when the bit is 0 is data". The children
		/// are a pair of adjacent bytes:
		///
		///     child0 = (thisAddress AND NOT 1) + offset * 2 + 2   (even)
		///     child1 = child0 + 1                                 (odd)
		///
		/// Walking from the root, every bit of the bitstream picks child 0 or child 1; landing on a
		/// child whose parent flagged it as data appends that child's byte to the output and starts
		/// again from the root. A byte can therefore be data for one branch and a node for the
		/// other - the two flags are what keeps that unambiguous.
		///
		/// The output is written in 32bit units: the stream is decoded until the unit holding the
		/// last requested byte is full, so a byte count that is not a multiple of four still has its
		/// last unit written whole (confirmed against the official BIOS, whose loop only tests its
		/// remaining-byte counter between units).
		///
		/// The bitstream is read the way the BIOS reads it: `ldr r5, [r0], #4` from the byte after
		/// the tree table, so a 32bit unit loaded from an address that is not word aligned is the
		/// ARM7TDMI's rotated word - the first *byte* consumed is the one at the unit address, and
		/// the two bytes before it come last. That is not a detail that can be dropped: with the
		/// deep tree the differential test uses, the rotation is what puts 99h/88h - which are also
		/// the tree's data bytes - in front of the bitstream that starts after them.
		/// </summary>
		static void HuffUnComp(GbaBus& bus, uint32_t source, uint32_t dest)
		{
			uint32_t header = bus.Read32(source);
			uint32_t unitBits = header & 0xF;
			int32_t outBytes = (int32_t)(header >> 8);

			if ((header & 0xF0) != 0x20 || (unitBits != 4 && unitBits != 8) || outBytes <= 0)
			{
				Log(LogLevel::Warn, "HLE BIOS: HuffUnComp stream at %08X is not a Huffman header "
					"(%08X)", source, header);
				return;
			}

			// The BIOS computes the bitstream address as source + 4 + (treeSize + 1) * 2 - the tree
			// size byte sits at source + 4 and the root at source + 5.
			uint32_t tree = source + 5;
			uint32_t bitstream = source + 4 + ((uint32_t)bus.Read8(source + 4) + 1) * 2;

			// A word of the output holds 32 / unitBits symbols; the BIOS counts them and stores the
			// accumulator as a 32bit unit. `remaining` is the byte counter its loop drives, which
			// goes negative on the unit that covers the last requested byte.
			uint32_t unitsPerWord = 32 / unitBits;
			int32_t remaining = outBytes;

			uint32_t node = tree;			// the root
			uint32_t accumulator = 0;
			uint32_t units = 0;
			uint32_t bits = 0;				// bits consumed from the bitstream
			uint32_t word = 0;
			uint32_t wordBits = 0;

			// A malformed tree cannot cycle (the child address always moves forward), but it can
			// walk far outside its own table looking for a data flag, so keep a ceiling on it: the
			// hardware would loop until it found one.
			uint32_t visits = 0;
			uint32_t visitLimit = (uint32_t)((outBytes + 3) & ~3) * 256 + 4096;

			while (remaining > 0 && visits++ < visitLimit)
			{
				if (wordBits == 0)
				{
					uint32_t address = bitstream + (bits >> 5) * 4;
					word = bus.Read32(address & ~3u);
					uint32_t rotation = (address & 3) * 8;

					if (rotation != 0)
						word = (word >> rotation) | (word << (32 - rotation));

					wordBits = 32;
				}

				uint32_t bit = (word >> 31) & 1;
				word <<= 1;
				wordBits--;
				bits++;

				uint8_t flags = bus.Read8(node);
				uint32_t child = (node & ~1u) + (((uint32_t)flags & 0x3F) + 1) * 2 + bit;

				bool isData = (bit == 0) ? ((flags & 0x80) != 0) : ((flags & 0x40) != 0);

				if (!isData)
				{
					node = child;
					continue;
				}

				// The accumulator is shifted by the unit size and the byte's low unitBits bits are
				// dropped in at the top; with 4 or 8 bit units a word is exactly `unitsPerWord`
				// symbols, so the first symbol ends up in the low bits - little endian.
				accumulator = (accumulator >> unitBits) |
					((uint32_t)bus.Read8(child) << (32 - unitBits));
				node = tree;

				if (++units == unitsPerWord)
				{
					bus.Write32(dest, accumulator);
					dest += 4;
					remaining -= 4;
					units = 0;
					accumulator = 0;
				}
			}

			if (visits >= visitLimit)
			{
				Log(LogLevel::Warn, "HLE BIOS: HuffUnComp stream at %08X has a broken tree (gave up "
					"after %u nodes)", source, visits);
			}
		}

		/// <summary>
		/// The GBA's run-length variant (GBATEK "RLUnComp"). The flag byte is *not* a bit field of
		/// eight blocks like LZ77's: bit 7 says whether the run is compressed and bits 0-6 are its
		/// length (N-1 bytes to copy, or N-3 copies of one byte), so one flag byte and one data run
		/// is the whole unit. An earlier version here read the flag as eight bits, which turned
		/// every run into a set of literals.
		/// </summary>
		static void RlUnComp(GbaBus& bus, uint32_t source, uint32_t dest, bool vram)
		{
			uint32_t header = (bus.Read8(source + 1) | (bus.Read8(source + 2) << 8) | (bus.Read8(source + 3) << 16));
			uint32_t src = source + 4;
			int32_t remaining = (int32_t)header;
			DecompressedOutput out(bus, dest, vram);

			while (remaining > 0)
			{
				uint8_t flags = bus.Read8(src++);
				uint32_t length = (uint32_t)(flags & 0x7F);

				if (flags & 0x80)
				{
					length += 3;
					uint8_t value = bus.Read8(src++);
					remaining -= (int32_t)length;

					for (uint32_t i = 0; i < length; i++)
						out.Put(value);
				}
				else
				{
					length += 1;
					remaining -= (int32_t)length;

					for (uint32_t i = 0; i < length; i++)
						out.Put(bus.Read8(src++));
				}
			}
		}

		/// <summary>
		/// The GBA's delta filters (GBATEK "Diff8bit/Diff16bitUnFilter"): the first unit is
		/// absolute and every following unit is a delta added to the previous one, in 8 or 16 bit
		/// arithmetic. The byte count in the header is the size after decompression; the BIOS
		/// writes the first unit before it looks at that count at all.
		/// </summary>
		static void DiffUnFilter(GbaBus& bus, uint32_t source, uint32_t dest, int width, bool vram)
		{
			uint32_t header = (bus.Read8(source + 1) | (bus.Read8(source + 2) << 8) | (bus.Read8(source + 3) << 16));
			uint32_t src = source + 4;
			int32_t remaining = (int32_t)header;

			if (width == 1)
			{
				DecompressedOutput out(bus, dest, vram);
				uint8_t value = bus.Read8(src++);

				out.Put(value);
				remaining--;

				while (remaining > 0)
				{
					value = (uint8_t)(value + bus.Read8(src++));
					out.Put(value);
					remaining--;
				}
			}
			else
			{
				uint16_t value = bus.Read16(src);
				src += 2;

				bus.Write16(dest, value);
				dest += 2;
				remaining -= 2;

				while (remaining > 0)
				{
					value = (uint16_t)(value + bus.Read16(src));
					src += 2;
					bus.Write16(dest, value);
					dest += 2;
					remaining -= 2;
				}
			}
		}

		// -----------------------------------------------------------------------------------
		// The dispatcher
		// -----------------------------------------------------------------------------------

		bool Swi(GbaBus& bus, uint32_t comment)
		{
			// A SWI with an unimplemented comment field is not swallowed: the emulator reports
			// it and treats it as "return to the caller", because running into the SWI vector of
			// a BIOS image we do not have would loop forever.
			callCounts[comment & 0xFF]++;

			switch (comment)
			{
				case SwiSoftReset:
					SoftResetInternal(bus);
					return true;

				case SwiRegisterRamReset:
					RegisterRamReset(bus, Reg(bus, 0));
					Return(bus);
					return true;

				case SwiHalt:
				case SwiStop:
				case SwiCustomHalt:
					// The CPU stops until an interrupt request arrives; the return address is the
					// caller's, so the BIOS contract ("the call returns after the interrupt") is
					// kept.
					bus.cpu.Halt();
					Return(bus);
					return true;

				case SwiIntrWait:
				case SwiVBlankIntrWait:
				{
					uint16_t mask = (comment == SwiVBlankIntrWait) ? INT_VBLANK : (uint16_t)Reg(bus, 1);
					bool discard = (comment == SwiVBlankIntrWait) ? false : (Reg(bus, 0) != 0);

					if (discard)
					{
						bus.irq.Acknowledge(mask);
					}

					if ((bus.irq.ReadIF() & mask) != 0)
					{
						// The interrupt is already pending: the call returns at once.
						Return(bus);
						return true;
					}

					waiting = true;
					waitMask = mask;
					bus.cpu.Halt();
					Return(bus);
					return true;
				}

				case SwiDiv:
					Divide(bus, Reg(bus, 0), Reg(bus, 1));
					Return(bus);
					return true;

				case SwiDivArm:
					Divide(bus, Reg(bus, 1), Reg(bus, 0));
					Return(bus);
					return true;

				case SwiSqrt:
				{
					uint32_t value = Reg(bus, 0);
					uint32_t root = (uint32_t)sqrt((double)value);
					// Fix up the floor in case of a rounding error at the boundary.
					while ((uint64_t)(root + 1) * (root + 1) <= value) root++;
					while ((uint64_t)root * root > value) root--;
					SetReg(bus, 0, root);
					Return(bus);
					return true;
				}

				case SwiArcTan:
				{
					int32_t tangent = (int32_t)Reg(bus, 0);
					double value = tangent / 65536.0;
					SetReg(bus, 0, AngleFromRadians(atan(value)) & 0xFFFF);
					Return(bus);
					return true;
				}

				case SwiArcTan2:
				{
					int32_t x = (int32_t)Reg(bus, 0);
					int32_t y = (int32_t)Reg(bus, 1);
					double angle = atan2((double)y, (double)x);
					SetReg(bus, 0, AngleFromRadians(angle) & 0xFFFF);
					Return(bus);
					return true;
				}

				case SwiCpuSet:
					CpuSet(bus, Reg(bus, 0), Reg(bus, 1), Reg(bus, 2));
					Return(bus);
					return true;

				case SwiCpuFastSet:
					CpuFastSet(bus, Reg(bus, 0), Reg(bus, 1), Reg(bus, 2));
					Return(bus);
					return true;

				case SwiGetBiosChecksum:
				{
					// The real BIOS returns a fixed checksum of its own image. Our image is the
					// custom boot ROM, so the checksum is computed over it: a game that verifies
					// the BIOS against a known value will not match (and none is known to).
					uint32_t sum = 0;
					for (uint32_t i = 0; i < BiosSize; i += 2)
					{
						sum += bus.bios.Read16(i);
					}
					SetReg(bus, 0, sum);
					Return(bus);
					return true;
				}

				case SwiBgAffineSet:
					BgAffineSet(bus, Reg(bus, 0), Reg(bus, 1), Reg(bus, 2));
					Return(bus);
					return true;

				case SwiObjAffineSet:
					ObjAffineSet(bus, Reg(bus, 0), Reg(bus, 1), Reg(bus, 2), Reg(bus, 3));
					Return(bus);
					return true;

				case SwiBitUnPack:
					BitUnPack(bus, Reg(bus, 0), Reg(bus, 1), Reg(bus, 2));
					Return(bus);
					return true;

				case SwiLz77UnCompWram:
					Lz77UnComp(bus, Reg(bus, 0), Reg(bus, 1), false);
					Return(bus);
					return true;

				case SwiLz77UnCompVram:
					Lz77UnComp(bus, Reg(bus, 0), Reg(bus, 1), true);
					Return(bus);
					return true;

				case SwiHuffUnComp:
					HuffUnComp(bus, Reg(bus, 0), Reg(bus, 1));
					Return(bus);
					return true;

				case SwiRlUnCompWram:
					RlUnComp(bus, Reg(bus, 0), Reg(bus, 1), false);
					Return(bus);
					return true;

				case SwiRlUnCompVram:
					RlUnComp(bus, Reg(bus, 0), Reg(bus, 1), true);
					Return(bus);
					return true;

				case SwiDiff8bitUnFilterWram:
					DiffUnFilter(bus, Reg(bus, 0), Reg(bus, 1), 1, false);
					Return(bus);
					return true;

				case SwiDiff8bitUnFilterVram:
					DiffUnFilter(bus, Reg(bus, 0), Reg(bus, 1), 1, true);
					Return(bus);
					return true;

				case SwiDiff16bitUnFilter:
					DiffUnFilter(bus, Reg(bus, 0), Reg(bus, 1), 2, false);
					Return(bus);
					return true;

				case SwiSoundBias:
				{
					uint16_t bias = bus.Read16(0x04000088);
					bias = (uint16_t)((bias & 0xFF00) | (Reg(bus, 0) & 0x3FF) | 0x0000);
					bus.Write16(0x04000088, bias);
					Return(bus);
					return true;
				}

				case SwiMidiKey2Freq:
					// GBATEK 1Fh: the WaveData's frequency scaled by the key and the fine value,
					// which is what the real BIOS returns (see MidiKey2FreqValue).
					SetReg(bus, 0, MidiKey2FreqValue(bus, Reg(bus, 0), Reg(bus, 1), Reg(bus, 2)));
					Return(bus);
					return true;

				case SwiSoundDriverInit:
					SoundDriverInit(bus, Reg(bus, 0));
					Return(bus);
					return true;

				case SwiSoundDriverMode:
					SoundDriverMode(bus, Reg(bus, 0));
					Return(bus);
					return true;

				case SwiSoundDriverMain:
					SoundDriverMain(bus);
					Return(bus);
					return true;

				case SwiSoundDriverVSync:
					SoundDriverVSync(bus);
					Return(bus);
					return true;

				case SwiSoundChannelClear:
					SoundChannelClear(bus);
					Return(bus);
					return true;

				case SwiSoundDriverVSyncOff:
					SoundDriverVSyncOff(bus);
					Return(bus);
					return true;

				case SwiSoundDriverVSyncOn:
					SoundDriverVSyncOn(bus);
					Return(bus);
					return true;

				case SwiHardReset:
					bus.cpu.Reset();
					return true;

				// The multiboot slave handshake and the undocumented sound entry points need the
				// BIOS's own RAM variables and are not implemented; they are reported so a game
				// that needs them is visible in the log instead of silently misbehaving.
				case SwiMultiBoot:
				case SwiSoundWhatever0:
				case SwiSoundWhatever1:
				case SwiSoundWhatever2:
				case SwiSoundWhatever3:
				case SwiSoundWhatever4:
				case SwiSoundGetJumpList:
				default:
					Log(LogLevel::Warn, "HLE BIOS: SWI %02X is not implemented; returning to the caller", comment);
					Return(bus);
					return true;
			}
		}
	}
}
