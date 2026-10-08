#pragma once

// This module is used to communicate with the JDI host. The host can be on the network, in a pluggable DLL or statically linked.

// Every string of the interface is UTF-8 (issue #372): a path that is handed to DvdMount/LoadFile,
// a value that goes through SetConfigString and a string that comes back from GetConfigString or
// DvdIsMounted are UTF-8. The front ends keep their own text wide, so they convert on their side
// with Util::WstringToString / Util::StringToWstring.

namespace UI
{
	class JdiClient
	{
	public:

		JdiClient();
		~JdiClient();

		// Generic

		std::string GetVersion();
		void ExecuteCommand(const std::string& cmdline);

		// Methods for controlling an optical drive

		bool DvdMount(const std::string& path);
		bool DvdMountSDK(const std::string& path);
		void DvdUnmount();

		void DvdSeek(int offset);
		void DvdRead(std::vector<uint8_t>& data);

		uint32_t DvdOpenFile(const std::string& filename);

		bool DvdCoverOpened();
		void DvdOpenCover();
		void DvdCloseCover();

		std::string DvdRegionById(char* DiskId);

		bool DvdIsMounted(std::string& path, bool& mountedIso);

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

		// Save states (see wiki/savestate.md). The two calls write and read the slot files the
		// `savestate`/`loadstate` commands work on, so the menu item and the command line do
		// exactly the same thing; `path` and `error` come back with the file that was used and
		// the reason a call failed, for the status bar. The machine is stopped for the duration
		// of the call, whichever side it is made from, because a state of a machine that is
		// running on another thread is a state of nothing.
		bool SaveState(int slot, std::string& path, std::string& error);
		bool LoadState(int slot, std::string& path, std::string& error);

		// Debug interface

		std::string DebugChannelToString(int chan);
		void QueryDebugMessages(std::list<std::pair<int, std::string>>& queue);
		int64_t GetResetGekkoMipsCounter();

		// Performance Counters, the emulated clock

		int64_t GetPerformanceCounter(int counter);
		void ResetPerformanceCounter(int counter);

		//! The emulated clock (TBR) in whole seconds: the time the console has counted since the
		//! machine was built. A front end shows it as a duration (the `OSSeconds` command).
		int64_t GetEmulatedSeconds();

		//! The rendering backend the GFX subsystem is running (`gxpipeline`): 0 is the shader
		//! (OpenGL) pipeline, 1 the software one. It is asked of the emulator rather than read from
		//! the configuration, because the two can differ - a save state carries the pipeline it was
		//! taken in. Answers 0 while no machine is running.
		int GetGfxPipeline();

		//! Whether the shader pipeline drops the uniform uploads whose value has not changed
		//! (`gxuniformcache`, the GFX_UNIFORM_CACHE setting). It is asked of the emulator, because
		//! the settings window shows the state of the machine that is running. Answers true while no
		//! machine is running, which is the shipped default.
		bool GetGfxUniformCache();

		// Misc

		bool JitcEnabled();

	private:
		// The two save state calls are one command with one answer, so they share the reading of
		// it: the file the state went to (or came from), the verdict and the reason it failed.
		bool StateCall(const char* request, std::string& path, std::string& error);
	};

	extern JdiClient* Jdi;
}

// banner API
std::vector<uint8_t> DVDLoadBanner(const wchar_t* dvdFile);
