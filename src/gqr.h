// Graphics Quantization Registers (GQR0-GQR7) and the paired-single quantization math.
//
// The pure conversion rules live here, separated from the interpreter, so that they can be
// unit tested without the whole CPU core (see testing/gqr_test.cpp).
//
// Reference: IBM Gekko RISC Microprocessor User's Manual v1.2, tables 2-16 (GQR layout),
// 2-17 (data types), 2-44 (load examples) and 2-45 (store examples); see also
// specs/architecture/gekko-registers.md section 4.6 and gekko-isa.md sections 5.5/5.7.

#pragma once

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

namespace Gekko
{
	// GQR[LD_TYPE] / GQR[ST_TYPE] encoding (table 2-17)
	enum class QuantType
	{
		SingleFloat = 0,	// single-precision floating point, no conversion at all
		Reserved1 = 1,
		Reserved2 = 2,
		Reserved3 = 3,
		U8 = 4,				// unsigned 8-bit integer
		U16 = 5,			// unsigned 16-bit integer
		S8 = 6,				// signed 8-bit integer
		S16 = 7,			// signed 16-bit integer
	};

	// GQR bit fields (table 2-16). The table numbers bits from the least significant one:
	// LD_SCALE 2:7, LD_TYPE 13:15, ST_SCALE 18:23, ST_TYPE 29:31. The manual's own worked
	// example confirms this reading - GQR0 = 0xE000E000 is documented as "s16 loads and
	// stores, scale 0", which is what this decoding produces (and the mirror-image reading
	// would call it "float, scale -32").
	struct GqrFields
	{
		uint8_t ldScale;			// 6-bit two's complement (-32..+31)
		QuantType ldType;
		uint8_t stScale;			// 6-bit two's complement
		QuantType stType;
	};

	inline GqrFields GqrDecode(uint32_t gqr)
	{

		GqrFields f;
		f.ldScale = (uint8_t)((gqr >> 2) & 0x3F);
		f.ldType = (QuantType)((gqr >> 13) & 7);
		f.stScale = (uint8_t)((gqr >> 18) & 0x3F);
		f.stType = (QuantType)((gqr >> 29) & 7);
		return f;
	}

	// -1 for the "no conversion" encodings (single float and the reserved codes)
	inline bool GqrIsIntegerType(QuantType type)
	{
		return (int)type >= 4;
	}

	// Width of one quantized memory operand, in bytes
	inline uint32_t GqrElementSize(QuantType type)
	{
		switch (type)
		{
			case QuantType::U8:
			case QuantType::S8:
				return 1;
			case QuantType::U16:
			case QuantType::S16:
				return 2;
			default:
				return 4;			// single float, and the reserved codes
		}
	}

	// 6-bit two's complement scale -> the signed exponent it stands for
	inline int GqrScaleExponent(uint8_t scale)
	{
		return (scale & 0x20) ? ((int)scale - 64) : (int)scale;
	}

	// Multiplier applied when a store converts a float to an integer: 2^S
	inline float GqrStoreScaleFactor(uint8_t scale)
	{
		return powf(2.0f, (float)GqrScaleExponent(scale));
	}

	// Multiplier applied when a load converts an integer to a float: 2^(-S)
	inline float GqrLoadScaleFactor(uint8_t scale)
	{
		return powf(2.0f, -(float)GqrScaleExponent(scale));
	}

	// Load (dequantization): the raw memory operand becomes the single F = I * 2^(-S).
	// A type-0 operand is not converted at all - no scaling either, and denormals pass
	// through untouched. The reserved codes are treated the same way (unspecified).
	inline float GqrDequantize(uint32_t raw, QuantType type, uint8_t ldScale)
	{
		switch (type)
		{
			case QuantType::U8:
				return (float)(uint8_t)raw * GqrLoadScaleFactor(ldScale);
			case QuantType::U16:
				return (float)(uint16_t)raw * GqrLoadScaleFactor(ldScale);
			case QuantType::S8:
				return (float)(int8_t)(uint8_t)raw * GqrLoadScaleFactor(ldScale);
			case QuantType::S16:
				return (float)(int16_t)(uint16_t)raw * GqrLoadScaleFactor(ldScale);

			default:
			{
				float value;
				memcpy(&value, &raw, sizeof(value));
				return value;
			}
		}
	}

	// Store (quantization): I = clamp(trunc(F * 2^S)) with saturation, round toward zero.
	// +Inf and NaN store as the positive saturation value, -Inf as the negative one; a
	// type-0 store writes a denormal as 0.0 (manual section on the quantized store).
	inline uint32_t GqrQuantize(float value, QuantType type, uint8_t stScale)
	{
		if (!GqrIsIntegerType(type))
		{
			if (fpclassify(value) == FP_SUBNORMAL)
				value = 0.0f;

			uint32_t raw;
			memcpy(&raw, &value, sizeof(raw));
			return raw;
		}

		float scaled = value * GqrStoreScaleFactor(stScale);
		double d = (double)scaled;

		// NaN saturates to the positive end; the range comparisons cannot see it.
		if (scaled != scaled)
			d = 1.0e30;

		switch (type)
		{
			case QuantType::U8:
			{
				if (d < 0.0) d = 0.0;
				if (d > 255.0) d = 255.0;
				return (uint32_t)(uint8_t)(int64_t)d;
			}
			case QuantType::U16:
			{
				if (d < 0.0) d = 0.0;
				if (d > 65535.0) d = 65535.0;
				return (uint32_t)(uint16_t)(int64_t)d;
			}
			case QuantType::S8:
			{
				if (d < -128.0) d = -128.0;
				if (d > 127.0) d = 127.0;
				return (uint32_t)(uint8_t)(int8_t)(int64_t)d;
			}
			default:	// S16
			{
				if (d < -32768.0) d = -32768.0;
				if (d > 32767.0) d = 32767.0;
				return (uint32_t)(uint16_t)(int16_t)(int64_t)d;
			}
		}
	}
}
