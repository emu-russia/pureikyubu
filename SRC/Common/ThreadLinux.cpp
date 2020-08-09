#include "pch.h"

void* Thread::RingleaderThreadProc(void* args)
{
	WrappedContext* wrappedCtx = (WrappedContext*)args;

	if (wrappedCtx->proc)
	{
		while (true)
		{
			wrappedCtx->proc(wrappedCtx->context);
		}
	}

	return nullptr;
}

Thread::Thread(ThreadProc threadProc, bool suspended, void* context, const char* name)
{
	running = !suspended;
	threadName = name;

	ctx.context = context;
	ctx.proc = threadProc;

	int status = pthread_create(&threadId, NULL, Thread::RingleaderThreadProc, &ctx);
	assert(status == 0);
}

Thread::~Thread()
{
	Suspend();
	//pthread_terminate(threadId);
}

void Thread::Resume()
{
	resumeLock.Lock();
	if (!running)
	{
		//pthread_kill(threadId, SIGCONT);
		running = true;
		resumeCounter++;
	}
	resumeLock.Unlock();
}

void Thread::Suspend()
{
	if (running)
	{
		running = false;
		suspendCounter++;
		//pthread_kill(threadId, SIGSTOP);
	}
}

void Thread::Sleep(size_t milliseconds)
{
	usleep(milliseconds * 1000);
}
