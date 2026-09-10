// Rendered-image galleries for the Transform Unit (XF) and the Command Processor (CP).
//
// The XF galleries show what the vertex program does to the geometry: the projection combine, the
// model-view matrix, the viewport registers, per-vertex lighting, texture coordinate generation and
// the indexed (per-vertex matrix) transform. Every picture goes through the real pipeline, so the
// vertex shader of the emulator runs over the vertices the gallery submits.
//
// The CP galleries drive the Command Processor with a real display list in the emulated main memory
// (the FIFO the CPU would fill): every case programs a different VCD/VAT vertex format and the
// picture shows the decoded attributes - the position, the vertex colour and the texture coordinate.
//
// See specs: gfx-xf.md 3.1-3.5 (projection, lighting, texgen), 4 (the XF registers), gfx.md 3 and
// command-processor.md 2-5 (the display list); the VCD/VAT layouts are in cp.h.

#include "pch.h"
#include "gfx_test_common.h"
#include "gfx_report_common.h"

#include <vector>

using namespace GfxUnitTest;

namespace pureikyubutest
{
	namespace
	{
		using namespace GfxGallery;

		//! Where the gallery textures and the CP arrays live in the emulated main memory.
		const uint32_t XfTexAddr = 0x00400000;
		const uint32_t CpDataAddr = 0x00500000;
		const uint32_t CpFifoBase = 0x00010000;
		const uint32_t CpFifoSize = 0x00001000;

		//! The viewport registers are written the way the hardware describes them (gfx-xf.md 4.6): the
		//! scale is half the size with a negative Y, and the offset carries the -342 origin of the
		//! hardware. `x`, `y`, `w` and `h` are the GL viewport the registers should produce.
		void SetViewport(GfxTestMachine& m, float x, float y, float w, float h)
		{
			float scaleX = w / 2.0f;
			float scaleY = -h / 2.0f;
			float offsetX = x + scaleX + 342.0f;
			float offsetY = y - scaleY + 342.0f;

			m.XfLoad(GFX::XF_VIEWPORT_SCALE_X_ID, AsBits(scaleX));
			m.XfLoad(GFX::XF_VIEWPORT_SCALE_Y_ID, AsBits(scaleY));
			m.XfLoad(GFX::XF_VIEWPORT_SCALE_Z_ID, AsBits(16777215.0f));
			m.XfLoad(GFX::XF_VIEWPORT_OFFSET_X_ID, AsBits(offsetX));
			m.XfLoad(GFX::XF_VIEWPORT_OFFSET_Y_ID, AsBits(offsetY));
			m.XfLoad(GFX::XF_VIEWPORT_OFFSET_Z_ID, AsBits(0.0f));
		}

		//! The colour layout of the XF colour registers: the word is (r, g, b, a) from the high byte down.
		uint32_t XfColor(int r, int g, int b, int a)
		{
			return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)a;
		}

		//! Put a row-major 4x4 matrix into the matrix RAM at matrix `matrix`.
		void SetMatrix(GfxTestMachine& m, int matrix, const float* values)
		{
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID + matrix * 4, values, 16);
		}

		//! The identity matrix in the rows of matrix 0.
		void IdentityMatrix(GfxTestMachine& m)
		{
			float identity[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
			SetMatrix(m, 0, identity);
		}

		//! The asymmetric shape the transform galleries draw: three quads that form a flag.
		void DrawFlagShape(GfxTestMachine& m, float dx, float dy, float dz, int matrixIndex = 0)
		{
			struct Quad
			{
				float x0, y0, x1, y1;
				Rgba bl, br, tr, tl;
			};

			const Quad quads[3] = {
				{ -0.30f, -0.25f,  0.30f, -0.10f,		// the bar
					MakeRgba(0xE0, 0x30, 0x20), MakeRgba(0xFF, 0x90, 0x20),
					MakeRgba(0xFF, 0xC0, 0x40), MakeRgba(0xB0, 0x20, 0x10) },
				{ -0.30f, -0.10f, -0.15f,  0.30f,		// the post
					MakeRgba(0x20, 0x80, 0xE0), MakeRgba(0x40, 0xC0, 0xFF),
					MakeRgba(0x20, 0xA0, 0xFF), MakeRgba(0x10, 0x40, 0x90) },
				{ -0.15f,  0.15f,  0.25f,  0.30f,		// the flag
					MakeRgba(0x30, 0xC0, 0x40), MakeRgba(0xC0, 0xFF, 0x40),
					MakeRgba(0xFF, 0xFF, 0x60), MakeRgba(0x20, 0x90, 0x30) },
			};

			for (const Quad& q : quads)
			{
				GFX::Vertex quad[4];
				quad[0] = GfxTestMachine::MakeVertex(q.x0 + dx, q.y0 + dy, dz, q.bl.R, q.bl.G, q.bl.B, q.bl.A);
				quad[1] = GfxTestMachine::MakeVertex(q.x1 + dx, q.y0 + dy, dz, q.br.R, q.br.G, q.br.B, q.br.A);
				quad[2] = GfxTestMachine::MakeVertex(q.x1 + dx, q.y1 + dy, dz, q.tr.R, q.tr.G, q.tr.B, q.tr.A);
				quad[3] = GfxTestMachine::MakeVertex(q.x0 + dx, q.y1 + dy, dz, q.tl.R, q.tl.G, q.tl.B, q.tl.A);

				for (int i = 0; i < 4; i++)
				{
					quad[i].matIdx0.PosNrmMatIdx = matrixIndex;
				}

				m.DrawQuad(quad);
			}
		}

		//! The quad of the texgen galleries. It carries host colours and host texture coordinates and it
		//! asks for texture matrix 4, which is where those galleries put their texture matrix: matrix 0
		//! holds the geometry matrix, and a 4x4 matrix occupies four rows of the matrix RAM, so the two
		//! matrices would overlap if the texture matrix lived in matrix 1.
		void DrawTexGenQuad(GfxTestMachine& m, int texMatrix)
		{
			const Rgba bl = MakeRgba(0x20, 0x40, 0xE0), br = MakeRgba(0xE0, 0xE0, 0x20);
			const Rgba tr = MakeRgba(0x20, 0xE0, 0x40), tl = MakeRgba(0xE0, 0x20, 0x20);

			GFX::Vertex quad[4];
			quad[0] = GfxTestMachine::MakeVertex(-1, -1, 0, bl.R, bl.G, bl.B, bl.A);
			quad[1] = GfxTestMachine::MakeVertex(1, -1, 0, br.R, br.G, br.B, br.A);
			quad[2] = GfxTestMachine::MakeVertex(1, 1, 0, tr.R, tr.G, tr.B, tr.A);
			quad[3] = GfxTestMachine::MakeVertex(-1, 1, 0, tl.R, tl.G, tl.B, tl.A);

			quad[0].TexCoord[0][0] = 0.25f; quad[0].TexCoord[0][1] = 0.25f;
			quad[1].TexCoord[0][0] = 0.75f; quad[1].TexCoord[0][1] = 0.25f;
			quad[2].TexCoord[0][0] = 0.75f; quad[2].TexCoord[0][1] = 0.75f;
			quad[3].TexCoord[0][0] = 0.25f; quad[3].TexCoord[0][1] = 0.75f;

			for (int i = 0; i < 4; i++)
			{
				quad[i].matIdx0.Tex0MatIdx = texMatrix;
			}

			m.DrawQuad(quad);
		}

		//! A disc of `segments` triangles whose normals fan out from the centre, so the per-vertex
		//! lighting of the emulator varies across it: the subject of the lighting galleries.
		void DrawLightedDisc(GfxTestMachine& m, float cx, float cy, float radius, int segments = 48)
		{
			std::vector<GFX::Vertex> vertices;

			GFX::Vertex center = GfxTestMachine::MakeVertex(cx, cy, 0, 0xFF, 0xFF, 0xFF, 0xFF);
			center.Normal[0] = 0.0f; center.Normal[1] = 0.0f; center.Normal[2] = 1.0f;
			vertices.push_back(center);

			for (int i = 0; i <= segments; i++)
			{
				float a = (float)i * 6.2831853f / (float)segments;
				float nx = cosf(a), ny = sinf(a);

				GFX::Vertex v = GfxTestMachine::MakeVertex(cx + nx * radius, cy + ny * radius, 0,
					0xFF, 0xFF, 0xFF, 0xFF);
				v.Normal[0] = nx;
				v.Normal[1] = ny;
				v.Normal[2] = 0.8f;
				vertices.push_back(v);
			}

			m.DrawPrimitive(GFX::RAS_TRIANGLE_FAN, vertices.data(), vertices.size());
		}

		//! A checkerboard over a colour ramp, so a texture coordinate transformation is visible.
		std::vector<uint8_t> CheckerTexture(int size, int tiles)
		{
			std::vector<uint8_t> raw;

			for (int t = 0; t < size; t += 4)
				for (int s = 0; s < size; s += 4)
				{
					for (int v = 0; v < 4; v++)
						for (int u = 0; u < 4; u++)
						{
							int x = s + u, y = t + v;
							bool white = (((x * tiles) / size) + ((y * tiles) / size)) % 2 != 0;
							raw.push_back(0xFF);
							raw.push_back((uint8_t)(white ? (x * 255 / (size - 1)) : 0x20));
						}

					for (int v = 0; v < 4; v++)
						for (int u = 0; u < 4; u++)
						{
							int x = s + u, y = t + v;
							bool white = (((x * tiles) / size) + ((y * tiles) / size)) % 2 != 0;
							raw.push_back((uint8_t)(white ? (y * 255 / (size - 1)) : 0x40));
							raw.push_back((uint8_t)(white ? 0xF0 : 0x60));
						}
				}

			return raw;
		}

		//! The display list builder of the CP gallery: the FIFO carries its words big-endian.
		class DisplayList
		{
			std::vector<uint8_t> bytes;

		public:
			void U8(uint8_t value) { bytes.push_back(value); }
			void U16(uint16_t value) { U8((uint8_t)(value >> 8)); U8((uint8_t)value); }
			void U32(uint32_t value) { U8((uint8_t)(value >> 24)); U8((uint8_t)(value >> 16)); U8((uint8_t)(value >> 8)); U8((uint8_t)value); }
			void F32(float value) { uint32_t bits; memcpy(&bits, &value, 4); U32(bits); }

			//! A bypass register load (the register id in the top byte, the 24-bit payload below it).
			void BpReg(uint8_t index, uint32_t value)
			{
				U8((uint8_t)(Flipper::CP_CMD_LOAD_BPREG | 0));
				U32(((uint32_t)index << 24) | (value & 0xffffff));
			}

			//! A CP register load: the VCD, the VAT and the array bases go through this one.
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

			void Align()
			{
				while ((bytes.size() % 32) != 0)
				{
					U8((uint8_t)(Flipper::CP_CMD_NOP | 0));
				}
			}

			const std::vector<uint8_t>& Bytes() const { return bytes; }
		};

		//! Put the display list into the emulated main memory and point the FIFO registers at it.
		int SetupFifo(GfxTestMachine& m, const std::vector<uint8_t>& list)
		{
			uint32_t padded = (uint32_t)((list.size() + 31) & ~31u);

			std::vector<uint8_t> image(padded, 0);
			memcpy(image.data(), list.data(), list.size());
			WriteMainMemory(CpFifoBase, image.data(), image.size());

			uint32_t top = CpFifoBase + CpFifoSize;
			uint32_t write = CpFifoBase + padded;

			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BASEL, CpFifoBase & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_BASEH, CpFifoBase >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_TOPL, top & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_TOPH, top >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_WPTRL, write & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_WPTRH, write >> 16);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_RPTRL, CpFifoBase & 0xffe0);
			PIRegWrite(PI_REGSPACE_CP | CP_FIFO_RPTRH, CpFifoBase >> 16);

			PIRegWrite(PI_REGSPACE_CP | CP_ENABLE, CP_CR_RDEN | CP_CR_WPINC);

			return (int)(padded / 32);
		}

		//! Walk the FIFO one 32-byte burst at a time, exactly as the CP thread does.
		void RunFifo(GfxTestMachine& m, int bursts)
		{
			for (int i = 0; i < bursts; i++)
			{
				m.flipper->cp->PumpFifo();
			}
		}

		//! The vertex format of one CP gallery case: the VCD gives the attribute types, the VAT the
		//! format and the component count of each attribute (cp.h).
		struct VertexFormat
		{
			int vcdPosition;		// AttrType: 1 = direct, 2 = index8
			int vcdColor;			// AttrType
			int vcdTexCoord;		// AttrType
			int posFmt;				// VatCompFormat
			int posShift;
			int posCount;			// 0 = XY, 1 = XYZ
			int colFmt;				// VatColorFormat
			int texFmt;				// VatCompFormat
			int texShift;
			int texCount;			// 0 = S only, 1 = ST
			bool bytedeq;
		};

		uint32_t BuildVcdLo(const VertexFormat& f)
		{
			// Position is the field 9:10 and Color0 the field 13:14
			return ((uint32_t)f.vcdPosition << 9) | ((uint32_t)f.vcdColor << 13);
		}

		uint32_t BuildVcdHi(const VertexFormat& f)
		{
			return (uint32_t)f.vcdTexCoord;			// Tex0Coord is the field 1:0
		}

		uint32_t BuildVatA(const VertexFormat& f)
		{
			return (uint32_t)f.posCount | ((uint32_t)f.posFmt << 1) | ((uint32_t)f.posShift << 4) |
				((uint32_t)f.colFmt << 14) | ((uint32_t)f.texCount << 21) | ((uint32_t)f.texFmt << 22) |
				((uint32_t)f.texShift << 25) | ((uint32_t)f.bytedeq << 30);
		}

		//! The colour bytes of one vertex in the format of the case.
		void PushColor(DisplayList& list, const VertexFormat& f, const Rgba& c)
		{
			switch (f.colFmt)
			{
				case 0:		// RGB565
					list.U16((uint16_t)(((c.R >> 3) << 11) | ((c.G >> 2) << 5) | (c.B >> 3)));
					break;
				case 1:		// RGB8
					list.U8(c.R); list.U8(c.G); list.U8(c.B);
					break;
				case 2:		// RGBX8
					list.U8(c.R); list.U8(c.G); list.U8(c.B); list.U8(0);
					break;
				case 3:		// RGBA4
					list.U16((uint16_t)(((c.R >> 4) << 12) | ((c.G >> 4) << 8) | ((c.B >> 4) << 4) | (c.A >> 4)));
					break;
				case 4:		// RGBA6
					list.U8((uint8_t)((c.R >> 2) << 2 | (c.G >> 6)));
					list.U8((uint8_t)((c.G & 0x3F) << 2 | (c.B >> 6)));
					list.U8((uint8_t)((c.B & 0x3F) << 2 | (c.A >> 6)));
					break;
				default:	// RGBA8
					list.U8(c.R); list.U8(c.G); list.U8(c.B); list.U8(c.A);
					break;
			}
		}

		//! The position bytes of one vertex in the format of the case.
		void PushPosition(DisplayList& list, const VertexFormat& f, float x, float y, float z)
		{
			float scale = (float)(1 << f.posShift);

			if (f.posFmt == 4)					// VFMT_F32
			{
				list.F32(x);
				list.F32(y);
				if (f.posCount == 1) list.F32(z);
			}
			else if (f.posFmt == 3)				// VFMT_S16
			{
				list.U16((uint16_t)(int16_t)(x * scale));
				list.U16((uint16_t)(int16_t)(y * scale));
				if (f.posCount == 1) list.U16((uint16_t)(int16_t)(z * scale));
			}
			else								// VFMT_U8
			{
				list.U8((uint8_t)(x * scale));
				list.U8((uint8_t)(y * scale));
				if (f.posCount == 1) list.U8((uint8_t)(z * scale));
			}
		}

		//! The texture coordinate bytes of one vertex in the format of the case.
		void PushTexCoord(DisplayList& list, const VertexFormat& f, float s, float t)
		{
			float scale = (float)(1 << f.texShift);

			if (f.texFmt == 4)					// VFMT_F32
			{
				list.F32(s);
				if (f.texCount == 1) list.F32(t);
			}
			else if (f.texFmt == 3)				// VFMT_S16
			{
				list.U16((uint16_t)(int16_t)(s * scale));
				if (f.texCount == 1) list.U16((uint16_t)(int16_t)(t * scale));
			}
			else								// VFMT_U8
			{
				list.U8((uint8_t)(s * scale));
				if (f.texCount == 1) list.U8((uint8_t)(t * scale));
			}
		}

		// ---------------------------------------------------------------------------------------
		// The indexed vertex attributes (CP_ARRAY_BASE / CP_ARRAY_STRIDE)
		// ---------------------------------------------------------------------------------------

		//! The array numbers the CP_ARRAY_BASE / CP_ARRAY_STRIDE registers select (cp.h, ArrayId).
		//! They are the attribute numbers of the hardware and *not* the VertexAttr (VTX_*) ones: the
		//! colour arrays are 2 and 3, and the texture arrays start at 4.
		enum CpArrayNumber
		{
			CpArrPos = (int)Flipper::ArrayId::Pos,			// 0
			CpArrNrm = (int)Flipper::ArrayId::Nrm,			// 1
			CpArrColor0 = (int)Flipper::ArrayId::Color0,	// 2
			CpArrTex0 = (int)Flipper::ArrayId::Tex0Coord,	// 4
		};

		//! Where the attribute arrays of this gallery live in the emulated main memory.
		const uint32_t CpArrayAddr = 0x00580000;

		//! The rows of one attribute array of an indexed picture.
		struct CpArrayData
		{
			int number = CpArrPos;			// the register number of the array
			uint32_t address = CpArrayAddr;
			int stride = 0;					// the distance between two rows, in bytes
			int rowCount = 4;				// how many rows the array holds
			std::vector<uint8_t> rows[8];	// the encoded rows themselves
		};

		int ClampByte(int value)
		{
			return value < 0 ? 0 : (value > 255 ? 255 : value);
		}

		//! One fixed point component of an array row. The console is a big-endian machine and main
		//! memory holds the bytes the guest CPU wrote there, so every multi-byte field of a row is
		//! big-endian - exactly like the words of the FIFO. A row written in the host byte order
		//! decodes to subnormal zeros (no geometry at all), which is what the first attempt at this
		//! gallery did.
		void RowFixed(std::vector<uint8_t>& row, int fmt, int shift, float value)
		{
			float scaled = value * (float)(1u << shift);
			int rounded = (int)(scaled + (scaled < 0 ? -0.5f : 0.5f));

			switch (fmt)
			{
				case Flipper::VFMT_U8:
					row.push_back((uint8_t)ClampByte(rounded));
					break;

				case Flipper::VFMT_S8:
					row.push_back((uint8_t)(int8_t)(rounded < -128 ? -128 : (rounded > 127 ? 127 : rounded)));
					break;

				case Flipper::VFMT_U16:
				case Flipper::VFMT_S16:
				{
					uint16_t half = (uint16_t)(int16_t)(rounded < -0x8000 ? -0x8000 : (rounded > 0x7fff ? 0x7fff : rounded));
					row.push_back((uint8_t)(half >> 8));
					row.push_back((uint8_t)half);
					break;
				}

				default:						// VFMT_F32
				{
					uint32_t bits;
					memcpy(&bits, &value, 4);
					row.push_back((uint8_t)(bits >> 24));
					row.push_back((uint8_t)(bits >> 16));
					row.push_back((uint8_t)(bits >> 8));
					row.push_back((uint8_t)bits);
					break;
				}
			}
		}

		//! One colour of an array row, in the layout of the VAT colour format.
		void RowColor(std::vector<uint8_t>& row, int fmt, const Rgba& c)
		{
			switch (fmt)
			{
				case 0:		// RGB565
				{
					uint16_t half = (uint16_t)(((c.R >> 3) << 11) | ((c.G >> 2) << 5) | (c.B >> 3));
					row.push_back((uint8_t)(half >> 8));
					row.push_back((uint8_t)half);
					break;
				}

				case 1:		// RGB8
					row.push_back(c.R); row.push_back(c.G); row.push_back(c.B);
					break;

				case 2:		// RGBX8
					row.push_back(c.R); row.push_back(c.G); row.push_back(c.B); row.push_back(0);
					break;

				case 3:		// RGBA4
				{
					uint16_t half = (uint16_t)(((c.R >> 4) << 12) | ((c.G >> 4) << 8) | ((c.B >> 4) << 4) | (c.A >> 4));
					row.push_back((uint8_t)(half >> 8));
					row.push_back((uint8_t)half);
					break;
				}

				default:	// RGBA8
					row.push_back(c.R); row.push_back(c.G); row.push_back(c.B); row.push_back(c.A);
					break;
			}
		}

		//! Write the rows of the arrays into the emulated main memory (each row at
		//! `address + index * stride`, which is what the CP computes) and program the array base and
		//! stride registers of the display list.
		void ProgramArrays(GfxTestMachine& m, DisplayList& list, const CpArrayData* arrays, int count)
		{
			for (int a = 0; a < count; a++)
			{
				for (int i = 0; i < arrays[a].rowCount; i++)
				{
					WriteMainMemory(arrays[a].address + (uint32_t)(i * arrays[a].stride),
						arrays[a].rows[i].data(), arrays[a].rows[i].size());
				}

				list.CpReg((uint8_t)(Flipper::CP_ARRAY_BASE_ID | arrays[a].number), arrays[a].address);
				list.CpReg((uint8_t)(Flipper::CP_ARRAY_STRIDE_ID | arrays[a].number), (uint32_t)arrays[a].stride);
			}
		}

		//! A direct attribute of the gallery pictures: the bytes a vertex carries itself.
		void PushRgba8(DisplayList& list, const Rgba& c)
		{
			list.U8(c.R); list.U8(c.G); list.U8(c.B); list.U8(c.A);
		}

		void PushF32x3(DisplayList& list, float x, float y, float z)
		{
			list.F32(x); list.F32(y); list.F32(z);
		}

		//! The VAT word of one attribute: the component count, the format, the shift and the
		//! dequantisation bit. The fields are where cp.h (VAT_group0) puts them.
		uint32_t VatPos(int count, int fmt, int shift)
		{
			return (uint32_t)count | ((uint32_t)fmt << 1) | ((uint32_t)shift << 4);
		}

		//! The ByteDequant bit of the VAT (bit 30): with it set, the shift applies to the unsigned
		//! byte / halfword components of the position and the texture coordinates.
		uint32_t VatByteDequant()
		{
			return 1u << 30;
		}

		uint32_t VatNrm(int count, int fmt)
		{
			return ((uint32_t)count << 9) | ((uint32_t)fmt << 10);
		}

		uint32_t VatColor(int fmt)
		{
			return (uint32_t)fmt << 14;
		}

		uint32_t VatTex0(int count, int fmt, int shift)
		{
			return ((uint32_t)count << 21) | ((uint32_t)fmt << 22) | ((uint32_t)shift << 25);
		}

		//! The VCD_Lo word: the position, normal and colour0 kinds sit in the fields 10:9, 12:11 and
		//! 14:13 (cp.h, VCD_Lo).
		uint32_t VcdLo(int pos, int nrm, int col0)
		{
			return ((uint32_t)pos << 9) | ((uint32_t)nrm << 11) | ((uint32_t)col0 << 13);
		}
	}

	TEST_CLASS(GfxReportXfTests)
	{
	public:

		// =========================================================================================
		// The projection
		// =========================================================================================

		// The projection combine (gfx-xf.md 3.2): the orthographic form against the perspective one,
		// whose homogeneous component is -eye.z. The scene is three copies of the flag at three
		// depths, so the two modes are told apart by whether a copy changes size with its depth.
		TEST_METHOD(Report_XfProjection)
		{
			GfxTestMachine& m = M();

			Report::Section("The projection combine",
				"Three copies of the same shape at three depths. With the orthographic mode (XF_PROJECT_ORTHO set)\n"
				"the clip position does not depend on the depth, so all three copies are the same size; with the\n"
				"perspective mode the homogeneous component is -eye.z, so the copies shrink with distance. The third\n"
				"picture is the perspective mode with a larger A, which stretches everything horizontally.");

			struct Case
			{
				const char* file;
				const char* title;
				bool ortho;
				float a, c, e, f;
				const char* note;
			};

			const Case cases[] = {
				{ "xf_proj_ortho.png", "orthographic", true, 1.0f, 1.0f, 0.0f, 0.0f,
					"Clip x = eye x and clip z = 0, so the depth of a copy has no effect on its size at all; the two\n"
					"perspective pictures of this gallery show what happens when it does." },
				{ "xf_proj_perspective.png", "perspective", false, 1.0f, 1.0f, -1.0f, -0.5f,
					"Clip x = eye x and the homogeneous component is the distance, so the far copies shrink.\n"
					"Because the depth shrinks them, they also drift towards the middle of the screen." },
				{ "xf_proj_perspective_wide.png", "perspective with A = 2", false, 2.0f, 1.0f, -1.0f, -0.5f,
					"The horizontal projection parameter stretches the whole scene." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0x10, 0x10, 0x18));

				m.XfLoad(GFX::XF_PROJECTION_A_ID, AsBits(c.a));
				m.XfLoad(GFX::XF_PROJECTION_B_ID, AsBits(0.0f));
				m.XfLoad(GFX::XF_PROJECTION_C_ID, AsBits(c.c));
				m.XfLoad(GFX::XF_PROJECTION_D_ID, AsBits(0.0f));
				m.XfLoad(GFX::XF_PROJECTION_E_ID, AsBits(c.e));
				m.XfLoad(GFX::XF_PROJECTION_F_ID, AsBits(c.f));
				m.XfLoad(GFX::XF_PROJECT_ORTHO_ID, AsBits(c.ortho ? 1.0f : 0.0f));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BeginFrame();

				DrawBackground(m);
				DrawFlagShape(m, -1.0f, 0.0f, -2.0f);
				DrawFlagShape(m, 0.0f, 0.0f, -3.0f);
				DrawFlagShape(m, 1.0f, 0.0f, -4.0f);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The model-view matrix
		// =========================================================================================

		// The geometry transform (gfx-xf.md 3.1): the position is multiplied by the 4x4 matrix the
		// vertex selects, so these pictures show a rotation, a scale, a translation and a shear.
		TEST_METHOD(Report_XfModelView)
		{
			GfxTestMachine& m = M();

			Report::Section("The model-view matrix",
				"One picture per transform of the same asymmetric shape (a flag of three quads). The matrix is\n"
				"written into the matrix RAM at the rows of matrix index 0, which is the matrix a vertex with the\n"
				"geometry matrix index 0 selects; the projection is the identity, so the picture is that matrix.");

			struct Case
			{
				const char* file;
				const char* title;
				float matrix[16];
				const char* note;
			};

			const Case cases[] = {
				{ "xf_mv_rot30.png", "rotation of 30 degrees about Z",
					{ 0.866f, -0.5f, 0, 0,  0.5f, 0.866f, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 },
					"The two upper rows are the cosine and the sine of the angle." },
				{ "xf_mv_rot75.png", "rotation of 75 degrees about Z",
					{ 0.259f, -0.966f, 0, 0,  0.966f, 0.259f, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 },
					"A larger angle, so the flag is nearly on its side." },
				{ "xf_mv_scale.png", "scale of (1.6, 0.6, 1)",
					{ 1.6f, 0, 0, 0,  0, 0.6f, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 },
					"The diagonal scales every axis on its own: the flag gets wider and flatter." },
				{ "xf_mv_translate.png", "translation of (0.6, 0.35, 0)",
					{ 1, 0, 0, 0.6f,  0, 1, 0, 0.35f,  0, 0, 1, 0,  0, 0, 0, 1 },
					"The fourth column translates the shape, because it multiplies the 1.0 of the homogeneous\n"
					"coordinate." },
				{ "xf_mv_shear.png", "shear of 0.8 in X",
					{ 1, 0.8f, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 },
					"The off-diagonal term leans the shape to the right by 0.8 times its height." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0x10, 0x10, 0x18));

				SetMatrix(m, 0, c.matrix);

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BeginFrame();

				DrawBackground(m);
				DrawFlagShape(m, 0, 0, 0);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The viewport registers
		// =========================================================================================

		// XF_VIEWPORT_SCALE / XF_VIEWPORT_OFFSET drive the GL viewport and the depth range
		// (gfx-xf.md 4.6). The pictures draw the same scene through three viewports.
		TEST_METHOD(Report_XfViewport)
		{
			GfxTestMachine& m = M();

			Report::Section("The viewport registers",
				"The same scene through three XF viewports. The registers describe half the size of the viewport and\n"
				"the offset of its centre, in the -342 space of the hardware; the tests set them so that the GL\n"
				"viewport is the whole EFB, a quadrant of it and a rectangle in the middle. Whatever is outside the\n"
				"viewport is never rasterized, so the frame clear colour stays there.");

			struct Case
			{
				const char* file;
				const char* title;
				float x, y, w, h;
				const char* note;
			};

			const Case cases[] = {
				{ "xf_vp_full.png", "the whole EFB", 0, 0, 640, 480,
					"The scene fills the frame, and the flag is where the projection put it." },
				{ "xf_vp_quadrant.png", "the top left quadrant", 0, 240, 320, 240,
					"Everything is drawn into a quarter of the frame, so the same scene is half as large." },
				{ "xf_vp_inset.png", "a rectangle in the middle", 160, 120, 320, 240,
					"The viewport is inset, and the scene follows it instead of the EFB." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0x08, 0x08, 0x10));

				SetViewport(m, c.x, c.y, c.w, c.h);

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BeginFrame();

				DrawBackground(m);
				DrawFlagShape(m, 0, 0, 0);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// Lighting
		// =========================================================================================

		// The per-vertex lighting of the XF (gfx-xf.md 3.3): the material and ambient sources, the
		// channel control and the light records. The subject is a disc whose normals fan out from its
		// centre, so the illumination varies across it.
		TEST_METHOD(Report_XfLighting)
		{
			GfxTestMachine& m = M();

			Report::Section("Per-vertex lighting",
				"A disc whose normals fan out from its centre, so the illumination changes across it. The channel\n"
				"control (XF_COLOR0CNTL) selects where the material and the ambient colour come from, whether the\n"
				"illumination is used at all and how the diffuse term is computed, and the light records give the\n"
				"colour and the position of every light.");

			struct Case
			{
				const char* file;
				const char* title;
				uint32_t colorCtl;
				uint32_t material;
				uint32_t ambient;
				Rgba light0;
				float light0Pos[3];
				Rgba light1;
				float light1Pos[3];
				const char* note;
			};

			const Rgba none = MakeRgba(0, 0, 0);

			const Case cases[] = {
				{ "xf_light_off.png", "the illumination is off", 0, XfColor(0xE0, 0x60, 0x20, 0xFF),
					XfColor(0x20, 0x20, 0x40, 0xFF), none, { 0,0,0 }, none, { 0,0,0 },
					"LightFunc = 0 leaves the illumination at 1.0, so both discs are the material colour." },
				{ "xf_light_ambient.png", "ambient only", (1u << 1),
					XfColor(0xE0, 0x60, 0x20, 0xFF), XfColor(0x30, 0x80, 0xC0, 0xFF), none, { 0,0,0 }, none, { 0,0,0 },
					"LightFunc = 1 uses the illumination, which is the ambient colour when no light is enabled." },
				{ "xf_light_diffuse.png", "one diffuse light", (1u << 1) | (1u << 2) | (2u << 7),
					XfColor(0xFF, 0xFF, 0xFF, 0xFF), XfColor(0x20, 0x20, 0x30, 0xFF),
					MakeRgba(0xFF, 0xE0, 0x80), { 4.0f, 3.0f, 3.0f }, none, { 0,0,0 },
					"DiffuseAtten = 2 clamps N.L to [0, 1] and the light sits to the upper right, so that side of\n"
					"the disc is the brightest." },
				{ "xf_light_two.png", "two lights", (1u << 1) | (1u << 2) | (1u << 3) | (2u << 7),
					XfColor(0xFF, 0xFF, 0xFF, 0xFF), XfColor(0x10, 0x10, 0x18, 0xFF),
					MakeRgba(0xFF, 0x40, 0x30), { 4.0f, 3.0f, 3.0f },
					MakeRgba(0x30, 0x60, 0xFF), { -4.0f, -3.0f, 3.0f },
					"Two lights on opposite sides add their contributions, so the disc is lit from both sides." },
				{ "xf_light_atten.png", "a light with distance attenuation",
					(1u << 1) | (1u << 2) | (1u << 9) | (2u << 7),
					XfColor(0xFF, 0xFF, 0xFF, 0xFF), XfColor(0x10, 0x10, 0x18, 0xFF),
					MakeRgba(0xFF, 0xFF, 0x40), { 1.2f, 0.9f, 0.6f }, none, { 0,0,0 },
					"Atten = 1 divides the light by k0 + k1*d + k2*d*d; this light is very close to the disc, so\n"
					"the near side is much brighter than the far one." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0x10, 0x10, 0x18));

				m.XfLoad(GFX::XF_NUMCOLS_ID, 1);
				m.XfLoad(GFX::XF_MATERIAL0_ID, c.material);
				m.XfLoad(GFX::XF_AMBIENT0_ID, c.ambient);
				m.XfLoad(GFX::XF_COLOR0CNTL_ID, c.colorCtl);
				m.XfLoad(GFX::XF_ALPHA0CNTL_ID, 0);

				m.XfLoad(GFX::XF_LIGHT0_RGBA_ID, XfColor(c.light0.R, c.light0.G, c.light0.B, c.light0.A));
				m.XfLoad(GFX::XF_LIGHT0_LPX_ID, AsBits(c.light0Pos[0]));
				m.XfLoad(GFX::XF_LIGHT0_LPY_ID, AsBits(c.light0Pos[1]));
				m.XfLoad(GFX::XF_LIGHT0_LPZ_ID, AsBits(c.light0Pos[2]));
				m.XfLoad(GFX::XF_LIGHT0_K0_ID, AsBits(1.0f));
				m.XfLoad(GFX::XF_LIGHT0_K1_ID, AsBits(0.6f));
				m.XfLoad(GFX::XF_LIGHT0_K2_ID, AsBits(0.4f));

				m.XfLoad(GFX::XF_LIGHT1_ID + 3, XfColor(c.light1.R, c.light1.G, c.light1.B, c.light1.A));
				m.XfLoad(GFX::XF_LIGHT1_ID + 0xA, AsBits(c.light1Pos[0]));
				m.XfLoad(GFX::XF_LIGHT1_ID + 0xB, AsBits(c.light1Pos[1]));
				m.XfLoad(GFX::XF_LIGHT1_ID + 0xC, AsBits(c.light1Pos[2]));
				m.XfLoad(GFX::XF_LIGHT1_ID + 7, AsBits(1.0f));

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BeginFrame();

				// The background is a host colour gradient, so it is drawn before the lighting of this
				// gallery is switched on: a lit channel replaces the host colour with the material and
				// the illumination, which would make the background flat as well
				m.XfLoad(GFX::XF_NUMCOLS_ID, 0);
				DrawBackground(m);

				m.XfLoad(GFX::XF_NUMCOLS_ID, 1);
				m.XfLoad(GFX::XF_MATERIAL0_ID, c.material);
				m.XfLoad(GFX::XF_AMBIENT0_ID, c.ambient);
				m.XfLoad(GFX::XF_COLOR0CNTL_ID, c.colorCtl);

				DrawLightedDisc(m, -0.5f, 0.0f, 0.75f);
				DrawLightedDisc(m, 0.5f, 0.0f, 0.45f);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// Texture coordinate generation
		// =========================================================================================

		// The texgen of the XF (gfx-xf.md 3.4): a regular transformation by a texture matrix (with and
		// without the projection bit) and the colour texgen. The sampler uses the generated coordinate,
		// so the pictures show it through a checkerboard texture.
		TEST_METHOD(Report_XfTexgen)
		{
			GfxTestMachine& m = M();

			Report::Section("Texture coordinate generation",
				"One picture per texgen mode, with the generated coordinate handed to a checkerboard texture (the\n"
				"sampler repeats, so a coordinate outside [0, 1] shows more of the pattern). The source row of the\n"
				"regular modes is the position, so the texture follows the geometry; the projection bit divides the\n"
				"coordinate by the third row of the matrix, and the colour texgen takes the coordinate from the host\n"
				"colour instead of the position.");

			struct Case
			{
				const char* file;
				const char* title;
				uint32_t texGen;
				float matrix[12];
				const char* note;
			};

			const Case cases[] = {
				{ "xf_texgen_identity.png", "regular, identity matrix", 0,
					{ 1,0,0,0,  0,1,0,0,  0,0,1,0 },
					"The coordinate is the position, so the pattern is pinned to the shape." },
				{ "xf_texgen_scale.png", "regular, matrix scale (0.5, 0.5)", 0,
					{ 0.5f,0,0,0,  0,0.5f,0,0,  0,0,1,0 },
					"Halving the coordinate doubles the size of the pattern." },
				{ "xf_texgen_translate.png", "regular, matrix offset (0.25, 0.5)", 0,
					{ 1,0,0,0.25f,  0,1,0,0.5f,  0,0,1,0 },
					"The constant column shifts the coordinate, which slides the pattern." },
				{ "xf_texgen_projected.png", "regular with the projection bit", (1u << 1),
					{ 0.5f,0,0,1,  0,1,0,0,  0,0,1,1 },
					"With the projection bit the coordinate is divided by the third row of the matrix, which here is\n"
					"the z of the position plus one." },
				{ "xf_texgen_color.png", "colour texgen from colour 0", (2u << 4),
					{ 1,0,0,0,  0,1,0,0,  0,0,1,0 },
					"(s, t) = (r, g:b) of the host colour, so the pattern is driven by the vertex colours." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetClearColor(m, MakeRgba(0x10, 0x10, 0x18));

				std::vector<uint8_t> raw = CheckerTexture(32, 4);
				SetupTexture(m, 0, XfTexAddr, 32, 32, GFX::TF_RGBA8, raw.data(), raw.size(),
					1u | (1u << 2));						// wrap_s = wrap_t = repeat
				SetupTextureStage0(m, 0);

				// The texture matrix lives in matrix 4: matrix 0 holds the geometry matrix, and a 4x4
				// matrix occupies four rows of the matrix RAM
				const int texMatrix = 4;
				m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID + texMatrix * 4, c.matrix, 12);
				m.XfLoad(GFX::XF_NUMTEX_ID, 1);
				m.XfLoad(GFX::XF_TEXGEN0_ID, c.texGen | (1u << 2));	// in_form = 1 (the full position)

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BeginFrame();

				DrawBackgroundThenTextureStage(m);

				// The quad carries host colours (which the colour texgen uses) and host texture
				// coordinates (which the regular texgen replaces)
				DrawTexGenQuad(m, texMatrix);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// The Rev B dual texture transform (gfx-xf.md 3.4, XF_DUALTEX / XF_DUALGEN): a second matrix is
		// applied to the generated coordinate.
		TEST_METHOD(Report_XfDualTexture)
		{
			GfxTestMachine& m = M();

			Report::Section("The dual texture transform",
				"The regular texgen of the previous gallery with the Rev B dual transform enabled: the generated\n"
				"coordinate is multiplied by a second matrix before the sampler sees it. The dual matrix here is a\n"
				"rotation with a shift, so the checkerboard is turned and offset instead of following the shape.");

			SetupPassThroughXF(m);
			SetClearColor(m, MakeRgba(0x10, 0x10, 0x18));

			std::vector<uint8_t> raw = CheckerTexture(32, 4);
			SetupTexture(m, 0, XfTexAddr, 32, 32, GFX::TF_RGBA8, raw.data(), raw.size(),
				1u | (1u << 2));
			SetupTextureStage0(m, 0);

			// The regular texgen: the position itself
			const int texMatrix = 4;
			float regular[12] = { 1,0,0,0,  0,1,0,0,  0,0,1,0 };
			m.XfLoadFloats(GFX::XF_MATRIX_MEMORY_ID + texMatrix * 4, regular, 12);
			m.XfLoad(GFX::XF_NUMTEX_ID, 1);
			m.XfLoad(GFX::XF_TEXGEN0_ID, 0u | (1u << 2));

			// The dual matrix: a rotation of 30 degrees plus a shift
			float dual[8] = {
				0.866f, -0.5f, 0, 0.15f,
				0.5f, 0.866f, 0, -0.1f,
			};
			m.XfLoadFloats(GFX::XF_DUALTEX_MATRIX_MEMORY_ID, dual, 8);
			m.XfLoad(GFX::XF_DUALTEX_ID, 1);
			m.XfLoad(GFX::XF_DUALGEN0_ID, 0);

			m.BpLoad(PE_ZMODE_ID, 0);
			m.BeginFrame();

			DrawBackgroundThenTextureStage(m);
			DrawTexGenQuad(m, texMatrix);

			Publish(m, "the dual texture transform", "xf_dual_texture.png",
				"The coordinate is the position (the same texgen as the identity picture of the texgen gallery), but\n"
				"the dual matrix rotates and shifts it, so the checkerboard is turned relative to the shape instead of\n"
				"following it.");
		}

		// =========================================================================================
		// The geometry matrix index of a vertex
		// =========================================================================================

		// The matrix index of a vertex (VCD PosNrmMatIdx, gfx-xf.md 3.1, XF_MATINDEX_A) selects which 4x4
		// matrix of the matrix RAM transforms it, so one draw can place the same shape in several places.
		TEST_METHOD(Report_XfMatrixIndex)
		{
			GfxTestMachine& m = M();

			Report::Section("The geometry matrix index of a vertex",
				"The matrix RAM holds 64 rows of four words, i.e. sixteen 4x4 matrices, and the matrix index of a\n"
				"vertex selects which of them transforms it. Both pictures draw the same three shapes: in the first\n"
				"one every vertex uses matrix 0 (the identity) and in the second one the second and the third shape\n"
				"use matrix 1, which rotates and scales, so the index decides the transform inside a single draw.");

			struct Case
			{
				const char* file;
				const char* title;
				bool useIndex;
				const char* note;
			};

			const Case cases[] = {
				{ "xf_matidx_zero.png", "every vertex uses matrix 0", false,
					"All three shapes are where their object coordinates put them." },
				{ "xf_matidx_one.png", "the second and third shapes use matrix 1", true,
					"Matrix 1 rotates by 40 degrees and scales by 1.2, so those two shapes move and turn while the\n"
					"first one stays where it was." },
			};

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupRasterStage0(m);
				SetClearColor(m, MakeRgba(0x10, 0x10, 0x18));

				IdentityMatrix(m);

				// Matrix 1 (rows 4-7): a rotation of 40 degrees scaled by 1.2
				float matrix1[16] = {
					0.919f, -0.643f, 0, 0.2f,
					0.643f, 0.919f, 0, -0.1f,
					0, 0, 1.2f, 0,
					0, 0, 0, 1,
				};
				SetMatrix(m, 4, matrix1);

				m.BpLoad(PE_ZMODE_ID, 0);
				m.BeginFrame();

				for (int i = 0; i < 3; i++)
				{
					int index = (c.useIndex && i > 0) ? 4 : 0;
					DrawFlagShape(m, -1.0f + i * 1.0f, 0.0f, 0.0f, index);
				}

				Publish(m, c.title, c.file, c.note);
			}
		}
	};

	TEST_CLASS(GfxReportCpTests)
	{
	public:

		// The vertex formats of the command processor
		// =========================================================================================

		// The CP unpacks the vertices of a draw command according to the VCD (which attribute is direct
		// or indexed) and the VAT (the format and the component count of each attribute). The pictures
		// of this gallery are drawn from a real display list in the emulated main memory, so they show
		// what the CP decoded: the position format changes the shape, the colour format the gradient and
		// the texture coordinate format the sampling.
		TEST_METHOD(Report_CpVertexFormats)
		{
			GfxTestMachine& m = M();

			Report::Section("The vertex formats of the command processor",
				"One picture per VCD/VAT configuration. Every picture is a quad drawn from a real display list in the\n"
				"emulated main memory, so the geometry, the colours and the texture coordinates are the ones the CP\n"
				"decoded out of the FIFO: with a fixed point position format the quad changes size (the shift of the\n"
				"VAT is where the binary point sits), a 16-bit colour format quantises the gradient, and the texture\n"
				"coordinate format decides where the texture is sampled. The attributes of these pictures all travel\n"
				"inside the vertex (the VCD kind \"direct\"); the other source of an attribute, the index into one of\n"
				"the attribute arrays, has its own section below.");

			struct Case
			{
				const char* file;
				const char* title;
				VertexFormat format;
				bool textured;
				bool centred;			// the quad of this case is half the size and centred
				bool halfTex;			// the texture coordinate of this case only reaches half of the texture
				const char* note;
			};

			const Case cases[] = {
				{ "cp_pos_f32.png", "position as three floats",
					{ 1, 1, 0, 4, 0, 1, 5, 4, 0, 1, false }, false, false, false,
					"The reference: the quad covers the whole EFB, because its coordinates are -1 and 1." },
				{ "cp_pos_s16.png", "position as three S16 with a shift of 12",
					{ 1, 1, 0, 3, 12, 1, 5, 4, 0, 1, true }, false, true, false,
					"A shift of twelve bits puts the binary point so that -4096 is -1.0; this case draws a quad of\n"
					"half the size, because that is what its vertex data says. The fixed point format is exact as\n"
					"long as the shift matches the range of the coordinates." },
				{ "cp_pos_u8.png", "position as three U8 with a shift of 7",
					{ 1, 1, 0, 0, 7, 1, 5, 4, 0, 1, true }, false, false, false,
					"An unsigned byte cannot be negative, so this quad sits in the corner of the clip space that the\n"
					"format can reach: the format decides what a program is able to draw." },
				{ "cp_col_rgb565.png", "colour as RGB565",
					{ 1, 1, 0, 4, 0, 1, 0, 4, 0, 1, false }, false, false, false,
					"Five and six bit components, so the four corner colours are quantised to that grid and the\n"
					"gradient between them shows the steps." },
				{ "cp_col_rgba4.png", "colour as RGBA4",
					{ 1, 1, 0, 4, 0, 1, 3, 4, 0, 1, false }, false, false, false,
					"Four bits per component, the coarsest of the colour formats, so the gradient is a visible ladder\n"
					"of steps. RGB8 and RGBA8 are not separate pictures here because both decode to the same eight\n"
					"bit colour, which is what the position cases of this gallery use." },
				{ "cp_col_rgba6.png", "colour as RGBA6",
					{ 1, 1, 0, 4, 0, 1, 4, 4, 0, 1, false }, false, false, false,
					"Six bits per component, packaged as three bytes: finer than RGB565, a little short of eight bit." },
				{ "cp_tex_u8.png", "texture coordinate as one U8",
					{ 1, 1, 1, 4, 0, 1, 5, 0, 7, 1, true }, true, false, true,
					"The coordinate is a byte with a shift of seven and this quad only asks for half of the texture,\n"
					"which is what a byte can address comfortably: the picture is the lower left corner of the\n"
					"texture, magnified." },
				{ "cp_tex_f32.png", "texture coordinate as two floats",
					{ 1, 1, 1, 4, 0, 1, 5, 4, 0, 1, false }, true, false, false,
					"The coordinate reaches 1.0 at the far corner, so the whole texture is used." },
				{ "cp_tex_s_only.png", "only the S coordinate (count = 1)",
					{ 1, 1, 1, 4, 0, 1, 5, 4, 0, 0, false }, true, false, false,
					"The VAT says the texture coordinate has one component, so T is zero for every vertex and the\n"
					"texture is sampled along its bottom row." },

			};

			// The texture of the textured cases: a fine checkerboard with a ramp in the rows and columns
			{
				std::vector<uint8_t> raw;

				for (int t = 0; t < 16; t += 4)
					for (int s = 0; s < 16; s += 4)
					{
						for (int v = 0; v < 4; v++)
							for (int u = 0; u < 4; u++)
							{
								int x = s + u, y = t + v;
								raw.push_back(0xFF);
								raw.push_back((uint8_t)((((x >> 1) + (y >> 1)) & 1) ? 0xF0 : 0x20));
							}

						for (int v = 0; v < 4; v++)
							for (int u = 0; u < 4; u++)
							{
								int x = s + u, y = t + v;
								raw.push_back((uint8_t)(y * 16));
								raw.push_back((uint8_t)(x * 16));
							}
					}

				SetupTexture(m, 0, CpDataAddr + 0x10000, 16, 16, GFX::TF_RGBA8, raw.data(), raw.size(),
					1u | (1u << 2));
			}

			for (const Case& c : cases)
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0x10, 0x10, 0x18));

				if (c.textured)
					SetupTextureStage0(m, 0);
				else
					SetupRasterStage0(m);

				// The quad of the picture: an unsigned byte position cannot be negative, so that case
				// draws the quad the format is able to reach
				const bool unsignedPos = (c.format.posFmt == 0);
				const float half = c.centred ? 0.5f : 1.0f;
				const float x0 = unsignedPos ? 0.0f : -half;
				const float y0 = unsignedPos ? 0.0f : -half;

				// The corner colours are deliberately "odd" values: a 5/6 bit format quantises them
				// visibly, while an eight bit format keeps them
				const Rgba colors[4] = {
					MakeRgba(0x1F, 0x37, 0x63), MakeRgba(0xC9, 0x2B, 0x57),
					MakeRgba(0x3D, 0xE1, 0x1B), MakeRgba(0x8F, 0x6D, 0xBB),
				};
				const float px[4] = { x0, half, half, x0 };
				const float py[4] = { y0, y0, half, half };
				const float texRange = c.halfTex ? 0.5f : 1.0f;
				const float ps[4] = { 0.0f, texRange, texRange, 0.0f };
				const float pt[4] = { 0.0f, 0.0f, texRange, texRange };

				// The position array of the indexed case
				if (c.format.vcdPosition == 2)
				{
					float positions[4][3];

					for (int i = 0; i < 4; i++)
					{
						positions[i][0] = px[i];
						positions[i][1] = py[i];
						positions[i][2] = 0.0f;
					}

					// The rows of an attribute array are in the byte order of the console (see the
					// indexed gallery below), not in the order a host memcpy would leave them in
					WriteMainMemory(CpDataAddr, positions, sizeof(positions));
				}

				m.BeginFrame();

				DisplayList list;
				list.CpReg(Flipper::CP_VCD_LO_ID, BuildVcdLo(c.format));
				list.CpReg(Flipper::CP_VCD_HI_ID, BuildVcdHi(c.format));
				list.CpReg(Flipper::CP_VAT_A_ID | 0, BuildVatA(c.format));

				if (c.format.vcdPosition == 2)
				{
					list.CpReg(Flipper::CP_ARRAY_BASE_ID | Flipper::VTX_POS, CpDataAddr);
					list.CpReg(Flipper::CP_ARRAY_STRIDE_ID | Flipper::VTX_POS, 12);
				}

				list.Draw((uint8_t)(Flipper::CP_CMD_DRAW_QUAD | 0), 4);

				for (int i = 0; i < 4; i++)
				{
					if (c.format.vcdPosition == 2)
						list.U8((uint8_t)i);
					else
						PushPosition(list, c.format, px[i], py[i], 0.0f);

					PushColor(list, c.format, colors[i]);

					if (c.format.vcdTexCoord == 1)
						PushTexCoord(list, c.format, ps[i], pt[i]);
				}

				list.Align();

				int bursts = SetupFifo(m, list.Bytes());
				RunFifo(m, bursts);

				Publish(m, c.title, c.file, c.note);
			}
		}

		// =========================================================================================
		// The indexed vertex attributes (CP_ARRAY_BASE / CP_ARRAY_STRIDE)
		// =========================================================================================

		// The VCD can describe a vertex attribute as an index into one of the sixteen attribute
		// arrays instead of a value the vertex carries itself: the index selects the row
		// (arrayBase + index * arrayStride) and the row is decoded with the format of the matching
		// VAT field, exactly like a direct attribute of the same format. The index is 8 or 16 bits
		// wide (the VCD kinds 2 and 3 - the kinds 0 and 1 are "absent" and "direct").
		//
		// Every picture below is a real display list in the emulated main memory: the CP reads the
		// rows out of that memory itself. The rows are written in the byte order of the console (a
		// big-endian Gekko), which is the same order the FIFO carries: the CP decodes an indexed row
		// with the very byte swaps it uses for the FIFO words. This is what the first version of the
		// gallery got wrong - it stored the rows with a host memcpy, so the decoder saw subnormal
		// zeros and the draw produced no geometry at all, without any CP halt (the CP was reading
		// the rows it was pointed at; they were simply not the rows the picture meant).
		TEST_METHOD(Report_CpIndexedArrays)
		{
			GfxTestMachine& m = M();

			Report::Section("The indexed vertex attributes (the attribute arrays)",
				"One picture per attribute kind that the VCD can make an index into an array. The vertex only carries\n"
				"the index; the row it selects (arrayBase + index * arrayStride, in the emulated main memory the CP\n"
				"reads) is what the pipeline transforms, lights or samples. The rows are stored the way the guest CPU\n"
				"leaves them in main memory - the console is big-endian - so the CP decodes them with the same byte\n"
				"swaps it uses for the FIFO.");

			// ---- the position out of an 8 bit indexed array -------------------------------------
			// The stride is 16 while a row is 12 bytes: the array has padding between the rows, which
			// is what an interleaved vertex buffer looks like to the CP.
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0x12, 0x14, 0x20));
				SetupRasterStage0(m);

				const float px[4] = { -0.85f, 0.55f, 0.90f, -0.40f };
				const float py[4] = { -0.60f, -0.85f, 0.50f, 0.85f };
				const Rgba colors[4] = {
					MakeRgba(0xE0, 0x30, 0x20), MakeRgba(0x30, 0xC0, 0x50),
					MakeRgba(0x30, 0x60, 0xE0), MakeRgba(0xF0, 0xD0, 0x30),
				};

				CpArrayData pos;
				pos.number = CpArrPos;
				pos.stride = 16;

				for (int i = 0; i < 4; i++)
				{
					RowFixed(pos.rows[i], Flipper::VFMT_F32, 0, px[i]);
					RowFixed(pos.rows[i], Flipper::VFMT_F32, 0, py[i]);
					RowFixed(pos.rows[i], Flipper::VFMT_F32, 0, 0.0f);
				}

				m.BeginFrame();

				DisplayList list;
				list.CpReg(Flipper::CP_VCD_LO_ID, VcdLo(Flipper::VCD_INDEX8, Flipper::VCD_NONE, Flipper::VCD_DIRECT));
				list.CpReg(Flipper::CP_VCD_HI_ID, 0);
				list.CpReg(Flipper::CP_VAT_A_ID | 0, VatPos(Flipper::VCNT_POS_XYZ, Flipper::VFMT_F32, 0) |
					VatColor(Flipper::VFMT_RGBA8));
				ProgramArrays(m, list, &pos, 1);

				list.Draw((uint8_t)(Flipper::CP_CMD_DRAW_QUAD | 0), 4);

				for (int i = 0; i < 4; i++)
				{
					list.U8((uint8_t)i);			// the index of the row, one byte
					PushRgba8(list, colors[i]);
				}

				list.Align();
				RunFifo(m, SetupFifo(m, list.Bytes()));

				Publish(m, "the position comes out of an 8 bit indexed array", "cp_array_pos_index8.png",
					"The vertex carries one byte per position and the array holds the four positions of a tilted quad,\n"
					"16 bytes apart (a 12 byte row plus 4 bytes of padding), so the stride, not just the base, decides\n"
					"which row the index reaches. The colour of the picture is a direct attribute of the same vertex,\n"
					"so this is the vertex format where only the position is indexed.");
			}

			// ---- the position out of a 16 bit indexed array with a fixed point row --------------
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0x10, 0x1C, 0x14));
				SetupRasterStage0(m);

				const float px[4] = { 0.0f, 0.85f, 0.0f, -0.85f };
				const float py[4] = { -0.85f, 0.0f, 0.85f, 0.0f };
				const Rgba colors[4] = {
					MakeRgba(0xFF, 0x20, 0x60), MakeRgba(0x20, 0xFF, 0x80),
					MakeRgba(0x60, 0x80, 0xFF), MakeRgba(0xFF, 0xE0, 0x20),
				};

				CpArrayData pos;
				pos.number = CpArrPos;
				pos.stride = 8;							// three halfwords plus two bytes of padding

				for (int i = 0; i < 4; i++)
				{
					RowFixed(pos.rows[i], Flipper::VFMT_S16, 12, px[i]);
					RowFixed(pos.rows[i], Flipper::VFMT_S16, 12, py[i]);
					RowFixed(pos.rows[i], Flipper::VFMT_S16, 12, 0.0f);
				}

				m.BeginFrame();

				DisplayList list;
				list.CpReg(Flipper::CP_VCD_LO_ID, VcdLo(Flipper::VCD_INDEX16, Flipper::VCD_NONE, Flipper::VCD_DIRECT));
				list.CpReg(Flipper::CP_VCD_HI_ID, 0);
				list.CpReg(Flipper::CP_VAT_A_ID | 0, VatPos(Flipper::VCNT_POS_XYZ, Flipper::VFMT_S16, 12) |
					VatByteDequant() | VatColor(Flipper::VFMT_RGB565));
				ProgramArrays(m, list, &pos, 1);

				list.Draw((uint8_t)(Flipper::CP_CMD_DRAW_QUAD | 0), 4);

				for (int i = 0; i < 4; i++)
				{
					list.U16((uint16_t)i);				// the index of the row, two bytes
					uint16_t packed = (uint16_t)(((colors[i].R >> 3) << 11) | ((colors[i].G >> 2) << 5) | (colors[i].B >> 3));
					list.U16(packed);
				}

				list.Align();
				RunFifo(m, SetupFifo(m, list.Bytes()));

				Publish(m, "16 bit indices into a fixed point position array", "cp_array_pos_index16.png",
					"Here the index is a halfword and the row a signed 16 bit position with a shift of twelve: -4096 is\n"
					"-1.0. The row is decoded by the same dequantisation the direct path uses, so the diamond of this\n"
					"picture is the geometry the four rows describe. The colour of the vertex is direct and packed as\n"
					"RGB565, which is the other half of the same vertex.");
			}

			// ---- the colour out of an 8 bit indexed array ---------------------------------------
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0x1E, 0x12, 0x26));
				SetupRasterStage0(m);

				const float px[4] = { -0.95f, 0.95f, 0.95f, -0.95f };
				const float py[4] = { -0.95f, -0.95f, 0.95f, 0.95f };
				const Rgba colors[4] = {
					MakeRgba(0x20, 0xE0, 0xC0), MakeRgba(0xE0, 0x20, 0x60),
					MakeRgba(0x50, 0x30, 0xE0), MakeRgba(0xF0, 0xA0, 0x20),
				};

				CpArrayData col;
				col.number = CpArrColor0;					// array 2, which is not the VTX_COLOR0 attribute number
				col.stride = 4;

				for (int i = 0; i < 4; i++)
				{
					RowColor(col.rows[i], Flipper::VFMT_RGBA8, colors[i]);
				}

				m.BeginFrame();

				DisplayList list;
				list.CpReg(Flipper::CP_VCD_LO_ID, VcdLo(Flipper::VCD_DIRECT, Flipper::VCD_NONE, Flipper::VCD_INDEX8));
				list.CpReg(Flipper::CP_VCD_HI_ID, 0);
				list.CpReg(Flipper::CP_VAT_A_ID | 0, VatPos(Flipper::VCNT_POS_XYZ, Flipper::VFMT_F32, 0) |
					VatColor(Flipper::VFMT_RGBA8));
				ProgramArrays(m, list, &col, 1);

				list.Draw((uint8_t)(Flipper::CP_CMD_DRAW_QUAD | 0), 4);

				for (int i = 0; i < 4; i++)
				{
					PushF32x3(list, px[i], py[i], 0.0f);
					list.U8((uint8_t)i);				// the colour comes out of the array
				}

				list.Align();
				RunFifo(m, SetupFifo(m, list.Bytes()));

				Publish(m, "the colour comes out of an 8 bit indexed array", "cp_array_color_index8.png",
					"The position of this quad is a direct attribute and the colour of every vertex is an index into the\n"
					"colour0 array (array 2 of the CP - the array numbers are not the VTX_* attribute numbers, where\n"
					"VTX_COLOR0 is 4). The raster stage hands the vertex colour to the TEV, so the four rows of the array\n"
					"are the four corners of the picture.");
			}

			// ---- the texture coordinate out of an 8 bit indexed array ---------------------------
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0x20, 0x14, 0x10));
				SetupTextureStage0(m);

				std::vector<uint8_t> raw = CheckerTexture(32, 4);
				SetupTexture(m, 0, CpArrayAddr + 0x20000, 32, 32, GFX::TF_RGBA8, raw.data(), raw.size(),
					1u | (1u << 2));					// repeat in both directions

				const float px[4] = { -0.9f, 0.9f, 0.9f, -0.9f };
				const float py[4] = { -0.9f, -0.9f, 0.9f, 0.9f };
				const float ts[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
				const float tt[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

				CpArrayData tex;
				tex.number = CpArrTex0;						// array 4
				tex.stride = 4;								// two bytes of coordinate plus two bytes of padding

				for (int i = 0; i < 4; i++)
				{
					RowFixed(tex.rows[i], Flipper::VFMT_U8, 7, ts[i]);
					RowFixed(tex.rows[i], Flipper::VFMT_U8, 7, tt[i]);
				}

				m.BeginFrame();

				DisplayList list;
				list.CpReg(Flipper::CP_VCD_LO_ID, VcdLo(Flipper::VCD_DIRECT, Flipper::VCD_NONE, Flipper::VCD_NONE));
				list.CpReg(Flipper::CP_VCD_HI_ID, Flipper::VCD_INDEX8);		// Tex0Coord is the field 1:0
				list.CpReg(Flipper::CP_VAT_A_ID | 0, VatPos(Flipper::VCNT_POS_XYZ, Flipper::VFMT_F32, 0) |
					VatTex0(Flipper::VCNT_TEX_ST, Flipper::VFMT_U8, 7) | VatByteDequant());
				ProgramArrays(m, list, &tex, 1);

				list.Draw((uint8_t)(Flipper::CP_CMD_DRAW_QUAD | 0), 4);

				for (int i = 0; i < 4; i++)
				{
					PushF32x3(list, px[i], py[i], 0.0f);
					list.U8((uint8_t)i);				// the texture coordinate comes out of the array
				}

				list.Align();
				RunFifo(m, SetupFifo(m, list.Bytes()));

				Publish(m, "the texture coordinate comes out of an 8 bit indexed array", "cp_array_texcoord_index8.png",
					"The quad is a direct attribute of the vertex and the texture coordinate is an index into the tex0\n"
					"array (array 4). A coordinate row is two unsigned bytes with a shift of seven, so the array can\n"
					"address the whole texture; the sampler repeats it, which is why the checkerboard of this picture\n"
					"tiles across the quad more than once.");
			}

			// ---- the normal out of an 8 bit indexed array (the lighting of the XF) --------------
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0x08, 0x08, 0x10));
				SetupRasterStage0(m);

				const float px[4] = { -0.9f, 0.9f, 0.9f, -0.9f };
				const float py[4] = { -0.9f, -0.9f, 0.9f, 0.9f };
				const float nx[4] = { -0.6f, 0.6f, 0.6f, -0.6f };
				const float ny[4] = { -0.6f, -0.6f, 0.6f, 0.6f };

				CpArrayData nrm;
				nrm.number = CpArrNrm;
				nrm.stride = 4;								// three signed bytes plus one byte of padding

				for (int i = 0; i < 4; i++)
				{
					RowFixed(nrm.rows[i], Flipper::VFMT_S8, 6, nx[i]);
					RowFixed(nrm.rows[i], Flipper::VFMT_S8, 6, ny[i]);
					RowFixed(nrm.rows[i], Flipper::VFMT_S8, 6, 0.5f);
				}

				// One diffuse light to the upper right: the illumination of a vertex is the normal the
				// CP fetched for it, so the four rows of the array light the four corners.
				m.XfLoad(GFX::XF_NUMCOLS_ID, 1);
				m.XfLoad(GFX::XF_MATERIAL0_ID, 0xFFFFFFFF);
				m.XfLoad(GFX::XF_AMBIENT0_ID, 0x202028FF);
				m.XfLoad(GFX::XF_COLOR0CNTL_ID, (1u << 1) | (1u << 2) | (2u << 7));
				m.XfLoad(GFX::XF_ALPHA0CNTL_ID, 0);
				m.XfLoad(GFX::XF_LIGHT0_RGBA_ID, 0xFFE080FF);
				m.XfLoad(GFX::XF_LIGHT0_LPX_ID, AsBits(4.0f));
				m.XfLoad(GFX::XF_LIGHT0_LPY_ID, AsBits(3.0f));
				m.XfLoad(GFX::XF_LIGHT0_LPZ_ID, AsBits(3.0f));
				m.XfLoad(GFX::XF_LIGHT0_K0_ID, AsBits(1.0f));
				m.XfLoad(GFX::XF_LIGHT0_K1_ID, AsBits(0.6f));
				m.XfLoad(GFX::XF_LIGHT0_K2_ID, AsBits(0.4f));

				m.BeginFrame();

				DisplayList list;
				list.CpReg(Flipper::CP_VCD_LO_ID, VcdLo(Flipper::VCD_DIRECT, Flipper::VCD_INDEX8, Flipper::VCD_NONE));
				list.CpReg(Flipper::CP_VCD_HI_ID, 0);
				list.CpReg(Flipper::CP_VAT_A_ID | 0, VatPos(Flipper::VCNT_POS_XYZ, Flipper::VFMT_F32, 0) |
					VatNrm(Flipper::VCNT_NRM_XYZ, Flipper::VFMT_S8));
				ProgramArrays(m, list, &nrm, 1);

				list.Draw((uint8_t)(Flipper::CP_CMD_DRAW_QUAD | 0), 4);

				for (int i = 0; i < 4; i++)
				{
					PushF32x3(list, px[i], py[i], 0.0f);
					list.U8((uint8_t)i);				// the normal comes out of the array
				}

				list.Align();
				RunFifo(m, SetupFifo(m, list.Bytes()));

				Publish(m, "the normal comes out of an 8 bit indexed array", "cp_array_normal_index8.png",
					"No colour attribute at all: the XF computes the vertex colour from the normal (one diffuse light to\n"
					"the upper right, material white, ambient almost black). A normal row is three signed bytes with a\n"
					"fixed binary point of six, and the four rows point into the four quadrants, so the illumination of\n"
					"the picture is the shape of the normal array.");
			}

			// ---- one array, two quads: the index selects the rows -------------------------------
			{
				SetupPassThroughXF(m);
				SetupDefaultPixelState(m);
				SetClearColor(m, MakeRgba(0x14, 0x18, 0x24));
				SetupRasterStage0(m);

				// The eight rows of one array: the first four are the left quad, the last four the
				// right one. Nothing but the index of the vertex tells them apart.
				const float pos[8][2] = {
					{ -0.90f, -0.70f }, { -0.20f, -0.70f }, { -0.20f, 0.20f }, { -0.90f, 0.20f },
					{  0.20f, -0.20f }, {  0.90f, -0.20f }, {  0.90f, 0.70f }, {  0.20f, 0.70f },
				};

				const Rgba colors[8] = {
					MakeRgba(0x20, 0x40, 0xE0), MakeRgba(0x30, 0x80, 0xFF),
					MakeRgba(0x20, 0xC0, 0xE0), MakeRgba(0x10, 0x20, 0x90),
					MakeRgba(0xE0, 0x20, 0x20), MakeRgba(0xFF, 0x90, 0x30),
					MakeRgba(0xFF, 0xD0, 0x40), MakeRgba(0x90, 0x10, 0x40),
				};

				CpArrayData posArray;
				posArray.number = CpArrPos;
				posArray.stride = 12;
				posArray.rowCount = 8;

				for (int i = 0; i < 8; i++)
				{
					RowFixed(posArray.rows[i], Flipper::VFMT_F32, 0, pos[i][0]);
					RowFixed(posArray.rows[i], Flipper::VFMT_F32, 0, pos[i][1]);
					RowFixed(posArray.rows[i], Flipper::VFMT_F32, 0, 0.0f);
				}

				m.BeginFrame();

				DisplayList list;
				list.CpReg(Flipper::CP_VCD_LO_ID, VcdLo(Flipper::VCD_INDEX8, Flipper::VCD_NONE, Flipper::VCD_DIRECT));
				list.CpReg(Flipper::CP_VCD_HI_ID, 0);
				list.CpReg(Flipper::CP_VAT_A_ID | 0, VatPos(Flipper::VCNT_POS_XYZ, Flipper::VFMT_F32, 0) |
					VatColor(Flipper::VFMT_RGBA8));
				ProgramArrays(m, list, &posArray, 1);

				// Two quads out of the one array: the index of the vertex decides which rows they are
				for (int quad = 0; quad < 2; quad++)
				{
					list.Draw((uint8_t)(Flipper::CP_CMD_DRAW_QUAD | 0), 4);

					for (int i = 0; i < 4; i++)
					{
						int row = quad * 4 + i;
						list.U8((uint8_t)row);
						PushRgba8(list, colors[row]);
					}
				}

				list.Align();
				RunFifo(m, SetupFifo(m, list.Bytes()));

				Publish(m, "one array, two quads: the index selects the rows", "cp_array_index_order.png",
					"The position array holds eight rows and the same display list draws two quads out of it: the index\n"
					"of each vertex is what picks its row, so the left quad is the rows 0-3 and the right one the rows\n"
					"4-7. The array is the vertex buffer of a guest that stores several shapes in one buffer.");
			}
		}
	};
}
