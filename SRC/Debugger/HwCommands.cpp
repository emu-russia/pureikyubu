// Various commands for debugging hardware (Flipper)
#include "pch.h"

namespace Debug
{
	void hw_init_handlers()
	{
		con.cmds["ramload"] = cmd_ramload;
		con.cmds["ramsave"] = cmd_ramsave;
		con.cmds["aramload"] = cmd_aramload;
		con.cmds["aramsave"] = cmd_aramsave;
	}

	void hw_help()
	{
		DBReport2(DbgChannel::Header, "## HW Debug Commands\n");
		DBReport("    ramload              - Load binary file to main memory\n");
		DBReport("    ramsave              - Save main memory content to file\n");
		DBReport("    aramload             - Load binary file to ARAM\n");
		DBReport("    aramsave             - Save ARAM content to file\n");
		DBReport("\n");
	}

	// Load binary file to main memory
	void cmd_ramload(std::vector<std::string>& args)
	{
		if (!mi.ram)
		{
			DBReport("Not loaded!\n");
			return;
		}

		if (args.size() < 3)
		{
			DBReport("syntax: ramload <file> <address>\n");
			DBReport("example of use: ramload lomem.bin 0x80000000\n");
			return;
		}

		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0) & RAMMASK;

		size_t dataSize = 0;
		uint8_t* data = (uint8_t *)UI::FileLoad(args[1].c_str(), &dataSize);

		if (address >= mi.ramSize || (address + dataSize) >= mi.ramSize)
		{
			free(data);
			DBReport("Address out of range!\n");
			return;
		}

		if (!data)
		{
			DBReport("Failed to load: %s\n", args[1].c_str());
			return;
		}

		memcpy(&mi.ram[address], data, dataSize);
		free(data);
	}

	// Save main memory content to file
	void cmd_ramsave(std::vector<std::string>& args)
	{
		if (!mi.ram)
		{
			DBReport("Not loaded!\n");
			return;
		}

		if (args.size() < 4)
		{
			DBReport("syntax: ramsave <file> <address> <size>\n");
			DBReport("example of use: ramsave lomem.bin 0x80000000 0x3300\n");
			return;
		}

		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0) & RAMMASK;
		uint32_t dataSize = (uint32_t)strtoul(args[3].c_str(), nullptr, 0);

		if (address >= mi.ramSize || (address + dataSize) >= mi.ramSize)
		{
			DBReport("Address out of range!\n");
			return;
		}

		if (!UI::FileSave(args[1].c_str(), &mi.ram[address], dataSize))
		{
			DBReport("Failed to save: %s\n", args[1].c_str());
		}
	}

	// Load binary file to ARAM
	void cmd_aramload(std::vector<std::string>& args)
	{
		if (!aram.mem)
		{
			DBReport("Not loaded!\n");
			return;
		}

		if (args.size() < 3)
		{
			DBReport("syntax: aramload <file> <address>\n");
			DBReport("example of use: aramload samples.bin 0x10000\n");
			return;
		}

		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0);

		size_t dataSize = 0;
		uint8_t* data = (uint8_t*)UI::FileLoad(args[1].c_str(), &dataSize);

		if (address >= ARAMSIZE || (address + dataSize) >= ARAMSIZE)
		{
			free(data);
			DBReport("Address out of range!\n");
			return;
		}

		if (!data)
		{
			DBReport("Failed to load: %s\n", args[1].c_str());
			return;
		}

		memcpy(&aram.mem[address], data, dataSize);
		free(data);
	}

	// Save ARAM content to file
	void cmd_aramsave(std::vector<std::string>& args)
	{
		if (!aram.mem)
		{
			DBReport("Not loaded!\n");
			return;
		}

		if (args.size() < 4)
		{
			DBReport("syntax: aramsave <file> <address> <size>\n");
			DBReport("example of use: aramsave samples.bin 0x10000 0x200\n");
			return;
		}

		uint32_t address = (uint32_t)strtoul(args[2].c_str(), nullptr, 0);
		uint32_t dataSize = (uint32_t)strtoul(args[3].c_str(), nullptr, 0);

		if (address >= ARAMSIZE || (address + dataSize) >= ARAMSIZE)
		{
			DBReport("Address out of range!\n");
			return;
		}

		if (!UI::FileSave(args[1].c_str(), &aram.mem[address], dataSize))
		{
			DBReport("Failed to save: %s\n", args[1].c_str());
		}
	}

};
