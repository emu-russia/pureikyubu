using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using System.Runtime.InteropServices;

namespace ManagedUi
{
    class DolwinNativeDll
    {
        [DllImport("DolwinNative.dll", CallingConvention=CallingConvention.Cdecl)]
        public static extern void Test();

        // set current DVD image for read/seek/open file operations
        // return 1 if no errors, and 0 if cannot use file
        [DllImport("DolwinNative.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool _N_DVDSetCurrent([MarshalAs(UnmanagedType.LPStr)] string file);

        // seek and read operations on current DVD
        [DllImport("DolwinNative.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void _N_DVDSeek(int position);

        [DllImport("DolwinNative.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void _N_DVDRead(IntPtr buffer, int length);

        // open file in DVD root. return file position, or 0 if no such file.
        // note : current DVD must be selected first!
        // example use : s32 banner = DVDOpenFile("/opening.bnr");
        [DllImport("DolwinNative.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern long _N_DVDOpenFile([MarshalAs(UnmanagedType.LPStr)] string dvdfile);

    }
}
