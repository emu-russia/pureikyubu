
#pragma once

namespace GX
{

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
	struct CPRegs
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

	// Vertex attributes

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

	enum class AttrType
	{
		VCD_NONE = 0,           // attribute stage disabled
		VCD_DIRECT,             // direct data
		VCD_INDEX8,				// 8-bit indexed data
		VCD_INDEX16             // 16-bit indexed data (rare)
	};

	// Vertex Components Count (from VAT register)

	enum class VatCount
	{
		VCNT_POS_XY = 0,
		VCNT_POS_XYZ = 1,
		VCNT_NRM_XYZ = 0,
		VCNT_NRM_NBT = 1,    // index is NBT
		VCNT_NRM_NBT3 = 2,    // index is one from N/B/T
		VCNT_CLR_RGB = 0,
		VCNT_CLR_RGBA = 1,
		VCNT_TEX_S = 0,
		VCNT_TEX_ST = 1
	};

	// Vertex Component Format (from VAT register)

	enum class VatFormat
	{
		// For Components (coords)
		VFMT_U8 = 0,
		VFMT_S8 = 1,
		VFMT_U16 = 2,
		VFMT_S16 = 3,
		VFMT_F32 = 4,

		// For Colors
		VFMT_RGB565 = 0,
		VFMT_RGB8 = 1,
		VFMT_RGBX8 = 2,
		VFMT_RGBA4 = 3,
		VFMT_RGBA6 = 4,
		VFMT_RGBA8 = 5
	};

}
