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
    }
}
