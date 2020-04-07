#include "pch.h"

namespace DVD
{
	DduCore DDU;		// Singletone

	DduCore::DduCore()
	{
		dduThread = new Thread(DduThreadProc, true, this);
		assert(dduThread);
		dvdAudioThread = new Thread(DvdAudioThreadProc, true, this);
		assert(dvdAudioThread);

		dataCache = new uint8_t[dataCacheSize];
		assert(dataCache);
		memset(dataCache, 0, dataCacheSize);
		streamingCache = new uint8_t[streamCacheSize];
		assert(streamingCache);
		memset(streamingCache, 0, streamCacheSize);

		Reset();
	}

	DduCore::~DduCore()
	{
		TransferComplete();
		delete dduThread;
		delete dvdAudioThread;
		delete[] dataCache;
		delete[] streamingCache;
	}

	void DduCore::ExecuteCommand()
	{
		// Execute command

		errorState = false;
		errorCode = 0;

		if (log)
		{
			DBReport2(DbgChannel::DVD, "Command: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n",
				commandBuffer[0], commandBuffer[1], commandBuffer[2], commandBuffer[3],
				commandBuffer[4], commandBuffer[5], commandBuffer[6], commandBuffer[7],
				commandBuffer[8], commandBuffer[9], commandBuffer[10], commandBuffer[11]);
		}

		switch (commandBuffer[0])
		{
			// Inquiry, read manufacture info (DMA)
			case 0x12:
				state = DduThreadState::ReadBogusData;
				break;

			// read sector / disk id
			case 0xA8:
				state = DduThreadState::ReadDvdData;
				{
					uint32_t seekTemp = (commandBuffer[4] << 24) |
						(commandBuffer[5] << 16) |
						(commandBuffer[6] << 8) |
						(commandBuffer[7]);
					seekVal = seekTemp << 2;

					// Use transaction size as hint for pre-caching
					if (commandBuffer[3] == 0x40)
					{
						transactionSize = sizeof(DiskID);
					}
					else
					{
						transactionSize = (commandBuffer[8] << 24) |
							(commandBuffer[9] << 16) |
							(commandBuffer[10] << 8) |
							(commandBuffer[11]);
					}
				}
				// Invalidate reading cache
				dataCachePtr = dataCacheSize;
				break;

			// seek
			case 0xAB:
				state = DduThreadState::ReadBogusData;
				break;

			// set stream (IMM)
			case 0xE1:
				state = DduThreadState::ReadBogusData;

				{
					uint32_t seekTemp = (commandBuffer[4] << 24) |
						(commandBuffer[5] << 16) |
						(commandBuffer[6] << 8) |
						(commandBuffer[7]);
					streamSeekVal = seekTemp << 2;
					streamCount = (commandBuffer[8] << 24) |
						(commandBuffer[9] << 16) |
						(commandBuffer[10] << 8) |
						(commandBuffer[11]);
				}

				if (log)
				{
					DBReport2(DbgChannel::DVD, "DVD Streaming setup. DVD positioned on track, starting %08X, %i bytes long.\n",
						streamSeekVal, streamCount);
				}
				break;

			// get stream status (IMM)
			case 0xE2:
				switch (commandBuffer[1])
				{
					case 0:             // Get stream enable
						state = DduThreadState::GetStreamEnable;
						immediateBuffer[0] = 0;
						immediateBuffer[1] = 0;
						immediateBuffer[2] = 0;
						immediateBuffer[3] = streamEnabledByDduCommand ? 1 : 0;
						immediateBufferPtr = 0;
						break;
					case 1:             // Get stream address
						state = DduThreadState::GetStreamOffset;
						{
							uint32_t seekTemp = streamSeekVal >> 2;
							immediateBuffer[0] = (seekTemp >> 24) & 0xff;
							immediateBuffer[1] = (seekTemp >> 16) & 0xff;
							immediateBuffer[2] = (seekTemp >> 8) & 0xff;
							immediateBuffer[3] = (seekTemp) & 0xff;
						}
						immediateBufferPtr = 0;
						break;
					default:
						state = DduThreadState::GetStreamBogus;
						DBReport2(DbgChannel::DVD, "Unknown GetStreamStatus: %i\n", commandBuffer[1]);
						break;
				}
				break;

			// stop motor
			case 0xE3:
				state = DduThreadState::ReadBogusData;
				break;

			// stream control (IMM)
			case 0xE4:
				state = DduThreadState::ReadBogusData;

				if (commandBuffer[1] & 1)
				{
					SetDvdAudioSampleRate(sampleRate);
					DvdAudioInitDecoder();
					streamEnabledByDduCommand = true;
				}
				else
				{
					streamEnabledByDduCommand = false;
				}

				// Invalidate streaming cache
				streamingCachePtr = streamCacheSize;

				if (log)
				{
					DBReport2(DbgChannel::DVD, "DVD Streaming. Play(?) : %s\n", (commandBuffer[1] & 1) ? "On" : "Off");
				}
				break;

			default:
				state = DduThreadState::ReadBogusData;

				DBReport2(DbgChannel::DVD, "Unknown DDU command: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n",
					commandBuffer[0], commandBuffer[1], commandBuffer[2], commandBuffer[3],
					commandBuffer[4], commandBuffer[5], commandBuffer[6], commandBuffer[7],
					commandBuffer[8], commandBuffer[9], commandBuffer[10], commandBuffer[11]);
				break;
		}

		commandPtr = 0;
	}

	void DduCore::DduThreadProc(void* Parameter)
	{
		DduCore* core = (DduCore*)Parameter;

		while (true)
		{
			// Wait Gekko ticks
			if (!core->transferRateNoLimit)
			{
				int64_t ticks = Gekko::Gekko->GetTicks();
				if (ticks >= core->savedGekkoTicks)
				{
					core->savedGekkoTicks = ticks + core->dduTicksPerByte;
				}
				else
				{
					continue;
				}
			}

			// Until break or transfer completed
			while (core->ddBusBusy)
			{
				if (core->busDir == DduBusDirection::HostToDdu)
				{
					switch (core->state)
					{
						case DduThreadState::WriteCommand:
							if (core->commandPtr < sizeof(core->commandBuffer))
							{
								core->commandBuffer[core->commandPtr] = core->hostToDduCallback();
								core->stats.bytesWrite++;
								core->commandPtr++;
							}

							if (core->commandPtr >= sizeof(core->commandBuffer))
							{
								core->ExecuteCommand();
							}
							break;

						// Hidden debug commands are not supported yet

						default:
							core->DeviceError(0);
							break;
					}
				}
				else
				{
					switch (core->state)
					{
						case DduThreadState::ReadDvdData:
							// Read-ahead new DVD data
							if (core->dataCachePtr >= dataCacheSize)
							{
								Seek(core->seekVal);
								size_t bytes = min(dataCacheSize, core->transactionSize);
								bool readResult = Read(core->dataCache, bytes);
								core->seekVal += (uint32_t)bytes;
								core->transactionSize -= bytes;

								if (core->seekVal >= DVD_SIZE || !readResult)
								{
									core->DeviceError(0);
								}

								core->dataCachePtr = 0;
							}

							core->dduToHostCallback(core->dataCache[core->dataCachePtr]);
							core->stats.bytesRead++;
							core->dataCachePtr++;
							break;

						case DduThreadState::ReadBogusData:
							core->dduToHostCallback(0);
							core->stats.bytesRead++;
							break;

						case DduThreadState::GetStreamEnable:
						case DduThreadState::GetStreamOffset:
						case DduThreadState::GetStreamBogus:
							if (core->immediateBufferPtr < sizeof(core->immediateBuffer))
							{
								core->dduToHostCallback(core->immediateBuffer[core->immediateBufferPtr]);
								core->stats.bytesRead++;
								core->immediateBufferPtr++;
							}
							else
							{
								core->DeviceError(0);
							}
							break;

						case DduThreadState::Idle:
							break;

						default:
							core->DeviceError(0);
							break;
					}
				}
			}

			// Sleep until next transfer
			core->dduThread->Suspend();
		}
	}

	// Enabling AISCLK forces the DDU to issue samples out even if there are none (zeros goes to output).
	void DduCore::EnableAudioStreamClock(bool enable)
	{
		if (enable)
		{
			dvdAudioThread->Resume();
		}
		else
		{
			dvdAudioThread->Suspend();
		}
	}

	void DduCore::DvdAudioThreadProc(void* Parameter)
	{
		uint16_t sample[2] = { 0, 0 };
		DduCore* core = (DduCore*)Parameter;

		while (true)
		{
			// If AISCLK is enabled but streaming is not enabled by the DDU command, DVD Audio will output only zeros.

			// If its time to send sample
			int64_t ticks = Gekko::Gekko->GetTicks();
			if (ticks < core->nextGekkoTicksToSample)
			{
				continue;
			}
			core->nextGekkoTicksToSample = ticks + core->TicksPerSample();

			// Invalidate cache
			if (core->streamEnabledByDduCommand)
			{
				if (core->streamingCachePtr >= streamCacheSize)
				{
					Seek(core->streamSeekVal);
					bool readResult = Read(core->streamingCache, streamCacheSize);

					if (!readResult)
					{
						core->DeviceError(0);
					}
				}
			}

			// From changing the playback frequency, the size of the ADPCM data does not change. The frequency of samples output to the outside changes.

			if (core->streamEnabledByDduCommand)
			{
				uint8_t* rawPtr = &core->streamingCache[core->streamingCachePtr];

				// TODO: Decode next Adpcm sample (LR)
				// Dirty hack to hear some noise
				sample[0] = (rawPtr[0] << 8) | rawPtr[1];
				sample[1] = (rawPtr[2] << 8) | rawPtr[3];

				core->streamSeekVal += 4;
				core->streamingCachePtr += 4;
			}
			else
			{
				sample[0] = 0;
				sample[1] = 0;
			}

			// Send sample

			if (core->streamCallback)
			{
				if (core->streamClockEnabled)
				{
					core->streamCallback(sample[0], sample[1]);
				}
				else
				{
					// If AIS clock is disabled just send zeros
					core->streamCallback(0, 0);
				}
			}

			core->stats.sampleCounter++;

			if (core->streamEnabledByDduCommand)
			{
				core->streamCount--;
				if (core->streamCount == 0)
				{
					core->streamEnabledByDduCommand = false;
				}
			}
		}
	}

	// Calculates how many Gekko ticks takes 1 sample, at the selected sample rate.
	int64_t DduCore::TicksPerSample()
	{
		if (sampleRate == DvdAudioSampleRate::Rate_32000)
		{
			// 32 kHz means 32,000 LR samples per second come from DVD Audio.
			// To find out how many ticks a sample takes, you need to divide the number of Gekko ticks per second by 32000.
			return gekkoOneSecond / 32000;
		}
		else
		{
			return gekkoOneSecond / 48000;
		}
	}

	void DduCore::SetDvdAudioSampleRate(DvdAudioSampleRate rate)
	{
		sampleRate = rate;
		nextGekkoTicksToSample = Gekko::Gekko->GetTicks() + TicksPerSample();
	}

	// Reset internal state. If you forget something, then it will come out later..
	void DduCore::Reset()
	{
		dduThread->Suspend();
		dvdAudioThread->Suspend();
		ddBusBusy = false;
		errorState = false;
		commandPtr = 0;
		dataCachePtr = dataCacheSize;
		streamingCachePtr = streamCacheSize;
		state = DduThreadState::WriteCommand;
		ResetStats();
		streamClockEnabled = false;
		gekkoOneSecond = Gekko::Gekko->OneSecond();
		dduTicksPerByte = gekkoOneSecond / transferRate;
		SetDvdAudioSampleRate(DvdAudioSampleRate::Rate_48000);
		streamEnabledByDduCommand = false;
	}

	void DduCore::Break()
	{
		// Abort data transfer
		DBReport2(DbgChannel::DVD, "DDU Break");
		ddBusBusy = false;
	}

	void DduCore::OpenCover()
	{
		if (coverStatus == CoverStatus::Open)
			return;

		coverStatus = CoverStatus::Open;
		DBReport2(DbgChannel::DVD, "cover opened\n");

		// Notify host hardware
		if (openCoverCallback)
		{
			openCoverCallback();
		}
	}

	void DduCore::CloseCover()
	{
		if (coverStatus == CoverStatus::Close)
			return;

		coverStatus = CoverStatus::Close;
		DBReport2(DbgChannel::DVD, "cover closed\n");

		// Notify host hardware
		if (closeCoverCallback)
		{
			closeCoverCallback();
		}
	}

	void DduCore::DeviceError(uint32_t reason)
	{
		DBReport2(DbgChannel::DVD, "DDU DeviceError: %08X\n", reason);
		// Will deassert DIERR after the next command is received from the host
		errorState = true;
		errorCode = reason;
		ddBusBusy = false;
		if (errorCallback)
		{
			errorCallback();
		}
	}

	void DduCore::StartTransfer(DduBusDirection direction)
	{
		if (log)
		{
			DBReport2(DbgChannel::DVD, "StartTransfer: %s\n", direction == DduBusDirection::DduToHost ? "Ddu->Host" : "Host->Ddu");
		}

		ddBusBusy = true;
		busDir = direction;

		savedGekkoTicks = Gekko::Gekko->GetTicks() + dduTicksPerByte;

		dduThread->Resume();
	}

	void DduCore::TransferComplete()
	{
		if (log)
		{
			DBReport2(DbgChannel::DVD, "TransferComplete");
		}

		ddBusBusy = false;

		switch (state)
		{
			case DduThreadState::WriteCommand:
				// Invalidate reading cache
				dataCachePtr = dataCacheSize;
				break;

			case DduThreadState::Idle:
			case DduThreadState::ReadDvdData:
			case DduThreadState::ReadBogusData:
			case DduThreadState::GetStreamEnable:
			case DduThreadState::GetStreamOffset:
			case DduThreadState::GetStreamBogus:
				state = DduThreadState::WriteCommand;
				break;
		}

		if (busDir == DduBusDirection::DduToHost)
		{
			stats.dduToHostTransferCount++;
		}
		else
		{
			stats.hostToDduTransferCount++;
		}
	}

}
