// An abstract class that defines what the graphic backend should be able to do

#pragma once

namespace GX
{
	class GXCore;		// Forward reference to parent class

	class GXBackend
	{
	public:
		GXBackend(GXCore* parent);
		virtual ~GXBackend();

		virtual void PassCPLoadReg(CPRegister id) = 0;
		virtual void PassBPLoadReg(BPRegister id) = 0;
		virtual void PassXFLoadReg(int startId, int count) = 0;

		virtual void DrawBegin(Primitive prim) = 0;
		virtual void DrawEnd() = 0;

		virtual void ProcessVertex(Vertex& v) = 0;
	};

}
