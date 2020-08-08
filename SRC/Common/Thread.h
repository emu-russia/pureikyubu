// Portable thread implementation.

#pragma once

#if defined(_WINDOWS) || defined(_PLAYGROUND_WINDOWS)
#include <windows.h>
#endif

typedef void (*ThreadProc)(void* param);

class Thread
{
	struct WrappedContext
	{
		ThreadProc proc;
		void* context;
	};

	WrappedContext ctx = { 0 };

	bool running = false;
	SpinLock resumeLock;
	int resumeCounter = 0;
	int suspendCounter = 0;

	std::string threadName;
	
	// Take care about this place. If it will differ between your projects you get wrecked!

#if defined(_WINDOWS) || defined(_PLAYGROUND_WINDOWS)
	HANDLE threadHandle = INVALID_HANDLE_VALUE;
	DWORD threadId = 0;
	static DWORD WINAPI RingleaderThreadProc(LPVOID lpParameter);
	static const size_t StackSize = 0;
#endif

public:

	// Create thread
	Thread(ThreadProc threadProc, bool suspended, void* context, const char* name);

	// Join thread
	~Thread();

	void Resume();
	void Suspend();
	bool IsRunning() { return running; }
};
