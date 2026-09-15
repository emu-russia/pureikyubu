// Setup Unit
#include "pch.h"

// Processing of BP address space registers Load Commands

using namespace Debug;

namespace GFX
{

	// The programmed scissor rectangle is in screen coordinates (the origin is the top left corner of
	// the EFB, gfx-su.md 4.1) and carries the +342 bias of the SU datapath, while GL measures its
	// scissor box from the bottom left corner, so the Y axis is flipped here. An empty rectangle is
	// passed on as a zero-sized one, which is what GL uses to reject every fragment.
	void SetupUnit::GL_SetScissor(int x, int y, int w, int h)
	{
		if (gfx == nullptr || !gfx->backend_started)
			return;

		// GL rejects a negative width or height; a rectangle that reaches outside the render target
		// is clipped by GL itself, so only the corner is clamped here.
		int sw = (int)gfx->scr_w;
		int sh = (int)gfx->scr_h;

		if (x < 0) { w += x; x = 0; }
		if (y < 0) { h += y; y = 0; }
		if (x + w > sw) w = sw - x;
		if (y + h > sh) h = sh - y;

		if (w < 0) w = 0;
		if (h < 0) h = 0;

		glScissor(x, sh - (h + y), w, h);
	}

	// -------------------------------------------------------------------------------------------
	// The scissor rectangle of the setup unit
	// -------------------------------------------------------------------------------------------

	// SU_SCIS0 holds the top left corner (sux, suy) and SU_SCIS1 the bottom right one (suw, suh), both
	// biased by SU_SCISSOR_ORIGIN, so the rectangle is inclusive on both corners.
	void SetupUnit::Scissor(int* x, int* y, int* w, int* h) const
	{
		*x = (int)su.scis0.sux - SU_SCISSOR_ORIGIN;
		*y = (int)su.scis0.suy - SU_SCISSOR_ORIGIN;
		*w = (int)su.scis1.suw - (int)su.scis0.sux + 1;
		*h = (int)su.scis1.suh - (int)su.scis0.suy + 1;
	}

	void SetupUnit::ApplyScissor()
	{
		int x, y, w, h;
		Scissor(&x, &y, &w, &h);
		GL_SetScissor(x, y, w, h);
	}

	// The render target changed size (a VI mode switch). The rectangle a game programmed is in screen
	// coordinates, so it is simply converted again; the reset rectangle means "the whole target" and
	// therefore follows the new size, otherwise a scene that never programs the scissor would clip
	// itself to the old, smaller target.
	void SetupUnit::ResizeScissor(int width, int height)
	{
		if (!su.scissorSet)
		{
			su.scis0.sux = SU_SCISSOR_ORIGIN;
			su.scis0.suy = SU_SCISSOR_ORIGIN;
			su.scis1.suw = SU_SCISSOR_ORIGIN + (unsigned)width - 1;
			su.scis1.suh = SU_SCISSOR_ORIGIN + (unsigned)height - 1;
		}

		ApplyScissor();
	}

	// The coordinate scale of one texture-coordinate pair, in texels (gfx-su.md 4.5/4.6): the register
	// holds the size of the texture minus one, and GX_SetTexCoordScaleManually stores `ss - 1` in
	// exactly the same field, so the multiplier the SU applies to the coordinate is `ssize + 1`.
	void SetupUnit::CoordScale(int pair, float* s, float* t) const
	{
		pair &= 7;

		*s = su.ssizeSet[pair] ? (float)(su.ssize[pair].ssize + 1) : 0.0f;
		*t = su.tsizeSet[pair] ? (float)(su.tsize[pair].tsize + 1) : 0.0f;
	}

	void SetupUnit::GL_SetCullMode(int mode)
	{
		switch(mode)
		{
			case GEN_REJECT_NONE:
				glDisable(GL_CULL_FACE);
				break;
			case GEN_REJECT_FRONT:
				// TODO: It's mixed up so far, not sure why, but it has to be that way
				glEnable(GL_CULL_FACE);
				glCullFace(GL_BACK);
				break;
			case GEN_REJECT_BACK:
				// TODO: It's mixed up so far, not sure why, but it has to be that way
				glEnable(GL_CULL_FACE);
				glCullFace(GL_FRONT);
				break;
			case GEN_REJECT_ALL:
				glEnable(GL_CULL_FACE);
				glCullFace(GL_FRONT_AND_BACK);
				break;
		}
	}

	// index range = 00..FF
	// reg size = 24 bit (value is already masked)
	void SetupUnit::loadSUReg(size_t index, uint32_t value)
	{
		switch (index)
		{
			//
			// gen mode
			//

			case GEN_MODE_ID:
			{
				gfx->genmode.bits = value;
				GL_SetCullMode(gfx->genmode.reject_en);

				// zfreeze freezes the Z buffer, which the Pixel Engine implements as a depth write mask
				gfx->pe->ApplyZMode();
			}
			break;

			// I don't see any use for MSLOC yet, I added it to avoid spamming with warnings

			case GEN_MSLOC0_ID:
				gfx->msloc[0].bits = value;
				break;
			case GEN_MSLOC1_ID:
				gfx->msloc[1].bits = value;
				break;
			case GEN_MSLOC2_ID:
				gfx->msloc[2].bits = value;
				break;
			case GEN_MSLOC3_ID:
				gfx->msloc[3].bits = value;
				break;

			//
			// set scissor box
			//

			case SU_SCIS0_ID:
			{
				su.scis0.bits = value;
				su.scissorSet = true;

				//GFXError("scissor (%i, %i)-(%i, %i)", x, y, w, h);
				ApplyScissor();
			}
			break;

			case SU_SCIS1_ID:
			{
				su.scis1.bits = value;
				su.scissorSet = true;

				//GFXError("scissor (%i, %i)-(%i, %i)", x, y, w, h);
				ApplyScissor();
			}
			break;

			//
			// The line and point size (0x22). The size is programmed in 1/16 pixel increments
			// (GX_SetLineWidth/GX_SetPointSize, gfx-su.md 4.1 and 5.2); the rasterizer applies it to
			// the GL line width / point size (see Rasterizer::DrawPrimitive). The texture offsets
			// ltoff/ptoff are decoded as well but have no GL counterpart: they shift the texture
			// coordinate of a line or a point by a fraction of a texel (gfx-su.md 4.1), which is a
			// sub-texel detail of the wide-primitive expansion the GL line/point rasterizer does not
			// reproduce.
			//

			case SU_LPSIZE_ID:
				su.lpsize.bits = value;
				break;

			//
			// texture coord scale. 0x30..0x3F is eight (S, T) pairs: the even index of a pair is
			// the S size (su_ts0) and the odd one its T size (su_ts1), see gfx-su.md 4.4-4.6.
			//

			case SU_SSIZE0_ID:
			case SU_SSIZE1_ID:
			case SU_SSIZE2_ID:
			case SU_SSIZE3_ID:
			case SU_SSIZE4_ID:
			case SU_SSIZE5_ID:
			case SU_SSIZE6_ID:
			case SU_SSIZE7_ID:
			{
				size_t num = (index - SU_SSIZE0_ID) >> 1;
				su.ssize[num].bits = value;
				su.ssizeSet[num] = true;
			}
			break;

			case SU_TSIZE0_ID:
			case SU_TSIZE1_ID:
			case SU_TSIZE2_ID:
			case SU_TSIZE3_ID:
			case SU_TSIZE4_ID:
			case SU_TSIZE5_ID:
			case SU_TSIZE6_ID:
			case SU_TSIZE7_ID:
			{
				size_t num = (index - SU_TSIZE0_ID) >> 1;
				su.tsize[num].bits = value;
				su.tsizeSet[num] = true;
			}
			break;

			//
			// global registers the SU owns but that do not affect the GL backend (there is no
			// hardware performance counter and no power-down control to honour here)
			//

			case SU_PERF_ID:
			case SU_SSMASK_ID:
				break;

			default:
				// The sequence of bypassing blocks for register load is as follows: SU -> RAS -> PE -> BUMP -> TX -> TEV -> Unknown reg load
				gfx->ras->loadRASReg(index, value);
				break;
		}
	}

	SetupUnit::SetupUnit(HWConfig* config, GFXCore* parent_gfx)
	{
		gfx = parent_gfx;
	}

	SetupUnit::~SetupUnit()
	{
	}

	//
	// The vertex stream coming from the XF. In this emulator the setup work (the primitive
	// assembly) happens in the GL backend, so the SU just drives the rasterizers with the
	// primitives and vertex rows the XF hands over.
	//

	void SetupUnit::BeginPrimitive(RAS_Primitive prim, size_t vtx_num)
	{
		gfx->ras->RAS_Begin(prim, vtx_num);
	}

	void SetupUnit::SendVertex(const Vertex* v)
	{
		gfx->ras->RAS_SendVertex(v);
	}

	void SetupUnit::EndPrimitive()
	{
		gfx->ras->RAS_End();
	}

	// The register reset state. Only the scissor rectangle has a value the backend depends on: the
	// hardware scissor resets to "everything" (the GX API initialises it to the screen), and a
	// rectangle left at register zero would be the 1x1 box at the internal origin (-342, -342),
	// which rejects every fragment of a scene that never programs the scissor. The rectangle is
	// therefore put at the whole render target here.
	void SetupUnit::Reset()
	{
		su = SUState{};

		unsigned w = (gfx != nullptr) ? (unsigned)gfx->scr_w : 640;
		unsigned h = (gfx != nullptr) ? (unsigned)gfx->scr_h : 480;

		su.scis0.sux = SU_SCISSOR_ORIGIN;
		su.scis0.suy = SU_SCISSOR_ORIGIN;
		su.scis1.suw = SU_SCISSOR_ORIGIN + w - 1;
		su.scis1.suh = SU_SCISSOR_ORIGIN + h - 1;

		ApplyScissor();

		soft_vertices.clear();
		soft_zfreeze = SoftPlane{};
		soft_zfreeze_valid = false;
	}

	// -------------------------------------------------------------------------------------------
	// The software Setup Unit (GFX_PIPELINE = soft, issue #384)
	//
	// The SU is the primitive assembler and the setup stage of the pipeline: it turns the vertex
	// stream of the XF into triangles (points, lines, strips and fans are all expanded here, see
	// gfx-su.md 3.1) and computes, for every triangle, the numbers the quad rasterizer walks with:
	// the bounding box, the three edge coefficients and the interpolation plane of every
	// interpolated parameter (gfx-su.md 3.4, gfx-su.md 2.3 `su_data_a`/`su_data_b`).
	// -------------------------------------------------------------------------------------------

	//! The value of one attribute of one vertex, in the plane space.
	//!
	//! The perspective-correct parameters (colours, texture coordinates) are carried divided by w,
	//! together with the plane of 1/w (gfx-ras1.md 3.3: the divisions s/w, t/w and 1/w happen at
	//! the pixel centres). Depth is screen-linear, like the Z plane of RAS0/RAS2.
	static void SoftBuildPlane(const float v[3], const float* x, const float* y, SoftPlane* plane)
	{
		// The barycentric solution of `v(x, y) = o + dx*x + dy*y` through the three vertices.
		float d = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);

		if (fabsf(d) < 1e-9f)
		{
			// A degenerate (zero-area) triangle: a constant plane is the only sane answer.
			plane->o = v[0];
			plane->dx = 0.0f;
			plane->dy = 0.0f;
			return;
		}

		plane->dx = ((v[1] - v[0]) * (y[2] - y[0]) - (v[2] - v[0]) * (y[1] - y[0])) / d;
		plane->dy = ((v[2] - v[0]) * (x[1] - x[0]) - (v[1] - v[0]) * (x[2] - x[0])) / d;
		plane->o = v[0] - plane->dx * x[0] - plane->dy * y[0];
	}

	//! The screen-space signed area of the triangle, in the window coordinate system of the EFB
	//! (the origin is the top left corner, Y grows downward).
	static float SoftSignedArea(const float* x, const float* y)
	{
		return (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
	}

	//! Front/back rejection of GEN_MODE.reject_en (gfx-su.md 3.6).
	//!
	//! The two pipelines have to reject the same triangles, so this follows the shader backend's
	//! mapping (SetupUnit::GL_SetCullMode, which sets `glFrontFace(GL_CW)` and culls the GL *back*
	//! faces for `reject_front` - the enum names look inverted there and the code carries a TODO
	//! saying so). The software window has Y growing downward while the GL window has it growing
	//! upward, so a triangle with a positive signed area here is the one the shader backend keeps
	//! for `reject_front`: that mode drops the negative-area triangles and `reject_back` the
	//! positive-area ones.
	static bool SetupUnitSoftReject(int reject, float area)
	{
		switch (reject)
		{
			case GEN_REJECT_FRONT: return area < 0.0f;
			case GEN_REJECT_BACK: return area > 0.0f;
			case GEN_REJECT_ALL: return true;
			default: return false;
		}
	}

	void SetupUnit::SoftSetupTriangle(const SoftVertex& v0, const SoftVertex& v1, const SoftVertex& v2)
	{
		const SoftVertex* v[3] = { &v0, &v1, &v2 };

		float x[3], y[3], z[3], w[3];
		for (int i = 0; i < 3; i++)
		{
			x[i] = v[i]->x;
			y[i] = v[i]->y;
			z[i] = v[i]->z;
			w[i] = (v[i]->invW != 0.0f) ? v[i]->invW : 1.0f;
		}

		float area = SoftSignedArea(x, y);

		if (area == 0.0f)
			return;

		if (SetupUnitSoftReject(gfx->genmode.reject_en, area))
			return;

		SoftTriangle tri;
		for (int i = 0; i < 3; i++)
		{
			tri.x[i] = x[i];
			tri.y[i] = y[i];
		}
		tri.area = area;

		// The edge coefficients, normalized so that the interior is positive (the `su_edge` stage of
		// gfx-su.md 3.4). `e[i]` is the edge opposite to vertex i: e0 = (v1, v2), e1 = (v2, v0),
		// e2 = (v0, v1); the coefficient vector is the edge direction rotated so that a point
		// inside the triangle gives a positive value.
		{
			float s = (area > 0.0f) ? 1.0f : -1.0f;

			tri.e[0][0] = -(y[2] - y[1]) * s;
			tri.e[0][1] = (x[2] - x[1]) * s;
			tri.e[0][2] = -tri.e[0][0] * x[1] - tri.e[0][1] * y[1];

			tri.e[1][0] = -(y[0] - y[2]) * s;
			tri.e[1][1] = (x[0] - x[2]) * s;
			tri.e[1][2] = -tri.e[1][0] * x[2] - tri.e[1][1] * y[2];

			tri.e[2][0] = -(y[1] - y[0]) * s;
			tri.e[2][1] = (x[1] - x[0]) * s;
			tri.e[2][2] = -tri.e[2][0] * x[0] - tri.e[2][1] * y[0];
		}

		// The raster bounding box, clamped to the EFB (the walk is done in 2x2 quads, the rasterizer
		// aligns the box itself).
		{
			float minx = x[0], maxx = x[0], miny = y[0], maxy = y[0];
			for (int i = 1; i < 3; i++)
			{
				if (x[i] < minx) minx = x[i];
				if (x[i] > maxx) maxx = x[i];
				if (y[i] < miny) miny = y[i];
				if (y[i] > maxy) maxy = y[i];
			}

			int iw = (int)gfx->RenderWidth();
			int ih = (int)gfx->RenderHeight();

			tri.minx = (int)floorf(minx);
			tri.miny = (int)floorf(miny);
			tri.maxx = (int)ceilf(maxx) - 1;
			tri.maxy = (int)ceilf(maxy) - 1;

			if (tri.minx < 0) tri.minx = 0;
			if (tri.miny < 0) tri.miny = 0;
			if (tri.maxx > iw - 1) tri.maxx = iw - 1;
			if (tri.maxy > ih - 1) tri.maxy = ih - 1;
		}

		// Depth. GEN_MODE.zfreeze holds the Z plane of the last triangle (gfx-su.md 3.4).
		if (gfx->genmode.zfreeze != 0 && soft_zfreeze_valid)
		{
			tri.z = soft_zfreeze;
		}
		else
		{
			SoftBuildPlane(z, x, y, &tri.z);

			if (gfx->genmode.zfreeze != 0)
			{
				soft_zfreeze = tri.z;
				soft_zfreeze_valid = true;
			}
		}

		// 1/w (the perspective correction of RAS1/RAS2)
		SoftBuildPlane(w, x, y, &tri.invW);

		// The rasterized colours (RAS2) and the texture coordinates (RAS1), divided by w.
		//
		// Both channels and all eight coordinate pairs get a plane: the GEN_MODE counts (ncol,
		// ntex) pace the setup stream of the hardware, they do not switch the attributes off, and
		// the TEV stage bindings decide which of them a draw really reads.
		//
		// For flat shading the colour planes have zero gradients (gfx-ras2.md 3.1): every
		// coefficient of the value plane is the provoking vertex's colour times the 1/w plane, so
		// that the ratio `value / (1/w)` is the constant colour of that vertex.
		bool flat = (gfx->genmode.flat_en != 0);

		for (int ch = 0; ch < 2; ch++)
		{
			for (int c = 0; c < 4; c++)
			{
				float val[3];
				for (int i = 0; i < 3; i++)
					val[i] = v[i]->color[ch][c] * 255.0f * w[i];

				SoftBuildPlane(val, x, y, &tri.color[ch][c]);

				if (flat)
				{
					float k = v[2]->color[ch][c] * 255.0f;
					tri.color[ch][c].o = tri.invW.o * k;
					tri.color[ch][c].dx = tri.invW.dx * k;
					tri.color[ch][c].dy = tri.invW.dy * k;
				}
			}
		}

		for (int i = 0; i < 8; i++)
		{
			for (int c = 0; c < 2; c++)
			{
				float val[3];
				for (int k = 0; k < 3; k++)
					val[k] = v[k]->tex[i][c] * w[k];

				SoftBuildPlane(val, x, y, &tri.tex[i][c]);
			}
		}

		gfx->ras->SoftDrawTriangle(tri);
	}

	// A point is expanded into the square quad of `psize` around it (gfx-su.md 3.3: the sequencer
	// adds/subtracts the point size to/from the vertex coordinates to build the geometry the edge
	// walker scans). SU_LPSIZE holds the size in 1/16 pixel units.
	void SetupUnit::SoftEmitPoint(const SoftVertex& v)
	{
		float size = (float)su.lpsize.psize / 16.0f;
		if (size <= 0.0f)
			size = 1.0f;

		float h = size * 0.5f;

		SoftVertex quad[4] = { v, v, v, v };
		quad[0].x = v.x - h; quad[0].y = v.y - h;
		quad[1].x = v.x + h; quad[1].y = v.y - h;
		quad[2].x = v.x + h; quad[2].y = v.y + h;
		quad[3].x = v.x - h; quad[3].y = v.y + h;

		gfx->xf->SoftClipTriangle(quad[0], quad[1], quad[2]);
		gfx->xf->SoftClipTriangle(quad[0], quad[2], quad[3]);
	}

	// A line is expanded into the quad strip of `lsize` around it, with the attributes of its two
	// endpoints (gfx-su.md 3.3). The width is programmed in 1/16 pixel units as well.
	void SetupUnit::SoftEmitLine(const SoftVertex& a, const SoftVertex& b)
	{
		float size = (float)su.lpsize.lsize / 16.0f;
		if (size <= 0.0f)
			size = 1.0f;

		float dx = b.x - a.x;
		float dy = b.y - a.y;
		float len = sqrtf(dx * dx + dy * dy);
		if (len <= 0.0f)
			return;

		// The half-width normal of the line direction
		float nx = -dy / len * size * 0.5f;
		float ny = dx / len * size * 0.5f;

		SoftVertex quad[4];
		quad[0] = a; quad[0].x = a.x + nx; quad[0].y = a.y + ny;
		quad[1] = b; quad[1].x = b.x + nx; quad[1].y = b.y + ny;
		quad[2] = b; quad[2].x = b.x - nx; quad[2].y = b.y - ny;
		quad[3] = a; quad[3].x = a.x - nx; quad[3].y = a.y - ny;

		gfx->xf->SoftClipTriangle(quad[0], quad[1], quad[2]);
		gfx->xf->SoftClipTriangle(quad[0], quad[2], quad[3]);
	}

	void SetupUnit::SoftBeginPrimitive(RAS_Primitive prim, size_t vtx_num)
	{
		soft_prim = prim;
		soft_vertices.clear();

		if (vtx_num > GFX_MAX_VERTICES)
			vtx_num = GFX_MAX_VERTICES;
		soft_vertices.reserve(vtx_num);
	}

	void SetupUnit::SoftSendVertex(const SoftVertex* v)
	{
		if (soft_vertices.size() < GFX_MAX_VERTICES)
			soft_vertices.push_back(*v);
	}

	void SetupUnit::SoftEndPrimitive()
	{
		const size_t n = soft_vertices.size();
		if (n == 0)
			return;

		switch (soft_prim)
		{
			case RAS_QUAD:
				for (size_t q = 0; q + 4 <= n; q += 4)
				{
					gfx->xf->SoftClipTriangle(soft_vertices[q + 0], soft_vertices[q + 1], soft_vertices[q + 2]);
					gfx->xf->SoftClipTriangle(soft_vertices[q + 0], soft_vertices[q + 2], soft_vertices[q + 3]);
				}
				break;

			case RAS_QUAD_STRIP:
				// Quad n uses the vertices (2n, 2n+1, 2n+3, 2n+2) as a fan (the expansion the CP
				// and the shader pipeline's index builder use)
				if (n >= 4)
				{
					for (size_t q = 0; q + 4 <= n; q += 2)
					{
						gfx->xf->SoftClipTriangle(soft_vertices[q + 0], soft_vertices[q + 1], soft_vertices[q + 3]);
						gfx->xf->SoftClipTriangle(soft_vertices[q + 0], soft_vertices[q + 3], soft_vertices[q + 2]);
					}
				}
				break;

			case RAS_TRIANGLE:
				for (size_t q = 0; q + 3 <= n; q += 3)
					gfx->xf->SoftClipTriangle(soft_vertices[q + 0], soft_vertices[q + 1], soft_vertices[q + 2]);
				break;

			case RAS_TRIANGLE_STRIP:
				for (size_t i = 0; i + 3 <= n; i++)
				{
					// The winding alternates on every triangle (gfx-su.md 3.1)
					if ((i & 1) == 0)
						gfx->xf->SoftClipTriangle(soft_vertices[i + 0], soft_vertices[i + 1], soft_vertices[i + 2]);
					else
						gfx->xf->SoftClipTriangle(soft_vertices[i + 1], soft_vertices[i + 0], soft_vertices[i + 2]);
				}
				break;

			case RAS_TRIANGLE_FAN:
				for (size_t i = 1; i + 1 < n; i++)
					gfx->xf->SoftClipTriangle(soft_vertices[0], soft_vertices[i], soft_vertices[i + 1]);
				break;

			case RAS_LINE:
				for (size_t i = 0; i + 2 <= n; i += 2)
					SoftEmitLine(soft_vertices[i + 0], soft_vertices[i + 1]);
				break;

			case RAS_LINE_STRIP:
				for (size_t i = 0; i + 1 < n; i++)
					SoftEmitLine(soft_vertices[i], soft_vertices[i + 1]);
				break;

			case RAS_POINT:
				for (size_t i = 0; i < n; i++)
					SoftEmitPoint(soft_vertices[i]);
				break;
		}

		soft_vertices.clear();
	}
}