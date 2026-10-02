// XFB output using SDL2. This is the only video backend there is (issue #421 removed the Win32 one,
// video.cpp); it presents the VI picture in the SDL window unless the GL backend owns its surface.
#include "pch.h"

static SDL_Window* render_target;
static SDL_Surface* surface;
static RGB* video_buffer;
static int xfb_width;
static int xfb_height;

//! The XFB frames this back end has output since the last call of VideoOutTakeFrames: the frame
//! rate the window title of a software-pipeline run shows (issue #458).
static std::atomic<int64_t> output_frames{ 0 };

using namespace Debug;

bool VideoOutOpen(HWConfig* config, int width, int height, RGB** gfxbuf)
{
	xfb_width = width;
	xfb_height = height;

	render_target = (SDL_Window*)config->renderTarget;
	if (!render_target)
		return true;

	video_buffer = new RGB[width * height];
	memset(video_buffer, 0, sizeof(RGB) * width * height);

	surface = SDL_GetWindowSurface(render_target);

	if (surface == NULL) {
		Report(Channel::VI, "SDL_GetWindowSurface failed: %s\n", SDL_GetError());
		return false;
	}

	SDL_UpdateWindowSurface(render_target);

	*gfxbuf = video_buffer;

	return true;
}

void VideoOutClose()
{
	if (video_buffer != nullptr) {
		delete[] video_buffer;
		video_buffer = nullptr;
	}

	surface = nullptr;
}

/* The rectangle of the window the console picture goes into: the largest one that has the shape the
   picture has (640x480 is 4:3), centered, with a black band around it. Stretching the picture over
   a window of another shape would bend everything in it (a widescreen window would make the cubes
   of a game into bricks), and a window the user maximized for the sake of a bigger picture is
   exactly where that shows.

   The window is a thing the user resizes and maximizes (SDL_WINDOW_RESIZABLE, issue #458), so the
   rectangle is computed from the window every time a frame is presented rather than kept. */
static SDL_Rect presentation_rect(int windowWidth, int windowHeight)
{
	SDL_Rect rect;

	float scaleX = (float)windowWidth / (float)xfb_width;
	float scaleY = (float)windowHeight / (float)xfb_height;
	float scale = (scaleX < scaleY) ? scaleX : scaleY;

	rect.w = (int)((float)xfb_width * scale);
	rect.h = (int)((float)xfb_height * scale);

	// An odd pixel of rounding is not worth a column or a row of the picture.
	if (rect.w > windowWidth) rect.w = windowWidth;
	if (rect.h > windowHeight) rect.h = windowHeight;
	if (rect.w < 1) rect.w = 1;
	if (rect.h < 1) rect.h = 1;

	rect.x = (windowWidth - rect.w) / 2;
	rect.y = (windowHeight - rect.h) / 2;

	return rect;
}

void VideoOutRefresh()
{
	if (!render_target)
		return;

	// The window also carries the OpenGL context the GFX backend draws the EFB into and presents
	// with SDL_GL_SwapWindow. Pushing a software surface into the same window fights that swap -
	// the two presenters alternate, which shows up as flicker between the VI picture and the GL one.
	// While the GL backend owns the window, it is the presenter; the VI still decodes the XFB and
	// counts frames, it just does not write the window.
	if (Flipper::HW != nullptr && Flipper::HW->gfx != nullptr && Flipper::HW->gfx->BackendStarted())
	{
		return;
	}

	// The surface belongs to the window and SDL throws it away when the window is resized: asking
	// for it again is what makes a picture of the new size, and it is a cheap question while the
	// surface is still valid.
	surface = SDL_GetWindowSurface(render_target);

	if (surface == nullptr)
		return;

	const int windowWidth = surface->w;
	const int windowHeight = surface->h;
	const SDL_Rect picture = presentation_rect(windowWidth, windowHeight);

	Uint32* const pixels = (Uint32*)surface->pixels;

	// The pitch of a window surface is the width of a row in bytes and may be padded, so the rows
	// are addressed with it rather than with the width of the window.
	const size_t pitch = (size_t)surface->pitch / sizeof(Uint32);

	// The bands around the picture are black. They are written every frame: the window keeps what
	// the last frame left in it, and the frame before a resize covers less of the window than the
	// one after it.
	for (int y = 0; y < windowHeight; y++)
	{
		SDL_memset(pixels + (size_t)y * pitch, 0, (size_t)windowWidth * sizeof(Uint32));
	}

	// The picture is scaled with the nearest sample of the source: the console picture is already
	// the resolution the console drew, and a filter would only blur the pixels a title drew one by
	// one. The source is stepped in 16.16 fixed point, which keeps the loop free of a division per
	// pixel.
	const uint32_t stepX = (uint32_t)(((uint64_t)xfb_width << 16) / (uint32_t)picture.w);
	const uint32_t stepY = (uint32_t)(((uint64_t)xfb_height << 16) / (uint32_t)picture.h);

	uint32_t sourceY = 0;

	for (int y = 0; y < picture.h; y++)
	{
		const RGB* const source = video_buffer + (size_t)(sourceY >> 16) * xfb_width;
		Uint32* const row = pixels + (size_t)(picture.y + y) * pitch;

		uint32_t sourceX = 0;

		for (int x = 0; x < picture.w; x++)
		{
			const RGB color = source[sourceX >> 16];

			// The window surface can be RGB or BGR (the SDL back ends differ), so the pixel is
			// built with the surface's own format instead of assuming the byte order of the
			// emulator's RGB word.
			row[picture.x + x] = SDL_MapRGB(surface->format, color.Red, color.Green, color.Blue);

			sourceX += stepX;
		}

		sourceY += stepY;
	}

	SDL_UpdateWindowSurface(render_target);

	// One picture is on the screen now: this is the frame rate a front end that presents the
	// picture through this back end reports (issue #458).
	output_frames++;
}

void VideoOutResize(int width, int height)
{
	// The window hands the new size to the next VideoOutRefresh by itself (SDL invalidates the
	// window surface on a resize), so there is nothing to keep here.
}

int64_t VideoOutTakeFrames()
{
	return output_frames.exchange(0);
}
