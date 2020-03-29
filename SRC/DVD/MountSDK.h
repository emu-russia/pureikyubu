
#pragma once

#include <string>

namespace DVD
{
	class MountDolphinSdk
	{
		bool mounted = false;

	public:
		MountDolphinSdk(const TCHAR* DolphinSDKPath);
		~MountDolphinSdk();

		bool Mounted() { return mounted; }

		void Seek(int position);
		void Read(void* buffer, size_t length);
	};
}
