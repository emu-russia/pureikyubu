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
        public static string CallJdi(string request)
        {
            var replyRaw = new char[256];
            string reply = new string(replyRaw);
            CallJdiReturnJson(request, out reply, reply.Length);
            return reply;
        }

        [DllImport("DolwinEmu.dll", CharSet=CharSet.Ansi, CallingConvention=CallingConvention.Cdecl)]
        public static extern void CallJdiReturnJson(string request, out string reply, int replySize);

    }
}
