// Support layer for the GFX (Flipper graphics) unit tests.
//
// The GFX sources (gfx.cpp, cp.cpp, xf.cpp, su.cpp, ras.cpp, tx.cpp, tev.cpp, pe.cpp, bump.cpp) are
// pulled into this project as file links (see pureikyubu_test.vcxproj), exactly like the DSP ones.
// They reference a handful of entities that live outside the graphics subsystem, and this file
// supplies them:
//
//   * the console main memory behind MemoryInterface::MIGetMemoryPointerForTX / ...ForCP, so that
//     texture decoding and the CP FIFO work on a real buffer;
//   * the VideoInterface / ProcessorInterface hooks the Pixel Engine and the CP report through
//     (the PI doubles themselves live in dsp_test_support.cpp, together with the register window
//     traps the CP tests use);
//   * the Gekko tick source (only the CP thread reads it, and no emulator thread runs in tests);
//   * a hidden window + the real GFXCore on top of it, so the tests exercise the actual OpenGL
//     pipeline instead of a model of it.
//
// Nothing here implements graphics behaviour: it only replaces the surrounding hardware.

#include "pch.h"
#include "gfx_test_common.h"

static Flipper::SerialInterface* serialInterface = nullptr;


namespace GfxUnitTest
{
	// -------------------------------------------------------------------------------------------
	// Console main memory
	//
	// The GFX tests use their own image: the DSP tests do not run at the same time, and a shared
	// buffer would only make the two suites depend on each other's leftovers.
	// -------------------------------------------------------------------------------------------

	namespace
	{
		const size_t TestMainMemorySize = 32 * 1024 * 1024;
		uint8_t* testMainMemory = nullptr;

		uint8_t* MainMemory()
		{
			if (testMainMemory == nullptr)
			{
				testMainMemory = new uint8_t[TestMainMemorySize];
				memset(testMainMemory, 0, TestMainMemorySize);
			}
			return testMainMemory;
		}
	}

	uint8_t* TestMainMemory(uint32_t physAddr, size_t size)
	{
		uint8_t* base = MainMemory();

		if ((size_t)physAddr + size > TestMainMemorySize)
		{
			return nullptr;
		}

		return base + physAddr;
	}

	void WriteMainMemory(uint32_t physAddr, const void* data, size_t size)
	{
		uint8_t* dst = TestMainMemory(physAddr, size);
		if (dst == nullptr)
		{
			return;
		}

		memcpy(dst, data, size);
	}

	void ClearMainMemory()
	{
		memset(MainMemory(), 0, TestMainMemorySize);
	}

	// -------------------------------------------------------------------------------------------
	// The Flipper device doubles
	//
	// The definitions below belong to the emulator's Flipper namespace, not to GfxUnitTest, so they
	// are written at the global scope further down this file.
	// -------------------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------------------
	// Gekko ticks
	// -------------------------------------------------------------------------------------------

	namespace
	{
		// Storage for the Core pointer: the GFX sources (the CP thread) call Core->GetTicks(), and
		// the test double of GekkoCore::GetTicks ignores `this`.
		uint8_t coreStorage[64];
	}

	// -------------------------------------------------------------------------------------------
	// Small helpers
	// -------------------------------------------------------------------------------------------

	std::wstring Widen(const std::string& text)
	{
		return Util::StringToWstring(text);
	}

	std::string OutputDir(const std::string& subDir)
	{
		std::string dir = "gfx_test_out";

		if (!subDir.empty())
		{
			dir += "/";
			dir += subDir;
		}

		// The VS test host runs the DLL from the solution output directory; keep the artifacts next
		// to it so that they are easy to find (scripts/VS2026/x64/<Config>/gfx_test_out).
		CreateDirectoryA(dir.c_str(), nullptr);

		return dir;
	}

	void CheckGLError(const char* what)
	{
		GLenum err = glGetError();
		if (err != GL_NO_ERROR)
		{
			char text[256];
			sprintf_s(text, "OpenGL error 0x%04X after %s", (unsigned)err, what);
			Assert::Fail(Widen(text).c_str());
		}
	}

	// -------------------------------------------------------------------------------------------
	// The test machine
	// -------------------------------------------------------------------------------------------

	GfxTestMachine::GfxTestMachine()
	{
	}

	GfxTestMachine::~GfxTestMachine()
	{
		Stop();
	}

	bool GfxTestMachine::CreateTestWindow(size_t width, size_t height)
	{
#ifdef _WINDOWS
		static const wchar_t* className = L"pureikyubu_gfx_unit_test";

		WNDCLASSEXW wc = {};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = DefWindowProcW;
		wc.hInstance = GetModuleHandleW(nullptr);
		wc.lpszClassName = className;

		if (RegisterClassExW(&wc) == 0)
		{
			DWORD err = GetLastError();
			if (err != ERROR_CLASS_ALREADY_EXISTS)
			{
				char text[128];
				sprintf_s(text, "RegisterClassExW failed: %u", (unsigned)err);
				lastError = text;
				return false;
			}
		}

		// The window is created so that its *client* area is exactly the render target. The pipeline
		// draws a width x height viewport, and a smaller client area would leave the top and the right
		// of every picture the tests publish unrendered (the window rect includes the frame and the
		// title bar, so passing the size straight to CreateWindowExW is not enough).
		RECT rect = { 0, 0, (LONG)width, (LONG)height };
		AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

		HWND hwnd = CreateWindowExW(0, className, L"pureikyubu gfx unit test", WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
			nullptr, nullptr, wc.hInstance, nullptr);

		if (hwnd == nullptr)
		{
			char text[128];
			sprintf_s(text, "CreateWindowExW failed: %u", (unsigned)GetLastError());
			lastError = text;
			return false;
		}

		window = hwnd;
		return true;
#else
		lastError = "the GFX unit tests need the Windows OpenGL backend";
		return false;
#endif
	}

	void GfxTestMachine::DestroyTestWindow()
	{
#ifdef _WINDOWS
		if (window != nullptr)
		{
			::DestroyWindow((HWND)window);
			window = nullptr;
		}
#endif
	}

	bool GfxTestMachine::Start(size_t width, size_t height)
	{
		if (started)
		{
			return true;
		}

		ClearMainMemory();
		PIClearTraps();
		PIClearAssertedInterrupts();

		if (!CreateTestWindow(width, height))
		{
			return false;
		}

		memset(&config, 0, sizeof(config));
		config.ramsize = 24 * 1024 * 1024;
		config.renderTarget = window;
		config.consoleVer = 1;

		// The Flipper device doubles: only the pointers are ever dereferenced, and the stubs above
		// ignore `this`, so plain storage is enough.
		flipperStorage = new uint8_t[sizeof(Flipper::Flipper)]();
		piStorage = new uint8_t[sizeof(Flipper::ProcessorInterface)]();
		memStorage = new uint8_t[sizeof(Flipper::MemoryInterface)]();
		viStorage = new uint8_t[sizeof(Flipper::VideoInterface)]();

		flipper = (Flipper::Flipper*)flipperStorage;
		flipper->pi = (Flipper::ProcessorInterface*)piStorage;
		flipper->mem = (Flipper::MemoryInterface*)memStorage;
		flipper->vi = (Flipper::VideoInterface*)viStorage;

		Flipper::HW = flipper;
		Core = (Gekko::GekkoCore*)coreStorage;

		gfx = new GFX::GFXCore(flipper, &config);
		flipper->gfx = gfx;

		// The CP owns the CPU-visible graphics registers and the display-list FIFO; the tests drive
		// it through the PI register window (PIRegWrite), exactly like the CPU does.
		flipper->cp = new Flipper::CommandProcessor(flipper, &config);

		// The peripherals of the machine. In the emulator the pool is opened once, with the emulator
		// itself (see EMUCtor); a test machine has no emulator around it, so it opens the pool here.
		// The four controller sockets then get a pad each, which is what the PAD plug-in double used
		// to answer with (see the peripheral subsystem doubles below).
		Peripherals::Instance().Open();

		for (int chan = 0; chan < 4; chan++)
		{
			Peripherals::Instance().Attach(chan, PERIPH_PORT_SI(chan));
		}

		Peripherals::Instance().MachineOpened();

		serialInterface = new Flipper::SerialInterface(flipper, &config);
		flipper->si = serialInterface;

		started = true;
		return true;
	}

	void GfxTestMachine::Stop()
	{
		if (!started)
		{
			return;
		}

		if (glOpen)
		{
			// Release the transform feedback objects while the context is still current
			if (tfProgram != 0) glDeleteProgram(tfProgram);
			if (tfFragShader != 0) glDeleteShader(tfFragShader);
			if (tfBuf != 0) glDeleteBuffers(1, &tfBuf);
			tfProgram = tfFragShader = tfBuf = 0;

			glOpen = false;
		}

		delete gfx;
		gfx = nullptr;

		delete flipper->cp;
		flipper->cp = nullptr;

		// The devices of the pool are unplugged while the bus they are plugged into still exists: a
		// memory card detaches from its EXI channel here. The pool itself is the emulator's and is
		// left open for the next machine.
		Peripherals::Instance().MachineClosed();

		delete serialInterface;
		serialInterface = nullptr;
		if (flipper != nullptr) flipper->si = nullptr;

		Flipper::HW = nullptr;
		Core = nullptr;

		delete[] flipperStorage; flipperStorage = nullptr;
		delete[] piStorage; piStorage = nullptr;
		delete[] memStorage; memStorage = nullptr;
		delete[] viStorage; viStorage = nullptr;

		flipper = nullptr;

		DestroyTestWindow();

		started = false;
	}

	void GfxTestMachine::Reset()
	{
		Assert::IsTrue(Start(), L"the GFX test machine could not be started");

		// The software pipeline owns no GL objects at all, so it is reset without a context.
		if (!SoftPipeline())
		{
			if (!glOpen)
			{
				glOpen = gfx->GL_OpenSubsystem();

				// A fresh context has fresh object names: the transform feedback program and buffer
				// of the previous one do not exist in it any more.
				tfProgram = tfFragShader = tfBuf = 0;
			}

			Assert::IsTrue(glOpen, Widen("the OpenGL backend could not be started: " + lastError).c_str());
		}

		gfx->ResetPipelineState();

		// The serial interface is stateful (the poll schedule and the interrupt flags) and is not
		// part of the pipeline state, so it is rebuilt. Its constructor also reinstalls the SI
		// register traps, which then point at the new instance.
		delete serialInterface;
		serialInterface = new Flipper::SerialInterface(flipper, &config);
		flipper->si = serialInterface;
	}

	bool GfxTestMachine::SoftPipeline() const
	{
		return gfx != nullptr && gfx->SoftPipeline();
	}

	void GfxTestMachine::SetPipeline(int pipeline)
	{
		Assert::IsTrue(Start(), L"the GFX test machine could not be started");

		gfx->SetPipeline(pipeline);

		// Switching the pipeline drops the OpenGL context (the software pipeline renders without
		// one), so the machine has to open a fresh one when it comes back to the shader pipeline.
		glOpen = false;

		Reset();
	}

	void GfxTestMachine::BpLoad(unsigned index, uint32_t value)
	{
		Assert::IsTrue(started, L"the GFX test machine is not running");
		gfx->xf->CPSuCommand(index, value);
	}

	void GfxTestMachine::XfLoad(unsigned index, uint32_t value)
	{
		Assert::IsTrue(started, L"the GFX test machine is not running");
		gfx->xf->CPRegLoadBegin(index, 1);
		gfx->xf->CPRegLoadData(value);
	}

	void GfxTestMachine::XfLoadBlock(unsigned start, const uint32_t* words, size_t count)
	{
		Assert::IsTrue(started, L"the GFX test machine is not running");
		gfx->xf->CPRegLoadBegin(start, count);
		for (size_t i = 0; i < count; i++)
		{
			gfx->xf->CPRegLoadData(words[i]);
		}
	}

	void GfxTestMachine::XfLoadFloats(unsigned start, const float* values, size_t count)
	{
		std::vector<uint32_t> words(count);
		for (size_t i = 0; i < count; i++)
		{
			memcpy(&words[i], &values[i], 4);
		}
		XfLoadBlock(start, words.data(), count);
	}

	uint32_t GfxTestMachine::XfRead(unsigned index)
	{
		Assert::IsTrue(started, L"the GFX test machine is not running");

		gfx->xf->CPRegRead(index);

		uint32_t value = 0;
		Assert::IsTrue(gfx->xf->CPTakeReadData(&value), L"the XF did not answer the register read");
		return value;
	}

	void GfxTestMachine::BeginFrame()
	{
		if (SoftPipeline())
		{
			// The software pipeline has no GL frame: the copy engine's clear is what starts a frame
			// (GFXCore::GPFrameBegin), and the picture leaves through the XFB. A frame has to be
			// finished before the next one starts - the shader path above does the same with
			// GL_EndFrame - otherwise the frame begin does not clear the EFB and the tests would see
			// the pixels of the previous frame.
			gfx->GPFrameDone();
			gfx->GPFrameBegin();
			return;
		}

		Assert::IsTrue(glOpen, L"no OpenGL context");

		// A frame must be finished before the next one starts, otherwise GL_BeginFrame() skips the
		// clear and the tests would see the pixels of the previous frame. The swap inside GL_EndFrame
		// targets a hidden window, and the back buffer contents are undefined until it completes, so
		// the swap is synchronised here as well.
		gfx->GL_EndFrame();
		glFinish();

		gfx->GL_BeginFrame();
		glFinish();
	}

	void GfxTestMachine::EndFrame()
	{
		if (SoftPipeline())
		{
			gfx->GPFrameDone();
			return;
		}

		Assert::IsTrue(glOpen, L"no OpenGL context");
		gfx->GL_EndFrame();
	}

	void GfxTestMachine::DrawPrimitive(GFX::RAS_Primitive prim, const GFX::Vertex* vertices, size_t count)
	{
		gfx->xf->CPDrawBegin(prim, count);
		for (size_t i = 0; i < count; i++)
		{
			gfx->xf->CPVertex(&vertices[i]);
		}
		gfx->xf->CPDrawEnd();
	}

	void GfxTestMachine::DrawQuad(const GFX::Vertex* vertices)
	{
		DrawPrimitive(GFX::RAS_QUAD, vertices, 4);
	}

	void GfxTestMachine::ReadColor(int x, int y, int width, int height, std::vector<uint8_t>& rgb)
	{
		if (SoftPipeline())
		{
			// The software EFB is a plain array with the origin at the top left corner, which is
			// already the order the tests expect.
			Assert::IsTrue(gfx->pe->ReadEfb(x, y, width, height, rgb), L"the software EFB could not be read");
			return;
		}

		Assert::IsTrue(glOpen, L"no OpenGL context");

		rgb.resize((size_t)width * height * 3);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());

		// glReadPixels works bottom-up; the tests (and the PNG writer) expect the top row first.
		std::vector<uint8_t> flipped(rgb.size());
		for (int row = 0; row < height; row++)
		{
			memcpy(&flipped[(size_t)row * width * 3],
				&rgb[(size_t)(height - 1 - row) * width * 3], (size_t)width * 3);
		}
		rgb.swap(flipped);
	}

	void GfxTestMachine::ReadColorPixel(int x, int y, uint8_t rgb[3])
	{
		std::vector<uint8_t> pixels;
		ReadColor(x, y, 1, 1, pixels);
		rgb[0] = pixels[0];
		rgb[1] = pixels[1];
		rgb[2] = pixels[2];
	}

	float GfxTestMachine::ReadDepthPixel(int x, int y)
	{
		if (SoftPipeline())
		{
			uint8_t rgba[4] = { 0 };
			uint32_t z24 = 0;

			Assert::IsTrue(gfx->pe->SoftPixel(x, y, rgba, &z24), L"the software EFB could not be read");
			return (float)z24 / 16777215.0f;
		}

		Assert::IsTrue(glOpen, L"no OpenGL context");

		float depth = -1.0f;
		glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
		return depth;
	}

	bool GfxTestMachine::SaveScreenshot(const std::string& filename, int x, int y, int width, int height)
	{
		return SaveScreenshot(filename, x, y, width, height, 1);
	}

	bool GfxTestMachine::SaveScreenshot(const std::string& filename, int x, int y, int width, int height,
		int downsample)
	{
		std::vector<uint8_t> rgb;
		ReadColor(x, y, width, height, rgb);

		if (downsample <= 1)
		{
			return Util::SavePng(filename.c_str(), rgb.data(), width, height);
		}

		int outW = width / downsample;
		int outH = height / downsample;
		std::vector<uint8_t> scaled((size_t)outW * outH * 3);

		for (int oy = 0; oy < outH; oy++)
		{
			for (int ox = 0; ox < outW; ox++)
			{
				unsigned sum[3] = { 0, 0, 0 };

				for (int dy = 0; dy < downsample; dy++)
				{
					for (int dx = 0; dx < downsample; dx++)
					{
						const uint8_t* src = &rgb[(((size_t)oy * downsample + dy) * width +
							(ox * downsample + dx)) * 3];
						sum[0] += src[0];
						sum[1] += src[1];
						sum[2] += src[2];
					}
				}

				unsigned n = (unsigned)(downsample * downsample);
				uint8_t* dst = &scaled[((size_t)oy * outW + ox) * 3];
				dst[0] = (uint8_t)(sum[0] / n);
				dst[1] = (uint8_t)(sum[1] / n);
				dst[2] = (uint8_t)(sum[2] / n);
			}
		}

		return Util::SavePng(filename.c_str(), scaled.data(), (size_t)outW, (size_t)outH);
	}

	GFX::Vertex GfxTestMachine::MakeVertex(float x, float y, float z)
	{
		GFX::Vertex v = {};
		v.Position[0] = x;
		v.Position[1] = y;
		v.Position[2] = z;
		return v;
	}

	GFX::Vertex GfxTestMachine::MakeVertex(float x, float y, float z, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
	{
		GFX::Vertex v = MakeVertex(x, y, z);
		v.Col[0].R = r;
		v.Col[0].G = g;
		v.Col[0].B = b;
		v.Col[0].A = a;
		return v;
	}

	// -------------------------------------------------------------------------------------------
	// The XF vertex shader probe (transform feedback)
	// -------------------------------------------------------------------------------------------

	namespace
	{
		// The varyings of the XF vertex shader, captured by transform feedback. The order must match
		// the array passed to glTransformFeedbackVaryings and GfxTestMachine::XFVertex.
		const char* kXFVaryings[] = {
			"gl_Position",
			"v_TexCoord0", "v_TexCoord1", "v_TexCoord2", "v_TexCoord3",
			"v_TexCoord4", "v_TexCoord5", "v_TexCoord6", "v_TexCoord7",
			"v_Color0", "v_Color1",
		};
		const int kXFVaryingCount = sizeof(kXFVaryings) / sizeof(kXFVaryings[0]);
	}

	bool GfxTestMachine::RunVertexShader(const std::vector<GFX::Vertex>& in, std::vector<XFVertex>& out)
	{
		Assert::IsTrue(glOpen, L"no OpenGL context");

		if (!CreateTransformFeedbackProgram())
		{
			return false;
		}

		out.assign(in.size(), XFVertex{});
		if (in.empty())
		{
			return true;
		}

		// The vertex attributes go through the same VAO/VBO the pipeline draws with
		glBindVertexArray(gfx->vao);
		glBindBuffer(GL_ARRAY_BUFFER, gfx->vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(in.size() * sizeof(GFX::Vertex)), in.data());

		// The XF register state is uploaded to the very same vertex program the emulator uses; the
		// GLProgram wrapper is only a uniform-location cache, so it is not the owner of the program.
		GFX::GLProgram program;
		program.prog = tfProgram;
		program.Use();
		gfx->xf->UploadUniforms(program);
		program.prog = 0;

		glEnable(GL_RASTERIZER_DISCARD);

		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tfBuf);
		glBeginTransformFeedback(GL_POINTS);
		glDrawArrays(GL_POINTS, 0, (GLsizei)in.size());
		glEndTransformFeedback();

		glDisable(GL_RASTERIZER_DISCARD);

		void* ptr = glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
			(GLsizeiptr)(in.size() * sizeof(XFVertex)), GL_MAP_READ_BIT);

		if (ptr == nullptr)
		{
			lastError = "the transform feedback buffer could not be mapped";
			glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
			glBindVertexArray(0);
			return false;
		}

		memcpy(out.data(), ptr, in.size() * sizeof(XFVertex));
		glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);

		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
		glBindVertexArray(0);

		return true;
	}

	bool GfxTestMachine::CreateTransformFeedbackProgram()
	{
		if (tfProgram != 0)
		{
			return true;
		}

		Assert::IsTrue(gfx->xf->CreateShader(), L"the XF vertex shader did not compile");

		// A program needs a fragment shader to link even when the rasterizer is discarded
		static const char* fragSource =
			"#version 330 core\n"
			"out vec4 o_Color;\n"
			"void main() { o_Color = vec4(0.0, 0.0, 0.0, 1.0); }\n";

		tfFragShader = GFX::CompileShaderStage(GL_FRAGMENT_SHADER, fragSource, "GFX TEST FRAGMENT");
		if (tfFragShader == 0)
		{
			return false;
		}

		tfProgram = glCreateProgram();
		glAttachShader(tfProgram, gfx->xf->VertexShader());
		glAttachShader(tfProgram, tfFragShader);

		glTransformFeedbackVaryings(tfProgram, kXFVaryingCount, kXFVaryings, GL_INTERLEAVED_ATTRIBS);
		glLinkProgram(tfProgram);

		GLint success = 0;
		glGetProgramiv(tfProgram, GL_LINK_STATUS, &success);
		if (!success)
		{
			char infoLog[0x4000] = { 0, };
			glGetProgramInfoLog(tfProgram, sizeof(infoLog) - 1, nullptr, infoLog);
			lastError = std::string("the transform feedback program did not link: ") + infoLog;
			glDeleteProgram(tfProgram);
			tfProgram = 0;
			return false;
		}

		glGenBuffers(1, &tfBuf);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, tfBuf);
		glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,
			(GLsizeiptr)(sizeof(XFVertex) * GFX_MAX_VERTICES), nullptr, GL_DYNAMIC_READ);
		glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

		return true;
	}

	// -------------------------------------------------------------------------------------------
	// The machine singleton
	// -------------------------------------------------------------------------------------------

	GfxTestMachine& Machine()
	{
		static GfxTestMachine* instance = nullptr;

		if (instance == nullptr)
		{
			instance = new GfxTestMachine();
		}

		return *instance;
	}

	void RequireGL()
	{
		// Starting (and resetting) the machine here means a rendering test can also be run on its own,
		// not only as part of the whole suite.
		GfxTestMachine& m = Machine();
		m.Reset();

		Assert::IsTrue(m.GLEnabled(),
			Widen("no OpenGL context: " + m.LastError()).c_str());
	}

	// -------------------------------------------------------------------------------------------
	// The HTML report
	// -------------------------------------------------------------------------------------------

	namespace
	{
		struct ReportItem
		{
			std::string title;
			std::string text;
			std::string image;
			bool isSection = false;
		};

		std::vector<ReportItem> reportItems;

		void WriteReport()
		{
			std::string html;

			html += "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n";
			html += "<title>pureikyubu GFX unit test report</title>\n";
			html += "<style>\n"
				"body { font-family: Segoe UI, sans-serif; margin: 24px; background: #f6f7f9; color: #222; }\n"
				"h1 { font-size: 22px; }\n"
				"h2 { font-size: 17px; margin-top: 28px; border-bottom: 1px solid #ccd; padding-bottom: 4px; }\n"
				".item { background: #fff; border: 1px solid #dde; border-radius: 6px; padding: 12px; margin: 12px 0; }\n"
				".item h3 { font-size: 14px; margin: 0 0 6px 0; }\n"
				".item p { font-size: 13px; margin: 0 0 8px 0; color: #444; white-space: pre-wrap; }\n"
				".shots { display: flex; flex-wrap: wrap; gap: 10px; align-items: flex-start; }\n"
				"figure { margin: 0; }\n"
				"img { image-rendering: pixelated; border: 1px solid #ccc; background: #000; max-width: 420px; }\n"
				"figcaption { font-size: 12px; color: #555; margin-top: 4px; }\n"
				"</style></head><body>\n";
			html += "<h1>pureikyubu &mdash; GFX (Flipper graphics) unit test report</h1>\n";
			html += "<p>Generated by the Flipper GFX unit tests. Every image below was rendered by the "
				"emulator's own OpenGL pipeline (XF vertex shader + TEV fragment shader) into the emulated EFB "
				"and read back with glReadPixels.</p>\n";

			for (const ReportItem& item : reportItems)
			{
				if (item.isSection)
				{
					html += "<h2>" + item.title + "</h2>\n";
					if (!item.text.empty())
					{
						html += "<p>" + item.text + "</p>\n";
					}
					continue;
				}

				html += "<div class=\"item\"><h3>" + item.title + "</h3>\n";
				if (!item.text.empty())
				{
					html += "<p>" + item.text + "</p>\n";
				}
				if (!item.image.empty())
				{
					html += "<div class=\"shots\"><figure><img src=\"" + item.image +
						"\"><figcaption>" + item.image + "</figcaption></figure></div>\n";
				}
				html += "</div>\n";
			}

			html += "</body></html>\n";

			std::string path = OutputDir() + "/gfx_report.html";
			FILE* f = fopen(path.c_str(), "wb");
			if (f != nullptr)
			{
				fwrite(html.data(), 1, html.size(), f);
				fclose(f);
			}
		}
	}

	void Report::Section(const std::string& title, const std::string& text)
	{
		ReportItem item;
		item.title = title;
		item.text = text;
		item.isSection = true;
		reportItems.push_back(item);
		WriteReport();
	}

	void Report::Image(const std::string& title, const std::string& pngFile, const std::string& text)
	{
		ReportItem item;
		item.title = title;
		item.text = text;
		item.image = pngFile;
		reportItems.push_back(item);
		WriteReport();
	}

	void Report::Flush()
	{
		WriteReport();
	}
}

// -------------------------------------------------------------------------------------------
// The Flipper device doubles (these live in the emulator's own namespace)
// -------------------------------------------------------------------------------------------

namespace Flipper
{
	void* MemoryInterface::MIGetMemoryPointerForTX(uint32_t phys_addr)
	{
		return GfxUnitTest::TestMainMemory(phys_addr, 0);
	}

	void* MemoryInterface::MIGetMemoryPointerForCP(uint32_t phys_addr)
	{
		return GfxUnitTest::TestMainMemory(phys_addr, 0);
	}

	void* MemoryInterface::MIGetMemoryPointerForPI(uint32_t phys_addr)
	{
		return GfxUnitTest::TestMainMemory(phys_addr, 0);
	}

	void VideoInterface::VIDisableXfb()
	{
		// The unit tests render into an offscreen window; there is no XFB to disable.
	}
}

// -------------------------------------------------------------------------------------------
// The peripheral subsystem of the machine under test.
//
// The SI reads the state of a controller from the device that is plugged into the channel (see
// peripherals.h), so the machine plugs a standard controller into every socket. Nothing drives
// their actuators - a unit test has no host input - so a test sees the neutral pad the plug-in
// double used to answer with (no buttons, the sticks centred, the triggers released), and it can
// tell "the channel was polled" from "the channel was skipped" by the read-status bits alone.
// -------------------------------------------------------------------------------------------

namespace
{
	//! The host input of the unit tests. Nothing is pressed until a test says so (see the
	//! TestHost* calls in gfx_test_common.h), which is what makes a test see the neutral pad.
	//! The host game controllers a test can drive (a pad is driven by the controller of its port).
	const int TestHostPads = 4;

	//! The key the default layout of a pad binds to A (see TestHostInput::DefaultBindings).
	const int TestDefaultKey = 0x7000;

	class TestHostInput : public HostInput
	{
	public:
		bool present[TestHostPads] = { false };
		std::map<int, bool> keys;
		std::map<int, bool> buttons;
		std::map<int, int> axes;

		//! The last motor command every host game controller was given (PAD_MOTOR_STOP when it was
		//! never given one).
		int rumble[TestHostPads] = { PAD_MOTOR_STOP, PAD_MOTOR_STOP, PAD_MOTOR_STOP, PAD_MOTOR_STOP };

		bool KeyDown(int scancode) override
		{
			auto it = keys.find(scancode);
			return it != keys.end() && it->second;
		}

		int GamepadCount() override
		{
			int count = 0;

			for (int i = 0; i < TestHostPads; i++)
			{
				if (present[i]) count++;
			}

			return count;
		}

		std::string GamepadName(int pad) override
		{
			return present[pad] ? "Test Pad" : "";
		}

		bool GamepadButton(int pad, int button) override
		{
			auto it = buttons.find(pad * 0x100 + button);
			return it != buttons.end() && it->second;
		}

		int GamepadAxis(int pad, int axis) override
		{
			auto it = axes.find(pad * 0x100 + axis);
			return it == axes.end() ? 0 : it->second;
		}

		bool Rumble(int pad, int cmd) override
		{
			if (pad < 0 || pad >= TestHostPads)
			{
				return false;
			}

			rumble[pad] = cmd;
			return true;
		}

		//! The minimal default layout of a pad: the game controller control of A and of the right
		//! direction of the main stick for every pad, and the key of A as well - which is what the
		//! rule of StandardPad::LoadConfig is: the keys belong to the first pad of the pool only.
		void DefaultBindings(uint32_t deviceType, const char* actuator, int& keyboard, int& gamepad) override
		{
			keyboard = 0;
			gamepad = 0;

			if (deviceType != PERIPH_DEVICE_STANDARD_PAD || actuator == nullptr)
			{
				return;
			}

			if (strcmp(actuator, "A") == 0)
			{
				keyboard = TestDefaultKey;
				gamepad = PERIPH_HOST_MAKE_BUTTON(0);
			}
			else if (strcmp(actuator, "XRIGHT100") == 0)
			{
				gamepad = PERIPH_HOST_MAKE_AXIS(0, true);
			}
		}
	};

	TestHostInput* test_host_input = nullptr;
}

HostInput* HostInputCreate()
{
	test_host_input = new TestHostInput();
	return test_host_input;
}

void HostInputDestroy()
{
	delete test_host_input;
	test_host_input = nullptr;
}

void HostInputUpdate()
{
}

namespace GfxUnitTest
{
	void TestHostClear()
	{
		if (test_host_input == nullptr)
		{
			return;
		}

		test_host_input->keys.clear();
		test_host_input->buttons.clear();
		test_host_input->axes.clear();

		for (int i = 0; i < TestHostPads; i++)
		{
			test_host_input->present[i] = false;
		}
	}

	void TestHostKey(int scancode, bool down)
	{
		if (test_host_input != nullptr)
		{
			test_host_input->keys[scancode] = down;
		}
	}

	void TestHostGamepad(int pad, bool present)
	{
		if (test_host_input != nullptr && pad >= 0 && pad < TestHostPads)
		{
			test_host_input->present[pad] = present;
		}
	}

	void TestHostGamepadButton(int pad, int button, bool down)
	{
		if (test_host_input != nullptr)
		{
			test_host_input->buttons[pad * 0x100 + button] = down;
		}
	}

	void TestHostGamepadAxis(int pad, int axis, int value)
	{
		if (test_host_input != nullptr)
		{
			test_host_input->axes[pad * 0x100 + axis] = value;
		}
	}

	int TestHostRumble(int pad)
	{
		if (test_host_input == nullptr || pad < 0 || pad >= TestHostPads)
		{
			return PAD_MOTOR_STOP;
		}

		return test_host_input->rumble[pad];
	}
}

// -------------------------------------------------------------------------------------------
// The settings double. The GFX pipeline choice is stored in the configuration (see
// GFXCore::SetPipeline); a unit test has no Data/Settings file to write, so the store below keeps
// the last value in memory and the getters answer from it.
// -------------------------------------------------------------------------------------------

static std::map<std::string, int> gfxTestConfigInts;
static std::map<std::string, bool> gfxTestConfigBools;
static std::map<std::string, std::wstring> gfxTestConfigStrings;

static const char* TestConfigKey(const char* var)
{
	return var != nullptr ? var : "";
}

int GetConfigInt(const char* var, const char* path)
{
	auto it = gfxTestConfigInts.find(TestConfigKey(var));
	return (it == gfxTestConfigInts.end()) ? 0 : it->second;
}

void SetConfigInt(const char* var, int newVal, const char* path)
{
	gfxTestConfigInts[TestConfigKey(var)] = newVal;
}

bool GetConfigBool(const char* var, const char* path)
{
	auto it = gfxTestConfigBools.find(TestConfigKey(var));
	return (it == gfxTestConfigBools.end()) ? false : it->second;
}

void SetConfigBool(const char* var, bool newVal, const char* path)
{
	gfxTestConfigBools[TestConfigKey(var)] = newVal;
}

wchar_t* GetConfigString(const char* var, const char* path)
{
	static std::wstring empty;

	auto it = gfxTestConfigStrings.find(TestConfigKey(var));

	if (it == gfxTestConfigStrings.end())
	{
		return const_cast<wchar_t*>(empty.c_str());
	}

	return const_cast<wchar_t*>(it->second.c_str());
}

void SetConfigString(const char* var, const wchar_t* newVal, const char* path)
{
	gfxTestConfigStrings[TestConfigKey(var)] = newVal != nullptr ? newVal : L"";
}

// Whether the variable is there at all. A device of the peripheral pool asks this before it falls
// back to the name a configuration that predates the pool used (see peripherals.cpp).
bool ConfigValueExists(const char* var, const char* path)
{
	std::string key = TestConfigKey(var);

	return gfxTestConfigInts.count(key) != 0 ||
		gfxTestConfigBools.count(key) != 0 ||
		gfxTestConfigStrings.count(key) != 0;
}

// -------------------------------------------------------------------------------------------
// The lists of objects of the settings double (the peripheral pool is one, see config.h). A member
// of an entry is keyed by the list, the entry and the member, and the double keeps the length of
// every list it has seen, which is what a device that has never been configured looks like to the
// pool: a list that does not have its entry.
// -------------------------------------------------------------------------------------------

static std::map<std::string, int> gfxTestArrayInts;
static std::map<std::string, std::wstring> gfxTestArrayStrings;
static std::map<std::string, int> gfxTestArraySizes;

static std::string TestArrayKey(const char* var, const char* path, int index, const char* member)
{
	std::string key = path != nullptr ? path : "";
	key += '/';
	key += TestConfigKey(var);
	key += '/';
	key += std::to_string(index);
	key += '/';
	key += member != nullptr ? member : "";
	return key;
}

static std::string TestArrayListKey(const char* var, const char* path)
{
	std::string key = path != nullptr ? path : "";
	key += '/';
	key += TestConfigKey(var);
	return key;
}

int GetConfigArraySize(const char* var, const char* path)
{
	auto it = gfxTestArraySizes.find(TestArrayListKey(var, path));
	return (it == gfxTestArraySizes.end()) ? 0 : it->second;
}

bool ConfigArrayValueExists(const char* var, const char* path, int index, const char* member)
{
	std::string key = TestArrayKey(var, path, index, member);

	return gfxTestArrayInts.count(key) != 0 || gfxTestArrayStrings.count(key) != 0;
}

int GetConfigArrayInt(const char* var, const char* path, int index, const char* member, int def)
{
	auto it = gfxTestArrayInts.find(TestArrayKey(var, path, index, member));
	return (it == gfxTestArrayInts.end()) ? def : it->second;
}

const wchar_t* GetConfigArrayString(const char* var, const char* path, int index, const char* member)
{
	static std::wstring empty;

	auto it = gfxTestArrayStrings.find(TestArrayKey(var, path, index, member));

	if (it == gfxTestArrayStrings.end())
	{
		return empty.c_str();
	}

	return it->second.c_str();
}

void SetConfigArrayInt(const char* var, const char* path, int index, const char* member, int value)
{
	gfxTestArrayInts[TestArrayKey(var, path, index, member)] = value;

	int& size = gfxTestArraySizes[TestArrayListKey(var, path)];
	if (size <= index) size = index + 1;
}

void SetConfigArrayString(const char* var, const char* path, int index, const char* member, const wchar_t* value)
{
	gfxTestArrayStrings[TestArrayKey(var, path, index, member)] = value != nullptr ? value : L"";

	int& size = gfxTestArraySizes[TestArrayListKey(var, path)];
	if (size <= index) size = index + 1;
}

// -------------------------------------------------------------------------------------------
// The HW interface profiler overlay double (issue #394). The GL backend draws the overlay from
// GFX::OsdDraw (gfxosd.cpp, linked into this project); the picture it draws is built by
// Debug::HwOsd, which pulls the report through the debug interface and rasterizes it with the
// debugger's font. That machinery needs a debugger session and the shipped font data, so the
// tests answer with "there is no picture": the overlay stays off, which is its default state
// anyway, and the GL side of it is still linked and exercised.
// -------------------------------------------------------------------------------------------

namespace Debug
{
	namespace HwOsd
	{
		void Update()
		{
		}

		bool CopyImage(std::vector<uint8_t>& rgba, int* width, int* height)
		{
			return false;
		}

		uint64_t Version()
		{
			return 0;
		}
	}
}
