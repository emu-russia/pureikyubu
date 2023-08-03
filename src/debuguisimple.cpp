// A simple example of a debugger that just prints debug messages. Works in its own thread. 

#include "pch.h"

Thread* debugger;

namespace UI
{
	extern JdiClient SimpleJdi;
}

void DebugThreadProc(void* param)
{
	static size_t mipsIterCounter = 1;
	std::list<std::pair<int, std::string>> queue;

	UI::SimpleJdi.QueryDebugMessages(queue);

	if (!queue.empty())
	{
		for (auto it = queue.begin(); it != queue.end(); ++it)
		{
			std::string channelName = UI::SimpleJdi.DebugChannelToString(it->first);

			if (channelName.size() != 0)
			{
				printf("%s: ", channelName.c_str());
			}

			printf("%s", it->second.c_str());
		}

		queue.clear();
		fflush(stdout);
	}

	Thread::Sleep(100);

	// Output Gekko mips every second

	mipsIterCounter++;
	if (mipsIterCounter == 10)
	{
		mipsIterCounter = 1;
		printf("Gekko mips: %f\n", (float)UI::SimpleJdi.GetResetGekkoMipsCounter() / 1000000.f);
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
