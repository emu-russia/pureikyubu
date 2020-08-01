
#include "pch.h"

using namespace Debug;

namespace DSP
{

	// Instant DMA
	void Dsp16::DoDma()
	{
		_TB(Dsp16::DoDma);
		uint8_t* ptr = nullptr;

		if (logDspDma)
		{
			Report(Channel::DSP, "Dsp16::Dma: Mmem: 0x%08X, DspAddr: 0x%04X, Size: 0x%04X, Ctrl: %i\n",
				DmaRegs.mmemAddr.bits, DmaRegs.dspAddr, DmaRegs.blockSize, DmaRegs.control.bits);
		}

		if (DmaRegs.control.Imem)
		{
			ptr = TranslateIMem(DmaRegs.dspAddr);
		}
		else
		{
			ptr = TranslateDMem(DmaRegs.dspAddr);
		}

		if (ptr == nullptr)
		{
			Halt("Dsp16::DoDma: invalid dsp address: 0x%04X\n", DmaRegs.dspAddr);
			_TE();
			return;
		}

		if (DmaRegs.mmemAddr.bits < mi.ramSize)
		{
			if (DmaRegs.control.Dsp2Mmem)
			{
				memcpy(&mi.ram[DmaRegs.mmemAddr.bits], ptr, DmaRegs.blockSize);
			}
			else
			{
				memcpy(ptr, &mi.ram[DmaRegs.mmemAddr.bits], DmaRegs.blockSize);
			}
		}

		// Dump ucode.
		if (dumpUcode)
		{
			if (DmaRegs.control.Imem && !DmaRegs.control.Dsp2Mmem)
			{
				auto filename = fmt::format(L"Data\\DspUcode_{:04X}.bin", DmaRegs.blockSize);
				auto buffer = std::vector<uint8_t>(ptr, ptr + DmaRegs.blockSize);
				
				Util::FileSave(filename, buffer);
				Report(Channel::DSP, "Ucode dumped to DspUcode_%04X.bin\n", DmaRegs.blockSize);
			}
		}

		// Dump PCM samples coming from mixer
#if 0
		if (!DmaRegs.control.Imem && DmaRegs.control.Dsp2Mmem && 
			(0x400 >= DmaRegs.dspAddr && DmaRegs.dspAddr < 0x600) && 
			DmaRegs.blockSize == 0x80 )
		{
			memcpy(&dspSamples[dspSampleSize], ptr, DmaRegs.blockSize);
			dspSampleSize += DmaRegs.blockSize;
		}
#endif

		_TE();
	}

	void Dsp16::SpecialAramImemDma(uint8_t* ptr, size_t byteCount)
	{
		memcpy(iram, ptr, byteCount);

		if (logDspDma)
		{
			Report(Channel::DSP, "MMEM -> IRAM transfer %d bytes.\n", byteCount);
		}
	}

}
