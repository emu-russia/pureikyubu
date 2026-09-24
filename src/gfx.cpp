#include "pch.h"

// There are still some parts of old sources with attempts to "abstract" the backend. It is absolutely hopeless, just use core OpenGL and don't worry about it.
//
// The backend is a modern OpenGL 3.3 (GLSL 330) shader pipeline:
// - the XF (Transform Unit) is emulated by a vertex shader (see xf.cpp);
// - the TEV (Texture Environment Unit) is emulated by a fragment shader (see tev.cpp).
//
// This module owns the GL context, the frame loop and the geometry buffers; the shaders themselves
// live with the pipeline blocks they emulate.
//
// A second, completely software rendering path of the same blocks lives next to the shader one
// (issue #384). It is selected by the GFX_PIPELINE configuration variable and can be switched at
// run time; the picture then leaves through the XFB the copy engine writes and the video interface,
// so this pipeline never opens a GL context at all. See "Software pipeline" in wiki/gfx.md.

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

	GFXCore* gfx_jdi_instance = nullptr;

	static Json::Value* CmdGxState(std::vector<std::string>& args);
	static Json::Value* CmdGxFrames(std::vector<std::string>& args);
	static Json::Value* CmdGxRegs(std::vector<std::string>& args);
	static Json::Value* CmdGxShader(std::vector<std::string>& args);
	static Json::Value* CmdGxTex(std::vector<std::string>& args);
	static Json::Value* CmdGxShot(std::vector<std::string>& args);
	static Json::Value* CmdGxPixel(std::vector<std::string>& args);
	static Json::Value* CmdGxReset(std::vector<std::string>& args);
	static Json::Value* CmdGxPipeline(std::vector<std::string>& args);
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
		JDI::Hub.AddCmd("gxpipeline", CmdGxPipeline);
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
		obj->AddUtf8String((std::string(name) + "Hex").c_str(), hex);
	}

	// A GL context is only current on the thread that drives the frame loop; the copy commands below
	// must not be issued from anywhere else (the JDI server and the debugger UI can run on another
	// thread).
	static bool GLContextCurrent()
	{
#if defined(GFX_NULL) && !defined(GFX_OFFSCREEN)
		// Headless: there is no context, and every GL call is a no-op (see gfxnull.h), so the
		// read-back commands are always allowed.
		return true;
#elif defined(_WINDOWS)
		return wglGetCurrentContext() != 0;
#else
		return true;
#endif
	}

	static Json::Value* GLErrorValue(const wchar_t* text)
	{
		Json::Value* output = MakeObject();
		output->AddUtf8String("error", Util::WstringToString(text).c_str());
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
		common->AddInt("pipeline", gfx->Pipeline());
		common->AddUtf8String("pipelineName", gfx->SoftPipeline() ? "soft" : "shader");
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

			FILE* f = Util::FileOpen(Util::StringToWstring(name), "wb");
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
		output->AddUtf8String("vertexShader", vertName.c_str());
		output->AddInt("vertexShaderSize", (int)vertSize);
		output->AddUtf8String("fragmentShader", fragName.c_str());
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

	bool GFXCore::HasGLContext() const
	{
		return GLContextCurrent();
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

		if (!gfx->SoftPipeline() && !GLContextCurrent())
		{
			return GLErrorValue(L"gxshot: no OpenGL context on the calling thread "
				L"(call it from the emulator thread while a frame is being rendered)");
		}

		int x, y, width, height;
		if (!ParseRect(args, 2, gfx, &x, &y, &width, &height))
		{
			return GLErrorValue(L"gxshot: the rectangle is outside the render target");
		}

		std::vector<uint8_t> rgba;
		gfx->pe->ReadEfb(x, y, width, height, rgba);

		// The EFB read-back carries the alpha plane; a PNG of the picture keeps the three colours.
		std::vector<uint8_t> rgb((size_t)width * height * 3);
		for (size_t i = 0; i < rgb.size() / 3; i++)
		{
			rgb[i * 3 + 0] = rgba[i * 4 + 0];
			rgb[i * 3 + 1] = rgba[i * 4 + 1];
			rgb[i * 3 + 2] = rgba[i * 4 + 2];
		}

		std::string filename = args[1];
		bool saved = Util::SavePng(filename.c_str(), rgb.data(), (size_t)width, (size_t)height);

		Json::Value* output = MakeObject();
		output->AddUtf8String("file", filename.c_str());
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

		if (!gfx->SoftPipeline() && !GLContextCurrent())
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

		if (gfx->SoftPipeline())
		{
			// The software EFB is a plain array, so the pixel can be read on any thread.
			uint32_t z24 = 0;
			if (!gfx->pe->SoftPixel(x, y, rgba, &z24))
			{
				return GLErrorValue(L"gxpixel: the coordinates are outside the render target");
			}

			Json::Value* output = MakeObject();
			output->AddInt("x", x);
			output->AddInt("y", y);
			output->AddInt("r", rgba[0]);
			output->AddInt("g", rgba[1]);
			output->AddInt("b", rgba[2]);
			output->AddInt("a", rgba[3]);
			output->AddInt("z24", (int)z24);
			output->AddFloat("z", (float)z24 / 16777215.0f);
			return output;
		}

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

		if (!gfx->SoftPipeline() && !GLContextCurrent())
		{
			return GLErrorValue(L"gxtexdump: no OpenGL context on the calling thread");
		}

		Json::Value* output = MakeObject();
		output->AddInt("map", id);
		output->AddUtf8String("file", filename.c_str());

		// The image is decoded from main memory on demand, which is what the draw path does as well
		std::vector<uint8_t> rgb;
		int width = 0, height = 0;

		if (!gfx->tx->DumpTexture(id, rgb, &width, &height))
		{
			output->AddBool("saved", false);
			output->AddUtf8String("reason", "the texture map is not valid");
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
	// gxpipeline - read or switch the rendering pipeline at run time
	// -------------------------------------------------------------------------------------------

	static Json::Value* CmdGxPipeline(std::vector<std::string>& args)
	{
		GFXCore* gfx = Gfx();
		if (gfx == nullptr)
		{
			return GLErrorValue(L"the GFX subsystem is not running");
		}

		if (args.size() > 1)
		{
			int value;

			if (args[1] == "shader" || args[1] == "gl")
				value = GFX_PIPELINE_SHADER;
			else if (args[1] == "soft" || args[1] == "sw")
				value = GFX_PIPELINE_SOFT;
			else
				value = atoi(args[1].c_str());

			if (!gfx->SetPipeline(value))
			{
				return GLErrorValue(L"gxpipeline: the pipeline is 0 (shader) or 1 (soft)");
			}
		}

		Json::Value* output = MakeObject();
		output->AddInt("pipeline", gfx->Pipeline());
		output->AddUtf8String("name", gfx->SoftPipeline() ? "soft" : "shader");
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
#ifdef GFX_OFFSCREEN
		// The headless build is given no window by its front end, so the hidden one the context
		// hangs on is created here, before the subsystem asks for that context.
		if (hwndMain == nullptr)
		{
			CreateOffscreenWindow();
		}
#endif
#endif

		bool res = GL_LazyOpenSubsystem();
		assert(res);

		// reset pipeline
		frame_done = true;

		vertex_data = new Vertex[GFX_MAX_VERTICES];
		memset(vertex_data, 0, sizeof(Vertex) * GFX_MAX_VERTICES);
		index_data = new uint32_t[GFX_MAX_INDICES];

		{
			const char* efbVar = getenv("GFX_EFB_DUMP");
			if (efbVar != nullptr && efbVar[0] != 0)
			{
				efb_dump_enabled = true;
				efb_dump_path = efbVar;
				const char* everyVar = getenv("GFX_DUMP_EVERY");
				if (everyVar != nullptr && everyVar[0] != 0)
				{
					efb_dump_every = atoi(everyVar);
					if (efb_dump_every < 1)
						efb_dump_every = 1;
				}
			}
		}

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

		// The rendering pipeline (issue #384): the shader (OpenGL) backend or the software one.
		// The choice is a configuration variable so that it survives a restart, and the debugger
		// can switch it at run time (SetPipeline).
		pipeline = (config->gfxPipeline == GFX_PIPELINE_SOFT) ? GFX_PIPELINE_SOFT : GFX_PIPELINE_SHADER;

		xf = new TransformUnit(config, this);
		su = new SetupUnit(config, this);
		ras = new Rasterizer(config, this);
		pe = new PixelEngine(flipper, config, this);
		bump = new BumpMappingUnit(config, this);
		tx = new TextureEngine(config, this);
		tev = new TextureEnvironmentUnit(config, this);

		Report(Channel::GP, "GFX pipeline: %s\n",
			SoftPipeline() ? "software (experimental, issue #384)" : "shader (OpenGL)");

		// The GX debug commands (issue #87). The node is registered from here so that the commands
		// exist exactly as long as the GFX subsystem does.
		gfx_jdi_instance = this;
		JDI::Hub.AddNode(L"GFX_JDI_JSON", JdiSpecs::GfxJdi, gfx_init_handlers);
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

	bool GFXCore::SetPipeline(int value)
	{
		if (value != GFX_PIPELINE_SHADER && value != GFX_PIPELINE_SOFT)
			return false;

		if (value == pipeline)
			return true;

		pipeline = value;

		// The choice is a configuration variable (issue #384: "keep the current pipeline as a
		// configuration variable"), so the console picks it up again on the next start.
		SetConfigInt(USER_GFX_PIPELINE, pipeline, USER_HW);

		// Switching to the software pipeline drops the GL context: the picture is drawn into the
		// software EFB from now on and the video interface shows the XFB the copy engine writes.
		// Switching back restarts the shader backend, which the next frame does on its own.
		if (SoftPipeline())
		{
			if (backend_started)
				GL_CloseSubsystem();

			frameReady = false;
			frame_dirty = false;
			frame_done = true;

			pe->SoftBeginFrame();
		}
		else
		{
			frameReady = false;
			frame_dirty = false;
			frame_done = true;
		}

		Report(Channel::GP, "GFX pipeline switched to %s\n",
			SoftPipeline() ? "software (experimental, issue #384)" : "shader (OpenGL)");
		return true;
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

#if defined(GFX_NULL) && !defined(GFX_OFFSCREEN)
		// Headless: there is no window to draw into and no driver to ask for a context. The null
		// backend (gfxnull.h) accepts every GL call, so the pipeline is simply marked as running;
		// the shaders, the geometry buffers and the textures are "created" as no-op handles, and
		// the drawing goes nowhere.
		Report(Channel::GP, "GFX: the null (headless) backend is running, nothing is presented\n");
		backend_started = true;
		return true;
#else

#if GFX_USE_SDL_WINDOW
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		// The EFB has an alpha plane and the copy engine can read it back (the a8 texture copy of
		// the cartoon-outline demo does), so the render target needs an alpha channel of its own.
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

		context = SDL_GL_CreateContext(render_window);
		if (context == nullptr)
		{
			Report(Channel::GP, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
			return false;
		}
#elif defined(GFX_OFFSCREEN)
		// Headless with a real driver: the context is created on a window that is never shown, the
		// frame is drawn into the framebuffer of the EFB, and nothing appears on the screen.
		if (hwndMain == nullptr)
		{
			Report(Channel::GP, "GFX: no offscreen window to create the context on\n");
			return false;
		}

		hdcgl = GetDC(hwndMain);
		if (hdcgl == NULL) return false;

		if (GL_SetPixelFormat(hdcgl) == 0)
		{
			Report(Channel::GP, "GFX: no suitable pixel format for the offscreen context\n");
			ReleaseDC(hwndMain, hdcgl);
			return false;
		}

		hglrc = wglCreateContext(hdcgl);
		if (hglrc == NULL)
		{
			Report(Channel::GP, "GFX: wglCreateContext failed for the offscreen context\n");
			ReleaseDC(hwndMain, hdcgl);
			return false;
		}

		if (wglMakeCurrent(hdcgl, hglrc) == FALSE)
		{
			Report(Channel::GP, "GFX: wglMakeCurrent failed for the offscreen context\n");
			wglDeleteContext(hglrc);
			ReleaseDC(hwndMain, hdcgl);
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

		// The EFB, the buffer the pipeline draws into (see the framebuffer note in gfx.h).
		if (!CreateEfbTarget())
		{
			return false;
		}

		// The buffer the copy engine's display copies write into (see the XFB note in gfx.h).
		if (!CreateXfbTarget())
		{
			return false;
		}

		// The frame the emulator starts on is a cleared one: the EFB is only cleared when a frame
		// draws (see CommandProcessor::DrawPrimitive), so a title whose first command is a display
		// copy would otherwise read whatever the driver left in the target.
		{
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClearDepth(1.0);

			glBindFramebuffer(GL_FRAMEBUFFER, DrawFbo());
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glBindFramebuffer(GL_FRAMEBUFFER, xfbFbo);
			glClear(GL_COLOR_BUFFER_BIT);

			glBindFramebuffer(GL_FRAMEBUFFER, DrawFbo());
		}

		ApplyDefaultGLState();

		// clear frame counter
		pe->frames = 0;

		if (ras->ras_wireframe) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}

		backend_started = true;
		return true;
#endif // GFX_NULL
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

		// The software EFB is a buffer of its own; a reset starts it empty.
		if (SoftPipeline())
			pe->SoftBeginFrame();
	}

	// -------------------------------------------------------------------------------------------
	// Save states
	//
	// The GFX engine is one section of a save state, and what the blocks below it own (the PE, the
	// XF, the SU, the rasterizer, the TEV, the texture unit and the bump unit) are sections of their
	// own. This one is the state of the engine itself: the shared GenMode registers, the flags that
	// describe where the frame loop stands and the size of the render target.
	//
	// What does *not* travel is everything the host owns. The framebuffer and buffer objects
	// (efbFbo/efbColor/efbDepth/xfbFbo/xfbColor/vao/vbo/ibo) are OpenGL objects of the process that
	// is running, not of the emulated console: they are recreated by CreateEfbTarget,
	// CreateXfbTarget and InitGeometryBuffers, and a state that named them would be naming an id of
	// another context. The window, the GL context and the device context (render_window/context and
	// the Win32 hwndMain/hglrc/hdcgl) belong to the front end, and backend_started describes the
	// lifecycle of that backend rather than the machine. The dump configuration (dump_enabled and
	// friends) is a debugger setting, and vertex_data/index_data are the scratch of the draw that
	// is in flight: a state is taken between FIFO commands, where the vertex stream is always empty,
	// so there is nothing in them to keep.
	// -------------------------------------------------------------------------------------------

	void GFXCore::SaveState(SaveStates::StateWriter& writer) const
	{
		// The shared registers (gfx-su.md 4.1): GEN_MODE and the four quad/sample locations. The
		// whole 32-bit register word goes out, not the decoded fields: the decoder is this build's,
		// the bits are the machine's.
		writer.Fields(genmode.bits);
		for (int i = 0; i < 4; i++)
		{
			writer.Fields(msloc[i].bits);
		}

		// Where the frame loop stands. These flags are the machine's, not the host's: frame_done
		// and frameReady say whether the frame that is being built is open, frame_clear_pending says
		// that its first primitive still owes the EFB a clear (see GPFrameDrawn), and frame_dirty
		// says that the frame holds a picture the display has not been handed yet.
		writer.Fields(frame_done, frameReady, frame_clear_pending, frame_dirty);

		// The display copy of the shader backend: whether the XFB holds the picture of the frame,
		// and the base the display copies of the frame that is being drawn started at.
		//
		// `xfb_frame` is not here, and it is the one member of this block that is deliberately left
		// out of the state: it names a frame of the counter the frame loop keeps
		// (`gfx_frame_counter`), which is a host value that does not travel - the front end has been
		// counting frames all along. A restored number could only be stale, and a display copy that
		// believed its base already belonged to the current frame would skip the update. A load
		// re-arms the member instead (see LoadState), which is what makes the next copy latch it.
		writer.Fields(xfb_pending, xfb_base);

		// The size of the render target - the EFB the pipeline draws into - and the rendering
		// pipeline in use (GFX_PIPELINE_SHADER or GFX_PIPELINE_SOFT). Both are part of the state of
		// the machine: a state taken in the software pipeline resumes in the software pipeline, and
		// a state whose render target is 640x480 must not be resumed into a 320x240 one.
		writer.Fields(scr_w, scr_h, pipeline);
	}

	void GFXCore::LoadState(SaveStates::StateReader& reader)
	{
		reader.Fields(genmode.bits);
		for (int i = 0; i < 4; i++)
		{
			reader.Fields(msloc[i].bits);
		}

		reader.Fields(frame_done, frameReady, frame_clear_pending, frame_dirty);
		reader.Fields(xfb_pending, xfb_base);

		// The size of the render target. It is read into the live members, and what the emulator was
		// running with is kept first: the section has to be consumed in exactly the order it was
		// written and completely, and the resize that a different size asks for happens only once
		// everything has been read.
		uint32_t width = scr_w;
		uint32_t height = scr_h;

		reader.Fields(scr_w, scr_h);

		// The pipeline in use (GFX_PIPELINE_SHADER or GFX_PIPELINE_SOFT). It is set directly rather
		// than through SetPipeline: that entry point also writes the configuration variable, and a
		// load is not the front end asking for a change of pipeline. It is put back before the
		// render target is resized below, because the resize takes a different path in each
		// pipeline (the software one has no framebuffer to reallocate).
		reader.Fields(pipeline);

		if (reader.Failed())
			return;

		// `xfb_frame` names a frame of the counter the frame loop keeps (`gfx_frame_counter`), which
		// is why it is not in the state at all (see SaveState). A load re-arms it instead, the way
		// DestroyXfbTarget does: -1 makes the next display copy latch the base rather than measure
		// its rectangle against the base of a frame that is long over.
		xfb_frame = -1;

		// The render target is resized only when the state really describes a different one:
		// ResizeRenderTarget reallocates the XFB, re-programs the viewport and recomputes the
		// scissor box, which is wasted work when the size is the one the emulator is already
		// running with.
		if (width != scr_w || height != scr_h)
		{
			ResizeRenderTarget(width, height);
		}
	}

	// -------------------------------------------------------------------------------------------
	// The host state that follows a load
	//
	// LoadState puts the register state of every block back, but several parts of the pipeline
	// cache what the registers said at the moment they were written: the GL viewport and the GL
	// scissor box are computed from registers and stored in the context, the texture maps keep a
	// decoded image that was keyed on the registers of the draw that produced it, and the TEV
	// fragment program is a generated shader that is linked from them. None of those are in a
	// state (they are the host's copies of it), so they are all rebuilt here from what the load
	// restored. The orchestrator calls this once, after every section of the GFX pipeline has been
	// applied and before the machine runs again.
	//
	// The one thing this deliberately does *not* do is clear the software EFB. `frame_clear_pending`
	// is the machine's own answer to "does the frame that is open still owe the EFB a clear": when
	// the state says false, the frame that was saved has already been cleared and drawn into - and
	// that picture is in the state (the PE section carries the whole software EFB array), so
	// calling SoftBeginFrame here would wipe exactly what was restored. When it says true the clear
	// happens on its own, at the first primitive of the frame, the way it does in a live run (see
	// GPFrameDrawn).
	// -------------------------------------------------------------------------------------------

	void GFXCore::RefreshAfterLoad()
	{
		// The viewport and the scissor box are recomputed from the restored registers. Both are
		// no-ops for a machine that never programmed one of them: the viewport keeps the default
		// the backend established, and the scissor becomes the whole render target again.
		if (xf != nullptr)
		{
			xf->RefreshViewport();
		}

		if (su != nullptr)
		{
			su->RefreshScissor();
		}

		// The depth state (PE_ZMODE, GEN_MODE.zfreeze) and the blend/logic/mask state (PE_CMODE0,
		// PE_CMODE1) live in the context, not in the registers the load restored. Both calls are
		// no-ops for the software pipeline, which applies the registers per pixel.
		if (pe != nullptr)
		{
			pe->ApplyZMode();
			pe->ApplyColorMode();
		}

		// A decoded texture map is keyed on the registers, the palette generation and the texture
		// bytes of the draw that produced it, so every map is stale after a load. They are marked
		// dirty rather than decoded here: the decode and the upload need a GL context and would
		// repeat the work of the first draw, which rebuilds them anyway (Rasterizer::SetUpPipeline
		// calls UpdateAndBindTextures). A map that the texture unit's own LoadState already marked
		// is marked again here, which costs nothing and keeps this method complete on its own.
		if (tx != nullptr)
		{
			for (int i = 0; i < 8; i++)
			{
				// The map keeps the texture object it was given by TexInit; what is invalid is the
				// decoded image it holds, so `valid` is cleared and `dirty` set: the next
				// UpdateAndBindTextures decodes the map from the restored registers and main memory.
				tx->texMap[i].valid = false;
				tx->texMap[i].dirty = true;
			}
		}

		// The TEV fragment program is generated from the TEV registers and the GEN_MODE shading
		// bits, so a program the last machine linked describes the wrong registers. GetTevProgram
		// links the variant the restored registers ask for and caches it, which is what the first
		// draw would do anyway; it is skipped when no GL context is current on this thread (linking
		// a shader needs one, and the state may be loaded from the debugger's thread) and in the
		// software pipeline, which has no shader at all.
		if (tev != nullptr && backend_started && HasGLContext() && !SoftPipeline())
		{
			tev->GetTevProgram();
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
		DestroyXfbTarget();
		DestroyEfbTarget();

		// The overlay owns GL objects of its own; they belong to the context that is going away.
		OsdDispose();

		//if(frameReady) GL_EndFrame();

#if defined(GFX_NULL) && !defined(GFX_OFFSCREEN)
		// Headless: nothing was created, so there is no context to destroy.
#elif GFX_USE_SDL_WINDOW
		SDL_GL_DeleteContext(context);
		context = nullptr;
#else
#ifdef GFX_OFFSCREEN
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(hglrc);
		hglrc = 0;

		if (hdcgl != 0)
		{
			ReleaseDC(hwndMain, hdcgl);
			hdcgl = 0;
		}

		if (hwndMain != nullptr)
		{
			DestroyWindow(hwndMain);
			hwndMain = nullptr;
		}
#else
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(hglrc);
#endif
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
	void GFXCore::ClearFrameBuffer()
	{
		// The clear covers the whole EFB: the scissor rectangle the title programmed clips its
		// primitives, not the clear the frame begins on, and a clear that ran later in the frame
		// (see GPFrameDrawn) would otherwise leave everything the scissor excludes untouched.
		GLint scissor[4];
		glGetIntegerv(GL_SCISSOR_BOX, scissor);
		glDisable(GL_SCISSOR_TEST);

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

		// The depth of this clear is the far plane, not the copy engine's clear value. The copy
		// engine's clear value is the one a display copy applies to the rectangle it read, with the
		// Z the title programmed (GX_MAX_Z24 in every SDK title). Clearing the depth with the reset
		// value of PE_COPY_CLEAR_Z (0) would leave the buffer at the near plane, so every fragment
		// of the first frame of a title would fail the compare and the frame would be lost: the
		// light map of the indirect bump demos is rendered in exactly that frame, which is why it
		// came out empty (issue #385).
		glClearDepth(1.0);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
		glEnable(GL_SCISSOR_TEST);

		// The clear bypassed the GL state the pixel engine registers describe, and it runs in the
		// middle of the frame now (the first primitive asks for it), so that state is put back
		// before the primitive that asked for the clear is drawn: the title programmed it for that
		// primitive, and drawing it with the clear's state (no depth test, no blend, both masks on)
		// renders it wrong.
		pe->ApplyZMode();
		pe->ApplyColorMode();
	}

	void GFXCore::GPFrameDrawn()
	{
		// The first primitive of the frame clears the EFB the frame started on (see GL_BeginFrame
		// and GL_DisplayCopy: a display copy that comes first is the picture of the frame before).
		//
		// The software pipeline reaches this the same way the shader one does (Rasterizer::RAS_End
		// for the shader path, SetupUnit::SoftEndPrimitive for the software one): the clear of its
		// own EFB array belongs here as well, and for the same reason. Clearing it where the frame
		// begins instead left the display copy of the frame that had just been drawn reading an EFB
		// that was already blank, and the console showed black (Zelda: The Wind Waker's title
		// screen in the software pipeline).
		if (frame_clear_pending)
		{
			if (SoftPipeline())
			{
				pe->SoftBeginFrame();
			}
			else
			{
				ClearFrameBuffer();
			}

			frame_clear_pending = false;
		}

		frame_dirty = true;
	}

	void GFXCore::GL_BeginFrame()
	{
		if (frameReady) return;

		// The EFB is cleared with the first primitive of the frame, not here (GPFrameDrawn): the
		// commands that come first are the copy engine's, and the display copy of the frame that
		// just ended has to read the EFB before the clear wipes it. The state the registers describe
		// is put back here all the same, so that the scene draws with it.
		frame_clear_pending = true;

		pe->ApplyZMode();
		pe->ApplyColorMode();

		frameReady = true;
	}

	// done rendering (call when frame is ready)
	void GFXCore::GL_EndFrame()
	{
		if (!frameReady) return;

		glFlush();

		// The picture of the frame is handed over before anything is read back from it: the dump
		// and the overlay both work on what the viewer sees (see PresentFrame).
		PresentFrame();

		if (dump_enabled)
			DumpFrame();

		// The profiler overlay goes on last: the frame is complete, the dump has been taken, and
		// the text is what the viewer sees on top of the picture (issue #394).
		OsdDraw();

		glFinish();

#if defined(GFX_NULL) && !defined(GFX_OFFSCREEN)
		// Headless: the frame was "drawn" into nowhere, so there is nothing to present.
#elif GFX_USE_SDL_WINDOW
		SDL_GL_SwapWindow(render_window);
#elif defined(GFX_OFFSCREEN)
		// The frame lives in the offscreen framebuffer; there is no window to swap it into. A
		// read-back (GFX_DUMP / GFX_EFB_DUMP / gxshot) is what takes it out of there.
#else
		SwapBuffers(hdcgl);
#endif

		// The picture has been handed over; the pipeline draws into the EFB again from here on
		// (PresentFrame left the window's back buffer bound, which is what the overlay painted on).
		glBindFramebuffer(GL_FRAMEBUFFER, DrawFbo());

		frameReady = false;
		pe->frames++;
		gfx_frame_counter++;


		Flipper::HW->cp->ResetFrameStats();
	}

	void GFXCore::DumpFrame()
	{
		if (SoftPipeline() || (gfx_frame_counter % dump_every) != 0)
			return;

		uint32_t w = scr_w, h = scr_h;

		// glReadPixels hands back RGB triplets with the rows running bottom-up; the PNG wants
		// them top-down, so the rows are copied out in reverse.
		std::vector<uint8_t> pixels((size_t)w * h * 3);
		glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

		std::vector<uint8_t> flipped((size_t)w * h * 3);
		for (uint32_t y = 0; y < h; y++)
		{
			memcpy(&flipped[(size_t)y * w * 3], &pixels[(size_t)(h - 1 - y) * w * 3], (size_t)w * 3);
		}

		char name[0x400];
		sprintf(name, "%s_%06d.png", dump_path.c_str(), gfx_frame_counter);

		if (Util::SavePng(name, flipped.data(), w, h))
		{
			Report(Channel::GP, "Frame dumped to %s\n", name);
		}
	}

	void GFXCore::GPFrameBegin()
	{
		if (SoftPipeline())
		{
			// The software pipeline has no frame buffer to open and no GL state to program, and its
			// EFB is cleared by the first primitive of the frame (GPFrameDrawn) for the same reason
			// the shader backend defers its clear: the copy engine's display copy of the frame that
			// just ended still has to read the EFB. This call only opens the frame.
			if (frame_done)
			{
				frame_clear_pending = true;
				frame_done = false;
			}
			return;
		}

		if (frame_done)
		{
			GL_OpenSubsystem();
			GL_BeginFrame();
			frame_done = 0;
		}
	}

	// rendering complete, swap buffers, sync to vretrace
	void GFXCore::DumpRenderTarget()
	{
		if ((gfx_frame_counter % efb_dump_every) != 0)
		{
			return;
		}

		std::vector<uint8_t> rgba;
		if (!pe->ReadEfb(0, 0, (int)scr_w, (int)scr_h, rgba))
		{
			return;
		}

		std::vector<uint8_t> rgb((size_t)scr_w * scr_h * 3);
		for (size_t i = 0; i < rgb.size() / 3; i++)
		{
			rgb[i * 3 + 0] = rgba[i * 4 + 0];
			rgb[i * 3 + 1] = rgba[i * 4 + 1];
			rgb[i * 3 + 2] = rgba[i * 4 + 2];
		}

		char name[0x400];
		sprintf(name, "%s_%06d.png", efb_dump_path.c_str(), gfx_frame_counter);

		if (Util::SavePng(name, rgb.data(), scr_w, scr_h))
		{
			Report(Channel::GP, "Frame dumped to %s\n", name);
		}
	}

	void GFXCore::GPFrameDone()
	{
		if (SoftPipeline())
		{
			// Nothing to present: the display copy has already written the XFB and the video
			// interface scans it out (the console's picture does not depend on this call). The
			// frame counters are the same ones the shader backend keeps, so the debugger and the
			// frame dumps see a frame in both pipelines.
			if (efb_dump_enabled)
			{
				DumpRenderTarget();
			}

			pe->frames++;
			gfx_frame_counter++;

			if (Flipper::HW != nullptr && Flipper::HW->cp != nullptr)
			{
				Flipper::HW->cp->ResetFrameStats();
			}

			frame_done = true;
			return;
		}

		if (efb_dump_enabled)
		{
			DumpRenderTarget();
		}

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
	// the copy engine hands the picture over to the display.
	//
	// This is what drives the titles whose movie player draws a frame, copies it to the XFB and
	// waits for the retrace without ever calling GXDrawDone: without the swap their frames would
	// stay on an unpresented back buffer, which is what kept the Metroid Prime FMV black (#349).
	// See the PE copy command for why only the full-frame copies present.
	void GFXCore::GPDisplayCopy()
	{
		if (SoftPipeline())
		{
			// The display copy itself wrote the XFB in main memory (PixelEngine::SoftDisplayCopy);
			// the video interface shows it. There is no back buffer to swap.
			frame_done = true;
			return;
		}

		if (frame_dirty)
		{
			GL_EndFrame();
			frame_dirty = false;
		}

		frame_done = true;
	}

	// -------------------------------------------------------------------------------------------
	// The XFB the copy engine writes (see the note in gfx.h)
	// -------------------------------------------------------------------------------------------

	void GFXCore::QuadOrigin(int* x, int* y) const
	{
		*x = 2 * (int)pe->pe.quad_offset.x;
		*y = 2 * (int)pe->pe.quad_offset.y;
	}

	//! The colour buffer the display copies write into. It has the size of the render target: the
	//! XFB holds the same number of scan lines as the EFB window the titles copy out of.
	bool GFXCore::CreateXfbTarget()
	{
		glGenFramebuffers(1, &xfbFbo);
		glBindFramebuffer(GL_FRAMEBUFFER, xfbFbo);

		glGenTextures(1, &xfbColor);
		glBindTexture(GL_TEXTURE_2D, xfbColor);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)scr_w, (GLsizei)scr_h, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, xfbColor, 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			Report(Channel::GP, "GFX: the XFB framebuffer is incomplete (0x%04X)\n", status);
			DestroyXfbTarget();
			return false;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, DrawFbo());
		return true;
	}

	void GFXCore::DestroyXfbTarget()
	{
		xfb_base = 0;
		xfb_frame = -1;

		if (xfbColor != 0)
		{
			glDeleteTextures(1, &xfbColor);
			xfbColor = 0;
		}

		if (xfbFbo != 0)
		{
			glDeleteFramebuffers(1, &xfbFbo);
			xfbFbo = 0;
		}

		xfb_pending = false;
	}

	//! The display copy of the shader pipeline (gfx-pe.md 5.6). The rectangle of the EFB becomes the
	//! same rectangle of the XFB, at the destination line the copy registers name: the destination
	//! base is an address in main memory, and the video interface scans the frame from the base it
	//! was given (VI_TFBL), so the line is the distance between the two in strides.
	void GFXCore::GL_DisplayCopy(int srcX, int srcY, int w, int h, uint32_t dstAddr, int stride)
	{
		if (SoftPipeline() || !backend_started || xfbFbo == 0)
			return;

		if (w <= 0 || h <= 0 || stride <= 0)
			return;

		// The line the rectangle belongs on is the distance between its destination and the base of
		// the frame the display scans. The video interface is the authority on that base, but a
		// title programs the display of the frame it has *just* copied - the base of a double
		// buffered XFB is therefore a frame behind the copies that build the next picture - and the
		// copies of one frame can even name both buffers (the bootrom writes its splash into the
		// two of them while it initialises).
		int dstLine = -1;

		if (Flipper::HW != nullptr && Flipper::HW->vi != nullptr)
		{
			uint32_t vbase[2] = { Flipper::HW->vi->XfbBase(), Flipper::HW->vi->XfbBottomBase() };

			for (int i = 0; i < 2 && dstLine < 0; i++)
			{
				if (vbase[i] == 0 || dstAddr < vbase[i])
					continue;

				int64_t line = ((int64_t)dstAddr - (int64_t)vbase[i]) / stride;

				if (line + h <= (int64_t)scr_h)
				{
					dstLine = (int)line;
				}
			}
		}

		if (dstLine < 0)
		{
			// The frame the copies of this frame build: its first copy names the base of the buffer,
			// and the copies that follow are lines away from it.
			if (xfb_frame != gfx_frame_counter)
			{
				xfb_frame = gfx_frame_counter;
				xfb_base = dstAddr;
			}

			int64_t offset = (int64_t)dstAddr - (int64_t)xfb_base;

			if (offset < 0 || offset >= (int64_t)scr_h * stride)
			{
				// The rectangle goes to memory that is not part of the frame the display shows (the
				// bootrom's third copy writes two scan lines of a small buffer of its own): the
				// picture of the frame does not change, and putting it at the top of the XFB would
				// tear it.
				return;
			}

			int64_t line = offset / stride;

			if (line + h <= (int64_t)scr_h)
			{
				dstLine = (int)line;
			}
			else
			{
				// The rectangle runs past the last line of the frame: the copy starts another
				// buffer of the XFB (the two the bootrom writes its splash into), so it is the base
				// from here on.
				xfb_base = dstAddr;
				dstLine = 0;
			}
		}

		if (srcX < 0) { w += srcX; srcX = 0; }
		if (srcY < 0) { h += srcY; srcY = 0; }
		if (srcX + w > (int)scr_w) w = (int)scr_w - srcX;
		if (srcY + h > (int)scr_h) h = (int)scr_h - srcY;
		if (w <= 0 || h <= 0)
			return;

		// glBlitFramebuffer is subject to the scissor test, and the emulator keeps the title's
		// scissor box enabled while the copy runs: the box is saved and restored around the blit.
		GLint scissor[4];
		glGetIntegerv(GL_SCISSOR_BOX, scissor);
		glDisable(GL_SCISSOR_TEST);

		// The render target measures its rows from the bottom, so the EFB row of a rectangle is the
		// row `scr_h - y - h` of the GL box. Both rectangles are the same size, so the blit is a
		// straight move of the picture to the line the copy asked for.
		glBindFramebuffer(GL_READ_FRAMEBUFFER, DrawFbo());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, xfbFbo);

		glBlitFramebuffer(
			srcX, (GLint)scr_h - (srcY + h), srcX + w, (GLint)scr_h - srcY,
			srcX, (GLint)scr_h - (dstLine + h), srcX + w, (GLint)scr_h - dstLine,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_FRAMEBUFFER, DrawFbo());

		glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
		glEnable(GL_SCISSOR_TEST);

		xfb_pending = true;
	}

	//! Hand the picture of the frame over to the display. What the display scans out is the XFB the
	//! copy engine wrote; a title that never copies out (there is no XFB then) is shown through its
	//! EFB, which is what the backend did for every title before the XFB existed.
	//!
	//! The picture goes to the window's back buffer - in the headless build that is the back buffer
	//! of the hidden window, which is what a frame dump reads. The EFB is left alone: it is a
	//! framebuffer of its own and the title keeps drawing into it.
	void GFXCore::PresentFrame()
	{
		GLuint source = (xfb_pending && xfbFbo != 0) ? xfbFbo : DrawFbo();

		if (source == 0)
			return;

		GLint scissor[4];
		glGetIntegerv(GL_SCISSOR_BOX, scissor);
		glDisable(GL_SCISSOR_TEST);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, source);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		glBlitFramebuffer(0, 0, (GLint)scr_w, (GLint)scr_h, 0, 0, (GLint)scr_w, (GLint)scr_h,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);

		glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
		glEnable(GL_SCISSOR_TEST);


		// The frame dump and the overlay that follow work on the picture that was just presented.
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		xfb_pending = false;
	}

	void GFXCore::ResizeRenderTarget(size_t width, size_t height)
	{
		if (SoftPipeline())
		{
			scr_w = (uint32_t)width;
			scr_h = (uint32_t)height;

			su->ResizeScissor((int)scr_w, (int)scr_h);
			pe->SoftBeginFrame();
			return;
		}

		if (backend_started) {
			scr_w = (uint32_t)width;
			scr_h = (uint32_t)height;

#ifdef GFX_OFFSCREEN
			// The offscreen target has the size of the emulated render target, so a video mode
			// change reallocates it. The attachments are recreated here, while nothing is drawing.
			DestroyEfbTarget();
			CreateEfbTarget();
#endif

			// The XFB has the size of the render target as well.
			DestroyXfbTarget();
			CreateXfbTarget();

			glViewport(0, 0, scr_w, scr_h);

			// The scissor box of the setup unit is expressed in screen coordinates, so it has to be
			// recomputed against the new target height (see SetupUnit::ResizeScissor).
			su->ResizeScissor((int)scr_w, (int)scr_h);
		}
	}

#ifdef GFX_OFFSCREEN

	//! Create the hidden window the offscreen context hangs on. It is never shown: it exists so
	//! that WGL has an HDC to attach a context and a pixel format to, which is the only way to get
	//! real OpenGL on Windows without putting anything on the screen.
	void GFXCore::CreateOffscreenWindow()
	{
		WNDCLASSA wc = { 0 };
		wc.lpfnWndProc = DefWindowProcA;
		wc.hInstance = GetModuleHandleA(nullptr);
		wc.lpszClassName = "pureikyubu-offscreen";
		RegisterClassA(&wc);			// A second registration of the same class fails harmlessly.

		hwndMain = CreateWindowExA(0, wc.lpszClassName, "pureikyubu (offscreen)",
			WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, (int)scr_w, (int)scr_h,
			nullptr, nullptr, wc.hInstance, nullptr);
	}

#endif // GFX_OFFSCREEN

	//! The EFB: the framebuffer the pipeline draws into. The colour attachment is a texture, which
	//! is what the copy engine's reads (a texture copy and a read-back of the frame) look at, and
	//! it is separate from the buffer the picture is presented in (see the framebuffer note in
	//! gfx.h): a title keeps drawing into the EFB after it has copied it out.
	bool GFXCore::CreateEfbTarget()
	{
		glGenFramebuffers(1, &efbFbo);
		glBindFramebuffer(GL_FRAMEBUFFER, efbFbo);

		glGenTextures(1, &efbColor);
		glBindTexture(GL_TEXTURE_2D, efbColor);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)scr_w, (GLsizei)scr_h, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, efbColor, 0);

		glGenRenderbuffers(1, &efbDepth);
		glBindRenderbuffer(GL_RENDERBUFFER, efbDepth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, (GLsizei)scr_w, (GLsizei)scr_h);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, efbDepth);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			Report(Channel::GP, "GFX: the EFB framebuffer is incomplete (0x%04X)\n", status);
			DestroyEfbTarget();
			return false;
		}

		glViewport(0, 0, (GLsizei)scr_w, (GLsizei)scr_h);
		return true;
	}

	void GFXCore::DestroyEfbTarget()
	{
		if (efbDepth != 0)
		{
			glDeleteRenderbuffers(1, &efbDepth);
			efbDepth = 0;
		}

		if (efbColor != 0)
		{
			glDeleteTextures(1, &efbColor);
			efbColor = 0;
		}

		if (efbFbo != 0)
		{
			glDeleteFramebuffers(1, &efbFbo);
			efbFbo = 0;
		}
	}
}
