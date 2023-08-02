// DVD interface

namespace DVD
{
    void InitSubsystem();
    void ShutdownSubsystem();

    // Mount current DVD image for read/seek/open file operations
    bool MountFile(const wchar_t* file);
    bool MountFile(const std::string& file);
    bool MountFile(const std::wstring& file);

    // Mount DolphinSDK directory
    bool MountSdk(const wchar_t* path);
    bool MountSdk(std::string path);

    // Unmount
    void Unmount();

    bool IsMounted();

    // Seek and read operations on mounted DVD
    void Seek(int position);
    int GetSeek();
    bool Read(void* buffer, size_t length);

    // Open file in DVD root. Return file position, or 0 if no such file.
    // Note: DVD must be mounted first!
    // example use : long banner = DVDOpenFile("/opening.bnr");
    long OpenFile(std::string& dvdfile);
}

// GAMECUBE Disk Structures

// size of DVD image
#define DVD_SIZE            0x57058000  // 1.4 GB

// Sector size
#define DVD_SECTOR_SIZE     2048

// DiskID

#define DVD_DISKID_MAGIC 0xC2339F3D

struct DiskID
{
    char gameName[4];
    char company[2];
    uint8_t diskNumber;
    uint8_t gameVersion;
    uint8_t streaming;
    uint8_t streamingBufSize;
    uint8_t padding[18];
    uint32_t magicNumber;       // DVD_DISKID_MAGIC
};

//
// general DVD tables (BB2, BI2)
//

// offsets
#define DVD_ID_OFFSET       0x0000  // disk id
#define DVD_GAMENAME_OFFSET 0x0020  // Game name (0x400) bytes
#define DVD_BB2_OFFSET      0x0420  // BB2
#define DVD_BI2_OFFSET      0x0440  // BI2
#define DVD_APPLDR_OFFSET   0x2440  // apploader

struct DVDBB2
{
    uint32_t     bootFilePosition;          // Where DOL executable is 
    uint32_t     FSTPosition;
    uint32_t     FSTLength;
    uint32_t     FSTMaxLength;
    uint32_t     userPosition;          // FST location in memory. A strange architectural solution, one could do OSAlloc.
    uint32_t     userLength;            // FST size in memory
    uint8_t      padding[8];
};

// BI2 is omitted here..

//
// file string table (FST):  { [FST Entry] [FST Entry] ... } { NameTable }
//

#define DVD_FST_MAX_SIZE 0x00100000 // 1 mb
#define DVD_MAXPATH      256        // Complete path length, including root /

#pragma pack(push, 1)

#pragma warning (push)
#pragma warning (disable: 4201)

struct DVDFileEntry
{
    uint8_t      isDir;                  // 1, if directory
    uint8_t      nameOffsetHi;      // Relative to Name Table start
    uint16_t     nameOffsetLo;
    union
    {
        struct                      // file
        {
            uint32_t     fileOffset;        // Relative to disk start (0)
            uint32_t     fileLength;        // In bytes
        };
        struct                      // directory
        {
            uint32_t     parentOffset;   // parent directory FST index
            uint32_t     nextOffset;     // next directory FST index
        };
    };
};

// Additional information: FSTNotes.md

#pragma warning (pop)		// warning C4201: nonstandard extension used: nameless struct/union

#pragma pack(pop)


// DVD file system, based on hotquik's code from Dolwin 0.09
// externals
bool    dvd_fs_init();
void	dvd_fs_shutdown();
int     dvd_open(const char* path);


namespace DVD
{
	class MountDolphinSdk
	{
		const bool logMount = false;
		bool mounted = false;
		uint32_t currentSeek = 0;
		wchar_t directory[0x1000] = { 0 };

		Json DvdDataInfo;
		const wchar_t* DvdDataJson = L"./Data/Json/DolphinSdkDvdData.json";

		const wchar_t* AppldrPath = L"/HW2/boot/apploader.img";
		const wchar_t* Bi2Path = L"/X86/bin/bi2.bin";
		const wchar_t* FilesRoot = L"/dvddata";
		const wchar_t* DolPath = L"pong.dol";			// SDK contains demos only in ELF format :/

		std::vector<uint8_t> DiskId;
		std::vector<uint8_t> GameName;
		std::vector<uint8_t> AppldrData;
		std::vector<uint8_t> Dol;
		std::vector<uint8_t> Bb2Data;
		std::vector<uint8_t> Bi2Data;
		std::vector<uint8_t> FstData;
		std::vector<uint8_t> NameTableData;

		uint32_t userFilesStart = 16 * 1024 * 1024;
		uint32_t userFilesOffset = 0;
		int entryCounter = 0;

		bool GenDiskId();
		bool GenApploader();
		bool GenDol();
		bool GenBi2();
		bool GenBb2();

		void AddString(std::string str);
		void ParseDvdDataEntryForFst(Json::Value* entry);
		void WalkAndGenerateFst(Json::Value* entry);
		bool GenFst();

		std::list<std::tuple<std::vector<uint8_t>&, uint32_t, size_t>> mapping;		// A collection for mapping generated DVD structures (eg FST) binary blobs to the virtual DVD address space.
		std::list<std::tuple<wchar_t*, uint32_t, size_t>> fileMapping;		// Collection of mapping real files from dvddata to virtual DVD address space. The files themselves are not loaded into memory.
		bool GenMap();
		void WalkAndMapFiles(Json::Value* entry);
		bool GenFileMap();
		void MapVector(std::vector<uint8_t>& v, uint32_t offset);
		void MapFile(wchar_t* path, uint32_t offset);
		uint8_t* TranslateMemory(uint32_t offset, size_t requestedSize, size_t& maxSize);
		FILE* TranslateFile(uint32_t offset, size_t requestedSize, size_t& maxSize);

		uint32_t RoundUp32(uint32_t offset)
		{
			return (offset + 31) & ~0x1f;
		}

		uint32_t RoundUpSector(uint32_t offset)
		{
			return (offset + (DVD_SECTOR_SIZE - 1)) & ~(DVD_SECTOR_SIZE - 1);
		}

		void SwapArea(void* _addr, int sizeInBytes);

	public:
		MountDolphinSdk(const wchar_t* DolphinSDKPath);
		~MountDolphinSdk();

		bool Mounted() { return mounted; }

		void Seek(int position);
		bool Read(void* buffer, size_t length);

		int GetSeek() { return currentSeek; }
		wchar_t* GetDirectory() { return directory; }
	};
}



// Obtaining a region by DiskID(first 4 bytes of a DVD).
// Region identification by DiskID is not very reliable.

// Regions according to: http://redump.org/discs/system/gc/

namespace DVD
{
	enum class Region
	{
		Unknown = -1,

		// PAL (Europe, Australia)

		EUR,			// English or mix of other languages. GW7P, GU4Y
		NOE,			// Deutschland: GW7D
		FRA,			// France: GLZF
		ESP,			// Spain: GBDS
		ITA,			// Italy: GQCI
		FAH,			// France and Holland: GFSX, GNEK
		HOL,			// Holland: GKJH
		AUS,			// Australia: D95U

		// NTSC-J (Japan)

		JPN,			// Japan: GENJ

		// NTSC (North America, Korea)

		USA,			// GW7E
		KOR,			// Korea: GTEW
	};

	Region RegionById(const char* DiskId);

	bool IsNtsc(Region region);
}



// all important data is placed here
struct DVDControl
{
    bool mountedImage;
    wchar_t gcm_filename[0x1000];
    int   gcm_size;       // size of mounted file
    int   seekval;        // current DVD position

    DVD::MountDolphinSdk* mountedSdk;
};

extern DVDControl dvd;             // share with other modules

// DDU Core

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
	typedef uint8_t(*HostToDduCallback)();
	typedef void (*DduToHostCallback)(uint8_t data);
	typedef void (*DduStreamCallback)(uint16_t l, uint16_t r);

	struct DduStats
	{
		int64_t bytesRead;
		int64_t bytesWrite;
		int dduToHostTransferCount;
		int hostToDduTransferCount;
		int64_t sampleCounter;
	};

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

	extern DduCore* DDU;
}


// very simple GCM reading (for .gcm files)

// externals for DVD callbacks (see DVD.h)
bool    GCMMountFile(const wchar_t* file);
void    GCMSeek(int position);
bool    GCMRead(uint8_t* buf, size_t length);


// GC DVD ADPCM Decoder

void DvdAudioInitDecoder();
void DvdAudioDecode(uint8_t adpcmBuffer[32], uint16_t pcmBuffer[2 * 28]);



// DVD Banner


// banner filename. must reside in DVD root.
#define DVD_BANNER_FILENAME     "opening.bnr"

#define DVD_BANNER_WIDTH        96
#define DVD_BANNER_HEIGHT       32

#define DVD_BANNER_ID           'BNR1'  // JP/US
#define DVD_BANNER_ID2          'BNR2'  // EU

// there two formats of banner file - US/Japan and European.
// the difference is that EUR version has comments on six common european
// languages : English, Deutsch, Francais, Espanol, Italiano and Nederlands.

// JP/US version
struct DVDBanner
{
	uint32_t     id;                         // 'BNR1'
	uint32_t     padding[7];
	uint8_t      image[2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT]; // RGB5A3 texture
	uint8_t      shortTitle[32];             // game name (short, for IPL menu)
	uint8_t      shortMaker[32];             // developer
	uint8_t      longTitle[64];              // game name (long, for Dolwin =:))
	uint8_t      longMaker[64];              // developer (long description)
	uint8_t      comment[128];               // comments. may include '\n'
};

// EUR version
struct DVDBanner2
{
	uint32_t     id;                         // 'BNR2'
	uint32_t     padding[7];
	uint8_t      image[2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT]; // RGB5A3 texture

	// comments on six european languages
	struct
	{
		uint8_t  shortTitle[32];
		uint8_t  shortMaker[32];
		uint8_t  longTitle[64];
		uint8_t  longMaker[64];
		uint8_t  comment[128];
	} comments[6];
};

// banner API
std::vector<uint8_t> DVDLoadBanner(const wchar_t* dvdFile);
