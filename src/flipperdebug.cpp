// Various commands for debugging hardware (Flipper). Available only after emulation has been started.
#include "pch.h"

using namespace Debug;

namespace Flipper
{
	const char *HwJdi = R"json(
{
  "info": {
	"description": "Various commands for debugging hardware (Flipper). Available only after emulation has been started.",
	"helpGroup": "HW Debug Commands"
  },

  "can": {

	"ramload": {
	  "help": "Load binary file to main memory",
	  "args": 2,
	  "usage": [
		"Syntax: ramload <file> <address>\n",
		"Example of use: ramload lomem.bin 0x80000000\n"
	  ]
	},

	"ramsave": {
	  "help": "Save main memory content to file",
	  "args": 3,
	  "usage": [
		"Syntax: ramsave <file> <address> <size>\n",
		"Example of use: ramsave lomem.bin 0x80000000 0x3300\n"
	  ]
	},

	"aramload": {
	  "help": "Load binary file to ARAM",
	  "args": 2,
	  "usage": [
		"Syntax: aramload <file> <address>\n",
		"Example of use: aramload samples.bin 0x10000\n"
	  ]
	},

	"aramsave": {
	  "help": "Save ARAM content to file",
	  "args": 3,
	  "usage": [
		"Syntax: aramsave <file> <address> <size>\n",
		"Example of use: aramsave samples.bin 0x10000 0x200\n"
	  ]
	},

	"nvi": {
	  "help": "Run emulation until the next VI interrupt. You can perform frame-by-frame emulation from a VI perspective"
	},

	"npe": {
	  "help": "Run emulation until the next PE interrupt (Done/Token). You can perform frame-by-frame emulation from the GFX Engine perspective"
	}

  },

  "todo": [ "Parametrize input/output Arrays"]

}
	)json";

	// Load binary file to main memory
	static Json::Value* cmd_ramload(std::vector<std::string>& args)
	{
		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0) & 0x0fffffff;
		auto data = Util::FileLoad(args[1]);

		uint8_t* ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForDebug(address);
		if (!ptr)
		{
			Report(Channel::Norm, "Address out of range!\n");
			return nullptr;
		}

		if (data.empty())
		{
			Report(Channel::Norm, "Failed to load: %s\n", args[1].c_str());
			return nullptr;
		}

		memcpy(ptr, data.data(), data.size());
		return nullptr;
	}

	// Save main memory content to file
	static Json::Value* cmd_ramsave(std::vector<std::string>& args)
	{
		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0) & 0x0fffffff;
		uint32_t dataSize = (uint32_t)strtoul(args[3].c_str(), nullptr, 0);

		uint8_t* ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForDebug(address);
		if (!ptr)
		{
			Report(Channel::Norm, "Address out of range!\n");
			return nullptr;
		}

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

		memcpy(&DSP::aram.mem[address], data.data(), data.size());
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

		auto ptr = &DSP::aram.mem[address];
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

		HW->pi->PIBreakOnNextInt(PI_INTERRUPT_VI);
		Core->Run();
		return nullptr;
	}

	static Json::Value* cmd_nextpe(std::vector<std::string>& args)
	{
		if (!JDI::Hub.ExecuteFastBool("IsLoaded")) {
			return nullptr;
		}

		HW->pi->PIBreakOnNextInt(PI_INTERRUPT_PE_FINISH | PI_INTERRUPT_PE_TOKEN);
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
