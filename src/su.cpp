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
	}
}