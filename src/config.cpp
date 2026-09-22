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
// Arrays of objects

// The section, the list and the entry a call is about. A list that is missing is created by a
// setter (and only by a setter): an entry that is not there is what a device that has never been
// configured looks like, so a getter must not bring it into being.
static Json::Value* GetConfigArray(const char* var, const char* path)
{
	Json::Value* root = GetSettingsRoot();
	Json::Value* section = (root != nullptr) ? root->ByName(path) : nullptr;

	if (section == nullptr || section->type != Json::ValueType::Object)
	{
		return nullptr;
	}

	Json::Value* array = section->ByName(var);

	return (array != nullptr && array->type == Json::ValueType::Array) ? array : nullptr;
}

// The entry `index` of the list. The list and the entries up to that one are made when `create` is
// set: an entry whose member is written for the first time is what a device that is added to the
// pool looks like, and it is also why the list can be longer than what a device has written so far.
static Json::Value* GetConfigArrayEntry(const char* var, const char* path, int index, bool create)
{
	if (index < 0)
	{
		return nullptr;
	}

	Json::Value* array = GetConfigArray(var, path);

	if (array == nullptr)
	{
		if (!create)
		{
			return nullptr;
		}

		Json::Value* root = GetSettingsRoot();
		Json::Value* section = (root != nullptr) ? root->ByName(path) : nullptr;

		if (section == nullptr || section->type != Json::ValueType::Object)
		{
			return nullptr;
		}

		array = section->AddArray(var);
	}

	// An entry has no name of its own (it is an element of the list), so the walk is by position.
	Json::Value* entry = nullptr;
	int at = 0;

	for (auto it = array->children.begin(); it != array->children.end(); ++it, ++at)
	{
		if (at == index)
		{
			entry = *it;
			break;
		}
	}

	if (entry != nullptr)
	{
		return (entry->type == Json::ValueType::Object) ? entry : nullptr;
	}

	if (!create)
	{
		return nullptr;
	}

	while (at <= index)
	{
		entry = array->AddObject(nullptr);
		at++;
	}

	return entry;
}

int GetConfigArraySize(const char* var, const char* path)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* array = GetConfigArray(var, path);
	int size = (array != nullptr) ? (int)array->children.size() : 0;


	return size;
}

bool ConfigArrayValueExists(const char* var, const char* path, int index, const char* member)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* entry = GetConfigArrayEntry(var, path, index, false);
	bool exists = (entry != nullptr) && entry->ByName(member) != nullptr;


	return exists;
}

int GetConfigArrayInt(const char* var, const char* path, int index, const char* member, int def)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* entry = GetConfigArrayEntry(var, path, index, false);
	Json::Value* value = (entry != nullptr) ? entry->ByName(member) : nullptr;

	int result = (value != nullptr && value->type == Json::ValueType::Int) ? (int)value->value.AsInt : def;


	return result;
}

const wchar_t* GetConfigArrayString(const char* var, const char* path, int index, const char* member)
{
	static wchar_t Empty[1] = { 0 };

	SettingsGuard guard;

	LoadSettings();

	Json::Value* entry = GetConfigArrayEntry(var, path, index, false);
	Json::Value* value = (entry != nullptr) ? entry->ByName(member) : nullptr;

	const wchar_t* result = Empty;

	if (value != nullptr && value->type == Json::ValueType::String)
	{
		result = value->value.AsString;
	}


	return result;
}

void SetConfigArrayInt(const char* var, const char* path, int index, const char* member, int value)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* entry = GetConfigArrayEntry(var, path, index, true);
	Json::Value* slot = (entry != nullptr) ? entry->ByName(member) : nullptr;

	if (slot == nullptr && entry != nullptr)
	{
		slot = entry->AddInt(member, value);
	}

	if (slot == nullptr || slot->type != Json::ValueType::Int)
	{
		ReportConfigError(member, path);
		return;
	}

	slot->value.AsInt = (uint64_t)value;

	SaveSettings();

}

void SetConfigArrayString(const char* var, const char* path, int index, const char* member, const wchar_t* value)
{
	SettingsGuard guard;

	LoadSettings();

	Json::Value* entry = GetConfigArrayEntry(var, path, index, true);
	Json::Value* slot = (entry != nullptr) ? entry->ByName(member) : nullptr;

	if (slot == nullptr && entry != nullptr)
	{
		slot = entry->AddString(member, value != nullptr ? value : L"");
	}

	if (slot == nullptr || slot->type != Json::ValueType::String)
	{
		ReportConfigError(member, path);
		return;
	}

	slot->ReplaceString(value != nullptr ? value : L"");

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

#pragma endregion "Config API"
