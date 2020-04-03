// Audio mixer (DirectSound)
#include "pch.h"

namespace Flipper
{
	AudioRing::AudioRing()
	{

	}
	AudioRing::~AudioRing()
	{

	}

	AudioMixer::AudioMixer()
	{
	}

	AudioMixer::~AudioMixer()
	{
	}

	void AudioMixer::SetVolumeL(AxChannel channel, uint8_t volume)
	{
	}

	void AudioMixer::SetVolumeR(AxChannel channel, uint8_t volume)
	{
	}

	void AudioMixer::SetSampleRate(AxChannel channel, AudioSampleRate value)
	{
	}

	void AudioMixer::PushBytes(AxChannel channel, uint8_t* sampleData, size_t sampleDataSize)
	{
	}
}
