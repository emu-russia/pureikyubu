# DVD

This component implements everything you need for a healthy emulation of the GameCube disk drive unit (DDU).

There are currently two ways to read virtual DVDs:
- Read sectors of a mounted GC DVD image (GCM)
- Reading sectors of a virtual disk mounted as a DolphinSDK folder. Required for comfortable launch of DolphinSDK demos

There are thoughts to add emulation of DVD Firmaware, so as not to emulate the HLE execution of disk commands and research the DVD firmware. But this is not a priority.

## DDU JDI

The debugging interface specification provided by this component is in Data\\DduJdi.json.
