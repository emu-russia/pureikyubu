/*

Null host input (the headless build).

Assume the GameCube was launched with no controllers attached and no keyboard: every device of the
peripheral pool is emulated as it is, but nothing drives its actuators. This is the backend of
`pureikyubu_headless.vcxproj` and of the CMake `HEADLESS` build, and it is what the unit tests link
(the tests drive the devices through the subsystem instead).

*/

#include "pch.h"

class NullHostInput : public HostInput
{
};

static NullHostInput* null_host_input = nullptr;

HostInput* HostInputCreate()
{
	null_host_input = new NullHostInput();
	return null_host_input;
}

void HostInputDestroy()
{
	delete null_host_input;
	null_host_input = nullptr;
}

void HostInputUpdate()
{
}
