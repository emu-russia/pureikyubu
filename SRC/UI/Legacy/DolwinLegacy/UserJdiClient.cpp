// JDI Communication.

#include "pch.h"

namespace UI
{
	JdiClient Jdi;		// Singletone.

	JdiClient::JdiClient()
	{

	}

	JdiClient::~JdiClient()
	{

	}

	// Generic

	std::string JdiClient::GetVersion()
	{

	}

	void JdiClient::ExecuteCommand(const std::string& cmdline)
	{

	}

	// Methods for controlling an optical drive

	bool JdiClient::DvdMount(const std::string& path)
	{

	}

	void JdiClient::DvdUnmount()
	{
		ExecuteCommand("UnmountDvd");
	}

	void JdiClient::DvdSeek(int offset)
	{

	}

	void JdiClient::DvdRead(std::vector<uint8_t>& data)
	{

	}

	uint32_t JdiClient::DvdOpenFile(const std::string& filename)
	{

	}

	bool JdiClient::DvdCoverOpened()
	{

	}

	void JdiClient::DvdOpenCover()
	{
		ExecuteCommand("OpenLid");
	}

	void JdiClient::DvdCloseCover()
	{
		ExecuteCommand("CloseLid");
	}

	// Configuration access

	std::string JdiClient::GetConfigString(const std::string& var, const std::string& path)
	{

	}

	void JdiClient::SetConfigString(const std::string& var, const std::string& newVal, const std::string& path)
	{

	}

	int JdiClient::GetConfigInt(const std::string& var, const std::string& path)
	{

	}

	void JdiClient::SetConfigInt(const std::string& var, int newVal, const std::string& path)
	{

	}

	bool JdiClient::GetConfigBool(const std::string& var, const std::string& path)
	{

	}

	void JdiClient::SetConfigBool(const std::string& var, bool newVal, const std::string& path)
	{

	}

	// Emulator controls

	void JdiClient::LoadFile(const std::string& filename)
	{

	}

	void JdiClient::Unload()
	{
		ExecuteCommand("unload");
	}

	void JdiClient::Run()
	{
		ExecuteCommand("run");
	}

	void JdiClient::Stop()
	{
		ExecuteCommand("stop");
	}

	void JdiClient::Reset()
	{
		ExecuteCommand("reset");
	}
}
