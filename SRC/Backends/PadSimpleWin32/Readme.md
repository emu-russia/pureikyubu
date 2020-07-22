# PadSimpleWin32

GameCube controllers emulation backend. This is shitty code in every way, don't look.

Will be gradually superseded by DirectInput implementation.

## Technical features

The code relies on polling the state of the keyboard using the Win32 GetAsyncKeyState method.

The binding of VK codes to the buttons of the GameCube controller is in the configuration (Settings.json).

The configuration (dialog box for the user) is handled by the code in the UI folder.

The backend consumer is the SI.cpp module in the HW folder.
