// Save states of the integrated GBA emulator: the two cursors and the image header (the format is
// described in gba_savestate.h).
//
// Nothing here knows about a device: the cursors are a byte image with a checksum and a run of
// sections, and the machine's own SaveState/LoadState members (one pair per device, and the pair
// on GbaSystem that ties them together, at the end of this file) are what fills a section.
//
// Byte order is little endian and explicit: the fields go out least significant byte first instead
// of as a memcpy of the host's own layout, so a state written on one machine is read on another and
// a compiler that pads a struct differently cannot change the format.

#include "gba_savestate.h"
#include "gba.h"
#include "gba_hlebios.h"

namespace GBA
{
	const uint8_t StateMagic[8] = { 'P', 'S', 'A', 'V', 'E', 'S', 'T', 0 };

	const char* StateMachineName(StateMachine machine)
	{
		switch (machine)
		{
		case StateMachine::Gba: return "Game Boy Advance";
		case StateMachine::GameBoy: return "Game Boy";
		default: return "unknown machine";
		}
	}

	// ---------------------------------------------------------------------------------------
	// Writing
	// ---------------------------------------------------------------------------------------

	void StateWriter::Unsigned(uint64_t value, size_t width)
	{
		for (size_t i = 0; i < width; i++)
		{
			image.push_back((uint8_t)(value >> (i * 8)));
		}
	}

	StateWriter& StateWriter::U8(uint8_t value) { Unsigned(value, 1); return *this; }
	StateWriter& StateWriter::U16(uint16_t value) { Unsigned(value, 2); return *this; }
	StateWriter& StateWriter::U32(uint32_t value) { Unsigned(value, 4); return *this; }
	StateWriter& StateWriter::U64(uint64_t value) { Unsigned(value, 8); return *this; }

	void StateWriter::Begin(const char* tag)
	{
		for (int i = 0; i < 4; i++)
		{
			image.push_back((uint8_t)(tag != nullptr ? tag[i] : '?'));
		}

		lengthOffset = image.size();
		sectionStart = image.size() + 4;

		for (int i = 0; i < 4; i++)
		{
			image.push_back(0);
		}
	}

	void StateWriter::End()
	{
		uint32_t length = (uint32_t)(image.size() - sectionStart);

		for (int i = 0; i < 4; i++)
		{
			image[lengthOffset + i] = (uint8_t)(length >> (i * 8));
		}
	}

	StateWriter& StateWriter::Raw(const void* data, size_t size)
	{
		if (data != nullptr && size != 0)
		{
			const uint8_t* bytes = (const uint8_t*)data;
			image.insert(image.end(), bytes, bytes + size);
		}

		return *this;
	}

	StateWriter& StateWriter::Bytes(const std::vector<uint8_t>& values)
	{
		U32((uint32_t)values.size());
		return Raw(values.empty() ? nullptr : values.data(), values.size());
	}

	StateWriter& StateWriter::Text(const std::string& text)
	{
		U32((uint32_t)text.size());
		return Raw(text.empty() ? nullptr : text.data(), text.size());
	}

	// ---------------------------------------------------------------------------------------
	// Reading
	// ---------------------------------------------------------------------------------------

	void StateReader::Fail(const std::string& reason)
	{
		if (!failed)
		{
			failed = true;
			error = reason;
		}
	}

	uint64_t StateReader::Unsigned(size_t width)
	{
		if (failed)
		{
			return 0;
		}

		if (cursor + width > size)
		{
			Fail("the image ends in the middle of a value");
			return 0;
		}

		uint64_t value = 0;

		for (size_t i = 0; i < width; i++)
		{
			value |= (uint64_t)data[cursor + i] << (i * 8);
		}

		cursor += width;
		return value;
	}

	uint8_t StateReader::U8() { return (uint8_t)Unsigned(1); }
	uint16_t StateReader::U16() { return (uint16_t)Unsigned(2); }
	uint32_t StateReader::U32() { return (uint32_t)Unsigned(4); }
	uint64_t StateReader::U64() { return Unsigned(8); }

	StateReader& StateReader::Raw(void* data, size_t size)
	{
		if (failed)
		{
			return *this;
		}

		if (cursor + size > this->size)
		{
			Fail("the image ends in the middle of a memory block");
			return *this;
		}

		if (data != nullptr && size != 0)
		{
			memcpy(data, this->data + cursor, size);
		}

		cursor += size;
		return *this;
	}

	StateReader& StateReader::Bytes(std::vector<uint8_t>& values)
	{
		uint32_t count = U32();

		if (failed)
		{
			return *this;
		}

		if (count > (64u << 20))
		{
			Fail("a byte block is larger than any state this module writes");
			return *this;
		}

		values.resize(count);
		return Raw(values.empty() ? nullptr : values.data(), values.size());
	}

	StateReader& StateReader::Text(std::string& text)
	{
		uint32_t count = U32();

		if (failed)
		{
			return *this;
		}

		if (count > (1u << 20) || cursor + count > size)
		{
			Fail("a string runs past the end of the image");
			return *this;
		}

		text.assign((const char*)data + cursor, count);
		cursor += count;
		return *this;
	}

	bool StateReader::PeekTag(char tag[5])
	{
		if (failed)
		{
			return false;
		}

		if (cursor + 8 > size)
		{
			Fail("the image ends before a section header");
			return false;
		}

		for (int i = 0; i < 4; i++)
		{
			tag[i] = (char)data[cursor + i];
		}

		tag[4] = 0;
		return true;
	}

	bool StateReader::Begin(const char* tag)
	{
		if (failed)
		{
			return false;
		}

		if (cursor + 8 > size)
		{
			Fail("the image ends before a section header");
			return false;
		}

		for (int i = 0; i < 4; i++)
		{
			if (data[cursor + i] != (uint8_t)tag[i])
			{
				char name[5] = { (char)data[cursor], (char)data[cursor + 1], (char)data[cursor + 2],
					(char)data[cursor + 3], 0 };
				Fail(std::string("the image has the section \"") + name + "\" where \"" + tag + "\" belongs");
				return false;
			}
		}

		cursor += 4;
		uint32_t length = U32();

		if (failed)
		{
			return false;
		}

		sectionEnd = cursor + length;

		if (sectionEnd > size)
		{
			Fail(std::string("the section \"") + tag + "\" runs past the end of the image");
			return false;
		}

		return true;
	}

	bool StateReader::End()
	{
		if (failed)
		{
			return false;
		}

		if (cursor != sectionEnd)
		{
			Fail(cursor < sectionEnd ?
				"a section has more bytes than this build reads (a state from another version?)" :
				"a section was read past its own end");
			return false;
		}

		return true;
	}

	bool StateReader::Skip()
	{
		if (failed)
		{
			return false;
		}

		if (cursor + 8 > size)
		{
			Fail("the image ends before a section header");
			return false;
		}

		cursor += 4;
		uint32_t length = U32();

		if (failed)
		{
			return false;
		}

		if (cursor + length > size)
		{
			Fail("a section runs past the end of the image");
			return false;
		}

		cursor += length;
		return true;
	}

	// ---------------------------------------------------------------------------------------
	// The image
	// ---------------------------------------------------------------------------------------

	uint64_t StateChecksum(const uint8_t* data, size_t size)
	{
		// FNV-1a 64: the offset basis and prime of the algorithm.
		uint64_t hash = 1469598103934665603ull;

		for (size_t i = 0; i < size; i++)
		{
			hash ^= data[i];
			hash *= 1099511628211ull;
		}

		return hash;
	}

	static void PutU32(std::vector<uint8_t>& out, uint32_t value)
	{
		for (int i = 0; i < 4; i++)
		{
			out.push_back((uint8_t)(value >> (i * 8)));
		}
	}

	static void PutU64(std::vector<uint8_t>& out, uint64_t value)
	{
		for (int i = 0; i < 8; i++)
		{
			out.push_back((uint8_t)(value >> (i * 8)));
		}
	}

	static uint32_t GetU32(const uint8_t* data)
	{
		return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
			((uint32_t)data[3] << 24);
	}

	static uint64_t GetU64(const uint8_t* data)
	{
		uint64_t value = 0;

		for (int i = 0; i < 8; i++)
		{
			value |= (uint64_t)data[i] << (i * 8);
		}

		return value;
	}

	void StateWrap(const std::vector<uint8_t>& payload, std::vector<uint8_t>& out)
	{
		out.clear();
		out.reserve(StateHeaderSize + payload.size());

		for (int i = 0; i < 8; i++)
		{
			out.push_back(StateMagic[i]);
		}

		PutU32(out, StateFormatVersion);
		PutU32(out, 0);
		PutU64(out, (uint64_t)payload.size());
		PutU64(out, StateChecksum(payload.empty() ? nullptr : payload.data(), payload.size()));
		out.insert(out.end(), payload.begin(), payload.end());
	}

	bool StateUnwrap(const uint8_t* image, size_t size, const uint8_t** payload, size_t* payloadSize,
		std::string& error)
	{
		if (image == nullptr || size < StateHeaderSize)
		{
			error = "the file is too short to be a save state";
			return false;
		}

		for (int i = 0; i < 8; i++)
		{
			if (image[i] != StateMagic[i])
			{
				error = "the file is not a GBA save state";
				return false;
			}
		}

		uint32_t version = GetU32(image + 0x08);

		if (version != StateFormatVersion)
		{
			char text[128];
			sprintf(text, "the save state is format version %u and this build reads %u",
				(unsigned)version, (unsigned)StateFormatVersion);
			error = text;
			return false;
		}

		uint64_t length = GetU64(image + 0x10);

		if (length != (uint64_t)(size - StateHeaderSize))
		{
			error = "the save state says it is a different size than it is";
			return false;
		}

		uint64_t checksum = GetU64(image + 0x18);

		if (checksum != StateChecksum(image + StateHeaderSize, (size_t)length))
		{
			error = "the save state is corrupt (its checksum does not match)";
			return false;
		}

		*payload = image + StateHeaderSize;
		*payloadSize = (size_t)length;
		return true;
	}

	// =======================================================================================
	// GbaSystem - the whole machine
	// =======================================================================================
	//
	// The sections, in the order they are written. A section per subsystem keeps a state readable
	// when something is wrong with it: the reader names the section whose layout did not fit, and
	// the meta section names the cartridge, so a state taken from another game is refused with
	// that name instead of loading a machine whose code does not match its memory.

	static const char* const TagMeta = "META";
	static const char* const TagCpu = "CPU ";
	static const char* const TagBus = "BUS ";
	static const char* const TagPpu = "PPU ";
	static const char* const TagApu = "APU ";
	static const char* const TagSio = "SIO ";
	static const char* const TagDma = "DMA ";
	static const char* const TagTimers = "TMR ";
	static const char* const TagIrq = "IRQ ";
	static const char* const TagKeypad = "KPD ";
	static const char* const TagCart = "CART";
	static const char* const TagHle = "HLE ";

	bool GbaSystem::SaveState(std::vector<uint8_t>& image, std::string* error) const
	{
		StateWriter writer;

		// -- the identity of the machine the state belongs to ---------------------------------
		writer.Begin(TagMeta);
		writer.Fields(StateMachine::Gba);
		writer.Text(bus->cart.Title());
		writer.Text(bus->cart.GameCode());
		writer.U64((uint64_t)bus->cart.RomSize());
		writer.U8(linkMode ? 1 : 0);
		writer.U8(bus->UsingCustomBios() ? 1 : 0);
		writer.U8(bus->HleBiosEnabled ? 1 : 0);
		writer.U8((uint8_t)bus->cart.GetSaveType());
		writer.End();

		// -- the CPU ---------------------------------------------------------------------------
		writer.Begin(TagCpu);
		bus->cpu.SaveState(writer);
		writer.End();

		// -- the bus: its registers, the waitstates and the memory -----------------------------
		writer.Begin(TagBus);
		bus->SaveState(writer);
		writer.End();

		// -- the devices ------------------------------------------------------------------------
		writer.Begin(TagPpu);
		bus->ppu.SaveState(writer);
		writer.End();

		writer.Begin(TagApu);
		bus->apu.SaveState(writer);
		writer.End();

		writer.Begin(TagSio);
		bus->sio.SaveState(writer);
		writer.End();

		writer.Begin(TagDma);
		bus->dma.SaveState(writer);
		writer.End();

		writer.Begin(TagTimers);
		bus->timers.SaveState(writer);
		writer.End();

		writer.Begin(TagIrq);
		bus->irq.SaveState(writer);
		writer.End();

		writer.Begin(TagKeypad);
		bus->keypad.SaveState(writer);
		writer.End();

		writer.Begin(TagCart);
		bus->cart.SaveState(writer);
		writer.End();

		// The host-side BIOS calls keep a little state of their own (the IntrWait the machine is
		// sitting in, the sound driver's work area); without it a state loaded during a wait would
		// never wake up.
		writer.Begin(TagHle);
		HleBios::SaveState(writer);
		writer.End();

		StateWrap(writer.Image(), image);

		if (error != nullptr)
		{
			error->clear();
		}

		return true;
	}

	bool GbaSystem::LoadState(const uint8_t* image, size_t size, std::string* error)
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

		// Everything that can be checked without touching the machine is checked before a single
		// byte of it is applied: the header (in StateUnwrap: the magic, the format version and the
		// checksum of the payload) and then the machine section, which names the cartridge this
		// state was taken from. Only after that do the sections reach the devices.
		StateReader reader(payload, payloadSize);

		// -- the meta section first: it says whether this state belongs to this cartridge ---------
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
		// layout, so a Game Boy state read as if it were a Game Boy Advance one would fail on
		// nonsense long before the names could be compared.
		StateMachine machine = StateMachine::Gba;
		reader.Fields(machine);

		if (reader.Failed())
		{
			if (error != nullptr)
			{
				*error = reader.Error();
			}
			return false;
		}

		if (machine != StateMachine::Gba)
		{
			if (error != nullptr)
			{
				*error = std::string("the save state is a ") + StateMachineName(machine) +
					" state, and this machine is a Game Boy Advance";
			}
			return false;
		}

		std::string title, gameCode;
		uint64_t romSize = 0;
		uint8_t link = 0, customBios = 0, hleBios = 0, saveType = 0;
		reader.Text(title);
		reader.Text(gameCode);
		reader.Fields(romSize, link, customBios, hleBios, saveType);
		reader.End();

		if (reader.Failed())
		{
			if (error != nullptr)
			{
				*error = reader.Error();
			}
			return false;
		}

		// A state is a picture of a machine *running a particular cartridge*: its memory holds
		// that game's variables and its program counter points into that game's code. Loading it
		// into a different cartridge (or into a machine with none at all, or the other way round)
		// is not a state that can work, so the three fields the header gives are compared before
		// anything is applied. A machine with *no* cartridge is a state of its own kind - the boot
		// ROM's link driver is what runs then - and its three fields are the empty ones.
		bool sameCartridge = (title == bus->cart.Title()) && (gameCode == bus->cart.GameCode()) &&
			(romSize == (uint64_t)bus->cart.RomSize());

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
						bus->cart.Title() + "\"";
				}
				else
				{
					*error = "the save state belongs to \"" + title + "\" and this machine is running \"" +
						bus->cart.Title() + "\"";
				}
			}
			return false;
		}

		// -- the sections ----------------------------------------------------------------
		//
		// Each one reaches its own device as it arrives, in the order SaveState wrote them. The
		// reader latches the first failure and refuses the sections after it, and every section
		// checks that it consumed exactly its own length - so a state whose layout this build does
		// not share (a member added to a device on one side only) is reported by name rather than
		// loaded as a machine that is quietly wrong. The header's checksum is what keeps a
		// *corrupt* image out; this is what keeps an *incompatible* one out.

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
			else if (strcmp(tag, TagSio) == 0)
			{
				reader.Begin(TagSio);
				bus->sio.LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagDma) == 0)
			{
				reader.Begin(TagDma);
				bus->dma.LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagTimers) == 0)
			{
				reader.Begin(TagTimers);
				bus->timers.LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagIrq) == 0)
			{
				reader.Begin(TagIrq);
				bus->irq.LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagKeypad) == 0)
			{
				reader.Begin(TagKeypad);
				bus->keypad.LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagCart) == 0)
			{
				reader.Begin(TagCart);
				bus->cart.LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagHle) == 0)
			{
				reader.Begin(TagHle);
				HleBios::LoadState(reader);
				reader.End();
			}
			else
			{
				// A section this build does not know. The header's checksum already said the image
				// is intact, so this is a state from a format that grew a section: the length in
				// the header is what makes stepping over it safe.
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

		// The state is applied by now, so the machine's own bookkeeping that a state does not
		// carry is put back the way the sections describe it.
		linkMode = (link != 0);

		if (error != nullptr)
		{
			error->clear();
		}

		return true;
	}

	bool GbaSystem::LoadState(const std::vector<uint8_t>& image, std::string* error)
	{
		return LoadState(image.empty() ? nullptr : image.data(), image.size(), error);
	}

	bool GbaSystem::SaveStateFile(const std::string& path, std::string* error) const
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

	bool GbaSystem::LoadStateFile(const std::string& path, std::string* error)
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

	std::string GbaSystem::StateFilePath(int slot) const
	{
		if (slot < 0)
		{
			slot = 0;
		}
		if (slot > MaxStateSlot)
		{
			slot = MaxStateSlot;
		}

		// The state lives next to the battery save and is named after the ROM:
		// `Metroid Fusion.gba` -> `Metroid Fusion.st0` (the suffix is the slot number). The save
		// path already sits in the configured save directory (GbaSystem::LoadRomFile builds it
		// there), so stripping its extension gives the whole base of the name.
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
		else
		{
			// Nothing to take a name from but the cartridge itself: a ROM loaded from an image has
			// no file name, so its title is used (with the characters a file name cannot hold
			// replaced), and a machine with no cartridge at all - the boot ROM's link driver - has
			// no title either and gets a fixed one.
			std::string name = bus->cart.Title();

			if (name.empty() && !romPath.empty())
			{
				name = RomBaseName();
			}

			if (name.empty())
			{
				name = "gba_state";
			}
			else
			{
				for (char& c : name)
				{
					if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
						c == '<' || c == '>' || c == '|')
					{
						c = '_';
					}
				}
			}

			base = settings.saveDirectory.empty() ? name : (settings.saveDirectory + "/" + name);
		}

		char suffix[16];
		sprintf(suffix, ".st%i", slot);
		return base + suffix;
	}

	std::string GbaSystem::RomBaseName() const
	{
		std::string path = romPath;
		size_t slash = path.find_last_of("/\\");
		std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
		size_t dot = base.find_last_of('.');
		return (dot == std::string::npos) ? base : base.substr(0, dot);
	}
}
