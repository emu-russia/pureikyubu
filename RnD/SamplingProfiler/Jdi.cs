using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using System.Runtime.InteropServices;

namespace SamplingProfiler
{
    class Jdi
    {
        [DllImport("DolwinNative.dll")]
        public static extern void InitEmu();

        [DllImport("DolwinNative.dll")]
        public static extern void ShutdownEmu();

        [DllImport("DolwinNative.dll", CharSet=CharSet.Ansi)]
        public static extern string CallJdi(string request);

    }
}
