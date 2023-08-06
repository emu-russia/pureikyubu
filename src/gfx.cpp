#include "pch.h"

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

		//hPlugin = GetModuleHandle(NULL);
		hwndMain = (HWND)config->renderTarget;

		bool res = GL_LazyOpenSubsystem(hwndMain);
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
}
