// CP: the vertex array fetch (the indexed vertex attributes).
//
// The VCD can describe a vertex attribute as an index into one of the sixteen attribute arrays
// (CP_ARRAY_BASE / CP_ARRAY_STRIDE) instead of a value carried by the vertex itself. This file
// covers that path: the index the vertex carries selects the row of the array, the row is decoded
// with the format the matching VAT field gives, and the result is what the pipeline transforms and
// rasterizes.
//
// Every test draws the same geometry twice through a real display list: once with the attributes
// travelling inside the vertex (the direct path, VCD kind 1) and once with them coming out of the
// attribute arrays (the indexed path, VCD kinds 2 and 3). The two pictures must be the same to the
// last pixel, which is what "the index selects the row and the row is decoded like a direct
// attribute" means. The rows are written into the emulated main memory the way the guest CPU would
// leave them there - in the console's byte order, see the note on EncodedQuad below - and the two
// draws encode the attribute bytes with the very same encoder, so the only difference between them
// is where the CP read the bytes from.
//
// See specs: command-processor.md 2-5, cp.h (VCD/VAT/ArrayBase/ArrayStride) and the YAGCD notes on
// the CP array registers (the attribute numbers of the arrays are ArrayId, not VertexAttr).

#include "pch.h"
#include "gfx_test_common.h"
#include "gfx_report_common.h"

using namespace GfxUnitTest;

namespace pureikyubutest
{
	namespace
	{
		using namespace GfxGallery;

		// ---------------------------------------------------------------------------------------
		// Emulated main memory
		// ---------------------------------------------------------------------------------------

		const uint32_t FifoBase = 0x00010000;
		const uint32_t FifoSize = 0x00001000;

		const uint32_t PosArrayAddr = 0x00500000;
		const uint32_t NrmArrayAddr = 0x00510000;
		const uint32_t ColArrayAddr = 0x00520000;
		const uint32_t TexArrayAddr = 0x00530000;
		const uint32_t TexDataAddr = 0x00540000;

		//! The numbers the CP_ARRAY_BASE / CP_ARRAY_STRIDE registers select. They are the attribute
		//! numbers of the hardware - the colour arrays are 2 and 3 and the texture arrays start at 4
		//! (cp.h, ArrayId) - and they are *not* the VertexAttr (VTX_*) numbers, where VTX_COLOR0 is 4.
		enum ArrayNumber
		{
			ArrayPos = (int)Flipper::ArrayId::Pos,			// 0
			ArrayNrm = (int)Flipper::ArrayId::Nrm,			// 1
			ArrayColor0 = (int)Flipper::ArrayId::Color0,	// 2
			ArrayTex0 = (int)Flipper::ArrayId::Tex0Coord,	// 4
		};

		// ---------------------------------------------------------------------------------------
		// Guest byte order
		// ---------------------------------------------------------------------------------------

		uint32_t Bits(float value)
		{
			uint32_t bits;
			memcpy(&bits, &value, 4);
			return bits;
		}

		void PushU8(std::vector<uint8_t>& out, uint8_t value)
		{
			out.push_back(value);
		}

		//! A 16-bit field of the console's main memory is big-endian, exactly like the FIFO words
		//! (the CP decodes an array row with the same _BYTESWAP_UINT16/32 the FIFO uses).
		void PushU16(std::vector<uint8_t>& out, uint16_t value)
		{
			out.push_back((uint8_t)(value >> 8));
			out.push_back((uint8_t)value);
		}

		void PushU32(std::vector<uint8_t>& out, uint32_t value)
		{
			out.push_back((uint8_t)(value >> 24));
			out.push_back((uint8_t)(value >> 16));
			out.push_back((uint8_t)(value >> 8));
			out.push_back((uint8_t)value);
		}

		int ClampInt(int value, int lo, int hi)
		{
			return value < lo ? lo : (value > hi ? hi : value);
		}

		//! One fixed point component, encoded in the VAT format. The shift is the binary point of
		//! the format: the decoder divides by 2^shift, so the encoder multiplies by it.
		void EncodeFixed(std::vector<uint8_t>& out, int fmt, int shift, float value)
		{
			float scaled = value * (float)(1u << shift);
			int rounded = (int)(scaled + (scaled < 0 ? -0.5f : 0.5f));

			switch (fmt)
			{
				case Flipper::VFMT_U8:	PushU8(out, (uint8_t)ClampInt(rounded, 0, 0xff)); break;
				case Flipper::VFMT_S8:	PushU8(out, (uint8_t)(int8_t)ClampInt(rounded, -0x80, 0x7f)); break;
				case Flipper::VFMT_U16:	PushU16(out, (uint16_t)ClampInt(rounded, 0, 0xffff)); break;
				case Flipper::VFMT_S16:	PushU16(out, (uint16_t)(int16_t)ClampInt(rounded, -0x8000, 0x7fff)); break;
				default:				PushU32(out, Bits(value)); break;		// VFMT_F32
			}
		}

		//! One colour component, encoded in the VAT colour format (the same layouts the CP decodes).
		void EncodeColor(std::vector<uint8_t>& out, int fmt, const Rgba& c)
		{
			switch (fmt)
			{
				case Flipper::VFMT_RGB565:
					PushU16(out, (uint16_t)(((c.R >> 3) << 11) | ((c.G >> 2) << 5) | (c.B >> 3)));
					break;
				case Flipper::VFMT_RGB8:
					PushU8(out, c.R); PushU8(out, c.G); PushU8(out, c.B);
					break;
				case Flipper::VFMT_RGBX8:
					PushU8(out, c.R); PushU8(out, c.G); PushU8(out, c.B); PushU8(out, 0);
					break;
				case Flipper::VFMT_RGBA4:
					PushU16(out, (uint16_t)(((c.R >> 4) << 12) | ((c.G >> 4) << 8) | ((c.B >> 4) << 4) | (c.A >> 4)));
					break;
				case Flipper::VFMT_RGBA6:
					PushU8(out, (uint8_t)(((c.R >> 2) << 2) | (c.G >> 6)));
					PushU8(out, (uint8_t)(((c.G & 0x3f) << 2) | (c.B >> 6)));
					PushU8(out, (uint8_t)(((c.B & 0x3f) << 2) | (c.A >> 6)));
					break;
				default:				// VFMT_RGBA8
					PushU8(out, c.R); PushU8(out, c.G); PushU8(out, c.B); PushU8(out, c.A);
					break;
			}
		}

		// ---------------------------------------------------------------------------------------
		// The test vertex format
		// ---------------------------------------------------------------------------------------

		//! The attribute kinds of one test vertex format: the VCD says which attributes are present
		//! and where they come from (VCD_NONE / VCD_DIRECT / VCD_INDEX8 / VCD_INDEX16), the VAT gives
		//! the component count and the format of each attribute.
		struct Layout
		{
			int posVcd = Flipper::VCD_INDEX8;
			int nrmVcd = Flipper::VCD_NONE;
			int colVcd = Flipper::VCD_NONE;
			int texVcd = Flipper::VCD_NONE;

			int posFmt = Flipper::VFMT_F32;
			int posCount = Flipper::VCNT_POS_XYZ;
			int posShift = 0;
			int posPad = 0;					// bytes of padding between two rows of the array

			int nrmFmt = Flipper::VFMT_S8;
			int nrmCount = Flipper::VCNT_NRM_XYZ;
			int nrmIdx3 = 0;				// nine normals taken as three staggered indices
			int nrmPad = 0;

			int colFmt = Flipper::VFMT_RGBA8;
			int colPad = 0;

			int texFmt = Flipper::VFMT_U8;
			int texCount = Flipper::VCNT_TEX_ST;
			int texShift = 7;
			int texPad = 0;

			bool bytedeq = true;			// the shift applies to the unsigned components

			int posShiftBits() const { return bytedeq ? posShift : 0; }
			int texShiftBits() const { return bytedeq ? texShift : 0; }
		};

		//! The attribute bytes of the four vertices of a test quad, encoded in the layout's formats
		//! and in the console's byte order. The same encoding is used whether the bytes are pushed
		//! into the FIFO (the direct path) or stored in an attribute array (the indexed path), so a
		//! difference between the two pictures can only come from the fetch itself.
		struct EncodedQuad
		{
			std::vector<uint8_t> pos[4];		// one row per vertex, for every attribute
			std::vector<uint8_t> nrm[4];
			std::vector<uint8_t> col[4];
			std::vector<uint8_t> tex[4];
		};

		//! The number of components the VAT asks of an attribute (the count field of the VAT packs
		//! two cases into one bit).
		int PosComponents(const Layout& l) { return l.posCount == Flipper::VCNT_POS_XYZ ? 3 : 2; }
		int TexComponents(const Layout& l) { return l.texCount == Flipper::VCNT_TEX_ST ? 2 : 1; }
		int NrmComponents(const Layout& l) { return l.nrmCount == Flipper::VCNT_NRM_NBT ? 9 : 3; }

		void EncodeQuad(const Layout& l, const float pos[4][3], const Rgba col[4], const float nrm[4][3],
			const float tex[4][2], EncodedQuad& out)
		{
			for (int i = 0; i < 4; i++)
			{
				if (l.posVcd != Flipper::VCD_NONE)
				{
					for (int c = 0; c < PosComponents(l); c++)
					{
						EncodeFixed(out.pos[i], l.posFmt, l.posShiftBits(), pos[i][c]);
					}
				}

				// The normal is dequantised with a fixed binary point of its own (6 for a byte,
				// 14 for a halfword - see the VAT notes in cp.h)
				if (l.nrmVcd != Flipper::VCD_NONE)
				{
					int shift = (l.nrmFmt == Flipper::VFMT_S16 || l.nrmFmt == Flipper::VFMT_U16) ? 14 : 6;

					for (int c = 0; c < NrmComponents(l); c++)
					{
						EncodeFixed(out.nrm[i], l.nrmFmt, shift, nrm[i][c % 3]);
					}
				}

				if (l.colVcd != Flipper::VCD_NONE)
				{
					EncodeColor(out.col[i], l.colFmt, col[i]);
				}

				if (l.texVcd != Flipper::VCD_NONE)
				{
					for (int c = 0; c < TexComponents(l); c++)
					{
						EncodeFixed(out.tex[i], l.texFmt, l.texShiftBits(), tex[i][c]);
					}
				}
			}
		}

		// ---------------------------------------------------------------------------------------
		// The display list
		// ---------------------------------------------------------------------------------------

		class DisplayList
		{
			std::vector<uint8_t> bytes;

		public:
			void U8(uint8_t value) { bytes.push_back(value); }
			void U16(uint16_t value) { U8((uint8_t)(value >> 8)); U8((uint8_t)value); }
			void U32(uint32_t value) { U8((uint8_t)(value >> 24)); U8((uint8_t)(value >> 16)); U8((uint8_t)(value >> 8)); U8((uint8_t)value); }

			void CpReg(uint8_t index, uint32_t value)
			{
				U8((uint8_t)(Flipper::CP_CMD_LOAD_CPREG | 0));
				U8(index);
				U32(value);
			}

			void Draw(uint8_t command, uint16_t vertexCount)
			{
				U8(command);
				U16(vertexCount);
			}

			void Append(const std::vector<uint8_t>& data)
			{
				bytes.insert(bytes.end(), data.begin(), data.end());
			}

			void Align()
			{
				while ((bytes.size() % 32) != 0)
				{
					U8((uint8_t)(Flipper::CP_CMD_NOP | 0));
				}
			}

			const std::vector<uint8_t>& Bytes() const { return bytes; }
		};

		uint32_t BuildVcdLo(const Layout& l, bool indexed)
		{
			// The position, normal and colour0 attribute kinds are the fields 10:9, 12:11 and 14:13
			uint32_t pos = indexed ? (uint32_t)l.posVcd : (l.posVcd ? (uint32_t)Flipper::VCD_DIRECT : 0u);
			uint32_t nrm = indexed ? (uint32_t)l.nrmVcd : (l.nrmVcd ? (uint32_t)Flipper::VCD_DIRECT : 0u);
			uint32_t col = indexed ? (uint32_t)l.colVcd : (l.colVcd ? (uint32_t)Flipper::VCD_DIRECT : 0u);

			return (pos << 9) | (nrm << 11) | (col << 13);
		}

		uint32_t BuildVcdHi(const Layout& l, bool indexed)
		{
			// Tex0Coord is the field 1:0
			uint32_t tex = indexed ? (uint32_t)l.texVcd : (l.texVcd ? (uint32_t)Flipper::VCD_DIRECT : 0u);
			return tex;
		}

		uint32_t BuildVatA(const Layout& l)
		{
			return (uint32_t)l.posCount | ((uint32_t)l.posFmt << 1) | ((uint32_t)l.posShift << 4) |
				((uint32_t)l.nrmCount << 9) | ((uint32_t)l.nrmFmt << 10) |
				((uint32_t)l.colFmt << 14) |
				((uint32_t)l.texCount << 21) | ((uint32_t)l.texFmt << 22) | ((uint32_t)l.texShift << 25) |
				((uint32_t)(l.bytedeq ? 1 : 0) << 30) | ((uint32_t)l.nrmIdx3 << 31);
		}

		struct AttributeArray
		{
			ArrayNumber number;						// the array the CP_ARRAY_BASE/STRIDE registers select
			int vcd;								// the attribute kind of the VCD for this array
			uint32_t address;						// where the rows of this array live
			const std::vector<uint8_t>* rows;		// the four encoded rows
			int pad;								// bytes of padding between two rows
		};

		//! Build the display list of one quad that the CP decodes.
		//!
		//! `indexed` = false: every attribute travels inside the vertex (all the VCD kinds of the
		//! layout are replaced by VCD_DIRECT). `indexed` = true: every attribute whose VCD kind is
		//! VCD_INDEX8 / VCD_INDEX16 comes out of its attribute array - the rows are written into the
		//! emulated main memory first and the vertex carries the index instead of the value.
		//!
		//! `order[i]` is the row the vertex `i` selects, so the arrays also cover the case where the
		//! vertices do not walk the array in order.
		std::vector<uint8_t> BuildQuadList(GfxTestMachine& m, const Layout& l, const EncodedQuad& v,
			const uint8_t order[4], bool indexed)
		{
			const AttributeArray specs[4] = {
				{ ArrayPos,		l.posVcd, PosArrayAddr, v.pos, l.posPad },
				{ ArrayNrm,		l.nrmVcd, NrmArrayAddr, v.nrm, l.nrmPad },
				{ ArrayColor0,	l.colVcd, ColArrayAddr, v.col, l.colPad },
				{ ArrayTex0,	l.texVcd, TexArrayAddr, v.tex, l.texPad },
			};

			AttributeArray arrays[4];
			int arrayCount = 0;

			for (int a = 0; a < 4; a++)
			{
				// An attribute travels inside the vertex unless the layout asks for an index and
				// this draw uses the arrays
				if (!indexed || specs[a].vcd < Flipper::VCD_INDEX8)
				{
					continue;
				}

				arrays[arrayCount++] = specs[a];
			}

			// The rows of the arrays, written the way the guest CPU would leave them in main memory
			for (int a = 0; a < arrayCount; a++)
			{
				size_t rowSize = arrays[a].rows[0].size();

				for (int i = 0; i < 4; i++)
				{
					WriteMainMemory(arrays[a].address + (uint32_t)(i * (rowSize + arrays[a].pad)),
						arrays[a].rows[i].data(), rowSize);
				}
			}

			DisplayList list;
			list.CpReg(Flipper::CP_VCD_LO_ID, BuildVcdLo(l, indexed));
			list.CpReg(Flipper::CP_VCD_HI_ID, BuildVcdHi(l, indexed));
			list.CpReg(Flipper::CP_VAT_A_ID | 0, BuildVatA(l));

			for (int a = 0; a < arrayCount; a++)
			{
				size_t rowSize = arrays[a].rows[0].size();

				list.CpReg((uint8_t)(Flipper::CP_ARRAY_BASE_ID | (int)arrays[a].number), arrays[a].address);
				list.CpReg((uint8_t)(Flipper::CP_ARRAY_STRIDE_ID | (int)arrays[a].number),
					(uint32_t)(rowSize + arrays[a].pad));
			}

			list.Draw((uint8_t)(Flipper::CP_CMD_DRAW_QUAD | 0), 4);
			// The vertices, in the order FifoWalk reads them: position, normal, colour0, texcoord0.
			// An indexed attribute contributes its index (one byte for VCD_INDEX8, two for
			// VCD_INDEX16), a direct one the value itself.
			for (int i = 0; i < 4; i++)
			{
				uint8_t index = order[i];

				if (indexed && l.posVcd >= Flipper::VCD_INDEX8)
				{
					if (l.posVcd == Flipper::VCD_INDEX8) list.U8(index); else list.U16(index);
				}
				else
				{
					list.Append(v.pos[i]);
				}

				if (l.nrmVcd != Flipper::VCD_NONE)
				{
					if (indexed && l.nrmVcd >= Flipper::VCD_INDEX8)
					{
						// The nine normal form of the indexed path is three staggered indices,
						// one per triple (the normal, the binormal and the tangent)
						int triples = (l.nrmCount == Flipper::VCNT_NRM_NBT && l.nrmIdx3) ? 3 : 1;

						for (int t = 0; t < triples; t++)
						{
							if (l.nrmVcd == Flipper::VCD_INDEX8) list.U8(index); else list.U16(index);
						}
					}
					else
					{
						list.Append(v.nrm[i]);
					}
				}

				if (l.colVcd != Flipper::VCD_NONE)
				{
					if (indexed && l.colVcd >= Flipper::VCD_INDEX8)
					{
						if (l.colVcd == Flipper::VCD_INDEX8) list.U8(index); else list.U16(index);
					}
					else
					{
						list.Append(v.col[i]);
					}
				}

				if (l.texVcd != Flipper::VCD_NONE)
				{
					if (indexed && l.texVcd >= Flipper::VCD_INDEX8)
					{
						if (l.texVcd == Flipper::VCD_INDEX8) list.U8(index); else list.U16(index);
					}
					else
					{
						list.Append(v.tex[i]);
					}
				}
			}

			list.Align();

			return list.Bytes();
		}

		//! Put the display list into the emulated main memory, point the FIFO registers at it and walk
		//! it one 32-byte burst at a time, exactly as the CP thread does.
		void RunList(GfxTestMachine& m, const std::vector<uint8_t>& list)
		{
			// The CP keeps its internal FIFO between the tests: anything a previous display list
			// left behind would be decoded again, so every list starts from an empty one.
			m.flipper->cp->CPAbortFifo();
			ClearTestLog();

			uint32_t padded = (uint32_t)((list.size() + 31) & ~31u);
			std::vector<uint8_t> image(padded, 0);
			memcpy(image.data(), list.data(), list.size());
			WriteMainMemory(FifoBase, image.data(), image.size());

			uint32_t top = FifoBase + FifoSize;
			uint32_t write = FifoBase + padded;

			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BASEL, FifoBase & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BASEH, FifoBase >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_TOPL, top & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_TOPH, top >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_WPTRL, write & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_WPTRH, write >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_RPTRL, FifoBase & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_RPTRH, FifoBase >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC);

			for (uint32_t i = 0; i < padded / 32; i++)
			{
				m.flipper->cp->PumpFifo();
			}

			Assert::AreEqual<std::wstring>(L"", Widen(TestLastHalt()), L"the CP halted on the display list");
		}

		// ---------------------------------------------------------------------------------------
		// Reading the picture back
		// ---------------------------------------------------------------------------------------

		void ReadEfb(GfxTestMachine& m, std::vector<uint8_t>& rgb)
		{
			m.ReadColor(0, 0, EfbWidth, EfbHeight, rgb);
		}

		//! How many EFB pixels of the two pictures differ.
		int CountDifferentPixels(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
		{
			int count = 0;

			for (size_t i = 0; i + 2 < a.size() && i + 2 < b.size(); i += 3)
			{
				if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2])
				{
					count++;
				}
			}

			return count;
		}

		void Pixel(const std::vector<uint8_t>& rgb, int x, int y, uint8_t out[3])
		{
			const uint8_t* p = &rgb[((size_t)y * EfbWidth + x) * 3];
			out[0] = p[0]; out[1] = p[1]; out[2] = p[2];
		}

		bool Close(int a, int b, int tolerance)
		{
			int d = a - b;
			return (d < 0 ? -d : d) <= tolerance;
		}

		//! How many different colours the picture is made of (a coarse grid, like the galleries use).
		int CountDistinctColors(const std::vector<uint8_t>& rgb)
		{
			std::set<uint32_t> distinct;

			for (int y = 0; y < EfbHeight; y += 5)
			{
				for (int x = 0; x < EfbWidth; x += 5)
				{
					const uint8_t* p = &rgb[((size_t)y * EfbWidth + x) * 3];
					distinct.insert(((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2]);
				}
			}

			return (int)distinct.size();
		}

		//! The window and the pipeline the array tests draw with: a pass-through XF, the rasterized
		//! vertex colour in the only TEV stage and a flat clear colour.
		GfxTestMachine& Prepare(const Rgba& clear)
		{
			GfxTestMachine& m = M();			// started, reset and required to have a GL context
			EnableTestLog(true);
			ClearTestLog();

			SetupPassThroughXF(m);
			SetupDefaultPixelState(m);
			SetClearColor(m, clear);
			SetupRasterStage0(m);

			return m;
		}

		//! The four positions, colours, normals and texture coordinates of the test quad. It is
		//! deliberately not symmetric: every corner is somewhere else, so a wrong row (or a wrong
		//! index) cannot hide behind the symmetry of the quad.
		struct QuadData
		{
			float pos[4][3];
			Rgba col[4];
			float nrm[4][3];
			float tex[4][2];
		};

		QuadData MakeQuadData()
		{
			QuadData q{};

			const float px[4] = { -0.9f, 0.7f, 0.9f, -0.7f };
			const float py[4] = { -0.8f, -0.9f, 0.8f, 0.9f };

			const Rgba colors[4] = {
				MakeRgba(0xE0, 0x20, 0x30), MakeRgba(0x20, 0xC0, 0x40),
				MakeRgba(0x30, 0x40, 0xE0), MakeRgba(0xF0, 0xE0, 0x30),
			};

			const float tex[4][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

			const float nrm[4][3] = {
				{ -0.6f, -0.6f, 0.5f }, { 0.6f, -0.6f, 0.5f }, { 0.6f, 0.6f, 0.5f }, { -0.6f, 0.6f, 0.5f },
			};

			for (int i = 0; i < 4; i++)
			{
				q.pos[i][0] = px[i];
				q.pos[i][1] = py[i];
				q.pos[i][2] = 0.0f;
				q.col[i] = colors[i];
				q.tex[i][0] = tex[i][0];
				q.tex[i][1] = tex[i][1];

				for (int c = 0; c < 3; c++)
				{
					q.nrm[i][c] = nrm[i][c];
				}
			}

			return q;
		}

		//! The texture of the indexed texture coordinate tests: a 16x16 checkerboard of four well
		//! separated colours, so the sampled texel names the coordinate that selected it.
		void SetupArrayTestTexture(GfxTestMachine& m)
		{
			const Rgba texel[4] = {
				MakeRgba(0xC0, 0x20, 0x20), MakeRgba(0x20, 0xC0, 0x20),
				MakeRgba(0x20, 0x20, 0xC0), MakeRgba(0xE0, 0xC0, 0x20),
			};

			std::vector<uint8_t> raw;

			for (int t = 0; t < 16; t += 2)
			{
				for (int s = 0; s < 16; s += 2)
				{
					const Rgba& c = texel[((s >> 1) & 1) | (((t >> 1) & 1) << 1)];

					for (int v = 0; v < 2; v++)
					{
						for (int u = 0; u < 2; u++)
						{
							raw.push_back(c.R);
							raw.push_back(c.G);
							raw.push_back(c.B);
							raw.push_back(c.A);
						}
					}
				}
			}

			SetupTexture(m, 0, TexDataAddr, 16, 16, GFX::TF_RGBA8, raw.data(), raw.size(),
				1u | (1u << 2));		// repeat in both directions, nearest filter
		}

		const uint8_t IdentityOrder[4] = { 0, 1, 2, 3 };

		//! A vertex order that is not a rotation of the array: walking the rows this way draws a
		//! different polygon, so a picture drawn with it cannot be the one of IdentityOrder unless
		//! the index was ignored.
		const uint8_t SwappedOrder[4] = { 1, 0, 2, 3 };

		//! A rotation of the array: the geometry is the same polygon, so only the attributes that
		//! hang off the vertices (the texture coordinates, the normals) move.
		const uint8_t RotatedOrder[4] = { 2, 3, 0, 1 };
	}

	TEST_CLASS(GfxCpArrayTest)
	{
	public:

		// =========================================================================================
		// The position array
		// =========================================================================================

		//! The reference case: the index in the vertex selects the row of the position array. The
		//! array is padded (a stride larger than a row), which is what an interleaved vertex buffer
		//! looks like to the CP.
		TEST_METHOD(Cp_IndexedPositionSelectsTheArrayRow)
		{
			const Rgba clear = MakeRgba(0x10, 0x10, 0x18);
			GfxTestMachine& m = Prepare(clear);

			QuadData q = MakeQuadData();

			Layout direct;
			direct.posVcd = Flipper::VCD_DIRECT;
			direct.posFmt = Flipper::VFMT_F32;

			Layout indexed = direct;
			indexed.posVcd = Flipper::VCD_INDEX8;
			indexed.posPad = 4;					// the rows are 12 bytes of position in a 16 byte step

			EncodedQuad v;
			EncodeQuad(direct, q.pos, q.col, q.nrm, q.tex, v);

			// The direct draw: the positions travel inside the vertex
			m.BeginFrame();
			RunList(m, BuildQuadList(m, direct, v, IdentityOrder, false));
			std::vector<uint8_t> directPixels;
			ReadEfb(m, directPixels);

			// The indexed draw: the same rows, read out of the position array
			m.BeginFrame();
			RunList(m, BuildQuadList(m, indexed, v, IdentityOrder, true));
			std::vector<uint8_t> indexedPixels;
			ReadEfb(m, indexedPixels);

			// The same geometry reached the EFB through both paths
			Assert::AreEqual(0, CountDifferentPixels(directPixels, indexedPixels),
				L"the indexed position array did not produce the geometry of the direct path");

			// ... and it is the geometry of the test quad, not a blank frame
			uint8_t inside[3], outside[3];
			Pixel(indexedPixels, EfbWidth / 2, EfbHeight / 2, inside);
			Pixel(indexedPixels, 2, 2, outside);
			Assert::AreNotEqual<int>(clear.R, inside[0], L"the quad did not cover the centre of the EFB");

			// The index really selects the row: walking the array out of order draws another quad
			m.BeginFrame();
			RunList(m, BuildQuadList(m, indexed, v, SwappedOrder, true));
			std::vector<uint8_t> rotatedPixels;
			ReadEfb(m, rotatedPixels);

			Assert::IsTrue(CountDifferentPixels(indexedPixels, rotatedPixels) > 1000,
				L"the vertices ignored the index and walked the array in order anyway");
		}

		//! The same, with 16-bit indices and a fixed point position format: the index is a halfword
		//! and the row is decoded with the shift of the VAT.
		TEST_METHOD(Cp_IndexedPosition16DecodesTheVatFormat)
		{
			const Rgba clear = MakeRgba(0x14, 0x18, 0x10);
			GfxTestMachine& m = Prepare(clear);

			QuadData q = MakeQuadData();

			Layout direct;
			direct.posVcd = Flipper::VCD_DIRECT;
			direct.posFmt = Flipper::VFMT_S16;
			direct.posShift = 13;

			Layout indexed = direct;
			indexed.posVcd = Flipper::VCD_INDEX16;

			EncodedQuad v;
			EncodeQuad(direct, q.pos, q.col, q.nrm, q.tex, v);

			m.BeginFrame();
			RunList(m, BuildQuadList(m, direct, v, IdentityOrder, false));
			std::vector<uint8_t> directPixels;
			ReadEfb(m, directPixels);

			m.BeginFrame();
			RunList(m, BuildQuadList(m, indexed, v, IdentityOrder, true));
			std::vector<uint8_t> indexedPixels;
			ReadEfb(m, indexedPixels);

			Assert::AreEqual(0, CountDifferentPixels(directPixels, indexedPixels),
				L"the 16-bit indexed position array did not match the direct path");

			// The halfword indices are what the vertex carries, so a wrong stride or a wrong
			// halfword order would move the quad
			uint8_t inside[3];
			Pixel(indexedPixels, EfbWidth / 2, EfbHeight / 2, inside);
			Assert::AreNotEqual<int>(clear.G, inside[1], L"the quad did not cover the centre of the EFB");
		}

		// =========================================================================================
		// The colour array
		// =========================================================================================

		//! The colour of the vertex comes out of the colour0 array (array 2 of the CP, which is not
		//! the VTX_COLOR0 attribute number). The rasterized vertex colour is what the TEV stage
		//! outputs, so the four rows of the array are the four corners of the picture.
		TEST_METHOD(Cp_IndexedColorArraySelectsTheCornerColours)
		{
			const Rgba clear = MakeRgba(0x18, 0x10, 0x14);
			GfxTestMachine& m = Prepare(clear);

			// A large quad whose corners are well inside the render target: every corner of the
			// picture is then the colour of one row of the array, and the samples stay away from
			// the edge of the window (the outer margins of the read-back are never rendered).
			QuadData q = MakeQuadData();
			const float extent = 0.75f;
			const float px[4] = { -extent, extent, extent, -extent };
			const float py[4] = { -extent, -extent, extent, extent };

			for (int i = 0; i < 4; i++)
			{
				q.pos[i][0] = px[i];
				q.pos[i][1] = py[i];
			}

			Layout direct;
			direct.posVcd = Flipper::VCD_DIRECT;
			direct.colVcd = Flipper::VCD_DIRECT;
			direct.colFmt = Flipper::VFMT_RGBA8;

			Layout indexed = direct;
			indexed.colVcd = Flipper::VCD_INDEX8;

			EncodedQuad v;
			EncodeQuad(direct, q.pos, q.col, q.nrm, q.tex, v);

			m.BeginFrame();
			RunList(m, BuildQuadList(m, direct, v, IdentityOrder, false));
			std::vector<uint8_t> directPixels;
			ReadEfb(m, directPixels);

			m.BeginFrame();
			RunList(m, BuildQuadList(m, indexed, v, IdentityOrder, true));
			std::vector<uint8_t> indexedPixels;
			ReadEfb(m, indexedPixels);

			Assert::AreEqual(0, CountDifferentPixels(directPixels, indexedPixels),
				L"the indexed colour array did not produce the colours of the direct path");

			// Every corner of the picture carries the colour of its own row of the array: the quad
			// is submitted in the fan order the rasterizer expects, so with the clip space of the
			// machine (x to the right, y upwards, the EFB read back from its top row) row 0 is the
			// bottom left corner of the picture and row 3 the top left one.
			const int inset = 10;
			const int x0 = (int)((1.0f - extent) * 0.5f * EfbWidth) + inset;
			const int x1 = EfbWidth - x0 - 1;
			const int y0 = (int)((1.0f - extent) * 0.5f * EfbHeight) + inset;
			const int y1 = EfbHeight - y0 - 1;

			struct Corner { int x, y; int row; };
			const Corner corners[4] = {
				{ x0, y1, 0 },		// bottom left
				{ x1, y1, 1 },
				{ x1, y0, 2 },
				{ x0, y0, 3 },		// top left
			};

			for (const Corner& c : corners)
			{
				uint8_t pixel[3];
				Pixel(indexedPixels, c.x, c.y, pixel);

				const Rgba& expected = q.col[c.row];
				char message[256];
				sprintf_s(message, "corner (%i,%i) is %02X%02X%02X, not row %i (%02X%02X%02X)",
					c.x, c.y, pixel[0], pixel[1], pixel[2], c.row, expected.R, expected.G, expected.B);

				Assert::IsTrue(Close(pixel[0], expected.R, 24) && Close(pixel[1], expected.G, 24) &&
					Close(pixel[2], expected.B, 24), Widen(message).c_str());
			}
		}

		//! The 16-bit index of the colour array, with the 16-bit packed colour format: the row is
		//! a halfword and the decoder swaps it out of the console byte order.
		TEST_METHOD(Cp_IndexedColor16DecodesThePackedFormat)
		{
			const Rgba clear = MakeRgba(0x10, 0x14, 0x18);
			GfxTestMachine& m = Prepare(clear);

			QuadData q = MakeQuadData();

			Layout direct;
			direct.posVcd = Flipper::VCD_DIRECT;
			direct.colVcd = Flipper::VCD_DIRECT;
			direct.colFmt = Flipper::VFMT_RGB565;

			Layout indexed = direct;
			indexed.colVcd = Flipper::VCD_INDEX16;

			EncodedQuad v;
			EncodeQuad(direct, q.pos, q.col, q.nrm, q.tex, v);

			m.BeginFrame();
			RunList(m, BuildQuadList(m, direct, v, IdentityOrder, false));
			std::vector<uint8_t> directPixels;
			ReadEfb(m, directPixels);

			m.BeginFrame();
			RunList(m, BuildQuadList(m, indexed, v, IdentityOrder, true));
			std::vector<uint8_t> indexedPixels;
			ReadEfb(m, indexedPixels);

			Assert::AreEqual(0, CountDifferentPixels(directPixels, indexedPixels),
				L"the 16-bit indexed RGB565 rows did not match the direct path");

			Assert::IsTrue(CountDistinctColors(indexedPixels) >= 6,
				L"the 16-bit packed colours of the array did not reach the TEV");
		}

		// =========================================================================================
		// The texture coordinate array
		// =========================================================================================

		//! The texture coordinate of the vertex comes out of the tex0 array (array 4). The rows are
		//! unsigned bytes, so the VAT shift is where the binary point of the coordinate sits.
		TEST_METHOD(Cp_IndexedTexCoordArraySelectsTheSample)
		{
			const Rgba clear = MakeRgba(0x18, 0x18, 0x10);
			GfxTestMachine& m = Prepare(clear);
			SetupArrayTestTexture(m);
			SetupTextureStage0(m);

			QuadData q = MakeQuadData();

			Layout direct;
			direct.posVcd = Flipper::VCD_DIRECT;
			direct.texVcd = Flipper::VCD_DIRECT;
			direct.texFmt = Flipper::VFMT_U8;
			direct.texShift = 7;

			Layout indexed = direct;
			indexed.texVcd = Flipper::VCD_INDEX16;

			EncodedQuad v;
			EncodeQuad(direct, q.pos, q.col, q.nrm, q.tex, v);

			m.BeginFrame();
			RunList(m, BuildQuadList(m, direct, v, IdentityOrder, false));
			std::vector<uint8_t> directPixels;
			ReadEfb(m, directPixels);

			m.BeginFrame();
			RunList(m, BuildQuadList(m, indexed, v, IdentityOrder, true));
			std::vector<uint8_t> indexedPixels;
			ReadEfb(m, indexedPixels);

			Assert::AreEqual(0, CountDifferentPixels(directPixels, indexedPixels),
				L"the indexed texture coordinate array did not sample like the direct path");

			// The texture is a checkerboard, so the sample proves the coordinate was fetched
			Assert::IsTrue(CountDistinctColors(indexedPixels) >= 4, L"the texture was not sampled");

			// Walking the array out of order samples the texture somewhere else
			m.BeginFrame();
			RunList(m, BuildQuadList(m, indexed, v, RotatedOrder, true));
			std::vector<uint8_t> rotatedPixels;
			ReadEfb(m, rotatedPixels);

			Assert::IsTrue(CountDifferentPixels(indexedPixels, rotatedPixels) > 1000,
				L"the texture coordinates ignored the index");
		}

		// =========================================================================================
		// The normal array
		// =========================================================================================

		//! The normal of the vertex comes out of the normal array (array 1). The lighting of the XF
		//! turns the normal into the vertex colour, so the picture shows the normal the CP fetched.
		TEST_METHOD(Cp_IndexedNormalArraySelectsTheIllumination)
		{
			const Rgba clear = MakeRgba(0x10, 0x10, 0x10);
			GfxTestMachine& m = Prepare(clear);

			QuadData q = MakeQuadData();

			Layout direct;
			direct.posVcd = Flipper::VCD_DIRECT;
			direct.nrmVcd = Flipper::VCD_DIRECT;
			direct.nrmFmt = Flipper::VFMT_S8;

			Layout indexed = direct;
			indexed.nrmVcd = Flipper::VCD_INDEX8;

			EncodedQuad v;
			EncodeQuad(direct, q.pos, q.col, q.nrm, q.tex, v);

			// One diffuse light to the upper right: the illumination of a vertex is its normal,
			// so every row of the normal array lights its own corner of the quad.
			const auto setupLighting = [&m]()
			{
				m.XfLoad(GFX::XF_NUMCOLS_ID, 1);
				m.XfLoad(GFX::XF_MATERIAL0_ID, 0xFFFFFFFF);
				m.XfLoad(GFX::XF_AMBIENT0_ID, 0x202028FF);
				m.XfLoad(GFX::XF_COLOR0CNTL_ID, (1u << 1) | (1u << 2) | (2u << 7));
				m.XfLoad(GFX::XF_ALPHA0CNTL_ID, 0);

				m.XfLoad(GFX::XF_LIGHT0_RGBA_ID, 0xFFE080FF);
				m.XfLoad(GFX::XF_LIGHT0_LPX_ID, Bits(4.0f));
				m.XfLoad(GFX::XF_LIGHT0_LPY_ID, Bits(3.0f));
				m.XfLoad(GFX::XF_LIGHT0_LPZ_ID, Bits(3.0f));
				m.XfLoad(GFX::XF_LIGHT0_K0_ID, Bits(1.0f));
				m.XfLoad(GFX::XF_LIGHT0_K1_ID, Bits(0.6f));
				m.XfLoad(GFX::XF_LIGHT0_K2_ID, Bits(0.4f));
			};

			m.BeginFrame();
			setupLighting();
			RunList(m, BuildQuadList(m, direct, v, IdentityOrder, false));
			std::vector<uint8_t> directPixels;
			ReadEfb(m, directPixels);

			m.BeginFrame();
			setupLighting();
			RunList(m, BuildQuadList(m, indexed, v, IdentityOrder, true));
			std::vector<uint8_t> indexedPixels;
			ReadEfb(m, indexedPixels);

			Assert::AreEqual(0, CountDifferentPixels(directPixels, indexedPixels),
				L"the indexed normal array did not light like the direct path");

			// The picture is the illumination of four different normals, not a flat frame
			Assert::IsTrue(CountDistinctColors(indexedPixels) >= 6,
				L"the normals of the array did not reach the lighting of the XF");

			// The same array read out of order swaps the illumination of the corners
			m.BeginFrame();
			setupLighting();
			RunList(m, BuildQuadList(m, indexed, v, RotatedOrder, true));
			std::vector<uint8_t> rotatedPixels;
			ReadEfb(m, rotatedPixels);

			Assert::IsTrue(CountDifferentPixels(indexedPixels, rotatedPixels) > 1000,
				L"the lighting ignored the normal index");
		}

		// =========================================================================================
		// The vertex walk
		// =========================================================================================

		//! The nine normal form: the VAT asks for the normal, the binormal and the tangent, and the
		//! indexed form of it (nrmidx3) is three staggered indices, one per triple. The attribute
		//! after it (the colour) proves that the vertex walk consumed exactly those bytes: if the
		//! size of the normal attribute were wrong, the colour would be decoded from the wrong place.
		TEST_METHOD(Cp_IndexedNormalsNbtConsumeThreeIndices)
		{
			const Rgba clear = MakeRgba(0x14, 0x10, 0x18);
			GfxTestMachine& m = Prepare(clear);

			QuadData q = MakeQuadData();

			Layout direct;
			direct.posVcd = Flipper::VCD_DIRECT;
			direct.nrmVcd = Flipper::VCD_DIRECT;
			direct.nrmFmt = Flipper::VFMT_S8;
			direct.nrmCount = Flipper::VCNT_NRM_NBT;
			direct.nrmIdx3 = 1;
			direct.colVcd = Flipper::VCD_DIRECT;
			direct.colFmt = Flipper::VFMT_RGBA8;

			// The direct path carries nine normals (the normal, the binormal and the tangent), the
			// indexed one three indices
			EncodedQuad v;
			EncodeQuad(direct, q.pos, q.col, q.nrm, q.tex, v);

			Layout indexed = direct;
			indexed.colVcd = Flipper::VCD_INDEX8;

			m.BeginFrame();
			RunList(m, BuildQuadList(m, direct, v, IdentityOrder, false));
			std::vector<uint8_t> directPixels;
			ReadEfb(m, directPixels);

			// The walk has to consume the right number of bytes whichever width the indices have:
			// three of them for the nine normal form, and the colour that follows proves where the
			// walk ended up
			for (unsigned vcd : { (unsigned)Flipper::VCD_INDEX8, (unsigned)Flipper::VCD_INDEX16 })
			{
				indexed.nrmVcd = (int)vcd;
				indexed.colVcd = (int)vcd;

				m.BeginFrame();
				RunList(m, BuildQuadList(m, indexed, v, IdentityOrder, true));
				std::vector<uint8_t> indexedPixels;
				ReadEfb(m, indexedPixels);

				char message[160];
				sprintf_s(message, "the indexed nine-normal attribute (VCD kind %u) did not consume three indices", vcd);

				Assert::AreEqual(0, CountDifferentPixels(directPixels, indexedPixels), Widen(message).c_str());
				Assert::IsTrue(CountDistinctColors(indexedPixels) >= 6,
					L"the vertex walk lost track of the attributes after the nine normals");
			}
		}

		//! A vertex format that mixes both paths: the position and the texture coordinate come out
		//! of arrays, the colour travels inside the vertex. The picture must be the one the direct
		//! format draws, which is what the CP's vertex walk is for.
		TEST_METHOD(Cp_MixedDirectAndIndexedAttributes)
		{
			const Rgba clear = MakeRgba(0x18, 0x14, 0x10);
			GfxTestMachine& m = Prepare(clear);
			SetupArrayTestTexture(m);
			SetupTextureStage0(m);

			QuadData q = MakeQuadData();

			Layout direct;
			direct.posVcd = Flipper::VCD_DIRECT;
			direct.posFmt = Flipper::VFMT_S16;
			direct.posShift = 12;
			direct.texVcd = Flipper::VCD_DIRECT;
			direct.texFmt = Flipper::VFMT_U8;
			direct.texShift = 7;
			direct.colVcd = Flipper::VCD_DIRECT;
			direct.colFmt = Flipper::VFMT_RGBA8;

			Layout indexed = direct;
			indexed.posVcd = Flipper::VCD_INDEX16;
			indexed.posPad = 4;
			indexed.texVcd = Flipper::VCD_INDEX8;
			indexed.colVcd = Flipper::VCD_DIRECT;		// the colour stays in the vertex

			EncodedQuad v;
			EncodeQuad(direct, q.pos, q.col, q.nrm, q.tex, v);

			m.BeginFrame();
			RunList(m, BuildQuadList(m, direct, v, IdentityOrder, false));
			std::vector<uint8_t> directPixels;
			ReadEfb(m, directPixels);

			m.BeginFrame();
			RunList(m, BuildQuadList(m, indexed, v, IdentityOrder, true));
			std::vector<uint8_t> indexedPixels;
			ReadEfb(m, indexedPixels);

			Assert::AreEqual(0, CountDifferentPixels(directPixels, indexedPixels),
				L"the mixed direct/indexed vertex format did not decode like the direct one");

			Assert::IsTrue(CountDistinctColors(indexedPixels) >= 6,
				L"the mixed vertex format produced no picture");
		}

		//! Three arrays in one vertex format, all of them with eight bit components: the position
		//! is unsigned, the texture coordinate signed and the colour an unpacked 24 bit value. The
		//! eight bit rows are the formats that are *not* byte swapped out of the console order, so
		//! they check the other half of the decoder.
		TEST_METHOD(Cp_IndexedByteRowsDecodeLikeTheDirectPath)
		{
			const Rgba clear = MakeRgba(0x10, 0x18, 0x14);
			GfxTestMachine& m = Prepare(clear);
			SetupArrayTestTexture(m);
			SetupTextureStage0(m);

			QuadData q = MakeQuadData();

			Layout direct;
			direct.posVcd = Flipper::VCD_DIRECT;
			direct.posFmt = Flipper::VFMT_U8;
			direct.posShift = 7;
			direct.texVcd = Flipper::VCD_DIRECT;
			direct.texFmt = Flipper::VFMT_S8;
			direct.texShift = 6;
			direct.colVcd = Flipper::VCD_DIRECT;
			direct.colFmt = Flipper::VFMT_RGB8;

			Layout indexed = direct;
			indexed.posVcd = Flipper::VCD_INDEX8;
			indexed.texPad = 2;						// the two byte coordinate rows sit in a four byte step
			indexed.colPad = 1;						// the three byte colours sit in a four byte step
			indexed.texVcd = Flipper::VCD_INDEX8;
			indexed.colVcd = Flipper::VCD_INDEX16;

			EncodedQuad v;
			EncodeQuad(direct, q.pos, q.col, q.nrm, q.tex, v);

			m.BeginFrame();
			RunList(m, BuildQuadList(m, direct, v, IdentityOrder, false));
			std::vector<uint8_t> directPixels;
			ReadEfb(m, directPixels);

			m.BeginFrame();
			RunList(m, BuildQuadList(m, indexed, v, IdentityOrder, true));
			std::vector<uint8_t> indexedPixels;
			ReadEfb(m, indexedPixels);

			Assert::AreEqual(0, CountDifferentPixels(directPixels, indexedPixels),
				L"the eight bit array rows did not decode like the direct path");

			Assert::IsTrue(CountDistinctColors(indexedPixels) >= 6,
				L"the eight bit array rows produced no picture");
		}
	};
}
