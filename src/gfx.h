// Flipper GFX Engine
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

		FifoProcessor * fifo;		// Internal CP FIFO

		bool GpRegsLog = false;

	public:
		GXCore();
		~GXCore();

		void Open();
		void Close();

		bool GL_LazyOpenSubsystem(void* hwnd);
		bool GL_OpenSubsystem();
		void GL_CloseSubsystem();
		void GL_BeginFrame();
		void GL_EndFrame();

		// Debug
		void DumpPIFIFO();
		void DumpCPFIFO();

#pragma region "Interface to Flipper"

		// CP Registers
		uint16_t CpReadReg(CPMappedRegister id);
		void CpWriteReg(CPMappedRegister id, uint16_t value);

		// Pixel Engine
		uint16_t PeReadReg(PEMappedRegister id);
		void PeWriteReg(PEMappedRegister id, uint16_t value);
		uint32_t EfbPeek(uint32_t addr);
		void EfbPoke(uint32_t addr, uint32_t value);

		// PI->CP Registers
		uint32_t PiCpReadReg(PI_CPMappedRegister id);
		void PiCpWriteReg(PI_CPMappedRegister id, uint32_t value);

		// Streaming FIFO (32-byte burst-only)
		void FifoWriteBurst(uint8_t data[32]);

		// TODO: Refactoring hacks
		void CPDrawDoneCallback();
		void CPDrawTokenCallback(uint16_t tokenValue);

#pragma endregion "Interface to Flipper"


#pragma region "Command Processor"

		void loadCPReg(size_t index, uint32_t value);

#pragma endregion "Command Processor"


#pragma region "Setup Unit"

		void GL_SetScissor(int x, int y, int w, int h);
		void GL_SetCullMode(int mode);
		void tryLoadTex(int id);
		void loadBPReg(size_t index, uint32_t value);

#pragma endregion "Setup Unit"

#pragma region "Texture Engine (Old)"

		void TexInit();
		void TexFree();
		void DumpTexture(Color* rgbaBuf, uint32_t addr, int fmt, int width, int height);
		void GetTlutCol(Color* c, unsigned id, unsigned entry);
		void RebindTexture(unsigned id);
		void LoadTexture(uint32_t addr, int id, int fmt, int width, int height);
		void LoadTlut(uint32_t addr, uint32_t tmem, uint32_t cnt);

#pragma endregion "Texture Engine (Old)"


#pragma region "Transform Unit (Old)"

		void DoLights(const Vertex* v);
		void DoTexGen(const Vertex* v);
		void VECNormalize(float vec[3]);
		void ApplyModelview(float* out, const float* in);
		void NormalTransform(float* out, const float* in);
		void GL_SetProjection(float* mtx);
		void GL_SetViewport(int x, int y, int w, int h, float znear, float zfar);
		void loadXFRegs(size_t startIdx, size_t amount);

#pragma endregion "Transform Unit (Old)"


#pragma region "Rasterizers"

		void GL_RenderTriangle(const Vertex* v0, const Vertex* v1, const Vertex* v2);
		void GL_RenderLine(const Vertex* v0, const Vertex* v1);
		void GL_RenderPoint(const Vertex* v0);

#pragma endregion "Rasterizers"


#pragma region "Pixel Engine"

		void GL_SetClear(Color clr, uint32_t z);
		void GL_DoSnapshot(bool sel, FILE* f, uint8_t* dst, int width, int height);
		void GL_MakeSnapshot(char* path);
		void GL_SaveBitmap(uint8_t* buf);

#pragma endregion "Pixel Engine"

	};

}
