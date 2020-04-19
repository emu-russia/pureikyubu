using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using System.IO;
using Newtonsoft.Json;

namespace SamplingProfiler
{
    class SampleDataObject
    {
        public UInt64 [] sampleData = null;
    }

    class Sample
    {
        public UInt64 time = 0;
        public UInt32 pc = 0;
    }

    class Segment
    {
        public UInt64 timeTotal = 0;
        public int hits = 0;
    }

    class SampleData
    {
        public List<Sample> samples = new List<Sample>();
        public Dictionary<UInt32, Segment> segments = new Dictionary<uint, Segment>();

        public void Append(string filename)
        {
            SampleDataObject json = JsonConvert.DeserializeObject<SampleDataObject>(File.ReadAllText(filename, Encoding.UTF8));

            UInt64 prevValue = 0;
            int count = 0;

            foreach (var value in json.sampleData)
            {
                count++;
                if (count == 2)
                {
                    Sample sample = new Sample();

                    sample.time = prevValue;
                    sample.pc = (UInt32)value;

                    samples.Add(sample);

                    count = 0;
                    continue;
                }

                prevValue = value;
            }
        }

        public void Clear()
        {
            samples.Clear();
        }

        public SortedDictionary<UInt32, Segment> Analyze()
        {
            SortedDictionary<UInt32, Segment> segments = new SortedDictionary<uint, Segment>();

            Sample prevSample = null;

            foreach (var sample in samples)
            {
                UInt32 baseAddress = sample.pc & 0xffffffe0;

                if (!segments.ContainsKey(baseAddress))
                {
                    segments[baseAddress] = new Segment();
                }

                segments[baseAddress].hits++;
                if (prevSample != null)
                {
                    segments[baseAddress].timeTotal += sample.time - prevSample.time;
                }

                prevSample = sample;
            }

            return segments;
        }

    }


}
