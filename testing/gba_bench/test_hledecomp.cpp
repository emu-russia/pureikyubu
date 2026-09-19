// The BIOS's data manglers - BitUnPack, LZ77, RL and the two delta filters - against the official
// BIOS itself.
//
// These are the functions a game's graphics and sampled sound come out of, so a mistake in one of
// them shows up as corrupted tiles or noise long after the call, and never at the call site. Every
// test here runs the same stream through the host implementation and through the official BIOS and
// compares the bytes they wrote; the LZ77 and RL streams are *encoded* from random data, so they
// are well formed and the comparison is about decoding rather than about not crashing.

#include "gba_test.h"
#include "hle_probe.h"

#include "gba.h"
#include "gba_hlebios.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace GBA;
using namespace GbaProbe;

namespace
{
	/// <summary>A tiny deterministic generator, so a failure can be reproduced from its case number.</summary>
	struct Random
	{
		uint32_t state;

		explicit Random(uint32_t seed) : state(seed) {}

		uint32_t Next()
		{
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			return state;
		}
	};

	const uint32_t FillBase = Output - 0x2000;		// a back reference can read behind the output
	const uint32_t FillSize = 0x2100;				// ... up to just past the compared output
	const uint32_t CompareSize = 96;				// more than any test writes, so an overrun shows

	uint8_t FillByte(uint32_t index)
	{
		return (uint8_t)(index * 7 + 3);
	}

	/// <summary>
	/// Give both machines the same memory around the source and the destination (a decoder reads
	/// arbitrary bytes when a stream points outside what it produced), then run the same SWI
	/// through each and compare what came out.
	/// </summary>
	bool Compare(SwiCaller& hle, SwiCaller& real, uint32_t swi, const std::vector<uint8_t>& source,
		const std::vector<uint8_t>& info, std::string& message)
	{
		for (SwiCaller* caller : { &hle, &real })
		{
			GbaBus& bus = caller->Bus();

			for (uint32_t i = 0; i < FillSize; i++)
				bus.Write8(FillBase + i, FillByte(i));

			for (size_t i = 0; i < source.size(); i++)
				bus.Write8(Source + i, source[i]);

			for (size_t i = 0; i < info.size(); i++)
				bus.Write8(Info + i, info[i]);

			for (uint32_t i = 0; i < CompareSize; i++)
				bus.Write8(Output + i, 0xEE);
		}

		if (!hle.Call(swi, Source, Output, info.empty() ? 0 : Info))
		{
			message = "the HLE call never returned";
			return false;
		}

		if (!real.Call(swi, Source, Output, info.empty() ? 0 : Info))
		{
			message = "the official BIOS call never returned";
			return false;
		}

		std::vector<uint8_t> mine = Bytes(hle.Bus(), Output, CompareSize);
		std::vector<uint8_t> theirs = Bytes(real.Bus(), Output, CompareSize);

		if (mine == theirs)
			return true;

		message = "HLE " + Dump(mine) + "\n    BIOS " + Dump(theirs);
		return false;
	}

	/// <summary>A valid LZ77 stream for `data`: literals and back references, packed into blocks.</summary>
	std::vector<uint8_t> EncodeLz77(const std::vector<uint8_t>& data, Random& random)
	{
		std::vector<uint8_t> out;
		out.push_back(0x10);
		out.push_back((uint8_t)(data.size() & 0xFF));
		out.push_back((uint8_t)((data.size() >> 8) & 0xFF));
		out.push_back((uint8_t)((data.size() >> 16) & 0xFF));

		size_t position = 0;

		while (position < data.size())
		{
			uint8_t flags = 0;
			std::vector<uint8_t> blocks;

			for (int bit = 0; bit < 8 && position < data.size(); bit++)
			{
				uint32_t window = (uint32_t)((position < 256) ? position : 256);
				size_t best = 0;
				size_t bestLength = 0;

				// Sometimes a zero displacement: GBATEK warns that the Vram variant cannot
				// handle it, which is exactly the interesting case to compare.
				if (position > 0 && (random.Next() % 8) == 0)
				{
					best = 0;
					bestLength = 3 + (random.Next() % 16);
				}
				else
				{
					for (uint32_t back = 1; back <= window; back++)
					{
						size_t length = 0;

						while (length < 18 && position + length < data.size() &&
							data[position + length] == data[position - back + length])
						{
							length++;
						}

						if (length > bestLength)
						{
							bestLength = length;
							best = back;
						}
					}
				}

				if (bestLength >= 3)
				{
					flags |= (uint8_t)(0x80 >> bit);

					uint32_t displacement = (uint32_t)(best == 0 ? 0 : best - 1);
					blocks.push_back((uint8_t)(((bestLength - 3) << 4) | ((displacement >> 8) & 0xF)));
					blocks.push_back((uint8_t)(displacement & 0xFF));
					position += bestLength;
				}
				else
				{
					blocks.push_back(data[position++]);
				}
			}

			out.push_back(flags);
			out.insert(out.end(), blocks.begin(), blocks.end());
		}

		return out;
	}

	/// <summary>A valid run-length stream for `data`: runs of three or more, literals otherwise.</summary>
	std::vector<uint8_t> EncodeRl(const std::vector<uint8_t>& data, Random& random)
	{
		std::vector<uint8_t> out;
		out.push_back(0x30);
		out.push_back((uint8_t)(data.size() & 0xFF));
		out.push_back((uint8_t)((data.size() >> 8) & 0xFF));
		out.push_back((uint8_t)((data.size() >> 16) & 0xFF));

		size_t position = 0;

		while (position < data.size())
		{
			size_t run = 1;

			while (position + run < data.size() && data[position + run] == data[position] && run < 130)
				run++;

			if (run >= 3)
			{
				// One flag byte is one run: bit 7 set and bits 0-6 the length minus three.
				out.push_back((uint8_t)(0x80 | (run - 3)));
				out.push_back(data[position]);
				position += run;
			}
			else
			{
				// Literals, up to 128 of them: bit 7 clear and bits 0-6 the length minus one.
				size_t start = position;

				while (position < data.size() && position - start < 128)
				{
					size_t ahead = 1;

					while (position + ahead < data.size() && data[position + ahead] == data[position] &&
						ahead < 3)
					{
						ahead++;
					}

					if (ahead >= 3)
						break;

					position++;
				}

				out.push_back((uint8_t)((position - start) - 1));
				out.insert(out.end(), data.begin() + start, data.begin() + position);
			}
		}

		return out;
	}

	/// <summary>Filter `data` the way the delta functions expect it: the first unit absolute.</summary>
	std::vector<uint8_t> EncodeDiff(const std::vector<uint8_t>& data, int width)
	{
		std::vector<uint8_t> out;
		out.push_back((uint8_t)(0x80 | width));
		out.push_back((uint8_t)(data.size() & 0xFF));
		out.push_back((uint8_t)((data.size() >> 8) & 0xFF));
		out.push_back((uint8_t)((data.size() >> 16) & 0xFF));

		if (width == 1)
		{
			for (size_t i = 0; i < data.size(); i++)
			{
				uint8_t previous = (i == 0) ? 0 : data[i - 1];
				out.push_back((uint8_t)(data[i] - previous));
			}
		}
		else
		{
			for (size_t i = 0; i + 1 < data.size(); i += 2)
			{
				uint16_t value = (uint16_t)(data[i] | (data[i + 1] << 8));
				uint16_t previous = (i == 0) ? 0 :
					(uint16_t)(data[i - 2] | (data[i - 1] << 8));

				value = (uint16_t)(value - previous);
				out.push_back((uint8_t)(value & 0xFF));
				out.push_back((uint8_t)(value >> 8));
			}
		}

		return out;
	}

	/// <summary>The BitUnPack parameter block: length, widths, and the offset with its zero flag.</summary>
	std::vector<uint8_t> PackInfo(uint32_t length, uint8_t sourceWidth, uint8_t destWidth,
		uint32_t offsetField)
	{
		return
		{
			(uint8_t)(length & 0xFF), (uint8_t)(length >> 8),
			sourceWidth, destWidth,
			(uint8_t)(offsetField & 0xFF), (uint8_t)((offsetField >> 8) & 0xFF),
			(uint8_t)((offsetField >> 16) & 0xFF), (uint8_t)((offsetField >> 24) & 0xFF),
		};
	}

	std::vector<uint8_t> RandomBytes(Random& random, uint32_t count)
	{
		std::vector<uint8_t> out;
		for (uint32_t i = 0; i < count; i++)
			out.push_back((uint8_t)random.Next());
		return out;
	}

	bool HaveBios(const char* what)
	{
		if (!FindBiosImage().empty())
			return true;

		GbaTest::Note(std::string("no real BIOS image in testing/gba_bench/bios/ - ") + what +
			" is skipped (put your own gba_bios.bin there to run it)");
		return false;
	}
}

GBA_TEST(HleBios, Lz77MatchesTheOfficialBios)
{
	if (!HaveBios("the LZ77 comparison"))
		return;

	SwiCaller hle(false);
	SwiCaller real(true);
	Random random(0x5AC0FFEEu);

	// The shapes the disassembly turns on: a literal, a back reference, a zero displacement (which
	// the Vram variant cannot copy because the byte is still in its accumulator), and a block that
	// reaches past the declared size.
	struct Case { const char* name; std::vector<uint8_t> stream; };

	std::vector<Case> cases;

	cases.push_back({ "one literal", { 0x10, 1, 0, 0, 0x00, 0x41 } });
	cases.push_back({ "a back reference",
		{ 0x10, 16, 0, 0, 0xA0, 0x00, 0x00, 0x41, 0x50, 0x00 } });
	cases.push_back({ "eight literals",
		{ 0x10, 8, 0, 0, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 } });
	cases.push_back({ "a block past the end",			// five bytes copied into a four byte size
		{ 0x10, 4, 0, 0, 0x80, 0x20, 0x00, 0x41 } });

	for (const Case& c : cases)
	{
		std::string message;
		for (uint32_t swi : { SwiLz77UnCompWram, SwiLz77UnCompVram })
		{
			bool same = Compare(hle, real, swi, c.stream, {}, message);
			GBA_CHECK_MSG(same, std::string(c.name) + " (SWI " + GbaTest::Hex(swi) + "): " + message);
		}
	}

	// Random data, encoded into a valid stream: 200 of them through both write variants.
	int failures = 0;

	for (int iteration = 0; iteration < 200; iteration++)
	{
		std::vector<uint8_t> data = RandomBytes(random, 8 + (random.Next() % 56));
		std::vector<uint8_t> stream = EncodeLz77(data, random);

		for (uint32_t swi : { SwiLz77UnCompWram, SwiLz77UnCompVram })
		{
			std::string message;

			if (!Compare(hle, real, swi, stream, {}, message))
			{
				failures++;

				if (failures <= 3)
				{
					GbaTest::Note("case " + std::to_string(iteration) + " (SWI " +
						GbaTest::Hex(swi) + "): " + message);
				}
			}
		}
	}

	GBA_CHECK_MSG(failures == 0, std::to_string(failures) + " of 400 random LZ77 streams decoded "
		"differently");
}

GBA_TEST(HleBios, RlMatchesTheOfficialBios)
{
	if (!HaveBios("the run-length comparison"))
		return;

	SwiCaller hle(false);
	SwiCaller real(true);
	Random random(0x1234ABCDu);

	// A run, a literal group, and a run that reaches past the declared size (the flag byte is one
	// token, so the BIOS copies all of it).
	std::vector<std::vector<uint8_t>> cases =
	{
		{ 0x30, 6, 0, 0, 0x83, 0x11 },						// six copies of 11h
		{ 0x30, 4, 0, 0, 0x03, 0x41, 0x42, 0x43, 0x44 },	// four literals
		{ 0x30, 2, 0, 0, 0x80, 0x77 },						// three copies, past the size
		{ 0x30, 8, 0, 0, 0x00, 0x01, 0x80, 0x02 },			// a literal then a run
	};

	for (size_t i = 0; i < cases.size(); i++)
	{
		std::string message;
		for (uint32_t swi : { SwiRlUnCompWram, SwiRlUnCompVram })
		{
			bool same = Compare(hle, real, swi, cases[i], {}, message);
			GBA_CHECK_MSG(same, "case " + std::to_string(i) + " (SWI " + GbaTest::Hex(swi) + "): " +
				message);
		}
	}

	int failures = 0;

	for (int iteration = 0; iteration < 200; iteration++)
	{
		std::vector<uint8_t> data = RandomBytes(random, 4 + (random.Next() % 60));

		// Runs are what the format is for: make the data mostly runs of a few values.
		for (size_t i = 0; i < data.size(); i++)
			data[i] = (uint8_t)(random.Next() % 3 ? data[i] : (random.Next() % 5));

		std::vector<uint8_t> stream = EncodeRl(data, random);

		for (uint32_t swi : { SwiRlUnCompWram, SwiRlUnCompVram })
		{
			std::string message;

			if (!Compare(hle, real, swi, stream, {}, message))
			{
				failures++;

				if (failures <= 3)
				{
					GbaTest::Note("case " + std::to_string(iteration) + " (SWI " +
						GbaTest::Hex(swi) + "): " + message);
				}
			}
		}
	}

	GBA_CHECK_MSG(failures == 0, std::to_string(failures) + " of 400 random run-length streams "
		"decoded differently");
}

GBA_TEST(HleBios, DiffFiltersMatchTheOfficialBios)
{
	if (!HaveBios("the delta filter comparison"))
		return;

	SwiCaller hle(false);
	SwiCaller real(true);
	Random random(0x0DDBA11u);

	// The 8bit filters accumulate in 8 bits and the 16bit one in halfwords: a stream whose deltas
	// carry into the high byte is what tells the two apart.
	std::vector<uint8_t> sixteen = { 0x82, 16, 0, 0, 0x34, 0x12, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00,
		0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00 };
	std::vector<uint8_t> eight = { 0x81, 16, 0, 0, 0x10, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
		0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02 };

	std::string message;
	GBA_CHECK_MSG(Compare(hle, real, SwiDiff16bitUnFilter, sixteen, {}, message),
		"the 16bit filter with a carry: " + message);

	for (uint32_t swi : { SwiDiff8bitUnFilterWram, SwiDiff8bitUnFilterVram })
	{
		GBA_CHECK_MSG(Compare(hle, real, swi, eight, {}, message),
			"the 8bit filter (SWI " + GbaTest::Hex(swi) + "): " + message);
	}

	int failures = 0;

	for (int iteration = 0; iteration < 200; iteration++)
	{
		uint32_t count = 1 + (random.Next() % 33);				// odd and even sizes both matter
		std::vector<uint8_t> eightData = RandomBytes(random, count);
		std::vector<uint8_t> sixteenData = RandomBytes(random, (count / 2 + 1) * 2);

		struct Run { uint32_t swi; std::vector<uint8_t> stream; };

		std::vector<Run> runs =
		{
			{ SwiDiff8bitUnFilterWram, EncodeDiff(eightData, 1) },
			{ SwiDiff8bitUnFilterVram, EncodeDiff(eightData, 1) },
			{ SwiDiff16bitUnFilter, EncodeDiff(sixteenData, 2) },
		};

		for (const Run& run : runs)
		{
			if (Compare(hle, real, run.swi, run.stream, {}, message))
				continue;

			failures++;

			if (failures <= 3)
			{
				GbaTest::Note("case " + std::to_string(iteration) + ", " + std::to_string(count) +
					" bytes (SWI " + GbaTest::Hex(run.swi) + "): " + message);
			}
		}
	}

	GBA_CHECK_MSG(failures == 0, std::to_string(failures) + " of 600 random filtered streams "
		"decoded differently");
}

GBA_TEST(HleBios, BitUnPackMatchesTheOfficialBios)
{
	if (!HaveBios("the BitUnPack comparison"))
		return;

	SwiCaller hle(false);
	SwiCaller real(true);
	Random random(0xB17B17u);

	// The offset is added to non-zero units, and setting its top bit makes it apply to zero units
	// as well - the official BIOS reads that bit out of the offset word itself, not from a byte of
	// its own: `mov r8, r11, lsr #31` followed by `lsl #1 / lsr #1` on the same word.
	std::vector<uint8_t> source = { 0xA5, 0x0F };

	struct Case { uint32_t length; uint8_t sourceWidth; uint8_t destWidth; uint32_t offset; };
	const Case cases[] =
	{
		{ 2, 1, 4, 1 },
		{ 2, 1, 4, 1 | 0x80000000u },
		{ 2, 2, 4, 3 },
		{ 2, 4, 8, 0x10 },
		{ 4, 8, 16, 0x1234 },
		{ 2, 1, 32, 0 },
		{ 3, 4, 4, 0 },						// three bytes: the last word needs six units to complete
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		const Case& c = cases[i];
		std::vector<uint8_t> info = PackInfo(c.length, c.sourceWidth, c.destWidth, c.offset);
		std::string message;

		bool same = Compare(hle, real, SwiBitUnPack, source, info, message);
		GBA_CHECK_MSG(same, "case " + std::to_string(i) + ": " + message);
	}

	int failures = 0;

	for (int iteration = 0; iteration < 200; iteration++)
	{
		static const uint8_t sourceWidths[] = { 1, 2, 4, 8 };
		static const uint8_t destWidths[] = { 1, 2, 4, 8, 16, 32 };

		uint32_t length = 1 + (random.Next() % 16);
		uint8_t sourceWidth = sourceWidths[random.Next() % 4];
		uint8_t destWidth = destWidths[random.Next() % 6];
		uint32_t offset = random.Next() & 0x7FFFFFFFu;

		if (random.Next() % 2)
			offset |= 0x80000000u;

		std::vector<uint8_t> data = RandomBytes(random, length);
		std::vector<uint8_t> info = PackInfo(length, sourceWidth, destWidth, offset);
		std::string message;

		if (Compare(hle, real, SwiBitUnPack, data, info, message))
			continue;

		failures++;

		if (failures <= 3)
		{
			GbaTest::Note("case " + std::to_string(iteration) + " (" + std::to_string(sourceWidth) +
				" -> " + std::to_string(destWidth) + " bit, " + std::to_string(length) + " bytes, "
				"offset " + GbaTest::Hex(offset) + "): " + message);
		}
	}

	GBA_CHECK_MSG(failures == 0, std::to_string(failures) + " of 200 random BitUnPack calls "
		"differed");
}
