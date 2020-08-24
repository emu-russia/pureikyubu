// Handling GX JDI Service Requests

#include "pch.h"

namespace GX
{

	// Dump PI/CP FIFO configuration
	static Json::Value* DumpFifo(std::vector<std::string>& args)
	{
		Flipper::Gx->DumpPIFIFO();
		Flipper::Gx->DumpCPFIFO();
		return nullptr;
	}

	static Json::Value* GXDump(std::vector<std::string>& args)
	{
		return nullptr;
	}

	void gx_init_handlers()
	{
		JDI::Hub.AddCmd("GXDump", GXDump);
		JDI::Hub.AddCmd("DumpFifo", DumpFifo);
	}

}
