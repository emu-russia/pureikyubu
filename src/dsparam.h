#pragma once

namespace DSP
{
	union DspAccelAddress
	{
		struct
		{
			uint16_t l;
			uint16_t h;
		};
		uint32_t addr;
	};

	struct DspAccel
	{
		uint16_t Fmt;					// Sample format
		uint16_t AdpcmCoef[16];
		uint16_t AdpcmPds;				// predictor / scale combination
		uint16_t AdpcmYn1;				// y[n - 1]
		uint16_t AdpcmYn2;				// y[n - 2]
		uint16_t AdpcmGan;				// gain to be applied
		DspAccelAddress StartAddress;
		DspAccelAddress EndAddress;
		DspAccelAddress CurrAddress;
	};

}


namespace DSP
{

	// Accelerator sample format

	enum class AccelFormat
	{
		RawByte = 0x0005,		// Seen in IROM
		RawUInt16 = 0x0006,		// 
		Pcm16 = 0x000A,			// Signed 16 bit PCM mono
		Pcm8 = 0x0019,			// Signed 8 bit PCM mono
		Adpcm = 0x0000,			// ADPCM encoded (both standard & extended)
	};

}

// Old code (ARAM.cpp)

#define ARAMSIZE        (16 * 1024 * 1024)  // 16 mb
#define ARAM            DSP::aram.mem

namespace DSP
{
	// ARAM state (registers and other data)
	struct ARControl
	{
		uint8_t* mem;                // aux. memory buffer (size is ARAMSIZE)
		volatile uint32_t    mmaddr, araddr;     // DMA address
		volatile uint32_t    cnt;                // count + transfer type (bit31)
		uint16_t    size;               // "AR_SIZE" (0x5012) register
		Thread* dmaThread;
		int64_t gekkoTicks;
		size_t gekkoTicksPerSlice;
		bool dspRunningBeforeAramDma;
		bool log;
	};

	void    AROpen();
	void    ARClose();

	extern  ARControl aram;
}