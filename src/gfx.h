// Flipper GFX Engine

/*

Flipper GFX subsystem emulation on top of a modern OpenGL (3.3 / GLSL 330) shader backend.

The fixed-function pipeline is gone. The two programmable stages of the original hardware are
mapped directly onto the two programmable stages of OpenGL:

- The Transform Unit (XF) is emulated by a vertex shader: geometry and texture matrix multiplies,
  the projection combine, per-vertex lighting (up to 2 channels x 8 lights) and texture coordinate
  generation (regular / colour / dual transform).
- The Texture Environment Unit (TEV) is emulated by a generated fragment shader: up to 16 combine
  stages, fog and the final alpha function. The shader source is generated from the TEV register
  state and cached, because the number of stages and the per-stage selectors are static per material.

What's supported:
- XF: geometry/normal transforms, projection, viewport, texgen (regular + colour), dual transform,
  lighting (material/ambient sources, N.L diffuse, cosine + distance attenuation, spotlight/specular)
- TEV: all 16 stages, all colour/alpha operand selects, K constants (Rev B), fog, alpha test
- All texture formats, up to 8 texture maps bound simultaneously through RAS1_TREF

What's not supported (yet):
- Bump mapping and indirect texturing
- Z-texture environment (TEV_Z_ENV is stored but not applied)
- Direct access to the EFB (Cpu2Efb)

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

// 1: Use SDL_Window as a render target; the appropriate SDL API calls are invoked to service it
#ifdef _LINUX
#define GFX_USE_SDL_WINDOW 1
#endif
#if defined(_WINDOWS) && !defined(GFX_USE_SDL_WINDOW)
#define GFX_USE_SDL_WINDOW 0
#endif

namespace GFX
{
	// Maximum number of vertices in a single draw command
	#define GFX_MAX_VERTICES 0x10000
	// Maximum number of indices for a single draw command (quads are expanded into triangles)
	#define GFX_MAX_INDICES 0x40000

	// Current emulated GFX frame (for frame dump file names)
	extern int gfx_frame_counter;

	GLuint CompileShaderStage(GLenum type, const char* source, const char* label);

	/// <summary>
	/// A linked GL program with a cache of uniform locations.
	/// </summary>
	class GLProgram
	{
		std::unordered_map<std::string, GLint> locations;

	public:
		GLuint prog = 0;

		~GLProgram();

		//! Link a fragment shader (given as source) with an already compiled vertex shader stage.
		bool Link(GLuint vertShader, const char* fragSource, const char* label);
		void Destroy();

		void Use() const { glUseProgram(prog); }
		GLint Uniform(const char* name);
	};

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
		bool frameReady = false;
		bool backend_started = false;

#if GFX_USE_SDL_WINDOW
		SDL_Window* render_window = nullptr;
		SDL_GLContext context{};
#else
		// Windows OpenGL stuff
		HWND hwndMain = nullptr;
		HGLRC hglrc = 0;
		HDC hdcgl = 0;
#endif

		uint32_t scr_w = 640, scr_h = 480;

		// Frame dump (debug). Configured by the GFX_DUMP / GFX_DUMP_EVERY environment variables.
		bool dump_enabled = false;
		std::string dump_path;
		int dump_every = 1;

		void DumpFrame();

	public:
		GFXCore(Flipper::Flipper* flipper, HWConfig* config);
		~GFXCore();

		bool GL_LazyOpenSubsystem();
		bool GL_OpenSubsystem();
		void GL_CloseSubsystem();
		void GL_BeginFrame();
		void GL_EndFrame();
		void GPFrameBegin();
		void GPFrameDone();

		void ResizeRenderTarget(size_t width, size_t height);

		// Geometry buffers
		GLuint vao = 0;
		GLuint vbo = 0;
		GLuint ibo = 0;
		Vertex* vertex_data = nullptr;
		uint32_t* index_data = nullptr;

		void InitGeometryBuffers();
		void DisposeGeometryBuffers();

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
