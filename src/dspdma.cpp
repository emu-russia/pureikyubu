#include "pch.h"

using namespace Debug;

namespace DSP
{

	// Instant DMA
	void Dsp16::DoDma()
	{
		uint8_t* ptr = nullptr;

		if (logDspDma)
		{
			Report(Channel::DSP, "Dsp16::Dma: Mmem: 0x%08X, DspAddr: 0x%04X, Size: 0x%04X, Ctrl: %i\n",
				DmaRegs.mmemAddr.bits, DmaRegs.dspAddr, DmaRegs.blockSize, DmaRegs.control.bits);
		}

		if (DmaRegs.control.Imem)
		{
			ptr = core->TranslateIMem(DmaRegs.dspAddr);
		}
		else
		{
			ptr = core->TranslateDMem(DmaRegs.dspAddr);
		}

		if (ptr == nullptr)
		{
			Halt("Dsp16::DoDma: invalid dsp address: 0x%04X\n", DmaRegs.dspAddr);
			return;
		}

		// The block size is a 16-bit register value, so it can exceed the memory it is aimed
		// at; copying it blindly overruns the DSP memory arrays (and, through them, the
		// DspCore object). Clip the transfer to what is left of the region it starts in.
		size_t available = 0;
		if (DmaRegs.control.Imem)
		{
			if (DmaRegs.dspAddr < (DspCore::IRAM_SIZE / 2))
			{
				available = DspCore::IRAM_SIZE - (size_t)DmaRegs.dspAddr * 2;
			}
			else if (DmaRegs.dspAddr >= DspCore::IROM_START_ADDRESS &&
				DmaRegs.dspAddr < DspCore::IROM_START_ADDRESS + (DspCore::IROM_SIZE / 2))
			{
				available = DspCore::IROM_SIZE - (size_t)(DmaRegs.dspAddr - DspCore::IROM_START_ADDRESS) * 2;
			}
		}
		else
		{
			if (DmaRegs.dspAddr < (DspCore::DRAM_SIZE / 2))
			{
				available = DspCore::DRAM_SIZE - (size_t)DmaRegs.dspAddr * 2;
			}
			else if (DmaRegs.dspAddr >= DspCore::DROM_START_ADDRESS &&
				DmaRegs.dspAddr < DspCore::DROM_START_ADDRESS + (DspCore::DROM_SIZE / 2))
			{
				available = DspCore::DROM_SIZE - (size_t)(DmaRegs.dspAddr - DspCore::DROM_START_ADDRESS) * 2;
			}
		}

		size_t count = DmaRegs.blockSize;
		if (count > available)
		{
			Report(Channel::DSP, "Dsp16::DoDma: block size 0x%04X exceeds the 0x%zX bytes at 0x%04X, clipped\n",
				DmaRegs.blockSize, DmaRegs.dspAddr, available);
			count = available;
		}

		TraceMark(0xD000'0000u | ((DmaRegs.control.Imem ? 0x1u : 0u) << 20) |
			((DmaRegs.control.Dsp2Mmem ? 0x1u : 0u) << 21) | DmaRegs.dspAddr);

		// The DSP-side clip above says nothing about main memory: mmemAddr is a 26-bit guest
		// register (up to 0x03FFFFFC, i.e. 64 MB - 4, while only 24 or 48 MB are allocated), so a
		// block that starts inside RAM can still end past the end of the allocation. Ask the
		// memory interface for the whole window - it returns nullptr unless all of it is inside.
		uint8_t* mem_ptr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForDSP(DmaRegs.mmemAddr.bits, count);
		if (count > 0)
		{
			if (mem_ptr == nullptr)
			{
				// Abandon the transfer instead of copying past main memory (the DSP memory was
				// not written either way, so there is no stale JIT block to invalidate).
				Report(Channel::DSP, "Dsp16::DoDma: 0x%zX bytes at mmem 0x%08X are outside main memory\n",
					count, DmaRegs.mmemAddr.bits);
				return;
			}

			if (DmaRegs.control.Dsp2Mmem)
			{
				memcpy(mem_ptr, ptr, count);
			}
			else
			{
				memcpy(ptr, mem_ptr, count);
			}

			// The DSP memory DMA is one of the profiled channels (issue #394); it moves the data
			// between the DSP memories and main memory, so it is Splash traffic as well.
			Debug::HwProfile::Count(Debug::HwProfile::Counter::DmaDsp, count);
			Debug::HwProfile::Count(
				DmaRegs.control.Dsp2Mmem ? Debug::HwProfile::Counter::SplashWrite : Debug::HwProfile::Counter::SplashRead,
				count);
		}

		// The DSP just rewrote instruction memory: every block compiled from it is stale. (The
		// recompiler also verifies the words of a block before it runs it, so this is only the
		// cheap path - a forgotten invalidation would be a recompile, not a wrong instruction.)
		if (DmaRegs.control.Imem)
		{
			core->InvalidateJit();
		}

		// Dump ucode.
		if (dumpUcode)
		{
			if (DmaRegs.control.Imem && !DmaRegs.control.Dsp2Mmem && count > 0)
			{
				std::string filename = "Data/DspUcode_" + std::to_string(count) + ".bin";
				auto buffer = std::vector<uint8_t>(ptr, ptr + count);

				Util::FileSave(filename, buffer);
				Report(Channel::DSP, "Ucode dumped to %s\n", filename.c_str());
			}
		}

		// Dump PCM samples coming from mixer
#if 0
		if (!DmaRegs.control.Imem && DmaRegs.control.Dsp2Mmem &&
			(0x400 >= DmaRegs.dspAddr && DmaRegs.dspAddr < 0x600) &&
			DmaRegs.blockSize == 0x80)
		{
			memcpy(&dspSamples[dspSampleSize], ptr, DmaRegs.blockSize);
			dspSampleSize += DmaRegs.blockSize;
		}
#endif

	}

	void Dsp16::SpecialAramImemDma(uint8_t* ptr, size_t byteCount)
	{
		TraceMark(0xE000'0000u | (uint32_t)byteCount);

		if (byteCount > DspCore::IRAM_SIZE)
		{
			Report(Channel::DSP, "Dsp16::SpecialAramImemDma: %zu bytes into an 0x%zX byte IRAM, clipped\n",
				byteCount, DspCore::IRAM_SIZE);
			byteCount = DspCore::IRAM_SIZE;
		}

		memcpy(core->iram, ptr, byteCount);

		// The instruction memory changed behind the core's back, so any block compiled from it
		// is stale. (The recompiler also verifies a block's words before running it.)
		core->InvalidateJit();

		if (logDspDma)
		{
			Report(Channel::DSP, "MMEM -> IRAM transfer %d bytes.\n", byteCount);
		}
	}

}