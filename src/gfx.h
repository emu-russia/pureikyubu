// Flipper GFX Engine

/*

Very limited Flipper GFXEngine emulation. Basic OpenGL 1.2 is used as a backend.

This code is out-of-phase because the graphical subsystem has been substantially redesigned. So there is now a mixture of new developments and old code.

What's supported:
- Software implementation of transform unit (XF). Very inaccurate - lighting is partially supported. The transformation of texture coordinates is not completed yet.
- All texture formats are supported, but TLUT versions may be buggy

What's not supported:
- The main drawback is the lack of TEV support. Because of this, complex scenes with effects are drawn with bugs or not drawn at all.
- Emulation of Advanced GX features such as Bump-mapping, indirect texturing, Z-textures not supported
- No texture caching
- There is no emulation of direct access to the EFB

*/

#pragma once

namespace GFX
{
	class GFXCore;
}

#include "pe.h"
#include "xf.h"
#include "su.h"
#include "ras.h"
#include "tev.h"
#include "bump.h"
#include "tx.h"

#define GFX_BLACKJACK_AND_SHADERS 0			// 1: Use modern OpenGL (VBO + Shaders). Under development, do not enable

// 1: Use SDL_Window as a render target; the appropriate SDL API calls are invoked to service it
#ifdef _LINUX
#define GFX_USE_SDL_WINDOW 1
#endif
#if defined(_WINDOWS) && !defined(GFX_USE_SDL_WINDOW)
#define GFX_USE_SDL_WINDOW 0
#endif

namespace GFX
{
	class GFXCore
	{
		friend TransformUnit;
		friend SetupUnit;
		friend Rasterizer;
		friend PixelEngine;
		friend BumpMappingUnit;
		friend TextureEngine;
		friend TextureEnvironmentUnit;

		bool frame_done = true;
		bool disableDraw = false;
		bool frameReady = false;
		bool backend_started = false;

#if GFX_USE_SDL_WINDOW
		SDL_Window* render_window = nullptr;
		SDL_GLContext context{};
#else
		// Windows OpenGL stuff
		HWND hwndMain;
		HGLRC hglrc = 0;
		HDC hdcgl = 0;
		PAINTSTRUCT psFrame{};
#endif

		bool make_shot = false;
		FILE* snap_file = nullptr;
		uint32_t snap_w, snap_h;

		// optionable
		uint32_t scr_w = 640, scr_h = 480;

	public:
		GFXCore(HWConfig* config);
		~GFXCore();

		bool GL_LazyOpenSubsystem();
		bool GL_OpenSubsystem();
		void GL_CloseSubsystem();
		void GL_BeginFrame();
		void GL_EndFrame();
		void GPFrameBegin();
		void GPFrameDone();

		void ResizeRenderTarget(size_t width, size_t height);
		
		GLuint vert_shader;
		GLuint frag_shader;
		GLuint shader_prog;

		GLuint vao;
		GLuint vbo;
		size_t vbo_size = 0x10000;		// Maximum number of vertices that can be used in Draw primitive parameters
		Vertex* vertex_data = nullptr;

		void UploadShaders(const char* vert_source, const char* frag_source);
		void DisposeShaders();
		void InitVBO();
		void DisposeVBO();
		void BindShadersWithVBO();

		// You probably don't need to reset the internal state of GFX because GXInit from Dolphin SDK is working hard on it

		// Gfx Common
		GenMode genmode{};
		GenMsloc msloc[4]{};

		TransformUnit* xf = nullptr;
		SetupUnit* su = nullptr;
		Rasterizer* ras = nullptr;
		BumpMappingUnit* bump = nullptr;
		TextureEngine* tx = nullptr;
		TextureEnvironmentUnit* tev = nullptr;
		PixelEngine* pe = nullptr;
	};
}