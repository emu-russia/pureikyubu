// The GFX command dump. The module description is in gfxdump.h.

#include "pch.h"

using namespace Debug;

namespace GFX
{
	// The dump of the running GFX subsystem, or nullptr when there is no machine.
	GfxDump* gfx_dump = nullptr;

	// One vertex row of the stream, in 32-bit words (see gfxdump.h, "The stream"): the position,
	// the normal, the binormal, the tangent, the two colours, the eight texture coordinate pairs and
	// the two matrix index words.
	static const size_t VertexWords = 32;

	// How much of a frame the panels show. The panel is redrawn from its Markdown every refresh, so
	// the text it is given is what the front end lays out and measures once per frame: the beginning
	// of the frame is shown and the files `gfxdump save` writes hold the rest.
	static const size_t ViewCommands = 2000;		//!< command lines
	static const size_t ViewHexBytes = 8192;		//!< bytes of the stream
	static const size_t ViewVertices = 64;			//!< vertex rows of one draw
	static const size_t ViewSliceBytes = 1024;		//!< bytes of one RAM slice
	static const size_t ViewSlices = 8;				//!< RAM slices


	// ========================================================================================
	// The stream
	//
	// Every word of it is big-endian, so that a dump written to a file does not depend on the host
	// (see gfxdump.h). The tag of a record carries its kind and the small field of the command, and
	// the record list the frame keeps says where each command starts, so nothing has to be decoded
	// to walk the stream.
	// ========================================================================================

	static uint32_t Tag(GfxDumpOp op, uint32_t field)
	{
		return ((uint32_t)op << 24) | (field & 0xFFFFFF);
	}

	static void PutWord(std::vector<uint8_t>& stream, uint32_t word)
	{
		stream.push_back((uint8_t)(word >> 24));
		stream.push_back((uint8_t)(word >> 16));
		stream.push_back((uint8_t)(word >> 8));
		stream.push_back((uint8_t)word);
	}

	static uint32_t Word(const std::vector<uint8_t>& stream, size_t offset)
	{
		return ((uint32_t)stream[offset] << 24) | ((uint32_t)stream[offset + 1] << 16) |
			((uint32_t)stream[offset + 2] << 8) | (uint32_t)stream[offset + 3];
	}

	// The word of a record, zero when the record does not really hold that much: a frame that was
	// cut off, or a capture that started in the middle of a block write, leaves a record that claims
	// more words than the stream has.
	static uint32_t WordAt(const GfxDumpFrame& frame, size_t offset, size_t limit)
	{
		if (offset + 4 > limit || offset + 4 > frame.stream.size())
			return 0;

		return Word(frame.stream, offset);
	}

	static uint32_t FloatBits(float value)
	{
		uint32_t bits;
		memcpy(&bits, &value, sizeof(bits));
		return bits;
	}

	static float BitsFloat(uint32_t bits)
	{
		float value;
		memcpy(&value, &bits, sizeof(value));
		return value;
	}

	static void PutFloat(std::vector<uint8_t>& stream, float value)
	{
		PutWord(stream, FloatBits(value));
	}

	// One vertex row: the words the XF receives for the vertex (the CP has already fetched and
	// unpacked the attributes, see gfxdump.h).
	static void PutVertex(std::vector<uint8_t>& stream, const Vertex* v)
	{
		for (int i = 0; i < 3; i++) PutFloat(stream, v->Position[i]);
		for (int i = 0; i < 3; i++) PutFloat(stream, v->Normal[i]);
		for (int i = 0; i < 3; i++) PutFloat(stream, v->Binormal[i]);
		for (int i = 0; i < 3; i++) PutFloat(stream, v->Tangent[i]);
		PutWord(stream, v->Col[0].RGBA);
		PutWord(stream, v->Col[1].RGBA);
		for (int t = 0; t < 8; t++)
		{
			PutFloat(stream, v->TexCoord[t][0]);
			PutFloat(stream, v->TexCoord[t][1]);
		}
		PutWord(stream, v->matIdx0.bits);
		PutWord(stream, v->matIdx1.bits);
	}


	// ========================================================================================
	// The recording
	// ========================================================================================

	GfxDumpFrame& GfxDump::Current()
	{
		// The frame boundary is the GFX frame counter (`gfx.h`): the frames of the dump are the
		// frames the pipeline presented, so a command that arrives after one was presented starts
		// the next one.
		if (currentOpen && current.frame == gfx_frame_counter)
			return current;

		Flush();

		current = GfxDumpFrame{};
		current.frame = gfx_frame_counter;
		currentOpen = true;
		progress.frame = current.frame;
		progress.records = 0;
		progress.bytes = 0;

		return current;
	}

	void GfxDump::Flush()
	{
		if (!currentOpen)
			return;

		currentOpen = false;

		// A frame that recorded nothing (a capture switched on and off between two frames) is not
		// worth a place in the history.
		if (current.records.empty())
		{
			current = GfxDumpFrame{};
			return;
		}

		current.id = ++lastId;

		std::shared_ptr<const GfxDumpFrame> frame = std::make_shared<const GfxDumpFrame>(std::move(current));
		current = GfxDumpFrame{};

		lock.Lock();
		frames.push_back(frame);

		while (frames.size() > MaxFrames)
			frames.erase(frames.begin());

		lock.Unlock();
	}

	// The emulation thread's side of the control flags: what the debugger posts is applied here,
	// where the frame in progress belongs to this thread alone (see gfxdump.h).
	bool GfxDump::Active()
	{
		if (clearPending)
		{
			clearPending = false;
			ResetCurrent();
		}

		if (stopPending)
		{
			stopPending = false;
			Flush();
		}

		return capturing;
	}

	void GfxDump::ResetCurrent()
	{
		current = GfxDumpFrame{};
		currentOpen = false;
		loadRemaining = 0;
		verticesRemaining = 0;
		progress = Progress{};
	}

	void GfxDump::RegLoadBegin(size_t startIdx, size_t amount)
	{
		if (!Active() || amount == 0)
			return;

		GfxDumpFrame& frame = Current();

		loadRemaining = 0;

		if (frame.truncated)
			return;

		// The whole block has to fit (see "the incomplete command" note in gfxdump.h).
		if ((frame.stream.size() + (2 + amount) * 4) > FrameByteLimit)
		{
			frame.truncated = true;
			return;
		}

		GfxDumpRecord record;
		record.op = GfxDumpOp::RegLoad;
		record.offset = (uint32_t)frame.stream.size();
		record.size = (uint32_t)((2 + amount) * 4);
		record.a = (uint32_t)startIdx;
		record.b = (uint32_t)amount;
		frame.records.push_back(record);

		PutWord(frame.stream, Tag(GfxDumpOp::RegLoad, (uint32_t)amount - 1));
		PutWord(frame.stream, (uint32_t)startIdx);

		loadRemaining = amount;
		progress.records = frame.records.size();
		progress.bytes = frame.stream.size();
	}

	void GfxDump::RegLoadData(uint32_t value)
	{
		if (!Active())
			return;

		GfxDumpFrame& frame = Current();

		// A data word that does not belong to a block write in progress would break the stream: the
		// XF answers such a word with a report of its own (see TransformUnit::CPRegLoadData).
		if (frame.truncated || loadRemaining == 0)
			return;

		loadRemaining--;
		PutWord(frame.stream, value);
		progress.bytes = frame.stream.size();
	}

	void GfxDump::RegRead(size_t index)
	{
		if (!Active())
			return;

		GfxDumpFrame& frame = Current();

		if (frame.truncated || (frame.stream.size() + 4) > FrameByteLimit)
		{
			frame.truncated = true;
			return;
		}

		GfxDumpRecord record;
		record.op = GfxDumpOp::RegRead;
		record.offset = (uint32_t)frame.stream.size();
		record.size = 4;
		record.a = (uint32_t)index;
		frame.records.push_back(record);

		PutWord(frame.stream, Tag(GfxDumpOp::RegRead, (uint32_t)index));
		progress.records = frame.records.size();
		progress.bytes = frame.stream.size();
	}

	void GfxDump::SuRegWrite(size_t index, uint32_t value, uint32_t mask)
	{
		if (!Active())
			return;

		GfxDumpFrame& frame = Current();

		if (frame.truncated || (frame.stream.size() + 12) > FrameByteLimit)
		{
			frame.truncated = true;
			return;
		}

		GfxDumpRecord record;
		record.op = GfxDumpOp::SuReg;
		record.offset = (uint32_t)frame.stream.size();
		record.size = 12;
		record.a = (uint32_t)index;
		record.b = value;
		record.c = mask;
		frame.records.push_back(record);

		PutWord(frame.stream, Tag(GfxDumpOp::SuReg, (uint32_t)index));
		PutWord(frame.stream, value);
		PutWord(frame.stream, mask);
		progress.records = frame.records.size();
		progress.bytes = frame.stream.size();
	}

	void GfxDump::DrawBegin(RAS_Primitive prim, size_t vtxNum)
	{
		if (!Active())
			return;

		GfxDumpFrame& frame = Current();

		verticesRemaining = 0;

		if (frame.truncated)
			return;

		if ((frame.stream.size() + (2 + vtxNum * VertexWords) * 4) > FrameByteLimit)
		{
			frame.truncated = true;
			return;
		}

		GfxDumpRecord record;
		record.op = GfxDumpOp::Draw;
		record.offset = (uint32_t)frame.stream.size();
		record.size = (uint32_t)((2 + vtxNum * VertexWords) * 4);
		record.a = (uint32_t)prim;
		record.b = (uint32_t)vtxNum;
		frame.records.push_back(record);

		PutWord(frame.stream, Tag(GfxDumpOp::Draw, (uint32_t)prim));
		PutWord(frame.stream, (uint32_t)vtxNum);

		verticesRemaining = vtxNum;
		progress.records = frame.records.size();
		progress.bytes = frame.stream.size();
	}

	void GfxDump::DrawVertex(const Vertex* v)
	{
		if (!Active() || v == nullptr)
			return;

		GfxDumpFrame& frame = Current();

		if (frame.truncated || verticesRemaining == 0)
			return;

		verticesRemaining--;
		PutVertex(frame.stream, v);
		progress.bytes = frame.stream.size();
	}

	void GfxDump::DrawEnd()
	{
		// The end of a draw only closes the vertex counter; a draw that was cut off is left as it is,
		// the frame is already marked as truncated.
		verticesRemaining = 0;
	}

	void GfxDump::RamSlice(uint32_t address, const void* data, size_t size, const char* note)
	{
		if (!Active() || data == nullptr || size == 0)
			return;

		// Only a reader that asked for the frame may see the slices, and it sees a published frame,
		// so the slice list of the frame in progress belongs to this thread.
		GfxDumpFrame& frame = Current();

		if (frame.truncated)
			return;

		// A texture a title programs once and draws every frame is attached once per frame, and the
		// same address may be read several times within one (a map that is bound twice).
		for (size_t i = 0; i < frame.slices.size(); i++)
		{
			if (frame.slices[i].address == address && frame.slices[i].data.size() == size)
				return;
		}

		if ((frame.stream.size() + size) > FrameByteLimit)
		{
			frame.truncated = true;
			return;
		}

		GfxDumpSlice slice;
		slice.address = address;
		slice.note = (note != nullptr) ? note : "";
		slice.data.assign((const uint8_t*)data, (const uint8_t*)data + size);

		frame.slices.push_back(std::move(slice));
	}


	// ========================================================================================
	// The control
	// ========================================================================================

	void GfxDump::SetCapturing(bool on)
	{
		if (on == capturing)
			return;

		capturing = on;

		if (on)
		{
			// The frame the recording starts in may be half a block write old: nothing of it may be
			// recorded, or the stream would start in the middle of a command.
			clearPending = true;
		}
		else
		{
			stopPending = true;
		}
	}

	void GfxDump::Clear()
	{
		lock.Lock();
		frames.clear();
		lock.Unlock();

		selected = -1;
		clearPending = true;
	}


	// ========================================================================================
	// What the debugger reads
	// ========================================================================================

	void GfxDump::FrameInfos(std::vector<FrameInfo>& out)
	{
		out.clear();

		lock.Lock();

		for (size_t i = 0; i < frames.size(); i++)
		{
			const GfxDumpFrame& frame = *frames[i];

			FrameInfo info;
			info.id = frame.id;
			info.frame = frame.frame;
			info.records = frame.records.size();
			info.bytes = frame.stream.size();
			info.slices = frame.slices.size();
			info.truncated = frame.truncated;

			for (size_t s = 0; s < frame.slices.size(); s++)
				info.sliceBytes += frame.slices[s].data.size();

			out.push_back(info);
		}

		lock.Unlock();
	}

	std::shared_ptr<const GfxDumpFrame> GfxDump::Frame(int id)
	{
		std::shared_ptr<const GfxDumpFrame> result;

		lock.Lock();

		// The id of a frame never comes back (the ring moves forward), so a selection that is not in
		// the history anymore falls back to the newest frame instead of showing nothing.
		for (size_t i = 0; i < frames.size(); i++)
		{
			if (id < 0 || frames[i]->id == id)
				result = frames[i];
		}

		lock.Unlock();

		return result;
	}

	GfxDump::Progress GfxDump::InProgress()
	{
		return progress;
	}


	// ========================================================================================
	// The decoded stream
	// ========================================================================================

	static std::string FormatSize(size_t bytes)
	{
		char text[0x40];

		if (bytes >= (1024 * 1024))
			sprintf(text, "%.2f MB", (double)bytes / (1024.0 * 1024.0));
		else if (bytes >= 1024)
			sprintf(text, "%.1f KB", (double)bytes / 1024.0);
		else
			sprintf(text, "%zi bytes", bytes);

		return text;
	}

	struct NameEntry
	{
		size_t index;
		const char* name;
	};

	static const NameEntry XfRegNames[] =
	{
		{ 0x1000, "XF_ERROR" },
		{ 0x1001, "XF_DIAGNOSTICS" },
		{ 0x1002, "XF_STATE0" },
		{ 0x1003, "XF_STATE1" },
		{ 0x1004, "XF_CLOCK" },
		{ 0x1005, "XF_CLIP_DISABLE" },
		{ 0x1006, "XF_PERF0" },
		{ 0x1007, "XF_PERF1" },
		{ 0x1008, "XF_INVTXSPEC" },
		{ 0x1009, "XF_NUMCOLS" },
		{ 0x100A, "XF_AMBIENT0" },
		{ 0x100B, "XF_AMBIENT1" },
		{ 0x100C, "XF_MATERIAL0" },
		{ 0x100D, "XF_MATERIAL1" },
		{ 0x100E, "XF_COLOR0CNTL" },
		{ 0x100F, "XF_COLOR1CNTL" },
		{ 0x1010, "XF_ALPHA0CNTL" },
		{ 0x1011, "XF_ALPHA1CNTL" },
		{ 0x1012, "XF_DUALTEX" },
		{ 0x1018, "XF_MATINDEX_A" },
		{ 0x1019, "XF_MATINDEX_B" },
		{ 0x101A, "XF_VIEWPORT_SCALE_X" },
		{ 0x101B, "XF_VIEWPORT_SCALE_Y" },
		{ 0x101C, "XF_VIEWPORT_SCALE_Z" },
		{ 0x101D, "XF_VIEWPORT_OFFSET_X" },
		{ 0x101E, "XF_VIEWPORT_OFFSET_Y" },
		{ 0x101F, "XF_VIEWPORT_OFFSET_Z" },
		{ 0x1020, "XF_PROJECTION_A" },
		{ 0x1021, "XF_PROJECTION_B" },
		{ 0x1022, "XF_PROJECTION_C" },
		{ 0x1023, "XF_PROJECTION_D" },
		{ 0x1024, "XF_PROJECTION_E" },
		{ 0x1025, "XF_PROJECTION_F" },
		{ 0x1026, "XF_PROJECT_ORTHO" },
		{ 0x103F, "XF_NUMTEX" },
	};

	// The name of an XF register. The matrix and the light memory are named by their block: a block
	// write into them carries an index into the block, not a register of its own.
	static std::string XfRegName(size_t index)
	{
		char text[0x40];

		if (index < XF_MATRIX_MEMORY_ID + XF_MATRIX_MEMORY_SIZE)
		{
			sprintf(text, "matrix memory +%zi", index);
			return text;
		}

		if (index >= XF_NORMAL_MATRIX_MEMORY_ID && index < XF_NORMAL_MATRIX_MEMORY_ID + XF_NORMAL_MATRIX_MEMORY_SIZE)
		{
			sprintf(text, "normal matrix +%zi", index - XF_NORMAL_MATRIX_MEMORY_ID);
			return text;
		}

		if (index >= XF_DUALTEX_MATRIX_MEMORY_ID && index < XF_DUALTEX_MATRIX_MEMORY_ID + XF_DUALTEX_MATRIX_MEMORY_SIZE)
		{
			sprintf(text, "dualtex matrix +%zi", index - XF_DUALTEX_MATRIX_MEMORY_ID);
			return text;
		}

		if (index >= XF_LIGHT_MEMORY_ID && index < XF_LIGHT_MEMORY_ID + XF_LIGHT_MEMORY_SIZE)
		{
			sprintf(text, "light %zi +%zi", (index - XF_LIGHT_MEMORY_ID) / XF_LIGHT_DATA_SIZE,
				(index - XF_LIGHT_MEMORY_ID) % XF_LIGHT_DATA_SIZE);
			return text;
		}

		for (size_t i = 0; i < _countof(XfRegNames); i++)
		{
			if (XfRegNames[i].index == index)
				return XfRegNames[i].name;
		}

		if (index >= XF_TEXGEN0_ID && index <= XF_TEXGEN7_ID)
		{
			sprintf(text, "XF_TEXGEN%zi", index - XF_TEXGEN0_ID);
			return text;
		}

		if (index >= XF_DUALGEN0_ID && index <= XF_DUALGEN7_ID)
		{
			sprintf(text, "XF_DUALGEN%zi", index - XF_DUALGEN0_ID);
			return text;
		}

		sprintf(text, "XF_REG_%04zX", index);
		return text;
	}

	static const NameEntry BpRegNames[] =
	{
		{ 0x00, "GEN_MODE" },
		{ 0x05, "GEN_RESERVED" },
		{ 0x0F, "BUMP_IMASK" },
		{ 0x20, "SU_SCIS0" },
		{ 0x21, "SU_SCIS1" },
		{ 0x22, "SU_LPSIZE" },
		{ 0x23, "SU_PERF" },
		{ 0x24, "RAS1_PERF" },
		{ 0x25, "RAS1_SS0" },
		{ 0x26, "RAS1_SS1" },
		{ 0x27, "RAS1_IREF" },
		{ 0x40, "PE_ZMODE" },
		{ 0x41, "PE_CMODE0" },
		{ 0x42, "PE_CMODE1" },
		{ 0x43, "PE_CONTROL" },
		{ 0x44, "PE_FIELD_MASK" },
		{ 0x45, "PE_FINISH" },
		{ 0x46, "PE_REFRESH" },
		{ 0x47, "PE_TOKEN" },
		{ 0x48, "PE_TOKEN_INT" },
		{ 0x49, "PE_COPY_SRC_ADDR" },
		{ 0x4A, "PE_COPY_SRC_SIZE" },
		{ 0x4B, "PE_COPY_DST_BASE0" },
		{ 0x4C, "PE_COPY_DST_BASE1" },
		{ 0x4D, "PE_COPY_DST_STRIDE" },
		{ 0x4E, "PE_COPY_SCALE" },
		{ 0x4F, "PE_COPY_CLEAR_AR" },
		{ 0x50, "PE_COPY_CLEAR_GB" },
		{ 0x51, "PE_COPY_CLEAR_Z" },
		{ 0x52, "PE_COPY_CMD" },
		{ 0x53, "PE_COPY_VFILTER0" },
		{ 0x54, "PE_COPY_VFILTER1" },
		{ 0x55, "PE_XBOUND" },
		{ 0x56, "PE_YBOUND" },
		{ 0x57, "PE_PERFMODE" },
		{ 0x58, "PE_CHICKEN" },
		{ 0x59, "PE_QUAD_OFFSET" },
		{ 0x60, "TX_LOADBLOCK0" },
		{ 0x61, "TX_LOADBLOCK1" },
		{ 0x62, "TX_LOADBLOCK2" },
		{ 0x63, "TX_LOADBLOCK3" },
		{ 0x64, "TX_LOADTLUT0" },
		{ 0x65, "TX_LOADTLUT1" },
		{ 0x66, "TX_INVTAGS" },
		{ 0x67, "TX_PERFMODE" },
		{ 0x68, "TX_MISC" },
		{ 0x69, "TX_REFRESH" },
		{ 0xE8, "TEV_RANGE_ADJ_C" },
		{ 0xE9, "TEV_RANGE_ADJ_0" },
		{ 0xEA, "TEV_RANGE_ADJ_1" },
		{ 0xEB, "TEV_RANGE_ADJ_2" },
		{ 0xEC, "TEV_RANGE_ADJ_3" },
		{ 0xED, "TEV_RANGE_ADJ_4" },
		{ 0xEE, "TEV_FOG_PARAM_0" },
		{ 0xEF, "TEV_FOG_PARAM_1" },
		{ 0xF0, "TEV_FOG_PARAM_2" },
		{ 0xF1, "TEV_FOG_PARAM_3" },
		{ 0xF2, "TEV_FOG_COLOR" },
		{ 0xF3, "TEV_ALPHAFUNC" },
		{ 0xF4, "TEV_Z_ENV_0" },
		{ 0xF5, "TEV_Z_ENV_1" },
		{ 0xF6, "TEV_KSEL_0" },
		{ 0xF7, "TEV_KSEL_1" },
		{ 0xF8, "TEV_KSEL_2" },
		{ 0xF9, "TEV_KSEL_3" },
		{ 0xFA, "TEV_KSEL_4" },
		{ 0xFB, "TEV_KSEL_5" },
		{ 0xFC, "TEV_KSEL_6" },
		{ 0xFD, "TEV_KSEL_7" },
		{ 0xFE, "SU_SSMASK" },
	};

	// The name of a bypass register. The families that repeat (the texture maps of the two blocks,
	// the TEV stages, the bump matrices, the texture references) are spelled out from their base, so
	// that a dump line says what the register is and not only what its number is.
	static std::string BpRegName(uint32_t index)
	{
		char text[0x40];

		// The bump mapping matrix and command registers (BUMP_MATRIX_A0 at 0x06, BUMP_CMD at 0x10).
		if (index >= 0x06 && index <= 0x0E)
		{
			static const char* axis[3] = { "A", "B", "C" };
			sprintf(text, "BUMP_MATRIX_%s%i", axis[(index - 0x06) % 3], (index - 0x06) / 3);
			return text;
		}

		if (index >= 0x10 && index <= 0x1F)
		{
			sprintf(text, "BUMP_CMD%i", index - 0x10);
			return text;
		}

		if (index >= 0x28 && index <= 0x2F)
		{
			sprintf(text, "RAS1_TREF%i", index - 0x28);
			return text;
		}

		// The texture coordinate scales: the even register of a pair is S, the odd one is T.
		if (index >= 0x30 && index <= 0x3F)
		{
			sprintf(text, "SU_%cSIZE%i", ((index & 1) ? 'T' : 'S'), (index - 0x30) / 2);
			return text;
		}

		// The texture maps: 0x80..0x9B programs the maps 0-3 and 0xA0..0xBB the maps 4-7 (tx.h).
		if ((index >= 0x80 && index <= 0x9B) || (index >= 0xA0 && index <= 0xBB))
		{
			uint32_t base = (index < 0xA0) ? 0x80 : 0xA0;
			uint32_t map = ((index < 0xA0) ? 0 : 4) + ((index - base) & 3);
			uint32_t kind = (index - base) >> 2;

			static const char* kinds[7] = {
				"SETMODE0", "SETMODE1", "SETIMAGE0", "SETIMAGE1", "SETIMAGE2", "SETIMAGE3", "SETTLUT"
			};

			if (kind < _countof(kinds))
			{
				sprintf(text, "TX_%s_I%i", kinds[kind], map);
				return text;
			}
		}

		// The TEV combine stages: the even register of a pair is the colour environment, the odd one
		// the alpha environment.
		if (index >= 0xC0 && index <= 0xDF)
		{
			sprintf(text, "TEV_%s_ENV_%X", ((index & 1) ? "ALPHA" : "COLOR"), (index - 0xC0) / 2);
			return text;
		}

		if (index >= 0xE0 && index <= 0xE7)
		{
			sprintf(text, "TEV_REGISTER%c_%i", ((index & 1) ? 'H' : 'L'), (index - 0xE0) / 2);
			return text;
		}

		for (size_t i = 0; i < _countof(BpRegNames); i++)
		{
			if (BpRegNames[i].index == index)
				return BpRegNames[i].name;
		}

		sprintf(text, "BP_REG_%02X", index);
		return text;
	}

	static const char* PrimitiveName(uint32_t prim)
	{
		switch ((RAS_Primitive)prim)
		{
			case RAS_QUAD: return "QUAD";
			case RAS_QUAD_STRIP: return "QUAD_STRIP";
			case RAS_TRIANGLE: return "TRIANGLE";
			case RAS_TRIANGLE_STRIP: return "TRIANGLE_STRIP";
			case RAS_TRIANGLE_FAN: return "TRIANGLE_FAN";
			case RAS_LINE: return "LINE";
			case RAS_LINE_STRIP: return "LINE_STRIP";
			case RAS_POINT: return "POINT";
			default: break;
		}

		return "UNKNOWN";
	}

	// One vertex row, at the offset the record holds it.
	//
	// The vertex the CP pushes into the XF carries every attribute the hardware has, and the ones the
	// drawing format did not supply are whatever the CP left in the row: the XF reads only what
	// XF_INVTXSPEC allows (the vertex specification of the state the draw runs with), so the listing
	// decodes the row with it. `full` prints the whole row as it is - the raw stream holds it either
	// way, and `gfxdump save` writes the listing where every field is printed.
	struct DecodeState
	{
		InVertexSpec vtxSpec{};			//!< XF_INVTXSPEC: which attributes the XF reads from the row
	};

	static void FormatVertex(const GfxDumpFrame& frame, size_t offset, size_t limit, size_t index,
		const DecodeState& state, bool full, std::string& out)
	{
		char text[0x200];

		// The position is the one attribute every vertex has.
		sprintf(text, "        vtx%-4zi pos(%.3f %.3f %.3f)",
			index,
			BitsFloat(WordAt(frame, offset + 0, limit)),
			BitsFloat(WordAt(frame, offset + 4, limit)),
			BitsFloat(WordAt(frame, offset + 8, limit)));
		out += text;

		if (full || state.vtxSpec.normalUsage >= 1)
		{
			sprintf(text, " nrm(%.3f %.3f %.3f)",
				BitsFloat(WordAt(frame, offset + 12, limit)),
				BitsFloat(WordAt(frame, offset + 16, limit)),
				BitsFloat(WordAt(frame, offset + 20, limit)));
			out += text;
		}

		if (full || state.vtxSpec.normalUsage >= 2)
		{
			sprintf(text, " bin(%.3f %.3f %.3f)",
				BitsFloat(WordAt(frame, offset + 24, limit)),
				BitsFloat(WordAt(frame, offset + 28, limit)),
				BitsFloat(WordAt(frame, offset + 32, limit)));
			out += text;
		}

		if (full)
		{
			sprintf(text, " tan(%.3f %.3f %.3f)",
				BitsFloat(WordAt(frame, offset + 36, limit)),
				BitsFloat(WordAt(frame, offset + 40, limit)),
				BitsFloat(WordAt(frame, offset + 44, limit)));
			out += text;
		}

		if (full || state.vtxSpec.color0Usage >= 1)
		{
			sprintf(text, " col0(%08X)", WordAt(frame, offset + 48, limit));
			out += text;
		}

		if (full || state.vtxSpec.color0Usage >= 2)
		{
			sprintf(text, " col1(%08X)", WordAt(frame, offset + 52, limit));
			out += text;
		}

		// The host supplied texture pairs, in the number the vertex specification names.
		size_t pairs = full ? 8 : state.vtxSpec.texCoords;

		if (pairs > 8)
			pairs = 8;

		for (size_t t = 0; t < pairs; t++)
		{
			size_t base = offset + 56 + t * 8;

			sprintf(text, " tex%zi(%.3f %.3f)", t,
				BitsFloat(WordAt(frame, base, limit)), BitsFloat(WordAt(frame, base + 4, limit)));
			out += text;
		}

		sprintf(text, " midx(%08X %08X)\n", WordAt(frame, offset + 120, limit), WordAt(frame, offset + 124, limit));
		out += text;
	}

	// One record of the stream, as the decoded listing shows it. `state` is the XF register state the
	// records before this one left behind, which is what the vertex rows are decoded with.
	static void FormatRecord(const GfxDumpFrame& frame, const GfxDumpRecord& record, DecodeState& state,
		size_t vertexLimit, bool full, std::string& out)
	{
		char text[0x200];

		// What the record really holds: a frame that was cut off leaves the last record short.
		size_t limit = record.offset + record.size;
		if (limit > frame.stream.size())
			limit = frame.stream.size();

		switch (record.op)
		{
			case GfxDumpOp::RegLoad:
			{
				sprintf(text, "%06X  xf.write   0x%04X %-24s +%u\n", record.offset, record.a,
					XfRegName(record.a).c_str(), record.b);
				out += text;

				// Four words a line: each one is printed with the register it lands in, which is what
				// makes a matrix block readable.
				for (size_t i = 0; i < record.b; i++)
				{
					size_t offset = record.offset + 8 + i * 4;

					if (offset + 4 > limit)
						break;

					if ((i % 4) == 0)
					{
						if (i > 0)
							out += "\n";

						sprintf(text, "        0x%04X = %08X", record.a + (uint32_t)i, WordAt(frame, offset, limit));
					}
					else
					{
						sprintf(text, "  0x%04X = %08X", record.a + (uint32_t)i, WordAt(frame, offset, limit));
					}

					out += text;

					// The vertex specification of the draws that follow is the part of the register
					// state the listing itself needs.
					if ((record.a + i) == XF_INVTXSPEC_ID)
						state.vtxSpec.bits = WordAt(frame, offset, limit);
				}

				out += "\n";
				break;
			}

			case GfxDumpOp::RegRead:
			{
				sprintf(text, "%06X  xf.read    0x%04X %s\n", record.offset, record.a, XfRegName(record.a).c_str());
				out += text;
				break;
			}

			case GfxDumpOp::SuReg:
			{
				sprintf(text, "%06X  bp.write   0x%02X   %-20s = 0x%06X  mask 0x%06X\n", record.offset,
					record.a, BpRegName(record.a).c_str(), record.b, record.c);
				out += text;
				break;
			}

			case GfxDumpOp::Draw:
			{
				sprintf(text, "%06X  draw       %-14s vtx %u\n", record.offset, PrimitiveName(record.a), record.b);
				out += text;

				size_t printed = my_min((size_t)record.b, vertexLimit);

				for (size_t i = 0; i < printed; i++)
					FormatVertex(frame, record.offset + 8 + i * VertexWords * 4, limit, i, state, full, out);

				if (printed < record.b)
				{
					sprintf(text, "        ... %zi more vertex rows\n", (size_t)record.b - printed);
					out += text;
				}

				break;
			}
		}
	}

	// One row of a hex dump: `00000000  xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx  |........|`.
	static void FormatHexRow(const uint8_t* data, size_t size, size_t offset, std::string& out)
	{
		char text[0x40];
		std::string ascii;

		sprintf(text, "%08zX  ", offset);
		out += text;

		for (size_t i = 0; i < 16; i++)
		{
			if (i == 8)
				out += ' ';

			if ((offset + i) < size)
			{
				sprintf(text, "%02X", data[offset + i]);
				out += text;
				ascii += (data[offset + i] >= 0x20 && data[offset + i] < 0x7F) ? (char)data[offset + i] : '.';
			}
			else
			{
				out += "  ";
				ascii += ' ';
			}

			out += ' ';
		}

		out += " |" + ascii + "|\n";
	}

	static void FormatHex(const uint8_t* data, size_t size, size_t limit, std::string& out)
	{
		if (limit > size)
			limit = size;

		// The rows are whole: half a row of a stream that ends in the middle of one would only hide
		// how long the stream is.
		for (size_t offset = 0; offset < limit; offset += 16)
			FormatHexRow(data, limit, offset, out);
	}


	// ========================================================================================
	// The Markdown the panels show
	// ========================================================================================

	// The capture state, the buttons that change it and the frames the history holds. It is the
	// answer of `gfxdump` and of every mode that changes something, so that a button click refreshes
	// the panel it was clicked in.
	static std::string MarkdownFrames()
	{
		if (gfx_dump == nullptr)
			return "";

		std::vector<GfxDump::FrameInfo> infos;
		gfx_dump->FrameInfos(infos);

		size_t streamBytes = 0, sliceBytes = 0;

		for (size_t i = 0; i < infos.size(); i++)
		{
			streamBytes += infos[i].bytes;
			sliceBytes += infos[i].sliceBytes;
		}

		std::string md;
		char text[0x200];

		md += "### GFX command dump\n\n";

		// The dump records what the CP pushes into the XF: the register block loads, the register
		// reads, the bypass words and the vertex rows of every draw (`gfxdump.h`).
		if (gfx_dump->Capturing())
			md += "Capture: **on**";
		else
			md += "Capture: **off**";

		sprintf(text, " - %zi frames held (%s stream, %s RAM)\n\n", infos.size(),
			FormatSize(streamBytes).c_str(), FormatSize(sliceBytes).c_str());
		md += text;

		if (gfx_dump->Capturing())
		{
			md += "[Stop capture](cmd:gfxdump capture off)";
		}
		else
		{
			md += "[Start capture](cmd:gfxdump capture on)";
		}

		md += "  [Save frame](cmd:gfxdump save)  [Clear](cmd:gfxdump clear)\n\n";

		// The dump is an artifact of the session: the files land in the folder of the debugger
		// session, and the session writes the last frame itself when it closes (debugui2.cpp).
		md += "_The selected frame is saved into the debug session folder (the stream, the RAM slices\n"
			"and the decoded listing)._\n\n";

		if (gfx_dump->Capturing())
		{
			GfxDump::Progress progress = gfx_dump->InProgress();

			sprintf(text, "Recording frame %i: %zi commands, %s\n\n", progress.frame, progress.records,
				FormatSize(progress.bytes).c_str());
			md += text;
		}

		if (infos.empty())
		{
			md += "_No frames yet. The capture button starts the recording; the frames appear here as the\n"
				"pipeline finishes them._\n";
			return md;
		}

		int selected = gfx_dump->Selected();

		for (size_t i = infos.size(); i-- > 0; )
		{
			const GfxDump::FrameInfo& info = infos[i];

			sprintf(text, "* **#%i** frame %i - %zi commands, %s stream, %zi RAM slices (%s)%s\n",
				info.id, info.frame, info.records, FormatSize(info.bytes).c_str(), info.slices,
				FormatSize(info.sliceBytes).c_str(),
				info.truncated ? ", truncated" : "");
			md += text;

			sprintf(text, "  [select](cmd:gfxdump select %i)\n", info.id);
			md += text;

			if (selected == info.id)
				md += "  _shown in the Commands and Hex tabs_\n";
		}

		return md;
	}

	// The decoded commands of one frame.
	static std::string MarkdownCommands(int id)
	{
		if (gfx_dump == nullptr)
			return "";

		std::shared_ptr<const GfxDumpFrame> frame = gfx_dump->Frame(id);

		if (frame == nullptr)
			return "### GFX command dump\n\n_No frame is held. Start a capture first._\n";

		std::string md;
		char text[0x200];

		sprintf(text, "### Frame #%i (GFX frame %i) - command history\n\n", frame->id, frame->frame);
		md += text;

		sprintf(text, "`%zi commands, %s`", frame->records.size(), FormatSize(frame->stream.size()).c_str());
		md += text;

		if (frame->records.size() > ViewCommands)
		{
			sprintf(text, " - showing the first %zi\n", ViewCommands);
			md += text;
		}
		else
		{
			md += "\n";
		}

		if (frame->truncated)
			md += "\n_The frame passed the size limit of the dump and was cut off._\n";

		md += "\n```\n";

		size_t printed = my_min(frame->records.size(), ViewCommands);

		DecodeState state;

		for (size_t i = 0; i < printed; i++)
			FormatRecord(*frame, frame->records[i], state, ViewVertices, false, md);

		if (printed < frame->records.size())
		{
			sprintf(text, "... %zi more commands (`gfxdump save` writes the whole frame)\n",
				frame->records.size() - printed);
			md += text;
		}

		md += "```\n";

		return md;
	}

	// The raw stream of one frame and the RAM it was executed with.
	static std::string MarkdownHex(int id)
	{
		if (gfx_dump == nullptr)
			return "";

		std::shared_ptr<const GfxDumpFrame> frame = gfx_dump->Frame(id);

		if (frame == nullptr)
			return "### GFX command dump\n\n_No frame is held. Start a capture first._\n";

		std::string md;
		char text[0x200];

		sprintf(text, "### Frame #%i (GFX frame %i) - raw XF stream\n\n", frame->id, frame->frame);
		md += text;

		sprintf(text, "`%zi commands, %s`", frame->records.size(), FormatSize(frame->stream.size()).c_str());
		md += text;

		if (frame->stream.size() > ViewHexBytes)
		{
			sprintf(text, " - showing the first %zi bytes\n", ViewHexBytes);
			md += text;
		}
		else
		{
			md += "\n";
		}

		md += "\n```\n";
		FormatHex(frame->stream.data(), frame->stream.size(), ViewHexBytes, md);
		md += "```\n";

		if (frame->slices.empty())
		{
			md += "\n_No RAM slices were attached to this frame._\n";
			return md;
		}

		size_t sliceBytes = 0;
		for (size_t i = 0; i < frame->slices.size(); i++)
			sliceBytes += frame->slices[i].data.size();

		// The RAM a draw reads is the texture it samples (and the palette of a paletted one): the
		// bytes travel with the frame, because the guest overwrites them long before anyone looks at
		// the dump.
		sprintf(text, "\n### RAM slices (%zi, %s)\n\n", frame->slices.size(), FormatSize(sliceBytes).c_str());
		md += text;

		size_t printed = my_min(frame->slices.size(), ViewSlices);

		for (size_t i = 0; i < printed; i++)
		{
			const GfxDumpSlice& slice = frame->slices[i];

			sprintf(text, "`%08X +%zi` %s\n\n```\n", slice.address, slice.data.size(), slice.note.c_str());
			md += text;

			FormatHex(slice.data.data(), slice.data.size(), ViewSliceBytes, md);

			if (slice.data.size() > ViewSliceBytes)
			{
				sprintf(text, "... %zi more bytes\n", slice.data.size() - ViewSliceBytes);
				md += text;
			}

			md += "```\n\n";
		}

		if (printed < frame->slices.size())
		{
			sprintf(text, "_... %zi more slices (the files `gfxdump save` writes hold them all)_\n",
				frame->slices.size() - printed);
			md += text;
		}

		return md;
	}


	// ========================================================================================
	// The artifacts
	// ========================================================================================

	// The whole decoded listing of a frame, with the raw stream under it: what the panels show the
	// beginning of.
	static std::string TextListing(const GfxDumpFrame& frame)
	{
		std::string text;
		char line[0x200];

		sprintf(line, "# GFX frame dump #%i (GFX frame counter %i)\n\n", frame.id, frame.frame);
		text += line;

		sprintf(line, "commands: %zi\nstream: %zi bytes\nRAM slices: %zi\n\n",
			frame.records.size(), frame.stream.size(), frame.slices.size());
		text += line;

		if (frame.truncated)
			text += "The frame passed the size limit of the dump and was cut off.\n\n";

		text += "## Commands\n\n";
		text += "```\n";

		DecodeState state;

		for (size_t i = 0; i < frame.records.size(); i++)
			FormatRecord(frame, frame.records[i], state, (size_t)-1, true, text);

		text += "```\n\n## Raw XF stream\n\n```\n";
		FormatHex(frame.stream.data(), frame.stream.size(), frame.stream.size(), text);
		text += "```\n";

		for (size_t i = 0; i < frame.slices.size(); i++)
		{
			const GfxDumpSlice& slice = frame.slices[i];

			sprintf(line, "\n## RAM slice %zi: `%08X +%zi` %s\n\n```\n", i, slice.address, slice.data.size(),
				slice.note.c_str());
			text += line;

			FormatHex(slice.data.data(), slice.data.size(), slice.data.size(), text);
			text += "```\n";
		}

		return text;
	}

	// The RAM slices of a frame, in one file: for every slice a `uint32 address`, a `uint32 size`, a
	// `uint32 noteLength`, the note (UTF-8) and the bytes, the note and the bytes each padded to a
	// four-byte boundary. The words are big-endian, like the stream.
	static std::vector<uint8_t> PackSlices(const GfxDumpFrame& frame)
	{
		std::vector<uint8_t> out;

		for (size_t i = 0; i < frame.slices.size(); i++)
		{
			const GfxDumpSlice& slice = frame.slices[i];

			uint32_t noteLength = (uint32_t)slice.note.size();

			PutWord(out, slice.address);
			PutWord(out, (uint32_t)slice.data.size());
			PutWord(out, noteLength);

			for (size_t c = 0; c < noteLength; c++)
				out.push_back((uint8_t)slice.note[c]);

			while ((out.size() % 4) != 0)
				out.push_back(0);

			out.insert(out.end(), slice.data.begin(), slice.data.end());

			while ((out.size() % 4) != 0)
				out.push_back(0);
		}

		return out;
	}

	// The session folder of the debugger, through the debug interface (it is a JDI entity). Empty
	// when the debugger is not running: the artifacts belong next to the session.
	static std::string SessionPath()
	{
		std::string path;

		Json::Value* value = nullptr;

		try
		{
			value = JDI::Hub.ExecuteFast("SessionPath");
		}
		catch (...)
		{
			return path;
		}

		if (value != nullptr)
		{
			if (value->type == Json::ValueType::Array && !value->children.empty())
			{
				Json::Value* first = value->children.front();
				if (first->type == Json::ValueType::String)
					path = Util::WstringToString(first->value.AsString);
			}

			delete value;
		}

		return path;
	}


	// ========================================================================================
	// The JDI commands (the GFX node)
	// ========================================================================================

	static Json::Value* MakeMarkdown(const std::string& markdown)
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Object;
		output->AddUtf8String("markdown", markdown.c_str());
		return output;
	}

	// The frame a mode works on: the one the arguments name, or the one the panel looks at.
	static int FrameArg(std::vector<std::string>& args, size_t index)
	{
		if (args.size() > index)
			return (int)strtol(args[index].c_str(), nullptr, 0);

		return gfx_dump->Selected();
	}

	// gfxdump save [id] - write the frame into the session folder.
	static std::string SaveFrame(int id)
	{
		std::shared_ptr<const GfxDumpFrame> frame = gfx_dump->Frame(id);

		if (frame == nullptr)
			return "### GFX command dump\n\n_No frame is held. Start a capture first._\n";

		std::string session = SessionPath();

		if (session.empty())
			return "**gfxdump:** the debugger is not running, there is nowhere to put the dump\n";

		std::vector<uint8_t> slices = PackSlices(*frame);
		std::string listing = TextListing(*frame);

		char name[0x100];

		sprintf(name, "gfxdump_%i_stream.bin", frame->id);
		std::string streamPath = session + "/" + name;

		sprintf(name, "gfxdump_%i_ram.bin", frame->id);
		std::string ramPath = session + "/" + name;

		sprintf(name, "gfxdump_%i.txt", frame->id);
		std::string textPath = session + "/" + name;

		std::vector<uint8_t> stream = frame->stream;
		std::vector<uint8_t> text(listing.begin(), listing.end());

		bool ok = Util::FileSave(streamPath, stream);
		ok = Util::FileSave(ramPath, slices) && ok;
		ok = Util::FileSave(textPath, text) && ok;

		std::string md;
		char line[0x400];

		if (!ok)
		{
			md += "**gfxdump:** the dump could not be written\n\n";
			return md;
		}

		sprintf(line, "### Frame #%i saved\n\n", frame->id);
		md += line;

		// The stream and the slices are the dump itself (the format is described in gfxdump.h), the
		// text file is the same listing the panel shows, in full.
		sprintf(line, "* `%s` - %s, the raw XF stream and its RAM slices in one file\n",
			streamPath.c_str(), FormatSize(stream.size()).c_str());
		md += line;

		sprintf(line, "* `%s` - %s, the commands of the frame, decoded\n", textPath.c_str(),
			FormatSize(text.size()).c_str());
		md += line;

		sprintf(line, "* `%s` - %s, the RAM slices of the frame\n", ramPath.c_str(),
			FormatSize(slices.size()).c_str());
		md += line;

		return md;
	}

	static Json::Value* CmdGfxDump(std::vector<std::string>& args)
	{
		if (gfx_dump == nullptr)
		{
			Report(Channel::Norm, "gfxdump: the GFX subsystem is not running\n");
			return nullptr;
		}

		std::string mode = (args.size() > 1) ? args[1] : "frames";

		if (mode == "capture")
		{
			bool on = !gfx_dump->Capturing();

			if (args.size() > 2)
			{
				std::string value = args[2];
				if (value == "on" || value == "1") on = true;
				else if (value == "off" || value == "0") on = false;
			}

			gfx_dump->SetCapturing(on);
		}
		else if (mode == "select")
		{
			if (args.size() < 3)
			{
				Report(Channel::Norm, "select: the frame id is expected (`gfxdump select <id>`)\n");
				return nullptr;
			}

			gfx_dump->Select((int)strtol(args[2].c_str(), nullptr, 0));
		}
		else if (mode == "clear")
		{
			gfx_dump->Clear();
		}
		else if (mode == "save")
		{
			return MakeMarkdown(SaveFrame(FrameArg(args, 2)));
		}
		else if (mode == "commands")
		{
			return MakeMarkdown(MarkdownCommands(FrameArg(args, 2)));
		}
		else if (mode == "hex")
		{
			return MakeMarkdown(MarkdownHex(FrameArg(args, 2)));
		}
		else if (mode != "frames")
		{
			Report(Channel::Norm, "gfxdump: unknown mode '%s'\n", mode.c_str());
			return nullptr;
		}

		// Every mode that changes something answers with the frame list, so that the panel the button
		// was clicked in is redrawn from the new state.
		return MakeMarkdown(MarkdownFrames());
	}

	void GfxDump::Reflector()
	{
		JDI::Hub.AddCmd("gfxdump", CmdGfxDump);
	}
}
