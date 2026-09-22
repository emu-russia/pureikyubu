# Peripheral devices

The GameCube reaches its peripherals through two different buses, and every peripheral is a device
of its own. The emulation of them used to be spread over the modules that happened to touch them:
`si.cpp` knew the answer bytes of the controller, `padsdl.cpp` knew both the SDL side and the way a
pad is polled, `memcard.cpp` mixed the card format with the EXI lifecycle, and the settings of a
device were kept wherever its consumer was. The subsystem described here is the single place where
a peripheral lives.

## The pieces

| Piece | What it is |
|---|---|
| **DeviceID** | What a device model is, in one number: `0x00010001` is the standard controller (DOL-003), `0x00020001` is a memory card. A model is registered by the module that implements it, so `peripherals.cpp` does not have to know about the card, and the unit test harness (which does not compile `memcard.cpp`) simply has no cards |
| **Port** | A socket of the console: the four SI channels, the two memory card slots. A port belongs to a bus (SI or EXI), and a device can only be plugged into a port of its own bus |
| **Pool** | The devices the user has. Every device owns the variables `Device<i>_*` of the `peripherals` section of the configuration: its model, its name, the port it is plugged into and its own settings |
| **Actuator** | One control of a device (the breaknes `IOState`). A device publishes its actuators and the settings window draws one row per actuator |
| **Binding** | What drives an actuator: a keyboard key and a host game controller button or axis |
| **HostInput** | The host side of the bindings, and the host controls a device drives back (the rumble motor). Implemented by the front end (`padsdl.cpp` over SDL2, `padnull.cpp` for the headless build) and by the unit tests |

## Who calls whom

The emulation calls a device, never the other way around:

* the SI poll of a channel (`SerialInterface::SIPoll`) asks the pool for the device in that socket.
  The pool resolves the bindings against the host input, hands the device the value of every
  actuator (`SetState`) and takes its per-channel state (`Poll`), which the SI register file packs
  into `SICnINBUFH`/`SICnINBUFL` the way the hardware does;
* a communication transfer (`SerialInterface::SICommand`) hands the device the command bytes of the
  channel and takes its answer back (`Transfer`);
* an EXI transfer of a card slot (`ExternalInterface::exi_write_cr`) is dispatched to the card that
  is in the slot (`ExiTransfer`), which runs the flash protocol of `memcard.cpp`;
* the rumble motor of a pad is programmed by the guest inside the payload of the `0x40` command and
  reaches the host game controller through `HostInput::Rumble`.

A device never makes an SDL call, and the front end never knows what a pad is: that split is what
the backend used to lack.

## The configuration

A configuration that was written before the pool existed has no `Device<i>_*` variables at all, and
it is not migrated: it is *read* through the old names, so that the emulator comes up with the same
controllers and the same cards as the build that wrote it. The four sockets are `PluggedIn_<n>` from
the `controllers` section, their bindings are `VKEY_FOR_*_<n>` / `GCKEY_FOR_*_<n>`, and the two card
slots are `MemcardA_*` / `MemcardB_*` from `memcards`. The first time the settings window writes a
device's own setting, the new name is what is stored and the old one is left alone. An old name is
also kept in step by `Attach`/`Detach`, so that a configuration written by this build is still
understood by a build that predates the pool.

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
once), so the window has no OK/Cancel.

## The pool while the emulator runs

The emulation thread takes a device out of the pool - to poll a pad, to run a card transfer - while
the settings window may be reconfiguring it. A device is therefore never destroyed while the
emulator is running: removing one marks its slot unused, and its object is kept until the pool is
closed in one piece when the machine is taken apart.

## Tests

`testing/peripherals_test.cpp` drives a pad the way a user does - it binds a control to a host
control of the test input double, says that the host reports it, polls the channel and reads the
answer out of the SI registers - so the whole chain (binding, actuator, the device's state, the
register file) is covered. The SI tests (`testing/gfx_si_spec_test.cpp`) cover the interface block
itself; their machine plugs a pad into every socket for the same reason.
