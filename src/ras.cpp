// Rasterizers (RAS0/RAS1/RAS2)
//
// Now we're putting all the drawing on the shoulders of the graphics API. If we get bored, we can make a software rasterizer.
//
// The per-vertex work (XF) and the per-pixel work (TEV) are done by the shaders; this module
// accumulates the vertices of a draw command, uploads them into the VBO and issues the draw call.

#include "pch.h"

using namespace Debug;

namespace GFX
{
	void Rasterizer::RAS_Begin(RAS_Primitive prim, size_t vtx_num)
	{
		current_prim = prim;
		vertex_count = 0;
	}

	void Rasterizer::RAS_SendVertex(const Vertex* v)
	{
		if (vertex_count < GFX_MAX_VERTICES)
		{
			gfx->vertex_data[vertex_count++] = *v;
		}
	}

	void Rasterizer::SetUpPipeline()
	{
		GLProgram* program = gfx->tev->GetTevProgram();
		if (program == nullptr)
			return;

		program->Use();

		// Textures must be decoded/uploaded before the shader can sample them
		gfx->tx->UpdateAndBindTextures();

		// The XF register state (matrices, lights, controls) and the TEV state
		gfx->xf->UploadUniforms(*program);
		gfx->tev->UploadUniforms(*program);
	}

	// The line width and the point size are programmed through SU_LPSIZE (gfx-su.md 4.1, 5.2): both
	// fields hold the size in 1/16 pixel increments (GX_SetLineWidth / GX_SetPointSize), so the size
	// in pixels is the register value divided by 16. A register value of zero would be a zero-width
	// line (which GL rejects), so the request is floored at one pixel, and the GL limit of the
	// context (the aliased line width range is as low as [1, 1] on some drivers) caps it.
	static void ApplyLineWidth(float width)
	{
		GLfloat range[2] = { 1.0f, 1.0f };
		glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, range);

		if (width < 1.0f)
			width = 1.0f;
		if (width > range[1])
			width = range[1];
		if (width < range[0])
			width = range[0];

		glLineWidth(width);
	}

	static void ApplyPointSize(float size)
	{
		GLfloat range[2] = { 1.0f, 1.0f };
		glGetFloatv(GL_POINT_SIZE_RANGE, range);

		if (size < 1.0f)
			size = 1.0f;
		if (size > range[1])
			size = range[1];
		if (size < range[0])
			size = range[0];

		glPointSize(size);
	}

	void Rasterizer::DrawPrimitive()
	{
		glBindVertexArray(gfx->vao);
		glBindBuffer(GL_ARRAY_BUFFER, gfx->vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(vertex_count * sizeof(Vertex)), gfx->vertex_data);

		GLenum mode = GL_TRIANGLES;
		size_t index_count = 0;

		switch (current_prim)
		{
			case RAS_QUAD:
			{
				// A Flipper quad is a fan of three-vertex groups (gfx-xf.md 2.2)
				size_t quads = vertex_count / 4;
				for (size_t q = 0; q < quads; q++)
				{
					uint32_t base = (uint32_t)(q * 4);
					gfx->index_data[index_count++] = base + 0;
					gfx->index_data[index_count++] = base + 1;
					gfx->index_data[index_count++] = base + 2;
					gfx->index_data[index_count++] = base + 0;
					gfx->index_data[index_count++] = base + 2;
					gfx->index_data[index_count++] = base + 3;
				}
				mode = GL_TRIANGLES;
			}
			break;

			case RAS_QUAD_STRIP:
			{
				// Quad n uses the vertices (2n, 2n+1, 2n+3, 2n+2) as a fan
				if (vertex_count >= 4)
				{
					size_t quads = vertex_count / 2 - 1;
					for (size_t q = 0; q < quads; q++)
					{
						uint32_t a = (uint32_t)(q * 2);
						uint32_t b = a + 1;
						uint32_t c = a + 3;
						uint32_t d = a + 2;
						gfx->index_data[index_count++] = a;
						gfx->index_data[index_count++] = b;
						gfx->index_data[index_count++] = c;
						gfx->index_data[index_count++] = a;
						gfx->index_data[index_count++] = c;
						gfx->index_data[index_count++] = d;
					}
				}
				mode = GL_TRIANGLES;
			}
			break;

			case RAS_TRIANGLE:
				mode = GL_TRIANGLES;
				break;
			case RAS_TRIANGLE_STRIP:
				mode = GL_TRIANGLE_STRIP;
				break;
			case RAS_TRIANGLE_FAN:
				mode = GL_TRIANGLE_FAN;
				break;
			case RAS_LINE:
				mode = GL_LINES;
				break;
			case RAS_LINE_STRIP:
				mode = GL_LINE_STRIP;
				break;
			case RAS_POINT:
				mode = GL_POINTS;
				break;
		}

		if (index_count != 0)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->ibo);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, (GLsizeiptr)(index_count * sizeof(uint32_t)), gfx->index_data);
			glDrawElements(mode, (GLsizei)index_count, GL_UNSIGNED_INT, nullptr);
		}
		else
		{
			// The line and point size of the setup unit (SU_LPSIZE) only applies to the primitives
			// that are rasterized as lines or points; the triangles keep the GL default of the
			// pipeline.
			if (mode == GL_LINES || mode == GL_LINE_STRIP)
			{
				ApplyLineWidth((float)gfx->su->State().lpsize.lsize / 16.0f);
			}
			else if (mode == GL_POINTS)
			{
				ApplyPointSize((float)gfx->su->State().lpsize.psize / 16.0f);
			}

			glDrawArrays(mode, 0, (GLsizei)vertex_count);
		}
	}

	void Rasterizer::RAS_End()
	{
		if (vertex_count == 0)
			return;

		SetUpPipeline();
		DrawPrimitive();

		vertex_count = 0;
	}

	Rasterizer::Rasterizer(HWConfig* config, GFXCore* parent_gfx)
	{
		gfx = parent_gfx;
	}

	Rasterizer::~Rasterizer()
	{
	}

	// One word of the BP register space that RAS1 owns (0x24-0x2F). Everything else is passed on to
	// the Pixel Engine, which continues the walk down the bypass chain.
	void Rasterizer::loadRASReg(size_t index, uint32_t value)
	{
		switch (index)
		{
			// The performance/break register is owned by RAS1 (gfx-ras1.md 4.1) but does not drive
			// the GL backend: the counter events and the break control have no host equivalent.
			case RAS1_PERF_ID: break;

			case RAS1_SS0_ID: ss[0].bits = value; break;
			case RAS1_SS1_ID: ss[1].bits = value; break;
			case RAS1_IREF_ID: iref = value; break;

			case RAS1_TREF0_ID:
			case RAS1_TREF1_ID:
			case RAS1_TREF2_ID:
			case RAS1_TREF3_ID:
			case RAS1_TREF4_ID:
			case RAS1_TREF5_ID:
			case RAS1_TREF6_ID:
			case RAS1_TREF7_ID:
				tref[index - RAS1_TREF0_ID].bits = value;
				break;

			default:
				gfx->pe->loadPEReg(index, value);
				break;
		}
	}

	void Rasterizer::Reset()
	{
		current_prim = RAS_QUAD;
		vertex_count = 0;

		for (int i = 0; i < 8; i++)
		{
			tref[i].bits = 0;
		}
		ss[0].bits = 0;
		ss[1].bits = 0;
		iref = 0;
	}
}
