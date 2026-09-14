/*

# User variables access. Json-based.

## How the settings worked before.

Previously stored in the registry, in separate keys.

## As it is now.

Settings are stored in Json. The default settings are stored in DefaultSettingsWin.json / DefaultSettingsSdl.json
(depending on the build) and overrided by the current settings from SettingsWin.json / SettingsSdl.json.
New settings are saved only in the current settings file.

*/

#include "pch.h"

static SpinLock settingsLock;
static bool SettingsLoaded = false;
static Json defaultSettings;		// singleton. Autodeleted at exit
static Json settings;		// singleton. Autodeleted at exit

// Returned to callers that ask for a settings value that is not there. It is writable on purpose:
// the accessors hand out a wchar_t*, exactly like a value stored in the document would be.
static wchar_t EmptyConfigString[] = L"";

// The settings document is the last Object child of the root (Deserialize puts it there). A file
// whose top level is not an object must not be mistaken for it - the old code used children.back()
// and then dereferenced whatever it found.
static Json::Value* GetSettingsRoot()
{
	for (auto it = settings.root.children.rbegin(); it != settings.root.children.rend(); ++it)
	{
		if ((*it)->type == Json::ValueType::Object)
		{
			return *it;
		}
	}

	return settings.root.children.empty() ? nullptr : settings.root.children.back();
}

// A missing section or a value of the wrong type is not fatal: these accessors are called all over
// the emulator during startup, so the problem is reported once and answered with the default.
static void ReportConfigError(const char* var, const char* path)
{
	static bool reported = false;

	if (!reported)
	{
		reported = true;
		Debug::Report(Debug::Channel::Error, "Config: cannot read \"%s\" from section \"%s\"", var, path ? path : "");
	}
}

static Json::Value* GetConfigSection(const char* var, const char* path)
{
	Json::Value* root = GetSettingsRoot();

	Json::Value* section = (root != nullptr) ? root->ByName(path) : nullptr;

	if (section == nullptr || section->type != Json::ValueType::Object)
	{
		ReportConfigError(var, path);
		return nullptr;
	}

	return section;
}

static void LoadSettings()
{
	if (SettingsLoaded)
		return;

	// Load default settings
	if (!Util::FileExists(EMU_DEFAULT_SETTINGS))
	{
		throw "Default settings missing!";
	}

	auto jsonText = Util::FileLoad(EMU_DEFAULT_SETTINGS);
	if (jsonText.empty())
	{
		throw "Default settings missing!";
	}

	// The default settings are shipped data, so a broken one is a build problem and still throws -
	// but it must not leave a partially parsed document behind, hence the local.
	Json defaultFile;

	try
	{
		defaultFile.Deserialize(jsonText.data(), jsonText.size());
	}
	catch (const char* error)
	{
		Debug::Report(Debug::Channel::Error, "Default settings are corrupt: %s", error);
		throw "Default settings missing!";
	}
	catch (...)
	{
		Debug::Report(Debug::Channel::Error, "Default settings are corrupt: the file is not a valid Json document");
		throw "Default settings missing!";
	}

	defaultSettings.Clone(&defaultFile);

	// Merge with current settings.
	settings.Clone(&defaultSettings);

	if (Util::FileExists(EMU_SETTINGS))
	{
		jsonText = Util::FileLoad(EMU_SETTINGS);

		if (jsonText.empty())
		{
			Debug::Report(Debug::Channel::Error, "User settings are empty and were ignored");
		}
		else
		{
			Json currentSettings;

			// The user's file is untrusted: whatever is wrong with it (syntax, an over-long
			// string, too deep nesting), the defaults cloned above are what the emulator runs
			// with, instead of a crash or a hang at startup.
			try
			{
				currentSettings.Deserialize(jsonText.data(), jsonText.size());

				settings.Merge(&currentSettings);
			}
			catch (const char* error)
			{
				Debug::Report(Debug::Channel::Error, "User settings were ignored: %s", error);
			}
			catch (...)
			{
				Debug::Report(Debug::Channel::Error, "User settings were ignored: the file is not a valid Json document");
			}
		}
	}

	SettingsLoaded = true;
}

static void SaveSettings()
{
	size_t textSize = 0;

	// Calculate Json size
	if (!SettingsLoaded)
	{
		return;
	}

	// The size-only pass does not touch the buffer (EmitChar writes only when sizeOnly is false),
	// so there is no stack buffer here that a stray write could overflow.
	settings.GetSerializedTextSize(nullptr, -1, textSize);

	// Serialize and save current settings.
	std::vector<uint8_t> text(2 * textSize, 0);
	settings.Serialize(text.data(), 2 * textSize, textSize);

	size_t size = strlen((char *)text.data());
	text.resize(size);

	Util::FileSave(EMU_SETTINGS, text);
}


#pragma region "Config API"

wchar_t* GetConfigString(const char* var, const char* path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = GetConfigSection(var, path);

	if (section == nullptr)
	{
		settingsLock.Unlock();
		return EmptyConfigString;
	}

	Json::Value* value = section->ByName(var);
	if (value == nullptr)
	{
		value = section->AddString(var, L"");
	}

	if (value->type != Json::ValueType::String)
	{
		ReportConfigError(var, path);
		settingsLock.Unlock();
		return EmptyConfigString;
	}

	settingsLock.Unlock();

	return value->value.AsString;
}

void SetConfigString(const char* var, const wchar_t* newVal, const char* path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = GetConfigSection(var, path);

	if (section == nullptr)
	{
		settingsLock.Unlock();
		return;
	}

	Json::Value* value = section->ByName(var);
	if (value == nullptr)
	{
		value = section->AddString(var, newVal);
	}

	if (value->type != Json::ValueType::String)
	{
		ReportConfigError(var, path);
		settingsLock.Unlock();
		return;
	}

	value->ReplaceString(newVal);

	SaveSettings();

	settingsLock.Unlock();
}

int GetConfigInt(const char* var, const char* path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = GetConfigSection(var, path);

	if (section == nullptr)
	{
		settingsLock.Unlock();
		return 0;
	}

	Json::Value* value = section->ByName(var);
	if (value == nullptr)
	{
		value = section->AddInt(var, 0);
	}

	if (value->type != Json::ValueType::Int)
	{
		ReportConfigError(var, path);
		settingsLock.Unlock();
		return 0;
	}

	settingsLock.Unlock();

	return (int)value->value.AsInt;
}

void SetConfigInt(const char* var, int newVal, const char* path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = GetConfigSection(var, path);

	if (section == nullptr)
	{
		settingsLock.Unlock();
		return;
	}

	Json::Value* value = section->ByName(var);
	if (value == nullptr)
	{
		value = section->AddInt(var, newVal);
	}

	if (value->type != Json::ValueType::Int)
	{
		ReportConfigError(var, path);
		settingsLock.Unlock();
		return;
	}

	value->value.AsInt = (uint64_t)newVal;

	SaveSettings();

	settingsLock.Unlock();
}

bool GetConfigBool(const char* var, const char* path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = GetConfigSection(var, path);

	if (section == nullptr)
	{
		settingsLock.Unlock();
		return false;
	}

	Json::Value* value = section->ByName(var);
	if (value == nullptr)
	{
		value = section->AddBool(var, false);
	}

	if (value->type != Json::ValueType::Bool)
	{
		ReportConfigError(var, path);
		settingsLock.Unlock();
		return false;
	}

	settingsLock.Unlock();

	return (int)value->value.AsBool;
}

void SetConfigBool(const char* var, bool newVal, const char* path)
{
	settingsLock.Lock();

	LoadSettings();

	Json::Value* section = GetConfigSection(var, path);

	if (section == nullptr)
	{
		settingsLock.Unlock();
		return;
	}

	Json::Value* value = section->ByName(var);
	if (value == nullptr)
	{
		value = section->AddBool(var, newVal);
	}

	if (value->type != Json::ValueType::Bool)
	{
		ReportConfigError(var, path);
		settingsLock.Unlock();
		return;
	}

	value->value.AsBool = newVal;

	SaveSettings();

	settingsLock.Unlock();
}

#pragma endregion "Config API"
