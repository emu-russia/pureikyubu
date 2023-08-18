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

#include "cp.h"
#include "pe.h"
#include "xf.h"
#include "su.h"
#include "ras.h"
#include "tev.h"
#include "tx.h"

#define GFX_BLACKJACK_AND_SHADERS 0

namespace GX
{
	class GXCore
	{
		// TODO: Refactoring hacks
		void DONE_INT();
		void TOKEN_INT();

		void CP_BREAK();
		void CP_OVF();
		void CP_UVF();

		static void CPThread(void* Param);

		FifoProcessor * fifo = nullptr;	// Internal CP FIFO

		bool gxOpened = false;
		bool frame_done = true;
		bool disableDraw = false;
		bool frameReady = false;
		bool backend_started = false;

		// logging
		bool logOpcode = false;
		bool logDrawCommands = false;
		bool GpRegsLog = false;

		// Windows OpenGL stuff
#ifdef _WINDOWS
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
		GXCore();
		~GXCore();

		void Open(HWConfig* config);
		void Close();

		bool GL_LazyOpenSubsystem();
		bool GL_OpenSubsystem();
		void GL_CloseSubsystem();
		void GL_BeginFrame();
		void GL_EndFrame();
		void GPFrameDone();

		void ResizeRenderTarget(size_t width, size_t height);

		// Debug
		void DumpPIFIFO();
		void DumpCPFIFO();
		
		GLuint vert_shader;
		GLuint frag_shader;
		GLuint shader_prog;

		void UploadShaders(const char* vert_source, const char* frag_source);
		void DisposeShaders();
		void InitVBO();
		void DisposeVBO();
		void BindShadersWithVBO();

		// You probably don't need to reset the internal state of GFX because GXInit from Dolphin SDK is working hard on it

#pragma region "Interface to Flipper"

		// PI FIFO
		volatile uint32_t pi_cp_base;
		volatile uint32_t pi_cp_top;
		volatile uint32_t pi_cp_wrptr;          // also WRAP bit

		// CP Registers
		uint16_t CpReadReg(CPMappedRegister id);
		void CpWriteReg(CPMappedRegister id, uint16_t value);

		// PI->CP Registers
		uint32_t PiCpReadReg(PI_CPMappedRegister id);
		void PiCpWriteReg(PI_CPMappedRegister id, uint32_t value);

		// Streaming FIFO (32-byte burst-only)
		void FifoWriteBurst(uint8_t data[32]);

		void CPDrawDoneCallback();
		void CPDrawTokenCallback(uint16_t tokenValue);

#pragma endregion "Interface to Flipper"


#pragma region "Gfx Common"

		GenMode genmode;
		GenMsloc msloc[4];

#pragma endregion "Gfx Common"


#pragma region "Command Processor"

		CPHostRegs cpregs;		// Mapped command processor registers
		CPState cp;				// Internal registers (for setting VCD/VAT, etc.)

		Thread* cp_thread;     // CP FIFO thread
		size_t	tickPerFifo;
		int64_t	updateTbrValue;

		// Stats
		size_t cpLoads;
		size_t xfLoads;
		size_t bpLoads;

		GLuint vao;
		GLuint vbo;
		size_t vbo_size = 0x10000;		// Maximum number of vertices that can be used in Draw primitive parameters
		Vertex* vertex_data = nullptr;

		void GXWriteFifo(uint8_t dataPtr[32]);
		void loadCPReg(size_t index, uint32_t value, FifoProcessor* gxfifo);
		std::string AttrToString(VertexAttr attr);
		int gx_vtxsize(unsigned v);
		void FifoReconfigure(FifoProcessor* gxfifo);
		void* GetArrayPtr(ArrayId arrayId, int idx, int compSize);
		void FetchComp(float* comp, int count, int type, int fmt, int shft, FifoProcessor* gxfifo, ArrayId arrayId);
		void FetchNorm(float* comp, int count, int type, int fmt, int shft, FifoProcessor* gxfifo, ArrayId arrayId, bool nrmidx3);
		Color FetchColor(int type, int fmt, FifoProcessor* gxfifo, ArrayId arrayId);
		void FifoWalk(unsigned vatnum, Vertex* vtx, FifoProcessor* gxfifo);
		void GxBadFifo(uint8_t command);
		void GxCommand(FifoProcessor* gxfifo);
		void CPAbortFifo();

#pragma endregion "Command Processor"


#pragma region "Transform Unit"

		XFState xf;

		TexGenOut tgout[8]{};
		Color colora[2]{};	// lighting stage output colors (COLOR0A0 / COLOR1A1)

		bool XF_LightColorEnabled(int chan, int light);
		bool XF_LightAlphaEnabled(int chan, int light);
		void XF_DoLights(const Vertex* v);
		void XF_DoTexGen(const Vertex* v);
		void VECNormalize(float vec[3]);
		void XF_ApplyModelview(const Vertex* v, float* out, const float* in);
		void NormalTransform(const Vertex* v, float* out, const float* in);
		void GL_SetProjection(float* mtx);
		void GL_SetViewport(int x, int y, int w, int h, float znear, float zfar);
		void loadXFRegs(size_t startIdx, size_t amount, FifoProcessor* gxfifo);

#pragma endregion "Transform Unit"


#pragma region "Setup Unit"

		SU_SCIS0 scis0;		// 0x20
		SU_SCIS1 scis1;		// 0x21
		SU_TS0 ssize[8];	// 0x3n
		SU_TS1 tsize[8];	// 0x3n

		void GL_SetScissor(int x, int y, int w, int h);
		void GL_SetCullMode(int mode);
		void tryLoadTex(int id);
		void loadBPReg(size_t index, uint32_t value);

#pragma endregion "Setup Unit"


#pragma region "Rasterizers"

		// perfomance counters
		size_t tris = 0, pts = 0, lines = 0;

		bool ras_wireframe = false;			// Enable wireframe drawing of primitives (DEBUG)
		bool ras_use_texture = false;

		void RAS_Begin(RAS_Primitive prim, size_t vtx_num);
		void RAS_End();
		void RAS_SendVertex(const Vertex* v);

#pragma endregion "Rasterizers"


#pragma region "Texture Engine"

		LoadTlut0 loadtlut0;		// 0x64
		LoadTlut1 loadtlut1;		// 0x65
		TexMode0 texmode0[8];		// 0x80-0x83, 0xA0-0xA3
		TexMode1 texmode1[8];		// 0x84-0x87, 0xA4-0xA7
		TexImage0 teximg0[8];		// 0x88-0x8B, 0xA8-0xAB
		TexImage1 teximg1[8];		// 0x8C-0x8F, 0xAC-0xAF
		TexImage2 teximg2[8];		// 0x90-0x93, 0xB0-0xB3
		TexImage3 teximg3[8];		// 0x94-0x97, 0xB4-0xB7
		SetTlut settlut[8];			// 0x98-0x9B, 0xB8-0xBB
		bool texvalid[4][8];

		#define GFX_MAX_TEXTURES 32

		TexEntry* tID[8];
		Color rgbabuf[1024 * 1024];
		TexEntry tcache[GFX_MAX_TEXTURES];
		unsigned tptr;
		GLuint texlist[GFX_MAX_TEXTURES + 1];     // gl texture list (0 entry reserved)
		uint8_t tlut[1024 * 1024];  // temporary TLUT buffer

		void TexInit();
		void TexFree();
		void DumpTexture(Color* rgbaBuf, uint32_t addr, int fmt, int width, int height);
		void GetTlutCol(Color* c, unsigned id, unsigned entry);
		void RebindTexture(unsigned id);
		void LoadTexture(uint32_t addr, int id, int fmt, int width, int height);
		void LoadTlut(uint32_t addr, uint32_t tmem, uint32_t cnt);

#pragma endregion "Texture Engine"


#pragma region "TEV"

		TEVState tev{};

		// TBD: Konst regs where to?

#pragma endregion "TEV"


#pragma region "Pixel Engine"

		size_t frames = 0;
		size_t pe_done_num = 0;   // number of drawdone (PE_FINISH) events

		PERegs peregs;		// PE PI regs

		PEState pe{};		// Internal PE state

		void GL_DoSnapshot(bool sel, FILE* f, uint8_t* dst, int width, int height);
		void GL_MakeSnapshot(char* path);
		void GL_SaveBitmap(uint8_t* buf);

		// Pixel Engine mapped regs
		uint16_t PeReadReg(PEMappedRegister id);
		void PeWriteReg(PEMappedRegister id, uint16_t value);
		uint32_t EfbPeek(uint32_t addr);
		void EfbPoke(uint32_t addr, uint32_t value);

#pragma endregion "Pixel Engine"

	};
}
