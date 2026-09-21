// hardware init/update code
// IMPORTANT: whole HW should use physical CPU addressing, not effective!
#include "pch.h"

using namespace Debug;

namespace Flipper
{
	Flipper* HW;
	DSP::Dsp16* DSP;      // Instance of dsp core

	Flipper::Flipper(HWConfig* config)
	{
		Report(Channel::Info,
			"-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n"
			"Hardware Initialization.\n"
			"-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n"
		);

		memsize = config->ramsize;

		Mixer = new AudioMixer(config);

		pi = new ProcessorInterface(this, config);
		mem = new MemoryInterface(this, config);
		vi = new VideoInterface(this, config);
		cp = new CommandProcessor(this, config);
		ai = new AudioInterface(this, config);
		DSP::DspAIOpen(this, config);			// TODO: find better place
		DSP::AROpen(this);       // aux. memory (ARAM)  TODO: find better place
		exi = new ExternalInterface(this, config);
		di = new DiskInterface(this, config);
		si = new SerialInterface(this, config);

		DSP->core->HardReset();

		// Load IROM.

		auto iromImage = Util::FileLoad(config->DspIromFilename);

		if (DSP->core->LoadIrom(iromImage))
		{
			Report(Channel::DSP, "Loaded DSP IROM: %s\n", Util::WstringToString(config->DspIromFilename).c_str());
		}
		else
		{
			Report(Channel::Norm, "Failed to load DSP IROM: %s\n", Util::WstringToString(config->DspIromFilename).c_str());
		}

		// Load DROM.

		auto dromImage = Util::FileLoad(config->DspDromFilename);

		if (DSP->core->LoadDrom(dromImage))
		{
			Report(Channel::DSP, "Loaded DSP DROM: %s\n", Util::WstringToString(config->DspDromFilename).c_str());
		}
		else
		{
			Report(Channel::Norm, "Failed to load DSP DROM\n", Util::WstringToString(config->DspDromFilename).c_str());
		}

		Report(Channel::Norm, "\n");

		gfx = new GFX::GFXCore(this, config);
		PADOpen();

		// open memory cards
		MCOpen(config);

		JDI::Hub.AddNode(L"HW_JDI_JSON", JdiSpecs::HwJdi, hw_init_handlers);
	}

	Flipper::~Flipper()
	{
		JDI::Hub.RemoveNode(L"HW_JDI_JSON");

		DSP->Suspend();

		if (cp) {
			delete cp;
			cp = nullptr;
		}
		if (ai) {
			delete ai;
			ai = nullptr;
		}
		DSP::DspAIClose();	// TODO: find better place
		DSP::ARClose();      // release ARAM  TODO: find better place
		if (si) {
			delete si;
			si = nullptr;
		}
		if (exi) {
			delete exi;
			exi = nullptr;
		}
		if (vi) {
			delete vi;
			vi = nullptr;
		}
		if (di) {
			delete di;
			di = nullptr;
		}
		if (mem) {
			delete mem;
			mem = nullptr;
		}
		if (pi) {
			delete pi;
			pi = nullptr;
		}

		if (Mixer) {
			delete Mixer;
			Mixer = nullptr;
		}
		PADClose();
		if (gfx) {
			delete gfx;
			gfx = nullptr;
		}

		// close memory cards
		MCClose();
	}

	void Flipper::Update(int64_t ticks)
	{
		// A pending deadline that is still ahead of us means there is nothing to do yet. When the
		// time base has jumped *backwards* (the CPU was reset) the anchor is stale and has to be
		// taken again, otherwise the periodic work would stop until the time base caught up.
		if (ticks < hwUpdateTbrValue && (hwUpdateTbrValue - ticks) < FlipperTickStep)
		{
			return;
		}
		hwUpdateTbrValue = ticks + FlipperTickStep;

		// update joypads and video
		vi->VIUpdate();
		si->SIPoll(vi->GetCurrentLine());

		// ... and step the devices that have to run in lock step with the CPU. They used to run on
		// their own threads, woken from here: the work they owe is a function of the time base, so
		// stepping them here instead does not change what they do - but it does make *when* they do
		// it, and therefore when their interrupts reach the CPU, a function of the time base too.
		// A device thread delivers its interrupt whenever the host happens to schedule it, and a
		// guest that only samples its interrupt line at instruction boundaries - the movie player
		// decodes in a tight loop - then sees the same frame come out differently from run to run.
		cp->DrainFifo();
		DSP::AITickSync(ticks);

		// The DSP core is stepped here too: one instruction per `GekkoTicksPerDspInstruction`
		// ticks, which is what its 81 MHz clock works out to.
		if (DSP != nullptr)
		{
			DSP->core->Update();
		}

		// ... and the DVD-audio sample clock, whose streaming shares the drive read pointer with
		// the DI transfer.
		if (DVD::DDU != nullptr)
		{
			DVD::DDU->AudioTick(ticks);
		}
	}

	uint32_t Flipper::GetMemorySize()
	{
		return (uint32_t)memsize;
	}
}