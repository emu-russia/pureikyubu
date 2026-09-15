// This module deals with everything related to textures: from Flipper's point of view (loading into TMEM, conversion),
// and from the point of view of graphics backend (texture upload to the real graphics device, bindings).
//
// The Flipper has 8 texture maps. Every map is decoded from main memory into an RGBA image and kept
// in its own GL texture object, so that a TEV configuration that uses several maps can bind them all
// at once (the per-stage bindings come from RAS1_TREF, see ras.h).
#pragma once

namespace GFX
{
	// Texture
	#define TX_LOADBLOCK0_ID 0x60
	#define TX_LOADBLOCK1_ID 0x61
	#define TX_LOADBLOCK2_ID 0x62
	#define TX_LOADBLOCK3_ID 0x63
	#define TX_LOADTLUT0_ID 0x64    // tlut base in memory
	#define TX_LOADTLUT1_ID 0x65    // tmem ofs and size
	#define TX_INVTAGS_ID 0x66
	#define TX_PERFMODE_ID 0x67
	#define TX_MISC_ID 0x68
	#define TX_REFRESH_ID 0x69

	#define TX_SETMODE0_I0_ID 0x80    // wrap (mode)
	#define TX_SETMODE0_I1_ID 0x81
	#define TX_SETMODE0_I2_ID 0x82
	#define TX_SETMODE0_I3_ID 0x83
	#define TX_SETMODE1_I0_ID 0x84
	#define TX_SETMODE1_I1_ID 0x85
	#define TX_SETMODE1_I2_ID 0x86
	#define TX_SETMODE1_I3_ID 0x87
	#define TX_SETIMAGE0_I0_ID 0x88    // texture width, height, format
	#define TX_SETIMAGE0_I1_ID 0x89
	#define TX_SETIMAGE0_I2_ID 0x8A
	#define TX_SETIMAGE0_I3_ID 0x8B
	#define TX_SETIMAGE1_I0_ID 0x8C
	#define TX_SETIMAGE1_I1_ID 0x8D
	#define TX_SETIMAGE1_I2_ID 0x8E
	#define TX_SETIMAGE1_I3_ID 0x8F
	#define TX_SETIMAGE2_I0_ID 0x90
	#define TX_SETIMAGE2_I1_ID 0x91
	#define TX_SETIMAGE2_I2_ID 0x92
	#define TX_SETIMAGE2_I3_ID 0x93
	#define TX_SETIMAGE3_I0_ID 0x94    // texture_map >> 5, physical address
	#define TX_SETIMAGE3_I1_ID 0x95
	#define TX_SETIMAGE3_I2_ID 0x96
	#define TX_SETIMAGE3_I3_ID 0x97
	#define TX_SETTLUT_I0_ID 0x98    // bind tlut with texture
	#define TX_SETTLUT_I1_ID 0x99
	#define TX_SETTLUT_I2_ID 0x9A
	#define TX_SETTLUT_I3_ID 0x9B

	#define TX_SETMODE0_I4_ID 0xA0
	#define TX_SETMODE0_I5_ID 0xA1
	#define TX_SETMODE0_I6_ID 0xA2
	#define TX_SETMODE0_I7_ID 0xA3
	#define TX_SETMODE1_I4_ID 0xA4
	#define TX_SETMODE1_I5_ID 0xA5
	#define TX_SETMODE1_I6_ID 0xA6
	#define TX_SETMODE1_I7_ID 0xA7
	#define TX_SETIMAGE0_I4_ID 0xA8
	#define TX_SETIMAGE0_I5_ID 0xA9
	#define TX_SETIMAGE0_I6_ID 0xAA
	#define TX_SETIMAGE0_I7_ID 0xAB
	#define TX_SETIMAGE1_I4_ID 0xAC
	#define TX_SETIMAGE1_I5_ID 0xAD
	#define TX_SETIMAGE1_I6_ID 0xAE
	#define TX_SETIMAGE1_I7_ID 0xAF
	#define TX_SETIMAGE2_I4_ID 0xB0
	#define TX_SETIMAGE2_I5_ID 0xB1
	#define TX_SETIMAGE2_I6_ID 0xB2
	#define TX_SETIMAGE2_I7_ID 0xB3
	#define TX_SETIMAGE3_I4_ID 0xB4
	#define TX_SETIMAGE3_I5_ID 0xB5
	#define TX_SETIMAGE3_I6_ID 0xB6
	#define TX_SETIMAGE3_I7_ID 0xB7
	#define TX_SETTLUT_I4_ID 0xB8
	#define TX_SETTLUT_I5_ID 0xB9
	#define TX_SETTLUT_I6_ID 0xBA
	#define TX_SETTLUT_I7_ID 0xBB

	// Texture Wrap mode
	enum TexWrapMode
	{
		TX_WRAP_CLAMP = 0,
		TX_WRAP_REPEAT,
		TX_WRAP_MIRROR,
	};

	// Texture format
	enum TexFormat : size_t
	{
		TF_I4 = 0,
		TF_I8,
		TF_IA4,
		TF_IA8,
		TF_RGB565,
		TF_RGB5A3,
		TF_RGBA8,
		TF_C4 = 8,
		TF_C8,
		TF_C14,
		TF_CMPR = 14    // s3tc
	};

	// Tlut format
	enum TlutFormat : size_t
	{
		TLUT_IA8 = 0,
		TLUT_RGB565,
		TLUT_RGB5A3,
	};

	// texture params
	union TexImage0
	{
		struct
		{
			unsigned width : 10;
			unsigned height : 10;
			unsigned fmt : 4;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	union TexImage1
	{
		struct
		{
			unsigned tmem_offset : 15;
			unsigned cache_width : 3;
			unsigned cache_height : 3;
			unsigned image_type : 1;
			unsigned unused : 2;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	union TexImage2
	{
		struct
		{
			unsigned tmem_offset : 15;
			unsigned cache_width : 3;
			unsigned cache_height : 3;
			unsigned unused : 3;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// texture location
	union TexImage3
	{
		struct
		{
			unsigned base : 21;
			unsigned unused : 3;
			unsigned rid : 8;
		};
		uint32_t bits;
	};

	// texture mode 0
	union TexMode0
	{
		struct
		{
			unsigned wrap_s : 2;		// TexWrapMode
			unsigned wrap_t : 2;		// TexWrapMode
			unsigned mag_filter : 1;
			unsigned min_filter : 3;
			unsigned diaglod_en : 1;
			unsigned lodbias : 8;
			unsigned round : 1;
			unsigned field_predict : 1;
			unsigned maxaniso : 2;
			unsigned lodclamp : 1;
			unsigned unused : 2;
			unsigned rid : 8;
		};
		uint32_t     bits;
	};

	union TexMode1
	{
		struct
		{
			unsigned minlod : 8;
			unsigned maxlod : 8;
			unsigned unused : 8;
			unsigned rid : 8;
		};
		uint32_t     bits;
	};

	// 0x64
	union LoadTlut0
	{
		struct
		{
			unsigned base : 21;
			unsigned unused : 3;
			unsigned rid : 8;
		};
		uint32_t     bits;
	};

	// 0x65
	union LoadTlut1
	{
		struct
		{
			unsigned tmem : 10;
			unsigned count : 11;
			unsigned unused : 3;
			unsigned rid : 8;
		};
		uint32_t     bits;
	};

	// TX_SETTLUT
	union SetTlut
	{
		struct
		{
			unsigned tmem : 10;
			unsigned fmt : 2;		// TlutFormat
			unsigned unused : 12;
			unsigned rid : 8;
		};
		uint32_t     bits;
	};

	struct S3TC_BLK
	{
		uint16_t     rgb0;       // color 2
		uint16_t     rgb1;       // color 1
		uint8_t      row[4];
	};

	struct TXState
	{
		uint32_t loadblock[4]{};	// 0x60-0x63. The emulator decodes textures from main memory on
									// demand, so a preload into TMEM has nothing to do; the register
									// words are kept so that the register file is complete.
		LoadTlut0 loadtlut0;		// 0x64
		LoadTlut1 loadtlut1;		// 0x65
		uint32_t invtags{};			// 0x66
		uint32_t perfmode{};		// 0x67
		uint32_t misc{};			// 0x68
		uint32_t refresh{};			// 0x69
		TexMode0 texmode0[8];		// 0x80-0x83, 0xA0-0xA3
		TexMode1 texmode1[8];		// 0x84-0x87, 0xA4-0xA7
		TexImage0 teximg0[8];		// 0x88-0x8B, 0xA8-0xAB
		TexImage1 teximg1[8];		// 0x8C-0x8F, 0xAC-0xAF
		TexImage2 teximg2[8];		// 0x90-0x93, 0xB0-0xB3
		TexImage3 teximg3[8];		// 0x94-0x97, 0xB4-0xB7
		SetTlut settlut[8];			// 0x98-0x9B, 0xB8-0xBB
	};

	// Texture map (one of the 8 hardware texture maps)
	struct TexMap
	{
		GLuint glTexture = 0;

		bool valid = false;			//!< A texture has been decoded for this map
		bool dirty = false;			//!< Needs decoding before the next draw
		bool paramsDirty = false;	//!< Needs (re)applying the sampler parameters

		// Geometry of the decoded image
		int width = 0, height = 0;		//!< Real texture size
		int dw = 0, dh = 0;				//!< Size of the GL image (power of two)
		float ds = 1.0f, dt = 1.0f;		//!< Texture coordinate scale (real / stored)

		// Size the GL image was last allocated with, so that a re-decode can upload into the
		// existing storage (glTexSubImage2D) instead of reallocating it (glTexImage2D)
		int glWidth = 0, glHeight = 0;

		// What the current GL image was decoded from
		uint32_t keyAddr = 0;
		int keyFmt = -1, keyWidth = 0, keyHeight = 0;
		uint32_t keyTlut = 0xFFFFFFFF;
		uint32_t keyTlutGen = 0;		//!< TLUT generation the image was decoded with
		uint64_t keyHash = 0;			//!< Content hash of the texture bytes the image was decoded from

		uint32_t appliedMode0 = 0xFFFFFFFF;	//!< TexMode0 value the sampler parameters were set from
		uint32_t appliedMode1 = 0xFFFFFFFF;	//!< TexMode1 value the LOD limits were set from
	};

	class TextureEngine
	{
		friend GFXCore;
		friend Rasterizer;
		GFXCore* gfx = nullptr;

		TXState tx{};

		#define GFX_MAX_TEXTURES 8

		TexMap texMap[GFX_MAX_TEXTURES];
		Color rgbabuf[1024 * 1024];
		uint8_t tlut[1024 * 1024];  // TLUT buffer

		//! Bumped by every palette load. A paletted texture is a function of the palette bytes as
		//! well as of its own, so a decoded image is only current while this counter is unchanged.
		uint32_t tlutGeneration = 0;

		bool active = false;

		// -------------------------------------------------------------------------------------
		// Software pipeline (GFX_PIPELINE = soft, issue #384)
		//
		// The software texture unit owns a real TMEM: the 32 x 16K x 16-bit embedded memory of
		// gfx-tc.md 3.1, one 32-byte cache line per (bank, word) column. The explicit load
		// commands stream main-memory tiles into it (3.6) and the hardware-managed images are
		// fetched through a tag cache in it (3.5); the filter datapath of gfx-tf.md 3 then
		// filters the texels it reads - point, bilinear or trilinear, with the format expansion
		// of gfx-tf.md 5.2 and the TLUT dereference of colour-index texels.
		// -------------------------------------------------------------------------------------

		//! The 32 banks of the texture memory, 16K 16-bit words each (1 MB in total).
		static const int SoftTmemBankCount = 32;
		std::vector<uint16_t> tmem;

		//! One tag per line slot of the cache of an image: the main-memory line it holds.
		struct SoftCacheTag
		{
			uint32_t line = 0xFFFFFFFF;
			bool valid = false;
		};

		SoftCacheTag softCacheTags[GFX_MAX_TEXTURES][4096];

		void SoftTmemInit();

		//! One 16-bit word of the texture memory. A line address beyond the low half addresses the
		//! upper half (the two 512 KB halves of gfx-tc.md 3.1).
		uint16_t& SoftTmemWord(uint32_t line, int bank);

		//! Copy one 32-byte line of main memory into the texture memory through the sixteen banks
		//! of a half (a cache line is 16 banks x 16 bits, gfx-tc.md 2.4).
		void SoftWriteLine(uint32_t line, const uint8_t* data);

		//! One byte of a 32-byte TMEM line (its bytes are the sixteen bank words, big-endian).
		uint8_t SoftLineByte(uint32_t line, int index);

		//! `Load_TextureBlock` (0x60-0x63): stream `count` 32-byte tiles of the image at `base`
		//! into the two TMEM offsets the load registers name (gfx-tc.md 3.6, 4.2).
		void SoftLoadBlock(uint32_t base, uint32_t off0, uint32_t off1, uint32_t count, unsigned format);

		//! `Load_TextureTLUT` (0x64/0x65): move `count` blocks of sixteen 16-bit palette entries
		//! into the upper half of the texture memory (gfx-tc.md 3.6, 4.2).
		void SoftLoadTlut(uint32_t base, uint32_t tmemOffset, uint32_t count);

		//! The 32-byte line of the tile `tile` of mip level `level` of an image: its TMEM line
		//! address for a pre-loaded image, or - for a hardware-managed one - the line it was
		//! fetched into through the tag cache.
		bool SoftTileLine(int map, int level, uint32_t tile, bool gb, uint32_t* line);

		//! The 16-bit texel word (or the two words of a 32-bit texel) of a texel, out of TMEM.
		bool SoftFetchTexel(int map, int level, int u, int v, float rgba[4]);



		//! The palette entry a colour index names (gfx-tf.md 3.7, gfx-tc.md 5.4).
		bool SoftTlutEntry(int map, uint32_t index, float rgba[4]);

		//! What DecodeTexture did with a map.
		enum class DecodeResult
		{
			Failed,			//!< The map is not usable (bad address or unknown format)
			Decoded,		//!< rgbabuf holds a new image that has to be uploaded
			Unchanged,		//!< The GL texture already holds exactly this image
		};

		void TexInit();
		void TexFree();
		void GetTlutCol(Color* c, unsigned id, unsigned entry);
		DecodeResult DecodeTexture(int id);
		//! Decode unconditionally into rgbabuf (the debugger dump and the caching decode above).
		bool ConvertTexture(int id);
		//! The byte size of the raw image of a map, as the decoder reads it.
		size_t TextureDataSize(int id);
		void UploadTexture(int id);
		void ApplyTextureParams(int id);
		void LoadTlut(uint32_t addr, uint32_t tmem, uint32_t cnt);
		void InvalidatePalettedTextures();

	public:
		TextureEngine(HWConfig* config, GFXCore* parent_gfx);
		~TextureEngine();

		void loadTXReg(size_t index, uint32_t value);

		//! The TX register state (read-only; used by the debugger and the unit tests).
		const TXState& State() const { return tx; }

		//! The decoded state of a texture map (read-only).
		const TexMap& Map(int id) const { return texMap[id & 7]; }

		//! The raw texture memory (the debugger and the unit tests can look into it).
		const uint16_t* Tmem() const { return tmem.empty() ? nullptr : tmem.data(); }

		//! Sample one texture map with the software texture unit: the coordinate operations and the
		//! level of detail of gfx-tc.md 3.3/3.4 followed by the filter datapath of gfx-tf.md 3.
		//! `coordIndex` names the SU_SSIZE/SU_TSIZE pair the coordinate is scaled by and `deriv`
		//! the screen-space derivatives (ds/dx, dt/dx, ds/dy, dt/dy) the LOD is computed from.
		//! Returns false when the map is not usable.
		bool SoftSample(int map, int coordIndex, float s, float t, const float* deriv, float rgba[4]);

		//! The same sampler with a coordinate that is already in texels. The indirect (bump) fetch
		//! of a TEV stage works in that space (gfx-bump.md 3.3), so it comes through here.
		bool SoftSampleTexel(int map, float uTexel, float vTexel, const float* deriv, float rgba[4]);

		//! Decode and upload all dirty texture maps and bind them to their texture units.
		void UpdateAndBindTextures();

		//! Upload the per-map texture coordinate scales to the TEV program.
		void UploadTexScales(class GLProgram& program);

		//! Decode one texture map into an RGB image (used by the debugger's `gxtexdump` command).
		bool DumpTexture(int id, std::vector<uint8_t>& rgb, int* width, int* height);

		//! Put the TX register state back into the reset state (the GL texture objects survive, the
		//! decoded images are marked as stale).
		void Reset();
	};
}
