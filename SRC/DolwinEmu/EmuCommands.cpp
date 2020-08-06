// Emu commands

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
				size_t size = wcslen(child->value.AsString);
				fwrite(child->value.AsString, sizeof(wchar_t), size, f);
			}

			// Skip other types for now
		}

		fclose(f);
		Report(Channel::Norm, "Saved as: %s\n", args[1].c_str());
	}

	return nullptr;
}

// Sleep specified number of milliseconds
static Json::Value* CmdSleep(std::vector<std::string>& args)
{
	Sleep(atoi(args[1].c_str()));
	return nullptr;
}

// Exit
static Json::Value* CmdExit(std::vector<std::string>& args)
{
	UNREFERENCED_PARAMETER(args);

	Report(Channel::Norm, ": exiting...\n");
	EMUClose();
	EMUDtor();
	exit(0);
}

static Json::Value* CmdLoad(std::vector<std::string>& args)
{
	if (args[1] != "Bootrom")
	{
		if (!Util::FileExists(args[1]))
		{
			Report(Channel::Norm, "file not exist! filepath=%s\n", args[1].c_str());
			return nullptr;
		}
	}

	EMUClose();
	EMUOpen(Util::StringToWstring(args[1]));

	return nullptr;
}

static Json::Value* CmdUnload(std::vector<std::string>& args)
{
	UNREFERENCED_PARAMETER(args);

	if (emu.loaded)
	{
		EMUClose();
	}
	else Report(Channel::Norm, "not loaded.\n");
	return nullptr;
}

static Json::Value* CmdReset(std::vector<std::string>& args)
{
	EMUReset();
	return nullptr;
}

// Return true if emulation state is `Loaded`
static Json::Value* CmdIsLoadedInternal(std::vector<std::string>& args)
{
	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Bool;

	output->value.AsBool = emu.loaded;
	
	return output;
}

static Json::Value* CmdGetLoadedInternal(std::vector<std::string>& args)
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
static Json::Value* CmdGetVersionInternal(std::vector<std::string>& args)
{
	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	output->AddString(nullptr, EMU_VERSION);

	return output;
}

static Json::Value* CmdGetConfig(std::vector<std::string>& args)
{
	Report(Channel::Norm, "%s = %s\n", USER_ANSI, Util::WstringToString(GetConfigString(USER_ANSI, USER_HW)).c_str());
	Report(Channel::Norm, "%s = %s\n", USER_SJIS, Util::WstringToString(GetConfigString(USER_SJIS, USER_HW)).c_str());
	Report(Channel::Norm, "%s = 0x%08X\n", USER_CONSOLE, GetConfigInt(USER_CONSOLE, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_OS_REPORT, GetConfigBool(USER_OS_REPORT, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_PI_RSWHACK, GetConfigBool(USER_PI_RSWHACK, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_VI_XFB, GetConfigBool(USER_VI_XFB, USER_HW));

	Report(Channel::Norm, "%s = %s\n", USER_BOOTROM, Util::WstringToString(GetConfigString(USER_BOOTROM, USER_HW)).c_str());
	Report(Channel::Norm, "%s = %s\n", USER_DSP_DROM, Util::WstringToString(GetConfigString(USER_DSP_DROM, USER_HW)).c_str());
	Report(Channel::Norm, "%s = %s\n", USER_DSP_IROM, Util::WstringToString(GetConfigString(USER_DSP_IROM, USER_HW)).c_str());

	Report(Channel::Norm, "%s = %i\n", USER_EXI_LOG, GetConfigBool(USER_EXI_LOG, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_VI_LOG, GetConfigBool(USER_VI_LOG, USER_HW));

	return nullptr;
}

static Json::Value* CmdGetConfigString(std::vector<std::string>& args)
{
	wchar_t* param = GetConfigString(args[2].c_str(), args[1].c_str());

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	output->AddString(nullptr, param);

	return output;
}

static Json::Value* CmdSetConfigString(std::vector<std::string>& args)
{
	SetConfigString(args[2].c_str(), Util::StringToWstring(args[3]).c_str(), args[1].c_str());
	return nullptr;
}

static Json::Value* CmdGetConfigInt(std::vector<std::string>& args)
{
	int param = GetConfigInt(args[2].c_str(), args[1].c_str());

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	output->AddInt(nullptr, param);

	return output;
}

static Json::Value* CmdSetConfigInt(std::vector<std::string>& args)
{
	SetConfigInt(args[2].c_str(), atoi(args[3].c_str()), args[1].c_str());
	return nullptr;
}

static Json::Value* CmdGetConfigBool(std::vector<std::string>& args)
{
	bool param = GetConfigBool(args[2].c_str(), args[1].c_str());

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	output->AddBool(nullptr, param);

	return output;
}

static Json::Value* CmdSetConfigBool(std::vector<std::string>& args)
{
	SetConfigBool(args[2].c_str(), args[3] == "true" ? true : false, args[1].c_str());
	return nullptr;
}

void EmuReflector()
{
	JDI::Hub.AddCmd("FileLoad", EmuFileLoad);
	JDI::Hub.AddCmd("FileSave", EmuFileSave);
	JDI::Hub.AddCmd("sleep", CmdSleep);
	JDI::Hub.AddCmd("exit", CmdExit);
	JDI::Hub.AddCmd("quit", CmdExit);
	JDI::Hub.AddCmd("x", CmdExit);
	JDI::Hub.AddCmd("q", CmdExit);
	JDI::Hub.AddCmd("load", CmdLoad);
	JDI::Hub.AddCmd("unload", CmdUnload);
	JDI::Hub.AddCmd("reset", CmdReset);
	JDI::Hub.AddCmd("IsLoaded", CmdIsLoadedInternal);
	JDI::Hub.AddCmd("GetLoaded", CmdGetLoadedInternal);
	JDI::Hub.AddCmd("GetVersion", CmdGetVersionInternal);

	JDI::Hub.AddCmd("GetConfig", CmdGetConfig);
	JDI::Hub.AddCmd("GetConfigString", CmdGetConfigString);
	JDI::Hub.AddCmd("SetConfigString", CmdSetConfigString);
	JDI::Hub.AddCmd("GetConfigInt", CmdGetConfigInt);
	JDI::Hub.AddCmd("SetConfigInt", CmdSetConfigInt);
	JDI::Hub.AddCmd("GetConfigBool", CmdGetConfigBool);
	JDI::Hub.AddCmd("SetConfigBool", CmdSetConfigBool);
}
