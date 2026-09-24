// Save states of the emulated GameCube: the two cursors and the image header (the format is
// described in savestate.h).
//
// Nothing here knows about a device: the cursors are a byte image with a checksum and a run of
// sections, and the machine's own SaveState/LoadState members (one pair per device, and the pair
// this file's `Save`/`Load` ties together) are what fills a section.
//
// Byte order is little endian and explicit: the fields go out least significant byte first instead
// of as a memcpy of the host's own layout, so the format does not depend on the compiler that
// wrote it and a state written by the 32-bit build is read by the 64-bit one.

#include "pch.h"
#include "savestate.h"

#include <cstdio>

namespace SaveStates
{
	const uint8_t Magic[8] = { 'P', 'S', 'A', 'V', 'E', 'S', 'T', 0 };

	const char* MachineName(Machine machine)
	{
		switch (machine)
		{
		case Machine::GameCube: return "GameCube";
		case Machine::GameBoyAdvance: return "Game Boy Advance";
		case Machine::GameBoy: return "Game Boy";
		default: return "unknown machine";
		}
	}

	// The two cursors are defined in savestate.h: a block that carries a SaveState/LoadState pair
	// needs nothing but that header, which is what lets the unit tests compile a handful of blocks
	// without the machine behind them. What is left here is the image itself (the header and the
	// checksum around the sections) and the machine that fills them.

	// ---------------------------------------------------------------------------------------
	// The image
	// ---------------------------------------------------------------------------------------

	uint64_t Checksum(const uint8_t* data, size_t size)
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

	void Wrap(const std::vector<uint8_t>& payload, std::vector<uint8_t>& out)
	{
		out.clear();
		out.reserve(HeaderSize + payload.size());

		for (int i = 0; i < 8; i++)
		{
			out.push_back(Magic[i]);
		}

		PutU32(out, FormatVersion);
		PutU32(out, 0);
		PutU64(out, (uint64_t)payload.size());
		PutU64(out, Checksum(payload.empty() ? nullptr : payload.data(), payload.size()));
		out.insert(out.end(), payload.begin(), payload.end());
	}

	bool Unwrap(const uint8_t* image, size_t size, const uint8_t** payload, size_t* payloadSize,
		std::string& error)
	{
		if (image == nullptr || size < HeaderSize)
		{
			error = "the file is too short to be a save state";
			return false;
		}

		for (int i = 0; i < 8; i++)
		{
			if (image[i] != Magic[i])
			{
				error = "the file is not a save state";
				return false;
			}
		}

		uint32_t version = GetU32(image + 0x08);

		if (version != FormatVersion)
		{
			char text[128];
			snprintf(text, sizeof(text), "the save state is format version %u and this build reads %u",
				(unsigned)version, (unsigned)FormatVersion);
			error = text;
			return false;
		}

		uint64_t length = GetU64(image + 0x10);

		if (length != (uint64_t)(size - HeaderSize))
		{
			error = "the save state says it is a different size than it is";
			return false;
		}

		uint64_t checksum = GetU64(image + 0x18);

		if (checksum != Checksum(image + HeaderSize, (size_t)length))
		{
			error = "the save state is corrupt (its checksum does not match)";
			return false;
		}

		*payload = image + HeaderSize;
		*payloadSize = (size_t)length;
		return true;
	}

	// =======================================================================================
	// The machine
	// =======================================================================================
	//
	// The sections, in the order they are written. One section per block keeps a state readable
	// when something is wrong with it: the reader names the section whose layout did not fit, and
	// the meta section names the image the state was taken from, so a state of another disc, of
	// the boot ROM, or of a console with more main memory is refused by name instead of loading a
	// machine whose code does not match its memory.

	static const char* const TagMeta = "META";
	static const char* const TagFlipper = "FLPR";
	static const char* const TagCpu = "CPU ";
	static const char* const TagMem = "MEM ";
	static const char* const TagPi = "PI  ";
	static const char* const TagVi = "VI  ";
	static const char* const TagAi = "AI  ";
	static const char* const TagDi = "DI  ";
	static const char* const TagSi = "SI  ";
	static const char* const TagExi = "EXI ";
	static const char* const TagCp = "CP  ";
	static const char* const TagDsp = "DSP ";
	static const char* const TagDvd = "DVD ";
	static const char* const TagGx = "GX  ";
	static const char* const TagPe = "PE  ";
	static const char* const TagXf = "XF  ";
	static const char* const TagSu = "SU  ";
	static const char* const TagRas = "RAS ";
	static const char* const TagTev = "TEV ";
	static const char* const TagTx = "TX  ";
	static const char* const TagBump = "BP  ";
	static const char* const TagHle = "HLE ";

	/// <summary>
	/// The identity of the image the console is running, as a string a state can be compared
	/// against: the four character game code of the disc in the drive, or "no disc" when the drive
	/// is empty and the executable came from a file. Reading it seeks the drive, so the read
	/// position is put back where it was - a save state must not move the drive.
	/// </summary>
	static std::string DiskIdentity()
	{
		if (!DVD::IsMounted())
		{
			return "no disc";
		}

		int position = DVD::GetSeek();

		std::wstring id;
		GetDiskId(id);

		DVD::Seek(position);

		std::string narrow = Util::WstringToString(id);

		return narrow.empty() ? std::string("unknown disc") : narrow;
	}

	/// <summary>The base name of a path, without its directory and (for the identity check) with
	/// the extension kept - it is the name of the file the user loaded, so it has to stay
	/// recognizable in a report.</summary>
	static std::string BaseName(const std::string& path)
	{
		size_t slash = path.find_last_of("/\\");
		return (slash == std::string::npos) ? path : path.substr(slash + 1);
	}

	/// <summary>
	/// Stop the core for as long as the object lives, and start it again afterwards the way it
	/// was. The menu and the debug interface call the machine from a thread that does not own it:
	/// the Gekko core runs on a thread of its own, and a state written while it is running is a
	/// state of nothing. The core notices `suspended` at the top of its next quantum, which is why
	/// the wait is here rather than a join (the front end's own Stop does the same).
	/// </summary>
	namespace
	{
		class CorePause
		{
			bool wasRunning = false;
			bool started = false;

		public:
			CorePause()
			{
				if (emu.loaded && Core != nullptr)
				{
					wasRunning = Core->IsRunning();
					started = true;

					if (wasRunning)
					{
						Core->Suspend();
						Thread::Sleep(100);
					}
				}
			}

			~CorePause()
			{
				if (started && wasRunning && Core != nullptr && emu.loaded)
				{
					Core->Run();
				}
			}
		};
	}

	bool Save(std::vector<uint8_t>& image, std::string* error)
	{
		if (!emu.loaded || Flipper::HW == nullptr || Core == nullptr)
		{
			if (error != nullptr)
			{
				*error = "nothing is running";
			}
			return false;
		}

		StateWriter writer;

		// -- the identity of the image the state belongs to ------------------------------------
		//
		// Everything a state deliberately leaves out of the machine - the disc, the boot ROM, the
		// memory size the front end was configured with - is named here, so that a state offered to
		// a console running another image is refused with that name instead of being applied to a
		// machine whose memory holds something else.
		writer.Begin(TagMeta);
		writer.Fields(Machine::GameCube);
		writer.U64((uint64_t)Flipper::HW->GetMemorySize());
		writer.U32((uint32_t)GetConfigInt(USER_CONSOLE, USER_HW));
		writer.U8(emu.bootrom ? 1 : 0);
		writer.Text(DiskIdentity());
		writer.Text(BaseName(Util::WstringToString(emu.lastLoaded)));
		writer.End();

		// -- the ASIC object itself (the deadline of the periodic work) ------------------------
		writer.Begin(TagFlipper);
		Flipper::HW->SaveState(writer);
		writer.End();

		// -- the processor ---------------------------------------------------------------------
		writer.Begin(TagCpu);
		Core->SaveState(writer);
		writer.End();

		// -- main memory and the memory interface ----------------------------------------------
		writer.Begin(TagMem);
		Flipper::HW->mem->SaveState(writer);
		writer.End();

		// -- the Flipper blocks ----------------------------------------------------------------
		writer.Begin(TagPi);
		Flipper::HW->pi->SaveState(writer);
		writer.End();

		writer.Begin(TagVi);
		Flipper::HW->vi->SaveState(writer);
		writer.End();

		writer.Begin(TagAi);
		Flipper::HW->ai->SaveState(writer);
		writer.End();

		writer.Begin(TagDi);
		Flipper::HW->di->SaveState(writer);
		writer.End();

		writer.Begin(TagSi);
		Flipper::HW->si->SaveState(writer);
		writer.End();

		writer.Begin(TagExi);
		Flipper::HW->exi->SaveState(writer);
		writer.End();

		writer.Begin(TagCp);
		Flipper::HW->cp->SaveState(writer);
		writer.End();

		writer.Begin(TagDsp);
		Flipper::DSP->SaveState(writer);
		writer.End();

		writer.Begin(TagDvd);
		DVD::DDU->SaveState(writer);
		writer.End();

		// -- the graphics pipeline -------------------------------------------------------------
		//
		// The blocks of the pipeline each have a section of their own: the picture a state resumes
		// on is the one the guest left in the EFB, and a state whose rasterization registers did
		// not fit is reported with the name of the block that did not rather than as a wrong
		// frame.
		writer.Begin(TagGx);
		Flipper::HW->gfx->SaveState(writer);
		writer.End();

		writer.Begin(TagPe);
		Flipper::HW->gfx->pe->SaveState(writer);
		writer.End();

		writer.Begin(TagXf);
		Flipper::HW->gfx->xf->SaveState(writer);
		writer.End();

		writer.Begin(TagSu);
		Flipper::HW->gfx->su->SaveState(writer);
		writer.End();

		writer.Begin(TagRas);
		Flipper::HW->gfx->ras->SaveState(writer);
		writer.End();

		writer.Begin(TagTev);
		Flipper::HW->gfx->tev->SaveState(writer);
		writer.End();

		writer.Begin(TagTx);
		Flipper::HW->gfx->tx->SaveState(writer);
		writer.End();

		writer.Begin(TagBump);
		Flipper::HW->gfx->bump->SaveState(writer);
		writer.End();

		// -- the host-side OS calls ------------------------------------------------------------
		writer.Begin(TagHle);
		HLE::SaveState(writer);
		writer.End();

		Wrap(writer.Image(), image);

		if (error != nullptr)
		{
			error->clear();
		}

		return true;
	}

	/// <summary>
	/// The fixes a restored machine needs before it runs again: the derived values that follow from
	/// the registers a state carried are recomputed, and the caches that a state does not carry are
	/// dropped, so that nothing of the machine that was running before the load survives into the
	/// one the state describes.
	/// </summary>
	static void RefreshAfterLoad()
	{
		// The recompiler's blocks are keyed by the code they were translated from, and a state can
		// put other code at those addresses. The block cache is dropped rather than compared: it
		// refills as the guest runs, and a stale block would be executed silently.
		if (Core->jit != nullptr)
		{
			Core->jit->InvalidateAll();
		}

		// The DSP recompiler bakes the instruction words into its blocks (its `FindBlock` cannot
		// tell two different programs apart), so it has to be dropped as well - but that is done by
		// `DspCore::LoadState` itself, which is also where the generation the restored blocks
		// compare against is put back. Doing it again here would bump that generation a second
		// time and make the next state differ from the one that was loaded.

		// The processor's interrupt line is re-derived from the interrupt controller the PI
		// section restored. The CPU section carries the line as a flag of its own and it is
		// normally already right, but the line is a *function* of INTSR and INTMR and the state
		// has just replaced both: deriving it here is what makes the two agree even when the
		// state came from a build whose PI section had grown or shrunk.
		if (Flipper::HW->pi->State().intsr & Flipper::HW->pi->State().intmr)
		{
			Core->AssertInterrupt();
		}
		else
		{
			Core->ClearInterrupt();
		}

		// The memory interface counts the translations its accessors are asked for, and the load
		// asked it for a few of its own (the sections after `MEM ` walk into main memory). The
		// counters the state carried are what the machine is supposed to have, so they go back
		// last - after every block that could have counted has been put in place.
		Flipper::HW->mem->RefreshAfterLoad();

		// The graphics pipeline's decoded state follows from the registers: the viewport, the
		// scissor, the depth and colour modes, the texture maps and the fragment program are all
		// rebuilt from what the sections restored.
		Flipper::HW->gfx->RefreshAfterLoad();
	}

	bool Load(const uint8_t* image, size_t size, std::string* error)
	{
		if (!emu.loaded || Flipper::HW == nullptr || Core == nullptr)
		{
			if (error != nullptr)
			{
				*error = "nothing is running";
			}
			return false;
		}

		std::string message;
		const uint8_t* payload = nullptr;
		size_t payloadSize = 0;

		if (!Unwrap(image, size, &payload, &payloadSize, message))
		{
			if (error != nullptr)
			{
				*error = message;
			}
			return false;
		}

		// Everything that can be checked without touching the machine is checked before a single
		// byte of it is applied: the header (in `Unwrap`: the magic, the format version and the
		// checksum of the payload) and then the machine section, which names the machine, the
		// image and the memory size this state was taken from. Only after that do the sections
		// reach the devices, and they are applied in the order they were written.
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
		// before anything else is read out of it: the machines do not share a layout, so a state of
		// another one read as if it were ours would fail on nonsense long before the names could be
		// compared.
		Machine machine = Machine::GameCube;
		reader.Fields(machine);

		if (reader.Failed())
		{
			if (error != nullptr)
			{
				*error = reader.Error();
			}
			return false;
		}

		if (machine != Machine::GameCube)
		{
			if (error != nullptr)
			{
				*error = std::string("the save state is a ") + MachineName(machine) +
					" state, and this machine is a GameCube";
			}
			return false;
		}

		uint64_t memorySize = 0;
		uint32_t consoleVer = 0;
		uint8_t bootrom = 0;
		std::string discId, imageName;

		reader.Fields(memorySize, consoleVer, bootrom);
		reader.Text(discId);
		reader.Text(imageName);
		reader.End();

		if (reader.Failed())
		{
			if (error != nullptr)
			{
				*error = reader.Error();
			}
			return false;
		}

		// The three fields that pin the state to a machine: the same main memory, the same console
		// revision and the same image. The console revision is not compared - a title that does not
		// look at it (most of them do not) runs on either - but the memory size is: the RAM section
		// of the state is exactly as large as the machine that wrote it, and applying a 24 MB state
		// to a 48 MB machine would leave half of its memory holding a mixture of the two runs.
		if (memorySize != (uint64_t)Flipper::HW->GetMemorySize())
		{
			if (error != nullptr)
			{
				char text[160];
				snprintf(text, sizeof(text),
					"the save state was taken on a console with %u MBytes of main memory and this one has %u",
					(unsigned)(memorySize >> 20), (unsigned)(Flipper::HW->GetMemorySize() >> 20));
				*error = text;
			}
			return false;
		}

		std::string currentDisc = DiskIdentity();
		std::string currentImage = BaseName(Util::WstringToString(emu.lastLoaded));

		if (imageName != currentImage || discId != currentDisc)
		{
			if (error != nullptr)
			{
				*error = "the save state belongs to \"" + imageName + "\" (" + discId +
					") and this console is running \"" + currentImage + "\" (" + currentDisc + ")";
			}
			return false;
		}

		// -- the sections ------------------------------------------------------------------------
		//
		// Each one reaches its own block as it arrives, in the order Save wrote them. The reader
		// latches the first failure and refuses the sections after it, and every section checks
		// that it consumed exactly its own length - so a state whose layout this build does not
		// share (a member added to a block on one side only) is reported by name rather than
		// loaded as a machine that is quietly wrong. The header's checksum is what keeps a
		// *corrupt* image out; this is what keeps an *incompatible* one out.
		while (!reader.Failed() && reader.Cursor() < reader.Size())
		{
			if (!reader.PeekTag(tag))
			{
				break;
			}

			if (strcmp(tag, TagFlipper) == 0)
			{
				reader.Begin(TagFlipper);
				Flipper::HW->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagCpu) == 0)
			{
				reader.Begin(TagCpu);
				Core->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagMem) == 0)
			{
				reader.Begin(TagMem);
				Flipper::HW->mem->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagPi) == 0)
			{
				reader.Begin(TagPi);
				Flipper::HW->pi->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagVi) == 0)
			{
				reader.Begin(TagVi);
				Flipper::HW->vi->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagAi) == 0)
			{
				reader.Begin(TagAi);
				Flipper::HW->ai->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagDi) == 0)
			{
				reader.Begin(TagDi);
				Flipper::HW->di->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagSi) == 0)
			{
				reader.Begin(TagSi);
				Flipper::HW->si->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagExi) == 0)
			{
				reader.Begin(TagExi);
				Flipper::HW->exi->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagCp) == 0)
			{
				reader.Begin(TagCp);
				Flipper::HW->cp->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagDsp) == 0)
			{
				reader.Begin(TagDsp);
				Flipper::DSP->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagDvd) == 0)
			{
				reader.Begin(TagDvd);
				DVD::DDU->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagGx) == 0)
			{
				reader.Begin(TagGx);
				Flipper::HW->gfx->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagPe) == 0)
			{
				reader.Begin(TagPe);
				Flipper::HW->gfx->pe->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagXf) == 0)
			{
				reader.Begin(TagXf);
				Flipper::HW->gfx->xf->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagSu) == 0)
			{
				reader.Begin(TagSu);
				Flipper::HW->gfx->su->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagRas) == 0)
			{
				reader.Begin(TagRas);
				Flipper::HW->gfx->ras->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagTev) == 0)
			{
				reader.Begin(TagTev);
				Flipper::HW->gfx->tev->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagTx) == 0)
			{
				reader.Begin(TagTx);
				Flipper::HW->gfx->tx->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagBump) == 0)
			{
				reader.Begin(TagBump);
				Flipper::HW->gfx->bump->LoadState(reader);
				reader.End();
			}
			else if (strcmp(tag, TagHle) == 0)
			{
				reader.Begin(TagHle);
				HLE::LoadState(reader);
				reader.End();
			}
			else
			{
				// A section this build does not know. The header's checksum already said the image
				// is intact, so this is a state from a format that grew a section: the length in
				// the section header is what makes stepping over it safe.
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

		RefreshAfterLoad();

		if (error != nullptr)
		{
			error->clear();
		}

		return true;
	}

	bool Load(const std::vector<uint8_t>& image, std::string* error)
	{
		return Load(image.empty() ? nullptr : image.data(), image.size(), error);
	}

	// ---------------------------------------------------------------------------------------
	// The files
	// ---------------------------------------------------------------------------------------

	bool SaveFile(const std::string& path, std::string* error)
	{
		std::vector<uint8_t> image;

		// The machine has to stand still while it is written: it runs on a thread of its own.
		{
			CorePause pause;

			if (!Save(image, error))
			{
				return false;
			}
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

		if (fclose(f) != 0)
		{
			written = 0;
		}

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

	bool LoadFile(const std::string& path, std::string* error)
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

		// The checks happen before the machine is touched only in part: `Load` refuses a header or
		// a machine section that does not match before it applies anything, but a state whose
		// sections fail half way through leaves the machine half loaded. That is the same
		// trade-off the GBA module makes, and the alternative - building a whole second machine -
		// is not what this emulator is shaped for; the state that fails is a foreign state, and
		// the guest is expected to be reset after a failed load rather than resumed.
		CorePause pause;

		return Load(image, error);
	}

	int ClampSlot(int slot)
	{
		if (slot < 0)
		{
			return 0;
		}
		if (slot > MaxSlot)
		{
			return MaxSlot;
		}
		return slot;
	}

	std::string SlotPath(int slot)
	{
		slot = ClampSlot(slot);

		// The state lives next to the image and is named after it, the way the GBA module names
		// its states after the cartridge: `Data/game.iso` gives `Data/game.st0`. A console with no
		// image of its own - the boot ROM - is running the pseudo-name "Bootrom", which is what
		// `emu.lastLoaded` holds, so its states are `Bootrom.st0` in the working directory.
		std::string base = Util::WstringToString(emu.lastLoaded);

		if (base.empty())
		{
			base = "gamecube";
		}

		size_t dot = base.find_last_of('.');
		size_t slash = base.find_last_of("/\\");

		if (dot != std::string::npos && (slash == std::string::npos || slash < dot))
		{
			base = base.substr(0, dot);
		}

		char suffix[16];
		snprintf(suffix, sizeof(suffix), ".st%i", slot);

		return base + suffix;
	}

	// ---------------------------------------------------------------------------------------
	// The reports
	//
	// The debug interface answers Markdown (see wiki/savestate.md), so what a command prints is
	// the same text the debugger would put in a panel: the slot, the file, what happened.
	// ---------------------------------------------------------------------------------------

	static long StateFileSize(const std::string& path)
	{
		if (!Util::FileExists(path))
		{
			return -1;
		}

		return (long)Util::FileSize(path);
	}

	static void MdBullet(std::string& md, const char* format, ...)
	{
		char text[0x400];
		va_list args;
		va_start(args, format);
		vsnprintf(text, sizeof(text), format, args);
		va_end(args);

		md += "* ";
		md += text;
		md += "\n";
	}

	StateResult SaveToSlot(int slot)
	{
		StateResult result;
		result.slot = ClampSlot(slot);
		result.path = SlotPath(result.slot);

		if (emu.loaded)
		{
			result.ok = SaveFile(result.path, &result.error);
		}
		else
		{
			result.error = "nothing is running";
		}

		result.markdown = "# Save State\n\n## Write\n";

		MdBullet(result.markdown, "slot: **%i**, file: `%s`", result.slot, result.path.c_str());

		if (result.ok)
		{
			result.size = StateFileSize(result.path);
			MdBullet(result.markdown, "result: **saved** (%li bytes)", result.size);
			Debug::Report(Debug::Channel::Norm, "savestate: slot %i -> %s\n",
				result.slot, result.path.c_str());
		}
		else
		{
			MdBullet(result.markdown, "result: **failed** - %s", result.error.c_str());
			Debug::Report(Debug::Channel::Norm, "savestate: %s\n", result.error.c_str());
		}

		return result;
	}

	StateResult LoadFromSlot(int slot)
	{
		StateResult result;
		result.slot = ClampSlot(slot);
		result.path = SlotPath(result.slot);

		if (emu.loaded)
		{
			result.size = StateFileSize(result.path);
			result.ok = LoadFile(result.path, &result.error);
		}
		else
		{
			result.error = "nothing is running";
		}

		result.markdown = "# Save State\n\n## Read\n";

		MdBullet(result.markdown, "slot: **%i**, file: `%s`", result.slot, result.path.c_str());

		if (result.ok)
		{
			MdBullet(result.markdown, "result: **loaded**");
			MdBullet(result.markdown, "ticks: **%lld**, pc: **0x%08X**",
				(long long)Core->GetTicks(), Core->regs.pc);
			Debug::Report(Debug::Channel::Norm, "loadstate: slot %i <- %s\n",
				result.slot, result.path.c_str());
		}
		else
		{
			MdBullet(result.markdown, "result: **failed** - %s", result.error.c_str());
			Debug::Report(Debug::Channel::Norm, "loadstate: %s\n", result.error.c_str());
		}

		return result;
	}

	std::string SaveReport(int slot)
	{
		return SaveToSlot(slot).markdown;
	}

	std::string LoadReport(int slot)
	{
		return LoadFromSlot(slot).markdown;
	}

	std::string SlotsReport()
	{
		std::string md = "# Save States\n\n## Slots\n";
		md += "| Slot | File | Size |\n";
		md += "|---|---|---|\n";

		int found = 0;

		for (int slot = 0; slot <= MaxSlot; slot++)
		{
			std::string path = SlotPath(slot);
			long size = StateFileSize(path);

			if (size < 0)
			{
				continue;
			}

			char line[0x400];
			snprintf(line, sizeof(line), "| %i | `%s` | %li |\n", slot, path.c_str(), size);
			md += line;
			found++;
		}

		if (found == 0)
		{
			MdBullet(md, "no save state has been written for this image yet");
		}
		else
		{
			MdBullet(md, "**%i** of the **%i** slots are in use", found, MaxSlot + 1);
		}

		return md;
	}
}
