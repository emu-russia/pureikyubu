// Null AX audio mixer.
// To add sound to your OS, you need to implement this small interface.

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

	class AudioMixer
	{
		bool enabled[(size_t)AxChannel::Max];

	public:
		AudioMixer(HWConfig* config);
		~AudioMixer();

		void Enable(AxChannel channel, bool enable);
		bool IsEnabled(AxChannel channel);

		void SetSampleRate(AxChannel channel, AudioSampleRate value);

		void PushBytes(AxChannel channel, uint8_t* sampleData, size_t sampleDataSize);
	};
}
