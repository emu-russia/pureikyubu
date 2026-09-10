// DSP Mailbox unit tests.
//
// The two 32-bit mailboxes are the only communication path between the CPU and the DSP,
// so their word-pair protocol has to be exact. Reference: dsp.md section 4.1
//
//   | register                      | CPU addr | DSP addr |
//   | CPU->DSP mailbox, high word   | 0x000    | 0xFFFE   |  high word / valid flag
//   | CPU->DSP mailbox, low word    | 0x002    | 0xFFFF   |  low word
//   | DSP->CPU mailbox, high word   | 0x004    | 0xFFFC   |  high word / valid flag
//   | DSP->CPU mailbox, low word    | 0x006    | 0xFFFD   |  low word
//
// Protocol:
//   * writes and reads go high word first, then low word;
//   * writing the high word clears the valid flag, writing the low word sets it;
//   * the receiver reads the high word first and the low word second, and reading the
//     low word clears the flag;
//   * software tests bit 15 of the high word register to see whether a message waits.

#include "pch.h"
#include "dsp_test_common.h"

namespace DspUnitTest
{
	TEST_CLASS(DspMailboxTest)
	{
		DspTestMachine& m = Machine();

		Dsp16& CPU() { return m.dsp; }

		// DSP-side addresses (dsp.cpp ReadDMem/WriteDMem interpret them as hardware registers)
		static const DspAddress CMBH = 0xFFFE;
		static const DspAddress CMBL = 0xFFFF;
		static const DspAddress DMBH = 0xFFFC;
		static const DspAddress DMBL = 0xFFFD;

		// The DSP reads/writes the mailbox registers through its own data memory interface.
		uint16_t DspRead(DspAddress a) { return m.Dsp(a); }
		void DspWrite(DspAddress a, uint16_t v) { m.Dsp(a, v); }

	public:

		TEST_METHOD_INITIALIZE(Setup)
		{
			m.Reset();
		}

		// ===============================================================
		// CPU -> DSP mailbox
		// ===============================================================

		TEST_METHOD(CpuToDsp_ValidFlagIsSetByTheLowWordOnly)
		{
			CPU().CpuToDspWriteHi(0x1234);
			Assert::AreEqual((uint16_t)0x1234, CPU().CpuToDspReadHi(false));
			Assert::AreEqual(0, (int)(CPU().CpuToDspReadHi(false) & 0x8000),
				L"writing the high word must clear the valid flag");

			CPU().CpuToDspWriteLo(0x5678);
			Assert::AreEqual((uint16_t)0x5678, CPU().CpuToDspReadLo(false));
			Assert::AreEqual((uint16_t)0x9234, CPU().CpuToDspReadHi(false),
				L"writing the low word must set the valid flag (bit 15 of the high word)");
		}

		TEST_METHOD(CpuToDsp_DspSeesTheMessage)
		{
			CPU().CpuToDspWriteHi(0x0001);
			CPU().CpuToDspWriteLo(0xF00D);

			Assert::AreEqual((uint16_t)0x8001, DspRead(CMBH), L"the DSP sees the valid flag");
			Assert::AreEqual((uint16_t)0xF00D, DspRead(CMBL));
		}

		TEST_METHOD(CpuToDsp_ReadingTheLowWordByTheDspClearsTheFlag)
		{
			CPU().CpuToDspWriteHi(0x0001);
			CPU().CpuToDspWriteLo(0x0002);

			Assert::AreEqual((uint16_t)0x8001, DspRead(CMBH));
			Assert::AreEqual((uint16_t)0x0002, DspRead(CMBL));
			Assert::AreEqual((uint16_t)0x0001, DspRead(CMBH), L"the DSP has consumed the message");
		}

		TEST_METHOD(CpuToDsp_ReadingByTheCpuDoesNotConsumeTheMessage)
		{
			CPU().CpuToDspWriteHi(0x0001);
			CPU().CpuToDspWriteLo(0x0002);

			Assert::AreEqual((uint16_t)0x0002, CPU().CpuToDspReadLo(false));
			Assert::AreEqual((uint16_t)0x8001, CPU().CpuToDspReadHi(false),
				L"the CPU reading its own mailbox must not clear the flag");
		}

		TEST_METHOD(CpuToDsp_HighWordBit15IsNotData)
		{
			// Bit 15 of the high word register is the valid flag, so the sender's bit 15
			// is discarded when the high word is written.
			CPU().CpuToDspWriteHi(0xFFFF);
			Assert::AreEqual((uint16_t)0x7FFF, CPU().CpuToDspReadHi(false));
		}

		TEST_METHOD(CpuToDsp_HalfWrittenMessageIsNotVisibleAsValid)
		{
			CPU().CpuToDspWriteHi(0x1111);
			CPU().CpuToDspWriteLo(0x2222);
			DspRead(CMBL);								// consume

			CPU().CpuToDspWriteHi(0x3333);				// start a new message
			Assert::AreEqual(0, (int)(DspRead(CMBH) & 0x8000),
				L"a half written message must not appear valid");
			Assert::AreEqual((uint16_t)0x3333, DspRead(CMBH));
		}

		// ===============================================================
		// DSP -> CPU mailbox
		// ===============================================================

		TEST_METHOD(DspToCpu_ValidFlagIsSetByTheLowWordOnly)
		{
			DspWrite(DMBH, 0x4321);
			Assert::AreEqual((uint16_t)0x4321, CPU().DspToCpuReadHi(false));
			Assert::AreEqual(0, (int)(CPU().DspToCpuReadHi(false) & 0x8000));

			DspWrite(DMBL, 0x8765);
			Assert::AreEqual((uint16_t)0xC321, CPU().DspToCpuReadHi(false),
				L"the low word write must set the valid flag");
			Assert::AreEqual((uint16_t)0x8765, CPU().DspToCpuReadLo(false));
		}

		TEST_METHOD(DspToCpu_ReadingTheLowWordByTheCpuClearsTheFlag)
		{
			DspWrite(DMBH, 0x0000);
			DspWrite(DMBL, 0xBEEF);

			Assert::AreEqual((uint16_t)0x8000, CPU().DspToCpuReadHi(false));
			Assert::AreEqual((uint16_t)0xBEEF, CPU().DspToCpuReadLo(false));
			Assert::AreEqual((uint16_t)0x0000, CPU().DspToCpuReadHi(false),
				L"the CPU has consumed the message");
		}

		TEST_METHOD(DspToCpu_DspReadingDoesNotConsumeTheMessage)
		{
			DspWrite(DMBH, 0x0000);
			DspWrite(DMBL, 0xBEEF);

			Assert::AreEqual((uint16_t)0xBEEF, DspRead(DMBL));
			Assert::AreEqual((uint16_t)0x8000, DspRead(DMBH),
				L"the DSP reading its own mailbox must not clear the flag");
		}

		// ===============================================================
		// Consistency of the word pair
		// ===============================================================

		TEST_METHOD(DspToCpu_MessagePairIsConsistentAcrossTheTwoReads)
		{
			// The receiver reads the high word and then the low word. If the sender posts its
			// next message between those two reads, the receiver must still see the message
			// whose valid flag it observed, never a mixture of the two.
			DspWrite(DMBH, 0x1111);
			DspWrite(DMBL, 0x2222);

			uint16_t hi = CPU().DspToCpuReadHi(false);
			Assert::AreEqual((uint16_t)0x9111, hi);

			// The sender posts the next message in between.
			DspWrite(DMBH, 0x3333);
			DspWrite(DMBL, 0x4444);

			uint16_t lo = CPU().DspToCpuReadLo(false);
			Assert::AreEqual((uint16_t)0x2222, lo,
				L"the low word must belong to the high word the receiver observed");
		}

		TEST_METHOD(CpuToDsp_MessagePairIsConsistentAcrossTheTwoReads)
		{
			CPU().CpuToDspWriteHi(0x1111);
			CPU().CpuToDspWriteLo(0x2222);

			uint16_t hi = DspRead(CMBH);
			Assert::AreEqual((uint16_t)0x9111, hi);

			// The CPU posts the next message between the DSP's two reads.
			CPU().CpuToDspWriteHi(0x3333);
			CPU().CpuToDspWriteLo(0x4444);

			uint16_t lo = DspRead(CMBL);
			Assert::AreEqual((uint16_t)0x2222, lo,
				L"the low word must belong to the high word the DSP observed");
		}

		// ===============================================================
		// Reset and access restrictions
		// ===============================================================

		TEST_METHOD(Mailboxes_AreClearedByReset)
		{
			CPU().CpuToDspWriteHi(0x1111);
			CPU().CpuToDspWriteLo(0x2222);
			DspWrite(DMBH, 0x3333);
			DspWrite(DMBL, 0x4444);

			m.core->HardReset();

			Assert::AreEqual((uint16_t)0x0000, CPU().CpuToDspReadHi(false));
			Assert::AreEqual((uint16_t)0x0000, CPU().CpuToDspReadLo(false));
			Assert::AreEqual((uint16_t)0x0000, CPU().DspToCpuReadHi(false));
			Assert::AreEqual((uint16_t)0x0000, CPU().DspToCpuReadLo(false));
		}

		TEST_METHOD(Dsp_CannotWriteTheCpuToDspMailbox)
		{
			// The CPU is the only writer of the CMB pair; the DSP writing it is a microcode
			// bug and the emulator reports it instead of silently corrupting the mailbox.
			DspWrite(CMBH, 0x1234);
			Assert::IsTrue(DspTestHaltCount() > 0, L"a DSP write to CMBH must be reported");

			m.Reset();
			DspWrite(CMBL, 0x1234);
			Assert::IsTrue(DspTestHaltCount() > 0, L"a DSP write to CMBL must be reported");
		}

		TEST_METHOD(Dsp_WritesToTheDspToCpuMailboxAreAllowed)
		{
			DspWrite(DMBH, 0x0102);
			DspWrite(DMBL, 0x0304);
			Assert::AreEqual(0, DspTestHaltCount(), L"the DSP owns the DMB pair");
			Assert::AreEqual((uint16_t)0x8102, CPU().DspToCpuReadHi(false));
			Assert::AreEqual((uint16_t)0x0304, CPU().DspToCpuReadLo(false));
		}

		TEST_METHOD(Mailbox_MessagePassesThroughTheDeviceUnchanged)
		{
			// End to end: a 32-bit message written by the CPU is read back by the DSP.
			const uint16_t hi = 0x0BAD;
			const uint16_t lo = 0xF00D;

			CPU().CpuToDspWriteHi(hi);
			CPU().CpuToDspWriteLo(lo);

			uint16_t readHi = DspRead(CMBH);
			uint16_t readLo = DspRead(CMBL);

			Assert::AreEqual(hi, (uint16_t)(readHi & 0x7FFF));
			Assert::AreEqual(lo, readLo);
			Assert::AreEqual((uint16_t)0x8000, (uint16_t)(readHi & 0x8000));
		}
	};
}
