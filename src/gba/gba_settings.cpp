// The GBA emulator's settings: the reader and the writer of build/Data/GBASettings.json.
//
// The GBA core must build and be tested without the GameCube side of the emulator, so this file
// does not use src/json.cpp (nor SDL, nor the pch). It carries its own reader and writer for the
// one document shape the settings file has. Both are deliberately strict and small:
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
// blank line between the sections and a trailing newline. ToJson() and DefaultJson() both go
// through the same writer, so the shipped file can never drift from the code (a unit test in
// testing/gba_bench/test_settings.cpp compares them byte for byte).
//
// What the reader enforces
// ------------------------
// The reader is a hand-written recursive descent parser, because it is fed a file the user (or
// anything that can write into the user's directory) can edit. It enforces the rules the security
// review requires of every input path (see wiki/security.md):
//
//   * the JSON grammar is strict: `{` and `}` for every object, `,` between members and no
//     trailing comma, `:` after every member name, `"` around every string, the JSON number
//     grammar (no leading zeros, no bare `.`, no `NaN`), and nothing after the document. A
//     document that breaks any of them is rejected with "<line>: <what was expected>";
//   * four hard limits keep a malformed document from exhausting anything: the document
//     (2 MByte), one string token (4096 bytes), one number token (24 characters) and the nesting
//     depth (32 levels, so the recursion in this file cannot exhaust the stack). Every limit is
//     checked *before* the data is copied or converted, so a 1 MByte string is refused rather
//     than allocated;
//   * a rejected document is never half applied: Parse() leaves the caller's settings at the
//     defaults, and Load() reports the reason. A missing file is not an error at all (the
//     defaults are used and the first Save writes the file).
//
// Not every problem is fatal, though, and the split is deliberate:
//
//   * a *syntax* problem (including a limit above) rejects the whole document;
//   * an unknown section, or an unknown member of a known section, is reported through
//     Log(Warn, ...) and skipped: a file written by a newer frontend must not stop this build;
//   * a member whose value has the wrong type, or whose number is not a whole number, is
//     reported and that one member keeps its default - a hand edited file does not lose the
//     whole configuration over one mistyped value;
//   * a number outside the range of its member is clamped (a scale of 0 becomes 1, a volume of
//     250 becomes 100), never rejected and never wrapped.
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

#include <cerrno>
#include <cstring>
#include <fstream>
#include <limits>

using namespace GBA;			// the whole file is GBA's own settings plumbing

namespace
{
	// ---------------------------------------------------------------------------------------
	// The limits and the ranges
	// ---------------------------------------------------------------------------------------

	// The whole document. The shipped file is about 1 KByte; Load also refuses to read more than
	// this into memory, so a file that is not a settings file at all (a ROM renamed to .json) is
	// stopped while it is read.
	const size_t MaxDocumentBytes = 2 * 1024 * 1024;

	// One string token, checked while the token is scanned and before anything is allocated.
	// Paths and key names are short (the Json engine on the GameCube side uses 0x1000 here).
	const size_t MaxStringBytes = 4096;

	// One number token. JSON gives numbers no length limit, this reader does: a 500 digit number
	// is a syntax error instead of being copied into a buffer or silently wrapped around.
	const size_t MaxNumberChars = 24;

	// Nested objects and arrays. The shipped document is three levels deep (document, section,
	// info) and a value never nests at all. The reader recurses once per level, so this limit is
	// what keeps a document like {"a":{"a":{"a":... from exhausting the stack.
	const int MaxDepth = 32;

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
		u16 bit;
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

	/// <summary>One scanned number token: where it is and whether it is a whole number.</summary>
	struct NumberToken
	{
		size_t begin = 0;
		size_t end = 0;
		bool whole = true;				// no fraction and no exponent
	};

	/// <summary>A strict JSON reader for one GbaSettings document. Everything the reader refuses
	/// is a syntax error or a limit; a mistyped value is reported by the member readers and the
	/// member keeps its default.</summary>
	class Reader
	{
	public:
		explicit Reader(const std::string& text) : text(text) {}

		/// <summary>Read the whole document. False = it was rejected; Error() says why.</summary>
		bool ReadDocument(GbaSettings& out);

		/// <summary>The failure message, "<line>: ...", empty while the document is valid.</summary>
		const std::string& Error() const { return error; }

	private:
		const std::string& text;
		size_t pos = 0;
		int depth = 0;
		std::string error;

		// -- the scanner -------------------------------------------------------------------

		bool AtEnd() const { return pos >= text.size(); }
		char Cur() const { return text[pos]; }
		int LineAt(size_t offset) const;
		bool Fail(const std::string& message);
		void SkipSpace();
		bool ReadString(std::string& value);
		bool ReadEscape(std::string& value);
		bool ReadHex4(u32& value);
		bool ReadLiteral(const char* literal);
		bool ReadName(std::string& name);
		bool ScanNumber(NumberToken& token);
		bool SkipValue();
		bool SkipArray();

		// -- values ------------------------------------------------------------------------

		bool ValueIsString();
		bool ValueIsNumber();
		bool ReadTextMember(std::string& target, const char* where);
		bool ReadBoolMember(bool& target, const char* where);
		bool ReadIntMember(int& target, const char* where, int minimum, int maximum);
		bool UnknownMember(const char* section, const std::string& name);

		/// <summary>Read the members of one object, handing each name to `onMember`, which has to
		/// consume exactly one value. Returns false on a syntax error. One object is walked at a
		/// time (a nested value is either skipped wholesale or read by the same handler), which is
		/// what the caller in ReadSection assumes.</summary>
		template <typename F>
		bool ForEachMember(F onMember)
		{
			SkipSpace();
			if (AtEnd())
				return Fail("unexpected end of the document, expected '{'");
			if (Cur() != '{')
				return Fail("expected '{'");
			if (++depth > MaxDepth)
				return Fail("more than 32 nested objects or arrays");
			pos++;

			bool first = true;
			while (true)
			{
				SkipSpace();
				if (AtEnd())
					return Fail("unexpected end of the document, expected '}'");
				if (Cur() == '}')
				{
					pos++;
					depth--;
					return true;
				}
				if (!first)
				{
					// Members are separated by ','. A '}' right after a ',' is a trailing comma:
					// nearly always a half deleted member, which is why it is refused instead of
					// ignored (the emulator's other settings files have no trailing comma either).
					if (Cur() != ',')
						return Fail("expected ',' between members");
					pos++;
					SkipSpace();
					if (AtEnd())
						return Fail("unexpected end of the document after ','");
					if (Cur() == '}')
						return Fail("unexpected '}' after ',' (a trailing comma)");
				}

				std::string name;
				if (!ReadName(name))
					return false;
				SkipSpace();
				if (AtEnd())
					return Fail("unexpected end of the document, expected ':'");
				if (Cur() != ':')
					return Fail("expected ':' after the member name \"" + name + "\"");
				pos++;

				if (!onMember(name))
					return false;
				first = false;
			}
		}

		// -- the sections ------------------------------------------------------------------

		bool ReadSection(GbaSettings& out, const std::string& name);
		bool ReadBoot(GbaSettings& out);
		bool ReadVideo(GbaSettings& out);
		bool ReadAudio(GbaSettings& out);
		bool ReadInput(GbaSettings& out);
		bool ReadLink(GbaSettings& out);
		bool ReadEmulation(GbaSettings& out);
	};

	/// <summary>The line of `offset` (1 based), for the "<line>: ..." messages.</summary>
	int Reader::LineAt(size_t offset) const
	{
		if (offset > text.size())
			offset = text.size();
		int line = 1;
		for (size_t i = 0; i < offset; i++)
			if (text[i] == '\n')
				line++;
		return line;
	}

	/// <summary>Record the first failure (the first one is the specific one) and return false.</summary>
	bool Reader::Fail(const std::string& message)
	{
		if (error.empty())
		{
			error = std::to_string(LineAt(pos));
			error += ": ";
			error += message;
		}
		return false;
	}

	void Reader::SkipSpace()
	{
		// A UTF-8 BOM is not a JSON token, but Notepad and some editors write one, so it is
		// skipped at the very start of the document (and only there: a BOM in the middle stays a
		// syntax error). '\r' is whitespace, which is what makes a CRLF file read exactly like an
		// LF one.
		if (pos == 0 && text.size() >= 3 &&
			(unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
			pos = 3;
		while (!AtEnd())
		{
			char c = text[pos];
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
				pos++;
			else
				break;
		}
	}

	bool Reader::ReadString(std::string& value)
	{
		if (AtEnd() || Cur() != '"')
			return Fail("expected a string");
		pos++;
		value.clear();
		size_t run = pos;				// where the current unescaped run starts

		while (true)
		{
			if (AtEnd())
				return Fail("unterminated string");
			char c = text[pos];
			if (c == '"')
			{
				value.append(text, run, pos - run);
				pos++;
				return true;
			}
			if (c == '\\')
			{
				value.append(text, run, pos - run);
				pos++;
				if (!ReadEscape(value))
					return false;
				run = pos;
				continue;
			}
			if ((unsigned char)c < 0x20)
				return Fail("a control character in a string (escape it as \\u00XX)");
			// The length is checked while the token is scanned, so a 1 MByte string is refused
			// here, before a single byte of it is copied into `value`. The byte being scanned is
			// counted, which is what makes the limit exactly 4096 bytes.
			if (value.size() + (pos - run) >= MaxStringBytes)
				return Fail("the string is longer than 4096 bytes");
			pos++;
		}
	}

	bool Reader::ReadEscape(std::string& value)
	{
		if (AtEnd())
			return Fail("unterminated escape in a string");
		char c = text[pos++];
		switch (c)
		{
		case '"': value += '"'; return true;
		case '\\': value += '\\'; return true;
		case '/': value += '/'; return true;
		case 'b': value += '\b'; return true;
		case 'f': value += '\f'; return true;
		case 'n': value += '\n'; return true;
		case 'r': value += '\r'; return true;
		case 't': value += '\t'; return true;
		case 'u': break;
		default:
			return Fail(std::string("unknown escape \"\\") + c + "\" in a string");
		}

		// \uXXXX is encoded as UTF-8, the way the rest of the settings files are written. A
		// surrogate pair is combined; an unpaired surrogate is refused rather than turned into the
		// invalid UTF-8 that encoding it alone (CESU-8) would produce.
		u32 code = 0;
		if (!ReadHex4(code))
			return false;
		if (code >= 0xD800 && code <= 0xDBFF)
		{
			if (pos + 1 < text.size() && text[pos] == '\\' && text[pos + 1] == 'u')
			{
				pos += 2;
				u32 low = 0;
				if (!ReadHex4(low))
					return false;
				if (low < 0xDC00 || low > 0xDFFF)
					return Fail("a high surrogate that is not followed by a low surrogate");
				code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
			}
			else
			{
				return Fail("an unpaired UTF-16 high surrogate");
			}
		}
		else if (code >= 0xDC00 && code <= 0xDFFF)
		{
			return Fail("an unpaired UTF-16 low surrogate");
		}

		if (code < 0x80)
		{
			value += (char)code;
		}
		else if (code < 0x800)
		{
			value += (char)(0xC0 | (code >> 6));
			value += (char)(0x80 | (code & 0x3F));
		}
		else if (code < 0x10000)
		{
			value += (char)(0xE0 | (code >> 12));
			value += (char)(0x80 | ((code >> 6) & 0x3F));
			value += (char)(0x80 | (code & 0x3F));
		}
		else
		{
			value += (char)(0xF0 | (code >> 18));
			value += (char)(0x80 | ((code >> 12) & 0x3F));
			value += (char)(0x80 | ((code >> 6) & 0x3F));
			value += (char)(0x80 | (code & 0x3F));
		}

		if (value.size() > MaxStringBytes)
			return Fail("the string is longer than 4096 bytes");
		return true;
	}

	bool Reader::ReadHex4(u32& value)
	{
		if (pos + 4 > text.size())
			return Fail("a \\u escape without four hexadecimal digits");
		value = 0;
		for (int i = 0; i < 4; i++)
		{
			char c = text[pos + i];
			u32 digit;
			if (c >= '0' && c <= '9')
				digit = (u32)(c - '0');
			else if (c >= 'a' && c <= 'f')
				digit = (u32)(c - 'a' + 10);
			else if (c >= 'A' && c <= 'F')
				digit = (u32)(c - 'A' + 10);
			else
				return Fail("a \\u escape without four hexadecimal digits");
			value = (value << 4) | digit;
		}
		pos += 4;
		return true;
	}

	bool Reader::ReadLiteral(const char* literal)
	{
		size_t length = strlen(literal);
		if (text.compare(pos, length, literal) == 0)
		{
			pos += length;
			return true;
		}
		return Fail(std::string("unexpected text where \"") + literal + "\" was expected");
	}

	bool Reader::ReadName(std::string& name)
	{
		if (AtEnd() || Cur() != '"')
			return Fail("expected a member name (a string)");
		return ReadString(name);
	}

	/// <summary>Scan one number: the JSON grammar exactly (an optional '-', no leading zero, an
	/// optional fraction and an optional exponent), and at most MaxNumberChars characters.</summary>
	bool Reader::ScanNumber(NumberToken& token)
	{
		token.begin = pos;
		token.whole = true;

		if (!AtEnd() && Cur() == '-')
			pos++;
		if (AtEnd())
			return Fail("unexpected end of the document in a number");
		if (Cur() == '0')
		{
			pos++;
			// A leading zero is only the number zero: "007" is not JSON, and the emulator's other
			// settings files already write canonical numbers.
			if (!AtEnd() && Cur() >= '0' && Cur() <= '9')
				return Fail("a leading zero in a number");
		}
		else if (Cur() >= '1' && Cur() <= '9')
		{
			while (!AtEnd() && Cur() >= '0' && Cur() <= '9')
				pos++;
		}
		else
		{
			return Fail("expected a digit");
		}

		if (!AtEnd() && Cur() == '.')
		{
			token.whole = false;
			pos++;
			if (AtEnd() || Cur() < '0' || Cur() > '9')
				return Fail("expected a digit after '.'");
			while (!AtEnd() && Cur() >= '0' && Cur() <= '9')
				pos++;
		}
		if (!AtEnd() && (Cur() == 'e' || Cur() == 'E'))
		{
			token.whole = false;
			pos++;
			if (!AtEnd() && (Cur() == '+' || Cur() == '-'))
				pos++;
			if (AtEnd() || Cur() < '0' || Cur() > '9')
				return Fail("expected a digit in the exponent");
			while (!AtEnd() && Cur() >= '0' && Cur() <= '9')
				pos++;
		}

		token.end = pos;
		// The token is scanned with a few pointer steps per digit and no allocation at all, and the
		// length is checked here, before it is converted: a 500 digit number never reaches the
		// converter.
		if (token.end - token.begin > MaxNumberChars)
			return Fail("the number is longer than 24 characters");
		return true;
	}

	bool Reader::SkipValue()
	{
		SkipSpace();
		if (AtEnd())
			return Fail("unexpected end of the document, expected a value");
		switch (Cur())
		{
		case '{':
			return ForEachMember([this](const std::string&) { return SkipValue(); });
		case '[':
			return SkipArray();
		case '"':
		{
			std::string ignored;
			return ReadString(ignored);
		}
		case 't': return ReadLiteral("true");
		case 'f': return ReadLiteral("false");
		case 'n': return ReadLiteral("null");
		default:
			if (Cur() == '-' || (Cur() >= '0' && Cur() <= '9'))
			{
				NumberToken token;
				return ScanNumber(token);
			}
			return Fail(std::string("unexpected character '") + Cur() + "' (expected a value)");
		}
	}

	bool Reader::SkipArray()
	{
		// No member of this file is an array. One is still parsed, so that a newer frontend can add
		// a member this build skips without the whole file being refused; an unterminated array is
		// a syntax error like any other.
		SkipSpace();
		if (AtEnd() || Cur() != '[')
			return Fail("expected '['");
		if (++depth > MaxDepth)
			return Fail("more than 32 nested objects or arrays");
		pos++;

		bool first = true;
		while (true)
		{
			SkipSpace();
			if (AtEnd())
				return Fail("unterminated array (expected ']' or a value)");
			if (Cur() == ']')
			{
				pos++;
				depth--;
				return true;
			}
			if (!first)
			{
				if (Cur() != ',')
					return Fail("expected ',' between the values of an array");
				pos++;
				SkipSpace();
				if (AtEnd())
					return Fail("unexpected end of the document after ','");
				if (Cur() == ']')
					return Fail("unexpected ']' after ',' (a trailing comma)");
			}
			if (!SkipValue())
				return false;
			first = false;
		}
	}

	bool Reader::ValueIsString()
	{
		SkipSpace();
		return !AtEnd() && Cur() == '"';
	}

	bool Reader::ValueIsNumber()
	{
		SkipSpace();
		return !AtEnd() && (Cur() == '-' || (Cur() >= '0' && Cur() <= '9'));
	}

	bool Reader::ReadTextMember(std::string& target, const char* where)
	{
		if (!ValueIsString())
		{
			Log(LogLevel::Warn, "gba settings: %s must be a string, the default is kept", where);
			return SkipValue();
		}
		return ReadString(target);
	}

	bool Reader::ReadBoolMember(bool& target, const char* where)
	{
		SkipSpace();
		if (!AtEnd() && Cur() == 't')
		{
			// A literal that is nearly right ("tru") is not a mistyped value but a broken
			// document, so it is read strictly and reported as a syntax error.
			if (!ReadLiteral("true"))
				return false;
			target = true;
			return true;
		}
		if (!AtEnd() && Cur() == 'f')
		{
			if (!ReadLiteral("false"))
				return false;
			target = false;
			return true;
		}
		Log(LogLevel::Warn, "gba settings: %s must be true or false, the default %s is kept",
			where, target ? "true" : "false");
		return SkipValue();
	}

	bool Reader::ReadIntMember(int& target, const char* where, int minimum, int maximum)
	{
		if (!ValueIsNumber())
		{
			Log(LogLevel::Warn, "gba settings: %s must be a number, the default %i is kept", where, target);
			return SkipValue();
		}

		NumberToken token;
		if (!ScanNumber(token))
			return false;
		if (!token.whole)
		{
			Log(LogLevel::Warn, "gba settings: %s is not a whole number, the default %i is kept", where, target);
			return true;
		}

		// The token is at most 24 characters, so the accumulator saturates after at most 20 digits:
		// every digit is checked, and a value that does not fit 64 bits lands on the end of the
		// range instead of wrapping around it.
		size_t i = token.begin;
		bool negative = false;
		if (text[i] == '-')
		{
			negative = true;
			i++;
		}
		const unsigned long long limit = negative ? 9223372036854775808ULL : 9223372036854775807ULL;
		unsigned long long value = 0;
		bool saturated = false;
		for (; i < token.end; i++)
		{
			unsigned digit = (unsigned)(text[i] - '0');
			if (value > (limit - digit) / 10)
			{
				saturated = true;
				break;
			}
			value = value * 10 + digit;
		}

		long long signed_value;
		if (saturated)
			signed_value = negative ? std::numeric_limits<long long>::min() : std::numeric_limits<long long>::max();
		else if (negative)
			signed_value = (value == 9223372036854775808ULL) ? std::numeric_limits<long long>::min() : -(long long)value;
		else
			signed_value = (long long)value;

		if (signed_value < minimum || signed_value > maximum)
		{
			int clamped = (int)(signed_value < minimum ? minimum : maximum);
			Log(LogLevel::Warn, "gba settings: %s is %lld, outside %i..%i, clamped to %i",
				where, signed_value, minimum, maximum, clamped);
			target = clamped;
			return true;
		}
		target = (int)signed_value;
		return true;
	}

	bool Reader::UnknownMember(const char* section, const std::string& name)
	{
		// A member this build does not know comes from a newer frontend (or is a typo). It is
		// reported and skipped: refusing the file would make an older core unable to read a
		// configuration a newer one wrote.
		Log(LogLevel::Warn, "gba settings: unknown member \"%s\" in section \"%s\" was skipped",
			name.c_str(), section);
		return SkipValue();
	}

	bool Reader::ReadDocument(GbaSettings& out)
	{
		if (text.size() > MaxDocumentBytes)
			return Fail("the document is larger than 2097152 bytes");
		SkipSpace();
		if (!ForEachMember([this, &out](const std::string& name) { return ReadSection(out, name); }))
			return false;
		SkipSpace();
		if (!AtEnd())
			return Fail("unexpected text after the document");
		return true;
	}

	bool Reader::ReadSection(GbaSettings& out, const std::string& name)
	{
		if (name == "info")
			return SkipValue();			// documentation only: written by Save, ignored here
		if (name == "boot")
			return ReadBoot(out);
		if (name == "video")
			return ReadVideo(out);
		if (name == "audio")
			return ReadAudio(out);
		if (name == "input")
			return ReadInput(out);
		if (name == "link")
			return ReadLink(out);
		if (name == "emulation")
			return ReadEmulation(out);

		Log(LogLevel::Warn, "gba settings: unknown section \"%s\" was skipped", name.c_str());
		return SkipValue();
	}

	bool Reader::ReadBoot(GbaSettings& out)
	{
		return ForEachMember([this, &out](const std::string& name)
		{
			if (name == "biosPath")
				return ReadTextMember(out.biosPath, "boot.biosPath");
			if (name == "useCustomBootRom")
				return ReadBoolMember(out.useCustomBootRom, "boot.useCustomBootRom");
			if (name == "skipBootAnimation")
				return ReadBoolMember(out.skipBootAnimation, "boot.skipBootAnimation");
			if (name == "hleBios")
				return ReadBoolMember(out.hleBios, "boot.hleBios");
			return UnknownMember("boot", name);
		});
	}

	bool Reader::ReadVideo(GbaSettings& out)
	{
		return ForEachMember([this, &out](const std::string& name)
		{
			if (name == "videoScale")
				return ReadIntMember(out.videoScale, "video.videoScale", MinScale, MaxScale);
			if (name == "fullscreen")
				return ReadBoolMember(out.fullscreen, "video.fullscreen");
			if (name == "vsync")
				return ReadBoolMember(out.vsync, "video.vsync");
			if (name == "integerScale")
				return ReadBoolMember(out.integerScale, "video.integerScale");
			if (name == "showFps")
				return ReadBoolMember(out.showFps, "video.showFps");
			if (name == "frameSkip")
				return ReadBoolMember(out.frameSkip, "video.frameSkip");
			return UnknownMember("video", name);
		});
	}

	bool Reader::ReadAudio(GbaSettings& out)
	{
		return ForEachMember([this, &out](const std::string& name)
		{
			if (name == "audioEnabled")
				return ReadBoolMember(out.audioEnabled, "audio.audioEnabled");
			if (name == "sampleRate")
				return ReadIntMember(out.sampleRate, "audio.sampleRate", MinSampleRate, MaxSampleRate);
			if (name == "volume")
				return ReadIntMember(out.volume, "audio.volume", MinVolume, MaxVolume);
			return UnknownMember("audio", name);
		});
	}

	bool Reader::ReadInput(GbaSettings& out)
	{
		return ForEachMember([this, &out](const std::string& name)
		{
			// The member *is* the action ("A", "START", "SPEED", ...), the value is the host key
			// name. Bind replaces a known action in place and appends an action this build does
			// not know, so a binding a newer frontend wrote survives a load/save round trip.
			if (!ValueIsString())
			{
				Log(LogLevel::Warn,
					"gba settings: the binding of \"%s\" must be a key name (a string), the default is kept",
					name.c_str());
				return SkipValue();
			}
			std::string key;
			if (!ReadString(key))
				return false;
			out.Bind(name, key);
			return true;
		});
	}

	bool Reader::ReadLink(GbaSettings& out)
	{
		return ForEachMember([this, &out](const std::string& name)
		{
			if (name == "linkEnabled")
				return ReadBoolMember(out.linkEnabled, "link.linkEnabled");
			if (name == "linkServer")
				return ReadBoolMember(out.linkServer, "link.linkServer");
			if (name == "linkAddress")
				return ReadTextMember(out.linkAddress, "link.linkAddress");
			if (name == "linkPlayers")
				return ReadIntMember(out.linkPlayers, "link.linkPlayers", MinLinkPlayers, MaxLinkPlayers);
			return UnknownMember("link", name);
		});
	}

	bool Reader::ReadEmulation(GbaSettings& out)
	{
		return ForEachMember([this, &out](const std::string& name)
		{
			if (name == "rtcEnabled")
				return ReadBoolMember(out.rtcEnabled, "emulation.rtcEnabled");
			if (name == "bootWithNoCartridge")
				return ReadBoolMember(out.bootWithNoCartridge, "emulation.bootWithNoCartridge");
			if (name == "saveDirectory")
				return ReadTextMember(out.saveDirectory, "emulation.saveDirectory");
			if (name == "logLevel")
				return ReadIntMember(out.logLevel, "emulation.logLevel", MinLogLevel, MaxLogLevel);
			return UnknownMember("emulation", name);
		});
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

	u16 GbaSettings::KeyBitFor(const std::string& key) const
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
			Member("volume", Number(volume), true), false);

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

		Reader reader(text);
		GbaSettings parsed = Defaults();
		if (!reader.ReadDocument(parsed))
		{
			// A rejected document is never half applied: the caller gets the defaults.
			out = Defaults();
			Report(error, reader.Error());
			return false;
		}

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
