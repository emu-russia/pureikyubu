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

		// A socket with nothing in it is not answered at all and raises no read status, which is how
		// the guest finds out that there is no controller (see Peripherals::PollSI).
		TEST_METHOD(Pad_AnEmptySocketIsNotAnswered)
		{
			GfxTestMachine& m = M();
			Peripherals& pool = Peripherals::Instance();

			PeripheralDevice* pad = pool.DeviceOnPort(PERIPH_PORT_SI(3));
			int index = IndexOf(pad);

			pool.Detach(index);

			Poll(m, 1u << 3);
			Assert::AreEqual<uint32_t>(0, ReadSiWord(SrReg) & SI_SR_RDST3, L"an empty socket raises no read status");

			// put the pad back, so that the other tests find the default pool
			pool.Attach(index, PERIPH_PORT_SI(3));
		}
	};
}
