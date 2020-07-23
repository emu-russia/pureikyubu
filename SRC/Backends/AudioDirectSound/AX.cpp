// Audio mixer (DirectSound)
#include "pch.h"

using namespace Debug;

namespace Flipper
{
	AudioRing::AudioRing(AudioMixer* parentInst)
	{
		HRESULT hr = DS_OK;
		mixer = parentInst;

		ringBuffer = new uint8_t[ringSize + 0x10];
		memset(ringBuffer, 0, ringSize);

		ringWritePtr = 0;
		ringReadPtr = 0;

		WAVEFORMATEX waveFmt = { 0 };

		waveFmt.wFormatTag = WAVE_FORMAT_PCM;
		waveFmt.nChannels = 2;
		waveFmt.wBitsPerSample = 16;
		waveFmt.nSamplesPerSec = 44100;
		waveFmt.nBlockAlign = (waveFmt.wBitsPerSample / 8) * waveFmt.nChannels;
		waveFmt.nAvgBytesPerSec = waveFmt.nSamplesPerSec * waveFmt.nBlockAlign;
		waveFmt.cbSize = 0;

		DSBUFFERDESC desc = { 0 };

		desc.dwSize = sizeof(DSBUFFERDESC);
		desc.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLVOLUME;
		desc.dwBufferBytes = ringSize;
		desc.lpwfxFormat = &waveFmt;
		desc.guid3DAlgorithm = GUID_NULL;

		hr = mixer->lpds->CreateSoundBuffer(&desc, &DSBuffer, NULL);
		assert(hr == DS_OK);

		hr = DSBuffer->SetVolume(DSBVOLUME_MAX);
		assert(hr == DS_OK);
	}

	AudioRing::~AudioRing()
	{
		delete[] ringBuffer;

		if (DSBuffer)
		{
			DSBuffer->Stop();
			DSBuffer->Release();
		}
	}

	// Get distance between Wrptr and Rdptr
	size_t AudioRing::CurrentSize()
	{
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
		uint8_t byte = ringBuffer[ringReadPtr++];
		if (ringReadPtr >= ringSize)
		{
			ringReadPtr = 0;
		}
		return byte;
	}

	void AudioRing::WaitBufferOverflow(void)
	{
		HRESULT hr = DS_OK;
		DWORD dwWriteCursor, dwPlayCursor;

		hr = DSBuffer->GetCurrentPosition(&dwPlayCursor, &dwWriteCursor);
		assert(hr == DS_OK);

		if (dwPlayCursor < dwWriteCursor)
		{
			while (dwPlayCursor < dwWriteCursor)
			{
				hr = DSBuffer->GetCurrentPosition(&dwPlayCursor, &dwWriteCursor);
				assert(hr == DS_OK);
			}
		}
	}

	void AudioRing::EmitSound()
	{
		HRESULT hr = DS_OK;

		if (!enabled)
			return;

		// Wait

		//WaitBufferOverflow();

		// Lock and load

		PVOID part1 = nullptr;
		DWORD part1Size = 0;
		PVOID part2 = nullptr;
		DWORD part2Size = 0;

		hr = DSBuffer->Lock(0, (DWORD)(frameSize * framesPerEmit), &part1, &part1Size, &part2, &part2Size,
			DSBLOCK_FROMWRITECURSOR );
		assert(hr == DS_OK);

		uint8_t* ptr = (uint8_t *)part1;
		size_t lockedBufferSize = part1Size;

		size_t byteCount = frameSize * framesPerEmit;
		while (byteCount--)
		{
			*ptr++ = FetchByte();
			lockedBufferSize--;
			if (lockedBufferSize == 0)
			{
				ptr = (uint8_t *)part2;
				lockedBufferSize = part2Size;
				if (!ptr)
					break;
			}
		}

		hr = DSBuffer->Unlock(part1, part1Size, part2, part2Size);
		assert(hr == DS_OK);
	}

	void AudioRing::Enable(bool enable)
	{
		HRESULT hr = DS_OK;

		if (enable)
		{
			hr = DSBuffer->Play(0, 0, bufferMode);
			assert(hr == DS_OK);

			enabled = true;

			ringReadPtr = ringWritePtr = 0;
		}
		else
		{
			hr = DSBuffer->Stop();
			assert(hr == DS_OK);

			hr = DSBuffer->SetCurrentPosition(0);
			assert(hr == DS_OK);

			enabled = false;
		}
	}

	void AudioRing::SetSampleRate(AudioSampleRate value)
	{
		HRESULT hr = DSBuffer->SetFrequency(value == AudioSampleRate::Rate_32000 ? 32000 : 48000);
		assert(hr == DS_OK);
	}

	void AudioRing::PushBytes(uint8_t* sampleData, size_t sampleDataSize)
	{
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

		if (CurrentSize() >= frameSize * framesPerEmit && enabled)
		{
			EmitSound();

			frameCounter += framesPerEmit;

			//DumpRing();
			//DumpDSBuffer();
			//exit(0);
		}
	}

	void AudioRing::DumpDSBuffer()
	{
		HRESULT hr = DS_OK;

		char filename[0x100] = { 0, };
		sprintf_s(filename, sizeof(filename), "Data\\AXDSBuffer_%04i.bin", (int)frameCounter);

		hr = DSBuffer->Stop();
		assert(hr == DS_OK);

		DWORD readCursor = 0;
		DWORD writeCursor = 0;

		hr = DSBuffer->GetCurrentPosition(&readCursor, &writeCursor);
		assert(hr == DS_OK);

		Report(Channel::AX, "frame: %i, readCursor: %i, writeCursor: %i\n", frameCounter, readCursor, writeCursor);

		PVOID part1 = nullptr;
		DWORD part1Size = 0;
		PVOID part2 = nullptr;
		DWORD part2Size = 0;

		hr = DSBuffer->Lock(0, 0, &part1, &part1Size, &part2, &part2Size, DSBLOCK_ENTIREBUFFER);
		assert(hr == DS_OK);
		
		auto part1_buff = std::vector<uint8_t>();
		part1_buff.assign((uint8_t*)part1, (uint8_t*)part1 + part1Size);

		std::string filenameStr = filename;
		Util::FileSave(filenameStr, part1_buff);
		Report(Channel::AX, "DSBuffer dumped to: %s\n", filename);

		hr = DSBuffer->Unlock(part1, part1Size, part2, part2Size);
		assert(hr == DS_OK);

		hr = DSBuffer->Play(0, 0, bufferMode);
		assert(hr == DS_OK);
	}

	void AudioRing::DumpRing()
	{
		char filename[0x100] = { 0, };
		sprintf_s(filename, sizeof(filename), "Data\\AXRing_%04i.bin", (int)frameCounter);

		Report(Channel::AX, "frame: %i, readPtr: %i, writePtr: %i\n", frameCounter, ringReadPtr, ringWritePtr);
		
		auto ringBuff = std::vector<uint8_t>();
		ringBuff.assign(ringBuffer, ringBuffer + ringSize);
		
		std::string filenameStr = filename;
		Util::FileSave(filenameStr, ringBuff);
		Report(Channel::AX, "Ring dumped to: %s\n", filename);
	}

	AudioMixer::AudioMixer(HWConfig *config)
	{
		HRESULT hr = DirectSoundCreate8(NULL, &lpds, NULL);
		assert(hr == DS_OK);
		assert(lpds);

		hr = lpds->SetCooperativeLevel((HWND)config->renderTarget, DSSCL_PRIORITY);
		assert(hr == DS_OK);

		// Create primary buffer
		DSBUFFERDESC bufferDesc = { 0 };
		WAVEFORMATEX waveFormat = { 0 };

		// Setup the primary buffer description.
		bufferDesc.dwSize = sizeof(DSBUFFERDESC);
		bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;
		bufferDesc.dwBufferBytes = 0;
		bufferDesc.dwReserved = 0;
		bufferDesc.lpwfxFormat = NULL;
		bufferDesc.guid3DAlgorithm = GUID_NULL;

		hr = lpds->CreateSoundBuffer(&bufferDesc, &PrimaryBuffer, NULL);
		assert(hr == DS_OK);

		// Setup the format of the primary sound bufffer.
		waveFormat.wFormatTag = WAVE_FORMAT_PCM;
		waveFormat.nSamplesPerSec = 44100;
		waveFormat.wBitsPerSample = 16;
		waveFormat.nChannels = 2;
		waveFormat.nBlockAlign = (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels;
		waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
		waveFormat.cbSize = 0;

		hr = PrimaryBuffer->SetFormat(&waveFormat);
		assert(hr == DS_OK);

		// Create rings
		Sources = new AudioRing * [numSources];
		assert(Sources);

		for (int i = 0; i < numSources; i++)
		{
			Sources[i] = new AudioRing(this);
			assert(Sources[i]);

			// Set default sample rate
			Sources[i]->SetSampleRate(AudioSampleRate::Rate_48000);
			Sources[i]->Enable(false);
		}
	}

	AudioMixer::~AudioMixer()
	{
		// Release rings
		for (int i = 0; i < numSources; i++)
		{
			delete Sources[i];
		}
		delete [] Sources;

		PrimaryBuffer->Release();
		lpds->Release();
	}

	void AudioMixer::Enable(AxChannel channel, bool enable)
	{
		Sources[(int)channel]->Enable(enable);
	}

	bool AudioMixer::IsEnabled(AxChannel channel)
	{
		return Sources[(int)channel]->IsEnabled();
	}

	void AudioMixer::SetSampleRate(AxChannel channel, AudioSampleRate value)
	{
		Sources[(int)channel]->SetSampleRate(value);
	}

	void AudioMixer::PushBytes(AxChannel channel, uint8_t* sampleData, size_t sampleDataSize)
	{
		Sources[(int)channel]->PushBytes(sampleData, sampleDataSize);
	}

}
