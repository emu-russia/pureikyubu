// Shared machinery for the GFX (Flipper graphics) unit tests.
//
// The GFX sources are pulled into the test DLL as file links (see pureikyubu_test.vcxproj), so the
// tests drive the very same code the emulator is built from: the pipeline is programmed through the
// same register paths the Command Processor uses, and the geometry is rasterized by the same
// OpenGL programs the emulator renders with.
//
// There are two ways to observe what the pipeline does:
//
//   * the XF vertex shader is run through transform feedback (RunVertexShader), which returns the
//     exact values every varying gets - no rasterization involved;
//   * the whole pipeline is run for real (BeginFrame / DrawQuad / ReadColor), which returns the
//     pixels the TEV/PE produced in the EFB.
//
// Both are compared against the reference models below, which are written from the Flipper
// specifications (C:\Work\gamecube-specs) rather than from the emulator's own code.

#pragma once

#include "CppUnitTest.h"

namespace GfxUnitTest
{
	using namespace Microsoft::VisualStudio::CppUnitTestFramework;

	// -------------------------------------------------------------------------------------------
	// Console main memory image used by the GFX tests
	// -------------------------------------------------------------------------------------------

	/// <summary>Physical pointer into the test console main memory (the MI test double).</summary>
	uint8_t* TestMainMemory(uint32_t physAddr, size_t size);

	/// <summary>Write a block of words into the test main memory.</summary>
	void WriteMainMemory(uint32_t physAddr, const void* data, size_t size);

	/// <summary>Fill the test main memory with a byte pattern.</summary>
	void ClearMainMemory();

	// -------------------------------------------------------------------------------------------
	// The test machine
	// -------------------------------------------------------------------------------------------

	/// <summary>
	/// A GFX pipeline that can be programmed and observed from a unit test.
	///
	/// Start() creates a hidden window, the Flipper device doubles and the real GFXCore on top of
	/// them; the OpenGL context is created by GFXCore itself (the same code path the emulator uses).
	/// </summary>
	class GfxTestMachine
	{
		// Raw storage for the Flipper device doubles. The GFX sources reach the Flipper blocks
		// through Flipper::HW, and the test double only ever needs its pi/mem/vi/cp pointers, so the
		// device objects are never constructed - the link-time stubs (gfx_test_support.cpp) ignore
		// `this` completely.
		uint8_t* flipperStorage = nullptr;
		uint8_t* piStorage = nullptr;
		uint8_t* memStorage = nullptr;
		uint8_t* viStorage = nullptr;

		bool started = false;
		bool glOpen = false;
		std::string lastError;

		// Transform feedback program for the XF vertex shader (see RunVertexShader). The input
		// vertices go through the geometry VAO/VBO the pipeline owns.
		GLuint tfProgram = 0;
		GLuint tfBuf = 0;
		GLuint tfFragShader = 0;

		bool CreateTestWindow(size_t width, size_t height);
		void DestroyTestWindow();
		bool CreateTransformFeedbackProgram();

	public:
		GfxTestMachine();
		~GfxTestMachine();

		HWConfig config{};
		Flipper::Flipper* flipper = nullptr;
		GFX::GFXCore* gfx = nullptr;

		void* window = nullptr;			// HWND on Windows

		// -- lifecycle -------------------------------------------------------------------------

		/// <summary>Create the console image, the Flipper doubles and the GFX pipeline.</summary>
		bool Start(size_t width = 640, size_t height = 480);
		void Stop();

		/// <summary>True when the OpenGL context is up; otherwise GLEnabled() explains why not.</summary>
		bool GLEnabled() const { return glOpen; }

		const std::string& LastError() const { return lastError; }

		/// <summary>Put every pipeline block back into its reset state and restore the default GL state.</summary>
		void Reset();

		// -- the command stream (what the Command Processor does over the CP -> XF interface) ----

		/// <summary>A bypass (BP) register load, index = 0x00..0xFF.</summary>
		void BpLoad(unsigned index, uint32_t value);

		/// <summary>A single-word XF register write (0x0000..0x1057).</summary>
		void XfLoad(unsigned index, uint32_t value);

		/// <summary>A block write of XF registers, as the CP_CMD_LOAD_XFREG command does.</summary>
		void XfLoadBlock(unsigned start, const uint32_t* words, size_t count);

		/// <summary>A block write of single precision floats into the XF register space.</summary>
		void XfLoadFloats(unsigned start, const float* values, size_t count);

		/// <summary>Read one XF register back, through the CP read-back path.</summary>
		uint32_t XfRead(unsigned index);

		// -- frames ----------------------------------------------------------------------------

		/// <summary>Start a frame (clears the EFB with the PE clear registers).</summary>
		void BeginFrame();

		/// <summary>Finish a frame.</summary>
		void EndFrame();

		/// <summary>Draw `vertices` as a quad (4 vertices, fan order) through the whole pipeline.</summary>
		void DrawQuad(const GFX::Vertex* vertices);

		/// <summary>Draw `vertices` with an explicit primitive.</summary>
		void DrawPrimitive(GFX::RAS_Primitive prim, const GFX::Vertex* vertices, size_t count);

		// -- reading the EFB -------------------------------------------------------------------

		/// <summary>Read a rectangle of EFB pixels as RGB triplets, bottom row first (GL order).</summary>
		void ReadColor(int x, int y, int width, int height, std::vector<uint8_t>& rgb);

		/// <summary>Read one EFB pixel.</summary>
		void ReadColorPixel(int x, int y, uint8_t rgb[3]);

		/// <summary>Read the EFB depth of one pixel.</summary>
		float ReadDepthPixel(int x, int y);

		/// <summary>Save a rectangle of the EFB as a PNG file (used by the HTML report).</summary>
		bool SaveScreenshot(const std::string& filename, int x, int y, int width, int height);

		/// <summary>
		/// Save a rectangle of the EFB as a PNG file, box-filtered down by `downsample` (the report
		/// images are half size so that the report stays small).
		/// </summary>
		bool SaveScreenshot(const std::string& filename, int x, int y, int width, int height, int downsample);

		// -- XF vertex shader probing ----------------------------------------------------------

		/// <summary>The varyings of one vertex, as the XF vertex shader produced them.</summary>
		struct XFVertex
		{
			float Position[4];
			float TexCoord[8][2];
			float Color0[4];
			float Color1[4];
		};

		/// <summary>
		/// Run the XF vertex shader over `in` with transform feedback enabled and return what it
		/// wrote. This exercises the real vertex program without involving the rasterizer.
		/// </summary>
		bool RunVertexShader(const std::vector<GFX::Vertex>& in, std::vector<XFVertex>& out);

		/// <summary>Build a Vertex with everything zeroed but the given position.</summary>
		static GFX::Vertex MakeVertex(float x, float y, float z);

		/// <summary>Build a Vertex with a position and a host colour.</summary>
		static GFX::Vertex MakeVertex(float x, float y, float z, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
	};

	/// <summary>The machine shared by all GFX tests in the DLL.</summary>
	GfxTestMachine& Machine();

	/// <summary>Fail the current test when no OpenGL context is available.</summary>
	void RequireGL();

	// -------------------------------------------------------------------------------------------
	// The CPU-visible Flipper register window (implemented in dsp_test_support.cpp, which owns the
	// ProcessorInterface test double).
	// -------------------------------------------------------------------------------------------

	/// <summary>Write a 32-bit word into the emulated PI register space (invokes the block traps).</summary>
	bool PIRegWrite(uint32_t addr, uint32_t value);

	/// <summary>Read a 32-bit word from the emulated PI register space.</summary>
	bool PIRegRead(uint32_t addr, uint32_t* value);

	/// <summary>Forget every trap installed so far (used between tests).</summary>
	void PIClearTraps();

	/// <summary>The interrupts asserted so far through PIAssertInt.</summary>
	uint32_t PIAssertedInterrupts();

	/// <summary>Clear the asserted interrupt accumulator.</summary>
	void PIClearAssertedInterrupts();

	// -------------------------------------------------------------------------------------------
	// Debug output capture (the doubles for Debug::Report / Debug::Halt live in
	// dsp_test_support.cpp)
	// -------------------------------------------------------------------------------------------

	/// <summary>Start collecting Debug::Report messages (they are dropped by default).</summary>
	void EnableTestLog(bool enable);

	/// <summary>Forget everything collected so far, including the halt counter.</summary>
	void ClearTestLog();

	/// <summary>Everything Debug::Report wrote since ClearTestLog().</summary>
	std::string TestLogText();

	/// <summary>The text of the last Debug::Halt() (empty when nothing halted the emulator).</summary>
	std::string TestLastHalt();

	/// <summary>How many times Debug::Halt() was called since ClearTestLog().</summary>
	int TestHaltCount();

	// -------------------------------------------------------------------------------------------
	// Small helpers
	// -------------------------------------------------------------------------------------------

	/// <summary>Throw when the GL error queue is not empty (the message names the operation).</summary>
	void CheckGLError(const char* what);

	/// <summary>Run `fn` and fail the test when it reports a failure, with the message attached.</summary>
	std::wstring Widen(const std::string& text);

	/// <summary>Directory the test report and the screenshots are written to.</summary>
	std::string OutputDir(const std::string& subDir = "");

	/// <summary>
	/// The HTML report of the rendered-image tests. Every RenderedImage* test adds its images here;
	/// the report is flushed to OutputDir()/gfx_report.html when the DLL is unloaded.
	/// </summary>
	class Report
	{
	public:
		static void Section(const std::string& title, const std::string& text);
		static void Image(const std::string& title, const std::string& pngFile,
			const std::string& text = "");
		static void Flush();
	};
}
