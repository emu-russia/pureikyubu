// Emu debug commands

#include "dolphin.h"

static Json::Value* EmuFileLoad(std::vector<std::string>& args)
{
	FILE* f;

	fopen_s(&f, args[1].c_str(), "rb");
	if (!f)
	{
		DBReport2(DbgChannel::Error, "Failed to open: %s\n", args[1].c_str());
		return nullptr;
	}

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	while (!feof(f))
	{
		uint8_t AsByte = 0;
		fread(&AsByte, 1, 1, f);
		output->AddInt(nullptr, AsByte);
	}

	fclose(f);
	DBReport("Loaded: %s (%zi bytes)\n", args[1].c_str(), output->children.size());

	return output;
}

static Json::Value* EmuFileSave(std::vector<std::string>& args)
{
	std::vector<std::string> cmdArgs;

	cmdArgs.insert(cmdArgs.begin(), args.begin() + 2, args.end());

	Json::Value* input = Debug::Hub.Execute(cmdArgs);
	if (input)
	{
		if (input->type != Json::ValueType::Array)
		{
			DBReport2(DbgChannel::Error, "Command returned invalid output (must be Array)\n");
			delete input;
			return nullptr;
		}

		FILE* f;

		fopen_s(&f, args[1].c_str(), "wb");
		if (!f)
		{
			DBReport2(DbgChannel::Error, "Failed to create file: %s\n", args[1].c_str());
		}

		for (auto it = input->children.begin(); it != input->children.end(); ++it)
		{
			Json::Value* child = *it;

			if (child->type == Json::ValueType::Int)
			{
				uint8_t AsByte = (uint8_t)child->value.AsInt;
				fwrite(&AsByte, 1, 1, f);
			}
			else if (child->type == Json::ValueType::String)
			{
				size_t size = _tcslen(child->value.AsString);
				fwrite(child->value.AsString, sizeof(TCHAR), size, f);
			}

			// Skip other types for now
		}

		fclose(f);
		DBReport("Saved as: %s\n", args[1].c_str());
	}

	return nullptr;
}

// Sleep specified number of milliseconds
static Json::Value* cmd_sleep(std::vector<std::string>& args)
{
	Sleep(atoi(args[1].c_str()));
	return nullptr;
}

// Exit
static Json::Value* cmd_exit(std::vector<std::string>& args)
{
	UNREFERENCED_PARAMETER(args);

	DBReport(": exiting...\n");
	EMUClose();
	EMUDtor();
	exit(0);
}

static Json::Value* GetLoaded(std::vector<std::string>& args)
{
	UNREFERENCED_PARAMETER(args);

	if (!emu.loaded)
		return nullptr;

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Object;

	output->AddString("loaded", ldat.currentFile);

	return output;
}

static Json::Value* cmd_boot(std::vector<std::string>& args)
{
	char filepath[0x1000] = { 0, };

	strncpy_s(filepath, sizeof(filepath), args[1].c_str(), 255);

	FILE* f = nullptr;
	fopen_s(&f, filepath, "rb");
	if (!f)
	{
		DBReport("file not exist! filepath=%s\n", filepath);
		return nullptr;
	}
	else fclose(f);

	LoadFile(filepath);
	EMUClose();
	EMUOpen();

	return nullptr;
}

static Json::Value* cmd_unload(std::vector<std::string>& args)
{
	UNREFERENCED_PARAMETER(args);

	if (emu.loaded)
	{
		EMUClose();
	}
	else DBReport("not loaded.\n");
	return nullptr;
}

static Json::Value* cmd_dop(std::vector<std::string>& args)
{
	UNREFERENCED_PARAMETER(args);

	if (ldat.patches.size() == 0)
	{
		DBReport("no patch data loaded.\n");
		return nullptr;
	}
	else ApplyPatches();
	return nullptr;
}

static Json::Value* cmd_plist(std::vector<std::string>& args)
{
	UNREFERENCED_PARAMETER(args);

	if (ldat.patches.size() == 0)
	{
		DBReport("no patch data loaded.\n");
		return nullptr;
	}

	DBReport("i----addr-----data-------------s-f-\n");
	int count = 0;
	for (auto it = ldat.patches.begin(); it != ldat.patches.end(); ++it)
	{
		Patch* p = *it;
		uint8_t* data = (uint8_t*)&p->data;
		const char* fmt = "%.3i: %08X %02X%02X%02X%02X%02X%02X%02X%02X %i %i\n";

		switch (p->dataSize)
		{
		case PATCH_SIZE_8:
			fmt = "%.3i: %08X %02X %02X%02X%02X%02X%02X%02X%02X %i %i\n";
			break;
		case PATCH_SIZE_16:
			fmt = "%.3i: %08X %02X%02X %02X%02X%02X%02X%02X%02X %i %i\n";
			break;
		case PATCH_SIZE_32:
			fmt = "%.3i: %08X %02X%02X%02X%02X %02X%02X%02X%02X %i %i\n";
			break;
		case PATCH_SIZE_64:
			fmt = "%.3i: %08X %02X%02X%02X%02X%02X%02X%02X%02X %i %i\n";
			break;
		default:
			fmt = "PATCH DAMAGED!";
		}

		DBReport(
			fmt,
			count + 1,
			_byteswap_ulong(p->effectiveAddress),
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[6], data[7],
			_byteswap_ushort(p->dataSize), _byteswap_ushort(p->freeze) & 1
		);

		count++;
	}
	DBReport("-----------------------------------\n");
	return nullptr;
}

void EmuReflector()
{
	Debug::Hub.AddCmd("FileLoad", EmuFileLoad);
	Debug::Hub.AddCmd("FileSave", EmuFileSave);
	Debug::Hub.AddCmd("sleep", cmd_sleep);
	Debug::Hub.AddCmd("exit", cmd_exit);
	Debug::Hub.AddCmd("quit", cmd_exit);
	Debug::Hub.AddCmd("x", cmd_exit);
	Debug::Hub.AddCmd("q", cmd_exit);
	Debug::Hub.AddCmd("GetLoaded", GetLoaded);
	Debug::Hub.AddCmd("boot", cmd_boot);
	Debug::Hub.AddCmd("unload", cmd_unload);
	Debug::Hub.AddCmd("dop", cmd_dop);
	Debug::Hub.AddCmd("plist", cmd_plist);
}
