// Emu debug commands

#include "dolphin.h"

static Json::Value* EmuFileLoad(std::vector<std::string>& args)
{
	size_t size = 0;
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

void EmuReflector()
{
	Debug::Hub.AddCmd("FileLoad", EmuFileLoad);
	Debug::Hub.AddCmd("FileSave", EmuFileSave);
}
