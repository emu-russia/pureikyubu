// This module deals with everything related to textures: from Flipper's point of view (loading into TMEM, conversion),
// and from the point of view of graphics backend (texture upload to the real graphics device, bindings).
#pragma once

namespace GX
{

	// Texture offset

	// Texture Culling mode

	// Texture Clip mode

	// Texture Wrap mode
	enum TexWrapMode
	{
		TX_WRAP_CLAMP = 0,
		TX_WRAP_REPEAT,
		TX_WRAP_MIRROR,
	};

	// Texture filter

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

	// Tlut size

	// Indirect texture format

	// Indirect texture bias select

	// Indirect texture alpha select

	// Indirect texture wrap

	// Indirect texture scale


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


	// TODO: Old implementation, will be redone nicely.

	// texture entry
	struct TexEntry
	{
		uint32_t  ramAddr;
		uint8_t* rawData;
		Color* rgbaData;      // allocated
		int fmt, tfmt;
		int w, h, dw, dh;
		float ds, dt;
		uint32_t bind;
	};

	struct S3TC_TEX
	{
		unsigned    t : 2;
	};

	struct S3TC_BLK
	{
		uint16_t     rgb0;       // color 2
		uint16_t     rgb1;       // color 1
		uint8_t      row[4];
	};
}
