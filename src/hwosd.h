/*

# The HW profiler overlay (issue #394)

The profiler (`hwprof.h`) knows how to measure the console's information-exchange channels. This
module is one of the two ways it shows the result: an overlay drawn over the emulated picture, and
the Markdown picture the new debugger (DebugUI2) puts into a panel.

## The text

The overlay does not read the profiler directly. It asks the debug interface for the report
(`hwprofile osd`), exactly like a host front end does, and keeps the text it gets:

    Debug::HwProfile -> hwprofile -> Debug::HwOsd -> the picture -> the frame

That keeps the GFX back end out of the emulated hardware's business: it only knows that there is a
block of text to draw, not where the numbers came from.

The text is rebuilt once a second (`RefreshMs`), from the render path itself - no thread is spent
on it, and the profiler's own `Sample` decides whether a new window has been measured.

## The picture

The text is rasterized into an RGBA8 picture with the same TrueType font the new debugger uses
(`Data/DebugUiMono.ttf`, see `debugui2gl.cpp`), so the overlay follows the console's own font
rather than carrying a bitmap font of its own. The picture is what both presentations draw:

- the GL backend uploads it as a texture and draws it over the finished frame
  (`GFX::OsdDraw`, called from `GFXCore::GL_EndFrame`);
- the software GFX pipeline hands its XFB to the video interface, so `VideoInterface::YUVBlit`
  blits the same picture into the RGB output buffer the video back end presents.

The picture is built once per refresh and shared; `Blit` and the GL path only read it.

*/

#pragma once

namespace Debug
{

namespace HwOsd
{

	//! How often the text is pulled through the debug interface, in milliseconds.
	constexpr uint64_t RefreshMs = 1000;

	//! Switch the overlay on or off and remember the choice in the settings (`HW_OSD`).
	void SetEnabled(bool enable);
	bool Enabled();

	//! Register the `hwprofile` and `hwsod` commands (called from `Debug::Reflector`).
	void Reflector();

	//! Pull the report and rebuild the picture when the refresh interval has passed. Called from
	//! the render paths once per frame; a no-op while the overlay is off.
	void Update();

	//! Copy the current picture out (RGBA8, `width` x `height` rows, 4 bytes per pixel). Returns
	//! false while the overlay is off or the font could not be loaded.
	bool CopyImage(std::vector<uint8_t>& rgba, int* width, int* height);

	//! Bumped every time the picture is rebuilt, so a renderer can cache what it made of it.
	uint64_t Version();

	//! Draw the picture into the top left corner of a 32-bit destination buffer, whose pixels are
	//! (blue, green, red, unused) - the format the VI output buffers use (see `vi.h`).
	void Blit(uint8_t* destination, int width, int height);

	//! Render the current report into a PNG file. `hwprofile image` uses it for the picture that
	//! goes next to the debugger session.
	bool SavePicture(const std::string& path);

}

}
