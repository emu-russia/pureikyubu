// XFB output using SDL2. Used in Linux build, but can be used in Windows if you replace ui with uisdl
#include "pch.h"

static SDL_Window* render_target;
static SDL_Surface* surface;
static RGB* video_buffer;
static int xfb_width;
static int xfb_height;

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
}

void VideoOutRefresh()
{
	if (!render_target)
		return;

	Uint32* const pixels = (Uint32*)surface->pixels;

	for (int y = 0; y < xfb_height; y++)
	{
		for (int x = 0; x < xfb_width; x++)
		{
			RGB color = video_buffer[xfb_width * y + x];
			pixels[x + y * surface->w] = color.raw;
		}
	}

	SDL_UpdateWindowSurface(render_target);
}

void VideoOutResize(int width, int height)
{
	// TODO
}
