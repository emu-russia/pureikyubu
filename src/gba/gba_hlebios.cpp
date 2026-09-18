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
				case SwiDivArm2:
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
				{
					// GBATEK 19.4: the WaveData structure's frequency (offset 4, 1.10.14) is
					// scaled by the MIDI key and the fine tune value.
					uint32_t waveData = Reg(bus, 0);
					int key = (int)(Reg(bus, 1) & 0x7F);
					int fine = (int)(Reg(bus, 2) & 0xFF);

					uint32_t frequency = bus.Read32(waveData + 4);
					double value = (double)frequency * pow(2.0, (key - 60) / 12.0) * (1.0 - (fine / 256.0) / 8.0);
					SetReg(bus, 0, (uint32_t)value);
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
