// Dolwin configuration

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using System.Xml.Serialization;
using System.IO;

namespace ManagedUi
{
    public class Config
    {
        const string ConfigPath = "Data\\config.xml";

        public class Properties
        {
            public UInt32 BinOrg = 0x3100;      // binary file loading offset (physical address)
            public int CpuCf = 1;               // CPU counter factor
            public int CpuDelay = 12;           // TBR/DEC update delay (number of instructions)
            public int CpuTime = 1;             // CPU bailout initial time
            public bool DebugEnabled = false;   // enable debugger
            public bool MakeMap = true;         // true: make map file, if missing (find symbols)
            public bool Mmu = false;            // memory translation mode (false: simple, true: mmu)
            public bool Patches = true;         // patches allowed, if true

            public string AnsiFont = "Data\\Arial 16.szp";  // bootrom ANSI font
            public string LastDirAll = ".\\";   // last used directory (all files)
            public string LastDirDvd = ".\\";   // last used directory (dvd)
            public string LastDirMap = ".\\Data";   // last used directory (map)
            public string LastDirPatch = ".\\Data";     // last used directory (patch)
            public string LastFile = null;      // last loaded file
            public List<string> SelectorPath = new List<string>() { ".\\", "c:\\" };  // path string for selector
            public bool Profiler = true;        // true: enable emu profiler
            public List<string> RecentFiles = new List<string>();   // recent files
            public bool Selector = true;        // selector disabled, if false
            public string SjisFont = "Data\\Lucida 16.szp";     // bootrom SJIS font
            public bool SmallIcons = false;     // show small icons, if true

            public UInt32 ConsoleVersion = 3;   // console version (see YAGCD), 0x00000003: latest production board
            public bool DspFake = true;         // true: use fake DSP mode (very dirty hack)
            public bool ExiLog = true;          // true: log EXI activities
            public bool GxPoll = false;         // true: poll controllers after GX draw done
            public bool OsReport = true;        // true: allow debugger output (by EXI). Require devkit ConsoleVersion value
            public bool RswHack = true;         // reset button hack
            public bool Rtc = false;            // true: real-time clock enabled
            public int ViCount = 0;             // lines count per single frame (0:auto)
            public bool ViLog = true;           // do debugger log output
            public bool ViStretch = false;      // true: stretch VI framebuffer to fit whole window
            public bool ViXfb = true;           // enable video frame buffer (GDI)

            public bool HleMtx = false;         // true: use matrix library HLE
        }

        public Properties Settings = new Properties();

        public void Load()
        {
            if ( !File.Exists(ConfigPath))
            {
                Settings = new Properties();
                return;
            }

            XmlSerializer ser = new XmlSerializer(typeof(Properties));
            using (FileStream fs = new FileStream(ConfigPath, FileMode.Open))
            {
                Settings = (Properties)ser.Deserialize(fs);
            }
        }

        public void Save()
        {
            XmlSerializer ser = new XmlSerializer(typeof(Properties));
            using (FileStream fs = new FileStream(ConfigPath, FileMode.Create))
            {
                ser.Serialize(fs, Settings);
            }
        }

        public void Reset()
        {
            Settings = new Properties();
        }

    }
}
