#include "pch.h"

using namespace Debug;

namespace GX
{
	GXCore::GXCore()
	{
		fifo = new FifoProcessor(this);
	}

	GXCore::~GXCore()
	{
		delete fifo;
	}

	void GXCore::Open(HWConfig* config)
	{
		if (gxOpened)
			return;

		tickPerFifo = 100;
		updateTbrValue = Core->GetTicks() + tickPerFifo;

		fifo->Reset();

		cp_thread = EMUCreateThread(CPThread, false, this, "CPThread");

#ifdef _WINDOWS
		hwndMain = (HWND)config->renderTarget;
#endif

		bool res = GL_LazyOpenSubsystem();
		assert(res);

		// reset pipeline
		frame_done = true;

		// flush texture cache
		TexInit();

		gxOpened = true;
	}

	void GXCore::Close()
	{
		if (!gxOpened)
			return;

		if (cp_thread)
		{
			EMUJoinThread(cp_thread);
			cp_thread = nullptr;
		}

		GL_CloseSubsystem();

		TexFree();

		gxOpened = false;
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

	bool GXCore::GL_LazyOpenSubsystem()
	{
		return true;
	}

	bool GXCore::GL_OpenSubsystem()
	{
		if (backend_started)
			return true;

#ifdef _WINDOWS
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

		//
		// change some GL drawing rules
		//

		glScissor(0, 0, scr_w, scr_h);
		glViewport(0, 0, scr_w, scr_h);

		glFrontFace(GL_CW);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_SCISSOR_TEST);

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

		// clear performance counters
		frames = tris = pts = lines = 0;

		backend_started = true;
		return true;
	}

	void GXCore::GL_CloseSubsystem()
	{
		if (!backend_started)
			return;

		//if(frameReady) GL_EndFrame();

		DisposeShaders();

		DisposeVBO();

#ifdef _WINDOWS
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(hglrc);
#endif

		backend_started = false;
	}

	// init rendering (call before drawing FIFO primitives)
	void GXCore::GL_BeginFrame()
	{
		if (frameReady) return;

#ifdef _WINDOWS
		BeginPaint(hwndMain, &psFrame);
#endif
		glDrawBuffer(GL_BACK);

		if (set_clear)
		{
			glClearColor(
				(float)(cr / 255.0f),
				(float)(cg / 255.0f),
				(float)(cb / 255.0f),
				(float)(ca / 255.0f)
			);

			glClearDepth((double)(clear_z / 16777215.0));

			set_clear = false;
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		frameReady = 1;
	}

	// done rendering (call when frame is ready)
	void GXCore::GL_EndFrame()
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
			GL_DoSnapshot(false, snap_file, NULL, snap_w, snap_h);
		}

		glFinish();
#ifdef _WINDOWS
		SwapBuffers(hdcgl);
		EndPaint(hwndMain, &psFrame);
#endif

		frameReady = 0;
		//Report(Channel::GP, "gfx frame: %d\n", frames);
		frames++;
		tris = pts = lines = 0;
		cpLoads = bpLoads = xfLoads = 0;
	}

	// rendering complete, swap buffers, sync to vretrace
	void GXCore::GPFrameDone()
	{
		GL_EndFrame();
		frame_done = true;
	}

	void GXCore::UploadShaders(const char* vert_source, const char* frag_source)
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

		Report(Channel::GP, "Shader program is uploaded to GPU\n");
	}

	void GXCore::DisposeShaders()
	{
		// TODO: Is that enough?
		glUseProgram(0);
		glDeleteProgram(shader_prog);
	}

	void GXCore::BindShadersWithVBO()
	{
		glBindAttribLocation(shader_prog, VTX_POSMATIDX, "in_PosMatIdx");

		glBindAttribLocation(shader_prog, VTX_TEX0MTXIDX, "in_Tex0MatIdx");
		glBindAttribLocation(shader_prog, VTX_TEX1MTXIDX, "in_Tex1MatIdx");
		glBindAttribLocation(shader_prog, VTX_TEX2MTXIDX, "in_Tex2MatIdx");
		glBindAttribLocation(shader_prog, VTX_TEX3MTXIDX, "in_Tex3MatIdx");
		glBindAttribLocation(shader_prog, VTX_TEX4MTXIDX, "in_Tex4MatIdx");
		glBindAttribLocation(shader_prog, VTX_TEX5MTXIDX, "in_Tex5MatIdx");
		glBindAttribLocation(shader_prog, VTX_TEX6MTXIDX, "in_Tex6MatIdx");
		glBindAttribLocation(shader_prog, VTX_TEX7MTXIDX, "in_Tex7MatIdx");

		glBindAttribLocation(shader_prog, VTX_POS, "in_Position");

		glBindAttribLocation(shader_prog, VTX_NRM, "in_Normal");
		glBindAttribLocation(shader_prog, VTX_BINRM, "in_Binormal");
		glBindAttribLocation(shader_prog, VTX_TANGENT, "in_Tangent");

		glBindAttribLocation(shader_prog, VTX_COLOR0, "in_Color0");
		glBindAttribLocation(shader_prog, VTX_COLOR1, "in_Color1");

		glBindAttribLocation(shader_prog, VTX_TEXCOORD0, "in_TexCoord0");
		glBindAttribLocation(shader_prog, VTX_TEXCOORD1, "in_TexCoord1");
		glBindAttribLocation(shader_prog, VTX_TEXCOORD2, "in_TexCoord2");
		glBindAttribLocation(shader_prog, VTX_TEXCOORD3, "in_TexCoord3");
		glBindAttribLocation(shader_prog, VTX_TEXCOORD4, "in_TexCoord4");
		glBindAttribLocation(shader_prog, VTX_TEXCOORD5, "in_TexCoord5");
		glBindAttribLocation(shader_prog, VTX_TEXCOORD6, "in_TexCoord6");
		glBindAttribLocation(shader_prog, VTX_TEXCOORD7, "in_TexCoord7");
	}

	void GXCore::InitVBO()
	{
		vertex_data = new Vertex[vbo_size];
		memset(vertex_data, 0, sizeof(Vertex) * vbo_size);

		vao = 0;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		vbo = 0;
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vbo_size * sizeof(Vertex), vertex_data, GL_STATIC_DRAW);

		glEnableVertexAttribArray(VTX_POSMATIDX);
		glVertexAttribPointer(VTX_POSMATIDX, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, PosMatIdx)));

		glEnableVertexAttribArray(VTX_TEX0MTXIDX);
		glVertexAttribPointer(VTX_TEX0MTXIDX, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Tex0MatIdx)));
		glEnableVertexAttribArray(VTX_TEX1MTXIDX);
		glVertexAttribPointer(VTX_TEX1MTXIDX, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Tex1MatIdx)));
		glEnableVertexAttribArray(VTX_TEX2MTXIDX);
		glVertexAttribPointer(VTX_TEX2MTXIDX, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Tex2MatIdx)));
		glEnableVertexAttribArray(VTX_TEX3MTXIDX);
		glVertexAttribPointer(VTX_TEX3MTXIDX, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Tex3MatIdx)));
		glEnableVertexAttribArray(VTX_TEX4MTXIDX);
		glVertexAttribPointer(VTX_TEX4MTXIDX, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Tex4MatIdx)));
		glEnableVertexAttribArray(VTX_TEX5MTXIDX);
		glVertexAttribPointer(VTX_TEX5MTXIDX, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Tex5MatIdx)));
		glEnableVertexAttribArray(VTX_TEX6MTXIDX);
		glVertexAttribPointer(VTX_TEX6MTXIDX, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Tex6MatIdx)));
		glEnableVertexAttribArray(VTX_TEX7MTXIDX);
		glVertexAttribPointer(VTX_TEX7MTXIDX, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Tex7MatIdx)));

		glEnableVertexAttribArray(VTX_POS);
		glVertexAttribPointer(VTX_POS, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Position)));

		glEnableVertexAttribArray(VTX_NRM);
		glVertexAttribPointer(VTX_NRM, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Normal)));
		glEnableVertexAttribArray(VTX_BINRM);
		glVertexAttribPointer(VTX_BINRM, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Binormal)));
		glEnableVertexAttribArray(VTX_TANGENT);
		glVertexAttribPointer(VTX_TANGENT, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Tangent)));

		glEnableVertexAttribArray(VTX_COLOR0);
		glVertexAttribPointer(VTX_COLOR0, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Color[0])));
		glEnableVertexAttribArray(VTX_COLOR1);
		glVertexAttribPointer(VTX_COLOR1, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, Color[1])));

		glEnableVertexAttribArray(VTX_TEXCOORD0);
		glVertexAttribPointer(VTX_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, TexCoord[0])));
		glEnableVertexAttribArray(VTX_TEXCOORD1);
		glVertexAttribPointer(VTX_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, TexCoord[1])));
		glEnableVertexAttribArray(VTX_TEXCOORD2);
		glVertexAttribPointer(VTX_TEXCOORD2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, TexCoord[2])));
		glEnableVertexAttribArray(VTX_TEXCOORD3);
		glVertexAttribPointer(VTX_TEXCOORD3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, TexCoord[3])));
		glEnableVertexAttribArray(VTX_TEXCOORD4);
		glVertexAttribPointer(VTX_TEXCOORD4, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, TexCoord[4])));
		glEnableVertexAttribArray(VTX_TEXCOORD5);
		glVertexAttribPointer(VTX_TEXCOORD5, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, TexCoord[5])));
		glEnableVertexAttribArray(VTX_TEXCOORD6);
		glVertexAttribPointer(VTX_TEXCOORD6, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, TexCoord[6])));
		glEnableVertexAttribArray(VTX_TEXCOORD7);
		glVertexAttribPointer(VTX_TEXCOORD7, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(offsetof(Vertex, TexCoord[7])));
	}

	void GXCore::DisposeVBO()
	{
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
