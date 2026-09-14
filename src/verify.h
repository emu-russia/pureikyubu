#pragma once

// Input verifiers.
//
// Everything the emulator takes from the outside world is checked here before a single byte of
// it is used as an address, a length or an index: the settings JSON (json.cpp), the executable
// images (main.cpp), the disc images (dvd.cpp, bootrtc.cpp), the memory card saves (memcard.cpp),
// the ROM dumps (bootrtc.cpp, dsp.cpp), the DMA engines fed by guest code (memcard.cpp,
// bootrtc.cpp, dsparam.cpp, dspdma.cpp, pi.cpp), the console scripts (debug.cpp) and the symbol
// maps (sym.cpp).
//
// The predicates are overflow-safe on purpose: they never add two untrusted values in a width
// that can wrap, so a field of 0xFFFFFFFF cannot defeat them. They are also the single place
// where the valid range of each artifact's field is written down - a caller that wants a
// different range is a bug, not a special case. testing/security_test.cpp pins every rule down.

#include <cstdint>
#include <cstddef>

namespace Verify
{
	// ---------------------------------------------------------------------------------------
	// Generic, overflow-safe range test
	// ---------------------------------------------------------------------------------------

	/// <summary>
	/// [offset, offset + size) lies inside [0, limit). The comparison never adds the two
	/// untrusted values: it subtracts instead, so no combination of 64-bit inputs can wrap it.
	/// </summary>
	inline bool Range(uint64_t offset, uint64_t size, uint64_t limit)
	{
		return size <= limit && offset <= (limit - size);
	}

	// The MI address decode used by the loaders (26 address bits): an address is translated to
	// physical memory by masking these bits off, exactly as MIGetMemoryPointerFor* does.
	const uint32_t MainMemoryMask = 0x03ff'ffff;

	/// <summary>
	/// A physical address window inside the emulated main memory. `physAddr` is masked the way
	/// the MI decodes it, then the whole [phys, phys + size) window has to fit in the RAM the
	/// memory interface actually allocated (24 or 48 MB, never the 64 MB the mask allows).
	/// This is the verifier for every DMA destination and every image section.
	/// </summary>
	inline bool MainMemory(uint32_t physAddr, uint64_t size, uint64_t ramSize)
	{
		return Range((uint64_t)(physAddr & MainMemoryMask), size, ramSize);
	}

	// The same window, for a caller that has already masked the address off (used when the
	// pointer was obtained from an MI accessor and only the length is left to check).
	inline bool MainMemoryRange(uint32_t physAddr, uint64_t size, uint64_t ramSize)
	{
		return Range((uint64_t)physAddr, size, ramSize);
	}

	// ---------------------------------------------------------------------------------------
	// Executable images (DOL / ELF)
	// ---------------------------------------------------------------------------------------

	/// <summary>
	/// One loaded section of an executable image. The section has to be present in the file
	/// ([fileOffset, fileOffset + size) inside the file) and it has to fit in main memory when
	/// translated to physical RAM. Both halves are needed: a section can be copied from a valid
	/// file offset to an address outside RAM (an overflow of the RAM allocation), or from past
	/// the end of a short file into valid RAM (the stream just returns what it has).
	/// </summary>
	inline bool ImageSection(uint64_t imageSize, uint64_t fileOffset, uint64_t size,
		uint32_t address, uint64_t ramSize)
	{
		return Range(fileOffset, size, imageSize) && MainMemory(address, size, ramSize);
	}

	// ---------------------------------------------------------------------------------------
	// Disc images (GCM / ISO / RVZ)
	// ---------------------------------------------------------------------------------------

	/// <summary>
	/// A DVD read of `length` bytes at `position`. The seek value carries the sign of the guest
	/// register it was derived from, so it is 64-bit and negative seeks are rejected: the old
	/// code only compared the start (`seekval >= size`), which a negative value passes, and then
	/// computed the length in a mix of int and size_t that wrapped into a multi-gigabyte read.
	/// </summary>
	inline bool DiscRead(int64_t position, uint64_t length, int64_t imageSize)
	{
		if (position < 0 || imageSize < 0)
		{
			return false;
		}

		return Range((uint64_t)position, length, (uint64_t)imageSize);
	}

	/// <summary>
	/// The root entry of a disc file system table. `nextOffset` counts FST entries (12 bytes
	/// each) and doubles as the size of the whole table, so it must describe a table that fits
	/// in the buffer that was read from the image - the old code used it as a subscript with no
	/// bound at all and byte-swapped its way through the heap.
	/// </summary>
	inline bool FstRoot(uint64_t fstSize, uint32_t nextOffset)
	{
		const uint64_t entrySize = 12;

		return nextOffset >= 1 && Range(0, (uint64_t)nextOffset * entrySize, fstSize);
	}

	/// <summary>
	/// An FST entry index. Valid indices are 0 .. entryCount-1, where entryCount is derived from
	/// the *validated* table size.
	/// </summary>
	inline bool FstEntry(uint64_t entryCount, uint64_t index)
	{
		return index < entryCount;
	}

	/// <summary>
	/// A file name inside the FST name table. The name table starts right after the last entry
	/// and has no terminator of its own, so a name offset has to leave at least one byte before
	/// the end of the table.
	/// </summary>
	inline bool FstName(uint64_t nameTableSize, uint64_t nameOffset)
	{
		return nameOffset < nameTableSize;
	}

	// ---------------------------------------------------------------------------------------
	// Memory cards (EXI devices)
	// ---------------------------------------------------------------------------------------

	/// <summary>
	/// A window inside a memory card image. The transfer length comes from the EXI_LEN register,
	/// which the guest writes without a mask (up to 0xFFFFFFFF), so the length itself has to be
	/// checked first - not just its end - and the end check has to be 64-bit, because the old
	/// `offset >= cardSize + size` wrapped in 32 bits and passed for huge lengths.
	/// </summary>
	inline bool MemcardWindow(uint64_t cardSize, uint64_t offset, uint64_t length)
	{
		return Range(offset, length, cardSize);
	}

	// ---------------------------------------------------------------------------------------
	// Console scripts (autoexec.cmd and the debugger's `script` command)
	// ---------------------------------------------------------------------------------------

	/// <summary>
	/// Copy one line out of an in-memory script file into `line`.
	/// On entry `position` is the offset of the first byte of the line (which may be a '\n');
	/// on return it points one byte past the line's terminator. The copy always stops at the end
	/// of the buffer, at a NUL and at end of line, and the result is always NUL-terminated, so a
	/// script without a trailing newline, a script with an embedded NUL and a script with a line
	/// longer than `lineSize` are all handled without reading or writing past either buffer.
	/// An over-long line is not executed in pieces: the tail past the buffer is skipped and
	/// `truncated` is set, so the caller can report it and move on to the next line.
	/// Returns false when the end of the script has been reached.
	/// </summary>
	inline bool ScriptLine(const uint8_t* data, size_t size, size_t& position, char* line, size_t lineSize, bool& truncated)
	{
		truncated = false;

		if (data == nullptr || line == nullptr || lineSize == 0)
		{
			return false;
		}

		while (position < size && data[position] == '\n')
		{
			position++;
		}

		if (position >= size)
		{
			line[0] = 0;
			return false;
		}

		size_t written = 0;
		bool skipping = false;

		while (position < size)
		{
			uint8_t c = data[position];

			if (c == 0)
			{
				// A NUL ends the script: the bytes behind it are not a command any more (the old
				// reader copied it into the line and then kept reading past the end of the file).
				position = size;
				break;
			}

			if (c == '\n')
			{
				position++;			// consume the terminator too
				break;
			}

			if (written + 1 >= lineSize)
			{
				// The line does not fit. Drop the rest of it rather than writing past the
				// caller's buffer or running a fragment of the command.
				skipping = true;
			}

			if (skipping)
			{
				truncated = true;
			}
			else
			{
				line[written++] = (char)c;
			}

			position++;
		}

		// An over-long line is not a command: hand the caller an empty line (which the trim
		// rejects) and let the `truncated` flag say what happened.
		line[truncated ? 0 : written] = 0;

		return true;
	}

	/// <summary>
	/// Trim the trailing whitespace of a script line in place and return the first non-blank
	/// character. An empty (or all-blank) line returns nullptr: the old code walked a pointer
	/// below the start of the buffer in that case and wrote a NUL out of bounds.
	/// </summary>
	inline char* ScriptTrim(char* line)
	{
		if (line == nullptr)
		{
			return nullptr;
		}

		size_t length = 0;

		while (line[length] != 0)
		{
			length++;
		}

		while (length > 0 && (uint8_t)line[length - 1] <= ' ')
		{
			line[--length] = 0;
		}

		char* p = line;

		while (*p != 0 && (uint8_t)*p <= ' ')
		{
			p++;
		}

		return (*p == 0) ? nullptr : p;
	}
}
