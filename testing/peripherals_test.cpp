// The emulated time base of the tests (dsp_test_support.cpp): a device that runs a machine of its
// own asks the clock how much time has passed since it last did, and a test drives that clock.

// The peripheral subsystem: the device pool, the bindings and the standard controller.
//
// The SI tests (gfx_si_spec_test.cpp) cover the interface block itself: the register window, the
// poll schedule and the read-status interrupt. These tests cover what is plugged into it. The
// machine of the tests starts from the default pool - the four controller sockets with a standard
// controller in every one of them and the two card slots - and from the host input of the tests,
// which is what the bindings of a device are resolved against.
//
// A controller is driven the way a user drives it: a control of the pad is bound to a host control
// (a key, a game controller button or an axis), the host reports it, the channel is polled and the
// answer is read out of the SI input buffer registers. That is the whole chain the subsystem is:
// binding -> actuator -> the device's own state -> PADState -> the SI register file.

#include "pch.h"
#include "gfx_test_common.h"
#include "../src/cont.h"
#include "../src/gba/gba.h"

// The emulated time base of the tests (dsp_test_support.cpp): a device that runs a machine of its
// own asks the clock how much time has passed since it last did, and a test drives that clock.
void DspTestSetGekkoTicks(int64_t ticks);

using namespace GfxUnitTest;

namespace pureikyubutest
{
	TEST_CLASS(PeripheralsTest)
	{
		//! The SI channel registers, from the software point of view (see gfx_si_spec_test.cpp).
		static const uint32_t ChanInH = 0x04;
		static const uint32_t ChanInL = 0x08;
		static const uint32_t ChanStride = 0x0C;
		static const uint32_t PollReg = 0x30;
		static const uint32_t SrReg = 0x38;

		static GfxTestMachine& M()
		{
			GfxTestMachine& m = Machine();
			m.Reset();
			PIClearAssertedInterrupts();
			ClearTestLog();

			TestHostClear();

			return m;
		}

		static void WriteSiWord(uint32_t offset, uint32_t value)
		{
			PIRegWrite(PI_REGSPACE_SI + offset, value >> 16);
			PIRegWrite(PI_REGSPACE_SI + offset + 2, value & 0xffff);
		}

		static uint32_t ReadSiWord(uint32_t offset)
		{
			uint32_t hi = 0, lo = 0;
			PIRegRead(PI_REGSPACE_SI + offset, &hi);
			PIRegRead(PI_REGSPACE_SI + offset + 2, &lo);
			return ((hi & 0xffff) << 16) | (lo & 0xffff);
		}

		//! Poll the channels (the SI issues the poll from the video blank, see SIPoll in si.cpp).
		//! The first step wraps the line counter, so every call starts a frame of its own.
		static void Poll(GfxTestMachine& m, uint32_t channels = 1)
		{
			uint32_t poll = (7u << 16) | (8u << 8);       // X = 7 lines, Y = 8 polls a frame

			for (uint32_t c = 0; c < 4; c++)
			{
				if (channels & (1u << c))
				{
					poll |= 1u << (7 - c);
				}
			}

			WriteSiWord(PollReg, poll);

			m.flipper->si->SIPoll(0);
			m.flipper->si->SIPoll(1);
		}

		//! The buttons of a channel, as SICnINBUFH exposes them (its high halfword).
		static uint16_t Buttons(uint32_t channel)
		{
			return (uint16_t)(ReadSiWord(ChanInH + channel * ChanStride) >> 16);
		}

		//! The main stick of a channel: the low halfword of SICnINBUFH holds stickY in its low byte
		//! and stickX in its high one (see si_inh_lo in si.cpp).
		static int8_t StickY(uint32_t channel)
		{
			return (int8_t)(uint8_t)ReadSiWord(ChanInH + channel * ChanStride);
		}

		static int8_t StickX(uint32_t channel)
		{
			return (int8_t)(uint8_t)(ReadSiWord(ChanInH + channel * ChanStride) >> 8);
		}

		//! The L trigger of a channel, as it sits in SICnINBUFL (see si_inl_lo in si.cpp).
		static uint8_t TriggerLeft(uint32_t channel)
		{
			return (uint8_t)(ReadSiWord(ChanInL + channel * ChanStride) >> 8);
		}

		//! The actuator of a device that has the given id ("A", "XUP100", ...).
		static int ActuatorOf(PeripheralDevice* device, const char* id)
		{
			for (int i = 0; i < device->ActuatorCount(); i++)
			{
				const PeriphActuator* actuator = device->Actuator(i);

				if (actuator != nullptr && strcmp(actuator->id, id) == 0)
				{
					return i;
				}
			}

			return -1;
		}

		//! The pool index of a device.
		static int IndexOf(PeripheralDevice* device)
		{
			for (int i = 0; i < PERIPH_MAX_DEVICES; i++)
			{
				if (Peripherals::Instance().Device(i) == device)
				{
					return i;
				}
			}

			return -1;
		}

		//! Bind a control to a host keyboard key and press the key.
		static void PressKey(PeripheralDevice* device, const char* actuator, int scancode)
		{
			int index = ActuatorOf(device, actuator);
			Assert::IsTrue(index >= 0, Widen(std::string("the pad has no control ") + actuator).c_str());

			device->ActuatorBindings(index)->keyboard = scancode;
			TestHostKey(scancode, true);
		}

		// =========================================================================================
		// The pool
		// =========================================================================================

		// The pool of the test machine is a standard controller in each of the four sockets. (The
		// harness does not compile the memory card - a card registers itself with the pool from
		// memcard.cpp, which is not part of it - so the two card slots are not checked here.)
		TEST_METHOD(Peripherals_EveryControllerPortHoldsAPad)
		{
			M();
			Peripherals& pool = Peripherals::Instance();

			for (int chan = 0; chan < 4; chan++)
			{
				int port = PERIPH_PORT_SI(chan);
				PeripheralDevice* device = pool.DeviceOnPort(port);

				Assert::IsNotNull(device, Widen("the pool has a pad for " + std::string(pool.PortName(port))).c_str());
				Assert::AreEqual<uint32_t>(PERIPH_DEVICE_STANDARD_PAD, device->Type(),
					Widen(std::string("the model of the device in ") + pool.PortName(port)).c_str());
			}
		}

		// A device belongs to a bus, and a port holds one device at a time: a pad is refused by a
		// card slot and by a socket that is already taken, and nothing is disturbed by the attempt.
		TEST_METHOD(Peripherals_ADeviceIsOnlyPluggedIntoItsOwnBus)
		{
			M();
			Peripherals& pool = Peripherals::Instance();

			PeripheralDevice* first = pool.DeviceOnPort(PERIPH_PORT_SI(0));
			PeripheralDevice* second = pool.DeviceOnPort(PERIPH_PORT_SI(1));

			Assert::IsNotNull(first, L"the first socket holds a pad");
			Assert::IsNotNull(second, L"the second socket holds a pad");

			Assert::IsFalse(pool.Attach(IndexOf(first), PERIPH_PORT_SLOTA), L"a pad cannot go into a card slot");
			Assert::IsFalse(pool.Attach(IndexOf(first), PERIPH_PORT_SI(1)), L"a socket holds one device at a time");

			Assert::IsTrue(first == pool.DeviceOnPort(PERIPH_PORT_SI(0)), L"the first pad is still in its socket");
			Assert::IsTrue(second == pool.DeviceOnPort(PERIPH_PORT_SI(1)), L"the second one is still in its socket");
		}

		// Unplugging a device keeps it in the pool (with its settings) and plugging it back in puts
		// it where it was asked to be.
		TEST_METHOD(Peripherals_UnpluggingAndPluggingBackIn)
		{
			M();
			Peripherals& pool = Peripherals::Instance();

			PeripheralDevice* pad = pool.DeviceOnPort(PERIPH_PORT_SI(0));
			int index = IndexOf(pad);

			Assert::IsTrue(index >= 0, L"the pad is in the pool");

			pool.Detach(index);
			Assert::IsNull(pool.DeviceOnPort(PERIPH_PORT_SI(0)), L"the socket is empty");
			Assert::IsTrue(pad == pool.Device(index), L"the pad is still in the pool");

			Assert::IsTrue(pool.Attach(index, PERIPH_PORT_SI(0)), L"the pad goes back");
			Assert::IsTrue(pad == pool.DeviceOnPort(PERIPH_PORT_SI(0)), L"and it is the same pad");
		}

		// A device that is added to the pool is not plugged into anything (it is up to the user),
		// and a device that is removed leaves the pool.
		TEST_METHOD(Peripherals_AddingAndRemovingADevice)
		{
			M();
			Peripherals& pool = Peripherals::Instance();

			int index = pool.AddDevice(PERIPH_DEVICE_STANDARD_PAD);

			Assert::IsTrue(index >= 0, L"the pool takes another pad");
			Assert::IsNotNull(pool.Device(index), L"the new pad is in the pool");
			Assert::AreEqual(-1, pool.PortOf(index), L"a new device is not plugged in");
			Assert::IsTrue(pool.Name(index) == "Standard Controller", L"it is named after its model");

			pool.RemoveDevice(index);
			Assert::IsNull(pool.Device(index), L"the pad left the pool");
		}

		// The default bindings of a model come from the host back end, and the keys of a pad belong
		// to the first pad of the pool only: one set of keys cannot drive four sockets at once.
		TEST_METHOD(Peripherals_DefaultBindingsAndTheFirstPadRule)
		{
			M();
			Peripherals& pool = Peripherals::Instance();
			int index = pool.AddDevice(PERIPH_DEVICE_STANDARD_PAD);
			PeripheralDevice* pad = pool.Device(index);

			int a = ActuatorOf(pad, "A");
			Assert::IsTrue(a >= 0, L"the pad has the A control");

			// The pad that was added is not the first of its model, so it got the game controller
			// control of A but no key (see StandardPad::LoadConfig).
			Assert::AreEqual(PERIPH_HOST_MAKE_BUTTON(0), pad->ActuatorBindings(a)->gamepad, L"the game controller of A");
			Assert::AreEqual(0, pad->ActuatorBindings(a)->keyboard, L"the next pad has no key of its own");

			// As the first pad of its model it gets the key too.
			pool.DefaultBindings(pad, true);
			Assert::AreEqual(TestDefaultKey, pad->ActuatorBindings(a)->keyboard, L"the key of the first pad");

			pool.ClearBindings(pad);
			Assert::AreEqual(0, pad->ActuatorBindings(a)->keyboard, L"the bindings were dropped");
			Assert::AreEqual(0, pad->ActuatorBindings(a)->gamepad, L"both of them");

			pool.RemoveDevice(index);
		}

		// The pool is what the settings window edits, and it is the configuration: a device that is
		// added, the name it is given and its bindings are all there after the emulator was closed
		// and started again (which is what the settings window of a later run reads).
		TEST_METHOD(Peripherals_ThePoolSurvivesARestart)
		{
			GfxTestMachine& m = M();
			Peripherals& pool = Peripherals::Instance();

			int index = pool.AddDevice(PERIPH_DEVICE_STANDARD_PAD);
			Assert::IsTrue(index >= 0, L"the pool takes another pad");

			pool.SetName(index, "Test Pad");

			PeripheralDevice* pad = pool.Device(index);
			int a = ActuatorOf(pad, "A");
			pool.SetBinding(pad, a, false, 0x1234);       // as the capture of the settings window does it
			Assert::IsTrue(ActuatorOf(pad, "A") == PAD_ACT_A, L"the actuators are in the order of cont.h");

			// The emulator is closed and started again.
			pool.Close();
			pool.Open();

			PeripheralDevice* again = pool.Device(index);
			Assert::IsNotNull(again, L"the added pad is in the pool again");
			Assert::IsTrue(pool.Name(index) == "Test Pad", L"with the name it was given");
			Assert::AreEqual(0x1234, again->ActuatorBindings(ActuatorOf(again, "A"))->keyboard,
				L"and with the binding it was given");

			pool.RemoveDevice(index);
		}

		// "Defaults" of the settings window means "this device": both the keys and the game controller,
		// even for a pad that is not the first one of the pool.
		TEST_METHOD(Peripherals_TheDefaultsButtonFillsEveryControl)
		{
			M();
			Peripherals& pool = Peripherals::Instance();

			int index = pool.AddDevice(PERIPH_DEVICE_STANDARD_PAD);
			PeripheralDevice* pad = pool.Device(index);

			pool.ClearBindings(pad);
			pool.DefaultBindings(pad, true);

			int a = ActuatorOf(pad, "A");
			Assert::AreEqual(PERIPH_HOST_MAKE_BUTTON(0), pad->ActuatorBindings(a)->gamepad, L"the game controller of A");
			Assert::AreEqual(TestDefaultKey, pad->ActuatorBindings(a)->keyboard, L"and its key");

			// The device's own settings were written back with it.
			int keyboard = pad->ActuatorBindings(a)->keyboard;
			pool.Close();
			pool.Open();

			Assert::AreEqual(keyboard, pool.Device(index)->ActuatorBindings(ActuatorOf(pool.Device(index), "A"))->keyboard,
				L"the defaults are in the configuration");

			pool.RemoveDevice(index);
		}

		// The configuration is JSON, and a device's socket is -1 when it is unplugged, so a negative
		// value has to come back out of the file as the same number. It used to be written as the
		// unsigned bit pattern (18446744073709551615 for -1), which the reader - whose positive range
		// stops at INT64_MAX - then clamped to 9223372036854775807: the emulator read *that* back as
		// -1 through a cast to `int`, so nothing inside it misbehaved, and that is exactly why the
		// file said something different from what it meant for everyone else.
		TEST_METHOD(Settings_ANegativeValueSurvivesTheFile)
		{
			Json written;
			Json::Value* root = written.root.AddObject("peripherals");
			Assert::IsNotNull(root, L"the document has a section");
			root->AddInt("port", -1);
			root->AddInt("port2", 5);

			size_t size = 0;
			written.GetSerializedTextSize(nullptr, (size_t)-1, size);
			Assert::IsTrue(size > 0, L"the document measures");

			std::string text(size, '\0');
			size_t actual = 0;
			written.Serialize(&text[0], text.size(), actual);

			Assert::IsTrue(text.find("-1") != std::string::npos,
				Widen("the file says -1, not its unsigned bit pattern: " + text).c_str());

			Json read;
			read.Deserialize(&text[0], text.size());
			Json::Value* section = read.root.children.back();
			Assert::IsNotNull(section, L"the document reads back");
			Assert::AreEqual(-1, (int)section->ByName("port")->value.AsInt, L"and the value with it");
			Assert::AreEqual(5, (int)section->ByName("port2")->value.AsInt, L"as do the others");
		}

		// A socket holds one device, and the configuration has to agree with that. The shipped
		// defaults name no port for the pads and cards at all (the pool places them where the console
		// expects the model), so a configuration that then gives a socket to another device names a
		// port that is already taken - and until this was handled the newcomer reported that it had
		// attached while the pool went on dispatching the socket to the device that was placed first:
		// a device that never answers, with nothing outside to show which of the two is in the socket.
		// The entry that names the port is the one that gets it.
		TEST_METHOD(Peripherals_AnEntryThatNamesAPortTakesItFromThePlacedDevice)
		{
			M();
			Peripherals& pool = Peripherals::Instance();

			// A device whose entry names no port, which is how the shipped defaults leave the models
			// that have a socket of their own: the pool places it.
			int placed = pool.AddDevice(PERIPH_DEVICE_MODEM);
			Assert::IsTrue(placed >= 0, L"the pool takes a modem adapter");

			pool.Close();
			pool.Open();

			int port = pool.PortOf(placed);

			Assert::IsTrue(port >= 0, L"an entry without a port is placed in the socket of its model");

			// A second device names that socket in its own entry, as the settings window writes it when
			// the user puts a model into a socket the first device is already in.
			int named = pool.AddDevice(PERIPH_DEVICE_BBA);
			Assert::IsTrue(named >= 0, L"the pool takes a broadband adapter");
			SetConfigEntryInt(pool.Device(named)->Config(), "Port", port);

			pool.Close();
			pool.Open();

			Assert::IsTrue(pool.DeviceOnPort(port) == pool.Device(named),
				L"the socket dispatches to the device that named it");
			Assert::IsTrue(GetConfigEntryInt(pool.Device(placed)->Config(), "Port", 0) == -1,
				L"and the device that was placed there was unplugged in the configuration too");

			pool.RemoveDevice(named);
			pool.RemoveDevice(placed);
		}

		// The settings of the devices the peripheral work added are properties like any other (see
		// peripherals.h), so the settings window edits them, the configuration keeps them and a
		// restart brings them back: what the window shows after a restart has to be what was set
		// before it. The two network adapters share a socket, so the test also pins that they do
		// not overwrite each other's entries.
		TEST_METHOD(Peripherals_TheNetworkAndPlayerSettingsSurviveARestart)
		{
			M();
			Peripherals& pool = Peripherals::Instance();

			int adapter = pool.AddDevice(PERIPH_DEVICE_BBA);
			Assert::IsTrue(adapter >= 0 && pool.Attach(adapter, PERIPH_PORT_SERIAL1),
				L"a broadband adapter is in the socket");
			int player = pool.AddDevice(PERIPH_DEVICE_GBPLAYER);
			Assert::IsTrue(player >= 0 && pool.Attach(player, PERIPH_PORT_HSP),
				L"and a Game Boy Player is on the high-speed port");

			// The MAC address is the adapter's text property; the link and the "run the Game Pak"
			// switch are the booleans.
			pool.Device(adapter)->SetProperty(0, "00:11:22:33:44:55");
			pool.Device(adapter)->SetProperty(1, "0");
			pool.Device(player)->SetProperty(2, "0");

			pool.Close();
			pool.Open();

			PeripheralDevice* adapterAgain = pool.Device(adapter);
			PeripheralDevice* playerAgain = pool.Device(player);

			Assert::IsNotNull(adapterAgain, L"the adapter is in the pool again");
			Assert::IsNotNull(playerAgain, L"and the Player");
			Assert::IsTrue(adapterAgain->GetProperty(0) == "00:11:22:33:44:55",
				L"the adapter kept its address");
			Assert::IsTrue(adapterAgain->GetProperty(1) == "0", L"and its link setting");
			Assert::IsTrue(playerAgain->GetProperty(2) == "0", L"the Player kept its switch");

			// The two entries are separate ones of the configuration, so the adapter's address did
			// not leak into the Player's entry (they share a bus).
			Assert::IsTrue(playerAgain->GetProperty(0).find("00:11:22:33:44:55") == std::string::npos,
				L"the Player's own path is not the adapter's value");

			pool.RemoveDevice(player);
			pool.RemoveDevice(adapter);
		}

		// =========================================================================================
		// The standard controller
		// =========================================================================================

		// A pad with nothing pressed reports nothing.
		TEST_METHOD(Pad_ANeutralPadReportsNoButton)
		{
			GfxTestMachine& m = M();

			Poll(m);

			Assert::AreEqual<uint16_t>(0, Buttons(0), L"no button is pressed");
			Assert::AreEqual<int>(0, (int)StickX(0), L"the stick is centred");
			Assert::AreEqual<int>(0, (int)StickY(0), L"both axes of it");
			Assert::AreEqual<int>(0, (int)TriggerLeft(0), L"the trigger is released");
		}

		// The buttons of a pad are the values of its digital controls: a bound key that is down
		// reports its button, and the poll of a channel is what puts them in the SI input buffer.
		TEST_METHOD(Pad_AKeyPressReachesTheSiInputBuffer)
		{
			GfxTestMachine& m = M();
			PeripheralDevice* pad = Peripherals::Instance().DeviceOnPort(PERIPH_PORT_SI(0));

			PressKey(pad, "A", 0x1000);
			PressKey(pad, "START", 0x1001);

			Poll(m, (1u << 0) | (1u << 1));

			Assert::AreEqual<uint16_t>(PAD_BUTTON_A | PAD_BUTTON_START, Buttons(0),
				L"the keys of A and Start are what the channel reports");

			// The bindings belong to the device, not to the channel: the pad of the next socket has
			// no key of its own and stays neutral.
			Assert::AreEqual<uint16_t>(0, Buttons(1), L"the next pad is not driven by these keys");
		}

		// The digital L and R are only reported when their analog control is pressed all the way
		// down, which is what the pad does (see StandardPad::Poll).
		TEST_METHOD(Pad_TheDigitalTriggersFollowTheAnalogOnes)
		{
			GfxTestMachine& m = M();
			PeripheralDevice* pad = Peripherals::Instance().DeviceOnPort(PERIPH_PORT_SI(0));

			PressKey(pad, "TRIGGERL", 0x2000);
			Poll(m);

			Assert::AreEqual<uint16_t>(PAD_TRIGGER_L, Buttons(0), L"the trigger is reported");
			Assert::AreEqual<int>(255, (int)TriggerLeft(0), L"the analog channel of L is at its top");
		}

		// A game controller axis drives the same controls: its deflection is scaled into the range
		// of the direction it was bound to.
		TEST_METHOD(Pad_AGameControllerAxisDrivesTheStick)
		{
			GfxTestMachine& m = M();
			PeripheralDevice* pad = Peripherals::Instance().DeviceOnPort(PERIPH_PORT_SI(0));

			// The pad of Port 1 is driven by the first host game controller (see HostPadOf).
			TestHostGamepad(0, true);

			int index = ActuatorOf(pad, "XRIGHT100");
			Assert::IsTrue(index >= 0, L"the pad has the right direction of the main stick");
			pad->ActuatorBindings(index)->gamepad = PERIPH_HOST_MAKE_AXIS(0, true);

			TestHostGamepadAxis(0, 0, 32767);
			Poll(m);

			Assert::AreEqual<int>(127, (int)StickX(0), L"the stick is at its right stop");
			Assert::AreEqual<int>(0, (int)StickY(0), L"the other axis is not moved");

			TestHostGamepadAxis(0, 0, 16384);
			Poll(m);

			Assert::AreEqual<int>(63, (int)StickX(0), L"half a deflection is half the stick");
		}

		// A key that drives half a deflection moves the stick by half of its travel, and the two
		// controls of one direction do not add up - the larger of them is what the stick sees.
		TEST_METHOD(Pad_TheHalfAndTheFullControlOfADirectionDoNotAddUp)
		{
			GfxTestMachine& m = M();
			PeripheralDevice* pad = Peripherals::Instance().DeviceOnPort(PERIPH_PORT_SI(0));

			PressKey(pad, "XUP50", 0x3000);
			Poll(m);

			Assert::AreEqual<int>(63, (int)StickY(0), L"the half control moves the stick by half");

			PressKey(pad, "XUP100", 0x3001);
			Poll(m);

			Assert::AreEqual<int>(127, (int)StickY(0), L"the full control is what the stick sees");
		}

		// The rumble motor is programmed by a communication transfer (command 0x40 with the motor
		// state in its payload), it reaches the host game controller, and its state is reported back
		// in the status byte of command 0x00.
		TEST_METHOD(Pad_TheMotorIsProgrammedAndReportedBack)
		{
			M();
			PeripheralDevice* pad = Peripherals::Instance().DeviceOnPort(PERIPH_PORT_SI(0));

			uint8_t buf[8] = { 0 };
			buf[0] = 0x00;
			pad->Transfer(1, 3, buf);
			Assert::AreEqual<uint8_t>(9, buf[0], L"a standard controller answers with its type");
			Assert::AreEqual<uint8_t>(0, buf[2], L"the motor is stopped");

			Assert::IsTrue(pad->SetMotor(PAD_MOTOR_RUMBLE), L"the motor command reached the host");
			Assert::AreEqual<int>(PAD_MOTOR_RUMBLE, TestHostRumble(0), L"the first host controller rumbles");

			memset(buf, 0, sizeof(buf));
			buf[0] = 0x00;
			pad->Transfer(1, 3, buf);
			Assert::AreEqual<uint8_t>(PAD_MOTOR_RUMBLE << 4, buf[2], L"the motor state is bits 5:4 of STAT");

			pad->SetMotor(PAD_MOTOR_STOP);
			Assert::AreEqual<int>(PAD_MOTOR_STOP, TestHostRumble(0), L"and it stops");
		}

		// The reset command is part of the pad's command set: it puts the controller's state machine
		// back to its power-on state, clears the latched motor command, and answers with the
		// identification - the same three bytes 0x00 gives (standard-controller.md 3). The console's
		// own library issues it from PADReset, so a pad that does not answer it is a pad the library
		// reports a transfer error for.
		TEST_METHOD(Pad_TheResetCommandIsAnsweredAndStopsTheMotor)
		{
			M();
			PeripheralDevice* pad = Peripherals::Instance().DeviceOnPort(PERIPH_PORT_SI(0));

			Assert::IsTrue(pad->SetMotor(PAD_MOTOR_RUMBLE), L"the motor is running");
			Assert::AreEqual<int>(PAD_MOTOR_RUMBLE, TestHostRumble(0), L"the host game controller rumbles");

			uint8_t buf[8] = { 0xFF };
			pad->Transfer(1, 3, buf);

			Assert::AreEqual<uint8_t>(9, buf[0], L"the reset answers with the type byte");
			Assert::AreEqual<uint8_t>(0, buf[1], L"and the sub-type");
			Assert::AreEqual<uint8_t>(0, buf[2], L"the latched motor command is cleared");
			Assert::AreEqual<int>(PAD_MOTOR_STOP, TestHostRumble(0), L"and the host stopped rumbling");
		}

		// Read origins (0x41) is what the guest library calls to learn the calibration of a pad.
		TEST_METHOD(Pad_TheOriginsAreAnswered)
		{
			M();
			PeripheralDevice* pad = Peripherals::Instance().DeviceOnPort(PERIPH_PORT_SI(0));

			uint8_t buf[8] = { 0 };
			buf[0] = 0x41;
			pad->Transfer(1, 8, buf);

			Assert::AreEqual<uint8_t>(0x41, buf[0], L"the command is echoed");
			Assert::AreEqual<uint8_t>(0x80, buf[2], L"the stick origin is the middle of the channel");
		}

		// =========================================================================================
		// The Game Boy Advance on a link cable (DOL-011)
		// =========================================================================================

		//! The pool index of a device after it is added, and the socket it is put in. The socket is
		//! freed first: the test machine plugs a pad into every one of them.
		static PeripheralDevice* PlugDevice(int& index, uint32_t type, int port)
		{
			Peripherals& pool = Peripherals::Instance();

			int existing = IndexOf(pool.DeviceOnPort(port));

			if (existing >= 0)
			{
				pool.Detach(existing);
			}

			index = pool.AddDevice(type);

			if (index < 0 || !pool.Attach(index, port))
			{
				return nullptr;
			}

			return pool.Device(index);
		}

		// The console drives a Game Boy Advance that is on a controller socket with its own command
		// set, and the answers *are* the Game Boy Advance's JOY registers (gba-link-cable.md 3, 4):
		// a JOY write puts the word in the machine's input port and raises its receive flag, a reset
		// takes it down again, and the status byte is the same register the Game Pak reads.
		TEST_METHOD(GbaLink_TheConsoleDrivesTheAdvanceThroughItsJoyRegisters)
		{
			M();

			Peripherals& pool = Peripherals::Instance();

			// The pool of the test machine has a pad in every socket, so one is taken out for the
			// cable. Everything the test looks at is collected first and the pool is put back before
			// the assertions, so that a failing assertion cannot leave a socket empty for the tests
			// that follow.
			int socket = PERIPH_PORT_SI(3);
			PeripheralDevice* pad = pool.DeviceOnPort(socket);
			int padIndex = IndexOf(pad);
			pool.Detach(padIndex);

			int index = pool.AddDevice(PERIPH_DEVICE_GBA_LINK);
			bool attached = index >= 0 && pool.Attach(index, socket);
			PeripheralDevice* link = pool.Device(index);

			uint32_t type = link != nullptr ? link->Type() : 0;

			// JOY-STATUS: three bytes, and a port that has seen nothing reports nothing.
			uint8_t status[8] = { 0x00 };
			uint8_t idle = 0xFF;
			if (link != nullptr)
			{
				link->Transfer(1, 3, status);
				idle = (uint8_t)(status[2] & 0x3A);
			}

			// JOY-WRITE: four data bytes into the Advance's JOY_RECV, one status byte back. The
			// receive flag is what tells the Game Pak that the console sent it something.
			uint8_t wr[8] = { 0x15, 0x11, 0x22, 0x33, 0x44 };
			uint8_t received = 0;
			if (link != nullptr)
			{
				link->Transfer(5, 1, wr);
				received = (uint8_t)(wr[0] & 0x02);
			}

			// JOY-READ: five bytes back, and the send flag is down because the Advance has not put
			// anything in JOY_TRANS yet.
			uint8_t rd[8] = { 0x14 };
			uint8_t sending = 0xFF;
			if (link != nullptr)
			{
				link->Transfer(1, 5, rd);
				sending = (uint8_t)(rd[4] & 0x08);
			}

			// JOY-RESET: the port's own state goes back, the receive flag with it.
			uint8_t rst[8] = { 0xFF };
			uint8_t afterReset = 0xFF;
			if (link != nullptr)
			{
				link->Transfer(1, 3, rst);
				afterReset = (uint8_t)(rst[2] & 0x3A);
			}

			// A command the cable does not know is reported and is not answered at all: the buffer
			// keeps the command byte, which is what the SI leaves there when nothing drives the line.
			uint8_t bad[8] = { 0x77 };
			if (link != nullptr)
			{
				link->Transfer(1, 3, bad);
			}

			pool.Detach(index);
			pool.RemoveDevice(index);
			pool.Attach(padIndex, socket);

			Assert::IsTrue(attached, L"the cable goes into the freed socket");
			Assert::AreEqual<uint32_t>(PERIPH_DEVICE_GBA_LINK, type, L"and it is the cable");
			Assert::AreEqual<uint8_t>(0, idle, L"an idle port has no status flags");
			Assert::AreEqual<uint8_t>(0x02, received, L"the port received a word");
			Assert::AreEqual<uint8_t>(0, sending, L"nothing is being sent");
			Assert::AreEqual<uint8_t>(0, afterReset, L"the reset cleared the port state");
			Assert::AreEqual<uint8_t>(0x77, bad[0], L"an unknown command is not answered");
		}

		// The Game Boy Advance end of the same cable: the registers a Game Pak watches, and the
		// flags that move when the console reads and writes them. This is the half that a cartridge
		// that speaks JOY bus mode sees, so it is tested on the machine itself.
		TEST_METHOD(GbaLink_TheAdvanceSeesTheConsoleInItsJoyRegisters)
		{
			GBA::GbaSystem machine;
			GBA::Sio& sio = machine.Link();

			sio.AttachConsole(true);

			// The console writes a 32bit word: it lands in JOY_RECV, low half first, and the receive
			// flag is up until the Game Pak takes it.
			sio.JoyConsoleWrite(machine.Bus(), 0x12345678u);

			// The flag is up until the Game Pak takes the data out of the register, so it is looked
			// at before the read below clears it.
			Assert::AreEqual<uint16_t>(0x0002, (uint16_t)(sio.JoyStatus() & 0x0002), L"receive flag");
			Assert::AreEqual<uint16_t>(0x0002, (uint16_t)(sio.Read16(machine.Bus(), 0x140, 0) & 0x0002),
				L"and the program is told about it");

			Assert::AreEqual<uint16_t>(0x5678, sio.Read16(machine.Bus(), 0x150, 0), L"JOY_RECV low");
			Assert::AreEqual<uint16_t>(0x1234, sio.Read16(machine.Bus(), 0x152, 0), L"JOY_RECV high");

			// Reading JOY_RECV is what takes the flag down (GBATEK "JOYSTAT").
			(void)sio.Read16(machine.Bus(), 0x150, 0);
			Assert::AreEqual<uint16_t>(0, (uint16_t)(sio.JoyStatus() & 0x0002), L"the flag is cleared");

			// The Game Pak puts a word in JOY_TRANS: the send flag goes up and stays up until the
			// console reads it, because the console is on the other end of the cable.
			sio.Write16(machine.Bus(), 0x154, 0xAAAA);
			sio.Write16(machine.Bus(), 0x156, 0xBBBB);
			Assert::AreEqual<uint16_t>(0x0008, (uint16_t)(sio.JoyStatus() & 0x0008), L"send flag");
			Assert::AreEqual<uint32_t>(0xBBBBAAAAu, sio.JoyTransmit(), L"the word is what was written");

			sio.JoyConsoleRead(machine.Bus());
			Assert::AreEqual<uint16_t>(0, (uint16_t)(sio.JoyStatus() & 0x0008), L"the console took it");
			Assert::AreEqual<uint16_t>(0x0004, (uint16_t)(sio.Read16(machine.Bus(), 0x140, 0) & 0x0004),
				L"and the send completed");

			// With no console on the cable a JOY_TRANS write completes on its own - that is how the
			// BIOS's own port probe learns that the socket is empty.
			sio.AttachConsole(false);
			sio.Write16(machine.Bus(), 0x154, 0x0001);
			Assert::AreEqual<uint16_t>(0, (uint16_t)(sio.JoyStatus() & 0x0008), L"no send is pending");
			Assert::AreEqual<uint16_t>(0x0002, (uint16_t)(sio.JoyStatus() & 0x0002), L"and it received");
		}

		// A Game Boy Advance that is used as a *link partner* is multi-booted by the console: the title
		// streams its program into a machine whose cartridge bay is **empty**, and the BIOS is what
		// receives it (that is how the Tingle Tuner, WarioWare's GBA modes and the Four Swords work).
		// A machine that only ran an inserted Game Pak therefore could never take part, so the ask is
		// what starts it: a port the console only polls as a pad, with nothing in the bay, still costs
		// nothing, and a port the console drives as a link runs the BIOS.
		TEST_METHOD(GbaLink_AnEmptyBayRunsWhenTheConsoleDrivesTheLink)
		{
			M();

			Peripherals& pool = Peripherals::Instance();

			int socket = PERIPH_PORT_SI(3);
			PeripheralDevice* pad = pool.DeviceOnPort(socket);
			int padIndex = IndexOf(pad);
			pool.Detach(padIndex);

			int index = pool.AddDevice(PERIPH_DEVICE_GBA_LINK);
			bool attached = index >= 0 && pool.Attach(index, socket);
			PeripheralDevice* link = pool.Device(index);

			std::string idle = attached ? link->GetProperty(3) : std::string();

			// The console polls the socket the way every title polls a controller: the bay is empty
			// and nothing of the machine runs, so the state line says so and no frame has passed.
			uint8_t status[8] = { 0x00 };
			if (link != nullptr)
			{
				link->Transfer(1, 3, status);
			}

			std::string polled = link != nullptr ? link->GetProperty(3) : std::string();

			// Now the console drives the link - the multi-boot starts with a reset and writes - and
			// the machine has to be running for the BIOS to answer at all.
			DspTestSetGekkoTicks(1000);

			uint8_t reset[8] = { 0xFF };
			if (link != nullptr)
			{
				link->Transfer(1, 3, reset);
			}

			DspTestSetGekkoTicks(1032);

			uint8_t read[8] = { 0x14 };
			if (link != nullptr)
			{
				link->Transfer(1, 5, read);
			}

			std::string driven = link != nullptr ? link->GetProperty(3) : std::string();

			// The pool is put back before the assertions, so that a failure cannot leave a socket
			// empty for the tests that follow.
			pool.RemoveDevice(index);
			pool.Attach(padIndex, socket);

			Assert::IsTrue(attached, L"a Game Boy Advance on the cable");
			Assert::IsTrue(idle == "No Game Pak", Widen("an empty bay: " + idle).c_str());
			Assert::IsTrue(polled == "No Game Pak", Widen("and a pad poll leaves it alone: " + polled).c_str());
			Assert::IsTrue(driven.find("drives the link") != std::string::npos,
				Widen("a driven port runs the machine: " + driven).c_str());
			Assert::IsTrue(driven.find("(0 frame(s))") == std::string::npos,
				Widen("and the BIOS has been running: " + driven).c_str());
		}

		// =========================================================================================
		// The Game Boy Player on the Hi-Speed Port
		// =========================================================================================

		// The console identifies a Game Boy Player with a loopback handshake through the port's
		// first 32-byte block: it writes a block of a constant byte and requires the complement
		// back. Nothing else can answer it, which is why the handshake is what tells a Player from a
		// plain SDRAM expansion module (see wiki/gbplayer.md).
		TEST_METHOD(GbaPlayer_ThePortAnswersTheIdentificationHandshake)
		{
			M();

			Peripherals& pool = Peripherals::Instance();
			int index = pool.AddDevice(PERIPH_DEVICE_GBPLAYER);

			Assert::IsTrue(index >= 0, L"the pool takes a Game Boy Player");
			Assert::IsTrue(pool.Attach(index, PERIPH_PORT_HSP), L"it goes onto the Hi-Speed Port");

			Flipper::HiSpeedPort* hsp = Flipper::HW->hsp;
			Assert::IsNotNull(hsp, L"the machine has the port");
			Assert::IsTrue(hsp->Attached(), L"and the Player is on it");
			Assert::AreEqual(3, hsp->ExpansionCode(), L"the Player claims a 16 MB window");

			// The window begins where the internal ARAM ends.
			uint32_t base = HSP_WINDOW_BASE;

			uint8_t out[HSP_BLOCK_SIZE];
			uint8_t in[HSP_BLOCK_SIZE];

			// Four rounds of the handshake: a block of one constant byte, and the complement back.
			const uint8_t challenge[4] = { 0xDE, 0xC3, 0x3C, 0xFF };

			for (uint8_t value : challenge)
			{
				memset(out, value, sizeof(out));

				Assert::IsTrue(hsp->Write(base, out, sizeof(out)), L"the window takes the block");
				Assert::IsTrue(hsp->Read(base, in, sizeof(in)), L"and answers the read");

				for (int i = 0; i < HSP_BLOCK_SIZE; i++)
				{
					Assert::AreEqual<uint8_t>((uint8_t)~value, in[i], L"the answer is the complement");
				}
			}

			// A channel the console has not written to answers the device's own state. The Player
			// comes up only after the console has moved enough blocks to have really started talking
			// to it - a title's own ARAM probe must not switch it on - so it is given those blocks
			// first, and then reports a running machine and an empty bay.
			for (int i = 0; i < 70; i++)
			{
				hsp->Read(base + 0xD00000, in, sizeof(in));
			}

			Assert::AreEqual<uint8_t>(0x02, in[HSP_BLOCK_SIZE - 1],
				L"the device's own state: the machine is running, the bay is empty");

			// A command channel is a register *both* sides write. The console's own command routine
			// reads the status out of such a channel, clears the event bits it is acknowledging,
			// sets the bits of the command it is sending and writes the result back (that routine is
			// in the disc's code, see wiki/gbplayer.md), so the channel has to answer the read with
			// what it holds. A channel that answered a constant would have the console read the same
			// event forever, acknowledge it, and poll in a loop - which is what the Start-up Disc
			// did until the writes were honoured.
			uint32_t status = base + 0x400000;

			memset(out, 0, sizeof(out));
			out[HSP_BLOCK_SIZE - 1] = 0x03;
			hsp->Write(status, out, sizeof(out));
			hsp->Read(status, in, sizeof(in));
			Assert::AreEqual<uint8_t>(0x03, in[HSP_BLOCK_SIZE - 1], L"the channel holds the status");

			out[HSP_BLOCK_SIZE - 1] = 0x91;			// bit 1 acknowledged, bits 4 and 7 commanded
			hsp->Write(status, out, sizeof(out));
			hsp->Read(status, in, sizeof(in));
			Assert::AreEqual<uint8_t>(0x91, in[HSP_BLOCK_SIZE - 1], L"the acknowledge sticks");

			// The mailbox is still not memory: an answer is the complement of the block written, and
			// that is the one thing a plain SDRAM module cannot do.
			memset(out, 0x5A, sizeof(out));
			hsp->Write(base, out, sizeof(out));
			hsp->Read(base, in, sizeof(in));
			Assert::AreEqual<uint8_t>(0xA5, in[0], L"the mailbox answers the complement");

			// A read answers the whole range it is asked for, and the console asks for more than one
			// block: the frame stream at +0x800000 comes in 3840-byte reads (wiki/gbplayer.md 4). Only
			// the block at the offset is the device's, so the rest of the range is zeros - never
			// whatever the caller's own buffer held.
			uint8_t range[HSP_BLOCK_SIZE * 3];
			memset(range, 0xAA, sizeof(range));

			Assert::IsTrue(hsp->Read(base + 0x800000, range, sizeof(range)), L"a multi-block read");

			for (int i = HSP_BLOCK_SIZE; i < (int)sizeof(range); i++)
			{
				Assert::AreEqual<uint8_t>(0, range[i], L"the rest of a multi-block read is zero");
			}

			pool.Detach(index);
			pool.RemoveDevice(index);
		}

		// =========================================================================================
		// The Broadband Adapter (DOL-015)
		// =========================================================================================

		//! One transfer of the command-then-data protocol the console's own driver speaks (see
		//! peripherals.h). The first transfer of a selection is the command word and the transfers
		//! after it carry the data; `word` is the value of the bytes on the wire, and they go into
		//! the top of the data register the way `EXIImm` puts them there (the first byte sent is
		//! bits 31:24).
		static void SerialTransfer(Flipper::ExternalInterface* exi, uint32_t word, int bytes, int rw,
			bool first)
		{
			exi->exi.firstImm = first;
			exi->exi.regs[0].cr = (uint16_t)(((bytes - 1) << 4) | (rw << 2) | 1);
			exi->exi.regs[0].data = word << (8 * (4 - bytes));

			Peripherals::Instance().TransferEXI(0, 2, exi);
		}

		//! A command word: two bytes naming a shim register (`0x4000 | reg << 8` when it is a
		//! write), or four naming a controller address (`0x8000` to read it, `0xC000` to write).
		static void SerialCommand(Flipper::ExternalInterface* exi, uint32_t word, int bytes)
		{
			SerialTransfer(exi, word, bytes, 1, true);
		}

		static uint32_t SerialRead(Flipper::ExternalInterface* exi, int bytes)
		{
			SerialTransfer(exi, 0, bytes, 0, false);
			return exi->exi.regs[0].data;
		}

		static void SerialWrite(Flipper::ExternalInterface* exi, uint32_t word, int bytes)
		{
			SerialTransfer(exi, word, bytes, 1, false);
		}

		//! The four bytes of a shim register read as a burst, most significant byte first.
		static uint32_t ShimRead(Flipper::ExternalInterface* exi, int reg, int bytes)
		{
			SerialCommand(exi, (uint32_t)(reg & 0xFF) << 8, 2);
			return SerialRead(exi, bytes);
		}

		//! A controller register read: the command word is four bytes - `0x8000` and the address -
		//! and the data that follows is a burst of `bytes` consecutive addresses.
		static uint32_t ChipRead(Flipper::ExternalInterface* exi, int reg, int bytes)
		{
			SerialCommand(exi, 0x80000000u | ((uint32_t)reg << 8), 4);
			return SerialRead(exi, bytes);
		}

		static void ChipWrite(Flipper::ExternalInterface* exi, int reg, uint32_t word, int bytes)
		{
			SerialCommand(exi, 0xC0000000u | ((uint32_t)reg << 8), 4);
			SerialWrite(exi, word, bytes);
		}

		// The adapter is told apart from the modem on the same chip select by its EXI device ID, and
		// the console reads its MAC address out of the controller's registers. Both are what the
		// driver's bring-up is built on, so both are pinned here.
		TEST_METHOD(Bba_TheAdapterIdentifiesItselfAndItsAddress)
		{
			M();

			Peripherals& pool = Peripherals::Instance();
			int index = pool.AddDevice(PERIPH_DEVICE_BBA);

			Assert::IsTrue(index >= 0, L"the pool takes a broadband adapter");
			Assert::IsTrue(pool.Attach(index, PERIPH_PORT_SERIAL1), L"serial port 1 takes it");

			// The device's own state is all the transfer needs: the EXI register file is the only
			// thing the adapter looks at.
			static uint8_t exiStorage[sizeof(Flipper::ExternalInterface)] = { 0 };
			Flipper::ExternalInterface* exi = (Flipper::ExternalInterface*)exiStorage;

			// The identification, exactly as the console's own driver reads it: `EXIGetID(0, 2)` is
			// a two-byte command word and a four-byte read of shim register 0.
			Assert::AreEqual<uint32_t>(0x04020200u, ShimRead(exi, 0x00, 4), L"the EXI device ID");

			// The controller's own identification, a two-byte read at address 0x44 and 0x46.
			Assert::AreEqual<uint32_t>(('M' << 24) | ('X' << 16), ChipRead(exi, 0x44, 2),
				L"the family signature");
			Assert::AreEqual<uint32_t>(('0' << 24) | ('1' << 16), ChipRead(exi, 0x46, 2),
				L"the chip revision signature");

			// The MAC address is the six bytes of PAR0..PAR5, which the driver reads as one burst:
			// the address walks with every byte (the library's readcmdLong sends one command word
			// and then a six-byte transfer).
			uint64_t address = ChipRead(exi, 0x20, 4);
			address = (address << 16) | (uint16_t)(ChipRead(exi, 0x24, 2) >> 16);
			Assert::AreEqual<uint64_t>(0x0009BF000001ull, address,
				L"the address the adapter answers to (Nintendo's block, unit 1)");

			// The link state the driver polls before it configures the interface (the
			// autonegotiation status reports a live 10BASE-T link).
			uint32_t nways = ChipRead(exi, 0x31, 1) >> 24;
			Assert::AreEqual<uint32_t>(0x03u, nways & 0x03u, L"the link is up");

			// A controller register write: the command word names the register and the data that
			// follows is the value - two transfers, which is what the driver makes (it writes 0x80
			// into MISC2 at bring-up).
			ChipWrite(exi, 0x50, 0x80u, 1);
			Assert::AreEqual<uint32_t>(0x80u, ChipRead(exi, 0x50, 1) >> 24, L"MISC2 keeps its value");

			// A shim register write, which the driver makes the same way: the command word is two
			// bytes and the value after it, here the two-byte revision the driver writes.
			SerialCommand(exi, 0x4000 | (0x04 << 8), 2);
			SerialWrite(exi, 0xD107u, 2);
			Assert::AreEqual<uint32_t>(0xD1u, ShimRead(exi, 0x04, 1) >> 24, L"the shim revision");

			// The interrupt mask the driver acknowledges into: command, then the byte.
			SerialCommand(exi, 0x4000 | (0x02 << 8), 2);
			SerialWrite(exi, 0xF8u, 1);
			Assert::AreEqual<uint32_t>(0xF8u, ShimRead(exi, 0x02, 1) >> 24, L"the interrupt mask");

			// The challenge/response: the emulated adapter has a challenge and accepts a response,
			// which is all it can do - the algorithm is not public (see bba.h).
			Assert::AreEqual<uint32_t>(0x5A5AA5A5u, ShimRead(exi, 0x08, 4), L"the challenge");
			SerialCommand(exi, 0x4000 | (0x09 << 8), 4);
			SerialWrite(exi, 0x12345678u, 4);
			Assert::AreEqual<uint32_t>(1u, ShimRead(exi, 0x0B, 1) >> 24, L"the response is accepted");

			// A transmit the way the driver makes it: the command word selects the transmit FIFO,
			// the frame is a burst of bytes into it, and NCRA starts the transmitter (the driver
			// reads the register back, sets its start bit and writes it).
			SerialCommand(exi, 0xC0000000u | (0x48 << 8), 4);
			for (int i = 0; i < 16; i++)
			{
				SerialWrite(exi, (uint32_t)(0x10 + i), 1);
			}
			ChipWrite(exi, 0x00, 0x04u, 1);
			Assert::AreEqual<uint32_t>(0u, ChipRead(exi, 0x3E, 2) & 0xFFFFu,
				L"the transmit FIFO is empty again");

			pool.Detach(index);
			pool.RemoveDevice(index);
		}

		// =========================================================================================
		// The command-word rule, through the register file
		// =========================================================================================

		// The tests above hand a transfer straight to the device: they set the command-word flag
		// themselves, which keeps them short but leaves the rule that sets it untested. These
		// helpers go through the channel's own register traps instead - the writes a CPU makes - so
		// the flag is set the way the guest sets it, by the CSR write that asserts the chip select.
		// That is the only thing that tells a command word from data, and it is what the bring-up of
		// the two adapters turns on: a driver that selects a socket it already has selected (which
		// is what the console's own `EXISelect` does on every transfer) is starting a new command
		// sequence, and if it is not read that way its first command word is taken for the data of
		// the sequence before it.

		//! One half of an EXI channel 0 register, written the way the CPU writes it.
		static void ExiRegWrite(uint32_t offset, uint32_t value)
		{
			Assert::IsTrue(PIRegWrite(PI_REGSPACE_EXI | offset, value),
				L"the EXI register window answers");
		}

		static uint32_t ExiRegRead(uint32_t offset)
		{
			uint32_t value = 0;

			Assert::IsTrue(PIRegRead(PI_REGSPACE_EXI | offset, &value),
				L"the EXI register window answers");

			return value;
		}

		//! One immediate transfer: the word goes into the data register and the CR write starts the
		//! transfer - `EXI_CR_TSTART` with the length and the direction, which is what the console's
		//! library writes. The device is the one the channel's own CSR selects.
		static uint32_t ExiImmediate(uint32_t word, int bytes, int rw)
		{
			uint32_t aligned = word << (8 * (4 - bytes));

			ExiRegWrite(EXI0_DATA, aligned >> 16);
			ExiRegWrite(EXI0_DATA + 2, aligned & 0xFFFF);
			ExiRegWrite(EXI0_CR + 2, (uint32_t)(((bytes - 1) << 4) | (rw << 2) | 1));

			return (ExiRegRead(EXI0_DATA) << 16) | ExiRegRead(EXI0_DATA + 2);
		}

		//! The driver's shim register read: a two-byte command word and then the read.
		static uint32_t ExiShimRead(int reg, int bytes)
		{
			ExiImmediate((uint32_t)(reg & 0xFF) << 8, 2, 1);
			return ExiImmediate(0, bytes, 0);
		}

		//! The driver's controller register read: a four-byte command word and then the read.
		static uint32_t ExiChipRead(int reg, int bytes)
		{
			ExiImmediate(0x80000000u | ((uint32_t)reg << 8), 4, 1);
			return ExiImmediate(0, bytes, 0);
		}

		// The identification of the adapter, made twice while the chip select never goes away. The
		// first sequence selects the socket; the second selects the very same socket again - the
		// select did not change, and the device answered a different register in the meantime, so
		// the answer of the second identification is where a command word that was taken for data
		// shows up: the two command bytes land in the register that was read last and the read
		// answers that register instead of the device ID.
		TEST_METHOD(Bba_ASelectThatDoesNotChangeStartsANewCommandSequence)
		{
			M();

			Peripherals& pool = Peripherals::Instance();
			int index = pool.AddDevice(PERIPH_DEVICE_BBA);

			Assert::IsTrue(index >= 0, L"the pool takes a broadband adapter");
			Assert::IsTrue(pool.Attach(index, PERIPH_PORT_SERIAL1), L"serial port 1 takes it");

			// The first command sequence of the driver's bring-up: the device ID out of shim
			// register 0, right after the socket is selected.
			ExiRegWrite(EXI0_CSR + 2, EXI_CSR_CS2B);
			uint32_t id = ExiShimRead(0x00, 4);

			// A sequence that leaves another register selected: the signature of the controller,
			// which the driver reads out of the chip's own address space.
			ExiRegWrite(EXI0_CSR + 2, EXI_CSR_CS2B);
			uint32_t signature = ExiChipRead(0x44, 2);

			// And back to the device ID, with the select asserted again but unchanged. The transfer
			// after the write is a command word, exactly as it was the first time.
			ExiRegWrite(EXI0_CSR + 2, EXI_CSR_CS2B);
			uint32_t again = ExiShimRead(0x00, 4);

			// The socket is given back before the answers are judged: a test that leaves a device
			// in the pool of the next one (the pool is the emulator's, not the test's) hides the
			// failure it was meant to show.
			pool.Detach(index);
			pool.RemoveDevice(index);

			Assert::AreEqual<uint32_t>(0x04020200u, id, L"the first device ID");
			Assert::AreEqual<uint32_t>(('M' << 24) | ('X' << 16), signature, L"the family signature");
			Assert::AreEqual<uint32_t>(0x04020200u, again,
				L"the device ID again, with the select never dropped");
		}

		// The chip selects of a channel are mutually exclusive, and the rule is per bit: a write that
		// asks for two of them takes none, and a write that asks for a second one while another is
		// already asserted drops both - the field ends up holding exactly what the write asked for,
		// minus the bits that lost the arbitration (see exi.cpp; the rule is what the hardware does,
		// expansion-interface.md 1.3). Nothing else in the suite drives the CSR, so it is pinned
		// here through the register window the CPU writes it through.
		TEST_METHOD(Exi_TheChipSelectsOfAChannelAreMutuallyExclusive)
		{
			M();

			const uint32_t all = EXI_CSR_CS0B | EXI_CSR_CS1B | EXI_CSR_CS2B;

			// Nothing is selected out of reset.
			Assert::AreEqual<uint32_t>(0u, ExiRegRead(EXI0_CSR + 2) & all, L"no select at reset");

			// Two selects in one write: neither is taken.
			ExiRegWrite(EXI0_CSR + 2, EXI_CSR_CS0B | EXI_CSR_CS2B);
			Assert::AreEqual<uint32_t>(0u, ExiRegRead(EXI0_CSR + 2) & all,
				L"a write that asks for two selects takes none");

			// One at a time is taken.
			ExiRegWrite(EXI0_CSR + 2, EXI_CSR_CS2B);
			Assert::AreEqual<uint32_t>(EXI_CSR_CS2B, ExiRegRead(EXI0_CSR + 2) & all,
				L"a write that asks for one takes it");

			// Asking for another one while one is asserted drops both.
			ExiRegWrite(EXI0_CSR + 2, EXI_CSR_CS0B);
			Assert::AreEqual<uint32_t>(0u, ExiRegRead(EXI0_CSR + 2) & all,
				L"a second select drops the one that was asserted");

			// And the one that was dropped by that arbitration can be taken again.
			ExiRegWrite(EXI0_CSR + 2, EXI_CSR_CS0B);
			Assert::AreEqual<uint32_t>(EXI_CSR_CS0B, ExiRegRead(EXI0_CSR + 2) & all,
				L"the dropped select can be taken again");

			// The register file is the machine's and outlives a test: a select left asserted here
			// would arbitrate against the next test's own select, so the channel is left deselected.
			ExiRegWrite(EXI0_CSR + 2, 0);
			Assert::AreEqual<uint32_t>(0u, ExiRegRead(EXI0_CSR + 2) & all,
				L"a write of no select deselects the channel");
		}

		// =========================================================================================
		// The Modem Adapter (DOL-012)
		// =========================================================================================

		//! A modem register write, the way the driver's `writecmd` makes it: the command word is the
		//! register number shifted into the second byte with the write flag above it, and the byte
		//! after it is the value.
		static void MdmWrite(Flipper::ExternalInterface* exi, int reg, int value)
		{
			SerialCommand(exi, 0x4000u | ((uint32_t)(reg & 0x3F) << 8), 2);
			SerialWrite(exi, (uint32_t)(value & 0xFF), 1);
		}

		//! A modem register read (`readcmd`): the command word is the register alone.
		static uint32_t MdmRead(Flipper::ExternalInterface* exi, int reg, int bytes)
		{
			SerialCommand(exi, (uint32_t)(reg & 0x3F) << 8, 2);
			return SerialRead(exi, bytes);
		}

		//! Read the AT answer a command produced, out of the data register the driver polls.
		static std::string MdmAnswer(Flipper::ExternalInterface* exi, const char* command)
		{
			for (const char* p = command; *p != 0; p++)
			{
				MdmWrite(exi, 0x00, *p);
			}

			MdmWrite(exi, 0x00, '\r');

			std::string answer;

			for (int guard = 0; guard < 128; guard++)
			{
				uint32_t status = MdmRead(exi, 0x01, 1) >> 24;

				if ((status & 0x01) == 0)
				{
					break;
				}

				answer += (char)(MdmRead(exi, 0x00, 1) >> 24);
			}

			return answer;
		}

		// The modem shares serial port 1 with the broadband adapter and is told apart from it by its
		// device ID; its AT interface is what the driver's bring-up waits on. The emulated adapter
		// has no line, so a dial is refused - with the result code a real modem reports.
		TEST_METHOD(Modem_TheAdapterIdentifiesItselfAndRefusesToDial)
		{
			M();

			Peripherals& pool = Peripherals::Instance();
			int index = pool.AddDevice(PERIPH_DEVICE_MODEM);

			Assert::IsTrue(index >= 0, L"the pool takes a modem adapter");
			Assert::IsTrue(pool.Attach(index, PERIPH_PORT_SERIAL1), L"serial port 1 takes it");

			static uint8_t exiStorage[sizeof(Flipper::ExternalInterface)] = { 0 };
			Flipper::ExternalInterface* exi = (Flipper::ExternalInterface*)exiStorage;

			// The device ID the driver requires (`readCID`: a command word of zero and a four-byte
			// read of that register).
			Assert::AreEqual<uint32_t>(0x02020000u, MdmRead(exi, 0x00, 4), L"the EXI device ID");

			// The status the driver polls before it takes data: nothing to read, and the UART is
			// ready to take a byte.
			Assert::AreEqual<uint32_t>(0x02u, MdmRead(exi, 0x01, 1) >> 24, L"the UART is idle");

			// The setup commands the driver sends are accepted, which is what its bring-up waits on.
			Assert::IsTrue(MdmAnswer(exi, "ATW1\\V0").find("OK") != std::string::npos,
				L"the result-code setup is accepted");
			Assert::IsTrue(MdmAnswer(exi, "ATE0").find("OK") != std::string::npos, L"echo off");
			Assert::IsTrue(MdmAnswer(exi, "ATS95=44").find("OK") != std::string::npos, L"an S register");

			// A dial: there is no line behind the emulated adapter, so the modem reports exactly the
			// code the driver parses for it.
			std::string dial = MdmAnswer(exi, "ATDT15551234");
			Assert::IsTrue(dial.find("NO DIALTONE") != std::string::npos,
				Widen("a dial with no line is refused, got: " + dial).c_str());

			// The scratch register is the driver's own byte: writing it and reading it back is how
			// the driver checks that the adapter answers registers at all.
			MdmWrite(exi, 0x53, 0x5A);
			Assert::AreEqual<uint32_t>(0x5Au, MdmRead(exi, 0x53, 1) >> 24, L"the scratch register");

			pool.Detach(index);
			pool.RemoveDevice(index);
		}

		//! A network the test owns: it remembers what the adapter transmitted and hands the adapter a
		//! frame when the test asks it to. The device never opens a socket itself (that is the front
		//! end's job, see bbaudp.cpp), so a backend is all the device needs to have a network.
		class TestBbaBackend : public BbaBackend
		{
		public:
			std::vector<std::vector<uint8_t>> sent;
			std::vector<std::vector<uint8_t>> toReceive;

			void Transmit(const uint8_t* frame, size_t length) override
			{
				sent.push_back(std::vector<uint8_t>(frame, frame + length));
			}

			bool HasReceive() override { return !toReceive.empty(); }

			bool Receive(uint8_t* frame, size_t capacity, size_t* length) override
			{
				if (toReceive.empty())
				{
					return false;
				}

				std::vector<uint8_t> next = toReceive.front();
				toReceive.erase(toReceive.begin());

				size_t take = next.size() < capacity ? next.size() : capacity;
				memcpy(frame, next.data(), take);

				if (length != nullptr)
				{
					*length = take;
				}

				return true;
			}

			bool LinkUp() override { return true; }
		};

		// The adapter's two data paths against a network the test owns: a frame the console writes
		// into the transmit FIFO reaches the network, and a frame the network hands over is found in
		// the receive ring with the descriptor the driver reads. This is the contract between the
		// device and whatever the front end installs as its backend.
		TEST_METHOD(Bba_FramesTravelBetweenTheConsoleAndTheNetwork)
		{
			M();

			Peripherals& pool = Peripherals::Instance();
			int index = pool.AddDevice(PERIPH_DEVICE_BBA);
			Assert::IsTrue(pool.Attach(index, PERIPH_PORT_SERIAL1), L"serial port 1 takes it");

			static uint8_t exiStorage[sizeof(Flipper::ExternalInterface)] = { 0 };
			Flipper::ExternalInterface* exi = (Flipper::ExternalInterface*)exiStorage;

			TestBbaBackend network;
			BbaBackend* previous = BbaGetBackend();
			BbaSetBackend(&network);

			// -- transmit, the way the driver makes it: the command word selects the transmit FIFO
			// and the frame is a burst of bytes into it, then NCRA is written back with its start
			// bit set. The frame has to reach the network whole, in the order it was written.
			const uint8_t frame[12] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 1, 2, 3, 4, 5, 6 };

			SerialCommand(exi, 0xC0000000u | (0x48 << 8), 4);

			for (int i = 0; i < (int)sizeof(frame); i++)
			{
				SerialWrite(exi, (uint32_t)frame[i], 1);
			}

			ChipWrite(exi, 0x00, 0x04u, 1);

			// -- receive: the network hands over a frame, the adapter puts it in the ring with a
			// descriptor, and the write pointer moves past it (which is how the driver sees that a
			// packet is waiting), while the read pointer stays where it was.
			uint32_t before = ChipRead(exi, 0x16, 2) >> 16;
			network.toReceive.push_back(std::vector<uint8_t>(frame, frame + sizeof(frame)));
			(void)ChipRead(exi, 0x16, 2);                       // any access polls the network

			uint32_t after = ChipRead(exi, 0x16, 2) >> 16;

			// What is in the ring: the four-byte descriptor at the read pointer and the frame after
			// it, which is what the driver reads out with a burst.
			uint32_t descriptor = ChipRead(exi, 0x100, 4);
			uint32_t head = ChipRead(exi, 0x104, 4);
			uint32_t tail = ChipRead(exi, 0x108, 4);

			// The pool and the network are put back before the assertions, so that a failure here
			// cannot leave the adapter in the socket for the tests that follow.
			BbaSetBackend(previous);
			pool.Detach(index);
			pool.RemoveDevice(index);

			Assert::AreEqual<size_t>(1, network.sent.size(), L"the frame reached the network");
			Assert::AreEqual<size_t>(sizeof(frame), network.sent[0].size(), L"whole");
			Assert::IsTrue(memcmp(frame, network.sent[0].data(), sizeof(frame)) == 0,
				L"and with the bytes the console wrote");
			Assert::IsTrue(after != before, L"the receive write pointer moved past the packet");

			// The frame is in the ring behind its descriptor, byte for byte. (How the descriptor
			// packs its length is the one part of the receive path no public document settles - a
			// real title is what would confirm it - so the test pins the payload and the length's
			// presence, not its encoding.)
			Assert::IsTrue(descriptor != 0, L"the packet has a descriptor");
			Assert::AreEqual<uint32_t>(0xFFFFFFFFu, head, L"the payload's first four bytes");
			Assert::AreEqual<uint32_t>(0xFFFF0102u, tail, L"and the four after them");
		}

		// A socket with nothing in it is not answered by a device, and the channel says so the way
		// the hardware does: the poll of an enabled channel always completes and sets its read
		// status, and a channel whose device does not answer latches NOREP as well. The guest tells
		// an empty socket from a busy one by that error (serial-interface.md 5.1, 7.1) - the
		// response bytes alone cannot tell them apart, because with nothing driving the line the SI
		// only hears itself.
		TEST_METHOD(Pad_AnEmptySocketIsNotAnswered)
		{
			GfxTestMachine& m = M();
			Peripherals& pool = Peripherals::Instance();

			PeripheralDevice* pad = pool.DeviceOnPort(PERIPH_PORT_SI(3));
			int index = IndexOf(pad);

			pool.Detach(index);

			Poll(m, 1u << 3);

			uint32_t sr = ReadSiWord(SrReg);

			// Put the pad back before the assertions, so that a failure here does not leave the
			// pool one controller short for the tests that follow.
			pool.Attach(index, PERIPH_PORT_SI(3));

			Assert::AreEqual<uint32_t>(SI_SR_RDST3, sr & SI_SR_RDST3,
				L"the poll of an empty socket still completes");
			Assert::AreEqual<uint32_t>(SI_SR_NOREP3, sr & SI_SR_NOREP3,
				L"and the channel latches no-response");
		}
	};
}
