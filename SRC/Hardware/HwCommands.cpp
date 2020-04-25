// Various commands for debugging hardware (Flipper). Available only after emulation has been started.
#include "pch.h"

namespace Flipper
{
	// Load binary file to main memory
	static Json::Value* cmd_ramload(std::vector<std::string>& args)
	{
		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0) & RAMMASK;

		size_t dataSize = 0;
		uint8_t* data = (uint8_t *)UI::FileLoad(args[1].c_str(), &dataSize);

		if (address >= mi.ramSize || (address + dataSize) >= mi.ramSize)
		{
			free(data);
			DBReport("Address out of range!\n");
			return nullptr;
		}

		if (!data)
		{
			DBReport("Failed to load: %s\n", args[1].c_str());
			return nullptr;
		}

		memcpy(&mi.ram[address], data, dataSize);
		free(data);
		return nullptr;
	}

	// Save main memory content to file
	static Json::Value* cmd_ramsave(std::vector<std::string>& args)
	{
		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0) & RAMMASK;
		uint32_t dataSize = (uint32_t)strtoul(args[3].c_str(), nullptr, 0);

		if (address >= mi.ramSize || (address + dataSize) >= mi.ramSize)
		{
			DBReport("Address out of range!\n");
			return nullptr;
		}

		if (!UI::FileSave(args[1].c_str(), &mi.ram[address], dataSize))
		{
			DBReport("Failed to save: %s\n", args[1].c_str());
		}
		return nullptr;
	}

	// Load binary file to ARAM
	static Json::Value* cmd_aramload(std::vector<std::string>& args)
	{
		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0);

		size_t dataSize = 0;
		uint8_t* data = (uint8_t*)UI::FileLoad(args[1].c_str(), &dataSize);

		if (address >= ARAMSIZE || (address + dataSize) >= ARAMSIZE)
		{
			free(data);
			DBReport("Address out of range!\n");
			return nullptr;
		}

		if (!data)
		{
			DBReport("Failed to load: %s\n", args[1].c_str());
			return nullptr;
		}

		memcpy(&aram.mem[address], data, dataSize);
		free(data);
		return nullptr;
	}

	// Save ARAM content to file
	static Json::Value* cmd_aramsave(std::vector<std::string>& args)
	{
		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0);
		uint32_t dataSize = (uint32_t)strtoul(args[3].c_str(), nullptr, 0);

		if (address >= ARAMSIZE || (address + dataSize) >= ARAMSIZE)
		{
			DBReport("Address out of range!\n");
			return nullptr;
		}

		if (!UI::FileSave(args[1].c_str(), &aram.mem[address], dataSize))
		{
			DBReport("Failed to save: %s\n", args[1].c_str());
		}
		return nullptr;
	}

	// Dump PI/CP FIFO configuration
	static Json::Value* DumpFifo(std::vector<std::string>& args)
	{
		DumpPIFIFO();
		DumpCPFIFO();
		return nullptr;
	}

	void hw_init_handlers()
	{
		Debug::Hub.AddCmd("ramload", cmd_ramload);
		Debug::Hub.AddCmd("ramsave", cmd_ramsave);
		Debug::Hub.AddCmd("aramload", cmd_aramload);
		Debug::Hub.AddCmd("aramsave", cmd_aramsave);
		Debug::Hub.AddCmd("DumpFifo", DumpFifo);
	}
};
