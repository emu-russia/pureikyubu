/*

# Peripheral devices

This is the implementation of the subsystem `peripherals.h` declares. Two things live here: the
pool of devices (what is in it, what is plugged where, what the settings dialog edits) and the
implementation of the standard controller, which is the device the emulator is built around.

## The pool and the configuration

The pool is the list of the devices the user has. Every device owns the variables `Device<i>_*` of
the "peripherals" section of the configuration: its model (`Device0_Type`), the name the user gave
it (`Device0_Name`), the port it is plugged into (`Device0_Port`) and whatever its own settings are
(a pad keeps its bindings there, a memory card keeps its file there).

A configuration that was written before the pool existed has no `Device<i>_*` variables at all.
Such a configuration is not migrated: it is *read* through the old names, so that the emulator
comes up with the same controllers and the same cards as the build that wrote it. The four sockets
are then `PluggedIn_<n>` from the "controllers" section, the bindings are `VKEY_FOR_*_<n>`, and the
two card slots are `MemcardA_*` / `MemcardB_*` from "memcards". The first time the settings dialog
writes a device's own setting, the new name is what is stored, and the old one is left alone.

## The pool is stable while the emulator runs

The emulation thread takes a device out of the pool (to poll a pad, to run a card transfer) while
the settings dialog may be reconfiguring it. A device is therefore never destroyed while the
emulator is running: removing one marks its slot unused, and the next device that is added reuses
the slot. The pool is dropped in one piece by `Close`, which the emulator calls when it takes the
machine apart, with no emulation running.

*/

#include "pch.h"

using namespace Debug;

// ---------------------------------------------------------------------------
// The device configuration

std::string PeriphConfigKey(int index, const char* name)
{
	char key[0x80];
	sprintf(key, "Device%i_%s", index, name);
	return key;
}

int PeriphConfigInt(int index, const char* name, int def)
{
	std::string key = PeriphConfigKey(index, name);

	if (!ConfigValueExists(key.c_str(), USER_PERIPH))
	{
		return def;
	}

	return GetConfigInt(key.c_str(), USER_PERIPH);
}

void PeriphConfigSetInt(int index, const char* name, int value)
{
	SetConfigInt(PeriphConfigKey(index, name).c_str(), value, USER_PERIPH);
}

std::string PeriphConfigString(int index, const char* name, const std::string& def)
{
	std::string key = PeriphConfigKey(index, name);

	if (!ConfigValueExists(key.c_str(), USER_PERIPH))
	{
		return def;
	}

	return Util::WstringToString(GetConfigString(key.c_str(), USER_PERIPH));
}

void PeriphConfigSetString(int index, const char* name, const std::string& value)
{
	SetConfigString(PeriphConfigKey(index, name).c_str(), Util::StringToWstring(value).c_str(), USER_PERIPH);
}

// ---------------------------------------------------------------------------
// The device models
//
// A model is what a DeviceID stands for: its name, the bus its devices are plugged into and the
// factory that creates one. The models of the devices that live outside this module (the memory
// card is in memcard.cpp, with the flash protocol it already owned) register themselves here, so
// this module does not have to know about them.

namespace
{
	struct PeriphModel
	{
		uint32_t    type;
		const char* name;
		const char* info;
		int         bus;
		PeripheralDevice* (*factory)(int index);
	};

	std::vector<PeriphModel>& Models()
	{
		// A function local static: a device module that registers its factory from a static
		// constructor of its own cannot construct this list, so the list is built on first use.
		static std::vector<PeriphModel> models;
		return models;
	}

	const PeriphModel* FindModel(uint32_t type)
	{
		for (const auto& model : Models())
		{
			if (model.type == type)
			{
				return &model;
			}
		}

		return nullptr;
	}

	//! The bus a model is plugged into (the model of a device always exists: the device was created
	//! from it).
	int ModelBus(uint32_t type)
	{
		const PeriphModel* model = FindModel(type);
		return model != nullptr ? model->bus : -1;
	}

	//! The bus a model is plugged into (the model of a device always exists: the device was created
	//! from it).
	int ModelBusOf(uint32_t type)
	{
		const PeriphModel* model = FindModel(type);
		return model != nullptr ? model->bus : -1;
	}

	const char* BusName(int bus)
	{
		return bus == PERIPH_BUS_SI ? "Serial Interface" : "EXI";
	}
}

// ---------------------------------------------------------------------------
// The ports
//
// The console has four controller sockets and two memory card slots. Every port belongs to a bus,
// and a device can only be plugged into a port of its own bus.

namespace
{
	struct PeriphPortDesc
	{
		const char* name;
		int         bus;
		uint32_t    defaultModel;       //!< the model the port is meant for
	};

	const PeriphPortDesc port_desc[PERIPH_PORT_MAX] =
	{
		{ "Controller Port 1",  PERIPH_BUS_SI,  PERIPH_DEVICE_STANDARD_PAD },
		{ "Controller Port 2",  PERIPH_BUS_SI,  PERIPH_DEVICE_STANDARD_PAD },
		{ "Controller Port 3",  PERIPH_BUS_SI,  PERIPH_DEVICE_STANDARD_PAD },
		{ "Controller Port 4",  PERIPH_BUS_SI,  PERIPH_DEVICE_STANDARD_PAD },
		{ "Memory Card Slot A", PERIPH_BUS_EXI, PERIPH_DEVICE_MEMCARD },
		{ "Memory Card Slot B", PERIPH_BUS_EXI, PERIPH_DEVICE_MEMCARD },
	};

	//! The host game controller a pad is driven by: the game controller the host reports as the Nth
	//! drives the pad in Port N, which is what the backend has always done.
	int HostPadOf(int port)
	{
		if (port >= PERIPH_PORT_SI0 && port <= PERIPH_PORT_SI3)
		{
			return port - PERIPH_PORT_SI0;
		}

		return 0;
	}

	//! The port of a device of the default pool in a configuration that predates the pool. The old
	//! format kept the state of the four sockets in "controllers" and the two card slots in
	//! "memcards" (see the comment at the top of the file).
	int LegacyPort(int index)
	{
		if (index >= PERIPH_PORT_SI0 && index <= PERIPH_PORT_SI3)
		{
			char key[0x40];
			sprintf(key, "PluggedIn_%i", index);
			return GetConfigBool(key, USER_PADS) ? index : -1;
		}

		if (index == PERIPH_DEFAULT_SLOTA)
		{
			return GetConfigBool(MemcardA_Connected_Key, USER_MEMCARDS) ? PERIPH_PORT_SLOTA : -1;
		}

		if (index == PERIPH_DEFAULT_SLOTB)
		{
			return GetConfigBool(MemcardB_Connected_Key, USER_MEMCARDS) ? PERIPH_PORT_SLOTB : -1;
		}

		return -1;
	}

	//! The value of one host control bound to an actuator. A button (the keyboard is one too) gives
	//! the actuator its full value, an axis gives its deflection scaled into the actuator's range,
	//! in the direction the binding was captured with.
	int BindingValue(HostInput* host, int pad, const PeriphActuator* actuator, int binding)
	{
		if (binding == 0)
		{
			return 0;
		}

		if (PERIPH_HOST_IS_BUTTON(binding))
		{
			return host->GamepadButton(pad, PERIPH_HOST_BUTTON(binding)) ? actuator->max : 0;
		}

		if (PERIPH_HOST_IS_AXIS(binding))
		{
			int axis = host->GamepadAxis(pad, PERIPH_HOST_AXIS(binding));
			bool positive = PERIPH_HOST_AXIS_POS(binding);

			if ((positive && axis <= 0) || (!positive && axis >= 0))
			{
				return 0;
			}

			int magnitude = positive ? axis : -axis;
			int value = (int)((int64_t)magnitude * actuator->max / 32767);

			return value > actuator->max ? actuator->max : value;
		}

		return 0;
	}
}

// ---------------------------------------------------------------------------
// The peripheral subsystem

Peripherals& Peripherals::Instance()
{
	static Peripherals instance;
	return instance;
}

void Peripherals::RegisterFactory(uint32_t type, const char* name, const char* info, int bus,
	PeripheralDevice* (*factory)(int index))
{
	for (auto& model : Models())
	{
		if (model.type == type)
		{
			model.name = name;
			model.info = info;
			model.bus = bus;
			model.factory = factory;
			return;
		}
	}

	Models().push_back(PeriphModel{ type, name, info, bus, factory });
}

const char* Peripherals::ModelName(uint32_t type)
{
	const PeriphModel* model = FindModel(type);
	return model != nullptr ? model->name : nullptr;
}

int Peripherals::ModelBus(uint32_t type)
{
	return ModelBusOf(type);
}

uint32_t Peripherals::DefaultModelOfPort(int port)
{
	if (port < 0 || port >= PERIPH_PORT_MAX)
	{
		return PERIPH_DEVICE_NONE;
	}

	return port_desc[port].defaultModel;
}

const char* Peripherals::PortName(int port)
{
	if (port < 0 || port >= PERIPH_PORT_MAX)
	{
		return "?";
	}

	return port_desc[port].name;
}

int Peripherals::PortBus(int port)
{
	if (port < 0 || port >= PERIPH_PORT_MAX)
	{
		return -1;
	}

	return port_desc[port].bus;
}

HostInput* Peripherals::Host()
{
	return host;
}

PeripheralDevice* Peripherals::CreateDevice(uint32_t type, int index)
{
	const PeriphModel* model = FindModel(type);

	if (model == nullptr || model->factory == nullptr)
	{
		Report(Channel::SI, "Peripheral device %08X is not supported by this build\n", type);
		return nullptr;
	}

	PeripheralDevice* device = model->factory(index);

	if (device == nullptr)
	{
		return nullptr;
	}

	// The pool slot is filled before the device reads its own settings: a device that has never been
	// configured asks the pool for the default bindings of its model (see StandardPad::LoadConfig).
	lock.Lock();
	devices[index] = device;
	lock.Unlock();

	device->LoadConfig(index);

	return device;
}

// ---------------------------------------------------------------------------
// The pool

void Peripherals::Open()
{
	Report(Channel::SI, "Peripheral devices\n");

	host = HostInputCreate();
	opened = true;

	for (int i = 0; i < PERIPH_MAX_DEVICES; i++)
	{
		ports[i] = -1;

		// What the pool of this console holds. A configuration that has never seen the pool has no
		// "Device<i>_Type" at all, and then the slot holds the device its port is meant for: the
		// first six slots are the four sockets and the two card slots.
		uint32_t type = (uint32_t)PeriphConfigInt(i, "Type",
			i < PERIPH_PORT_MAX ? (int)DefaultModelOfPort(i) : PERIPH_DEVICE_NONE);

		if (type == PERIPH_DEVICE_NONE)
		{
			continue;
		}

		devices[i] = CreateDevice(type, i);

		if (devices[i] == nullptr)
		{
			continue;
		}

		names[i] = PeriphConfigString(i, "Name", ModelName(type));
		// The port is read after the device exists, so that a configuration that predates the pool
		// is asked for the old name only when the new one is not there (see LegacyPort). The
		// configuration is not written back here: building the pool from it must not change it.
		std::string portKey = PeriphConfigKey(i, "Port");
		int port = ConfigValueExists(portKey.c_str(), USER_PERIPH)
			? GetConfigInt(portKey.c_str(), USER_PERIPH)
			: LegacyPort(i);

		if (port >= 0 && port < PERIPH_PORT_MAX)
		{
			Plug(i, port);
		}
	}
}

void Peripherals::Close()
{
	if (!opened)
	{
		return;
	}

	for (int i = 0; i < PERIPH_MAX_DEVICES; i++)
	{
		if (devices[i] != nullptr)
		{
			if (ports[i] >= 0)
			{
				devices[i]->Detach();
			}

			delete devices[i];
			devices[i] = nullptr;
		}

		ports[i] = -1;
		names[i].clear();
	}

	for (auto device : retired)
	{
		delete device;
	}

	retired.clear();

	HostInputDestroy();
	host = nullptr;
	opened = false;
}

PeripheralDevice* Peripherals::Device(int index)
{
	if (index < 0 || index >= PERIPH_MAX_DEVICES)
	{
		return nullptr;
	}

	lock.Lock();
	PeripheralDevice* device = devices[index];
	lock.Unlock();

	return device;
}

int Peripherals::AddDevice(uint32_t type)
{
	lock.Lock();

	int index = -1;

	for (int i = 0; i < PERIPH_MAX_DEVICES; i++)
	{
		if (devices[i] == nullptr)
		{
			index = i;
			break;
		}
	}

	lock.Unlock();

	if (index < 0)
	{
		Report(Channel::SI, "The peripheral pool is full\n");
		return -1;
	}

	PeripheralDevice* device = CreateDevice(type, index);

	if (device == nullptr)
	{
		return -1;
	}

	lock.Lock();
	ports[index] = -1;
	names[index] = ModelName(type);
	lock.Unlock();

	PeriphConfigSetInt(index, "Type", (int)type);

	return index;
}

void Peripherals::RemoveDevice(int index)
{
	if (index < 0 || index >= PERIPH_MAX_DEVICES)
	{
		return;
	}

	Detach(index);

	lock.Lock();
	PeripheralDevice* device = devices[index];
	devices[index] = nullptr;
	names[index].clear();
	lock.Unlock();

	// The device object outlives its pool entry for as long as the emulator runs: a thread may have
	// just taken it out of the pool. It is destroyed with the pool, in Close.
	if (device != nullptr)
	{
		retired.push_back(device);
	}

	PeriphConfigSetInt(index, "Type", PERIPH_DEVICE_NONE);
}

std::string Peripherals::Name(int index)
{
	if (index < 0 || index >= PERIPH_MAX_DEVICES)
	{
		return "";
	}

	lock.Lock();
	std::string name = names[index];
	lock.Unlock();

	return name;
}

void Peripherals::SetName(int index, const std::string& name)
{
	if (index < 0 || index >= PERIPH_MAX_DEVICES)
	{
		return;
	}

	lock.Lock();
	names[index] = name;
	lock.Unlock();

	PeriphConfigSetString(index, "Name", name);
}

int Peripherals::PortOf(int index)
{
	if (index < 0 || index >= PERIPH_MAX_DEVICES)
	{
		return -1;
	}

	lock.Lock();
	int port = ports[index];
	lock.Unlock();

	return port;
}

PeripheralDevice* Peripherals::DeviceOnPort(int port)
{
	return Device(DeviceIndexOnPort(port));
}

int Peripherals::DeviceIndexOnPort(int port)
{
	if (port < 0 || port >= PERIPH_PORT_MAX)
	{
		return -1;
	}

	lock.Lock();

	int found = -1;

	for (int i = 0; i < PERIPH_MAX_DEVICES; i++)
	{
		if (devices[i] != nullptr && ports[i] == port)
		{
			found = i;
			break;
		}
	}

	lock.Unlock();

	return found;
}

void Peripherals::Plug(int index, int port)
{
	lock.Lock();
	ports[index] = port;
	lock.Unlock();

	devices[index]->Attach(port);
}

void Peripherals::WriteLegacyPort(int index, int port)
{
	char key[0x40];

	// The four sockets used to be a fixed pad per socket, so an older build can only be told that a
	// pad is in the socket that matches its number.
	if (index >= PERIPH_PORT_SI0 && index <= PERIPH_PORT_SI3)
	{
		sprintf(key, "PluggedIn_%i", index);
		SetConfigBool(key, port == index, USER_PADS);
		return;
	}

	if (index == PERIPH_DEFAULT_SLOTA)
	{
		SetConfigBool(MemcardA_Connected_Key, port == PERIPH_PORT_SLOTA, USER_MEMCARDS);
		return;
	}

	if (index == PERIPH_DEFAULT_SLOTB)
	{
		SetConfigBool(MemcardB_Connected_Key, port == PERIPH_PORT_SLOTB, USER_MEMCARDS);
		return;
	}
}

bool Peripherals::Attach(int index, int port)
{
	if (index < 0 || index >= PERIPH_MAX_DEVICES || port < 0 || port >= PERIPH_PORT_MAX)
	{
		return false;
	}

	PeripheralDevice* device = Device(index);

	if (device == nullptr)
	{
		return false;
	}

	// A device belongs to a bus: a pad cannot be plugged into a card slot.
	PeripheralDevice* existing = DeviceOnPort(port);

	if (existing != nullptr && existing != device)
	{
		Report(Channel::SI, "%s already holds a device\n", PortName(port));
		return false;
	}

	if (ModelBusOf(device->Type()) != PortBus(port))
	{
		Report(Channel::SI, "%s is not a %s port\n", PortName(port), BusName(PortBus(port)));
		return false;
	}

	Detach(index);
	Plug(index, port);

	PeriphConfigSetInt(index, "Port", port);
	WriteLegacyPort(index, port);

	return true;
}

void Peripherals::Detach(int index)
{
	if (index < 0 || index >= PERIPH_MAX_DEVICES)
	{
		return;
	}

	PeripheralDevice* device = Device(index);

	lock.Lock();
	int port = ports[index];
	ports[index] = -1;
	lock.Unlock();

	if (port < 0)
	{
		return;
	}

	PeriphConfigSetInt(index, "Port", -1);
	WriteLegacyPort(index, -1);

	if (device != nullptr)
	{
		device->Detach();
	}
}

void Peripherals::DefaultBindings(int index, bool keyboard)
{
	PeripheralDevice* device = Device(index);

	if (device == nullptr || host == nullptr)
	{
		return;
	}

	for (int i = 0; i < device->ActuatorCount(); i++)
	{
		const PeriphActuator* actuator = device->Actuator(i);
		PeriphBindings* bindings = device->ActuatorBindings(i);

		if (actuator == nullptr || bindings == nullptr)
		{
			continue;
		}

		int key = 0, gamepad = 0;
		host->DefaultBindings(device->Type(), actuator->id, key, gamepad);

		if (keyboard && key != 0)
		{
			bindings->keyboard = key;
		}

		bindings->gamepad = gamepad;
	}
}

void Peripherals::ClearBindings(int index)
{
	PeripheralDevice* device = Device(index);

	if (device == nullptr)
	{
		return;
	}

	for (int i = 0; i < device->ActuatorCount(); i++)
	{
		PeriphBindings* bindings = device->ActuatorBindings(i);

		if (bindings != nullptr)
		{
			bindings->keyboard = 0;
			bindings->gamepad = 0;
		}
	}
}

bool Peripherals::FirstOfModel(int index)
{
	PeripheralDevice* device = Device(index);

	if (device == nullptr)
	{
		return false;
	}

	for (int i = 0; i < index; i++)
	{
		PeripheralDevice* other = Device(i);

		if (other != nullptr && other->Type() == device->Type())
		{
			return false;
		}
	}

	return true;
}

// ---------------------------------------------------------------------------
// The emulation side

bool Peripherals::PollSI(int chan, PADState* state)
{
	int port = PERIPH_PORT_SI(chan);
	int index = DeviceIndexOnPort(port);
	PeripheralDevice* device = Device(index);

	if (device == nullptr)
	{
		return false;
	}

	// The bindings are resolved here, on the emulation thread: the device is handed the value of
	// every actuator and turns them into its protocol by itself.
	int pad = HostPadOf(port);

	for (int i = 0; i < device->ActuatorCount(); i++)
	{
		const PeriphActuator* actuator = device->Actuator(i);
		PeriphBindings* bindings = device->ActuatorBindings(i);

		if (actuator == nullptr || bindings == nullptr)
		{
			continue;
		}

		int value = 0;

		if (bindings->keyboard != 0 && host != nullptr && host->KeyDown(bindings->keyboard))
		{
			value = actuator->max;
		}

		if (host != nullptr)
		{
			int fromGamepad = BindingValue(host, pad, actuator, bindings->gamepad);
			value = my_max(value, fromGamepad);
		}

		device->SetState(i, value);
	}

	return device->Poll(state);
}

void Peripherals::TransferSI(int chan, int outlen, int inlen, uint8_t* buf)
{
	PeripheralDevice* device = DeviceOnPort(PERIPH_PORT_SI(chan));

	// Nothing is plugged into the channel: the transfer is not answered at all, which is how the
	// guest finds out that there is no controller (SISR[NOREP]).
	if (device == nullptr)
	{
		return;
	}

	device->Transfer(outlen, inlen, buf);
}

void Peripherals::TransferEXI(int chan, int sel, Flipper::ExternalInterface* exi)
{
	// The devices of a channel are told apart by the chip select the transfer selects (see
	// exi.cpp): a memory card is on CS0B of the channel of its slot.
	if (sel != 0 || chan > 1)
	{
		return;
	}

	PeripheralDevice* device = DeviceOnPort(PERIPH_PORT_SLOT(chan));

	if (device == nullptr)
	{
		return;
	}

	device->ExiTransfer(exi);
}

bool Peripherals::SetMotorSI(int chan, int cmd)
{
	PeripheralDevice* device = DeviceOnPort(PERIPH_PORT_SI(chan));

	if (device == nullptr)
	{
		return false;
	}

	return device->SetMotor(cmd);
}

// ---------------------------------------------------------------------------
// The standard controller (DOL-003)
//
// The pad is a Joybus device: the console polls it, and the pad answers with its buttons and its six
// analog channels. What the guest library sees is the emulator's PADState, and the SI register file
// packs it into the input buffer registers the way the hardware does (see si.cpp).
//
// The actuators are the controls the host drives. A keyboard key can only say "pressed", so the two
// sticks publish the half and the full deflection of every direction as separate controls: that is
// what gives a key-driven stick its magnitude. A game controller drives the same controls with its
// buttons and its axes, and its axis deflection is scaled into the range of the control.

namespace
{
	enum
	{
		ACT_UP = 0,
		ACT_DOWN,
		ACT_LEFT,
		ACT_RIGHT,
		ACT_XUP50,
		ACT_XUP100,
		ACT_XDOWN50,
		ACT_XDOWN100,
		ACT_XLEFT50,
		ACT_XLEFT100,
		ACT_XRIGHT50,
		ACT_XRIGHT100,
		ACT_CXUP,
		ACT_CXDOWN,
		ACT_CXLEFT,
		ACT_CXRIGHT,
		ACT_TRIGGERL,
		ACT_TRIGGERR,
		ACT_TRIGGERZ,
		ACT_A,
		ACT_B,
		ACT_X,
		ACT_Y,
		ACT_START,

		ACT_MAX
	};

	//! The analog controls swing the full scale of a keyboard-driven direction and half of it for
	//! the "50%" controls; the two triggers have the range of the analog channel of the pad.
	constexpr int ACT_FULL = 127;
	constexpr int ACT_HALF = 63;
	constexpr int ACT_TRIG = 255;

	const PeriphActuator pad_actuators[ACT_MAX] =
	{
		{ "Buttons",       "Up",          "UP",        1 },
		{ "Buttons",       "Down",        "DOWN",      1 },
		{ "Buttons",       "Left",        "LEFT",      1 },
		{ "Buttons",       "Right",       "RIGHT",     1 },
		{ "Control Stick", "Up 50%",      "XUP50",     ACT_HALF },
		{ "Control Stick", "Up 100%",     "XUP100",    ACT_FULL },
		{ "Control Stick", "Down 50%",    "XDOWN50",   ACT_HALF },
		{ "Control Stick", "Down 100%",   "XDOWN100",  ACT_FULL },
		{ "Control Stick", "Left 50%",    "XLEFT50",   ACT_HALF },
		{ "Control Stick", "Left 100%",   "XLEFT100",  ACT_FULL },
		{ "Control Stick", "Right 50%",   "XRIGHT50",  ACT_HALF },
		{ "Control Stick", "Right 100%",  "XRIGHT100", ACT_FULL },
		{ "C Stick",       "C Up",        "CXUP",      ACT_FULL },
		{ "C Stick",       "C Down",      "CXDOWN",    ACT_FULL },
		{ "C Stick",       "C Left",      "CXLEFT",    ACT_FULL },
		{ "C Stick",       "C Right",     "CXRIGHT",   ACT_FULL },
		{ "Triggers",      "L",           "TRIGGERL",  ACT_TRIG },
		{ "Triggers",      "R",           "TRIGGERR",  ACT_TRIG },
		{ "Triggers",      "Z",           "TRIGGERZ",  1 },
		{ "Buttons",       "A",           "A",         1 },
		{ "Buttons",       "B",           "B",         1 },
		{ "Buttons",       "X",           "X",         1 },
		{ "Buttons",       "Y",           "Y",         1 },
		{ "Buttons",       "Start",       "START",     1 },
	};

	//! The configuration variable of a binding of a pad. A configuration that predates the pool
	//! keeps the bindings of a socket under the old name (`VKEY_FOR_A_0` for the first pad).
	int LoadBinding(int index, const char* kind, const char* suffix, bool& found)
	{
		char key[0x80];

		sprintf(key, "Device%i_%s_FOR_%s", index, kind, suffix);

		if (ConfigValueExists(key, USER_PERIPH))
		{
			found = true;
			return GetConfigInt(key, USER_PERIPH);
		}

		sprintf(key, "%s_FOR_%s_%i", kind, suffix, index);

		if (ConfigValueExists(key, USER_PADS))
		{
			found = true;
			return GetConfigInt(key, USER_PADS);
		}

		found = false;
		return 0;
	}

	void SaveBinding(int index, const char* kind, const char* suffix, int value)
	{
		char key[0x80];
		sprintf(key, "Device%i_%s_FOR_%s", index, kind, suffix);
		SetConfigInt(key, value, USER_PERIPH);
	}
}

//! The standard controller, DOL-003.
class StandardPad : public PeripheralDevice
{
	int             index = -1;                     //!< the pool index (names the configuration)
	int             port = -1;
	int             state[ACT_MAX] = { 0 };         //!< the value of every actuator
	PeriphBindings  bindings[ACT_MAX];
	int             motor = PAD_MOTOR_STOP;

	//! The value of a control, as the host drives it.
	int Act(int actuator) const { return state[actuator]; }

	//! The pressure of a direction of a stick: the half and the full control of the same direction
	//! may both be driven at once, and the larger of the two is what the stick sees.
	int Dir(int half, int full) const { return my_max(state[half], state[full]); }

	//! One axis of a stick from the two directions that drive it. The actuator keeps the deviation
	//! from the middle of the channel, which is what the guest reads (see si.cpp).
	static int8_t StickAxis(int negative, int positive)
	{
		int value = positive - negative;

		if (value > 127) value = 127;
		if (value < -127) value = -127;

		return (int8_t)value;
	}

public:
	StandardPad(int index) : index(index)
	{
		for (int i = 0; i < ACT_MAX; i++)
		{
			bindings[i].keyboard = 0;
			bindings[i].gamepad = 0;
		}
	}

	uint32_t Type() override { return PERIPH_DEVICE_STANDARD_PAD; }

	int ActuatorCount() override { return ACT_MAX; }
	const PeriphActuator* Actuator(int index) override
	{
		return (index >= 0 && index < ACT_MAX) ? &pad_actuators[index] : nullptr;
	}
	PeriphBindings* ActuatorBindings(int index) override
	{
		return (index >= 0 && index < ACT_MAX) ? &bindings[index] : nullptr;
	}

	void SetState(int actuator, int value) override
	{
		if (actuator >= 0 && actuator < ACT_MAX)
		{
			if (value < 0) value = 0;
			if (value > pad_actuators[actuator].max) value = pad_actuators[actuator].max;

			state[actuator] = value;
		}
	}

	// -----------------------------------------------------------------------

	void LoadConfig(int index) override
	{
		this->index = index;

		bool configured = false;

		for (int i = 0; i < ACT_MAX; i++)
		{
			bool found = false;

			bindings[i].keyboard = LoadBinding(index, "VKEY", pad_actuators[i].id, found);
			configured |= found;

			bindings[i].gamepad = LoadBinding(index, "GCKEY", pad_actuators[i].id, found);
			configured |= found;
		}

		// A pad that has never been configured gets the standard layout of its model right away, so
		// that a pad which is plugged into a socket works without a trip to the settings dialog. The
		// keyboard half of it only goes to the first pad of the pool (see FirstOfModel).
		if (!configured)
		{
			Peripherals::Instance().DefaultBindings(index, Peripherals::Instance().FirstOfModel(index));
		}
	}

	void SaveConfig(int index) override
	{
		for (int i = 0; i < ACT_MAX; i++)
		{
			SaveBinding(index, "VKEY", pad_actuators[i].id, bindings[i].keyboard);
			SaveBinding(index, "GCKEY", pad_actuators[i].id, bindings[i].gamepad);
		}
	}

	void Attach(int port) override
	{
		this->port = port;
	}

	void Detach() override
	{
		// A motor that is still running must be stopped on the host when the pad is unplugged.
		HostInput* host = Peripherals::Instance().Host();

		if (motor != PAD_MOTOR_STOP && host != nullptr)
		{
			host->Rumble(HostPadOf(port), PAD_MOTOR_STOP);
		}

		motor = PAD_MOTOR_STOP;
		port = -1;
	}

	// -----------------------------------------------------------------------

	//! The answer to the Joybus command set (standard-controller.md 3).
	void Transfer(int outlen, int inlen, uint8_t* buf) override
	{
		uint8_t cmd = buf[0];

		switch (cmd)
		{
			// Get type and status
			case 0x00:
			{
				// 0 : use sub-type
				// 2 : n64 mouse
				// 5 : n64 controller
				// 9 : default gc controller
				buf[0] = 9;
				buf[1] = 0;         // sub-type
				buf[2] = (uint8_t)(motor << 4);     // STAT: the latched motor state is bits 5:4
				break;
			}

			// The standard poll: the data the guest reads comes from the poll (see Poll), so a
			// communication transfer of it has nothing to answer.
			case 0x40:
			case 0x42:
				return;

			// Read the calibration origins
			case 0x41:
			{
				buf[0] = 0x41;
				buf[1] = 0;
				buf[2] = buf[3] = buf[4] = buf[5] = 0x80;
				buf[6] = buf[7] = 0x1f;
				break;
			}

			default:
			{
				Debug::Halt(
					"Unknown SI command. chan:%i, cmd:%02X, out:%i, in:%i\n",
					port >= PERIPH_PORT_SI0 ? port - PERIPH_PORT_SI0 : 0, cmd, outlen, inlen);
			}
		}
	}

	//! The state the guest polls: the buttons and the six analog channels.
	bool Poll(PADState* pad) override
	{
		memset(pad, 0, sizeof(PADState));

		uint16_t button = 0;

		if (Act(ACT_UP)) button |= PAD_BUTTON_UP;
		if (Act(ACT_DOWN)) button |= PAD_BUTTON_DOWN;
		if (Act(ACT_LEFT)) button |= PAD_BUTTON_LEFT;
		if (Act(ACT_RIGHT)) button |= PAD_BUTTON_RIGHT;

		if (Act(ACT_A)) button |= PAD_BUTTON_A;
		if (Act(ACT_B)) button |= PAD_BUTTON_B;
		if (Act(ACT_X)) button |= PAD_BUTTON_X;
		if (Act(ACT_Y)) button |= PAD_BUTTON_Y;
		if (Act(ACT_START)) button |= PAD_BUTTON_START;

		// The digital L and R are only reported when their analog control is pressed all the way
		// down, which is what the pad does.
		if (Act(ACT_TRIGGERL) > 0)
		{
			button |= PAD_TRIGGER_L;
		}

		if (Act(ACT_TRIGGERR) > 0)
		{
			button |= PAD_TRIGGER_R;
		}

		if (Act(ACT_TRIGGERZ))
		{
			button |= PAD_TRIGGER_Z;
		}

		pad->button = button;

		pad->stickX = StickAxis(Dir(ACT_XLEFT50, ACT_XLEFT100), Dir(ACT_XRIGHT50, ACT_XRIGHT100));
		pad->stickY = StickAxis(Dir(ACT_XDOWN50, ACT_XDOWN100), Dir(ACT_XUP50, ACT_XUP100));
		pad->substickX = StickAxis(state[ACT_CXLEFT], state[ACT_CXRIGHT]);
		pad->substickY = StickAxis(state[ACT_CXDOWN], state[ACT_CXUP]);
		pad->triggerLeft = (uint8_t)Act(ACT_TRIGGERL);
		pad->triggerRight = (uint8_t)Act(ACT_TRIGGERR);

		return true;
	}

	bool SetMotor(int cmd) override
	{
		HostInput* host = Peripherals::Instance().Host();

		if (host == nullptr)
		{
			return false;
		}

		motor = cmd;

		return host->Rumble(HostPadOf(port), cmd);
	}
};

// ---------------------------------------------------------------------------
// The built in models

namespace
{
	PeripheralDevice* CreateStandardPad(int index)
	{
		return new StandardPad(index);
	}
}

//! The models this module implements. The devices of the other modules register themselves (see
//! memcard.cpp), which is why this runs when the pool is first built rather than at load time.
static struct PeriphBuiltinModels
{
	PeriphBuiltinModels()
	{
		Peripherals::RegisterFactory(PERIPH_DEVICE_STANDARD_PAD,
			"Standard Controller", "DOL-003, the pad on one of the four SI sockets",
			PERIPH_BUS_SI, CreateStandardPad);
	}
} periph_builtin_models;
