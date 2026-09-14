/*

This component is used to support emulation without any audio output requiring platform-specific code.

Consider that you are running the GameCube without the sound when using the backend.

*/

// Null AX audio mixer.

#include "pch.h"

// The null mixer has no output device, so there is no buffer to fill and nothing to play. It still
// keeps the per-channel enable state: the rest of the emulator only uses it to decide whether to
// push samples, and throwing them away is exactly what "no audio output" means. The state is kept
// here rather than in the class because the two real backends (DirectSound and SDL) carry their own
// device objects instead.

static bool channelEnabled[(size_t)Flipper::AxChannel::Max];

namespace Flipper
{
	AudioMixer::AudioMixer(HWConfig* config)
	{
		channelEnabled[(size_t)AxChannel::AudioDma] = false;
		channelEnabled[(size_t)AxChannel::DvdAudio] = false;
	}

	AudioMixer::~AudioMixer()
	{

	}

	void AudioMixer::Enable(AxChannel channel, bool enable)
	{
		channelEnabled[(size_t)channel] = enable;
	}

	bool AudioMixer::IsEnabled(AxChannel channel)
	{
		return channelEnabled[(size_t)channel];
	}

	void AudioMixer::SetSampleRate(AxChannel channel, AudioSampleRate value)
	{

	}

	void AudioMixer::PushBytes(AxChannel channel, uint8_t* sampleData, size_t sampleDataSize)
	{

	}
}
