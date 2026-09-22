/*

# Peripheral devices

This is the implementation of the subsystem `peripherals.h` declares: the pool of devices (what is
in it, what is plugged where, what the settings window edits) and the dispatch of the emulation to
the device that is plugged into a port. The devices themselves live in their own modules - the
standard controller in cont.cpp, the memory card in memcard.cpp - and register the factory that
creates them.

## The pool and the configuration

The pool is the list of the devices the user has, and it is the "Devices" list of the "peripherals"
section of the configuration: one entry per device, in the order the pool has them, which is why a
device is addressed by its index and not by a number in the name of a variable (see config.h). An
entry holds the model of the device ("Type"), the name the user gave it ("Name"), the port it is
plugged into ("Port") and whatever the device keeps of its own (a pad keeps its bindings there, a
memory card its file).

An entry whose "Type" is not there is a device slot that was never configured, and the slot of one
of the console's ports holds the device that port is meant for. An entry whose "Type" is 0 is a
device that was taken out of the pool: its slot stays empty (so that the index of every other
device, and of the variables of its configuration, does not change) and the next device that is
added takes it.

A configuration that was written before the pool existed has none of this. Such a configuration is
not migrated: it is *read* through the old names, so that the emulator comes up with the same
controllers and the same cards as the build that wrote it. The four sockets are then
`PluggedIn_<n>` from the "controllers" section, the bindings are `VKEY_FOR_*_<n>`, and the two card
slots are `MemcardA_*` / `MemcardB_*` from "memcards".

## The pool lives as long as the emulator does

The pool is opened when the emulator starts and closed when it is shut down, not with the machine:
the settings window can be used with no game loaded, and what it edits is the pool (a device is
plugged into a port of the console, which exists whether or not a machine is running). The machine
only matters to a memory card, which is an EXI device: it connects to the channel of the machine
that reads it, so the pool is told when a machine appears and when one goes away (MachineOpened /
MachineClosed).

## A device is never destroyed while the emulator runs

The emulation thread takes a device out of the pool (to poll a pad, to run a card transfer) while
the settings window may be reconfiguring it, so a device is never destroyed before the pool itself
is closed in one piece. A device that is removed from the pool is kept alive (and its slot is marked
empty) until then.

*/

#include "pch.h"

using namespace Debug;

// ---------------------------------------------------------------------------
// The device configuration
//
// Every device of the pool owns the variables of its own entry of the list (see config.h), so a
// device implementation names a variable by its own name ("Type", "VKEY_FOR_A") and nothing else.

bool PeriphConfigExists(int index, const char* name)
{
	return ConfigArrayValueExists(PERIPH_DEVICES, USER_PERIPH, index, name);
}

int PeriphConfigInt(int index, const char* name, int def)
{
	return GetConfigArrayInt(PERIPH_DEVICES, USER_PERIPH, index, name, def);
}

void PeriphConfigSetInt(int index, const char* name, int value)
{
	SetConfigArrayInt(PERIPH_DEVICES, USER_PERIPH, index, name, value);
}

std::string PeriphConfigString(int index, const char* name, const std::string& def)
{
	const wchar_t* value = GetConfigArrayString(PERIPH_DEVICES, USER_PERIPH, index, name);

	return (*value != 0) ? Util::WstringToString(value) : def;
}

void PeriphConfigSetString(int index, const char* name, const std::string& value)
{
	SetConfigArrayString(PERIPH_DEVICES, USER_PERIPH, index, name, Util::StringToWstring(value).c_str());
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

//! The host game controller a pad in a port is driven by: the host game controller the host reports
//! under the number of its socket, which is what the SDL backend has always done.
int HostPadOfPort(int port)
{
	if (port >= PERIPH_PORT_SI0 && port <= PERIPH_PORT_SI3)
	{
		return port - PERIPH_PORT_SI0;
	}

	return 0;
}

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
	if (opened)
	{
		return;
	}

	Report(Channel::SI, "Peripheral devices\n");

	host = HostInputCreate();
	opened = true;

	// A configuration that has never seen the pool has no list at all, and then the console comes up
	// with the device every port is meant for: the four sockets and the two card slots.
	for (int i = 0; i < PERIPH_MAX_DEVICES; i++)
	{
		ports[i] = -1;

		uint32_t type = PERIPH_DEVICE_NONE;

		if (ConfigArrayValueExists(PERIPH_DEVICES, USER_PERIPH, i, "Type"))
		{
			type = (uint32_t)PeriphConfigInt(i, "Type", PERIPH_DEVICE_NONE);
		}
		else if (i < PERIPH_PORT_MAX)
		{
			type = DefaultModelOfPort(i);
		}

		if (type == PERIPH_DEVICE_NONE)
		{
			continue;       // a slot of the pool that holds no device
		}

		if (CreateDevice(type, i) == nullptr)
		{
			continue;
		}

		names[i] = PeriphConfigString(i, "Name", ModelName(type));

		// The port is read after the device exists, so that a configuration that predates the pool
		// is asked for the old name only when the new one is not there (see LegacyPort). The
		// configuration is not written back here: building the pool from it must not change it.
		int port = ConfigArrayValueExists(PERIPH_DEVICES, USER_PERIPH, i, "Port")
			? PeriphConfigInt(i, "Port", -1)
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

	MachineClosed();

	for (int i = 0; i < PERIPH_MAX_DEVICES; i++)
	{
		delete devices[i];
		devices[i] = nullptr;

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

void Peripherals::MachineOpened()
{
	lock.Lock();

	int attached[PERIPH_MAX_DEVICES];
	int count = 0;

	for (int i = 0; i < PERIPH_MAX_DEVICES; i++)
	{
		if (devices[i] != nullptr && ports[i] >= 0)
		{
			attached[count++] = i;
		}
	}

	lock.Unlock();

	// A device is plugged into a socket of the console, and the console has just been built: a
	// memory card goes into its slot here (it is an EXI device and needs the console to talk to).
	for (int i = 0; i < count; i++)
	{
		devices[attached[i]]->Attach(ports[attached[i]]);
	}
}

void Peripherals::MachineClosed()
{
	// The console is being taken apart, so the devices that were plugged into it are unplugged (a
	// card flushes the file it was writing to). The pool keeps them and the ports they are in: only
	// the machine they were talking to is gone.
	for (int i = 0; i < PERIPH_MAX_DEVICES; i++)
	{
		if (devices[i] != nullptr && ports[i] >= 0)
		{
			devices[i]->Detach();
		}
	}
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

	// The device is written to the configuration as it is added, name and settings together, so that
	// the pool of the next run has it (a device that is only in the memory of this run is not a
	// device the user has).
	PeriphConfigSetInt(index, "Type", (int)type);
	PeriphConfigSetString(index, "Name", names[index]);
	device->SaveConfig(index);

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

void Peripherals::ApplyDefaultBindings(int index, bool keyboard)
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

void Peripherals::DefaultBindings(int index, bool keyboard)
{
	ApplyDefaultBindings(index, keyboard);

	PeripheralDevice* device = Device(index);

	if (device != nullptr)
	{
		device->SaveConfig(index);
	}
}

void Peripherals::SetBinding(int index, int actuator, bool gamepad, int binding)
{
	PeripheralDevice* device = Device(index);

	if (device == nullptr)
	{
		return;
	}

	PeriphBindings* bindings = device->ActuatorBindings(actuator);

	if (bindings == nullptr)
	{
		return;
	}

	if (gamepad)
	{
		bindings->gamepad = binding;
	}
	else
	{
		bindings->keyboard = binding;
	}

	device->SaveConfig(index);
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

	device->SaveConfig(index);
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
	int pad = HostPadOfPort(port);

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
