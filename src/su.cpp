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
	void SetupUnit::loadBPReg(size_t index, uint32_t value)
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
			return;

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

				scis0.bits = value;

				x = scis0.sux - 342;
				y = scis0.suy - 342;
				w = scis1.suw - scis0.sux + 1;
				h = scis1.suh - scis0.suy + 1;

				//GFXError("scissor (%i, %i)-(%i, %i)", x, y, w, h);
				GL_SetScissor(x, y, w, h);
			}
			return;

			case SU_SCIS1_ID:
			{
				int x, y, w, h;

				scis1.bits = value;

				x = scis0.sux - 342;
				y = scis0.suy - 342;
				w = scis1.suw - scis0.sux + 1;
				h = scis1.suh - scis0.suy + 1;

				//GFXError("scissor (%i, %i)-(%i, %i)", x, y, w, h);
				GL_SetScissor(x, y, w, h);
			}
			return;

			// Pixel Engine block

			case PE_ZMODE_ID:
			{
				static const char* zf[] = {
					"NEVER",
					"LESS",
					"EQUAL",
					"LEQUAL",
					"GREATER",
					"NEQUAL",
					"GEQUAL",
					"ALWAYS"
				};

				static uint32_t glzf[] = {
					GL_NEVER,
					GL_LESS,
					GL_EQUAL,
					GL_LEQUAL,
					GL_GREATER,
					GL_NOTEQUAL,
					GL_GEQUAL,
					GL_ALWAYS
				};

				gfx->pe->pe.zmode.bits = value;

				/*/
				GFXError(
					"z mode:\n"
					"compare: %s\n"
					"func: %s\n"
					"update: %s",
					(bpRegs.zmode.enable) ? ("yes") : ("no"),
					zf[bpRegs.zmode.func],
					(bpRegs.zmode.mask) ? ("yes") : ("no")
				);
				/*/

				if (gfx->pe->pe.zmode.enable)
				{
					glEnable(GL_DEPTH_TEST);
					glDepthFunc(glzf[gfx->pe->pe.zmode.func]);
					glDepthMask(gfx->pe->pe.zmode.mask);
				}
				else glDisable(GL_DEPTH_TEST);
			}
			return;

			// set blending rules
			case PE_CMODE0_ID:
			{
				gfx->pe->pe.cmode0.bits = value;

				static const char* logicop[] = {
					"clear",
					"and",
					"revand",
					"copy",
					"invand",
					"nop",
					"xor",
					"or",
					"nor",
					"eqv",
					"inv",
					"revor",
					"invcopy",
					"invor",
					"nand",
					"set"
				};

				static const char* sfactor[] = {
					"zero",
					"one",
					"srcclr",
					"invsrcclr",
					"srcalpha",
					"invsrcalpha",
					"dstalpha",
					"invdstalpha"
				};

				static const char* dfactor[] = {
					"zero",
					"one",
					"dstclr",
					"invdstclr",
					"srcalpha",
					"invsrcalpha",
					"dstalpha",
					"invdstalpha"
				};

				/*/
				GFXError(
					"blend rules\n\n"
					"blend:%s, logic:%s\n"
					"logic op : %s\n"
					"sfactor : %s\n"
					"dfactor : %s\n",
					(bpRegs.cmode0.blend_en) ? ("on") : ("off"),
					(bpRegs.cmode0.logop_en) ? ("on") : ("off"),
					logicop[bpRegs.cmode0.logop],
					sfactor[bpRegs.cmode0.sfactor],
					dfactor[bpRegs.cmode0.dfactor]
				);
				/*/

				static uint32_t glsf[] = {
					GL_ZERO,
					GL_ONE,
					GL_SRC_COLOR,
					GL_ONE_MINUS_SRC_COLOR,
					GL_SRC_ALPHA,
					GL_ONE_MINUS_SRC_ALPHA,
					GL_DST_ALPHA,
					GL_ONE_MINUS_DST_ALPHA
				};

				static uint32_t gldf[] = {
					GL_ZERO,
					GL_ONE,
					GL_DST_COLOR,
					GL_ONE_MINUS_DST_COLOR,
					GL_SRC_ALPHA,
					GL_ONE_MINUS_SRC_ALPHA,
					GL_DST_ALPHA,
					GL_ONE_MINUS_DST_ALPHA
				};

				// blend hack
				if (gfx->pe->pe.cmode0.blend_en)
				{
					glEnable(GL_BLEND);
					glBlendFunc(glsf[gfx->pe->pe.cmode0.sfactor], gldf[gfx->pe->pe.cmode0.dfactor]);
				}
				else glDisable(GL_BLEND);

				static uint32_t logop[] = {
					GL_CLEAR,
					GL_AND,
					GL_AND_REVERSE,
					GL_COPY,
					GL_AND_INVERTED,
					GL_NOOP,
					GL_XOR,
					GL_OR,
					GL_NOR,
					GL_EQUIV,
					GL_INVERT,
					GL_OR_REVERSE,
					GL_COPY_INVERTED,
					GL_OR_INVERTED,
					GL_NAND,
					GL_SET
				};

				// logic operations
				if (gfx->pe->pe.cmode0.logop_en)
				{
					glEnable(GL_COLOR_LOGIC_OP);
					glLogicOp(logop[gfx->pe->pe.cmode0.logop]);
				}
				else glDisable(GL_COLOR_LOGIC_OP);
			}
			return;

			case PE_CMODE1_ID:
				gfx->pe->pe.cmode1.bits = value;
				break;

			// TODO: Make a GP update when copying the frame buffer by Pixel Engine.

			// draw done
			case PE_FINISH_ID:
			{
				gfx->GPFrameDone();

				gfx->pe->pe_done_num++;
				if (gfx->pe->pe_done_num == 1)
				{
					Flipper::HW->vi->VIDisableXfb();	// disable VI output
				}
				gfx->pe->DONE_INT();
			}
			break;

			case PE_TOKEN_ID:
			{
				gfx->pe->pe.token.bits = value;
				if (gfx->pe->pe.token.token == gfx->pe->pe.token_int.token)
				{
					gfx->GPFrameDone();

					Flipper::HW->vi->VIDisableXfb();	// disable VI output
					gfx->pe->TOKEN_INT();
				}
			}
			break;

			// token
			case PE_TOKEN_INT_ID:
				gfx->pe->pe.token_int.bits = value;
				break;

			case PE_COPY_CLEAR_AR_ID:
				gfx->pe->pe.copy_clear_ar.bits = value;
				break;

			case PE_COPY_CLEAR_GB_ID:
				gfx->pe->pe.copy_clear_gb.bits = value;
				break;

			case PE_COPY_CLEAR_Z_ID:
				gfx->pe->pe.copy_clear_z.bits = value;
				break;

			//
			// SetImage0. texture image width, height, format
			//

			case TX_SETIMAGE0_I0_ID:
			{
				gfx->tx->teximg0[0].bits = value;
				gfx->tx->texvalid[0][0] = true;
				gfx->tx->tryLoadTex(0);
			}
			return;

			case TX_SETIMAGE0_I1_ID:
			{
				gfx->tx->teximg0[1].bits = value;
				gfx->tx->texvalid[0][1] = true;
				//tryLoadTex(1);
			}
			return;

			case TX_SETIMAGE0_I2_ID:
			{
				gfx->tx->teximg0[2].bits = value;
				gfx->tx->texvalid[0][2] = true;
				//tryLoadTex(2);
			}
			return;

			case TX_SETIMAGE0_I3_ID:
			{
				gfx->tx->teximg0[3].bits = value;
				gfx->tx->texvalid[0][3] = true;
				//tryLoadTex(3);
			}
			return;

			case TX_SETIMAGE0_I4_ID:
			{
				gfx->tx->teximg0[4].bits = value;
				gfx->tx->texvalid[0][4] = true;
				//tryLoadTex(4);
			}
			return;

			case TX_SETIMAGE0_I5_ID:
			{
				gfx->tx->teximg0[5].bits = value;
				gfx->tx->texvalid[0][5] = true;
				//tryLoadTex(5);
			}
			return;

			case TX_SETIMAGE0_I6_ID:
			{
				gfx->tx->teximg0[6].bits = value;
				gfx->tx->texvalid[0][6] = true;
				//tryLoadTex(6);
			}
			return;

			case TX_SETIMAGE0_I7_ID:
			{
				gfx->tx->teximg0[7].bits = value;
				gfx->tx->texvalid[0][7] = true;
				//tryLoadTex(7);
			}
			return;

			// SetImage1

			// SetImage2

			//
			// SetImage3. texture image base
			//

			case TX_SETIMAGE3_I0_ID:
			{
				gfx->tx->teximg3[0].bits = value;
				gfx->tx->texvalid[3][0] = true;
				gfx->tx->tryLoadTex(0);
			}
			return;

			case TX_SETIMAGE3_I1_ID:
			{
				gfx->tx->teximg3[1].bits = value;
				gfx->tx->texvalid[3][1] = true;
				//tryLoadTex(1);
			}
			return;

			case TX_SETIMAGE3_I2_ID:
			{
				gfx->tx->teximg3[2].bits = value;
				gfx->tx->texvalid[3][2] = true;
				//tryLoadTex(2);
			}
			return;

			case TX_SETIMAGE3_I3_ID:
			{
				gfx->tx->teximg3[3].bits = value;
				gfx->tx->texvalid[3][3] = true;
				//tryLoadTex(3);
			}
			return;

			case TX_SETIMAGE3_I4_ID:
			{
				gfx->tx->teximg3[4].bits = value;
				gfx->tx->texvalid[3][4] = true;
				//tryLoadTex(4);
			}
			return;

			case TX_SETIMAGE3_I5_ID:
			{
				gfx->tx->teximg3[5].bits = value;
				gfx->tx->texvalid[3][5] = true;
				//tryLoadTex(5);
			}
			return;

			case TX_SETIMAGE3_I6_ID:
			{
				gfx->tx->teximg3[6].bits = value;
				gfx->tx->texvalid[3][6] = true;
				//tryLoadTex(6);
			}
			return;

			case TX_SETIMAGE3_I7_ID:
			{
				gfx->tx->teximg3[7].bits = value;
				gfx->tx->texvalid[3][7] = true;
				//tryLoadTex(7);
			}
			return;

			//
			// load tlut
			//

			case TX_LOADTLUT0_ID:
			{
				gfx->tx->loadtlut0.bits = value;

				gfx->tx->LoadTlut(
					(gfx->tx->loadtlut0.base << 5),   // ram address
					(gfx->tx->loadtlut1.tmem << 9),   // tlut offset
					gfx->tx->loadtlut1.count          // tlut size
				);
			}
			return;

			case TX_LOADTLUT1_ID:
			{
				gfx->tx->loadtlut1.bits = value;

				gfx->tx->LoadTlut(
					(gfx->tx->loadtlut0.base << 5),   // ram address
					(gfx->tx->loadtlut1.tmem << 9),   // tlut offset
					gfx->tx->loadtlut1.count          // tlut size
				);
			}
			return;

			//
			// set tlut
			//

			case TX_SETTLUT_I0_ID:
			{
				gfx->tx->settlut[0].bits = value;
			}
			return;

			//
			// set texture modes
			//

			case TX_SETMODE0_I0_ID:
			{
				gfx->tx->texmode0[0].bits = value;
			}
			return;

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
				ssize[num].bits = value;
			}
			return;

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
				tsize[num].bits = value;
			}
			return;

#pragma region "TEV bypass"

			case TEV_COLOR_ENV_0_ID: gfx->tev->tev.color_env[0].bits = value; break;
			case TEV_ALPHA_ENV_0_ID: gfx->tev->tev.alpha_env[0].bits = value; break;
			case TEV_COLOR_ENV_1_ID: gfx->tev->tev.color_env[1].bits = value; break;
			case TEV_ALPHA_ENV_1_ID: gfx->tev->tev.alpha_env[1].bits = value; break;
			case TEV_COLOR_ENV_2_ID: gfx->tev->tev.color_env[2].bits = value; break;
			case TEV_ALPHA_ENV_2_ID: gfx->tev->tev.alpha_env[2].bits = value; break;
			case TEV_COLOR_ENV_3_ID: gfx->tev->tev.color_env[3].bits = value; break;
			case TEV_ALPHA_ENV_3_ID: gfx->tev->tev.alpha_env[3].bits = value; break;
			case TEV_COLOR_ENV_4_ID: gfx->tev->tev.color_env[4].bits = value; break;
			case TEV_ALPHA_ENV_4_ID: gfx->tev->tev.alpha_env[4].bits = value; break;
			case TEV_COLOR_ENV_5_ID: gfx->tev->tev.color_env[5].bits = value; break;
			case TEV_ALPHA_ENV_5_ID: gfx->tev->tev.alpha_env[5].bits = value; break;
			case TEV_COLOR_ENV_6_ID: gfx->tev->tev.color_env[6].bits = value; break;
			case TEV_ALPHA_ENV_6_ID: gfx->tev->tev.alpha_env[6].bits = value; break;
			case TEV_COLOR_ENV_7_ID: gfx->tev->tev.color_env[7].bits = value; break;
			case TEV_ALPHA_ENV_7_ID: gfx->tev->tev.alpha_env[7].bits = value; break;
			case TEV_COLOR_ENV_8_ID: gfx->tev->tev.color_env[8].bits = value; break;
			case TEV_ALPHA_ENV_8_ID: gfx->tev->tev.alpha_env[8].bits = value; break;
			case TEV_COLOR_ENV_9_ID: gfx->tev->tev.color_env[9].bits = value; break;
			case TEV_ALPHA_ENV_9_ID: gfx->tev->tev.alpha_env[9].bits = value; break;
			case TEV_COLOR_ENV_A_ID: gfx->tev->tev.color_env[0xa].bits = value; break;
			case TEV_ALPHA_ENV_A_ID: gfx->tev->tev.alpha_env[0xa].bits = value; break;
			case TEV_COLOR_ENV_B_ID: gfx->tev->tev.color_env[0xb].bits = value; break;
			case TEV_ALPHA_ENV_B_ID: gfx->tev->tev.alpha_env[0xb].bits = value; break;
			case TEV_COLOR_ENV_C_ID: gfx->tev->tev.color_env[0xc].bits = value; break;
			case TEV_ALPHA_ENV_C_ID: gfx->tev->tev.alpha_env[0xc].bits = value; break;
			case TEV_COLOR_ENV_D_ID: gfx->tev->tev.color_env[0xd].bits = value; break;
			case TEV_ALPHA_ENV_D_ID: gfx->tev->tev.alpha_env[0xd].bits = value; break;
			case TEV_COLOR_ENV_E_ID: gfx->tev->tev.color_env[0xe].bits = value; break;
			case TEV_ALPHA_ENV_E_ID: gfx->tev->tev.alpha_env[0xe].bits = value; break;
			case TEV_COLOR_ENV_F_ID: gfx->tev->tev.color_env[0xf].bits = value; break;
			case TEV_ALPHA_ENV_F_ID: gfx->tev->tev.alpha_env[0xf].bits = value; break;

			case TEV_REGISTERL_0_ID: gfx->tev->tev.regl[0].bits = value; break;
			case TEV_REGISTERH_0_ID: gfx->tev->tev.regh[0].bits = value; break;
			case TEV_REGISTERL_1_ID: gfx->tev->tev.regl[1].bits = value; break;
			case TEV_REGISTERH_1_ID: gfx->tev->tev.regh[1].bits = value; break;
			case TEV_REGISTERL_2_ID: gfx->tev->tev.regl[2].bits = value; break;
			case TEV_REGISTERH_2_ID: gfx->tev->tev.regh[2].bits = value; break;
			case TEV_REGISTERL_3_ID: gfx->tev->tev.regl[3].bits = value; break;
			case TEV_REGISTERH_3_ID: gfx->tev->tev.regh[3].bits = value; break;
			case TEV_RANGE_ADJ_C_ID: gfx->tev->tev.rangeadj_control.bits = value; break;
			case TEV_RANGE_ADJ_0_ID: gfx->tev->tev.range_adj[0].bits = value; break;
			case TEV_RANGE_ADJ_1_ID: gfx->tev->tev.range_adj[1].bits = value; break;
			case TEV_RANGE_ADJ_2_ID: gfx->tev->tev.range_adj[2].bits = value; break;
			case TEV_RANGE_ADJ_3_ID: gfx->tev->tev.range_adj[3].bits = value; break;
			case TEV_RANGE_ADJ_4_ID: gfx->tev->tev.range_adj[4].bits = value; break;
			case TEV_FOG_PARAM_0_ID: gfx->tev->tev.fog_param0.bits = value; break;
			case TEV_FOG_PARAM_1_ID: gfx->tev->tev.fog_param1.bits = value; break;
			case TEV_FOG_PARAM_2_ID: gfx->tev->tev.fog_param2.bits = value; break;
			case TEV_FOG_PARAM_3_ID: gfx->tev->tev.fog_param3.bits = value; break;
			case TEV_FOG_COLOR_ID: gfx->tev->tev.fog_color.bits = value; break;
			case TEV_ALPHAFUNC_ID: gfx->tev->tev.alpha_func.bits = value; break;
			case TEV_Z_ENV_0_ID: gfx->tev->tev.zenv0.bits = value; break;
			case TEV_Z_ENV_1_ID: gfx->tev->tev.zenv1.bits = value; break;
			case TEV_KSEL_0_ID: gfx->tev->tev.ksel[0].bits = value; break;
			case TEV_KSEL_1_ID: gfx->tev->tev.ksel[1].bits = value; break;
			case TEV_KSEL_2_ID: gfx->tev->tev.ksel[2].bits = value; break;
			case TEV_KSEL_3_ID: gfx->tev->tev.ksel[3].bits = value; break;
			case TEV_KSEL_4_ID: gfx->tev->tev.ksel[4].bits = value; break;
			case TEV_KSEL_5_ID: gfx->tev->tev.ksel[5].bits = value; break;
			case TEV_KSEL_6_ID: gfx->tev->tev.ksel[6].bits = value; break;
			case TEV_KSEL_7_ID: gfx->tev->tev.ksel[7].bits = value; break;

#pragma endregion "TEV bypass"

			default:
			{
				Report(Channel::GP, "Unknown BP load, index: 0x%02X\n", index);
				break;
			}
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