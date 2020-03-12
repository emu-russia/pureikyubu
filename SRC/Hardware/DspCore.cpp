// Low-level DSP core

#include "pch.h"

namespace DSP
{

	DspCore::DspCore(HWConfig* config)
	{
		pendingThreadTerminate = false;

		threadHandle = CreateThread(NULL, 0, DspThreadProc, this, CREATE_SUSPENDED, &threadId);

		Reset();
	}

	DspCore::~DspCore()
	{
		Run();

		pendingThreadTerminate = true;

		// Wait thread join
		while (pendingThreadTerminate);
	}

	DWORD WINAPI DspCore::DspThreadProc(LPVOID lpParameter)
	{
		DspCore* core = (DspCore*)lpParameter;

		while (!core->pendingThreadTerminate)
		{
			// Do DSP actions
			core->Update();

			Sleep(1);
		}

		return 0;
	}

	void DspCore::Reset()
	{
		savedGekkoTicks = cpu.tb.uval;
	}

	void DspCore::AddBreakpoint(uint16_t imemAddress)
	{
		MySpinLock::Lock(&breakPointsSpinLock);
		breakpoints.push_back(imemAddress);
		MySpinLock::Unlock(&breakPointsSpinLock);
	}

	void DspCore::ClearBreakpoints()
	{
		MySpinLock::Lock(&breakPointsSpinLock);
		breakpoints.clear();
		MySpinLock::Unlock(&breakPointsSpinLock);
	}

	void DspCore::Run()
	{
		if (!running)
		{
			ResumeThread(threadHandle);
			running = true;
		}
	}

	void DspCore::Suspend()
	{
		if (running)
		{
			SuspendThread(threadHandle);
			running = false;
		}
	}

	void DspCore::Step()
	{
		if (IsRunning())
		{
			DBReport(_DSP "It is impossible while running.\n");
			return;
		}


	}

	void DspCore::Update()
	{

	}

}
