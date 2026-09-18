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

	enum InterruptBit : uint16_t
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
	const uint32_t VectorReset = 0x00000000;
	const uint32_t VectorUndefined = 0x00000004;
	const uint32_t VectorSwi = 0x00000008;
	const uint32_t VectorPrefetchAbort = 0x0000000C;
	const uint32_t VectorDataAbort = 0x00000010;
	const uint32_t VectorIrq = 0x00000018;
	const uint32_t VectorFiq = 0x0000001C;

	// ---------------------------------------------------------------------------------------
	// The CPSR (ARM7TDMI). The CPU header has the full set; the mode bits are repeated here
	// because the bus has to know the CPU mode when it serves a banked register access.
	// ---------------------------------------------------------------------------------------

	enum CpuMode : uint32_t
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

	const uint32_t MemBios = 0x00000000;		// 16 KByte, readable only
	const uint32_t MemEwram = 0x02000000;		// 256 KByte
	const uint32_t MemIwram = 0x03000000;		// 32 KByte
	const uint32_t MemIo = 0x04000000;		// 1 KByte, mirrored through 0x04000400
	const uint32_t MemPalette = 0x05000000;		// 1 KByte
	const uint32_t MemVram = 0x06000000;		// 96 KByte
	const uint32_t MemOam = 0x07000000;		// 1 KByte
	const uint32_t MemRom1 = 0x08000000;		// up to 32 MByte, waitstates per WAITCNT
	const uint32_t MemRom2 = 0x0A000000;
	const uint32_t MemRom3 = 0x0C000000;
	const uint32_t MemSram = 0x0E000000;			// SRAM/Flash, 64 KByte window

	const uint32_t EwramSize = 256 * 1024;
	const uint32_t IwramSize = 32 * 1024;
	const uint32_t IoSize = 0x400;
	const uint32_t PaletteSize = 0x400;
	const uint32_t VramSize = 0x18000;

	/// <summary>VRAM is mirrored every 128 KByte (GBATEK "GBA Memory Map"), so the address
	/// decoder wraps through this window before it wraps through the 96 KByte bank itself.</summary>
	const uint32_t VramMirror = 0x20000;

	const uint32_t OamSize = 0x400;
	const uint32_t BiosSize = 0x4000;

	// ---------------------------------------------------------------------------------------
	// Small helpers
	// ---------------------------------------------------------------------------------------

	template <typename T> inline T Bit(T value, int bit) { return (T)((value >> bit) & 1); }
	template <typename T> inline T Bits(T value, int shift, T mask) { return (T)((value >> shift) & mask); }

	inline uint16_t Swap16(uint16_t value) { return (uint16_t)((value << 8) | (value >> 8)); }

	inline uint32_t Swap32(uint32_t value)
	{
		return (value << 24) | ((value & 0xFF00) << 8) | ((value >> 8) & 0xFF00) | (value >> 24);
	}

	// A 15-bit GBA colour (0bbbbbgggggrrrrr) expanded to XRGB8888 for the host.
	inline uint32_t Color15ToXrgb(uint16_t color)
	{
		uint32_t r = (color & 0x1F) << 3;
		uint32_t g = ((color >> 5) & 0x1F) << 3;
		uint32_t b = ((color >> 10) & 0x1F) << 3;
		r |= r >> 5;
		g |= g >> 5;
		b |= b >> 5;
		return 0xFF000000 | (r << 16) | (g << 8) | b;
	}

	// A 15-bit GBA colour with an alpha coefficient (0..16) applied, as the blending unit does.
	inline uint16_t Blend15(uint16_t a, uint16_t b, int eva, int evb)
	{
		int r = (((a & 0x1F) * eva) + ((b & 0x1F) * evb)) >> 4;
		int g = ((((a >> 5) & 0x1F) * eva) + (((b >> 5) & 0x1F) * evb)) >> 4;
		int bl = ((((a >> 10) & 0x1F) * eva) + (((b >> 10) & 0x1F) * evb)) >> 4;
		if (r > 31) r = 31;
		if (g > 31) g = 31;
		if (bl > 31) bl = 31;
		return (uint16_t)(r | (g << 5) | (bl << 10));
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
		uint8_t* storage = nullptr;
		uint32_t size = 0;
		uint32_t mask = 0;			// size-1 when the size is a power of two, otherwise 0
		bool powerOfTwo = true;

	public:
		MemoryBank() = default;

		void Init(uint32_t bytes);
		void Free();

		uint8_t* Data() { return storage; }
		const uint8_t* Data() const { return storage; }
		uint32_t Size() const { return size; }

		/// <summary>
		/// Fold an address into the bank. Every region of the GBA is mirrored through its size;
		/// where the size is not a power of two (VRAM is 96 KByte) the hardware wraps with a
		/// modulo instead, which is what this does.
		/// </summary>
		uint32_t Mirror(uint32_t offset) const { return powerOfTwo ? (offset & mask) : (offset % (size ? size : 1)); }

		uint8_t Read8(uint32_t offset) const { return storage[Mirror(offset)]; }
		uint16_t Read16(uint32_t offset) const { uint16_t v; memcpy(&v, storage + Mirror(offset), 2); return v; }
		uint32_t Read32(uint32_t offset) const { uint32_t v; memcpy(&v, storage + Mirror(offset), 4); return v; }

		/// <summary>Write through the mirroring rule.</summary>
		void Write8(uint32_t offset, uint8_t value) { storage[Mirror(offset)] = value; }
		void Write16(uint32_t offset, uint16_t value) { memcpy(storage + Mirror(offset), &value, 2); }
		void Write32(uint32_t offset, uint32_t value) { memcpy(storage + Mirror(offset), &value, 4); }

		void Fill(uint8_t value) { if (storage) memset(storage, value, size); }
	};
}
