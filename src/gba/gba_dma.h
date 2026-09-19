// GBA DMA: four channels (DMA0..DMA3) at 0x040000B0 .. 0x040000DF.
//
// The channels differ in the address ranges they may touch and in which start timing they
// support (GBATEK "DMA Transfers"):
//
//   channel  source              destination        start timings
//   DMA0     internal            internal           immediate, VBlank
//   DMA1     internal            internal           immediate, VBlank, HBlank
//   DMA2     internal            internal           immediate, VBlank, HBlank
//   DMA3     internal            cartridge          immediate, VBlank, HBlank, special (FIFO)
//
// "Special" is the video-capture / sound-FIFO mode: with it, the transfer runs whenever the
// destination FIFO asks for data (the sound channels A and B) or once per scanline (the display
// capture), instead of on the VBlank/HBlank edge.
//
// Word count 0 means 0x4000 words (DMA0..DMA2) or 0x10000 words (DMA3).

#pragma once

#include "gba_types.h"

namespace GBA
{
	class GbaBus;

	class Dma
	{
	public:
		/// <summary>One channel's registers, which are also what the debugger shows.</summary>
		struct Channel
		{
			uint32_t source = 0;		// DMAxSAD (the low half, which is what a read returns)
			uint32_t dest = 0;			// DMAxDAD (the low half, which is what a read returns)
			uint16_t count = 0;			// DMAxCNT_L
			uint16_t control = 0;		// DMAxCNT_H
			bool active = false;	// the channel is enabled and not finished
			bool pending = false;	// it was triggered and waits for its slice
			int latched = 0;		// words left in the current transfer

			// The addresses the *registers* hold, in full. A transfer copies them into the
			// pointers below when the enable bit goes from 0 to 1 (GBATEK "Source and Destination
			// Address and Word Count Registers"), and a repeat copies only the word count (and
			// optionally DAD) - so the source pointer keeps running, which is what streams a
			// sound buffer through the FIFO. Writing SAD/DAD while a transfer runs therefore has
			// to leave the pointer alone.
			uint32_t sourceRegister = 0;
			uint32_t destRegister = 0;

			// The running pointers. While the channel is idle, `sourceLatch` doubles as "the high
			// half of SAD seen so far", which is what a read of DMAxSAD_H returns (see the note at
			// the top of the .cpp).
			uint32_t sourceLatch = 0;
			uint32_t destLatch = 0;
		};

		void Reset();

		uint16_t Read16(uint32_t offset, uint16_t openBus) const;

		/// <summary>Write a DMA register (0x0B0..0x0DF). A write that enables a channel with the
		/// immediate timing starts it right away, which is why the bus is passed in.</summary>
		void Write16(GbaBus& bus, uint32_t offset, uint16_t value);

		/// <summary>Run the channels that were triggered by the VBlank edge.</summary>
		void OnVBlank(GbaBus& bus) { Trigger(bus, 1); }

		/// <summary>Run the channels that were triggered by the HBlank edge.</summary>
		void OnHBlank(GbaBus& bus) { Trigger(bus, 2); }

		/// <summary>The video capture channel (DMA3 special) runs once per scanline.</summary>
		void OnScanline(GbaBus& bus);

		/// <summary>The sound FIFOs ask for a refill (the bus calls this from the APU).</summary>
		void OnFifoRequest(GbaBus& bus, int channel);

		/// <summary>Run the transfers the FIFO hardware asked for. The bus calls this once per
		/// device slice so that a FIFO refill does not have to run inside an APU tick.</summary>
		void ServiceFifoRequests(GbaBus& bus);

		/// <summary>Perform one channel's transfer now (used by the immediate timing and by tests).</summary>
		/// <returns>The cycles the transfer took.</returns>
		int RunNow(GbaBus& bus, int channel);

		bool Active(int channel) const { return channels[channel].active; }
		const Channel& Get(int channel) const { return channels[channel]; }

		/// <summary>True when any channel is waiting for a slice.</summary>
		bool AnyPending() const;

	private:
		Channel channels[4]{};
		bool fifoRequest[2]{};		// a FIFO asked for a refill since the last service
		bool scanlineRequest = false;

		void Trigger(GbaBus& bus, int timing);
		int Perform(GbaBus& bus, int channel);
	};
}
