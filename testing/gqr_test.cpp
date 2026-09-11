// Graphics Quantization Register (GQR) unit tests.
//
// The subject is the pure conversion model in src/gqr.h, which the interpreter uses for the
// quantized paired-single load/store instructions (psq_l*, psq_st*). Every expectation below
// is taken from the IBM Gekko RISC Microprocessor User's Manual v1.2 as restated by
// specs/architecture/gekko-registers.md section 4.6 and gekko-isa.md section 5.5:
//
//   * GQR layout: LD_SCALE in bits 2-7, LD_TYPE in 13-15, ST_SCALE in 18-23, ST_TYPE in 29-31
//     (PowerPC bit numbering, bit 0 = MSB);
//   * types: 0 single float (no conversion), 1-3 reserved, 4 u8, 5 u16, 6 s8, 7 s16;
//   * scales are 6-bit two's complement;
//   * load:  F = I * 2^(-S)   (table 2-44 worked examples);
//   * store: I = clamp(trunc(F * 2^S)), rounding toward zero, +Inf/NaN to the positive
//            saturation value, -Inf to the negative one (table 2-45 worked examples).

#include "pch.h"
#include "../src/gqr.h"

namespace GekkoUnitTest
{
	using QuantType = Gekko::QuantType;

	TEST_CLASS(GqrTest)
	{
		// 6-bit two's complement scale field for the signed value s
		static uint8_t S(int s) { return (uint8_t)(s & 0x3F); }

		static float Deq(uint32_t raw, QuantType t, int s) { return Gekko::GqrDequantize(raw, t, S(s)); }
		static uint32_t Qua(float v, QuantType t, int s) { return Gekko::GqrQuantize(v, t, S(s)); }

		// ------------------------------------------------------------------
		// GQR register layout (table 2-16)
		// ------------------------------------------------------------------

		TEST_METHOD(Decode_FieldsSitAtTheDocumentedBitPositions)
		{
			// The manual numbers the bits of the GQR figure from the most significant end (bit 0
			// is the leftmost bit), so "LD_SCALE 2:7" is the field at the top of the word. With
			// the usual least-significant-first numbering the fields are LD_SCALE 24:29,
			// LD_TYPE 16:18, ST_SCALE 8:13 and ST_TYPE 0:2.
			for (int s = -32; s <= 31; s++)
			{
				uint32_t gqr = ((uint32_t)S(s) << 24);
				Assert::AreEqual((int)S(s), (int)Gekko::GqrDecode(gqr).ldScale);
				Assert::AreEqual((int)QuantType::SingleFloat, (int)Gekko::GqrDecode(gqr).ldType);
				Assert::AreEqual(0, (int)Gekko::GqrDecode(gqr).stScale);
				Assert::AreEqual((int)QuantType::SingleFloat, (int)Gekko::GqrDecode(gqr).stType);
			}

			// LD_TYPE in bits 16-18
			for (int t = 0; t < 8; t++)
			{
				uint32_t gqr = ((uint32_t)t << 16);
				Assert::AreEqual(t, (int)Gekko::GqrDecode(gqr).ldType);
			}

			// ST_SCALE in bits 8-13
			for (int s = -32; s <= 31; s++)
			{
				uint32_t gqr = ((uint32_t)S(s) << 8);
				Assert::AreEqual((int)S(s), (int)Gekko::GqrDecode(gqr).stScale);
			}

			// ST_TYPE in bits 0-2
			for (int t = 0; t < 8; t++)
			{
				uint32_t gqr = ((uint32_t)t);
				Assert::AreEqual(t, (int)Gekko::GqrDecode(gqr).stType);
			}
		}

		TEST_METHOD(Decode_TypicalQuantizedVertexFormatGqrs)
		{
			// The values the SDK packs for the quantized vertex formats (u8, u16, s8, s16, all
			// with scale 0) - the packer puts the type in the top field of each half.
			uint32_t gqr = 0x00040004;
			Gekko::GqrFields f = Gekko::GqrDecode(gqr);
			Assert::AreEqual((int)QuantType::U8, (int)f.ldType);
			Assert::AreEqual((int)QuantType::U8, (int)f.stType);
			Assert::AreEqual(0, (int)f.ldScale);
			Assert::AreEqual(0, (int)f.stScale);

			gqr = 0x00070007;
			f = Gekko::GqrDecode(gqr);
			Assert::AreEqual((int)QuantType::S16, (int)f.ldType);
			Assert::AreEqual((int)QuantType::S16, (int)f.stType);

			// A nonzero scale on both halves (u8, scale -3)
			gqr = 0x3D043D04;
			f = Gekko::GqrDecode(gqr);
			Assert::AreEqual((int)QuantType::U8, (int)f.ldType);
			Assert::AreEqual((int)QuantType::U8, (int)f.stType);
			Assert::AreEqual(-3, Gekko::GqrScaleExponent(f.ldScale));
			Assert::AreEqual(-3, Gekko::GqrScaleExponent(f.stScale));
		}

		TEST_METHOD(Decode_ScaleFieldIsSixBitTwosComplement)
		{
			Assert::AreEqual(1.0f, Gekko::GqrStoreScaleFactor(S(0)));
			Assert::AreEqual(0.5f, Gekko::GqrStoreScaleFactor(S(-1)));
			Assert::AreEqual(0.25f, Gekko::GqrStoreScaleFactor(S(-2)));
			Assert::AreEqual(2.0f, Gekko::GqrStoreScaleFactor(S(1)));
			Assert::AreEqual(4.0f, Gekko::GqrStoreScaleFactor(S(2)));
			Assert::AreEqual((float)ldexp(1.0, 31), Gekko::GqrStoreScaleFactor(S(31)));
			Assert::AreEqual((float)ldexp(1.0, -32), Gekko::GqrStoreScaleFactor(S(-32)));
		}

		TEST_METHOD(ElementSize_FollowsTheType)
		{
			Assert::AreEqual(1u, Gekko::GqrElementSize(QuantType::U8));
			Assert::AreEqual(1u, Gekko::GqrElementSize(QuantType::S8));
			Assert::AreEqual(2u, Gekko::GqrElementSize(QuantType::U16));
			Assert::AreEqual(2u, Gekko::GqrElementSize(QuantType::S16));
			Assert::AreEqual(4u, Gekko::GqrElementSize(QuantType::SingleFloat));
		}

		// ------------------------------------------------------------------
		// Load: F = I * 2^(-S)  (table 2-44)
		// ------------------------------------------------------------------

		TEST_METHOD(Load_IntegerOneScalesAsTheManualTable)
		{
			// LD_SCALE -2, -1, 0, +1, +2 -> 4.00, 2.00, 1.00, 0.50, 0.25
			const int scales[5] = { -2, -1, 0, 1, 2 };
			const float expect[5] = { 4.00f, 2.00f, 1.00f, 0.50f, 0.25f };

			for (int i = 0; i < 5; i++)
			{
				Assert::AreEqual(expect[i], Deq(1, QuantType::U8, scales[i]));
				Assert::AreEqual(expect[i], Deq(1, QuantType::U16, scales[i]));
				Assert::AreEqual(expect[i], Deq(1, QuantType::S8, scales[i]));
				Assert::AreEqual(expect[i], Deq(1, QuantType::S16, scales[i]));
			}
		}

		TEST_METHOD(Load_UnsignedTypesSignExtendNothing)
		{
			Assert::AreEqual(255.0f, Deq(0xFF, QuantType::U8, 0));
			Assert::AreEqual(65535.0f, Deq(0xFFFF, QuantType::U16, 0));
			Assert::AreEqual(128.0f, Deq(0x80, QuantType::U8, 0));
		}

		TEST_METHOD(Load_SignedTypesSignExtend)
		{
			Assert::AreEqual(-1.0f, Deq(0xFF, QuantType::S8, 0));
			Assert::AreEqual(-128.0f, Deq(0x80, QuantType::S8, 0));
			Assert::AreEqual(-1.0f, Deq(0xFFFF, QuantType::S16, 0));
			Assert::AreEqual(-32768.0f, Deq(0x8000, QuantType::S16, 0));
			Assert::AreEqual(32767.0f, Deq(0x7FFF, QuantType::S16, 0));
		}

		TEST_METHOD(Load_U16WithScaleZeroBecomesTheFloatRange)
		{
			// The manual's remark: "a u16 with S = 0 becomes the float 0...65535.0"
			Assert::AreEqual(0.0f, Deq(0, QuantType::U16, 0));
			Assert::AreEqual(65535.0f, Deq(65535, QuantType::U16, 0));
		}

		// ------------------------------------------------------------------
		// Store: I = clamp(trunc(F * 2^S))  (table 2-45)
		// ------------------------------------------------------------------

		TEST_METHOD(Store_Float100ScalesAsTheManualTable)
		{
			// 100.0 with ST_SCALE -2, -1, 0, +1, +2 -> 25, 50, 100, 200, 400 (clamped per type)
			Assert::AreEqual(25u, Qua(100.0f, QuantType::U8, -2));
			Assert::AreEqual(50u, Qua(100.0f, QuantType::U8, -1));
			Assert::AreEqual(100u, Qua(100.0f, QuantType::U8, 0));
			Assert::AreEqual(200u, Qua(100.0f, QuantType::U8, 1));
			Assert::AreEqual(255u, Qua(100.0f, QuantType::U8, 2));		// 400 clamps to 255

			Assert::AreEqual(400u, Qua(100.0f, QuantType::U16, 2));

			Assert::AreEqual(25u, Qua(100.0f, QuantType::S8, -2));
			Assert::AreEqual(127u, Qua(100.0f, QuantType::S8, 1));		// 200 clamps to 127
			Assert::AreEqual(127u, Qua(100.0f, QuantType::S8, 2));

			Assert::AreEqual(200u, Qua(100.0f, QuantType::S16, 1));
			Assert::AreEqual(400u, Qua(100.0f, QuantType::S16, 2));
		}

		TEST_METHOD(Store_SaturatesAtTheTypeLimits)
		{
			Assert::AreEqual(0u, Qua(-0.5f, QuantType::U8, 0));
			Assert::AreEqual(0u, Qua(-1.0f, QuantType::U16, 0));
			Assert::AreEqual(255u, Qua(255.9f, QuantType::U8, 0));
			Assert::AreEqual(255u, Qua(256.0f, QuantType::U8, 0));
			Assert::AreEqual(65535u, Qua(65536.0f, QuantType::U16, 0));

			Assert::AreEqual(127u, Qua(127.9f, QuantType::S8, 0));
			Assert::AreEqual(127u, Qua(128.0f, QuantType::S8, 0));
			Assert::AreEqual(0x80u, Qua(-128.0f, QuantType::S8, 0));		// stored as the byte 0x80
			Assert::AreEqual(0x80u, Qua(-128.9f, QuantType::S8, 0));
			Assert::AreEqual(0x80u, Qua(-1000.0f, QuantType::S8, 0));

			Assert::AreEqual(32767u, Qua(32768.0f, QuantType::S16, 0));
			Assert::AreEqual(0x8000u, Qua(-32768.9f, QuantType::S16, 0));
			Assert::AreEqual(0x8000u, Qua(-1.0e9f, QuantType::S16, 0));
		}

		TEST_METHOD(Store_RoundsTowardZero)
		{
			Assert::AreEqual(100u, Qua(100.9f, QuantType::U8, 0));
			Assert::AreEqual(0u, Qua(0.9f, QuantType::U8, 0));
			Assert::AreEqual(99u, Qua(99.999f, QuantType::U8, 0));
			Assert::AreEqual(0x9Cu, Qua(-100.9f, QuantType::S8, 0));		// -100
			Assert::AreEqual((uint32_t)(uint16_t)(int16_t)-100, Qua(-100.9f, QuantType::S16, 0));
		}

		TEST_METHOD(Store_InfinitiesAndNanSaturate)
		{
			float inf = (float)HUGE_VAL;
			float nan = inf - inf;

			Assert::AreEqual(255u, Qua(inf, QuantType::U8, 0));
			Assert::AreEqual(0u, Qua(-inf, QuantType::U8, 0));
			Assert::AreEqual(255u, Qua(nan, QuantType::U8, 0), L"NaN stores as the positive saturation value");

			Assert::AreEqual(65535u, Qua(inf, QuantType::U16, 0));
			Assert::AreEqual(0u, Qua(-inf, QuantType::U16, 0));
			Assert::AreEqual(65535u, Qua(nan, QuantType::U16, 0));

			Assert::AreEqual(127u, Qua(inf, QuantType::S8, 0));
			Assert::AreEqual(0x80u, Qua(-inf, QuantType::S8, 0));
			Assert::AreEqual(127u, Qua(nan, QuantType::S8, 0));

			Assert::AreEqual(32767u, Qua(inf, QuantType::S16, 0));
			Assert::AreEqual(0x8000u, Qua(-inf, QuantType::S16, 0));
			Assert::AreEqual(32767u, Qua(nan, QuantType::S16, 0));
		}

		// ------------------------------------------------------------------
		// Type 0 (single float): no conversion at all
		// ------------------------------------------------------------------

		TEST_METHOD(FloatType_LoadPassesTheBitsThroughAndIgnoresTheScale)
		{
			// "passed through with no conversion" - the scale must not be applied, and
			// denormals must survive untouched.
			uint32_t value = 0x3F800000;			// 1.0f
			Assert::AreEqual(1.0f, Deq(value, QuantType::SingleFloat, 0));
			Assert::AreEqual(1.0f, Deq(value, QuantType::SingleFloat, 2), L"LD_SCALE must be ignored for float data");
			Assert::AreEqual(1.0f, Deq(value, QuantType::SingleFloat, -2));

			uint32_t denormal = 0x00000001;			// smallest positive denormal
			float d;
			memcpy(&d, &denormal, sizeof(d));
			float loaded = Deq(denormal, QuantType::SingleFloat, 0);
			Assert::AreEqual(denormal, *(uint32_t*)&loaded, L"a denormal float load is not flushed");
			Assert::IsTrue(fpclassify(loaded) == FP_SUBNORMAL);

			uint32_t negZero = 0x80000000;
			float nz = Deq(negZero, QuantType::SingleFloat, 0);
			Assert::AreEqual(negZero, *(uint32_t*)&nz, L"-0.0 keeps its sign bit");
		}

		TEST_METHOD(FloatType_StoreKeepsValuesButFlushesDenormalsToZero)
		{
			Assert::AreEqual(0x3F800000u, Qua(1.0f, QuantType::SingleFloat, 0));
			Assert::AreEqual(0x3F800000u, Qua(1.0f, QuantType::SingleFloat, 2), L"ST_SCALE must be ignored for float data");
			Assert::AreEqual(0x40000000u, Qua(2.0f, QuantType::SingleFloat, -2));

			// "a denormal ps1 stores as 0.0 through psq_st and is not restored exactly"
			uint32_t denormal = 0x00000001;
			float d; memcpy(&d, &denormal, sizeof(d));
			uint32_t stored = Qua(d, QuantType::SingleFloat, 0);
			Assert::AreEqual(0x00000000u, stored, L"a denormal float store becomes +0.0");

			Assert::AreEqual(0x80000000u, Qua(-0.0f, QuantType::SingleFloat, 0), L"a true -0.0 is not a denormal");
			Assert::AreEqual(0x00000000u, Qua(0.0f, QuantType::SingleFloat, 0));
		}

		TEST_METHOD(ReservedTypesBehaveAsNoConversion)
		{
			// Types 1..3 are reserved; the model treats them like type 0 (no conversion),
			// which is the only sane reading of "reserved" for an emulator.
			float f = -12.5f;
			uint32_t raw;
			memcpy(&raw, &f, sizeof(raw));

			float loaded = Gekko::GqrDequantize(raw, QuantType::Reserved1, S(-2));
			uint32_t loadedRaw;
			memcpy(&loadedRaw, &loaded, sizeof(loadedRaw));
			Assert::AreEqual(raw, loadedRaw);

			float loaded2 = Gekko::GqrDequantize(raw, QuantType::Reserved2, S(2));
			uint32_t loaded2Raw;
			memcpy(&loaded2Raw, &loaded2, sizeof(loaded2Raw));
			Assert::AreEqual(raw, loaded2Raw);

			Assert::AreEqual(raw, Gekko::GqrQuantize(f, QuantType::Reserved2, S(2)));
		}

		TEST_METHOD(IsIntegerType_CoversOnlyTheFourIntegerCodes)
		{
			Assert::IsFalse(Gekko::GqrIsIntegerType(QuantType::SingleFloat));
			Assert::IsFalse(Gekko::GqrIsIntegerType(QuantType::Reserved1));
			Assert::IsFalse(Gekko::GqrIsIntegerType(QuantType::Reserved2));
			Assert::IsFalse(Gekko::GqrIsIntegerType(QuantType::Reserved3));
			Assert::IsTrue(Gekko::GqrIsIntegerType(QuantType::U8));
			Assert::IsTrue(Gekko::GqrIsIntegerType(QuantType::U16));
			Assert::IsTrue(Gekko::GqrIsIntegerType(QuantType::S8));
			Assert::IsTrue(Gekko::GqrIsIntegerType(QuantType::S16));
		}

		// ------------------------------------------------------------------
		// Round trip: a store followed by a load of the same scale returns the
		// quantized value (the "fixed point round trip" games rely on).
		// ------------------------------------------------------------------

		TEST_METHOD(RoundTrip_IntegerTypesPreserveTheQuantizedValue)
		{
			// A store followed by a load with the same scale returns the value the integer
			// format can hold: exact as long as neither truncation nor saturation bites, so
			// the value and the scale are paired to stay inside the type's range.
			struct Case { float value; QuantType type; int maxScale; };
			const Case cases[] =
			{
				{ 1000.0f, QuantType::S16, 4 },		// 1000 * 16 = 16000, fits
				{ 100.0f,  QuantType::S16, 4 },
				{ 100.0f,  QuantType::S8,  0 },		// 100 * 2 would already clamp
				{ 50.0f,   QuantType::S8,  1 },		// 50 * 2 = 100, fits
				{ 200.0f,  QuantType::U8,  0 },
				{ 100.0f,  QuantType::U8,  1 },		// 100 * 2 = 200, fits
			};

			for (const Case& c : cases)
			{
				for (int s = 0; s <= c.maxScale; s++)
				{
					uint32_t q = Gekko::GqrQuantize(c.value, c.type, S(s));
					Assert::AreEqual(c.value, Gekko::GqrDequantize(q, c.type, S(s)));
				}
			}

			// Negative scales divide: the value must be a multiple of 2^|S| to survive, and
			// what lands in memory is the truncated integer of the type's range.
			for (int s = -8; s < 0; s++)
			{
				uint32_t q = Gekko::GqrQuantize(1024.0f, QuantType::S16, S(s));
				Assert::AreEqual((float)(int16_t)(uint16_t)q * powf(2.0f, (float)-s),
					Gekko::GqrDequantize(q, QuantType::S16, S(s)));
			}
		}
	};
}
