// A simple example of a debugger that just prints debug messages. Works in its own thread. 

#include "pch.h"

Thread* debugger;

void DebugThreadProc(void* param)
{
	std::list<std::pair<int, std::string>> queue;

	UI::Jdi.QueryDebugMessages(queue);

	if (!queue.empty())
	{
		for (auto it = queue.begin(); it != queue.end(); ++it)
		{
			std::string channelName = UI::Jdi.DebugChannelToString(it->first);

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
}

void DebugStart()
{
	debugger = new Thread(DebugThreadProc, false, nullptr, "DebugThread");
}

void DebugStop()
{
	delete debugger;
}
