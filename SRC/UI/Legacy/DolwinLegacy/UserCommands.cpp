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

	UI::DolwinError(L"Error", L"%s", Util::StringToWstring(text).c_str());

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

	UI::DolwinReport(L"%s", Util::StringToWstring(text).c_str());

	return nullptr;
}

Json::Value* CmdGetRenderTarget(std::vector<std::string>& args)
{
	// Return HWND as RenderTarget

	Json::Value* value = new Json::Value();
	value->type = Json::ValueType::Int;
	value->value.AsInt = (uint64_t)wnd.hMainWindow;
	return value;
}

void UIReflector()
{
	UI::Jdi->JdiAddCmd("UIError", CmdUIError);
	UI::Jdi->JdiAddCmd("UIReport", CmdUIReport);
	UI::Jdi->JdiAddCmd("GetRenderTarget", CmdGetRenderTarget);
}
