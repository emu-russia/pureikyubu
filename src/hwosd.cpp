/*

The HW profiler overlay: the text is pulled through the debug interface and rasterized into a
picture the GFX back ends draw. See hwosd.h for the whole picture.

*/

#include "pch.h"

// The debugger's own GL front end (debugui2gl.cpp) already compiles the TrueType rasterizer into
// the emulator, and this module is linked into the same binary. A static copy of the library keeps
// the two implementations from colliding at link time.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <chrono>

namespace Debug
{

namespace HwOsd
{

	// ------------------------------------------------------------------------------------
	// The state
	//
	// The picture is read by the render paths (the emulation thread) and written by the render
	// path and by the debug interface (the debugger thread), so everything below the public
	// interface is guarded by `osdLock`. The helpers whose name ends in `Locked` assume that the
	// caller already holds it and never take it themselves.
	// ------------------------------------------------------------------------------------

	static SpinLock osdLock;

	static std::atomic<bool> enabled{ false };
	static std::atomic<bool> enabledKnown{ false };
	static uint64_t version = 0;

	static std::vector<std::string> lines;
	static std::vector<uint8_t> picture;
	static int picWidth = 0, picHeight = 0;

	static std::atomic<uint64_t> lastRefresh{ 0 };

	static uint64_t NowMs()
	{
		static const auto origin = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();
		return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - origin).count();
	}

	// ------------------------------------------------------------------------------------
	// The font
	// ------------------------------------------------------------------------------------

	namespace
	{

		//! One rasterized code point. The font is monospaced, so the advance is not kept here.
		struct Glyph
		{
			int width = 0, height = 0;
			int xoff = 0, yoff = 0;
			std::vector<uint8_t> bitmap;
		};

		//! The pixel height the overlay text is rasterized at. The report is 50-odd characters
		//! wide, and this is the size at which the whole table still fits over a 640x480 picture.
		constexpr float FontPixelHeight = 13.0f;

		std::vector<uint8_t> fontData;
		stbtt_fontinfo font = {};
		bool fontReady = false;
		float fontScale = 0.0f;
		float fontAscent = 0.0f;
		float charAdvance = 0.0f;
		float lineAdvance = 0.0f;
		std::map<int, Glyph> glyphs;

		bool BuildFont()
		{
			if (fontReady)
				return true;

			fontData = Util::FileLoad("Data/DebugUiMono.ttf");

			if (fontData.empty())
			{
				Report(Channel::Error, "hwsod: Data/DebugUiMono.ttf is missing, the profiler overlay will not be drawn\n");
				fontData.clear();
				return false;
			}

			if (!stbtt_InitFont(&font, fontData.data(), stbtt_GetFontOffsetForIndex(fontData.data(), 0)))
			{
				Report(Channel::Error, "hwsod: Data/DebugUiMono.ttf is not a TrueType font\n");
				fontData.clear();
				return false;
			}

			int ascentRaw = 0, descentRaw = 0, lineGapRaw = 0;
			stbtt_GetFontVMetrics(&font, &ascentRaw, &descentRaw, &lineGapRaw);

			fontScale = stbtt_ScaleForPixelHeight(&font, FontPixelHeight);
			fontAscent = ascentRaw * fontScale;

			// The advance of one character. The font is monospaced, so the picture can be sized
			// from the longest line without laying the text out first.
			int advanceRaw = 0, lsb = 0;
			stbtt_GetGlyphHMetrics(&font, stbtt_FindGlyphIndex(&font, 'M'), &advanceRaw, &lsb);
			charAdvance = ceilf(advanceRaw * fontScale);
			lineAdvance = ceilf((ascentRaw - descentRaw + lineGapRaw) * fontScale) + 1.0f;

			fontReady = true;
			return true;
		}

		const Glyph* GetGlyph(int codepoint)
		{
			auto it = glyphs.find(codepoint);
			if (it != glyphs.end())
				return &it->second;

			Glyph glyph;
			unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, 0, fontScale, codepoint,
				&glyph.width, &glyph.height, &glyph.xoff, &glyph.yoff);

			if (bitmap != nullptr && glyph.width > 0 && glyph.height > 0)
			{
				glyph.bitmap.assign(bitmap, bitmap + (size_t)glyph.width * glyph.height);
			}
			else
			{
				glyph.width = glyph.height = 0;
			}

			if (bitmap != nullptr)
				stbtt_FreeBitmap(bitmap, nullptr);

			auto inserted = glyphs.insert(std::make_pair(codepoint, std::move(glyph)));
			return &inserted.first->second;
		}

		//! Blend one glyph into the RGBA picture. `x`/`y` are the top left corner of the character
		//! cell; the glyph's own offset places the ink inside it.
		void DrawGlyph(uint8_t* rgba, int width, int height, int x, int y, int codepoint,
			uint8_t red, uint8_t green, uint8_t blue)
		{
			if (codepoint < 32 || codepoint >= 127)
				return;

			const Glyph* glyph = GetGlyph(codepoint);
			if (glyph == nullptr || glyph->width == 0 || glyph->height == 0)
				return;

			int baseX = x + glyph->xoff;
			int baseY = y + (int)fontAscent + glyph->yoff;

			for (int gy = 0; gy < glyph->height; gy++)
			{
				int py = baseY + gy;
				if (py < 0 || py >= height)
					continue;

				for (int gx = 0; gx < glyph->width; gx++)
				{
					int px = baseX + gx;
					if (px < 0 || px >= width)
						continue;

					uint8_t coverage = glyph->bitmap[(size_t)gy * glyph->width + gx];
					if (coverage == 0)
						continue;

					uint8_t* pixel = &rgba[((size_t)py * width + px) * 4];
					uint32_t alpha = coverage;

					// The picture itself is composited over whatever the caller draws it on, so
					// the ink is blended into the destination rather than replacing it.
					pixel[0] = (uint8_t)((red * alpha + pixel[0] * (255 - alpha)) / 255);
					pixel[1] = (uint8_t)((green * alpha + pixel[1] * (255 - alpha)) / 255);
					pixel[2] = (uint8_t)((blue * alpha + pixel[2] * (255 - alpha)) / 255);
					pixel[3] = (uint8_t)(pixel[3] + (255 - pixel[3]) * alpha / 255);
				}
			}
		}

		void DrawText(uint8_t* rgba, int width, int height, int x, int y, const std::string& text,
			uint8_t red, uint8_t green, uint8_t blue)
		{
			float pen = (float)x;

			for (size_t i = 0; i < text.size(); i++)
			{
				DrawGlyph(rgba, width, height, (int)pen, y, (uint8_t)text[i], red, green, blue);
				pen += charAdvance;
			}
		}

	}

	// ------------------------------------------------------------------------------------
	// The picture
	// ------------------------------------------------------------------------------------

	//! Rasterize the current `lines` into the picture. The caller holds `osdLock`.
	static void BuildPictureLocked()
	{
		picture.clear();
		picWidth = picHeight = 0;
		version++;

		if (!BuildFont() || lines.empty())
		{
			return;
		}

		size_t longest = 0;
		for (size_t i = 0; i < lines.size(); i++)
		{
			if (lines[i].size() > longest)
				longest = lines[i].size();
		}

		const int padding = 4;
		const uint8_t background = 0xC0;		// the emulated picture shows through a little

		picWidth = (int)(longest * charAdvance) + padding * 2;
		picHeight = (int)(lines.size() * lineAdvance) + padding * 2;

		picture.assign((size_t)picWidth * picHeight * 4, 0);

		for (size_t i = 0; i < (size_t)picWidth * picHeight; i++)
		{
			picture[i * 4 + 3] = background;
		}

		for (size_t i = 0; i < lines.size(); i++)
		{
			// The first line is the header (the window and the real-time factor), the rest is the
			// table. The header is tinted so that the eye finds the table under it.
			bool header = (i == 0);

			DrawText(picture.data(), picWidth, picHeight, padding,
				padding + (int)(i * lineAdvance), lines[i],
				header ? 0x70 : 0xFF, header ? 0xD0 : 0xFF, header ? 0xFF : 0xFF);
		}
	}

	//! Render the current picture into a PNG file. The caller holds `osdLock`.
	static bool SavePictureLocked(const std::string& path)
	{
		if (picture.empty() || picWidth <= 0 || picHeight <= 0)
			return false;

		std::vector<uint8_t> rgb((size_t)picWidth * picHeight * 3);
		for (size_t i = 0; i < (size_t)picWidth * picHeight; i++)
		{
			rgb[i * 3 + 0] = picture[i * 4 + 0];
			rgb[i * 3 + 1] = picture[i * 4 + 1];
			rgb[i * 3 + 2] = picture[i * 4 + 2];
		}

		return Util::SavePng(path.c_str(), rgb.data(), picWidth, picHeight);
	}

	// ------------------------------------------------------------------------------------
	// The report, through the debug interface
	// ------------------------------------------------------------------------------------

	//! The session folder of the new debugger, through the debug interface (it is a JDI entity:
	//! `SessionPath`). Empty when the debugger is not running.
	static std::string SessionPath()
	{
		std::string path;

		Json::Value* value = JDI::Hub.ExecuteFast("SessionPath");
		if (value != nullptr)
		{
			if (value->type == Json::ValueType::Array && !value->children.empty())
			{
				Json::Value* first = value->children.front();
				if (first->type == Json::ValueType::String)
					path = Util::WstringToString(first->value.AsString);
			}

			delete value;
		}

		return path;
	}

	//! Ask the profiler for the fixed-width report and rasterize it. The overlay deliberately goes
	//! through the debug interface rather than calling into the emulated hardware, so that the GFX
	//! back end knows nothing about where the numbers come from. The caller holds `osdLock`.
	static void RefreshTextLocked()
	{
		std::vector<std::string> request;
		request.push_back("hwprofile");
		request.push_back("osd");

		Json::Value* value = JDI::Hub.Execute(request);

		lines.clear();

		if (value != nullptr)
		{
			if (value->type == Json::ValueType::Array)
			{
				for (auto it = value->children.begin(); it != value->children.end(); ++it)
				{
					if ((*it)->type == Json::ValueType::String)
						lines.push_back(Util::WstringToString((*it)->value.AsString));
				}
			}

			delete value;
		}

		BuildPictureLocked();
	}

	static void RefreshIfDue()
	{
		uint64_t now = NowMs();
		uint64_t previous = lastRefresh.load();

		if (previous != 0 && now - previous < RefreshMs)
			return;

		lastRefresh.store(now);

		osdLock.Lock();
		RefreshTextLocked();
		osdLock.Unlock();
	}

	// ------------------------------------------------------------------------------------
	// The public interface
	// ------------------------------------------------------------------------------------

	void SetEnabled(bool enable)
	{
		enabled.store(enable);
		enabledKnown.store(true);

		osdLock.Lock();
		lines.clear();
		picture.clear();
		picWidth = picHeight = 0;
		version++;
		osdLock.Unlock();

		lastRefresh.store(0);

		SetConfigBool(USER_HW_OSD, enable, USER_UI);
	}

	bool Enabled()
	{
		if (!enabledKnown.load())
		{
			enabled.store(GetConfigBool(USER_HW_OSD, USER_UI));
			enabledKnown.store(true);
		}

		return enabled.load();
	}

	void Update()
	{
		if (!Enabled())
			return;

		RefreshIfDue();
	}

	bool CopyImage(std::vector<uint8_t>& rgba, int* width, int* height)
	{
		osdLock.Lock();

		bool ok = !picture.empty();

		if (ok)
		{
			rgba = picture;
			if (width != nullptr) *width = picWidth;
			if (height != nullptr) *height = picHeight;
		}

		osdLock.Unlock();

		return ok;
	}

	uint64_t Version()
	{
		osdLock.Lock();
		uint64_t value = version;
		osdLock.Unlock();
		return value;
	}

	void Blit(uint8_t* destination, int width, int height)
	{
		if (destination == nullptr)
			return;

		osdLock.Lock();

		if (!picture.empty())
		{
			int rows = (picHeight < height) ? picHeight : height;
			int columns = (picWidth < width) ? picWidth : width;

			for (int y = 0; y < rows; y++)
			{
				const uint8_t* src = &picture[(size_t)y * picWidth * 4];
				uint8_t* dst = destination + (size_t)y * width * 4;

				for (int x = 0; x < columns; x++)
				{
					uint32_t alpha = src[x * 4 + 3];

					// The VI output buffer holds (blue, green, red, unused) - see the `RGB` union.
					dst[x * 4 + 0] = (uint8_t)((src[x * 4 + 2] * alpha + dst[x * 4 + 0] * (255 - alpha)) / 255);
					dst[x * 4 + 1] = (uint8_t)((src[x * 4 + 1] * alpha + dst[x * 4 + 1] * (255 - alpha)) / 255);
					dst[x * 4 + 2] = (uint8_t)((src[x * 4 + 0] * alpha + dst[x * 4 + 2] * (255 - alpha)) / 255);
				}
			}
		}

		osdLock.Unlock();
	}

	bool SavePicture(const std::string& path)
	{
		osdLock.Lock();
		bool ok = SavePictureLocked(path);
		osdLock.Unlock();
		return ok;
	}

	// ------------------------------------------------------------------------------------
	// The debug interface
	// ------------------------------------------------------------------------------------

	//! Take a reading of the emulated machine and fold it into the profiler. The two cores own
	//! their instruction counters (the profiler is a pure function of the machine's state), so
	//! this is the place that reads them.
	static void SampleMachine()
	{
		if (Core == nullptr)
			return;

		uint64_t gekkoInstructions = (uint64_t)Core->GetInstructionCounter();

		uint64_t dspInstructions =
			(Flipper::DSP != nullptr && Flipper::DSP->core != nullptr) ?
				(uint64_t)Flipper::DSP->core->GetInstructionCounter() : 0;

		HwProfile::Sample(gekkoInstructions, dspInstructions,
			(uint64_t)Core->GetTicks(), (uint64_t)Core->OneSecond());
	}

	// hwprofile [text|image|osd|reset] - the report of the HW interface profiler.
	static Json::Value* CmdHwProfile(std::vector<std::string>& args)
	{
		std::string mode = (args.size() > 1) ? args[1] : "text";

		if (mode == "reset")
		{
			HwProfile::Reset();
			return nullptr;
		}

		// Take the measurement if the window is due, so that the answer describes the last second
		// of emulation rather than the one before the last report.
		SampleMachine();

		if (mode == "osd")
		{
			std::vector<std::string> report;
			HwProfile::ReportToLines(report);

			Json::Value* output = new Json::Value();
			output->type = Json::ValueType::Array;

			for (size_t i = 0; i < report.size(); i++)
			{
				output->AddUtf8String(nullptr, report[i].c_str());
			}

			return output;
		}

		std::string markdown;
		HwProfile::ReportToMarkdown(markdown);

		if (mode == "image")
		{
			// The picture belongs next to the session (the debugger resolves image references
			// against its session folder), so it is the session that gives us the path.
			std::string session = SessionPath();

			if (session.empty())
			{
				Report(Channel::Error, "hwprofile: the new debugger is not running, there is nowhere to put the picture\n");
			}
			else
			{
				const char* file = "hwprofile.png";

				std::vector<std::string> report;
				HwProfile::ReportToLines(report);

				osdLock.Lock();

				// The picture is built from this very report, then the overlay's own picture is
				// put back: the two share the rasterizer, not the content.
				std::vector<std::string> savedLines = lines;
				std::vector<uint8_t> savedPicture = picture;
				int savedWidth = picWidth, savedHeight = picHeight;

				lines = report;
				BuildPictureLocked();
				bool stored = SavePictureLocked(session + "/" + file);

				lines = savedLines;
				picture = savedPicture;
				picWidth = savedWidth;
				picHeight = savedHeight;
				version++;

				osdLock.Unlock();

				if (stored)
				{
					markdown += "\n![HW interface profile](" + std::string(file) + ")\n";
				}
				else
				{
					Report(Channel::Error, "hwprofile: cannot store the picture in %s\n", session.c_str());
				}
			}
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Object;
		output->AddUtf8String("markdown", markdown.c_str());
		return output;
	}

	// hwsod [0|1] - the profiler overlay in the emulated picture.
	static Json::Value* CmdHwSod(std::vector<std::string>& args)
	{
		if (args.size() > 1)
		{
			SetEnabled(atoi(args[1].c_str()) != 0);
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Bool;
		output->value.AsBool = Enabled();
		return output;
	}

	void Reflector()
	{
		JDI::Hub.AddCmd("hwprofile", CmdHwProfile);
		JDI::Hub.AddCmd("hwsod", CmdHwSod);
	}

}

}
