// The module is needed for minimal support for Flipper.
// In fact, only a rudimentary MI is needed and possibly interrupts.

#include "pch.h"

namespace Flipper
{
	class BogusFlipper
	{
	public:
		BogusFlipper();
		~BogusFlipper();
	};

	BogusFlipper::BogusFlipper()
	{
		HWConfig* config = new HWConfig;

		config->ramsize = RAMSIZE;

		MIOpen(config);

		delete config;
	}

	BogusFlipper::~BogusFlipper()
	{
		MIClose();
	}

	BogusFlipper BogusHW;
}
