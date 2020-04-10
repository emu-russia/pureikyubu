// Portable thread implementation.

#pragma once

#ifdef _WINDOWS
#include <Windows.h>
#endif

#include <string>
#include "Spinlock.h"
#include "../Debugger/Debugger.h"

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
	SpinLock resumeLock;
	int resumeCounter = 0;
	int suspendCounter = 0;

	char threadName[0x100] = { 0 };

public:

	// Create thread
	Thread(ThreadProc threadProc, bool suspended, void * context, const char * name)
	{
		running = !suspended;
		strcpy_s(threadName, sizeof(threadName) - 1, name);

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
			resumeCounter++;
			DBReport("%s Resume\n", threadName);
		}
	}

	void Suspend()
	{
		if (running)
		{
			running = false;
			suspendCounter++;
			DBReport("%s Suspend\n", threadName);
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
