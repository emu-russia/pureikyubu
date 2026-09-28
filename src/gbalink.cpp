/*

# The GBA link cable (DOL-011)

The implementation of the device `gbalink.h` describes: the Game Boy Advance it carries, the
settings that put a Game Pak in it, and the console's side of the Joybus command set.

## The console's side

The SI channel of the socket the cable is in hands this device the command bytes of a communication
transfer and takes the answer back. The automatic poll never reaches it: a Game Boy Advance is not a
game pad, and a channel whose device does not answer a pad poll latches no-response - which is what
the hardware does as well, and how a guest that probes the socket finds out what is in it.

The four commands are the ones the console library uses, and each one is a read or a write of one of
the Game Boy Advance's own JOY registers - the console does not tell the GBA what to do, it *becomes*
the other end of its link port. That is why a Game Pak that speaks JOY bus mode needs no emulation of
its own here: it is the same code that would run on hardware.

## The Game Boy Advance in the device

The machine runs only when it has something to do: it is advanced by the emulated time that has
passed since the console last spoke to it (`RunToNow`), and only when the "Run Game Pak" switch is
on and a cartridge is loaded. A Game Pak that is inserted but not running costs nothing, which is
what a user who left a cartridge in the device and went back to a GameCube game wants.

The device is also usable with **no port at all** - that is the "Game Boy Advance as a separate
device" of the settings: it keeps its cartridge and its machine and simply is not on a socket.

*/

#include "pch.h"
#include "gbalink.h"
#include "gba/gba.h"

using namespace Debug;

//! The actuators of the Game Boy Advance. A Game Pak is driven the same way wherever it is played:
//! eight directions, four buttons and the two shoulder buttons, all of them plain buttons.
static const PeriphActuator gbl_actuators[GBL_ACT_MAX] =
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
		GBL_PROP_GAMEPAK = 0,       //!< the cartridge image in the slot
		GBL_PROP_BIOS,              //!< the BIOS of the machine
		GBL_PROP_RUN,               //!< run the Game Pak
		GBL_PROP_STATE,             //!< what is in the slot, read only

		GBL_PROP_MAX
	};

	const PeriphProperty gbl_properties[GBL_PROP_MAX] =
	{
		{ "Game Pak",     PERIPH_PROP_ROM,  GBL_PROP_GAMEPAK },
		{ "BIOS",         PERIPH_PROP_ROM,  GBL_PROP_BIOS },
		{ "Run Game Pak", PERIPH_PROP_BOOL, GBL_PROP_RUN },
		{ "GBA",          PERIPH_PROP_INFO, GBL_PROP_STATE },
	};
}

// ---------------------------------------------------------------------------
// The device

class GbaLinkDevice : public PeripheralDevice
{

	int             port = -1;
	int             state[GBL_ACT_MAX] = { 0 };
	PeriphBindings  bindings[GBL_ACT_MAX];

	std::wstring    gamePak;
	std::wstring    bios;
	bool            run = true;

	std::unique_ptr<GBA::GbaSystem> gba;

	//! Whether the console has driven the port as a Game Boy Advance link (see Transfer): the
	//! port answers the pad protocol either way, and this is what tells a user whether the title
	//! ever took the connection further.
	bool            driven = false;

	//! The emulated time the machine has been run up to (0: it has not started yet).
	int64_t         ranTo = 0;

	//! How many frames the Game Pak has produced (the info line shows it, and it is how a user can
	//! see that the machine in the device is alive).
	uint32_t        frames = 0;

	//! The keys the host is holding, as the KEY_* bits of the machine (see gba_keypad.h).
	uint16_t KeyMask() const
	{
		uint16_t mask = 0;

		if (state[GBL_ACT_A])      mask |= GBA::KEY_A;
		if (state[GBL_ACT_B])      mask |= GBA::KEY_B;
		if (state[GBL_ACT_SELECT]) mask |= GBA::KEY_SELECT;
		if (state[GBL_ACT_START])  mask |= GBA::KEY_START;
		if (state[GBL_ACT_RIGHT])  mask |= GBA::KEY_RIGHT;
		if (state[GBL_ACT_LEFT])   mask |= GBA::KEY_LEFT;
		if (state[GBL_ACT_UP])     mask |= GBA::KEY_UP;
		if (state[GBL_ACT_DOWN])   mask |= GBA::KEY_DOWN;
		if (state[GBL_ACT_R])      mask |= GBA::KEY_R;
		if (state[GBL_ACT_L])      mask |= GBA::KEY_L;

		return mask;
	}

	//! Whether the machine inside the device is worth running. A Game Pak in the bay is, and so is
	//! a console that has driven the port as a link: a title that multi-boots its partner (the
	//! Tingle Tuner, WarioWare's GBA modes, the Four Swords) does it with an *empty* bay, and the
	//! program it streams goes into the Game Boy Advance's own RAM - the BIOS is what receives it,
	//! so a machine that only ran a cartridge could never take part. `driven` is what keeps the
	//! other half of the bargain: a device that the console only polls as a pad, with nothing
	//! inserted, still costs nothing.
	bool powered() const { return run && gba != nullptr && (gba->RomLoaded() || driven); }

	void MarkDriven(uint8_t cmd)
	{
		if (!driven)
		{
			driven = true;
			Report(Channel::SI, "GBA Link: the console drives the port as a link (command %02X)\n", cmd);
		}
	}

	//! Advance the Game Boy Advance by the emulated time that has passed since the last time it was
	//! run. One frame of the machine is 280896 cycles of its 16777216 Hz clock, and the console's
	//! time base counts the same seconds, so the number of frames due is a division. A time base
	//! whose second is shorter than a frame (the unit tests drive one that way) still gets one frame
	//! per unit rather than none at all.
	void RunToNow()
	{
		if (!powered())
		{
			return;
		}

		int64_t period = Core->OneSecond() * 280896 / 16777216;

		if (period <= 0)
		{
			period = 1;
		}

		int64_t now = Core->GetTicks();

		if (ranTo == 0)
		{
			ranTo = now;
		}

		// A console that has been away for a long time (a load, a long stall) must not be answered
		// with a burst of a hundred frames, so the catch-up is bounded and the rest is dropped.
		int budget = 4;

		while (now - ranTo >= period && budget-- > 0)
		{
			gba->RunFrame();
			frames++;
			ranTo += period;
		}

		if (now - ranTo >= period)
		{
			ranTo = now;
		}
	}

	//! The status byte every command answers with. It is the Game Boy Advance's own JOYSTAT, which
	//! is what the console library masks with 0x3A.
	uint8_t JoyStatus()
	{
		RunToNow();

		if (gba == nullptr)
		{
			return 0;
		}

		return (uint8_t)(gba->Link().JoyStatus() & 0x3A);
	}

public:
	GbaLinkDevice()
	{
		for (int i = 0; i < GBL_ACT_MAX; i++)
		{
			bindings[i].keyboard = 0;
			bindings[i].gamepad = 0;
		}

		gba = std::make_unique<GBA::GbaSystem>();
	}

	uint32_t Type() override { return PERIPH_DEVICE_GBA_LINK; }

	// -----------------------------------------------------------------------
	// Actuators

	int ActuatorCount() override { return GBL_ACT_MAX; }

	const PeriphActuator* Actuator(int index) override
	{
		return (index >= 0 && index < GBL_ACT_MAX) ? &gbl_actuators[index] : nullptr;
	}

	PeriphBindings* ActuatorBindings(int index) override
	{
		return (index >= 0 && index < GBL_ACT_MAX) ? &bindings[index] : nullptr;
	}

	void SetState(int actuator, int value) override
	{
		if (actuator >= 0 && actuator < GBL_ACT_MAX)
		{
			state[actuator] = value != 0 ? 1 : 0;
		}

		if (gba != nullptr)
		{
			gba->SetPressedKeys(KeyMask());
		}
	}

	// -----------------------------------------------------------------------
	// Properties

	int PropertyCount() override { return GBL_PROP_MAX; }

	const PeriphProperty* Property(int index) override
	{
		return (index >= 0 && index < GBL_PROP_MAX) ? &gbl_properties[index] : nullptr;
	}

	std::string GetProperty(int id) override
	{
		switch (id)
		{
			case GBL_PROP_GAMEPAK:
				return Util::WstringToString(gamePak);

			case GBL_PROP_BIOS:
				return Util::WstringToString(bios);

			case GBL_PROP_RUN:
				return run ? "1" : "0";

			case GBL_PROP_STATE:
			{
				char text[0x100];

				if (gba != nullptr && gba->RomLoaded())
				{
					snprintf(text, sizeof(text), "%s (%s), %u frame(s)%s",
						gba->RomTitle().c_str(), gba->RomGameCode().c_str(), frames,
						port < 0 ? " - not on a cable" : "");

					return text;
				}

				if (driven)
				{
					// An empty bay the console is driving is the state a multi-boot partner is in:
					// the machine is running its BIOS, waiting for the program (see `powered`).
					snprintf(text, sizeof(text),
						"No Game Pak - the console drives the link (%u frame(s))", frames);

					return text;
				}

				return "No Game Pak";
			}
		}

		return "";
	}

	void SetProperty(int id, const std::string& value) override
	{
		switch (id)
		{
			case GBL_PROP_GAMEPAK:
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

			case GBL_PROP_BIOS:
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

			case GBL_PROP_RUN:
			{
				run = value == "1" || value == "true";
				SetConfigEntryInt(config, "Run", run ? 1 : 0);
				break;
			}
		}
	}

	// -----------------------------------------------------------------------

	void InsertGamePak()
	{
		if (gba == nullptr)
		{
			return;
		}

		if (gamePak.empty() || !Util::FileExists(gamePak))
		{
			gba->EjectRom();

			if (!gamePak.empty())
			{
				Report(Channel::SI, "GBA Link: cannot find the Game Pak %s\n",
					Util::WstringToString(gamePak).c_str());
			}

			return;
		}

		std::string error;

		if (!gba->LoadRomFile(Util::WstringToString(gamePak), error))
		{
			Report(Channel::SI, "GBA Link: %s\n", error.c_str());
			gba->EjectRom();
			return;
		}

		Report(Channel::SI, "GBA Link: inserted %s (%s)\n",
			gba->RomTitle().c_str(), gba->RomGameCode().c_str());

		gba->Reset();
		gba->SetPressedKeys(KeyMask());

		ranTo = 0;
		frames = 0;
	}

	void InsertBios()
	{
		if (gba == nullptr)
		{
			return;
		}

		std::string error;

		if (!gba->LoadBiosFile(Util::WstringToString(bios), error))
		{
			Report(Channel::SI, "GBA Link: %s\n", error.c_str());
		}
	}

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

		// The cable is in a socket now, so the Game Boy Advance's link port has a console on it:
		// a write of JOY_TRANS waits for the console to ask for the value instead of completing
		// on its own (see gba_sio.cpp). The machine runs whether or not it is on a socket - that
		// is the difference between "a GBA and its link cable" and "a GBA".
		if (gba != nullptr)
		{
			gba->Link().AttachConsole(true);
		}

		Report(Channel::SI, "GBA Link: attached to %s\n",
			Peripherals::Instance().PortName(port));
	}

	void Detach() override
	{
		if (gba != nullptr)
		{
			gba->Link().AttachConsole(false);
		}

		port = -1;
	}

	// -----------------------------------------------------------------------
	// The buses

	//! The automatic poll of the channel. A Game Boy Advance is not a game pad: it does not answer
	//! the pad's own poll, so the channel latches no-response and the guest that probes the socket
	//! sees a device that is not a controller. The console library still runs the GBA, it just
	//! never asks it that question.
	bool Poll(PADState* state) override
	{
		return false;
	}

	//! A communication transfer of the channel: the console's Joybus command set (gba-link-cable.md 3).
	void Transfer(int outlen, int inlen, uint8_t* buf) override
	{
		uint8_t cmd = buf[0];

		if (gba == nullptr)
		{
			return;
		}

		switch (cmd)
		{
			// JOY-STATUS: the two type bytes and the status. The console library masks the status
			// with 0x3A, so the two type bytes are the library's business and are zero here.
			case 0x00:
			{
				if (inlen >= 3)
				{
					buf[0] = 0;
					buf[1] = 0;
					buf[2] = JoyStatus();
				}
				break;
			}

			// JOY-READ: the 32-bit word the Game Pak put in JOY_TRANS, then the status. The word
			// goes out most significant byte first, which is what the console library assembles.
			case 0x14:
			{
				MarkDriven(cmd);

				RunToNow();

				uint32_t value = gba->Link().JoyTransmit();

				if (inlen >= 5)
				{
					buf[0] = (uint8_t)(value >> 24);
					buf[1] = (uint8_t)(value >> 16);
					buf[2] = (uint8_t)(value >> 8);
					buf[3] = (uint8_t)value;
				}

				// The console has taken the value: the Game Pak is told that the send completed.
				gba->Link().JoyConsoleRead(gba->Bus());

				if (inlen >= 5)
				{
					buf[4] = JoyStatus();
				}
				break;
			}

			// JOY-WRITE: four data bytes into the Game Pak's JOY_RECV, then the status.
			case 0x15:
			{
				MarkDriven(cmd);

				if (outlen >= 5)
				{
					uint32_t value = ((uint32_t)buf[1] << 24) | ((uint32_t)buf[2] << 16) |
						((uint32_t)buf[3] << 8) | (uint32_t)buf[4];

					gba->Link().JoyConsoleWrite(gba->Bus(), value);
					RunToNow();
				}

				if (inlen >= 1)
				{
					buf[0] = JoyStatus();
				}
				break;
			}

			// JOY-RESET: the port's own state goes back, and the Game Pak is told. Repeated resets
			// are how the console asks for a JOY Reboot.
			case 0xFF:
			{
				MarkDriven(cmd);
				gba->Link().JoyConsoleReset(gba->Bus());
				gba->SetPressedKeys(KeyMask());
				RunToNow();

				if (inlen >= 3)
				{
					buf[0] = 0;
					buf[1] = 0;
					buf[2] = JoyStatus();
				}
				break;
			}

			default:
			{
				Report(Channel::SI, "GBA Link: unknown command %02X (out %i, in %i)\n",
					cmd, outlen, inlen);
				break;
			}
		}

	}

	// -----------------------------------------------------------------------
	// What the front end can show

	//! The machine behind the device, for a front end that presents its picture and its sound (and
	//! for a test that drives the two sides against each other).
	GBA::GbaSystem* Gba() override { return gba.get(); }

	GBA::GbaSystem* System() { return gba.get(); }
};

// ---------------------------------------------------------------------------
// The registration

namespace
{
	PeripheralDevice* CreateGbaLink()
	{
		return new GbaLinkDevice();
	}

	//! The device registers its own factory with the peripheral subsystem. A build that does not
	//! compile this file (or the GBA core it drives) has no GBA link cable.
	struct GbaLinkRegistrar
	{
		GbaLinkRegistrar()
		{
			Peripherals::RegisterFactory(PERIPH_DEVICE_GBA_LINK, "GBA + Link Cable",
				"DOL-011, a Game Boy Advance on a controller socket", PERIPH_BUS_SI, CreateGbaLink);
		}
	};

	GbaLinkRegistrar gbalink_registrar;
}
