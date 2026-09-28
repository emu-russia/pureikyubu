/*

# The Game Boy Player (DOL-017)

The implementation of the device `gbplayer.h` describes: the cartridge bay, the machine behind it,
the controls the host drives, the properties the settings window edits, and the two sides of the
port's window that the console actually talks to.

## The machine

The Player runs the Game Pak itself, so the device owns a Game Boy Advance machine (the module in
`src/gba`, the same one the emulator runs as a portable console) and gives it the cartridge the user
put in. The machine is stepped from the emulated time base: the byte-wide port's blocks tell the
console what the Player is doing, but the thing that produces a picture is the Game Pak running in
there, and it has to run at the rate of a real Game Boy Advance (16777216 Hz, 280896 cycles to the
frame).

## The window

The console reaches the port as a set of 32-byte **channels** at fixed offsets, every one of them
moved by an ARAM DMA (see wiki/gbplayer.md for the map and for how it was read off the disc's code).
Three kinds of block matter here:

  * the **identification mailbox** (window offset 0) answers the *complement* of the last block
    written to it. That is the handshake: the console writes a 32-byte block of a constant byte and
    requires its complement back, which is how it tells a Game Boy Player from a plain SDRAM
    expansion module (and how it tells "something is there" from "nothing is");
  * the **command channels** (`0x40_0000`, `0x50_0000`, `0x90_0000`, `0xC0_0000`, `0xD0_0000`) are
    registers *both* sides write. The console's own command routine reads a channel's status byte,
    clears the event bits it is acknowledging, sets the bits of the command it is sending and writes
    the result back; the device answers the next read with what the channel then holds. A channel
    that answered a constant would never let an event be acknowledged, and the console's driver would
    poll it in a loop forever - which is what the Start-up Disc did until the writes were honoured;
  * a channel the console has not written to yet, and the **state block** it polls in between,
    answer the device's own state: whether a Game Pak is in the bay, whether the machine is running,
    and the frame counter.

The port's interrupt line (PI bit 13) belongs to the **device**: it is raised here when the Player
switches on, and the console's ISR stops it by clearing the status bits it reads. Raising it on
every block the console moves would have the console servicing an interrupt it caused itself.

The commands themselves - which bit starts the Player's uploaded client, and what that client
answers with - are not established: the disc tells the Player to start and then waits for the
program it uploaded into ARAM (see wiki/gbplayer.md) to answer, and nothing in this emulation runs
that program yet. The channels hold what the console writes, which is what the protocol needs from
this side, and the module comment at `Read` says where the shape comes from.

*/

#include "pch.h"
#include "gbplayer.h"
#include "gba/gba.h"

using namespace Debug;

//! The actuators of the Game Boy Player. A Game Pak of the Player is driven exactly like a
//! handheld: eight directions, four buttons and the two shoulder buttons, and every one of them is
//! a plain button (the analog sticks of a GameCube pad are not part of the device).
static const PeriphActuator gbp_actuators[GBP_ACT_MAX] =
{
	{ "Buttons", "A",      "A",      1 },
	{ "Buttons", "B",      "B",      1 },
	{ "Buttons", "Select", "SELECT", 1 },
	{ "Buttons", "Start",  "START",  1 },
	{ "D-Pad",   "Right",  "RIGHT",  1 },
	{ "D-Pad",   "Left",   "LEFT",   1 },
	{ "D-Pad",   "Up",     "UP",     1 },
	{ "D-Pad",   "Down",   "DOWN",   1 },
	{ "Buttons", "R",      "R",      1 },
	{ "Buttons", "L",      "L",      1 },
};

// ---------------------------------------------------------------------------
// The properties of the device

namespace
{
	enum
	{
		GBP_PROP_GAMEPAK = 0,       //!< the cartridge image in the bay
		GBP_PROP_BIOS,              //!< the BIOS of the machine inside the Player
		GBP_PROP_RUN,               //!< run the Game Pak while the Player is up
		GBP_PROP_STATE,             //!< what is in the bay, read only

		GBP_PROP_MAX
	};

	const PeriphProperty gbp_properties[GBP_PROP_MAX] =
	{
		{ "Game Pak",      PERIPH_PROP_ROM,  GBP_PROP_GAMEPAK },
		{ "BIOS",          PERIPH_PROP_ROM,  GBP_PROP_BIOS },
		{ "Run Game Pak",  PERIPH_PROP_BOOL, GBP_PROP_RUN },
		{ "Player",        PERIPH_PROP_INFO, GBP_PROP_STATE },
	};
}

//! The debug command that writes the Player's picture out (see the registration at the end of the
//! file); the device's own constructor publishes it, so it has to be declared before the class.
//! It is in the anonymous namespace, as its definition is.
namespace
{
	Json::Value* cmd_gbpshot(std::vector<std::string>& args);
}

// ---------------------------------------------------------------------------
// The device

class GameBoyPlayerDevice : public PeripheralDevice, public Flipper::HSPDevice
{
	int             port = -1;
	int             state[GBP_ACT_MAX] = { 0 };
	PeriphBindings  bindings[GBP_ACT_MAX];

	//! The cartridge image and the BIOS the user picked (empty: the bay is empty / the built-in
	//! boot ROM).
	std::wstring    gamePak;
	std::wstring    bios;

	//! The machine that runs the cartridge. It exists whether or not a Game Pak is in the bay: a
	//! Player that is plugged in and empty is still a Player the console can talk to (and the
	//! Start-up Disc's menu offers "Change Game Pak" while it runs).
	std::unique_ptr<GBA::GbaSystem> gba;

	//! When the next frame of the Game Pak is due, in the emulated time base.
	int64_t         nextFrame = 0;

	//! Whether the Game Pak runs while the Player is up. The machine inside the Player is a whole
	//! console of its own, and emulating it costs about as much as the console the Player is
	//! attached to; a user who has a cartridge in the bay but is playing a GameCube game can leave
	//! it off and the Game Pak stays where it is (see Tick).
	bool            run = true;

	//! Whether the Player has been switched on: the console has talked to the port since the
	//! machine was reset. A Player that is plugged in with a Game Pak in it still costs nothing
	//! while the console ignores it - which is every game that is not the Start-up Disc - and the
	//! Game Pak starts running the moment the disc brings the Player up (see Tick).
	bool            active = false;

	//! The "Game Pak is in the bay" line of the device's status (see StatusBlock).
	bool            cartridgeIn = false;

	// -----------------------------------------------------------------------
	// The window

	//! How many of the console's 32-byte blocks the device keeps. The console uses seven offsets
	//! (see the module comment); one slot each, plus room for the base mailbox.
	static const int BlockCount = 8;

	struct Block
	{
		bool        valid = false;
		uint32_t    offset = 0;
		uint8_t     data[HSP_BLOCK_SIZE] = { 0 };
	};

	Block           blocks[BlockCount];

	//! The serial number of the frame the Game Pak last produced (the console watches it to know
	//! that the picture moved).
	uint32_t        frames = 0;

	//! The slot the block of an offset lives in, or nullptr.
	Block* Find(uint32_t offset)
	{
		for (auto& block : blocks)
		{
			if (block.valid && block.offset == offset)
			{
				return &block;
			}
		}

		return nullptr;
	}

	//! The slot a block of an offset is written to (an unused one when the offset is new).
	Block* Slot(uint32_t offset)
	{
		Block* found = Find(offset);

		if (found != nullptr)
		{
			return found;
		}

		for (auto& block : blocks)
		{
			if (!block.valid)
			{
				block.valid = true;
				block.offset = offset;
				return &block;
			}
		}

		// Every slot is taken: a device that answers more offsets than it knows about recycles the
		// oldest one. The console's own offsets are fixed, so this only happens to a guest that
		// invents addresses.
		blocks[0].offset = offset;
		return &blocks[0];
	}

	//! Whether the machine inside the Player is running the Game Pak: it is switched on by the
	//! console (see `Power`) and only then does anything happen inside.
	bool running() const { return active && run && gba != nullptr; }

	//! The byte at offset 31 of a status block - the one the console reads. What its bits mean is
	//! not established (see the module comment); the emulation reports "the bay holds a Game Pak"
	//! and "the machine is running" in the two low bits, and `GBP_STATUS` overrides the whole byte
	//! for experiments against a running disc.
	uint8_t statusByte()
	{
		const char* override = getenv("GBP_STATUS");
		if (override != nullptr && override[0] != 0)
		{
			return (uint8_t)strtoul(override, nullptr, 16);
		}

		// The two low bits are the only ones this device has something to report about, and they
		// are the same two the block's first byte carries: a Game Pak in the bay, and a machine
		// that is running. Every other bit is unknown and stays clear - a status that is *decided*
		// is what a caller can act on, where the half-word the return register happened to hold was
		// not. (This path had no return at all, which is what a status read saw.)
		return (uint8_t)((cartridgeIn ? 0x01 : 0x00) | (running() ? 0x02 : 0x00));
	}

	//! The status the device answers a status read with. The first byte is the state of the bay
	//! (bit 0: a Game Pak is in it, bit 1: the machine is running), the rest is the frame counter
	//! in the byte order the console reads its status words in (a big-endian word at +0x18).
	void StatusBlock(uint8_t* data)
	{
		memset(data, 0, HSP_BLOCK_SIZE);

		data[0] = (uint8_t)((cartridgeIn ? 1 : 0) | (running() ? 2 : 0));

		// The status byte the console's driver actually reads is the *last* one of the block: its
		// per-offset routines take `lbz data,31` and permute those bits into their own status word.
		data[31] = statusByte();


		data[0x18] = (uint8_t)(frames >> 24);
		data[0x19] = (uint8_t)(frames >> 16);
		data[0x1A] = (uint8_t)(frames >> 8);
		data[0x1B] = (uint8_t)(frames);
	}

public:
	GameBoyPlayerDevice()
	{
		for (int i = 0; i < GBP_ACT_MAX; i++)
		{
			bindings[i].keyboard = 0;
			bindings[i].gamepad = 0;
		}

		gba = std::make_unique<GBA::GbaSystem>();

		// The Player's picture has no other way out of a GameCube session, so the device publishes
		// the command that writes it out. It is done here, when a Player is created, and not from a
		// static constructor: a static one would run before the debug interface it registers with.
		static bool jdiRegistered = false;

		if (!jdiRegistered)
		{
			jdiRegistered = true;
			JDI::Hub.AddCmd("gbpshot", cmd_gbpshot);
		}
	}

	uint32_t Type() override { return PERIPH_DEVICE_GBPLAYER; }

	// -----------------------------------------------------------------------
	// Actuators

	int ActuatorCount() override { return GBP_ACT_MAX; }

	const PeriphActuator* Actuator(int index) override
	{
		return (index >= 0 && index < GBP_ACT_MAX) ? &gbp_actuators[index] : nullptr;
	}

	PeriphBindings* ActuatorBindings(int index) override
	{
		return (index >= 0 && index < GBP_ACT_MAX) ? &bindings[index] : nullptr;
	}

	void SetState(int actuator, int value) override
	{
		if (actuator >= 0 && actuator < GBP_ACT_MAX)
		{
			state[actuator] = value != 0 ? 1 : 0;
		}

		// The host's controls are the Game Pak's keys: the mask is handed to the machine on every
		// change, so a key press reaches the cartridge on the next frame it runs.
		if (gba != nullptr)
		{
			gba->SetPressedKeys(KeyMask());
		}
	}

	//! The keys the host is holding, as the KEY_* bits of the machine (see gba_keypad.h).
	uint16_t KeyMask() const
	{
		uint16_t mask = 0;

		if (state[GBP_ACT_A])      mask |= GBA::KEY_A;
		if (state[GBP_ACT_B])      mask |= GBA::KEY_B;
		if (state[GBP_ACT_SELECT]) mask |= GBA::KEY_SELECT;
		if (state[GBP_ACT_START])  mask |= GBA::KEY_START;
		if (state[GBP_ACT_RIGHT])  mask |= GBA::KEY_RIGHT;
		if (state[GBP_ACT_LEFT])   mask |= GBA::KEY_LEFT;
		if (state[GBP_ACT_UP])     mask |= GBA::KEY_UP;
		if (state[GBP_ACT_DOWN])   mask |= GBA::KEY_DOWN;
		if (state[GBP_ACT_R])      mask |= GBA::KEY_R;
		if (state[GBP_ACT_L])      mask |= GBA::KEY_L;

		return mask;
	}

	// -----------------------------------------------------------------------
	// Properties

	int PropertyCount() override { return GBP_PROP_MAX; }

	const PeriphProperty* Property(int index) override
	{
		return (index >= 0 && index < GBP_PROP_MAX) ? &gbp_properties[index] : nullptr;
	}

	std::string GetProperty(int id) override
	{
		switch (id)
		{
			case GBP_PROP_GAMEPAK:
				return Util::WstringToString(gamePak);

			case GBP_PROP_BIOS:
				return Util::WstringToString(bios);

			case GBP_PROP_RUN:
				return run ? "1" : "0";

			case GBP_PROP_STATE:
			{
				if (gba == nullptr || !gba->RomLoaded())
				{
					return "No Game Pak";
				}

				// The cartridge reports its own title and game code out of its header, which is
				// what the Start-up Disc's menu shows as well.
				std::string title = gba->RomTitle();
				std::string code = gba->RomGameCode();

				if (title.empty())
				{
					title = "Game Pak";
				}

				return title + " (" + code + ")";
			}
		}

		return "";
	}

	void SetProperty(int id, const std::string& value) override
	{
		switch (id)
		{
			case GBP_PROP_GAMEPAK:
			{
				if (Util::WstringToString(gamePak) == value)
				{
					return;
				}

				gamePak = Util::StringToWstring(value);
				SetConfigEntryString(config, "GamePak", gamePak.c_str());

				InsertGamePak();
				break;
			}

			case GBP_PROP_BIOS:
			{
				if (Util::WstringToString(bios) == value)
				{
					return;
				}

				bios = Util::StringToWstring(value);
				SetConfigEntryString(config, "BIOS", bios.c_str());

				InsertBios();
				break;
			}

			case GBP_PROP_RUN:
			{
				run = value == "1" || value == "true";
				SetConfigEntryInt(config, "Run", run ? 1 : 0);
				break;
			}
		}
	}

	// -----------------------------------------------------------------------
	// The cartridge bay

	//! Put the cartridge the configuration names into the bay (the bay is emptied when the file is
	//! not there or the name is empty). This is what the settings window's "Game Pak" row ends in,
	//! and it is also what a Player that is plugged in runs on power-up.
	void InsertGamePak()
	{
		if (gba == nullptr)
		{
			return;
		}

		if (gamePak.empty() || !Util::FileExists(gamePak))
		{
			gba->EjectRom();
			cartridgeIn = false;

			if (!gamePak.empty())
			{
				Report(Channel::HSP, "Game Boy Player: cannot find the Game Pak %s\n",
					Util::WstringToString(gamePak).c_str());
			}

			return;
		}

		std::string error;

		if (!gba->LoadRomFile(Util::WstringToString(gamePak), error))
		{
			Report(Channel::HSP, "Game Boy Player: %s\n", error.c_str());
			gba->EjectRom();
			cartridgeIn = false;
			return;
		}

		cartridgeIn = true;

		Report(Channel::HSP, "Game Boy Player: inserted %s (%s)\n",
			gba->RomTitle().c_str(), gba->RomGameCode().c_str());

		gba->Reset();
		gba->SetPressedKeys(KeyMask());

		// The machine starts running from the moment it is powered on.
		nextFrame = 0;
		frames = 0;
	}

	//! Install the BIOS image the configuration names (an empty name restores the built-in boot
	//! ROM of the machine).
	void InsertBios()
	{
		if (gba == nullptr)
		{
			return;
		}

		std::string error;

		if (!gba->LoadBiosFile(Util::WstringToString(bios), error))
		{
			Report(Channel::HSP, "Game Boy Player: %s\n", error.c_str());
		}
	}

	// -----------------------------------------------------------------------

	void LoadConfig() override
	{
		gamePak = GetConfigEntryString(config, "GamePak");
		bios = GetConfigEntryString(config, "BIOS");
		run = GetConfigEntryInt(config, "Run", 1) != 0;

		InsertBios();
		InsertGamePak();
	}

	void SaveConfig() override
	{
		SetConfigEntryString(config, "GamePak", gamePak.c_str());
		SetConfigEntryString(config, "BIOS", bios.c_str());
		SetConfigEntryInt(config, "Run", run ? 1 : 0);
	}

	void Attach(int port) override
	{
		this->port = port;

		// The device goes onto the Hi-Speed Port of the machine. A machine that is not there yet
		// (the pool is built before any console is) is what MachineOpened is for: the port is the
		// one that exists with the console.
		if (Flipper::HW != nullptr && Flipper::HW->hsp != nullptr)
		{
			Flipper::HW->hsp->Plug(this);
		}
	}

	void Detach() override
	{
		if (Flipper::HW != nullptr && Flipper::HW->hsp != nullptr &&
			Flipper::HW->hsp->Device() == this)
		{
			Flipper::HW->hsp->Unplug();
		}

		active = false;
		port = -1;
	}

	// -----------------------------------------------------------------------
	// The Hi-Speed Port device

	int ExpansionSizeCode() override
	{
		// The Game Boy Player's window is the 16 MB the console's own ARAM driver settles on
		// (AMCR expansion code 3). It is a window of mailboxes, not memory, which is why the
		// console's plain-SDRAM probe must fail before the Player is recognised.
		return 3;
	}

	void Reset() override
	{
		frames = 0;
		nextFrame = 0;
		active = false;
		blocksMoved = 0;

		if (gba != nullptr)
		{
			gba->Reset();
			gba->SetPressedKeys(KeyMask());
		}
	}

	//! How many blocks the console has to move through the window before the emulation treats the
	//! Player as switched on. Every GameCube title brings its ARAM library up, and that library
	//! *probes* for a plain SDRAM expansion module by moving a handful of test blocks past the
	//! 16 MB boundary - which lands in this window. The Player's own driver, once it has identified
	//! the device, moves hundreds of blocks; the count is what tells the two apart, so that an
	//! inserted Game Pak costs nothing to a game that only probed.
	static const int  PowerAfterBlocks = 64;

	//! How many blocks the console has moved through the window since the machine was reset.
	int             blocksMoved = 0;

	//! The console has addressed the Player: from now on it is running, and so is the Game Pak.
	void Power()
	{
		if (blocksMoved < PowerAfterBlocks && ++blocksMoved < PowerAfterBlocks)
		{
			return;
		}

		if (!active)
		{
			active = true;
			nextFrame = 0;

			if (gba != nullptr)
			{
				gba->Reset();
				gba->SetPressedKeys(KeyMask());
			}

			// The one event of the device's own that the console has to be told about: the Player
			// has been switched on and its status channel now carries the bay and running bits.
			if (Flipper::HW != nullptr && Flipper::HW->hsp != nullptr)
			{
				Flipper::HW->hsp->AssertId();
			}
		}
	}

	void Write(uint32_t offset, const uint8_t* data, uint32_t len) override
	{
		Power();

		// A write is a command and is recorded whole: the device answers a read from its own state
		// (see Read), not from what the command bytes were.
		while (len > 0)
		{
			Block* block = Slot(offset);
			uint32_t at = offset % HSP_BLOCK_SIZE;
			uint32_t chunk = HSP_BLOCK_SIZE - at;

			if (chunk > len)
			{
				chunk = len;
			}

			memcpy(block->data + at, data, chunk);

			data += chunk;
			offset += chunk;
			len -= chunk;
		}
	}

	void Read(uint32_t offset, uint8_t* data, uint32_t len) override
	{
		Power();

		// The answer of one block, which is what the device has to say about any offset.
		uint8_t answer[HSP_BLOCK_SIZE];

		if (offset < HSP_BLOCK_SIZE)
		{
			// The identification mailbox is the handshake of the port: the console writes a 32-byte
			// block of a constant byte and requires the complement back, which is the one thing a
			// memory window cannot do and the way the Player is told apart from a plain SDRAM module.
			// The mailbox is the block *at this offset*, not whichever slot the pool handed out
			// first: only the console's own identification writes to offset 0, but it is a block of
			// the same pool as the channels and the answer has to be about the right one.
			Block* mailbox = Find(0);
			uint8_t zero[HSP_BLOCK_SIZE] = { 0 };
			const uint8_t* block = mailbox != nullptr ? mailbox->data : zero;

			for (int i = 0; i < HSP_BLOCK_SIZE; i++)
			{
				answer[i] = (uint8_t)~block[i];
			}
		}
		else
		{
			// Every other offset the console uses is one of its command channels, and each of them is
			// a register that *both* sides write: a read answers what the channel holds. The console's
			// own command routine makes that plain - it reads the status out of `+0x400000`, clears
			// the event bits it is acknowledging, sets the bits of the command it is sending and
			// writes the result back. A channel that answered a constant would never let an event be
			// acknowledged: the console would read the same status forever, acknowledge it, and poll
			// in a loop - which is exactly what the Start-up Disc did before this was modelled.
			Block* block = Find(offset);

			if (block != nullptr)
			{
				memcpy(answer, block->data, HSP_BLOCK_SIZE);
			}
			else
			{
				// A channel the console has not written to yet answers the device's own state.
				StatusBlock(answer);
			}
		}

		// The port's contract is that the whole range a read asks for is answered (see HSPDevice in
		// hsp.h), and the console does ask for more than one block: the frame stream at `+0x800000`
		// is read in 3840-byte blocks (wiki/gbplayer.md 4). Only the block at `offset` is the
		// device's - the stream itself is not modelled - so the rest of the range is answered with
		// zeros. A guest that read less would be handed whatever its own buffer happened to hold.
		uint32_t head = len < HSP_BLOCK_SIZE ? len : HSP_BLOCK_SIZE;

		memcpy(data, answer, head);

		if (len > head)
		{
			memset(data + head, 0, len - head);
		}
	}

	//! The emulated time advanced: run the Game Pak at the rate of the machine inside the Player.
	void Tick(int64_t ticks) override
	{
		// A Game Pak that the console has not switched the Player on for is not running: the
		// machine inside the Player is its own, and it costs as much to emulate as the console
		// does. An inserted Game Pak therefore costs nothing to a GameCube game that never asks
		// the port for anything.
		if (!run || !active || gba == nullptr || !gba->RomLoaded())
		{
			return;
		}

		// One frame of a Game Boy Advance is 280896 cycles of its 16777216 Hz clock. The machine
		// runs whole frames, so a `Tick` that is early by a fraction of a frame does not run one.
		int64_t period = Core->OneSecond() * 280896 / 16777216;

		if (period <= 0)
		{
			return;
		}

		if (nextFrame == 0)
		{
			nextFrame = ticks;
		}

		int budget = 4;     // never fall more than a few frames behind the console

		while (ticks >= nextFrame && budget-- > 0)
		{
			gba->RunFrame();
			frames++;
			nextFrame += period;
		}

		if (ticks >= nextFrame)
		{
			// Too far behind to catch up: skip to now rather than run a burst of frames.
			nextFrame = ticks + period;
		}
	}

	// -----------------------------------------------------------------------
	// What the front end can show

	//! The machine that runs the Game Pak, for a front end that presents its picture and its sound
	//! and for the debug command that writes it out (see `gbpshot`).
	GBA::GbaSystem* Gba() override { return gba.get(); }

	GBA::GbaSystem* System() { return gba.get(); }
};

// ---------------------------------------------------------------------------
// The registration

namespace
{
	PeripheralDevice* CreateGameBoyPlayer()
	{
		return new GameBoyPlayerDevice();
	}

	//! The Player registers its own factory with the peripheral subsystem. A build that does not
	//! compile this file (or the GBA core it drives) has no Game Boy Player.
	struct GameBoyPlayerRegistrar
	{
		GameBoyPlayerRegistrar()
		{
			Peripherals::RegisterFactory(PERIPH_DEVICE_GBPLAYER, "Game Boy Player",
				"DOL-017, the add-on on the Hi-Speed Port", PERIPH_BUS_HSP, CreateGameBoyPlayer);
		}
	};

	GameBoyPlayerRegistrar gameboyplayer_registrar;

	// -----------------------------------------------------------------------
	// The Player's picture, for a front end (or a script) that cannot show it

	//! The device of the pool that carries a portable machine and has one running, or nullptr. Both
	//! of them are asked: the Player in its bay and the Game Boy Advance on a link cable are the two
	//! devices whose picture a GameCube session cannot otherwise see.
	//! \param match a word to look for in the device's name. An empty one takes the first device
	//!        that carries a running machine, which is the Player when the pool has both.
	PeripheralDevice* PooledGba(const std::string& match, std::string* name)
	{
		Peripherals& pool = Peripherals::Instance();

		for (int i = 0; i < pool.Count(); i++)
		{
			PeripheralDevice* device = pool.Device(i);

			if (device == nullptr || device->Gba() == nullptr || !device->Gba()->RomLoaded())
			{
				continue;
			}

			std::string deviceName = pool.Name(i);

			if (match.empty() || deviceName.find(match) != std::string::npos)
			{
				if (name != nullptr)
				{
					*name = deviceName;
				}

				return device;
			}
		}

		return nullptr;
	}

	/*
	 * Write the picture the Game Pak inside the Player is producing to a PNG.
	 *
	 *   gbpshot <filename.png> [device]
	 *
	 * With a third word only the device whose name contains it is looked at, which is how a run that
	 * has both a Player and a Game Boy Advance on a link cable chooses between them (`gbpshot
	 * gba.png GBA`).
	 *
	 * The frame is the 240x160 LCD of the portable machine a peripheral of the pool is running - the
	 * Game Pak in the Game Boy Player's bay, or the Game Boy Advance on a link cable - which is the
	 * picture that device puts on the console's video output on real hardware. A run with neither
	 * device in the pool is told so rather than being given a black picture.
	 */
	Json::Value* cmd_gbpshot(std::vector<std::string>& args)
	{
		if (args.size() < 2)
		{
			Report(Channel::Norm, "gbpshot: PNG file name expected\n");
			return nullptr;
		}

		std::string match = args.size() > 2 ? args[2] : "";
		std::string deviceName;
		PeripheralDevice* device = PooledGba(match, &deviceName);

		if (device == nullptr)
		{
			Report(Channel::Norm, "gbpshot: no device with a Game Pak in it "
				"(a Game Boy Player with a cartridge, or a Game Boy Advance on a link cable)\n");
			return nullptr;
		}

		GBA::GbaSystem* gba = device->Gba();

		const uint32_t* frame = gba->FrameBuffer();
		std::vector<uint8_t> rgb((size_t)GBA::ScreenWidth * GBA::ScreenHeight * 3);

		// The machine's frame is XRGB8888; the PNG keeps the three colours.
		for (size_t i = 0; i < rgb.size() / 3; i++)
		{
			uint32_t pixel = frame[i];
			rgb[i * 3 + 0] = (uint8_t)(pixel >> 16);
			rgb[i * 3 + 1] = (uint8_t)(pixel >> 8);
			rgb[i * 3 + 2] = (uint8_t)pixel;
		}

		bool saved = Util::SavePng(args[1].c_str(), rgb.data(), GBA::ScreenWidth, GBA::ScreenHeight);

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Object;
		output->AddUtf8String("file", args[1].c_str());
		// The device's name as the settings dialog shows it, so a script that dumped a picture knows
		// which of the two machines it came from.
		output->AddUtf8String("device", deviceName.c_str());
		output->AddInt("width", GBA::ScreenWidth);
		output->AddInt("height", GBA::ScreenHeight);
		output->AddInt("frames", gba->FrameCounter());
		output->AddBool("saved", saved);
		return output;
	}

}
