// Command Processor (CP) tests.
//
// The CP is where the graphics command stream comes from: the CPU fills a FIFO in main memory and
// the CP walks it 32 bytes at a time, decoding the display list into register loads and draw
// commands (gfx.md 3, command-processor.md 2-5).
//
// These tests drive the CP exactly the way the emulator does: the FIFO registers are written
// through the PI register window (which is what the CPU does), the display list is placed in the
// emulated main memory and the FIFO is pumped with CommandProcessor::PumpFifo (the deterministic
// equivalent of one CP thread tick).

#include "pch.h"
#include "gfx_test_common.h"

using namespace GfxUnitTest;

namespace pureikyubutest
{
	TEST_CLASS(GfxCpTest)
	{
		//! Where the graphics FIFO lives in the emulated main memory.
		static const uint32_t FifoBase = 0x00010000;
		static const uint32_t FifoSize = 0x00001000;

		//! A little builder for a display list. The FIFO carries its words big-endian (the CP reads
		//! them with FifoProcessor::Read16/Read32, which shift the first byte into the high bits).
		class DisplayList
		{
			std::vector<uint8_t> bytes;

		public:
			void U8(uint8_t value) { bytes.push_back(value); }
			void U16(uint16_t value) { U8((uint8_t)(value >> 8)); U8((uint8_t)value); }
			void U24(uint32_t value) { U8((uint8_t)(value >> 16)); U8((uint8_t)(value >> 8)); U8((uint8_t)value); }
			void U32(uint32_t value) { U8((uint8_t)(value >> 24)); U24(value); }
			void F32(float value) { uint32_t bits; memcpy(&bits, &value, 4); U32(bits); }

			//! A bypass register load: the opcode and then one 32-bit word (the register id in the
			//! top byte, the 24-bit payload below it), which is how the CP decodes it.
			void BpReg(uint8_t index, uint32_t value)
			{
				U8((uint8_t)(Flipper::CP_CMD_LOAD_BPREG | 0));
				U32(((uint32_t)index << 24) | (value & 0xffffff));
			}

			//! An XF register block load.
			void XfReg(uint16_t index, const float* values, size_t count)
			{
				U8((uint8_t)(Flipper::CP_CMD_LOAD_XFREG | 0));
				U16((uint16_t)(count - 1));
				U16(index);
				for (size_t i = 0; i < count; i++)
				{
					F32(values[i]);
				}
			}

			//! A CP register load (through the FIFO, not the PI window).
			void CpReg(uint8_t index, uint32_t value)
			{
				U8((uint8_t)(Flipper::CP_CMD_LOAD_CPREG | 0));
				U8(index);
				U32(value);
			}

			//! The NOP that pads the list up to a whole 32-byte burst.
			void Align()
			{
				while ((bytes.size() % 32) != 0)
				{
					U8((uint8_t)(Flipper::CP_CMD_NOP | 0));
				}
			}

			const std::vector<uint8_t>& Bytes() const { return bytes; }
		};

		static GfxTestMachine& M()
		{
			GfxTestMachine& m = Machine();
			m.Reset();
			return m;
		}

		//! Configure the FIFO so that PumpFifo will walk `words` 32-byte bursts of the display list.
		static int SetupFifo(GfxTestMachine& m, const std::vector<uint8_t>& list)
		{
			uint32_t padded = (uint32_t)((list.size() + 31) & ~31u);

			std::vector<uint8_t> image(padded, 0);
			memcpy(image.data(), list.data(), list.size());
			WriteMainMemory(FifoBase, image.data(), image.size());

			// The FIFO registers are 16-bit halves of one 32-bit address: the low half keeps bits
			// 15:5 (the low five bits are always zero) and the high half bits 31:16.
			//
			// The ring is deliberately larger than the display list: `read == write` means "empty",
			// so a list that ended exactly at `top` would look empty to the CP.
			uint32_t top = FifoBase + FifoSize;
			uint32_t write = FifoBase + padded;

			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BASEL, FifoBase & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BASEH, FifoBase >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_TOPL, top & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_TOPH, top >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_WPTRL, write & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_WPTRH, write >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_RPTRL, FifoBase & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_RPTRH, FifoBase >> 16);

			// Enable FIFO reads
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC);

			return (int)(padded / 32);
		}

		//! Walk the FIFO one 32-byte burst at a time, exactly as the CP thread does.
		static void RunFifo(GfxTestMachine& m, int bursts)
		{
			for (int i = 0; i < bursts; i++)
			{
				m.flipper->cp->PumpFifo();
			}
		}

	public:

		// =========================================================================================
		// The register window
		// =========================================================================================

		TEST_METHOD(Cp_TheFifoRegistersAreVisibleToTheCpu)
		{
			GfxTestMachine& m = M();

			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BASEL, 0x1234);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BASEH, 0x0abc);

			uint32_t low = 0, high = 0;
			Assert::IsTrue(PIRegRead(PI_REGSPACE_CP | CP_FIFO_BASEL, &low), L"the register window must answer");
			Assert::IsTrue(PIRegRead(PI_REGSPACE_CP | CP_FIFO_BASEH, &high), L"...");

			Assert::AreEqual<uint32_t>(0x1220, low & 0xffe0, L"the base is kept in bits 25:5");
			Assert::AreEqual<uint32_t>(0x0abc, high, L"the high half");
		}

		// =========================================================================================
		// The command stream
		// =========================================================================================

		// A bypass load in the display list must reach the pipeline block that owns the register.
		TEST_METHOD(Cp_BypassLoadsReachThePipeline)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			list.BpReg(GEN_MODE_ID, (2u << 10));			// ntev = 2
			list.BpReg(PE_ZMODE_ID, 0);
			// SU_SCIS0 packs suy in bits 11:0 and sux in bits 23:12
			list.BpReg(SU_SCIS0_ID, (20u + 342) | ((10u + 342) << 12));
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());
			RunFifo(m, bursts);

			Assert::AreEqual<unsigned>(2, m.gfx->genmode.ntev, L"GEN_MODE reached the SU");
			Assert::AreEqual<unsigned>(10 + 342, m.gfx->su->State().scis0.sux, L"SU_SCIS0 reached the SU");
			Assert::AreEqual<unsigned>(20 + 342, m.gfx->su->State().scis0.suy, L"...");
		}

		// An XF register block load in the display list must land in the XF matrix RAM.
		TEST_METHOD(Cp_XfRegisterBlockLoadsReachTheXf)
		{
			GfxTestMachine& m = M();

			float matrix[16] = {
				1,2,3,4,
				5,6,7,8,
				9,10,11,12,
				13,14,15,16,
			};

			DisplayList list;
			list.XfReg(GFX::XF_MATRIX_MEMORY_ID, matrix, 16);
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());
			RunFifo(m, bursts);

			for (int i = 0; i < 16; i++)
			{
				Assert::AreEqual(matrix[i], m.gfx->xf->xf.mvTexMtx[i], 0.0001f, L"the matrix reached the XF");
			}
		}

		// The counters the CP keeps for the frame statistics must move with the stream.
		TEST_METHOD(Cp_TheFrameStatisticsCountTheLoads)
		{
			GfxTestMachine& m = M();

			Flipper::CommandProcessorStats before{};
			m.flipper->cp->GetStats(&before);

			DisplayList list;
			list.BpReg(GEN_MODE_ID, 0);
			list.BpReg(PE_ZMODE_ID, 0);
			list.BpReg(RAS1_TREF0_ID, 0);
			list.Align();

			int bursts = SetupFifo(m, list.Bytes());
			RunFifo(m, bursts);

			Flipper::CommandProcessorStats after{};
			m.flipper->cp->GetStats(&after);

			Assert::AreEqual<size_t>(before.bpLoads + 3, after.bpLoads, L"the bypass loads were counted");
		}

		// ResetFrameStats clears the counters (it runs at every frame end).
		TEST_METHOD(Cp_ResetFrameStatsClearsTheCounters)
		{
			GfxTestMachine& m = M();

			DisplayList list;
			list.BpReg(GEN_MODE_ID, 0);
			list.Align();
			int bursts = SetupFifo(m, list.Bytes());
			RunFifo(m, bursts);

			m.flipper->cp->ResetFrameStats();

			Flipper::CommandProcessorStats stats{};
			m.flipper->cp->GetStats(&stats);
			Assert::AreEqual<size_t>(0, stats.bpLoads, L"the counter was cleared");
		}
	};
}
