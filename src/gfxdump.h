/*

# GFX Command Dump

The debug utility that records the graphics command stream as it enters the Transform Unit and keeps
the last frames of it, so that a picture that came out wrong can be looked at as the sequence of
commands that produced it (`gfxdump`).

## Where the recording starts

The CP produces the command stream, but its own state (the VCD / VAT registers, the attribute arrays,
the FIFO pointers) is not part of that stream: everything the CP emits leaves it towards the XF, which
is the entry point of the pipeline (wiki/gfx.md, "Command path"). The dump therefore starts where the
XF starts: it records the XF register block loads, the register reads, the bypass (BP) register words
the XF forwards to the SU and the vertex rows of every draw command, and nothing of what the CP did to
produce them ("the CP is the producer, not the subject").

## The frames

The dump is cut into frames by the GFX frame counter, which is the frame the pipeline presented, so a
frame of the dump is the picture the console showed. The history is a ring of `MaxFrames` frames: the
oldest one is dropped when a new one arrives, and a frame that would pass `FrameByteLimit` is cut off
where it stands (the frame is then marked as truncated and its last, incomplete command is left out of
the decoded listing).

The frame in progress is not part of the ring yet: it is published when the next frame starts. The
debugger shows its size from a small progress record, which is the only thing a reader may touch
while the emulation thread keeps appending to it.

## The stream

The stream of a frame is a self-describing sequence of big-endian 32-bit words, so that a dump written
to a file can be read back by anything and does not depend on the host's word order or on the struct
layout of this build. Every command starts with a tag word that carries its kind (`GfxDumpOp`) and the
small field of the command (`count - 1` for a block write, the register for a read, the primitive for
a draw); the words that follow are the command itself:

- `RegLoad` - the XF address, then the data words;
- `RegRead` - nothing (the register is in the tag);
- `SuReg` - the value, then the write mask of the BP mask register that preceded it;
- `Draw` - the vertex count, then the vertex rows, 32 words each (position, normal, binormal, tangent,
  two colours, eight texture coordinate pairs and the two matrix index words).

A vertex row carries every attribute the hardware has, and a draw only supplies the ones its format
names: the decoded listing prints the attributes the vertex specification (XF_INVTXSPEC) of the
draw's state allows and leaves the rest to the raw stream, which holds the row whole. `gfxdump save`
writes a listing where every field is printed.

## The RAM slices

Some of the commands cannot be executed without the guest RAM they read, and that RAM is not part of
the stream: a draw samples a texture (and, for a paletted one, the TLUT) that lives in main memory and
was programmed by a register write long before it. The texture engine hands every such read to the
dump (`RamSlice`), and the bytes are attached to the frame that needed them, so that a dump is
self-contained: a texture the emulation thread overwrites in the next frame is still in the dump of
the frame that sampled it. The slices travel to the file next to the stream (`gfxdump save`).

## The commands

- `gfxdump [frames]` - the capture state and the frames the history holds (the "Frames" tab);
- `gfxdump commands [id]` - the decoded commands of one frame (the "Commands" tab);
- `gfxdump hex [id]` - the raw stream and the RAM slices of one frame (the "Hex" tab);
- `gfxdump capture [on|off|toggle]` - the capture button;
- `gfxdump select <id>` - the frame the panel looks at;
- `gfxdump clear` - drop the history;
- `gfxdump save [id]` - write the frame into the session folder: the stream, the RAM slices and the
  decoded listing (the panel shows the beginning of what those files hold in full).

The panels of the debugger are filled from these commands, one tab per view, so the very same report
is available from the command line. The "capture" and "select" answers are the frame list, which is
what makes the buttons of the panel work: an item that the Markdown writes as a `cmd:` link is drawn
as a button and its click runs the command (`debugui2.h`).

*/

#pragma once

#include <memory>

namespace GFX
{
	// The kind of a record of the dumped stream. The value is the tag byte of the record, so it is
	// part of the dump format and must not be renumbered.
	enum class GfxDumpOp : uint32_t
	{
		RegLoad = 1,		//!< xf_cmd_regload: a block write into the XF register space
		RegRead = 2,		//!< xf_cmd_regread: a read of one XF register
		SuReg = 3,			//!< a bypass register word, forwarded by the XF to the SU
		Draw = 4,			//!< a draw command, with the vertex rows that follow it
	};

	// One command of the dumped stream: where it starts, how long it is and the fields its tag does
	// not carry, so that a reader does not have to walk the stream to list the commands.
	struct GfxDumpRecord
	{
		GfxDumpOp op = GfxDumpOp::RegLoad;
		uint32_t offset = 0;		//!< byte offset of the record in the frame stream
		uint32_t size = 0;			//!< its size in bytes, tag included
		uint32_t a = 0;				//!< the first field: the XF register, the BP register, the primitive
		uint32_t b = 0;				//!< the second one: the word count, the value, the vertex count
		uint32_t c = 0;				//!< and the third: the write mask
	};

	// A slice of guest RAM a command of the frame was executed with: the texture a draw samples, or
	// the palette of a paletted one. The bytes are a copy, so the slice keeps what the frame saw even
	// though the guest goes on writing to the RAM it came from.
	struct GfxDumpSlice
	{
		uint32_t address = 0;
		std::string note;
		std::vector<uint8_t> data;
	};

	// The stream of one GFX frame, with the commands it was cut into and the RAM it needed.
	struct GfxDumpFrame
	{
		int id = 0;							//!< the ordinal of the frame in the dump (the debugger selects by this)
		int frame = 0;						//!< the value of the GFX frame counter the frame was recorded at
		std::vector<uint8_t> stream;		//!< the raw stream, big-endian 32-bit words
		std::vector<GfxDumpRecord> records;
		std::vector<GfxDumpSlice> slices;
		bool truncated = false;				//!< the frame passed the byte limit and was cut off
	};

	class GfxDump
	{
	public:
		// How much of one frame is kept, and how many frames the history holds. The dump lives in
		// memory, so both are a bound on what the debugger costs the emulator.
		static const size_t MaxFrames = 12;
		static const size_t FrameByteLimit = 8 * 1024 * 1024;

		// What the debugger shows in the frame list.
		struct FrameInfo
		{
			int id = 0;
			int frame = 0;
			size_t records = 0;
			size_t bytes = 0;
			size_t slices = 0;
			size_t sliceBytes = 0;
			bool truncated = false;
		};

		// The frame being recorded, as a reader may see it. Only ever a size, never the stream.
		struct Progress
		{
			int frame = 0;
			size_t records = 0;
			size_t bytes = 0;
		};

	private:
		// ---- the recording side: only the emulation thread touches these ----

		GfxDumpFrame current;
		bool currentOpen = false;
		size_t loadRemaining = 0;			//!< data words the block write in progress still expects
		size_t verticesRemaining = 0;		//!< vertex rows the draw in progress still expects
		int lastId = 0;

		GfxDumpFrame& Current();
		void Flush();
		bool Active();
		void ResetCurrent();

		// ---- the control: the debugger writes these, the emulation thread applies them ----

		// The frame in progress belongs to the emulation thread, which is the only one that may
		// close it: the debugger only posts a request here and the next command applies it. A
		// mis-timed read of one of these flags costs a frame or two of recording, no more.
		bool capturing = false;
		bool stopPending = false;
		bool clearPending = false;

		// ---- the published history ----

		SpinLock lock;
		std::vector<std::shared_ptr<const GfxDumpFrame>> frames;
		int selected = -1;					//!< the frame the debugger looks at (-1: the newest one)

		Progress progress;

	public:
		// -------------------------------------------------------------------------------------
		// The recording hooks. The XF calls the first seven as the CP pushes the stream into it
		// (xf.cpp, "CP -> XF interface"); the texture engine calls the last one when it reads the
		// RAM a draw samples (tx.cpp).
		// -------------------------------------------------------------------------------------

		void RegLoadBegin(size_t startIdx, size_t amount);
		void RegLoadData(uint32_t value);
		void RegRead(size_t index);
		void SuRegWrite(size_t index, uint32_t value, uint32_t mask);
		void DrawBegin(RAS_Primitive prim, size_t vtxNum);
		void DrawVertex(const Vertex* v);
		void DrawEnd();

		//! Attach a slice of guest RAM to the frame being recorded. `address` is the physical
		//! address the slice was read from and `note` names what the bytes are.
		void RamSlice(uint32_t address, const void* data, size_t size, const char* note);

		// -------------------------------------------------------------------------------------
		// The control the debugger has over the dump.
		// -------------------------------------------------------------------------------------

		bool Capturing() { return capturing; }
		void SetCapturing(bool on);

		//! Drop the whole history and the frame in progress.
		void Clear();

		//! The frame the panel looks at (-1 selects the newest one).
		void Select(int id) { selected = id; }
		int Selected() { return selected; }

		// -------------------------------------------------------------------------------------
		// What the debugger reads. A reader gets the frames the history holds and may format one
		// for as long as it holds the pointer: a published frame is never touched again.
		// -------------------------------------------------------------------------------------

		void FrameInfos(std::vector<FrameInfo>& out);
		std::shared_ptr<const GfxDumpFrame> Frame(int id);
		Progress InProgress();

		// -------------------------------------------------------------------------------------
		// The JDI commands of the dump (the GFX node).
		// -------------------------------------------------------------------------------------

		static void Reflector();
	};

	//! The dump of the running GFX subsystem, or nullptr when there is no machine.
	extern GfxDump* gfx_dump;
}
