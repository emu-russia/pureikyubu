using System;
using System.Collections.Generic;
using System.Text;

using System.Runtime.InteropServices;

// Console API: https://www.pinvoke.net/default.aspx/kernel32/ConsoleFunctions.html

namespace ManagedConsole
{
    public partial class Console
    {
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool AllocConsole();

        [DllImport("kernel32.dll", SetLastError = true, ExactSpelling = true)]
        static extern bool FreeConsole();

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool SetConsoleTitle(string lpConsoleTitle);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern IntPtr GetStdHandle(
                int nStdHandle);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool SetConsoleWindowInfo(
                IntPtr hConsoleOutput,
                bool bAbsolute,
                [In] ref SMALL_RECT lpConsoleWindow);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool SetConsoleScreenBufferSize(
            IntPtr hConsoleOutput,
            COORD dwSize);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool GetConsoleCursorInfo(
                IntPtr hConsoleOutput,
                out CONSOLE_CURSOR_INFO lpConsoleCursorInfo);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool GetConsoleMode(
                IntPtr hConsoleHandle,
                out uint lpMode);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool SetConsoleMode(
                IntPtr hConsoleHandle,
                uint dwMode);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool WriteConsoleOutput(
                IntPtr hConsoleOutput,
                CHAR_INFO[] lpBuffer,
                COORD dwBufferSize,
                COORD dwBufferCoord,
                ref SMALL_RECT lpWriteRegion);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool SetConsoleCursorPosition(
                IntPtr hConsoleOutput,
                COORD dwCursorPosition);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool PeekConsoleInput(
                IntPtr hConsoleInput,
                [Out] INPUT_RECORD[] lpBuffer,
                uint nLength,
                out uint lpNumberOfEventsRead);

        [DllImport("kernel32.dll", EntryPoint = "ReadConsoleInputW", CharSet = CharSet.Unicode)]
        static extern bool ReadConsoleInput(
                IntPtr hConsoleInput,
                [Out] INPUT_RECORD[] lpBuffer,
                uint nLength,
                out uint lpNumberOfEventsRead);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool CloseHandle(IntPtr hObject);

        [StructLayout(LayoutKind.Sequential)]
        struct COORD
        {

            public short X;
            public short Y;

        }

        struct SMALL_RECT
        {

            public short Left;
            public short Top;
            public short Right;
            public short Bottom;

        }

        struct CONSOLE_SCREEN_BUFFER_INFO
        {

            public COORD dwSize;
            public COORD dwCursorPosition;
            public short wAttributes;
            public SMALL_RECT srWindow;
            public COORD dwMaximumWindowSize;

        }

        [StructLayout(LayoutKind.Explicit)]
        struct INPUT_RECORD
        {
            [FieldOffset(0)]
            public ushort EventType;
            [FieldOffset(4)]
            public KEY_EVENT_RECORD KeyEvent;
            [FieldOffset(4)]
            public MOUSE_EVENT_RECORD MouseEvent;
            [FieldOffset(4)]
            public WINDOW_BUFFER_SIZE_RECORD WindowBufferSizeEvent;
            [FieldOffset(4)]
            public MENU_EVENT_RECORD MenuEvent;
            [FieldOffset(4)]
            public FOCUS_EVENT_RECORD FocusEvent;
        };

        [StructLayout(LayoutKind.Explicit, CharSet = CharSet.Unicode)]
        struct KEY_EVENT_RECORD
        {
            [FieldOffset(0), MarshalAs(UnmanagedType.Bool)]
            public bool bKeyDown;
            [FieldOffset(4), MarshalAs(UnmanagedType.U2)]
            public ushort wRepeatCount;
            [FieldOffset(6), MarshalAs(UnmanagedType.U2)]
            //public VirtualKeys wVirtualKeyCode;
            public ushort wVirtualKeyCode;
            [FieldOffset(8), MarshalAs(UnmanagedType.U2)]
            public ushort wVirtualScanCode;
            [FieldOffset(10)]
            public char UnicodeChar;
            [FieldOffset(12), MarshalAs(UnmanagedType.U4)]
            //public ControlKeyState dwControlKeyState;
            public uint dwControlKeyState;
        }

        [StructLayout(LayoutKind.Sequential)]
        struct MOUSE_EVENT_RECORD
        {
            public COORD dwMousePosition;
            public uint dwButtonState;
            public uint dwControlKeyState;
            public uint dwEventFlags;
        }

        struct WINDOW_BUFFER_SIZE_RECORD
        {
            public COORD dwSize;

            public WINDOW_BUFFER_SIZE_RECORD(short x, short y)
            {
                dwSize = new COORD();
                dwSize.X = x;
                dwSize.Y = y;
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        struct MENU_EVENT_RECORD
        {
            public uint dwCommandId;
        }

        [StructLayout(LayoutKind.Sequential)]
        struct FOCUS_EVENT_RECORD
        {
            public uint bSetFocus;
        }

        //CHAR_INFO struct, which was a union in the old days
        // so we want to use LayoutKind.Explicit to mimic it as closely
        // as we can
        [StructLayout(LayoutKind.Explicit)]
        struct CHAR_INFO
        {
            [FieldOffset(0)]
            public char UnicodeChar;
            [FieldOffset(0)]
            public char AsciiChar;
            [FieldOffset(2)] //2 bytes seems to work properly
            public UInt16 Attributes;
        }

        [StructLayout(LayoutKind.Sequential)]
        struct CONSOLE_CURSOR_INFO
        {
            uint Size;
            bool Visible;
        }

        const int FOCUS_EVENT = 0x0010;
        const int KEY_EVENT = 0x0001;
        const int MENU_EVENT = 0x0008;
        const int MOUSE_EVENT = 0x0002;
        const int WINDOW_BUFFER_SIZE_EVENT = 0x0004;

        const int LEFT_ALT_PRESSED = 0x0002;
        const int LEFT_CTRL_PRESSED = 0x0008;
        const int RIGHT_ALT_PRESSED = 0x0001;
        const int RIGHT_CTRL_PRESSED = 0x0004;
        const int SHIFT_PRESSED = 0x0010;

        const int STD_INPUT_HANDLE = -10;
        const int STD_OUTPUT_HANDLE = -11;
        const int STD_ERROR_HANDLE = -12;

        const int INVALID_HANDLE_VALUE = -1;

        const uint ENABLE_MOUSE_INPUT = 0x0010;
    }
}
