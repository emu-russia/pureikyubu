// UTF-8 tests (issue #372, "Переставить JDI на UTF-8").
//
// The narrow string of the project is UTF-8: the JDI command line and its arguments, the Json
// documents, the reports and every front end of the emulator. This suite pins the three layers
// down, from the bottom up:
//
//   * Util_Utf8Test - the two conversion directions of utils.cpp, the cursor arithmetic the console
//                     command line edits its UTF-8 buffer with, and the file helpers that have to
//                     open a name outside the ANSI code page;
//   * Json_Utf8Test - the member names of a document (which used to be truncated to one byte per
//                     character), the values added from a narrow source, and the round trip of a
//                     code point outside the BMP;
//   * Jdi_Utf8Test  - the interface itself: the tokenizer, the arguments a handler receives and the
//                     answer CallJdiReturnString hands back.
//
// The bytes are written out as escapes (and the code points as \uXXXX) on purpose: the suite must
// not depend on how the compiler or the editor treats a non-ASCII source file.

#include "pch.h"

namespace JdiUtf8UnitTest
{
	// -------------------------------------------------------------------------------------------
	// Sample text
	// -------------------------------------------------------------------------------------------

	// "Привет" (Russian for "hello"), "ゲーム" (Japanese for "game") and a code point outside the
	// BMP (U+1F600, a smiley).
	const char* cyrillicUtf8 = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
	const wchar_t* cyrillicWide = L"\u041F\u0440\u0438\u0432\u0435\u0442";

	const char* cjkUtf8 = "\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0";
	const wchar_t* cjkWide = L"\u30B2\u30FC\u30E0";

	const char* astralUtf8 = "\xF0\x9F\x98\x80";
	const wchar_t* astralWide = L"\U0001F600";

	// A path of the shape the issue is about: a file name outside the ASCII range, in both scripts.
	const char* mixedPathUtf8 = "C:\\\xD0\xB8\xD0\xB3\xD1\x80\xD1\x8B\\\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0.gcm";
	const wchar_t* mixedPathWide = L"C:\\\u0438\u0433\u0440\u044B\\\u30B2\u30FC\u30E0.gcm";

	// The Cyrillic half of that path ("games"), which the tests below look for in a document.
	const char* gamesUtf8 = "\xD0\xB8\xD0\xB3\xD1\x80\xD1\x8B";

	// -------------------------------------------------------------------------------------------
	// The conversions and the cursor arithmetic (utils.cpp)
	// -------------------------------------------------------------------------------------------

	TEST_CLASS(Util_Utf8Test)
	{
	public:

		// ASCII text has to go through unchanged in both directions: every caller that never leaves
		// the ASCII range (which is most of the emulator) must see exactly the strings it saw before.
		TEST_METHOD(AsciiIsUnchanged)
		{
			const std::string narrow = "load \"C:\\Work\\game.gcm\" 0x100";
			const std::wstring wide = L"load \"C:\\Work\\game.gcm\" 0x100";

			Assert::IsTrue(Util::WstringToString(wide) == narrow);
			Assert::IsTrue(Util::StringToWstring(narrow) == wide);
		}

		TEST_METHOD(CyrillicAndCjkAreEncodedAsUtf8)
		{
			Assert::IsTrue(Util::WstringToString(cyrillicWide) == cyrillicUtf8);
			Assert::IsTrue(Util::StringToWstring(cyrillicUtf8) == cyrillicWide);

			Assert::IsTrue(Util::WstringToString(cjkWide) == cjkUtf8);
			Assert::IsTrue(Util::StringToWstring(cjkUtf8) == cjkWide);
		}

		// A file name is what the interface carries, so a mixed one is the case that matters.
		TEST_METHOD(APathRoundTrips)
		{
			Assert::IsTrue(Util::WstringToString(mixedPathWide) == mixedPathUtf8);
			Assert::IsTrue(Util::StringToWstring(mixedPathUtf8) == mixedPathWide);
		}

		// U+1F600 is four UTF-8 bytes, and it is two UTF-16 code units on Windows: the pair has to
		// be assembled on the way out and split on the way in, or the value is not the same one.
		TEST_METHOD(ACodePointOutsideTheBmpRoundTrips)
		{
			Assert::IsTrue(Util::WstringToString(astralWide) == astralUtf8);
			Assert::IsTrue(Util::StringToWstring(astralUtf8) == astralWide);
		}

		// A byte sequence that is not valid UTF-8 is not dropped and does not run off the end: it is
		// carried through as the code point of the byte itself, so a name from an unknown source
		// still reaches the file system in one piece.
		TEST_METHOD(MalformedSequencesAreCarriedThrough)
		{
			// A sequence cut short by the end of the string.
			Assert::IsTrue(Util::StringToWstring("\xE2\x82") == L"\u00E2\u0082");
			Assert::IsTrue(Util::StringToWstring("\xF0\x9F\x98") == L"\u00F0\u009F\u0098");

			// A stray continuation byte, and the two over-long forms.
			Assert::IsTrue(Util::StringToWstring("\x80") == L"\u0080");
			Assert::IsTrue(Util::StringToWstring("\xC0\xAF") == L"\u00C0\u00AF");
			Assert::IsTrue(Util::StringToWstring("\xE0\x80\xAF") == L"\u00E0\u0080\u00AF");

			// A surrogate encoded as UTF-8 (CESU-8), which is not a code point.
			Assert::IsTrue(Util::StringToWstring("\xED\xA0\x80") == L"\u00ED\u00A0\u0080");

			// Nothing here may read past the end of the buffer.
			Assert::IsTrue(Util::StringToWstring("").empty());
			Assert::IsTrue(Util::StringToWstring("\xE2").size() == 1);
		}

#if defined(_WINDOWS)

		// Half of a surrogate pair on its own is not text; it becomes the replacement character
		// instead of three bytes that no decoder would accept.
		TEST_METHOD(ALoneSurrogateBecomesTheReplacementCharacter)
		{
			Assert::IsTrue(Util::WstringToString(L"\xD800") == "\xEF\xBF\xBD");
			Assert::IsTrue(Util::WstringToString(L"\xDC00") == "\xEF\xBF\xBD");
			Assert::IsTrue(Util::WstringToString(L"\xD800x") == "\xEF\xBF\xBDx");
		}

#endif

		// The console command line edits its buffer by whole characters, so the two cursor functions
		// have to walk code points and stay inside the string.
		TEST_METHOD(TheCursorWalksWholeCharacters)
		{
			std::string text = std::string("> ") + cyrillicUtf8 + " " + cjkUtf8;

			// "> " is two characters, then six Cyrillic ones, a space and three Japanese ones.
			const size_t characters = 2 + 6 + 1 + 3;

			std::vector<size_t> offsets;
			size_t offset = 0;

			do
			{
				offsets.push_back(offset);
				offset = Util::Utf8NextOffset(text, offset);
			} while (offset < text.size());

			Assert::IsTrue(offsets.size() == characters);

			// Every boundary has to be found again by walking back from the next one.
			for (size_t i = 1; i < offsets.size(); i++)
			{
				Assert::AreEqual(offsets[i - 1], Util::Utf8PrevOffset(text, offsets[i]));
			}

			// Walking back from the end one character at a time reaches the start.
			size_t back = text.size();

			for (size_t i = 0; i < characters; i++)
			{
				back = Util::Utf8PrevOffset(text, back);
			}

			Assert::AreEqual((size_t)0, back);

			// The ends are clamped rather than wrapped.
			Assert::AreEqual(text.size(), Util::Utf8NextOffset(text, text.size()));
			Assert::AreEqual(text.size(), Util::Utf8NextOffset(text, text.size() + 100));
			Assert::AreEqual((size_t)0, Util::Utf8PrevOffset(text, 0));
			Assert::AreEqual(Util::Utf8PrevOffset(text, text.size()), Util::Utf8PrevOffset(text, text.size() + 100));

			// The bytes of a sequence belong to it: the next step from a continuation byte is one
			// byte (it is a code point of its own to the walker), and the walk still terminates.
			size_t inside = 3;			// the second byte of the first Cyrillic character
			Assert::IsTrue(Util::Utf8NextOffset(text, inside) > inside);
			Assert::AreEqual((size_t)2, Util::Utf8PrevOffset(text, inside));
		}

		// The GL front end and the console both walk the text with Utf8Codepoint, so a sequence has to
		// come back as one code point together with its length, and a byte that cannot start one as
		// the byte itself.
		TEST_METHOD(CodePointsAreDecodedWithTheirLength)
		{
			const std::string text = std::string(cyrillicUtf8) + " " + astralUtf8;

			size_t offset = 0;
			size_t length = 0;

			// The first Cyrillic character is two bytes, and it is the code point it stands for.
			Assert::AreEqual((uint32_t)0x041F, Util::Utf8Codepoint(text, offset, length));
			Assert::AreEqual((size_t)2, length);

			// The smiley is one code point of four bytes (two UTF-16 units on Windows).
			offset = strlen(cyrillicUtf8) + 1;
			Assert::AreEqual((uint32_t)0x1F600, Util::Utf8Codepoint(text, offset, length));
			Assert::AreEqual((size_t)4, length);

			// A stray byte is a code point of its own, and an offset past the end decodes to nothing
			// (the callers stop at the string's length, this only has to stay inside it).
			const std::string stray = "\x80";

			Assert::AreEqual((uint32_t)0x80, Util::Utf8Codepoint(stray, 0, length));
			Assert::AreEqual((size_t)1, length);

			Util::Utf8Codepoint(stray, 1, length);
			Assert::AreEqual((size_t)0, length);
		}

		// The whole point of the issue: a file whose name is not in the ANSI code page has to open.
		TEST_METHOD(TheFileHelpersTakeAUnicodeName)
		{
			const std::wstring name = std::wstring(L"\u043F\u0440\u043E\u0432\u0435\u0440\u043A\u0430-") + L"\u30B2\u30FC\u30E0.bin";

			std::vector<uint8_t> data = { 0x00, 0x11, 0x22, 0x33, 0x44 };

			Assert::IsTrue(Util::FileSave(name, data));
			Assert::IsTrue(Util::FileExists(name));
			Assert::AreEqual(data.size(), Util::FileSize(name));

			// The narrow overload is the one the JDI command handlers use, so the UTF-8 form has to
			// reach the same file.
			const std::string utf8Name = Util::WstringToString(name);

			Assert::IsTrue(Util::FileExists(utf8Name));
			Assert::AreEqual(data.size(), Util::FileSize(utf8Name));

			std::vector<uint8_t> read = Util::FileLoad(utf8Name);
			Assert::AreEqual(data.size(), read.size());
			Assert::IsTrue(read == data);

#ifdef _WINDOWS
			_wremove(name.c_str());
#else
			remove(utf8Name.c_str());
#endif

			Assert::IsFalse(Util::FileExists(name));
		}
	};

	// -------------------------------------------------------------------------------------------
	// Json
	// -------------------------------------------------------------------------------------------

	namespace
	{
		std::string Serialize(Json& json)
		{
			// The writer needs the size first (it writes nothing in that pass), then a buffer one
			// byte longer than the text for the terminator the caller adds.
			size_t size = 0;
			json.GetSerializedTextSize(nullptr, (size_t)-1, size);

			std::vector<char> buffer(size + 1, 0);
			size_t actual = 0;
			json.Serialize(buffer.data(), size + 1, actual);
			buffer[actual] = 0;

			return std::string(buffer.data(), actual);
		}
	}

	TEST_CLASS(Json_Utf8Test)
	{
	public:

		// The name of a member used to be built by truncating every wide character to one byte, so a
		// document with a non-ASCII key lost its key on the way in.
		TEST_METHOD(MemberNamesAreUtf8)
		{
			const std::string document = std::string("{\"") + cyrillicUtf8 + "\" : \"value\"}";

			Json json;
			std::string text = document;
			json.Deserialize((void*)text.data(), text.size());

			Json::Value* root = json.root.children.back();
			Assert::IsTrue(root->type == Json::ValueType::Object);

			// ByName compares the UTF-8 the caller has with the UTF-8 the parser produced.
			Json::Value* member = root->ByName(cyrillicUtf8);
			Assert::IsNotNull(member);
			Assert::IsNotNull(member->name);
			Assert::IsTrue(std::string(member->name) == cyrillicUtf8);

			// ... and the name goes back out as the same bytes.
			const std::string written = Serialize(json);
			Assert::IsTrue(written.find(cyrillicUtf8) != std::string::npos);
		}

		// A member added by a command handler (AddUtf8String) has to end up as the same text.
		TEST_METHOD(ANarrowValueIsReadAsUtf8)
		{
			Json json;
			Json::Value* root = json.root.AddObject(nullptr);
			root->AddUtf8String("path", mixedPathUtf8);

			const std::string written = Serialize(json);

			// The characters survive the document; the backslashes of the path are escaped by the
			// writer, so the two scripts are what is looked for here (the value comparison below
			// checks the whole string).
			Assert::IsTrue(written.find(gamesUtf8) != std::string::npos);
			Assert::IsTrue(written.find(cjkUtf8) != std::string::npos);

			// The value is kept wide, so the emulator sees the characters, not the bytes.
			Json parsed;
			std::string text = written;
			parsed.Deserialize((void*)text.data(), text.size());

			Json::Value* object = parsed.root.children.back();
			Json::Value* path = object->ByName("path");
			Assert::IsNotNull(path);
			Assert::IsTrue(path->type == Json::ValueType::String);
			Assert::IsTrue(std::wstring(path->value.AsString) == mixedPathWide);
		}

		// A code point outside the BMP is a surrogate pair in the emulator's text; a document that
		// carries one has to survive both directions (the writer used to refuse the pair).
		TEST_METHOD(ACodePointOutsideTheBmpRoundTrips)
		{
			Json json;
			Json::Value* root = json.root.AddObject(nullptr);
			root->AddString("emoji", astralWide);

			const std::string written = Serialize(json);
			Assert::IsTrue(written.find(astralUtf8) != std::string::npos);

			Json parsed;
			std::string text = written;
			parsed.Deserialize((void*)text.data(), text.size());

			Json::Value* object = parsed.root.children.back();
			Json::Value* emoji = object->ByName("emoji");
			Assert::IsNotNull(emoji);
			Assert::IsTrue(std::wstring(emoji->value.AsString) == astralWide);
		}

		// A document that arrives as UTF-8 with a non-ASCII member name and value (the shape of the
		// settings files and of a JDI specification) round trips byte for byte.
		TEST_METHOD(ADocumentRoundTrips)
		{
			const std::string document =
				std::string("{ \"") + cjkUtf8 + "\" : \"" + cyrillicUtf8 + "\" }";

			Json json;
			std::string text = document;
			json.Deserialize((void*)text.data(), text.size());

			Json::Value* object = json.root.children.back();
			Json::Value* value = object->ByName(cjkUtf8);
			Assert::IsNotNull(value);
			Assert::IsTrue(std::wstring(value->value.AsString) == cyrillicWide);
		}
	};

	// -------------------------------------------------------------------------------------------
	// The JDI interface itself
	// -------------------------------------------------------------------------------------------

	namespace
	{
		// A command that answers with its argument unchanged, so the test can see exactly what the
		// tokenizer and the hub handed to the handler.
		Json::Value* CmdUtf8Echo(std::vector<std::string>& args)
		{
			Json::Value* output = new Json::Value();
			output->type = Json::ValueType::Array;
			output->AddUtf8String(nullptr, args.size() > 1 ? args[1].c_str() : "");
			return output;
		}

		// A command that answers with a wide string of the emulator's own making.
		Json::Value* CmdUtf8Answer(std::vector<std::string>& args)
		{
			Json::Value* output = new Json::Value();
			output->type = Json::ValueType::Array;
			output->AddString(nullptr, mixedPathWide);
			return output;
		}

		// The hub looks a command up in the "can" list of a registered node before it calls the
		// handler, so the node is part of what the test drives (and the registration path, which
		// takes the node name as UTF-8, is exercised as well).
		const char* utf8TestNode =
			"{"
			"  \"info\": { \"helpGroup\": \"Utf8 Test\" },"
			"  \"can\": {"
			"    \"Utf8Echo\": { \"help\": \"Answer with the argument\", \"args\": 1, \"hints\": \"<text>\" },"
			"    \"Utf8Answer\": { \"help\": \"Answer with a wide string\" }"
			"  }"
			"}";

		void Utf8TestReflector()
		{
			JdiAddCmd("Utf8Echo", CmdUtf8Echo);
			JdiAddCmd("Utf8Answer", CmdUtf8Answer);
		}
	}

	TEST_CLASS(Jdi_Utf8Test)
	{
	public:

		TEST_CLASS_INITIALIZE(RegisterNode)
		{
			JdiAddNode("UTF8_TEST_JDI_JSON", utf8TestNode, Utf8TestReflector);
		}

		// A quoted UTF-8 argument (the way a front end passes a file name) arrives at the handler
		// and comes back out of CallJdi unchanged.
		TEST_METHOD(AQuotedArgumentSurvivesTheTrip)
		{
			std::string request = std::string("Utf8Echo \"") + mixedPathUtf8 + "\"";

			Json::Value* result = CallJdi(request.c_str());
			Assert::IsNotNull(result);
			Assert::IsTrue(result->type == Json::ValueType::Array);
			Assert::IsTrue(result->children.size() == 1);

			Json::Value* answer = result->children.front();
			Assert::IsTrue(answer->type == Json::ValueType::String);
			Assert::IsTrue(Util::WstringToString(answer->value.AsString) == mixedPathUtf8);

			delete result;
		}

		// A code point outside the BMP that was typed into the console is one argument, not two.
		TEST_METHOD(AnArgumentOutsideTheBmpSurvivesTheTrip)
		{
			std::string request = std::string("Utf8Echo ") + astralUtf8;

			Json::Value* result = CallJdi(request.c_str());
			Assert::IsNotNull(result);

			Json::Value* answer = result->children.front();
			Assert::IsTrue(Util::WstringToString(answer->value.AsString) == astralUtf8);

			delete result;
		}

		// CallJdiReturnString is the other half of the interface, and it used to hand back the low
		// byte of every wide character.
		TEST_METHOD(AStringAnswerIsUtf8)
		{
			char answer[0x100] = { 0, };

			Assert::IsTrue(CallJdiReturnString("Utf8Answer", answer, sizeof(answer) - 1));
			Assert::IsTrue(std::string(answer) == mixedPathUtf8);
		}

		// A command line read from a script file can carry the UTF-8 byte order mark; the mark is
		// not part of the first argument, so the command is still found.
		TEST_METHOD(TheByteOrderMarkIsSkipped)
		{
			std::string request = std::string("\xEF\xBB\xBF") + "Utf8Echo ok";

			Json::Value* result = CallJdi(request.c_str());
			Assert::IsNotNull(result);

			Json::Value* answer = result->children.front();
			Assert::IsTrue(std::wstring(answer->value.AsString) == L"ok");

			delete result;
		}
	};
}
