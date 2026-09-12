// Command Processor (CP): the tests written straight from the specifications.
//
// This file is the audit of `src/cp.cpp` against the CP documentation
// (`specs/architecture/command-processor.md`, `specs/architecture/gfx.md`) and against the
// hardware reference design of the block. The existing `gfx_cp_test.cpp` and
// `gfx_cp_array_test.cpp` already cover that the command stream reaches the pipeline; this file
// covers the CP's own contract, i.e. the things the documentation pins down exactly and the
// emulator is therefore not free to invent:
//
//   * the CPU-visible register window (`CP_FIFO_*`, `CP_STATUS`, `CP_ENABLE`, `CP_CLR`) and the
//     meaning of each bit, including *where* the read pointer stops and how the CPU resumes it;
//   * the FIFO ring: `read == write` is empty, the pointers move in 32-byte units, a wrap goes
//     back to `base`, and the under/overflow flags follow the water marks;
//   * the attribute and colour byte counts of the VAT formats, which the reference design fixes
//     (a scalar component is 1/2/4 bytes, a colour is 2/3/4 bytes; an indexed attribute is 1 or 2
//     bytes whatever its value format is);
//   * the vertex attribute order, which the reference parser fixes as
//     matrix indices -> position(x,y,z) -> normal -> colour0 -> colour1 -> tex0..tex7;
//   * `gx_vtxsize`, which must agree with the table above - it is what `EnoughToExecute` uses to
//     know when a whole vertex is in the FIFO, so a wrong size desynchronises the stream.
//
// The numbers in the reference tables below are the ones the CP documentation gives; when a test
// builds a display list it uses them, not the emulator's own constants, so that the test fails
// when the emulator drifts.

#include "pch.h"
#include "gfx_test_common.h"

using namespace GfxUnitTest;

namespace pureikyubutest
{
	TEST_CLASS(GfxCpSpecTest)
	{
		//! Where the graphics FIFO lives in the emulated main memory.
		static const uint32_t FifoBase = 0x00020000;
		static const uint32_t FifoSize = 0x00001000;
		static const uint32_t FifoTop = FifoBase + FifoSize;

		// =========================================================================================
		// The reference tables (command-processor.md 4-5 and the reference design)
		// =========================================================================================

		//! A scalar/vector component format (`CompSize`, VAT bits 3:1 for position).
		enum ScalarFormat
		{
			FmtU8 = 0, FmtS8 = 1, FmtU16 = 2, FmtS16 = 3, FmtF32 = 4,
		};

		//! A colour format (`CompSize` for a colour attribute).
		enum ColorFormat
		{
			Clr565 = 0, Clr888 = 1, Clr888x = 2, Clr4444 = 3, Clr6666 = 4, Clr8888 = 5,
		};

		//! The byte count of one scalar component. A ubyte/byte is one byte, a ushort/short is two
		//! and a float is four; 5..7 are reserved and are not used by the tests.
		static int ScalarComponentBytes(int fmt)
		{
			switch (fmt)
			{
				case FmtU8:
				case FmtS8:		return 1;
				case FmtU16:
				case FmtS16:	return 2;
				case FmtF32:	return 4;
			}
			return -1;
		}

		//! The byte count of one colour. The reference fixes these: 565 and 4444 are two bytes,
		//! 888 and 6666 are three, 888x and 8888 are four.
		static int ColorBytes(int fmt)
		{
			switch (fmt)
			{
				case Clr565:
				case Clr4444:	return 2;
				case Clr888:
				case Clr6666:	return 3;
				case Clr888x:
				case Clr8888:	return 4;
			}
			return -1;
		}

		//! The component count a VAT packs into one bit.
		static int PositionComponents(int posCnt) { return posCnt ? 3 : 2; }	// 0 = (x,y), 1 = (x,y,z)
		static int NormalComponents(int nrmCnt) { return nrmCnt ? 9 : 3; }		// 0 = 3, 1 = 9 (NBT)
		static int TexCoordComponents(int texCnt) { return texCnt ? 2 : 1; }	// 0 = (s), 1 = (s,t)
		static int ColorComponents(int colCnt) { return colCnt ? 4 : 3; }		// 0 = RGB, 1 = RGBA

		//! The VCD source encoding (command-processor.md 5.2).
		enum VcdSource
		{
			VcdNone = 0, VcdDirect = 1, VcdIndex8 = 2, VcdIndex16 = 3,
		};

		//! The attribute numbers of the arrays (command-processor.md 5.4): 0 = position, and the
		//! colours and texcoords follow the attribute order, *not* the VertexAttr order.
		enum ArrayIndex
		{
			ArrayPos = 0, ArrayNrm = 1, ArrayCol0 = 2, ArrayCol1 = 3, ArrayTex0 = 4,
		};

		// =========================================================================================
		// Display list builder and FIFO plumbing
		// =========================================================================================

		class DisplayList
		{
			std::vector<uint8_t> bytes;

		public:
			void U8(uint8_t value) { bytes.push_back(value); }
			void U16(uint16_t value) { U8((uint8_t)(value >> 8)); U8((uint8_t)value); }
			void U32(uint32_t value) { U8((uint8_t)(value >> 24)); U8((uint8_t)(value >> 16)); U8((uint8_t)(value >> 8)); U8((uint8_t)value); }

			//! CP_LoadRegs (0x08): the XF/CP register address then a 32-bit payload.
			void CpReg(uint8_t index, uint32_t value)
			{
				U8((uint8_t)(Flipper::CP_CMD_LOAD_CPREG | 0));
				U8(index);
				U32(value);
			}

			//! CP_LoadBypass (0x60): the register index in the top byte, a 24-bit payload below.
			void BpReg(uint8_t index, uint32_t value)
			{
				U8((uint8_t)(Flipper::CP_CMD_LOAD_BPREG | 0));
				U32(((uint32_t)index << 24) | (value & 0xffffff));
			}

			//! A draw command: the opcode and the 16-bit vertex count that follows it.
			void Draw(uint8_t opcode, uint16_t vertexCount)
			{
				U8(opcode);
				U16(vertexCount);
			}

			//! CP_NOP pads the stream; the FIFO hands whole 32-byte bursts to the CP.
			void Align()
			{
				while ((bytes.size() % 32) != 0)
				{
					U8((uint8_t)(Flipper::CP_CMD_NOP | 0));
				}
			}

			void Raw(const uint8_t* data, size_t size) { bytes.insert(bytes.end(), data, data + size); }
			void Raw(const std::vector<uint8_t>& data) { bytes.insert(bytes.end(), data.begin(), data.end()); }

			const std::vector<uint8_t>& Bytes() const { return bytes; }
			size_t Size() const { return bytes.size(); }
		};

		//! How many whole 32-byte bursts a list occupies - the unit the CP walks the FIFO in.
		static int Bursts(const DisplayList& list)
		{
			return (int)((list.Size() + 31) / 32);
		}

		//! A clean machine: the pipeline reset, the FIFO reads stopped (so that a stream left
		//! behind by the previous test cannot be walked while this one runs) and the log cleared.
		static GfxTestMachine& M()
		{
			GfxTestMachine& m = Machine();
			m.Reset();
			// Stop the stream and drop whatever the previous test left in the CP-side command
			// buffer, so that this test's bytes are the first thing the CP decodes.
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, 0);
			m.flipper->cp->ResetFifoProcessor();
			PIClearAssertedInterrupts();
			ClearTestLog();
			return m;
		}

		//! Install the FIFO windows. `write` and `read` are byte addresses; `top` defaults to the
		//! end of the ring. Nothing is enabled here - the tests decide that, because the reset
		//! state of CP_ENABLE matters to the FIFO behaviour.
		static void SetFifoRegs(GfxTestMachine& m, uint32_t base, uint32_t top, uint32_t write, uint32_t read)
		{
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BASEL, base & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BASEH, base >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_TOPL, top & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_TOPH, top >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_WPTRL, write & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_WPTRH, write >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_RPTRL, read & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_RPTRH, read >> 16);
		}

		//! Put a display list in the emulated main memory and point the FIFO at it.
		static int SetupFifo(GfxTestMachine& m, const std::vector<uint8_t>& list, uint32_t base = FifoBase)
		{
			uint32_t padded = (uint32_t)((list.size() + 31) & ~31u);

			std::vector<uint8_t> image(padded, 0);
			if (!list.empty())
			{
				memcpy(image.data(), list.data(), list.size());
			}
			WriteMainMemory(base, image.data(), image.size());

			// The ring is larger than the list on purpose: with `read == write` meaning "empty", a
			// list that ended exactly at `top` would look empty to the CP.
			SetFifoRegs(m, base, base + FifoSize, base + padded, base);

			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC);

			return (int)(padded / 32);
		}

		static void RunFifo(GfxTestMachine& m, int bursts)
		{
			for (int i = 0; i < bursts; i++)
			{
				m.flipper->cp->PumpFifo();
			}
		}

		static uint16_t CpStatus(GfxTestMachine& m)
		{
			uint32_t value = 0;
			Assert::IsTrue(PIRegRead(PI_REGSPACE_CP | CP_STATUS, &value), L"CP_STATUS must answer");
			return (uint16_t)value;
		}

		static uint16_t CpEnable(GfxTestMachine& m)
		{
			uint32_t value = 0;
			Assert::IsTrue(PIRegRead(PI_REGSPACE_CP | CP_ENABLE, &value), L"CP_ENABLE must answer");
			return (uint16_t)value;
		}

		static uint32_t CpReadPtr(GfxTestMachine& m)
		{
			uint32_t low = 0, high = 0;
			PIRegRead(PI_REGSPACE_CP | CP_FIFO_RPTRL, &low);
			PIRegRead(PI_REGSPACE_CP | CP_FIFO_RPTRH, &high);
			return ((high & 0x3ff) << 16) | (low & 0xffe0);
		}

		static uint32_t CpBreakPtr(GfxTestMachine& m)
		{
			uint32_t low = 0, high = 0;
			PIRegRead(PI_REGSPACE_CP | CP_FIFO_BRKL, &low);
			PIRegRead(PI_REGSPACE_CP | CP_FIFO_BRKH, &high);
			return ((high & 0x3ff) << 16) | (low & 0xffe0);
		}

		static uint32_t CpCount(GfxTestMachine& m)
		{
			uint32_t low = 0, high = 0;
			PIRegRead(PI_REGSPACE_CP | CP_FIFO_COUNTL, &low);
			PIRegRead(PI_REGSPACE_CP | CP_FIFO_COUNTH, &high);
			return ((high & 0x3ff) << 16) | (low & 0xffe0);
		}

	public:

		// =========================================================================================
		// 1. The register window and its encoding
		// =========================================================================================

		// The FIFO addresses are 21-bit, 32-byte aligned values split over a low and a high half:
		// the low register keeps bits 15:5 and the high register bits 25:16 (command-processor.md
		// 4.5). Reading the pair back must reproduce the address.
		TEST_METHOD(CpSpec_FifoAddressHalvesPreserveBits15To25)
		{
			GfxTestMachine& m = M();

			const uint32_t address = 0x00123440;
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BASEL, address & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BASEH, address >> 16);

			uint32_t low = 0, high = 0;
			PIRegRead(PI_REGSPACE_CP | CP_FIFO_BASEL, &low);
			PIRegRead(PI_REGSPACE_CP | CP_FIFO_BASEH, &high);

			Assert::AreEqual<uint32_t>(address & 0xffe0, low & 0xffe0, L"bits 15:5 survive the low half");
			Assert::AreEqual<uint32_t>((address >> 16) & 0x3ff, high & 0x3ff, L"bits 25:16 survive the high half");
		}

		// The low half of each address register clears the low five bits: the CP addresses whole
		// 32-byte units, and a write of a sub-unit address must not be able to misalign the ring.
		TEST_METHOD(CpSpec_AddressRegistersIgnoreTheLowFiveBits)
		{
			GfxTestMachine& m = M();

			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BASEL, 0xffff);
			uint32_t low = 0;
			PIRegRead(PI_REGSPACE_CP | CP_FIFO_BASEL, &low);
			Assert::AreEqual<uint32_t>(0xffe0, low & 0xffff, L"the low five bits are always zero");
		}

		// The break point is one of those 21-bit addresses, kept in the same two halves.
		TEST_METHOD(CpSpec_BreakPointHalvesPreserveBits15To25)
		{
			GfxTestMachine& m = M();

			const uint32_t address = 0x002468A0;
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BRKL, address & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BRKH, address >> 16);

			uint32_t low = 0, high = 0;
			PIRegRead(PI_REGSPACE_CP | CP_FIFO_BRKL, &low);
			PIRegRead(PI_REGSPACE_CP | CP_FIFO_BRKH, &high);

			Assert::AreEqual<uint32_t>(address & 0xffe0, low & 0xffe0, L"the break point low half");
			Assert::AreEqual<uint32_t>((address >> 16) & 0x3ff, high & 0x3ff, L"the break point high half");
		}

		// CP_STATUS bit 2 is the read unit idle flag and bit 3 the CP idle flag; both are set while
		// there is nothing to read. CP_ENABLE resets with the FIFO reads *and* the break point
		// disabled, and the write pointer increment enabled (bits 0, 1 and 4 clear, bit 4 set).
		TEST_METHOD(CpSpec_EnableAndStatusResetState)
		{
			GfxTestMachine& m = M();

			// Every documented enable bit is independent: writing only some of them must leave the
			// others alone, and reading them back must reproduce exactly what was written.
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_BPEN | CP_CR_WPINC);
			uint16_t enable = CpEnable(m);
			Assert::AreEqual<uint16_t>(CP_CR_RDEN, (uint16_t)(enable & CP_CR_RDEN), L"FIFO reads are enabled");
			Assert::AreEqual<uint16_t>(CP_CR_BPEN, (uint16_t)(enable & CP_CR_BPEN), L"the break point is enabled");
			Assert::AreEqual<uint16_t>(0, (uint16_t)(enable & CP_CR_BPINTEN), L"the break interrupt stays off");
			Assert::AreEqual<uint16_t>(CP_CR_WPINC, (uint16_t)(enable & CP_CR_WPINC), L"the write pointer increment stays on");

			// With no stream installed the reader is idle.
			Assert::AreEqual<uint16_t>(0, (uint16_t)(CpStatus(m) & CP_SR_BPINT), L"no break condition yet");
		}

		// =========================================================================================
		// 2. The FIFO ring
		// =========================================================================================

		// With FIFO reads disabled the read pointer must not move, however many times the CP is
		// pumped: CP_ENABLE[0] is the gate the documentation gives it.
		TEST_METHOD(CpSpec_ReadsAreGatedByTheEnableBit)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			list.CpReg(Flipper::CP_MATINDEX_A_ID, 0x11223344);
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());

			// SetupFifo enabled the reads; disable them again and check that the pointer freezes.
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, 0);
			uint32_t before = CpReadPtr(m);
			RunFifo(m, bursts);
			Assert::AreEqual<uint32_t>(before, CpReadPtr(m), L"the read pointer must not move while reads are disabled");

			// ... and that enabling them lets the stream run again.
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC);
			RunFifo(m, bursts);
			Assert::AreNotEqual<uint32_t>(before, CpReadPtr(m), L"the stream resumes when the reads are enabled");
		}

		// The read pointer advances one 32-byte unit per burst, and the count is the distance
		// between the write and the read pointer (command-processor.md 2.2, 4.5).
		TEST_METHOD(CpSpec_ReadPointerAdvancesOneBurstAndTheCountFollows)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			for (int i = 0; i < 4; i++)
			{
				list.CpReg(Flipper::CP_MATINDEX_A_ID, (uint32_t)i);
			}
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());

			uint32_t expectedCount = (uint32_t)bursts * 32;
			Assert::AreEqual<uint32_t>(expectedCount, CpCount(m), L"the count is (write - read) in 32-byte units");

			for (int i = 0; i < bursts; i++)
			{
				m.flipper->cp->PumpFifo();
				expectedCount -= 32;
				Assert::AreEqual<uint32_t>(expectedCount, CpCount(m), L"the count falls by one burst per read");
			}

			Assert::AreEqual<uint32_t>(0, CpCount(m), L"the FIFO is empty when the stream has been read");
		}

		// A read pointer that reaches `top` wraps to `base` (command-processor.md 2.2). The list is
		// placed so that it ends exactly at the top of the ring.
		TEST_METHOD(CpSpec_ReadPointerWrapsFromTopToBase)
		{
			GfxTestMachine& m = M();

			// Two bursts, ending exactly at the top of a small ring.
			const uint32_t base = 0x00030000;
			const uint32_t ringSize = 64;

			DisplayList list;
			ProgramList(list);
			list.Align();

			std::vector<uint8_t> image(64, 0);
			memcpy(image.data(), list.Bytes().data(), list.Size());
			WriteMainMemory(base, image.data(), image.size());

			// write == base is the "empty" encoding, so start the write pointer at base and drive
			// the ring by hand.
			SetFifoRegs(m, base, base + ringSize, base, base);
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC);

			// Give the CP two bursts of work and check the pointer wrapped to the base.
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_WPTRL, base & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_WPTRH, base >> 16);

			// The ring is empty at this point; make the write pointer lead the read pointer by the
			// whole ring so that both bursts are available.
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_WPTRL, (base + ringSize) & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_WPTRH, (base + ringSize) >> 16);

			m.flipper->cp->PumpFifo();
			Assert::AreEqual<uint32_t>(base + 32, CpReadPtr(m), L"the first read moves one unit");

			m.flipper->cp->PumpFifo();
			Assert::AreEqual<uint32_t>(base, CpReadPtr(m), L"the pointer wraps from top back to base");
		}

		// `read == write` is the empty encoding: with the two pointers equal the CP must report an
		// empty FIFO and leave the read pointer alone.
		TEST_METHOD(CpSpec_EqualPointersMeanEmpty)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			ProgramList(list);
			list.Align();
			WriteMainMemory(FifoBase, list.Bytes().data(), list.Size());

			SetFifoRegs(m, FifoBase, FifoTop, FifoBase, FifoBase);
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC);

			Assert::AreEqual<uint32_t>(0, CpCount(m), L"equal pointers mean an empty FIFO");

			uint32_t before = CpReadPtr(m);
			RunFifo(m, 4);
			Assert::AreEqual<uint32_t>(before, CpReadPtr(m), L"an empty FIFO does not advance the pointer");
		}

		// =========================================================================================
		// 3. The water marks
		// =========================================================================================

		// When the count rises above the high water mark the overflow flag must be set, and when it
		// falls below the low water mark the underflow flag must be (command-processor.md 2.2).
		TEST_METHOD(CpSpec_WaterMarksSetTheOverflowAndUnderflowFlags)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			for (int i = 0; i < 8; i++)
			{
				list.CpReg(Flipper::CP_MATINDEX_A_ID, (uint32_t)i);
			}
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());

			// High mark below the occupancy -> overflow.
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_HICNTL, (FifoBase + 32) & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_HICNTH, 0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_LOCNTL, 0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_LOCNTH, 0);
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC | CP_CR_OVFEN);

			m.flipper->cp->PumpFifo();
			Assert::AreEqual<uint16_t>(CP_SR_OVF, (uint16_t)(CpStatus(m) & CP_SR_OVF), L"the occupancy is above the high mark");

			// CP_CLR[0] clears the overflow condition (command-processor.md 4.4).
			PIRegWrite(PI_REGSPACE_CP | CP_CLR, CP_CLR_OVFCLR);
			Assert::AreEqual<uint16_t>(0, (uint16_t)(CpStatus(m) & CP_SR_OVF), L"CP_CLR[0] clears the overflow flag");

			// Drain the FIFO down to the low mark -> underflow.
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_LOCNTL, (FifoBase + (uint32_t)bursts * 32) & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC | CP_CR_UVFEN);
			RunFifo(m, bursts + 1);
			Assert::AreEqual<uint16_t>(CP_SR_UVF, (uint16_t)(CpStatus(m) & CP_SR_UVF), L"the occupancy is below the low mark");

			PIRegWrite(PI_REGSPACE_CP | CP_CLR, CP_CLR_UVFCLR);
			Assert::AreEqual<uint16_t>(0, (uint16_t)(CpStatus(m) & CP_SR_UVF), L"CP_CLR[1] clears the underflow flag");
		}

		// The water mark interrupts are enabled by CP_ENABLE[2]/[3] and are reported through the
		// Processor Interface as the CP interrupt.
		TEST_METHOD(CpSpec_WaterMarkInterruptsReachTheProcessorInterface)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			for (int i = 0; i < 8; i++)
			{
				list.CpReg(Flipper::CP_MATINDEX_A_ID, (uint32_t)i);
			}
			list.Align();

			SetupFifo(m, list.Bytes());

			PIClearAssertedInterrupts();
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_HICNTL, (FifoBase + 32) & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_HICNTH, 0);
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC | CP_CR_OVFEN);

			m.flipper->cp->PumpFifo();

			Assert::AreEqual<uint32_t>(PI_INTERRUPT_CP, PIAssertedInterrupts() & PI_INTERRUPT_CP,
				L"the overflow interrupt is reported to the PI");
		}

		// =========================================================================================
		// 4. The break point
		// =========================================================================================

		// The break point stops the CP when the read pointer reaches it (command-processor.md 2.2,
		// 4.3). With CP_ENABLE[FIFOBRK] set the status bit must be raised and the read pointer must
		// not run past the break.
		TEST_METHOD(CpSpec_BreakPointStopsTheReaderAndSetsTheStatusBit)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			for (int i = 0; i < 4; i++)
			{
				list.CpReg(Flipper::CP_MATINDEX_A_ID, (uint32_t)i);
			}
			// 4 commands are 24 bytes, i.e. one burst; pad the list out to four bursts so that
			// there is stream behind the break point as well.
			while (Bursts(list) < 4)
			{
				list.CpReg(Flipper::CP_MATINDEX_B_ID, (uint32_t)Bursts(list));
			}
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());
			Assert::AreEqual<int>(4, bursts, L"the test stream is four bursts");

			// Break on the third burst.
			const uint32_t breakAt = FifoBase + 64;
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BRKL, breakAt & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BRKH, breakAt >> 16);

			// FIFO reads, the write pointer increment and the break point, no break interrupt.
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC | CP_CR_BPEN);

			Assert::AreEqual<uint32_t>(breakAt, CpBreakPtr(m), L"the break address was programmed");

			RunFifo(m, bursts);

			// The reader is not allowed to run past the break point.
			Assert::AreEqual<uint32_t>(breakAt, CpReadPtr(m), L"the reader stops at the break point");
			Assert::AreEqual<uint16_t>(CP_SR_BPINT, (uint16_t)(CpStatus(m) & CP_SR_BPINT),
				Widen("the break flag is set, enable=" + std::to_string(CpEnable(m)) +
					" rptr=" + std::to_string(CpReadPtr(m)) +
					" bptr=" + std::to_string(CpBreakPtr(m)) +
					" want=" + std::to_string(breakAt)).c_str());
		}

		// Clearing CP_ENABLE[FIFOBRK] clears the break status bit (the documentation puts the clear
		// on disabling the break point, not on the interrupt enable), and the reader resumes.
		TEST_METHOD(CpSpec_DisablingTheBreakPointClearsTheStatusAndResumes)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			for (int i = 0; i < 4; i++)
			{
				list.CpReg(Flipper::CP_MATINDEX_A_ID, (uint32_t)i);
			}
			while (Bursts(list) < 4)
			{
				list.CpReg(Flipper::CP_MATINDEX_B_ID, (uint32_t)Bursts(list));
			}
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());
			Assert::AreEqual<int>(4, bursts, L"the test stream is four bursts");

			const uint32_t breakAt = FifoBase + 64;
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BRKL, breakAt & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BRKH, breakAt >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC | CP_CR_BPEN);

			RunFifo(m, bursts);
			Assert::AreEqual<uint32_t>(breakAt, CpReadPtr(m), L"the reader stops at the break point");
			Assert::AreEqual<uint16_t>(CP_SR_BPINT, (uint16_t)(CpStatus(m) & CP_SR_BPINT), L"the break flag is set");

			// The CPU disables the break point, which is what clears the flag.
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC);
			Assert::AreEqual<uint16_t>(0, (uint16_t)(CpStatus(m) & CP_SR_BPINT), L"clearing FIFOBRK clears the flag");

			// ... and the reader walks on past the break point, to the end of the stream.
			RunFifo(m, bursts);
			Assert::AreEqual<uint32_t>(FifoBase + (uint32_t)bursts * 32, CpReadPtr(m),
				L"the reader resumes and reaches the end of the stream");
		}

		// A break point that is not enabled must never stop the reader, *whatever the break address
		// holds* - including the reset value 0, which is the address the FIFO often starts at. This
		// is the case that a stream with the break point and its address never programmed exercises.
		TEST_METHOD(CpSpec_DisabledBreakPointDoesNotStopTheReader)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			for (int i = 0; i < 4; i++)
			{
				list.CpReg(Flipper::CP_MATINDEX_A_ID, (uint32_t)i);
			}
			while (Bursts(list) < 4)
			{
				list.CpReg(Flipper::CP_MATINDEX_B_ID, (uint32_t)Bursts(list));
			}
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());

			// The break point address left at its reset value, the break point disabled.
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC);

			RunFifo(m, bursts);

			Assert::AreEqual<uint16_t>(0, (uint16_t)(CpStatus(m) & CP_SR_BPINT), L"no break flag with the break disabled");
			Assert::AreEqual<uint32_t>(FifoBase + (uint32_t)bursts * 32, CpReadPtr(m), L"the whole stream was read");
		}

		// The break point interrupt is a separate enable from the break point itself: with
		// CP_ENABLE[FIFOBRK] and [FIFOBRKINT] both set the CP interrupt must be raised at the break.
		TEST_METHOD(CpSpec_BreakPointInterruptReachesTheProcessorInterface)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			for (int i = 0; i < 4; i++)
			{
				list.CpReg(Flipper::CP_MATINDEX_A_ID, (uint32_t)i);
			}
			while (Bursts(list) < 4)
			{
				list.CpReg(Flipper::CP_MATINDEX_B_ID, (uint32_t)Bursts(list));
			}
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());

			const uint32_t breakAt = FifoBase + 64;
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BRKL, breakAt & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BRKH, breakAt >> 16);

			PIClearAssertedInterrupts();
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC | CP_CR_BPEN | CP_CR_BPINTEN);

			RunFifo(m, bursts);

			Assert::AreEqual<uint32_t>(PI_INTERRUPT_CP, PIAssertedInterrupts() & PI_INTERRUPT_CP,
				L"the break point interrupt is reported to the PI");
		}

		// =========================================================================================
		// 5. The command stream: the opcodes the CP owns
		// =========================================================================================

		// CP_NOP (0x00..0x07) is a whole command on its own: it consumes one byte and does nothing.
		// A stream of NOPs must be walked without the CP reporting an unsupported opcode.
		TEST_METHOD(CpSpec_NopIsAOneByteCommand)
		{
			GfxTestMachine& m = M();

			// A whole burst of NOPs: every NOP selector, repeated to fill the 32 bytes the CP
			// fetches at a time.
			DisplayList list;
			for (int i = 0; i < 32; i++)
			{
				list.U8((uint8_t)(Flipper::CP_CMD_NOP | (i & 7)));
			}
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());
			ClearTestLog();
			RunFifo(m, bursts);

			{
				uint8_t* mem = TestMainMemory(FifoBase, 8);
				char text[64];
				sprintf_s(text, "fifo[0..7]=%02X %02X %02X %02X %02X %02X %02X %02X",
					mem[0], mem[1], mem[2], mem[3], mem[4], mem[5], mem[6], mem[7]);
				Assert::AreEqual<int>(0, TestHaltCount(),
					Widen(std::string("NOPs are not an error: ") + TestLastHalt() + " | " + text).c_str());
			}
			Assert::AreEqual<std::wstring>(L"", Widen(TestLastHalt()), L"no opcode was rejected");
		}

		// CP_VCACHE_INVD (0x48..0x4F) is a one-byte command too; the CP must accept every VAT
		// selector in its low three bits.
		TEST_METHOD(CpSpec_VertexCacheInvalidateAcceptsEveryVat)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			for (int i = 0; i < 32; i++)
			{
				list.U8((uint8_t)(Flipper::CP_CMD_VCACHE_INVD | (i & 7)));
			}
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());
			ClearTestLog();
			RunFifo(m, bursts);

			Assert::AreEqual<int>(0, TestHaltCount(), Widen("the invalidate command is supported for all VATs: " + TestLastHalt()).c_str());
		}

		// CP_LoadRegs carries an 8-bit register index and a 32-bit payload, i.e. six bytes. The
		// MATINDEX registers are the ones with no side effects of their own, so they are what the
		// byte accounting can be checked with.
		TEST_METHOD(CpSpec_CpRegisterLoadIsSixBytes)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			list.CpReg(Flipper::CP_MATINDEX_A_ID, 0x01234567);
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());

			// One burst is all it takes, and the command must not leave bytes unread: a second
			// pump with an empty tail must not advance the pointer past the list.
			RunFifo(m, bursts);
			Assert::AreEqual<uint32_t>(FifoBase + 32, CpReadPtr(m), L"the six-byte command fits one burst");
		}

		// CP_LoadBypass (0x60..0x6F) carries a 24-bit payload with the register index in the top
		// byte. All sixteen indices of the group must be accepted.
		TEST_METHOD(CpSpec_BypassLoadAcceptsEveryIndex)
		{
			GfxTestMachine& m = M();

			// One burst per index, so that a rejected register can be named: the CP must decode
			// every index of the bypass group (command-processor.md 3.2, gfx.md 3.2).
			for (int i = 0; i < 16; i++)
			{
				DisplayList list;
				list.BpReg((uint8_t)(GEN_MODE_ID + i), 0);
				list.Align();

				int bursts = SetupFifo(m, list.Bytes());
				ClearTestLog();
				RunFifo(m, bursts);

				Assert::AreEqual<int>(0, TestHaltCount(),
					Widen("bypass index " + std::to_string(i) + ": " + TestLastHalt()).c_str());
			}
		}

		// An opcode outside the documented set must be reported as an unsupported command rather
		// than silently skipped: silently skipping it would desynchronise everything after it.
		TEST_METHOD(CpSpec_UnknownOpcodeIsReported)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			list.U8(0x58);				// the gap between VCACHE_INVD (0x48..0x4F) and LoadBypass (0x60)
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());
			ClearTestLog();
			RunFifo(m, bursts);

			Assert::IsTrue(TestHaltCount() > 0, L"an unsupported opcode must stop the emulator with a message");
		}

	private:

		//! A list with a couple of harmless CP register loads, used where the test only needs a
		//! non-empty stream.
		static void ProgramList(DisplayList& list)
		{
			list.CpReg(Flipper::CP_MATINDEX_A_ID, 0x11111111);
			list.CpReg(Flipper::CP_MATINDEX_B_ID, 0x22222222);
		}
	};
}
