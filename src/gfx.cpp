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
	}

	GFXCore::~GFXCore()
	{
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

		//
		// change some GL drawing rules
		//

		glScissor(0, 0, scr_w, scr_h);
		glViewport(0, 0, scr_w, scr_h);

		glFrontFace(GL_CW);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_SCISSOR_TEST);

		// clear frame counter
		pe->frames = 0;

		if (ras->ras_wireframe) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}

		backend_started = true;
		return true;
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

		glClearColor(
			(float)(pe->pe.copy_clear_ar.red / 255.0f),
			(float)(pe->pe.copy_clear_gb.green / 255.0f),
			(float)(pe->pe.copy_clear_gb.blue / 255.0f),
			(float)(pe->pe.copy_clear_ar.alpha / 255.0f)
		);

		glClearDepth((double)(pe->pe.copy_clear_z.value / 16777215.0));

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
		GL_EndFrame();
		frame_done = true;
	}

	void GFXCore::ResizeRenderTarget(size_t width, size_t height)
	{
		if (backend_started) {
			scr_w = (uint32_t)width;
			scr_h = (uint32_t)height;
			glViewport(0, 0, scr_w, scr_h);
		}
	}
}
