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

	typedef struct _DduStats
	{
		int64_t bytesRead;
		int64_t bytesWrite;
		int dduToHostTransferCount;
		int hostToDduTransferCount;
	} DduStats;

	enum class DduThreadState
	{
		Idle = 0,
		WriteCommand,
		ReadManufactureInfo,
		ReadDvdData,
		GetStreamEnable,
		GetStreamOffset,
		GetStreamBogus,
	};

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
		uint8_t commandBuffer[12] = { 0 };
		int commandPtr = 0;
		uint8_t immediateBuffer[4] = { 0 };
		int immediateBufferPtr = 0;
		static const size_t cacheSize = 32 * 1024;
		uint8_t* dataCache = nullptr;
		uint8_t* streamingCache = nullptr;
		int dataCachePtr = 0;
		int streaminCachePtr = 0;
		DduThreadState state = DduThreadState::Idle;
		uint32_t seekVal = 0;
		uint32_t streamSeekVal = 0;
		uint32_t streamCount = 0;

		Thread* dduThread = nullptr;
		static void DduThreadProc(void* Parameter);

		Thread* dvdAudioThread = nullptr;
		static void DvdAudioThreadProc(void* Parameter);

		void ExecuteCommand();

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

		// Handling of DIRST signal
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


		DduStats stats = { 0 };
	};

	extern DduCore DDU;
}
