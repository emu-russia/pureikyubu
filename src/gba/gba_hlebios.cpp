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

		static void BitUnPack(GbaBus& bus, uint32_t source, uint32_t dest, uint32_t info)
		{
			uint16_t sourceLength = bus.Read16(info + 0);
			uint8_t sourceWidth = bus.Read8(info + 2);
			uint8_t destWidth = bus.Read8(info + 3);
			uint32_t dataOffset = bus.Read32(info + 4);
			uint8_t flags = bus.Read8(info + 8);

			if (sourceWidth == 0 || sourceWidth > 8 || destWidth == 0 || destWidth > 8)
			{
				Log(LogLevel::Warn, "BitUnPack: source/dest width %i/%i is not supported", sourceWidth, destWidth);
				return;
			}

			uint32_t destMask = (destWidth == 8) ? 0xFF : ((1u << destWidth) - 1);
			uint32_t destBit = 0;
			uint8_t writeByte = 0;
			uint32_t byteBits = 0;

			for (uint32_t i = 0; i < (uint32_t)sourceLength * 8 / sourceWidth; i++)
			{
				// Read one source unit.
				uint8_t byte = bus.Read8(source + (i * sourceWidth) / 8);
				uint32_t shift = (i * sourceWidth) % 8;
				uint32_t value = (byte >> shift) & ((1u << sourceWidth) - 1);

				if (flags & 0x01)
				{
					value = (value + dataOffset) & destMask;
				}
				else if (value != 0)
				{
					value = (value + dataOffset) & destMask;
				}

				// Write it to the destination, bit by bit.
				for (int b = 0; b < destWidth; b++)
				{
					if ((value >> b) & 1)
					{
						writeByte |= (uint8_t)(1 << byteBits);
					}
					byteBits++;
					if (byteBits == 8)
					{
						bus.Write8(dest + destBit, writeByte);
						destBit++;
						writeByte = 0;
						byteBits = 0;
					}
				}
			}

			if (byteBits != 0)
			{
				bus.Write8(dest + destBit, writeByte);
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
		// What is *not* here yet: the mixer itself (SoundDriverMain writing the virtual channels
		// into pcmbuf). Until it is, Main keeps the buffer silent rather than letting the FIFOs
		// play whatever was left behind - the game's sound effects (which go through the PSG
		// channels, set up by the registers below) still work.
		//
		// Where the mixer lives in the official image, for whoever finishes it (found by stepping
		// into the SWI and disassembling - the driver is Thumb with an ARM inner loop):
		//
		//   * SWI 1Ch enters at 01DC8h (Thumb). It checks the work area's identifier against
		//     68736D53h, increments it, then computes the mixed buffer it is about to fill:
		//     `pcmbuf = <literal at 02104h> + area + (area[0Bh] - (area[04h] - 1)) * area[10h]`,
		//     where area[04h] is the documented "DmaCount" (which buffer), area[0Bh] a count and
		//     area[10h] the stride (the "NoUse" field) - the two literals are the offsets 350h and
		//     980h the FIFO DMAs point at. The area's +20h, +24h and +28h hold *Thumb code
		//     pointers* (called through a trampoline at 02102h), which is why the header has to be
		//     read as the driver's own rather than as the documented SoundArea.
		//   * At 01E1Ch it switches to ARM (`add r1, pc, #0` / `bx r1` -> 01E24h) and runs the
		//     per-sample loop at 01E30h: signed bytes are read out of the channel's mix state
		//     (`ldrsb`, the reverb path adding a delay line), summed, scaled by the channel's
		//     volume (`mul` then `mov r0, r1, asr #9`, a 9 bit volume scale) and stored to both
		//     output halves, with `tst r0, #80h` / `addne r0, #1` as the sign fix-up of an 8 bit
		//     result. The loop count is the stride, so one pass fills a whole buffer.
		//   * The Thumb loop at 01E8Eh..01E9Eh clears two 15 entry arrays through a pair of
		//     pointers with `stmia`: the per-channel mix state of the driver's virtual channels.
		//     That is where the channel array's *layout* has to be read from next - GBATEK puts
		//     `vchn` right after the header, but the real header is a table of code pointers, so
		//     the array is elsewhere and the stride it uses is what the per-channel loop indexes.
		// -----------------------------------------------------------------------------------

		const uint32_t SoundAreaSize = 0xFB0;
		const uint32_t SoundIdent = 0x68736D53;
		const uint32_t SoundPcmA = 0x350;			// the right channel's mixed buffer
		const uint32_t SoundPcmB = 0x980;			// the left one
		const uint32_t SoundPcmHalf = 0x630;		// PCM_BF: bytes per half, and the stride
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

			// The real driver stops its virtual channels here; the HLE mixer is not written yet,
			// so what this can do is stop the sound the hardware is playing: silence both halves
			// of the mixed buffer (the FIFOs keep streaming it, which is what "stops the sound"
			// has to mean while the DMA repeats).
			SoundSilence(bus, SoundPcmA);
			SoundSilence(bus, SoundPcmB);
		}

		static void SoundDriverMain(GbaBus& bus)
		{
			if (!soundReady)
				return;

			// The mixer is the one piece of the driver that is still missing: fill both halves of
			// the mixed buffer with silence, so a game without a real BIOS hears nothing where its
			// music should be - rather than the garbage a FIFO that is never refilled leaves
			// behind.
			SoundSilence(bus, SoundPcmA);
			SoundSilence(bus, SoundPcmB);
		}

		static void SoundDriverVSync(GbaBus& bus)
		{
			// "An extremely short system call that resets the sound DMA" (GBATEK): the driver
			// re-arms the two FIFO channels so they start streaming the half just mixed.
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

		/// <summary>The GBA's LZ77 variant: a 4-bit length and a 12-bit backward offset.</summary>
		static void Lz77UnComp(GbaBus& bus, uint32_t source, uint32_t dest, bool vram)
		{
			uint32_t header = (bus.Read8(source + 1) | (bus.Read8(source + 2) << 8) | (bus.Read8(source + 3) << 16));
			uint32_t src = source + 4;
			uint32_t dst = dest;
			uint32_t written = 0;

			while (written < header)
			{
				uint8_t flags = bus.Read8(src++);

				for (int bit = 0; bit < 8 && written < header; bit++)
				{
					if (flags & 0x80)
					{
						uint8_t b0 = bus.Read8(src++);
						uint8_t b1 = bus.Read8(src++);
						uint32_t length = (b0 >> 4) + 3;
						uint32_t offset = (((b0 & 0xF) << 8) | b1) + 1;

						for (uint32_t i = 0; i < length && written < header; i++)
						{
							uint8_t value = bus.Read8(dst - offset);
							bus.Write8(dst, value);
							dst++;
							written++;
						}
					}
					else
					{
						bus.Write8(dst, bus.Read8(src++));
						dst++;
						written++;
					}

					flags <<= 1;
				}
			}

			if (vram)
			{
				// The Vram variant writes halfwords (the VRAM bus is 16-bit). The byte-wise
				// writes above land in the same place, so nothing else is needed; the flag is
				// kept so the two entry points stay distinguishable.
			}
		}

		/// <summary>
		/// The GBA's Huffman decompressor (GBATEK "SWI 13h - HuffUnComp"). The stream starts with a
		/// 32bit header - bits 0-3 the size of one data unit in bits (4 or 8; a 4 bit stream packs
		/// its symbols into bytes, low nibble first), bits 4-7 the compression type (2), bits 8-31
		/// the size of the decompressed data in bytes - then an 8bit tree size ((size of the tree
		/// table / 2) - 1), the tree table itself and the bitstream.
		///
		/// A tree node is 8 bits. A node that is not data is: bits 0-5 the offset to the next child
		/// node, bit 6 "the node1 child is data", bit 7 "the node0 child is data", with
		/// child0 = (thisAddress AND NOT 1) + offset * 2 + 2 and child1 = child0 + 1. Walking from
		/// the root, every bit of the bitstream (bit 31 of each 32bit unit first) picks child 0 or
		/// child 1; landing on a child whose parent flagged it as data appends that node's byte
		/// (masked to the unit size) to the output and starts again from the root.
		///
		/// The output is written in 32bit units, so the last unit is padded with zeros.
		/// </summary>
		static void HuffUnComp(GbaBus& bus, uint32_t source, uint32_t dest)
		{
			uint32_t header = bus.Read32(source);
			uint32_t unitBits = header & 0xF;
			uint32_t outBytes = header >> 8;

			if (header & 0xF0 != 0x20 || unitBits == 0 || unitBits > 8 || outBytes == 0)
			{
				Log(LogLevel::Warn, "HLE BIOS: HuffUnComp stream at %08X is not a Huffman header "
					"(%08X)", source, header);
				return;
			}

			uint32_t tree = source + 5;
			uint32_t bitstream = tree + ((uint32_t)bus.Read8(source + 4) + 1) * 2;
			uint32_t mask = (1u << unitBits) - 1;

			// The output is written in 32bit units and the hardware does not stop in the middle of
			// one: it keeps decoding until the unit that holds the last requested byte is full.
			// (Found by comparing against the official BIOS: with a 7 byte stream its last unit's
			// fourth byte is a decoded symbol, not a zero.)
			uint32_t total = (outBytes + 3) & ~3u;

			uint32_t node = tree;			// the root
			uint32_t write = 0;				// the byte being packed
			uint32_t filled = 0;			// bits in it
			uint32_t written = 0;
			uint32_t bits = 0;				// the bit index into the bitstream

			while (written < total)
			{
				uint32_t word = bus.Read32(bitstream + (bits >> 5) * 4);
				uint32_t bit = (word >> (31 - (bits & 31))) & 1;
				bits++;

				uint32_t child = (node & ~1u) + (((uint32_t)bus.Read8(node) & 0x3F) * 2) + 2 + bit;
				uint8_t flags = bus.Read8(node);
				bool isData = (bit == 0) ? ((flags & 0x80) != 0) : ((flags & 0x40) != 0);

				if (!isData)
				{
					node = child;
					continue;
				}

				write |= ((uint32_t)bus.Read8(child) & mask) << filled;
				filled += unitBits;

				if (filled == 8)
				{
					bus.Write8(dest + written, (uint8_t)write);
					written++;
					write = 0;
					filled = 0;
				}

				node = tree;
			}

			// A stream whose last unit the decoder did not finish (a 4 bit unit that stopped on a
			// half byte) still ends on a unit boundary: the rest is zero.
			while (written < total)
			{
				bus.Write8(dest + written, (uint8_t)write);
				written++;
				write = 0;
			}
		}

		/// <summary>The GBA's run-length variant: a length byte and the byte to repeat.</summary>
		static void RlUnComp(GbaBus& bus, uint32_t source, uint32_t dest)
		{
			uint32_t header = (bus.Read8(source + 1) | (bus.Read8(source + 2) << 8) | (bus.Read8(source + 3) << 16));
			uint32_t src = source + 4;
			uint32_t dst = dest;
			uint32_t written = 0;

			while (written < header)
			{
				uint8_t flags = bus.Read8(src++);

				for (int bit = 0; bit < 8 && written < header; bit++)
				{
					if (flags & 0x80)
					{
						uint8_t length = bus.Read8(src++);
						uint8_t value = bus.Read8(src++);

						for (uint32_t i = 0; i < (uint32_t)length + 3 && written < header; i++)
						{
							bus.Write8(dst++, value);
							written++;
						}
					}
					else
					{
						bus.Write8(dst++, bus.Read8(src++));
						written++;
					}

					flags <<= 1;
				}
			}
		}

		/// <summary>The GBA's delta filters: the first byte is absolute, the rest are deltas.</summary>
		static void DiffUnFilter(GbaBus& bus, uint32_t source, uint32_t dest, int width)
		{
			uint32_t header = (bus.Read8(source + 1) | (bus.Read8(source + 2) << 8) | (bus.Read8(source + 3) << 16));
			uint32_t src = source + 4;

			if (width == 1)
			{
				uint8_t value = 0;
				for (uint32_t i = 0; i < header; i++)
				{
					value = (uint8_t)(value + bus.Read8(src + i));
					bus.Write8(dest + i, value);
				}
			}
			else
			{
				uint8_t value = 0;
				uint32_t halves = header / 2;
				for (uint32_t i = 0; i < halves; i++)
				{
					value = (uint8_t)(value + bus.Read8(src + i));
					bus.Write8(dest + i * 2, value);
					bus.Write8(dest + i * 2 + 1, 0);
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
				case SwiRlUnCompVram:
					RlUnComp(bus, Reg(bus, 0), Reg(bus, 1));
					Return(bus);
					return true;

				case SwiDiff8bitUnFilterWram:
				case SwiDiff8bitUnFilterVram:
					DiffUnFilter(bus, Reg(bus, 0), Reg(bus, 1), 1);
					Return(bus);
					return true;

				case SwiDiff16bitUnFilter:
					DiffUnFilter(bus, Reg(bus, 0), Reg(bus, 1), 2);
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
