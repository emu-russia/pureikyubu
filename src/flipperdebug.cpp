// Various commands for debugging hardware (Flipper). Available only after emulation has been started.
#include "pch.h"

using namespace Debug;

namespace Flipper
{

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

	// Dump a range of physical memory as a Markdown hexdump. This is what the new debugger
	// (debugui2) shows in its "Splash Memory" panel.
	static Json::Value* cmd_memdump(std::vector<std::string>& args)
	{
		if (!JDI::Hub.ExecuteFastBool("IsLoaded") || HW == nullptr)
		{
			Report(Channel::Norm, "memdump: nothing is running\n");
			return nullptr;
		}

		uint32_t address = (uint32_t)strtoul(args[1].c_str(), nullptr, 0) & 0x0FFFFFFF;

		int lines = (int)strtoul(args[2].c_str(), nullptr, 0);
		if (lines <= 0)
			lines = 16;
		if (lines > 256)
			lines = 256;

		std::string md;
		char text[0x100];

		sprintf(text, "# Physical Memory (Splash)\n\n`0x%08X` .. `0x%08X`\n\n```\n",
			address, (address + lines * 16 - 1) & 0x0FFFFFFF);
		md += text;

		for (int row = 0; row < lines; row++)
		{
			if (HW->mem->MIGetMemoryPointerForDebug(address) == nullptr)
			{
				md += "          (end of memory)\n";
				break;
			}

			sprintf(text, "%08X  ", address);
			md += text;

			std::string chars;

			// The pointer is asked for every byte: the last row of the dump may well run past
			// the end of Splash memory.
			for (int b = 0; b < 16; b++)
			{
				uint8_t* byte = (uint8_t*)HW->mem->MIGetMemoryPointerForDebug(address + b);
				if (byte == nullptr)
				{
					md += "   ";
					chars += ' ';
					continue;
				}

				sprintf(text, "%02X ", *byte);
				md += text;
				chars += (*byte >= 0x20 && *byte < 0x7F) ? (char)*byte : '.';
			}

			md += " |" + chars + "|\n";
			address += 16;
		}

		md += "```\n";

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Object;
		output->AddAnsiString("markdown", md.c_str());

		return output;
	}

	void hw_init_handlers()
	{
		JDI::Hub.AddCmd("ramload", cmd_ramload);
		JDI::Hub.AddCmd("ramsave", cmd_ramsave);
		JDI::Hub.AddCmd("aramload", cmd_aramload);
		JDI::Hub.AddCmd("aramsave", cmd_aramsave);
		JDI::Hub.AddCmd("nvi", cmd_nextvi);
		JDI::Hub.AddCmd("npe", cmd_nextpe);
		JDI::Hub.AddCmd("memdump", cmd_memdump);
	}
};
