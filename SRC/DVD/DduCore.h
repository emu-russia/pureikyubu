// This component emulates the DDU connector(P9).
// Description here: https://github.com/ogamespec/dolwin/blob/master/Docs/HW/DiskInterface.md

#pragma once

namespace DVD
{
	enum class CoverStatus
	{
		Close = 0,
		Open,
	};

	typedef void (*DduCallback)();

	class DduCore
	{
		CoverStatus coverStatus = CoverStatus::Close;
		DduCallback openCoverCallback = nullptr;
		DduCallback closeCoverCallback = nullptr;

	public:
		DduCore();
		~DduCore();

		// Mechanical interface to GameCube disk lid
		void OpenCover();
		void CloseCover();
		CoverStatus GetCoverStatus() { return coverStatus; }

		// Flipper interface to cover events
		void SetCoverOpenCallback(DduCallback callback)
		{
			openCoverCallback = callback;
		}
		void SetCoverCloseCallback(DduCallback callback)
		{
			closeCoverCallback = callback;
		}

	};

	extern DduCore DDU;
}
