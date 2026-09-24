// Save states of the integrated GBA emulator: the state format and its two cursors.
//
// A save state is the whole machine in a byte image: the CPU (every register and every banked
// shadow of every mode), the memory (EWRAM, IWRAM, the palette, VRAM, OAM, the raw I/O file), the
// devices (the PPU with its registers and its timing, the APU with its channels and its FIFOs, the
// DMA channels, the timers, the serial port, the interrupt controller, the keypad) and the
// cartridge's save memory together with the state machines of the Flash, the EEPROM and the RTC.
// The cartridge *ROM* is not in it: a state belongs to the image it was taken from, and the image
// is identified by its title, its game code and its size instead (see `ChunkMeta`).
//
// The image is a header, a checksum of the payload, and then one *section* per subsystem:
//
//   0x00  magic   "PSAVEST" + NUL
//   0x08  u32     the format version (`StateFormatVersion`)
//   0x0C  u32     reserved (0)
//   0x10  u64     the size of the payload
//   0x18  u64     the FNV-1a 64 checksum of the payload
//   0x20  payload: sections
//
// The format is shared by the two machines of this module, so the first field of the first
// section says which one the state belongs to (`StateMachine`): a Game Boy state offered to a
// Game Boy Advance (or the other way round) is refused with that name rather than read as
// nonsense.
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
// for a `std::vector` and `Raw` for a block of memory. A read that runs off the end of the image
// latches a failure and every later read answers zero, so a truncated or foreign image is refused
// by the caller and never becomes a machine.
//
// The module is self contained (see src/gba/Readme.md): the cursors are the C++ standard library
// and nothing else, so the save states build and are tested without the GameCube side.

#pragma once

#include "gba_types.h"

#include <string>
#include <type_traits>

namespace GBA
{
	/// <summary>
	/// The version of the state format. A reader refuses an image whose version it does not know;
	/// the version is raised whenever a section's layout changes in a way an older reader would
	/// misread (adding a member to a device is enough - the section length check would catch it,
	/// but the version makes the reason readable).
	/// </summary>
	const uint32_t StateFormatVersion = 1;

	/// <summary>The eight bytes every state image starts with ("PSAVEST" and a NUL).</summary>
	extern const uint8_t StateMagic[8];

	/// <summary>The size of the image header (`StateHeaderSize` bytes before the first section).</summary>
	const size_t StateHeaderSize = 0x20;

	/// <summary>
	/// The save state slots a frontend offers (`.st0` .. `.st9`). Both machines use the same ten,
	/// and the quick save keys step through them.
	/// </summary>
	const int MaxStateSlot = 9;

	/// <summary>
	/// The machine a state was taken from. It is the first field of the first section: both
	/// machines of this module write the same image format, and neither can read the other's.
	/// </summary>
	enum class StateMachine : uint8_t
	{
		Gba = 1,			// the Game Boy Advance (src/gba/gba_*.cpp, arm7tdmi.cpp)
		GameBoy = 2,		// the Game Boy / Game Boy Color (src/gba/gb_*.cpp)
	};

	/// <summary>The name of a machine kind, for the reports and the error messages.</summary>
	const char* StateMachineName(StateMachine machine);

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
			else if constexpr (std::is_same_v<T, bool>)
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
				// significant byte first, exactly like an integer of the same width. The Game
				// Boy's high pass filter keeps its two capacitors as doubles and they are part of
				// the machine's sound state, so they have to travel with it.
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

		template <typename T>
		StateReader& Values(std::vector<T>& values)
		{
			uint32_t count = U32();

			if (failed)
				return *this;

			// A length that cannot have come from this machine is a broken image: the vectors it
			// holds are the APU's pending samples and the like, never megabytes of them.
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
			else if constexpr (std::is_same_v<T, bool>)
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
	uint64_t StateChecksum(const uint8_t* data, size_t size);

	/// <summary>
	/// Write the image header in front of the payload `StateWriter` built. The writer holds the
	/// sections only; this is what makes it a file. `out` is the complete image.
	/// </summary>
	void StateWrap(const std::vector<uint8_t>& payload, std::vector<uint8_t>& out);

	/// <summary>
	/// Check the header of a candidate image and point `payload`/`payloadSize` at its sections.
	/// Answers false and fills `error` when the image is not one of ours, is a format this build
	/// does not know, or does not match its own checksum.
	/// </summary>
	bool StateUnwrap(const uint8_t* image, size_t size, const uint8_t** payload, size_t* payloadSize,
		std::string& error);
}
