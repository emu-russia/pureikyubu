// A very small test framework for the GBA core tests.
//
// The GBA module has to be testable without Visual Studio (see testing/Readme.md: the rest of the
// emulator uses CppUnitTest), so the tests register themselves here and the runner in
// test_main.cpp runs them. The framework only does what the tests need: registration, a check
// that records a readable failure and aborts the failing test, and a per-test pass/fail summary.

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>

namespace GbaTest
{
	struct TestCase
	{
		const char* suite;
		const char* name;
		void (*fn)();
	};

	/// <summary>A failed check. Thrown to abort the test that failed.</summary>
	struct Failure
	{
		std::string message;
	};

	std::vector<TestCase>& Registry();

	inline int Register(const char* suite, const char* name, void (*fn)())
	{
		Registry().push_back(TestCase{ suite, name, fn });
		return 0;
	}

	struct Registrar
	{
		Registrar(const char* suite, const char* name, void (*fn)()) { Register(suite, name, fn); }
	};

	/// <summary>Fail the running test with a formatted message.</summary>
	[[noreturn]] void Fail(const char* file, int line, const std::string& message);

	/// <summary>Print a line that belongs to the running test.</summary>
	void Note(const std::string& message);

	/// <summary>A check-independent failure counter (for tests that count things themselves).</summary>
	int& FailureCount();
}

#define GBA_TEST(suite, name) \
	static void suite##_##name(); \
	static GbaTest::Registrar gba_reg_##suite##_##name(#suite, #name, &suite##_##name); \
	static void suite##_##name()

#define GBA_CHECK(condition) \
	do { if (!(condition)) { GbaTest::Fail(__FILE__, __LINE__, std::string("check failed: ") + #condition); } } while (0)

#define GBA_CHECK_MSG(condition, message) \
	do { if (!(condition)) { GbaTest::Fail(__FILE__, __LINE__, std::string("check failed: ") + #condition + " (" + (message) + ")"); } } while (0)

namespace GbaTest
{
	inline std::string Hex(uint64_t value)
	{
		char text[32];
		snprintf(text, sizeof(text), "0x%llX", (unsigned long long)value);
		return text;
	}
}

#define GBA_CHECK_EQ(actual, expected) \
	do { \
		auto gba_actual = (actual); \
		auto gba_expected = (expected); \
		if (!(gba_actual == gba_expected)) \
		{ \
			GbaTest::Fail(__FILE__, __LINE__, std::string("expected ") + #actual + " == " + GbaTest::Hex((uint64_t)gba_expected) + \
				", got " + GbaTest::Hex((uint64_t)gba_actual)); \
		} \
	} while (0)

/// <summary>Compare two strings and print both of them when they differ (a disassembly, a path, a
/// name - anything GBA_CHECK_EQ cannot show as a number).</summary>
#define GBA_CHECK_STR(actual, expected) \
	do { \
		std::string gba_actual = (actual); \
		std::string gba_expected = (expected); \
		if (gba_actual != gba_expected) \
		{ \
			GbaTest::Fail(__FILE__, __LINE__, std::string("expected \"") + gba_expected + "\", got \"" + \
				gba_actual + "\""); \
		} \
	} while (0)

/// <summary>Compare two 16-bit words and print both in hexadecimal.</summary>
#define GBA_CHECK_HEX16(actual, expected) \
	do { \
		uint16_t gba_actual = (uint16_t)(actual); \
		uint16_t gba_expected = (uint16_t)(expected); \
		if (gba_actual != gba_expected) \
		{ \
			GbaTest::Fail(__FILE__, __LINE__, std::string("expected ") + #actual + " == " + GbaTest::Hex(gba_expected) + \
				", got " + GbaTest::Hex(gba_actual)); \
		} \
	} while (0)

/// <summary>Compare two 32-bit values and print both in hexadecimal.</summary>
#define GBA_CHECK_HEX32(actual, expected) \
	do { \
		uint32_t gba_actual = (uint32_t)(actual); \
		uint32_t gba_expected = (uint32_t)(expected); \
		if (gba_actual != gba_expected) \
		{ \
			GbaTest::Fail(__FILE__, __LINE__, std::string("expected ") + #actual + " == " + GbaTest::Hex(gba_expected) + \
				", got " + GbaTest::Hex(gba_actual)); \
		} \
	} while (0)

#define GBA_FAIL(message) GbaTest::Fail(__FILE__, __LINE__, (message))
