/*

# Peripheral devices

This is the implementation of the subsystem `peripherals.h` declares: the pool of devices (what is
in it, what is plugged where, what the settings window edits) and the dispatch of the emulation to
the device that is plugged into a port. The devices themselves live in their own modules - the
standard controller in cont.cpp, the memory card in memcard.cpp - and register the factory that
creates them.

## The pool and the configuration

The pool is the list of the devices the user has, and it is the "Devices" list of the "peripherals"
section of the configuration: one entry per device, in the order the settings window shows them
(see the list accessors in config.h). Every device owns the settings of its own entry - its model
("Type"), the name the user gave it ("Name"), the port it is plugged into ("Port") and whatever the
device keeps of its own (a pad its bindings, a memory card its file) - and it holds the entry, not
its position: a device that is taken out of the pool takes its settings with it and leaves no hole
behind, so nothing that is stored next to a device can end up belonging to another one.

The pool of a fresh console is what `DefaultSettings.json` ships: the four controller sockets and
the two memory card slots, each with the device that port is meant for, and none of them plugged in.

## The pool lives as long as the emulator does

The pool is opened when the emulator starts and closed when it is shut down, not with the machine:
the settings window can be used with no game loaded, and what it edits is the pool (a device is
plugged into a port of the console, which exists whether or not a machine is running). The machine
only matters to a memory card, which is an EXI device: it goes into its slot when a machine appears
and is taken out (and flushed) when one goes away (MachineOpened / MachineClosed).

## A device is never destroyed while the emulator runs

The emulation thread takes a device out of the pool (to poll a pad, to run a card transfer) while
the settings window may be reconfiguring it, so a device that is removed from the pool is detached
and kept alive (with the settings it was using) until the pool itself is closed in one piece.

*/

#include "pch.h"

using namespace Debug;

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
		PeripheralDevice* (*factory)();
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
	PeripheralDevice* (*factory)())
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

	// The pool is what the list holds, and nothing else: a configuration that was written before the
	// devices were one list kept them in variables of the section ("Device3_Name",
	// "Device3_VKEY_FOR_A"), and those are dropped here so that they do not travel along.
	ConfigSectionKeepOnly(USER_PERIPH, PERIPH_DEVICES);

	// The pool is the list of the configuration: every entry of it is one device (see the module
	// comment), and a fresh console finds the four sockets and the two card slots there because that
	// is what the shipped defaults say.
	int count = ConfigListSize(PERIPH_DEVICES, USER_PERIPH);

	for (int at = 0; at < count && at < PERIPH_MAX_DEVICES; at++)
	{
		ConfigEntry* entry = ConfigListAt(PERIPH_DEVICES, USER_PERIPH, at);

		if (entry == nullptr)
		{
			continue;
		}

		uint32_t type = (uint32_t)GetConfigEntryInt(entry, "Type", PERIPH_DEVICE_NONE);

		if (type == PERIPH_DEVICE_NONE)
		{
			continue;       // an entry of the list that names no model is not a device
		}

		PeripheralDevice* device = CreateDevice(type, entry);

		if (device == nullptr)
		{
			continue;
		}

		int port = GetConfigEntryInt(entry, "Port", -1);

		if (port >= 0 && port < PERIPH_PORT_MAX)
		{
			Plug(DeviceIndexOf(device), port);
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

	devices.clear();
	ports.clear();
	names.clear();

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
	std::vector<std::pair<PeripheralDevice*, int>> attached;
	AttachedDevices(attached);

	// The console exists now, so a device that talks to it goes into its socket: a memory card is an
	// EXI device and needs a console to talk to (see MemoryCardDevice::Attach).
	for (auto& plugged : attached)
	{
		plugged.first->Attach(plugged.second);
	}
}

void Peripherals::MachineClosed()
{
	std::vector<std::pair<PeripheralDevice*, int>> attached;
	AttachedDevices(attached);

	// The console is being taken apart: the devices are unplugged from it (a card flushes the file it
	// was writing to). The pool keeps them and the ports they are in.
	for (auto& plugged : attached)
	{
		plugged.first->Detach();
	}
}

void Peripherals::AttachedDevices(std::vector<std::pair<PeripheralDevice*, int>>& attached)
{
	attached.clear();

	lock.Lock();

	for (size_t i = 0; i < devices.size(); i++)
	{
		if (ports[i] >= 0)
		{
			attached.push_back(std::make_pair(devices[i], ports[i]));
		}
	}

	lock.Unlock();
}

int Peripherals::Count()
{
	lock.Lock();
	int count = (int)devices.size();
	lock.Unlock();

	return count;
}

PeripheralDevice* Peripherals::CreateDevice(uint32_t type, ConfigEntry* entry)
{
	const PeriphModel* model = FindModel(type);

	if (model == nullptr || model->factory == nullptr)
	{
		Report(Channel::SI, "Peripheral device %08X is not supported by this build\n", type);
		return nullptr;
	}

	PeripheralDevice* device = model->factory();

	if (device == nullptr)
	{
		return nullptr;
	}

	device->SetConfig(entry);

	// The device joins the pool before it reads its settings: the layout of one that was never
	// configured depends on the devices that are already there (see ContPad::LoadConfig).
	std::string name = Util::WstringToString(GetConfigEntryString(entry, "Name"));

	if (name.empty())
	{
		name = ModelName(type);
	}

	lock.Lock();
	devices.push_back(device);
	ports.push_back(-1);
	names.push_back(name);
	lock.Unlock();

	device->LoadConfig();

	return device;
}

int Peripherals::DeviceIndexOf(PeripheralDevice* device)
{
	lock.Lock();

	int found = -1;

	for (size_t i = 0; i < devices.size(); i++)
	{
		if (devices[i] == device)
		{
			found = (int)i;
			break;
		}
	}

	lock.Unlock();

	return found;
}

PeripheralDevice* Peripherals::Device(int index)
{
	lock.Lock();

	PeripheralDevice* device = (index >= 0 && index < (int)devices.size()) ? devices[index] : nullptr;

	lock.Unlock();

	return device;
}

int Peripherals::AddDevice(uint32_t type)
{
	if (Count() >= PERIPH_MAX_DEVICES)
	{
		Report(Channel::SI, "The peripheral pool is full\n");
		return -1;
	}

	// The device is written to the configuration as it is added, so that the next run finds it: the
	// entry first (a device is the settings of its entry), then the settings the model starts with.
	ConfigEntry* entry = ConfigListAppend(PERIPH_DEVICES, USER_PERIPH);

	if (entry == nullptr)
	{
		return -1;
	}

	SetConfigEntryInt(entry, "Type", (int)type);

	PeripheralDevice* device = CreateDevice(type, entry);

	if (device == nullptr)
	{
		ConfigListRemove(PERIPH_DEVICES, USER_PERIPH, entry);
		return -1;
	}

	SetConfigEntryString(entry, "Name", Util::StringToWstring(names.back()).c_str());

	// The defaults of the model are what the entry holds from now on.
	device->SaveConfig();

	return DeviceIndexOf(device);
}

void Peripherals::RemoveDevice(int index)
{
	PeripheralDevice* device = Device(index);

	if (device == nullptr)
	{
		return;
	}

	Detach(index);

	lock.Lock();
	devices.erase(devices.begin() + index);
	ports.erase(ports.begin() + index);
	names.erase(names.begin() + index);
	lock.Unlock();

	// The entry of the device leaves the list with it: a device is its own settings, and the ones
	// that follow it are not disturbed (they are entries of their own, not positions).
	ConfigListRemove(PERIPH_DEVICES, USER_PERIPH, device->Config());
	device->SetConfig(nullptr);

	// The object outlives its entry for as long as the emulator runs: a thread may have just taken
	// it out of the pool. It is destroyed with the pool, in Close.
	retired.push_back(device);
}

std::string Peripherals::Name(int index)
{
	lock.Lock();
	std::string name = (index >= 0 && index < (int)names.size()) ? names[index] : "";
	lock.Unlock();

	return name;
}

void Peripherals::SetName(int index, const std::string& name)
{
	PeripheralDevice* device = Device(index);

	if (device == nullptr || device->Config() == nullptr)
	{
		return;
	}

	lock.Lock();
	names[index] = name;
	lock.Unlock();

	SetConfigEntryString(device->Config(), "Name", Util::StringToWstring(name).c_str());
}

void Peripherals::ApplyDefaultBindings(PeripheralDevice* device, bool keyboard)
{
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

void Peripherals::DefaultBindings(PeripheralDevice* device, bool keyboard)
{
	ApplyDefaultBindings(device, keyboard);

	if (device != nullptr)
	{
		device->SaveConfig();
	}
}

void Peripherals::SetBinding(PeripheralDevice* device, int actuator, bool gamepad, int binding)
{
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

	device->SaveConfig();
}

void Peripherals::ClearBindings(PeripheralDevice* device)
{
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

	device->SaveConfig();
}

bool Peripherals::FirstOfModel(PeripheralDevice* device)
{
	if (device == nullptr)
	{
		return false;
	}

	lock.Lock();

	bool first = true;

	for (size_t i = 0; i < devices.size(); i++)
	{
		if (devices[i] == device)
		{
			break;
		}

		if (devices[i] != nullptr && devices[i]->Type() == device->Type())
		{
			first = false;
			break;
		}
	}

	lock.Unlock();

	return first;
}

// ---------------------------------------------------------------------------
// The ports

int Peripherals::DeviceIndexOnPort(int port)
{
	if (port < 0 || port >= PERIPH_PORT_MAX)
	{
		return -1;
	}

	lock.Lock();

	int found = -1;

	for (size_t i = 0; i < devices.size(); i++)
	{
		if (ports[i] == port)
		{
			found = (int)i;
			break;
		}
	}

	lock.Unlock();

	return found;
}

void Peripherals::Plug(int index, int port)
{
	lock.Lock();

	if (index >= 0 && index < (int)devices.size())
	{
		ports[index] = port;
	}

	lock.Unlock();

	PeripheralDevice* device = Device(index);

	if (device != nullptr)
	{
		device->Attach(port);
	}
}

PeripheralDevice* Peripherals::DeviceOnPort(int port)
{
	return Device(DeviceIndexOnPort(port));
}

bool Peripherals::Attach(int index, int port)
{
	PeripheralDevice* device = Device(index);

	if (device == nullptr || port < 0 || port >= PERIPH_PORT_MAX)
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

	SetConfigEntryInt(device->Config(), "Port", port);

	return true;
}

void Peripherals::Detach(int index)
{
	PeripheralDevice* device = Device(index);

	if (device == nullptr)
	{
		return;
	}

	lock.Lock();
	int port = (index >= 0 && index < (int)ports.size()) ? ports[index] : -1;
	if (index >= 0 && index < (int)ports.size())
	{
		ports[index] = -1;
	}
	lock.Unlock();

	if (port < 0)
	{
		return;
	}

	if (device->Config() != nullptr)
	{
		SetConfigEntryInt(device->Config(), "Port", -1);
	}

	device->Detach();
}

int Peripherals::PortOf(int index)
{
	lock.Lock();
	int port = (index >= 0 && index < (int)ports.size()) ? ports[index] : -1;
	lock.Unlock();

	return port;
}

// ---------------------------------------------------------------------------
// The emulation

bool Peripherals::PollSI(int chan, PADState* state)
{
	PeripheralDevice* device = DeviceOnPort(PERIPH_PORT_SI(chan));

	if (device == nullptr)
	{
		return false;
	}

	// The bindings are resolved here, on the emulation thread: the device is handed the value of
	// every actuator and turns them into its protocol by itself.
	int pad = HostPadOfPort(PERIPH_PORT_SI(chan));

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

bool Peripherals::TransferSI(int chan, int outlen, int inlen, uint8_t* buf)
{
	PeripheralDevice* device = DeviceOnPort(PERIPH_PORT_SI(chan));

	// Nothing is plugged into the channel: the transfer is not answered at all, which is how the
	// guest finds out that there is no controller (SISR[NOREP], latched by the caller).
	if (device == nullptr)
	{
		return false;
	}

	device->Transfer(outlen, inlen, buf);

	return true;
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
