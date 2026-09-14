// Game Boy Advance (and Game Boy) emulator: common types and constants.
//
// The whole module is written from the public hardware specifications (the ARM Architecture Reference Manual for the
// ARM7TDMI, GBATEK for the GBA peripherals, the Pan Docs for the DMG/CGB) and from the
// instruction encodings in those documents. It is deliberately self contained: it includes
// nothing from the GameCube side of the emulator, so the core can be compiled and unit tested
// without SDL, OpenGL or ImGui. See testing/gba_bench.

#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <memory>

namespace GBA
{
	using u8 = uint8_t;
	using u16 = uint16_t;
	using u32 = uint32_t;
	using u64 = uint64_t;

	using s8 = int8_t;
	using s16 = int16_t;
	using s32 = int32_t;
	using s64 = int64_t;

	// ---------------------------------------------------------------------------------------
	// Screen
	// ---------------------------------------------------------------------------------------

	const int ScreenWidth = 240;
	const int ScreenHeight = 160;

	// The GBA clock: 16.78 MHz. Everything (the CPU, the timers, the LCD, the sound) is
	// scheduled from this clock. The four cycle counts per dot give 1232 cycles per scanline.
	const int CyclesPerSecond = 16777216;
	const int CyclesPerScanline = 1232;
	const int ScanlinesTotal = 228;			// 160 visible + 68 VBlank
	const int CyclesPerFrame = CyclesPerScanline * ScanlinesTotal;

	// ---------------------------------------------------------------------------------------
	// Interrupts (GBA): the IE/IF bits and the CPU exception vectors
	// ---------------------------------------------------------------------------------------

	enum InterruptBit : u16
	{
		INT_VBLANK = 0x0001,
		INT_HBLANK = 0x0002,
		INT_VCOUNT = 0x0004,
		INT_TIMER0 = 0x0008,
		INT_TIMER1 = 0x0010,
		INT_TIMER2 = 0x0020,
		INT_TIMER3 = 0x0040,
		INT_SIO = 0x0080,
		INT_DMA0 = 0x0100,
		INT_DMA1 = 0x0200,
		INT_DMA2 = 0x0400,
		INT_DMA3 = 0x0800,
		INT_KEYPAD = 0x1000,
		INT_GAMEPAK = 0x2000,
	};

	// The CPU exception vectors, in the low 16 KByte of the BIOS.
	const u32 VectorReset = 0x00000000;
	const u32 VectorUndefined = 0x00000004;
	const u32 VectorSwi = 0x00000008;
	const u32 VectorPrefetchAbort = 0x0000000C;
	const u32 VectorDataAbort = 0x00000010;
	const u32 VectorIrq = 0x00000018;
	const u32 VectorFiq = 0x0000001C;

	// ---------------------------------------------------------------------------------------
	// The CPSR (ARM7TDMI). The CPU header has the full set; the mode bits are repeated here
	// because the bus has to know the CPU mode when it serves a banked register access.
	// ---------------------------------------------------------------------------------------

	enum CpuMode : u32
	{
		ModeUser = 0x10,
		ModeFiq = 0x11,
		ModeIrq = 0x12,
		ModeSupervisor = 0x13,
		ModeAbort = 0x17,
		ModeUndefined = 0x1B,
		ModeSystem = 0x1F,
		ModeMask = 0x1F,
	};

	// ---------------------------------------------------------------------------------------
	// Memory map (the GBA address decoder). Every region is mirrored through its size.
	// ---------------------------------------------------------------------------------------

	const u32 MemBios = 0x00000000;			// 16 KByte, readable only
	const u32 MemEwram = 0x02000000;		// 256 KByte
	const u32 MemIwram = 0x03000000;		// 32 KByte
	const u32 MemIo = 0x04000000;			// 1 KByte, mirrored through 0x04000400
	const u32 MemPalette = 0x05000000;		// 1 KByte
	const u32 MemVram = 0x06000000;			// 96 KByte
	const u32 MemOam = 0x07000000;			// 1 KByte
	const u32 MemRom1 = 0x08000000;			// up to 32 MByte, waitstates per WAITCNT
	const u32 MemRom2 = 0x0A000000;
	const u32 MemRom3 = 0x0C000000;
	const u32 MemSram = 0x0E000000;			// SRAM/Flash, 64 KByte window

	const u32 EwramSize = 256 * 1024;
	const u32 IwramSize = 32 * 1024;
	const u32 IoSize = 0x400;
	const u32 PaletteSize = 0x400;
	const u32 VramSize = 0x18000;

	/// <summary>VRAM is mirrored every 128 KByte (GBATEK "GBA Memory Map"), so the address
	/// decoder wraps through this window before it wraps through the 96 KByte bank itself.</summary>
	const u32 VramMirror = 0x20000;

	const u32 OamSize = 0x400;
	const u32 BiosSize = 0x4000;

	// ---------------------------------------------------------------------------------------
	// Small helpers
	// ---------------------------------------------------------------------------------------

	template <typename T> inline T Bit(T value, int bit) { return (T)((value >> bit) & 1); }
	template <typename T> inline T Bits(T value, int shift, T mask) { return (T)((value >> shift) & mask); }

	inline u16 Swap16(u16 value) { return (u16)((value << 8) | (value >> 8)); }

	inline u32 Swap32(u32 value)
	{
		return (value << 24) | ((value & 0xFF00) << 8) | ((value >> 8) & 0xFF00) | (value >> 24);
	}

	// A 15-bit GBA colour (0bbbbbgggggrrrrr) expanded to XRGB8888 for the host.
	inline u32 Color15ToXrgb(u16 color)
	{
		u32 r = (color & 0x1F) << 3;
		u32 g = ((color >> 5) & 0x1F) << 3;
		u32 b = ((color >> 10) & 0x1F) << 3;
		r |= r >> 5;
		g |= g >> 5;
		b |= b >> 5;
		return 0xFF000000 | (r << 16) | (g << 8) | b;
	}

	// A 15-bit GBA colour with an alpha coefficient (0..16) applied, as the blending unit does.
	inline u16 Blend15(u16 a, u16 b, int eva, int evb)
	{
		int r = (((a & 0x1F) * eva) + ((b & 0x1F) * evb)) >> 4;
		int g = ((((a >> 5) & 0x1F) * eva) + (((b >> 5) & 0x1F) * evb)) >> 4;
		int bl = ((((a >> 10) & 0x1F) * eva) + (((b >> 10) & 0x1F) * evb)) >> 4;
		if (r > 31) r = 31;
		if (g > 31) g = 31;
		if (bl > 31) bl = 31;
		return (u16)(r | (g << 5) | (bl << 10));
	}

	// ---------------------------------------------------------------------------------------
	// Logging. The module never calls printf directly: the frontend installs a sink so that the
	// unit tests can capture the messages (the emulator's own Debug::Report lives on the
	// GameCube side and must not be linked into the standalone harness).
	// ---------------------------------------------------------------------------------------

	enum class LogLevel
	{
		Error,
		Warn,
		Info,
		Debug,
	};

	using LogSink = void (*)(LogLevel level, const char* text, void* user);

	/// <summary>Route the core's messages to `sink` (pass nullptr to discard them).</summary>
	void SetLogSink(LogSink sink, void* user);

	/// <summary>Report one message through the current sink.</summary>
	void Log(LogLevel level, const char* format, ...);

	// ---------------------------------------------------------------------------------------
	// A fixed size byte buffer with the mirroring rule the GBA address decoder uses.
	// ---------------------------------------------------------------------------------------

	class MemoryBank
	{
		u8* storage = nullptr;
		u32 size = 0;
		u32 mask = 0;			// size-1 when the size is a power of two, otherwise 0
		bool powerOfTwo = true;

	public:
		MemoryBank() = default;

		void Init(u32 bytes);
		void Free();

		u8* Data() { return storage; }
		const u8* Data() const { return storage; }
		u32 Size() const { return size; }

		/// <summary>
		/// Fold an address into the bank. Every region of the GBA is mirrored through its size;
		/// where the size is not a power of two (VRAM is 96 KByte) the hardware wraps with a
		/// modulo instead, which is what this does.
		/// </summary>
		u32 Mirror(u32 offset) const { return powerOfTwo ? (offset & mask) : (offset % (size ? size : 1)); }

		u8 Read8(u32 offset) const { return storage[Mirror(offset)]; }
		u16 Read16(u32 offset) const { u16 v; memcpy(&v, storage + Mirror(offset), 2); return v; }
		u32 Read32(u32 offset) const { u32 v; memcpy(&v, storage + Mirror(offset), 4); return v; }

		/// <summary>Write through the mirroring rule.</summary>
		void Write8(u32 offset, u8 value) { storage[Mirror(offset)] = value; }
		void Write16(u32 offset, u16 value) { memcpy(storage + Mirror(offset), &value, 2); }
		void Write32(u32 offset, u32 value) { memcpy(storage + Mirror(offset), &value, 4); }

		void Fill(u8 value) { if (storage) memset(storage, value, size); }
	};
}
