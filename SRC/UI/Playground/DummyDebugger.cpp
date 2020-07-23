// A simple example of a debugger that just prints debug messages. Works in its own thread. 

#include "pch.h"

Thread* debugger;

void DebugThreadProc(void* param)
{
	while (true)
	{
		Sleep(10);
	}
}

void DebugStart()
{
	debugger = new Thread(DebugThreadProc, false, nullptr, "DebugThread");
}

void DebugStop()
{
	delete debugger;
}
