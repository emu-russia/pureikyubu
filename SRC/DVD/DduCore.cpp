#include "pch.h"

namespace DVD
{
	DduCore DDU;		// Singletone

	DduCore::DduCore()
	{

	}

	DduCore::~DduCore()
	{

	}

	void DduCore::OpenCover()
	{
		if (coverStatus == CoverStatus::Open)
			return;

		coverStatus = CoverStatus::Open;
		DBReport2(DbgChannel::DVD, "cover opened\n");

		// Notify master hardware
		if (openCoverCallback)
		{
			openCoverCallback();
		}
	}

	void DduCore::CloseCover()
	{
		if (coverStatus == CoverStatus::Close)
			return;

		coverStatus = CoverStatus::Close;
		DBReport2(DbgChannel::DVD, "cover closed\n");

		// Notify master hardware
		if (closeCoverCallback)
		{
			closeCoverCallback();
		}
	}

}
