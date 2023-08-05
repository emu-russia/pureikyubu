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

	void GXCore::Open()
	{
		memset(&state, 0, sizeof(state));

		state.tickPerFifo = 100;
		state.updateTbrValue = Core->GetTicks() + state.tickPerFifo;

		fifo->Reset();

		state.cp_thread = EMUCreateThread(CPThread, false, this, "CPThread");
	}

	void GXCore::Close()
	{
		if (state.cp_thread)
		{
			EMUJoinThread(state.cp_thread);
			state.cp_thread = nullptr;
		}
	}

}
