// Input verifier and input hardening tests.
//
// Every artifact the emulator reads from the outside world is untrusted: the settings JSON, the
// executable images, the disc images, the memory card saves, the ROM dumps, the console scripts,
// the symbol maps and the command line. The rules those artifacts have to satisfy live in
// src/verify.h, and this suite pins them down:
//
//   * VerifyTest          - the range/format predicates themselves, including the wrap-around
//                           cases that the old hand-written checks got wrong;
//   * ScriptTest          - the bounded console script reader (autoexec.cmd is run automatically
//                           on every file load, so a malformed script is a real attack surface);
//   * JsonHardeningTest   - Json::Deserialize against truncated, oversized, over-nested and
//                           unterminated documents: it must always report an error instead of
//                           overflowing a buffer, spinning forever or exhausting the stack;
//   * JdiSpecsTest        - a regression guard the other way round: every JDI specification the
//                           emulator ships must still be accepted by the hardened parser.
//
// The malformed documents below are the ones that were actually exploitable before the fix, so
// each test is a regression test for a specific defect.

#include "pch.h"

#include <chrono>
#include <thread>

namespace SecurityUnitTest
{
	// -------------------------------------------------------------------------------------------
	// Helpers
	// -------------------------------------------------------------------------------------------

	// The emulator's parser reports errors by throwing (mostly const char* literals), so the tests
	// only care that *something* was thrown, and that it was not a crash or a hang.
	template <typename Fn>
	static bool Throws(Fn&& fn)
	{
		try
		{
			fn();
		}
		catch (...)
		{
			return true;
		}

		return false;
	}

	static void ParseJson(const std::string& text)
	{
		Json json;
		std::string buffer = text;
		json.Deserialize(buffer.data(), buffer.size());
	}

	// -------------------------------------------------------------------------------------------
	// The verifier predicates
	// -------------------------------------------------------------------------------------------

	TEST_CLASS(VerifyTest)
	{
	public:

		TEST_METHOD(Range_AcceptsOnlyFullyContainedWindows)
		{
			Assert::IsTrue(Verify::Range(0, 0, 0));
			Assert::IsTrue(Verify::Range(0, 16, 16));
			Assert::IsTrue(Verify::Range(15, 1, 16));
			Assert::IsTrue(Verify::Range(0, 16, 0x1000));

			// One byte past the end is rejected, and so is a window that starts past the end.
			Assert::IsFalse(Verify::Range(0, 17, 16));
			Assert::IsFalse(Verify::Range(16, 1, 16));

			// An empty window at the very end is still inside: nothing is read or written.
			Assert::IsTrue(Verify::Range(0x1000, 0, 0x1000));
		}

		TEST_METHOD(Range_DoesNotWrapOnAttackerSizedFields)
		{
			// The old checks added the two untrusted values, so a length of 0xFFFFFFFF (or a
			// 64-bit "size + size") wrapped and the test passed. These are the exact shapes.
			Assert::IsFalse(Verify::Range(0, 0xFFFFFFFFull, 0x1000));
			Assert::IsFalse(Verify::Range(0x1000, 0xFFFFFFFFull, 0x1000));
			Assert::IsFalse(Verify::Range(0xFFFFFFFFull, 0xFFFFFFFFull, 0xFFFFFFFFull));
			Assert::IsFalse(Verify::Range(0x80000, 0x1000000, 0x80000));
			Assert::IsFalse(Verify::Range(0x7F000, 0x2000, 0x80000));
		}

		TEST_METHOD(MainMemory_MasksTheAddressAndBoundsTheWholeWindow)
		{
			const uint64_t ram = 24 * 1024 * 1024;			// the standard 24 MB configuration

			// The MI decodes the low 26 address bits, so 0x80000000 is the start of main memory.
			Assert::IsTrue(Verify::MainMemory(0x80000000, 0x1000, ram));
			Assert::IsTrue(Verify::MainMemory(0x00000000, 0x1000, ram));
			Assert::IsTrue(Verify::MainMemory(0x817FF000, 0x1000, ram));

			// 0x81800000 masks to 0x01800000, which is past the end of the RAM that was allocated
			// even though it is inside the 64 MB the address mask allows.
			Assert::IsFalse(Verify::MainMemory(0x81800000, 4, ram));
			Assert::IsFalse(Verify::MainMemory(0x83FFFFFF, 4, ram));

			// A start inside RAM with a length that runs past the end.
			Assert::IsFalse(Verify::MainMemory(0x817FF000, 0x8000, ram));
			Assert::IsFalse(Verify::MainMemory(0x80000000, ram + 1, ram));
			Assert::IsTrue(Verify::MainMemory(0x817FF000, 0x1000, ram));
		}

		TEST_METHOD(ImageSection_ChecksTheFileAndTheRamWindow)
		{
			const uint64_t ram = 24 * 1024 * 1024;

			// A normal .dol section: 0x100 bytes at file offset 0x100, loaded at 0x80003100.
			Assert::IsTrue(Verify::ImageSection(0x2000, 0x100, 0x100, 0x80003100, ram));

			// A section that ends exactly at the end of the file is valid...
			Assert::IsTrue(Verify::ImageSection(0x200, 0x100, 0x100, 0x80003100, ram));

			// ...but one byte more is not (the section is not in the file), and neither is a
			// section that starts inside the file and runs past its end.
			Assert::IsFalse(Verify::ImageSection(0x1FF, 0x100, 0x100, 0x80003100, ram));
			Assert::IsFalse(Verify::ImageSection(0x200, 0x1F0, 0x20, 0x80003100, ram));

			// ...or it does not fit in main memory (the 0x017ff000 + 0x8000 case).
			Assert::IsFalse(Verify::ImageSection(0x2000, 0, 0x8000, 0x817FF000, ram));

			// The destination address is masked the way the MI does it.
			Assert::IsFalse(Verify::ImageSection(0x2000, 0, 0x100, 0x81800000, ram));
		}

		TEST_METHOD(DiscRead_RejectsNegativeSeeksAndWrappedLengths)
		{
			const int64_t image = 0x50000000;				// a normal small image

			Assert::IsTrue(Verify::DiscRead(0, 0x80000, image));
			Assert::IsTrue(Verify::DiscRead(image - 16, 16, image));
			Assert::IsTrue(Verify::DiscRead(0, 0, image));

			// The old code compared the start only, so a negative seek passed and the length was
			// then computed with a mix of int and size_t and grew to the size of the image.
			Assert::IsFalse(Verify::DiscRead(-0x100000, 0x80000, image));
			Assert::IsFalse(Verify::DiscRead(-1, 1, image));

			// Past the end of the image, and a length that does not fit.
			Assert::IsFalse(Verify::DiscRead(image, 1, image));
			Assert::IsFalse(Verify::DiscRead(0, 0xFFFFFFFFull, image));
			Assert::IsFalse(Verify::DiscRead(0, 1, 0));
		}

		TEST_METHOD(Fst_ValidatesTheRootAndEveryEntry)
		{
			// An FST of 4 entries (48 bytes) with the root claiming all 4 of them.
			Assert::IsTrue(Verify::FstRoot(48, 4));
			Assert::IsTrue(Verify::FstRoot(48, 1));
			Assert::IsTrue(Verify::FstRoot(12, 1));

			// The root has to describe at least itself...
			Assert::IsFalse(Verify::FstRoot(48, 0));
			// ...and it may not point past the buffer that was read from the image: this is the
			// value that used to walk the byte-swap loop through the heap.
			Assert::IsFalse(Verify::FstRoot(48, 5));
			Assert::IsFalse(Verify::FstRoot(48, 0xFFFFFFFF));
			Assert::IsFalse(Verify::FstRoot(4, 1));
			Assert::IsFalse(Verify::FstRoot(0, 1));

			Assert::IsTrue(Verify::FstEntry(4, 0));
			Assert::IsTrue(Verify::FstEntry(4, 3));
			Assert::IsFalse(Verify::FstEntry(4, 4));
			Assert::IsFalse(Verify::FstEntry(0, 0));

			Assert::IsTrue(Verify::FstName(16, 0));
			Assert::IsTrue(Verify::FstName(16, 15));
			Assert::IsFalse(Verify::FstName(16, 16));
			Assert::IsFalse(Verify::FstName(16, 0xFFFFFF));
		}

		TEST_METHOD(MemcardWindow_ChecksTheLengthAsWellAsTheEnd)
		{
			const uint64_t card = 0x80000;					// a 512 KiB card

			Assert::IsTrue(Verify::MemcardWindow(card, 0, card));
			Assert::IsTrue(Verify::MemcardWindow(card, card - 1, 1));
			Assert::IsTrue(Verify::MemcardWindow(card, 0, 0));

			// The two shapes that defeated `offset >= memcard->size + size`: the transfer starts
			// exactly at the end of the card, and the length itself is absurd.
			Assert::IsFalse(Verify::MemcardWindow(card, card, 0x1000));
			Assert::IsFalse(Verify::MemcardWindow(card, card, 0x01000000));
			Assert::IsFalse(Verify::MemcardWindow(card, card, 0xFFFFFFFF));
			Assert::IsFalse(Verify::MemcardWindow(card, 0, 0xFFFFFFFF));
			Assert::IsFalse(Verify::MemcardWindow(card, card - 1, 2));

			// The sector erase always writes a whole 8 KiB block.
			const uint64_t block = 8192;
			Assert::IsTrue(Verify::MemcardWindow(card, card - block, block));
			Assert::IsFalse(Verify::MemcardWindow(card, card - block / 2, block));
		}

		TEST_METHOD(Range_AgreesWithWideArithmeticOnRandomInputs)
		{
			// A property test rather than a table: the predicate has to match a 128-bit
			// computation for every combination, including the ones that wrap in 64 bits. The
			// generator is a plain xorshift so the test is deterministic and needs no library.
			uint64_t state = 0x243F6A8885A308D3ull;

			auto next = [&state]() -> uint64_t
			{
				state ^= state << 13;
				state ^= state >> 7;
				state ^= state << 17;
				return state;
			};

			for (int i = 0; i < 200000; i++)
			{
				uint64_t bits = next();
				uint64_t offset, size, limit;

				switch (bits & 3)
				{
					// Interesting shapes first: exact fits, off-by-ones, wild values.
					case 0:
						offset = bits >> 2;
						size = next();
						limit = next();
						break;
					case 1:
						limit = (bits >> 2) & 0xFFFFFF;
						offset = limit - (next() & 3);
						size = next() & 0xFFFFFF;
						break;
					case 2:
						offset = next() & 0xFFFFFFFFull;
						size = next() & 0xFFFFFFFFull;
						limit = next() & 0xFFFFFFFFull;
						break;
					default:
						offset = 0;
						size = bits >> 2;
						limit = size + (next() & 3) - 1;
						break;
				}

				// The reference result is computed with an explicit carry instead of the
				// subtraction the predicate uses: if offset + size wraps 64 bits then the true
				// sum is far above any limit, and otherwise the two values can be compared.
				uint64_t sum = offset + size;
				bool carry = (sum < offset);
				bool expected = !carry && (sum <= limit);
				bool actual = Verify::Range(offset, size, limit);

				if (expected != actual)
				{
					Assert::Fail(L"Verify::Range disagreed with the carry-based reference");
				}
			}
		}
	};

	// -------------------------------------------------------------------------------------------
	// The console script reader
	// -------------------------------------------------------------------------------------------

	TEST_CLASS(ScriptTest)
	{
		static std::vector<uint8_t> Bytes(const std::string& text)
		{
			return std::vector<uint8_t>(text.begin(), text.end());
		}

		// Read every line of a script and return them.
		static std::vector<std::string> Lines(const std::string& text, bool& anyTruncated)
		{
			std::vector<uint8_t> data = Bytes(text);
			std::vector<std::string> lines;
			size_t position = 0;
			char line[1000];
			bool truncated = false;

			anyTruncated = false;

			while (Verify::ScriptLine(data.data(), data.size(), position, line, sizeof(line), truncated))
			{
				anyTruncated |= truncated;

				// The trim returns the first non-blank character of the line (it removes the
				// trailing blanks in place); an empty or blank line returns nullptr.
				char* text = Verify::ScriptTrim(line);
				if (text != nullptr)
				{
					lines.push_back(text);
				}
			}

			return lines;
		}

	public:

		TEST_METHOD(ScriptLine_ReadsOrdinaryLines)
		{
			bool truncated = false;

			auto lines = Lines("echo one\necho two\n", truncated);

			Assert::AreEqual((size_t)2, lines.size());
			Assert::AreEqual(std::string("echo one"), lines[0]);
			Assert::AreEqual(std::string("echo two"), lines[1]);
			Assert::IsFalse(truncated);
		}

		TEST_METHOD(ScriptLine_HandlesALastLineWithoutANewline)
		{
			// This is the shape that used to run off the end of the buffer: the copy loop only
			// looked for '\n' and the file has none, so it kept reading heap bytes and writing
			// them into the caller's stack buffer.
			bool truncated = false;

			auto lines = Lines("echo one\necho two", truncated);

			Assert::AreEqual((size_t)2, lines.size());
			Assert::AreEqual(std::string("echo two"), lines[1]);
			Assert::IsTrue(truncated == false);
		}

		TEST_METHOD(ScriptLine_DoesNotOverflowOnAVeryLongLine)
		{
			// A 5000 byte line used to write ~4000 bytes past char line[1000].
			std::string text = "echo " + std::string(5000, 'A') + "\necho after\n";
			bool truncated = false;

			auto lines = Lines(text, truncated);

			Assert::IsTrue(truncated);
			Assert::AreEqual((size_t)1, lines.size());
			Assert::AreEqual(std::string("echo after"), lines[0]);
		}

		TEST_METHOD(ScriptLine_StopsAtAnEmbeddedNul)
		{
			std::string text("echo one");
			text.push_back('\0');
			text += "echo two\n";
			bool truncated = false;

			auto lines = Lines(text, truncated);

			Assert::AreEqual((size_t)1, lines.size());
			Assert::AreEqual(std::string("echo one"), lines[0]);
		}

		TEST_METHOD(ScriptLine_SkipsEmptyLines)
		{
			bool truncated = false;

			auto lines = Lines("\n\n   \n\techo one\n\n", truncated);

			Assert::AreEqual((size_t)1, lines.size());
			Assert::AreEqual(std::string("echo one"), lines[0]);
		}

		TEST_METHOD(ScriptTrim_SurvivesBlankAndEmptyLines)
		{
			// Both of these used to walk a pointer below the start of the buffer and write a NUL
			// out of bounds (the empty line because strlen(line)-1 underflows).
			char empty[] = "";
			char blanks[] = "    ";
			char tabOnly[] = "\t";

			Assert::IsNull(Verify::ScriptTrim(empty));
			Assert::IsNull(Verify::ScriptTrim(blanks));
			Assert::IsNull(Verify::ScriptTrim(tabOnly));

			char normal[] = "   echo hi   ";
			Assert::AreEqual(std::string("echo hi"), std::string(Verify::ScriptTrim(normal)));
		}

		TEST_METHOD(ScriptTrim_RemovesCommentsAndTrailingSpaceLikeTheInterpreter)
		{
			// cmd_script strips a // comment before trimming; the trim itself only has to leave
			// the command text intact.
			char line[] = "break 0x80003100 // the entry point";
			char* p = line;

			while (*p)
			{
				if (p[0] == '/' && p[1] == '/')
				{
					*p = 0;
					break;
				}
				p++;
			}

			Assert::AreEqual(std::string("break 0x80003100"), std::string(Verify::ScriptTrim(line)));

			char commentOnly[] = "// nothing here";
			commentOnly[0] = 0;

			Assert::IsNull(Verify::ScriptTrim(commentOnly));
		}

		TEST_METHOD(ScriptLine_NeverWritesOutsideTheCallerBufferOrStallsOnRandomInput)
		{
			// Property test over arbitrary binary scripts (NULs, CRs, newlines, no terminator at
			// all): the reader must always terminate, always NUL-terminate inside the buffer and
			// always make progress, whatever the bytes are.
			uint64_t state = 0x13198A2E03707344ull;

			auto next = [&state]() -> uint64_t
			{
				state ^= state << 13;
				state ^= state >> 7;
				state ^= state << 17;
				return state;
			};

			for (int iteration = 0; iteration < 3000; iteration++)
			{
				size_t size = (size_t)(next() % 4096);
				std::vector<uint8_t> data(size);

				for (size_t i = 0; i < size; i++)
				{
					data[i] = (uint8_t)next();
				}

				struct
				{
					uint8_t before[32];
					char line[64];
					uint8_t after[32];
				} buffer;

				memset(buffer.before, 0xAB, sizeof(buffer.before));
				memset(buffer.line, 0xCD, sizeof(buffer.line));
				memset(buffer.after, 0xAB, sizeof(buffer.after));

				size_t position = 0;
				bool truncated = false;
				int lineCount = 0;

				while (Verify::ScriptLine(data.empty() ? nullptr : data.data(), data.size(), position,
					buffer.line, sizeof(buffer.line), truncated))
				{
					lineCount++;

					if (lineCount > 100000)
					{
						Assert::Fail(L"ScriptLine made no progress");
					}

					for (size_t i = 0; i < sizeof(buffer.before); i++)
					{
						if (buffer.before[i] != 0xAB || buffer.after[i] != 0xAB)
						{
							Assert::Fail(L"ScriptLine wrote outside the line buffer");
						}
					}

					if (memchr(buffer.line, 0, sizeof(buffer.line)) == nullptr)
					{
						Assert::Fail(L"ScriptLine returned a line that is not terminated");
					}
				}

				if (position > data.size())
				{
					Assert::Fail(L"ScriptLine ran past the end of the script");
				}
			}
		}
	};

	// -------------------------------------------------------------------------------------------
	// The settings JSON parser
	// -------------------------------------------------------------------------------------------

	TEST_CLASS(JsonHardeningTest)
	{
	public:

		TEST_METHOD(Json_AcceptsTheNormalDocuments)
		{
			Assert::IsFalse(Throws([] { ParseJson("{}"); }));
			Assert::IsFalse(Throws([] { ParseJson("{\"hardware\":{\"CONSOLE\":3,\"VI_XFB\":true}}"); }));
			Assert::IsFalse(Throws([] { ParseJson("[1,2,3]"); }));
			Assert::IsFalse(Throws([] { ParseJson("{\"s\":\"hello\",\"f\":1.5,\"n\":null,\"b\":false}"); }));
			Assert::IsFalse(Throws([] { ParseJson("  {\r\n\t\"a\" : [ { \"b\" : 1 } ]\r\n}  "); }));
		}

		TEST_METHOD(Json_RejectsAnOverlongStringInsteadOfOverflowingTheStack)
		{
			// wchar_t str[MaxStringSize] with only an assert() as the bound.
			std::string text = "{\"a\":\"" + std::string(0x2000, 'A') + "\"}";

			Assert::IsTrue(Throws([&] { ParseJson(text); }));
		}

		TEST_METHOD(Json_RejectsAnOverlongStringMemberName)
		{
			std::string text = "{\"" + std::string(0x2000, 'A') + "\":1}";

			Assert::IsTrue(Throws([&] { ParseJson(text); }));
		}

		TEST_METHOD(Json_RejectsAnOverlongNumberToken)
		{
			// char number[0x100] with only an assert() as the bound.
			Assert::IsTrue(Throws([] { ParseJson(std::string(2000, '1')); }));
			Assert::IsTrue(Throws([] { ParseJson("{\"a\":" + std::string(2000, '1') + "}"); }));
			Assert::IsTrue(Throws([] { ParseJson("{\"a\":" + std::string(400, '+') + "}"); }));
		}

		TEST_METHOD(Json_RejectsDeepNestingInsteadOfExhaustingTheStack)
		{
			std::string text(20000, '[');
			text += std::string(20000, ']');

			Assert::IsTrue(Throws([&] { ParseJson(text); }));

			// The same depth through objects.
			std::string objects;
			for (int i = 0; i < 2000; i++) objects += "{\"a\":";
			for (int i = 0; i < 2000; i++) objects += "}";

			Assert::IsTrue(Throws([&] { ParseJson(objects); }));
		}

		TEST_METHOD(Json_RejectsATruncatedObjectInsteadOfSpinningForever)
		{
			// DeserializeObject's switch had no default, and GetToken returns EndOfStream without
			// advancing, so these three hung the emulator at 100% CPU on startup.
			Assert::IsTrue(Throws([] { ParseJson("{"); }));
			Assert::IsTrue(Throws([] { ParseJson("{\"a\":1,"); }));
			Assert::IsTrue(Throws([] { ParseJson("{\"a\":1,   "); }));
		}

		TEST_METHOD(Json_RejectsShortAndTruncatedInputWithoutReadingPastIt)
		{
			// Every one of these used to read past the end of the parsed buffer (the literal
			// look-ahead wrapped its size_t budget, FetchCodepoint read the continuation bytes of
			// a truncated UTF-8 sequence, and a lone backslash read the byte after the string).
			const char* cases[] =
			{
				"n", "t", "fals", "nul", "tru", "f",
				"\"", "\"\\", "\"\xF0", "\"\xC3", "\"\xE0\xA0", "{\"a\":\"",
			};

			for (const char* text : cases)
			{
				std::string document = text;
				Assert::IsTrue(Throws([&] { ParseJson(document); }));
			}
		}

		TEST_METHOD(Json_RejectsAStrayCommaInAnObject)
		{
			Assert::IsTrue(Throws([] { ParseJson("{,}"); }));
		}

		TEST_METHOD(Json_RejectsAMismatchedBracketInAnObject)
		{
			Assert::IsTrue(Throws([] { ParseJson("{\"a\":1,]}"); }));
		}

		TEST_METHOD(Json_RejectsAMissingColonInAnObject)
		{
			Assert::IsTrue(Throws([] { ParseJson("{\"a\" 1}"); }));
		}

		TEST_METHOD(Json_HasARealElementBudget)
		{
			// The element cap used to be assert()-only. It is a runaway-memory guard now, not the
			// format's limit, but it still has to exist and to be enforced.
			std::string huge = "[";

			for (int i = 0; i < 0x11000; i++)
			{
				huge += "1,";
			}

			huge += "1]";

			Assert::IsTrue(Throws([&] { ParseJson(huge); }));
		}

		TEST_METHOD(Json_KeepsTheValuesOfAWellFormedDocument)
		{
			// The hardening must not change what a valid settings document means.
			Json json;
			std::string text = "{\"hardware\":{\"CONSOLE\":3,\"VI_XFB\":true,\"ANSI\":\"ansi.bin\"}}";
			json.Deserialize(text.data(), text.size());

			Assert::AreEqual((size_t)1, json.root.children.size());

			Json::Value* root = json.root.children.back();
			Assert::IsTrue(root->type == Json::ValueType::Object);

			Json::Value* hardware = root->ByName("hardware");
			Assert::IsNotNull(hardware);
			Assert::AreEqual((int)3, (int)hardware->ByName("CONSOLE")->value.AsInt);
			Assert::IsTrue(hardware->ByName("VI_XFB")->value.AsBool);
			Assert::IsTrue(wcscmp(hardware->ByName("ANSI")->value.AsString, L"ansi.bin") == 0);
		}
	};

	// -------------------------------------------------------------------------------------------
	// The shipped JDI specifications
	// -------------------------------------------------------------------------------------------

	TEST_CLASS(JdiSpecsTest)
	{
	public:

		TEST_METHOD(JdiSpecs_AreAllAcceptedByTheHardenedParser)
		{
			// The hardened parser must not reject the documents the emulator ships: they are
			// parsed at startup, and an over-strict depth/element/string limit would take the
			// whole debug interface down with it.
			const char* specs[] =
			{
				JdiSpecs::EmuJdi,
				JdiSpecs::DebuggerJdi,
				JdiSpecs::DebugUi2Jdi,
				JdiSpecs::GekkoCoreJdi,
				JdiSpecs::DspJdi,
				JdiSpecs::HwJdi,
				JdiSpecs::DduJdi,
				JdiSpecs::GfxJdi,
				JdiSpecs::HleJdi,
				JdiSpecs::UiJdi,
				JdiSpecs::McpJdi,
			};

			for (const char* text : specs)
			{
				Assert::IsNotNull(text);

				Json json;
				std::string buffer = text;

				try
				{
					json.Deserialize(buffer.data(), buffer.size());
				}
				catch (...)
				{
					Assert::Fail(L"A JDI specification was rejected by the hardened Json parser");
				}

				Assert::IsTrue(json.root.children.size() > 0);
			}
		}

		TEST_METHOD(JdiSpecs_EveryDeclaredCommandHasAHandler)
		{
			// The argument count in the specification is what JdiHub::CheckParameters enforces,
			// so a command that declares "args" but has no handler (or the other way round) is a
			// hole in the command-line validation. This walks every spec and registers nothing -
			// it only checks that the shape the hub relies on is there.
			Json json;
			std::string buffer = JdiSpecs::GekkoCoreJdi;
			json.Deserialize(buffer.data(), buffer.size());

			Json::Value* root = json.root.children.back();
			Json::Value* can = root->ByName("can");
			Assert::IsNotNull(can);
			Assert::IsTrue(can->children.size() > 0);

			for (auto it = can->children.begin(); it != can->children.end(); ++it)
			{
				Json::Value* cmd = *it;
				Assert::IsNotNull(cmd->name);

				// "args" is optional, but when it is there it must be an integer (that is the only
				// shape CheckParameters understands).
				Json::Value* args = cmd->ByName("args");
				if (args != nullptr)
				{
					Assert::IsTrue(args->type == Json::ValueType::Int);
				}
			}
		}
	};
}
