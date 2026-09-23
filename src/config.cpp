/*

# User variables access. Json-based.

## How the settings worked before.

Previously stored in the registry, in separate keys.

## As it is now.

Settings are stored in Json. The default settings are stored in DefaultSettings.json and overrided by
the current settings from Settings.json. New settings are saved only in the current settings file.

The Windows and the SDL builds used to keep separate pairs of files (DefaultSettingsWin.json and
DefaultSettingsSdl.json), so that the two ports could run from the same directory; the Win32 build
is gone (issue #421), so there is one pair of files now.

*/

#include "pch.h"

static SpinLock settingsLock;

//! The lock of the settings document, released even when reading the document throws. A spin lock
//! that a throw leaves held is never released (every later call spins on it forever), and the
//! settings are read from the very beginning of the emulator's life, so the exception a broken
//! settings file produces must not be able to do that.
struct SettingsGuard
{
	SettingsGuard() { settingsLock.Lock(); }
	~SettingsGuard() { settingsLock.Unlock(); }
};

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

	// The sections of the builds before the peripheral device pool: the pads and the memory cards had
	// their settings in them, and this one keeps them in the pool (see peripherals.h). They are not
	// read any more and they are dropped from the document here, so that the file this build writes
	// does not carry them along.
	Json::Value* root = GetSettingsRoot();

	if (root != nullptr)
	{
		root->Remove(root->ByName(USER_PADS_OBSOLETE));
		root->Remove(root->ByName(USER_MEMCARDS_OBSOLETE));
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
	SettingsGuard guard;

	LoadSettings();

	Json::Value* section = GetConfigSection(var, path);

	if (section == nullptr)
	{
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
		return EmptyConfigString;
	}


	return value->value.AsString;
}

void SetConfigString(const char* var, const wchar_t* newVal, const char* path)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* section = GetConfigSection(var, path);

	if (section == nullptr)
	{
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
		return;
	}

	value->ReplaceString(newVal);

	SaveSettings();

}

int GetConfigInt(const char* var, const char* path)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* section = GetConfigSection(var, path);

	if (section == nullptr)
	{
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
		return 0;
	}


	return (int)value->value.AsInt;
}

void SetConfigInt(const char* var, int newVal, const char* path)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* section = GetConfigSection(var, path);

	if (section == nullptr)
	{
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
		return;
	}

	value->value.AsInt = (uint64_t)newVal;

	SaveSettings();

}

bool GetConfigBool(const char* var, const char* path)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* section = GetConfigSection(var, path);

	if (section == nullptr)
	{
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
		return false;
	}


	return (int)value->value.AsBool;
}

// Whether the variable is there at all. The section is looked up directly instead of through
// GetConfigSection: a caller that asks whether a setting exists does not want the missing-section
// complaint of the getters, and the absence of the section is an answer to the question, not an
// error.
bool ConfigValueExists(const char* var, const char* path)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* root = GetSettingsRoot();
	Json::Value* section = (root != nullptr) ? root->ByName(path) : nullptr;
	Json::Value* value = nullptr;

	if (section != nullptr && section->type == Json::ValueType::Object)
	{
		value = section->ByName(var);
	}


	return value != nullptr;
}

// ---------------------------------------------------------------------------
// Lists of objects

// The list, the entry and the member a call is about. A list that is missing is created by the
// caller that appends to it, and never by a getter (an entry that is not there is what something
// that was never configured looks like).
static Json::Value* GetConfigList(const char* var, const char* path)
{
	Json::Value* root = GetSettingsRoot();
	Json::Value* section = (root != nullptr) ? root->ByName(path) : nullptr;

	if (section == nullptr || section->type != Json::ValueType::Object)
	{
		return nullptr;
	}

	Json::Value* list = section->ByName(var);

	return (list != nullptr && list->type == Json::ValueType::Array) ? list : nullptr;
}

int ConfigListSize(const char* var, const char* path)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* list = GetConfigList(var, path);

	return (list != nullptr) ? (int)list->children.size() : 0;
}

ConfigEntry* ConfigListAt(const char* var, const char* path, int at)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* list = GetConfigList(var, path);

	if (list == nullptr || at < 0)
	{
		return nullptr;
	}

	int i = 0;

	for (auto it = list->children.begin(); it != list->children.end(); ++it, ++i)
	{
		if (i == at)
		{
			return ((*it)->type == Json::ValueType::Object) ? (ConfigEntry*)*it : nullptr;
		}
	}

	return nullptr;
}

ConfigEntry* ConfigListAppend(const char* var, const char* path)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* root = GetSettingsRoot();
	Json::Value* section = (root != nullptr) ? root->ByName(path) : nullptr;

	if (section == nullptr || section->type != Json::ValueType::Object)
	{
		ReportConfigError(var, path);
		return nullptr;
	}

	Json::Value* list = section->ByName(var);

	if (list == nullptr)
	{
		list = section->AddArray(var);
	}

	if (list->type != Json::ValueType::Array)
	{
		ReportConfigError(var, path);
		return nullptr;
	}

	// An entry has no name of its own (it is an element of the list).
	return (ConfigEntry*)list->AddObject(nullptr);
}

void ConfigListRemove(const char* var, const char* path, ConfigEntry* entry)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* list = GetConfigList(var, path);

	if (list == nullptr || entry == nullptr)
	{
		return;
	}

	if (list->Remove((Json::Value*)entry))
	{
		SaveSettings();
	}
}

bool ConfigEntryValueExists(const ConfigEntry* entry, const char* member)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* object = (Json::Value*)entry;

	return object != nullptr && object->ByName(member) != nullptr;
}

int GetConfigEntryInt(const ConfigEntry* entry, const char* member, int def)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* object = (Json::Value*)entry;
	Json::Value* value = (object != nullptr) ? object->ByName(member) : nullptr;

	return (value != nullptr && value->type == Json::ValueType::Int) ? (int)value->value.AsInt : def;
}

void SetConfigEntryInt(ConfigEntry* entry, const char* member, int value)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* object = (Json::Value*)entry;

	if (object == nullptr)
	{
		return;
	}

	Json::Value* slot = object->ByName(member);

	if (slot == nullptr)
	{
		slot = object->AddInt(member, value);
	}

	if (slot->type != Json::ValueType::Int)
	{
		ReportConfigError(member, nullptr);
		return;
	}

	slot->value.AsInt = (uint64_t)value;

	SaveSettings();
}

const wchar_t* GetConfigEntryString(const ConfigEntry* entry, const char* member)
{
	static wchar_t Empty[1] = { 0 };

	SettingsGuard guard;

	LoadSettings();

	Json::Value* object = (Json::Value*)entry;
	Json::Value* value = (object != nullptr) ? object->ByName(member) : nullptr;

	if (value == nullptr || value->type != Json::ValueType::String)
	{
		return Empty;
	}

	return value->value.AsString;
}

void SetConfigEntryString(ConfigEntry* entry, const char* member, const wchar_t* value)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* object = (Json::Value*)entry;

	if (object == nullptr)
	{
		return;
	}

	Json::Value* slot = object->ByName(member);

	if (slot == nullptr)
	{
		slot = object->AddString(member, value != nullptr ? value : L"");
	}

	if (slot->type != Json::ValueType::String)
	{
		ReportConfigError(member, nullptr);
		return;
	}

	slot->ReplaceString(value != nullptr ? value : L"");

	SaveSettings();
}


void ConfigSectionKeepOnly(const char* path, const char* keep)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* root = GetSettingsRoot();
	Json::Value* section = (root != nullptr) ? root->ByName(path) : nullptr;

	if (section == nullptr || section->type != Json::ValueType::Object)
	{
		return;
	}

	// The members are collected first: taking one out while walking the list of them would walk into
	// the one that follows it.
	std::vector<Json::Value*> obsolete;

	for (auto it = section->children.begin(); it != section->children.end(); ++it)
	{
		Json::Value* value = *it;

		if (value->name == nullptr || strcmp(value->name, keep) == 0)
		{
			continue;
		}

		obsolete.push_back(value);
	}

	if (obsolete.empty())
	{
		return;
	}

	for (auto value : obsolete)
	{
		section->Remove(value);
	}

	SaveSettings();
}

void SetConfigBool(const char* var, bool newVal, const char* path)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* section = GetConfigSection(var, path);

	if (section == nullptr)
	{
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
		return;
	}

	value->value.AsBool = newVal;

	SaveSettings();
}
