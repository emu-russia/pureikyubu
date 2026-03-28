// Setup Unit
#include "pch.h"

// Processing of BP address space registers Load Commands

using namespace Debug;

#define NO_VIEWPORT

namespace GFX
{

	void SetupUnit::GL_SetScissor(int x, int y, int w, int h)
	{
		//h += 32;
#ifndef NO_VIEWPORT
		glScissor(x, scr_h - (h + y), w, h);
#endif
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
				int x, y, w, h;

				su.scis0.bits = value;

				x = su.scis0.sux - 342;
				y = su.scis0.suy - 342;
				w = su.scis1.suw - su.scis0.sux + 1;
				h = su.scis1.suh - su.scis0.suy + 1;

				//GFXError("scissor (%i, %i)-(%i, %i)", x, y, w, h);
				GL_SetScissor(x, y, w, h);
			}
			break;

			case SU_SCIS1_ID:
			{
				int x, y, w, h;

				su.scis1.bits = value;

				x = su.scis0.sux - 342;
				y = su.scis0.suy - 342;
				w = su.scis1.suw - su.scis0.sux + 1;
				h = su.scis1.suh - su.scis0.suy + 1;

				//GFXError("scissor (%i, %i)-(%i, %i)", x, y, w, h);
				GL_SetScissor(x, y, w, h);
			}
			break;

			//
			// texture coord scale
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
				int num = (index >> 1) & 1;
				su.ssize[num].bits = value;
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
				int num = (index >> 1) & 1;
				su.tsize[num].bits = value;
			}
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
}