// SI (serial / controller interface): tests written from the specifications.
//
// The serial interface is the block that frames the four controller ports and runs the automatic
// controller poll. The reference specifications spell out three things that are tested here:
//
//   * the register window - the SI has no 32-bit port: every 32-bit register is reached as two
//     16-bit halves, the high word at the base (even) address and the low word at base + 2;
//   * the register set and its bit fields - SICnOUTBUF is read/write, SIPOLL holds X (the poll
//     interval in horizontal lines), Y (the polls a frame) and the per-channel enables, and
//     SICOMCSR holds TSTART / CHANNEL / INLNGTH in its low half and OUTLNGTH plus the two
//     interrupt bits and masks in its high half;
//   * the poll schedule and the read-status interrupt - a poll raises RDSTINT on an enabled
//     channel, the SI line is asserted only while that flag is unmasked, and reading the channel's
//     input buffer is what clears it again.
//
// The schedule is driven through `SIPoll(line)`, which takes the video line counter explicitly, so
// the whole schedule is testable without a running VI. A wrap of that counter is a vertical blank.

#include "pch.h"
#include "gfx_test_common.h"

using namespace GfxUnitTest;

namespace pureikyubutest
{
	TEST_CLASS(GfxSiSpecTest)
	{
		//! The SI channel registers, from the software point of view (each channel owns an output
		//! buffer and the two halves of its input buffer).
		static const uint32_t ChanOut = 0x00;
		static const uint32_t ChanInH = 0x04;
		static const uint32_t ChanInL = 0x08;
		static const uint32_t ChanStride = 0x0C;

		static const uint32_t PollReg = 0x30;
		static const uint32_t ComCsr = 0x34;
		static const uint32_t SrReg = 0x38;

		//! SIPOLL bits: X is the interval in video lines (25:16), Y the number of polls a frame
		//! (15:8), then one enable and one vblank-copy bit per channel.
		static uint32_t PollX(uint32_t x) { return (x & 0x3ff) << 16; }
		static uint32_t PollY(uint32_t y) { return (y & 0xff) << 8; }
		static uint32_t PollEn(int channel) { return 1u << (7 - channel); }

		static GfxTestMachine& M()
		{
			GfxTestMachine& m = Machine();
			m.Reset();
			PIClearAssertedInterrupts();
			ClearTestLog();
			return m;
		}

		//! The SerialInterface the machine owns, reached through the emulated hardware.
		static Flipper::SerialInterface* Si(GfxTestMachine& m)
		{
			return m.flipper->si;
		}

		// =========================================================================================
		// The register window: every 32-bit register is two 16-bit halves.
		// =========================================================================================

		static void WriteSiHi(uint32_t offset, uint32_t value)
		{
			PIRegWrite(PI_REGSPACE_SI + offset, value & 0xffff);
		}

		static void WriteSiLo(uint32_t offset, uint32_t value)
		{
			PIRegWrite(PI_REGSPACE_SI + offset + 2, value & 0xffff);
		}

		//! A 32-bit write is the high word at the base address and the low word at base + 2.
		static void WriteSiWord(uint32_t offset, uint32_t value)
		{
			WriteSiHi(offset, value >> 16);
			WriteSiLo(offset, value);
		}

		static uint32_t ReadSiHi(uint32_t offset)
		{
			uint32_t value = 0;
			PIRegRead(PI_REGSPACE_SI + offset, &value);
			return value & 0xffff;
		}

		static uint32_t ReadSiLo(uint32_t offset)
		{
			uint32_t value = 0;
			PIRegRead(PI_REGSPACE_SI + offset + 2, &value);
			return value & 0xffff;
		}

		static uint32_t ReadSiWord(uint32_t offset)
		{
			return (ReadSiHi(offset) << 16) | ReadSiLo(offset);
		}

		//! Program the poll register with one enabled channel and the given interval / budget.
		static void SetPoll(GfxTestMachine& m, uint32_t x, uint32_t y, uint32_t channels = 1)
		{
			uint32_t poll = PollX(x) | PollY(y);
			for (uint32_t c = 0; c < 4; c++)
			{
				if (channels & (1u << c))
				{
					poll |= PollEn((int)c);
				}
			}
			WriteSiWord(PollReg, poll);
			PIClearAssertedInterrupts();
		}

		static bool RdstInt(GfxTestMachine& m)
		{
			return (ReadSiWord(ComCsr) & SI_COMCSR_RDSTINT) != 0;
		}

		static bool TcInt(GfxTestMachine& m)
		{
			return (ReadSiWord(ComCsr) & SI_COMCSR_TCINT) != 0;
		}

		static bool SiInterruptAsserted()
		{
			return (PIAssertedInterrupts() & PI_INTERRUPT_SI) != 0;
		}

		//! Read a channel's input buffer, which is the CPU-side acknowledge of a poll.
		static void ReadInputBuffer(uint32_t channel = 0)
		{
			ReadSiWord(ChanInH + channel * ChanStride);
		}

		// =========================================================================================
		// 1. The register window
		// =========================================================================================

		// The poll register is a full 32-bit register; what the CPU writes must read back.
		TEST_METHOD(SiSpec_PollRegisterRoundTrips)
		{
			GfxTestMachine& m = M();

			WriteSiWord(PollReg, PollX(7) | PollY(2) | PollEn(0));
			uint32_t poll = ReadSiWord(PollReg);

			Assert::AreEqual<uint32_t>(7, (poll >> 16) & 0x3ff, L"SIPOLL[X] is the poll interval");
			Assert::AreEqual<uint32_t>(2, (poll >> 8) & 0xff, L"SIPOLL[Y] is the polls per frame");
			Assert::AreEqual<uint32_t>(PollEn(0), poll & PollEn(0), L"channel 0 is enabled");
			Assert::AreEqual<uint32_t>(0, poll & PollEn(1), L"channel 1 stays disabled");
		}

		// The high halfword of SIPOLL carries X, the low halfword carries Y and the enables: a
		// 16-bit write to one half must leave the other half alone.
		TEST_METHOD(SiSpec_PollHalvesAreIndependent)
		{
			GfxTestMachine& m = M();

			WriteSiWord(PollReg, PollX(0x155) | PollY(0x2a) | PollEn(int(3)));
			Assert::AreEqual<uint32_t>(0x155, (ReadSiWord(PollReg) >> 16) & 0x3ff, L"X came from the high half");

			WriteSiLo(PollReg, 0);
			uint32_t poll = ReadSiWord(PollReg);

			Assert::AreEqual<uint32_t>(0x155, (poll >> 16) & 0x3ff, L"the low half did not disturb X");
			Assert::AreEqual<uint32_t>(0, (poll >> 8) & 0xff, L"writing the low half cleared Y");
		}

		// SICnOUTBUF is read/write, and a write must not disturb the other channels (each channel
		// owns its own buffer).
		TEST_METHOD(SiSpec_ChannelOutputBuffersAreIndependent)
		{
			GfxTestMachine& m = M();

			for (uint32_t c = 0; c < 4; c++)
			{
				WriteSiWord(ChanOut + c * ChanStride, 0x00A00000u + c);
			}

			for (uint32_t c = 0; c < 4; c++)
			{
				Assert::AreEqual<uint32_t>(0x00A00000u + c, ReadSiWord(ChanOut + c * ChanStride),
					Widen("channel " + std::to_string(c) + " kept its output buffer").c_str());
			}
		}

		// The input buffers are read-only: a write is ignored and the response bytes survive it.
		TEST_METHOD(SiSpec_InputBuffersAreReadOnly)
		{
			GfxTestMachine& m = M();

			SetPoll(m, 1, 4, 1);
			Si(m)->SIPoll(0);			// one poll, so the input buffer holds something

			uint32_t before = ReadSiWord(ChanInH);
			WriteSiWord(ChanInH, 0xdeadbeef);
			Assert::AreEqual<uint32_t>(before, ReadSiWord(ChanInH), L"SICnINBUFH is read-only");
		}

		// COMCSR splits across the two halfwords: the channel, the input length and TSTART live in
		// the low halfword, the output length and the interrupt/mask bits in the high halfword.
		TEST_METHOD(SiSpec_ComCsrFieldsSitAtTheDocumentedBitPositions)
		{
			GfxTestMachine& m = M();

			uint32_t value = (2u << 1) | (5u << 8) | (9u << 16);
			WriteSiWord(ComCsr, value);
			uint32_t csr = ReadSiWord(ComCsr);

			Assert::AreEqual<uint32_t>(2, SI_COMCSR_CHAN(csr), L"CHANNEL is bits 2:1");
			Assert::AreEqual<uint32_t>(5, SI_COMCSR_INLEN(csr), L"INLNGTH is bits 14:8");
			Assert::AreEqual<uint32_t>(9, SI_COMCSR_OUTLEN(csr), L"OUTLNGTH is bits 22:16");
			Assert::AreEqual<uint32_t>(0, csr & SI_COMCSR_TSTART, L"TSTART is clear when not written");
		}

		// Writing TSTART runs the transfer; TSTART is a pending bit rather than a stored one, so it
		// reads back clear and the completion flag is raised instead.
		TEST_METHOD(SiSpec_TstartReadsClearOnceTheTransferRan)
		{
			GfxTestMachine& m = M();

			WriteSiWord(ComCsr, (0u << 1) | (1u << 8) | (1u << 16) | SI_COMCSR_TSTART);

			uint32_t csr = ReadSiWord(ComCsr);
			Assert::AreEqual<uint32_t>(0, csr & SI_COMCSR_TSTART, L"TSTART is a pending bit");
			Assert::AreEqual<uint32_t>(SI_COMCSR_TCINT, csr & SI_COMCSR_TCINT, L"the transfer completed");

			// TCINT is write-1-to-clear.
			WriteSiHi(ComCsr, SI_COMCSR_TCINT >> 16);
			Assert::AreEqual<uint32_t>(0, TcInt(m), L"writing 1 to TCINT clears it");
		}

		// A fresh SI raises no interrupt: both flags and both masks reset to zero.
		TEST_METHOD(SiSpec_FreshInterfaceIsQuiet)
		{
			GfxTestMachine& m = M();

			Assert::AreEqual<uint32_t>(0, ReadSiWord(ComCsr) & (SI_COMCSR_RDSTINT | SI_COMCSR_TCINT),
				L"both interrupt flags reset clear");
			Assert::IsFalse(SiInterruptAsserted(), L"a freshly powered SI asserts nothing");
		}

		// =========================================================================================
		// 2. The poll schedule
		// =========================================================================================

		// `Y == 0` means no polling at all: the schedule must not poll however far the line counter
		// advances.
		TEST_METHOD(SiSpec_ZeroPollsPerFrameDisablesPolling)
		{
			GfxTestMachine& m = M();

			SetPoll(m, 1, 0);				// a perfectly good interval, but no budget
			PIClearAssertedInterrupts();

			for (uint32_t line = 0; line <= 64; line++)
			{
				Si(m)->SIPoll(line);
			}

			Assert::IsFalse(RdstInt(m), L"no poll with Y == 0");
			Assert::IsFalse(SiInterruptAsserted(), L"no SI interrupt with Y == 0");
		}

		// A channel whose enable bit is clear is never polled, even with a valid X and Y.
		TEST_METHOD(SiSpec_DisabledChannelIsNeverPolled)
		{
			GfxTestMachine& m = M();

			SetPoll(m, 1, 4, 0);			// interval and budget, but every enable bit clear
			PIClearAssertedInterrupts();

			for (uint32_t line = 0; line <= 64; line++)
			{
				Si(m)->SIPoll(line);
			}

			Assert::IsFalse(RdstInt(m), L"a disabled channel is not polled");
			Assert::IsFalse(SiInterruptAsserted(), L"and raises no interrupt");
		}

		// One poll happens at the start of a frame, and the next one only after `X` lines: with
		// X = 4, lines 11..13 must not poll again after the poll at line 10.
		TEST_METHOD(SiSpec_PollsAreSpacedByXVideoLines)
		{
			GfxTestMachine& m = M();

			SetPoll(m, 4, 8, 1);
			PIClearAssertedInterrupts();

			Si(m)->SIPoll(10);				// the first poll of the frame
			Assert::IsTrue(RdstInt(m), L"the first poll of the frame happens at the blank");

			// The guest acknowledges the poll by reading the input buffer, which clears RDSTINT.
			ReadInputBuffer();
			Assert::IsFalse(RdstInt(m), L"reading the input buffer clears RDSTINT");

			for (uint32_t line = 11; line < 14; line++)
			{
				Si(m)->SIPoll(line);
			}
			Assert::IsFalse(RdstInt(m), L"no poll before the interval has elapsed");

			Si(m)->SIPoll(14);				// 10 + 4 lines
			Assert::IsTrue(RdstInt(m), L"the next poll lands X lines after the first");
		}

		// The line counter wraps once per frame; a wrap is a new vertical blank, so the budget
		// resets and the first poll of the new frame becomes due.
		TEST_METHOD(SiSpec_TheScheduleIsAnchoredToTheVerticalBlank)
		{
			GfxTestMachine& m = M();

			SetPoll(m, 100, 1, 1);		// one poll a frame, and an interval longer than the frame
			PIClearAssertedInterrupts();

			// Frame 1: a mid-frame poll happens once.
			Si(m)->SIPoll(50);
			Assert::IsTrue(RdstInt(m), L"the first poll of the frame");
			ReadInputBuffer();

			// Still inside frame 1: the budget is spent and the interval has not elapsed.
			Si(m)->SIPoll(60);
			Si(m)->SIPoll(90);
			Assert::IsFalse(RdstInt(m), L"the frame budget of one poll is spent");

			// The counter wraps to the next frame, which re-arms the schedule.
			Si(m)->SIPoll(1);
			Assert::IsTrue(RdstInt(m), L"a wrap of the line counter is a new frame");
		}

		// `Y` caps the number of polls a frame: with X = 1 and Y = 2, only two polls may happen
		// however many lines pass.
		TEST_METHOD(SiSpec_PollsPerFrameIsTheBudget)
		{
			GfxTestMachine& m = M();

			SetPoll(m, 1, 2, 1);
			PIClearAssertedInterrupts();

			int polls = 0;
			for (uint32_t line = 0; line <= 20; line++)
			{
				bool before = RdstInt(m);
				Si(m)->SIPoll(line);
				if (!before && RdstInt(m))
				{
					polls++;
					ReadInputBuffer();
				}
			}

			Assert::AreEqual<int>(2, polls, L"a frame issues at most Y polls");
		}

		// A poll updates only the channel it belongs to: with channels 0 and 1 enabled, channel 2
		// and 3 stay silent.
		TEST_METHOD(SiSpec_OnlyEnabledChannelsReceiveData)
		{
			GfxTestMachine& m = M();

			SetPoll(m, 1, 4, (1u << 0) | (1u << 1));
			PIClearAssertedInterrupts();

			Si(m)->SIPoll(0);

			uint32_t sr = ReadSiWord(SrReg);
			Assert::AreEqual<uint32_t>(SI_SR_RDST0, sr & SI_SR_RDST0, L"channel 0 was polled");
			Assert::AreEqual<uint32_t>(SI_SR_RDST1, sr & SI_SR_RDST1, L"channel 1 was polled");
			Assert::AreEqual<uint32_t>(0, sr & SI_SR_RDST2, L"channel 2 was not polled");
			Assert::AreEqual<uint32_t>(0, sr & SI_SR_RDST3, L"channel 3 was not polled");
		}

		// =========================================================================================
		// 3. The read-status interrupt
		// =========================================================================================

		// A poll raises RDSTINT, but the SI interrupt is raised only when the read-status interrupt
		// is unmasked.
		TEST_METHOD(SiSpec_RdstInterruptHonoursItsMask)
		{
			GfxTestMachine& m = M();

			SetPoll(m, 1, 4, 1);
			WriteSiWord(ComCsr, ReadSiWord(ComCsr) & ~SI_COMCSR_RDSTINTMSK);
			PIClearAssertedInterrupts();

			Si(m)->SIPoll(5);

			Assert::IsTrue(RdstInt(m), L"the poll set the read-status flag");
			Assert::IsFalse(SiInterruptAsserted(), L"a masked read-status flag raises no interrupt");
		}

		// With the mask set the poll raises the Processor Interface interrupt.
		TEST_METHOD(SiSpec_RdstInterruptReachesTheProcessorInterface)
		{
			GfxTestMachine& m = M();

			SetPoll(m, 1, 4, 1);
			WriteSiWord(ComCsr, ReadSiWord(ComCsr) | SI_COMCSR_RDSTINTMSK);
			PIClearAssertedInterrupts();

			Si(m)->SIPoll(5);

			Assert::IsTrue(SiInterruptAsserted(), L"the poll is reported to the PI");
		}

		// Reading the input buffer of the polled channel is what clears the flag, and once every
		// channel's flag is clear the PI interrupt must be dropped as well.
		TEST_METHOD(SiSpec_ReadingTheInputBufferClearsTheInterrupt)
		{
			GfxTestMachine& m = M();

			SetPoll(m, 1, 4, 1);
			WriteSiWord(ComCsr, ReadSiWord(ComCsr) | SI_COMCSR_RDSTINTMSK);
			PIClearAssertedInterrupts();

			Si(m)->SIPoll(5);
			Assert::IsTrue(SiInterruptAsserted(), L"the poll is reported to the PI");

			ReadInputBuffer();

			Assert::IsFalse(RdstInt(m), L"the buffer read cleared the flag");
			Assert::IsFalse(SiInterruptAsserted(), L"and the PI interrupt went away with it");
		}

		// The flag is cleared by reading the input buffer, not by acknowledge through COMCSR.
		TEST_METHOD(SiSpec_ComCsrWriteDoesNotClearThePollFlag)
		{
			GfxTestMachine& m = M();

			SetPoll(m, 1, 4, 1);
			PIClearAssertedInterrupts();

			Si(m)->SIPoll(5);
			Assert::IsTrue(RdstInt(m), L"the poll set the read-status flag");

			WriteSiWord(ComCsr, ReadSiWord(ComCsr) & ~SI_COMCSR_RDSTINTMSK);
			Assert::IsTrue(RdstInt(m), L"RDSTINT survives a COMCSR write");
		}
	};
}
