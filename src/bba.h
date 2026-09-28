/*

# The Broadband Adapter (DOL-015)

The 10BASE-T Ethernet adapter. It plugs into the console's **serial port 1** and is reached over
**EXI channel 0, chip select 2** - the same line the modem uses, which is why the two are told apart
by their EXI device ID (`0x04020200` here, `0x02020000` for the modem).

The adapter is not memory-mapped. The console talks to a small **EXI shim** on the adapter's board
with a command protocol, and that shim fronts an off-the-shelf Ethernet controller (the Macronix
MX98730EC, a member of the MX98726/MX98728 family). Everything the console does is therefore one of
two things:

  * a **shim** access - the adapter's own command registers, which carry the device ID, the
    interrupt state, a revision, and the anti-piracy challenge/response;
  * a **chip** access - a register of the Ethernet controller, reached through the shim.

## The selector word

Every access is an EXI immediate transfer whose **high half is the selector**:

| Selector | Meaning |
|---|---|
| `0x8000 \| regno` | select a chip register for reading |
| `0xC000 \| regno` | select a chip register for writing (the low half is the value) |
| `regno` | select a shim register for reading |
| `0x4000 \| regno` | select a shim register for writing (the low half is the value) |

A write therefore carries the selector and the value in one 32-bit transfer, and a read is a
selector write followed by a read transfer that takes the selected register.

## What is emulated

The registers, the identification, the MAC address, the link state and the transmit/receive ring are
emulated; the network itself is behind a **backend** (see `BbaBackend`), which is what the front end
supplies. The device is complete without one - the console identifies the adapter, reads its MAC,
brings the chip up and can send and receive frames - but a frame handed to a backend with no network
on the other side goes nowhere, and no frame ever arrives.

*/

#pragma once

//! The MAC address the adapter answers to when the configuration does not name one. It is a
//! Nintendo address (the 00:09:BF block is Nintendo's), which is what a retail adapter carries.
#define BBA_DEFAULT_MAC     "00:09:BF:00:00:01"

// ---------------------------------------------------------------------------
// The network behind the adapter

//! Where the frames of the emulated adapter go, and where the ones it receives come from. The
//! device does not open a socket, a tap interface or a capture handle itself: those are host
//! facilities and belong to the front end, exactly like the rumble motor of a pad. A backend that
//! is not installed (nullptr) is a console whose adapter is plugged into nothing.
class BbaBackend
{
public:
	virtual ~BbaBackend() {}

	//! A frame the console transmitted. The length is the frame's own, without any padding.
	//!
	//! The console writes the frame into the adapter's FIFO two bytes at a time (the selector takes
	//! the high half of every EXI transfer), so this is called once per transmit command with the
	//! whole frame; it is not called per FIFO write.
	virtual void Transmit(const uint8_t* frame, size_t length) {}

	//! Whether a frame is waiting to be delivered to the console. The device polls this when the
	//! guest looks for a received packet.
	virtual bool HasReceive() { return false; }

	//! The next frame the console should receive. Answers false when there is none.
	virtual bool Receive(uint8_t* frame, size_t capacity, size_t* length) { return false; }

	//! The link state the adapter reports (a cable in a live hub).
	virtual bool LinkUp() { return false; }
};

//! The backend the device is using (nullptr: no network). The owner is whoever installed it - the
//! emulator installs one when it starts and takes it away when it is shut down (see main.cpp).
BbaBackend* BbaGetBackend();
void BbaSetBackend(BbaBackend* backend);

//! Create the backend of this build out of the configuration, or nullptr when the configuration
//! names no peer (or the host cannot give a socket). Implemented by the front end (`bbaudp.cpp`),
//! the same way `HostInputCreate` is: a device never opens a socket by itself.
BbaBackend* BbaNetworkCreate();

// ---------------------------------------------------------------------------
// The device

namespace Flipper
{
	class ExternalInterface;
}

//! The device model's DeviceID (see peripherals.h).
#define PERIPH_DEVICE_BBA_ID    0x04020200
