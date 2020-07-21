#include "pch.h"

using namespace Debug;

DWORD WINAPI Thread::RingleaderThreadProc(LPVOID lpParameter)
{
	WrappedContext* wrappedCtx = (WrappedContext*)lpParameter;

	if (wrappedCtx->proc)
	{
		wrappedCtx->proc(wrappedCtx->context);
	}

	return 0;
}

Thread::Thread(ThreadProc threadProc, bool suspended, void* context, const char* name)
{
	running = !suspended;
	strcpy_s(threadName, sizeof(threadName) - 1, name);

#ifdef _WINDOWS
	ctx.context = context;
	ctx.proc = threadProc;
	threadHandle = CreateThread(NULL, StackSize, RingleaderThreadProc, &ctx, suspended ? CREATE_SUSPENDED : 0, &threadId);
	assert(threadHandle != INVALID_HANDLE_VALUE);
#endif

	Report(Channel::Norm, "Thread: %s\n", threadName);
}

Thread::~Thread()
{
	Suspend();

#ifdef _WINDOWS
	TerminateThread(threadHandle, 0);
	WaitForSingleObject(threadHandle, 1000);
#endif

	Report(Channel::Norm, "~Thread: %s\n", threadName);
}

void Thread::Resume()
{
	resumeLock.Lock();
	if (!running)
	{
#ifdef _WINDOWS
		ResumeThread(threadHandle);
#endif
		running = true;
		resumeCounter++;
		//Report(Channel::Norm, "%s Resume\n", threadName);
	}
	resumeLock.Unlock();
}

void Thread::Suspend()
{
	if (running)
	{
		running = false;
		suspendCounter++;
		//Report(Channel::Norm, "%s Suspend\n", threadName);
#ifdef _WINDOWS
		SuspendThread(threadHandle);
#endif
	}
}
