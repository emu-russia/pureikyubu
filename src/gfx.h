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


// =============================================================================================
// Software (CPU) GFX pipeline - the shared definitions (issue #384)
//
// The software pipeline is a second, completely separate rendering path next to the OpenGL
// shader backend:
//
//     XF                 SU                    RAS                    TX       TEV      PE
//     SoftTransform  ->  primitive assembly -> quad walk + setup  ->  sample -> combine -> EFB
//
// It is selected by the GFX_PIPELINE configuration variable (see config.h) and can be switched
// at run time (GFXCore::SetPipeline). Every block keeps both implementations side by side: the
// GL/GLSL one and the software one - the two paths do not share any rendering state.
//
// The types below are the values that travel between the software blocks. They are written by
// the block that produces them and read by the next one, exactly like the hardware buses:
//
//   * `SoftVertex`   - one vertex of the XF output stream (gfx-xf.md 2.2, gfx-su.md 2.1): the
//                      window-space position and the per-vertex attributes;
//   * `SoftPlane`    - one interpolation plane of the SU datapath (gfx-su.md 2.3, gfx-ras1.md
//                      5.1, gfx-ras2.md 5.1): a value at the raster origin plus the two
//                      screen-space slopes;
//   * `SoftTriangle` - the SU setup record of one triangle that the quad-based RAS walks;
//   * `SoftFragment` - the rasterized sample the software TEV combines.
//
// References: gfx.md (pipeline), gfx-xf.md, gfx-su.md, gfx-ras0.md, gfx-ras1.md, gfx-ras2.md,
// gfx-tc.md (TMEM), gfx-tf.md (filtering), gfx-tev.md, gfx-pe.md.
// =============================================================================================

namespace GFX
{
	// -------------------------------------------------------------------------------------------
	// XF output stream
	// -------------------------------------------------------------------------------------------

	//! One vertex as the software Transform Unit emits it (gfx-xf.md 3.2, the BOP output).
	//!
	//! The XF performs the geometry/texture transforms, the projection combine, the lighting and
	//! the texture coordinate generation in floating point, just like the vertex shader of the
	//! shader pipeline; the bottom of the pipe then divides by the homogeneous component, maps the
	//! result through the viewport scale/offset registers and hands the vertex to the SU.
	struct SoftVertex
	{
		//! Clip-space position (the shader pipeline hands this to GL as gl_Position).
		float clip[4];

		//! Window-space position: X/Y in EFB pixels (the origin is the top left corner of the EFB,
		//! Y grows downward, see XF_VIEWPORT_*) and Z in 24-bit depth units.
		float x = 0.0f, y = 0.0f, z = 0.0f;

		//! 1/w of the vertex. The rasterizers interpolate the attributes divided by w and multiply
		//! the result by this reciprocal, which is the perspective correction of RAS1/RAS2.
		float invW = 1.0f;

		//! Per-channel RGBA colour in 0..1 (channels 0 and 1, see XF_NUMCOLS).
		float color[2][4]{};

		//! Texture coordinates of up to eight coordinate pairs (XF_NUMTEX).
		float tex[8][2]{};
	};

	// -------------------------------------------------------------------------------------------
	// SU setup records
	// -------------------------------------------------------------------------------------------

	//! A linear interpolation plane: the value at the EFB origin (0, 0) plus the change per screen
	//! pixel along X and Y. The planes are what the SU datapath produces for every interpolated
	//! parameter (gfx-su.md 3.4 `su_param`, gfx-ras1.md 5.1, gfx-ras2.md 5.1).
	struct SoftPlane
	{
		float o = 0.0f, dx = 0.0f, dy = 0.0f;

		float Eval(float x, float y) const { return o + dx * x + dy * y; }
	};

	//! The setup of one triangle: the edges the quad walker evaluates, the bounding box and the
	//! interpolation planes of every attribute.
	//!
	//! The perspective-correct attributes (colours and texture coordinates) are carried as planes
	//! of `attribute / w`; the `invW` plane carries `1 / w`. Their ratio is the perspective-correct
	//! value (gfx-ras1.md 3.3: "the divides s/w, t/w and 1/w are carried out at pixel centres").
	//! The depth plane is screen-linear, as the Z plane of RAS0/RAS2 is.
	struct SoftTriangle
	{
		//! Window-space X/Y of the three vertices (EFB pixels).
		float x[3]{}, y[3]{};

		//! Edge functions: `e[i][0] * x + e[i][1] * y + e[i][2]`. They are normalized so that the
		//! interior of the triangle is positive; a sample is covered when all three are >= 0.
		float e[3][3]{};

		//! Twice the signed area of the triangle (positive for the front face unless the winding
		//! is reversed by the transform).
		float area = 0.0f;

		//! The pixel bounding box of the triangle, clamped to the EFB.
		int minx = 0, miny = 0, maxx = -1, maxy = -1;

		SoftPlane z;			//!< Depth (24-bit units) - screen-linear, like the Z plane
		SoftPlane invW;			//!< 1/w (or 1.0 for an orthographic projection)

		SoftPlane color[2][4];	//!< Rasterized colours, divided by w (RAS2)
		SoftPlane tex[8][2];	//!< Texture coordinates, divided by w (RAS1)
	};

	// -------------------------------------------------------------------------------------------
	// The rasterized sample the TEV combines
	// -------------------------------------------------------------------------------------------

	//! The fragment a rasterized sample carries into the TEV: the interpolated rasterized
	//! colours, the texture coordinates, the depth and the screen position (the fog and the
	//! indirect stages need them).
	struct SoftFragment
	{
		float x = 0.0f, y = 0.0f;
		float z = 0.0f;			//!< 24-bit depth of the sample
		float color[2][4]{};	//!< Rasterized (interpolated) colours, in 0..255
		float tex[8][2]{};		//!< Interpolated texture coordinates, in texel units

		//! Screen-space derivatives of the texture coordinates, per coordinate pair:
		//! (ds/dx, dt/dx, ds/dy, dt/dy). The texture unit computes the level of detail from them
		//! (gfx-tc.md 3.3: "the texel/pixel ratio across the quad").
		float dtex[8][4]{};
	};
}

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
	// The rendering pipelines the GFX subsystem can run (config variable GFX_PIPELINE, issue #384):
	// the OpenGL shader backend, or the software (CPU) pipeline of the same hardware blocks.
	#define GFX_PIPELINE_SHADER 0
	#define GFX_PIPELINE_SOFT 1

	// Maximum number of vertices in a single draw command
	#define GFX_MAX_VERTICES 0x10000
	// Maximum number of indices for a single draw command (quads are expanded into triangles)
	#define GFX_MAX_INDICES 0x40000

	// Current emulated GFX frame (for frame dump file names)
	extern int gfx_frame_counter;

	//! Draw the HW profiler overlay (issue #394) over the finished frame. The caller owns the GL
	//! context (it is the same thread that renders the frame); the function is a no-op while the
	//! overlay is off. The picture it draws is built by Debug::HwOsd, which pulls the report
	//! through the debug interface.
	void OsdDraw();

	//! Release the GL objects the overlay owns. Called when the GL context goes away.
	void OsdDispose();

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

		// The frame holds content that has not been handed to the display yet. A display copy only
		// presents such a frame: the copy engine may be asked to write the XFB several times per
		// frame (init sequences, two XFB buffers), and swapping for every copy would show the
		// cleared EFB of the next frame in between (a flicker).
		bool frame_dirty = false;

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

		// Dump the render target to a PNG every N frames, wherever the frame is finished (even
		// when the backend decides not to present it). Configured by GFX_EFB_DUMP.
		bool efb_dump_enabled = false;
		std::string efb_dump_path;
		int efb_dump_every = 1;
		void DumpRenderTarget();

		void DumpFrame();

		//! Restore the OpenGL state that the GFX registers are applied on top of.
		void ApplyDefaultGLState();

	public:

		//! True while the frame loop owns the GL context. Reads of the render target are only valid
		//! on that thread.
		bool HasGLContext() const;
		GFXCore(Flipper::Flipper* flipper, HWConfig* config);
		~GFXCore();

		bool GL_LazyOpenSubsystem();
		bool GL_OpenSubsystem();
		void GL_CloseSubsystem();
		void GL_BeginFrame();
		void GL_EndFrame();
		void GPFrameBegin();
		void GPFrameDone();

		//! A full-frame display copy handed the finished EFB over to the display (PE_COPY_CMD.opcode).
		void GPDisplayCopy();

		//! A primitive was rasterized: the frame now holds content the display has not seen yet.
		void GPFrameDrawn() { frame_dirty = true; }

		void ResizeRenderTarget(size_t width, size_t height);

		//! True when the OpenGL backend has been started (a context is current).
		bool BackendStarted() const { return backend_started; }

		//! The size of the render target (the EFB window the pipeline draws into).
		size_t RenderWidth() const { return scr_w; }
		size_t RenderHeight() const { return scr_h; }

		//! Put every pipeline block back into its reset state (the software equivalent of a GX
		//! reset). The emulator calls it when the console is reset, the debugger with `gxreset`,
		//! and the unit tests to get a known starting point.
		void ResetPipelineState();

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

		//! The rendering pipeline in use (config variable GFX_PIPELINE, see config.h). It can be
		//! switched at run time with SetPipeline(); the software pipeline is a path of its own and
		//! never touches the OpenGL backend.
		int pipeline = GFX_PIPELINE_SHADER;

		//! True when the software (CPU) GFX pipeline is the active one.
		bool SoftPipeline() const { return pipeline == GFX_PIPELINE_SOFT; }

		//! The active pipeline (GFX_PIPELINE_SHADER or GFX_PIPELINE_SOFT).
		int Pipeline() const { return pipeline; }

		//! Switch the pipeline and store the choice in the configuration (the console picks it up
		//! again on the next start). Returns false for an unknown value.
		bool SetPipeline(int value);

		TransformUnit* xf = nullptr;
		SetupUnit* su = nullptr;
		Rasterizer* ras = nullptr;
		BumpMappingUnit* bump = nullptr;
		TextureEngine* tx = nullptr;
		TextureEnvironmentUnit* tev = nullptr;
		PixelEngine* pe = nullptr;
	};
}
