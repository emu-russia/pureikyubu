// GBA DMA: four channels (DMA0..DMA3) at 0x040000B0 .. 0x040000DF.
//
// Implemented from GBATEK "GBA DMA Transfers". The register file, channel by channel:
//
//   0B0h DMA0SAD_L  0B2h DMA0SAD_H  0B4h DMA0DAD_L  0B6h DMA0DAD_H
//   0B8h DMA0CNT_L  0BAh DMA0CNT_H
//   0BCh DMA1SAD_L  0BEh DMA1SAD_H  0C0h DMA1DAD_L  0C2h DMA1DAD_H
//   0C4h DMA1CNT_L  0C6h DMA1CNT_H
//   0C8h DMA2SAD_L  0CAh DMA2SAD_H  0CCh DMA2DAD_L  0CEh DMA2DAD_H
//   0D0h DMA2CNT_L  0D2h DMA2CNT_H
//   0D4h DMA3SAD_L  0D6h DMA3SAD_H  0D8h DMA3DAD_L  0DAh DMA3DAD_H
//   0DCh DMA3CNT_L  0DEh DMA3CNT_H
//
// SAD and DAD are 32bit registers, so each of them is written as two halfwords: the low one
// first and then the high one. The class holds the low half in Channel::source/::dest and uses
// Channel::sourceLatch/::destLatch as the "high half seen so far" while the channel is idle, so
// that the two halves come together into the address the transfer actually uses. (The latches
// are the natural place: they are unused until the enable bit is written, and the enable bit
// takes both halves.)
//
// DMAxCNT_H, the bit meanings (GBATEK "40000BAh - DMA0CNT_H - DMA 0 Control (R/W)"):
//
//   bit 0-4   not used
//   bit 5-6   destination address control  (0 = increment, 1 = decrement, 2 = fixed,
//                                           3 = increment + reload on repeat)
//   bit 7-8   source address control       (0 = increment, 1 = decrement, 2 = fixed,
//                                           3 = prohibited)
//   bit 9     repeat (0 = off, 1 = the transfer restarts on the next start condition)
//   bit 10    transfer type (0 = 16bit units, 1 = 32bit units; DMA3 only, DMA0-2 are 16bit)
//   bit 11    Game Pak DRQ (DMA3 only: the cartridge drives the request line)
//   bit 12-13 start timing (0 = immediately, 1 = VBlank, 2 = HBlank, 3 = special:
//             DMA0 prohibited, DMA1/DMA2 = sound FIFO, DMA3 = video capture)
//   bit 14    interrupt when the word count has been transferred
//   bit 15    enable
//
// Word count 0 means 0x4000 units for DMA0-2 (14bit registers) and 0x10000 for DMA3.
//
// Address ranges (GBATEK "DMA Transfers"): only DMA3 may transfer to/from the Game Pak (ROM,
// Flash, the EEPROM bit stream); DMA0-2 are restricted to internal memory (BIOS, EWRAM, IWRAM,
// I/O, palette, VRAM, OAM). No channel can reach the 8bit SRAM/Flash save window.
//
// Timing: "2N+2(n-1)S+xI" - the first unit pays a non-sequential access and the rest sequential
// ones, for the read and again for the write; the internal time is 2 cycles, or 4 when both the
// source and the destination sit in the Game Pak (GBATEK "Transfer Rate/Timing"). This
// implementation charges 4 cycles per 32bit unit and 2 per 16bit unit (both bus directions) plus
// the internal overhead, and hands the total to the bus so the CPU clock accounts for it.

#include "gba_dma.h"
#include "gba_bus.h"
#include "gba_apu.h"

namespace GBA
{
	namespace
	{
		// DMAxCNT_H
		const u16 DmaDestControl = 0x0060;		// bits 5-6
		const u16 DmaSourceControl = 0x0180;	// bits 7-8
		const u16 DmaRepeat = 0x0200;			// bit 9
		const u16 DmaWord = 0x0400;				// bit 10 (DMA3 only)
		const u16 DmaDrq = 0x0800;				// bit 11 (DMA3 only, Game Pak DRQ)
		const u16 DmaTiming = 0x3000;			// bits 12-13
		const u16 DmaIrq = 0x4000;				// bit 14
		const u16 DmaEnable = 0x8000;			// bit 15

		// The four channel bases, in channel order (twelve bytes apart).
		const u32 DmaBases[4] = { 0x0B0, 0x0BC, 0x0C8, 0x0D4 };

		// The offset of each register inside a channel, in halfwords: SAD (two), DAD (two),
		// CNT_L, CNT_H.
		const int RegSad = 0;
		const int RegDad = 1;
		const int RegCount = 2;
		const int RegControl = 3;

		// The FIFO destinations of the two direct-sound channels (GBATEK "GBA Sound").
		const u32 FifoA = 0x040000A0;
		const u32 FifoB = 0x040000A4;

		// Memory ranges, for the "which channels may touch what" rules of GBATEK "DMA Transfers".
		const u32 RomStart = 0x08000000;		// Game Pak ROM / Flash / EEPROM bit stream
		const u32 RomEnd = 0x0E000000;
		const u32 SramStart = 0x0E000000;		// SRAM/Flash, 8bit wide: no DMA channel can use it
		const u32 SramEnd = 0x10000000;

		bool IsCartridge(u32 address)
		{
			return address >= RomStart && address < RomEnd;
		}

		bool IsSram(u32 address)
		{
			return address >= SramStart && address < SramEnd;
		}

		/// <summary>BIOS ROM (0x00000000..0x00003FFF) is readable but never writable.</summary>
		bool IsBios(u32 address)
		{
			return address < BiosSize;
		}

		bool IsFifo(u32 address)
		{
			return address == FifoA || address == FifoB;
		}

		/// <summary>How many units a word count of zero means.</summary>
		int MaxUnits(int index)
		{
			return (index == 3) ? 0x10000 : 0x4000;
		}

		/// <summary>
		/// Put the two halves of a 32bit DMA address (SAD/DAD) back together: the low half lives
		/// in Channel::source/::dest, the high half in the channel's latch (see the note at the
		/// top of the file).
		/// </summary>
		u32 UnionAddress(u16 low, u32 highHalf)
		{
			return (u32)low | (highHalf & 0xFFFF0000u);
		}

		/// <summary>
		/// Decode a DMA register offset (relative to 0x04000000) into the channel and the
		/// register number, or return false when the offset is not a DMA register at all.
		/// </summary>
		bool DecodeDmaRegister(u32 offset, int& index, int& reg)
		{
			if ((offset & 1) != 0)
				return false;

			for (int channel = 0; channel < 4; channel++)
			{
				u32 within = offset - DmaBases[channel];
				if (within > 10)
					continue;

				// Codes: 0/2 = SAD (low/high), 4/6 = DAD, 8 = CNT_L, 10 = CNT_H.
				static const int codes[6] = { 0, 0, 1, 1, 2, 3 };
				index = channel;
				reg = codes[within / 2];
				return true;
			}

			return false;
		}

		/// <summary>The unit size of a 32bit transfer (GBATEK DMAxCNT_H bit 10, DMA3 only).</summary>
		bool UsesWords(const Dma::Channel& channel, int index)
		{
			return index == 3 && (channel.control & DmaWord) != 0;
		}

		/// <summary>True for a special-timing channel whose destination is a sound FIFO.</summary>
		bool IsFifoMode(const Dma::Channel& channel, int index)
		{
			if (index == 0)
				return false;

			if (((channel.control & DmaTiming) >> 12) != 3)
				return false;

			return IsFifo(channel.destLatch) || IsFifo(channel.dest);
		}

		/// <summary>
		/// The EEPROM protocol (GBATEK "GBA Cartridges", "EEPROM"): a DMA3 transfer of 16bit
		/// units that writes into the ROM area. The word count is the number of units: 9 for a
		/// 2bit command, 17 for a 6bit read command or a 14bit write command, 73 for a 64bit
		/// read and 81 for a 64bit write. A transfer that small aimed at the Game Pak is the
		/// EEPROM bit stream; anything larger (a real ROM copy) runs through the normal path.
		/// </summary>
		bool LooksLikeEeprom(int index, const Dma::Channel& channel, u32 count)
		{
			if (index != 3)
				return false;

			if ((channel.control & DmaTiming) != 0)
				return false;

			if (channel.control & DmaWord)
				return false;

			if (!IsCartridge(channel.dest) && !IsCartridge(channel.destLatch))
				return false;

			return count <= 17;
		}

		/// <summary>The number of units the current (or next) transfer moves.</summary>
		int UnitCount(const Dma::Channel& channel, int index)
		{
			if (channel.latched > 0)
				return channel.latched;

			if (channel.count == 0)
				return MaxUnits(index);

			return channel.count;
		}

		/// <summary>Reload DAD for a repeat channel using the increment+reload destination.</summary>
		void ApplyDestReload(Dma::Channel& channel, u32 fullDest)
		{
			if (((channel.control & DmaDestControl) >> 5) == 3)
				channel.destLatch = fullDest;
		}

		/// <summary>
		/// Run an EEPROM transfer to completion.
		///
		/// The games poll the DMA enable bit - or the cartridge's EEPROM state - in the
		/// instruction after the one that wrote CNT_H, so an EEPROM transfer cannot be deferred
		/// into a later bus slice: it runs here, inside the register write, before the CPU
		/// executes anything else. The EEPROM itself (the bit collection, the chip select and the
		/// 64bit shift out) belongs to Cart, which consumes the 16bit writes this transfer makes
		/// into the ROM window through Cart::WriteRom16 and completes the protocol as they
		/// arrive; the DMA engine only has to move the 9/17 halfwords.
		/// </summary>
		void RunEepromTransfer(Dma& dma, GbaBus& bus, int index)
		{
			dma.RunNow(bus, index);

			Dma::Channel& channel = const_cast<Dma::Channel&>(dma.Get(index));
			channel.active = false;
			channel.pending = false;
			channel.control &= (u16)~DmaEnable;
			channel.latched = (channel.count == 0) ? (u16)MaxUnits(index) : channel.count;
		}
	}

	void Dma::Reset()
	{
		for (int i = 0; i < 4; i++)
			channels[i] = Channel{};

		fifoRequest[0] = false;
		fifoRequest[1] = false;
		scanlineRequest = false;
	}

	u16 Dma::Read16(u32 offset, u16 openBus) const
	{
		// The bus may hand the access over as the offset relative to 0x04000000 or as the full
		// address; both decode the same way once the 0x04000000 base is gone.
		if (offset >= 0x04000000)
			offset -= 0x04000000;

		int index = 0;
		int reg = 0;
		if (!DecodeDmaRegister(offset, index, reg))
			return openBus;

		// Inside one channel the offset codes are 0/2 = SAD_L/H, 4/6 = DAD_L/H, 8 = CNT_L,
		// 10 = CNT_H; the two halves of a 32bit register both read their low half here, the
		// high half only through the idle-latch trick described at the top of the file.
		u32 within = offset - DmaBases[index];
		const Channel& channel = channels[index];

		switch (reg)
		{
		case RegSad:
			if (within == 2)
				return (u16)(channel.sourceLatch >> 16);
			return (u16)channel.source;

		case RegDad:
			if (within == 6)
				return (u16)(channel.destLatch >> 16);
			return (u16)channel.dest;

		case RegCount:
			return channel.count;

		default:
			return channel.control;
		}
	}

	void Dma::Write16(GbaBus& bus, u32 offset, u16 value)
	{
		// See Read16: accept the offset relative to 0x04000000 or the full address.
		if (offset >= 0x04000000)
			offset -= 0x04000000;

		int index = 0;
		int reg = 0;
		if (!DecodeDmaRegister(offset, index, reg))
		{
		}

		u32 within = offset - DmaBases[index];
		bool highHalf = (within == 2) || (within == 6);
		Channel& channel = channels[index];

		switch (reg)
		{
		case RegSad:		// DMAxSAD: 32bit, written as two halfwords
			if (highHalf)
				channel.sourceLatch = (channel.sourceLatch & 0xFFFF) | ((u32)value << 16);
			else
				channel.sourceLatch = (channel.sourceLatch & 0xFFFF0000) | value;
			channel.source = (u16)channel.sourceLatch;
			return;

		case RegDad:		// DMAxDAD: 32bit, written as two halfwords
			if (highHalf)
				channel.destLatch = (channel.destLatch & 0xFFFF) | ((u32)value << 16);
			else
				channel.destLatch = (channel.destLatch & 0xFFFF0000) | value;
			channel.dest = (u16)channel.destLatch;
			return;

		case RegCount:		// DMAxCNT_L: 14bit for DMA0-2, 16bit for DMA3
			channel.count = (index == 3) ? value : (u16)(value & 0x3FFF);
			return;

		default:
			break;
		}

		// DMAxCNT_H. The channel setup (latching SAD/DAD/CNT_L) happens on the 0 -> 1 edge of
		// the enable bit, so the old value is examined before the new one is stored.
		bool wasEnabled = (channel.control & DmaEnable) != 0;
		channel.control = (u16)(value & ~0x001F);	// bits 0-4 do not exist
		bool nowEnabled = (channel.control & DmaEnable) != 0;

		if (!nowEnabled)
		{
			// Software stopped the channel (only possible for a blanking/FIFO transfer: for all
			// others the CPU is held until the transfer finishes by itself).
			channel.active = false;
			channel.pending = false;
			return;
		}

		if (!wasEnabled)
		{
			// Enable changed 0 -> 1: reload SAD, DAD and CNT_L (GBATEK "Source and Destination
			// Address and Word Count Registers"). Both halves of SAD and DAD are in the latches
			// by now, so the transfer gets the full 32bit addresses.
			channel.latched = (channel.count == 0) ? (u16)MaxUnits(index) : channel.count;
			channel.active = true;
			channel.pending = false;

			if (((channel.control & DmaDestControl) >> 5) == 3)
			{
				// "Increment + reload": remember the address the transfer starts from, because
				// DAD is put back to it before every repeat.
				channel.destLatch = UnionAddress(channel.dest, channel.destLatch);
			}
		}

		u16 timing = (u16)((channel.control & DmaTiming) >> 12);

		if (timing != 0)
		{
			// VBlank (1), HBlank (2) and special (3) transfers are armed here and started by the
			// bus: OnVBlank/OnHBlank at the edge, OnFifoRequest/OnScanline for the special ones.
			return;
		}

		// Immediately: the transfer runs as part of this write. Because the setup above only
		// happens on the 0 -> 1 edge, a write that merely changes the address controls of an
		// already enabled channel does not restart it.
		if (LooksLikeEeprom(index, channel, (u32)UnitCount(channel, index)))
		{
			RunEepromTransfer(*this, bus, index);
			return;
		}

		RunNow(bus, index);
	}

	void Dma::OnScanline(GbaBus& bus)
	{
		// Video capture (GBATEK "Video Capture Mode (DMA3 only)"): start timing 3 on DMA3 with
		// the destination in VRAM. It works like an HBlank transfer - the number of units in the
		// word count register is copied once per scanline - and the capture runs for the visible
		// lines only, which is the window the PPU's scanline callback delimits.
		Channel& channel = channels[3];
		if (!channel.active || !(channel.control & DmaEnable))
			return;

		if (((channel.control & DmaTiming) >> 12) != 3)
			return;

		if (IsFifoMode(channel, 3))
			return;

		scanlineRequest = true;
		channel.pending = true;
		RunNow(bus, 3);
		channel.pending = false;
	}

	void Dma::OnFifoRequest(GbaBus& bus, int fifo)
	{
		// The APU raises this when its FIFO holds fewer than 16 bytes (GBATEK "GBA Sound
		// Channel A and B"). The channel that serves the FIFO is found through its destination
		// address, and the refill is completed here and now: the sound hardware keeps reading the
		// FIFO while a "pending" DMA would not have filled it yet.
		if (fifo < 0 || fifo > 1)
			return;

		fifoRequest[fifo] = true;

		for (int i = 1; i < 3; i++)
		{
			Channel& channel = channels[i];
			if (!channel.active || !(channel.control & DmaEnable))
				continue;

			if (((channel.control & DmaTiming) >> 12) != 3)
				continue;

			u32 target = (fifo == 0) ? FifoA : FifoB;
			if ((channel.destLatch & ~3u) != target)
				continue;

			// FIFO mode always moves four 32bit units, whatever CNT_L and bit 10 say.
			channel.latched = 4;
			channel.pending = true;
			RunNow(bus, i);
			channel.pending = false;
		}
	}

	void Dma::ServiceFifoRequests(GbaBus& bus)
	{
		// The refill already happened in OnFifoRequest, where the APU asked for it. This is the
		// bus's per-slice hook: it finishes any channel that is still marked pending, which a
		// frontend (or a test) can arrange by setting the request flags, and clears the state.
		for (int i = 1; i < 3; i++)
		{
			Channel& channel = channels[i];
			if (!channel.pending)
				continue;

			if (((channel.control & DmaTiming) >> 12) != 3 || !channel.active)
			{
				channel.pending = false;
				continue;
			}

			channel.latched = 4;
			RunNow(bus, i);
			channel.pending = false;
		}

		fifoRequest[0] = false;
		fifoRequest[1] = false;
	}

	int Dma::RunNow(GbaBus& bus, int index)
	{
		if (index < 0 || index > 3)
			return 0;

		Channel& channel = channels[index];

		// A test (or an immediate write) may ask for a transfer on a channel that was never
		// armed through CNT_H, so obviously stale latches are filled in from the registers.
		if (!channel.active || channel.latched <= 0)
		{
			channel.sourceLatch = UnionAddress(channel.source, channel.sourceLatch);
			channel.destLatch = UnionAddress(channel.dest, channel.destLatch);
			channel.latched = (channel.count == 0) ? (u16)MaxUnits(index) : channel.count;
			channel.active = true;
		}

		channel.pending = false;
		int cycles = Perform(bus, index);

		if (channel.active)
		{
			// Repeat: the enable bit stays set and the transfer restarts from the addresses it
			// was started with on the next start condition. "Upon Repeat: Reloads CNT_L, and
			// optionally DAD" (GBATEK "Source and Destination Address and Word Count Registers").
			channel.sourceLatch = UnionAddress(channel.source, channel.sourceLatch);
			channel.latched = (channel.count == 0) ? (u16)MaxUnits(index) : channel.count;
			ApplyDestReload(channel, UnionAddress(channel.dest, channel.destLatch));
		}

		return cycles;
	}

	bool Dma::AnyPending() const
	{
		for (int i = 0; i < 4; i++)
		{
			if (channels[i].pending)
				return true;
		}

		return false;
	}

	void Dma::Trigger(GbaBus& bus, int timing)
	{
		// The channels are examined in priority order (DMA0 first, GBATEK "DMA Transfers"): a
		// lower-priority channel would be held while a higher-priority one runs. Here each
		// triggered channel runs to completion in turn.
		for (int i = 0; i < 4; i++)
		{
			Channel& channel = channels[i];
			if (!channel.active || !(channel.control & DmaEnable))
				continue;

			if (((channel.control & DmaTiming) >> 12) != timing)
				continue;

			if (timing == 1 && LooksLikeEeprom(i, channel, (u32)UnitCount(channel, i)))
			{
				RunEepromTransfer(*this, bus, i);
				continue;
			}

			channel.pending = true;
			RunNow(bus, i);
			channel.pending = false;
		}
	}

	int Dma::Perform(GbaBus& bus, int index)
	{
		Channel& channel = channels[index];

		if (!channel.active || (channel.control & DmaEnable) == 0)
			return 0;

		bool word = UsesWords(channel, index);
		bool fifoMode = IsFifoMode(channel, index);

		if (fifoMode)
		{
			// "4 units of 32bits (16 bytes) are transferred, both Word Count register and DMA
			// Transfer Type bit are ignored" (GBATEK "Sound DMA (FIFO Timing Mode)"): a FIFO
			// refill is always a 32bit transfer, whatever CNT_H bit 10 says, so the source steps
			// by four bytes per unit even when the register asks for 16bit units.
			word = true;
		}

		int units = UnitCount(channel, index);

		if (units <= 0)
			return 0;

		u32 sourceControl = (channel.control & DmaSourceControl) >> 7;

		if (sourceControl == 3)
		{
			// "3 = Prohibited" (GBATEK DMAxCNT_H bits 7-8): nothing is transferred.
			Log(LogLevel::Warn, "DMA%i: prohibited source address control, transfer ignored", index);
			channel.active = false;
			channel.control &= (u16)~DmaEnable;
			return 0;
		}

		// A FIFO destination never moves (GBATEK "Sound DMA"). Every other destination follows
		// bits 5-6: 0 increments, 1 decrements, 2 is fixed, and 3 ("increment + reload") moves
		// exactly like 0 while the transfer runs - the only difference is that DAD is put back to
		// the address the transfer started from before the next repeat (see the end of Perform).
		u32 destControl = (channel.control & DmaDestControl) >> 5;
		if (fifoMode)
			destControl = 2;

		u32 source = channel.sourceLatch;
		u32 dest = channel.destLatch;
		int cycles = 0;

		u32 fifoWords[4] = { 0, 0, 0, 0 };
		int fifoCount = 0;

		for (int unit = 0; unit < units; unit++)
		{
			// Only DMA3 reaches the Game Pak; DMA0-2 read internal memory. Nothing can reach the
			// 8bit SRAM. An access a channel may not make returns the open bus value (all ones on
			// an idle bus) or is dropped, as the hardware does.
			bool readable = (index == 3) || (!IsCartridge(source) && !IsSram(source));
			bool writable = (index == 3) || !IsCartridge(dest);

			if (IsFifo(dest))
			{
				// The FIFO registers are write-only staging for the sound hardware, so the words
				// are collected here and handed to the APU through Apu::FifoDmaDone, the
				// interface the sound engine reads (writing them through the bus as well would
				// make the APU store the same 16 bytes twice).
				u32 value = readable ? bus.Read32(source) : 0xFFFFFFFF;
				if (fifoCount < 4)
					fifoWords[fifoCount++] = value;
			}
			else if (IsBios(dest))
			{
				// BIOS ROM is readable but never writable, so the write is dropped.
				Log(LogLevel::Warn, "DMA%i: write to the BIOS at %08X ignored", index, dest);
			}
			else if (writable)
			{
				if (word)
					bus.Write32(dest, readable ? bus.Read32(source) : 0xFFFFFFFF);
				else
					bus.Write16(dest, readable ? bus.Read16(source) : 0xFFFF);
			}
			else
			{
				Log(LogLevel::Warn, "DMA%i: destination %08X is not accessible, write ignored",
					index, dest);
			}

			switch (sourceControl)
			{
			case 0: source += word ? 4 : 2; break;		// increment
			case 1: source -= word ? 4 : 2; break;		// decrement
			default: break;								// fixed
			}

			switch (destControl)
			{
			case 0:							// increment
			case 3: dest += word ? 4 : 2; break;		// increment + reload per repeat
			case 1: dest -= word ? 4 : 2; break;		// decrement
			default: break;								// fixed
			}

			// "Of which, 1N+(n-1)S are read cycles, and the other 1N+(n-1)S are write cycles"
			// (GBATEK "Transfer Rate/Timing"): two bus cycles per unit, 4 bytes wide when the
			// transfer is a 32bit one.
			cycles += word ? 4 : 2;
		}

		// "The internal time for DMA processing is 2I (normally), or 4I (if both source and
		// destination are in gamepak memory area)" (GBATEK "Transfer Rate/Timing").
		cycles += (IsCartridge(channel.sourceLatch) && IsCartridge(channel.destLatch)) ? 4 : 2;

		if (fifoMode && fifoCount > 0)
		{
			int which = (channel.destLatch == FifoB) ? 1 : 0;

			// The collected words are the FIFO's new contents. Handing them over here (rather than
			// writing them through the bus) keeps the DMA from re-reading the FIFO register it is
			// writing: the refill is one 16 byte block, in transfer order.
			bus.apu.FifoDmaDone(which, fifoWords, fifoCount);

			bus.apu.ClearFifoRequest(which);
			fifoRequest[which] = false;
		}

		channel.sourceLatch = source;
		channel.destLatch = dest;
		// The transfer advanced the internal pointers: keep both the latches (what the next
		// trigger uses) and the register view in step, so a debugger that reads SAD/DAD back sees
		// the address the transfer ended on.
		// Keep the register view at the address the transfer started from while the channel
		// repeats: the next trigger re-latches SAD and (for "increment + reload") DAD from there,
		// so that is what the registers have to show. A one-shot transfer instead leaves the
		// registers showing where it ended up.
		if ((channel.control & DmaRepeat) == 0)
		{
			channel.source = source;
			channel.dest = dest;
		}
		channel.latched = 0;

		// The word count has been transferred: raise the channel's interrupt when bit 14 asks
		// for it (IF bits 8-11 are DMA0-3, see the InterruptBit enum).
		if (channel.control & DmaIrq)
			bus.irq.Raise((u16)(INT_DMA0 << index));

		if ((channel.control & DmaRepeat) == 0)
		{
			channel.control &= (u16)~DmaEnable;
			channel.active = false;
		}
		else if (((channel.control & DmaDestControl) >> 5) == 3)
		{
			// Repeat with "increment + reload": DAD goes back to the address the transfer was
			// started from, so the next start condition writes the same block again (GBATEK
			// "Upon Repeat: Reloads CNT_L, and optionally DAD").
			channel.destLatch = UnionAddress(channel.dest, channel.destLatch);
		}

		// The waitstates the transfer stole from the CPU.
		bus.AddWaitCycles(cycles);
		return cycles;
	}
}

