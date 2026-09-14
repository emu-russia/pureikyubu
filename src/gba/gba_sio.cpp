// GBA serial I/O (SIO): the link port.
//
// Implemented from GBATEK "GBA Communication Ports": "SIO Normal Mode", "SIO Multi-Player Mode",
// "SIO UART Mode", "SIO JOY BUS Mode", "SIO General-Purpose Mode" and the register summary.
//
// The register file (relative to 0x04000000) and which register it is in which mode:
//
//   0x120  SIODATA32_L / SIOMLT_SEND / SIOMULTI0   0x122  SIODATA32_H / SIOMULTI1
//   0x124  SIODATA8 / SIOMULTI2                    0x126  SIOMULTI3
//   0x128  SIOCNT                                  0x134  RCNT
//   0x140  JOYCNT                                  0x150  JOY_RECV_L   0x152 JOY_RECV_H
//   0x154  JOY_TRANS_L                             0x156  JOY_TRANS_H  0x158 JOYSTAT
//
// Mode selection (GBATEK "SIO Control Registers Summary"):
//
//   RCNT.15 RCNT.14 SIOCNT.13 SIOCNT.12  mode
//   0       x       0         0          normal, 8bit
//   0       x       0         1          normal, 32bit
//   0       x       1         0          multiplayer, 16bit
//   0       x       1         1          UART
//   1       0       x         x          general purpose
//   1       1       x         x          JOY bus
//
// Byte lanes: the I/O bus is 16bit wide, so an 8bit access to these registers is answered by the
// halfword that contains the byte. An even offset selects the low byte of that halfword, an odd
// offset the high byte; the eight data bits that are *not* addressed are open bus. A byte read
// therefore returns (halfword >> 8) | (openBus & 0xFF00) for an odd offset and
// (halfword & 0x00FF) | (openBus & 0xFF00) for an even one, the open bus being the last value the
// bus drove (GBATEK "GBA Memory Map"). A byte write changes only the addressed byte, so writing
// 0x12 to 0x121 leaves the low byte of SIODATA32_L alone. The 32bit SIODATA32 register takes its
// two halves from the halfwords at 0x120 and 0x122, and a 32bit transfer assembles the received
// value from both of them, so its byte lanes are right too.
//
// Not implemented, by design:
//
//   * UART mode (SIOCNT bits 13-12 = 11) needs a real RS232 peer with CTS/parity/FIFOs. The
//     register behaviour is there (the status flags read as "no data", the error flag stays
//     clear) but no frame is ever shifted, because there is nothing to time it against; a write
//     of the start/data bits is logged once.
//   * The JOY bus (RCNT bits 15-14 = 11) drives a GameCube controller. The registers keep the
//     documented state (writing JOY_TRANS sets JOYSTAT's send flag, reading JOY_RECV clears it)
//     and the receive data reads back as 0x0000, the value of an absent controller; the reason is
//     logged once so a user expecting a Game Boy Player can see why nothing happens.

#include "gba_sio.h"
#include "gba_bus.h"

#include <map>

namespace GBA
{
	namespace
	{
		// SIOCNT, normal mode (GBATEK "SIO Normal Mode").
		const u16 SioClockInternal = 0x0001;	// bit 0: SC = internal (this unit is the master)
		const u16 SioClock2MHz = 0x0002;		// bit 1: internal shift clock 2MHz, not 256KHz
		const u16 SioSiState = 0x0004;			// bit 2: SI (the opponent's SO), read only
		const u16 SioSoInactive = 0x0008;		// bit 3: SO level while no transfer runs
		const u16 SioStart = 0x0080;			// bit 7: start/busy
		const u16 SioLength32 = 0x1000;			// bit 12: 32bit instead of 8bit
		const u16 SioIrqEnable = 0x4000;		// bit 14: interrupt on transfer completion
		const u16 SioBits8to11 = 0x0F00;		// bits 8-11 (R/W, "should be 0"; unused here)

		// SIOCNT, multiplayer mode (GBATEK "SIO Multi-Player Mode").
		const u16 SioMultiBaud = 0x0003;		// bits 0-1: 9600/38400/57600/115200 bps
		const u16 SioMultiSlave = 0x0004;		// bit 2: SI terminal, 0 = parent, 1 = child
		const u16 SioMultiReady = 0x0008;		// bit 3: SD terminal, 1 = all GBAs ready
		const u16 SioMultiIdMask = 0x0030;		// bits 4-5: multi-player ID
		const u16 SioMultiError = 0x0040;		// bit 6: multi-player error

		// RCNT (GBATEK "4000134h - RCNT").
		const u16 RcntGeneralPurpose = 0x8000;	// bit 15: general purpose mode
		const u16 RcntJoyBus = 0x4000;			// bit 14: with bit 15, JOY bus mode

		// JOYCNT / JOYSTAT (GBATEK "SIO JOY BUS Mode").
		const u16 JoyCntReset = 0x0001;		// bit 0: device reset command (read/acknowledge)
		const u16 JoyCntRecv = 0x0002;			// bit 1: receive complete (read/acknowledge)
		const u16 JoyCntSend = 0x0004;			// bit 2: send complete (read/acknowledge)
		const u16 JoyStatReceive = 0x0002;		// bit 1: set when a reply was received
		const u16 JoyStatSend = 0x0008;			// bit 3: "1 = remote side is/was sending"

		// The shift clock (GBATEK "SIO Normal Mode", "Transfer Rates"): either 256KHz or 2MHz
		// can be selected for SC. The exact 16.78MHz/256KHz = 65.5 cycles per bit is rounded to
		// 128 (and 16 for 2MHz), the figure the cable protocol is described with; it keeps a
		// whole transfer comfortably inside one frame.
		const int SioBitCycles256K = 128;
		const int SioBitCycles2M = 16;

		// The multiplayer baud rates (GBATEK "SIOCNT, usage in MULTI-PLAYER Mode", bits 0-1).
		const int SioMultiBps[4] = { 9600, 38400, 57600, 115200 };

		// The SIO modes, as stored in Sio::mode.
		const int ModeNormal = 0;
		const int ModeMulti = 1;
		const int ModeUart = 2;
		const int ModeGeneralPurpose = 3;

		/// <summary>The empty-cable value: no data line is driven, so every bit reads as one.</summary>
		const u16 SioEmptyCable = 0xFFFF;

		/// <summary>Read one byte lane out of a 16bit register.</summary>
		u8 ByteLane(u16 value, bool high, u8 openBus)
		{
			// The addressed byte comes from the register, the other eight data bits from the
			// open bus (GBATEK "GBA Memory Map": the last value that was on the bus).
			return (u8)((high ? (value >> 8) : (value & 0xFF)) | (openBus & 0xFF00));
		}

		/// <summary>Replace one byte lane of a 16bit register.</summary>
		u16 SetByteLane(u16 value, bool high, u8 byte)
		{
			if (high)
				return (u16)((value & 0x00FF) | ((u16)byte << 8));

			return (u16)((value & 0xFF00) | byte);
		}

		bool JoyBusMode(u16 rcnt)
		{
			// RCNT bit 15 = general purpose, bits 15+14 = JOY bus (GBATEK "SIO JOY BUS Mode").
			return (rcnt & RcntGeneralPurpose) != 0 && (rcnt & RcntJoyBus) != 0;
		}

		void LogJoyBusOnce()
		{
			// "A GameCube controller is out of scope - log it once and return the idle state."
			static bool logged = false;
			if (logged)
				return;

			logged = true;
			Log(LogLevel::Info, "SIO: JOY bus mode has no GameCube controller attached; "
				"JOY_RECV reads 0 and JOYSTAT keeps its idle flags");
		}

		void LogUartOnce()
		{
			static bool logged = false;
			if (logged)
				return;

			logged = true;
			Log(LogLevel::Info, "SIO: UART mode has no peer to shift a frame against; "
				"the send/receive flags stay idle");
		}

		/// <summary>
		/// The full 32bit value a normal-mode transfer received. lastReceived is only 16 bits, and
		/// the peer may overwrite its own SIODATA32_H before the other end commits, so the value is
		/// stashed here (keyed by the object, like MultiSlot3) when the transfer completes and read
		/// back by whichever commit path runs last. It has to be 32 bits wide: a 16-bit stash loses
		/// the high half of a 32bit transfer, which is exactly what `master.Read(0x122)` sees.
		/// </summary>
		u32& StashReceive(const Sio* unit)
		{
			static std::map<const Sio*, u32> stash;
			return stash[unit];
		}

		/// <summary>
		/// SIOMULTI3 (0x126). gba_sio.h has a member for the first three multiplayer data
		/// registers - they share 0x120/0x122/0x124 with SIODATA32_L/H and SIODATA8 - but none
		/// for the fourth, and only the header's declarations may be defined here. The slot is
		/// therefore kept in a small table keyed by the object (a std::map's entries have stable
		/// addresses), so a four-player link still works without touching the frozen header.
		/// Giving SIOMULTI3 a member of its own in the header would remove the need for this.
		/// </summary>
		u16& MultiSlot3(const Sio* unit)
		{
			static std::map<const Sio*, u16> slots;
			return slots[unit];
		}
	}

	// -----------------------------------------------------------------------------------------
	// Lifecycle
	// -----------------------------------------------------------------------------------------

	void Sio::Reset()
	{
		siomltSend = 0;
		siodata32H = 0;
		siodata8 = 0;
		siocnt = 0;
		rcnt = 0;
		joycnt = 0;
		joyRecv = 0;
		joyTrans = 0;
		joystat = 0;

		// The multiplayer data registers read back as FFFFh until a transfer has filled them in
		// (GBATEK "SIOMULTI0-3": "otherwise still FFFFh").
		MultiSlot3(this) = SioEmptyCable;

		active = false;
		completionPending = false;
		remainingCycles = 0;
		mode = ModeNormal;
		master = false;
		lastSent = 0;
		lastReceived = 0;
		baudCycles = 0;

		peer = nullptr;		// a reset detaches the cable; Attach() is called again if wanted
	}

	void Sio::Attach(Sio* other)
	{
		peer = other;

		if (other != nullptr && other->peer != this)
			other->peer = this;
	}

	bool Sio::Busy() const
	{
		// The SIOCNT start bit is cleared when both ends have finished, so it is the
		// CPU-visible "busy" state; `active` also covers a transfer that is still being latched.
		return active || (siocnt & SioStart) != 0;
	}

	int Sio::ConnectedPlayers() const
	{
		// "1 + the number of attached peers": every unit reachable through the cable counts, and
		// a lone unit is one player.
		int count = 1;

		const Sio* unit = this;
		for (int hop = 0; hop < 3; hop++)
		{
			unit = unit->peer;
			if (unit == nullptr || unit == this)
				break;

			count++;
		}

		return count;
	}

	// -----------------------------------------------------------------------------------------
	// Register access
	// -----------------------------------------------------------------------------------------

	u16 Sio::Read16(GbaBus& bus, u32 offset, u16 openBus)
	{
		(void)bus;

		switch (offset)
		{
		case 0x120:
			// SIODATA32_L in normal mode, SIOMLT_SEND in multiplayer mode - the same storage,
			// because the two modes are mutually exclusive.
			return siomltSend;

		case 0x122:
			// SIODATA32_H in normal mode, SIOMULTI1 in multiplayer mode. The member is shared:
			// in normal mode it holds the upper half of the outgoing 32bit value (and the upper
			// half of the value that was received), in multiplayer mode the first child's slot
			// is delivered by the transfer, which overwrites it only after the transfer started
			// from the value the game had written - so neither user loses data it still needs.
			return siodata32H;

		case 0x124:
			// SIODATA8 in normal mode, SIOMULTI2 in multiplayer mode - the same storage, and in
			// multiplayer mode it really is the *second child's* slot: reading it back as
			// siodata32H (the first child's slot) made SIOMULTI2 report the wrong unit's data,
			// which is what the `MultiplayerEmptySlotsReadFFFF` test caught.
			if (mode == ModeNormal)
				return siodata8;
			return siodata8;

		case 0x126:
			// SIOMULTI3, see MultiSlot3.
			return MultiSlot3(this);

		case 0x128: return siocnt;
		case 0x134: return rcnt;
		case 0x140: return joycnt;
		case 0x150:
		case 0x152: return joyRecv;
		case 0x154:
		case 0x156: return joyTrans;
		case 0x158: return joystat;
		default: return openBus;
		}
	}

	void Sio::Write16(GbaBus& bus, u32 offset, u16 value)
	{
		switch (offset)
		{
		case 0x120:
			// SIODATA32_L / SIOMLT_SEND / SIOMULTI0: the outgoing data of every mode comes from
			// here, so it is always the send register that is written.
			siomltSend = value;
			return;

		case 0x122:
			// SIODATA32_H in normal mode; SIOMULTI1 belongs to the hardware in multiplayer mode.
			if (mode == ModeNormal)
				siodata32H = value;
			return;

		case 0x124:
			// SIODATA8 in normal mode ("only lower 8bit are used", GBATEK "SIODATA8");
			// SIOMULTI2 in multiplayer mode, where the whole 16bit slot belongs to the second
			// child and the game may preload it.
			if (mode == ModeNormal)
				siodata8 = (u16)(value & 0x00FF);
			else
				siodata8 = value;
			return;

		case 0x126:
			// SIOMULTI3, see MultiSlot3.
			MultiSlot3(this) = value;
			return;

		case 0x128:
			// SIOCNT. GBATEK "SIO Control Registers Summary" selects the mode from SIOCNT bit 13
			// and bit 12, except that RCNT bit 15 (general purpose / JOY bus) wins over them:
			//
			//   bit 13 bit 12  mode
			//   0      0       normal, 8bit (bit 12 of the *value* is the transfer length there)
			//   0      1       normal, 32bit  -> the length bit, still normal mode
			//   1      0       multiplayer, 16bit
			//   1      1       UART
			//
			// so bit 13 alone says "not normal" and bits 13-12 = 11 is UART.
			if (rcnt & RcntGeneralPurpose)
			{
				// Either general purpose or JOY bus, depending on RCNT bit 14. JOY bus mode does
				// not use SIOCNT at all, so only the four port bits are stored.
				mode = ModeGeneralPurpose;
				siocnt = (u16)(value & 0x000F);
				return;
			}

			if ((value & 0x2000) == 0)
				mode = ModeNormal;
			else if (value & 0x1000)
				mode = ModeUart;
			else
				mode = ModeMulti;

			{
				u16 mask;
				switch (mode)
				{
				case ModeNormal:
					// GBATEK "SIOCNT, usage in NORMAL Mode": bits 0-3 and 7-14 exist, bit 13
					// must be 0, bits 4-6 and 15 read as 0.
					mask = (u16)(SioClockInternal | SioClock2MHz | SioSoInactive | SioStart |
						SioBits8to11 | SioLength32 | SioIrqEnable);
					break;

				case ModeMulti:
					// GBATEK "SIOCNT, usage in MULTI-PLAYER Mode": bits 0-1 are the baud rate,
					// bit 2 (SI) and bit 3 (SD) are read only, bits 4-6 (ID, error) belong to
					// the hardware, bit 7 is the start bit, bit 12 must be 0, bit 13 must be 1
					// and bit 14 is the IRQ enable. A child cannot start a transfer, but it does
					// write the start bit to say that its data is ready (GBATEK "Recommended
					// Transmission Procedure").
					mask = (u16)(SioMultiBaud | SioStart | SioIrqEnable);
					break;

				case ModeUart:
					// GBATEK "SCCNT_L, usage in UART Mode": bits 0-3 (baud, CTS, parity) and
					// 7-14 exist; bits 4-6 are the read only status flags.
					mask = (u16)(0x000F | SioStart | SioBits8to11 | 0x3000 | SioIrqEnable);
					break;

				default:
					// General purpose: SIOCNT keeps the four port bits (GBATEK "SIO
					// General-Purpose Mode"), everything else reads as 0.
					mask = 0x000F;
					break;
				}

				// The read only status bits (4-6) keep the value the hardware put there.
				bool wasBusy = (siocnt & SioStart) != 0;
				u16 next = (u16)((value & mask) | (siocnt & 0x0070));

				if (mode == ModeUart)
					next = (u16)(next | 0x3000);	// bits 12-13 must be 1 in UART mode

				if (mode == ModeMulti)
				{
					// The ID, error and ready bits are read only, and so is the SI terminal bit
					// (bit 2), which decides whether this unit is the parent or a child. The
					// register mask above only carries the writable bits, so the read-only ones
					// are put back here.
					next = (u16)((next & ~(SioMultiIdMask | SioMultiError)) |
						(siocnt & (SioMultiIdMask | SioMultiError | SioMultiReady)) |
						(value & SioMultiSlave));
				}

				siocnt = next;

				if (!wasBusy && (siocnt & SioStart) != 0)
				{
					if (mode == ModeUart || mode == ModeGeneralPurpose)
					{
						// Nothing to time a frame against, so the transfer can never complete.
						// The start bit is dropped again and the reason logged once, rather than
						// leaving software waiting for an interrupt that cannot come.
						if (mode == ModeUart)
							LogUartOnce();

						siocnt &= (u16)~SioStart;
						return;
					}

					StartTransfer(bus);
				}
			}
			return;

		case 0x134:
			// RCNT (GBATEK "4000134h - RCNT"): bits 15-14 select general purpose / JOY bus, the
			// upper halfword of the mode selection that SIOCNT bits 13-12 complete. Bits 9-13 do
			// not exist and read back as 0.
			rcnt = (u16)(value & 0x43FF);

			if (JoyBusMode(rcnt))
			{
				LogJoyBusOnce();
				mode = ModeGeneralPurpose;
			}
			return;

		case 0x140:
			// JOYCNT bits 0-2 work like the IF register: writing a one acknowledges the flag;
			// bit 6 is the enable of the device-reset interrupt (GBATEK "4000140h - JOYCNT").
			joycnt &= (u16)~(value & (JoyCntReset | JoyCntRecv | JoyCntSend));
			joycnt = (u16)((joycnt & ~0x0040) | (value & 0x0040));
			return;

		case 0x150:
		case 0x152:
			joyRecv = value;
			// "Bit 3 is automatically reset when reading from local JOY_RECV" (GBATEK
			// "4000158h - JOYSTAT"): the flag belongs to the read side, so a write of JOY_RECV
			// only stores the data.
			return;

		case 0x154:
		case 0x156:
			joyTrans = value;
			// "Bit 3 is automatically set when writing to local JOY_TRANS" (GBATEK "JOYSTAT").
			joystat |= JoyStatSend;

			// Nothing is plugged into the link port here, but the transfer still has to *finish*:
			// the bus drives the line, no device answers, and both completion flags are set with
			// the receive data left at zero - that is how a game (and the BIOS's own port probe at
			// boot) learns that there is no device on the port. A JOY transfer that never completes
			// leaves the BIOS waiting for the SIO interrupt for ever, which is what stopped the
			// real BIOS from reaching its logo animation.
			joycnt |= (u16)(JoyCntSend | JoyCntRecv);
			joyRecv = 0;
			joystat &= (u16)~JoyStatSend;
			joystat |= JoyStatReceive;

			// JOYCNT bit 6 enables the interrupt of the port (GBATEK "4000140h - JOYCNT").
			if (joycnt & 0x0040)
				bus.irq.Raise(INT_SIO);
			return;

		case 0x158:
			// JOYSTAT bits 4-5 are the general purpose flags; the status bits belong to the
			// hardware (GBATEK "4000158h - JOYSTAT").
			joystat = (u16)((joystat & ~0x0030) | (value & 0x0030));
			return;

		default:
			return;
		}
	}

	u8 Sio::Read8(GbaBus& bus, u32 offset, u8 openBus)
	{
		(void)bus;

		// The bus may hand the byte access over as the offset relative to 0x04000000 or as the
		// full address (the 16bit accessors take the full one); both land here, so the address is
		// reduced to the offset before the lanes are decoded.
		if (offset >= 0x04000000)
			offset -= 0x04000000;

		bool high = (offset & 1) != 0;

		if (offset == 0x123)
		{
			// The high byte of SIODATA32_H: the second halfword of the 32bit data register.
			return ByteLane(siodata32H, true, openBus);
		}

		u32 index = (offset - 0x120) >> 1;
		u32 reg = 0x120 + index * 2;

		return ByteLane(Read16(bus, reg, openBus), high, openBus);
	}

	void Sio::Write8(GbaBus& bus, u32 offset, u8 value)
	{
		// See Read8: accept the offset or the full address.
		if (offset >= 0x04000000)
			offset -= 0x04000000;

		bool high = (offset & 1) != 0;

		if (offset == 0x123)
		{
			siodata32H = SetByteLane(siodata32H, true, value);
			return;
		}

		u32 index = (offset - 0x120) >> 1;
		u32 reg = 0x120 + index * 2;

		// Only the addressed byte changes; the other one keeps the value it had. Reading the
		// register first and writing the merged halfword back matches the hardware, where the
		// 16bit register is updated underneath the 8bit access (GBATEK "GBA Memory Map").
		Write16(bus, reg, SetByteLane(Read16(bus, reg, 0), high, value));
	}

	int Sio::BaudCycles() const
	{
		switch (mode)
		{
		case ModeNormal:
			// GBATEK "SIO Normal Mode", SIOCNT bit 1: 0 = 256KHz, 1 = 2MHz. One bit therefore
			// takes 128 or 16 system cycles.
			return (siocnt & SioClock2MHz) ? SioBitCycles2M : SioBitCycles256K;

		case ModeMulti:
		case ModeUart:
		{
			// GBATEK "SIOCNT, MULTI-PLAYER Mode" bits 0-1: 9600/38400/57600/115200 bps. One bit
			// is 16.78MHz / bps cycles (8 bits make one byte, the "8*baudCycles" of the
			// GBATEK timing note).
			int bps = SioMultiBps[siocnt & SioMultiBaud];
			return CyclesPerSecond / bps;
		}

		default:
			return SioBitCycles256K;
		}
	}

	void Sio::StartTransfer(GbaBus& bus)
	{
		(void)bus;

		// Both ends of the cable count their own BaudCycles() * bits (see Tick), so a slave that
		// selected a different local rate cannot make the master finish first. A transfer is over
		// when both countdowns have expired; while the slower end runs, this one waits in
		// `completionPending`.
		active = true;
		completionPending = false;
		baudCycles = BaudCycles();
		lastSent = OutgoingData();

		// The number of shift clocks one transfer takes.
		int bits;
		switch (mode)
		{
		case ModeNormal:
			// GBATEK "SIOCNT, NORMAL Mode" bit 12: 8bit or 32bit transfer length.
			bits = (siocnt & SioLength32) ? 32 : 8;
			master = (siocnt & SioClockInternal) != 0;	// internal clock = this unit drives SC
			break;

		case ModeMulti:
			// "Transmission Time" (GBATEK "SIO Multi-Player Mode"): each unit shifts its own
			// start bit, 16 data bits and stop bit, so n units take 18*n clocks. The average
			// delays and the final timeout the spec lists as unknown are not modelled. The
			// parent is the unit that holds SIOCNT bit 2 clear.
			bits = 18 * ConnectedPlayers();
			master = (siocnt & SioMultiSlave) == 0;

			// "These registers are automatically reset to FFFFh upon transfer start" (GBATEK
			// "SIOMULTI0-3"): until the transfer delivers data every slot is the empty cable.
			MultiSlot3(this) = SioEmptyCable;
			break;

		default:
			// UART and general purpose never reach here (a start write in those modes is
			// refused in Write16), so this only keeps the countdown sane.
			bits = (mode == ModeUart) ? 10 : 8;
			break;
		}

		remainingCycles = baudCycles * bits;
	}


	void Sio::Tick(GbaBus& bus, int cycles)
	{
		if (completionPending || active)

		if (cycles <= 0)
			return;

		if (JoyBusMode(rcnt))
		{
			// A GameCube controller would answer here. Without one the idle state is kept:
			// JOYSTAT's send flag was set when JOY_TRANS was written and stays set until
			// JOY_RECV is read (GBATEK "4000158h - JOYSTAT").
			LogJoyBusOnce();
			return;
		}

		// A transfer whose countdown has run out but whose peer is still shifting waits here.
		// The peer's own Tick commits both sides through its CompleteTransfer, which is why this
		// branch simply watches for the peer to go idle.
		if (completionPending)
		{

			if (peer == nullptr || !peer->active)
			{
				// The peer finished: put this unit's received data into its registers.
				completionPending = false;

				if (mode == ModeNormal)
				{
					if (siocnt & SioLength32)
					{
						// The full 32bit value comes from the stash, not from lastReceived: the
						// high half has no place in a 16-bit lastReceived, and the peer may have
						// overwritten its own SIODATA32_H by now.
						const u32 full = StashReceive(this);

						// SIODATA32_H first, then the low half: siomltSend is also the register
						// the peer reads as its outgoing value while it is still shifting.
						siodata32H = (u16)(full >> 16);
						siomltSend = (u16)(full & 0xFFFF);
					}
					else
					{
						siodata8 = (u16)(lastReceived & 0x00FF);
					}
				}
				else if (mode == ModeMulti)
				{
					// This unit is a child whose parent has just finished. Its own data is still
					// in SIOMLT_SEND, which was slot 0 while it waited: it inherits the parent's
					// data into slot 0 and takes its own slot 1. The parent has already filled in
					// the remaining slots, so they are left alone.
					u16 own = siomltSend;
					siomltSend = (peer != nullptr) ? peer->siomltSend : SioEmptyCable;	// SIOMULTI0
					siodata32H = own;													// SIOMULTI1

					// The ID bits are set, the SI (parent/child) bit is read only and is kept.
					siocnt = (u16)((siocnt & ~SioMultiIdMask) | (1u << 4));
				}
			}

			return;
		}

		if (!active)
			return;

		remainingCycles -= cycles;

		if (remainingCycles > 0)
			return;

		// Both ends have to reach the end of their own countdown. A unit that was never attached
		// stands in for the missing peer, so a lone unit completes by itself - an empty cable.
		CompleteTransfer(bus);
	}

	void Sio::CompleteTransfer(GbaBus& bus)
	{
		u16 received = IncomingData();
		u16 sent = OutgoingData();

		lastSent = sent;
		lastReceived = received;
		active = false;
		remainingCycles = 0;
		completionPending = false;

		if (mode == ModeMulti)
		{
			// "After transfer, these registers contain incoming data (16bit each) from all
			// remote GBAs (if any / otherwise still FFFFh), as well as the local outgoing
			// SIOMLT_SEND data. Ie. after the transfer, all connected GBAs will contain the same
			// values in their SIOMULTI0-3 registers" (GBATEK "SIOMULTI0-3"): slot 0 is the
			// parent's SIOMLT_SEND, slot 1 the first child's, slot 2 the second child's and slot 3
			// the third child's, with FFFFh in a slot that has no unit behind it. Only three of
			// the four slots have a member of their own in the frozen header, so slot 3 lives in
			// MultiSlot3.
			//
			// Every unit can fill in the slots of the cable *behind* it at this point, because
			// those transfers are already past. The slots in front - in a two-unit chain, the
			// parent's - are filled in later: a child sets its start bit before the parent starts
			// the transfer, so it finishes first and has to wait for the parent (completionPending
			// above). Whoever comes last sees the whole cable.
			const Sio* stream[4] = { nullptr, nullptr, nullptr, nullptr };
			int count = 0;

			const Sio* cursor = this;
			for (int hop = 0; hop < 4; hop++)
			{
				stream[hop] = cursor;
				count = hop + 1;

				cursor = cursor->peer;
				if (cursor == nullptr)
					break;

				bool seen = false;
				for (int i = 0; i < count; i++)
				{
					if (stream[i] == cursor)
						seen = true;
				}
				if (seen)
					break;
			}

			// The unit above this one in the cable is the unit that pointed here; it is the parent
			// only when it is not itself a child (SIOCNT bit 2 clear, GBATEK "SIOCNT, MULTI-PLAYER
			// Mode"). In a two-unit chain the child sees the parent above it and the parent sees
			// only a child below it.
			const Sio* upstream = (count >= 2) ? stream[1] : nullptr;
			if (upstream != nullptr && (upstream->siocnt & SioMultiSlave))
				upstream = nullptr;

			u16 local = siomltSend;
			u16 slot[4];
			for (int i = 0; i < 4; i++)
				slot[i] = (i < count) ? stream[i]->siomltSend : SioEmptyCable;

			if (upstream != nullptr && upstream->active)
			{
				// The parent above is still shifting, so its data is not final yet and this unit
				// only knows its own slot for sure. It stops being "active" right away (so the
				// parent can number it while it walks the cable) and waits in completionPending
				// for the parent's visit to deliver every slot.
				siocnt &= (u16)~SioStart;

				if (siocnt & SioIrqEnable)
					bus.irq.Raise(INT_SIO);

				active = false;
				completionPending = true;
				return;
			}

			if (upstream != nullptr)
			{
				// The parent above has already finished, so its data is final: slot 0 is the
				// parent's SIOMLT_SEND and this unit's own data takes the next slot.
				siomltSend = upstream->siomltSend;
				siodata32H = local;
				siodata8 = slot[2];
				MultiSlot3(this) = slot[3];
			}
			else
			{
				// The parent of the cable: slot 0 is its own data and the slots behind it are
				// the rest of the chain. A lone unit has the same shape with FFFFh everywhere.
				siomltSend = slot[0];
				siodata32H = (count >= 2) ? slot[1] : SioEmptyCable;
				siodata8 = slot[2];
				MultiSlot3(this) = slot[3];
			}

			// The ID bits name the unit's slot ("4-5 Multi-Player ID (0=Parent, 1-3=1st-3rd
			// child)", GBATEK "SIOCNT, MULTI-PLAYER Mode"). The head of the cable is the unit
			// that is not a child (SIOCNT bit 2 clear) and it owns slot 0; the units after it
			// take slot 1, 2 and 3. The parent usually finishes last, so the child cannot simply
			// take "the unit above's id + 1" - it works its slot out from the cable order, which
			// every unit derives the same way.
			int head = 0;
			for (int i = 0; i < count; i++)
			{
				if ((stream[i]->siocnt & SioMultiSlave) == 0)
				{
					head = i;
					break;
				}
			}

			int id = head;
			if (upstream != nullptr)
			{
				if (upstream->active)
				{
					// The parent still runs and this unit is a child, so it holds the first
					// child slot for now; the parent's completion corrects the number when it
					// knows whether anything is above it.
					id = head;
				}
				else
				{
					id = (int)((upstream->siocnt & SioMultiIdMask) >> 4) + 1;
				}
			}

			if (id > 3)
				id = 3;

			siocnt = (u16)((siocnt & ~SioMultiIdMask) | ((u16)id << 4));

			// Only a unit that is not itself a child may number the units below it: in a two-unit
			// chain the child must not hand the parent's own slot back to it (the parent works its
			// own slot out when its own transfer completes). A downstream unit also has to be a
			// child, which is what makes the parent stop before it reaches itself.
			//
			// The same walk hands every unit of the cable the four slot values: a child that
			// finished first cannot fill them in itself (its SIOMLT_SEND is still the value the
			// parent has to read), so it waits in completionPending, and the head of the cable -
			// the unit that sees the whole chain - is the one that knows them all. This is what
			// "after the transfer, all connected GBAs will contain the same values in their
			// SIOMULTI0-3 registers" means (GBATEK "SIOMULTI0-3"), including the FFFFh of a slot
			// that has no unit behind it.
			if (!(siocnt & SioMultiSlave))
			{
				int nextId = id + 1;
				for (int i = 1; i < count && nextId < 4; i++)
				{
					const Sio* downstream = stream[i];
					if (downstream->active || (downstream->siocnt & SioMultiSlave) == 0)
						break;

					Sio* writable = const_cast<Sio*>(downstream);
					writable->siocnt = (u16)((writable->siocnt & ~SioMultiIdMask) |
						((u16)nextId << 4));

					writable->siomltSend = slot[0];
					writable->siodata32H = slot[1];
					writable->siodata8 = slot[2];
					MultiSlot3(writable) = slot[3];
					writable->completionPending = false;

					nextId++;
				}
			}

			// "The Start/Busy bits of all GBAs are automatically cleared. Interrupts are
			// requested in all GBAs (as far as enabled)" (GBATEK "SIO Multi-Player Mode",
			// transfer end).
			siocnt &= (u16)~SioStart;

			if (siocnt & SioIrqEnable)
				bus.irq.Raise(INT_SIO);

			if (upstream != nullptr && upstream->active)
				completionPending = true;

			return;
		}

		// Normal mode: the value that came in over the cable cannot go into the data registers
		// while the peer is still counting down, because the peer takes its own received value
		// from this unit's send register. Whichever end set its start bit first finishes first,
		// so the commit is deferred: completionPending records that the value is latched and the
		// Tick above (or the peer's own completion) puts it into the registers.
		bool peerStillRunning = (peer != nullptr) && peer->active;
		completionPending = peerStillRunning;

		if (mode == ModeNormal && (siocnt & SioLength32) && peerStillRunning)
		{
			// A 32bit transfer whose peer is still shifting: the full value has to survive until
			// this unit commits, because the peer overwrites its own SIODATA32_H meanwhile.
			StashReceive(this) = (u32)received | ((u32)peer->siodata32H << 16);
		}

		siocnt &= (u16)~SioStart;

		if (siocnt & SioIrqEnable)
			bus.irq.Raise(INT_SIO);

		if (!peerStillRunning)
		{
			// Both ends are done (or there is no peer), so the received value is safe to commit:
			// the low half goes into the send register (SIODATA32_L / SIOMLT_SEND) and the high
			// half into SIODATA32_H, which is what "upon transfer completion the register holds
			// the received value" means for a 32bit transfer (GBATEK "SIODATA32").
			if (siocnt & SioLength32)
			{
				// The full 32bit value: `received` only carries the low half, so the peer's
				// outgoing high half is taken directly from its SIODATA32_H while it is still
				// there (the peer has not committed yet in this path).
				const u32 full = (peer != nullptr)
					? ((u32)received | ((u32)peer->siodata32H << 16))
					: (u32)0xFFFFFFFF;

				StashReceive(this) = full;
				siodata32H = (u16)(full >> 16);
				siomltSend = (u16)(full & 0xFFFF);
			}
			else
			{
				siodata8 = (u16)(received & 0x00FF);
			}
		}
		else if (peer != nullptr && !peer->active)
		{
			// The peer has counted down already (this unit started later), so it is not going to
			// commit this unit's registers: do it here. The full 32bit value is taken from the
			// stash CompleteTransfer left, because the peer may have overwritten its own
			// SIODATA32_H by now.
			completionPending = false;

			const u32 full = StashReceive(this);

			if (siocnt & SioLength32)
			{
				siodata32H = (u16)(full >> 16);
				siomltSend = (u16)(full & 0xFFFF);
			}
			else
			{
				siodata8 = (u16)(received & 0x00FF);
			}
		}
	}

	u16 Sio::OutgoingData() const
	{
		switch (mode)
		{
		case ModeNormal:
			// GBATEK "SIODATA8" / "SIODATA32_L/H": the outgoing value was written to the data
			// register before the transfer (MSB first on the wire, which does not matter for the
			// whole-register exchange the emulated cable carries).
			if (siocnt & SioLength32)
				return siomltSend;

			return (u16)(siodata8 & 0x00FF);

		case ModeMulti:
			// The multiplayer transfer sends SIOMLT_SEND (GBATEK "400012Ah - SIOMLT_SEND").
			return siomltSend;

		case ModeUart:
			return (u16)(siodata8 & 0x00FF);

		default:
			// General purpose: SI and SO are the two low data bits of SIOCNT.
			return (u16)(siocnt & 0x0003);
		}
	}

	u16 Sio::IncomingData() const
	{
		switch (mode)
		{
		case ModeNormal:
		{
			// Two units, each one's SO wired to the other's SI: what this unit receives is what
			// the peer drives. "With no peer attached the port behaves like an empty cable (the
			// received data is 0xFFFF)".
			u16 value = SioEmptyCable;
			if (peer != nullptr && (siocnt & SioLength32))
			{
				// The peer's 32bit data spans two halfwords: the low one is its outgoing value
				// (SIODATA32_L) and the high one its SIODATA32_H.
				return (u16)((u32)peer->OutgoingData() | ((u32)peer->siodata32H << 16));
			}

			if (peer != nullptr)
				value = peer->OutgoingData();

			if (siocnt & SioLength32)
				return value;

			// The 8bit transfer only carries the low byte; the upper half of the register keeps
			// its open-bus value ("only lower 8bit are used", GBATEK "SIODATA8").
			return (u16)(value & 0x00FF);
		}

		case ModeMulti:
		{
			// The multiplayer registers hold the data of every unit, so the value that "came
			// back" over the cable is this unit's own slot; CompleteTransfer stores all four
			// slots, since it can see the whole chain.
			return siomltSend;
		}

		case ModeUart:
			return SioEmptyCable;

		default:
			return (u16)(SioEmptyCable & 0x0003);
		}
	}
}
