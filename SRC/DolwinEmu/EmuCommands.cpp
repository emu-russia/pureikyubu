// Emu debug commands

#include "pch.h"

using namespace Debug;

static Json::Value* EmuFileLoad(std::vector<std::string>& args)
{
	FILE* f;

	fopen_s(&f, args[1].c_str(), "rb");
	if (!f)
	{
		Report(Channel::Error, "Failed to open: %s\n", args[1].c_str());
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
	Report(Channel::Norm, "Loaded: %s (%zi bytes)\n", args[1].c_str(), output->children.size());

	return output;
}

static Json::Value* EmuFileSave(std::vector<std::string>& args)
{
	std::vector<std::string> cmdArgs;

	cmdArgs.insert(cmdArgs.begin(), args.begin() + 2, args.end());

	Json::Value* input = JDI::Hub.Execute(cmdArgs);
	if (input)
	{
		if (input->type != Json::ValueType::Array)
		{
			Report(Channel::Error, "Command returned invalid output (must be Array)\n");
			delete input;
			return nullptr;
		}

		FILE* f;

		fopen_s(&f, args[1].c_str(), "wb");
		if (!f)
		{
			Report(Channel::Error, "Failed to create file: %s\n", args[1].c_str());
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
		Report(Channel::Norm, "Saved as: %s\n", args[1].c_str());
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

	Report(Channel::Norm, ": exiting...\n");
	EMUClose();
	EMUDtor();
	exit(0);
}

static Json::Value* cmd_load(std::vector<std::string>& args)
{
	char filepath[0x1000] = { 0, };

	strncpy_s(filepath, sizeof(filepath), args[1].c_str(), 255);

	FILE* f = nullptr;
	fopen_s(&f, filepath, "rb");
	if (!f)
	{
		Report(Channel::Norm, "file not exist! filepath=%s\n", filepath);
		return nullptr;
	}
	else fclose(f);

	std::string str = filepath;
	std::wstring wstr = Util::StringToWstring(str);
	EMUClose();
	EMUOpen(wstr);

	return nullptr;
}

static Json::Value* cmd_unload(std::vector<std::string>& args)
{
	UNREFERENCED_PARAMETER(args);

	if (emu.loaded)
	{
		EMUClose();
	}
	else Report(Channel::Norm, "not loaded.\n");
	return nullptr;
}

static Json::Value* cmd_reset(std::vector<std::string>& args)
{
	EMUReset();
	return nullptr;
}

// Return true if emulation state is `Loaded`
static Json::Value* IsLoadedInternal(std::vector<std::string>& args)
{
	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Bool;

	output->value.AsBool = emu.loaded;
	
	return output;
}

static Json::Value* GetLoadedInternal(std::vector<std::string>& args)
{
	UNREFERENCED_PARAMETER(args);

	if (!emu.loaded)
		return nullptr;

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Object;

	output->AddString("loaded", emu.lastLoaded.c_str());

	return output;
}

// Get emulator version
static Json::Value* GetVersionInternal(std::vector<std::string>& args)
{
	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	output->AddString(nullptr, EMU_VERSION);

	return output;
}

void EmuReflector()
{
	JDI::Hub.AddCmd("FileLoad", EmuFileLoad);
	JDI::Hub.AddCmd("FileSave", EmuFileSave);
	JDI::Hub.AddCmd("sleep", cmd_sleep);
	JDI::Hub.AddCmd("exit", cmd_exit);
	JDI::Hub.AddCmd("quit", cmd_exit);
	JDI::Hub.AddCmd("x", cmd_exit);
	JDI::Hub.AddCmd("q", cmd_exit);
	JDI::Hub.AddCmd("load", cmd_load);
	JDI::Hub.AddCmd("unload", cmd_unload);
	JDI::Hub.AddCmd("reset", cmd_reset);
	JDI::Hub.AddCmd("IsLoaded", IsLoadedInternal);
	JDI::Hub.AddCmd("GetLoaded", GetLoadedInternal);
	JDI::Hub.AddCmd("GetVersion", GetVersionInternal);
}
