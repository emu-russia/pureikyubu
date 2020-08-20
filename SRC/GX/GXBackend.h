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

	};

}
