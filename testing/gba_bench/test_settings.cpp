// The GBA settings file: build/Data/GBASettings.json.
//
// The tests here are the only place where the shipped document, the reader and the writer are
// compared with each other, so the file can never drift from the code and a malformed document is
// rejected instead of taking the emulator down. The malformed corpus is the one the security
// review asks of every reader on an input path (wiki/security.md): a truncated document, missing
// separators, an unterminated token, an oversized string, an oversized number and a document that
// nests deeper than the reader allows. Every one of them must be reported and must not crash, spin
// or exhaust the stack.
//
// The settings file is read from the repository (the WSL mount of the working copy), so the tests
// work from any working directory; the files these tests write land in /tmp.

#include "gba_test.h"
#include "gba_settings.h"
#include "gba_keypad.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace GBA;

namespace
{
	// The shipped document, at its place in the repository.
	const char* const ShippedPath = "/mnt/c/Work/pureikyubu/build/Data/GBASettings.json";

	// Where the tests that have to write a settings file write it.
	const char* const ScratchPath = "/tmp/gba_settings_test.json";
	const char* const CorruptPath = "/tmp/gba_settings_corrupt.json";
	const char* const MissingPath = "/tmp/gba_settings_does_not_exist.json";

	/// <summary>The whole file as a string (the tests compare documents byte for byte).</summary>
	std::string ReadFile(const std::string& path)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
			GBA_FAIL("cannot read " + path);

		std::string text;
		char buffer[4096];
		while (true)
		{
			file.read(buffer, sizeof(buffer));
			std::streamsize got = file.gcount();
			if (got > 0)
				text.append(buffer, (size_t)got);
			if (got < (std::streamsize)sizeof(buffer))
				break;
		}
		if (file.bad() || (file.fail() && !file.eof()))
			GBA_FAIL("cannot read " + path);
		return text;
	}

	/// <summary>The line of `text` that contains `at`, for a readable difference report.</summary>
	std::string LineOf(const std::string& text, size_t at)
	{
		if (at > text.size())
			at = text.size();
		size_t begin = at;
		while (begin > 0 && text[begin - 1] != '\n')
			begin--;
		size_t end = at;
		while (end < text.size() && text[end] != '\n')
			end++;
		return text.substr(begin, end - begin);
	}

	/// <summary>Where two documents first disagree. "They differ" alone is not actionable, so the
	/// byte offset and both lines are reported.</summary>
	std::string Difference(const std::string& expected, const std::string& actual)
	{
		size_t limit = expected.size() < actual.size() ? expected.size() : actual.size();
		size_t at = 0;
		while (at < limit && expected[at] == actual[at])
			at++;

		std::string message = "the documents differ at byte " + std::to_string(at);
		message += " (expected " + std::to_string(expected.size()) + " bytes, got " + std::to_string(actual.size()) + ")";
		message += "\n       expected: " + LineOf(expected, at);
		message += "\n       actual:   " + LineOf(actual, at);
		return message;
	}

	/// <summary>Compare two settings member by member (the bindings included).</summary>
	void CheckSame(const GbaSettings& actual, const GbaSettings& expected, const std::string& what)
	{
		GBA_CHECK_MSG(actual.biosPath == expected.biosPath, what + ": biosPath");
		GBA_CHECK_MSG(actual.useCustomBootRom == expected.useCustomBootRom, what + ": useCustomBootRom");
		GBA_CHECK_MSG(actual.skipBootAnimation == expected.skipBootAnimation, what + ": skipBootAnimation");
		GBA_CHECK_MSG(actual.hleBios == expected.hleBios, what + ": hleBios");

		GBA_CHECK_MSG(actual.videoScale == expected.videoScale, what + ": videoScale");
		GBA_CHECK_MSG(actual.fullscreen == expected.fullscreen, what + ": fullscreen");
		GBA_CHECK_MSG(actual.vsync == expected.vsync, what + ": vsync");
		GBA_CHECK_MSG(actual.integerScale == expected.integerScale, what + ": integerScale");
		GBA_CHECK_MSG(actual.showFps == expected.showFps, what + ": showFps");
		GBA_CHECK_MSG(actual.frameSkip == expected.frameSkip, what + ": frameSkip");

		GBA_CHECK_MSG(actual.audioEnabled == expected.audioEnabled, what + ": audioEnabled");
		GBA_CHECK_MSG(actual.sampleRate == expected.sampleRate, what + ": sampleRate");
		GBA_CHECK_MSG(actual.volume == expected.volume, what + ": volume");

		GBA_CHECK_MSG(actual.linkEnabled == expected.linkEnabled, what + ": linkEnabled");
		GBA_CHECK_MSG(actual.linkServer == expected.linkServer, what + ": linkServer");
		GBA_CHECK_MSG(actual.linkAddress == expected.linkAddress, what + ": linkAddress");
		GBA_CHECK_MSG(actual.linkPlayers == expected.linkPlayers, what + ": linkPlayers");

		GBA_CHECK_MSG(actual.rtcEnabled == expected.rtcEnabled, what + ": rtcEnabled");
		GBA_CHECK_MSG(actual.bootWithNoCartridge == expected.bootWithNoCartridge, what + ": bootWithNoCartridge");
		GBA_CHECK_MSG(actual.debugger == expected.debugger, what + ": debugger");
		GBA_CHECK_MSG(actual.saveDirectory == expected.saveDirectory, what + ": saveDirectory");
		GBA_CHECK_MSG(actual.logLevel == expected.logLevel, what + ": logLevel");

		GBA_CHECK_MSG(actual.keys.size() == expected.keys.size(), what + ": key binding count");
		for (size_t i = 0; i < actual.keys.size() && i < expected.keys.size(); i++)
		{
			GBA_CHECK_MSG(actual.keys[i].action == expected.keys[i].action,
				what + ": binding " + std::to_string(i) + " action");
			GBA_CHECK_MSG(actual.keys[i].key == expected.keys[i].key,
				what + ": binding " + std::to_string(i) + " (" + actual.keys[i].action + ") key");
		}
	}
}

// -------------------------------------------------------------------------------------------
// The defaults and the round trip
// -------------------------------------------------------------------------------------------

GBA_TEST(Settings, Defaults)
{
	// The scalar defaults are the header's member initializers: this test is what notices a
	// changed initializer, and the shipped file is what notices a changed binding.
	GbaSettings settings = GbaSettings::Defaults();

	GBA_CHECK(settings.biosPath.empty());
	GBA_CHECK(settings.useCustomBootRom);
	GBA_CHECK(!settings.skipBootAnimation);
	GBA_CHECK(settings.hleBios);

	GBA_CHECK_EQ(settings.videoScale, 3);
	GBA_CHECK(!settings.fullscreen);
	GBA_CHECK(settings.vsync);
	GBA_CHECK(settings.integerScale);
	GBA_CHECK(settings.showFps);
	GBA_CHECK(!settings.frameSkip);

	GBA_CHECK(settings.audioEnabled);
	GBA_CHECK_EQ(settings.sampleRate, 32768);
	GBA_CHECK_EQ(settings.volume, 100);

	GBA_CHECK(!settings.linkEnabled);
	GBA_CHECK(!settings.linkServer);
	GBA_CHECK(settings.linkAddress == "127.0.0.1:33333");
	GBA_CHECK_EQ(settings.linkPlayers, 2);

	GBA_CHECK(settings.rtcEnabled);
	GBA_CHECK(!settings.bootWithNoCartridge);
	GBA_CHECK(!settings.debugger);			// the debugger window does not open by itself
	GBA_CHECK(settings.saveDirectory.empty());
	GBA_CHECK_EQ(settings.logLevel, 1);

	GBA_CHECK_EQ(settings.keys.size(), (size_t)11);
}

GBA_TEST(Settings, RoundTripDefaults)
{
	// defaults -> text -> parse -> the same values, member by member.
	GbaSettings settings = GbaSettings::Defaults();
	std::string text = settings.ToJson();

	GBA_CHECK_MSG(text == GbaSettings::DefaultJson(),
		"ToJson() of the defaults must be the shipped document:\n" + Difference(GbaSettings::DefaultJson(), text));

	GbaSettings parsed;
	std::string error = "not cleared";
	GBA_CHECK_MSG(GbaSettings::Parse(text, parsed, &error), "the defaults document must parse: " + error);
	GBA_CHECK_MSG(error.empty(), "a valid document leaves no error: " + error);
	CheckSame(parsed, settings, "defaults round trip");

	// The values survive a second trip through the writer unchanged.
	GBA_CHECK(parsed.ToJson() == text);
}

GBA_TEST(Settings, ShippedFileIsDefaultJson)
{
	// The shipped document and DefaultJson() are the same bytes: the file can never drift from the
	// code, which is what the harness (and the frontend that ships the file) relies on.
	std::string shipped = ReadFile(ShippedPath);
	std::string generated = GbaSettings::DefaultJson();
	if (shipped != generated)
		GBA_FAIL(Difference(generated, shipped));
}

GBA_TEST(Settings, ShippedFileRoundTrips)
{
	// Load the shipped file, save it, and get byte identical text back.
	GbaSettings settings;
	std::string error;
	GBA_CHECK_MSG(GbaSettings::Load(ShippedPath, settings, &error), "the shipped file must load: " + error);
	GBA_CHECK_MSG(error.empty(), "loading the shipped file is not an error: " + error);
	CheckSame(settings, GbaSettings::Defaults(), "the shipped file is the defaults");

	GBA_CHECK_MSG(settings.Save(ScratchPath, &error), "cannot save: " + error);
	std::string written = ReadFile(ScratchPath);
	if (written != ReadFile(ShippedPath))
		GBA_FAIL(Difference(ReadFile(ShippedPath), written));
	std::remove(ScratchPath);
}

GBA_TEST(Settings, SaveAndLoad)
{
	// Every member changed, including a path with a backslash (the writer escapes it and the
	// reader has to unescape it again), saved and loaded back.
	GbaSettings settings = GbaSettings::Defaults();
	settings.biosPath = "C:\\bios\\gba.bin";
	settings.useCustomBootRom = false;
	settings.skipBootAnimation = true;
	settings.hleBios = false;
	settings.videoScale = 4;
	settings.fullscreen = true;
	settings.vsync = false;
	settings.integerScale = false;
	settings.showFps = false;
	settings.frameSkip = true;
	settings.audioEnabled = false;
	settings.sampleRate = 48000;
	settings.volume = 42;
	settings.linkEnabled = true;
	settings.linkServer = true;
	settings.linkAddress = "10.0.0.2:33333";
	settings.linkPlayers = 4;
	settings.rtcEnabled = false;
	settings.bootWithNoCartridge = true;
	settings.debugger = true;
	settings.saveDirectory = "/home/me/gba saves";
	settings.logLevel = 3;
	settings.Bind("A", "Return");
	settings.Bind("SPEED", "Tab");

	std::string error;
	GBA_CHECK_MSG(settings.Save(ScratchPath, &error), "cannot save: " + error);
	GBA_CHECK_MSG(ReadFile(ScratchPath) == settings.ToJson(), "a saved file is exactly ToJson()");

	GbaSettings loaded;
	GBA_CHECK_MSG(GbaSettings::Load(ScratchPath, loaded, &error), "cannot load: " + error);
	CheckSame(loaded, settings, "a modified settings round trip");

	std::remove(ScratchPath);
}

// -------------------------------------------------------------------------------------------
// The bindings
// -------------------------------------------------------------------------------------------

GBA_TEST(Settings, KeyBindingNames)
{
	// The eleven actions, in the order KeyBindingNames() lists them in and the file writes them
	// in. The list is the GBA's ten keypad keys plus SPEED (the emulator's fast forward).
	const char* const expectedNames[11] =
	{
		"A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN", "R", "L", "SPEED"
	};
	const char* const expectedKeys[11] =
	{
		"X", "Z", "Backspace", "Return", "Right", "Left", "Up", "Down", "S", "A", "Space"
	};

	const char* const* names = KeyBindingNames();
	GBA_CHECK(names != nullptr);

	int count = 0;
	while (names[count] != nullptr)
		count++;
	GBA_CHECK_EQ(count, 11);
	GBA_CHECK_EQ(count, (int)(sizeof(expectedNames) / sizeof(expectedNames[0])));

	GbaSettings settings = GbaSettings::Defaults();
	GBA_CHECK_EQ(settings.keys.size(), (size_t)count);

	for (int i = 0; i < count; i++)
	{
		GBA_CHECK_MSG(std::string(names[i]) == expectedNames[i],
			std::string("the name at index ") + std::to_string(i) + " is " + names[i]);
		GBA_CHECK_MSG(settings.keys[(size_t)i].action == expectedNames[i],
			std::string("the binding at index ") + std::to_string(i) + " is " + settings.keys[(size_t)i].action);
		GBA_CHECK_MSG(settings.keys[(size_t)i].key == expectedKeys[i],
			std::string("the default key of ") + expectedNames[i] + " must be " + expectedKeys[i]);
		GBA_CHECK_MSG(settings.KeyFor(expectedNames[i]) == expectedKeys[i],
			std::string("KeyFor(") + expectedNames[i] + ")");
	}

	// The bindings are distinct, and an action that does not exist is not bound.
	for (int i = 0; i < count; i++)
		for (int j = i + 1; j < count; j++)
			GBA_CHECK_MSG(settings.keys[(size_t)i].key != settings.keys[(size_t)j].key,
				std::string("two actions use the same default key: ") + expectedNames[i] + " and " + expectedNames[j]);

	GBA_CHECK(settings.KeyFor("TURBO").empty());
	GBA_CHECK(settings.KeyFor("").empty());
	GBA_CHECK(settings.KeyFor("a").empty());			// the action names are exact
}

GBA_TEST(Settings, KeyBitFor)
{
	GbaSettings settings = GbaSettings::Defaults();

	GBA_CHECK_EQ(settings.KeyBitFor("X"), KEY_A);
	GBA_CHECK_EQ(settings.KeyBitFor("x"), KEY_A);		// one letter matches ignoring case
	GBA_CHECK_EQ(settings.KeyBitFor("Z"), KEY_B);
	GBA_CHECK_EQ(settings.KeyBitFor("A"), KEY_L);		// the "A" key is the L shoulder
	GBA_CHECK_EQ(settings.KeyBitFor("S"), KEY_R);
	GBA_CHECK_EQ(settings.KeyBitFor("Up"), KEY_UP);
	GBA_CHECK_EQ(settings.KeyBitFor("Down"), KEY_DOWN);
	GBA_CHECK_EQ(settings.KeyBitFor("Left"), KEY_LEFT);
	GBA_CHECK_EQ(settings.KeyBitFor("Right"), KEY_RIGHT);
	GBA_CHECK_EQ(settings.KeyBitFor("Return"), KEY_START);
	GBA_CHECK_EQ(settings.KeyBitFor("Backspace"), KEY_SELECT);

	GBA_CHECK_EQ(settings.KeyBitFor("up"), 0);			// a named key is exact
	GBA_CHECK_EQ(settings.KeyBitFor("return"), 0);
	GBA_CHECK_EQ(settings.KeyBitFor("Space"), 0);		// SPEED has no keypad bit
	GBA_CHECK_EQ(settings.KeyBitFor(""), 0);
	GBA_CHECK_EQ(settings.KeyBitFor("F1"), 0);

	// The event follows the binding: rebinding a key moves its bit.
	settings.Bind("A", "F1");
	GBA_CHECK_EQ(settings.KeyBitFor("F1"), KEY_A);
	GBA_CHECK_EQ(settings.KeyBitFor("X"), 0);
	GBA_CHECK_EQ(settings.KeyBitFor("A"), KEY_L);

	// An unbound action answers no key at all.
	settings.Bind("B", "");
	GBA_CHECK(settings.KeyFor("B").empty());
	GBA_CHECK_EQ(settings.KeyBitFor("Z"), 0);
}

GBA_TEST(Settings, Bind)
{
	GbaSettings settings = GbaSettings::Defaults();

	// A known action is replaced in place: the eleven keep their order.
	settings.Bind("A", "Q");
	GBA_CHECK_EQ(settings.keys.size(), (size_t)11);
	GBA_CHECK(settings.KeyFor("A") == "Q");
	GBA_CHECK(settings.keys[0].action == "A");

	// An action this build does not know is appended rather than rejected: a binding that a newer
	// frontend wrote has to survive a load/save round trip instead of being silently dropped.
	settings.Bind("TURBO", "F2");
	GBA_CHECK_EQ(settings.keys.size(), (size_t)12);
	GBA_CHECK(settings.KeyFor("TURBO") == "F2");
	GBA_CHECK(settings.keys[11].action == "TURBO");
	GBA_CHECK_EQ(settings.KeyBitFor("F2"), 0);			// no KEY_* bit belongs to it

	// The empty action name cannot be addressed by the UI nor written to the file.
	settings.Bind("", "F3");
	GBA_CHECK_EQ(settings.keys.size(), (size_t)12);
	GBA_CHECK(settings.KeyFor("").empty());

	// Both the replaced and the appended binding survive the writer and the reader.
	std::string error;
	GbaSettings parsed;
	GBA_CHECK_MSG(GbaSettings::Parse(settings.ToJson(), parsed, &error), "cannot parse: " + error);
	GBA_CHECK_EQ(parsed.keys.size(), (size_t)12);
	GBA_CHECK(parsed.KeyFor("A") == "Q");
	GBA_CHECK(parsed.KeyFor("TURBO") == "F2");
	GBA_CHECK(parsed.KeyFor("START") == "Return");
	GBA_CHECK(parsed.keys[11].action == "TURBO");

	// A name that needs escaping cannot break the document: the writer escapes a member name as
	// well as a member value, so a hand edited (or hostile) action name still round trips.
	GbaSettings escaped = GbaSettings::Defaults();
	escaped.Bind("A\"B\\C", "F4");
	GBA_CHECK_MSG(GbaSettings::Parse(escaped.ToJson(), parsed, &error),
		"an action name with a quote and a backslash must round trip: " + error);
	GBA_CHECK(parsed.KeyFor("A\"B\\C") == "F4");
}

// -------------------------------------------------------------------------------------------
// The documents that must be refused
// -------------------------------------------------------------------------------------------

GBA_TEST(Settings, MalformedDocuments)
{
	// Every one of these has to be rejected with a message that starts with the line, and none of
	// them may crash, spin or leave a half applied configuration behind.
	const char* const documents[] =
	{
		"",												// an empty file
		"   \n\t",										// only whitespace
		"{",											// truncated right after the brace
		"{\"boot\": {",									// truncated inside a section
		"{\"boot\" {}}",								// a missing ':'
		"{\"boot\": {} \"video\": {}}",					// a missing ','
		"{\"boot\": {},}",								// a trailing comma
		"{\"video\": {\"videoScale\": 05}}",			// a leading zero in a number
		"{\"video\": {\"videoScale\": +3}}",			// a sign JSON does not have
		"{\"boot\": {\"biosPath\": \"unterminated}}",	// a string that never ends
		"{\"boot\": {\"biosPath\": \"a\nb\"}}",			// a raw newline inside a string
		"{\"boot\": {\"biosPath\": \"\\q\"}}",			// an unknown escape
		"{\"info\": [1, 2, 3}",							// an unterminated array
		"{\"info\": [1 2]}",							// a missing ',' in an array
		"{\"boot\": tru}",								// a broken literal
		"{\"boot\": }",									// a missing value
		"{\"boot\": {}} {\"video\": {}}",				// text after the document
		"[]",											// not an object
		"null",
	};

	for (const char* document : documents)
	{
		GbaSettings settings = GbaSettings::Defaults();
		std::string error;
		std::string what = std::string("the document must be rejected: \"") + document + "\"";
		GBA_CHECK_MSG(!GbaSettings::Parse(document, settings, &error), what);
		GBA_CHECK_MSG(!error.empty(), what + " (a rejection has to explain itself)");
		// The message names the line first, like the other readers in the project.
		GBA_CHECK_MSG(error.find_first_of("0123456789") == 0, "the message starts with a line number: " + error);

		// Nothing of a rejected document may be applied.
		CheckSame(settings, GbaSettings::Defaults(), what + " (the defaults are what is left)");
	}
}

GBA_TEST(Settings, OversizedInputs)
{
	// A 1 MByte string: it is refused while the token is scanned, so it is never copied into the
	// settings (the allocation in this test is the only one).
	{
		std::string text = "{\"info\": {\"description\": \"";
		text.append(1024 * 1024, 'a');
		text += "\"}}";

		GbaSettings settings;
		std::string error;
		GBA_CHECK_MSG(!GbaSettings::Parse(text, settings, &error), "a 1 MByte string must be rejected");
		GBA_CHECK_MSG(error.find("string") != std::string::npos, "the message names the string limit: " + error);
	}

	// The string limit itself: 4096 bytes are accepted, 4097 are not.
	{
		GbaSettings settings;
		std::string error;
		std::string text = "{\"boot\": {\"biosPath\": \"" + std::string(4096, 'b') + "\"}}";
		GBA_CHECK_MSG(GbaSettings::Parse(text, settings, &error), "a 4096 byte string is accepted: " + error);
		GBA_CHECK_EQ(settings.biosPath.size(), (size_t)4096);

		text = "{\"boot\": {\"biosPath\": \"" + std::string(4097, 'b') + "\"}}";
		GBA_CHECK_MSG(!GbaSettings::Parse(text, settings, &error), "a 4097 byte string must be rejected");
	}

	// A number with 500 digits: the token is scanned (no allocation) and refused before anything
	// is converted, so it can neither overflow nor wrap.
	{
		std::string text = "{\"audio\": {\"volume\": ";
		text.append(500, '9');
		text += "}}";

		GbaSettings settings;
		std::string error;
		GBA_CHECK_MSG(!GbaSettings::Parse(text, settings, &error), "a 500 digit number must be rejected");
		GBA_CHECK_MSG(error.find("number") != std::string::npos, "the message names the number limit: " + error);
	}

	// 100000 nested objects: the depth limit rejects the document long before the recursion in the
	// reader could exhaust the stack. (The document is 600 KByte, below the document limit, so the
	// depth limit is what refuses it.)
	{
		std::string text;
		text.reserve(700000);
		for (int i = 0; i < 100000; i++)
			text += "{\"a\":";
		text += "1";
		for (int i = 0; i < 100001; i++)
			text += "}";

		GbaSettings settings;
		std::string error;
		GBA_CHECK_MSG(!GbaSettings::Parse(text, settings, &error), "100000 nested objects must be rejected");
		GBA_CHECK_MSG(error.find("nested") != std::string::npos, "the message names the depth limit: " + error);
		CheckSame(settings, GbaSettings::Defaults(), "the deep document left the defaults");
	}
}

// -------------------------------------------------------------------------------------------
// The documents that must be accepted
// -------------------------------------------------------------------------------------------

GBA_TEST(Settings, Encodings)
{
	// A UTF-8 BOM (Notepad writes one) and CRLF line endings are both accepted, and both give the
	// same values as the LF form.
	const std::string lf = "{\n\t\"video\":\n\t{\n\t\t\"videoScale\": 5,\n\t\t\"vsync\": false\n\t}\n}\n";
	const std::string crlf = "{\r\n\t\"video\":\r\n\t{\r\n\t\t\"videoScale\": 5,\r\n\t\t\"vsync\": false\r\n\t}\r\n}\r\n";
	const std::string bom = "\xEF\xBB\xBF" + lf;

	const std::string documents[3] = { lf, crlf, bom };
	for (int i = 0; i < 3; i++)
	{
		GbaSettings settings;
		std::string error;
		GBA_CHECK_MSG(GbaSettings::Parse(documents[i], settings, &error),
			std::string("document ") + std::to_string(i) + " must parse: " + error);
		GBA_CHECK_EQ(settings.videoScale, 5);
		GBA_CHECK(!settings.vsync);
		GBA_CHECK_EQ(settings.volume, 100);		// everything else is still the default
	}

	// A BOM is only allowed at the very start: in the middle it is not a JSON token.
	GbaSettings settings;
	std::string error;
	GBA_CHECK(!GbaSettings::Parse("{\"boot\": \xEF\xBB\xBF{}}", settings, &error));
}

GBA_TEST(Settings, MissingMembers)
{
	// A document that names the sections but none of the members leaves every default in place.
	const char* const documents[] =
	{
		"{}",
		"{\"info\": {}}",
		"{\"boot\": {}, \"video\": {}, \"audio\": {}, \"input\": {}, \"link\": {}, \"emulation\": {}}",
		"{\"boot\": {}}\n\n",							// whitespace around and after the document
	};

	for (const char* document : documents)
	{
		GbaSettings settings;
		std::string error;
		GBA_CHECK_MSG(GbaSettings::Parse(document, settings, &error),
			std::string("must parse: ") + document + " (" + error + ")");
		GBA_CHECK_MSG(error.empty(), std::string("a valid document leaves no error: ") + error);
		CheckSame(settings, GbaSettings::Defaults(), std::string("the defaults are kept: ") + document);
	}

	// A partial document: what it names is applied, what it omits keeps its default.
	GbaSettings settings;
	std::string error;
	GBA_CHECK_MSG(GbaSettings::Parse("{\"video\": {\"videoScale\": 7}, \"audio\": {\"volume\": 40}}", settings, &error),
		"a partial document must parse: " + error);

	GbaSettings expected = GbaSettings::Defaults();
	expected.videoScale = 7;
	expected.volume = 40;
	CheckSame(settings, expected, "the two named members and nothing else");

	// A member that is named twice is applied twice: the last value wins.
	GBA_CHECK(GbaSettings::Parse("{\"audio\": {\"volume\": 10, \"volume\": 20}}", settings, &error));
	GBA_CHECK_EQ(settings.volume, 20);
}

GBA_TEST(Settings, Clamping)
{
	// A number outside the range of its member is clamped, not rejected: a hand edited file with a
	// scale of 0 must not stop the emulator from starting.
	GbaSettings settings;
	std::string error;
	GBA_CHECK_MSG(GbaSettings::Parse(
		"{\"video\": {\"videoScale\": 0}, \"audio\": {\"sampleRate\": -1, \"volume\": 250}, "
		"\"link\": {\"linkPlayers\": 9}, \"emulation\": {\"logLevel\": 9}}", settings, &error),
		"a document with out of range numbers still parses: " + error);
	GBA_CHECK_EQ(settings.videoScale, 1);			// 1..10
	GBA_CHECK_EQ(settings.sampleRate, 8000);		// 8000..192000
	GBA_CHECK_EQ(settings.volume, 100);				// 0..100
	GBA_CHECK_EQ(settings.linkPlayers, 4);			// 2..4
	GBA_CHECK_EQ(settings.logLevel, 3);				// 0..3

	// The other end of every range.
	GBA_CHECK_MSG(GbaSettings::Parse(
		"{\"video\": {\"videoScale\": 99}, \"audio\": {\"sampleRate\": 4000000, \"volume\": -5}, "
		"\"link\": {\"linkPlayers\": 1}, \"emulation\": {\"logLevel\": -1}}", settings, &error),
		"the high end of every range: " + error);
	GBA_CHECK_EQ(settings.videoScale, 10);
	GBA_CHECK_EQ(settings.sampleRate, 192000);
	GBA_CHECK_EQ(settings.volume, 0);
	GBA_CHECK_EQ(settings.linkPlayers, 2);
	GBA_CHECK_EQ(settings.logLevel, 0);

	// A short number that does not fit 64 bits saturates instead of wrapping around.
	GBA_CHECK(GbaSettings::Parse("{\"audio\": {\"volume\": 99999999999999999999999}}", settings, &error));
	GBA_CHECK_EQ(settings.volume, 100);

	// A number that is not a whole number is reported and the default is kept.
	GBA_CHECK(GbaSettings::Parse("{\"audio\": {\"volume\": 55.5}}", settings, &error));
	GBA_CHECK_EQ(settings.volume, 100);
	GBA_CHECK(GbaSettings::Parse("{\"audio\": {\"sampleRate\": 1e5}}", settings, &error));
	GBA_CHECK_EQ(settings.sampleRate, 32768);
}

GBA_TEST(Settings, MutatedDocuments)
{
	// A deterministic mutation fuzz over the shipped document, the way testing/security_test.cpp
	// treats the other readers: every mutated copy either parses or is refused with a message and
	// the defaults - never a crash, never a spin, never a half applied configuration. The seed is
	// fixed, so a failure is reproducible.
	std::string base = GbaSettings::DefaultJson();
	unsigned state = 0x12345678u;
	auto next = [&state]()
	{
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return state;
	};

	int parsed = 0;
	int refused = 0;
	for (int round = 0; round < 2000; round++)
	{
		std::string text = base;
		int mutations = 1 + (int)(next() % 3);
		for (int m = 0; m < mutations && !text.empty(); m++)
		{
			size_t at = (size_t)(next() % text.size());
			switch (next() % 5)
			{
			case 0: text[at] = (char)(next() & 0xFF); break;					// a random byte
			case 1: text.erase(at, 1 + next() % 8); break;						// delete a slice
			case 2: text.insert(at, 1 + next() % 8, (char)(next() & 0xFF)); break;
			case 3: text.resize(at); break;										// truncate
			case 4: text.insert(at, text.substr(at, 1 + next() % 16)); break;	// duplicate a slice
			}
		}

		GbaSettings settings = GbaSettings::Defaults();
		std::string error;
		if (GbaSettings::Parse(text, settings, &error))
		{
			parsed++;
		}
		else
		{
			refused++;
			GBA_CHECK_MSG(!error.empty(), "a refused document has to explain itself");
			CheckSame(settings, GbaSettings::Defaults(), "a refused document leaves the defaults");
		}
	}

	// Both outcomes have to happen: a corpus that only parsed would not exercise the failure paths
	// at all, and one that only failed would not exercise the reader.
	GBA_CHECK_MSG(parsed > 0, "the mutation corpus has to contain documents that parse");
	GBA_CHECK_MSG(refused > 0, "the mutation corpus has to contain documents that are refused");
}

GBA_TEST(Settings, MistypedValues)
{
	// A value of the wrong type is not a syntax error: the document is well formed and the member
	// keeps its default, so one mistyped value does not cost the user the rest of the file.
	GbaSettings settings;
	std::string error;
	const char* const document =
		"{\"boot\": {\"useCustomBootRom\": \"yes\"}, \"video\": {\"videoScale\": \"three\"}, "
		"\"audio\": {\"sampleRate\": null}, \"emulation\": {\"saveDirectory\": 7}, "
		"\"link\": {\"linkAddress\": [\"a\", \"b\"]}, \"input\": {\"A\": 3, \"B\": {}}}";

	GBA_CHECK_MSG(GbaSettings::Parse(document, settings, &error),
		"a mistyped member is reported, not fatal: " + error);
	GBA_CHECK_MSG(error.empty(), "a mistyped member is not a document error: " + error);
	CheckSame(settings, GbaSettings::Defaults(), "every mistyped member keeps its default");
}

GBA_TEST(Settings, ReportsProblems)
{
	// An unknown section, an unknown member and a clamped number are reported through the log and
	// the document still loads: a file written by a newer frontend must not stop this build.
	std::vector<std::string> messages;
	SetLogSink([](LogLevel, const char* text, void* user)
	{
		((std::vector<std::string>*)user)->push_back(text);
	}, &messages);

	GbaSettings settings;
	std::string error;
	bool ok = GbaSettings::Parse(
		"{\"future\": {\"x\": 1}, \"video\": {\"futureScale\": 2, \"videoScale\": 0}}", settings, &error);

	SetLogSink(nullptr, nullptr);

	GBA_CHECK_MSG(ok, "an unknown section is not fatal: " + error);
	GBA_CHECK_EQ(settings.videoScale, 1);
	GBA_CHECK_EQ(settings.volume, 100);

	bool sawSection = false;
	bool sawMember = false;
	bool sawClamp = false;
	for (const std::string& message : messages)
	{
		if (message.find("section") != std::string::npos && message.find("future") != std::string::npos)
			sawSection = true;
		if (message.find("futureScale") != std::string::npos)
			sawMember = true;
		if (message.find("videoScale") != std::string::npos && message.find("clamped") != std::string::npos)
			sawClamp = true;
	}

	GBA_CHECK_MSG(sawSection, "the unknown section must be reported");
	GBA_CHECK_MSG(sawMember, "the unknown member must be reported");
	GBA_CHECK_MSG(sawClamp, "the clamped number must be reported");
}

// -------------------------------------------------------------------------------------------
// Loading from disk
// -------------------------------------------------------------------------------------------

GBA_TEST(Settings, LoadMissingFile)
{
	// A settings file that does not exist is not an error: the defaults are used (and the first
	// Save writes the file). A value the caller had is replaced by the defaults.
	std::remove(MissingPath);

	GbaSettings settings = GbaSettings::Defaults();
	settings.volume = 3;
	std::string error = "stale";
	GBA_CHECK_MSG(GbaSettings::Load(MissingPath, settings, &error), "a missing file must load");
	GBA_CHECK_MSG(error.empty(), "a missing file is not an error: " + error);
	CheckSame(settings, GbaSettings::Defaults(), "a missing file leaves the defaults");
}

GBA_TEST(Settings, LoadCorruptFile)
{
	// A truncated document is refused, the message names the file and the line, and the caller is
	// left with the defaults.
	{
		std::ofstream file(CorruptPath, std::ios::binary | std::ios::trunc);
		file << "{\n\t\"video\":\n\t{\n\t\t\"videoScale\": 5\n";
	}
	GbaSettings settings = GbaSettings::Defaults();
	settings.volume = 3;
	std::string error;
	GBA_CHECK_MSG(!GbaSettings::Load(CorruptPath, settings, &error), "a truncated file must be refused");
	GBA_CHECK_MSG(error.find(CorruptPath) == 0, "the message names the file: " + error);
	CheckSame(settings, GbaSettings::Defaults(), "a corrupt file leaves the defaults");
	std::remove(CorruptPath);

	// An empty file is corrupt, not missing: an empty settings file is a mistake worth reporting.
	{
		std::ofstream file(CorruptPath, std::ios::binary | std::ios::trunc);
	}
	GBA_CHECK_MSG(!GbaSettings::Load(CorruptPath, settings, &error), "an empty file must be refused");
	GBA_CHECK_MSG(!error.empty(), "an empty file is reported");
	CheckSame(settings, GbaSettings::Defaults(), "an empty file leaves the defaults");
	std::remove(CorruptPath);
}

GBA_TEST(Settings, LoadDirectory)
{
	// A directory is not a settings file. On Linux it can be opened but not read, so this is the
	// path that has to tell "cannot read" apart from "an empty document".
	GbaSettings settings = GbaSettings::Defaults();
	settings.volume = 3;
	std::string error;
	GBA_CHECK_MSG(!GbaSettings::Load("/tmp", settings, &error), "a directory must not load");
	GBA_CHECK_MSG(!error.empty(), "loading a directory is reported");
	GBA_CHECK_MSG(error.find("/tmp") == 0, "the message names the path: " + error);
	CheckSame(settings, GbaSettings::Defaults(), "a failed load leaves the defaults");
}
