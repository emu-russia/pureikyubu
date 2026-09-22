# Peripheral devices

The GameCube reaches its peripherals through two different buses, and every peripheral is a device
of its own. The emulation of them used to be spread over the modules that happened to touch them:
`si.cpp` knew the answer bytes of the controller, `padsdl.cpp` knew both the SDL side and the way a
pad is polled, `memcard.cpp` mixed the card format with the EXI lifecycle, and the settings of a
device were kept wherever its consumer was. The subsystem described here is the single place a
peripheral lives, and `uisettings.cpp` is the single place its settings are edited.

## The pieces

| Piece | What it is |
|---|---|
| **DeviceID** | What a device model is, in one number: `0x00010001` is the standard controller (DOL-003), `0x00020001` is a memory card. A model is registered by the module that implements it (`Peripherals::RegisterFactory`), so the subsystem does not know what a pad or a card is, and a build that does not compile that module simply has no such devices |
| **Port** | A socket of the console: the four SI channels, the two memory card slots. A port belongs to a bus (SI or EXI), and a device can only be plugged into a port of its own bus |
| **Pool** | The devices the user has, in the order the configuration lists them. Every device owns the variables of its own entry: its model, its name, the port it is plugged into and its own settings |
| **Actuator** | One control of a device (`PAD_ACT_*` in cont.h for a pad). A device publishes its actuators and the settings window draws one row per actuator |
| **Binding** | What drives an actuator: a keyboard key and a host game controller button or axis |
| **HostInput** | The host side of the bindings, and the host controls a device drives back (the rumble motor). Implemented by the front end (`padsdl.cpp` over SDL2, `padnull.cpp` for the headless build) and by the unit tests |

The device implementations are modules of their own: the standard controller is `cont.cpp` (the
Joybus command set, the analog channels, the rumble motor and the actuator table) and the memory
card is `memcard.cpp` (the card format and the flash protocol it already owned).

## Who calls whom

The emulation calls a device, never the other way around:

* the SI poll of a channel (`SerialInterface::SIPoll`) asks the pool for the device in that socket.
  The pool resolves the bindings against the host input, hands the device the value of every
  actuator (`SetState`) and takes its per-channel state (`Poll`), which the SI register file packs
  into `SICnINBUFH`/`SICnINBUFL` the way the hardware does;
* a communication transfer (`SerialInterface::SICommand`) hands the device the command bytes of the
  channel and takes its answer back (`Transfer`): 0x00 is the identification, 0x41 the calibration
  origins, and the motor is programmed by the payload of 0x40;
* an EXI transfer of a card slot (`ExternalInterface::exi_write_cr`) is dispatched to the card that
  is in the slot (`ExiTransfer`), which runs the flash protocol;
* the rumble motor of a pad reaches the host game controller through `HostInput::Rumble`.

A device never makes an SDL call, and the front end never knows what a pad is: that split is what
the backend used to lack.

## The configuration

The pool is the `Devices` list of the `peripherals` section: one entry per device, in the order the
pool has them, which is why a device is addressed by its index and not by a number in the name of a
variable (see the array accessors in `config.h`).

```json
"peripherals":
{
    "Devices":
    [
        { },
        { "Type": 65537, "Name": "Controller 1", "Port": 0, "VKEY_FOR_A": 27, "GCKEY_FOR_A": 65536 }
    ]
}
```

* an entry **without** `Type` is a slot that was never configured, and the slot of one of the
  console's ports holds the device that port is meant for. That is why a fresh configuration needs
  no list at all: the console comes up with four pads and two cards, and the list appears the first
  time the user changes something;
* an entry whose `Type` is `0` is a device that was taken out of the pool. Its slot stays empty, so
  that the index of every other device (and of the variables of its configuration) does not change;
* a configuration written before the pool existed is not migrated: it is *read* through the old
  names, so that the emulator comes up with the same controllers and cards as the build that wrote
  it. The four sockets are then `PluggedIn_<n>` from the `controllers` section, their bindings are
  `VKEY_FOR_*_<n>` / `GCKEY_FOR_*_<n>`, and the card slots are `MemcardA_*` / `MemcardB_*` from
  `memcards`. `Attach`/`Detach` keeps those in step as well, so a configuration this build writes is
  still understood by a build that predates the pool.

## The lifetime

The pool is opened with the **emulator**, not with the machine: the settings window edits the pool
with no game loaded, and the devices are plugged into the sockets of the console, which exist
whether or not a machine is running (`Peripherals::Open`, called by `EMUCtor`). It is closed by
`EMUDtor`.

The machine matters to a device that talks to it - a memory card is read through an EXI channel - so
the pool is told when one appears and when one goes away: `MachineOpened` (from the `Flipper`
constructor) puts the cards into their slots, and `MachineClosed` (from the destructor) takes them
out and flushes the files they were writing. The pool keeps the devices and the ports they are in,
so the next machine finds them where the user put them.

A device is never destroyed while the emulator runs: the emulation thread takes one out of the pool
(to poll a pad, to run a card transfer) while the settings window may be reconfiguring it. A device
that is removed from the pool is kept alive (with its slot marked empty) until the pool itself is
closed in one piece.

## The settings window

Every setting of the emulator is in one window with a vertical strip of tabs on the left and a
property grid on the right (`uisettings.cpp`): the tabs are "General" (the game selector),
"GCN Hardware" (the console version and the firmware), "Controllers" (the devices of the pool on the
SI bus and their bindings) and "Memory Cards" (the devices on the EXI bus). The window is generic
over the devices: it asks a device for its actuators and its properties and draws them, so a new
device model appears in it without a line of front end code.

The dialog that each of those pages replaced - `Options -> Controllers -> Port n`,
`Options -> Memcards -> Slot A`, and the settings that had no dialog at all (the selector view) -
are gone from the menu, which now carries a single `Options -> Settings...` item.

A setting takes effect as it is edited (a binding is used by the next poll, a card file is opened at
once), and the pool writes the device back itself (`SetBinding`, `DefaultBindings`, `ClearBindings`,
`AddDevice`), so the window never has to remember to save anything: it has no OK/Cancel either.

## Tests

`testing/peripherals_test.cpp` drives a pad the way a user does - it binds a control to a host
control of the test input double, says that the host reports it, polls the channel and reads the
answer out of the SI registers - so the whole chain (binding, actuator, the device's state, the
register file) is covered. It also covers the pool, the buses, the motor, the origins, and that a
device which is added (with its name and its bindings) is still there after the emulator was closed
and started again. The SI tests (`testing/gfx_si_spec_test.cpp`) cover the interface block itself;
their machine plugs a pad into every socket for the same reason.
