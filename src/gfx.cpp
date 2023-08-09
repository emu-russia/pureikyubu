#include "pch.h"

using namespace Debug;

namespace GX
{
	GXCore::GXCore()
	{
		memset(&state, 0, sizeof(state));

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

		memset(&state, 0, sizeof(state));

		state.tickPerFifo = 100;
		state.updateTbrValue = Core->GetTicks() + state.tickPerFifo;

		fifo->Reset();

		state.cp_thread = EMUCreateThread(CPThread, false, this, "CPThread");

#ifdef _WINDOWS
		hwndMain = (HWND)config->renderTarget;
#endif

		bool res = GL_LazyOpenSubsystem();
		assert(res);

		// vertex programs extension
		//SetupVertexShaders();
		//ReloadVertexShaders();

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

		if (state.cp_thread)
		{
			EMUJoinThread(state.cp_thread);
			state.cp_thread = nullptr;
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

		// set wireframe mode
		//glEnable(GL_CULL_FACE);
		//glCullFace(GL_BACK);
#ifdef WIREFRAME
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

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
		state.cpLoads = state.bpLoads = state.xfLoads = 0;
	}

	// rendering complete, swap buffers, sync to vretrace
	void GXCore::GPFrameDone()
	{
		GL_EndFrame();
		frame_done = true;
	}
}
