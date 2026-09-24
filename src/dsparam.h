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
		uint16_t AdpcmXn;				// x[n], the decoder input sample of the General IIR mode
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
		volatile uint32_t    mmaddr, araddr;     // AMMAH/L and AMAAH/L - the DMA window
		volatile uint32_t    cnt;                // AMBLH/L - block length (bit 31 = direction)
		volatile bool        masked;             // AMDM - ARAM-DMA requests masked by the DSP (ARAM dedicated to the accelerator)
		uint16_t    amcr;               // AMCR (0x12) - the AR driver stores the ARAM size code here
		bool log;

		// Save state
		//
		// ARAM is not a view of main memory but a separate 16 MB heap block of the controller,
		// so it is part of the machine and has to travel with the state - it holds the audio
		// data the guest has staged there and the microcode's own work areas. The registers
		// around it are the DMA window (main-memory address, ARAM address and block length) and
		// the two configuration bits the driver reads back (AMCR and the accelerator mask).
		//
		// The `mem` pointer itself is not written (a state never carries a pointer); the bytes
		// it points at are, with `Raw`. `log` is a host switch and stays out.
		//
		// Only the buffer of exactly ARAMSIZE bytes is accepted on load: the length written in
		// front of it is checked against ARAMSIZE before a byte is copied, so a state whose ARAM
		// block is another size is refused instead of being read into the buffer.

		/// <summary>Write the ARAM controller registers and the whole ARAM buffer.</summary>
		void SaveState(SaveStates::StateWriter& writer) const;

		/// <summary>Read them back, refusing an ARAM block that is not ARAMSIZE bytes.</summary>
		void LoadState(SaveStates::StateReader& reader);
	};

	void    AROpen(Flipper::Flipper* flipper);
	void    ARClose();

	extern  ARControl aram;
}