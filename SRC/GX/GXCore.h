// Main module with interface for Flipper (HW)

#pragma once

namespace GX
{

	class GXCore
	{
		friend GXBackend;
		GXBackend* backend = nullptr;

		TextureConverter texconv;
		TextureCache texcache;

		State state;

	public:
		GXCore();
		~GXCore();

		void Reset();

#pragma region "Interface to Flipper"

#pragma endregion "Interface to Flipper"


	};

}
