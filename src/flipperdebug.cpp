// Various commands for debugging hardware (Flipper). Available only after emulation has been started.
#include "pch.h"

using namespace Debug;

namespace Flipper
{
	// Load binary file to main memory
	static Json::Value* cmd_ramload(std::vector<std::string>& args)
	{
		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0) & RAMMASK;
		auto data = Util::FileLoad(args[1]);

		if (address >= mi.ramSize || (address + data.size()) >= mi.ramSize)
		{
			Report(Channel::Norm, "Address out of range!\n");
			return nullptr;
		}

		if (data.empty())
		{
			Report(Channel::Norm, "Failed to load: %s\n", args[1].c_str());
			return nullptr;
		}

		memcpy(&mi.ram[address], data.data(), data.size());
		return nullptr;
	}

	// Save main memory content to file
	static Json::Value* cmd_ramsave(std::vector<std::string>& args)
	{
		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0) & RAMMASK;
		uint32_t dataSize = (uint32_t)strtoul(args[3].c_str(), nullptr, 0);

		if (address >= mi.ramSize || (address + dataSize) >= mi.ramSize)
		{
			Report(Channel::Norm, "Address out of range!\n");
			return nullptr;
		}

		auto ptr = &mi.ram[address];
		auto buffer = std::vector<uint8_t>();
		buffer.assign(ptr, ptr + dataSize);

		if (!Util::FileSave(args[1], buffer))
		{
			Report(Channel::Norm, "Failed to save: %s\n", args[1].c_str());
		}
		return nullptr;
	}

	// Load binary file to ARAM
	static Json::Value* cmd_aramload(std::vector<std::string>& args)
	{
		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0);
		auto data = Util::FileLoad(args[1]);

		if (address >= ARAMSIZE || (address + data.size()) >= ARAMSIZE)
		{
			Report(Channel::Norm, "Address out of range!\n");
			return nullptr;
		}

		if (data.empty())
		{
			Report(Channel::Norm, "Failed to load: %s\n", args[1].c_str());
			return nullptr;
		}

		memcpy(&aram.mem[address], data.data(), data.size());
		return nullptr;
	}

	// Save ARAM content to file
	static Json::Value* cmd_aramsave(std::vector<std::string>& args)
	{
		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0);
		uint32_t dataSize = (uint32_t)strtoul(args[3].c_str(), nullptr, 0);

		if (address >= ARAMSIZE || (address + dataSize) >= ARAMSIZE)
		{
			Report(Channel::Norm, "Address out of range!\n");
			return nullptr;
		}

		auto ptr = &aram.mem[address];
		auto buffer = std::vector<uint8_t>();
		buffer.assign(ptr, ptr + dataSize);

		if (!Util::FileSave(args[1], buffer))
		{
			Report(Channel::Norm, "Failed to save: %s\n", args[1].c_str());
		}
		return nullptr;
	}

	static Json::Value* cmd_nextvi(std::vector<std::string>& args)
	{
		if (!JDI::Hub.ExecuteFastBool("IsLoaded")) {
			return nullptr;
		}

		PIBreakOnNextInt(PI_INTERRUPT_VI);
		Core->Run();
		return nullptr;
	}

	static Json::Value* cmd_nextpe(std::vector<std::string>& args)
	{
		if (!JDI::Hub.ExecuteFastBool("IsLoaded")) {
			return nullptr;
		}

		PIBreakOnNextInt(PI_INTERRUPT_PE_FINISH | PI_INTERRUPT_PE_TOKEN);
		Core->Run();
		return nullptr;
	}

	void hw_init_handlers()
	{
		JDI::Hub.AddCmd("ramload", cmd_ramload);
		JDI::Hub.AddCmd("ramsave", cmd_ramsave);
		JDI::Hub.AddCmd("aramload", cmd_aramload);
		JDI::Hub.AddCmd("aramsave", cmd_aramsave);
		JDI::Hub.AddCmd("nvi", cmd_nextvi);
		JDI::Hub.AddCmd("npe", cmd_nextpe);
	}
};
