// Null AX audio mixer.

#include "pch.h"

namespace Flipper
{
	AudioMixer::AudioMixer(HWConfig* config)
	{
		enabled[(size_t)AxChannel::AudioDma] = false;
		enabled[(size_t)AxChannel::DvdAudio] = false;
	}

	AudioMixer::~AudioMixer()
	{

	}

	void AudioMixer::Enable(AxChannel channel, bool enable)
	{
		enabled[(size_t)channel] = enable;
	}

	bool AudioMixer::IsEnabled(AxChannel channel)
	{
		return enabled[(size_t)channel];
	}

	void AudioMixer::SetSampleRate(AxChannel channel, AudioSampleRate value)
	{

	}

	void AudioMixer::PushBytes(AxChannel channel, uint8_t* sampleData, size_t sampleDataSize)
	{

	}
}
