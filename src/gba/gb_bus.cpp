// The Game Boy address decoder and its on-chip peripherals. See gb_bus.h for the specification
// pages this is written from.

#include "gb_bus.h"

#include <cstring>

namespace GBA
{
	// ---------------------------------------------------------------------------------------
	// Reset and configuration
	// ---------------------------------------------------------------------------------------

	void GbBus::Reset()
	{
		// The power-up register values the Pan Docs list for the DMG (and the CGB where they
		// differ). The boot ROM overwrites most of them.
		interruptFlag = 0xE1;
		interruptEnable = 0x00;

		divider = 0xAB00;			// DIV reads 0xAB right after power-up
		tima = 0x00;
		tma = 0x00;
		tac = 0xF8;					// bits 3-7 read as ones; the timer is off
		timaReloading = false;
		timaReloadDelay = 0;
		lastTimerBit = false;

		joypadSelect = 0x30;		// both halves deselected: nothing is pulled low
		stopped = false;

		serialData = 0x00;
		serialControl = 0x7E;		// bit 7 clear (no transfer), bit 1 selects the normal clock
		serialBitTimer = 0;
		serialBitsLeft = 0;
		serialActive = false;
		serialInternalClock = false;
		serialTransfers = 0;

		dmaRegister = 0xFF;
		oamDmaCycles = 0;
		oamDmaSource = 0;

		hdma1 = hdma2 = hdma3 = hdma4 = hdma5 = 0xFF;
		hdmaActive = false;
		hdmaHblank = false;
		hdmaSource = 0xFFFF;
		hdmaDest = 0xFFFF;
		hdmaBlocks = 0;

		key1 = 0x7E;
		vbk = 0xFE;
		svbk = 0xF8;
		opri = 0x00;

		doubleSpeed = false;
		clockSpeed = GbCyclesPerSecond;
		totalCycles = 0;

		for (int bank = 0; bank < 8; bank++)
			for (int i = 0; i < 0x1000; i++)
				wram[bank][i] = 0x00;
		for (int i = 0; i < 0x7F; i++)
			hram[i] = 0x00;
	}

	void GbBus::SetCgb(bool value)
	{
		cgb = value;
		ppu.SetCgb(value);

		if (!value)
		{
			key1 = 0x7E;
			vbk = 0xFE;
			svbk = 0xF8;
		}
	}

	void GbBus::SetDoubleSpeed(bool enabled)
	{
		// KEY1 (0xFF4D): bit 7 is the current speed, bit 0 the switch request. Only a CGB can
		// change the speed, and the switch itself happens in the STOP instruction's hook.
		doubleSpeed = enabled;
		cpu.doubleSpeed = enabled;

		if (enabled)
			key1 |= 0x80;
		else
			key1 &= (u8)~0x80;
	}

	void GbBus::SetBootRom(const u8* image, u32 size)
	{
		// The two sizes the hardware has: the DMG's 256 byte boot ROM and the CGB's 2304 byte one
		// (Pan Docs "Power Up Sequence": the CGB's is split in two with the cartridge header in
		// the middle, which is why the image is kept whole rather than truncated).
		u32 count = (size < sizeof(bootRom)) ? size : (u32)sizeof(bootRom);
		if (image != nullptr && count != 0)
		{
			memcpy(bootRom, image, count);
			bootRomSize = count;
			bootRomLoaded = true;
		}
		else
		{
			memset(bootRom, 0x00, sizeof(bootRom));
			bootRomSize = 0;
			bootRomLoaded = false;
		}
	}

	void GbBus::MapBootRom(bool mapped)
	{
		// The boot ROM is mapped at 0x0000: 0x100 bytes on a DMG and, on a CGB, the two halves
		// 0x0000-0x00FF and 0x0200-0x08FF with the cartridge header in the middle (Pan Docs
		// "Power Up Sequence"). The program unmaps it by writing anything to 0xFF50.
		bootRomMapped = mapped && bootRomLoaded;
	}

	// ---------------------------------------------------------------------------------------
	// The memory map
	// ---------------------------------------------------------------------------------------

	u8 GbBus::Peek(u16 address) const
	{
		if (address < 0x8000)
			return cart.ReadRom(address);
		if (address < 0xA000)
			return ppu.VramBank(vbk & 0x01)[address - 0x8000];
		if (address < 0xC000)
			return cart.ReadRam(address);
		if (address < 0xE000)
			return (address < 0xD000) ? wram[0][address - 0xC000]
				: wram[cgb ? (svbk & 0x07) : 1][address - 0xD000];
		if (address < 0xFE00)
			return Peek((u16)(address - 0x2000));		// echo RAM
		if (address < 0xFEA0)
			return ppu.Oam()[address - 0xFE00];
		if (address < 0xFF00)
			return 0x00;								// the unusable region
		if (address < 0xFF80)
			return PeekIo(address);
		if (address < 0xFFFF)
			return hram[address - 0xFF80];
		return interruptEnable;
	}

	u8 GbBus::PeekIo(u16 address) const
	{
		// The same decoding as ReadIo, but without the side effects: a const view of the register
		// file for the emulator's own report and for the tests.
		if (address >= 0xFF10 && address <= 0xFF3F)
			return apu.ReadRegister(address);
		// 0xFF46 (DMA) sits between LYC (0xFF45) and BGP (0xFF47) and belongs to the bus, not
		// to the PPU, so it is excluded from the PPU's register range.
		if (address >= 0xFF40 && address <= 0xFF4B && address != 0xFF46)
			return ppu.ReadRegister(address);

		switch (address)
		{
		case 0xFF00: return JoypadValue();
		case 0xFF01: return serialData;
		case 0xFF02: return serialControl;
		case 0xFF04: return (u8)(divider >> 8);
		case 0xFF05: return tima;
		case 0xFF06: return tma;
		case 0xFF07: return (u8)(0xF8 | (tac & 0x07));
		case 0xFF0F: return interruptFlag;
		case 0xFF46: return dmaRegister;
		case 0xFF4D: return cgb ? key1 : 0xFF;
		case 0xFF4F: return cgb ? vbk : 0xFF;
		case 0xFF50: return 0xFF;
		case 0xFF51: return hdma1;
		case 0xFF52: return hdma2;
		case 0xFF53: return hdma3;
		case 0xFF54: return hdma4;
		case 0xFF55: return hdma5;
		case 0xFF6C: return cgb ? opri : 0xFF;
		case 0xFF70: return cgb ? svbk : 0xFF;
		default: return 0xFF;
		}
	}

	u8 GbBus::JoypadValue() const
	{
		// The joypad register (Pan Docs "Joypad Input"): bits 4-5 select which half of the matrix
		// is read, the low nibble is low for a pressed button. With neither half selected the low
		// nibble reads as ones.
		u8 value = (u8)(0xC0 | joypadSelect | 0x0F);

		if ((joypadSelect & 0x10) == 0)
		{
			// bit 4 low: the direction keys (P10)
			if (pressedKeys & GbButtonRight) value &= (u8)~0x01;
			if (pressedKeys & GbButtonLeft) value &= (u8)~0x02;
			if (pressedKeys & GbButtonUp) value &= (u8)~0x04;
			if (pressedKeys & GbButtonDown) value &= (u8)~0x08;
		}
		if ((joypadSelect & 0x20) == 0)
		{
			// bit 5 low: the action buttons (P11)
			if (pressedKeys & GbButtonA) value &= (u8)~0x01;
			if (pressedKeys & GbButtonB) value &= (u8)~0x02;
			if (pressedKeys & GbButtonSelect) value &= (u8)~0x04;
			if (pressedKeys & GbButtonStart) value &= (u8)~0x08;
		}

		return value;
	}

	u8 GbBus::ReadIo(u16 address)
	{
		if (address >= 0xFF10 && address <= 0xFF3F)
			return apu.ReadRegister(address);

		// 0xFF46 (DMA) is the bus's register between the PPU's LYC and BGP.
		if (address >= 0xFF40 && address <= 0xFF4B && address != 0xFF46)
			return ppu.ReadRegister(address);

		// The CGB colour palette registers: BCPS/BCPD at 0xFF68/0xFF69 and OCPS/OCPD at
		// 0xFF6A/0xFF6B (Pan Docs "CGB Registers"). They are the only way a CGB game can define
		// its colours, so the bus has to pass them through to the PPU - which ignores them on a
		// monochrome console, where the addresses read back as 0xFF anyway.
		if (address >= 0xFF68 && address <= 0xFF6B)
			return ppu.ReadRegister(address);

		switch (address)
		{
		case 0xFF00: return JoypadValue();
		case 0xFF01: return serialData;
		case 0xFF02: return serialControl;
		case 0xFF04: return (u8)(divider >> 8);
		case 0xFF05: return tima;
		case 0xFF06: return tma;
		case 0xFF07: return (u8)(0xF8 | (tac & 0x07));
		case 0xFF0F: return interruptFlag;
		case 0xFF46: return dmaRegister;
		case 0xFF4D:
			// KEY1 (Pan Docs "CGB Registers"): bit 7 the current speed (0 normal, 1 double),
			// bit 0 the switch request, bits 1-6 unused and reading as ones.
			return cgb ? key1 : 0xFF;
		case 0xFF4F: return cgb ? vbk : 0xFF;
		case 0xFF50: return 0xFF;
		case 0xFF51: return hdma1;
		case 0xFF52: return hdma2;
		case 0xFF53: return hdma3;
		case 0xFF54: return hdma4;
		case 0xFF55:
			// HDMA5: while an HBlank DMA is running bit 7 is set and bits 0-6 count the 16 byte
			// blocks left minus one; otherwise the register keeps its last value (0xFF once a
			// transfer has finished, Pan Docs "CGB Registers").
			if (hdmaActive && hdmaHblank && hdmaBlocks > 0)
				return (u8)(0x80 | ((hdmaBlocks - 1) & 0x7F));
			return hdma5;
		case 0xFF6C: return cgb ? opri : 0xFF;
		case 0xFF70: return cgb ? svbk : 0xFF;
		default: return 0xFF;
		}
	}

	u8 GbBus::ReadByte(u16 address)
	{
		// The boot ROM overlay (Pan Docs "Power Up Sequence": the boot ROM is mapped at power-up
		// and unmapped by a write to 0xFF50). The DMG's ROM is one 256 byte page at 0x0000; the
		// CGB's is split in two with the cartridge header in the middle - 0x0000-0x00FF and
		// 0x0200-0x08FF - so the header at 0x0100-0x01FF stays readable the whole time.
		if (bootRomMapped && address < bootRomSize
			&& (address < 0x100 || address >= 0x200))
			return bootRom[address];

		if (address < 0x8000)
			return cart.ReadRom(address);

		if (address < 0xA000)
		{
			// VRAM is inaccessible during mode 3 (Pan Docs "Accessing VRAM and OAM"): the read
			// returns 0xFF.
			if (ppu.VramBlocked())
				return 0xFF;
			return ppu.VramBank(vbk & 0x01)[address - 0x8000];
		}

		if (address < 0xC000)
			return cart.ReadRam(address);

		if (address < 0xE000)
		{
			// WRAM: 0xC000..0xCFFF is always bank 0, 0xD000..0xDFFF is the bank SVBK selects
			// (bank 1 on a DMG, Pan Docs "CGB Registers").
			if (address < 0xD000)
				return wram[0][address - 0xC000];
			return wram[cgb ? (svbk & 0x07) : 1][address - 0xD000];
		}

		if (address < 0xFE00)
		{
			// Echo RAM: a mirror of 0xC000..0xDDFF (Pan Docs "Memory Map").
			return ReadByte((u16)(address - 0x2000));
		}

		if (address < 0xFEA0)
		{
			// OAM is inaccessible in modes 2 and 3 (Pan Docs "Accessing VRAM and OAM"), except
			// while an OAM DMA is writing it.
			if (ppu.OamBlocked() && oamDmaCycles == 0)
				return 0xFF;
			return ppu.Oam()[address - 0xFE00];
		}

		if (address < 0xFF00)
			return 0x00;			// the unusable region (Pan Docs: reads are undefined)

		if (address < 0xFF80)
			return ReadIo(address);

		if (address < 0xFFFF)
			return hram[address - 0xFF80];

		return interruptEnable;
	}

	void GbBus::WriteByte(u16 address, u8 value)
	{
		if (address < 0x8000)
		{
			cart.WriteRom(address, value);
			return;
		}

		if (address < 0xA000)
		{
			if (!ppu.VramBlocked())
				ppu.VramBank(vbk & 0x01)[address - 0x8000] = value;
			return;
		}

		if (address < 0xC000)
		{
			cart.WriteRam(address, value);
			return;
		}

		if (address < 0xE000)
		{
			if (address < 0xD000)
				wram[0][address - 0xC000] = value;
			else
				wram[cgb ? (svbk & 0x07) : 1][address - 0xD000] = value;
			return;
		}

		if (address < 0xFE00)
		{
			WriteByte((u16)(address - 0x2000), value);
			return;
		}

		if (address < 0xFEA0)
		{
			if (!ppu.OamBlocked() || oamDmaCycles != 0)
				ppu.Oam()[address - 0xFE00] = value;
			return;
		}

		if (address < 0xFF00)
			return;					// the unusable region

		if (address < 0xFF80)
		{
			WriteIo(address, value);
			return;
		}

		if (address < 0xFFFF)
		{
			hram[address - 0xFF80] = value;
			return;
		}

		interruptEnable = value;
	}

	void GbBus::WriteIo(u16 address, u8 value)
	{
		if (address >= 0xFF10 && address <= 0xFF3F)
		{
			apu.WriteRegister(address, value);
			return;
		}

		// 0xFF46 (DMA) is the bus's register between the PPU's LYC and BGP.
		if (address >= 0xFF40 && address <= 0xFF4B && address != 0xFF46)
		{
			ppu.WriteRegister(address, value);
			return;
		}

		// The CGB colour palette registers (see ReadIo): without them a CGB game cannot define a
		// single colour, so the whole picture - sprites included - stays on the palette the
		// machine installed at reset.
		if (address >= 0xFF68 && address <= 0xFF6B)
		{
			ppu.WriteRegister(address, value);
			return;
		}

		switch (address)
		{
		case 0xFF00:
			// Only bits 4-5 are writable; the rest of the register belongs to the hardware.
			joypadSelect = (u8)(value & 0x30);
			// A button that is already held can request the joypad interrupt when the program
			// selects its half of the matrix (Pan Docs "Interrupt Sources").
			if ((joypadSelect & 0x30) != 0x30)
				RequestInterrupt(GbIntJoypad);
			break;

		case 0xFF01: serialData = value; break;
		case 0xFF02:
		{
			// SC (Pan Docs "Serial Data Transfer"): bit 7 starts a transfer, bit 0 selects the
			// clock (0 = the external clock, 1 = this machine's internal clock) and on a CGB
			// bit 1 selects the fast clock.
			serialControl = value;
			if ((value & 0x80) && !serialActive)
			{
				serialActive = true;
				serialInternalClock = (value & 0x01) != 0;
				serialShiftOut = serialData;
				serialShiftIn = 0x00;
				serialBitsLeft = 8;
				serialBitTimer = 0;
				lastSent = serialData;
			}
			break;
		}

		case 0xFF04:
			// DIV is the top byte of a 16-bit counter; writing anything resets the whole counter
			// (Pan Docs "Timer and Divider Registers"). The reset can produce a falling edge on
			// the timer input, which the next TickTimer notices.
			divider = 0x0000;
			break;
		case 0xFF05: tima = value; break;
		case 0xFF06:
			// A TMA write in the same clock as the reload transfers the old value; the reload
			// window is modelled by TickTimer reading TMA when the delay expires.
			tma = value;
			break;
		case 0xFF07:
			// TAC: only bits 0-2 are writable. Turning the timer on or changing the clock select
			// can increment TIMA once - the falling edge the chosen divider bit sees during the
			// write, which the edge detector produces for free.
			tac = (u8)(0xF8 | (value & 0x07));
			break;
		case 0xFF0F:
			// IF: the low five bits are the interrupt flags; the unused bits read as ones.
			interruptFlag = (u8)(0xE0 | (value & 0x1F));
			break;

		case 0xFF46:
			StartOamDma(value);
			break;

		case 0xFF4D:
			if (cgb)
			{
				// KEY1: bit 0 requests a speed switch, which the next STOP performs (Pan Docs
				// "CGB Registers").
				key1 = (u8)((key1 & 0x80) | (value & 0x01) | 0x7E);
			}
			break;

		case 0xFF4F:
			if (cgb)
			{
				vbk = (u8)(0xFE | (value & 0x01));
				ppu.WriteRegister(0xFF4F, value);
			}
			break;

		case 0xFF50:
			// Writing anything here unmaps the boot ROM (Pan Docs "Memory Map"). The value is
			// not stored; only the fact that the write happened matters.
			bootRomMapped = false;
			break;

		case 0xFF51: hdma1 = value; break;
		case 0xFF52: hdma2 = value; break;
		case 0xFF53: hdma3 = value; break;
		case 0xFF54: hdma4 = value; break;
		case 0xFF55: StartHdma(value); break;

		case 0xFF6C:
			if (cgb)
				opri = (u8)(value & 0x01);
			break;

		case 0xFF70:
			if (cgb)
			{
				// SVBK selects the WRAM bank at 0xD000; a written 0 selects bank 1 (Pan Docs
				// "CGB Registers").
				u8 bank = (u8)(value & 0x07);
				svbk = (u8)(0xF8 | (bank == 0 ? 1 : bank));
			}
			break;

		default:
			break;
		}
	}

	// ---------------------------------------------------------------------------------------
	// Running
	// ---------------------------------------------------------------------------------------

	int GbBus::Run(int cycles)
	{
		// The CPU runs one M-cycle for every four system clocks in normal speed and for every
		// two in double speed. STOP and HALT make it consume its M-cycle without doing anything,
		// so the rest of the machine still advances (which is what a frame needs).
		int perInstruction = doubleSpeed ? 2 : 4;
		int used = 0;

		while (cycles - used >= perInstruction)
		{
			int mcycles = cpu.Step();
			used += mcycles * perInstruction;

			// An OAM DMA stalls the CPU for its whole transfer (Pan Docs "OAM DMA": 160
			// M-cycles, one byte per clock); Run() stops issuing instructions until it is done.
			if (oamDmaCycles != 0)
				break;
		}

		if (used == 0)
		{
			// Nothing ran (HALT, STOP or a DMA): the caller still has to let the rest of the
			// machine advance, so the whole request goes to the devices.
			TickDevices(cycles);
			return 0;
		}

		TickDevices(used);
		return used;
	}

	void GbBus::TickDevices(int cycles)
	{
		if (cycles <= 0)
			return;

		totalCycles += (u64)cycles;

		TickTimer(cycles);

		// The LCD and the sound controller run on the same clock as the timer; the PPU works out
		// its own dots from the speed mode, the APU is clocked at the system rate.
		u8 request = ppu.Tick(cycles, doubleSpeed);
		if (request & 0x01)
			RequestInterrupt(GbIntVBlank);
		if (request & 0x02)
			RequestInterrupt(GbIntStat);

		apu.Tick(cycles);
		TickSerial(cycles);

		// The OAM DMA moves one byte per system clock (its 160 bytes take 640 clocks, which is
		// the 160 M-cycles the hardware spends on it).
		if (oamDmaCycles > 0)
		{
			for (int i = 0; i < cycles && oamDmaCycles > 0; i++)
			{
				CopyOamDmaByte();
				oamDmaCycles--;
			}
		}

		// The HDMA block transfer happens once per HBlank (Pan Docs "CGB Registers": HDMA copies
		// 16 bytes per HBlank, nothing during VBlank), so it is driven by the edge into mode 0
		// rather than by the level: a batch of ticks inside one HBlank must transfer one block,
		// not one per batch.
		int mode = ppu.LcdEnabled() ? ppu.Mode() : -1;
		if (hdmaActive && hdmaHblank && mode == 0 && hdmaLastMode != 0)
			TickHdma();
		hdmaLastMode = mode;
	}

	void GbBus::WakeFromStop()
	{
		stopped = false;
		cpu.stopped = false;
	}

	// ---------------------------------------------------------------------------------------
	// The timer
	// ---------------------------------------------------------------------------------------

	int GbBus::TimerDividerBit(u8 control)
	{
		// The clock select picks which bit of the divider counter increments TIMA (Pan Docs
		// "Timer and Divider Registers"): 00 = bit 9 (4096 Hz), 01 = bit 3 (262144 Hz),
		// 10 = bit 5 (65536 Hz), 11 = bit 7 (16384 Hz).
		switch (control & 0x03)
		{
		case 0: return 9;
		case 1: return 3;
		case 2: return 5;
		default: return 7;
		}
	}

	bool GbBus::TimerInputHigh() const
	{
		if ((tac & 0x04) == 0)
			return false;
		return (divider & (1 << TimerDividerBit(tac))) != 0;
	}

	void GbBus::TickTimer(int systemCycles)
	{
		for (int i = 0; i < systemCycles; i++)
		{
			// DIV is the top byte of a counter clocked at 16384 Hz, i.e. every 256 system clocks.
			divider = (u16)(divider + 1);

			// TIMA counts the falling edges of the selected divider bit. Testing the bit before
			// and after the increment is what makes the TAC write glitch (a write that turns the
			// timer on while the selected bit is already high produces an edge) and the DIV write
			// glitch fall out without special cases.
			bool high = TimerInputHigh();
			if (lastTimerBit && !high)
			{
				if (!timaReloading)
				{
					if (tima == 0xFF)
					{
						// The overflow: TIMA reads 0 for the four clocks it takes the hardware
						// to reload it from TMA, and the interrupt is requested then (Pan Docs
						// "Timer Obscure Behaviour").
						tima = 0x00;
						timaReloading = true;
						timaReloadDelay = 4;
					}
					else
					{
						tima++;
					}
				}
			}
			lastTimerBit = high;

			if (timaReloading && --timaReloadDelay <= 0)
			{
				tima = tma;
				timaReloading = false;
				RequestInterrupt(GbIntTimer);
			}
		}
	}

	// ---------------------------------------------------------------------------------------
	// The serial port
	// ---------------------------------------------------------------------------------------

	void GbBus::TickSerial(int systemCycles)
	{
		if (!serialActive)
			return;

		// The bit rate: 8192 Hz (one bit per 512 system clocks) with the normal clock, 262144 Hz
		// (one bit per 16 clocks) with the CGB's fast clock (Pan Docs "Serial Data Transfer").
		// The rate is in system clocks, so double speed needs no special case here.
		int bitPeriod = SerialFastClock() ? 16 : 512;

		if (!serialInternalClock)
		{
			// This machine is the slave: the master clocks it through SerialClock() and
			// SerialTransferBit(), so there is nothing to do on this side.
			return;
		}

		serialBitTimer += systemCycles;
		while (serialBitTimer >= bitPeriod && serialBitsLeft > 0)
		{
			serialBitTimer -= bitPeriod;

			// The cable is two shift registers wired together: this machine shifts its byte out
			// of bit 7 while the peer's byte comes in at bit 0. An unconnected cable reads all
			// ones (the serial input floats high).
			u8 incoming = 0xFF;
			if (serialPeer != nullptr)
			{
				incoming = serialPeer->PeerByte();
				if (serialPeer->PeerReceiving())
					serialPeer->PeerClock((u8)((serialShiftOut >> 7) & 1));
			}

			serialShiftIn = (u8)((serialShiftIn << 1) | (incoming & 1));
			serialShiftOut = (u8)(serialShiftOut << 1);
			serialBitsLeft--;
		}

		if (serialBitsLeft == 0)
			FinishSerial();
	}

	void GbBus::SerialTransferBit()
	{
		// One bit of an externally clocked transfer: the master called this after shifting.
		if (!serialActive || serialInternalClock || serialBitsLeft <= 0)
			return;

		serialShiftIn = (u8)((serialShiftIn << 1) | ((serialShiftOut >> 7) & 1));
		serialShiftOut = (u8)(serialShiftOut << 1);
		serialBitsLeft--;
		if (serialBitsLeft == 0)
			FinishSerial();
	}

	void GbBus::FinishSerial()
	{
		// The completed transfer: the received byte lands in SB, bit 7 of SC clears and the
		// serial interrupt is requested (Pan Docs "Serial Data Transfer").
		serialData = serialShiftIn;
		lastReceived = serialShiftIn;
		serialControl &= (u8)~0x80;
		serialActive = false;
		serialTransfers++;
		RequestInterrupt(GbIntSerial);
	}

	bool GbBus::InterruptsPending() const
	{
		return (interruptFlag & interruptEnable & 0x1F) != 0;
	}

	// ---------------------------------------------------------------------------------------
	// DMA
	// ---------------------------------------------------------------------------------------

	void GbBus::StartOamDma(u8 value)
	{
		// OAM DMA (Pan Docs "OAM DMA"): the register's value is the source page, i.e. the
		// transfer copies 160 bytes from (value << 8) into 0xFE00..0xFE9F.
		dmaRegister = value;
		oamDmaSource = (u16)(value << 8);
		oamDmaCycles = 160;			// 160 M-cycles, one byte per clock

		// The first byte is copied at once; the rest follow while the CPU is stalled.
		CopyOamDmaByte();
		oamDmaCycles--;
		if (oamDmaCycles < 0)
			oamDmaCycles = 0;
	}

	void GbBus::CopyOamDmaByte()
	{
		int index = 160 - oamDmaCycles;
		if (index < 0 || index >= 160)
			return;

		u16 source = (u16)(oamDmaSource + index);
		u8 value;

		// The source can be any region; reading through Peek keeps the VRAM bank selection and
		// the cartridge in the picture. The unusable region reads as 0xFF.
		if (source >= 0xFEA0 && source < 0xFF00)
			value = 0xFF;
		else
			value = Peek(source);

		ppu.Oam()[index] = value;
	}

	void GbBus::StartHdma(u8 value)
	{
		// HDMA1-5 (Pan Docs "CGB Registers"): HDMA5's bit 7 selects the mode. Zero means a
		// general purpose DMA that copies everything at once (and stalls the CPU); one means an
		// HBlank DMA that copies 16 bytes per HBlank.
		if (!cgb)
			return;

		int blocks = (value & 0x7F) + 1;

		if (hdmaActive && hdmaHblank && (value & 0x80) == 0)
		{
			// Writing bit 7 clear while an HBlank DMA runs stops it, and HDMA5 then reads back
			// with bit 7 set and the remaining blocks (Pan Docs).
			hdmaActive = false;
			hdmaHblank = false;
			hdma5 = (u8)(0x80 | ((hdmaBlocks - 1) & 0x7F));
			return;
		}

		// The source and destination come from the four address registers, with the low four
		// bits dropped (Pan Docs: "the lower 4 bits are ignored"); the destination is always in
		// VRAM, bank 0.
		hdmaSource = (u16)(((hdma1 << 8) | hdma2) & 0xFFF0);
		hdmaDest = (u16)(0x8000 | (((hdma3 << 8) | hdma4) & 0x1FF0));
		hdmaBlocks = blocks;

		if ((value & 0x80) == 0)
		{
			// General purpose DMA: all the blocks now, and the CPU is halted for the duration.
			hdmaActive = false;
			hdmaHblank = false;
			while (hdmaBlocks > 0)
				TickHdma();
			hdma5 = 0xFF;
			hdmaLastMode = ppu.LcdEnabled() ? ppu.Mode() : -1;
		}
		else
		{
			hdmaActive = true;
			hdmaHblank = true;
			hdma5 = (u8)(0x80 | ((hdmaBlocks - 1) & 0x7F));
		}
	}

	void GbBus::TickHdma()
	{
		if (hdmaBlocks <= 0)
		{
			hdmaActive = false;
			hdmaHblank = false;
			hdma5 = 0xFF;
			return;
		}

		for (int i = 0; i < 16; i++)
		{
			u16 source = (u16)(hdmaSource + i);
			u16 dest = (u16)((hdmaDest + i) & 0x1FFF);

			// The source may be the cartridge ROM, the cartridge RAM or the WRAM; the
			// destination is always VRAM, bank 0.
			u8 value;
			if (source < 0x8000)
				value = cart.ReadRom(source);
			else if (source < 0xA000)
				value = 0xFF;		// HDMA never reads VRAM
			else if (source < 0xC000)
				value = cart.ReadRam(source);
			else if (source < 0xE000)
				value = (source < 0xD000) ? wram[0][source - 0xC000]
					: wram[cgb ? (svbk & 0x07) : 1][source - 0xD000];
			else
				value = Peek(source);

			ppu.VramBank(0)[dest] = value;
		}

		// The addresses wrap within their regions (Pan Docs gives the masks: the source stays
		// inside 0x0000..0x7FF0 and the destination inside 0x8000..0x9FF0).
		hdmaSource = (u16)((hdmaSource + 16) & 0x7FF0);
		hdmaDest = (u16)(0x8000 | ((hdmaDest + 16) & 0x1FF0));

		hdmaBlocks--;
		if (hdmaBlocks <= 0)
		{
			hdmaActive = false;
			hdmaHblank = false;
			hdma5 = 0xFF;
		}
		else
		{
			hdma5 = (u8)(0x80 | ((hdmaBlocks - 1) & 0x7F));
		}
	}
}
