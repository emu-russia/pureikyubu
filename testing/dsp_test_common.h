// Common helpers for the DSP core unit tests.
//
// The tests drive the DSP core instruction by instruction through the public API of
// DspCore / Dsp16 (registers, Step(), TranslateIMem / TranslateDMem, ReadDMem / WriteDMem),
// exactly the way the debugger does. No emulator internal is poked around.

#pragma once

#include "pch.h"

// ----------------------------------------------------------------------
// Test environment (implemented in dsp_test_support.cpp)
// ----------------------------------------------------------------------

/// <summary>Pointer into the simulated console main memory (nullptr on overflow).</summary>
uint8_t* DspTestMainMemory(uint32_t physAddr, size_t size);
uint8_t* DspTestMainMemoryBase();

/// <summary>Allocate the ARAM SDRAM image (the emulator does this in AROpen).</summary>
void DspTestInitAram();

void DspTestSetGekkoTicks(int64_t ticks);
int64_t DspTestGetGekkoTicks();

/// <summary>Debug::Report capture (see dsp_test_support.cpp).</summary>
void DspTestLogClear();
void DspTestLogEnable(bool enable);
std::string DspTestLogText();

/// <summary>Debug::Halt capture: the tests assert that no Halt happened, or on its text.</summary>
std::string DspTestLastHalt();
int DspTestHaltCount();

namespace Flipper
{
	extern uint32_t TestPIAssertedInts;
}

namespace DspUnitTest
{
	using namespace DSP;
	using namespace Microsoft::VisualStudio::CppUnitTestFramework;

	// ------------------------------------------------------------------
	// Memory / program helpers
	// ------------------------------------------------------------------

	/// <summary>
	/// Write a 16-bit word into DSP program memory (IRAM at 0x0000-0x0FFF or IROM at 0x8000-0x8FFF).
	/// </summary>
	inline void PokeIMem(DspCore* core, DspAddress addr, uint16_t word)
	{
		uint8_t* ptr = core->TranslateIMem(addr);
		if (ptr == nullptr)
		{
			Assert::Fail(L"PokeIMem: address is not translatable");
		}
		*(uint16_t*)ptr = _BYTESWAP_UINT16(word);
	}

	/// <summary>
	/// Read a 16-bit word from DSP program memory.
	/// </summary>
	inline uint16_t PeekIMem(DspCore* core, DspAddress addr)
	{
		return core->ReadIMem(addr);
	}

	/// <summary>
	/// Place a sequence of instruction words into IRAM starting at `addr`.
	/// </summary>
	inline void Assemble(DspCore* core, DspAddress addr, const std::vector<uint16_t>& words)
	{
		for (size_t i = 0; i < words.size(); i++)
		{
			PokeIMem(core, addr + (DspAddress)i, words[i]);
		}
	}

	// ------------------------------------------------------------------
	// Register helpers
	// ------------------------------------------------------------------

	/// <summary>
	/// Dump of the arithmetic PSR flags, as defined by dsp-isa.md 4.13.
	/// </summary>
	struct Flags
	{
		int c = 0;
		int v = 0;
		int z = 0;
		int n = 0;
		int e = 0;
		int u = 0;
		int sv = 0;
		int tb = 0;

		bool operator==(const Flags& o) const
		{
			return c == o.c && v == o.v && z == o.z && n == o.n && e == o.e && u == o.u;
		}
	};

	inline Flags GetFlags(DspCore* core)
	{
		Flags f;
		f.c = core->regs.psr.c;
		f.v = core->regs.psr.v;
		f.z = core->regs.psr.z;
		f.n = core->regs.psr.n;
		f.e = core->regs.psr.e;
		f.u = core->regs.psr.u;
		f.sv = core->regs.psr.sv;
		f.tb = core->regs.psr.tb;
		return f;
	}

	inline std::wstring FlagsToString(const Flags& f)
	{
		wchar_t buf[64];
		swprintf_s(buf, _countof(buf), L"C=%d V=%d Z=%d N=%d E=%d U=%d", f.c, f.v, f.z, f.n, f.e, f.u);
		return buf;
	}

	/// <summary>
	/// Reference implementation of the flag rules of dsp-isa.md 4.13.
	/// The rules are recomputed here straight from the specification text (independently of
	/// DspCore::ModifyFlags), so a mismatch points at the emulator rather than at the test.
	/// </summary>
	class FlagRules
	{
	public:
		static int bit(uint64_t n, int b) { return (int)((n >> b) & 1); }

		// Carry rules

		static int C1(uint64_t ds, uint64_t s, uint64_t dd)
		{
			return (bit(ds, 39) & bit(s, 39)) | ((1 - bit(dd, 39)) & (bit(ds, 39) | bit(s, 39)));
		}

		static int C2(uint64_t ds, uint64_t s, uint64_t dd)
		{
			int ns = 1 - bit(s, 39);
			return (bit(ds, 39) & ns) | ((1 - bit(dd, 39)) & (bit(ds, 39) | ns));
		}

		/// <summary>
		/// div step: bit 15 of the divisor decides whether the divisor is added or subtracted.
		/// </summary>
		static int C3(uint64_t ds, uint64_t s, uint64_t dd)
		{
			if ((bit(ds, 39) ^ bit(s, 15)) != 0)
			{
				return (bit(ds, 39) & bit(s, 15)) | ((1 - bit(dd, 39)) & (bit(ds, 39) | bit(s, 15)));
			}
			int n15 = 1 - bit(s, 15);
			return (bit(ds, 39) & n15) | ((1 - bit(dd, 39)) & (bit(ds, 39) | n15));
		}

		static int C4(uint64_t ds, uint64_t dd) { return (1 - bit(ds, 39)) & (1 - bit(dd, 39)); }
		static int C6(uint64_t ds, uint64_t dd) { return bit(ds, 39) & (1 - bit(dd, 39)); }
		static int C7(uint64_t p, uint64_t d) { return bit(p, 39) & (1 - bit(d, 39)); }
		static int C8(uint64_t p, uint64_t s, uint64_t d)
		{
			return (bit(p, 39) & bit(s, 39)) | ((1 - bit(d, 39)) & (bit(p, 39) | bit(s, 39)));
		}

		// Overflow rules

		static int V1(uint64_t ds, uint64_t s, uint64_t dd)
		{
			return (bit(ds, 39) & bit(s, 39) & (1 - bit(dd, 39))) | ((1 - bit(ds, 39)) & (1 - bit(s, 39)) & bit(dd, 39));
		}

		static int V2(uint64_t ds, uint64_t s, uint64_t dd)
		{
			int ns = 1 - bit(s, 39);
			return (bit(ds, 39) & ns & (1 - bit(dd, 39))) | ((1 - bit(ds, 39)) & bit(s, 39) & bit(dd, 39));
		}

		static int V3(uint64_t ds, uint64_t dd) { return bit(ds, 39) & bit(dd, 39); }
		static int V4(uint64_t ds, uint64_t dd) { return (1 - bit(ds, 39)) & bit(dd, 39); }
		static int V5(uint64_t dd) { return bit(dd, 39); }
		static int V6(uint64_t p, uint64_t d) { return (1 - bit(p, 39)) & bit(d, 39); }
		static int V7(uint64_t p, uint64_t s, uint64_t d)
		{
			return (bit(p, 39) & bit(s, 39) & (1 - bit(d, 39))) | ((1 - bit(p, 39)) & (1 - bit(s, 39)) & bit(d, 39));
		}

		static int V8(uint64_t ds, uint64_t dd) { return bit(ds, 39) & (1 - bit(dd, 39)); }

		// Zero / negative / extension / unnormalization rules

		static int Z1(uint64_t dd) { return (dd & 0xFFFFFFFFFFULL) == 0 ? 1 : 0; }
		static int Z2(uint64_t dd) { return (dd & 0xFFFF0000ULL) == 0 ? 1 : 0; }
		static int Z3(uint64_t dd) { return (dd & 0xFFFFFFFFFFULL) == 0 ? 1 : 0; }
		static int N1(uint64_t dd) { return bit(dd, 39); }
		static int N2(uint64_t dd) { return bit(dd, 31); }

		static int E1(uint64_t dd)
		{
			uint64_t ext = (dd >> 31) & 0x1FF;
			return !(ext == 0 || ext == 0x1FF) ? 1 : 0;
		}

		static int U1(uint64_t dd) { return 1 - (bit(dd, 31) ^ bit(dd, 30)); }
	};

	// ------------------------------------------------------------------
	// Test machine
	// ------------------------------------------------------------------

	/// <summary>
	/// A Dsp16 device with a DspCore inside, plus convenience wrappers used by the tests.
	/// </summary>
	class DspTestMachine
	{
	public:

		Dsp16 dsp;
		DspCore* core = nullptr;

		static const DspAddress IRAM_BASE = 0x0000;
		static const size_t IRAM_BYTES = 8 * 1024;
		static const DspAddress IROM_BASE = 0x8000;
		static const size_t IROM_BYTES = 8 * 1024;
		static const size_t DRAM_BYTES = 8 * 1024;
		static const DspAddress DROM_BASE = 0x1000;
		static const size_t DROM_BYTES = 4 * 1024;

		DspTestMachine()
		{
			core = dsp.core;

			// Hook the device into the Flipper DSP slot, as the emulator does.
			Flipper::DSP = &dsp;
			Flipper::TestPIAssertedInts = 0;

			DspTestInitAram();
			DspTestLogClear();

			Reset();
		}

		/// <summary>
		/// Hard-reset the core and wipe all four memories, so that every test starts from a known state.
		/// </summary>
		void Reset()
		{
			core->HardReset();

			memset(core->TranslateIMem(IRAM_BASE), 0, IRAM_BYTES);
			memset(core->TranslateIMem(IROM_BASE), 0, IROM_BYTES);
			memset(core->TranslateDMem(0), 0, DRAM_BYTES);
			memset(core->TranslateDMem(DROM_BASE), 0, DROM_BYTES);

			// The core is usually tested from IRAM address 0.
			core->regs.pc = IRAM_BASE;

			// Fresh capture for every test.
			DspTestLogClear();
			Flipper::TestPIAssertedInts = 0;
			DspTestSetGekkoTicks(0);
		}

		/// <summary>
		/// Load the contents of a file (raw byte image) into IRAM.
		/// </summary>
		bool LoadIRAM(const std::string& filename)
		{
			auto data = Util::FileLoad(filename);
			if (data.empty() || data.size() > IRAM_BYTES)
			{
				return false;
			}
			memcpy(core->TranslateIMem(IRAM_BASE), data.data(), data.size());
			return true;
		}

		/// <summary>
		/// Load an IROM image (8 KB) into the internal instruction ROM.
		/// </summary>
		bool LoadIROM(const std::wstring& filename)
		{
			auto data = Util::FileLoad(filename);
			std::vector<uint8_t> image(data.begin(), data.end());
			return core->LoadIrom(image);
		}

		/// <summary>
		/// Load a DROM image (4 KB) into the internal data ROM.
		/// </summary>
		bool LoadDROM(const std::wstring& filename)
		{
			auto data = Util::FileLoad(filename);
			std::vector<uint8_t> image(data.begin(), data.end());
			return core->LoadDrom(image);
		}

		// --- execution ---------------------------------------------------

		/// <summary>
		/// Assemble and run a program placed at IRAM address 0.
		/// </summary>
		void Run(const std::vector<uint16_t>& words, int steps)
		{
			Assemble(core, IRAM_BASE, words);
			core->regs.pc = IRAM_BASE;
			for (int i = 0; i < steps; i++)
			{
				Step();
			}
		}

		/// <summary>
		/// Execute a single instruction (as the debugger "dstep" command does).
		/// </summary>
		void Step()
		{
			core->Step();
		}

		/// <summary>
		/// Execute `count` instructions.
		/// </summary>
		void Steps(int count)
		{
			for (int i = 0; i < count; i++)
			{
				core->Step();
			}
		}

		/// <summary>
		/// Decode an instruction (with its following words) and hand the result to the disassembler.
		/// Used to check that the decoder and the disassembler agree on the whole opcode space.
		/// </summary>
		std::string DisasmWord(const std::vector<uint16_t>& words)
		{
			Assemble(core, IRAM_BASE, words);
			DecoderInfo info = { 0 };
			DspAddress pc = IRAM_BASE;
			Decoder::Decode(core->TranslateIMem(pc), DspCore::MaxInstructionSizeInBytes, info);
			return DspDisasm::Disasm(pc, info);
		}

		/// <summary>
		/// Decode an instruction without executing it; returns the decoded size in bytes.
		/// </summary>
		size_t DecodeSize(const std::vector<uint16_t>& words)
		{
			Assemble(core, IRAM_BASE, words);
			DecoderInfo info = { 0 };
			Decoder::Decode(core->TranslateIMem(IRAM_BASE), DspCore::MaxInstructionSizeInBytes, info);
			return info.sizeInBytes;
		}

		// --- memory ------------------------------------------------------

		uint16_t DMem(DspAddress addr) { return dsp.ReadDMem(addr); }
		void DMem(DspAddress addr, uint16_t value) { dsp.WriteDMem(addr, value); }

		/// <summary>Shorthand for the DSP side memory/data-port access.</summary>
		uint16_t Dsp(DspAddress addr) { return dsp.ReadDMem(addr); }
		void Dsp(DspAddress addr, uint16_t value) { dsp.WriteDMem(addr, value); }

		// --- accumulators ------------------------------------------------

		int64_t A() { return DspCore::SignExtend40((int64_t)core->regs.a.bits); }
		int64_t B() { return DspCore::SignExtend40((int64_t)core->regs.b.bits); }

		void SetA(int64_t value) { core->regs.a.bits = (uint64_t)value & 0xFF'FFFFFFFFULL; }
		void SetB(int64_t value) { core->regs.b.bits = (uint64_t)value & 0xFF'FFFFFFFFULL; }

		/// <summary>
		/// Fold the product register pair into the true 40-bit product:
		/// true product == (ps2 &lt;&lt; 32 | ps1 &lt;&lt; 16 | ps0) + (pc1 &lt;&lt; 16), per dsp.md 2.10.
		/// </summary>
		int64_t Product()
		{
			uint64_t pso = ((uint64_t)core->regs.prod.h << 32) | ((uint64_t)core->regs.prod.m1 << 16) | core->regs.prod.l;
			uint64_t pco = (uint64_t)core->regs.prod.m2 << 16;
			return DspCore::SignExtend40((int64_t)((pso + pco) & 0xFF'FFFFFFFFULL));
		}

		void SetX(uint32_t value) { core->regs.x.bits = value; }
		void SetY(uint32_t value) { core->regs.y.bits = value; }

		/// <summary>
		/// Preload the flags with a known pattern, so that "flags left untouched" cases are visible.
		/// </summary>
		void SetFlags(int c, int v, int z, int n, int e, int u)
		{
			core->regs.psr.c = c;
			core->regs.psr.v = v;
			core->regs.psr.z = z;
			core->regs.psr.n = n;
			core->regs.psr.e = e;
			core->regs.psr.u = u;
		}

		/// <summary>
		/// A pattern which is unlikely to be produced by an instruction by accident.
		/// </summary>
		void SetDefaultFlags() { SetFlags(1, 0, 0, 1, 1, 0); }

		Flags PsrFlags() { return GetFlags(core); }

		// --- assertion helpers -------------------------------------------

		void AssertA(int64_t expected, const wchar_t* message = L"accumulator a")
		{
			Assert::AreEqual(expected, A(), message);
		}

		void AssertB(int64_t expected, const wchar_t* message = L"accumulator b")
		{
			Assert::AreEqual(expected, B(), message);
		}

		void AssertFlags(int c, int v, int z, int n, int e, int u, const wchar_t* message = L"PSR flags")
		{
			Flags actual = PsrFlags();
			wchar_t buf[256];
			std::wstring text = FlagsToString(actual);
			swprintf_s(buf, _countof(buf), L"%s: expected C=%d V=%d Z=%d N=%d E=%d U=%d, got %s",
				message, c, v, z, n, e, u, text.c_str());
			Assert::IsTrue(actual.c == c && actual.v == v && actual.z == z && actual.n == n && actual.e == e && actual.u == u, buf);
		}
	};

	/// <summary>
	/// Locate a repository data file (e.g. "build/Data/dsp_irom.bin") by walking up from the
	/// directory the test host runs in (the test DLL lives deep inside scripts/VS2026).
	/// </summary>
	inline bool FindRepoFile(const std::string& relative, std::string& out)
	{
		char cwd[MAX_PATH] = { 0 };
		if (GetCurrentDirectoryA(sizeof(cwd), cwd) == 0)
		{
			return false;
		}

		std::string dir = cwd;

		for (int depth = 0; depth < 10; depth++)
		{
			std::string candidate = dir + "\\" + relative;
			for (auto& c : candidate)
			{
				if (c == '/') c = '\\';
			}
			if (Util::FileExists(candidate))
			{
				out = candidate;
				return true;
			}

			size_t pos = dir.find_last_of("\\/");
			if (pos == std::string::npos || pos < 3)
			{
				break;
			}
			dir = dir.substr(0, pos);
		}

		return false;
	}

	/// <summary>
	/// The single DSP test machine shared by every test class.
	/// It is intentionally never destroyed: the DSP device owns an emulator thread, and a
	/// dangling static at DLL unload is far more trouble than one leaked thread at exit.
	/// </summary>
	inline DspTestMachine& Machine()
	{
		static DspTestMachine* instance = nullptr;
		if (instance == nullptr)
		{
			instance = new DspTestMachine();
		}
		return *instance;
	}

	// ------------------------------------------------------------------
	// Instruction encoders (dsp-isa.md 3 and 6)
	//
	// These reproduce the encoding tables of the ISA specification; the tests use them to
	// build instruction words, which makes them an independent check of the decoder.
	// ------------------------------------------------------------------

	// Register file codes (dsp-isa.md 3.1)

	enum Reg32
	{
		REG_R0 = 0, REG_R1, REG_R2, REG_R3,
		REG_M0 = 4, REG_M1, REG_M2, REG_M3,
		REG_L0 = 8, REG_L1, REG_L2, REG_L3,
		REG_PCS = 12, REG_PSS, REG_EAS, REG_LCS,
		REG_A2 = 16, REG_B2, REG_DPP, REG_PSR,
		REG_PS0 = 20, REG_PS1, REG_PS2, REG_PC1,
		REG_X0 = 24, REG_Y0, REG_X1, REG_Y1,
		REG_A0 = 28, REG_B0, REG_A1 = 30, REG_B1,
	};

	// Small operand fields (dsp-isa.md 3.2)

	enum Reg8Ab						// ldsa, mvsi, packed ld
	{
		R8A_X0 = 0, R8A_Y0 = 1, R8A_X1 = 2, R8A_Y1 = 3, R8A_A0 = 4, R8A_B0 = 5, R8A_A = 6, R8A_B = 7,
	};

	enum Reg8P						// add/sub/amv sources
	{
		R8P_X0 = 0, R8P_Y0 = 1, R8P_X1 = 2, R8P_Y1 = 3, R8P_X = 4, R8P_Y = 5, R8P_AB = 6, R8P_PROD = 7,
	};

	enum Reg4Xy						// div/max source, packed mv/ls data
	{
		R4XY_X0 = 0, R4XY_Y0 = 1, R4XY_X1 = 2, R4XY_Y1 = 3,
	};

	enum Reg4Ab0					// packed st/mv sources
	{
		R4AB0_A0 = 0, R4AB0_B0 = 1, R4AB0_A = 2, R4AB0_B = 3,
	};

	// Modifiers (mn), 2-bit field used by the move/modeless forms

	enum Mod
	{
		MOD_NONE = 0, MOD_DEC = 1, MOD_INC = 2, MOD_PLUS_M = 3,
	};

	// 3-bit modifier field of the standalone `mr rn,mn` form (dsp-isa.md section 4.4)

	enum MrMod
	{
		MRM_PLUS_0 = 0, MRM_MINUS_1 = 1, MRM_PLUS_1 = 2, MRM_MINUS_M = 3,
		MRM_PLUS_M0 = 4, MRM_PLUS_M1 = 5, MRM_PLUS_M2 = 6, MRM_PLUS_M3 = 7,
	};

	// stsa source register codes

	enum StsaReg
	{
		STSA_A2 = 0, STSA_B2 = 1, STSA_A0 = 4, STSA_B0 = 5, STSA_A = 6, STSA_B = 7,
	};

	enum Cc
	{
		CC_GE = 0, CC_LT = 1, CC_GT = 2, CC_LE = 3,
		CC_NZ = 4, CC_Z = 5, CC_NC = 6, CC_C = 7,
		CC_NE = 8, CC_E = 9, CC_NM = 10, CC_M = 11,
		CC_NT = 12, CC_T = 13, CC_V = 14, CC_ALWAYS = 15,
	};

	/// <summary>
	/// Instruction word builders. For the two-word forms the caller appends the second word
	/// through Assemble(). The optional `mem` argument is the low half of a packed word.
	/// </summary>
	class Enc
	{
	public:
		// --- control transfer --------------------------------------------

		// The encodings below follow the operand encodings of dsp-isa.md section 3 and the
		// opcode spaces of section 6. They are written out by hand, independently of
		// src/dspdec.cpp, so that they also serve as a check of the decoder.

		static uint16_t Jmp(Cc cc = CC_ALWAYS) { return 0x0290 | (uint16_t)cc; }		// + target word
		static uint16_t Call(Cc cc = CC_ALWAYS) { return 0x02B0 | (uint16_t)cc; }		// + target word
		static uint16_t Rets(Cc cc = CC_ALWAYS) { return 0x02D0 | (uint16_t)cc; }
		static uint16_t Reti(Cc cc = CC_ALWAYS) { return 0x02F0 | (uint16_t)cc; }
		static uint16_t Exec(Cc cc = CC_ALWAYS) { return 0x0270 | (uint16_t)cc; }
		static uint16_t Trap() { return 0x0020; }
		static uint16_t Wait() { return 0x0021; }
		static uint16_t JmpReg(uint16_t rn, Cc cc = CC_ALWAYS) { return 0x1700 | ((rn & 3) << 5) | (uint16_t)cc; }
		static uint16_t CallReg(uint16_t rn, Cc cc = CC_ALWAYS) { return 0x1710 | ((rn & 3) << 5) | (uint16_t)cc; }

		// --- loop / repeat -----------------------------------------------

		static uint16_t Loop(uint8_t lc) { return 0x1100 | lc; }						// + end address word
		static uint16_t LoopReg(uint16_t reg32) { return 0x0060 | (reg32 & 0x1f); }
		static uint16_t Rep(uint8_t rc) { return 0x1000 | rc; }
		static uint16_t RepReg(uint16_t reg32) { return 0x0040 | (reg32 & 0x1f); }

		// --- misc single-word --------------------------------------------

		static uint16_t Nop() { return 0x0000; }
		static uint16_t Pld(uint16_t d_ab, uint16_t rn, uint16_t mn) { return 0x0210 | ((d_ab & 1) << 8) | ((mn & 3) << 2) | (rn & 3); }
		static uint16_t Mr(uint16_t rn, uint16_t mn3) { return ((mn3 & 7) << 2) | (rn & 3); }
		static uint16_t ClrPsr(uint16_t bit) { return 0x1200 | (bit & 7); }
		static uint16_t SetPsr(uint16_t bit) { return 0x1300 | (bit & 7); }
		static uint16_t ClrIm() { return 0x8A00; }
		static uint16_t SetIm() { return 0x8B00; }
		static uint16_t ClrDp() { return 0x8C00; }
		static uint16_t SetDp() { return 0x8D00; }
		static uint16_t ClrXl() { return 0x8E00; }
		static uint16_t SetXl() { return 0x8F00; }

		// --- immediate ALU ------------------------------------------------

		static uint16_t Adsi(uint16_t d_ab, int8_t si) { return 0x0400 | ((d_ab & 1) << 8) | (uint8_t)si; }
		static uint16_t Cmpsi(uint16_t s_ab, int8_t si) { return 0x0600 | ((s_ab & 1) << 8) | (uint8_t)si; }
		static uint16_t Lsfi(uint16_t d_ab, int8_t si) { return 0x1400 | ((d_ab & 1) << 8) | ((uint8_t)si & 0x7f); }
		static uint16_t Asfi(uint16_t d_ab, int8_t si) { return 0x1480 | ((d_ab & 1) << 8) | ((uint8_t)si & 0x7f); }
		static uint16_t Adli(uint16_t d_ab) { return 0x0200 | ((d_ab & 1) << 8); }		// + immediate word
		static uint16_t Cmpli(uint16_t s_ab) { return 0x0280 | ((s_ab & 1) << 8); }
		static uint16_t Xorli(uint16_t d_a1b1) { return 0x0220 | ((d_a1b1 & 1) << 8); }
		static uint16_t Anli(uint16_t d_a1b1) { return 0x0240 | ((d_a1b1 & 1) << 8); }
		static uint16_t Orli(uint16_t d_a1b1) { return 0x0260 | ((d_a1b1 & 1) << 8); }

		// --- step / accumulator / logic -----------------------------------

		static uint16_t Norm(uint16_t d_ab, uint16_t rn) { return 0x0204 | ((d_ab & 1) << 8) | (rn & 3); }
		static uint16_t Negc(uint16_t d_ab) { return 0x020D | ((d_ab & 1) << 8); }
		static uint16_t Div(uint16_t d_ab, uint16_t s_xy) { return 0x0208 | ((d_ab & 1) << 8) | ((s_xy & 3) << 5); }
		static uint16_t Max(uint16_t d_ab, uint16_t s_xy) { return 0x0209 | ((d_ab & 1) << 8) | ((s_xy & 3) << 5); }
		// lsf/asf exist in two flavours and the shift behaves differently in each, so they
		// are spelled out separately:
		//
		//   * the SINGLE (non-packable) negated forms 0x024A/0x024B (-x1), 0x026A/0x026B (-y1)
		//     and 0x02CA/0x02CB (-b1 when d is a, -a1 when d is b) shift by the *negated*
		//     source value;
		//   * the ALU1 parallel forms 0x3480/0x3680 (lsf d,x1/y1) and 0x3880/0x3A80
		//     (asf d,x1/y1), plus 0x3C80/0x3E80 for the other accumulator half.
		static uint16_t LsfNeg(uint16_t d_ab, uint16_t s_xy) { return (s_xy == 0 ? 0x024A : 0x026A) | ((d_ab & 1) << 8); }
		static uint16_t AsfNeg(uint16_t d_ab, uint16_t s_xy) { return (s_xy == 0 ? 0x024B : 0x026B) | ((d_ab & 1) << 8); }
		static uint16_t LsfNegOther(uint16_t d_ab) { return 0x02CA | ((d_ab & 1) << 8); }
		static uint16_t AsfNegOther(uint16_t d_ab) { return 0x02CB | ((d_ab & 1) << 8); }
		static uint16_t Addc(uint16_t d_ab, uint16_t s_xy) { return 0x028C | ((d_ab & 1) << 8) | ((s_xy & 1) << 5); }
		static uint16_t Subc(uint16_t d_ab, uint16_t s_xy) { return 0x028D | ((d_ab & 1) << 8) | ((s_xy & 1) << 5); }

		static uint16_t Add(uint16_t d_ab, uint16_t s_reg8p, uint16_t mem = 0) { return 0x4000 | ((s_reg8p & 7) << 9) | ((d_ab & 1) << 8) | mem; }
		static uint16_t Sub(uint16_t d_ab, uint16_t s_reg8p, uint16_t mem = 0) { return 0x5000 | ((s_reg8p & 7) << 9) | ((d_ab & 1) << 8) | mem; }
		static uint16_t Amv(uint16_t d_ab, uint16_t s_reg8p, uint16_t mem = 0) { return 0x6000 | ((s_reg8p & 7) << 9) | ((d_ab & 1) << 8) | mem; }
		static uint16_t Addl(uint16_t d_ab, uint16_t s_xy, uint16_t mem = 0) { return 0x7000 | ((s_xy & 1) << 9) | ((d_ab & 1) << 8) | mem; }
		static uint16_t Inc(uint16_t d, uint16_t mem = 0) { return 0x7400 | ((d & 3) << 8) | mem; }
		static uint16_t Dec(uint16_t d, uint16_t mem = 0) { return 0x7800 | ((d & 3) << 8) | mem; }
		static uint16_t Neg(uint16_t d_ab, uint16_t mem = 0) { return 0x7C00 | ((d_ab & 1) << 8) | mem; }
		static uint16_t NegP(uint16_t d_ab, uint16_t mem = 0) { return 0x7E00 | ((d_ab & 1) << 8) | mem; }
		static uint16_t ClrAcc(uint16_t d_ab, uint16_t mem = 0) { return 0x8100 | ((d_ab & 1) << 11) | mem; }
		static uint16_t ClrP(uint16_t mem = 0) { return 0x8400 | mem; }
		static uint16_t TstP(uint16_t mem = 0) { return 0x8500 | mem; }
		static uint16_t TstXY(uint16_t s_xy, uint16_t mem = 0) { return 0x8600 | ((s_xy & 1) << 8) | mem; }
		static uint16_t CmpAB(uint16_t mem = 0) { return 0x8200 | mem; }
		static uint16_t CmpDX(uint16_t d_ab, uint16_t s_xy, uint16_t mem = 0) { return 0xC100 | ((s_xy & 1) << 12) | ((d_ab & 1) << 11) | mem; }
		static uint16_t MpyX1X1(uint16_t mem = 0) { return 0x8300 | mem; }
		static uint16_t Asr16(uint16_t d_ab, uint16_t mem = 0) { return 0x9100 | ((d_ab & 1) << 11) | mem; }
		static uint16_t Abs(uint16_t d_ab, uint16_t mem = 0) { return 0xA100 | ((d_ab & 1) << 11) | mem; }
		static uint16_t TstAB(uint16_t d_ab, uint16_t mem = 0) { return 0xB100 | ((d_ab & 1) << 11) | mem; }
		static uint16_t Lsl16(uint16_t d_ab, uint16_t mem = 0) { return 0xF000 | ((d_ab & 1) << 8) | mem; }
		static uint16_t Lsr16(uint16_t d_ab, uint16_t mem = 0) { return 0xF400 | ((d_ab & 1) << 8) | mem; }
		static uint16_t Addp(uint16_t d_ab, uint16_t s_xy, uint16_t mem = 0) { return 0xF800 | ((s_xy & 1) << 9) | ((d_ab & 1) << 8) | mem; }
		static uint16_t Rnd(uint16_t d_ab, uint16_t mem = 0) { return 0xFC00 | ((d_ab & 1) << 8) | mem; }
		static uint16_t Rndp(uint16_t d_ab, uint16_t mem = 0) { return 0xFE00 | ((d_ab & 1) << 8) | mem; }

		// --- logic (type 1 / type 2) ---------------------------------------

		// 0011 00sd 0xxx xxxx : xor d, x1/y1
		static uint16_t XorXY(uint16_t d_a1b1, uint16_t s_xy, uint16_t mem = 0) { return 0x3000 | ((s_xy & 1) << 9) | ((d_a1b1 & 1) << 8) | mem; }
		// 0011 000d 1xxx xxxx : xor d, other accumulator half
		static uint16_t XorOther(uint16_t d_a1b1, uint16_t mem = 0) { return 0x3080 | ((d_a1b1 & 1) << 8) | mem; }
		// 0011 001d 1xxx xxxx : not d
		static uint16_t Not(uint16_t d_a1b1, uint16_t mem = 0) { return 0x3280 | ((d_a1b1 & 1) << 8) | mem; }
		// 0011 01sd 0xxx xxxx : and d, x1/y1
		static uint16_t AndXY(uint16_t d_a1b1, uint16_t s_xy, uint16_t mem = 0) { return 0x3400 | ((s_xy & 1) << 9) | ((d_a1b1 & 1) << 8) | mem; }
		// 0011 01sd 1xxx xxxx : lsf d, x1/y1
		static uint16_t LsfXY(uint16_t d_ab, uint16_t s_xy, uint16_t mem = 0) { return 0x3480 | ((s_xy & 1) << 9) | ((d_ab & 1) << 8) | mem; }
		// 0011 10sd 0xxx xxxx : or d, x1/y1
		static uint16_t OrXY(uint16_t d_a1b1, uint16_t s_xy, uint16_t mem = 0) { return 0x3800 | ((s_xy & 1) << 9) | ((d_a1b1 & 1) << 8) | mem; }
		// 0011 10sd 1xxx xxxx : asf d, x1/y1
		static uint16_t AsfXY(uint16_t d_ab, uint16_t s_xy, uint16_t mem = 0) { return 0x3880 | ((s_xy & 1) << 9) | ((d_ab & 1) << 8) | mem; }
		// 0011 110d 0xxx xxxx : and d, other half
		static uint16_t AndOther(uint16_t d_a1b1, uint16_t mem = 0) { return 0x3C00 | ((d_a1b1 & 1) << 8) | mem; }
		// 0011 110d 1xxx xxxx : lsf d, other half
		static uint16_t LsfOther2(uint16_t d_ab, uint16_t mem = 0) { return 0x3C80 | ((d_ab & 1) << 8) | mem; }
		// 0011 111d 0xxx xxxx : or d, other half
		static uint16_t OrOther(uint16_t d_a1b1, uint16_t mem = 0) { return 0x3E00 | ((d_a1b1 & 1) << 8) | mem; }
		// 0011 111d 1xxx xxxx : asf d, other half
		static uint16_t AsfOther2(uint16_t d_ab, uint16_t mem = 0) { return 0x3E80 | ((d_ab & 1) << 8) | mem; }

		// --- multiply ------------------------------------------------------

		// 1sss s000 xxxx xxxx : mpy s1,s2
		// 1sss s01d xxxx xxxx : rnmpy d,s1,s2
		// 1sss s10d xxxx xxxx : admpy d,s1,s2
		// 1sss s11d xxxx xxxx : mvmpy d,s1,s2
		static uint16_t Mpy(uint16_t s, uint16_t mem = 0) { return 0x8000 | ((s & 0xf) << 11) | mem; }
		static uint16_t Rnmpy(uint16_t d_ab, uint16_t s, uint16_t mem = 0) { return 0x8200 | ((s & 0xf) << 11) | ((d_ab & 1) << 8) | mem; }
		static uint16_t Admpy(uint16_t d_ab, uint16_t s, uint16_t mem = 0) { return 0x8400 | ((s & 0xf) << 11) | ((d_ab & 1) << 8) | mem; }
		static uint16_t Mvmpy(uint16_t d_ab, uint16_t s, uint16_t mem = 0) { return 0x8600 | ((s & 0xf) << 11) | ((d_ab & 1) << 8) | mem; }

		// mac / macn forms
		static uint16_t Mac(uint16_t ss, uint16_t mem = 0) { return 0xE000 | ((ss & 3) << 8) | mem; }
		static uint16_t Macn(uint16_t ss, uint16_t mem = 0) { return 0xE400 | ((ss & 3) << 8) | mem; }
		static uint16_t Mac2(uint16_t ss, uint16_t mem = 0) { return 0xE800 | ((ss & 3) << 8) | mem; }
		static uint16_t Macn2(uint16_t ss, uint16_t mem = 0) { return 0xEC00 | ((ss & 3) << 8) | mem; }
		static uint16_t MacF1(uint16_t s_xy, uint16_t mem = 0) { return 0xF200 | ((s_xy & 1) << 8) | mem; }
		static uint16_t MacnF1(uint16_t s_xy, uint16_t mem = 0) { return 0xF600 | ((s_xy & 1) << 8) | mem; }

		// --- bit test ------------------------------------------------------

		static uint16_t Btstl(uint16_t d_a1b1) { return 0x02A0 | ((d_a1b1 & 1) << 8); }	// + mask word
		static uint16_t Btsth(uint16_t d_a1b1) { return 0x02C0 | ((d_a1b1 & 1) << 8); }

		// --- data moves ----------------------------------------------------

		static uint16_t Ld(uint16_t d_reg32, uint16_t rn, uint16_t mn) { return 0x1800 | ((mn & 3) << 7) | ((rn & 3) << 5) | (d_reg32 & 0x1f); }
		static uint16_t St(uint16_t rn, uint16_t mn, uint16_t s_reg32) { return 0x1A00 | ((mn & 3) << 7) | ((rn & 3) << 5) | (s_reg32 & 0x1f); }
		static uint16_t Mv(uint16_t d_reg32, uint16_t s_reg32) { return 0x1C00 | ((d_reg32 & 0x1f) << 5) | (s_reg32 & 0x1f); }
		static uint16_t Ldsa(uint16_t d_reg8ab, uint8_t sa) { return 0x2000 | ((d_reg8ab & 7) << 8) | sa; }
		// stsa register codes: a2=0, b2=1, a0=4, b0=5, a=6, b=7 (dsp-isa.md section 3.2)
		static uint16_t Stsa(uint8_t sa, uint16_t s_code) { return 0x2800 | ((s_code & 7) << 8) | sa; }
		static uint16_t Ldla(uint16_t d_reg32) { return 0x00C0 | (d_reg32 & 0x1f); }	// + address word
		static uint16_t Stla(uint16_t s_reg32) { return 0x00E0 | (s_reg32 & 0x1f); }
		static uint16_t Mvsi(uint16_t d_reg8ab, int8_t si) { return 0x0800 | ((d_reg8ab & 7) << 8) | (uint8_t)si; }
		static uint16_t Mvli(uint16_t d_reg32) { return 0x0080 | (d_reg32 & 0x1f); }	// + immediate word
		static uint16_t Stli(uint8_t sa) { return 0x1600 | sa; }						// + immediate word

		// --- parallel move half (low part of a packed word) ------------------

		// |mr rn,mn|    0011 xxxx x000 mmrr|
		static uint16_t PMr(uint16_t rn, uint16_t mn) { return ((mn & 3) << 2) | (rn & 3); }
		// |mv d,s |    0011 xxxx x001 ddss|
		static uint16_t PMv(uint16_t d4xy, uint16_t s4ab0) { return 0x0010 | ((d4xy & 3) << 2) | (s4ab0 & 3); }
		// The 1-bit modifier field of the packed move halves encodes +1 as 0 and +m as 1:
		// `mn2` is that raw field value.
		//
		// |st rn,mn,s| 0011 xxxx x01s smrr|
		static uint16_t PSt(uint16_t rn, uint16_t mn2, uint16_t s4ab0) { return 0x0020 | ((s4ab0 & 3) << 3) | ((mn2 & 1) << 2) | (rn & 3); }
		// |ld d,rn,mn| 0011 xxxx x1dd dmrr|
		static uint16_t PLd(uint16_t d_reg8ab, uint16_t rn, uint16_t mn2) { return 0x0040 | ((d_reg8ab & 7) << 3) | ((mn2 & 1) << 2) | (rn & 3); }
		// |ls d,r0,n r3,m,s| 01xx xxxx 10dd mn0s|
		static uint16_t PLs(uint16_t d4xy, uint16_t mn2_load, uint16_t mn2_store, uint16_t s_ab)
		{
			return 0x0080 | ((d4xy & 3) << 4) | ((mn2_store & 1) << 3) | ((mn2_load & 1) << 2) | (s_ab & 1);
		}
		// |ls2 d,r3,m r0,n,s| 01xx xxxx 10dd mn1s|
		static uint16_t PLs2(uint16_t d4xy, uint16_t n_inc, uint16_t m_inc, uint16_t s_ab)
		{
			return 0x0080 | ((d4xy & 3) << 4) | ((m_inc & 1) << 3) | ((n_inc & 1) << 2) | 2 | (s_ab & 1);
		}
		// |ldd d1,rn,mn d2,r3,m3| 1xxx xxxx 11dd mnrr|
		static uint16_t PLdd(uint16_t d_pair, uint16_t rn, uint16_t mn2_n, uint16_t mn2_m)
		{
			return 0x00C0 | ((d_pair & 3) << 4) | ((mn2_m & 1) << 3) | ((mn2_n & 1) << 2) | (rn & 3);
		}
	};
}
