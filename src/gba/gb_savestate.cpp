// Save states of the Game Boy and Game Boy Color (the machine side of the format described in
// gba_savestate.h; the Game Boy Advance has its own pair of functions in gba_savestate.cpp).
//
// The two machines share one image format and one pair of cursors, and the first field of the
// state's first section is the machine the state belongs to. That is what keeps a Game Boy state
// out of a Game Boy Advance and the other way round, and it is why the error a user sees names the
// machine instead of reporting a broken section.
//
// What is in a Game Boy state:
//
//   META   the machine kind, the console kind (DMG or CGB), the cartridge the state was taken
//          from (its title, its type byte and its ROM size) and the boot bookkeeping
//   CPU    the register file, the interrupt master enable, HALT/STOP and the double speed flag
//   BUS    the eight WRAM banks, HRAM, the interrupts, the timer, the joypad, the serial port,
//          OAM DMA, the CGB's own registers (KEY1, VBK, SVBK, OPRI) and the HDMA/GDMA registers
//   PPU    the LCD registers, both VRAM banks, OAM, the CGB's two palette banks, where the LCD
//          is inside the line it is drawing and the picture that is on the screen
//   APU    the four channels, the wave RAM, the frame sequencer and the two high pass capacitors
//   CART   the battery backed RAM, the mapper's registers and the real time clock
//
// Not in it, on purpose: the ROM image and the boot ROM image (the frontend loaded them and the
// settings choose them), the `.sav` path, and the link cable's peer, which is another machine.

#include "gba_savestate.h"
#include "gb.h"

namespace GBA
{
	static const char* const TagMeta = "META";
	static const char* const TagCpu = "CPU ";
	static const char* const TagBus = "BUS ";
	static const char* const TagPpu = "PPU ";
	static const char* const TagApu = "APU ";
	static const char* const TagCart = "CART";

	bool GbSystem::SaveState(std::vector<uint8_t>& image, std::string* error) const
	{
		StateWriter writer;

		// -- the machine and the cartridge the state belongs to --------------------------------
		writer.Begin(TagMeta);
		writer.Fields(StateMachine::GameBoy);
		writer.U8(settings.cgb ? 1 : 0);
		writer.Text(bus->cart.Header().title);
		writer.U8(bus->cart.Header().cartType);
		writer.U8(bus->cart.Header().romSizeCode);
		writer.U64((uint64_t)bus->cart.Rom().size());
		writer.Fields(inBootRom, headerChecksum, bootRegisterA);
		writer.End();

		writer.Begin(TagCpu);
		bus->cpu.SaveState(writer);
		writer.End();

		writer.Begin(TagBus);
		bus->SaveState(writer);
		writer.End();

		writer.Begin(TagPpu);
		bus->ppu.SaveState(writer);
		writer.End();

		writer.Begin(TagApu);
		bus->apu.SaveState(writer);
		writer.End();

		writer.Begin(TagCart);
		bus->cart.SaveState(writer);
		writer.End();

		StateWrap(writer.Image(), image);

		if (error != nullptr)
		{
			error->clear();
		}

		return true;
	}

	bool GbSystem::LoadState(const uint8_t* image, size_t size, std::string* error)
	{
		std::string message;
		const uint8_t* payload = nullptr;
		size_t payloadSize = 0;

		if (!StateUnwrap(image, size, &payload, &payloadSize, message))
		{
			if (error != nullptr)
			{
				*error = message;
			}
			return false;
		}

		// Everything that can be checked without touching the machine is checked first: the image
		// header (the magic, the version and the checksum), the machine it belongs to, the console
		// kind and the cartridge. Only then do the sections reach the devices.
		StateReader reader(payload, payloadSize);
		char tag[5];

		if (!reader.PeekTag(tag) || strcmp(tag, TagMeta) != 0)
		{
			if (error != nullptr)
			{
				*error = "the save state has no machine section";
			}
			return false;
		}

		reader.Begin(TagMeta);

		// The machine the state belongs to is the first field of the section, and it is checked
		// before anything else is read out of it: the two machines' sections do not have the same
		// layout, so a Game Boy Advance state read as if it were a Game Boy one would fail on
		// nonsense long before the names could be compared.
		StateMachine machine = StateMachine::GameBoy;
		reader.Fields(machine);

		if (reader.Failed())
		{
			if (error != nullptr)
			{
				*error = reader.Error();
			}
			return false;
		}

		if (machine != StateMachine::GameBoy)
		{
			if (error != nullptr)
			{
				*error = std::string("the save state is a ") + StateMachineName(machine) +
					" state, and this machine is a Game Boy";
			}
			return false;
		}

		uint8_t cgb = 0;
		std::string title;
		uint8_t cartType = 0, romSizeCode = 0;
		uint64_t romBytes = 0;
		bool bootRom = false;
		uint8_t checksum = 0, registerA = 0;
		reader.Fields(cgb);
		reader.Text(title);
		reader.Fields(cartType, romSizeCode, romBytes);
		reader.Fields(bootRom, checksum, registerA);
		reader.End();

		if (reader.Failed())
		{
			if (error != nullptr)
			{
				*error = reader.Error();
			}
			return false;
		}

		// The console kind is part of the machine: a state taken on a CGB carries the colour
		// palettes, the two VRAM banks and (possibly) the double speed mode, and a state taken on
		// a DMG has none of them. Loading one into the other is refused rather than half applied.
		if ((cgb != 0) != settings.cgb)
		{
			if (error != nullptr)
			{
				*error = std::string("the save state was taken on a ") +
					(cgb != 0 ? "Game Boy Color" : "Game Boy (DMG)") + " and this machine is a " +
					(settings.cgb ? "Game Boy Color" : "Game Boy (DMG)");
			}
			return false;
		}

		// The same rule as the Game Boy Advance's: a state belongs to the cartridge it was taken
		// from (a machine with no cartridge is a kind of its own, and its fields are the empty
		// ones). The console kind was checked above.
		bool sameCartridge = (title == bus->cart.Header().title) &&
			(cartType == bus->cart.Header().cartType) &&
			(romBytes == (uint64_t)bus->cart.Rom().size());

		if (!sameCartridge)
		{
			if (error != nullptr)
			{
				if (!bus->cart.IsLoaded())
				{
					*error = "the save state belongs to \"" + title + "\" and this machine has no cartridge";
				}
				else if (title.empty())
				{
					*error = "the save state was taken with no cartridge and this machine is running \"" +
						bus->cart.Header().title + "\"";
				}
				else
				{
					*error = "the save state belongs to \"" + title + "\" and this machine is running \"" +
						bus->cart.Header().title + "\"";
				}
			}
			return false;
		}

		// -- the sections ------------------------------------------------------------------------

		while (!reader.Failed() && reader.Cursor() < reader.Size())
		{
			if (!reader.PeekTag(tag))
			{
				break;
			}

			if (strcmp(tag, TagCpu) == 0)
			{
				reader.Begin(TagCpu);
				bus->cpu.LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagBus) == 0)
			{
				reader.Begin(TagBus);
				bus->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagPpu) == 0)
			{
				reader.Begin(TagPpu);
				bus->ppu.LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagApu) == 0)
			{
				reader.Begin(TagApu);
				bus->apu.LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagCart) == 0)
			{
				reader.Begin(TagCart);
				bus->cart.LoadState(reader);
				reader.End();
			}
			else
			{
				reader.Skip();
			}
		}

		if (reader.Failed())
		{
			if (error != nullptr)
			{
				*error = reader.Error();
			}
			return false;
		}

		if (reader.Cursor() != reader.Size())
		{
			if (error != nullptr)
			{
				*error = "the save state has bytes after its last section";
			}
			return false;
		}

		// The machine's own bookkeeping, which a state does not carry as device state. The link
		// cable is not among these: a peer is another machine (or another process) and the state
		// of the cable belongs to the frontend that plugged it in.
		inBootRom = bootRom;
		headerChecksum = checksum;
		bootRegisterA = registerA;

		if (error != nullptr)
		{
			error->clear();
		}

		return true;
	}

	bool GbSystem::LoadState(const std::vector<uint8_t>& image, std::string* error)
	{
		return LoadState(image.empty() ? nullptr : image.data(), image.size(), error);
	}

	bool GbSystem::SaveStateFile(const std::string& path, std::string* error) const
	{
		std::vector<uint8_t> image;

		if (!SaveState(image, error))
		{
			return false;
		}

		FILE* f = fopen(path.c_str(), "wb");

		if (f == nullptr)
		{
			if (error != nullptr)
			{
				*error = "cannot write " + path;
			}
			return false;
		}

		size_t written = image.empty() ? 0 : fwrite(image.data(), 1, image.size(), f);
		fclose(f);

		if (written != image.size())
		{
			if (error != nullptr)
			{
				*error = "cannot write " + path;
			}
			return false;
		}

		return true;
	}

	bool GbSystem::LoadStateFile(const std::string& path, std::string* error)
	{
		FILE* f = fopen(path.c_str(), "rb");

		if (f == nullptr)
		{
			if (error != nullptr)
			{
				*error = "no save state in " + path;
			}
			return false;
		}

		std::vector<uint8_t> image;
		uint8_t chunk[64 * 1024];

		for (;;)
		{
			size_t got = fread(chunk, 1, sizeof(chunk), f);

			if (got == 0)
			{
				break;
			}

			image.insert(image.end(), chunk, chunk + got);
		}

		fclose(f);

		return LoadState(image, error);
	}

	std::string GbSystem::StateFilePath(int slot) const
	{
		if (slot < 0)
		{
			slot = 0;
		}
		if (slot > MaxStateSlot)
		{
			slot = MaxStateSlot;
		}

		// Next to the battery save, named after the ROM without its extension: `Zelda.gbc` gives
		// `Zelda.st0`, exactly as the Game Boy Advance names the states of its cartridges (a
		// `.gb` and a `.gba` of the same name then write the same file, which the machine field of
		// the state turns into "this is a Game Boy state" rather than into a wrong machine).
		//
		// The Game Boy's own `.sav` is always written next to the ROM (GbCart::LoadRomFile), so
		// its states are as well and there is no save directory to honour.
		std::string base;

		if (!bus->cart.SaveFilePath().empty())
		{
			base = bus->cart.SaveFilePath();
			size_t dot = base.find_last_of('.');
			if (dot != std::string::npos && base.find_last_of("/\\") < dot)
			{
				base = base.substr(0, dot);
			}
		}
		else if (!romPath.empty())
		{
			base = romPath;
			size_t dot = base.find_last_of('.');
			if (dot != std::string::npos && base.find_last_of("/\\") < dot)
			{
				base = base.substr(0, dot);
			}
		}
		else if (!bus->cart.Header().title.empty())
		{
			// A cartridge loaded from an image has no file to name the state after, so the ROM's
			// own title is used. The characters a file name cannot hold are replaced.
			base = bus->cart.Header().title;

			for (char& c : base)
			{
				if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
					c == '<' || c == '>' || c == '|')
				{
					c = '_';
				}
			}
		}
		else
		{
			base = "gb_state";
		}

		char suffix[16];
		sprintf(suffix, ".st%i", slot);
		return base + suffix;
	}
}
