#include "pch.h"

namespace DVD
{
	DduCore DDU;		// Singletone

	DduCore::DduCore()
	{
		dduThread = new Thread(DduThreadProc, true, this);
		dvdAudioThread = new Thread(DvdAudioThreadProc, true, this);

		Reset();
	}

	DduCore::~DduCore()
	{
		delete dduThread;
		delete dvdAudioThread;
	}

	void DduCore::DduThreadProc(void* Parameter)
	{
		DduCore* core = (DduCore*)Parameter;

		// Until break or transfer completed
		while (core->ddBusBusy)
		{
			if (core->busDir == DduBusDirection::HostToDdu)
			{

			}
			else
			{

			}
		}

		// Sleep until next transfer
		core->dduThread->Suspend();
	}

	void DduCore::DvdAudioThreadProc(void* Parameter)
	{

		// Sleep until streaming is not started again
	}

	void DduCore::Reset()
	{
		dduThread->Suspend();
		ddBusBusy = false;
		errorState = false;
	}

	void DduCore::Break()
	{
		// Abort data transfer
		DBReport2(DbgChannel::DVD, "Break");
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
		DBReport2(DbgChannel::DVD, "DeviceError\n");
		// Will deassert DIERRb after the next command is received from the host
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
		DBReport2(DbgChannel::DVD, "StartTransfer: %s\n", direction == DduBusDirection::DduToHost ? "Ddu->Host" : "Host->Ddu");

		errorState = false;
		ddBusBusy = true;

		dduThread->Resume();
	}

	void DduCore::TransferComplete()
	{
		DBReport2(DbgChannel::DVD, "TransferComplete");
		ddBusBusy = false;
	}

// execute DVD command
#if 0
static void DICommand()
{
    uint32_t dimar = DIMAR & RAMMASK;

    if (di.log)
    {
        //DBReport2(DbgChannel::DI, "%08X %08X %08X\n", di.cmdbuf[0], di.cmdbuf[1], di.cmdbuf[2]);
    }

    switch(di.cmdbuf[0] >> 24)
    {
        case 0x12:          // read manufacture info (DMA)
            memset(&mi.ram[dimar], 0, 0x20);
            DILEN = 0;
            break;

        case 0xA8:          // read sector (disk id) (DMA)
            // Non-DMA disk transfer
            assert(DICR & DI_CR_DMA);
            // Not aligned disk DMA transfer. Should be on 32-byte boundary.
            assert((DILEN & 0x1f) == 0);

            BeginProfileDVD();
            DVD::Seek(di.cmdbuf[1] << 2);
            DVD::Read(&mi.ram[dimar], DILEN);
            EndProfileDVD();

            if (di.log)
            {
                DBReport2(DbgChannel::DI, "dma transfer (dimar:%08X, ofs:%08X, len:%i b)\n",
                    DIMAR, di.cmdbuf[1] << 2, DILEN);
            }
            DILEN = 0;
            break;

        case 0xAB:          // seek
            break;

        case 0xE3:          // stop motor (IMM)
            if (di.log)
            {
                DBReport2(DbgChannel::DI, "Stop Motor\n");
            }
            break;

        case 0xE1:          // set stream (IMM).
            if(di.cmdbuf[1] << 2)
            {
                di.strseek  = di.cmdbuf[1] << 2;
                di.strcount = di.cmdbuf[2];

                if (di.log)
                {
                    DBReport2(DbgChannel::DI, "DVD Streaming setup. DVD positioned on track, starting %08X, %i bytes long.\n",
                        di.strseek, di.strcount);
                }
            }
            break;

        case 0xE2:          // get stream status (IMM)
            switch ((di.cmdbuf[0] >> 16) & 0xFF)
            {
                case 0:             // Get stream enable
                    di.immbuf = di.streaming & 1;
                    break;
                case 1:             // Get stream address
                    di.immbuf = di.strseek >> 2;
                    break;
                default:
                    DBReport2(DbgChannel::DI, "Unknown GetStreamStatus: %i\n", (di.cmdbuf[0] >> 16) & 0xFF);
                    break;
            }
            break;

        case 0xE4:          // stream control (IMM)
            di.streaming = (di.cmdbuf[0] >> 16) & 1;
            if (di.log)
            {
                DBReport2(DbgChannel::DI, "DVD Streaming. Play(?) : %s\n", di.streaming ? "On" : "Off");
            }
            break;

        // unknown command
        default:
            UI::DolwinError(
                _T("DVD Subsystem Failure"),
                _T("Unknown DVD command : %08X\n\n")
                _T("type:%s\n")
                _T("dma:%s\n")
                _T("madr:%08X\n")
                _T("dlen:%08X\n")
                _T("imm:%08X\n"),
                di.cmdbuf[0],
                (DICR & DI_CR_RW) ? (_T("write")) : (_T("read")),
                (DICR & DI_CR_DMA) ? (_T("yes")) : (_T("no")),
                DIMAR,
                DILEN,
                di.immbuf
            );
            return;
    }

    DICR &= ~DI_CR_TSTART;
    DIINT();
}
#endif 

}
