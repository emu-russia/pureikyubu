// Portable thread implementation.

#pragma once

#ifdef _WINDOWS
#include <Windows.h>
#endif

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

	bool running = false;
	SpinLock resumeLock;
	int resumeCounter = 0;
	int suspendCounter = 0;

	bool terminateFlag = false;

	char threadName[0x100] = { 0 };

#ifdef _WINDOWS
	HANDLE threadHandle = INVALID_HANDLE_VALUE;
	DWORD threadId = 0;
	static DWORD WINAPI RingleaderThreadProc(LPVOID lpParameter);
#endif
public:

	// Create thread
	Thread(ThreadProc threadProc, bool suspended, void* context, const char* name);

	// Join thread
	~Thread();

	void Resume();
	void Suspend();
	bool IsRunning() { return running; }

	void Terminate()
	{
		terminateFlag = true;
	}
	bool Terminated() { return terminateFlag; }
};
