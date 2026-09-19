// The high level BIOS calls (HLE) against the official BIOS itself.
//
// The emulator cannot ship the official IPL, so its service calls are implemented in the host
// (`src/gba/gba_hlebios.cpp`). That makes the official image the perfect reference: when the user
// has one in `testing/gba_bench/bios/`, these tests call the *real* function through its own SWI
// dispatcher and compare, byte for byte. The image is copyrighted and not in the repository, so
// every test that needs it says so and is skipped without it; the tests that only need the
// specification run either way.

#include "gba_test.h"
#include "hle_probe.h"

#include "gba.h"
#include "gba_hlebios.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace GBA;

namespace
{
	using namespace GbaProbe;

	/// <summary>
	/// A well-formed Huffman stream: a tree whose root has both children as data, and the given
	/// bitstream. The tree table is one node plus two data bytes, which is the 4 byte table whose
	/// "(size / 2) - 1" header byte is 1, so the bitstream starts at
	/// source + 4 + (1 + 1) * 2 = source + 8 - the first address after the table (the BIOS's own
	/// arithmetic, which is `add r0, r2, r10, lsl #1` with r2 = source + 4).
	/// </summary>
	void BuildHuffmanStream(GbaBus& bus, uint32_t source, uint32_t bitstream, uint32_t outBytes,
		uint8_t node0, uint8_t node1, uint32_t unitBits = 8)
	{
		for (uint32_t i = 0; i < 64; i++)
			bus.Write8(source + i, 0);

		uint32_t header = 0x20 | unitBits | (outBytes << 8);
		bus.Write32(source + 0, header);
		bus.Write8(source + 4, 1);					// (tree table / 2) - 1
		bus.Write8(source + 5, 0xC0);				// the root: both children are data
		bus.Write8(source + 6, node0);				// child0 = (addr AND NOT 1) + 0 * 2 + 2
		bus.Write8(source + 7, node1);				// child1
		bus.Write32(source + 8, bitstream);
	}

	/// <summary>
	/// The timer 0 reload the driver programmed: TMxCNT_L reads back as the *counter*, which keeps
	/// counting between the call and the read, so the timer is stopped and enabled again first -
	/// enabling a GBA timer loads the counter from the reload.
	/// </summary>
	uint16_t Timer0Reload(GbaBus& bus)
	{
		bus.Write16(0x04000102, 0x0000);
		bus.Write16(0x04000102, 0x0080);
		return bus.Read16(0x04000100);
	}
}

// -------------------------------------------------------------------------------------------
// The SWI numbers
// -------------------------------------------------------------------------------------------

GBA_TEST(HleBios, TheSwiNumbersAreTheOfficialOnes)
{
	// GBATEK "GBA BIOS Functions": the sound driver lives at 1Ah..1Fh and 28h/29h. An earlier
	// version of the header had the whole block at 28h..2Fh and called 1Ah "DivArm2", so a game's
	// SoundDriverInit was answered with a *division* and its music never started.
	GBA_CHECK_EQ((int)SwiSoundBias, 0x19);
	GBA_CHECK_EQ((int)SwiSoundDriverInit, 0x1A);
	GBA_CHECK_EQ((int)SwiSoundDriverMode, 0x1B);
	GBA_CHECK_EQ((int)SwiSoundDriverMain, 0x1C);
	GBA_CHECK_EQ((int)SwiSoundDriverVSync, 0x1D);
	GBA_CHECK_EQ((int)SwiSoundChannelClear, 0x1E);
	GBA_CHECK_EQ((int)SwiMidiKey2Freq, 0x1F);
	GBA_CHECK_EQ((int)SwiMultiBoot, 0x25);
	GBA_CHECK_EQ((int)SwiHardReset, 0x26);
	GBA_CHECK_EQ((int)SwiCustomHalt, 0x27);
	GBA_CHECK_EQ((int)SwiSoundDriverVSyncOff, 0x28);
	GBA_CHECK_EQ((int)SwiSoundDriverVSyncOn, 0x29);
	GBA_CHECK_EQ((int)SwiSoundGetJumpList, 0x2A);

	// DivArm (07h) is the only arm-swapped divide the BIOS has.
	GBA_CHECK_EQ((int)SwiDiv, 0x06);
	GBA_CHECK_EQ((int)SwiDivArm, 0x07);
}

GBA_TEST(HleBios, TheSoundDriverCallsAreImplemented)
{
	HleBios::Reset();

	const uint32_t numbers[] = { SwiSoundDriverInit, SwiSoundDriverMode, SwiSoundDriverMain,
		SwiSoundDriverVSync, SwiSoundChannelClear, SwiMidiKey2Freq, SwiSoundDriverVSyncOff,
		SwiSoundDriverVSyncOn, SwiHuffUnComp };

	for (uint32_t number : numbers)
		GBA_CHECK_MSG(HleBios::Implemented(number),
			"SWI " + GbaTest::Hex(number) + " is not implemented");
}

// -------------------------------------------------------------------------------------------
// MidiKey2Freq
// -------------------------------------------------------------------------------------------

GBA_TEST(HleBios, MidiKey2FreqIsAnOctavePerTwelveKeys)
{
	// GBATEK 1Fh: the WaveData's `freq` is "sampling rate * 2^((180 - original key)/12)", so the
	// value for a key is that frequency scaled by 2^((mk - 180)/12) - which makes the reference
	// key 180 play the sample at its own rate and every twelve keys an octave. The fine value is
	// a fraction of a halftone, and the result is truncated to a whole number.
	SwiCaller caller(false);

	const uint32_t wave = Source;
	caller.Bus().Write32(wave + 0, 0);
	caller.Bus().Write32(wave + 4, 0x00010000);		// the frequency to scale

	// 2^((mk-180)/12) * 65536: one octave per 12 keys, and exact at the anchors.
	struct Anchor { uint32_t key; uint32_t expected; };
	const Anchor anchors[] =
	{
		{ 60, 64 },			// ten octaves down: 65536 / 1024
		{ 72, 128 },
		{ 84, 256 },
		{ 96, 512 },
		{ 108, 1024 },
	};

	for (const Anchor& a : anchors)
	{
		GBA_CHECK(caller.Call(SwiMidiKey2Freq, wave, a.key, 0));
		GBA_CHECK_MSG(caller.R0() == a.expected,
			"key " + std::to_string(a.key) + " gave " + GbaTest::Hex(caller.R0()) +
			", expected " + GbaTest::Hex(a.expected));
	}

	// The fine value only ever raises the frequency (it is a fraction of a halftone up).
	caller.Call(SwiMidiKey2Freq, wave, 84, 0);
	uint32_t plain = caller.R0();
	caller.Call(SwiMidiKey2Freq, wave, 84, 255);
	GBA_CHECK_MSG(caller.R0() > plain, "the fine value has to raise the frequency");
	GBA_CHECK_MSG(caller.R0() < plain * 2, "the fine value is a fraction of a halftone");
}

GBA_TEST(HleBios, MidiKey2FreqMatchesTheOfficialBios)
{
	if (FindBiosImage().empty())
	{
		GbaTest::Note("no real BIOS image - the comparison is skipped");
		return;
	}

	SwiCaller hle(false);
	SwiCaller real(true);

	const uint32_t frequencies[] = { 0x00010000, 0x0000C350, 0x00100000, 0x0BADF00D };

	for (uint32_t frequency : frequencies)
	{
		hle.Bus().Write32(Source + 4, frequency);
		real.Bus().Write32(Source + 4, frequency);

		for (uint32_t key = 0; key <= 127; key++)
		{
			// A whole key (no fine adjustment) has to match the BIOS's own fixed point arithmetic:
			// the HLE uses the BIOS's semitone table, reconstructed from its outputs, so the two
			// agree to within a unit per few thousand (the BIOS's table is 16.16 and its product is
			// truncated).
			GBA_CHECK(hle.Call(SwiMidiKey2Freq, Source, key, 0));
			GBA_CHECK(real.Call(SwiMidiKey2Freq, Source, key, 0));

			uint32_t mine = hle.R0();
			uint32_t theirs = real.R0();
			uint32_t difference = (mine > theirs) ? (mine - theirs) : (theirs - mine);

			GBA_CHECK_MSG(difference <= 4 || (uint64_t)difference * 512 <= theirs,
				"freq " + GbaTest::Hex(frequency) + " key " + std::to_string(key) + ": HLE " +
				GbaTest::Hex(mine) + " against the BIOS " + GbaTest::Hex(theirs));

			// With a fine adjustment the BIOS's own interpolation is its own fixed point
			// approximation: the two stay within a few units, well under a cent of pitch.
			for (uint32_t fine = 0; fine <= 255; fine += 51)
			{
				hle.Call(SwiMidiKey2Freq, Source, key, fine);
				real.Call(SwiMidiKey2Freq, Source, key, fine);

				uint32_t mine = hle.R0();
				uint32_t theirs = real.R0();
				uint32_t difference = (mine > theirs) ? (mine - theirs) : (theirs - mine);

				GBA_CHECK_MSG(difference <= 4 || (uint64_t)difference * 512 <= theirs,
					"freq " + GbaTest::Hex(frequency) + " key " + std::to_string(key) +
					" fine " + std::to_string(fine) + ": HLE " + GbaTest::Hex(mine) +
					" against the BIOS " + GbaTest::Hex(theirs));
			}
		}
	}
}

// -------------------------------------------------------------------------------------------
// The sound driver
// -------------------------------------------------------------------------------------------

GBA_TEST(HleBios, TheSoundDriverSetsUpTheFifos)
{
	// What the real BIOS does when a game calls SoundDriverInit and SoundDriverMode, as measured
	// with a probe against the official image: the work area is identified with 68736D53h, the
	// two FIFO DMAs stream the mixed buffer (two 0x630 byte halves at +0x350 and +0x980) into
	// FIFO A and FIFO B with CNT = B600h, SOUNDCNT_H is 210Eh and the timer 0 reload is the
	// BIOS's own value for the playback frequency index.
	SwiCaller caller(false);

	for (uint32_t i = 0; i < 0x1000; i++)
		caller.Bus().Write8(Work + i, 0xDE);

	GBA_CHECK(caller.Call(SwiSoundDriverInit, Work));

	// The identifier is both the return value and the first word of the area.
	GBA_CHECK_MSG(caller.R0() == 0x68736D53,
		"SoundDriverInit returned " + GbaTest::Hex(caller.R0()));
	GBA_CHECK_HEX32(caller.Bus().Read32(Work), 0x68736D53);

	// Everything the driver owns is cleared: the work area is 0xFB0 bytes (what the real BIOS
	// clears, measured), and the byte after it is left alone.
	GBA_CHECK_EQ((int)caller.Bus().Read8(Work + 0x400), 0);
	GBA_CHECK_EQ((int)caller.Bus().Read8(Work + 0xFB0 - 1), 0);
	GBA_CHECK_EQ((int)caller.Bus().Read8(Work + 0xFB0), 0xDE);

	// SOUNDCNT_H and SOUNDCNT_X, and the default playback frequency's timer reload.
	GBA_CHECK_HEX16(caller.Bus().Read16(0x04000082), 0x210E);
	GBA_CHECK_MSG((caller.Bus().Read16(0x04000084) & 0x0080) != 0,
		"the master enable bit has to be set");
	GBA_CHECK_MSG(Timer0Reload(caller.Bus()) >= 0xFB1A - 8 && Timer0Reload(caller.Bus()) <= 0xFB1A + 8,
		"the default frequency index (4 = 13379 Hz) wants the reload 0xFB1A, got " +
		GbaTest::Hex(Timer0Reload(caller.Bus())));
	GBA_CHECK_HEX16(caller.Bus().Read16(0x04000102), 0x0080);

	// The two FIFO channels: DMA1 -> FIFO A from the first half of the mixed buffer, DMA2 -> FIFO
	// B from the second one, both repeating with an incrementing source (B600h).
	GBA_CHECK_HEX32(caller.Bus().Read32(0x040000BC), Work + 0x350);
	GBA_CHECK_HEX32(caller.Bus().Read32(0x040000C0), 0x040000A0);
	GBA_CHECK_HEX16(caller.Bus().Read16(0x040000C6), 0xB600);
	GBA_CHECK_HEX32(caller.Bus().Read32(0x040000C8), Work + 0x980);
	GBA_CHECK_HEX32(caller.Bus().Read32(0x040000CC), 0x040000A4);
	GBA_CHECK_HEX16(caller.Bus().Read16(0x040000D2), 0xB600);

	// The mode picks the timer reload for its frequency index (index 4 and index 0 are the
	// default 13379 Hz, index 1 is 5734 Hz in the BIOS's own table).
	caller.Call(SwiSoundDriverMode, (8u << 8) | (15u << 12) | (1u << 16) | (9u << 20));
	uint16_t indexOne = Timer0Reload(caller.Bus());
	GBA_CHECK_MSG(indexOne >= 0xF492 - 8 && indexOne <= 0xF492 + 8,
		"frequency index 1 (5734 Hz) wants the reload 0xF492, got " + GbaTest::Hex(indexOne));

	// VSyncOff stops the two channels, VSyncOn arms them again.
	caller.Call(SwiSoundDriverVSyncOff);
	GBA_CHECK_HEX16(caller.Bus().Read16(0x040000C6), 0x0000);
	GBA_CHECK_HEX16(caller.Bus().Read16(0x040000D2), 0x0000);

	caller.Call(SwiSoundDriverVSyncOn);
	GBA_CHECK_HEX16(caller.Bus().Read16(0x040000C6), 0xB600);
	GBA_CHECK_HEX16(caller.Bus().Read16(0x040000D2), 0xB600);

	// Until the mixer writes into them, Main still fills a sub-buffer of the ring (the driver's
	// own layout): with 7 sub-buffers of 0xE0h bytes the first call after VSync lands at +0E0h,
	// and with no channels playing that sub-buffer is silence.
	uint32_t ringCount = caller.Bus().Read8(Work + 0x0B);
	uint32_t ringStride = caller.Bus().Read32(Work + 0x10);
	uint32_t ringDma = caller.Bus().Read8(Work + 4);
	if (ringDma == 0)
		ringDma = ringCount;
	uint32_t ringIndex = (ringCount + 1 - ringDma) % ringCount;
	uint32_t ringOffset = ringIndex * ringStride;

	caller.Bus().Write8(Work + 0x350 + ringOffset, 0x7F);
	caller.Bus().Write8(Work + 0x980 + ringOffset, 0x7F);
	caller.Call(SwiSoundDriverMain);
	GBA_CHECK_EQ((int)caller.Bus().Read8(Work + 0x350 + ringOffset), 0);
	GBA_CHECK_EQ((int)caller.Bus().Read8(Work + 0x980 + ringOffset), 0);
	GBA_CHECK_HEX16(caller.Bus().Read16(0x040000C6), 0xB600);
}

// -------------------------------------------------------------------------------------------
// The mixer
// -------------------------------------------------------------------------------------------

namespace
{
	/// <summary>
	/// One virtual channel of the driver's own array (16 entries of 0x30 bytes at work+0x50),
	/// playing a synthetic wave: a 64 sample ramp with no loop, so the mixed output is easy to
	/// predict. The field offsets are the ones the disassembled driver reads.
	/// </summary>
	void SetupChannel(GbaBus& bus, uint32_t work, uint32_t wave)
	{
		for (uint32_t i = 0; i < 128; i++)
			bus.Write8(wave + i, 0);

		bus.Write32(wave + 0, 0);					// type/stat: no loop
		bus.Write32(wave + 4, 13379);				// freq: the sample rate at its own key
		bus.Write32(wave + 8, 0);					// loop
		bus.Write32(wave + 12, 64);					// size
		for (uint32_t i = 0; i < 64; i++)
			bus.Write8(wave + 16 + i, (uint8_t)(0x10 + i));

		uint32_t channel = work + 0x50;

		bus.Write8(channel + 0x00, 0x80);			// sf: start
		bus.Write8(channel + 0x02, 0xFF);			// rv
		bus.Write8(channel + 0x03, 0xFF);			// lv
		bus.Write8(channel + 0x04, 0xFF);			// at
		bus.Write8(channel + 0x05, 0xFF);			// de
		bus.Write8(channel + 0x06, 0xFF);			// su
		bus.Write8(channel + 0x07, 0x00);			// re
		bus.Write32(channel + 0x20, 13379);			// fr: one sample per output sample
		bus.Write32(channel + 0x24, wave);			// wp
	}

	/// <summary>The sub-buffer the next Main fills, as bytes (the ring index the driver uses).</summary>
	std::vector<uint8_t> MixedOutput(GbaBus& bus, uint32_t work, uint32_t half, uint32_t bytes)
	{
		uint32_t count = bus.Read8(work + 0x0B);
		uint32_t stride = bus.Read32(work + 0x10);
		uint32_t dmaCount = bus.Read8(work + 4);
		if (dmaCount == 0)
			dmaCount = count;

		uint32_t index = (count + 1 - dmaCount) % count;
		return Bytes(bus, work + half + index * stride, bytes);
	}
}

GBA_TEST(HleBios, TheMixerMatchesTheOfficialBios)
{
	if (FindBiosImage().empty())
	{
		GbaTest::Note("no real BIOS image - the mixer comparison is skipped");
		return;
	}

	// The same synthetic channel through both mixers, then the mixed output byte for byte. This is
	// the strongest check the harness has for the sound driver: the official BIOS's own mixer is
	// the reference for the envelope, the volume scaling, the sample stepping and the ring.
	SwiCaller hle(false);
	SwiCaller real(true);

	for (SwiCaller* caller : { &hle, &real })
	{
		caller->Call(SwiSoundDriverInit, Work);
		caller->Call(SwiSoundDriverMode, 0x0094F800);
		SetupChannel(caller->Bus(), Work, Source);
		caller->Call(SwiSoundDriverVSync);
		caller->Call(SwiSoundDriverMain);
	}

	std::vector<uint8_t> mine = MixedOutput(hle.Bus(), Work, 0x350, 64);
	std::vector<uint8_t> theirs = MixedOutput(real.Bus(), Work, 0x350, 64);

	// The ramp has to come out as the ramp scaled by the channel's level, which is what makes the
	// comparison meaningful rather than two silences matching.
	GBA_CHECK_MSG(mine[0] != 0, "the HLE mixer produced silence: " + Dump(mine));
	GBA_CHECK_MSG(mine[0] == 0x0F,
		"the first mixed byte is " + GbaTest::Hex(mine[0]) + ", expected 0x0F (the wave data's "
		"0x10 scaled by the full level) in " + Dump(mine));

	for (size_t i = 0; i < mine.size(); i++)
		GBA_CHECK_MSG(mine[i] == theirs[i],
			"byte " + std::to_string(i) + ": HLE " + GbaTest::Hex(mine[i]) +
			" against the BIOS " + GbaTest::Hex(theirs[i]) +
			"\n    HLE  " + Dump(mine) + "\n    BIOS " + Dump(theirs));

	// The left half gets the same samples (the channel is centred).
	std::vector<uint8_t> mineLeft = MixedOutput(hle.Bus(), Work, 0x980, 64);
	GBA_CHECK_MSG(mineLeft[0] == mine[0], "the left half does not match the right one");
}

GBA_TEST(HleBios, TheSoundDriverMatchesTheOfficialBios)
{
	if (FindBiosImage().empty())
	{
		GbaTest::Note("no real BIOS image - the comparison is skipped");
		return;
	}

	// The measurable effects of Init (the identifier) and Mode (the registers) have to be the
	// same in both, because a game was written against the BIOS's behaviour.
	SwiCaller hle(false);
	SwiCaller real(true);

	hle.Call(SwiSoundDriverInit, Work);
	real.Call(SwiSoundDriverInit, Work);

	GBA_CHECK_MSG(hle.R0() == real.R0(),
		"SoundDriverInit returned " + GbaTest::Hex(hle.R0()) + " against the BIOS's " +
		GbaTest::Hex(real.R0()));
	GBA_CHECK_HEX32(hle.Bus().Read32(Work), real.Bus().Read32(Work));

	hle.Call(SwiSoundDriverMode, 0x0094F800);
	real.Call(SwiSoundDriverMode, 0x0094F800);

	GBA_CHECK_HEX16(hle.Bus().Read16(0x04000082), real.Bus().Read16(0x04000082));
	GBA_CHECK_HEX16(hle.Bus().Read16(0x04000084), real.Bus().Read16(0x04000084));
	GBA_CHECK_HEX16(hle.Bus().Read16(0x04000102), real.Bus().Read16(0x04000102));

	// The two DMA channels too: the same source halves, destinations and control words.
	GBA_CHECK_HEX32(hle.Bus().Read32(0x040000BC), real.Bus().Read32(0x040000BC));
	GBA_CHECK_HEX32(hle.Bus().Read32(0x040000C0), real.Bus().Read32(0x040000C0));
	GBA_CHECK_HEX16(hle.Bus().Read16(0x040000C6), real.Bus().Read16(0x040000C6));
	GBA_CHECK_HEX32(hle.Bus().Read32(0x040000C8), real.Bus().Read32(0x040000C8));
	GBA_CHECK_HEX32(hle.Bus().Read32(0x040000CC), real.Bus().Read32(0x040000CC));
	GBA_CHECK_HEX16(hle.Bus().Read16(0x040000D2), real.Bus().Read16(0x040000D2));

	// And the frequency index table for every index GBATEK lists.
	for (uint32_t index = 0; index <= 12; index++)
	{
		uint32_t mode = (8u << 8) | (15u << 12) | (index << 16) | (9u << 20);
		hle.Call(SwiSoundDriverMode, mode);
		real.Call(SwiSoundDriverMode, mode);

		int mine = Timer0Reload(hle.Bus());
		int theirs = Timer0Reload(real.Bus());
		int difference = (mine > theirs) ? (mine - theirs) : (theirs - mine);

		GBA_CHECK_MSG(difference <= 16,
			"frequency index " + std::to_string(index) + ": HLE timer " + GbaTest::Hex(mine) +
			" against the BIOS's " + GbaTest::Hex(theirs));
	}
}

GBA_TEST(HleBios, HuffmanDecodesTheDocumentedStream)
{
	// An 8 bit stream whose root has both children as data: every bit of the bitstream is then one
	// output byte, which is what makes the expected output easy to write down from GBATEK's rules
	// (bit 31 of the 32bit bitstream unit first, 0 = the node0 child, 1 = the node1 child).
	SwiCaller caller(false);
	BuildHuffmanStream(caller.Bus(), Source, 0x60000000, 4, 'A', 'B');

	GBA_CHECK(caller.Call(SwiHuffUnComp, Source, Output));

	// 0110 = A B B A, and the tail of the last 32bit unit is zero.
	const std::vector<uint8_t> expected = { 'A', 'B', 'B', 'A', 0, 0, 0, 0 };
	std::vector<uint8_t> actual = Bytes(caller.Bus(), Output, 8);

	for (size_t i = 0; i < expected.size(); i++)
		GBA_CHECK_MSG(actual[i] == expected[i],
			"byte " + std::to_string(i) + " is " + GbaTest::Hex(actual[i]) +
			", expected " + GbaTest::Hex(expected[i]) + " (" + Dump(actual) + ")");
}

GBA_TEST(HleBios, HuffmanMatchesTheOfficialBios)
{
	if (FindBiosImage().empty())
	{
		GbaTest::Note("no real BIOS image in testing/gba_bench/bios/ - the comparison is skipped "
			"(put your own gba_bios.bin there to run it)");
		return;
	}

	// A 4 bit stream first: its symbols are nibbles packed into bytes, which is what a game's 4bpp
	// graphics use - and the order the two nibbles come out in is exactly the kind of detail the
	// document leaves open (the hardware writes 32bit units).
	{
		SwiCaller hle4(false);
		SwiCaller real4(true);

		BuildHuffmanStream(hle4.Bus(), Source, 0xA5A5A5A5u, 4, 0x0A, 0x0B, 4);
		BuildHuffmanStream(real4.Bus(), Source, 0xA5A5A5A5u, 4, 0x0A, 0x0B, 4);

		for (uint32_t i = 0; i < 16; i++)
		{
			hle4.Bus().Write8(Output + i, 0xEE);
			real4.Bus().Write8(Output + i, 0xEE);
		}

		GBA_CHECK(hle4.Call(SwiHuffUnComp, Source, Output));
		GBA_CHECK(real4.Call(SwiHuffUnComp, Source, Output));

		std::vector<uint8_t> mine4 = Bytes(hle4.Bus(), Output, 16);
		std::vector<uint8_t> theirs4 = Bytes(real4.Bus(), Output, 16);

		for (size_t i = 0; i < mine4.size(); i++)
			GBA_CHECK_MSG(mine4[i] == theirs4[i],
				"4 bit stream, byte " + std::to_string(i) + ": HLE " + GbaTest::Hex(mine4[i]) +
				" against the BIOS " + GbaTest::Hex(theirs4[i]) +
				"\n    HLE  " + Dump(mine4) + "\n    BIOS " + Dump(theirs4));
	}
	// The same streams through both implementations. The tree is two data children under the
	// root, so the compressed bits are exactly the output symbols; the streams differ in the
	// number of bytes, the symbols and the bit patterns.
	struct Case { uint32_t bits; uint32_t outBytes; uint8_t node0; uint8_t node1; };

	const Case cases[] =
	{
		{ 0x00000000u, 4, 'A', 'B' },
		{ 0xFFFFFFFFu, 4, 'A', 'B' },
		{ 0x60000000u, 4, 'A', 'B' },
		{ 0xA5A5A5A5u, 7, 0x11, 0x22 },
		{ 0x12345678u, 12, 0xF0, 0x0F },
		{ 0x00000001u, 1, 0x7F, 0x80 },
		{ 0x5A5A5A5Au, 8, 0x10, 0x0F },		// a longer stream: several 32bit units
	};

	SwiCaller hle(false);
	SwiCaller real(true);

	// A tree with a *node* under the root, and a bitstream at an address whose 32bit unit is not
	// word aligned: both the child addressing and the ARM7TDMI's rotated `ldr` of the bitstream are
	// needed to get this one right. The bits come out as 1001 1001 1000 ... because the unit is
	// loaded from source + 10, so the bytes the rotation puts in front are source + 9 and
	// source + 8 - which are the tree's own two data bytes 99h and 88h.
	{
		SwiCaller treeHle(false);
		SwiCaller treeReal(true);

		for (SwiCaller* caller : { &treeHle, &treeReal })
		{
			GbaBus& bus = caller->Bus();

			for (uint32_t i = 0; i < 64; i++)
				bus.Write8(Source + i, 0);

			bus.Write32(Source + 0, 0x20 | 8 | (4u << 8));	// 8 bit units, 4 bytes out
			bus.Write8(Source + 4, 2);						// (tree table / 2) - 1 -> 6 bytes
			bus.Write8(Source + 5, 0x40);					// root: child0 a node, child1 data
			bus.Write8(Source + 6, 0xC0);					// child0: both children are data
			bus.Write8(Source + 7, 0x77);					// the root's data child (bit 1)
			bus.Write8(Source + 8, 0x88);					// the node's child0
			bus.Write8(Source + 9, 0x99);					// the node's child1
			bus.Write8(Source + 10, 0x00);					// the bitstream starts here
			bus.Write8(Source + 11, 0x00);

			for (uint32_t i = 0; i < 8; i++)
				bus.Write8(Output + i, 0xEE);

			caller->Call(SwiHuffUnComp, Source, Output);
		}

		std::vector<uint8_t> mineTree = Bytes(treeHle.Bus(), Output, 8);
		std::vector<uint8_t> theirsTree = Bytes(treeReal.Bus(), Output, 8);

		// 1 -> the root's data child, 0 0 -> down to the node and its child0, 1 1 -> the root's data
		// child twice.
		const std::vector<uint8_t> expectedTree = { 0x77, 0x88, 0x77, 0x77, 0xEE, 0xEE, 0xEE, 0xEE };

		GBA_CHECK_MSG(mineTree[0] != 0xEE, "the tree stream was not decompressed");

		for (size_t i = 0; i < expectedTree.size(); i++)
			GBA_CHECK_MSG(mineTree[i] == expectedTree[i],
				"deep tree byte " + std::to_string(i) + ": HLE " + GbaTest::Hex(mineTree[i]) +
				", expected " + GbaTest::Hex(expectedTree[i]) + " (" + Dump(mineTree) + ")");

		for (size_t i = 0; i < mineTree.size(); i++)
			GBA_CHECK_MSG(mineTree[i] == theirsTree[i],
				"deep tree byte " + std::to_string(i) + ": HLE " + GbaTest::Hex(mineTree[i]) +
				" against the BIOS " + GbaTest::Hex(theirsTree[i]) +
				"\n    HLE  " + Dump(mineTree) + "\n    BIOS " + Dump(theirsTree));
	}

	for (const Case& c : cases)
	{
		BuildHuffmanStream(hle.Bus(), Source, c.bits, c.outBytes, c.node0, c.node1);
		BuildHuffmanStream(real.Bus(), Source, c.bits, c.outBytes, c.node0, c.node1);

		// Both destinations start from the same pattern, so a byte the decompressor does not
		// write is visible as that pattern in both.
		for (uint32_t i = 0; i < 32; i++)
		{
			hle.Bus().Write8(Output + i, 0xEE);
			real.Bus().Write8(Output + i, 0xEE);
		}

		GBA_CHECK(hle.Call(SwiHuffUnComp, Source, Output));
		GBA_CHECK(real.Call(SwiHuffUnComp, Source, Output));

		std::vector<uint8_t> mine = Bytes(hle.Bus(), Output, 32);
		std::vector<uint8_t> theirs = Bytes(real.Bus(), Output, 32);

		for (size_t i = 0; i < mine.size(); i++)
			GBA_CHECK_MSG(mine[i] == theirs[i],
				"bits " + GbaTest::Hex(c.bits) + " byte " + std::to_string(i) + ": HLE " +
				GbaTest::Hex(mine[i]) + " against the BIOS " + GbaTest::Hex(theirs[i]));
	}
}

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

	/// <summary>
	/// Lay out one node of a random Huffman tree. The byte at `at` becomes a node whose two
	/// children are the next free pair of adjacent bytes; a child is written as data (its byte is a
	/// symbol) or as another node, and the parent's flags say which, exactly as the format has it.
	/// </summary>
	void BuildRandomNode(uint8_t* image, uint32_t base, uint32_t limit, uint32_t& nextFree,
		uint32_t at, int depth, Random& random)
	{
		if (depth <= 0 || nextFree + 2 > limit)
		{
			image[at - base] = (uint8_t)random.Next();
			return;
		}

		uint32_t pair = nextFree;
		nextFree += 2;

		bool leaf0 = depth == 1 || (random.Next() % 3) == 0;
		bool leaf1 = depth == 1 || (random.Next() % 3) == 0;

		uint32_t offset = (pair - (at & ~1u)) / 2 - 1;
		uint8_t flags = (uint8_t)((leaf0 ? 0x80 : 0x00) | (leaf1 ? 0x40 : 0x00));
		image[at - base] = (uint8_t)((offset & 0x3F) | flags);

		BuildRandomNode(image, base, limit, nextFree, pair, leaf0 ? 0 : depth - 1, random);
		BuildRandomNode(image, base, limit, nextFree, pair + 1, leaf1 ? 0 : depth - 1, random);
	}
}

GBA_TEST(HleBios, HuffmanMatchesTheOfficialBiosOnRandomTrees)
{
	// The hand written cases above cover the paths that were read out of the BIOS's own HuffUnComp
	// (the child addressing, the two leaf flags, the rotated bitstream load, the unit that is
	// written whole). This walks random streams - random trees with nodes and data children at
	// random offsets, random bitstreams, both unit sizes - through both implementations, because a
	// decompressor is only as right as the streams it has never seen.
	if (FindBiosImage().empty())
	{
		GbaTest::Note("no real BIOS image in testing/gba_bench/bios/ - the comparison is skipped "
			"(put your own gba_bios.bin there to run it)");
		return;
	}

	SwiCaller hle(false);
	SwiCaller real(true);

	const uint32_t ImageBase = Source;
	const uint32_t ImageSize = 512;			// the tree, the bitstream, and the bytes the walk may read
	const uint32_t TreeBase = Source + 5;
	const uint32_t TreeLimit = Source + 61;	// the tree table stays small enough for 6 bit offsets
	const uint32_t OutputSize = 32;

	const int iterations = 300;
	int failures = 0;

	for (int iteration = 0; iteration < iterations; iteration++)
	{
		Random random(0x9E3779B9u ^ (uint32_t)iteration * 2654435761u);

		uint32_t unitBits = (random.Next() & 1) ? 4 : 8;
		uint32_t outBytes = 1 + (random.Next() % 24);

		std::vector<uint8_t> image(ImageSize);
		for (uint32_t i = 0; i < ImageSize; i++)
			image[i] = (uint8_t)random.Next();

		// The header and the tree, then the bitstream after it - the BIOS's own address arithmetic
		// for the bitstream is source + 4 + (treeSize + 1) * 2, so the tree size byte is whatever
		// makes that land on the first free even byte after the tree.
		uint32_t nextFree = TreeBase + 1;

		if (nextFree & 1)
			nextFree++;

		BuildRandomNode(image.data(), ImageBase, TreeLimit, nextFree, TreeBase,
			1 + (int)(random.Next() % 3), random);

		uint32_t bitstream = (nextFree + 1) & ~1u;
		uint32_t treeSizeByte = (bitstream - (Source + 4)) / 2 - 1;

		uint32_t header = 0x20 | unitBits | (outBytes << 8);
		image[0] = (uint8_t)header;
		image[1] = (uint8_t)(header >> 8);
		image[2] = (uint8_t)(header >> 16);
		image[3] = (uint8_t)(header >> 24);
		image[4] = (uint8_t)treeSizeByte;

		// Every byte the decompressor will read has to be identical in both machines, or a
		// difference in the output could be a difference in the memory rather than in the walk.
		for (uint32_t i = 0; i < ImageSize; i++)
		{
			hle.Bus().Write8(ImageBase + i, image[i]);
			real.Bus().Write8(ImageBase + i, image[i]);
		}

		for (uint32_t i = 0; i < OutputSize; i++)
		{
			hle.Bus().Write8(Output + i, 0xEE);
			real.Bus().Write8(Output + i, 0xEE);
		}

		GBA_CHECK(hle.Call(SwiHuffUnComp, Source, Output));
		GBA_CHECK(real.Call(SwiHuffUnComp, Source, Output));

		std::vector<uint8_t> mine = Bytes(hle.Bus(), Output, OutputSize);
		std::vector<uint8_t> theirs = Bytes(real.Bus(), Output, OutputSize);

		if (mine == theirs)
			continue;

		failures++;

		if (failures <= 3)
		{
			GbaTest::Note("random case " + std::to_string(iteration) + " (" +
				std::to_string(unitBits) + " bit units, " + std::to_string(outBytes) + " bytes out):"
				"\n    HLE  " + Dump(mine) + "\n    BIOS " + Dump(theirs));
		}
	}

	GBA_CHECK_MSG(failures == 0, std::to_string(failures) + " of " + std::to_string(iterations) +
		" random streams decoded differently");
}

