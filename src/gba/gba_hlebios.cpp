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
		static u64 callCounts[0x100];
		static bool waiting = false;
		static u16 waitMask = 0;

		static u32 Reg(GbaBus& bus, int index) { return bus.cpu.Reg(index); }
		static void SetReg(GbaBus& bus, int index, u32 value) { bus.cpu.SetReg(index, value); }

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
		}

		u64 CallCount(u32 comment)
		{
			return callCounts[comment & 0xFF];
		}

		bool Implemented(u32 comment)
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
				case SwiRlUnCompWram:
				case SwiRlUnCompVram:
				case SwiDiff8bitUnFilterWram:
				case SwiDiff8bitUnFilterVram:
				case SwiDiff16bitUnFilter:
				case SwiSoundBias:
				case SwiMidiKey2Freq:
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

		static void WriteMemory8(GbaBus& bus, u32 address, u8 value)
		{
			bus.Write8(address, value);
		}

		static void WriteMemory16(GbaBus& bus, u32 address, u16 value)
		{
			bus.Write16(address, value);
		}

		static u8 ReadMemory8(GbaBus& bus, u32 address)
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

			for (u32 i = 0; i < IwramSize - 0x200; i++)
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

		static void RegisterRamReset(GbaBus& bus, u32 flags)
		{
			if (flags & 0x01)
			{
				bus.ewram.Fill(0);
			}

			if (flags & 0x02)
			{
				// The last 0x200 bytes hold the BIOS's own variables and are preserved.
				for (u32 i = 0; i < IwramSize - 0x200; i++)
				{
					bus.iwram.Write8(i, 0);
				}
			}

			if (flags & 0x04)
			{
				for (u32 i = 0; i < PaletteSize; i++)
				{
					bus.ppu.WritePalette(i, 0);
				}
			}

			if (flags & 0x08)
			{
				for (u32 i = 0; i < VramSize; i++)
				{
					bus.ppu.WriteVram(i, 0);
				}
			}

			if (flags & 0x10)
			{
				for (u32 i = 0; i < OamSize; i++)
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
				for (u32 offset = 0x060; offset <= 0x0A7; offset++)
				{
					bus.apu.Write8(offset, 0);
				}
			}

			if (flags & 0x80)
			{
				// "Other I/O": everything up to 0x04000060 except DISPCNT, plus the timers, the
				// DMA channels and the interrupt registers.
				u16 dispcnt = bus.ppu.DispCnt();
				for (u32 offset = 0x000; offset < 0x060; offset += 2)
				{
					bus.ppu.Write16(bus, offset, 0, (int)bus.TotalCycles());
				}
				bus.ppu.Write16(bus, 0x000, dispcnt, (int)bus.TotalCycles());

				for (u32 offset = 0x0B0; offset <= 0x0DE; offset += 2)
				{
					bus.dma.Write16(bus, offset, 0);
				}

				for (u32 offset = 0x100; offset <= 0x10E; offset += 2)
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

		static void Divide(GbaBus& bus, u32 numerator, u32 denominator)
		{
			s32 num = (s32)numerator;
			s32 den = (s32)denominator;

			if (den == 0)
			{
				// GBATEK: dividing by zero leaves the numerator in r1 and a zero quotient.
				SetReg(bus, 0, 0);
				SetReg(bus, 1, (u32)num);
				SetReg(bus, 3, 0);
				return;
			}

			s32 quotient = num / den;
			s32 remainder = num % den;

			SetReg(bus, 0, (u32)quotient);
			SetReg(bus, 1, (u32)remainder);
			SetReg(bus, 3, (u32)(quotient < 0 ? -quotient : quotient));
		}

		/// <summary>The angle unit the BIOS uses: a full circle is 0x10000.</summary>
		static u32 AngleFromRadians(double radians)
		{
			double units = radians * 65536.0 / (2.0 * 3.14159265358979323846);
			return (u32)(s32)lround(units);
		}

		// -----------------------------------------------------------------------------------
		// Blocks of memory
		// -----------------------------------------------------------------------------------

		static void CpuSet(GbaBus& bus, u32 source, u32 dest, u32 control)
		{
			bool fill = (control & 0x01000000) != 0;
			bool word = (control & 0x04000000) != 0;
			u32 count = control & 0x001FFFFF;

			if (count == 0)
			{
				count = 0x200000;
			}

			if (word)
			{
				for (u32 i = 0; i < count; i++)
				{
					u32 value = fill ? bus.Read32(source) : bus.Read32(source + i * 4);
					bus.Write32(dest + i * 4, value);
				}
			}
			else
			{
				for (u32 i = 0; i < count; i++)
				{
					u16 value = fill ? bus.Read16(source) : bus.Read16(source + i * 2);
					bus.Write16(dest + i * 2, value);
				}
			}
		}

		static void CpuFastSet(GbaBus& bus, u32 source, u32 dest, u32 control)
		{
			bool fill = (control & 0x01000000) != 0;
			u32 count = control & 0x001FFFFF;

			if (count == 0)
			{
				count = 0x200000;
			}

			// The BIOS rounds the length up to a multiple of 8 words.
			count = (count + 7) & ~7u;

			for (u32 i = 0; i < count; i++)
			{
				u32 value = fill ? bus.Read32(source) : bus.Read32(source + i * 4);
				bus.Write32(dest + i * 4, value);
			}
		}

		static void BgAffineSet(GbaBus& bus, u32 source, u32 dest, u32 count)
		{
			for (u32 i = 0; i < count; i++)
			{
				u32 src = source + i * 20;
				u32 dst = dest + i * 16;

				s32 bgx = (s32)bus.Read32(src + 0);			// 1.19.12
				s32 bgy = (s32)bus.Read32(src + 4);
				s16 dispX = (s16)bus.Read16(src + 8);
				s16 dispY = (s16)bus.Read16(src + 10);
				s16 scaleX = (s16)bus.Read16(src + 12);		// 1.7.8
				s16 scaleY = (s16)bus.Read16(src + 14);
				u16 angle = bus.Read16(src + 16);			// 0..0xFFFF, 0x10000 = 360 degrees

				double radians = angle * (2.0 * 3.14159265358979323846 / 65536.0);
				double cosA = cos(radians);
				double sinA = sin(radians);

				double sx = scaleX / 256.0;
				double sy = scaleY / 256.0;

				// The reference point is the screen position mapped back through the rotation.
				double dx = bgx / 4096.0 - ((dispX * sx * cosA) - (dispY * sx * sinA));
				double dy = bgy / 4096.0 - ((dispX * sy * sinA) + (dispY * sy * cosA));

				s16 pa = (s16)lround(sx * cosA * 256.0);
				s16 pb = (s16)lround(-sx * sinA * 256.0);
				s16 pc = (s16)lround(sy * sinA * 256.0);
				s16 pd = (s16)lround(sy * cosA * 256.0);

				bus.Write16(dst + 0, (u16)pa);
				bus.Write16(dst + 2, (u16)pb);
				bus.Write16(dst + 4, (u16)pc);
				bus.Write16(dst + 6, (u16)pd);
				bus.Write32(dst + 8, (u32)(s32)lround(dx * 256.0));
				bus.Write32(dst + 12, (u32)(s32)lround(dy * 256.0));
			}
		}

		static void ObjAffineSet(GbaBus& bus, u32 source, u32 dest, u32 count, u32 stride)
		{
			for (u32 i = 0; i < count; i++)
			{
				u32 src = source + i * 8;
				u32 dst = dest + i * stride;

				s16 scaleX = (s16)bus.Read16(src + 0);
				s16 scaleY = (s16)bus.Read16(src + 2);
				u16 angle = bus.Read16(src + 4);

				double radians = angle * (2.0 * 3.14159265358979323846 / 65536.0);
				double sx = scaleX / 256.0;
				double sy = scaleY / 256.0;

				s16 pa = (s16)lround(sx * cos(radians) * 256.0);
				s16 pb = (s16)lround(-sx * sin(radians) * 256.0);
				s16 pc = (s16)lround(sy * sin(radians) * 256.0);
				s16 pd = (s16)lround(sy * cos(radians) * 256.0);

				bus.Write16(dst + 0, (u16)pa);
				bus.Write16(dst + 2, (u16)pb);
				bus.Write16(dst + 4, (u16)pc);
				bus.Write16(dst + 6, (u16)pd);
			}
		}

		static void BitUnPack(GbaBus& bus, u32 source, u32 dest, u32 info)
		{
			u16 sourceLength = bus.Read16(info + 0);
			u8 sourceWidth = bus.Read8(info + 2);
			u8 destWidth = bus.Read8(info + 3);
			u32 dataOffset = bus.Read32(info + 4);
			u8 flags = bus.Read8(info + 8);

			if (sourceWidth == 0 || sourceWidth > 8 || destWidth == 0 || destWidth > 8)
			{
				Log(LogLevel::Warn, "BitUnPack: source/dest width %i/%i is not supported", sourceWidth, destWidth);
				return;
			}

			u32 destMask = (destWidth == 8) ? 0xFF : ((1u << destWidth) - 1);
			u32 destBit = 0;
			u8 writeByte = 0;
			u32 byteBits = 0;

			for (u32 i = 0; i < (u32)sourceLength * 8 / sourceWidth; i++)
			{
				// Read one source unit.
				u8 byte = bus.Read8(source + (i * sourceWidth) / 8);
				u32 shift = (i * sourceWidth) % 8;
				u32 value = (byte >> shift) & ((1u << sourceWidth) - 1);

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
						writeByte |= (u8)(1 << byteBits);
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
		// The decompressors
		// -----------------------------------------------------------------------------------

		/// <summary>The GBA's LZ77 variant: a 4-bit length and a 12-bit backward offset.</summary>
		static void Lz77UnComp(GbaBus& bus, u32 source, u32 dest, bool vram)
		{
			u32 header = (bus.Read8(source + 1) | (bus.Read8(source + 2) << 8) | (bus.Read8(source + 3) << 16));
			u32 src = source + 4;
			u32 dst = dest;
			u32 written = 0;

			while (written < header)
			{
				u8 flags = bus.Read8(src++);

				for (int bit = 0; bit < 8 && written < header; bit++)
				{
					if (flags & 0x80)
					{
						u8 b0 = bus.Read8(src++);
						u8 b1 = bus.Read8(src++);
						u32 length = (b0 >> 4) + 3;
						u32 offset = (((b0 & 0xF) << 8) | b1) + 1;

						for (u32 i = 0; i < length && written < header; i++)
						{
							u8 value = bus.Read8(dst - offset);
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

		/// <summary>The GBA's run-length variant: a length byte and the byte to repeat.</summary>
		static void RlUnComp(GbaBus& bus, u32 source, u32 dest)
		{
			u32 header = (bus.Read8(source + 1) | (bus.Read8(source + 2) << 8) | (bus.Read8(source + 3) << 16));
			u32 src = source + 4;
			u32 dst = dest;
			u32 written = 0;

			while (written < header)
			{
				u8 flags = bus.Read8(src++);

				for (int bit = 0; bit < 8 && written < header; bit++)
				{
					if (flags & 0x80)
					{
						u8 length = bus.Read8(src++);
						u8 value = bus.Read8(src++);

						for (u32 i = 0; i < (u32)length + 3 && written < header; i++)
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
		static void DiffUnFilter(GbaBus& bus, u32 source, u32 dest, int width)
		{
			u32 header = (bus.Read8(source + 1) | (bus.Read8(source + 2) << 8) | (bus.Read8(source + 3) << 16));
			u32 src = source + 4;

			if (width == 1)
			{
				u8 value = 0;
				for (u32 i = 0; i < header; i++)
				{
					value = (u8)(value + bus.Read8(src + i));
					bus.Write8(dest + i, value);
				}
			}
			else
			{
				u8 value = 0;
				u32 halves = header / 2;
				for (u32 i = 0; i < halves; i++)
				{
					value = (u8)(value + bus.Read8(src + i));
					bus.Write8(dest + i * 2, value);
					bus.Write8(dest + i * 2 + 1, 0);
				}
			}
		}

		// -----------------------------------------------------------------------------------
		// The dispatcher
		// -----------------------------------------------------------------------------------

		bool Swi(GbaBus& bus, u32 comment)
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
					u16 mask = (comment == SwiVBlankIntrWait) ? INT_VBLANK : (u16)Reg(bus, 1);
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
				case SwiDivArm2:
					Divide(bus, Reg(bus, 1), Reg(bus, 0));
					Return(bus);
					return true;

				case SwiSqrt:
				{
					u32 value = Reg(bus, 0);
					u32 root = (u32)sqrt((double)value);
					// Fix up the floor in case of a rounding error at the boundary.
					while ((u64)(root + 1) * (root + 1) <= value) root++;
					while ((u64)root * root > value) root--;
					SetReg(bus, 0, root);
					Return(bus);
					return true;
				}

				case SwiArcTan:
				{
					s32 tangent = (s32)Reg(bus, 0);
					double value = tangent / 65536.0;
					SetReg(bus, 0, AngleFromRadians(atan(value)) & 0xFFFF);
					Return(bus);
					return true;
				}

				case SwiArcTan2:
				{
					s32 x = (s32)Reg(bus, 0);
					s32 y = (s32)Reg(bus, 1);
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
					u32 sum = 0;
					for (u32 i = 0; i < BiosSize; i += 2)
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
					u16 bias = bus.Read16(0x04000088);
					bias = (u16)((bias & 0xFF00) | (Reg(bus, 0) & 0x3FF) | 0x0000);
					bus.Write16(0x04000088, bias);
					Return(bus);
					return true;
				}

				case SwiMidiKey2Freq:
				{
					// GBATEK 19.4: the WaveData structure's frequency (offset 4, 1.10.14) is
					// scaled by the MIDI key and the fine tune value.
					u32 waveData = Reg(bus, 0);
					int key = (int)(Reg(bus, 1) & 0x7F);
					int fine = (int)(Reg(bus, 2) & 0xFF);

					u32 frequency = bus.Read32(waveData + 4);
					double value = (double)frequency * pow(2.0, (key - 60) / 12.0) * (1.0 - (fine / 256.0) / 8.0);
					SetReg(bus, 0, (u32)value);
					Return(bus);
					return true;
				}

				case SwiHardReset:
					bus.cpu.Reset();
					return true;

				// The sound driver entry points and the multiboot slave handshake need the BIOS's
				// own RAM variables and are not implemented; they are reported once per call so a
				// game that needs them is visible in the log instead of silently misbehaving.
				case SwiMultiBoot:
				case SwiSoundDriverInit:
				case SwiSoundDriverMain:
				case SwiSoundDriverMode:
				case SwiSoundDriverVsync:
				case SwiSoundChannelClear:
				case SwiSoundDriverVsyncOff:
				case SwiSoundDriverVsyncOn:
				case SwiHuffUnComp:
				default:
					Log(LogLevel::Warn, "HLE BIOS: SWI %02X is not implemented; returning to the caller", comment);
					Return(bus);
					return true;
			}
		}
	}
}
