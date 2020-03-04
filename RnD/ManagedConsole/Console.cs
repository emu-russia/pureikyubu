using System;
using System.Collections.Generic;
using System.Drawing;

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
        List<Window> windows = new List<Window>();

        public void Allocate(string title, Size size)
        {
            if (allocated)
                return;

            buf = new CHAR_INFO[size.Width * size.Height];
            consize = new Size(size.Width, size.Height);
            pos = new Point(0, 0);

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

            allocated = true;
        }

        public void Free()
        {
            if (!allocated)
                return;

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

        #endregion "Console Output"



        #region "Console Input"

        #endregion "Console Input"


    }

}
