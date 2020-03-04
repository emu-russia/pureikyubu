using System;
using System.Collections.Generic;
using System.Drawing;
using System.Threading;

using System.Runtime.InteropServices;

namespace ManagedConsole
{
    /// <summary>
    /// Color codes for console
    /// </summary>
    public enum ColorCode
    {
        BLACK = 0,
        DBLUE,
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

    public partial class Console
    {
        bool allocated = false;
        IntPtr input;
        IntPtr output;
        CONSOLE_CURSOR_INFO curinfo = new CONSOLE_CURSOR_INFO();
        CHAR_INFO [] buf;
        Size consize;
        Point pos;
        Point cursor;
        List<Window> windows = new List<Window>();
        Thread thread;
        bool stopPolling = false;

        public void Allocate(string title, Size size)
        {
            if (allocated)
                return;

            buf = new CHAR_INFO[size.Width * size.Height];
            consize = new Size(size.Width, size.Height);
            pos = new Point(0, 0);
            cursor = new Point(0, 0);
            stopPolling = false;

            for (int i=0; i<buf.Length; i++)
            {
                buf[i].Attributes = 0;
                buf[i].UnicodeChar = ' ';
            }

            bool res = AllocConsole();
            if (!res && Marshal.GetLastWin32Error() == 5)
            {
                // Close if already opened by some instance
                FreeConsole();

                res = AllocConsole();
                if (!res)
                    throw new Exception("AllocConsole failed! LastError: " + Marshal.GetLastWin32Error().ToString());
            }

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

            SetCursor(cursor);

            thread = new Thread(PollingThreadProc);
            thread.Start();

            allocated = true;
        }

        public void Free()
        {
            if (!allocated)
                return;

            stopPolling = true;
            thread.Join();

            windows.Clear();

            CloseHandle(input);
            CloseHandle(output);

            FreeConsole();
            allocated = false;
        }

        public bool IsAllocated()
        {
            return allocated;
        }

        #region "Console Output"

        public void PrintAt(ColorCode color, int x, int y, string text)
        {
            if (!allocated)
                throw new Exception("Allocate console first!");

            pos.X = x;
            pos.Y = y;

            for (int i=0; i<text.Length; i++)
            {
                if (pos.Y >= consize.Height)
                    break;

                if (text[i] == '\n')
                {
                    pos.X = x;
                    pos.Y++;
                }
                else if (text[i] == '\t')
                {
                    int numToRound = pos.X + 1;
                    int multiple = 4;
                    int untilX = ((((numToRound) + (multiple) - 1) / (multiple)) * (multiple));

                    int numspaces = untilX - pos.X;
                    while (numspaces-- != 0)
                    {
                        PrintChar(' ', color);
                    }
                }
                else
                {
                    PrintChar(text[i], color);
                }
            }
        }

        private void PrintChar(char ch, ColorCode color)
        {
            ushort oldAttr = buf[pos.Y * consize.Width + pos.X].Attributes;
            oldAttr = (ushort)((oldAttr & 0xf0) | ((int)color));
            buf[pos.Y * consize.Width + pos.X].Attributes = oldAttr;
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

        // Fill the console region with the specified color
        public void Fill(Rectangle rect, ColorCode backColor)
        {
            for (int y=rect.Y; y<rect.Height; y++)
            {
                if (y >= consize.Height)
                    break;

                for (int x =rect.X; x<rect.Width; x++)
                {
                    if (x >= consize.Width)
                        break;

                    ushort oldAttr = buf[y * consize.Width + x].Attributes;
                    oldAttr = (ushort)((oldAttr & 0xf) | ((int)backColor << 4));
                    buf[y * consize.Width + x].Attributes = oldAttr;
                }
            }
        }

        public void Invalidate()
        {
            Blit(new Rectangle(0, 0, consize.Width, consize.Height));
        }

        public void Blit (Rectangle rect)
        {
            COORD sz = new COORD();
            sz.X = (short)consize.Width;
            sz.Y = (short)consize.Height;

            COORD pos = new COORD();
            pos.X = (short)rect.X;
            pos.Y = (short)rect.Y;

            SMALL_RECT rgn = new SMALL_RECT();

            rgn.Left = (short)rect.X;
            rgn.Top = (short)rect.Y;
            rgn.Right = (short)(rect.X + rect.Width - 1);
            rgn.Bottom = (short)(rect.Y + rect.Height - 1);

            bool success = WriteConsoleOutput(output, buf, sz, pos, ref rgn);
        }

        public void SetCursor (Point pos)
        {
            COORD cr = new COORD();
            cr.X = (short)pos.X;
            cr.Y = (short)pos.Y;
            cursor.X = pos.X;
            cursor.Y = pos.Y;
            SetConsoleCursorPosition(output, cr);
        }

        public Point GetCursor ()
        {
            return new Point(cursor.X, cursor.Y);
        }

        #endregion "Console Output"


        #region "Console Input"

        public enum Key
        {
            None = 0,
            Esc, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
            Tilda, Key1, Key2, Key3, Key4, Key5, Key6, Key7, Key8, Key9, Key0, KeyMinus, KeyPlus,
            Backspace, Enter, Space,
            Up, Down, Left, Right,
            NumSlash, NumAsterisk, NumMinus, NumPlus,
            Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9, Num0,
            Q, W, E, R, T, Y, U, I, O, P,
            A, S, D, F, G, H, J, K, L,
            Z, X, C, V, B, N, M,
            LSquare, RSquare, Colon, Quotes, BackSlash, LAngle, RAngle, Slash,
            Ins, Del, Home, End, PageUp, PageDown,
        }

        public class KeyInfo
        {
            public Key KeyCode = Key.None;
            public bool Shift = false;
            public bool Ctrl = false;
            public bool Alt = false;

            public override string ToString()
            {
                switch (KeyCode)
                {
                    case Key.Tilda: return Shift ? "~" : "`";

                    case Key.Key1: return Shift ? "!" : "1";
                    case Key.Key2: return Shift ? "@" : "2";
                    case Key.Key3: return Shift ? "#" : "3";
                    case Key.Key4: return Shift ? "$" : "4";
                    case Key.Key5: return Shift ? "%" : "5";
                    case Key.Key6: return Shift ? "^" : "6";
                    case Key.Key7: return Shift ? "&" : "7";
                    case Key.Key8: return Shift ? "*" : "8";
                    case Key.Key9: return Shift ? "(" : "9";
                    case Key.Key0: return Shift ? ")" : "0";
                    case Key.KeyMinus: return Shift ? "_" : "-";
                    case Key.KeyPlus: return Shift ? "+" : "=";

                    case Key.Q: return Shift ? "Q" : "q";
                    case Key.W: return Shift ? "W" : "w";
                    case Key.E: return Shift ? "E" : "e";
                    case Key.R: return Shift ? "R" : "r";
                    case Key.T: return Shift ? "T" : "t";
                    case Key.Y: return Shift ? "Y" : "y";
                    case Key.U: return Shift ? "U" : "u";
                    case Key.I: return Shift ? "I" : "i";
                    case Key.O: return Shift ? "O" : "o";
                    case Key.P: return Shift ? "P" : "p";

                    case Key.A: return Shift ? "A" : "a";
                    case Key.S: return Shift ? "S" : "s";
                    case Key.D: return Shift ? "D" : "d";
                    case Key.F: return Shift ? "F" : "f";
                    case Key.G: return Shift ? "G" : "g";
                    case Key.H: return Shift ? "H" : "h";
                    case Key.J: return Shift ? "J" : "j";
                    case Key.K: return Shift ? "K" : "k";
                    case Key.L: return Shift ? "L" : "l";

                    case Key.Z: return Shift ? "Z" : "z";
                    case Key.X: return Shift ? "X" : "x";
                    case Key.C: return Shift ? "C" : "c";
                    case Key.V: return Shift ? "V" : "v";
                    case Key.B: return Shift ? "B" : "b";
                    case Key.N: return Shift ? "N" : "n";
                    case Key.M: return Shift ? "M" : "m";

                    case Key.Space: return " ";

                    case Key.NumSlash: return "/";
                    case Key.NumAsterisk: return "*";
                    case Key.NumMinus: return "-";
                    case Key.NumPlus: return "+";
                    case Key.Num0: return "0";
                    case Key.Num1: return "1";
                    case Key.Num2: return "2";
                    case Key.Num3: return "3";
                    case Key.Num4: return "4";
                    case Key.Num5: return "5";
                    case Key.Num6: return "6";
                    case Key.Num7: return "7";
                    case Key.Num8: return "8";
                    case Key.Num9: return "9";

                    case Key.LSquare: return Shift ? "{" : "[";
                    case Key.RSquare: return Shift ? "}" : "]";
                    case Key.Colon: return Shift ? ":" : ";";
                    case Key.Quotes: return Shift ? "\"" : "'";
                    case Key.BackSlash: return Shift ? "|" : "\\";
                    case Key.LAngle: return Shift ? "<" : ",";
                    case Key.RAngle: return Shift ? ">" : ".";
                    case Key.Slash: return Shift ? "?" : "/";
                }

                return "";
            }
        }

        // Key Press Event
        public virtual void OnKeyInput(KeyInfo key)
        { }

        private KeyInfo PollKeyboard ()
        {
            INPUT_RECORD [] record = new INPUT_RECORD[1];

            uint count;

            PeekConsoleInput(input, record, 1, out count);
            if (count == 0)
                return null;

            ReadConsoleInput(input, record, 1, out count);
            if (count == 0)
                return null;

            if (record[0].EventType == KEY_EVENT && record[0].KeyEvent.bKeyDown)
            {
                return Translate(record[0]);
            }

            return null;
        }

        private KeyInfo Translate(INPUT_RECORD record)
        {
            KeyInfo key = new KeyInfo();

            key.Shift = (record.KeyEvent.dwControlKeyState & SHIFT_PRESSED) != 0;
            key.Alt = ((record.KeyEvent.dwControlKeyState & LEFT_ALT_PRESSED) != 0) ||
                ((record.KeyEvent.dwControlKeyState & RIGHT_ALT_PRESSED) != 0);
            key.Ctrl = ((record.KeyEvent.dwControlKeyState & LEFT_CTRL_PRESSED) != 0) ||
                ((record.KeyEvent.dwControlKeyState & RIGHT_CTRL_PRESSED) != 0);

            switch (record.KeyEvent.wVirtualKeyCode)
            {
                case 0x1B: key.KeyCode = Key.Esc; break;
                case 0x08: key.KeyCode = Key.Backspace; break;
                case 0x20: key.KeyCode = Key.Space; break;
                case 0x0D: key.KeyCode = Key.Enter; break;

                case 0x70: key.KeyCode = Key.F1; break;
                case 0x71: key.KeyCode = Key.F2; break;
                case 0x72: key.KeyCode = Key.F3; break;
                case 0x73: key.KeyCode = Key.F4; break;
                case 0x74: key.KeyCode = Key.F5; break;
                case 0x75: key.KeyCode = Key.F6; break;
                case 0x76: key.KeyCode = Key.F7; break;
                case 0x77: key.KeyCode = Key.F8; break;
                case 0x78: key.KeyCode = Key.F9; break;
                case 0x79: key.KeyCode = Key.F10; break;
                case 0x7A: key.KeyCode = Key.F11; break;
                case 0x7B: key.KeyCode = Key.F12; break;

                case 0x31: key.KeyCode = Key.Key1; break;
                case 0x32: key.KeyCode = Key.Key2; break;
                case 0x33: key.KeyCode = Key.Key3; break;
                case 0x34: key.KeyCode = Key.Key4; break;
                case 0x35: key.KeyCode = Key.Key5; break;
                case 0x36: key.KeyCode = Key.Key6; break;
                case 0x37: key.KeyCode = Key.Key7; break;
                case 0x38: key.KeyCode = Key.Key8; break;
                case 0x39: key.KeyCode = Key.Key9; break;
                case 0x30: key.KeyCode = Key.Key0; break;

                case 0xC0: key.KeyCode = Key.Tilda; break;
                case 0xBD: key.KeyCode = Key.KeyMinus; break;
                case 0xBB: key.KeyCode = Key.KeyPlus; break;

                case 0x51: key.KeyCode = Key.Q; break;
                case 0x57: key.KeyCode = Key.W; break;
                case 0x45: key.KeyCode = Key.E; break;
                case 0x52: key.KeyCode = Key.R; break;
                case 0x54: key.KeyCode = Key.T; break;
                case 0x59: key.KeyCode = Key.Y; break;
                case 0x55: key.KeyCode = Key.U; break;
                case 0x49: key.KeyCode = Key.I; break;
                case 0x4F: key.KeyCode = Key.O; break;
                case 0x50: key.KeyCode = Key.P; break;

                case 0x41: key.KeyCode = Key.A; break;
                case 0x53: key.KeyCode = Key.S; break;
                case 0x44: key.KeyCode = Key.D; break;
                case 0x46: key.KeyCode = Key.F; break;
                case 0x47: key.KeyCode = Key.G; break;
                case 0x48: key.KeyCode = Key.H; break;
                case 0x4A: key.KeyCode = Key.J; break;
                case 0x4B: key.KeyCode = Key.K; break;
                case 0x4C: key.KeyCode = Key.L; break;

                case 0x5A: key.KeyCode = Key.Z; break;
                case 0x58: key.KeyCode = Key.X; break;
                case 0x43: key.KeyCode = Key.C; break;
                case 0x56: key.KeyCode = Key.V; break;
                case 0x42: key.KeyCode = Key.B; break;
                case 0x4E: key.KeyCode = Key.N; break;
                case 0x4D: key.KeyCode = Key.M; break;

                case 0x60: key.KeyCode = Key.Num0; break;
                case 0x61: key.KeyCode = Key.Num1; break;
                case 0x62: key.KeyCode = Key.Num2; break;
                case 0x63: key.KeyCode = Key.Num3; break;
                case 0x64: key.KeyCode = Key.Num4; break;
                case 0x65: key.KeyCode = Key.Num5; break;
                case 0x66: key.KeyCode = Key.Num6; break;
                case 0x67: key.KeyCode = Key.Num7; break;
                case 0x68: key.KeyCode = Key.Num8; break;
                case 0x69: key.KeyCode = Key.Num9; break;

                case 0x6F: key.KeyCode = Key.NumSlash; break;
                case 0x6A: key.KeyCode = Key.NumAsterisk; break;
                case 0x6D: key.KeyCode = Key.NumMinus; break;
                case 0x6B: key.KeyCode = Key.NumPlus; break;

                case 0x26: key.KeyCode = Key.Up; break;
                case 0x28: key.KeyCode = Key.Down; break;
                case 0x25: key.KeyCode = Key.Left; break;
                case 0x27: key.KeyCode = Key.Right; break;

                case 0xDB: key.KeyCode = Key.LSquare; break;
                case 0xDD: key.KeyCode = Key.RSquare; break;
                case 0xBA: key.KeyCode = Key.Colon; break;
                case 0xDE: key.KeyCode = Key.Quotes; break;
                case 0xDC: key.KeyCode = Key.BackSlash; break;
                case 0xBC: key.KeyCode = Key.LAngle; break;
                case 0xBE: key.KeyCode = Key.RAngle; break;
                case 0xBF: key.KeyCode = Key.Slash; break;

                case 0x2D: key.KeyCode = Key.Ins; break;
                case 0x2E: key.KeyCode = Key.Del; break;
                case 0x24: key.KeyCode = Key.Home; break;
                case 0x23: key.KeyCode = Key.End; break;
                case 0x21: key.KeyCode = Key.PageUp; break;
                case 0x22: key.KeyCode = Key.PageDown; break;
            }

            return key;
        }

        private void PollingThreadProc()
        {
            while (!stopPolling)
            {
                KeyInfo key = PollKeyboard();

                if (key != null)
                {
                    if (key.KeyCode != Key.None)
                    {
                        OnKeyInput(key);

                        foreach (var w in windows)
                        {
                            if (w.IsActive())
                                w.OnKeyInput(key);
                        }
                    }
                }

                Thread.Sleep(10);
            }
        }

        #endregion "Console Input"


    }

}
