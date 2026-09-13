// DSPcore recompiler (JIT) tests. See src/dspjit.h for the design.
//
// Everything here is differential: the recompiler must produce exactly the same state as
// the interpreter, instruction for instruction. The interpreter is the reference, so a
// disagreement is a recompiler bug (the interpreter itself is checked against the hardware
// vectors by dsp_golden_alu_test.cpp).
//
// The tests drive the recompiler through the two public entry points the emulator uses:
// `DspCore::RunJitBlock` (one compiled basic block) and, for the block-size sensitive
// cases, `DspCore::SetJitMaxBlockInstrs`. Nothing is poked around behind the core's back.

#include "pch.h"
#include "dsp_test_common.h"
#include "dsp_golden_alu_vectors.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace DspUnitTest
{
	TEST_CLASS(DspJitTest)
	{
		DspTestMachine m;

		static const uint64_t MASK40 = 0x0000'00FF'FFFF'FFFFULL;

		// ------------------------------------------------------------------
		// State capture and comparison
		// ------------------------------------------------------------------

		/// <summary>
		/// Everything the interpreter's own state consists of: the register file (accumulators,
		/// operands, the product, the address/modifier/length registers, dpp, psr and pc), the
		/// four stacks and the retired-instruction counter.
		/// </summary>
		struct CoreState
		{
			uint64_t a = 0, b = 0, prod = 0;
			uint32_t x = 0, y = 0;
			uint16_t psr = 0;
			uint32_t pc = 0;
			int64_t counter = 0;
			uint16_t r[4] = { 0, 0, 0, 0 };
			uint16_t mreg[4] = { 0, 0, 0, 0 };
			uint16_t lreg[4] = { 0, 0, 0, 0 };
			uint16_t dpp = 0;
			std::vector<uint16_t> pcs, pss, eas, lcs;
		};

		static void CaptureStack(DspStack* stack, std::vector<uint16_t>& out)
		{
			out.clear();
			for (int i = 0; i < stack->size(); i++)
			{
				out.push_back(stack->at(i));
			}
		}

		CoreState Capture()
		{
			CoreState s;
			DspRegs& r = m.core->regs;

			s.a = r.a.bits;
			s.b = r.b.bits;
			s.x = r.x.bits;
			s.y = r.y.bits;
			s.prod = r.prod.bitsPacked;
			s.psr = r.psr.bits;
			s.pc = r.pc;
			s.counter = m.core->GetInstructionCounter();
			s.dpp = r.dpp;

			for (int i = 0; i < 4; i++)
			{
				s.r[i] = r.r[i];
				s.mreg[i] = r.m[i];
				s.lreg[i] = r.l[i];
			}

			CaptureStack(r.pcs, s.pcs);
			CaptureStack(r.pss, s.pss);
			CaptureStack(r.eas, s.eas);
			CaptureStack(r.lcs, s.lcs);

			return s;
		}

		/// <summary>
		/// A human readable description of every field that differs (empty when the two states
		/// are identical). Used instead of a single assertion so that the failure message points
		/// at the field, not just at "states differ".
		/// </summary>
		static std::wstring Diff(const CoreState& e, const CoreState& g)
		{
			std::wstring d;
			wchar_t buf[128];

			auto add = [&](const wchar_t* name, unsigned long long a, unsigned long long b)
			{
				if (a != b)
				{
					swprintf_s(buf, L"%ls: %llX vs %llX; ", name, a, b);
					d += buf;
				}
			};

			add(L"a", e.a, g.a);
			add(L"b", e.b, g.b);
			add(L"prod", e.prod, g.prod);
			add(L"x", e.x, g.x);
			add(L"y", e.y, g.y);
			add(L"psr", e.psr, g.psr);
			add(L"pc", e.pc, g.pc);
			add(L"counter", (unsigned long long)e.counter, (unsigned long long)g.counter);
			add(L"dpp", e.dpp, g.dpp);

			for (int i = 0; i < 4; i++)
			{
				wchar_t name[8];
				swprintf_s(name, L"r%d", i); add(name, e.r[i], g.r[i]);
				swprintf_s(name, L"m%d", i); add(name, e.mreg[i], g.mreg[i]);
				swprintf_s(name, L"l%d", i); add(name, e.lreg[i], g.lreg[i]);
			}

			if (e.pcs != g.pcs) d += L"pcs; ";
			if (e.pss != g.pss) d += L"pss; ";
			if (e.eas != g.eas) d += L"eas; ";
			if (e.lcs != g.lcs) d += L"lcs; ";

			return d;
		}

		// ------------------------------------------------------------------
		// Program setup
		// ------------------------------------------------------------------

		/// <summary>
		/// The register state of one golden vector: a clean core with the vector's operands.
		/// </summary>
		void ApplyOperandSet(const DspGolden::OperandSet& s)
		{
			m.Reset();

			m.core->regs.a.bits = s.a;
			m.core->regs.b.bits = s.b;

			m.core->regs.x.h = s.x1;
			m.core->regs.x.l = s.x0;
			m.core->regs.y.h = s.y1;
			m.core->regs.y.l = s.y0;

			m.core->regs.prod.h = (uint8_t)((s.p >> 32) & 0xFF);
			m.core->regs.prod.m1 = (uint16_t)((s.p >> 16) & 0xFFFF);
			m.core->regs.prod.l = (uint16_t)(s.p & 0xFFFF);
			m.core->regs.prod.m2 = 0;

			m.core->regs.psr.bits = 0;
			m.core->regs.pc = 0;

			// The instruction counter is a running total; both engines have to start it from the
			// same value or the comparison below measures the harness, not the core.
			m.core->ResetInstructionCounter();
		}

		static std::vector<uint16_t> VectorWords(const DspGolden::Vector& v)
		{
			if (v.twoWord)
			{
				return { v.word, v.word2 };
			}
			return { v.word };
		}

		/// <summary>
		/// A rich, fixed starting state for the opcode sweep: non-trivial accumulator/operand
		/// values, valid circular-addressing registers and a chosen psr. The same state is
		/// applied before each engine runs one word, so the only variable is the engine.
		/// </summary>
		void ApplySweepState(uint16_t psr)
		{
			m.Reset();

			m.core->regs.a.bits = 0x5A12345678ULL;
			m.core->regs.b.bits = 0xA5EDCBA987ULL;
			m.core->regs.x.h = 0x0005; m.core->regs.x.l = 0xFFFB;
			m.core->regs.y.h = 0x1234; m.core->regs.y.l = 0xEDCC;

			m.core->regs.prod.h = 0x5A;
			m.core->regs.prod.m1 = 0x1234;
			m.core->regs.prod.l = 0x5678;
			m.core->regs.prod.m2 = 0;

			m.core->regs.r[0] = 0x0010;
			m.core->regs.r[1] = 0x0020;
			m.core->regs.r[2] = 0x0030;
			m.core->regs.r[3] = 0x0040;
			m.core->regs.m[0] = 0x0001;
			m.core->regs.m[1] = 0x0002;
			m.core->regs.m[2] = 0xFFFE;
			m.core->regs.m[3] = 0x0004;
			m.core->regs.l[0] = 0x000F;
			m.core->regs.l[1] = 0x001F;
			m.core->regs.l[2] = 0xFFFF;
			m.core->regs.l[3] = 0x003F;

			m.core->regs.dpp = 0x00FF;
			m.core->regs.psr.bits = psr;
			m.core->regs.pc = 0;

			m.core->ResetInstructionCounter();
		}

		// ------------------------------------------------------------------
		// Tests
		// ------------------------------------------------------------------

		TEST_METHOD(Jit_IsAvailable)
		{
			Assert::IsTrue(m.core->GetJit() != nullptr, L"the DSP core must own a recompiler");
			Assert::IsTrue(m.core->GetJit()->IsSupported(), L"the recompiler must be built for this host");
		}

		/// <summary>
		/// The recompiler translates a whole straight-line run, not one word at a time: a block
		/// of nops must retire many instructions in a single RunJitBlock call.
		/// </summary>
		TEST_METHOD(Jit_CompilesWholeBlocks)
		{
			m.core->SetJitMaxBlockInstrs(32);

			std::vector<uint16_t> nops(64, 0x0000);
			Assemble(m.core, 0, nops);
			m.core->regs.pc = 0;

			uint32_t retired = m.core->RunJitBlock();
			Assert::IsTrue(retired >= 8, L"a block of nops must retire more than a couple of words");
			Assert::AreEqual((uint32_t)32, retired, L"the block must stop at the configured word limit");
			Assert::AreEqual((uint32_t)32, m.core->regs.pc, L"32 words must advance the pc by 32");
		}

		/// <summary>
		/// Every word of the hardware golden vector table, run once by the interpreter and once
		/// by the recompiler with a one-word block limit, must leave identical state. That covers
		/// the whole data-path opcode space (immediate and accumulator ALU, logic, shifts,
		/// addc/subc/negc, norm, div, max, the multiply family and the two-word long immediates).
		/// </summary>
		TEST_METHOD(Jit_MatchesInterpreterOnGoldenVectors)
		{
			m.core->SetJitMaxBlockInstrs(1);

			size_t mismatches = 0;
			std::wstring firstMismatch;

			for (size_t i = 0; i < DspGolden::kAluVectorCount; i++)
			{
				const DspGolden::Vector& v = DspGolden::kAluVectors[i];
				const DspGolden::OperandSet& s = DspGolden::kAluOperandSets[v.set % DspGolden::kAluOperandSetCount];
				std::vector<uint16_t> words = VectorWords(v);

				ApplyOperandSet(s);
				m.Run(words, 1);
				CoreState interp = Capture();

				ApplyOperandSet(s);
				Assemble(m.core, 0, words);
				m.core->regs.pc = 0;
				m.core->RunJitBlock();
				CoreState recompiled = Capture();

				std::wstring diff = Diff(interp, recompiled);
				if (!diff.empty())
				{
					mismatches++;
					if (firstMismatch.empty())
					{
						wchar_t buf[256];
						swprintf_s(buf, L"word %04X: ", v.word);
						firstMismatch = buf + diff;
					}
				}
			}

			Assert::IsTrue(mismatches == 0, firstMismatch.c_str());
		}

		/// <summary>
		/// A long deterministic program assembled from the golden words (so it is real data-path
		/// code, not random garbage) must leave the same state on both engines. The recompiler
		/// retires whole blocks, so the interpreter is run for exactly as many instructions as the
		/// recompiler actually retired.
		/// </summary>
		TEST_METHOD(Jit_MatchesInterpreterOnRandomProgram)
		{
			std::vector<uint16_t> program;
			uint32_t seed = 0x1234'5678;

			while (program.size() < 3000)
			{
				seed = seed * 1103515245u + 12345u;
				const DspGolden::Vector& v = DspGolden::kAluVectors[(seed >> 8) % DspGolden::kAluVectorCount];
				std::vector<uint16_t> words = VectorWords(v);
				program.insert(program.end(), words.begin(), words.end());
			}

			const uint32_t wanted = 2000;

			m.core->SetJitMaxBlockInstrs(16);
			m.Reset();
			Assemble(m.core, 0, program);
			m.core->regs.pc = 0;
			m.core->ResetInstructionCounter();

			uint32_t retired = 0;
			uint32_t biggestBlock = 0;
			while (retired < wanted)
			{
				uint32_t count = m.core->RunJitBlock();
				if (count > biggestBlock) biggestBlock = count;
				retired += count;
			}
			Assert::IsTrue(biggestBlock > 1, L"the recompiler must actually run multi-word blocks");
			CoreState recompiled = Capture();

			m.Reset();
			Assemble(m.core, 0, program);
			m.core->regs.pc = 0;
			m.core->ResetInstructionCounter();
			for (uint32_t i = 0; i < retired; i++)
			{
				m.core->Step();
			}
			CoreState interp = Capture();

			std::wstring diff = Diff(interp, recompiled);
			Assert::IsTrue(retired >= wanted, L"the recompiler must retire at least the requested words");
			Assert::IsTrue(diff.empty(), diff.c_str());
		}

		/// <summary>
		/// A block is compiled from the words that are there when it is compiled, so changing a
		/// word must make the recompiler drop it. Nothing here calls InvalidateAll: the block's
		/// own copy of the words is the backstop, and this test pins it down.
		/// </summary>
		TEST_METHOD(Jit_RecompilesWhenTheInstructionStreamChanges)
		{
			m.core->SetJitMaxBlockInstrs(4);

			std::vector<uint16_t> nops(8, 0x0000);
			Assemble(m.core, 0, nops);
			m.core->regs.psr.bits = 0;
			m.core->regs.pc = 0;

			Assert::AreEqual((uint32_t)4, m.core->RunJitBlock(), L"the first block must be the four nops");
			Assert::AreEqual((int)0, (int)m.core->regs.psr.im, L"the nops must not touch im");

			// 0x8B00 is `set im` (the IROM uses it at 0x8009). The block that covers it was just
			// compiled from nops, so running the same address again has to notice the change.
			PokeIMem(m.core, 1, 0x8B00);
			m.core->regs.pc = 0;
			m.core->RunJitBlock();

			Assert::AreEqual((int)1, (int)m.core->regs.psr.im,
				L"the recompiler must recompile a block whose words changed");
		}

		/// <summary>
		/// Turning the recompiler off must fall back to the interpreter (one word per call).
		/// </summary>
		TEST_METHOD(Jit_CanBeSwitchedOff)
		{
			m.core->SetJitMaxBlockInstrs(32);
			m.core->JitEnabled = false;

			std::vector<uint16_t> nops(8, 0x0000);
			Assemble(m.core, 0, nops);
			m.core->regs.pc = 0;

			Assert::AreEqual((uint32_t)1, m.core->RunJitBlock(), L"with the recompiler off, one word is retired");
			Assert::AreEqual((uint32_t)1, m.core->regs.pc, L"the interpreter advanced the pc by one word");

			m.core->JitEnabled = true;
		}

		/// <summary>
		/// A block must not run past an interrupt. The generated code tests the pending flags
		/// after every word and leaves the block as soon as one is set, so the core is back in
		/// DspCore::Update, which runs CheckInterrupts before the next instruction - exactly the
		/// order the interpreter has.
		/// </summary>
		TEST_METHOD(Jit_StopsAtAPendingCpuInterrupt)
		{
			m.core->SetJitMaxBlockInstrs(32);

			std::vector<uint16_t> nops(64, 0x0000);

			// Without a request the whole block retires.
			m.Reset();
			Assemble(m.core, 0, nops);
			m.core->regs.pc = 0;
			m.core->regs.psr.te3 = 1;
			m.core->regs.psr.et = 1;
			m.dsp.ClearCpuIntRequest();
			Assert::AreEqual((uint32_t)32, m.core->RunJitBlock(), L"a clear block must run to its limit");

			// With one, the block has to leave after the first word.
			m.Reset();
			Assemble(m.core, 0, nops);
			m.core->regs.pc = 0;
			m.core->regs.psr.te3 = 1;
			m.core->regs.psr.et = 1;
			m.dsp.SetIntBit(true);
			Assert::AreEqual((uint32_t)1, m.core->RunJitBlock(), L"a pending CPU interrupt must stop the block");

			// ... and the interpreter, driven the way Update is, then takes it.
			for (int i = 0; i < 4; i++)
			{
				m.core->Step();
			}
			Assert::IsTrue(m.core->regs.pcs->size() > 0, L"the interrupt must have been delivered");
			m.dsp.SetIntBit(false);
		}

		/// <summary>
		/// Every one of the 65536 possible instruction words, run once by each engine from the
		/// same state, must leave identical state. This is the whole opcode space, not just the
		/// data path: control transfers, the stack instructions, memory ops, the undefined words
		/// and the ones that trap. Two psr settings are swept so that the conditional forms are
		/// taken both ways.
		/// </summary>
		TEST_METHOD(Jit_MatchesInterpreterOnEveryOpcode)
		{
			m.core->SetJitMaxBlockInstrs(1);

			size_t mismatches = 0;
			std::wstring firstMismatch;
			const uint16_t psrValues[2] = { 0x0000, 0x3FFF };

			// The full sweep is quick, but the range can be narrowed from the environment
			// (DSP_SWEEP_FROM / DSP_SWEEP_TO) when a failure has to be bisected to one word.
			uint32_t rangeFrom = 0;
			uint32_t rangeTo = 0xFFFF;
			if (const char* r = getenv("DSP_SWEEP_FROM")) rangeFrom = (uint32_t)strtoul(r, nullptr, 0);
			if (const char* r = getenv("DSP_SWEEP_TO")) rangeTo = (uint32_t)strtoul(r, nullptr, 0);

			for (int phase = 0; phase < 2; phase++)
			{
				uint16_t psr = psrValues[phase];

				for (uint32_t word = rangeFrom; word <= rangeTo; word++)
				{
					if ((word & 0x0FFF) == 0)
					{
						DspTestLogClear();
					}

					ApplySweepState(psr);
					Assemble(m.core, 0, { (uint16_t)word });
					m.core->Step();
					CoreState interp = Capture();

					ApplySweepState(psr);
					Assemble(m.core, 0, { (uint16_t)word });
					m.core->RunJitBlock();
					CoreState recompiled = Capture();

					std::wstring diff = Diff(interp, recompiled);
					if (!diff.empty())
					{
						mismatches++;
						if (firstMismatch.empty())
						{
							wchar_t buf[512];
							swprintf_s(buf, L"word %04X psr %04X: ", word, psr);
							firstMismatch = buf + diff;
							DspTestLogEnable(true);
						}
					}
				}
			}

			Assert::IsTrue(mismatches == 0, firstMismatch.c_str());
		}

		/// <summary>
		/// A real interrupt round trip: the CPU->DSP request latches, CheckInterrupts pushes pc/pss
		/// and vectors to 0x000E, the handler there ends in `reti`, and the core returns. Both
		/// engines must retire the same number of instructions to reach the same state - which is
		/// what fails if a whole block runs while the interrupt is in its pending-delay window.
		/// </summary>
		TEST_METHOD(Jit_MatchesInterpreterOnCpuInterrupt)
		{
			// 0x000E is the CpuInt vector (DspInterrupt::CpuInt * 2), and 0x02FF is `reti`
			// (0x02DF is the `rets` the IROM ends with). The rest is nops, entered at 0x0010.
			// The program array is indexed by halfword address, so the vector word goes at 0x000E.
			std::vector<uint16_t> program(64, 0x0000);
			program[0x000E] = 0x02FF;

			const uint32_t wanted = 300;

			m.core->SetJitMaxBlockInstrs(16);
			m.Reset();
			Assemble(m.core, 0, program);
			m.core->regs.psr.te3 = 1;
			m.core->regs.psr.et = 1;
			m.core->regs.pc = 0x0010;
			m.core->ResetInstructionCounter();
			m.dsp.ClearCpuIntRequest();
			m.dsp.SetIntBit(true);

			uint32_t retired = 0;
			while (retired < wanted)
			{
				retired += m.core->RunJitBlock();
			}
			CoreState recompiled = Capture();

			m.Reset();
			Assemble(m.core, 0, program);
			m.core->regs.psr.te3 = 1;
			m.core->regs.psr.et = 1;
			m.core->regs.pc = 0x0010;
			m.core->ResetInstructionCounter();
			m.dsp.ClearCpuIntRequest();
			m.dsp.SetIntBit(true);

			for (uint32_t i = 0; i < retired; i++)
			{
				m.core->Step();
			}
			CoreState interp = Capture();

			std::wstring diff = Diff(interp, recompiled);
			Assert::IsTrue(retired >= wanted, L"the recompiler must retire at least the requested words");
			Assert::IsTrue(diff.empty(), diff.c_str());

			// The interrupt must have been taken and returned from: nothing left on pcs and the
			// status restored by `reti`.
			Assert::AreEqual((size_t)0, recompiled.pcs.size(), L"the reti must pop the interrupt's return address");
			Assert::AreEqual((int)1, (int)m.core->regs.psr.et, L"reti must restore the interrupt enable");
		}

		/// <summary>
		/// The ucode upload path: a running block triggers a mem->IMEM DSP-DMA, and the DMA
		/// rewrites instruction words *ahead* of the block's pc. The interpreter re-fetches every
		/// word, so it executes the new code; a compiled block holds the words it was built from,
		/// so it must notice the code change and leave.
		///
		/// This is the boot sequence (the IROM loader DMAs the microcode into IRAM and jumps into
		/// it), and it is why a block has to be dropped as soon as the instruction stream changes.
		/// </summary>
		TEST_METHOD(Jit_SeesTheCodeADspDmaWrites)
		{
			// IRAM: nop; stli DSBL,4 (2 words); nop; nop; nop
			//                                 ^ 0x0004 is rewritten by the DMA to `set im`.
			// The new code sits in console main memory at 0x00100000.
			const uint32_t hostAddr = 0x00100000;

			auto setup = [&]()
			{
				m.Reset();

				m.core->regs.dpp = 0x00FF;			// stli addresses the 0xFF00 page
				m.core->regs.pc = 0;

				uint8_t* host = DspTestMainMemory(hostAddr, 4);
				Assert::IsNotNull(host, L"main memory must be available for the DMA source");
				host[0] = 0x8B; host[1] = 0x00;		// `set im` (the IROM uses it at 0x8009)
				host[2] = 0x00; host[3] = 0x00;		// nop

				// The block the DMA lands in is word 4, after the stli.
				Assemble(m.core, 0, { 0x0000, 0x16CB, 0x0004, 0x0000, 0x0000, 0x0000 });

				// Memory -> IMEM window, destination word 4, four bytes.
				m.dsp.WriteDMem(0xFFCE, (uint16_t)((hostAddr >> 16) & 0x03FF));	// DSMAH
				m.dsp.WriteDMem(0xFFCF, (uint16_t)(hostAddr & 0xFFFF));			// DSMAL
				m.dsp.WriteDMem(0xFFCD, 4);										// DSPA
				m.dsp.WriteDMem(0xFFC9, 2);										// DSCR: Imem=1, Dsp2Mem=0
				// (DSBL is written by the program itself: that write is what starts the DMA.)

				m.core->ResetInstructionCounter();
			};

			const uint32_t wanted = 6;

			m.core->SetJitMaxBlockInstrs(8);
			setup();
			uint32_t retired = 0;
			while (retired < wanted)
			{
				retired += m.core->RunJitBlock();
			}
			CoreState recompiled = Capture();

			setup();
			for (uint32_t i = 0; i < retired; i++)
			{
				m.core->Step();
			}
			CoreState interp = Capture();

			std::wstring diff = Diff(interp, recompiled);
			Assert::IsTrue(diff.empty(), diff.c_str());
			Assert::AreEqual((int)1, (int)m.core->regs.psr.im,
				L"the code the DMA wrote must have been executed");
		}

		/// <summary>
		/// Every entry point of the real IROM (the reset code, the mailbox handshakes, the four
		/// DSP-DMA blocks, the command dispatcher and the whole DSP self-test), each run for a
		/// fixed number of instructions from the same plausible state and compared. The reset
		/// path alone does not reach the self-test or the DMA blocks, so this is what covers the
		/// code the boot ROM actually spends its time in.
		/// </summary>
		TEST_METHOD(Jit_MatchesInterpreterOnEveryIromEntry)
		{
			std::string path;
			if (!FindRepoFile("build/Data/dsp_irom.bin", path))
			{
				Assert::Fail(L"build/Data/dsp_irom.bin was not found");
			}

			m.core->SetJitMaxBlockInstrs(32);

			const DspAddress first = 0x8000;
			const DspAddress last = 0x88EB;
			const uint32_t wanted = 64;

			size_t mismatches = 0;
			std::wstring firstMismatch;

			auto setup = [&](DspAddress entry)
			{
				m.Reset();
				m.LoadIROM(Util::StringToWstring(path));

				m.core->regs.dpp = 0x00FF;
				for (int i = 0; i < 4; i++)
				{
					m.core->regs.l[i] = 0xFFFF;
				}

				// A return address, so that a `rets` at the end of a routine returns instead of
				// raising the stack-underflow Error.
				m.core->regs.pcs->push(0x800E);
				m.core->regs.pc = entry;
				m.core->ResetInstructionCounter();
			};

			for (DspAddress pc = first; pc < last; pc++)
			{
				if ((pc & 0xFF) == 0)
				{
					DspTestLogClear();
				}

				uint32_t retired = 0;
				setup(pc);
				while (retired < wanted)
				{
					retired += m.core->RunJitBlock();
				}
				CoreState recompiled = Capture();

				setup(pc);
				for (uint32_t i = 0; i < retired; i++)
				{
					m.core->Step();
				}
				CoreState interp = Capture();

				std::wstring diff = Diff(interp, recompiled);
				if (!diff.empty())
				{
					mismatches++;
					if (firstMismatch.empty())
					{
						wchar_t buf[512];
						swprintf_s(buf, L"entry %04X: ", pc);
						firstMismatch = buf + diff;
					}
				}
			}

			Assert::IsTrue(mismatches == 0, firstMismatch.c_str());
		}

		/// <summary>
		/// The interpreter and the recompiler share the DecoderInfo the handlers read. After a
		/// block runs, `DspInterpreter::info` points at that block's *cached* DecoderInfo - so an
		/// interpreted instruction (the fallback the core takes while an interrupt is pending, or
		/// a debugger step) must not decode through it, or it overwrites the block's instructions.
		///
		/// With the boot ROM this corrupted the wait routine's `jmpnt` with the reset vector's
		/// `mvli`, and the DSP left the mailbox loop for an address it had just decoded.
		/// </summary>
		TEST_METHOD(Jit_KeepsItsCachedInstructionsWhenTheInterpreterRuns)
		{
			m.core->SetJitMaxBlockInstrs(2);

			m.Reset();
			m.core->regs.psr.bits = 0;
			m.core->regs.pc = 0;
			Assemble(m.core, 0, { 0x0000, 0x8B00, 0x0000 });	// nop, `set im`, nop

			Assert::AreEqual((uint32_t)2, m.core->RunJitBlock(), L"the block must hold the two words");
			Assert::AreEqual((int)1, (int)m.core->regs.psr.im, L"the block must have executed `set im`");

			// One interpreted instruction decodes into whatever DecoderInfo the interpreter points
			// at; before the fix that was the block's entry for its second word (`set im`).
			m.core->Step();

			m.core->regs.psr.im = 0;
			m.core->regs.pc = 0;
			DspTestLogClear();
			m.core->RunJitBlock();

			Assert::AreEqual(0, DspTestHaltCount(),
				L"the block's handlers must still see their own operands");
			Assert::AreEqual((int)1, (int)m.core->regs.psr.im,
				L"the cached block must still execute its own instructions");
		}

		/// <summary>
		/// The boot-ROM shape of the same bug: the wait routine's block is compiled and then the
		/// core interprets a single instruction (the CPU->DSP request forces the fallback). With
		/// the fix the cached block stays a wait routine and keeps spinning at 0x8078.
		/// </summary>
		TEST_METHOD(Jit_KeepsTheWaitBlockIntactAfterAnInterpretedStep)
		{
			std::string path;
			if (!FindRepoFile("build/Data/dsp_irom.bin", path))
			{
				Assert::Fail(L"build/Data/dsp_irom.bin was not found");
			}

			m.Reset();
			m.LoadIROM(Util::StringToWstring(path));
			m.core->regs.pc = 0x8000;

			// Run into the mailbox wait routine...
			for (int i = 0; i < 200 && m.core->regs.pc != 0x8078; i++)
			{
				m.core->RunJitBlock();
			}
			Assert::AreEqual((uint32_t)0x8078, m.core->regs.pc, L"the boot path must reach the wait routine");

			// ... and run it once, so that its block exists and the interpreter's DecoderInfo
			// points at that block's cache afterwards.
			m.core->RunJitBlock();
			Assert::AreEqual((uint32_t)0x8078, m.core->regs.pc, L"the wait routine must spin here");

			DspTestLogEnable(true);
			DspTestLogClear();
			m.core->GetJit()->DumpBlock(0x8078);
			std::string dumpBefore = DspTestLogText();
			Assert::IsTrue(dumpBefore.find("DSPBLOCK pc=8078") != std::string::npos,
				L"the wait routine's block must be cached");

			// Make the next step take the interpreter path (a latched CPU->DSP request does
			// that), so it decodes into whatever DecoderInfo the recompiler left behind.
			m.dsp.SetIntBit(true);
			m.core->RunJitBlock();
			m.dsp.SetIntBit(false);
			m.dsp.ClearCpuIntRequest();
			m.core->ReturnFromInterrupt();

			DspTestLogClear();
			m.core->GetJit()->DumpBlock(0x8078);
			std::string dumpAfter = DspTestLogText();

			Assert::AreEqual(Util::StringToWstring(dumpBefore), Util::StringToWstring(dumpAfter),
				L"an interpreted instruction must not rewrite the block's decoded instructions");

			// Re-enter from the reset vector, like the Reset interrupt of the real boot does.
			m.core->regs.pcs->clear();
			m.core->regs.psr.bits = 0;
			m.core->regs.pc = 0x8000;

			for (int i = 0; i < 60 && m.core->regs.pc != 0x8078; i++)
			{
				m.core->RunJitBlock();
			}
			for (int i = 0; i < 5; i++)
			{
				m.core->RunJitBlock();
			}

			Assert::AreEqual((uint32_t)0x8078, m.core->regs.pc,
				L"the wait routine must still spin at 0x8078");
		}

		/// <summary>
		/// The real IROM boot path (mailbox handshake, the command dispatcher and the wait loop)
		/// on the recompiler must match the interpreter instruction for instruction.
		/// </summary>
		TEST_METHOD(Jit_MatchesInterpreterOnIromBoot)
		{
			std::string path;
			if (!FindRepoFile("build/Data/dsp_irom.bin", path))
			{
				Assert::Fail(L"build/Data/dsp_irom.bin was not found");
			}

			const uint32_t wanted = 20000;

			m.Reset();
			Assert::IsTrue(m.LoadIROM(Util::StringToWstring(path)), L"the IROM image must load");
			m.core->regs.pc = 0x8000;
			m.core->ResetInstructionCounter();

			m.core->SetJitMaxBlockInstrs(32);
			uint32_t retired = 0;
			uint32_t biggestBlock = 0;
			while (retired < wanted)
			{
				uint32_t count = m.core->RunJitBlock();
				if (count > biggestBlock) biggestBlock = count;
				retired += count;
			}
			Assert::IsTrue(biggestBlock > 1, L"the recompiler must actually run multi-word blocks");
			CoreState recompiled = Capture();

			m.Reset();
			Assert::IsTrue(m.LoadIROM(Util::StringToWstring(path)), L"the IROM image must load");
			m.core->regs.pc = 0x8000;
			m.core->ResetInstructionCounter();
			for (uint32_t i = 0; i < retired; i++)
			{
				m.core->Step();
			}
			CoreState interp = Capture();

			std::wstring diff = Diff(interp, recompiled);
			Assert::IsTrue(diff.empty(), diff.c_str());
		}
	};
}
