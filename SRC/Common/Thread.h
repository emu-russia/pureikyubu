// edgbla requested a portable thread implementation.

#pragma once

#ifdef _WINDOWS
#include <Windows.h>
#endif

typedef void (*ThreadProc)(void* param);

class Thread
{
	typedef struct _WrappedContext
	{
		ThreadProc proc;
		void* context;
	} WrappedContext;

	WrappedContext ctx = { 0 };

#ifdef _WINDOWS
	HANDLE threadHandle = INVALID_HANDLE_VALUE;
	DWORD threadId = 0;
	static DWORD WINAPI RingleaderThreadProc(LPVOID lpParameter)
	{
		WrappedContext* wrappedCtx = (WrappedContext*)lpParameter;

		if (wrappedCtx->proc)
		{
			wrappedCtx->proc(wrappedCtx->context);
		}

		return 0;
	}
#endif

	bool running = false;

public:

	// Create thread
	Thread(ThreadProc threadProc, bool suspended, void * context)
	{
		running = !suspended;

#ifdef _WINDOWS
		ctx.context = context;
		ctx.proc = threadProc;
		threadHandle = CreateThread(NULL, 0, RingleaderThreadProc, this, suspended ? CREATE_SUSPENDED : 0, &threadId);
		assert(threadHandle != INVALID_HANDLE_VALUE);
#endif
	}

	// Join thread
	~Thread()
	{
#ifdef _WINDOWS
		TerminateThread(threadHandle, 0);
		WaitForSingleObject(threadHandle, 1000);
#endif
	}

	void Resume()
	{
		if (!running)
		{
#ifdef _WINDOWS
			ResumeThread(threadHandle);
#endif
			running = true;
		}
	}

	void Suspend()
	{
		if (running)
		{
			running = false;
#ifdef _WINDOWS
			SuspendThread(threadHandle);
#endif
		}
	}

	bool IsRunning()
	{
		return running;
	}

};
