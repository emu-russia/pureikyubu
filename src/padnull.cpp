/*

Null host input (the headless build).

Assume the GameCube was launched with no controllers attached and no keyboard: every device of the
peripheral pool is emulated as it is, but nothing drives its actuators. This is the backend of
`pureikyubu_headless.vcxproj` and of the CMake `HEADLESS` build, and it is what the unit tests link
(the tests drive the devices through the subsystem instead).

*/

#include "pch.h"

/*
 * The headless build has no host devices at all, which also means there is nothing to press: a
 * device of the pool is emulated exactly as it is, but nothing drives its actuators. `KeyDown`
 * answers from a set the debug interface can fill (`hkey`), so an unattended run - a test script,
 * an agent driving the emulator over the MCP server - can press the controls the way a player
 * would, without a window or a real keyboard.
 */
class NullHostInput : public HostInput
{
public:
	static const int MaxKey = 512;

	bool KeyDown(int scancode) override
	{
		return scancode >= 0 && scancode < MaxKey && injected[scancode];
	}

	bool injected[MaxKey] = {};
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

void HostInputSetKey(int code, bool down)
{
	if (null_host_input == nullptr)
	{
		return;
	}

	if (code < 0 || code >= NullHostInput::MaxKey)
	{
		return;
	}

	null_host_input->injected[code] = down;
}
