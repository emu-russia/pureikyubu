/*

# Dolwin user variables access. Json-based.

## How the settings worked before.

Previously stored in the registry, in separate keys.

## As it is now.

Settings are stored in Json. The default settings are stored in DefaultSettings.json and overrided by the current settings from Settings.json.
New settings are saved only in Settings.json.

*/

#include "pch.h"

static SpinLock settingsLock;
static bool SettingsLoaded = false;
static Json defaultSettings;		// singletone. Autodeleted at exit
static Json settings;		// singletone. Autodeleted at exit

static void LoadSettings()
{
	if (SettingsLoaded)
		return;

	// Load default settings
	assert(UI::FileExists(DOLWIN_DEFAULT_SETTINGS));

	auto jsonText = UI::FileLoad(DOLWIN_DEFAULT_SETTINGS);
	assert(!jsonText.empty());

	try
	{
		defaultSettings.Deserialize(jsonText.data(), jsonText.size());
	}
	catch (...)
	{
		UI::DolwinError(_T("Critical Error"), _T("Default settings cannot be loaded!"));
		return;
	}

	/* Merge with current settings. */
	settings.Clone(&defaultSettings);

	if (UI::FileExists(DOLWIN_SETTINGS))
	{
		jsonText = UI::FileLoad(DOLWIN_SETTINGS);
		assert(!jsonText.empty());

		Json currentSettings;

		try
		{
			currentSettings.Deserialize(jsonText.data(), jsonText.size());
		}
		catch (...)
		{
			UI::DolwinReport(_T("Current settings cannot be deserialized. Check json syntax. Falling back to defaults."));
			return;
		}

		settings.Merge(&currentSettings);
	}

	SettingsLoaded = true;
}

static void SaveSettings()
{
	uint8_t bogus[0x100] = { 0, };
	size_t textSize = 0;

	// Calculate Json size
	if (!SettingsLoaded)
	{
		UI::DolwinError(L"Critical Error", L"Settings must be loaded first!");
		return;
	}

	try
	{
		settings.GetSerializedTextSize(bogus, -1, textSize);
	}
	catch (...)
	{
		UI::DolwinError(L"Critical Error", L"Settings cannot be saved!");
		return;
	}

	/* Serialize and save current settings. */
	std::vector<uint8_t> text(2 * textSize, 0);
	try
	{
		settings.Serialize(text.data(), 2 * textSize, textSize);
	}
	catch (...)
	{
		UI::DolwinError(L"Critical Error", L"Settings cannot be saved!");
		return;
	}

	UI::FileSave(DOLWIN_SETTINGS, text);
}


#pragma region "Dolwin config API"

std::wstring_view GetConfigString(std::string_view var, std::string_view path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = settings.root.children.back()->ByName(path.data());
	assert(section);

	Json::Value* value = section->ByName(var.data());
	if (value == nullptr)
	{
		value = section->AddString(var.data(), L"");
	}

	assert(value->type == Json::ValueType::String);

	settingsLock.Unlock();

	return std::wstring_view(value->value.AsString);
}

void SetConfigString(std::string_view var, std::wstring_view newVal, std::string_view path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value * section = settings.root.children.back()->ByName(path.data());
	assert(section);

	Json::Value* value = section->ByName(var.data());
	if (value == nullptr)
	{
		value = section->AddString(var.data(), newVal.data());
	}

	assert(value->type == Json::ValueType::String);

	value->ReplaceString(newVal.data());

	SaveSettings();

	settingsLock.Unlock();
}

int GetConfigInt(std::string_view var, std::string_view path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = settings.root.children.back()->ByName(path.data());
	assert(section);

	Json::Value* value = section->ByName(var.data());
	if (value == nullptr)
	{
		value = section->AddInt(var.data(), 0);
	}

	assert(value->type == Json::ValueType::Int);

	settingsLock.Unlock();

	return (int)value->value.AsInt;
}

void SetConfigInt(std::string_view var, int newVal, std::string_view path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = settings.root.children.back()->ByName(path.data());
	assert(section);

	Json::Value* value = section->ByName(var.data());
	if (value == nullptr)
	{
		value = section->AddInt(var.data(), newVal);
	}

	assert(value->type == Json::ValueType::Int);

	value->value.AsInt = (uint64_t)newVal;

	SaveSettings();

	settingsLock.Unlock();
}

bool GetConfigBool(std::string_view var, std::string_view path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = settings.root.children.back()->ByName(path.data());
	assert(section);

	Json::Value* value = section->ByName(var.data());
	if (value == nullptr)
	{
		value = section->AddBool(var.data(), false);
	}

	assert(value->type == Json::ValueType::Bool);

	settingsLock.Unlock();

	return (int)value->value.AsBool;
}

void SetConfigBool(std::string_view var, bool newVal, std::string_view path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = settings.root.children.back()->ByName(path.data());
	assert(section);

	Json::Value* value = section->ByName(var.data());
	if (value == nullptr)
	{
		value = section->AddBool(var.data(), newVal);
	}

	assert(value->type == Json::ValueType::Bool);

	value->value.AsBool = newVal;

	SaveSettings();

	settingsLock.Unlock();
}

#pragma endregion "Dolwin config API"
