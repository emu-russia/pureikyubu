// GBA serial I/O (SIO): the link port, which is what makes the emulator usable as a GBA Link
// or a Game Boy Player replacement.
//
// Registers (relative to 0x04000000), from GBATEK "GBA Serial I/O":
//
//   0x120 SIODATA32_L / SIOMLT_SEND   0x122 SIODATA32_H   0x124 SIODATA8
//   0x128 SIOCNT                       0x134 RCNT
//   0x140 JOYCNT                       0x150 JOY_RECV_L    0x152 JOY_RECV_H
//   0x154 JOY_TRANS_L                  0x156 JOY_TRANS_H   0x158 JOYSTAT
//
// The four modes are the normal 8/32-bit mode (two players, one of them the master), the
// multiplayer mode (up to four players, 16 bits each, one transfer per player slot), the UART
// mode (which is what a "GBA Link" cable used with a PC or another GBA in the JOY bus) and the
// general purpose mode. JOY bus mode (RCNT bit 15) drives a GameCube controller instead of a
// peer, which is how the Game Boy Player talks to the console.
//
// Two Sio objects can be attached to each other (Attach); that is the "link cable": both sides
// see the peer's data, and the transfer completes after the cable delay both of them count down.
// With no peer attached the port behaves like an empty cable (the received data is 0xFFFF), which
// is the state the emulator is in when the user runs a single instance.

#pragma once

#include "gba_types.h"

namespace GBA
{
	class GbaBus;
	class StateWriter;
	class StateReader;

	class Sio
	{
	public:
		void Reset();

		/// <summary>
		/// Write the port's registers and the transfer in progress into a save state. The *cable*
		/// is not part of it: the peer is another machine (or another process), and a state cannot
		/// carry it. A state taken in the middle of a transfer therefore resumes that transfer on
		/// its own side of the cable; the peer's half is whatever it is doing.
		/// </summary>
		void SaveState(StateWriter& writer) const;
		void LoadState(StateReader& reader);

		uint16_t Read16(GbaBus& bus, uint32_t offset, uint16_t openBus);
		uint8_t Read8(GbaBus& bus, uint32_t offset, uint8_t openBus);

		void Write16(GbaBus& bus, uint32_t offset, uint16_t value);
		void Write8(GbaBus& bus, uint32_t offset, uint8_t value);

		/// <summary>Advance the cable delay counters and complete the transfers they finish.</summary>
		void Tick(GbaBus& bus, int cycles);

		/// <summary>Attach the other end of the link cable (nullptr detaches).</summary>
		void Attach(Sio* other);

		/// <summary>The peer, if the cable has one.</summary>
		Sio* Peer() const { return peer; }

		/// <summary>True while a transfer is in progress.</summary>
		bool Busy() const;

		/// <summary>Number of attached peers (the multiplayer mode asks for it).</summary>
		int ConnectedPlayers() const;

		/// <summary>The value the port drove onto the cable in the last transfer.</summary>
		uint16_t LastSent() const { return lastSent; }

		/// <summary>The value that came back from the cable in the last transfer.</summary>
		uint16_t LastReceived() const { return lastReceived; }

	private:
		Sio* peer = nullptr;

		// -- registers ---------------------------------------------------------------------

		uint16_t siomltSend = 0;	// 0x120 (also SIODATA32_L)
		uint16_t siodata32H = 0;	// 0x122
		uint16_t siodata8 = 0;		// 0x124
		uint16_t siocnt = 0;		// 0x128
		uint16_t rcnt = 0;		// 0x134
		uint16_t joycnt = 0;		// 0x140
		uint16_t joyRecv = 0;		// 0x150/0x152
		uint16_t joyTrans = 0;		// 0x154/0x156
		uint16_t joystat = 0;		// 0x158

		// -- transfer state ----------------------------------------------------------------

		bool active = false;
		int remainingCycles = 0;	// the cable delay left before both ends complete
		int mode = 0;				// SioMode: 0 = normal, 1 = multi, 2 = uart, 3 = gp
		bool master = false;
		uint16_t lastSent = 0;
		uint16_t lastReceived = 0;
		int baudCycles = 0;

		// A pending transfer is completed when the countdown reaches zero. Both ends are driven
		// from their own Tick, so a transfer completes only after the slower end finished.
		bool completionPending = false;

		void StartTransfer(GbaBus& bus);
		void CompleteTransfer(GbaBus& bus);

		/// <summary>How many CPU cycles one bit of the transfer takes in the current mode.</summary>
		int BaudCycles() const;

		/// <summary>The data this end drives onto the cable, in the current mode.</summary>
		uint16_t OutgoingData() const;

		/// <summary>Combine the peer's data into the received value, in the current mode.</summary>
		uint16_t IncomingData() const;
	};
}
