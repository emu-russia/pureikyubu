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

	enum class DvdAudioSampleRate
	{
		Rate_32000 = 0,
		Rate_48000,
	};

	typedef void (*DduCallback)();
	typedef uint8_t (*HostToDduCallback)();
	typedef void (*DduToHostCallback)(uint8_t data);
	typedef void (*DduStreamCallback)(uint16_t l, uint16_t r);

	typedef struct _DduStats
	{
		int64_t bytesRead;
		int64_t bytesWrite;
		int dduToHostTransferCount;
		int hostToDduTransferCount;
		int64_t sampleCounter;
	} DduStats;

	enum class DduThreadState
	{
		Idle = 0,
		WriteCommand,
		ReadBogusData,
		ReadDvdData,
		GetStreamEnable,
		GetStreamOffset,
		GetStreamBogus,
	};

	class DduCore
	{
		CoverStatus coverStatus = CoverStatus::Close;		// Mechanical cover status
		DduCallback openCoverCallback = nullptr;
		DduCallback closeCoverCallback = nullptr;

#pragma region "Error Handling"

		void DeviceError(uint32_t reason);				// DIERR
		DduCallback errorCallback = nullptr;		// Device error callback
		bool errorState = false;				// DDU is in error state
		uint32_t errorCode = 0;					// Last error code

#pragma endregion "Error Handling"

#pragma region "DDU commands Data bus processing"

		Thread* dduThread = nullptr;
		static void DduThreadProc(void* Parameter);
		void ExecuteCommand();
		bool ddBusBusy = false;		// Command-in/Data-out transfer in progress
		static const int transferRate = 2000000;	// Bytes / second
		int64_t savedGekkoTicks = 0;		// Used to check if the next byte is ready.
		int64_t dduTicksPerByte = 0;			// How many Gekko ticks must pass to send one byte of data to the host.
		bool transferRateNoLimit = false;		// Unlimited data transfer speed (xz what can be influenced by too fast loading in games, but it doesn't seem to affect anything).
		DduBusDirection busDir;
		HostToDduCallback hostToDduCallback = nullptr;
		DduToHostCallback dduToHostCallback = nullptr;
		uint8_t commandBuffer[12] = { 0 };
		int commandPtr = 0;
		uint8_t immediateBuffer[4] = { 0 };
		int immediateBufferPtr = 0;
		static const size_t dataCacheSize = 512 * 1024;
		uint8_t* dataCache = nullptr;			// Data cache used for speculative loading of DVD data for ReadSector command
		int dataCachePtr = 0;
		DduThreadState state = DduThreadState::Idle;		// DduThread internal state
		uint32_t seekVal = 0;						// Current seek for ReadSector command (data cache size based)
		size_t transactionSize = 0;					// Hint for the next data transaction

#pragma endregion "DDU commands Data bus processing"

#pragma region "DVD Audio processing"

		Thread* dvdAudioThread = nullptr;
		static void DvdAudioThreadProc(void* Parameter);
		uint32_t streamSeekVal = 0;					// Current seek for streaming (sample-based)
		int32_t streamCount = 0;				// Decoded LR sample counter
		DduStreamCallback streamCallback = nullptr;
		bool streamClockEnabled = false;
		DvdAudioSampleRate sampleRate = DvdAudioSampleRate::Rate_32000;
		int64_t nextGekkoTicksToSample = 0;
		int64_t gekkoOneSecond = 0;
		int64_t TicksPerSample();
		bool streamEnabledByDduCommand = false;
		static const size_t streamCacheSize = 32 * 1024;
		uint8_t* streamingCache = nullptr;		// The stream cache is used to store raw ADPCM data (undecoded)
		int streamingCachePtr = 0;
		uint16_t pcmPlaybackBuffer[2 * 28] = { 0 };
		size_t pcmPlaybackCounter = 0;
		FILE* adpcmStreamFile = nullptr;
		bool adpcmStreamDump = false;
		FILE* decodedStreamFile = nullptr;
		bool decodedStreamDump = false;

#pragma endregion "DVD Audio processing"

		bool log = true;
		bool logCommands = false;
		bool logTransfers = false;

	public:
		DduCore();
		~DduCore();

#pragma region "Various helper signals (COVER, RST, BRK)"

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

#pragma endregion "Various helper signals (COVER, RST, BRK)"

#pragma region "DDU Bus interface"

		// DDU Bus interface

		void SetTransferCallbacks(HostToDduCallback hostToDdu, DduToHostCallback dduToHost)
		{
			hostToDduCallback = hostToDdu;
			dduToHostCallback = dduToHost;
		}

		void StartTransfer(DduBusDirection direction);
		void TransferComplete();

#pragma endregion "DDU Bus interface"

#pragma region "Streaming Audio interface"

		// DDU core interface to streaming audio

		void EnableAudioStreamClock(bool enable);
		bool IsAudioStreamClockEnabled() { return streamClockEnabled; }
		void SetDvdAudioSampleRate(DvdAudioSampleRate rate);
		void SetStreamCallback(DduStreamCallback callback)
		{
			streamCallback = callback;
		}

#pragma endregion "Streaming Audio interface"

		// Stats
		DduStats stats = { 0 };
		void ResetStats()
		{
			memset(&stats, 0, sizeof(stats));
		}
	};

	extern DduCore DDU;
}
