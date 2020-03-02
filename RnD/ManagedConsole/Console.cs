using System;
using System.Runtime.InteropServices;
using System.Drawing;

namespace ManagedConsole
{
    /// <summary>
    /// Color codes for console
    /// </summary>
    public enum ColorCode
    {
        DBLUE = 1,
        GREEN,
        CYAN,
        RED,
        PUR,
        BROWN,
        NORM,
        GRAY,
        BLUE,
        BGREEN,
        BCYAN,
        BRED,
        BPUR,
        YEL,
        YELLOW = YEL,
        WHITE,
    }

    public class Console
    {
        bool allocated = false;
        IntPtr input;
        IntPtr output;
        CONSOLE_CURSOR_INFO curinfo = new CONSOLE_CURSOR_INFO();
        CHAR_INFO [] buf;
        Size consize;
        Point pos;
        UInt16 attr;

        public void Allocate(string title, Size size)
        {
            if (allocated)
                return;

            buf = new CHAR_INFO[size.Width * size.Height];
            consize = new Size(size.Width, size.Height);
            pos = new Point(0, 0);
            attr = 0;

            AllocConsole();

            input = GetStdHandle(STD_INPUT_HANDLE);
            if (input == (IntPtr)INVALID_HANDLE_VALUE)
                throw new Exception("Cannot obtain input handler");

            output = GetStdHandle(STD_OUTPUT_HANDLE);
            if (output == (IntPtr)INVALID_HANDLE_VALUE)
                throw new Exception("Cannot obtain output handler");

            GetConsoleCursorInfo(output, out curinfo);
            uint Mode;
            GetConsoleMode(input, out Mode);
            Mode &= ~ENABLE_MOUSE_INPUT;
            SetConsoleMode(input, Mode);

            SMALL_RECT rect = new SMALL_RECT();

            rect.Top = rect.Left = 0;
            rect.Right = (short)(size.Width - 1);
            rect.Bottom = (short)(size.Height - 1);

            COORD coord = new COORD();

            coord.X = (short)size.Width;
            coord.Y = (short)size.Height;

            SetConsoleWindowInfo(output, true, ref rect);
            SetConsoleScreenBufferSize(output, coord);

            SetConsoleTitle(title);

            allocated = true;
        }

        public void Free()
        {
            if (!allocated)
                return;

            CloseHandle(input);
            CloseHandle(output);

            FreeConsole();
            allocated = false;
        }

        public bool IsAllocated()
        {
            return allocated;
        }

        public void PrintAt(ColorCode color, int x, int y, string text)
        {
            if (!allocated)
                throw new Exception("Allocate console first!");


        }

        private void PrintChar(char ch)
        {
            buf[pos.Y * consize.Width + pos.X].Attributes = attr;
            buf[pos.Y * consize.Width + pos.X].AsciiChar = ch;
            pos.X++;
            if (pos.X >= consize.Width)
            {
                pos.X = 0;
                pos.Y++;
                if (pos.Y >= consize.Height)
                {
                    pos.Y = consize.Height - 1;
                }
            }
        }

        public void SetAttr(int fg, int bg)
        {
            attr = (ushort)((bg << 4) | fg);
        }

        public void SetAttrFg(int fg, int bg)
        {
            attr = (ushort)((attr & ~0xf) | fg);
        }

        public void SetAttrBg(int bg)
        {
            attr = (ushort)((attr & ~(0xf << 4)) | (bg << 4));
        }

        // Console API: https://www.pinvoke.net/default.aspx/kernel32/ConsoleFunctions.html

        #region "Interop Area"

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

        const int STD_INPUT_HANDLE = -10;
        const int STD_OUTPUT_HANDLE = -11;
        const int STD_ERROR_HANDLE = -12;

        const int INVALID_HANDLE_VALUE = -1;

        const uint ENABLE_MOUSE_INPUT = 0x0010;

        #endregion "Interop Area"


    }

}
