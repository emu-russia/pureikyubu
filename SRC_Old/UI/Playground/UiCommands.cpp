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
	// Playground doesn't return any RenderTarget.

	Json::Value* value = new Json::Value();
	value->type = Json::ValueType::Int;
	value->value.AsInt = 0;
	return value;
}

void UIReflector()
{
	UI::Jdi.JdiAddCmd("UIError", CmdUIError);
	UI::Jdi.JdiAddCmd("UIReport", CmdUIReport);
	UI::Jdi.JdiAddCmd("GetRenderTarget", CmdGetRenderTarget);
}
