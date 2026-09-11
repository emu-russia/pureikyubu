#include "pch.h"

// There are still some parts of old sources with attempts to "abstract" the backend. It is absolutely hopeless, just use core OpenGL and don't worry about it.
//
// The backend is a modern OpenGL 3.3 (GLSL 330) shader pipeline:
// - the XF (Transform Unit) is emulated by a vertex shader (see xf.cpp);
// - the TEV (Texture Environment Unit) is emulated by a fragment shader (see tev.cpp).
//
// This module owns the GL context, the frame loop and the geometry buffers; the shaders themselves
// live with the pipeline blocks they emulate.

using namespace Debug;

namespace GFX
{
	int gfx_frame_counter = 0;

	// -------------------------------------------------------------------------------------------
	// GX debug interface (JDI)
	//
	// The commands below are the GX half of the Json Debug Interface (issue #87): they report the
	// state of the whole GFX pipeline, the generated shaders, the texture cache, the frame
	// statistics and the contents of the emulated EFB. The command descriptions live in the JSON
	// node right here, so a new command only has to be added in one file.
	//
	// Address conventions:
	//   * XF registers are the 16-bit XF register space (0x0000..0x1057, see xf.h);
	//   * BP registers are the 8-bit bypass space (0x00..0xFF, see gfx.md 10.1);
	//   * EFB coordinates are in EFB pixels, the origin is the top left corner.
	// -------------------------------------------------------------------------------------------

	static const char* GfxJdi = R"json(
{
	"info":
	{
		"description": "Flipper GFX (GX) JDI",
		"helpGroup": "GFX Debug Commands"
	},

	"can": {
		"gx":
		{
			"help": "Show the state of the whole GFX pipeline",
			"output": "Object with one member per pipeline block and the frame counters"
		},

		"gxframes":
		{
			"help": "Show the GFX frame statistics",
			"output": "Object with the frame counters and the draw command counters of the CP"
		},

		"gxregs":
		{
			"help": "Dump the register state of one GFX block",
			"args": 1,
			"hints": "<block>",
			"usage": [
				"Syntax: gxregs <block>\n",
				"Blocks: xf, su, ras, tx, tev, pe, bump, cp, all\n",
				"Example of use: gxregs tev\n"
			],
			"output": "Object with the raw register fields of the block"
		},

		"gxshader":
		{
			"help": "Write the GLSL shaders the pipeline uses into files",
			"args": 1,
			"hints": "<basename>",
			"usage": [
				"Syntax: gxshader <basename>\n",
				"Writes <basename>.vert.glsl and <basename>.frag.glsl\n",
				"Example of use: gxshader gfx_shader\n"
			],
			"output": "Object with the file names and their sizes"
		},

		"gxtex":
		{
			"help": "Show the texture cache: what every one of the eight texture maps holds",
			"output": "Array of 8 objects (one per texture map) with the decoded image and its registers"
		},

		"gxshot":
		{
			"help": "Save a screenshot of the emulated EFB as a PNG file",
			"args": 1,
			"hints": "<filename.png> [x y width height]",
			"usage": [
				"Syntax: gxshot <filename.png> [x y width height]\n",
				"Without the optional rectangle the whole render target is saved.\n",
				"Example of use: gxshot frame.png\n"
			],
			"output": "Object with the file name and the size of the saved image"
		},

		"gxpixel":
		{
			"help": "Read one EFB pixel",
			"args": 2,
			"hints": "<x> <y>",
			"usage": [
				"Syntax: gxpixel <x> <y>\n",
				"Reads the colour and the depth of one EFB pixel (origin: top left).\n",
				"Example of use: gxpixel 320 240\n"
			],
			"output": "Object with r, g, b, a and z"
		},

		"gxreset":
		{
			"help": "Reset the GFX pipeline register state (the software equivalent of a GX reset)"
		},

		"gxtexdump":
		{
			"help": "Save the image of one texture map as a PNG file",
			"args": 2,
			"hints": "<map 0-7> <filename.png>",
			"usage": [
				"Syntax: gxtexdump <map> <filename.png>\n",
				"Example of use: gxtexdump 0 map0.png\n"
			],
			"output": "Object with the file name and the size of the saved image"
		}
	}
}
)json";

	GFXCore* gfx_jdi_instance = nullptr;

	static Json::Value* CmdGxState(std::vector<std::string>& args);
	static Json::Value* CmdGxFrames(std::vector<std::string>& args);
	static Json::Value* CmdGxRegs(std::vector<std::string>& args);
	static Json::Value* CmdGxShader(std::vector<std::string>& args);
	static Json::Value* CmdGxTex(std::vector<std::string>& args);
	static Json::Value* CmdGxShot(std::vector<std::string>& args);
	static Json::Value* CmdGxPixel(std::vector<std::string>& args);
	static Json::Value* CmdGxReset(std::vector<std::string>& args);
	static Json::Value* CmdGxTexDump(std::vector<std::string>& args);

	static void gfx_init_handlers()
	{
		JDI::Hub.AddCmd("gx", CmdGxState);
		JDI::Hub.AddCmd("gxframes", CmdGxFrames);
		JDI::Hub.AddCmd("gxregs", CmdGxRegs);
		JDI::Hub.AddCmd("gxshader", CmdGxShader);
		JDI::Hub.AddCmd("gxtex", CmdGxTex);
		JDI::Hub.AddCmd("gxshot", CmdGxShot);
		JDI::Hub.AddCmd("gxpixel", CmdGxPixel);
		JDI::Hub.AddCmd("gxreset", CmdGxReset);
		JDI::Hub.AddCmd("gxtexdump", CmdGxTexDump);
	}

	// -------------------------------------------------------------------------------------------
	// JDI helpers
	// -------------------------------------------------------------------------------------------

	//! The pipeline the debug commands talk to (the emulator has exactly one GFXCore).
	static GFXCore* Gfx()
	{
		return gfx_jdi_instance;
	}

	static Json::Value* MakeObject()
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Object;
		return output;
	}

	static Json::Value* MakeArray()
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;
		return output;
	}

	static void AddHex(Json::Value* obj, const char* name, uint32_t value)
	{
		// The Json engine only carries signed 64-bit integers, so an unsigned 32-bit register value
		// is reported as an unsigned decimal number and, for convenience, as a hex string.
		char hex[16];
		sprintf(hex, "0x%X", value);
		obj->AddUInt32(name, value);
		obj->AddAnsiString((std::string(name) + "Hex").c_str(), hex);
	}

	// A GL context is only current on the thread that drives the frame loop; the copy commands below
	// must not be issued from anywhere else (the JDI server and the debugger UI can run on another
	// thread).
	static bool GLContextCurrent()
	{
#ifdef _WINDOWS
		return wglGetCurrentContext() != 0;
#else
		return true;
#endif
	}

	static Json::Value* GLErrorValue(const wchar_t* text)
	{
		Json::Value* output = MakeObject();
		output->AddAnsiString("error", Util::WstringToString(text).c_str());
		return output;
	}

	// -------------------------------------------------------------------------------------------
	// gx - the whole pipeline state
	// -------------------------------------------------------------------------------------------

	static Json::Value* CmdGxState(std::vector<std::string>& args)
	{
		GFXCore* gfx = Gfx();
		if (gfx == nullptr)
		{
			return GLErrorValue(L"the GFX subsystem is not running");
		}

		Json::Value* output = MakeObject();

		Json::Value* common = output->AddObject("common");
		AddHex(common, "genmode", gfx->genmode.bits);
		common->AddInt("ntex", (int)gfx->genmode.ntex);
		common->AddInt("ncol", (int)gfx->genmode.ncol);
		common->AddInt("ntev", (int)gfx->genmode.ntev);
		common->AddInt("nbmp", (int)gfx->genmode.nbmp);
		common->AddBool("zfreeze", gfx->genmode.zfreeze != 0);
		common->AddInt("reject", (int)gfx->genmode.reject_en);
		common->AddBool("ms_en", gfx->genmode.ms_en != 0);
		common->AddBool("flat_en", gfx->genmode.flat_en != 0);
		common->AddInt("backend_started", gfx->BackendStarted() ? 1 : 0);
		common->AddInt("scr_w", (int)gfx->RenderWidth());
		common->AddInt("scr_h", (int)gfx->RenderHeight());

		Json::Value* xf = output->AddObject("xf");
		AddHex(xf, "numTex", gfx->xf->xf.numTex);
		AddHex(xf, "numColors", gfx->xf->xf.numColors);
		xf->AddBool("projectOrtho", gfx->xf->xf.projectOrtho);
		xf->AddBool("dualTexTran", gfx->xf->xf.dualTexTran != 0);
		xf->AddBool("vertexShader", gfx->xf->VertexShader() != 0);
		{
			Json::Value* proj = xf->AddArray("projection");
			for (int i = 0; i < 6; i++)
			{
				proj->AddFloat(nullptr, gfx->xf->xf.projectionParam[i]);
			}
			Json::Value* vp = xf->AddArray("viewport");
			for (int i = 0; i < 3; i++)
			{
				vp->AddFloat(nullptr, gfx->xf->xf.viewportScale[i]);
			}
			for (int i = 0; i < 3; i++)
			{
				vp->AddFloat(nullptr, gfx->xf->xf.viewportOffset[i]);
			}
			Json::Value* tg = xf->AddArray("texgen");
			for (int i = 0; i < 8; i++)
			{
				tg->AddUInt32(nullptr, gfx->xf->xf.tex[i].bits);
			}
		}

		Json::Value* su = output->AddObject("su");
		AddHex(su, "scis0", gfx->su->State().scis0.bits);
		AddHex(su, "scis1", gfx->su->State().scis1.bits);
		{
			Json::Value* size = su->AddArray("texSize");
			for (int i = 0; i < 8; i++)
			{
				size->AddUInt32(nullptr, gfx->su->State().ssize[i].bits);
				size->AddUInt32(nullptr, gfx->su->State().tsize[i].bits);
			}
		}

		Json::Value* ras = output->AddObject("ras");
		AddHex(ras, "iref", gfx->ras->Iref());
		{
			Json::Value* tref = ras->AddArray("tref");
			for (int i = 0; i < 8; i++)
			{
				tref->AddUInt32(nullptr, gfx->ras->Tref(i).bits);
			}
			Json::Value* ss = ras->AddArray("ss");
			ss->AddUInt32(nullptr, gfx->ras->SS(0).bits);
			ss->AddUInt32(nullptr, gfx->ras->SS(1).bits);
		}

		Json::Value* tev = output->AddObject("tev");
		AddHex(tev, "alphaFunc", gfx->tev->State().alpha_func.bits);
		AddHex(tev, "fogParam3", gfx->tev->State().fog_param3.bits);
		AddHex(tev, "zenv0", gfx->tev->State().zenv0.bits);
		AddHex(tev, "zenv1", gfx->tev->State().zenv1.bits);
		tev->AddBool("program", gfx->tev->GetTevProgramNoCreate() != nullptr);

		Json::Value* pe = output->AddObject("pe");
		AddHex(pe, "zmode", gfx->pe->State().zmode.bits);
		AddHex(pe, "cmode0", gfx->pe->State().cmode0.bits);
		AddHex(pe, "cmode1", gfx->pe->State().cmode1.bits);
		AddHex(pe, "control", gfx->pe->State().control.bits);
		AddHex(pe, "clearAR", gfx->pe->State().copy_clear_ar.bits);
		AddHex(pe, "clearGB", gfx->pe->State().copy_clear_gb.bits);
		AddHex(pe, "clearZ", gfx->pe->State().copy_clear_z.bits);

		Json::Value* bump = output->AddObject("bump");
		bump->AddBool("indirectActive", gfx->bump->IndirectActive());
		{
			Json::Value* cmd = bump->AddArray("cmd");
			for (int i = 0; i < 16; i++)
			{
				cmd->AddUInt32(nullptr, gfx->bump->State().cmd[i].bits);
			}
			Json::Value* mtx = bump->AddArray("matrix");
			for (int i = 0; i < 3; i++)
			{
				mtx->AddUInt32(nullptr, gfx->bump->State().matrix[i].a.bits);
				mtx->AddUInt32(nullptr, gfx->bump->State().matrix[i].b.bits);
				mtx->AddUInt32(nullptr, gfx->bump->State().matrix[i].c.bits);
			}
		}

		Json::Value* frames = output->AddObject("frames");
		frames->AddInt("pe_frames", (int)gfx->pe->Frames());
		frames->AddInt("gfx_frame_counter", gfx_frame_counter);

		return output;
	}

	// -------------------------------------------------------------------------------------------
	// gxframes - the frame and command statistics
	// -------------------------------------------------------------------------------------------

	static Json::Value* CmdGxFrames(std::vector<std::string>& args)
	{
		GFXCore* gfx = Gfx();
		if (gfx == nullptr)
		{
			return GLErrorValue(L"the GFX subsystem is not running");
		}

		Json::Value* output = MakeObject();

		output->AddInt("gfx_frame_counter", gfx_frame_counter);
		output->AddInt("pe_frames", (int)gfx->pe->Frames());

		if (Flipper::HW != nullptr && Flipper::HW->cp != nullptr)
		{
			Flipper::CommandProcessorStats stats{};
			Flipper::HW->cp->GetStats(&stats);

			Json::Value* cp = output->AddObject("cp");
			cp->AddInt("bp_loads", (int)stats.bpLoads);
			cp->AddInt("xf_loads", (int)stats.xfLoads);
			cp->AddInt("cp_loads", (int)stats.cpLoads);
			cp->AddInt("triangles", (int)stats.tris);
			cp->AddInt("points", (int)stats.points);
			cp->AddInt("lines", (int)stats.lines);
		}

		return output;
	}

	// -------------------------------------------------------------------------------------------
	// gxregs - the register state of one block
	// -------------------------------------------------------------------------------------------

	static Json::Value* CmdGxRegs(std::vector<std::string>& args)
	{
		GFXCore* gfx = Gfx();
		if (gfx == nullptr)
		{
			return GLErrorValue(L"the GFX subsystem is not running");
		}

		std::string block = args.size() > 1 ? args[1] : "all";
		Json::Value* output = MakeObject();

		if (block == "xf" || block == "all")
		{
			Json::Value* xf = output->AddObject("xf");
			for (size_t i = 0; i < 16; i++)
			{
				char name[32];
				sprintf(name, "mvTexMtx%zu", i);
				xf->AddFloat(name, gfx->xf->xf.mvTexMtx[i]);
			}
			for (int i = 0; i < 8; i++)
			{
				char name[32];
				sprintf(name, "light%d_rgba", i);
				AddHex(xf, name, gfx->xf->xf.light[i].rgba.RGBA);
				sprintf(name, "light%d_a0", i);
				xf->AddFloat(name, gfx->xf->xf.light[i].a[0]);
				sprintf(name, "light%d_k0", i);
				xf->AddFloat(name, gfx->xf->xf.light[i].k[0]);
			}
		}

		if (block == "pe" || block == "all")
		{
			Json::Value* pe = output->AddObject("pe");
			const char* names[] = {
				"zmode", "cmode0", "cmode1", "control", "field_mask", "finish", "refresh",
				"token", "token_int", "copy_src_addr", "copy_src_size", "copy_dst_base0",
				"copy_dst_base1", "copy_dst_stride", "copy_scale", "copy_clear_ar", "copy_clear_gb",
				"copy_clear_z", "copy_cmd", "vfilter0", "vfilter1", "xbound", "ybound", "perfmode",
				"chicken", "quad_offset"
			};
			const uint32_t* regs = (const uint32_t*)&gfx->pe->State();
			for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])); i++)
			{
				AddHex(pe, names[i], regs[i]);
			}
			AddHex(pe, "sr", gfx->pe->Regs().sr);
		}

		if (block == "tev" || block == "all")
		{
			Json::Value* tev = output->AddObject("tev");
			{
				Json::Value* env = tev->AddArray("colorEnv");
				for (int i = 0; i < 16; i++)
				{
					env->AddUInt32(nullptr, gfx->tev->State().color_env[i].bits);
				}
				Json::Value* aenv = tev->AddArray("alphaEnv");
				for (int i = 0; i < 16; i++)
				{
					aenv->AddUInt32(nullptr, gfx->tev->State().alpha_env[i].bits);
				}
				Json::Value* reg = tev->AddArray("colorReg");
				for (int i = 0; i < 4; i++)
				{
					reg->AddUInt32(nullptr, gfx->tev->State().regl[i].bits);
					reg->AddUInt32(nullptr, gfx->tev->State().regh[i].bits);
				}
				Json::Value* konst = tev->AddArray("konstReg");
				for (int i = 0; i < 4; i++)
				{
					konst->AddUInt32(nullptr, gfx->tev->State().kregl[i].bits);
					konst->AddUInt32(nullptr, gfx->tev->State().kregh[i].bits);
				}
				Json::Value* ksel = tev->AddArray("ksel");
				for (int i = 0; i < 8; i++)
				{
					ksel->AddUInt32(nullptr, gfx->tev->State().ksel[i].bits);
				}
			}
			AddHex(tev, "fogParam0", gfx->tev->State().fog_param0.bits);
			AddHex(tev, "fogParam1", gfx->tev->State().fog_param1.bits);
			AddHex(tev, "fogParam2", gfx->tev->State().fog_param2.bits);
			AddHex(tev, "fogParam3", gfx->tev->State().fog_param3.bits);
			AddHex(tev, "fogColor", gfx->tev->State().fog_color.bits);
			AddHex(tev, "alphaFunc", gfx->tev->State().alpha_func.bits);
			AddHex(tev, "zenv0", gfx->tev->State().zenv0.bits);
			AddHex(tev, "zenv1", gfx->tev->State().zenv1.bits);
		}

		if (block == "bump" || block == "all")
		{
			Json::Value* bump = output->AddObject("bump");
			AddHex(bump, "imask", gfx->bump->State().imask.bits);
			for (int i = 0; i < 16; i++)
			{
				char name[32];
				sprintf(name, "cmd%d", i);
				AddHex(bump, name, gfx->bump->State().cmd[i].bits);
			}
			for (int i = 0; i < 3; i++)
			{
				char name[32];
				sprintf(name, "matrix%d_a", i);
				AddHex(bump, name, gfx->bump->State().matrix[i].a.bits);
				sprintf(name, "matrix%d_b", i);
				AddHex(bump, name, gfx->bump->State().matrix[i].b.bits);
				sprintf(name, "matrix%d_c", i);
				AddHex(bump, name, gfx->bump->State().matrix[i].c.bits);
			}
		}

		if (block == "tx" || block == "all")
		{
			Json::Value* tx = output->AddObject("tx");
			Json::Value* maps = tx->AddArray("map");
			for (int i = 0; i < 8; i++)
			{
				const GFX::TexMap& m = gfx->tx->Map(i);
				Json::Value* item = maps->AddObject(nullptr);
				item->AddInt("valid", m.valid ? 1 : 0);
				item->AddInt("width", m.width);
				item->AddInt("height", m.height);
				item->AddInt("glWidth", m.dw);
				item->AddInt("glHeight", m.dh);
				item->AddUInt32("base", gfx->tx->State().teximg3[i].base << 5);
				AddHex(item, "image0", gfx->tx->State().teximg0[i].bits);
				AddHex(item, "mode0", gfx->tx->State().texmode0[i].bits);
			}
		}

		if (block == "su" || block == "all")
		{
			Json::Value* su = output->AddObject("su");
			AddHex(su, "scis0", gfx->su->State().scis0.bits);
			AddHex(su, "scis1", gfx->su->State().scis1.bits);
		}

		if (block == "ras" || block == "all")
		{
			Json::Value* ras = output->AddObject("ras");
			Json::Value* tref = ras->AddArray("tref");
			for (int i = 0; i < 8; i++)
			{
				tref->AddUInt32(nullptr, gfx->ras->Tref(i).bits);
			}
			AddHex(ras, "iref", gfx->ras->Iref());
		}

		if (block == "cp" || block == "all")
		{
			if (Flipper::HW != nullptr && Flipper::HW->cp != nullptr)
			{
				Flipper::CommandProcessorStats stats{};
				Flipper::HW->cp->GetStats(&stats);

				Json::Value* cp = output->AddObject("cp");
				cp->AddInt("bp_loads", (int)stats.bpLoads);
				cp->AddInt("xf_loads", (int)stats.xfLoads);
				cp->AddInt("cp_loads", (int)stats.cpLoads);
				cp->AddInt("triangles", (int)stats.tris);
				cp->AddInt("points", (int)stats.points);
				cp->AddInt("lines", (int)stats.lines);
			}
		}

		return output;
	}

	// -------------------------------------------------------------------------------------------
	// gxshader - write the generated shaders to files
	// -------------------------------------------------------------------------------------------

	static Json::Value* CmdGxShader(std::vector<std::string>& args)
	{
		GFXCore* gfx = Gfx();
		if (gfx == nullptr)
		{
			return GLErrorValue(L"the GFX subsystem is not running");
		}

		std::string base = args.size() > 1 ? args[1] : "gfx_shader";
		std::string vertName = base + ".vert.glsl";
		std::string fragName = base + ".frag.glsl";

		auto writeText = [](const std::string& name, const char* text) -> size_t
		{
			if (text == nullptr)
			{
				return 0;
			}

			FILE* f = fopen(name.c_str(), "wb");
			if (f == nullptr)
			{
				return 0;
			}

			size_t len = strlen(text);
			fwrite(text, 1, len, f);
			fclose(f);
			return len;
		};

		size_t vertSize = writeText(vertName, gfx->xf->VertexShaderSource());
		size_t fragSize = writeText(fragName, gfx->tev->FragmentShaderSource());

		Json::Value* output = MakeObject();
		output->AddAnsiString("vertexShader", vertName.c_str());
		output->AddInt("vertexShaderSize", (int)vertSize);
		output->AddAnsiString("fragmentShader", fragName.c_str());
		output->AddInt("fragmentShaderSize", (int)fragSize);
		return output;
	}

	// -------------------------------------------------------------------------------------------
	// gxtex - the texture cache
	// -------------------------------------------------------------------------------------------

	static Json::Value* CmdGxTex(std::vector<std::string>& args)
	{
		GFXCore* gfx = Gfx();
		if (gfx == nullptr)
		{
			return GLErrorValue(L"the GFX subsystem is not running");
		}

		Json::Value* output = MakeArray();

		for (int i = 0; i < 8; i++)
		{
			const TexMap& m = gfx->tx->Map(i);

			Json::Value* item = output->AddObject(nullptr);
			item->AddInt("map", i);
			item->AddInt("valid", m.valid ? 1 : 0);
			item->AddInt("dirty", m.dirty ? 1 : 0);
			item->AddUInt32("base", gfx->tx->State().teximg3[i].base << 5);
			AddHex(item, "image0", gfx->tx->State().teximg0[i].bits);
			item->AddInt("width", gfx->tx->State().teximg0[i].width + 1);
			item->AddInt("height", gfx->tx->State().teximg0[i].height + 1);
			item->AddInt("format", (int)gfx->tx->State().teximg0[i].fmt);
			AddHex(item, "mode0", gfx->tx->State().texmode0[i].bits);
			AddHex(item, "mode1", gfx->tx->State().texmode1[i].bits);
			AddHex(item, "tlut", gfx->tx->State().settlut[i].bits);
			item->AddInt("decodedWidth", m.width);
			item->AddInt("decodedHeight", m.height);
			item->AddInt("glWidth", m.dw);
			item->AddInt("glHeight", m.dh);
			item->AddBool("glObject", m.glTexture != 0);
		}

		return output;
	}

	// -------------------------------------------------------------------------------------------
	// gxshot / gxpixel / gxtexdump - reading the EFB back
	// -------------------------------------------------------------------------------------------

	//! Parse the optional "x y width height" tail of a command.
	static bool ParseRect(std::vector<std::string>& args, size_t first, GFXCore* gfx,
		int* x, int* y, int* width, int* height)
	{
		*x = 0;
		*y = 0;
		*width = (int)gfx->RenderWidth();
		*height = (int)gfx->RenderHeight();

		if (args.size() >= first + 4)
		{
			*x = atoi(args[first + 0].c_str());
			*y = atoi(args[first + 1].c_str());
			*width = atoi(args[first + 2].c_str());
			*height = atoi(args[first + 3].c_str());
		}

		if (*width <= 0 || *height <= 0)
		{
			return false;
		}

		if (*x < 0 || *y < 0 || (*x + *width) > (int)gfx->RenderWidth() || (*y + *height) > (int)gfx->RenderHeight())
		{
			return false;
		}

		return true;
	}

	//! Read a rectangle of the EFB into an RGB buffer, top row first.
	static bool ReadEfb(int x, int y, int width, int height, std::vector<uint8_t>& rgb)
	{
		rgb.resize((size_t)width * height * 3);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());

		// glReadPixels returns the bottom row first
		std::vector<uint8_t> flipped(rgb.size());
		for (int row = 0; row < height; row++)
		{
			memcpy(&flipped[(size_t)row * width * 3],
				&rgb[(size_t)(height - 1 - row) * width * 3], (size_t)width * 3);
		}
		rgb.swap(flipped);

		return true;
	}

	static Json::Value* CmdGxShot(std::vector<std::string>& args)
	{
		GFXCore* gfx = Gfx();
		if (gfx == nullptr)
		{
			return GLErrorValue(L"the GFX subsystem is not running");
		}

		if (args.size() < 2)
		{
			return GLErrorValue(L"gxshot: the file name is missing");
		}

		if (!GLContextCurrent())
		{
			return GLErrorValue(L"gxshot: no OpenGL context on the calling thread "
				L"(call it from the emulator thread while a frame is being rendered)");
		}

		int x, y, width, height;
		if (!ParseRect(args, 2, gfx, &x, &y, &width, &height))
		{
			return GLErrorValue(L"gxshot: the rectangle is outside the render target");
		}

		std::vector<uint8_t> rgb;
		ReadEfb(x, y, width, height, rgb);

		std::string filename = args[1];
		bool saved = Util::SavePng(filename.c_str(), rgb.data(), (size_t)width, (size_t)height);

		Json::Value* output = MakeObject();
		output->AddAnsiString("file", filename.c_str());
		output->AddInt("x", x);
		output->AddInt("y", y);
		output->AddInt("width", width);
		output->AddInt("height", height);
		output->AddBool("saved", saved);
		return output;
	}

	static Json::Value* CmdGxPixel(std::vector<std::string>& args)
	{
		GFXCore* gfx = Gfx();
		if (gfx == nullptr)
		{
			return GLErrorValue(L"the GFX subsystem is not running");
		}

		if (args.size() < 3)
		{
			return GLErrorValue(L"gxpixel: x and y are required");
		}

		if (!GLContextCurrent())
		{
			return GLErrorValue(L"gxpixel: no OpenGL context on the calling thread");
		}

		// The command uses EFB coordinates with the origin at the top left; GL reads bottom-up.
		int x = atoi(args[1].c_str());
		int y = atoi(args[2].c_str());

		if (x < 0 || y < 0 || x >= (int)gfx->RenderWidth() || y >= (int)gfx->RenderHeight())
		{
			return GLErrorValue(L"gxpixel: the coordinates are outside the render target");
		}

		uint8_t rgba[4] = { 0 };
		GLfloat depth = 0.0f;

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(x, (int)gfx->RenderHeight() - 1 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
		glReadPixels(x, (int)gfx->RenderHeight() - 1 - y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

		Json::Value* output = MakeObject();
		output->AddInt("x", x);
		output->AddInt("y", y);
		output->AddInt("r", rgba[0]);
		output->AddInt("g", rgba[1]);
		output->AddInt("b", rgba[2]);
		output->AddInt("a", rgba[3]);
		output->AddInt("z24", (int)(depth * 16777215.0f));
		output->AddFloat("z", depth);
		return output;
	}

	static Json::Value* CmdGxTexDump(std::vector<std::string>& args)
	{
		GFXCore* gfx = Gfx();
		if (gfx == nullptr)
		{
			return GLErrorValue(L"the GFX subsystem is not running");
		}

		if (args.size() < 3)
		{
			return GLErrorValue(L"gxtexdump: the map id and the file name are required");
		}

		int id = atoi(args[1].c_str()) & 7;
		std::string filename = args[2];

		if (!GLContextCurrent())
		{
			return GLErrorValue(L"gxtexdump: no OpenGL context on the calling thread");
		}

		Json::Value* output = MakeObject();
		output->AddInt("map", id);
		output->AddAnsiString("file", filename.c_str());

		// The image is decoded from main memory on demand, which is what the draw path does as well
		std::vector<uint8_t> rgb;
		int width = 0, height = 0;

		if (!gfx->tx->DumpTexture(id, rgb, &width, &height))
		{
			output->AddBool("saved", false);
			output->AddAnsiString("reason", "the texture map is not valid");
			return output;
		}

		output->AddBool("saved", Util::SavePng(filename.c_str(), rgb.data(), (size_t)width, (size_t)height));
		output->AddInt("width", width);
		output->AddInt("height", height);
		return output;
	}

	static Json::Value* CmdGxReset(std::vector<std::string>& args)
	{
		GFXCore* gfx = Gfx();
		if (gfx == nullptr)
		{
			return GLErrorValue(L"the GFX subsystem is not running");
		}

		gfx->ResetPipelineState();

		Json::Value* output = MakeObject();
		output->AddBool("reset", true);
		return output;
	}

	// -------------------------------------------------------------------------------------------
	// GL object helpers

	GLuint CompileShaderStage(GLenum type, const char* source, const char* label)
	{
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);

		GLint success = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			char infoLog[0x10000] = { 0, };
			glGetShaderInfoLog(shader, sizeof(infoLog) - 1, nullptr, infoLog);
			Report(Channel::GP, "%s SHADER COMPILE ERROR:\n%s\n", label, infoLog);
			glDeleteShader(shader);
			return 0;
		}

		return shader;
	}

	GLProgram::~GLProgram()
	{
		Destroy();
	}

	bool GLProgram::Link(GLuint vertShader, const char* fragSource, const char* label)
	{
		if (!vertShader || !fragSource)
			return false;

		GLuint fragShader = CompileShaderStage(GL_FRAGMENT_SHADER, fragSource, label);
		if (!fragShader)
			return false;

		prog = glCreateProgram();
		glAttachShader(prog, vertShader);
		glAttachShader(prog, fragShader);
		glLinkProgram(prog);

		glDeleteShader(fragShader);

		GLint success = 0;
		glGetProgramiv(prog, GL_LINK_STATUS, &success);
		if (!success)
		{
			char infoLog[0x10000] = { 0, };
			glGetProgramInfoLog(prog, sizeof(infoLog) - 1, nullptr, infoLog);
			Report(Channel::GP, "%s SHADER LINK ERROR:\n%s\n", label, infoLog);
			glDeleteProgram(prog);
			prog = 0;
			return false;
		}

		return true;
	}

	void GLProgram::Destroy()
	{
		if (prog)
		{
			glDeleteProgram(prog);
			prog = 0;
		}
		locations.clear();
	}

	GLint GLProgram::Uniform(const char* name)
	{
		if (!prog)
			return -1;

		auto it = locations.find(name);
		if (it != locations.end())
			return it->second;

		GLint loc = glGetUniformLocation(prog, name);
		locations[name] = loc;
		return loc;
	}

	// -------------------------------------------------------------------------------------------

	GFXCore::GFXCore(Flipper::Flipper* flipper, HWConfig* config)
	{
#if GFX_USE_SDL_WINDOW
		render_window = (SDL_Window*)config->renderTarget;
#else
		hwndMain = (HWND)config->renderTarget;
#endif

		bool res = GL_LazyOpenSubsystem();
		assert(res);

		// reset pipeline
		frame_done = true;

		vertex_data = new Vertex[GFX_MAX_VERTICES];
		memset(vertex_data, 0, sizeof(Vertex) * GFX_MAX_VERTICES);
		index_data = new uint32_t[GFX_MAX_INDICES];

		// Frame dump
		const char* dumpVar = getenv("GFX_DUMP");
		if (dumpVar != nullptr && dumpVar[0] != 0)
		{
			dump_enabled = true;
			dump_path = dumpVar;
			const char* everyVar = getenv("GFX_DUMP_EVERY");
			if (everyVar != nullptr && everyVar[0] != 0)
			{
				dump_every = atoi(everyVar);
				if (dump_every < 1)
					dump_every = 1;
			}
		}

		xf = new TransformUnit(config, this);
		su = new SetupUnit(config, this);
		ras = new Rasterizer(config, this);			// TODO: For now, only single instance; will be developed for software rendering.
		pe = new PixelEngine(flipper, config, this);
		bump = new BumpMappingUnit(config, this);
		tx = new TextureEngine(config, this);
		tev = new TextureEnvironmentUnit(config, this);

		// The GX debug commands (issue #87). The node is registered from here so that the commands
		// exist exactly as long as the GFX subsystem does.
		gfx_jdi_instance = this;
		JDI::Hub.AddNode(L"GFX_JDI_JSON", GfxJdi, gfx_init_handlers);
	}

	GFXCore::~GFXCore()
	{
		JDI::Hub.RemoveNode(L"GFX_JDI_JSON");
		gfx_jdi_instance = nullptr;

		GL_CloseSubsystem();

		delete xf;
		delete su;
		delete ras;
		delete pe;
		delete bump;
		delete tx;
		delete tev;

		delete[] vertex_data;
		vertex_data = nullptr;
		delete[] index_data;
		index_data = nullptr;
	}

	bool GFXCore::GL_LazyOpenSubsystem()
	{
		return true;
	}

#ifdef _WINDOWS
	static int GL_SetPixelFormat(HDC hdc)
	{
		static PIXELFORMATDESCRIPTOR pfd = {
			sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
			PFD_TYPE_RGBA,
			24,
			0, 0, 0, 0, 0, 0,
			0, 0,
			0, 0, 0, 0, 0,
			24,
			0,
			0,
			PFD_MAIN_PLANE,
			0,
			0, 0, 0
		};

		int pixFmt;

		if ((pixFmt = ChoosePixelFormat(hdc, &pfd)) == 0) return 0;
		if (SetPixelFormat(hdc, pixFmt, &pfd) == FALSE) return 0;
		DescribePixelFormat(hdc, pixFmt, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

		if (pfd.dwFlags & PFD_NEED_PALETTE) return 0;

		return 1;
	}
#endif

	bool GFXCore::GL_OpenSubsystem()
	{
		if (backend_started)
			return true;

#if GFX_USE_SDL_WINDOW
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

		context = SDL_GL_CreateContext(render_window);
		if (context == nullptr)
		{
			Report(Channel::GP, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
			return false;
		}
#else
		hdcgl = GetDC(hwndMain);

		if (hdcgl == NULL) return false;

		if (GL_SetPixelFormat(hdcgl) == 0)
		{
			ReleaseDC(hwndMain, hdcgl);
			return false;
		}

		hglrc = wglCreateContext(hdcgl);
		if (hglrc == NULL)
		{
			ReleaseDC(hwndMain, hdcgl);
			return false;
		}

		wglMakeCurrent(hdcgl, hglrc);
#endif

		glewExperimental = GL_TRUE;
		GLenum err = glewInit();
		if (GLEW_OK != err)
		{
			Report(Channel::GP, "Error: %s\n", glewGetErrorString(err));
			return false;
		}

		Report(Channel::GP, "OpenGL version: %s\n", (const char*)glGetString(GL_VERSION));
		Report(Channel::GP, "OpenGL renderer: %s\n", (const char*)glGetString(GL_RENDERER));
		Report(Channel::GP, "GLSL version: %s\n", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

		if (!xf->CreateShader())
		{
			Report(Channel::GP, "Cannot create the XF vertex shader\n");
			return false;
		}

		InitGeometryBuffers();

		// Texture objects can only be created once a context is current
		tx->TexInit();

		ApplyDefaultGLState();

		// clear frame counter
		pe->frames = 0;

		if (ras->ras_wireframe) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}

		backend_started = true;
		return true;
	}

	// The GL state that the GFX register loads are applied on top of. It is also what
	// ResetPipelineState() restores, so that a reset pipeline does not inherit the GL state of the
	// scene that was rendered before it.
	void GFXCore::ApplyDefaultGLState()
	{
		glScissor(0, 0, scr_w, scr_h);
		glViewport(0, 0, scr_w, scr_h);

		glFrontFace(GL_CW);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_SCISSOR_TEST);

		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ZERO);
		glDisable(GL_COLOR_LOGIC_OP);
		glLogicOp(GL_COPY);
		glDisable(GL_CULL_FACE);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthRange(0.0, 1.0);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDisable(GL_ALPHA_TEST);
	}

	// Put every pipeline block back into its reset state.
	void GFXCore::ResetPipelineState()
	{
		genmode.bits = 0;
		for (int i = 0; i < 4; i++)
		{
			msloc[i].bits = 0;
		}

		frame_dirty = false;

		xf->Reset();
		su->Reset();
		ras->Reset();
		bump->Reset();
		tx->Reset();
		tev->Reset();
		pe->Reset();

		if (backend_started)
		{
			ApplyDefaultGLState();

			if (ras->ras_wireframe)
			{
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			}
		}
	}

	void GFXCore::GL_CloseSubsystem()
	{
		if (!backend_started)
			return;

		xf->DisposeShader();
		tev->DisposePrograms();
		tx->TexFree();
		DisposeGeometryBuffers();

		//if(frameReady) GL_EndFrame();

#if GFX_USE_SDL_WINDOW
		SDL_GL_DeleteContext(context);
		context = nullptr;
#else
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(hglrc);
#endif

		backend_started = false;
	}

	void GFXCore::InitGeometryBuffers()
	{
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(Vertex) * GFX_MAX_VERTICES, vertex_data, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &ibo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)sizeof(uint32_t) * GFX_MAX_INDICES, index_data, GL_DYNAMIC_DRAW);

		GLsizei stride = sizeof(Vertex);

		glEnableVertexAttribArray(Flipper::VTX_POS);
		glVertexAttribPointer(Flipper::VTX_POS, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offsetof(Vertex, Position)));
		glEnableVertexAttribArray(Flipper::VTX_NRM);
		glVertexAttribPointer(Flipper::VTX_NRM, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offsetof(Vertex, Normal)));
		glEnableVertexAttribArray(Flipper::VTX_BINRM);
		glVertexAttribPointer(Flipper::VTX_BINRM, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offsetof(Vertex, Binormal)));
		glEnableVertexAttribArray(Flipper::VTX_TANGENT);
		glVertexAttribPointer(Flipper::VTX_TANGENT, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offsetof(Vertex, Tangent)));

		// Colours are kept in GFX::Color, whose bytes are laid out as (A, B, G, R);
		// the vertex shader puts them back into (R, G, B, A) order.
		glEnableVertexAttribArray(Flipper::VTX_COLOR0);
		glVertexAttribPointer(Flipper::VTX_COLOR0, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (GLvoid*)(offsetof(Vertex, Col[0])));
		glEnableVertexAttribArray(Flipper::VTX_COLOR1);
		glVertexAttribPointer(Flipper::VTX_COLOR1, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (GLvoid*)(offsetof(Vertex, Col[1])));

		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD0);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offsetof(Vertex, TexCoord[0])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD1);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offsetof(Vertex, TexCoord[1])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD2);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD2, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offsetof(Vertex, TexCoord[2])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD3);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD3, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offsetof(Vertex, TexCoord[3])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD4);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD4, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offsetof(Vertex, TexCoord[4])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD5);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD5, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offsetof(Vertex, TexCoord[5])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD6);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD6, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offsetof(Vertex, TexCoord[6])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD7);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD7, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offsetof(Vertex, TexCoord[7])));

		glEnableVertexAttribArray(Flipper::VTX_MATIDX0);
		glVertexAttribIPointer(Flipper::VTX_MATIDX0, 1, GL_UNSIGNED_INT, stride, (GLvoid*)(offsetof(Vertex, matIdx0)));
		glEnableVertexAttribArray(Flipper::VTX_MATIDX1);
		glVertexAttribIPointer(Flipper::VTX_MATIDX1, 1, GL_UNSIGNED_INT, stride, (GLvoid*)(offsetof(Vertex, matIdx1)));

		glBindVertexArray(0);
	}

	void GFXCore::DisposeGeometryBuffers()
	{
		if (vao)
		{
			glDeleteVertexArrays(1, &vao);
			vao = 0;
		}
		if (vbo)
		{
			glDeleteBuffers(1, &vbo);
			vbo = 0;
		}
		if (ibo)
		{
			glDeleteBuffers(1, &ibo);
			ibo = 0;
		}
	}

	// init rendering (call before drawing FIFO primitives)
	void GFXCore::GL_BeginFrame()
	{
		if (frameReady) return;

		glDrawBuffer(GL_BACK);

		if (pe->TakePendingCopyClear())
		{
			// A copy command of the previous frame asked for the EFB to be cleared. It is the copy
			// engine's clear, so it honours the PE_COPY_CMD bounds and restores the PE state itself.
			pe->ApplyCopyClear();
		}
		else
		{
			// The frame clear is the copy engine's clear, not a draw call: it must not be affected by
			// the blending, logic op, write mask or depth state the previous scene left behind.
			glDisable(GL_BLEND);
			glDisable(GL_COLOR_LOGIC_OP);
			glDisable(GL_DEPTH_TEST);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDepthMask(GL_TRUE);

			glClearColor(
				(float)(pe->pe.copy_clear_ar.red / 255.0f),
				(float)(pe->pe.copy_clear_gb.green / 255.0f),
				(float)(pe->pe.copy_clear_gb.blue / 255.0f),
				(float)(pe->pe.copy_clear_ar.alpha / 255.0f)
			);

			glClearDepth((double)(pe->pe.copy_clear_z.value / 16777215.0));

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}

		// ... and the state the registers describe is put back, so that the scene draws with it.
		pe->ApplyZMode();
		pe->ApplyColorMode();

		frameReady = true;
	}

	// done rendering (call when frame is ready)
	void GFXCore::GL_EndFrame()
	{
		if (!frameReady) return;

		glFlush();

		if (dump_enabled)
			DumpFrame();

		glFinish();

#if GFX_USE_SDL_WINDOW
		SDL_GL_SwapWindow(render_window);
#else
		SwapBuffers(hdcgl);
#endif

		frameReady = false;
		pe->frames++;
		gfx_frame_counter++;
		Flipper::HW->cp->ResetFrameStats();
	}

	void GFXCore::DumpFrame()
	{
		if ((gfx_frame_counter % dump_every) != 0)
			return;

		uint32_t w = scr_w, h = scr_h;

		std::vector<uint8_t> pixels((size_t)w * h * 3);
		glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

		// glReadPixels returns RGB triplets, while a 24-bit BMP stores them as BGR.
		for (size_t i = 0; i < (size_t)w * h; i++)
		{
			std::swap(pixels[i * 3 + 0], pixels[i * 3 + 2]);
		}

		// BMP is bottom-up, exactly like the GL framebuffer, so no flip is needed
		uint8_t hdr[54] = { 0 };
		uint32_t dataSize = w * h * 3;
		uint32_t fileSize = 54 + dataSize;

		hdr[0] = 'B'; hdr[1] = 'M';
		memcpy(&hdr[2], &fileSize, 4);
		hdr[10] = 54;
		hdr[14] = 40;
		memcpy(&hdr[18], &w, 4);
		memcpy(&hdr[22], &h, 4);
		hdr[26] = 1;
		hdr[28] = 24;
		memcpy(&hdr[34], &dataSize, 4);

		char name[0x400];
		sprintf(name, "%s_%06d.bmp", dump_path.c_str(), gfx_frame_counter);

		FILE* f = fopen(name, "wb");
		if (f == nullptr)
			return;

		fwrite(hdr, 1, sizeof(hdr), f);
		fwrite(pixels.data(), 1, dataSize, f);
		fclose(f);

		Report(Channel::GP, "Frame dumped to %s\n", name);
	}

	void GFXCore::GPFrameBegin()
	{
		if (frame_done)
		{
			GL_OpenSubsystem();
			GL_BeginFrame();
			frame_done = 0;
		}
	}

	// rendering complete, swap buffers, sync to vretrace
	void GFXCore::GPFrameDone()
	{
		// PE_FINISH / PE_TOKEN (GXDrawDone and friends) are the frame boundary most titles use: the
		// picture is complete by then. A frame that a full-frame display copy has already presented
		// (the movie players) holds nothing new, so it is not swapped a second time.
		if (frame_dirty)
		{
			GL_EndFrame();
			frame_dirty = false;
		}

		frame_done = true;
	}

	// A full-frame display copy (PE_COPY_CMD.opcode = display) makes the finished EFB the XFB that
	// the video interface scans out (gfx-pe.md 5.6), so it is the moment the frame becomes visible:
	// the copy engine hands the picture over to the display. The backend displays the EFB itself
	// instead of a real XFB, which is why the swap happens here.
	//
	// This is what drives the titles whose movie player draws a frame, copies it to the XFB and
	// waits for the retrace without ever calling GXDrawDone: without the swap their frames would
	// stay on an unpresented back buffer, which is what kept the Metroid Prime FMV black (#349).
	// See the PE copy command for why only the full-frame copies present.
	void GFXCore::GPDisplayCopy()
	{
		if (frame_dirty)
		{
			GL_EndFrame();
			frame_dirty = false;
		}

		frame_done = true;
	}

	void GFXCore::ResizeRenderTarget(size_t width, size_t height)
	{
		if (backend_started) {
			scr_w = (uint32_t)width;
			scr_h = (uint32_t)height;
			glViewport(0, 0, scr_w, scr_h);

			// The scissor box of the setup unit is expressed in screen coordinates, so it has to be
			// recomputed against the new target height (see SetupUnit::ResizeScissor).
			su->ResizeScissor((int)scr_w, (int)scr_h);
		}
	}
}
