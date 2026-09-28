/*

# The Modem Adapter (DOL-012)

The implementation of the device `mdm.h` describes: the UART the console configures, the AT
interface the driver talks to, and the status registers that carry the results back.

## The register interface

Every access is an EXI immediate transfer of the console's own shape: the outgoing word is
`(regno << 8) | value`, so the register number is the first byte on the wire and the value the
second, and a read transfer takes the register back. The device answers out of a byte register file
plus a small receive queue, and the two registers the driver polls are the status (`0x41`) and the
raw/error status (`0x12`, `0x0D`).

## The AT interface

The driver sends its setup strings through the UART data register. The emulated adapter collects
them until a carriage return, answers the way a modem without a line would, and puts the answer in
the receive queue - which is what the status register's "data available" bit reports and what a read
of the data register takes back out.

The setup commands the SDK sends (`ATW1\V0`, `ATE0`, `ATS0=…`, `ATS95=…`, `AT+GCI=…`, `AT+MS=…`,
`AT+ES=…`, `AT%C…`, `AT&P…`) are answered `OK`, because that is what the driver waits for before it
lets the socket layer open the connection. A dial (`ATDT…`, `ATDP…`) is answered with the line's
failure code, which for the emulation's default line is `NO DIALTONE`.

*/

#include "pch.h"
#include "mdm.h"

using namespace Debug;

// ---------------------------------------------------------------------------
// The line of the build

namespace
{
	ModemLine* g_line = nullptr;
}

ModemLine* ModemGetLine()
{
	return g_line;
}

void ModemSetLine(ModemLine* line)
{
	g_line = line;
}

// ---------------------------------------------------------------------------
// The registers

namespace
{
	// The register numbers the driver uses (modem-adapter.md 3.2). They are the adapter's own
	// register file; the vendor's 16550 names are given for the ones that have them.
	enum
	{
		MDM_DATA        = 0x00,     //!< RBR (read) / THR (write): the UART data, also the AT path
		MDM_ID          = 0x00,     //!< the EXI device ID: read as four bytes from register 0
		MDM_STATUS      = 0x01,     //!< the status/FIFO register polled before every transfer
		MDM_MCR         = 0x04,     //!< the modem control register
		MDM_05          = 0x05,
		MDM_DIVISOR0    = 0x06,     //!< the first divisor/threshold pair
		MDM_DIVISOR1    = 0x07,
		MDM_09          = 0x09,
		MDM_0A          = 0x0A,
		MDM_0B          = 0x0B,
		MDM_0C          = 0x0C,
		MDM_ERRSTAT     = 0x0D,     //!< the error status the driver reads
		MDM_THRESH0     = 0x0E,     //!< the second pair, read and written at initialisation
		MDM_THRESH1     = 0x0F,
		MDM_DIVISOR2    = 0x10,     //!< the third pair
		MDM_DIVISOR3    = 0x11,
		MDM_RAWSTAT     = 0x12,     //!< the raw status the driver reads
		MDM_SCRATCH     = 0x13,     //!< SCR: a byte the driver keeps for itself

		MDM_REGS        = 0x40
	};

	// The status register's bits. The layout is the emulation's own (the specification records the
	// register as "status/FIFO, polled before every payload transfer" and nothing else), so the two
	// the driver needs are named here and everything else reads zero.
	#define MDM_ST_RX_READY     0x01    //!< the answer queue has a byte for the console
	#define MDM_ST_TX_READY     0x02    //!< the UART can take another byte
}

// ---------------------------------------------------------------------------
// The device

class ModemDevice : public PeripheralDevice
{
	//! The register file, as the driver leaves it.
	uint8_t     regs[MDM_REGS] = { 0 };

	//! The answer queue: what the AT interface produced and the console has not read yet.
	uint8_t     rx[512] = { 0 };
	uint32_t    rxHead = 0;
	uint32_t    rxTail = 0;

	//! The AT command line being collected.
	char        atLine[256] = { 0 };
	uint32_t    atLength = 0;

	//! Whether the last AT command was a dial that the line refused.
	bool        carrier = false;

	uint32_t    QueueCount() const { return rxTail - rxHead; }

	void Queue(const char* text)
	{
		for (const char* p = text; *p != 0; p++)
		{
			if (rxTail - rxHead >= sizeof(rx))
			{
				return;     // the queue is full: the console is not reading it
			}

			rx[rxTail++ % sizeof(rx)] = (uint8_t)*p;
		}
	}

	//! A byte out of the answer queue.
	uint8_t Dequeue()
	{
		if (QueueCount() == 0)
		{
			return 0;
		}

		return rx[rxHead++ % sizeof(rx)];
	}

	// -----------------------------------------------------------------------

	//! The console wrote a byte to the UART: collect it into the AT line and answer the command when
	//! it is complete.
	void WriteData(uint8_t value)
	{
		if (value == '\r' || value == '\n')
		{
			if (atLength > 0)
			{
				atLine[atLength] = 0;
				Answer(atLine);
				atLength = 0;
			}
			return;
		}

		if (value == '\b')
		{
			if (atLength > 0)
			{
				atLength--;
			}
			return;
		}

		if (atLength + 1 < sizeof(atLine))
		{
			atLine[atLength++] = (char)value;
		}
	}

	//! The result of one AT command. A modem with no line plugged into it answers the setup commands
	//! and refuses to dial; the codes are the ones the driver parses.
	void Answer(const char* command)
	{
		std::string text = command;

		// An empty "AT" is the attention command.
		if (text.size() < 2 || (text[0] != 'A' && text[0] != 'a') || (text[1] != 'T' && text[1] != 't'))
		{
			Queue("\r\nERROR\r\n");
			return;
		}

		std::string body = text.substr(2);
		std::string upper;

		for (char c : body)
		{
			upper += (char)toupper((unsigned char)c);
		}

		// A dial: tone (`ATDT`) or pulse (`ATDP`) followed by the number.
		if (upper.compare(0, 2, "DT") == 0 || upper.compare(0, 2, "DP") == 0)
		{
			Dial(upper.c_str() + 2);
			return;
		}

		// A hang-up.
		if (upper.compare(0, 1, "H") == 0)
		{
			if (g_line != nullptr)
			{
				g_line->HangUp();
			}

			carrier = false;
			Queue("\r\nOK\r\n");
			return;
		}

		// A query the driver makes after a connection ("+MS?" and its relatives): answered with the
		// values the driver expects to parse, so that a caller that only asks stays happy.
		if (upper.find("+MS?") != std::string::npos)
		{
			Queue("\r\n+MS: 300,28800,300,28800\r\nOK\r\n");
			return;
		}

		// Everything else the driver sends (the result-code set-up, echo off, the S registers, the
		// country and modulation selections, the compression and dial-pulse settings) is accepted.
		Queue("\r\nOK\r\n");
	}

	//! The number after "ATDT"/"ATDP", handed to the line. With no line the modem reports the code a
	//! real one would.
	void Dial(const char* number)
	{
		if (g_line != nullptr && g_line->Dial(number))
		{
			carrier = true;
			Queue("\r\nCONNECT 28800\r\n");
			return;
		}

		carrier = false;

		std::string failure = g_line != nullptr ? g_line->DialFailure() : "NO DIALTONE";
		std::string answer = "\r\n" + failure + "\r\n";

		Queue(answer.c_str());
	}

	// -----------------------------------------------------------------------

	//! One byte of the modem's register file, which is what the data transfers of the protocol move.
	//! Register 0 is both the UART's data register and the first byte of the EXI device ID: the
	//! adapter's shim answers the four-byte read the console identifies it with (see `ReadData`).
	uint8_t ReadRegByte(int reg)
	{
		switch (reg)
		{
			case MDM_DATA:
				return Dequeue();

			case MDM_STATUS:
				// The console polls this before it takes data: the receive bit is what tells it that
				// there is something to take, and the transmit bit is always set because the
				// emulated UART has room for another byte.
				return (uint8_t)(MDM_ST_TX_READY | (QueueCount() > 0 ? MDM_ST_RX_READY : 0));

			case MDM_ERRSTAT:
				return 0;       // no framing, parity or overrun error

			case MDM_RAWSTAT:
				return (uint8_t)(carrier ? 0x10 : 0x00);

			case MDM_MCR: case MDM_05: case MDM_09: case MDM_0A: case MDM_0B: case MDM_0C:
			case MDM_DIVISOR0: case MDM_DIVISOR1:
			case MDM_THRESH0: case MDM_THRESH1: case MDM_DIVISOR2: case MDM_DIVISOR3:
			case MDM_SCRATCH:
				return regs[reg];

			default:
				return 0;
		}
	}

	void WriteReg(int reg, uint32_t value, int bytes)
	{
		switch (reg)
		{
			case MDM_DATA:
				WriteData((uint8_t)value);
				return;

			case MDM_STATUS: case MDM_ERRSTAT: case MDM_RAWSTAT:
				return;         // read only

			case MDM_MCR: case MDM_05: case MDM_09: case MDM_0A: case MDM_0B: case MDM_0C:
			case MDM_DIVISOR0: case MDM_DIVISOR1:
			case MDM_THRESH0: case MDM_THRESH1: case MDM_DIVISOR2: case MDM_DIVISOR3:
			case MDM_SCRATCH:
				regs[reg] = (uint8_t)value;
				return;

			default:
				return;
		}
	}

	// -----------------------------------------------------------------------

	void Reset()
	{
		memset(regs, 0, sizeof(regs));
		rxHead = rxTail = 0;
		atLength = 0;
		carrier = false;

		// The registers the driver expects to find in their power-on state: the scratch register and
		// the threshold pair are zero, and the divisor pairs hold what the bring-up writes.
		regs[MDM_DIVISOR0] = 0x00;
		regs[MDM_DIVISOR1] = 0x32;
		regs[MDM_THRESH0] = 0x00;
		regs[MDM_THRESH1] = 0x00;
	}

public:
	ModemDevice()
	{
		Reset();
	}

	uint32_t Type() override { return PERIPH_DEVICE_MODEM; }

	// -----------------------------------------------------------------------
	// Properties

	int PropertyCount() override { return 2; }

	const PeriphProperty* Property(int index) override
	{
		static const PeriphProperty props[] =
		{
			{ "Carrier", PERIPH_PROP_INFO, 0 },
			{ "Line",    PERIPH_PROP_INFO, 1 },
		};

		return (index >= 0 && index < 2) ? &props[index] : nullptr;
	}

	std::string GetProperty(int id) override
	{
		switch (id)
		{
			case 0:
				return carrier ? "Connected" : "No carrier";

			case 1:
				return g_line != nullptr ? "A line simulator is installed"
					: "No line (a dial reports NO DIALTONE)";
		}

		return "";
	}

	// -----------------------------------------------------------------------

	void LoadConfig() override {}

	void SaveConfig() override {}

	void Attach(int port) override
	{
		Report(Channel::EXI, "Modem: attached to %s\n", Peripherals::Instance().PortName(port));
	}

	// -----------------------------------------------------------------------
	// The bus

	//! One EXI transfer: `[command][data]`, the protocol the console's own modem driver speaks. The
	//! command word is a two-byte write of the register number in its second byte (`0x4000` in the
	//! word for a write), and the transfers after it are the data - one byte per register, walking
	//! forward with the address, which is how the driver reads a two-byte register.
	void ExiTransfer(Flipper::ExternalInterface* exi, bool first) override
	{
		EXIRegs& regs0 = exi->exi.regs[0];
		int rw = EXI_CR_RW(regs0.cr);
		bool dma = (regs0.cr & EXI_CR_DMA) != 0;

		if (first)
		{
			// The command word: the register is six bits at the top of the word and the write flag
			// (`0x4000` of the selector the driver builds, so bit 30 here) above the data transfers.
			address = (regs0.data >> 24) & 0x3F;
			writing = (regs0.data & 0x40000000) != 0;
			selValid = true;
			return;
		}

		if (!selValid)
		{
			Report(Channel::EXI, "Modem: a data transfer with no command before it\n");
			regs0.data = 0;
			return;
		}

		if (dma)
		{
			// The payload of a real call moves through a DMA transfer; with no line there is no
			// payload, so the transfer completes and nothing is moved.
			return;
		}

		int bytes = EXI_CR_TLEN(regs0.cr) + 1;

		if (rw == 1 || (rw == 2 && writing))
		{
			for (int i = 0; i < bytes; i++)
			{
				WriteReg((int)address, (regs0.data >> (24 - i * 8)) & 0xFF, bytes);
				address++;
			}
			return;
		}

		regs0.data = ReadData(bytes);
	}

	//! The answer bytes of a read transfer, the first one in bits 31:24 of the data register.
	//!
	//! The four-byte read of register 0 is the identification: the console asks for the device's EXI
	//! ID with it (`readCID` in the modem library), while the same address read as a single byte is
	//! the UART's data register - the length is what tells the two apart.
	uint32_t ReadData(int bytes)
	{
		if (address == 0 && bytes == 4)
		{
			address += 4;
			return PERIPH_DEVICE_MODEM_ID;
		}

		uint32_t value = 0;

		for (int i = 0; i < bytes; i++)
		{
			value |= (uint32_t)ReadRegByte((int)address) << (24 - i * 8);
			address++;
		}

		return value;
	}

	//! The register the last command word selected, and the direction it asked for.
	uint32_t address = 0;
	bool     writing = false;
	bool     selValid = false;
};

// ---------------------------------------------------------------------------
// The registration

namespace
{
	PeripheralDevice* CreateModemAdapter()
	{
		return new ModemDevice();
	}

	//! The modem registers its own factory with the peripheral subsystem. The port it is meant for
	//! is serial port 1 (EXI0 chip select 2), which it shares with the broadband adapter.
	struct ModemRegistrar
	{
		ModemRegistrar()
		{
			Peripherals::RegisterFactory(PERIPH_DEVICE_MODEM, "Modem Adapter",
				"DOL-012, 56 kbit/s dial-up on serial port 1", PERIPH_BUS_EXI, CreateModemAdapter);
		}
	};

	ModemRegistrar modem_registrar;
}
