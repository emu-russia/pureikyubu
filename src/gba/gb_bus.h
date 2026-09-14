// The Game Boy address bus: the whole memory map (ROM, VRAM, external RAM, WRAM, echo RAM, OAM,
// the unusable region, the I/O registers, HRAM and IE), the timer and its divider, the joypad,
// the serial port with the two machine link, the OAM DMA and the CGB's HDMA/GDMA and WRAM bank
// register.
//
// Written from the Pan Docs "Memory Map", "Timer and Divider Registers", "Timer Obscure
// Behaviour", "Serial Data Transfer (Link Cable)", "Joypad Input", "CGB Registers" (KEY1, VBK,
// HDMA1-5, RP, SVBK) and the Game Boy Programming Manual for the DMA behaviour.
//
// The clock: this module counts the *system* clocks - the 4.194304 MHz the timer, the LCD and the
// sound controller run on - and lets the CPU have one M-cycle for every four of them (two in CGB
// double speed). DIV is the top byte of a 16-bit counter clocked at 16384 Hz, TIMA counts the
// falling edges of one of that counter's bits chosen by TAC, and the overflow takes the four-clock
// delay the Pan Docs describe (which is what makes the "TIMA reads 0" window observable).

#pragma once

#include "gba_types.h"
#include "gb_cpu.h"
#include "gb_ppu.h"
#include "gb_apu.h"
#include "gb_cart.h"

namespace GBA
{
	// ---------------------------------------------------------------------------------------
	// Interrupt bits (Pan Docs "Interrupts": IF at 0xFF0F, IE at 0xFFFF)
	// ---------------------------------------------------------------------------------------

	enum GbInterrupt : u8
	{
		GbIntVBlank = 0x01,
		GbIntStat = 0x02,
		GbIntTimer = 0x04,
		GbIntSerial = 0x08,
		GbIntJoypad = 0x10,
	};

	/// <summary>The buttons of the joypad, in the bit order of the P1 register.</summary>
	enum GbButton : u8
	{
		GbButtonRight = 0x01,
		GbButtonLeft = 0x02,
		GbButtonUp = 0x04,
		GbButtonDown = 0x08,
		GbButtonA = 0x10,
		GbButtonB = 0x20,
		GbButtonSelect = 0x40,
		GbButtonStart = 0x80,
	};

	// ---------------------------------------------------------------------------------------
	// The serial port and the link cable
	// ---------------------------------------------------------------------------------------

	/// <summary>
	/// The other end of the serial cable. A machine implements this; so does a test that checks a
	/// two machine transfer. The cable is two shift registers wired together, so the machine that
	/// drives the clock shifts both bytes and tells the other end with PeerClock().
	/// </summary>
	class GbSerialPeer
	{
	public:
		virtual ~GbSerialPeer() = default;

		/// <summary>True when the peer has a transfer running (so it is on the cable).</summary>
		virtual bool PeerActive() = 0;

		/// <summary>True when the peer is listening to an external clock (it is the slave).</summary>
		virtual bool PeerReceiving() = 0;

		/// <summary>The byte currently in the peer's shift register (what it is sending).</summary>
		virtual u8 PeerByte() = 0;

		/// <summary>Shift one bit into the peer's shift register.</summary>
		virtual void PeerClock(u8 bit) = 0;
	};

	class GbBus : public GbCpuBus
	{
	public:
		// The sub-devices. The machine owns them; the bus only routes their registers.
		GbCpu cpu;
		GbPpu ppu;
		GbApu apu;
		GbCart cart;

		// -- the CPU's interface -------------------------------------------------------------

		u8 ReadByte(u16 address) override;
		void WriteByte(u16 address, u8 value) override;
		bool InterruptsPending() const override;

		/// <summary>Read a byte the way the emulator's own report does, ignoring the boot ROM
		/// overlay and the DMA conflicts.</summary>
		u8 Peek(u16 address) const;

		// -- configuration -------------------------------------------------------------------

		void SetCgb(bool cgb);
		bool Cgb() const { return cgb; }

		/// <summary>The system clock (4.194304 MHz) the timer, the LCD and the sound run on.</summary>
		int ClockSpeed() const { return clockSpeed; }

		/// <summary>The CGB's double speed mode (KEY1 bit 7); the machine sets it on a switch.</summary>
		void SetDoubleSpeed(bool enabled);
		bool DoubleSpeed() const { return doubleSpeed; }

		// -- the machine's running -----------------------------------------------------------

		void Reset();

		/// <summary>
		/// Run `cycles` system clocks: the CPU for as many M-cycles as those clocks allow and
		/// every device for all of them. Returns the number of clocks actually consumed, or 0
		/// when the CPU could not run (HALT, STOP or a DMA in progress) and the caller has to
		/// let the devices have the whole request.
		/// </summary>
		int Run(int cycles);

		/// <summary>Advance every device by `cycles` (Run calls it).</summary>
		void TickDevices(int cycles);

		u64 TotalCycles() const { return totalCycles; }

		// -- interrupts ----------------------------------------------------------------------

		u8 If() const { return interruptFlag; }
		void SetIf(u8 value) { interruptFlag = (u8)(0xE0 | (value & 0x1F)); }
		void RequestInterrupt(u8 bit) { interruptFlag |= (u8)(bit & 0x1F); }
		u8 Ie() const { return interruptEnable; }
		void SetIe(u8 value) { interruptEnable = value; }
		bool Pending() const { return (interruptFlag & interruptEnable & 0x1F) != 0; }

		// -- the joypad ----------------------------------------------------------------------

		/// <summary>Set the pressed buttons (the GbButton bits).</summary>
		void SetPressedKeys(u8 mask) { pressedKeys = mask; }
		u8 PressedKeys() const { return pressedKeys; }

		/// <summary>True while the CPU is in STOP and a button may wake it.</summary>
		bool InStopMode() const { return stopped; }

		/// <summary>Leave STOP mode (the machine calls it when a button is pressed).</summary>
		void WakeFromStop();

		/// <summary>The value of the joypad register for the currently selected half.</summary>
		u8 JoypadValue() const;

		// -- the serial port -----------------------------------------------------------------

		/// <summary>Plug another machine's port into this one (nullptr unplugs it).</summary>
		void AttachSerialPeer(GbSerialPeer* peer) { serialPeer = peer; }
		GbSerialPeer* SerialPeer() const { return serialPeer; }

		u8 SerialData() const { return serialData; }
		u8 SerialControl() const { return serialControl; }
		bool SerialActive() const { return serialActive; }
		u8 LastSent() const { return lastSent; }
		u8 LastReceived() const { return lastReceived; }

		/// <summary>The number of completed transfers (the link test counts them).</summary>
		int SerialTransfers() const { return serialTransfers; }

		/// <summary>True when the port is listening to the external clock (this machine is the
		/// slave of a link).</summary>
		bool SerialExternalClock() const { return serialActive && !serialInternalClock; }

		/// <summary>Shift one bit into this port's shift register (the peer clocks it).</summary>
		void SerialClock(u8 bit) { serialShiftIn = (u8)((serialShiftIn << 1) | (bit & 1)); }

		/// <summary>Process one bit of an externally clocked transfer (the master's cable).</summary>
		void SerialTransferBit();

		/// <summary>Finish a transfer: SB gets the received byte and the interrupt is requested.</summary>
		void FinishSerial();

		/// <summary>True when the machine is a CGB running in double speed (the serial clock rule).</summary>
		bool SerialFastClock() const { return cgb && (serialControl & 0x02) != 0; }

		// -- the boot ROM --------------------------------------------------------------------

		/// <summary>Install a 256 byte boot ROM image (nullptr clears it).</summary>
		void SetBootRom(const u8* image, u32 size);
		void MapBootRom(bool mapped);
		bool BootRomMapped() const { return bootRomMapped; }

		// -- battery -------------------------------------------------------------------------

		bool SaveBattery(std::string* error) { return cart.SaveSaveFile(error); }

		// -- the CGB only registers, for the tests and the report -----------------------------

		u8 Key1() const { return key1; }
		u8 Svbk() const { return svbk; }
		u8 Vbk() const { return vbk; }
		bool HdmaActive() const { return hdmaActive; }
		int HdmaRemainingBlocks() const { return hdmaBlocks; }

	private:
		// -- memory --------------------------------------------------------------------------

		u8 wram[8][0x1000]{};			// the CGB's eight 4 KByte banks (a DMG uses the first two)
		u8 hram[0x7F]{};
		/// <summary>The boot ROM image, at most 0x900 bytes: the DMG's is 256 bytes (mapped at
		/// 0x0000-0x00FF) and the CGB's is 2304 bytes (mapped at 0x0000-0x00FF and 0x0200-0x08FF,
		/// with the cartridge header at 0x0100-0x01FF readable in between - the ROM is split in
		/// two parts, Pan Docs "Power Up Sequence").</summary>
		u8 bootRom[0x900]{};
		u32 bootRomSize = 0;
		bool bootRomLoaded = false;
		bool bootRomMapped = true;

		u8 interruptFlag = 0xE1;		// the post-boot values (Pan Docs "Power Up Sequence")
		u8 interruptEnable = 0x00;

		// -- configuration -------------------------------------------------------------------

		bool cgb = false;
		bool doubleSpeed = false;
		int clockSpeed = GbCyclesPerSecond;

		// -- the timer (Pan Docs "Timer and Divider Registers") ------------------------------

		u16 divider = 0xAB00;			// the 16-bit counter whose top byte is DIV
		u8 tima = 0x00;
		u8 tma = 0x00;
		u8 tac = 0xF8;					// the post-boot value (only bits 0-2 are writable)
		bool timaReloading = false;		// the four clock reload window after an overflow
		int timaReloadDelay = 0;
		bool lastTimerBit = false;		// the falling edge detector

		// -- the joypad ----------------------------------------------------------------------

		u8 joypadSelect = 0x30;			// P1 bits 4-5: which half of the matrix is read
		u8 pressedKeys = 0x00;
		bool stopped = false;

		// -- the serial port -----------------------------------------------------------------

		u8 serialData = 0x00;
		u8 serialControl = 0x7E;		// the post-boot value
		int serialBitTimer = 0;
		int serialBitsLeft = 0;
		bool serialActive = false;
		bool serialInternalClock = false;
		u8 serialShiftOut = 0;
		u8 serialShiftIn = 0;
		u8 lastSent = 0x00;
		u8 lastReceived = 0x00;
		int serialTransfers = 0;
		GbSerialPeer* serialPeer = nullptr;

		// -- DMA -----------------------------------------------------------------------------

		u8 dmaRegister = 0xFF;			// the post-boot value
		int oamDmaCycles = 0;			// the OAM DMA transfer still in progress
		u16 oamDmaSource = 0;

		u8 hdma1 = 0xFF, hdma2 = 0xFF, hdma3 = 0xFF, hdma4 = 0xFF, hdma5 = 0xFF;
		bool hdmaActive = false;
		bool hdmaHblank = false;
		u16 hdmaSource = 0;
		u16 hdmaDest = 0;
		int hdmaBlocks = 0;				// the 16 byte blocks left
		int hdmaLastMode = -1;			// to detect the mode 0 edge the transfer runs on

		// -- the CGB only registers ----------------------------------------------------------

		u8 key1 = 0x7E;					// bit 7 the current speed, bit 0 the switch request
		u8 vbk = 0xFE;
		u8 svbk = 0xF8;					// bits 0-2 select the WRAM bank at 0xD000
		u8 opri = 0x00;					// the object priority mode

		// -- the machine's accounting --------------------------------------------------------

		u64 totalCycles = 0;

		// -- helpers -------------------------------------------------------------------------

		/// <summary>The I/O register block 0xFF00..0xFF7F, split from ReadByte/WriteByte so the
		/// memory map reads as one list of regions.</summary>
		u8 ReadIo(u16 address);
		void WriteIo(u16 address, u8 value);
		u8 PeekIo(u16 address) const;

		bool TimerInputHigh() const;
		void TickTimer(int systemCycles);
		void TickSerial(int systemCycles);
		void TickHdma();
		void StartOamDma(u8 value);
		void CopyOamDmaByte();

		/// <summary>Start a CGB HDMA/GDMA transfer from HDMA5 (Pan Docs "CGB Registers").</summary>
		void StartHdma(u8 value);

		static int TimerDividerBit(u8 tac);
	};
}
