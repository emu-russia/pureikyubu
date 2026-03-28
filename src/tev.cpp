#include "pch.h"

namespace GFX
{
	TextureEnvironmentUnit::TextureEnvironmentUnit(HWConfig* config, GFXCore* parent_gfx)
	{
		gfx = parent_gfx;
	}

	TextureEnvironmentUnit::~TextureEnvironmentUnit()
	{
	}

	void TextureEnvironmentUnit::loadTEVReg(size_t index, uint32_t value)
	{
		switch (index)
		{
			case TEV_COLOR_ENV_0_ID: tev.color_env[0].bits = value; break;
			case TEV_ALPHA_ENV_0_ID: tev.alpha_env[0].bits = value; break;
			case TEV_COLOR_ENV_1_ID: tev.color_env[1].bits = value; break;
			case TEV_ALPHA_ENV_1_ID: tev.alpha_env[1].bits = value; break;
			case TEV_COLOR_ENV_2_ID: tev.color_env[2].bits = value; break;
			case TEV_ALPHA_ENV_2_ID: tev.alpha_env[2].bits = value; break;
			case TEV_COLOR_ENV_3_ID: tev.color_env[3].bits = value; break;
			case TEV_ALPHA_ENV_3_ID: tev.alpha_env[3].bits = value; break;
			case TEV_COLOR_ENV_4_ID: tev.color_env[4].bits = value; break;
			case TEV_ALPHA_ENV_4_ID: tev.alpha_env[4].bits = value; break;
			case TEV_COLOR_ENV_5_ID: tev.color_env[5].bits = value; break;
			case TEV_ALPHA_ENV_5_ID: tev.alpha_env[5].bits = value; break;
			case TEV_COLOR_ENV_6_ID: tev.color_env[6].bits = value; break;
			case TEV_ALPHA_ENV_6_ID: tev.alpha_env[6].bits = value; break;
			case TEV_COLOR_ENV_7_ID: tev.color_env[7].bits = value; break;
			case TEV_ALPHA_ENV_7_ID: tev.alpha_env[7].bits = value; break;
			case TEV_COLOR_ENV_8_ID: tev.color_env[8].bits = value; break;
			case TEV_ALPHA_ENV_8_ID: tev.alpha_env[8].bits = value; break;
			case TEV_COLOR_ENV_9_ID: tev.color_env[9].bits = value; break;
			case TEV_ALPHA_ENV_9_ID: tev.alpha_env[9].bits = value; break;
			case TEV_COLOR_ENV_A_ID: tev.color_env[0xa].bits = value; break;
			case TEV_ALPHA_ENV_A_ID: tev.alpha_env[0xa].bits = value; break;
			case TEV_COLOR_ENV_B_ID: tev.color_env[0xb].bits = value; break;
			case TEV_ALPHA_ENV_B_ID: tev.alpha_env[0xb].bits = value; break;
			case TEV_COLOR_ENV_C_ID: tev.color_env[0xc].bits = value; break;
			case TEV_ALPHA_ENV_C_ID: tev.alpha_env[0xc].bits = value; break;
			case TEV_COLOR_ENV_D_ID: tev.color_env[0xd].bits = value; break;
			case TEV_ALPHA_ENV_D_ID: tev.alpha_env[0xd].bits = value; break;
			case TEV_COLOR_ENV_E_ID: tev.color_env[0xe].bits = value; break;
			case TEV_ALPHA_ENV_E_ID: tev.alpha_env[0xe].bits = value; break;
			case TEV_COLOR_ENV_F_ID: tev.color_env[0xf].bits = value; break;
			case TEV_ALPHA_ENV_F_ID: tev.alpha_env[0xf].bits = value; break;

			case TEV_REGISTERL_0_ID: tev.regl[0].bits = value; break;
			case TEV_REGISTERH_0_ID: tev.regh[0].bits = value; break;
			case TEV_REGISTERL_1_ID: tev.regl[1].bits = value; break;
			case TEV_REGISTERH_1_ID: tev.regh[1].bits = value; break;
			case TEV_REGISTERL_2_ID: tev.regl[2].bits = value; break;
			case TEV_REGISTERH_2_ID: tev.regh[2].bits = value; break;
			case TEV_REGISTERL_3_ID: tev.regl[3].bits = value; break;
			case TEV_REGISTERH_3_ID: tev.regh[3].bits = value; break;
			case TEV_RANGE_ADJ_C_ID: tev.rangeadj_control.bits = value; break;
			case TEV_RANGE_ADJ_0_ID: tev.range_adj[0].bits = value; break;
			case TEV_RANGE_ADJ_1_ID: tev.range_adj[1].bits = value; break;
			case TEV_RANGE_ADJ_2_ID: tev.range_adj[2].bits = value; break;
			case TEV_RANGE_ADJ_3_ID: tev.range_adj[3].bits = value; break;
			case TEV_RANGE_ADJ_4_ID: tev.range_adj[4].bits = value; break;
			case TEV_FOG_PARAM_0_ID: tev.fog_param0.bits = value; break;
			case TEV_FOG_PARAM_1_ID: tev.fog_param1.bits = value; break;
			case TEV_FOG_PARAM_2_ID: tev.fog_param2.bits = value; break;
			case TEV_FOG_PARAM_3_ID: tev.fog_param3.bits = value; break;
			case TEV_FOG_COLOR_ID: tev.fog_color.bits = value; break;
			case TEV_ALPHAFUNC_ID: tev.alpha_func.bits = value; break;
			case TEV_Z_ENV_0_ID: tev.zenv0.bits = value; break;
			case TEV_Z_ENV_1_ID: tev.zenv1.bits = value; break;
			case TEV_KSEL_0_ID: tev.ksel[0].bits = value; break;
			case TEV_KSEL_1_ID: tev.ksel[1].bits = value; break;
			case TEV_KSEL_2_ID: tev.ksel[2].bits = value; break;
			case TEV_KSEL_3_ID: tev.ksel[3].bits = value; break;
			case TEV_KSEL_4_ID: tev.ksel[4].bits = value; break;
			case TEV_KSEL_5_ID: tev.ksel[5].bits = value; break;
			case TEV_KSEL_6_ID: tev.ksel[6].bits = value; break;
			case TEV_KSEL_7_ID: tev.ksel[7].bits = value; break;

			default:
			{
				Debug::Report(Debug::Channel::GP, "Unknown reg load, index: 0x%02X\n", index);
				break;
			}
		}
	}
}