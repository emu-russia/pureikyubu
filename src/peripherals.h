/*

# Peripheral devices

The GameCube reaches its peripherals through two different buses, and every peripheral is a device
of its own: the standard controller is an intelligent Joybus device that answers a command set, the
memory card is a flash card that speaks an EXI command protocol, and both of them are plugged into a
socket of the console (one of the four SI channels for a pad, one of the two memory card slots for a
card). Emulating them used to be spread over the modules that happened to touch them: `si.cpp` knew
the controller's answer bytes, `padsdl.cpp` knew both the SDL side and the way a pad is polled,
`memcard.cpp` mixed the card format with the EXI lifecycle, and the configuration of a device was
kept wherever its consumer was.

This module is the subsystem: the pool of devices, the ports they are plugged into, the bindings
that drive them and the dispatch of the emulation to the device of a port. What it does not contain
is a device:

  * a device is identified by its **DeviceID** (`PERIPH_DEVICE_*`), which fully defines its model
    and its implementation;
  * the devices the user has are kept in a **pool**; each one carries its own name, its own settings
    and the port it is plugged into, and the settings window edits them one by one;
  * a device publishes its **actuators** - the controls the host drives - and the host binds a
    keyboard key and a game controller button or axis to each of them;
  * the emulation calls the device: the SI poll asks the pad on a channel for its state, an EXI
    transfer asks the card on a slot to run its side of the protocol.

The device implementation knows nothing about the host: it never calls SDL. The host is a
`HostInput` (see below), implemented by the front end (`padsdl.cpp` over SDL2, `padnull.cpp` for the
headless build) and by the unit tests, and the bindings are resolved against it. That is the split
the backend used to lack: the SDL calls are on one side of the interface, the emulated device on the
other.

A device implementation lives next to the hardware it speaks to and registers its factory with
`RegisterFactory` from a constructor of its own, so that a build which does not compile it simply
has no such devices:

  * the standard controller is `cont.cpp` (with `cont.h`, which names its actuators);
  * the memory card is `memcard.cpp`, with the card format and the flash protocol it already owned.

*/

#pragma once

// ---------------------------------------------------------------------------
// Device models

//! Nothing is plugged into the port.
#define PERIPH_DEVICE_NONE              0x00000000

//! DOL-003, the standard controller: a Joybus device on one of the four SI channels.
#define PERIPH_DEVICE_STANDARD_PAD      0x00010001

//! DOL-008 / DOL-014 / DOL-020, the memory card: an EXI device on the CS0B line of a card slot.
#define PERIPH_DEVICE_MEMCARD           0x00020001

// The buses a device is plugged into. A device can only be attached to a port of its own bus.
#define PERIPH_BUS_SI                   0
#define PERIPH_BUS_EXI                  1

// ---------------------------------------------------------------------------
// Ports

//! The sockets of the console. The order is the order the settings dialog shows them in.
enum
{
	PERIPH_PORT_SI0 = 0,        //!< the four controller sockets (SI channels 0..3)
	PERIPH_PORT_SI1,
	PERIPH_PORT_SI2,
	PERIPH_PORT_SI3,
	PERIPH_PORT_SLOTA,          //!< the two memory card slots (EXI0 CS0B / EXI1 CS0B)
	PERIPH_PORT_SLOTB,

	PERIPH_PORT_MAX
};

//! The port of an SI channel (0..3).
inline int PERIPH_PORT_SI(int chan) { return PERIPH_PORT_SI0 + chan; }

//! The port of a memory card slot (see MEMCARD_SLOTA / MEMCARD_SLOTB).
inline int PERIPH_PORT_SLOT(int slot) { return PERIPH_PORT_SLOTA + slot; }

// ---------------------------------------------------------------------------
// Actuators
//
// An actuator is one control of a device. The host drives it with a value from 0 to `max` (0 means
// "released" or "in the middle", the device says which), and the device turns the values of all of
// its actuators into whatever its bus protocol wants to see.
//
// The standard controller is a pad driven by a keyboard and a game controller, so its actuators
// include the half and full deflection of each stick direction: a key can only say "pressed", and
// the two keys of a direction are what gives a key-driven stick its magnitude. A device that is
// driven by an analog host control alone needs no such pair - it publishes one actuator per axis
// and the binding scales the host axis to the actuator's range.

struct PeriphActuator
{
	const char* group;      //!< the group the settings dialog shows the control under
	const char* name;       //!< the name of the control
	const char* id;         //!< the short name of the control in the configuration ("XUP100")
	int         max;        //!< the value a fully deflected / pressed control has (1 for a button)
};

// ---------------------------------------------------------------------------
// Bindings
//
// A binding is one integer per host control, exactly as it is stored in the configuration: either
// a keyboard key or a host game controller button / axis with the direction that drives the
// control. The host backend turns a binding into a name and into a value; the emulator only
// carries the integer around.

#define PERIPH_HOST_BUTTON_BASE     0x00010000
#define PERIPH_HOST_AXIS_BASE       0x00020000

#define PERIPH_HOST_IS_BUTTON(v)    ((v) >= PERIPH_HOST_BUTTON_BASE && (v) < PERIPH_HOST_AXIS_BASE)
#define PERIPH_HOST_IS_AXIS(v)      ((v) >= PERIPH_HOST_AXIS_BASE)

#define PERIPH_HOST_BUTTON(v)       ((v) - PERIPH_HOST_BUTTON_BASE)
#define PERIPH_HOST_AXIS(v)         (((v) - PERIPH_HOST_AXIS_BASE) >> 1)
#define PERIPH_HOST_AXIS_POS(v)     (((v) & 1) != 0)        // 1: positive direction, 0: negative

#define PERIPH_HOST_MAKE_BUTTON(b)      (PERIPH_HOST_BUTTON_BASE + (b))
#define PERIPH_HOST_MAKE_AXIS(a, pos)   (PERIPH_HOST_AXIS_BASE + ((a) << 1) + ((pos) ? 1 : 0))

//! A keyboard binding is a host scancode and a game controller binding is one of the PERIPH_HOST_*
//! values; 0 means "not assigned" in both cases.
struct PeriphBindings
{
	int keyboard;       //!< the host keyboard scancode, 0: not assigned
	int gamepad;        //!< the host game controller control (PERIPH_HOST_*), 0: not assigned
};

// ---------------------------------------------------------------------------
// The state of a Joybus device, as the SI register file exposes it
//
// (This is the emulator's own per-channel state, not the wire format: the SI register file packs it
// into SICnINBUFH/L the way the hardware does, and the guest library reads it back out.)

struct PADState
{
	uint16_t    button;         //!< combination of PAD_BUTTON*
	int8_t      stickX;         //!< -127...127, 0: centred
	int8_t      stickY;
	int8_t      substickX;      //!< -127...127, 0: centred
	int8_t      substickY;
	uint8_t     triggerLeft;    //!< 0...255
	uint8_t     triggerRight;
};

// The controller buttons, in the bit positions the guest library uses (see PAD_BUTTON_* in
// dolphin/pad.h and the response byte layout of the standard controller).
#define PAD_BUTTON_LEFT         (0x0001)
#define PAD_BUTTON_RIGHT        (0x0002)
#define PAD_BUTTON_DOWN         (0x0004)
#define PAD_BUTTON_UP           (0x0008)
#define PAD_TRIGGER_Z           (0x0010)
#define PAD_TRIGGER_R           (0x0020)
#define PAD_TRIGGER_L           (0x0040)
#define PAD_BUTTON_A            (0x0100)
#define PAD_BUTTON_B            (0x0200)
#define PAD_BUTTON_X            (0x0400)
#define PAD_BUTTON_Y            (0x0800)
#define PAD_BUTTON_START        (0x1000)

// The rumble motor commands (the two-bit motor state of the controller's status byte).
#define PAD_MOTOR_STOP          0
#define PAD_MOTOR_RUMBLE        1
#define PAD_MOTOR_STOP_HARD     2

// ---------------------------------------------------------------------------
// Device properties
//
// Everything a device wants the settings dialog to show about it that is not an actuator. The
// value is a string; the editor is picked by the kind, so a device does not need to know how the
// dialog draws it.

#define PERIPH_PROP_BOOL        0       //!< a checkbox ("1" or "0")
#define PERIPH_PROP_FILE        1       //!< a file name, picked with the host's file browser
#define PERIPH_PROP_INFO        2       //!< read only text (a size, a state)

struct PeriphProperty
{
	const char* name;       //!< the name of the property (the left column of the grid)
	int         kind;       //!< PERIPH_PROP_*
	int         id;         //!< the device's own identifier of the property
};

// ---------------------------------------------------------------------------
// A device

class PeripheralDevice
{
protected:
	//! The entry of the pool list that holds the settings of this device (see config.h). The pool
	//! sets it when the device is created and clears it when the device leaves the pool, so a device
	//! implementation reads and writes its own settings through it and never sees the document they
	//! live in.
	ConfigEntry* config = nullptr;

public:
	virtual ~PeripheralDevice() {}

	//! The DeviceID of the model.
	virtual uint32_t Type() = 0;

	//! The configuration of the device, for the pool and for the device itself.
	ConfigEntry* Config() const { return config; }
	void SetConfig(ConfigEntry* entry) { config = entry; }

	// -----------------------------------------------------------------------
	// Actuators (what the host drives)

	//! The number of actuators of the model.
	virtual int ActuatorCount() { return 0; }

	//! The descriptor of an actuator.
	virtual const PeriphActuator* Actuator(int index) { return nullptr; }

	//! The bindings of an actuator. The settings dialog edits them in place.
	virtual PeriphBindings* ActuatorBindings(int index) { return nullptr; }

	//! Set the value of an actuator (0...PeriphActuator::max). The subsystem resolves the bindings
	//! against the host input and calls this before every poll, so the device only ever has to turn
	//! actuator values into its bus protocol.
	virtual void SetState(int actuator, int value) {}

	// -----------------------------------------------------------------------
	// Properties (everything else the settings dialog shows)

	virtual int PropertyCount() { return 0; }
	virtual const PeriphProperty* Property(int index) { return nullptr; }
	virtual std::string GetProperty(int id) { return ""; }
	virtual void SetProperty(int id, const std::string& value) {}

	// -----------------------------------------------------------------------
	// Configuration

	//! Read the device's own settings from its entry. Called by the pool when the device is created.
	virtual void LoadConfig() {}

	//! Write the device's own settings back (the pool does it when the user edits one of them).
	virtual void SaveConfig() {}

	// -----------------------------------------------------------------------
	// The port

	//! The device was plugged into `port`. A card opens its file here, a pad picks up the bindings.
	virtual void Attach(int port) {}

	//! The device was unplugged (a card flushes its file and closes it).
	virtual void Detach() {}

	// -----------------------------------------------------------------------
	// The buses

	//! The SI auto poll: fill the per-channel state the SI register file exposes. Returns false
	//! when the device does not answer at all (the guest then sees "no controller").
	virtual bool Poll(PADState* state) { return false; }

	//! A communication transfer over an SI channel (SICOM). `buf` holds the command and the output
	//! bytes of the transfer and the response is written back into it; a device that does not
	//! answer leaves it alone.
	virtual void Transfer(int outlen, int inlen, uint8_t* buf) {}

	//! A transfer over the EXI channel the device is plugged into. A memory card moves whole blocks
	//! through the channel's DMA path, so the interface hands the device the channel's registers
	//! rather than a flat buffer.
	virtual void ExiTransfer(Flipper::ExternalInterface* exi) {}

	//! The rumble motor command of a Joybus device (PAD_MOTOR_*). Returns false when the model has
	//! no motor.
	virtual bool SetMotor(int cmd) { return false; }
};

// ---------------------------------------------------------------------------
// The host side

//! The host input the bindings are resolved against, and the host controls the devices drive (the
//! rumble motor). The front end implements it: `padsdl.cpp` over SDL2, `padnull.cpp` for the
//! headless build. The emulator only ever sees this interface, so a device implementation can not
//! depend on a host API.
class HostInput
{
public:
	virtual ~HostInput() {}

	//! Keep the host devices open and their cached state fresh. Called once a frame from the thread
	//! that pumps the host's events (the UI thread); the values the emulation thread reads are a
	//! snapshot taken here.
	virtual void Update() {}

	//! The state of a host keyboard key, by its host scancode.
	virtual bool KeyDown(int scancode) { return false; }

	//! The host game controllers, in the order the host reports them.
	virtual int GamepadCount() { return 0; }
	virtual std::string GamepadName(int pad) { return ""; }
	virtual bool GamepadButton(int pad, int button) { return false; }

	//! The deflection of a host game controller axis, in the host's own units (-32768...32767).
	virtual int GamepadAxis(int pad, int axis) { return 0; }

	//! The name of a binding, for the settings dialog.
	virtual std::string BindingName(int binding) { return "..."; }

	//! Apply a rumble command (PAD_MOTOR_*) to a host game controller. Returns false when the host
	//! cannot rumble.
	virtual bool Rumble(int pad, int cmd) { return false; }

	//! The default binding of one actuator of a device model: the keyboard scancode and the host
	//! game controller control of the usual layout of that device (the Xbox-style one for a pad).
	//! Defined by the back end, because a binding names a host control and only the back end knows
	//! how the host numbers them. `actuator` is the PeriphActuator::id of the control; 0 means
	//! "not assigned".
	virtual void DefaultBindings(uint32_t deviceType, const char* actuator, int& keyboard, int& gamepad)
	{
		keyboard = 0;
		gamepad = 0;
	}
};

// ---------------------------------------------------------------------------
// The device configuration
//
// Every device of the pool owns the settings of its own entry of the list (see "the configuration"
// in the module comment): "Type" is its model, "Name" the name the user gave it, "Port" the port it
// is plugged into, and whatever else the device itself keeps ("VKEY_FOR_A", "File"). A device reads
// and writes them through the entry accessors of config.h and the entry of the device, so nothing
// outside this subsystem names a setting of a device.

//! The host game controller a pad in a port is driven by: the host game controller the host reports
//! under the number of its socket (see the device implementations, which use it for the motor).
int HostPadOfPort(int port);

//! The host input of this build. The back end is compiled into the front end (padsdl.cpp in the
//! SDL one, padnull.cpp in the headless one), so the peripheral subsystem creates it when it is
//! opened and destroys it when it is closed.
HostInput* HostInputCreate();
void HostInputDestroy();

// ---------------------------------------------------------------------------
// The subsystem

//! The list of the devices of the pool, in the "peripherals" section of the configuration (see the
//! list accessors in config.h). One entry per device, in the order the pool has them - the order the
//! settings window shows them in, and nothing else: a device is what its entry says, not where the
//! entry happens to be.
#define PERIPH_DEVICES  "Devices"

//! How many devices the console may have in its pool.
#define PERIPH_MAX_DEVICES      16

class Peripherals
{
	//! The devices of the pool, in the order of the list of the configuration.
	std::vector<PeripheralDevice*> devices;

	//! The port of every device (-1: not plugged in).
	std::vector<int> ports;

	//! The names the user gave the devices (empty: the model name).
	std::vector<std::string> names;

	//! The devices that were removed from the pool while the emulator was running. Their object is
	//! kept alive (a thread may have just taken it out of the pool) and destroyed with the pool.
	std::vector<PeripheralDevice*> retired;

	SpinLock lock;

	HostInput* host = nullptr;

	bool opened = false;

	//! Create one device of a model and give it its entry of the pool list.
	PeripheralDevice* CreateDevice(uint32_t type, ConfigEntry* entry);

	//! The index of the device in a port (-1: the port is empty).
	int DeviceIndexOnPort(int port);

	//! The index of a device in the pool (-1 when it is not there).
	int DeviceIndexOf(PeripheralDevice* device);

	//! Plug a device in without writing the configuration (used while the pool is built from it).
	void Plug(int index, int port);

	//! The devices that are plugged in, as (device, port) pairs, without the lock being held while
	//! the caller uses them (the device may do file I/O, and the emulation thread takes the lock for
	//! a lookup only).
	void AttachedDevices(std::vector<std::pair<PeripheralDevice*, int>>& attached);

public:
	static Peripherals& Instance();

	//! Build the pool from the configuration and plug the devices into the ports the configuration
	//! gives them. The emulator calls it once, when it starts (not when a machine is created: the
	//! settings window edits the pool with no game loaded).
	void Open();

	//! Unplug every device (a memory card is flushed and closed) and drop the pool.
	void Close();

	//! The machine the devices are plugged into has been built: a device that talks to it (a memory
	//! card, which is read through an EXI channel) arms itself here. The machine calls it once its
	//! own interfaces exist.
	void MachineOpened();

	//! The machine is being taken apart: the devices are unplugged from it (a card flushes the file
	//! it was writing), and the pool keeps them and the ports they are in.
	void MachineClosed();

	// -----------------------------------------------------------------------
	// The pool

	//! How many devices the pool holds.
	int Count();

	//! The device of a pool index, or nullptr. The index is a position in the list the settings
	//! window shows; the device itself is its own entry of the configuration.
	PeripheralDevice* Device(int index);

	//! Add a device of the given model. Returns its pool index, or -1 when the pool is full or the
	//! model is unknown to this build. The device, its name and its settings are written to the
	//! configuration here: a device that is not in the list is not a device the user has.
	int AddDevice(uint32_t type);

	//! Take a device out of the pool, with the settings it holds.
	void RemoveDevice(int index);

	//! The name of a device as the settings dialog shows it.
	std::string Name(int index);
	void SetName(int index, const std::string& name);

	//! Put the model's default host bindings into every actuator of a device (see
	//! HostInput::DefaultBindings). `keyboard` is the caller's: the keyboard defaults belong to the
	//! first device of a kind, because one set of keys cannot drive the pads of four ports at once.
	//!
	//! The application does not write the device back: this is the layout a device that has never
	//! been configured gets while the pool is built, and building the pool must not change the
	//! configuration.
	void ApplyDefaultBindings(PeripheralDevice* device, bool keyboard);

	//! The same, written back to the configuration (the "Defaults" button of the settings window).
	//! A caller that means "this device, whatever it takes" passes true for `keyboard`.
	void DefaultBindings(PeripheralDevice* device, bool keyboard);

	//! Bind one actuator of a device to a host control and write it back (what the capture of the
	//! settings window does when the input arrives).
	void SetBinding(PeripheralDevice* device, int actuator, bool gamepad, int binding);

	//! Drop every binding of a device and write it back.
	void ClearBindings(PeripheralDevice* device);

	//! Whether a device is the first one of its model in the pool. The keyboard defaults belong to
	//! it: one set of keys cannot drive the pads of four sockets at once.
	bool FirstOfModel(PeripheralDevice* device);

	// -----------------------------------------------------------------------
	// The ports

	//! The port of a device, or -1.
	int PortOf(int index);

	//! The device plugged into a port. Returns nullptr when the port is empty.
	PeripheralDevice* DeviceOnPort(int port);

	//! Plug a device into a port (it is first unplugged from wherever it was). The port has to
	//! match the bus of the device.
	bool Attach(int index, int port);

	//! Unplug a device.
	void Detach(int index);

	//! How many ports the console has.
	int PortCount() { return PERIPH_PORT_MAX; }

	//! The name of a port ("Controller Port 1", "Memory Card Slot A").
	const char* PortName(int port);

	//! The bus of a port (PERIPH_BUS_*).
	int PortBus(int port);

	// -----------------------------------------------------------------------
	// The models

	//! Register a device model. Called by the module that implements the device.
	static void RegisterFactory(uint32_t type, const char* name, const char* info, int bus, PeripheralDevice* (*factory)());

	//! The name of a model ("Standard Controller"), or nullptr when this build has no such device.
	static const char* ModelName(uint32_t type);

	//! The bus a model is plugged into (PERIPH_BUS_*), or -1 when this build has no such device.
	static int ModelBus(uint32_t type);

	//! The model of a port when the configuration does not say (the device the port is meant for).
	static uint32_t DefaultModelOfPort(int port);

	// -----------------------------------------------------------------------
	// The host input

	//! The host input of the build (nullptr when the subsystem is closed).
	HostInput* Host();

	// -----------------------------------------------------------------------
	// The emulation

	//! The SI auto poll of one channel (see si.cpp). Returns false when the channel has no device
	//! that answers (the guest then sees "no controller").
	bool PollSI(int chan, PADState* state);

	//! A communication transfer of one SI channel.
	//! Hand a COM transfer to the device on a channel. Returns false when nothing is plugged into
	//! the channel, in which case nothing answers it (the caller latches the channel's no-response
	//! error, see serial-interface.md 7.1).
	bool TransferSI(int chan, int outlen, int inlen, uint8_t* buf);

	//! A transfer of one EXI channel and chip select (see exi.cpp).
	void TransferEXI(int chan, int sel, Flipper::ExternalInterface* exi);

	//! The rumble motor command of one SI channel.
	bool SetMotorSI(int chan, int cmd);
};

// ---------------------------------------------------------------------------
// The back end helpers the front end calls directly

//! Keep the host input devices open and their state fresh (once a frame, from the UI thread).
void HostInputUpdate();
