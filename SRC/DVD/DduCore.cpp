#include "pch.h"

namespace DVD
{
	DduCore DDU;		// Singletone

	DduCore::DduCore()
	{
		dduTicksPerByte = Gekko::Gekko->OneSecond() / transferRate;

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
		delete dduThread;
		delete dvdAudioThread;
		delete[] dataCache;
		delete[] streamingCache;
	}

	void DduCore::ExecuteCommand()
	{
		// Execute command

		errorState = false;

		if (log)
		{
			DBReport2(DbgChannel::DVD, "Command: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n",
				commandBuffer[0], commandBuffer[1], commandBuffer[2], commandBuffer[3],
				commandBuffer[4], commandBuffer[5], commandBuffer[6], commandBuffer[7],
				commandBuffer[8], commandBuffer[9], commandBuffer[10], commandBuffer[11]);
		}

		switch (commandBuffer[0])
		{
			// read manufacture info (DMA)
			case 0x12:
				state = DduThreadState::ReadBogusData;
				break;

			// read sector (disk id)
			case 0xA8:
				state = DduThreadState::ReadDvdData;
				{
					uint32_t seekTemp = (commandBuffer[4] << 24) |
						(commandBuffer[5] << 16) |
						(commandBuffer[6] << 8) |
						(commandBuffer[7]);
					seekVal = seekTemp << 2;

					// Use transaction size as hint for pre-caching
					transactionSize = (commandBuffer[8] << 24) |
						(commandBuffer[9] << 16) |
						(commandBuffer[10] << 8) |
						(commandBuffer[11]);
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
						immediateBuffer[3] = dvdAudioThread->IsRunning() ? 1 : 0;
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
						immediateBuffer[0] = 0;
						immediateBuffer[1] = 0;
						immediateBuffer[2] = 0;
						immediateBuffer[3] = 0;
						immediateBufferPtr = 0;
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
					dvdAudioThread->Resume();
				}
				else
				{
					dvdAudioThread->Suspend();
				}

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
								core->commandPtr++;
							}

							if (core->commandPtr >= sizeof(core->commandBuffer))
							{
								core->ExecuteCommand();
							}
							break;

						// Hidden debug commands are not supported yet

						default:
							core->DeviceError();
							break;
					}
				}
				else
				{
					switch (core->state)
					{
						case DduThreadState::ReadDvdData:
							// Read-ahead new DVD data
							if (core->dataCachePtr >= core->dataCacheSize)
							{
								Seek(core->seekVal);
								size_t bytes = min(core->dataCacheSize, core->transactionSize);
								bool readResult = Read(core->dataCache, bytes);
								core->seekVal += bytes;
								core->transactionSize -= bytes;

								if (core->seekVal >= DVD_SIZE || !readResult)
								{
									core->DeviceError();
								}

								core->dataCachePtr = 0;
							}

							core->dduToHostCallback(core->dataCache[core->dataCachePtr]);
							core->dataCachePtr++;
							break;

						case DduThreadState::ReadBogusData:
							core->dduToHostCallback(0);
							break;

						case DduThreadState::GetStreamEnable:
						case DduThreadState::GetStreamOffset:
						case DduThreadState::GetStreamBogus:
							if (core->immediateBufferPtr < sizeof(core->immediateBuffer))
							{
								core->dduToHostCallback(core->immediateBuffer[core->immediateBufferPtr]);
								core->immediateBufferPtr++;
							}
							else
							{
								core->DeviceError();
							}
							break;

						case DduThreadState::Idle:
							break;

						default:
							core->DeviceError();
							break;
					}
				}
			}

			// Sleep until next transfer
			core->dduThread->Suspend();
		}
	}

	void DduCore::DvdAudioThreadProc(void* Parameter)
	{
		Sleep(10);

		// Sleep until streaming is not started again
	}

	void DduCore::Reset()
	{
		dduThread->Suspend();
		ddBusBusy = false;
		errorState = false;
		commandPtr = 0;
		dataCachePtr = dataCacheSize;
		state = DduThreadState::WriteCommand;
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

	void DduCore::DeviceError()
	{
		DBReport2(DbgChannel::DVD, "DDU DeviceError\n");
		// Will deassert DIERR after the next command is received from the host
		errorState = true;
		ddBusBusy = false;
		if (errorCallback)
		{
			errorCallback();
		}
		dduThread->Suspend();
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
