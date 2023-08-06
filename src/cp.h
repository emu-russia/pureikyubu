#pragma once

namespace GX
{

	// CP Commands. Format of commands transmitted via FIFO and display lists (DL)

	enum CPCommand : uint8_t
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

	// CP Registers (from CPU side). 16-bit access

	enum class CPMappedRegister
	{
		CP_STATUS_ID = 0x00,
		CP_ENABLE_ID = 0x01,
		CP_CLR_ID = 0x02,
		CP_MEMPERF_SEL_ID = 0x03,
		CP_STM_LOW_ID = 0x05,
		CP_FIFO_BASEL_ID = 0x10,
		CP_FIFO_BASEH_ID = 0x11,
		CP_FIFO_TOPL_ID = 0x12,
		CP_FIFO_TOPH_ID = 0x13,
		CP_FIFO_HICNTL_ID = 0x14,
		CP_FIFO_HICNTH_ID = 0x15,
		CP_FIFO_LOCNTL_ID = 0x16,
		CP_FIFO_LOCNTH_ID = 0x17,
		CP_FIFO_COUNTL_ID = 0x18,
		CP_FIFO_COUNTH_ID = 0x19,
		CP_FIFO_WPTRL_ID = 0x1a,
		CP_FIFO_WPTRH_ID = 0x1b,
		CP_FIFO_RPTRL_ID = 0x1c,
		CP_FIFO_RPTRH_ID = 0x1d,
		CP_FIFO_BRKL_ID = 0x1e,
		CP_FIFO_BRKH_ID = 0x1f,
		CP_COUNTER0L_ID = 0x20,
		CP_COUNTER0H_ID = 0x21,
		CP_COUNTER1L_ID = 0x22,
		CP_COUNTER1H_ID = 0x23,
		CP_COUNTER2L_ID = 0x24,
		CP_COUNTER2H_ID = 0x25,
		CP_COUNTER3L_ID = 0x26,
		CP_COUNTER3H_ID = 0x27,
		CP_VC_CHKCNTL_ID = 0x28,
		CP_VC_CHKCNTH_ID = 0x29,
		CP_VC_MISSL_ID = 0x2a,
		CP_VC_MISSH_ID = 0x2b,
		CP_VC_STALLL_ID = 0x2c,
		CP_VC_STALLH_ID = 0x2d,
		CP_FRCLK_CNTL_ID = 0x2e,
		CP_FRCLK_CNTH_ID = 0x2f,
		CP_XF_ADDR_ID = 0x30,
		CP_XF_DATAL_ID = 0x31,
		CP_XF_DATAH_ID = 0x32,
	};

	// CP STATUS register mask layout
	#define CP_SR_OVF       (1 << 0)        // FIFO overflow (fifo_count > FIFO_HICNT)
	#define CP_SR_UVF       (1 << 1)        // FIFO underflow (fifo_count < FIFO_LOCNT)
	#define CP_SR_RD_IDLE   (1 << 2)        // FIFO read unit idle
	#define CP_SR_CMD_IDLE  (1 << 3)        // CP idle
	#define CP_SR_BPINT     (1 << 4)        // FIFO reach break point (cleared by disable FIFO break point)

	// CP ENABLE register mask layout
	#define CP_CR_RDEN      (1 << 0)        // Enable FIFO reads, reset value is 0 disable
	#define CP_CR_BPEN      (1 << 1)        // FIFO break point enable bit, reset value is 0 disable. Write 0 to clear BPINT
	#define CP_CR_OVFEN     (1 << 2)        // FIFO overflow interrupt enable, reset value is 0 disable
	#define CP_CR_UVFEN     (1 << 3)        // FIFO underflow interrupt enable, reset value is 0 disable
	#define CP_CR_WPINC     (1 << 4)        // FIFO write pointer increment enable, reset value is 1 enable
	#define CP_CR_BPINTEN   (1 << 5)        // FIFO break point interrupt enable, reset value is 0 disable

	// CP clear register mask layout
	#define CP_CLR_OVFCLR   (1 << 0)        // clear FIFO overflow interrupt
	#define CP_CLR_UVFCLR   (1 << 1)        // clear FIFO underflow interrupt

	#pragma pack(push, 8)

	// CPU CP registers
	struct CPHostRegs
	{
		uint16_t     sr;         // status
		uint16_t     cr;         // control
		union
		{
			struct
			{
				uint16_t basel;
				uint16_t baseh;
			};
			volatile uint32_t     base;
		};
		union
		{
			struct
			{
				uint16_t topl;
				uint16_t toph;
			};
			volatile uint32_t     top;
		};
		union
		{
			struct
			{
				uint16_t lomarkl;
				uint16_t lomarkh;
			};
			volatile uint32_t     lomark;
		};
		union
		{
			struct
			{
				uint16_t himarkl;
				uint16_t himarkh;
			};
			volatile uint32_t     himark;
		};
		union
		{
			struct
			{
				uint16_t cntl;
				uint16_t cnth;
			};
			volatile uint32_t     cnt;
		};
		union
		{
			struct
			{
				uint16_t wrptrl;
				uint16_t wrptrh;
			};
			volatile uint32_t     wrptr;
		};
		union
		{
			struct
			{
				uint16_t rdptrl;
				uint16_t rdptrh;
			};
			volatile uint32_t     rdptr;
		};
		union
		{
			struct
			{
				uint16_t bpptrl;
				uint16_t bpptrh;
			};
			volatile uint32_t     bpptr;
		};
	};

	#pragma pack(pop)

	// CP Registers (from GX side). These registers are available only for writing, with the CP_LoadRegs command

	enum class CPRegister
	{
		CP_VC_STAT_RESET_ID = 0x00,
		CP_STAT_ENABLE_ID = 0x10,
		CP_STAT_SEL_ID = 0x20,
		CP_MATINDEX_A_ID = 0x30,			// MatrixIndexA 0011xxxx 
		CP_MATINDEX_B_ID = 0x40,			// MatrixIndexB 0100xxxx 
		CP_VCD_LO_ID = 0x50,				// VCD_Lo 0101xxxx
		CP_VCD_HI_ID = 0x60,				// VCD_Hi 0110xxxx
		CP_VAT_A_ID = 0x70,					// VAT_group0 0111x,vat[2:0]
		CP_VAT_B_ID = 0x80,					// VAT_group1 1000x,vat[2:0]
		CP_VAT_C_ID = 0x90,					// VAT_group2 1001x,vat[2:0]
		CP_ARRAY_BASE_ID = 0xa0,			// ArrayBase 1001,array[3:0]
		CP_ARRAY_STRIDE_ID = 0xb0,			// ArrayStride 1011,array[3:0]
	};

	// Vertex attributes.
	// Specifies the sequence of attributes in the raw vertex data. Which of the attributes are present is selected by the VCD settings.

	enum class VertexAttr
	{
		VTX_POSMATIDX = 0,      // Position/Normal Matrix Index
		VTX_TEX0MTXIDX,         // Texture Coordinate 0 Matrix Index
		VTX_TEX1MTXIDX,         // Texture Coordinate 1 Matrix Index
		VTX_TEX2MTXIDX,         // Texture Coordinate 2 Matrix Index
		VTX_TEX3MTXIDX,         // Texture Coordinate 3 Matrix Index
		VTX_TEX4MTXIDX,         // Texture Coordinate 4 Matrix Index
		VTX_TEX5MTXIDX,         // Texture Coordinate 5 Matrix Index
		VTX_TEX6MTXIDX,         // Texture Coordinate 6 Matrix Index
		VTX_TEX7MTXIDX,         // Texture Coordinate 7 Matrix Index
		VTX_POS,                // Position
		VTX_NRM,                // Normal or Normal/Binormal/Tangent
		VTX_COLOR0,             // Color 0
		VTX_COLOR1,             // Color 1
		VTX_TEXCOORD0,          // Texture Coordinate 0
		VTX_TEXCOORD1,          // Texture Coordinate 1
		VTX_TEXCOORD2,          // Texture Coordinate 2
		VTX_TEXCOORD3,          // Texture Coordinate 3
		VTX_TEXCOORD4,          // Texture Coordinate 4
		VTX_TEXCOORD5,          // Texture Coordinate 5
		VTX_TEXCOORD6,          // Texture Coordinate 6
		VTX_TEXCOORD7,          // Texture Coordinate 7
		VTX_MAX_ATTR
	};

	// Attribute types (from VCD register)

	enum AttrType : unsigned
	{
		VCD_NONE = 0,           // attribute stage disabled
		VCD_DIRECT,             // direct data
		VCD_INDEX8,				// 8-bit indexed data
		VCD_INDEX16             // 16-bit indexed data
	};

	// Vertex Components Count (from VAT register)

	enum VatPosCount : unsigned
	{
		VCNT_POS_XY = 0,		// two (x,y)
		VCNT_POS_XYZ = 1,		// three (x,y,z)
	};

	enum VatNormCount : unsigned
	{
		VCNT_NRM_XYZ = 0,		// three normals
		VCNT_NRM_NBT = 1,		// nine normals
	};

	enum VatColorCount : unsigned
	{
		VCNT_CLR_RGB = 0,		// three (r,g,b)
		VCNT_CLR_RGBA = 1,		// four (r,g,b,a)
	};

	enum VatTexCoordCount : unsigned
	{
		VCNT_TEX_S = 0,			// one (s)
		VCNT_TEX_ST = 1			// two (s,t)
	};

	// Vertex Component Format (from VAT register)

	enum VatCompFormat : unsigned
	{
		// For Components (normal, coords)
		VFMT_U8 = 0,			// ubyte
		VFMT_S8 = 1,			// byte
		VFMT_U16 = 2,			// ushort
		VFMT_S16 = 3,			// short
		VFMT_F32 = 4,			// float
	};

	// Vertex Color Format (from VAT register)

	enum VatColorFormat : unsigned
	{
		VFMT_RGB565 = 0,		// 16 bit 565 (three comp)
		VFMT_RGB8 = 1,			// 24 bit 888 (three comp)
		VFMT_RGBX8 = 2,			// 32 bit 888x (three comp)
		VFMT_RGBA4 = 3,			// 16 bit 4444 (four comp)
		VFMT_RGBA6 = 4,			// 24 bit 6666 (four comp)
		VFMT_RGBA8 = 5			// 32 bit 8888 (four comp)
	};

	#pragma pack(push, 1)

	union MatrixIndexA
	{
		struct
		{
			unsigned PosNrmIndex : 6;
			unsigned Tex0Index : 6;
			unsigned Tex1Index : 6;
			unsigned Tex2Index : 6;
			unsigned Tex3Index : 6;
		};
		uint32_t bits;
	};

	static_assert (sizeof(MatrixIndexA) == sizeof(uint32_t), "MatrixIndexA invalid definition!");

	union MatrixIndexB
	{
		struct
		{
			unsigned Tex4Index : 6;
			unsigned Tex5Index : 6;
			unsigned Tex6Index : 6;
			unsigned Tex7Index : 6;
		};
		uint32_t bits;
	};

	static_assert (sizeof(MatrixIndexB) == sizeof(uint32_t), "MatrixIndexB invalid definition!");

	union VCD_Lo
	{
		struct
		{
			unsigned PosNrmMatIdx : 1;
			unsigned Tex0MatIdx : 1;
			unsigned Tex1MatIdx : 1;
			unsigned Tex2MatIdx : 1;
			unsigned Tex3MatIdx : 1;
			unsigned Tex4MatIdx : 1;
			unsigned Tex5MatIdx : 1;
			unsigned Tex6MatIdx : 1;
			unsigned Tex7MatIdx : 1;
			unsigned Position : 2;		// AttrType
			unsigned Normal : 2;		// AttrType
			unsigned Color0 : 2;		// AttrType
			unsigned Color1 : 2;		// AttrType
		};
		uint32_t bits;
	};

	static_assert (sizeof(VCD_Lo) == sizeof(uint32_t), "VCD_Lo invalid definition!");

	union VCD_Hi
	{
		struct
		{
			unsigned Tex0Coord : 2;		// AttrType
			unsigned Tex1Coord : 2;		// AttrType
			unsigned Tex2Coord : 2;		// AttrType
			unsigned Tex3Coord : 2;		// AttrType
			unsigned Tex4Coord : 2;		// AttrType
			unsigned Tex5Coord : 2;		// AttrType
			unsigned Tex6Coord : 2;		// AttrType
			unsigned Tex7Coord : 2;		// AttrType
		};
		uint32_t bits;
	};

	static_assert (sizeof(VCD_Hi) == sizeof(uint32_t), "VCD_Hi invalid definition!");

	union VAT_group0
	{
		struct
		{
			unsigned	poscnt : 1;
			unsigned	posfmt : 3;
			unsigned	posshft : 5;			// Location of decimal point from LSB. This shift applies to all u/short components and to u/byte components where ByteDequant is asserted
			unsigned    nrmcnt : 1;
			unsigned	nrmfmt : 3;				// Normal location of decimal point predefined as follow: Byte: 6, Short: 14
			unsigned    col0cnt : 1;
			unsigned	col0fmt : 3;
			unsigned    col1cnt : 1;
			unsigned	col1fmt : 3;
			unsigned    tex0cnt : 1;
			unsigned	tex0fmt : 3;
			unsigned 	tex0shft : 5;
			unsigned	bytedeq : 1;			// Shift applies for u/byte and u/short components of position and texture attributes.
			unsigned 	nrmidx3 : 1;			// When nine normals selected in indirect mode, input will be treated as three staggered indices (one per triple biased by component size), into normal table.
		};
		uint32_t bits;
	};

	static_assert (sizeof(VAT_group0) == sizeof(uint32_t), "VAT_group0 invalid definition!");

	union VAT_group1
	{
		struct
		{
			unsigned    tex1cnt : 1;
			unsigned	tex1fmt : 3;
			unsigned 	tex1shft : 5;
			unsigned    tex2cnt : 1;
			unsigned	tex2fmt : 3;
			unsigned 	tex2shft : 5;
			unsigned    tex3cnt : 1;
			unsigned	tex3fmt : 3;
			unsigned 	tex3shft : 5;
			unsigned    tex4cnt : 1;
			unsigned	tex4fmt : 3;
			unsigned 	vcache : 1;
		};
		uint32_t bits;
	};

	static_assert (sizeof(VAT_group1) == sizeof(uint32_t), "VAT_group1 invalid definition!");

	union VAT_group2
	{
		struct
		{
			unsigned 	tex4shft : 5;
			unsigned    tex5cnt : 1;
			unsigned	tex5fmt : 3;
			unsigned	tex5shft : 5;
			unsigned    tex6cnt : 1;
			unsigned	tex6fmt : 3;
			unsigned	tex6shft : 5;
			unsigned    tex7cnt : 1;
			unsigned	tex7fmt : 3;
			unsigned 	tex7shft : 5;
		};
		uint32_t bits;
	};

	static_assert (sizeof(VAT_group2) == sizeof(uint32_t), "VAT_group2 invalid definition!");

	union ArrayBase
	{
		struct
		{
			unsigned  Base : 26;
		};
		uint32_t bits;
	};

	static_assert (sizeof(ArrayBase) == sizeof(uint32_t), "ArrayBase invalid definition!");

	union ArrayStride
	{
		struct
		{
			unsigned  Stride : 8;
		};
		uint32_t bits;
	};

	static_assert (sizeof(ArrayStride) == sizeof(uint32_t), "ArrayStride invalid definition!");

	// Array name for ArrayBase and ArrayStride

	enum class ArrayId
	{
		Pos = 0,
		Nrm,
		Color0,
		Color1,
		Tex0Coord,
		Tex1Coord,
		Tex2Coord,
		Tex3Coord,
		Tex4Coord,
		Tex5Coord,
		Tex6Coord,
		Tex7Coord,

		// Used by XF_IndexLoadRegA/B/C/D commands

		IndexRegA,
		IndexRegB,
		IndexRegC,
		IndexRegD,

		Max,
	};

	struct CPState
	{
		MatrixIndexA matIndexA;			// 0011xxxx
		MatrixIndexB matIndexB;				// 0100xxxx
		VCD_Lo vcdLo;				// 0101xxxx
		VCD_Hi vcdHi;				// 0110xxxx
		VAT_group0 vatA[8];			// 0111x,vat[2:0]
		VAT_group1 vatB[8];			// 1000x,vat[2:0]
		VAT_group2 vatC[8];			// 1001x,vat[2:0]	
		ArrayBase arrayBase[(size_t)ArrayId::Max];		// 1010,array[3:0]
		ArrayStride arrayStride[(size_t)ArrayId::Max];	// 1011,array[3:0]
	};

	#pragma pack(pop)

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


namespace GX
{
	class GXCore;

	class FifoProcessor
	{
		size_t fifoSize = 1024 * 1024;
		uint8_t* fifo = nullptr;
		size_t readPtr = 0;
		size_t writePtr = 0;
		bool allocated = false;

		GXCore* gxcore = nullptr;
		SpinLock lock;

	public:
		size_t GetSize();
		bool EnoughToExecute();

		size_t vertexSize[8] = { 0 };
		void RecalcVertexSize();		// Called every time the VCD / VAT settings are changed.

		uint8_t Read8();
		uint16_t Read16();
		uint32_t Read32();
		float ReadFloat();

		uint8_t Peek8(size_t offset);
		uint8_t Peek16(size_t offset);

		void ExecuteCommand();

		FifoProcessor(GXCore* gx);
		FifoProcessor(GXCore* gx, uint8_t* fifoPtr, size_t size);	// Call FIFO
		~FifoProcessor();

		void WriteBytes(uint8_t dataPtr[32]);

		void Reset();
	};

}


void CPOpen();
void CPClose();
