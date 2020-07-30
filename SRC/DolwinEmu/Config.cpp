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
	if (!Util::FileExists(DOLWIN_DEFAULT_SETTINGS))
	{
		throw "Default settings missing!";
	}

	auto jsonText = Util::FileLoad(DOLWIN_DEFAULT_SETTINGS);
	if (jsonText.empty())
	{
		throw "Default settings missing!";
	}

	defaultSettings.Deserialize(jsonText.data(), jsonText.size());

	// Merge with current settings.
	settings.Clone(&defaultSettings);

	if (Util::FileExists(DOLWIN_SETTINGS))
	{
		jsonText = Util::FileLoad(DOLWIN_SETTINGS);
		assert(!jsonText.empty());

		Json currentSettings;

		currentSettings.Deserialize(jsonText.data(), jsonText.size());

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
		return;
	}

	settings.GetSerializedTextSize(bogus, -1, textSize);

	// Serialize and save current settings.
	std::vector<uint8_t> text(2 * textSize, 0);
	settings.Serialize(text.data(), 2 * textSize, textSize);

	Util::FileSave(DOLWIN_SETTINGS, text);
}


#pragma region "Config API"

TCHAR* GetConfigString(const char* var, const char* path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = settings.root.children.back()->ByName(path);
	assert(section);

	Json::Value* value = section->ByName(var);
	if (value == nullptr)
	{
		value = section->AddString(var, L"");
	}

	assert(value->type == Json::ValueType::String);

	settingsLock.Unlock();

	return value->value.AsString;
}

void SetConfigString(const char* var, const TCHAR* newVal, const char* path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value * section = settings.root.children.back()->ByName(path);
	assert(section);

	Json::Value* value = section->ByName(var);
	if (value == nullptr)
	{
		value = section->AddString(var, newVal);
	}

	assert(value->type == Json::ValueType::String);

	value->ReplaceString(newVal);

	SaveSettings();

	settingsLock.Unlock();
}

int GetConfigInt(const char* var, const char* path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = settings.root.children.back()->ByName(path);
	assert(section);

	Json::Value* value = section->ByName(var);
	if (value == nullptr)
	{
		value = section->AddInt(var, 0);
	}

	assert(value->type == Json::ValueType::Int);

	settingsLock.Unlock();

	return (int)value->value.AsInt;
}

void SetConfigInt(const char* var, int newVal, const char* path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = settings.root.children.back()->ByName(path);
	assert(section);

	Json::Value* value = section->ByName(var);
	if (value == nullptr)
	{
		value = section->AddInt(var, newVal);
	}

	assert(value->type == Json::ValueType::Int);

	value->value.AsInt = (uint64_t)newVal;

	SaveSettings();

	settingsLock.Unlock();
}

bool GetConfigBool(const char* var, const char* path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = settings.root.children.back()->ByName(path);
	assert(section);

	Json::Value* value = section->ByName(var);
	if (value == nullptr)
	{
		value = section->AddBool(var, false);
	}

	assert(value->type == Json::ValueType::Bool);

	settingsLock.Unlock();

	return (int)value->value.AsBool;
}

void SetConfigBool(const char* var, bool newVal, const char* path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = settings.root.children.back()->ByName(path);
	assert(section);

	Json::Value* value = section->ByName(var);
	if (value == nullptr)
	{
		value = section->AddBool(var, newVal);
	}

	assert(value->type == Json::ValueType::Bool);

	value->value.AsBool = newVal;

	SaveSettings();

	settingsLock.Unlock();
}

#pragma endregion "Config API"
