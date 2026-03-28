#include "pch.h"

// There are still some parts of old sources with attempts to "abstract" the backend. It is absolutely hopeless, just use core OpenGL and don't worry about it.

using namespace Debug;

namespace GFX
{
	GFXCore::GFXCore(HWConfig* config)
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

		xf = new TransformUnit(config, this);
		su = new SetupUnit(config, this);
		ras = new Rasterizer(config, this);			// TODO: For now, only single instance; will be developed for software rendering.
		pe = new PixelEngine(config, this);
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

	bool GFXCore::GL_LazyOpenSubsystem()
	{
		return true;
	}

	bool GFXCore::GL_OpenSubsystem()
	{
		if (backend_started)
			return true;

#if GFX_USE_SDL_WINDOW
		context = SDL_GL_CreateContext(render_window);
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

		Report(Channel::GP, "OpenGL version: %s\n", (char*)glGetString(GL_VERSION));

		//
		// change some GL drawing rules
		//

		glScissor(0, 0, scr_w, scr_h);
		glViewport(0, 0, scr_w, scr_h);

		glFrontFace(GL_CW);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_SCISSOR_TEST);

#if GFX_BLACKJACK_AND_SHADERS
		glewExperimental = GL_TRUE;
		GLenum err = glewInit();
		if (GLEW_OK != err)
		{
			/* Problem: glewInit failed, something is seriously wrong. */
			Report(Channel::GP, "Error: %s\n", glewGetErrorString(err));
			return false;
		}

		auto vert_shader_source = Util::FileLoad("Data/gfx.vert");
		vert_shader_source.push_back(0);
		auto frag_shader_source = Util::FileLoad("Data/gfx.frag");
		frag_shader_source.push_back(0);
		UploadShaders((const char *)vert_shader_source.data(), (const char*)frag_shader_source.data());

		InitVBO();
#endif

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

		//if(frameReady) GL_EndFrame();

#if GFX_BLACKJACK_AND_SHADERS
		DisposeShaders();
		DisposeVBO();
#endif

#if GFX_USE_SDL_WINDOW
		SDL_GL_DeleteContext(context);
#else
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(hglrc);
#endif

		backend_started = false;
	}

	// init rendering (call before drawing FIFO primitives)
	void GFXCore::GL_BeginFrame()
	{
		if (frameReady) return;

#if !GFX_USE_SDL_WINDOW
		BeginPaint(hwndMain, &psFrame);
#endif
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
		bool showPerf = false;
		if (!frameReady) return;

		glFlush();

		/*/
			if(glGetError() != GL_NO_ERROR)
			{
				MessageBox(
					hwndMain,
					"Error, during GL frame rendering.",
					"We have big problem here!",
					MB_OK | MB_TOPMOST
				);
				ExitProcess(0);
			}
		/*/

		// do snapshot
		if (make_shot)
		{
			make_shot = false;
			pe->GL_DoSnapshot(false, snap_file, NULL, snap_w, snap_h);
		}

		glFinish();

#if GFX_USE_SDL_WINDOW
		SDL_GL_SwapWindow(render_window);
#else
		SwapBuffers(hdcgl);
		EndPaint(hwndMain, &psFrame);
#endif

		frameReady = false;
		//Report(Channel::GP, "gfx frame: %d\n", frames);
		pe->frames++;
		Flipper::HW->cp->ResetFrameStats();
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

	void GFXCore::UploadShaders(const char* vert_source, const char* frag_source)
	{
		char infoLog[0x1000]{};
		int success;

		// Perform magic spells to compile the vertex shader program

		vert_shader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vert_shader, 1, &vert_source, nullptr);
		glCompileShader(vert_shader);

		glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(vert_shader, sizeof(infoLog), nullptr, infoLog);
			Report(Channel::GP, "VERTEX SHADER COMPILE ERROR: %s\n", infoLog);
			Halt("Halted.\n");
		};

		// Perform magic spells to compile the frag shader program

		frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(frag_shader, 1, &frag_source, nullptr);
		glCompileShader(frag_shader);

		glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(frag_shader, sizeof(infoLog), nullptr, infoLog);
			Report(Channel::GP, "FRAGMENT SHADER COMPILE ERROR: %s\n", infoLog);
			Halt("Halted.\n");
		};

		// Perform magic spells to "link" shader programs and further use them instead of a fixed pipeline

		shader_prog = glCreateProgram();
		glAttachShader(shader_prog, vert_shader);
		glAttachShader(shader_prog, frag_shader);
		
		BindShadersWithVBO();
		
		glLinkProgram(shader_prog);

		GLenum gl_error = glGetError();
		if (gl_error != GL_NO_ERROR) {
			Halt("GL glLinkProgram Error: %x\n", gl_error);
		}

		glGetProgramiv(shader_prog, GL_LINK_STATUS, &success);
		if (!success)
		{
			glGetProgramInfoLog(shader_prog, sizeof(infoLog), nullptr, infoLog);
			Report(Channel::GP, "SHADER LINK ERROR: %s\n", infoLog);
			Halt("Halted.\n");
		}

		glDeleteShader(vert_shader);
		glDeleteShader(frag_shader);

		glUseProgram(shader_prog);

		gl_error = glGetError();
		if (gl_error != GL_NO_ERROR) {
			Halt("GL UploadShaders Error: %x\n", gl_error);
		}

		Report(Channel::GP, "Shader program is uploaded to GPU\n");
	}

	void GFXCore::DisposeShaders()
	{
		glFinish();

		// TODO: For some reason, it's falling down.
		return;

		// TODO: Is that enough?
		glUseProgram(0);
		glDeleteProgram(shader_prog);
	}

	void GFXCore::BindShadersWithVBO()
	{
		GLenum gl_error;

		glBindAttribLocation(shader_prog, Flipper::VTX_POS, "in_Position");
		gl_error = glGetError();
		if (gl_error != GL_NO_ERROR) {
			Halt("GL glBindAttribLocation VTX_POS Error: %x\n", gl_error);
		}

		glBindAttribLocation(shader_prog, Flipper::VTX_NRM, "in_Normal");
		glBindAttribLocation(shader_prog, Flipper::VTX_BINRM, "in_Binormal");
		glBindAttribLocation(shader_prog, Flipper::VTX_TANGENT, "in_Tangent");

		glBindAttribLocation(shader_prog, Flipper::VTX_COLOR0, "in_Color0");
		glBindAttribLocation(shader_prog, Flipper::VTX_COLOR1, "in_Color1");
		gl_error = glGetError();
		if (gl_error != GL_NO_ERROR) {
			Halt("GL glBindAttribLocation VTX_COLOR1 Error: %x\n", gl_error);
		}

		glBindAttribLocation(shader_prog, Flipper::VTX_TEXCOORD0, "in_TexCoord0");
		glBindAttribLocation(shader_prog, Flipper::VTX_TEXCOORD1, "in_TexCoord1");
		glBindAttribLocation(shader_prog, Flipper::VTX_TEXCOORD2, "in_TexCoord2");
		glBindAttribLocation(shader_prog, Flipper::VTX_TEXCOORD3, "in_TexCoord3");
		glBindAttribLocation(shader_prog, Flipper::VTX_TEXCOORD4, "in_TexCoord4");
		glBindAttribLocation(shader_prog, Flipper::VTX_TEXCOORD5, "in_TexCoord5");
		glBindAttribLocation(shader_prog, Flipper::VTX_TEXCOORD6, "in_TexCoord6");
		glBindAttribLocation(shader_prog, Flipper::VTX_TEXCOORD7, "in_TexCoord7");
		gl_error = glGetError();
		if (gl_error != GL_NO_ERROR) {
			Halt("GL glBindAttribLocation VTX_TEXCOORD7 Error: %x\n", gl_error);
		}

		glBindAttribLocation(shader_prog, Flipper::VTX_MATIDX0, "MatrixIndex0");
		gl_error = glGetError();
		if (gl_error != GL_NO_ERROR) {
			Halt("GL glBindAttribLocation VTX_MATIDX0 Error: %x\n", gl_error);
		}
		glBindAttribLocation(shader_prog, Flipper::VTX_MATIDX1, "MatrixIndex1");
		gl_error = glGetError();
		if (gl_error != GL_NO_ERROR) {
			Halt("GL glBindAttribLocation VTX_MATIDX1 Error: %x\n", gl_error);
		}
	}

	void GFXCore::InitVBO()
	{
		GLenum gl_error;

		vertex_data = new Vertex[vbo_size];
		memset(vertex_data, 0, sizeof(Vertex) * vbo_size);

		vao = 0;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		vbo = 0;
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		// TODO: Map/Unmap VBO
		//glBufferData(GL_ARRAY_BUFFER, vbo_size * sizeof(Vertex), vertex_data, GL_STATIC_DRAW);

		gl_error = glGetError();
		if (gl_error != GL_NO_ERROR) {
			Halt("GL InitVBO after glBufferData Error: %x\n", gl_error);
		}

		GLsizei attr_stride = sizeof(Vertex);

		glEnableVertexAttribArray(Flipper::VTX_POS);
		glVertexAttribPointer(Flipper::VTX_POS, 3, GL_FLOAT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, Position)));
		gl_error = glGetError();
		if (gl_error != GL_NO_ERROR) {
			Halt("GL InitVBO after glVertexAttribPointer VTX_POS Error: %x\n", gl_error);
		}

		glEnableVertexAttribArray(Flipper::VTX_NRM);
		glVertexAttribPointer(Flipper::VTX_NRM, 3, GL_FLOAT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, Normal)));
		glEnableVertexAttribArray(Flipper::VTX_BINRM);
		glVertexAttribPointer(Flipper::VTX_BINRM, 3, GL_FLOAT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, Binormal)));
		glEnableVertexAttribArray(Flipper::VTX_TANGENT);
		glVertexAttribPointer(Flipper::VTX_TANGENT, 3, GL_FLOAT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, Tangent)));

		glEnableVertexAttribArray(Flipper::VTX_COLOR0);
		glVertexAttribPointer(Flipper::VTX_COLOR0, 4, GL_UNSIGNED_BYTE, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, Col[0])));
		glEnableVertexAttribArray(Flipper::VTX_COLOR1);
		glVertexAttribPointer(Flipper::VTX_COLOR1, 4, GL_UNSIGNED_BYTE, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, Col[1])));
		gl_error = glGetError();
		if (gl_error != GL_NO_ERROR) {
			Halt("GL InitVBO after glVertexAttribPointer VTX_COLOR1 Error: %x\n", gl_error);
		}

		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD0);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, TexCoord[0])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD1);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, TexCoord[1])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD2);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD2, 2, GL_FLOAT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, TexCoord[2])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD3);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD3, 2, GL_FLOAT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, TexCoord[3])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD4);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD4, 2, GL_FLOAT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, TexCoord[4])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD5);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD5, 2, GL_FLOAT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, TexCoord[5])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD6);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD6, 2, GL_FLOAT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, TexCoord[6])));
		glEnableVertexAttribArray(Flipper::VTX_TEXCOORD7);
		glVertexAttribPointer(Flipper::VTX_TEXCOORD7, 2, GL_FLOAT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, TexCoord[7])));
		gl_error = glGetError();
		if (gl_error != GL_NO_ERROR) {
			Halt("GL InitVBO after glVertexAttribPointer VTX_TEXCOORD7 Error: %x\n", gl_error);
		}

		glEnableVertexAttribArray(Flipper::VTX_MATIDX0);
		glVertexAttribPointer(Flipper::VTX_MATIDX0, 1, GL_UNSIGNED_INT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, matIdx0)));
		glEnableVertexAttribArray(Flipper::VTX_MATIDX1);
		glVertexAttribPointer(Flipper::VTX_MATIDX1, 1, GL_UNSIGNED_INT, GL_FALSE, attr_stride, (GLvoid*)(offsetof(Vertex, matIdx1)));
		gl_error = glGetError();
		if (gl_error != GL_NO_ERROR) {
			Halt("GL InitVBO after glVertexAttribPointer VTX_MATIDX1 Error: %x\n", gl_error);
		}
	}

	void GFXCore::DisposeVBO()
	{
		glFinish();

		// TODO: For some reason, it's falling down.
		return;

		glBindVertexArray(0);
		glDeleteVertexArrays(1, &vao);
		vao = 0;

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glDeleteBuffers(1, &vbo);
		vbo = 0;

		if (vertex_data != nullptr) {
			delete[] vertex_data;
			vertex_data = nullptr;
		}
	}
}