/*

# The Modem Adapter (DOL-012)

The 56 kbit/s dial-up modem. It shares the console's **serial port 1** with the Broadband Adapter
(EXI channel 0, chip select 2) and is told apart from it by its EXI device ID, `0x02020000`.

The adapter presents a **16550-style UART register file** to the console; the driver configures it
and then drives the modem itself with **AT commands**. A resident PPP layer carries IP over the
dial-up link.

## What the console does with it

Every register transaction is the same shape: the outgoing word is `(regno << 8) | value`, so the
big-endian EXI stream carries the register number first and the value second, and a read transfer
takes the register back. The device ID is read from register `0x02` and is compared twice during the
driver's initialisation.

The driver's bring-up (see modem-adapter.md 3.1) is: acknowledge `0x0002`, read the ID, acknowledge
`0x0080`, write the scratch register `0x53`, write the two divisor/threshold pairs (`0x46`/`0x47`
and `0x50`/`0x51`, then `0x4E`/`0x4F`), send the AT setup strings, and read the status register
`0x41` before every payload transfer.

## What is emulated

The register file, the identification, the acknowledge commands and the AT interface are emulated;
the **telephone line is not**. The emulated adapter is a modem with a working UART and no line
plugged into it: the setup commands are answered `OK` (which is what the driver waits for), and a
dial attempt is answered `NO DIALTONE`. The result codes are the ones the driver parses
(`CONNECT`, `NO CARRIER`, `NO DIALTONE`, `NO ANSWER`, `BUSY`, `RING`, `ERROR`), so a title that only
uses the network setup screens sees a modem that is present and configured but cannot reach a
provider - which is what a dial-up adapter is without a line simulator behind it. A front end that
has one (a serial-over-TCP bridge, a PPP server) installs a `ModemLine` and the AT interface hands
the call to it.

The register layout is a **model** in one respect, and the source marks it: the two spaces the
driver touches (the 16550 file the vendor documents and the wider register numbers the driver
actually uses) do not correspond, so the emulated registers are the ones the driver uses, stored
where it puts them and read back as it wrote them.

*/

#pragma once

//! The device model's DeviceID (see peripherals.h).
#define PERIPH_DEVICE_MODEM_ID  0x02020000

// ---------------------------------------------------------------------------
// The telephone line

//! What is on the other end of the adapter's line. The default is a line that is not there at all,
//! which is a modem that configures itself and refuses to dial; a front end with a line simulator
//! behind it installs its own and the AT interface hands the call over.
class ModemLine
{
public:
	virtual ~ModemLine() {}

	//! Dial `number`. Answers false when there is no line to dial - the modem then reports the
	//! result code a real one would ("NO DIALTONE").
	virtual bool Dial(const char* number) { return false; }

	//! Hang up.
	virtual void HangUp() {}

	//! Whether the line currently carries a call.
	virtual bool Connected() { return false; }

	//! The result code a dial attempt that cannot be made reports.
	virtual const char* DialFailure() { return "NO DIALTONE"; }
};

//! The line of this build (see mdm.cpp). nullptr means "no line simulator installed".
ModemLine* ModemGetLine();
void ModemSetLine(ModemLine* line);
