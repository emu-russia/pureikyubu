// This module is used to communicate with the JDI host. The host can be on the network, in a pluggable DLL or statically linked.

// Now there is a fundamental limitation - all strings are Ansi (std::string). Historically, this is due to the fact that all commands were typed in the debug console. In time, we will move to Unicode (std::wstring).

#pragma once

#ifdef _WINDOWS
typedef bool (__cdecl* CALL_JDI_NO_RETURN)(const char* request);
typedef bool (__cdecl* CALL_JDI_RETURN_INT)(const char* request, int* valueOut);
typedef bool (__cdecl* CALL_JDI_RETURN_STRING)(const char* request, char* valueOut, size_t valueSize);
typedef bool (__cdecl* CALL_JDI_RETURN_BOOL)(const char* request, bool* valueOut);
#endif

namespace UI
{
	class JdiClient
	{
#ifdef _WINDOWS
		CALL_JDI_NO_RETURN CallJdiNoReturn = nullptr;
		CALL_JDI_RETURN_INT CallJdiReturnInt = nullptr;
		CALL_JDI_RETURN_STRING CallJdiReturnString = nullptr;
		CALL_JDI_RETURN_BOOL CallJdiReturnBool = nullptr;

		HMODULE dll = nullptr;
#endif

	public:
		JdiClient();
		~JdiClient();

		// Generic

		std::string GetVersion();
		void ExecuteCommand(const std::string& cmdline);

		// Methods for controlling an optical drive

		bool DvdMount(const std::string& path);
		void DvdUnmount();

		void DvdSeek(int offset);
		void DvdRead(std::vector<uint8_t>& data);

		uint32_t DvdOpenFile(const std::string& filename);

		bool DvdCoverOpened();
		void DvdOpenCover();
		void DvdCloseCover();

		// Configuration access

		std::string GetConfigString(const std::string& var, const std::string& path);
		void SetConfigString(const std::string& var, const std::string& newVal, const std::string& path);
		int GetConfigInt(const std::string& var, const std::string& path);
		void SetConfigInt(const std::string& var, int newVal, const std::string& path);
		bool GetConfigBool(const std::string& var, const std::string& path);
		void SetConfigBool(const std::string& var, bool newVal, const std::string& path);

		// Emulator controls

		void LoadFile(const std::string& filename);
		void Unload();
		void Run();
		void Stop();
		void Reset();

	};

	extern JdiClient Jdi;
}
