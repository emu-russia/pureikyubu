// Flipper GFX Engine

/*

Very limited Flipper GFXEngine emulation. Basic OpenGL is used as a backend.

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

namespace GX
{

	// Defines the state of the entire graphics system
	struct State
	{
		PERegs peregs;
		CPHostRegs cpregs;		// Mapped command processor registers
		CPState cp;
		XFState xf;

		// PI FIFO
		volatile uint32_t    pi_cp_base;
		volatile uint32_t    pi_cp_top;
		volatile uint32_t    pi_cp_wrptr;          // also WRAP bit

		// Command Processor
		Thread* cp_thread;     // CP FIFO thread
		size_t	tickPerFifo;
		int64_t	updateTbrValue;

		// Stats
		size_t cpLoads;
		size_t xfLoads;
		size_t bpLoads;
	};

}


namespace GX
{

	class GXCore
	{
		State state;

		// TODO: Refactoring hacks
		void DONE_INT();
		void TOKEN_INT();

		void CP_BREAK();
		void CP_OVF();
		void CP_UVF();

		static void CPThread(void* Param);

		FifoProcessor * fifo = nullptr;	// Internal CP FIFO

		bool GpRegsLog = false;
		bool gxOpened = false;
		bool frame_done = true;
		bool logDrawCommands = false;
		bool disableDraw = false;

		size_t usevat;	// current VAT
		Vertex* vtx; // current vertex to collect data

		HWND hwndMain;

		uint32_t lastFifoSize;

		uint8_t cr = 0, cg = 0, cb = 0, ca = 0;
		uint32_t clear_z = -1;
		bool set_clear = false;

		bool make_shot = false;
		FILE* snap_file = nullptr;
		uint32_t snap_w, snap_h;

		HGLRC hglrc = 0;
		HDC hdcgl = 0;

		PAINTSTRUCT psFrame{};
		int frameReady = 0;

		// optionable
		uint32_t scr_w = 640, scr_h = 480;

		// perfomance counters
		uint32_t frames = 0, tris = 0, pts = 0, lines = 0;

		Vertex tri[3]{};		// triangle to be rendered

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

		// Debug
		void DumpPIFIFO();
		void DumpCPFIFO();

#pragma region "Interface to Flipper"

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


#pragma region "Command Processor"

		void GXWriteFifo(uint8_t dataPtr[32]);
		void loadCPReg(size_t index, uint32_t value, FifoProcessor* gxfifo);
		std::string AttrToString(VertexAttr attr);
		int gx_vtxsize(unsigned v);
		void FifoReconfigure(FifoProcessor* gxfifo);
		void* GetArrayPtr(ArrayId arrayId, int idx, int compSize);
		void FetchComp(float* comp, int count, int type, int fmt, int shft, FifoProcessor* gxfifo, ArrayId arrayId);
		void FetchNorm(float* comp, int count, int type, int fmt, int shft, FifoProcessor* gxfifo, ArrayId arrayId, bool nrmidx3);
		Color FetchColor(int type, int fmt, FifoProcessor* gxfifo, ArrayId arrayId);
		void FifoWalk(unsigned vatnum, FifoProcessor* gxfifo);
		void GxBadFifo(uint8_t command);
		void GxCommand(FifoProcessor* gxfifo);

#pragma endregion "Command Processor"


#pragma region "Transform Unit (Old)"

		TexGenOut tgout[8]{};
		Color rasca[2]{};	// lighting stage output colors

		void DoLights(const Vertex* v);
		void DoTexGen(const Vertex* v);
		void VECNormalize(float vec[3]);
		void ApplyModelview(float* out, const float* in);
		void NormalTransform(float* out, const float* in);
		void GL_SetProjection(float* mtx);
		void GL_SetViewport(int x, int y, int w, int h, float znear, float zfar);
		void loadXFRegs(size_t startIdx, size_t amount, FifoProcessor* gxfifo);

#pragma endregion "Transform Unit (Old)"


#pragma region "Setup Unit"

		GenMode genmode;	// TODO: SU?
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

		void GL_RenderTriangle(const Vertex* v0, const Vertex* v1, const Vertex* v2);
		void GL_RenderLine(const Vertex* v0, const Vertex* v1);
		void GL_RenderPoint(const Vertex* v0);

#pragma endregion "Rasterizers"


#pragma region "Texture Engine (Old)"

		LoadTlut0 loadtlut0;		// 0x64
		LoadTlut1 loadtlut1;		// 0x65
		TexMode0 texmode0[8];		// 0x80-0x83, 0xA0-0xA3
		TEXIMAGE0 teximg0[8];		// 0x88-0x8B, 0xA8-0xAB
		TEXIMAGE3 teximg3[8];		// 0x94-0x97, 0xB4-0xB7
		SetTlut settlut[8];			// 0x98-0x9B, 0xB8-0xBB
		bool texvalid[4][8];

		void TexInit();
		void TexFree();
		void DumpTexture(Color* rgbaBuf, uint32_t addr, int fmt, int width, int height);
		void GetTlutCol(Color* c, unsigned id, unsigned entry);
		void RebindTexture(unsigned id);
		void LoadTexture(uint32_t addr, int id, int fmt, int width, int height);
		void LoadTlut(uint32_t addr, uint32_t tmem, uint32_t cnt);

#pragma endregion "Texture Engine (Old)"


#pragma region "TEV"

		// TBD.

#pragma endregion "TEV"


#pragma region "Pixel Engine"

		Color copyClearRGBA;
		uint32_t copyClearZ;
		uint16_t tokint;
		PE_ZMODE zmode;		// 0x40
		ColMode0 cmode0;	// 0x41
		ColMode1 cmode1;	// 0x42

		void GL_SetClear(Color clr, uint32_t z);
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
