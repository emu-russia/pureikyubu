// This module is used to communicate with the JDI host. The host can be on the network or in a pluggable DLL.

// Now there is a fundamental limitation - all strings are Ansi (std::string). Historically, this is due to the fact that all commands were typed in the debug console. In time, we will move to Unicode (std::wstring).

#pragma once

namespace UI
{
	class JdiClient
	{
	public:
		JdiClient();
		~JdiClient();

		// Generic

		std::string GetVersion();
		void ExecuteCommand(std::string& cmdline);

		// Methods for controlling an optical drive

		void DvdMount(std::string& path);
		void DvdUnmount();

		void DvdSeek(int offset);
		void DvdRead(std::vector<uint8_t>& data);

		uint32_t DvdOpenFile(std::string& filename);

		bool DvdCoverOpened();
		void DvdOpenCover();
		void DvdCloseCover();

		// Configuration access

		std::string GetConfigString(std::string& var, std::string& path);
		void SetConfigString(std::string& var, std::string& newVal, std::string& path);
		int GetConfigInt(std::string& var, std::string& path);
		void SetConfigInt(std::string& var, int newVal, std::string& path);
		bool GetConfigBool(std::string& var, std::string& path);
		void SetConfigBool(std::string& var, bool newVal, std::string& path);

		// Emulator controls

		void LoadFile(std::string& filename);
		void Unload();
		void Run();
		void Stop();

	};
}
