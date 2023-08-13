#include "pch.h"

Json::Value* CmdUIError(std::vector<std::string>& args)
{
	std::string text = "";

	if (args.size() < 2)
	{
		return nullptr;
	}

	for (int i = 1; i < args.size(); i++)
	{
		text += args[i] + " ";
	}

	printf("UIError: %s\n", text.c_str());

	return nullptr;
}

Json::Value* CmdUIReport(std::vector<std::string>& args)
{
	std::string text = "";

	if (args.size() < 2)
	{
		return nullptr;
	}

	for (int i = 1; i < args.size(); i++)
	{
		text += args[i] + " ";
	}

	printf("UIReport: %s\n", text.c_str());

	return nullptr;
}

Json::Value* CmdGetRenderTarget(std::vector<std::string>& args)
{
	// SimpleUI doesn't return any RenderTarget.

	Json::Value* value = new Json::Value();
	value->type = Json::ValueType::Int;
	value->value.AsInt = 0;
	return value;
}

void UIReflector()
{
	JdiAddCmd("UIError", CmdUIError);
	JdiAddCmd("UIReport", CmdUIReport);
	JdiAddCmd("GetRenderTarget", CmdGetRenderTarget);
}

int main(int argc, char** argv)
{
	// Check parameters

	if (argc < 2)
	{
		printf("Use: pureikyubu <file>\n");
		return -1;
	}

	EMUCtor();

	// Create an interface for communicating with the emulator core
	UI::Jdi = new UI::JdiClient;

	// Add UI methods

	JdiAddNode(UI_JDI_JSON, UIReflector);

	// Say hello

	printf("pureikyubu, Nintendo GameCube emulator version %s\n", UI::Jdi->GetVersion().c_str());

	// Load file and run

	printf("Press any key to stop emulation...\n\n");

	UI::Jdi->LoadFile(argv[1]);
	UI::Jdi->Run();
	Debug::debugger = new Debug::Debugger();

	// Wait key press..

#ifdef _WINDOWS
	_getch();
#endif

#ifdef _LINUX
	getc(stdin);
#endif

	// Unload

	UI::Jdi->Unload();
	JdiRemoveNode(UI_JDI_JSON);
	delete UI::Jdi;
	delete Debug::debugger;
	EMUDtor();

	printf("\nThank you for flying pureikyubu airlines!\n");
	return 0;
}
