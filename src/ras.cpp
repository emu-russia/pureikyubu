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

	//! Extend the pixel engine's bounding box with the window extent of the vertices of this draw
	//! (gfx-pe.md 6.17). The hardware extends the box while it walks the quads that produce pixels;
	//! the shader pipeline leaves the transform to the vertex program, so the window positions are
	//! formed here from the same XF registers that program reads - the geometry matrix of the
	//! vertex's slot, the projection combine and the viewport scale/offset - and the rasterizer cuts
	//! them where the clipper would have (a vertex at or behind the eye has no window position).
	void Rasterizer::ExtendBoundingBox()
	{
		const XFState& xf = gfx->xf->xf;

		float sc[3], off[3];
		gfx->xf->SoftViewport(sc, off);

		int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;

		for (size_t i = 0; i < vertex_count; i++)
		{
			const Vertex* v = &gfx->vertex_data[i];
			size_t mbase = (size_t)(v->matIdx0.PosNrmMatIdx & 0x3F) * 4;

			float ex = xf.mvTexMtx[mbase + 0] * v->Position[0] + xf.mvTexMtx[mbase + 1] * v->Position[1] +
				xf.mvTexMtx[mbase + 2] * v->Position[2] + xf.mvTexMtx[mbase + 3];
			float ey = xf.mvTexMtx[mbase + 4] * v->Position[0] + xf.mvTexMtx[mbase + 5] * v->Position[1] +
				xf.mvTexMtx[mbase + 6] * v->Position[2] + xf.mvTexMtx[mbase + 7];
			float ez = xf.mvTexMtx[mbase + 8] * v->Position[0] + xf.mvTexMtx[mbase + 9] * v->Position[1] +
				xf.mvTexMtx[mbase + 10] * v->Position[2] + xf.mvTexMtx[mbase + 11];

			float cx, cy, cw;

			if (xf.projectOrtho)
			{
				cx = xf.projectionParam[0] * ex + xf.projectionParam[1];
				cy = xf.projectionParam[2] * ey + xf.projectionParam[3];
				cw = 1.0f;
			}
			else
			{
				cx = xf.projectionParam[0] * ex + xf.projectionParam[1] * ez;
				cy = xf.projectionParam[2] * ey + xf.projectionParam[3] * ez;
				cw = -ez;
			}

			if (cw <= 0.0f)
				continue;

			int x = (int)floorf(cx / cw * sc[0] + off[0] + 0.5f);
			int y = (int)floorf(cy / cw * sc[1] + off[1] + 0.5f);

			if (x < minX) minX = x;
			if (x > maxX) maxX = x;
			if (y < minY) minY = y;
			if (y > maxY) maxY = y;
		}

		if (minX > maxX || minY > maxY)
			return;

		gfx->pe->ExtendBoundingBox(minX, minY, maxX, maxY);
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

		// The frame now holds content: the display copy that closes it presents this picture
		// (see GFXCore::GPDisplayCopy).
		gfx->GPFrameDrawn();

		// The bounding box follows what is drawn, and it is read from the CPU in the middle of a
		// frame, so it is extended before the primitive goes to the GPU.
		ExtendBoundingBox();

		SetUpPipeline();
		DrawPrimitive();

		vertex_count = 0;
	}

	// -------------------------------------------------------------------------------------------
	// The software rasterizers (GFX_PIPELINE = soft, issue #384)
	//
	// RAS0/RAS1/RAS2 walk the primitive on the 2x2-pixel quad grid of the hardware: every step
	// covers one quad and produces its 12-bit coverage mask (three sub-samples per pixel, one bit
	// each), and only the pixels with a covered sub-sample are shaded (gfx-ras0.md 3.2/3.3,
	// gfx-ras2.md 3.2/3.3).
	//
	// The interpolation is done from the planes the Setup Unit computed; the perspective-correct
	// parameters are the ratio of their plane and the 1/w plane (gfx-ras1.md 3.3).
	// -------------------------------------------------------------------------------------------

	//! Is the sample (x, y) inside the triangle? The edge functions are positive inside, and the
	//! samples exactly on an edge follow the "top left" rule, so that two triangles sharing an edge
	//! do not both cover the pixels of that edge.
	static bool SoftInside(const SoftTriangle& tri, float x, float y)
	{
		for (int i = 0; i < 3; i++)
		{
			float e = tri.e[i][0] * x + tri.e[i][1] * y + tri.e[i][2];

			if (e > 0.0f)
				continue;

			if (e < 0.0f)
				return false;

			// On the edge: it belongs to the triangle only when it is a top or a left edge. In the
			// window coordinate system of the EFB (Y grows downward) the interior is on the side
			// the gradient (a, b) points to, so an edge is a top edge when b > 0 and a left edge
			// when b == 0 and a > 0.
			float a = tri.e[i][0], b = tri.e[i][1];
			if (!(b > 0.0f || (b == 0.0f && a > 0.0f)))
				return false;
		}

		return true;
	}

	//! Shade one covered pixel and hand it to the pixel engine.
	void Rasterizer::SoftShadePixel(const SoftTriangle& tri, int px, int py, float sx, float sy)
	{
		SoftFragment f;
		f.x = sx;
		f.y = sy;

		float w = tri.invW.Eval(sx, sy);
		float rw = (fabsf(w) > 1e-12f) ? (1.0f / w) : 0.0f;

		f.z = tri.z.Eval(sx, sy);

		for (int ch = 0; ch < 2; ch++)
		{
			for (int c = 0; c < 4; c++)
				f.color[ch][c] = tri.color[ch][c].Eval(sx, sy) * rw;
		}

		for (int i = 0; i < 8; i++)
		{
			float sa = tri.tex[i][0].Eval(sx, sy);
			float ta = tri.tex[i][1].Eval(sx, sy);

			f.tex[i][0] = sa * rw;
			f.tex[i][1] = ta * rw;

			// The screen-space derivatives of the perspective-correct coordinate, for the LOD of
			// the texture unit: d(a/w)/dx = (a' - (a/w) * w') / w.
			f.dtex[i][0] = (tri.tex[i][0].dx - f.tex[i][0] * tri.invW.dx) * rw;	// ds/dx
			f.dtex[i][1] = (tri.tex[i][1].dx - f.tex[i][1] * tri.invW.dx) * rw;	// dt/dx
			f.dtex[i][2] = (tri.tex[i][0].dy - f.tex[i][0] * tri.invW.dy) * rw;	// ds/dy
			f.dtex[i][3] = (tri.tex[i][1].dy - f.tex[i][1] * tri.invW.dy) * rw;	// dt/dy
		}

		float rgba[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float depth = f.z;

		if (!gfx->tev->SoftShade(f, rgba, &depth))
			return;			// the alpha function discarded the fragment

		gfx->pe->SoftWritePixel(px, py, rgba, depth);
	}

	void Rasterizer::SoftQuad(const SoftTriangle& tri, int qx, int qy)
	{
		bool ms = (gfx->genmode.ms_en != 0);

		bool covered[4] = { false, false, false, false };
		float evalx[4] = { 0, 0, 0, 0 };
		float evaly[4] = { 0, 0, 0, 0 };
		uint32_t mask = 0;

		for (int p = 0; p < 4; p++)
		{
			int dx = p & 1;
			int dy = p >> 1;

			float cx = (float)qx + (float)dx + 0.5f;
			float cy = (float)qy + (float)dy + 0.5f;

			int cov = 0;
			int ncov = 0;
			float firstx = cx, firsty = cy;

			for (int s = 0; s < 3; s++)
			{
				float sxs = cx, sys = cy;

				if (ms)
				{
					// The sample locations of the quad come from GEN_MSLOC0..3, one register per
					// pixel, three 4-bit X/Y offsets each (gfx-ras0.md 3.3, gfx-pe.md 6.21).
					//
					// The encoding of the offsets is not fully pinned down by the available
					// specification: it describes them as 1/12-pixel distances from the quad
					// centre, while the RTL applies them with the sign of the pixel's position in
					// the quad. The model reads a field as a signed 1/12-pixel offset from the
					// *pixel centre* with the value 6 - the value the SDK's GXInit programs -
					// meaning "no offset", so the three sub-samples of a pixel degenerate to its
					// centre unless the title programs a real pattern.
					const GenMsloc& m = gfx->msloc[p];
					unsigned ox = (s == 0) ? m.xs0 : ((s == 1) ? m.xs1 : m.xs2);
					unsigned oy = (s == 0) ? m.ys0 : ((s == 1) ? m.ys1 : m.ys2);

					sxs = cx + ((float)ox - 6.0f) / 12.0f;
					sys = cy + ((float)oy - 6.0f) / 12.0f;
				}

				if (SoftInside(tri, sxs, sys))
				{
					cov |= (1 << s);
					if (ncov == 0)
					{
						firstx = sxs;
						firsty = sys;
					}
					ncov++;
				}
			}

			// The 12-bit coverage mask groups three sub-sample bits per pixel (gfx-ras0.md 5.2);
			// the mapping of the four groups to the physical pixels of the quad is not confirmed by
			// the specification, so the model uses the reading order of the quad (bit 0 = left top).
			mask |= (uint32_t)cov << (3 * p);

			covered[p] = (cov != 0);

			// The evaluation point of the pixel: a fully covered pixel at its centre, a partially
			// covered one at one of its covered sub-samples (gfx-ras2.md 3.3 - "the pixel's 3-bit
			// sub-mask selects a representative covered sub-sample").
			evalx[p] = (ncov == 3) ? cx : firstx;
			evaly[p] = (ncov == 3) ? cy : firsty;
		}

		if (mask == 0)
			return;

		for (int p = 0; p < 4; p++)
		{
			if (!covered[p])
				continue;

			SoftShadePixel(tri, qx + (p & 1), qy + (p >> 1), evalx[p], evaly[p]);
		}
	}

	void Rasterizer::SoftDrawTriangle(const SoftTriangle& tri)
	{
		if (gfx->pe == nullptr)
			return;

		if (tri.maxx < tri.minx || tri.maxy < tri.miny)
			return;

		// The rasterizers clamp every primitive to the SU scissor rectangle (gfx-su.md 3.3/4.1)
		int sx, sy, sw, sh;
		gfx->su->Scissor(&sx, &sy, &sw, &sh);

		int x0 = tri.minx, y0 = tri.miny, x1 = tri.maxx, y1 = tri.maxy;

		if (x0 < sx) x0 = sx;
		if (y0 < sy) y0 = sy;
		if (x1 > sx + sw - 1) x1 = sx + sw - 1;
		if (y1 > sy + sh - 1) y1 = sy + sh - 1;

		if (x1 < x0 || y1 < y0)
			return;

		// The quad grid: the walk starts at the even pixel coordinates of the bounding box.
		int qx0 = x0 & ~1;
		int qy0 = y0 & ~1;

		for (int qy = qy0; qy <= y1; qy += 2)
		{
			for (int qx = qx0; qx <= x1; qx += 2)
			{
				SoftQuad(tri, qx, qy);
			}
		}
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
	void Rasterizer::loadRASReg(size_t index, uint32_t value, uint32_t mask)
	{
		switch (index)
		{
			// The performance/break register is owned by RAS1 (gfx-ras1.md 4.1) but does not drive
			// the GL backend: the counter events and the break control have no host equivalent.
			case RAS1_PERF_ID: break;

			case RAS1_SS0_ID: ss[0].bits = MergeBpWriteMask(ss[0].bits, value, mask); break;
			case RAS1_SS1_ID: ss[1].bits = MergeBpWriteMask(ss[1].bits, value, mask); break;
			case RAS1_IREF_ID: iref = MergeBpWriteMask(iref, value, mask); break;

			case RAS1_TREF0_ID:
			case RAS1_TREF1_ID:
			case RAS1_TREF2_ID:
			case RAS1_TREF3_ID:
			case RAS1_TREF4_ID:
			case RAS1_TREF5_ID:
			case RAS1_TREF6_ID:
			case RAS1_TREF7_ID:
			{
				RAS1_TREF& reg = tref[index - RAS1_TREF0_ID];
				reg.bits = MergeBpWriteMask(reg.bits, value, mask);
			}
			break;

			default:
				gfx->pe->loadPEReg(index, value, mask);
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

	// -------------------------------------------------------------------------------------------
	// Save states
	//
	// The rasterizers own the register state that the rest of the pipeline reads from RAS1: the
	// eight texture/colour source references (RAS1_TREF0..7, one register per pair of TEV stages),
	// the two coordinate shift scale registers of the indirect stages and the indirect reference
	// (RAS1_IREF). The texture unit binds the maps the TEV stages name through these words, so a
	// state without them samples the wrong textures.
	//
	// What does not travel: the `gfx` back-pointer; `current_prim` and `vertex_count`, which are the
	// primitive being assembled - always empty at a command boundary, because a state is taken
	// between FIFO commands; and `ras_wireframe`, which is a debugger toggle rather than machine
	// state.
	// -------------------------------------------------------------------------------------------

	void Rasterizer::SaveState(SaveStates::StateWriter& writer) const
	{
		// The texture/colour source references, as their 32-bit words.
		for (int i = 0; i < 8; i++)
		{
			writer.Fields(tref[i].bits);
		}

		writer.Fields(ss[0].bits, ss[1].bits);
		writer.Fields(iref);
	}

	void Rasterizer::LoadState(SaveStates::StateReader& reader)
	{
		for (int i = 0; i < 8; i++)
		{
			reader.Fields(tref[i].bits);
		}

		reader.Fields(ss[0].bits, ss[1].bits);
		reader.Fields(iref);
	}
}
