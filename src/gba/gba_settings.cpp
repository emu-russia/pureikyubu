// The GBA emulator's settings: the reader and the writer of build/Data/GBASettings.json.
//
// The document is read with the emulator's shared Json engine (src/json.cpp), the same one the
// GameCube side uses for its settings and for JDI. Json is self contained (the C++ standard
// library and verify.h only), so the portable GBA core still builds and is tested without the
// GameCube side of the emulator; gba_settings links the engine instead of carrying a parser of its
// own. The engine enforces the JSON grammar and the limits a document from the outside world has
// to respect (see wiki/security.md): a strict grammar, a 4096 code unit string, a bounded number
// token, a nesting depth and a per-container element count. Everything it refuses is a syntax
// error and rejects the whole document, which Parse() reports as "<line>: <what was expected>".
//
// The document
// ------------
// A flat object with the sections `info`, `boot`, `video`, `audio`, `input`, `link` and
// `emulation`, in that order, one member of GbaSettings per member of a section, named exactly as
// gba_settings.h names it. The `input` section is special: its members are the GBA actions
// (KeyBindingNames()) and its values are host key names, so a binding is one line:
//
//	{
//		"info":
//		{
//			"description": "written by the emulator, ignored on load"
//		},
//
//		"boot":
//		{
//			"biosPath": "",
//			"useCustomBootRom": true,
//			"skipBootAnimation": false,
//			"hleBios": true
//		},
//
//		...
//
//		"input":
//		{
//			"A": "X",
//			"B": "Z",
//			...
//			"SPEED": "Space"
//		}
//	}
//
// The layout is the project's settings layout: a tab per nesting level, one member per line, a
// blank line between the sections and a trailing newline. The writer below is therefore the GBA
// module's own (the shared engine serializes with the GameCube layout, two spaces and CRLF);
// ToJson() and DefaultJson() both go through it, so the shipped file can never drift from the code
// (a unit test in testing/gba_bench/test_settings.cpp compares them byte for byte).
//
// What the loader enforces
// ------------------------
// The engine validates the grammar, so this file only has to watch the shape and the values:
//
//   * a syntax problem (including a limit) rejects the whole document; the caller is left with the
//     defaults and the message names the line;
//   * an unknown section, or an unknown member of a known section, is reported through Log(Warn,
//     ...) and skipped: a file written by a newer frontend must not stop this build;
//   * a member whose value has the wrong type, or whose number is not a whole number, is reported
//     and that one member keeps its default - a hand edited file does not lose the whole
//     configuration over one mistyped value;
//   * a number outside the range of its member is clamped (a scale of 0 becomes 1, a volume of
//     250 becomes 100), never rejected and never wrapped;
//   * a rejected document is never half applied: Parse() leaves the caller's settings at the
//     defaults, and Load() reports the reason. A missing file is not an error at all (the defaults
//     are used and the first Save writes the file).
//
// The host key names
// ------------------
// A binding stores the SDL 2.0 key name (KeyFor/KeyBitFor/Bind in gba_settings.h) verbatim:
// "A".."Z", "0".."9", "F1".."F24", the named keys ("Up", "Down", "Left", "Right", "Return",
// "Backspace", "Space", "Tab", "Escape", "Left Shift", "Keypad 5", ...). The reader does not
// interpret them, so any name SDL has round trips; KeyBitFor() matches an event against the
// bindings. Two names match when they are equal, or when both are a single letter and equal
// ignoring case - that way a file written as "x" works with SDL's "X", while the multi-character
// names stay exact ("Up" does not match "up", which is not an SDL key name).

#include "gba_settings.h"
#include "gba_keypad.h"
#include "json.h"

#include <cerrno>
#include <cstdint>
#include <fstream>
#include <string>

using namespace GBA;			// the whole file is GBA's own settings plumbing

namespace
{
	// ---------------------------------------------------------------------------------------
	// The limits and the ranges
	// ---------------------------------------------------------------------------------------

	// The whole document. The shipped file is about 1 KByte; Load also refuses to read more than
	// this into memory, so a file that is not a settings file at all (a ROM renamed to .json) is
	// stopped while it is read. The token limits (string, number, nesting, element count) belong
	// to the Json engine, which enforces them while it scans.
	const size_t MaxDocumentBytes = 2 * 1024 * 1024;

	// The ranges of the numeric members. A number outside its range is clamped to the nearest end;
	// a number that is not a whole number at all is reported and the default is kept.
	const int MinScale = 1;
	const int MaxScale = 10;
	const int MinSampleRate = 8000;
	const int MaxSampleRate = 192000;
	const int MinVolume = 0;
	const int MaxVolume = 100;
	const int MinLinkPlayers = 2;
	const int MaxLinkPlayers = 4;
	const int MinLogLevel = 0;
	const int MaxLogLevel = 3;

	// The "info" section is documentation: it is written by every save and ignored by every load.
	const char* const Description =
		"The GBA emulator's configuration. These are the shipped defaults; a user's file is merged "
		"over them, so a member that is missing keeps its default.";

	// ---------------------------------------------------------------------------------------
	// The bindings
	// ---------------------------------------------------------------------------------------

	// The eleven actions of KeyBindingNames(), in the order the settings file lists them: the ten
	// keys of the GBA keypad (gba_keypad.h) followed by SPEED, the emulator's fast forward. The
	// two arrays are parallel and the same length; BindingNames is also the list the tests and the
	// frontend iterate, which is why it is the one with the nullptr terminator.
	const char* const BindingNames[] =
	{
		"A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN", "R", "L", "SPEED", nullptr
	};

	// The shipped binding of every action: comfortable on a keyboard (X/Z for the two buttons,
	// the arrows for the d-pad, Enter/Backspace for Start/Select, S/A for the shoulders and Space
	// for fast forward). These are the defaults Defaults() builds, and the keys the shipped
	// build/Data/GBASettings.json contains.
	const char* const DefaultKeys[] =
	{
		"X", "Z", "Backspace", "Return", "Right", "Left", "Up", "Down", "S", "A", "Space"
	};

	const int BindingCount = (int)(sizeof(DefaultKeys) / sizeof(DefaultKeys[0]));
	static_assert(sizeof(BindingNames) / sizeof(BindingNames[0]) == sizeof(DefaultKeys) / sizeof(DefaultKeys[0]) + 1,
		"BindingNames and DefaultKeys must describe the same eleven actions");

	// The actions that drive a keypad bit. SPEED is not in this table: it has no KEY_* bit, and
	// KeyBitFor() answers 0 for it.
	struct ActionBit
	{
		const char* action;
		uint16_t bit;
	};

	const ActionBit ActionBits[] =
	{
		{ "A", KEY_A },
		{ "B", KEY_B },
		{ "SELECT", KEY_SELECT },
		{ "START", KEY_START },
		{ "RIGHT", KEY_RIGHT },
		{ "LEFT", KEY_LEFT },
		{ "UP", KEY_UP },
		{ "DOWN", KEY_DOWN },
		{ "R", KEY_R },
		{ "L", KEY_L },
	};

	const int ActionBitCount = (int)(sizeof(ActionBits) / sizeof(ActionBits[0]));

	/// <summary>True when a host key name is a single letter (the names that ignore case).</summary>
	bool IsLetter(const std::string& name)
	{
		return name.size() == 1 &&
			((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z'));
	}

	/// <summary>Compare a host key name with a binding: exact, or one letter ignoring case.</summary>
	bool SameKeyName(const std::string& a, const std::string& b)
	{
		if (a == b)
			return true;
		if (!IsLetter(a) || !IsLetter(b))
			return false;
		char ca = a[0];
		char cb = b[0];
		if (ca >= 'a' && ca <= 'z')
			ca = (char)(ca - 'a' + 'A');
		if (cb >= 'a' && cb <= 'z')
			cb = (char)(cb - 'a' + 'A');
		return ca == cb;
	}

	/// <summary>Report a problem: to the log, and to the caller when it asked for the message.</summary>
	void Report(std::string* error, const std::string& message)
	{
		Log(LogLevel::Warn, "gba settings: %s", message.c_str());
		if (error != nullptr)
			*error = message;
	}

	// ---------------------------------------------------------------------------------------
	// The writer
	// ---------------------------------------------------------------------------------------

	/// <summary>A JSON string: quoted, with the characters JSON must escape escaped. The bytes
	/// above 0x7F are UTF-8 and are written through unchanged.</summary>
	std::string Escape(const std::string& value)
	{
		std::string text = "\"";
		for (char c : value)
		{
			unsigned char byte = (unsigned char)c;
			switch (c)
			{
			case '"': text += "\\\""; break;
			case '\\': text += "\\\\"; break;
			case '\b': text += "\\b"; break;
			case '\f': text += "\\f"; break;
			case '\n': text += "\\n"; break;
			case '\r': text += "\\r"; break;
			case '\t': text += "\\t"; break;
			default:
				if (byte < 0x20)
				{
					// The remaining control characters have no short escape: \u00XX.
					const char* hex = "0123456789abcdef";
					text += "\\u00";
					text += hex[(byte >> 4) & 0xF];
					text += hex[byte & 0xF];
				}
				else
				{
					text += c;
				}
				break;
			}
		}
		text += "\"";
		return text;
	}

	/// <summary>One member line of a section: two tabs, "name": value, a comma unless it is the
	/// last member. The name is escaped like any other string, because the names of the binding
	/// members come from the file (an unknown action is kept, see Bind).</summary>
	std::string Member(const char* name, const std::string& value, bool last)
	{
		std::string text = "\t\t";
		text += Escape(name);
		text += ": ";
		text += value;
		if (!last)
			text += ",";
		text += "\n";
		return text;
	}

	/// <summary>One section: the name, its braces, and a blank line after it unless it is last.</summary>
	std::string Section(const char* name, const std::string& members, bool last)
	{
		std::string text = "\t\"";
		text += name;
		text += "\":\n\t{\n";
		text += members;
		text += "\t}";
		text += last ? "\n" : ",\n\n";
		return text;
	}

	std::string Boolean(bool value)
	{
		return value ? "true" : "false";
	}

	std::string Number(int value)
	{
		return std::to_string(value);
	}

	/// <summary>True when `action` is one of the eleven names KeyBindingNames() lists.</summary>
	bool IsKnownAction(const std::string& action)
	{
		for (int i = 0; i < BindingCount; i++)
			if (action == BindingNames[i])
				return true;
		return false;
	}

	/// <summary>The bindings the writer emits: the eleven actions in KeyBindingNames() order,
	/// followed by the actions this build does not know (Bind appends those). An action that is
	/// missing from the settings is written with its default key, so a document always names all
	/// eleven and a hand edited file cannot lose a binding by writing it back.</summary>
	std::vector<GbaKeyBinding> BindingsForFile(const GbaSettings& settings)
	{
		std::vector<GbaKeyBinding> result;
		GbaSettings defaults = GbaSettings::Defaults();

		for (int i = 0; i < BindingCount; i++)
		{
			std::string action = BindingNames[i];
			bool written = false;
			for (const auto& binding : settings.keys)
			{
				if (binding.action != action)
					continue;
				result.push_back(binding);
				written = true;
				break;
			}
			if (!written)
				result.push_back(defaults.keys[(size_t)i]);
		}

		for (const auto& binding : settings.keys)
		{
			if (IsKnownAction(binding.action))
				continue;					// the eleven were written above, in their canonical order
			bool written = false;
			for (const auto& existing : result)
				if (existing.action == binding.action)
					written = true;
			if (!written)
				result.push_back(binding);
		}

		return result;
	}

	// ---------------------------------------------------------------------------------------
	// The reader
	// ---------------------------------------------------------------------------------------

	/// <summary>A member that must be a string; a mistyped member is reported and keeps its
	/// default. Json keeps the text wide and converts it to the project's UTF-8 on the way out.</summary>
	void AssignText(std::string& target, Json::Value* value, const char* where)
	{
		if (value->type != Json::ValueType::String)
		{
			Log(LogLevel::Warn, "gba settings: %s must be a string, the default is kept", where);
			return;
		}
		target = Json::WideToUtf8(value->value.AsString);
	}

	/// <summary>A member that must be true or false.</summary>
	void AssignBool(bool& target, Json::Value* value, const char* where)
	{
		if (value->type != Json::ValueType::Bool)
		{
			Log(LogLevel::Warn, "gba settings: %s must be true or false, the default %s is kept",
				where, target ? "true" : "false");
			return;
		}
		target = value->value.AsBool;
	}

	/// <summary>A member that must be a whole number inside [minimum, maximum]. A number outside
	/// the range is clamped to the nearest end; a number that is not a whole number at all keeps
	/// the default.</summary>
	void AssignInt(int& target, Json::Value* value, const char* where, int minimum, int maximum)
	{
		if (value->type == Json::ValueType::Float)
		{
			Log(LogLevel::Warn, "gba settings: %s is not a whole number, the default %i is kept", where, target);
			return;
		}
		if (value->type != Json::ValueType::Int)
		{
			Log(LogLevel::Warn, "gba settings: %s must be a number, the default %i is kept", where, target);
			return;
		}

		// Json keeps an integer in the two's complement form it already uses elsewhere, so the
		// saturated end of the range (a number that did not fit) clamps like any out of range one.
		int64_t number = (int64_t)value->value.AsInt;
		if (number < minimum || number > maximum)
		{
			int clamped = (int)(number < minimum ? minimum : maximum);
			Log(LogLevel::Warn, "gba settings: %s is %lld, outside %i..%i, clamped to %i",
				where, (long long)number, minimum, maximum, clamped);
			target = clamped;
			return;
		}
		target = (int)number;
	}

	/// <summary>Report a member of a known section that this build does not know.</summary>
	void UnknownMember(const char* section, const std::string& name)
	{
		Log(LogLevel::Warn, "gba settings: unknown member \"%s\" in section \"%s\" was skipped",
			name.c_str(), section);
	}

	void ReadBoot(Json::Value* section, GbaSettings& out)
	{
		for (Json::Value* member : section->children)
		{
			if (member->name == nullptr)
				continue;
			std::string name = member->name;
			if (name == "biosPath")
				AssignText(out.biosPath, member, "boot.biosPath");
			else if (name == "useCustomBootRom")
				AssignBool(out.useCustomBootRom, member, "boot.useCustomBootRom");
			else if (name == "skipBootAnimation")
				AssignBool(out.skipBootAnimation, member, "boot.skipBootAnimation");
			else if (name == "hleBios")
				AssignBool(out.hleBios, member, "boot.hleBios");
			else
				UnknownMember("boot", name);
		}
	}

	void ReadVideo(Json::Value* section, GbaSettings& out)
	{
		for (Json::Value* member : section->children)
		{
			if (member->name == nullptr)
				continue;
			std::string name = member->name;
			if (name == "videoScale")
				AssignInt(out.videoScale, member, "video.videoScale", MinScale, MaxScale);
			else if (name == "fullscreen")
				AssignBool(out.fullscreen, member, "video.fullscreen");
			else if (name == "vsync")
				AssignBool(out.vsync, member, "video.vsync");
			else if (name == "integerScale")
				AssignBool(out.integerScale, member, "video.integerScale");
			else if (name == "showFps")
				AssignBool(out.showFps, member, "video.showFps");
			else if (name == "frameSkip")
				AssignBool(out.frameSkip, member, "video.frameSkip");
			else
				UnknownMember("video", name);
		}
	}

	void ReadAudio(Json::Value* section, GbaSettings& out)
	{
		for (Json::Value* member : section->children)
		{
			if (member->name == nullptr)
				continue;
			std::string name = member->name;
			if (name == "audioEnabled")
				AssignBool(out.audioEnabled, member, "audio.audioEnabled");
			else if (name == "sampleRate")
				AssignInt(out.sampleRate, member, "audio.sampleRate", MinSampleRate, MaxSampleRate);
			else if (name == "volume")
				AssignInt(out.volume, member, "audio.volume", MinVolume, MaxVolume);
			else if (name == "highPassFilter")
				AssignBool(out.highPassFilter, member, "audio.highPassFilter");
			else
				UnknownMember("audio", name);
		}
	}

	void ReadInput(Json::Value* section, GbaSettings& out)
	{
		for (Json::Value* member : section->children)
		{
			if (member->name == nullptr)
				continue;

			// The member *is* the action ("A", "START", "SPEED", ...), the value is the host key
			// name. Bind replaces a known action in place and appends an action this build does
			// not know, so a binding a newer frontend wrote survives a load/save round trip.
			if (member->type != Json::ValueType::String)
			{
				Log(LogLevel::Warn,
					"gba settings: the binding of \"%s\" must be a key name (a string), the default is kept",
					member->name);
				continue;
			}

			out.Bind(member->name, Json::WideToUtf8(member->value.AsString));
		}
	}

	void ReadLink(Json::Value* section, GbaSettings& out)
	{
		for (Json::Value* member : section->children)
		{
			if (member->name == nullptr)
				continue;
			std::string name = member->name;
			if (name == "linkEnabled")
				AssignBool(out.linkEnabled, member, "link.linkEnabled");
			else if (name == "linkServer")
				AssignBool(out.linkServer, member, "link.linkServer");
			else if (name == "linkAddress")
				AssignText(out.linkAddress, member, "link.linkAddress");
			else if (name == "linkPlayers")
				AssignInt(out.linkPlayers, member, "link.linkPlayers", MinLinkPlayers, MaxLinkPlayers);
			else
				UnknownMember("link", name);
		}
	}

	void ReadEmulation(Json::Value* section, GbaSettings& out)
	{
		for (Json::Value* member : section->children)
		{
			if (member->name == nullptr)
				continue;
			std::string name = member->name;
			if (name == "rtcEnabled")
				AssignBool(out.rtcEnabled, member, "emulation.rtcEnabled");
			else if (name == "bootWithNoCartridge")
				AssignBool(out.bootWithNoCartridge, member, "emulation.bootWithNoCartridge");
			else if (name == "debugger")
				AssignBool(out.debugger, member, "emulation.debugger");
			else if (name == "saveDirectory")
				AssignText(out.saveDirectory, member, "emulation.saveDirectory");
			else if (name == "logLevel")
				AssignInt(out.logLevel, member, "emulation.logLevel", MinLogLevel, MaxLogLevel);
			else
				UnknownMember("emulation", name);
		}
	}

	/// <summary>Apply one section of the document: a known section is applied, an unknown one is
	/// reported and skipped.</summary>
	void ReadSection(Json::Value* section, GbaSettings& out)
	{
		if (section->name == nullptr)
			return;

		std::string name = section->name;

		if (name == "info")
			return;						// documentation only: written by Save, ignored here

		if (section->type != Json::ValueType::Object)
		{
			// The syntax is valid, but the section is not an object: a mistyped section, not a
			// broken document. Every member of it keeps its default.
			Log(LogLevel::Warn, "gba settings: section \"%s\" must be an object, it was skipped", name.c_str());
			return;
		}

		if (name == "boot")
			ReadBoot(section, out);
		else if (name == "video")
			ReadVideo(section, out);
		else if (name == "audio")
			ReadAudio(section, out);
		else if (name == "input")
			ReadInput(section, out);
		else if (name == "link")
			ReadLink(section, out);
		else if (name == "emulation")
			ReadEmulation(section, out);
		else
			Log(LogLevel::Warn, "gba settings: unknown section \"%s\" was skipped", name.c_str());
	}

	/// <summary>Apply a document the Json engine has already accepted. The root is an object
	/// (Parse checks that) and every section is applied in the order the document lists it.</summary>
	void ReadDocument(Json::Value* root, GbaSettings& out)
	{
		for (Json::Value* section : root->children)
		{
			ReadSection(section, out);
		}
	}

}

namespace GBA
{
	const char* const* KeyBindingNames()
	{
		return BindingNames;
	}

	GbaSettings GbaSettings::Defaults()
	{
		// The scalar members are the header's member initializers, so the header stays the one
		// place a scalar default is written down. Only the bindings are built here: they are the
		// eleven entries of the two parallel tables at the top of this file.
		GbaSettings settings;
		settings.keys.reserve(BindingCount);
		for (int i = 0; i < BindingCount; i++)
			settings.keys.push_back(GbaKeyBinding{ BindingNames[i], DefaultKeys[i] });
		return settings;
	}

	std::string GbaSettings::KeyFor(const std::string& action) const
	{
		for (const auto& binding : keys)
			if (binding.action == action)
				return binding.key;
		return std::string();
	}

	uint16_t GbaSettings::KeyBitFor(const std::string& key) const
	{
		if (key.empty())
			return 0;					// no key event has an empty name: "" means "unbound"

		// The first binding that names the key decides, even when that action has no keypad bit:
		// a key that is bound twice (a hand edited file) drives the binding the file lists first.
		for (const auto& binding : keys)
		{
			if (!SameKeyName(binding.key, key))
				continue;
			for (int i = 0; i < ActionBitCount; i++)
				if (binding.action == ActionBits[i].action)
					return ActionBits[i].bit;
			return 0;					// SPEED, or an action this build does not know
		}
		return 0;
	}

	void GbaSettings::Bind(const std::string& action, const std::string& key)
	{
		if (action.empty())
		{
			// An empty action name cannot be addressed by the UI or by the file: there is nothing
			// to bind and appending it would write a nameless member.
			Log(LogLevel::Warn, "gba settings: cannot bind the empty action name");
			return;
		}

		for (auto& binding : keys)
		{
			if (binding.action != action)
				continue;
			binding.key = key;			// an empty key name unbinds the action
			return;
		}

		// An action this build does not know is appended, not rejected: a binding that a newer
		// frontend wrote is kept and written back instead of being silently dropped. ToJson writes
		// the eleven known actions first, in KeyBindingNames() order, and the appended ones after.
		Log(LogLevel::Warn, "gba settings: appended the unknown binding action \"%s\"", action.c_str());
		keys.push_back(GbaKeyBinding{ action, key });
	}

	std::string GbaSettings::ToJson() const
	{
		// The project's settings layout, in the section order the header documents. Every member of
		// GbaSettings is here, named exactly as the header names it, and nothing else (the "info"
		// section is the one exception: it describes the file and has no member in the struct).
		std::string text = "{\n";

		text += Section("info", Member("description", Escape(Description), true), false);

		text += Section("boot",
			Member("biosPath", Escape(biosPath), false) +
			Member("useCustomBootRom", Boolean(useCustomBootRom), false) +
			Member("skipBootAnimation", Boolean(skipBootAnimation), false) +
			Member("hleBios", Boolean(hleBios), true), false);

		text += Section("video",
			Member("videoScale", Number(videoScale), false) +
			Member("fullscreen", Boolean(fullscreen), false) +
			Member("vsync", Boolean(vsync), false) +
			Member("integerScale", Boolean(integerScale), false) +
			Member("showFps", Boolean(showFps), false) +
			Member("frameSkip", Boolean(frameSkip), true), false);

		text += Section("audio",
			Member("audioEnabled", Boolean(audioEnabled), false) +
			Member("sampleRate", Number(sampleRate), false) +
			Member("volume", Number(volume), false) +
			Member("highPassFilter", Boolean(highPassFilter), true), false);

		std::vector<GbaKeyBinding> bindings = BindingsForFile(*this);
		std::string members;
		for (size_t i = 0; i < bindings.size(); i++)
			members += Member(bindings[i].action.c_str(), Escape(bindings[i].key), i + 1 == bindings.size());
		text += Section("input", members, false);

		text += Section("link",
			Member("linkEnabled", Boolean(linkEnabled), false) +
			Member("linkServer", Boolean(linkServer), false) +
			Member("linkAddress", Escape(linkAddress), false) +
			Member("linkPlayers", Number(linkPlayers), true), false);

		text += Section("emulation",
			Member("rtcEnabled", Boolean(rtcEnabled), false) +
			Member("bootWithNoCartridge", Boolean(bootWithNoCartridge), false) +
			Member("debugger", Boolean(debugger), false) +
			Member("saveDirectory", Escape(saveDirectory), false) +
			Member("logLevel", Number(logLevel), true), true);

		text += "}\n";
		return text;
	}

	std::string GbaSettings::DefaultJson()
	{
		// The shipped document is the defaults written by the writer above, so the file in
		// build/Data and the code cannot drift apart (the unit test compares them byte for byte).
		return Defaults().ToJson();
	}

	bool GbaSettings::Parse(const std::string& text, GbaSettings& out, std::string* error)
	{
		if (error != nullptr)
			error->clear();

		// The document comes from the user's directory, so the size is checked before the parser
		// sees it: a file that is not a settings file at all is refused here.
		if (text.size() > MaxDocumentBytes)
		{
			out = Defaults();
			Report(error, "1: the document is larger than 2097152 bytes");
			return false;
		}

		Json json;

		try
		{
			json.Deserialize((void*)text.data(), text.size());
		}
		catch (const char* message)
		{
			// A rejected document is never half applied: the caller gets the defaults.
			out = Defaults();
			int line = json.GetErrorLine();
			if (line <= 0)
				line = 1;
			Report(error, std::to_string(line) + ": " + message);
			return false;
		}
		catch (...)
		{
			// Anything the engine throws that is not one of its messages (an allocation failure,
			// a broken invariant): the document is refused, the caller keeps the defaults.
			out = Defaults();
			Report(error, "1: the document is not a valid Json document");
			return false;
		}

		// The document is a set of sections, so its root has to be an object. The engine accepts
		// any value at the top level (the emulator's other documents do too), so the shape is
		// checked here.
		if (json.root.children.empty() || json.root.children.back()->type != Json::ValueType::Object)
		{
			out = Defaults();
			Report(error, "1: the document must be an object");
			return false;
		}

		GbaSettings parsed = Defaults();
		ReadDocument(json.root.children.back(), parsed);
		out = parsed;
		return true;
	}

	bool GbaSettings::Load(const std::string& path, GbaSettings& out, std::string* error)
	{
		if (error != nullptr)
			error->clear();

		// Whatever happens below, the caller is left with a configuration it can run.
		out = Defaults();

		errno = 0;
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
		{
			// A file that does not exist yet is the normal first run, not an error: the defaults are
			// used and Save writes the file back on the first change. Every other reason (a
			// permission, a path that is not a file) has to be reported, because silently defaulting
			// would look exactly like a first run.
			if (errno == ENOENT || errno == ENOTDIR)
			{
				Log(LogLevel::Info, "gba settings: no settings file at %s, the defaults are used", path.c_str());
				return true;
			}
			Report(error, path + ": cannot open the settings file");
			return false;
		}

		// The document is read with the reader's own limit, so a file that is not a settings file at
		// all is refused while it is read instead of being held in memory first.
		std::string text;
		char buffer[4096];
		while (true)
		{
			file.read(buffer, sizeof(buffer));
			std::streamsize got = file.gcount();
			if (got > 0)
				text.append(buffer, (size_t)got);
			if (text.size() > MaxDocumentBytes)
			{
				Report(error, path + ": the settings file is larger than 2097152 bytes");
				return false;
			}
			if (got < (std::streamsize)sizeof(buffer))
				break;
		}
		if (file.bad() || (file.fail() && !file.eof()))
		{
			// A directory is what usually lands here: Linux lets it be opened but not read. It is
			// reported as a broken settings file, never mistaken for an empty one.
			Report(error, path + ": cannot read the settings file");
			return false;
		}

		std::string reason;
		if (!Parse(text, out, &reason))
		{
			// Parse already put the defaults in place; the message says which line of which file.
			Report(error, path + ": " + reason);
			return false;
		}

		return true;
	}

	bool GbaSettings::Save(const std::string& path, std::string* error) const
	{
		if (error != nullptr)
			error->clear();

		std::string text = ToJson();

		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		if (!file.is_open())
		{
			Report(error, path + ": cannot write the settings file");
			return false;
		}

		file.write(text.data(), (std::streamsize)text.size());
		file.close();
		if (!file)
		{
			// A full disk, a read-only directory or a failing network share: the caller has to know
			// that the file on disk is not the configuration it thinks it saved.
			Report(error, path + ": cannot write the settings file");
			return false;
		}

		return true;
	}
}
