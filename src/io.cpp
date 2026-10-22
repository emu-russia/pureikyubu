/*

# I/O devices

This is the implementation of the subsystem `io.h` declares: the pool of devices (what is
in it, what is plugged where, what the settings window edits) and the dispatch of the emulation to
the device that is plugged into a port. The devices themselves live in their own modules - the
standard controller in cont.cpp, the memory card in memcard.cpp - and register the factory that
creates them.

## The pool and the configuration

The pool is the list of the devices the user has, and it is the "Devices" list of the "io"
section of the configuration: one entry per device, in the order the settings window shows them
(see the list accessors in config.h). Every device owns the settings of its own entry - its model
("Type"), the name the user gave it ("Name"), the port it is plugged into ("Port") and whatever the
device keeps of its own (a pad its bindings, a memory card its file) - and it holds the entry, not
its position: a device that is taken out of the pool takes its settings with it and leaves no hole
behind, so nothing that is stored next to a device can end up belonging to another one.

The pool of a fresh console is what `DefaultSettings.json` ships: the four controller sockets and
the two memory card slots, each with the device that port is meant for. The shipped list does not
say where the devices are plugged in - that is what "the port is meant for" means - so a device
whose entry names no port goes into the first free port of its bus when the pool is opened, and the
configuration is told (see `Open`). A device the user unplugged on purpose names its port as -1 and
stays where it was put.

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
	struct IoModel
	{
		uint32_t    type;
		const char* name;
		const char* info;
		int         bus;
		int         page;               //!< the settings page it is edited on (IO_PAGE_*)
		IoDevice* (*factory)();
	};

	std::vector<IoModel>& Models()
	{
		// A function local static: a device module that registers its factory from a static
		// constructor of its own cannot construct this list, so the list is built on first use.
		static std::vector<IoModel> models;
		return models;
	}

	const IoModel* FindModel(uint32_t type)
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
		const IoModel* model = FindModel(type);
		return model != nullptr ? model->bus : -1;
	}

	//! The bus a model is plugged into (the model of a device always exists: the device was created
	//! from it).
	int ModelBusOf(uint32_t type)
	{
		const IoModel* model = FindModel(type);
		return model != nullptr ? model->bus : -1;
	}

	//! The settings page a model is edited on (the model of a device always exists).
	int ModelPageOf(uint32_t type)
	{
		const IoModel* model = FindModel(type);
		return model != nullptr ? model->page : -1;
	}

	const char* BusName(int bus)
	{
		switch (bus)
		{
			case IO_BUS_SI:  return "Serial Interface";
			case IO_BUS_EXI: return "EXI";
			case IO_BUS_HSP: return "Hi-Speed Port";
		}

		return "?";
	}
}

// ---------------------------------------------------------------------------
// The ports
//
// The console has four controller sockets and two memory card slots. Every port belongs to a bus,
// and a device can only be plugged into a port of its own bus.

namespace
{
	struct IoPortDesc
	{
		const char* name;
		int         bus;
		uint32_t    defaultModel;       //!< the model the port is meant for
	};

	const IoPortDesc port_desc[IO_PORT_MAX] =
	{
		{ "Controller Port 1",  IO_BUS_SI,  IO_DEVICE_STANDARD_PAD },
		{ "Controller Port 2",  IO_BUS_SI,  IO_DEVICE_STANDARD_PAD },
		{ "Controller Port 3",  IO_BUS_SI,  IO_DEVICE_STANDARD_PAD },
		{ "Controller Port 4",  IO_BUS_SI,  IO_DEVICE_STANDARD_PAD },
		{ "Memory Card Slot A", IO_BUS_EXI, IO_DEVICE_MEMCARD },
		{ "Memory Card Slot B", IO_BUS_EXI, IO_DEVICE_MEMCARD },
		{ "Serial Port 1",      IO_BUS_EXI, IO_DEVICE_BBA },
		{ "Hi-Speed Port",      IO_BUS_HSP, IO_DEVICE_GBPLAYER },
	};

	//! The value of one host control bound to an actuator. A button (the keyboard is one too) gives
	//! the actuator its full value, an axis gives its deflection scaled into the actuator's range,
	//! in the direction the binding was captured with.
	int BindingValue(HostInput* host, int pad, const IoActuator* actuator, int binding)
	{
		if (binding == 0)
		{
			return 0;
		}

		if (IO_HOST_IS_BUTTON(binding))
		{
			return host->GamepadButton(pad, IO_HOST_BUTTON(binding)) ? actuator->max : 0;
		}

		if (IO_HOST_IS_AXIS(binding))
		{
			int axis = host->GamepadAxis(pad, IO_HOST_AXIS(binding));
			bool positive = IO_HOST_AXIS_POS(binding);

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
// The I/O subsystem

//! The host game controller a pad in a port is driven by: the host game controller the host reports
//! under the number of its socket, which is what the SDL backend has always done.
int HostPadOfPort(int port)
{
	if (port >= IO_PORT_SI0 && port <= IO_PORT_SI3)
	{
		return port - IO_PORT_SI0;
	}

	return 0;
}

Io& Io::Instance()
{
	static Io instance;
	return instance;
}

void Io::RegisterFactory(uint32_t type, const char* name, const char* info, int bus, int page,
	IoDevice* (*factory)())
{
	for (auto& model : Models())
	{
		if (model.type == type)
		{
			model.name = name;
			model.info = info;
			model.bus = bus;
			model.page = page;
			model.factory = factory;
			return;
		}
	}

	Models().push_back(IoModel{ type, name, info, bus, page, factory });
}

const char* Io::ModelName(uint32_t type)
{
	const IoModel* model = FindModel(type);
	return model != nullptr ? model->name : nullptr;
}

int Io::ModelBus(uint32_t type)
{
	return ModelBusOf(type);
}

int Io::ModelPage(uint32_t type)
{
	return ModelPageOf(type);
}

int Io::ModelCount()
{
	return (int)Models().size();
}

uint32_t Io::ModelAt(int index)
{
	const std::vector<IoModel>& models = Models();

	if (index < 0 || index >= (int)models.size())
	{
		return IO_DEVICE_NONE;
	}

	return models[index].type;
}

uint32_t Io::DefaultModelOfPort(int port)
{
	if (port < 0 || port >= IO_PORT_MAX)
	{
		return IO_DEVICE_NONE;
	}

	return port_desc[port].defaultModel;
}

const char* Io::PortName(int port)
{
	if (port < 0 || port >= IO_PORT_MAX)
	{
		return "?";
	}

	return port_desc[port].name;
}

int Io::PortBus(int port)
{
	if (port < 0 || port >= IO_PORT_MAX)
	{
		return -1;
	}

	return port_desc[port].bus;
}

HostInput* Io::Host()
{
	return host;
}

// ---------------------------------------------------------------------------
// The pool

void Io::Open()
{
	if (opened)
	{
		return;
	}

	Report(Channel::SI, "I/O devices\n");

	host = HostInputCreate();
	opened = true;

	// The pool is what the list holds, and nothing else: a configuration that was written before the
	// devices were one list kept them in variables of the section ("Device3_Name",
	// "Device3_VKEY_FOR_A"), and those are dropped here so that they do not travel along.
	ConfigSectionKeepOnly(USER_IO, IO_DEVICES);

	// The pool is the list of the configuration: every entry of it is one device (see the module
	// comment), and a fresh console finds the four sockets and the two card slots there because that
	// is what the shipped defaults say.
	int count = ConfigListSize(IO_DEVICES, USER_IO);

	for (int at = 0; at < count && at < IO_MAX_DEVICES; at++)
	{
		ConfigEntry* entry = ConfigListAt(IO_DEVICES, USER_IO, at);

		if (entry == nullptr)
		{
			continue;
		}

		uint32_t type = (uint32_t)GetConfigEntryInt(entry, "Type", IO_DEVICE_NONE);

		if (type == IO_DEVICE_NONE)
		{
			continue;       // an entry of the list that names no model is not a device
		}

		IoDevice* device = CreateDevice(type, entry);

		if (device == nullptr)
		{
			continue;
		}

		// An entry that does not name a port at all is a device that has never been placed: the
		// shipped defaults are written that way (they list the four sockets and the two card slots
		// but do not say where any of them is plugged in), and until this was handled here a fresh
		// installation came up with every device of the pool dangling and no controller answering.
		// It goes where the console expects its model and the configuration is told, so that the
		// next run finds it there.
		//
		// An entry that *does* name a port is a decision the user made, "-1" (unplugged)
		// included: only a member that is not there is a missing value.
		int port = GetConfigEntryInt(entry, "Port", -1);

		if (!ConfigEntryValueExists(entry, "Port"))
		{
			port = FreePortFor(device);

			if (port >= 0)
			{
				SetConfigEntryInt(entry, "Port", port);
			}
		}

		if (port >= 0 && port < IO_PORT_MAX)
		{
			// A port holds one device, and an entry that names a port the configuration has already
			// given away takes it: the device that was there is unplugged, and its own entry is told
			// so that the file comes back consistent. The shipped defaults are the case that needs
			// this - they name no port at all and are placed by `FreePortFor` above, so a user who
			// puts a Game Boy Advance into a controller socket writes that socket's number into the
			// *new* entry; until this was handled the pad that had been placed automatically kept the
			// socket: the new device reported that it had attached, the pool dispatched the port to
			// the pad, and nothing said which of the two was really in the socket.
			int occupant = DeviceIndexOnPort(port);

			if (occupant >= 0 && occupant != DeviceIndexOf(device))
			{
				Report(Channel::Norm, "%s was unplugged: %s takes the port\n",
					Name(occupant).c_str(), Name(DeviceIndexOf(device)).c_str());
				Detach(occupant);
			}

			Plug(DeviceIndexOf(device), port);
		}
	}
}

int Io::FreePortFor(IoDevice* device)
{
	if (device == nullptr)
	{
		return -1;
	}

	int bus = ModelBusOf(device->Type());

	for (int port = 0; port < IO_PORT_MAX; port++)
	{
		if (port_desc[port].bus != bus)
		{
			continue;
		}

		if (DeviceIndexOnPort(port) < 0)
		{
			return port;
		}
	}

	return -1;
}

void Io::Close()
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

void Io::MachineOpened()
{
	std::vector<std::pair<IoDevice*, int>> attached;
	AttachedDevices(attached);

	// The console exists now, so a device that talks to it goes into its socket: a memory card is an
	// EXI device and needs a console to talk to (see MemoryCardDevice::Attach).
	for (auto& plugged : attached)
	{
		plugged.first->Attach(plugged.second);
	}
}

void Io::MachineClosed()
{
	std::vector<std::pair<IoDevice*, int>> attached;
	AttachedDevices(attached);

	// The console is being taken apart: the devices are unplugged from it (a card flushes the file it
	// was writing to). The pool keeps them and the ports they are in.
	for (auto& plugged : attached)
	{
		plugged.first->Detach();
	}
}

void Io::AttachedDevices(std::vector<std::pair<IoDevice*, int>>& attached)
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

int Io::Count()
{
	lock.Lock();
	int count = (int)devices.size();
	lock.Unlock();

	return count;
}

IoDevice* Io::CreateDevice(uint32_t type, ConfigEntry* entry)
{
	const IoModel* model = FindModel(type);

	if (model == nullptr || model->factory == nullptr)
	{
		Report(Channel::SI, "I/O device %08X is not supported by this build\n", type);
		return nullptr;
	}

	IoDevice* device = model->factory();

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

int Io::DeviceIndexOf(IoDevice* device)
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

IoDevice* Io::Device(int index)
{
	lock.Lock();

	IoDevice* device = (index >= 0 && index < (int)devices.size()) ? devices[index] : nullptr;

	lock.Unlock();

	return device;
}

int Io::AddDevice(uint32_t type)
{
	if (Count() >= IO_MAX_DEVICES)
	{
		Report(Channel::SI, "The I/O pool is full\n");
		return -1;
	}

	// The device is written to the configuration as it is added, so that the next run finds it: the
	// entry first (a device is the settings of its entry), then the settings the model starts with.
	ConfigEntry* entry = ConfigListAppend(IO_DEVICES, USER_IO);

	if (entry == nullptr)
	{
		return -1;
	}

	SetConfigEntryInt(entry, "Type", (int)type);

	IoDevice* device = CreateDevice(type, entry);

	if (device == nullptr)
	{
		ConfigListRemove(IO_DEVICES, USER_IO, entry);
		return -1;
	}

	SetConfigEntryString(entry, "Name", Util::StringToWstring(names.back()).c_str());

	// The defaults of the model are what the entry holds from now on.
	device->SaveConfig();

	return DeviceIndexOf(device);
}

void Io::RemoveDevice(int index)
{
	IoDevice* device = Device(index);

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
	ConfigListRemove(IO_DEVICES, USER_IO, device->Config());
	device->SetConfig(nullptr);

	// The object outlives its entry for as long as the emulator runs: a thread may have just taken
	// it out of the pool. It is destroyed with the pool, in Close.
	retired.push_back(device);
}

std::string Io::Name(int index)
{
	lock.Lock();
	std::string name = (index >= 0 && index < (int)names.size()) ? names[index] : "";
	lock.Unlock();

	return name;
}

void Io::SetName(int index, const std::string& name)
{
	IoDevice* device = Device(index);

	if (device == nullptr || device->Config() == nullptr)
	{
		return;
	}

	lock.Lock();
	names[index] = name;
	lock.Unlock();

	SetConfigEntryString(device->Config(), "Name", Util::StringToWstring(name).c_str());
}

void Io::ApplyDefaultBindings(IoDevice* device, bool keyboard)
{
	if (device == nullptr || host == nullptr)
	{
		return;
	}

	for (int i = 0; i < device->ActuatorCount(); i++)
	{
		const IoActuator* actuator = device->Actuator(i);
		IoBindings* bindings = device->ActuatorBindings(i);

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

void Io::DefaultBindings(IoDevice* device, bool keyboard)
{
	ApplyDefaultBindings(device, keyboard);

	if (device != nullptr)
	{
		device->SaveConfig();
	}
}

void Io::SetBinding(IoDevice* device, int actuator, bool gamepad, int binding)
{
	if (device == nullptr)
	{
		return;
	}

	IoBindings* bindings = device->ActuatorBindings(actuator);

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

void Io::ClearBindings(IoDevice* device)
{
	if (device == nullptr)
	{
		return;
	}

	for (int i = 0; i < device->ActuatorCount(); i++)
	{
		IoBindings* bindings = device->ActuatorBindings(i);

		if (bindings != nullptr)
		{
			bindings->keyboard = 0;
			bindings->gamepad = 0;
		}
	}

	device->SaveConfig();
}

bool Io::FirstOfModel(IoDevice* device)
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

int Io::DeviceIndexOnPort(int port)
{
	if (port < 0 || port >= IO_PORT_MAX)
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

void Io::Plug(int index, int port)
{
	lock.Lock();

	if (index >= 0 && index < (int)devices.size())
	{
		ports[index] = port;
	}

	lock.Unlock();

	IoDevice* device = Device(index);

	if (device != nullptr)
	{
		device->Attach(port);
	}
}

IoDevice* Io::DeviceOnPort(int port)
{
	return Device(DeviceIndexOnPort(port));
}

bool Io::Attach(int index, int port)
{
	IoDevice* device = Device(index);

	if (device == nullptr || port < 0 || port >= IO_PORT_MAX)
	{
		return false;
	}

	// A device belongs to a bus: a pad cannot be plugged into a card slot.
	IoDevice* existing = DeviceOnPort(port);

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

void Io::Detach(int index)
{
	IoDevice* device = Device(index);

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

int Io::PortOf(int index)
{
	lock.Lock();
	int port = (index >= 0 && index < (int)ports.size()) ? ports[index] : -1;
	lock.Unlock();

	return port;
}

// ---------------------------------------------------------------------------
// The emulation

bool Io::PollSI(int chan, PADState* state)
{
	IoDevice* device = DeviceOnPort(IO_PORT_SI(chan));

	if (device == nullptr)
	{
		return false;
	}

	// The bindings are resolved here, on the emulation thread: the device is handed the value of
	// every actuator and turns them into its protocol by itself.
	int pad = HostPadOfPort(IO_PORT_SI(chan));

	for (int i = 0; i < device->ActuatorCount(); i++)
	{
		const IoActuator* actuator = device->Actuator(i);
		IoBindings* bindings = device->ActuatorBindings(i);

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

bool Io::TransferSI(int chan, int outlen, int inlen, uint8_t* buf)
{
	IoDevice* device = DeviceOnPort(IO_PORT_SI(chan));

	// Nothing is plugged into the channel: the transfer is not answered at all, which is how the
	// guest finds out that there is no controller (SISR[NOREP], latched by the caller).
	if (device == nullptr)
	{
		return false;
	}

	device->Transfer(outlen, inlen, buf);

	return true;
}

void Io::TransferEXI(int chan, int sel, Flipper::ExternalInterface* exi)
{
	// The devices of a channel are told apart by the chip select the transfer selects (see
	// exi.cpp). The two on-chip devices the console owns - the boot ROM / RTC / SRAM chip on
	// channel 0 CS1B and the AD16 debugger of channel 2 - are not devices of the pool and are served by the
	// channel itself; every other combination is a socket of the pool:
	//
	//   EXI0 CS0B  Slot A          (a memory card)
	//   EXI1 CS0B  Slot B          (a memory card)
	//   EXI0 CS2B  serial port 1   (the broadband adapter and the modem adapter)
	int port = -1;

	if (sel == 0 && chan == 0) port = IO_PORT_SLOTA;
	else if (sel == 0 && chan == 1) port = IO_PORT_SLOTB;
	else if (sel == 2 && chan == 0) port = IO_PORT_SERIAL1;

	if (port < 0)
	{
		return;
	}

	IoDevice* device = DeviceOnPort(port);

	if (device == nullptr)
	{
		return;
	}

	// The transfer that begins a command sequence - the one the console makes right after it
	// asserts the chip select - is the command word, and every transfer after it is data. The
	// channel's flag says which one this is (see exi.cpp); the device is the one that knows what
	// its own commands mean, so the flag is passed on and cleared here.
	bool first = exi->exi.firstImm;
	exi->exi.firstImm = false;

	device->ExiTransfer(exi, first);
}

bool Io::SetMotorSI(int chan, int cmd)
{
	IoDevice* device = DeviceOnPort(IO_PORT_SI(chan));

	if (device == nullptr)
	{
		return false;
	}

	return device->SetMotor(cmd);
}
