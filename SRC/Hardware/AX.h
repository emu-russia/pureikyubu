// AX audio mixer

#pragma once

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

	class AudioRing
	{
	public:
		AudioRing();
		~AudioRing();
	};

	class AudioMixer
	{
		AudioRing** Sources = nullptr;
		size_t numSources = 0;

	public:
		AudioMixer();
		~AudioMixer();

		void SetVolumeL(AxChannel channel, uint8_t volume);
		void SetVolumeR(AxChannel channel, uint8_t volume);

		void SetSampleRate(AxChannel channel, AudioSampleRate value);

		void PushBytes(AxChannel channel, uint8_t* sampleData, size_t sampleDataSize);
	};
}
