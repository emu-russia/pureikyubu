/*

# The Broadband Adapter (DOL-015)

The implementation of the device `bba.h` describes: the adapter's EXI shim and the Ethernet
controller behind it.

## How the console reaches it

Every access is an EXI immediate (or DMA) transfer, and a selection is a **command word followed by
the data**. The console asserts the chip select, writes the command word, moves the data, and
releases the select:

  * the **command word** of a controller access is a four-byte write whose high half is `0x8000`
    (read) or `0xC000` (write) and whose second byte is the controller address - sixteen bits, the
    register file in page 0 and the packet memory in the pages above it;
  * the command word of a **shim** access is a two-byte write of `reg << 8`, with `0x4000` in it
    for a write;
  * the transfers after the command word are the data: the bytes of the register or of the packet
    memory, the first one in bits 31:24 of the data register, and the address walks forward with
    every byte - which is how the driver reads the six MAC bytes or a packet in one burst.

The device is told which transfer is the command word (the first one after the chip select is
asserted), because the same two-byte write can carry a command or a byte of data.

The console never touches the Ethernet controller directly - the shim does, and the shim is what the
device emulates as a register file: the controller's registers are reached through the shim and
behave the way the MX98726/MX98728 family's descriptions say (the register *names* and *numbers* of
that family are what the public documentation records; see the specification page for how far that
map can be trusted for this particular part).

## The parts that are established and the parts that are modelled

Established by the documentation and by what the console's own driver does:

  * the EXI device ID `0x04020200`, read from shim register 0;
  * the selector word and the two register spaces;
  * the register map of the controller, including the MAC address registers, the identification
    registers, the link-state bits of the autonegotiation status, the receive/transmit page
    pointers and the boundary registers;
  * the initialisation order (MISC2, the interrupt mask, the test and boundary registers, then
    `0x02 = 0xF8` to acknowledge).

Modelled, because no source describes them for this part:

  * the **challenge/response** handshake. Its algorithm is not public, so the emulated adapter
    answers the challenge register with a fixed value and *accepts* whatever the console writes to
    the response register, reporting "OK". A console that validates the pair therefore proceeds;
    there is nothing here that could tell a right answer from a wrong one;
  * the **descriptor** of a received packet, which the specification gives as a length split across
    the middle of the word. The emulated ring writes and reads it that way, and the receive path is
    the one place where a real title is needed to confirm it.

## The network

The adapter has no network of its own: frames go to a `BbaBackend`, which the front end installs
(see bba.h). With none installed the adapter is a console with the cable unplugged - it identifies
itself, reports the link down, and no frame ever arrives.

*/

#include "pch.h"
#include "bba.h"

using namespace Debug;

// ---------------------------------------------------------------------------
// The backend of the build

namespace
{
	BbaBackend* g_backend = nullptr;
}

BbaBackend* BbaGetBackend()
{
	return g_backend;
}

void BbaSetBackend(BbaBackend* backend)
{
	g_backend = backend;
}

// ---------------------------------------------------------------------------
// The register numbers

namespace
{
	// The shim's own registers, addressed by the low selector.
	enum
	{
		SHIM_ID         = 0x00,     //!< the EXI device ID, and the transmit status byte
		SHIM_01         = 0x01,     //!< read during the driver's initialisation
		SHIM_IRQ_MASK   = 0x02,     //!< which of the chip's interrupts reach the console
		SHIM_IRQ_STATUS = 0x03,     //!< the latched interrupts, write 1 to clear
		SHIM_REVID      = 0x04,     //!< the revision of the adapter, written by the driver
		SHIM_CHALLENGE  = 0x08,     //!< the anti-piracy challenge (read, 4 bytes)
		SHIM_RESPONSE   = 0x09,     //!< the response to it (write, 4 bytes)
		SHIM_STATUS     = 0x0B,     //!< 1: the response was accepted, 2: it was not
		SHIM_0F         = 0x0F,     //!< read during the driver's initialisation
	};

	// The interrupt bits of the shim's status register.
	#define SHIM_IRQ_CHIP       0x80     //!< the Ethernet controller is asking
	#define SHIM_IRQ_KILL       0x40     //!< the "killing" interrupt (should not happen)
	#define SHIM_IRQ_CMDERR     0x20     //!< a command error
	#define SHIM_IRQ_CHALREQ    0x10     //!< the challenge was asked for
	#define SHIM_IRQ_CHALSTAT   0x08     //!< the challenge/response finished

	// The Ethernet controller's registers (offsets into the register file).
	enum
	{
		NCRA        = 0x00,
		NCRB        = 0x01,
		TRA         = 0x02,
		TRB         = 0x03,
		LTPS        = 0x04,
		LRPS        = 0x05,
		MISSED_L    = 0x06,
		MISSED_H    = 0x07,
		IMR         = 0x08,
		IR          = 0x09,
		BP          = 0x0A,         // 16 bit
		TLBP        = 0x0C,         // 16 bit
		TWP         = 0x0E,         // 16 bit
		TRP         = 0x12,         // 16 bit
		RXINTT      = 0x14,         // 16 bit
		RWP         = 0x16,         // 16 bit
		RRP         = 0x18,         // 16 bit
		RHBP        = 0x1A,         // 16 bit
		EEPROM      = 0x1C,
		BICT        = 0x1D,
		IORDP       = 0x1E,         // 16 bit
		PAR0        = 0x20,         // six bytes: the MAC address
		MAR0        = 0x26,         // eight bytes: the multicast hash table
		ANALOG      = 0x2E,
		DINTVAL     = 0x2F,
		NWAYC       = 0x30,
		NWAYS       = 0x31,
		GCA         = 0x32,
		GCB         = 0x33,
		TWD         = 0x34,         // 4 bytes, write only
		HOSTIF      = 0x3A,
		TXFIFOCNT   = 0x3E,         // 16 bit
		RRD         = 0x40,         // 4 bytes, read only
		ID1         = 0x44,         // 2 bytes, "MX"
		ID2         = 0x46,         // 2 bytes, "0001"
		WRTXFIFOD   = 0x48,         // 4 bytes, write only
		IORD        = 0x4C,         // 4 bytes, read only
		MISC2       = 0x50,
		HRPKTCNT    = 0x52,         // 16 bit
		FRAGCNT     = 0x54,         // 3 bytes

		CHIP_REGS   = 0x58
	};

	// The packet ring lives at page pointer 0x100 and runs to 0xF00; the pointers the driver writes
	// are offsets into it, so a 12-bit field with the low eight bits being the page and the high
	// four the position within it.
	#define PACKET_BASE     0x0100
	#define PACKET_END      0x0F00
	#define PACKET_SIZE     (PACKET_END - PACKET_BASE)     // 0xE00

	// The largest frame the adapter's FIFO takes directly (the specification's "maximum direct FIFO
	// packet").
	#define BBA_MAX_FRAME   1518
}

// ---------------------------------------------------------------------------
// The device

class BbaDevice : public PeripheralDevice
{
	// -- the shim ---------------------------------------------------------------------------

	uint8_t     shimIrqMask = 0;
	uint8_t     shimIrqStatus = 0;
	uint8_t     shimRevId = 0xD1;
	uint8_t     shimStatus = 1;         // the response was accepted
	uint8_t     shimChallenge[4];

	// -- the Ethernet controller ------------------------------------------------------------

	//! The controller's register file. A register that is two or four bytes wide is stored at its
	//! own offset, least significant byte first, which is how the byte-wide host interface presents
	//! it.
	uint8_t     chip[CHIP_REGS] = { 0 };

	//! The packet ring, in the adapter's own order: the descriptor of a received packet is followed
	//! by its payload.
	uint8_t     ring[PACKET_SIZE] = { 0 };

	//! The transmit FIFO: the console writes a frame into it four bytes at a time and the chip
	//! moves it to the ring and onto the wire when the transmitter is started.
	uint8_t     txFifo[BBA_MAX_FRAME + 4] = { 0 };
	uint32_t    txLength = 0;

	//! Where the burst reads and writes of the ring are: WRTXFIFOD and RRD have no address of their
	//! own, they walk the ring from the pointer the driver left.
	uint32_t    ringWalk = PACKET_BASE;

	//! The packets the backend has handed over and the console has not taken yet.
	uint32_t    rxPackets = 0;

	// -- the device --------------------------------------------------------------------------

	std::string mac = BBA_DEFAULT_MAC;
	bool        link = true;

	//! The address the adapter answers to, parsed out of the configuration once.
	uint8_t     macBytes[6] = { 0x00, 0x09, 0xBF, 0x00, 0x00, 0x01 };

	// -----------------------------------------------------------------------
	// The register file

	//! A two-byte register. The bytes are held in the order they travel on the wire, so the byte at
	//! the register's own address is the most significant one of the value.
	uint32_t Read16(int reg) const { return ((uint32_t)chip[reg] << 8) | (uint32_t)chip[reg + 1]; }
	void Write16(int reg, uint32_t value)
	{
		chip[reg] = (uint8_t)(value >> 8);
		chip[reg + 1] = (uint8_t)value;
	}

	//! One byte of the Ethernet controller, at its 16 bit address. The register file is the first
	//! page and the packet memory the pages above it (see `RingIndex`). The registers that are a
	//! view of the adapter's state rather than storage - the identification, the link state, the
	//! FIFO counters - answer here, so that a burst read of several bytes is a walk of the
	//! addresses, which is how the console's driver reads the MAC address.
	uint8_t ReadChipByte(uint32_t at)
	{
		if (at >= PACKET_BASE)
		{
			return ring[RingIndex(at)];
		}

		switch (at)
		{
			case ID1:       return 'M';             // the chip's family signature, "MX"
			case ID1 + 1:   return 'X';
			case ID2:       return '0';             // the chip's revision signature, "01"
			case ID2 + 1:   return '1';

			case NWAYS:
				// The link-state bits the driver polls: a live 10BASE-T link with autonegotiation
				// completed is what the adapter reports when its cable is in a working hub.
				return (uint8_t)((chip[NWAYS] & 0xFC) | (link ? 0x03 : 0x00));

			case HOSTIF:
				// DREQB (bit 2) is clear while the chip can take a command, RRDYB (bit 1) is clear
				// while a received packet is waiting, WRDYB (bit 0) while the write side is ready.
				return (uint8_t)((rxPackets > 0 ? 0x00 : 0x02) | 0x04);

			case EEPROM:
				// EELD and EESEL are up (no serial EEPROM is present on the adapter's board); the
				// data-out bit reads as 0.
				return (uint8_t)(chip[EEPROM] | 0x20);

			case ANALOG:
				// The transceiver is up: PWD10B (bit 2) and DS120 (bit 0) are what the driver
				// requires, and the emulated part reports both set.
				return (uint8_t)(chip[ANALOG] | 0x05);

			case FRAGCNT: case FRAGCNT + 1: case FRAGCNT + 2:
				return 0;

			default:
				return chip[at < CHIP_REGS ? at : 0];
		}
	}

	//! One byte written to the controller. A read-only register keeps its value; the transmit path
	//! and the ring pointers are the ones with a side effect.
	void WriteChipByte(uint32_t at, uint8_t value)
	{
		if (at >= PACKET_BASE)
		{
			ring[RingIndex(at)] = value;
			return;
		}

		switch (at)
		{
			case NCRA:
				// RESET is self-clearing, and the two transmit bits start the frame the FIFO holds:
				// the console writes 0x00 and then the status it read back with the start bit set.
				if ((value & 0x06) != 0)
				{
					Transmit();
					value &= (uint8_t)~0x06;
				}
				chip[NCRA] = value;
				return;

			case IR:
				// The chip's interrupt status is write-1-to-clear.
				chip[IR] = (uint8_t)(chip[IR] & ~value);
				UpdateInterrupts();
				return;

			case ID1: case ID1 + 1: case ID2: case ID2 + 1:
			case HOSTIF: case RRD:
				return;                             // read only

			case WRTXFIFOD:
				// The transmit FIFO: the console streams the frame into it (an immediate transfer
				// or a DMA one) and the transmitter takes it when NCRA says so.
				if (txLength < sizeof(txFifo))
				{
					txFifo[txLength++] = value;
					Write16(TXFIFOCNT, txLength);
				}
				return;

			default:
				if (at < CHIP_REGS)
				{
					chip[at] = value;

					if (at == RRP)
					{
						// A burst read of the ring follows the pointer the driver left.
						ringWalk = (Read16(RRP) & 0x0FFF) | PACKET_BASE;
					}
				}
				return;
		}
	}

	// -----------------------------------------------------------------------
	// The ring

	//! Where an offset in the packet memory is in the emulator's buffer.
	static uint32_t RingIndex(uint32_t offset)
	{
		return (offset - PACKET_BASE) % PACKET_SIZE;
	}

	//! Start the transmitter: the frame in the FIFO goes onto the wire, and the status the driver
	//! polls says the transmit finished without an error.
	void Transmit()
	{
		if (txLength == 0)
		{
			return;
		}

		if (g_backend != nullptr && txLength <= BBA_MAX_FRAME)
		{
			g_backend->Transmit(txFifo, txLength);
		}

		txLength = 0;
		Write16(TXFIFOCNT, 0);

		// LTPS (the last transmit packet status): no error, no collision, no underrun.
		chip[LTPS] = 0;
	}

	//! Ask the backend whether a frame arrived, and put it in the ring with the descriptor the
	//! specification describes when one did. The console finds it by comparing RWP and RRP.
	void PollReceive()
	{
		if (g_backend == nullptr || !g_backend->HasReceive())
		{
			return;
		}

		uint8_t frame[BBA_MAX_FRAME];
		size_t length = 0;

		if (!g_backend->Receive(frame, sizeof(frame), &length) || length == 0)
		{
			return;
		}

		if (length > BBA_MAX_FRAME)
		{
			length = BBA_MAX_FRAME;
		}

		uint32_t write = (Read16(RWP) & 0x0FFF) | PACKET_BASE;
		uint32_t total = (uint32_t)length + 4;      // the descriptor is part of the packet

		if (write + total > PACKET_END)
		{
			// The packet does not fit before the end of the ring: it is dropped, which the missed
			// packet counter reports.
			chip[MISSED_L]++;
			return;
		}

		// The descriptor: the length is split across the middle of the word - its low bits at
		// 23:20 and its high nibble at 11:8.
		uint32_t descriptor =
			(((uint32_t)length & 0x0F) << 20) |
			(((uint32_t)length & 0xFF0) << 8) |
			((((uint32_t)length >> 12) & 0x0F) << 8);

		uint32_t at = write;
		for (int i = 0; i < 4; i++)
		{
			ring[RingIndex(at)] = (uint8_t)(descriptor >> (24 - i * 8));
			at++;
		}

		for (size_t i = 0; i < length; i++)
		{
			ring[RingIndex(at)] = frame[i];
			at++;
		}

		Write16(RWP, at);

		// The receive interrupt: what the driver's callback is called for.
		chip[IR] |= 0x01;           // FRAGI is the receive-side cause the driver watches
		rxPackets++;
		UpdateInterrupts();
	}

	//! Move the chip's interrupt line to the console: the shim's status bit follows the chip's own
	//! interrupt status and the mask the driver wrote.
	void UpdateInterrupts()
	{
		bool pending = (chip[IR] & chip[IMR]) != 0;

		if (pending)
		{
			shimIrqStatus |= SHIM_IRQ_CHIP;
		}
		else
		{
			shimIrqStatus &= (uint8_t)~SHIM_IRQ_CHIP;
		}

		if (shimIrqStatus & shimIrqMask)
		{
			// The console's EXI interrupt line. The shim cannot raise it by itself in this
			// emulation - the channel's own interrupt is driven by the EXI block - so the state is
			// kept in the shim's status register, which is what the driver reads.
		}
	}

	// -----------------------------------------------------------------------
	// The shim

	//! One byte of the adapter's own command registers. The shim is an eight bit space, so its
	//! registers are held as the bytes they are; the ones that are a view of the adapter's state
	//! answer here.
	uint8_t ReadShimByte(int reg)
	{
		switch (reg)
		{
			case SHIM_ID + 0:
				// Read as one byte this is the transmit status (bit 3 is the acknowledge error and
				// bits 1:2 are what the driver polls); read as four it is the device ID, which
				// `ReadData` answers before the shim is asked.
				return 0x00;

			case SHIM_IRQ_MASK:     return shimIrqMask;
			case SHIM_IRQ_STATUS:   return shimIrqStatus;
			case SHIM_REVID:        return shimRevId;

			case SHIM_CHALLENGE + 0:
			case SHIM_CHALLENGE + 1:
			case SHIM_CHALLENGE + 2:
				// The challenge, which is read as the four bytes starting here (`ReadData`). The
				// register after these three is the status one, so the fourth byte is not named.
				shimIrqStatus |= SHIM_IRQ_CHALREQ;
				return shimChallenge[reg - SHIM_CHALLENGE];

			case SHIM_STATUS:       return shimStatus;

			default:                return 0;
		}
	}

	void WriteShimByte(int reg, uint8_t value)
	{
		switch (reg)
		{
			case SHIM_IRQ_MASK:
				shimIrqMask = value;
				UpdateInterrupts();
				return;

			case SHIM_IRQ_STATUS:
				// Write 1 to clear, like every other interrupt status of the pair.
				shimIrqStatus &= (uint8_t)~value;
				return;

			case SHIM_REVID:
				// The driver writes the revision it expects the adapter to report (two bytes: the
				// revision and the device id, which land in this register and the next one).
				shimRevId = value;
				return;

			case SHIM_RESPONSE:
				// The algorithm that would check the response is not public, so the emulated adapter
				// accepts it and says so - a console that validates the handshake proceeds, and
				// there is nothing here that could tell a right answer from a wrong one.
				shimStatus = 1;
				shimIrqStatus |= SHIM_IRQ_CHALSTAT;
				return;

			default:
				return;
		}
	}

	// -----------------------------------------------------------------------
	// The command word

	//! The space a command word selected.
	enum class Space
	{
		Shim,           //!< the adapter's own command registers
		Chip,           //!< the Ethernet controller behind the shim
	};

	Space       space = Space::Shim;
	uint32_t    address = 0;            //!< the shim register, or the chip's 16 bit address
	bool        writing = false;        //!< the direction the command word asked for
	bool        selValid = false;

	//! The word that begins a command sequence, which is what the console writes right after it
	//! asserts the chip select (see the module comment). The controller's address is sixteen bits -
	//! the register file is page 0 and the packet memory is the pages above it - and the shim's is
	//! the eight bit register number in the second byte.
	void Command(uint32_t word)
	{
		// Bit 31 says which space the word addresses and bit 30 the direction; the write flag of a
		// shim word (`0x4000`, so bit 30 once the word is in the data register) and the controller's
		// write bit (`0xC000` against `0x8000`) are the same bit, which is why one test serves both.
		writing = (word & 0x40000000) != 0;

		if ((word & 0x80000000) != 0)
		{
			// The controller: the address is sixteen bits, of which the console leaves eight. It
			// reaches above the register file - the packet memory is the pages from 0x100 - so the
			// address, not a register number, is what the command word carries.
			space = Space::Chip;
			address = (word >> 8) & 0xFFFF;
		}
		else
		{
			// The shim: the register is six bits at the top of the word.
			space = Space::Shim;
			address = (word >> 24) & 0x3F;
		}

		selValid = true;
	}

	//! One byte of the selected space, with the address walking forward as the console reads or
	//! writes: a burst transfer is a run of consecutive addresses, which is how the driver reads
	//! the six MAC bytes or a packet out of the ring in one go.
	//! Whether an address of the controller is a port rather than a cell of its memory: a burst
	//! that opens one of these streams every byte through it instead of walking on. The transmit
	//! FIFO is the one that matters - the driver writes a whole frame into it with one transfer.
	static bool PortAddress(uint32_t at)
	{
		return at == WRTXFIFOD || at == RRD || at == TWD || at == IORD;
	}

	uint8_t ReadByte()
	{
		uint8_t value = (space == Space::Chip) ? ReadChipByte(address) : ReadShimByte((int)address);

		if (space != Space::Chip || !PortAddress(address))
		{
			address++;
		}

		return value;
	}

	void WriteByte(uint8_t value)
	{
		if (space == Space::Chip)
		{
			WriteChipByte(address, value);

			if (!PortAddress(address))
			{
				address++;
			}
		}
		else
		{
			WriteShimByte((int)address, value);
			address++;
		}
	}

	//! The value bytes of an immediate transfer, most significant first: the console puts the first
	//! byte it wants to send in bits 31:24 of the data register.
	//!
	//! The four-byte read of the shim's register 0 is the identification - that is how the console
	//! asks a device for its EXI ID (`EXIGetID` writes the selector 0x0000 and reads four bytes) -
	//! while the same address read as a single byte is the transmit status byte.
	uint32_t ReadData(int bytes)
	{
		if (space == Space::Shim && bytes == 4)
		{
			if (address == SHIM_ID)
			{
				address += 4;
				return PERIPH_DEVICE_BBA_ID;
			}

			if (address == SHIM_CHALLENGE)
			{
				// The challenge itself: a fixed value, because the algorithm behind it is not
				// public (see the module comment). Reading it tells the console that it is talking
				// to a shim that has one.
				address += 4;
				shimIrqStatus |= SHIM_IRQ_CHALREQ;
				return ((uint32_t)shimChallenge[0] << 24) | ((uint32_t)shimChallenge[1] << 16) |
					((uint32_t)shimChallenge[2] << 8) | (uint32_t)shimChallenge[3];
			}
		}

		uint32_t value = 0;

		for (int i = 0; i < bytes; i++)
		{
			value |= (uint32_t)ReadByte() << (24 - i * 8);
		}

		return value;
	}

	void WriteData(uint32_t word, int bytes)
	{
		for (int i = 0; i < bytes; i++)
		{
			WriteByte((uint8_t)(word >> (24 - i * 8)));
		}
	}

	//! The bytes of a DMA transfer, which move a whole buffer at once.
	void ReadBuffer(uint8_t* buffer, uint32_t count)
	{
		for (uint32_t i = 0; i < count; i++)
		{
			buffer[i] = ReadByte();
		}
	}

	void WriteBuffer(const uint8_t* buffer, uint32_t count)
	{
		for (uint32_t i = 0; i < count; i++)
		{
			WriteByte(buffer[i]);
		}
	}

	// -----------------------------------------------------------------------

	void UpdateMacBytes()
	{
		unsigned b[6] = { 0 };
		int found = sscanf(mac.c_str(), "%x:%x:%x:%x:%x:%x",
			&b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);

		if (found != 6)
		{
			found = sscanf(mac.c_str(), "%x-%x-%x-%x-%x-%x",
				&b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
		}

		if (found != 6)
		{
			Report(Channel::EXI, "BBA: the address %s is not six hex bytes; the default is used\n",
				mac.c_str());
			mac = BBA_DEFAULT_MAC;
			sscanf(mac.c_str(), "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
		}

		for (int i = 0; i < 6; i++)
		{
			macBytes[i] = (uint8_t)b[i];
		}
	}

	//! Put the address into the controller's registers, and the identification the driver reads.
	void ApplyIdentification()
	{
		for (int i = 0; i < 6; i++)
		{
			chip[PAR0 + i] = macBytes[i];
		}

		// The receive and transmit pointers the driver initialises (it writes 0x0100 into both).
		Write16(RWP, PACKET_BASE);
		Write16(RRP, PACKET_BASE);
		Write16(BP, PACKET_END);
		Write16(RHBP, PACKET_END);
	}

public:
	BbaDevice()
	{
		// The shim's challenge is fixed (see the module comment).
		shimChallenge[0] = 0x5A;
		shimChallenge[1] = 0x5A;
		shimChallenge[2] = 0xA5;
		shimChallenge[3] = 0xA5;

		UpdateMacBytes();
		ApplyIdentification();
	}

	uint32_t Type() override { return PERIPH_DEVICE_BBA; }

	// -----------------------------------------------------------------------
	// Properties

	int PropertyCount() override { return 3; }

	const PeriphProperty* Property(int index) override
	{
		static const PeriphProperty props[] =
		{
			{ "MAC address", PERIPH_PROP_TEXT, 0 },
			{ "Link",        PERIPH_PROP_BOOL, 1 },
			{ "Network",     PERIPH_PROP_INFO, 2 },
		};

		return (index >= 0 && index < 3) ? &props[index] : nullptr;
	}

	std::string GetProperty(int id) override
	{
		switch (id)
		{
			case 0:
				return mac;

			case 1:
				return link ? "1" : "0";

			case 2:
				if (g_backend == nullptr)
				{
					return "No network backend in this build";
				}

				return g_backend->LinkUp() ? "Connected" : "Cable unplugged";
		}

		return "";
	}

	void SetProperty(int id, const std::string& value) override
	{
		switch (id)
		{
			case 0:
				mac = value;
				SetConfigEntryString(config, "MAC", Util::StringToWstring(mac).c_str());
				UpdateMacBytes();
				ApplyIdentification();
				break;

			case 1:
				link = value == "1" || value == "true";
				SetConfigEntryInt(config, "Link", link ? 1 : 0);
				break;
		}
	}

	// -----------------------------------------------------------------------

	void LoadConfig() override
	{
		mac = Util::WstringToString(GetConfigEntryString(config, "MAC"));

		if (mac.empty())
		{
			mac = BBA_DEFAULT_MAC;
		}

		link = GetConfigEntryInt(config, "Link", 1) != 0;

		UpdateMacBytes();
		ApplyIdentification();
	}

	void SaveConfig() override
	{
		SetConfigEntryString(config, "MAC", Util::StringToWstring(mac).c_str());
		SetConfigEntryInt(config, "Link", link ? 1 : 0);
	}

	// -----------------------------------------------------------------------
	// The bus

	//! One EXI transfer. `first` marks the transfer that begins a command sequence - the one the
	//! console makes right after it asserts the chip select - which is the command word; every
	//! transfer after it carries data. That is the protocol the console's own driver speaks (see
	//! the module comment): `[command][data]`, with the command word a two-byte write of the shim
	//! register or a four-byte write of the controller's address, and the data the bytes of a
	//! register or of the packet memory, walking forward.
	void ExiTransfer(Flipper::ExternalInterface* exi, bool first) override
	{
		EXIRegs& regs = exi->exi.regs[0];
		int rw = EXI_CR_RW(regs.cr);
		bool dma = (regs.cr & EXI_CR_DMA) != 0;

		// A frame that arrived while the console was busy is put into the ring before the driver
		// looks for it.
		PollReceive();

		if (first)
		{
			Command(regs.data);
			return;
		}

		if (!selValid)
		{
			Report(Channel::EXI, "BBA: a data transfer with no command before it\n");
			regs.data = 0;
			return;
		}

		if (dma)
		{
			// A DMA transfer moves a whole buffer: the driver sends a frame with one (into the
			// transmit FIFO) and reads a packet out of the ring with another.
			uint32_t count = regs.len;
			uint8_t* buffer = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForIO(
				regs.madr & EXI_MADR_MASK, count);

			if (buffer == nullptr)
			{
				Report(Channel::EXI, "BBA: the DMA buffer (0x%08X, %u bytes) is out of main memory\n",
					regs.madr, count);
				return;
			}

			if (rw == 1 || (rw == 2 && writing))
			{
				WriteBuffer(buffer, count);
			}
			else
			{
				ReadBuffer(buffer, count);
			}

			return;
		}

		int bytes = EXI_CR_TLEN(regs.cr) + 1;

		switch (rw)
		{
			case 0:     // read: the answer comes out of the selected address
				regs.data = ReadData(bytes);
				return;

			case 1:     // write: the bytes of the data register go to the selected address
				WriteData(regs.data, bytes);
				return;

			default:
				Report(Channel::EXI, "BBA: unknown transfer mode %i\n", rw);
				return;
		}
	}
};

// ---------------------------------------------------------------------------
// The registration

namespace
{
	PeripheralDevice* CreateBroadbandAdapter()
	{
		return new BbaDevice();
	}

	//! The adapter registers its own factory with the peripheral subsystem. The port it is meant for
	//! is serial port 1 (EXI0 chip select 2), which it shares with the modem adapter.
	struct BbaRegistrar
	{
		BbaRegistrar()
		{
			Peripherals::RegisterFactory(PERIPH_DEVICE_BBA, "Broadband Adapter",
				"DOL-015, 10BASE-T Ethernet on serial port 1", PERIPH_BUS_EXI,
				PERIPH_PAGE_NETWORK, CreateBroadbandAdapter);
		}
	};

	BbaRegistrar bba_registrar;
}
