// Save states of the emulated GameCube: the state format and its two cursors.
//
// A save state is the whole machine in a byte image: the Gekko processor (every register, the
// machine state register, the time base, the segment registers, the translation caches and the
// breakpoints), main memory (Splash) with the memory interface that owns it, the Flipper blocks
// (the command processor and its FIFO, the pixel engine, the transform/setup/raster/texture/TEV
// stages, the video, audio, disk, serial and external interfaces, the processor interface, the
// DSP with its core, its DMA engines and its ARAM) and the drive. What is *not* in a state is the
// image the console is running - the disc or the executable, the boot ROM, the DSP ROMs, the
// memory card files - because those belong to the front end and the settings rather than to the
// machine; a state names the image it was taken from instead (see the META section) and refuses
// to load into a console running another one.
//
// The image is a header, a checksum of the payload, and then one *section* per subsystem:
//
//   0x00  magic   "PSAVEST" + NUL
//   0x08  u32     the format version (`FormatVersion`)
//   0x0C  u32     reserved (0)
//   0x10  u64     the size of the payload
//   0x18  u64     the FNV-1a 64 checksum of the payload
//   0x20  payload: sections
//
// A section is a four character tag, a u32 length, and the bytes of the section. A reader walks
// the sections in order and dispatches on the tag, which is what lets a future version add one
// without moving everything after it - and what makes a state written by another build fail with
// the name of the section that did not fit instead of silently loading garbage. Every section
// checks that it consumed exactly its own length, so a member added to a device but left out of
// its SaveState/LoadState pair is a load error rather than a wrong machine.
//
// The two cursors are the whole interface the devices see: `StateWriter` appends to the image and
// `StateReader` walks it, both with `Fields` for a run of members, `Array` for a C array, `Values`
// for a `std::vector`, `Bytes` for a byte vector, `Text` for a string and `Raw` for a block of
// memory. A read that runs off the end of the image latches a failure and every later read answers
// zero, so a truncated or foreign image is refused by the caller and never becomes a machine.
//
// The format is shared with the integrated GBA emulator (`src/gba/gba_savestate.h`): the magic,
// the header and the section framing are the same, so one reader-friendly description covers both,
// but a state of one machine is never read as the other's - the first field of the first section
// names the machine it was taken from.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace SaveStates
{
	/// <summary>
	/// The version of the state format. A reader refuses an image whose version it does not know;
	/// the version is raised whenever a section's layout changes in a way an older reader would
	/// misread (adding a member to a device is enough - the section length check would catch it,
	/// but the version makes the reason readable).
	/// </summary>
	const uint32_t FormatVersion = 1;

	/// <summary>The eight bytes every state image starts with ("PSAVEST" and a NUL).</summary>
	extern const uint8_t Magic[8];

	/// <summary>The size of the image header (`HeaderSize` bytes before the first section).</summary>
	const size_t HeaderSize = 0x20;

	/// <summary>
	/// The save state slots a frontend offers (`.st0` .. `.st9`). The quick save keys step through
	/// them and the `savestate`/`loadstate` commands take the slot as an argument.
	/// </summary>
	const int MaxSlot = 9;

	/// <summary>
	/// The machine a state was taken from. It is the first field of the first section: the image
	/// format is shared with the integrated GBA emulator, and neither machine can read the other's
	/// state.
	/// </summary>
	enum class Machine : uint8_t
	{
		GameCube = 1,		// the GameCube (this module: src/gekko*.cpp, src/flipper.cpp and the blocks)
		GameBoyAdvance = 2,	// the integrated GBA emulator (src/gba/gba_savestate.cpp)
		GameBoy = 3,		// the integrated Game Boy (src/gba/gb_savestate.cpp)
	};

	/// <summary>The name of a machine kind, for the reports and the error messages.</summary>
	const char* MachineName(Machine machine);

	// ---------------------------------------------------------------------------------------
	// Writing
	// ---------------------------------------------------------------------------------------

	class StateWriter
	{
	public:
		StateWriter() = default;

		/// <summary>Open a section. Every `Begin` is closed by its `End`.</summary>
		void Begin(const char* tag);

		/// <summary>Close the section `Begin` opened, patching its length into the image.</summary>
		void End();

		/// <summary>A run of members, each written at its own width (see `Value`).</summary>
		template <typename... T>
		StateWriter& Fields(const T&... values)
		{
			(Value(values), ...);
			return *this;
		}

		/// <summary>A C array, written element by element.</summary>
		template <typename T, size_t N>
		StateWriter& Array(const T (&values)[N])
		{
			for (size_t i = 0; i < N; i++)
				Value(values[i]);
			return *this;
		}

		/// <summary>A 2D C array, written row by row.</summary>
		template <typename T, size_t N, size_t M>
		StateWriter& Array(const T (&values)[N][M])
		{
			for (size_t i = 0; i < N; i++)
				for (size_t j = 0; j < M; j++)
					Value(values[i][j]);
			return *this;
		}

		/// <summary>A vector: its length and then its elements.</summary>
		template <typename T>
		StateWriter& Values(const std::vector<T>& values)
		{
			U32((uint32_t)values.size());
			for (const T& value : values)
				Value(value);
			return *this;
		}

		/// <summary>A vector of bytes: its length and then the bytes themselves.</summary>
		StateWriter& Bytes(const std::vector<uint8_t>& values);

		/// <summary>A block of memory, exactly as it lies in the machine.</summary>
		StateWriter& Raw(const void* data, size_t size);

		/// <summary>A string: its length and then its bytes.</summary>
		StateWriter& Text(const std::string& text);

		StateWriter& U8(uint8_t value);
		StateWriter& U16(uint16_t value);
		StateWriter& U32(uint32_t value);
		StateWriter& U64(uint64_t value);

		const std::vector<uint8_t>& Image() const { return image; }
		std::vector<uint8_t>& Image() { return image; }
		size_t Size() const { return image.size(); }

	private:
		std::vector<uint8_t> image;
		size_t lengthOffset = 0;			// where the open section's length goes
		size_t sectionStart = 0;			// where the open section's payload starts

		/// <summary>The bytes of an unsigned value, least significant byte first.</summary>
		void Unsigned(uint64_t value, size_t width);

		/// <summary>The bits of a float or a double, as the unsigned integer of the same width.</summary>
		template <typename T>
		static uint64_t FloatBits(T value)
		{
			if constexpr (sizeof(T) == 4)
			{
				uint32_t bits;
				memcpy(&bits, &value, sizeof(bits));
				return bits;
			}
			else
			{
				uint64_t bits;
				memcpy(&bits, &value, sizeof(bits));
				return bits;
			}
		}

		template <typename T>
		void Value(const T& value)
		{
			if constexpr (std::is_array_v<T>)
			{
				for (const auto& element : value)
					Value(element);
			}
			// `remove_cv_t` so that a `volatile bool` - the Flipper blocks keep several of their
			// flags that way - takes this arm and goes out as one byte, exactly like the plain
			// `bool` it is. Everything else is a cv-qualified scalar too and the integral and
			// floating point arms already ignore the qualifiers.
			else if constexpr (std::is_same_v<std::remove_cv_t<T>, bool>)
			{
				image.push_back(value ? 1 : 0);
			}
			else if constexpr (std::is_enum_v<T>)
			{
				Unsigned((uint64_t)(int64_t)value, sizeof(T));
			}
			else if constexpr (std::is_integral_v<T>)
			{
				Unsigned((uint64_t)(int64_t)value, sizeof(T));
			}
			else if constexpr (std::is_floating_point_v<T>)
			{
				// A float or a double goes out as the bits of its IEEE-754 representation, least
				// significant byte first, exactly like an integer of the same width. The GFX
				// pipeline keeps its matrices and its rasterization planes as floats and they are
				// part of the machine's state, so they have to travel with it.
				Unsigned(FloatBits(value), sizeof(T));
			}
			else
			{
				static_assert(sizeof(T) == 0, "a save state field must be a bool, an integer, an enum or a string");
			}
		}
	};

	// ---------------------------------------------------------------------------------------
	// Reading
	// ---------------------------------------------------------------------------------------

	class StateReader
	{
	public:
		StateReader(const uint8_t* data, size_t size) : data(data), size(size) {}

		/// <summary>The tag of the next section, without consuming it (5 bytes, NUL terminated).</summary>
		bool PeekTag(char tag[5]);

		/// <summary>Open the section with this tag. A different tag is a load failure.</summary>
		bool Begin(const char* tag);

		/// <summary>Close the open section: it must have been read exactly to its end.</summary>
		bool End();

		/// <summary>Step over the next section without reading it (a section this build does not
		/// know: a state from a newer format, or an image that is not one at all - the header's
		/// checksum is what tells those apart).</summary>
		bool Skip();

		/// <summary>True once a read has failed. Every later read answers zero and changes nothing.</summary>
		bool Failed() const { return failed; }

		/// <summary>What went wrong, as a sentence for the user ("" when nothing has).</summary>
		const std::string& Error() const { return error; }

		/// <summary>Latch a failure (the devices use it for a value they cannot accept).</summary>
		void Fail(const std::string& reason);

		template <typename... T>
		StateReader& Fields(T&... values)
		{
			(Value(values), ...);
			return *this;
		}

		template <typename T, size_t N>
		StateReader& Array(T (&values)[N])
		{
			for (size_t i = 0; i < N; i++)
				Value(values[i]);
			return *this;
		}

		template <typename T, size_t N, size_t M>
		StateReader& Array(T (&values)[N][M])
		{
			for (size_t i = 0; i < N; i++)
				for (size_t j = 0; j < M; j++)
					Value(values[i][j]);
			return *this;
		}

		template <typename T>
		StateReader& Values(std::vector<T>& values)
		{
			uint32_t count = U32();

			if (failed)
				return *this;

			// A length that cannot have come from this machine is a broken image: the vectors it
			// holds are the CP's FIFO backlog and the like, never megabytes of them.
			if (count > (16u << 20))
			{
				Fail("a vector length is out of range");
				return *this;
			}

			values.resize(count);

			for (T& value : values)
				Value(value);

			return *this;
		}

		StateReader& Bytes(std::vector<uint8_t>& values);
		StateReader& Raw(void* data, size_t size);
		StateReader& Text(std::string& text);

		uint8_t U8();
		uint16_t U16();
		uint32_t U32();
		uint64_t U64();

		size_t Cursor() const { return cursor; }
		size_t Size() const { return size; }

	private:
		const uint8_t* data = nullptr;
		size_t size = 0;
		size_t cursor = 0;
		size_t sectionEnd = 0;
		bool failed = false;
		std::string error;

		/// <summary>The bytes of an unsigned value, least significant byte first.</summary>
		uint64_t Unsigned(size_t width);

		/// <summary>The float or double an unsigned value read back is the bits of.</summary>
		template <typename T>
		static T FloatFrom(uint64_t bits)
		{
			if constexpr (sizeof(T) == 4)
			{
				uint32_t half = (uint32_t)bits;
				T value;
				memcpy(&value, &half, sizeof(value));
				return value;
			}
			else
			{
				T value;
				memcpy(&value, &bits, sizeof(value));
				return value;
			}
		}

		template <typename T>
		void Value(T& value)
		{
			if constexpr (std::is_array_v<T>)
			{
				for (auto& element : value)
					Value(element);
			}
			else if constexpr (std::is_same_v<std::remove_cv_t<T>, bool>)
			{
				value = (U8() != 0);
			}
			else if constexpr (std::is_enum_v<T>)
			{
				value = (T)Unsigned(sizeof(T));
			}
			else if constexpr (std::is_integral_v<T>)
			{
				value = (T)Unsigned(sizeof(T));
			}
			else if constexpr (std::is_floating_point_v<T>)
			{
				value = FloatFrom<T>(Unsigned(sizeof(T)));
			}
			else
			{
				static_assert(sizeof(T) == 0, "a save state field must be a bool, an integer, an enum or a string");
			}
		}
	};

	// ---------------------------------------------------------------------------------------
	// The image
	// ---------------------------------------------------------------------------------------

	/// <summary>The FNV-1a 64 hash the header carries (a state that does not match its own hash is
	/// refused before any of it is applied).</summary>
	uint64_t Checksum(const uint8_t* data, size_t size);

	/// <summary>
	/// Write the image header in front of the payload `StateWriter` built. The writer holds the
	/// sections only; this is what makes it a file. `out` is the complete image.
	/// </summary>
	void Wrap(const std::vector<uint8_t>& payload, std::vector<uint8_t>& out);

	/// <summary>
	/// Check the header of a candidate image and point `payload`/`payloadSize` at its sections.
	/// Answers false and fills `error` when the image is not one of ours, is a format this build
	/// does not know, or does not match its own checksum.
	/// </summary>
	bool Unwrap(const uint8_t* image, size_t size, const uint8_t** payload, size_t* payloadSize,
		std::string& error);

	// ---------------------------------------------------------------------------------------
	// The whole machine
	//
	// The emulator is `Gekko::Core` (the processor) and `Flipper::HW` (everything else), and a
	// state is those two written out section by section. The image pair does not pause anything:
	// the caller owns the machine, and the two file helpers below are for the callers that do not
	// - the front end's menu and the debug interface - so they stop the core for the duration and
	// start it again afterwards the way it was.
	// ---------------------------------------------------------------------------------------

	/// <summary>Write the machine into a state image. The core must be stopped (see SaveFile).</summary>
	bool Save(std::vector<uint8_t>& image, std::string* error = nullptr);

	/// <summary>Put a state image back into the machine. The core must be stopped (see LoadFile).</summary>
	bool Load(const uint8_t* image, size_t size, std::string* error = nullptr);
	bool Load(const std::vector<uint8_t>& image, std::string* error = nullptr);

	/// <summary>Write the machine into a file, stopping the core for the duration.</summary>
	bool SaveFile(const std::string& path, std::string* error = nullptr);

	/// <summary>Read a state file back into the machine, stopping the core for the duration.</summary>
	bool LoadFile(const std::string& path, std::string* error = nullptr);

	/// <summary>The slot a caller asked for, brought into 0 .. MaxSlot.</summary>
	int ClampSlot(int slot);

	/// <summary>
	/// The file of a slot: the state lives next to the image that is running and is named after
	/// it, the way the GBA module names its states after the cartridge - `Data/game.iso` gives
	/// `Data/game.st0`, and a console with no image of its own (the boot ROM) uses `Bootrom.st0`.
	/// </summary>
	std::string SlotPath(int slot);

	/// <summary>
	/// What a slot operation did, with the report that describes it. The reports below are this
	/// structure written as Markdown, and the structure is also what the data-shaped callers need:
	/// the front end's menu (which shows the file it wrote) and the `savestate` command (whose
	/// answer carries the fields next to the report text).
	/// </summary>
	struct StateResult
	{
		bool ok = false;			// the state was written or read
		int slot = 0;				// the slot it happened in
		std::string path;			// the file of that slot
		std::string error;			// why it failed ("" when it did not)
		long size = -1;				// the size of the file, or -1 when there is none
		std::string markdown;		// the report of the attempt, as the debugger shows it
	};

	/// <summary>Write the machine into the file of a slot (the core is stopped for the duration).</summary>
	StateResult SaveToSlot(int slot);

	/// <summary>Read the file of a slot back into the machine (the core is stopped for the duration).</summary>
	StateResult LoadFromSlot(int slot);

	/// <summary>The report of a write (the same text `SaveToSlot` fills in).</summary>
	std::string SaveReport(int slot);

	/// <summary>The report of a read (the same text `LoadFromSlot` fills in).</summary>
	std::string LoadReport(int slot);

	/// <summary>The slots that hold a state, as a Markdown table.</summary>
	std::string SlotsReport();

	// ---------------------------------------------------------------------------------------
	// The cursors, defined here rather than in savestate.cpp
	//
	// A block that implements its own SaveState/LoadState pair needs nothing but this header: the
	// machine-level half of the subsystem (the image header, the sections and the files) is what
	// savestate.cpp holds, and it is the only part that depends on the emulator. That split is what
	// lets the unit tests compile a handful of blocks - the graphics pipeline, the command
	// processor, the serial interface, the DSP - without dragging the whole emulator in behind
	// them.
	// ---------------------------------------------------------------------------------------

	inline void StateWriter::Unsigned(uint64_t value, size_t width)
	{
		for (size_t i = 0; i < width; i++)
		{
			image.push_back((uint8_t)(value >> (i * 8)));
		}
	}

	inline StateWriter& StateWriter::U8(uint8_t value) { Unsigned(value, 1); return *this; }
	inline StateWriter& StateWriter::U16(uint16_t value) { Unsigned(value, 2); return *this; }
	inline StateWriter& StateWriter::U32(uint32_t value) { Unsigned(value, 4); return *this; }
	inline StateWriter& StateWriter::U64(uint64_t value) { Unsigned(value, 8); return *this; }

	inline void StateWriter::Begin(const char* tag)
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

	inline void StateWriter::End()
	{
		uint32_t length = (uint32_t)(image.size() - sectionStart);

		for (int i = 0; i < 4; i++)
		{
			image[lengthOffset + i] = (uint8_t)(length >> (i * 8));
		}
	}

	inline StateWriter& StateWriter::Raw(const void* data, size_t size)
	{
		if (data != nullptr && size != 0)
		{
			const uint8_t* bytes = (const uint8_t*)data;
			image.insert(image.end(), bytes, bytes + size);
		}

		return *this;
	}

	inline StateWriter& StateWriter::Bytes(const std::vector<uint8_t>& values)
	{
		U32((uint32_t)values.size());
		return Raw(values.empty() ? nullptr : values.data(), values.size());
	}

	inline StateWriter& StateWriter::Text(const std::string& text)
	{
		U32((uint32_t)text.size());
		return Raw(text.empty() ? nullptr : text.data(), text.size());
	}

	inline void StateReader::Fail(const std::string& reason)
	{
		if (!failed)
		{
			failed = true;
			error = reason;
		}
	}

	inline uint64_t StateReader::Unsigned(size_t width)
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

	inline uint8_t StateReader::U8() { return (uint8_t)Unsigned(1); }
	inline uint16_t StateReader::U16() { return (uint16_t)Unsigned(2); }
	inline uint32_t StateReader::U32() { return (uint32_t)Unsigned(4); }
	inline uint64_t StateReader::U64() { return Unsigned(8); }

	inline StateReader& StateReader::Raw(void* data, size_t size)
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

	inline StateReader& StateReader::Bytes(std::vector<uint8_t>& values)
	{
		uint32_t count = U32();

		if (failed)
		{
			return *this;
		}

		if (count > (64u << 20))
		{
			Fail("a byte block is larger than any state this emulator writes");
			return *this;
		}

		values.resize(count);
		return Raw(values.empty() ? nullptr : values.data(), values.size());
	}

	inline StateReader& StateReader::Text(std::string& text)
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

	inline bool StateReader::PeekTag(char tag[5])
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

	inline bool StateReader::Begin(const char* tag)
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

	inline bool StateReader::End()
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

	inline bool StateReader::Skip()
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
}
