// Audio mixer (SDL2)
#include "pch.h"

using namespace Debug;

namespace Flipper
{
    AudioRing::AudioRing(AudioMixer* parentInst)
    {
        mixer = parentInst;

        ringBuffer = new uint8_t[ringSize];
        memset(ringBuffer, 0, ringSize);

        ringWritePtr = 0;
        ringReadPtr = 0;

        // Initialize SDL audio specification
        SDL_zero(audioSpec);
        audioSpec.freq = 48000;  // Default sample rate
        audioSpec.format = AUDIO_S16LSB;  // 16-bit PCM, little endian (standard for Windows)
        audioSpec.channels = 2;   // Stereo
        audioSpec.samples = static_cast<Uint16>(frameSize / 4);  // samples = bytes / (bytes per sample * channels)
        audioSpec.callback = SDLCallback;
        audioSpec.userdata = this;

        // Open audio device
        audioDevice = SDL_OpenAudioDevice(NULL, 0, &audioSpec, NULL, 0);
        if (audioDevice == 0) {
            Report(Channel::AX, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
            assert(audioDevice != 0);
        }

        initialized = true;
    }

    AudioRing::~AudioRing()
    {
        if (initialized && audioDevice != 0)
        {
            SDL_CloseAudioDevice(audioDevice);
        }

        delete[] ringBuffer;
    }

    size_t AudioRing::CurrentSize()
    {
        // Calculate available bytes in ring buffer
        if (ringWritePtr >= ringReadPtr)
        {
            return ringWritePtr - ringReadPtr;
        }
        else
        {
            return ringWritePtr + (ringSize - ringReadPtr);
        }
    }

    uint8_t AudioRing::FetchByte()
    {
        uint8_t byte = ringBuffer[ringReadPtr];
        ringReadPtr = (ringReadPtr + 1) % ringSize;
        return byte;
    }

    void AudioRing::SDLCallback(void* userdata, Uint8* stream, int len)
    {
        AudioRing* ring = static_cast<AudioRing*>(userdata);

        if (!ring->enabled)
        {
            // Fill with silence if not enabled
            memset(stream, 0, len);
            return;
        }

        Uint8* writePtr = stream;
        int remaining = len;

        while (remaining > 0)
        {
            size_t available = ring->CurrentSize();

            if (available == 0)
            {
                // Underflow - fill remaining with silence
                memset(writePtr, 0, remaining);
                ring->frameCounter++;
                break;
            }

            size_t bytesToCopy = my_min(static_cast<size_t>(remaining), available);

            // Copy data in two parts if it wraps around
            size_t firstPart = my_min(bytesToCopy, ring->ringSize - ring->ringReadPtr);

            // Copy first part
            memcpy(writePtr, &ring->ringBuffer[ring->ringReadPtr], firstPart);

            // Update pointers
            ring->ringReadPtr = (ring->ringReadPtr + firstPart) % ring->ringSize;
            writePtr += firstPart;
            remaining -= (int)firstPart;
            bytesToCopy -= firstPart;

            // Copy second part if needed
            if (bytesToCopy > 0)
            {
                memcpy(writePtr, &ring->ringBuffer[ring->ringReadPtr], bytesToCopy);
                ring->ringReadPtr = (ring->ringReadPtr + bytesToCopy) % ring->ringSize;
                writePtr += bytesToCopy;
                remaining -= (int)bytesToCopy;
            }
        }
    }

    void AudioRing::Enable(bool enable)
    {
        if (!initialized)
            return;

        if (enable)
        {
            // Clear buffers when enabling
            ringReadPtr = ringWritePtr = 0;
            memset(ringBuffer, 0, ringSize);

            // Start audio playback (0 = unpause)
            SDL_PauseAudioDevice(audioDevice, 0);

            enabled = true;
            Report(Channel::AX, "Audio enabled\n");
        }
        else
        {
            // Pause audio playback (1 = pause)
            SDL_PauseAudioDevice(audioDevice, 1);

            enabled = false;
            Report(Channel::AX, "Audio disabled\n");
        }
    }

    void AudioRing::SetSampleRate(AudioSampleRate value)
    {
        if (!initialized)
            return;

        int newFreq = (value == AudioSampleRate::Rate_32000) ? 32000 : 48000;

        // Only change if different
        if (audioSpec.freq == newFreq)
            return;

        bool wasEnabled = enabled;

        if (wasEnabled)
        {
            Enable(false);
        }

        // Close and reopen with new sample rate
        SDL_CloseAudioDevice(audioDevice);

        audioSpec.freq = newFreq;

        audioDevice = SDL_OpenAudioDevice(NULL, 0, &audioSpec, NULL, 0);
        assert(audioDevice != 0);

        Report(Channel::AX, "Sample rate changed to %d Hz\n", newFreq);

        if (wasEnabled)
        {
            Enable(true);
        }
    }

    void AudioRing::PushBytes(uint8_t* sampleData, size_t sampleDataSize)
    {
        if (!enabled || sampleDataSize == 0)
            return;

        SDL_LockAudioDevice(audioDevice);

        while (sampleDataSize != 0)
        {
            ringBuffer[ringWritePtr + 1] = *sampleData++;
            ringBuffer[ringWritePtr] = *sampleData++;
            ringWritePtr += 2;
            sampleDataSize -= 2;
            if (ringWritePtr >= ringSize)
            {
                ringWritePtr = 0;
            }
        }

        SDL_UnlockAudioDevice(audioDevice);
    }

    void AudioRing::DumpRing()
    {
        char filename[0x100] = { 0, };
        sprintf_s(filename, sizeof(filename), "Data\\AXRing_%04zu.bin", frameCounter);

        Report(Channel::AX, "frame: %zu, readPtr: %zu, writePtr: %zu, size: %zu\n",
            frameCounter, ringReadPtr, ringWritePtr, CurrentSize());

        auto ringBuff = std::vector<uint8_t>();
        ringBuff.assign(ringBuffer, ringBuffer + ringSize);

        std::string filenameStr = filename;
        Util::FileSave(filenameStr, ringBuff);
        Report(Channel::AX, "Ring dumped to: %s\n", filename);
    }

    AudioMixer::AudioMixer(HWConfig* config)
    {
        // Initialize SDL audio subsystem
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            Report(Channel::AX, "SDL_InitSubSystem failed: %s\n", SDL_GetError());
            assert(false);
        }

        // Create rings
        Sources = new AudioRing * [numSources];
        assert(Sources);

        for (int i = 0; i < numSources; i++)
        {
            Sources[i] = new AudioRing(this);
            assert(Sources[i]);

            // Set default sample rate
            Sources[i]->SetSampleRate(AudioSampleRate::Rate_48000);
        }

        Report(Channel::AX, "AudioMixer initialized\n");
    }

    AudioMixer::~AudioMixer()
    {
        // Disable all sources first
        for (int i = 0; i < numSources; i++)
        {
            if (Sources[i]) {
                Sources[i]->Enable(false);
            }
        }

        // Release rings
        for (int i = 0; i < numSources; i++)
        {
            delete Sources[i];
        }
        delete[] Sources;

        // Quit SDL audio subsystem
        SDL_QuitSubSystem(SDL_INIT_AUDIO);

        Report(Channel::AX, "AudioMixer shutdown\n");
    }

    void AudioMixer::Enable(AxChannel channel, bool enable)
    {
        if (channel < AxChannel::Max) {
            Sources[(int)channel]->Enable(enable);
        }
    }

    bool AudioMixer::IsEnabled(AxChannel channel)
    {
        if (channel < AxChannel::Max) {
            return Sources[(int)channel]->IsEnabled();
        }
        return false;
    }

    void AudioMixer::SetSampleRate(AxChannel channel, AudioSampleRate value)
    {
        if (channel < AxChannel::Max) {
            Sources[(int)channel]->SetSampleRate(value);
        }
    }

    void AudioMixer::PushBytes(AxChannel channel, uint8_t* sampleData, size_t sampleDataSize)
    {
        if (channel < AxChannel::Max && sampleData && sampleDataSize > 0) {
            Sources[(int)channel]->PushBytes(sampleData, sampleDataSize);
        }
    }
}