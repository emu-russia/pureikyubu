// AX audio mixer.
// To add sound to your OS, you need to implement this small interface.

#pragma once

#include <dsound.h>

namespace Flipper
{
	enum class AudioSampleRate
	{
		Rate_32000 = 0,
		Rate_48000,
	};

	enum class AxChannel
	{
		AudioDma = 0,
		DvdAudio,
		Max,
	};

	class AudioMixer;

	class AudioRing
	{
		AudioMixer* mixer;

		// Accumulate frameSize bytes in the circular buffer.
		// When we have accumulated, lock the DirectSound buffer and emit frameSize bytes there.

		// The frameSize size is not very large and not very small.
		// It should not be longer than 1 PAL / NTSC video frame, but not less than 1/3 video frame.

		static const size_t maxFrames = 16;
		static const size_t frameSize = 0x1000;				// Audio frame size
		static const size_t ringSize = maxFrames * frameSize;
		uint8_t* ringBuffer = nullptr;
		size_t ringWritePtr = 0;
		size_t ringReadPtr = 0;

		size_t CurrentSize();
		uint8_t FetchByte();
		void EmitSound();

		LPDIRECTSOUNDBUFFER DSBuffer = nullptr;

		bool enabled = false;

		size_t frameCounter = 0;

		void DumpDSBuffer();
		void DumpRing();

		void WaitBufferOverflow(void);

		size_t framesPerEmit = maxFrames / 2;

		int bufferMode = DSBPLAY_LOOPING;

	public:
		AudioRing(AudioMixer * parentInst);
		~AudioRing();

		void Enable(bool enable);
		bool IsEnabled() { return enabled; }
		void SetSampleRate(AudioSampleRate value);
		void PushBytes(uint8_t* sampleData, size_t sampleDataSize);
	};

	class AudioMixer
	{
		friend AudioRing;

		AudioRing** Sources = nullptr;
		static const size_t numSources = 2;

		LPDIRECTSOUND8 lpds = nullptr;
		LPDIRECTSOUNDBUFFER PrimaryBuffer = nullptr;

	public:
		AudioMixer(HWConfig* config);
		~AudioMixer();

		void Enable(AxChannel channel, bool enable);
		bool IsEnabled(AxChannel channel);

		void SetSampleRate(AxChannel channel, AudioSampleRate value);

		void PushBytes(AxChannel channel, uint8_t* sampleData, size_t sampleDataSize);
	};
}
