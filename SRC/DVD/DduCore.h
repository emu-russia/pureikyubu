// This component emulates the DDU connector(P9).
// Description here: https://github.com/ogamespec/dolwin/blob/master/Docs/HW/DiskInterface.md

#pragma once

#include "../Common/Thread.h"

namespace DVD
{
	enum class CoverStatus
	{
		Close = 0,
		Open,
	};

	enum class DduBusDirection
	{
		DduToHost = 0,
		HostToDdu,
	};

	typedef void (*DduCallback)();
	typedef uint8_t (*HostToDduCallback)();
	typedef void (*DduToHostCallback)(uint8_t data);

	class DduCore
	{
		CoverStatus coverStatus = CoverStatus::Close;
		DduCallback openCoverCallback = nullptr;
		DduCallback closeCoverCallback = nullptr;

		DduCallback errorCallback = nullptr;		// Device error callback

		void DeviceError();				// DIERR
		bool errorState = false;

		bool ddBusBusy = false;		// Command-in/Data-out transfer in progress
		DduBusDirection busDir;
		HostToDduCallback hostToDduCallback = nullptr;
		DduToHostCallback dduToHostCallback = nullptr;

		Thread* dduThread = nullptr;
		static void DduThreadProc(void* Parameter);

		Thread* dvdAudioThread = nullptr;
		static void DvdAudioThreadProc(void* Parameter);

	public:
		DduCore();
		~DduCore();

		// Mechanical interface to disk lid (DICOVER)
		void OpenCover();
		void CloseCover();
		CoverStatus GetCoverStatus() { return coverStatus; }

		// Host interface to cover events
		void SetCoverOpenCallback(DduCallback callback)
		{
			openCoverCallback = callback;
		}
		void SetCoverCloseCallback(DduCallback callback)
		{
			closeCoverCallback = callback;
		}

		// Handling of DIRSTb signal
		void Reset();

		void SetErrorCallback(DduCallback callback)
		{
			errorCallback = callback;
		}

		// Handling Break (DIBRK signal)
		void Break();

		// DDU Bus interface

		void SetTransferCallbacks(HostToDduCallback hostToDdu, DduToHostCallback dduToHost)
		{
			hostToDduCallback = hostToDdu;
			dduToHostCallback = dduToHost;
		}

		void StartTransfer(DduBusDirection direction);
		void TransferComplete();

		// DVD Audio related

	};

	extern DduCore DDU;
}
