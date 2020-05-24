using System;
using System.Collections.Generic;
using System.Linq;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace EventLogMonitor
{
    enum EventLogSubsystem
    {
        Unknown = 0,
        CP,
        PE,
        VI,
        GP,
        PI,
        CPU,
        MI,
        DSP,
        DI,
        AR,
        AI,
        AIS,
        SI,
        EXI,
        MC,
        DVD,
        AX,
    }

    enum EventLogType
    {
        Unknown = 0,
        DebugReport,		// From DBReport2
    }

    internal class RawEventLogEntry
    {
        public EventLogType type = EventLogType.Unknown;
        public byte[] data = null;
    }

    class EventLogEntry
    {
        public UInt64 timestamp = 0;
        public EventLogSubsystem subsystem = EventLogSubsystem.Unknown;
        public EventLogType type = EventLogType.Unknown;
        public byte[] data = null;
    }

    class EventLog
    {
        public static List<EventLogEntry> ParseLog (string text)
        {
            List<EventLogEntry> log = new List<EventLogEntry>();

            JObject eventLogJson = JObject.Parse(text);

            foreach (var obj in eventLogJson)
            {
                if (obj.Key == "subsystems")
                {
                    foreach (var sub in obj.Value)
                    {
                        ParseSubsystem(sub, log);
                    }
                }
            }

            return log;
        }

        private static void ParseSubsystem (JToken sub, List<EventLogEntry> log)
        {
            EventLogSubsystem subsystem = SubFromString(sub.ToObject<JProperty>().Name);

            foreach (var stamp in sub.Children())
            {
                EventLogEntry entry = new EventLogEntry();

                JProperty prop = stamp.First.ToObject<JProperty>();

                entry.timestamp = Convert.ToUInt64(prop.Name);
                entry.subsystem = subsystem;

                foreach (var inner in stamp.First.Children())
                {
                    entry.type = (EventLogType)Convert.ToInt64(inner.First.ToObject<JProperty>().Value);
                    entry.data = inner.Last.First.ToObject<JArray>().ToObject<byte[]>();
                }

                log.Add(entry);
            }
        }

        private static EventLogSubsystem SubFromString (string subName)
        {
            EventLogSubsystem subsystem = EventLogSubsystem.Unknown;

            switch (subName)
            {
                case "CP": return EventLogSubsystem.CP;
                case "PE": return EventLogSubsystem.PE;
                case "VI": return EventLogSubsystem.VI;
                case "GP": return EventLogSubsystem.GP;
                case "PI": return EventLogSubsystem.PI;
                case "CPU": return EventLogSubsystem.CPU;
                case "MI": return EventLogSubsystem.MI;
                case "DSP": return EventLogSubsystem.DSP;
                case "DI": return EventLogSubsystem.DI;
                case "AR": return EventLogSubsystem.AR;
                case "AI": return EventLogSubsystem.AI;
                case "AIStream": return EventLogSubsystem.AIS;
                case "SI": return EventLogSubsystem.SI;
                case "EXI": return EventLogSubsystem.EXI;
                case "MemCards": return EventLogSubsystem.MC;
                case "DVD": return EventLogSubsystem.DVD;
                case "AudioDAC": return EventLogSubsystem.AX;
            }

            return subsystem;
        }

    }

}
