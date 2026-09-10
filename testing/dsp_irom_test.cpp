// DSP IROM (build/Data/dsp_irom.bin) tests.
//
// The 8 KB instruction ROM at program address 0x8000 is the DSP boot microcode: it runs
// after a hardware reset (the reset vector points at 0x8000, dsp.md section 2.7) while the
// CPU is still bringing the audio system up. It performs the DSP<->CPU mailbox handshake and
// then serves the CPU command set that downloads the audio microcode into IRAM over DSP-DMA.
//
// These tests disassemble the ROM with the emulator's own decoder/disassembler and execute
// its boot path on a bare Dsp16 device (no Flipper needed: the mailbox, the program ROM and
// the DSP-DMA registers are all inside the DSP).

#include "pch.h"
#include "dsp_test_common.h"

#include <set>

namespace DspUnitTest
{
	TEST_CLASS(DspIromTest)
	{
		DspTestMachine& m = Machine();

		static const DspAddress IROM_BASE = 0x8000;
		static const size_t IROM_WORDS = 4096;

		// IROM entry points discovered by the disassembly (see the report in wiki/dsp_irom.md).
		static const DspAddress IROM_Entry = 0x8000;		// reset vector / hardware init
		static const DspAddress IROM_Loop = 0x800E;			// main CPU command loop
		static const DspAddress IROM_Dispatch = 0x801F;		// command dispatcher
		static const DspAddress IROM_WaitMail = 0x8078;		// wait for a CPU -> DSP message
		static const DspAddress IROM_WaitAck = 0x807E;		// wait for the CPU to take the answer
		static const DspAddress IROM_Data = 0x88F0;			// first word of the ROM fill pattern
		static const DspAddress IROM_RampEnd = 0x8FFE;		// end of the incrementing ramp

		std::string iromPath;

		bool LoadIrom()
		{
			if (iromPath.empty())
			{
				std::string found;
				if (!FindRepoFile("build/Data/dsp_irom.bin", found))
				{
					return false;
				}
				iromPath = found;
			}
			return m.LoadIROM(Util::StringToWstring(iromPath));
		}

		/// <summary>
		/// Linear walk over the whole ROM, exactly as the interpreter would fetch it.
		/// </summary>
		std::string DisassembleIrom(int* undecoded)
		{
			std::string text;
			DspAddress pc = IROM_BASE;
			int unknown = 0;

			while (pc < IROM_BASE + IROM_WORDS)
			{
				DecoderInfo info = { 0 };
				Decoder::Decode(m.core->TranslateIMem(pc), DspCore::MaxInstructionSizeInBytes, info);
				if (info.sizeInBytes == 0)
				{
					break;
				}

				if ((!info.parallel && info.instr == DspRegularInstruction::Unknown) ||
					(info.parallel && info.parallelInstr == DspParallelInstruction::Unknown))
				{
					unknown++;
				}

				text += DspDisasm::Disasm(pc, info);
				text += "\n";
				pc += (DspAddress)(info.sizeInBytes >> 1);
			}

			if (undecoded) *undecoded = unknown;
			return text;
		}

		/// <summary>
		/// Recursive-descent disassembly: collects every program address that the core can
		/// actually reach from the reset vector, and reports the instructions that do not
		/// decode. Data embedded in the ROM is skipped (it is simply never reached).
		/// </summary>
		void WalkCode(DspAddress pc, std::set<DspAddress>& visited, std::vector<DspAddress>& undecoded)
		{
			std::vector<DspAddress> work;
			work.push_back(pc);

			while (!work.empty())
			{
				DspAddress at = work.back();
				work.pop_back();

				if (at < IROM_BASE || at >= IROM_BASE + IROM_WORDS)
				{
					continue;
				}
				if (visited.count(at))
				{
					continue;
				}
				visited.insert(at);

				DecoderInfo info = { 0 };
				Decoder::Decode(m.core->TranslateIMem(at), DspCore::MaxInstructionSizeInBytes, info);

				DspAddress next = at + (DspAddress)(info.sizeInBytes >> 1);

				if (info.sizeInBytes == 0)
				{
					continue;
				}

				if (!info.parallel)
				{
					switch (info.instr)
					{
						case DspRegularInstruction::Unknown:
							undecoded.push_back(at);
							work.push_back(next);
							continue;
						case DspRegularInstruction::jmp:
						case DspRegularInstruction::call:
							work.push_back(info.ImmOperand.Address);
							work.push_back(next);
							continue;
						case DspRegularInstruction::rets:
						case DspRegularInstruction::reti:
						case DspRegularInstruction::trap:
						case DspRegularInstruction::wait:
							// No fall-through. A conditional rets may fall through, but the
							// microcode only uses it unconditionally.
							if (info.cc != ConditionCode::always)
							{
								work.push_back(next);
							}
							continue;
						default:
							break;
					}
				}
				else if (info.parallelInstr == DspParallelInstruction::Unknown)
				{
					undecoded.push_back(at);
				}

				work.push_back(next);
			}
		}

	public:

		TEST_METHOD_INITIALIZE(Setup)
		{
			m.Reset();
		}

		// ===============================================================
		// The image and its decode
		// ===============================================================

		TEST_METHOD(Irom_ImageIsPresent)
		{
			Assert::IsTrue(LoadIrom(), L"build/Data/dsp_irom.bin must be reachable from the test host");
		}

		TEST_METHOD(Irom_EveryReachableInstructionDecodes)
		{
			if (!LoadIrom()) { Assert::Fail(L"IROM image not found"); }

			std::set<DspAddress> visited;
			std::vector<DspAddress> undecoded;
			WalkCode(IROM_Entry, visited, undecoded);

			wchar_t msg[256];
			swprintf_s(msg, _countof(msg), L"%zu reachable instructions, first undecoded at 0x%04X",
				visited.size(), undecoded.empty() ? 0 : (uint32_t)undecoded[0]);
			Assert::IsTrue(undecoded.empty(), msg);

			// The walk must cover the whole boot code (the dispatcher is reached only through
			// a conditional branch, so a linear walk is not enough).
			Assert::IsTrue(visited.count(IROM_Entry) == 1, L"the reset vector must be code");
			Assert::IsTrue(visited.count(IROM_Loop) == 1, L"the main loop must be reachable");
			Assert::IsTrue(visited.count(IROM_Dispatch) == 1, L"the dispatcher must be reachable");
			Assert::IsTrue(visited.count(IROM_WaitMail) == 1, L"the mailbox wait routine must be reachable");
			Assert::IsTrue(visited.count(IROM_WaitAck) == 1, L"the ack wait routine must be reachable");
			Assert::IsTrue(visited.size() > 100, L"the boot code is more than a stub");
		}

		TEST_METHOD(Irom_TailIsALookupTableNotCode)
		{
			if (!LoadIrom()) { Assert::Fail(L"IROM image not found"); }

			std::set<DspAddress> visited;
			std::vector<DspAddress> undecoded;
			WalkCode(IROM_Entry, visited, undecoded);

			// Nothing from 0x88F0 to the end of the ROM is reachable as code: that region is
			// the ROM fill pattern (an incrementing ramp), not microcode.
			for (DspAddress addr = IROM_Data; addr < IROM_BASE + IROM_WORDS; addr++)
			{
				Assert::IsTrue(visited.count(addr) == 0, L"the ROM tail must not be executed as code");
			}

			for (DspAddress addr = IROM_Data; addr < IROM_RampEnd - 1; addr++)
			{
				Assert::AreEqual((uint16_t)(m.core->ReadIMem(addr) + 1), m.core->ReadIMem(addr + 1),
					L"the ROM tail is an incrementing fill pattern");
			}

			Assert::AreEqual((uint16_t)0x88F0, m.core->ReadIMem(IROM_Data));
			Assert::AreEqual((uint16_t)0x8FFD, m.core->ReadIMem(IROM_RampEnd - 1));
		}

		TEST_METHOD(Irom_DumpDisassemblyForReview)
		{
			// Regenerates build/Data/dsp_irom_disasm.txt so that the analysis in the
			// repository stays in sync with the decoder.
			if (!LoadIrom()) { Assert::Fail(L"IROM image not found"); }

			int unknown = 0;
			std::string text = DisassembleIrom(&unknown);
			Assert::IsTrue(text.size() > 1000);

			std::string dir = iromPath.substr(0, iromPath.find_last_of("\\/"));
			std::string outPath = dir + "\\dsp_irom_disasm.txt";

			std::vector<uint8_t> bytes(text.begin(), text.end());
			Assert::IsTrue(Util::FileSave(outPath, bytes), L"the disassembly must be writable");
		}

		// ===============================================================
		// Executing the boot path
		// ===============================================================

		TEST_METHOD(Irom_ResetVectorEntersTheBootCode)
		{
			if (!LoadIrom()) { Assert::Fail(L"IROM image not found"); }

			m.core->HardReset();
			Assert::AreEqual((uint32_t)IROM_Entry, m.core->regs.pc, L"hard reset must vector to the IROM");

			m.Step();
			Assert::AreEqual((uint16_t)0x00FF, m.core->regs.dpp, L"the boot code sets dpp = 0xFF first");
			Assert::AreEqual((uint32_t)(IROM_Entry + 2), m.core->regs.pc);
		}

		TEST_METHOD(Irom_BootPostsTheHandshakeMessage)
		{
			if (!LoadIrom()) { Assert::Fail(L"IROM image not found"); }

			m.core->HardReset();

			// Boot code up to the handshake (0x8000..0x800D) is 12 instructions.
			m.Steps(12);

			// The boot code clears the interrupt enables and the mode bits and selects
			// integer mode before talking to the CPU.
			Assert::AreEqual(0, (int)m.core->regs.psr.et);
			Assert::AreEqual(0, (int)m.core->regs.psr.te0);
			Assert::AreEqual(0, (int)m.core->regs.psr.te1);
			Assert::AreEqual(0, (int)m.core->regs.psr.te2);
			Assert::AreEqual(0, (int)m.core->regs.psr.te3);
			Assert::AreEqual(0, (int)m.core->regs.psr.xl);
			Assert::AreEqual(0, (int)m.core->regs.psr.dp);
			Assert::AreEqual(1, (int)m.core->regs.psr.im);

			// stli DMBH,#0x8071 + stli DMBL,#0xFEED = the "DSP is alive" message.
			Assert::AreEqual((uint16_t)0x8071, (uint16_t)m.dsp.DspToCpuReadHi(false),
				L"the DSP must offer 0x8071 as the high word (bit 15 is the valid flag)");
			Assert::AreEqual((uint16_t)0xFEED, (uint16_t)m.dsp.DspToCpuReadLo(false),
				L"the DSP must offer 0xFEED as the low word");
		}

		TEST_METHOD(Irom_BootWaitsForTheCpuMessage)
		{
			if (!LoadIrom()) { Assert::Fail(L"IROM image not found"); }

			m.core->HardReset();
			m.Steps(16);

			// The main loop calls the "wait for a CPU->DSP message" routine at 0x8078 and
			// spins there until the CPU writes the CPU->DSP mailbox.
			for (int i = 0; i < 64; i++)
			{
				m.Step();
			}

			Assert::IsTrue(m.core->regs.pc == IROM_WaitMail || m.core->regs.pc == (IROM_WaitMail + 1) ||
				m.core->regs.pc == (IROM_WaitMail + 3),
				L"the boot code must block in the mailbox wait loop");
			Assert::AreEqual(0, DspTestHaltCount());

			// A CPU message releases it.
			m.dsp.CpuToDspWriteHi(0x80F3);
			m.dsp.CpuToDspWriteLo(0x0000);
			m.Steps(2);
			Assert::IsTrue(m.core->regs.pc != IROM_WaitMail, L"the wait loop must be left");
		}

		TEST_METHOD(Irom_CommandDispatcherLoadsRegisters)
		{
			if (!LoadIrom()) { Assert::Fail(L"IROM image not found"); }

			m.core->HardReset();

			// Message 1: the 0x80F3 "audio system ready" handshake with command 0xA001,
			// which makes the DSP load m0 and m1 from the next mailbox message.
			m.dsp.CpuToDspWriteHi(0x80F3);
			m.dsp.CpuToDspWriteLo(0xA001);

			// Run until the dispatcher has consumed the first message and is waiting for the
			// second one: pc back in the wait loop with the mailbox valid flag cleared.
			int budget = 4000;
			while (budget-- > 0)
			{
				m.Step();
				if (m.core->regs.pc == IROM_WaitMail && (m.dsp.CpuToDspReadHi(false) & 0x8000) == 0)
				{
					break;
				}
			}
			Assert::IsTrue(budget > 0, L"the boot code must consume the first message");
			Assert::AreEqual(0, DspTestHaltCount(), L"the boot path must not trap");

			// Message 2 carries the two words the handler stores.
			m.dsp.CpuToDspWriteHi(0x80F3);
			m.dsp.CpuToDspWriteLo(0x1234);

			budget = 4000;
			while (budget-- > 0 && m.core->regs.m[1] != 0x1234)
			{
				m.Step();
			}

			Assert::AreEqual((uint16_t)0x1234, m.core->regs.m[1], L"A001 loads m1 from the mailbox");
			Assert::AreEqual((uint16_t)0x80F3, m.core->regs.m[0], L"A001 loads m0 from the mailbox high word");
			Assert::AreEqual(0, DspTestHaltCount());
		}

		TEST_METHOD(Irom_UnknownCommandIsEchoedBackAndTheDspKeepsRunning)
		{
			if (!LoadIrom()) { Assert::Fail(L"IROM image not found"); }

			m.core->HardReset();

			// A message whose high word is not the 0x80F3 handshake is echoed straight back
			// with the 0xFEEE marker, so that the CPU can tell "not ready" from "command
			// accepted"; the DSP must then return to its main loop.
			m.dsp.CpuToDspWriteHi(0x1234);
			m.dsp.CpuToDspWriteLo(0x5678);

			int budget = 4000;
			while (budget-- > 0)
			{
				m.Step();
				if (m.core->regs.pc == IROM_WaitAck)
				{
					break;
				}
			}

			Assert::IsTrue(m.core->regs.pc == IROM_WaitAck, L"the DSP must answer and wait for the ack");
			Assert::AreEqual((uint16_t)0xFEEE, (uint16_t)m.dsp.DspToCpuReadHi(false),
				L"the answer high word carries the 0xFEEE marker");
			Assert::AreEqual(0, DspTestHaltCount());
		}
	};
}
