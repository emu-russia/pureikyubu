// All GX architectural definitions (register names, bit names, etc.)

// More GX info: https://github.com/ogamespec/dolwin-docs/blob/master/HW/GraphicsSystem/GX.md

#pragma once

namespace GX
{
	// CP Commands. Format of commands transmitted via FIFO and display lists (DL)

	enum class CPCommand
	{
		CP_CMD_NOP = 0x00,					// 00000000
		CP_CMD_VCACHE_INVD = 0x48,			// 01001xxx
		CP_CMD_CALL_DL = 0x40,				// 01000xxx
		CP_CMD_LOAD_BPREG = 0x60,			// 0110,SUattr(3:0), Address[7:0], 24 bits data
		CP_CMD_LOAD_CPREG = 0x08,			// 00001xxx, Address[7:0], 32 bits data
		CP_CMD_LOAD_XFREG = 0x10,			// 00010xxx
		CP_CMD_LOAD_INDXA = 0x20,			// 00100xxx
		CP_CMD_LOAD_INDXB = 0x28,			// 00101xxx
		CP_CMD_LOAD_INDXC = 0x30,			// 00110xxx
		CP_CMD_LOAD_INDXD = 0x38,			// 00111xxx
		CP_CMD_DRAW_QUAD = 0x80,			// 10000,vat(2:0)
		CP_CMD_DRAW_TRIANGLE = 0x90,		// 10010,vat(2:0)
		CP_CMD_DRAW_STRIP = 0x98,			// 10011,vat(2:0)
		CP_CMD_DRAW_FAN = 0xA0,				// 10100,vat(2:0)
		CP_CMD_DRAW_LINE = 0xA8,			// 10101,vat(2:0)
		CP_CMD_DRAW_LINESTRIP = 0xB0,		// 10110,vat(2:0)
		CP_CMD_DRAW_POINT = 0xB8,			// 10111,vat(2:0)
	};

	// Primitive (for backend)

	enum class Primitive
	{
		Quads,
		Triangles,
		TriangleStrip,
		TriangleFan,
		Lines,
		LineStrip,
		Points,
	};

	// Processed vertex (for backend)

	struct Vertex
	{
		int bogus;
	};

	// PI->CP FIFO registers
	enum class PI_CPMappedRegister
	{
		PI_CPBAS_ID = 3,
		PI_CPTOP_ID = 4,
		PI_CPWRT_ID = 5,
		PI_CPABT_ID = 6,
	};

	// PI CP write pointer wrap bit
	#define PI_CPWRT_WRAP   0x0400'0000

}

#include "PixelEngine.h"
#include "CPRegs.h"
#include "XFRegs.h"
#include "BPRegs.h"
