# Event Log

The Event Log component is used to collect events during host emulation (GameCube). Each event contains a time stamp 
(by the value of the Gekko TBR register), the category of the subsystem where the event occurred (`DbgChannel`), 
and the type of event (parameterized event or debug message).

## Parameterized Events

A parameterized event is an ordinary event whose parameter is an arbitrary data set (`vector<uint8_t>&`).
Data along with the type of event is saved in event history.

## Debug Messages

Debug messages are output by the methods `DBReport`, `DBReport2`, and `DBHalt`. This is a historically traditional way of debugging in Dolwin.

After implementing Event Log facility, debug messages displayed by the `DBReport2` method are additionally saved in the event history
(since `DBReport2` contains the `DbgChannel` parameter).

The usual debug messages output by the `DBReport` method, as well as the emulator debug stops (`DBHalt`), do not saved into the event history.

## Event History

The history of events is stored as Json in memory. The history of events can be viewed any time after the start of the emulation 
using the Event Log Monitor tool.

## Json Sample

```json
{
	"subsystems":
	{
		"CPU":			// DbgChannel
		{
			"234234234234":		// Gekko TBR value
			{
				"type": 1,			// EventType
				"data": [ ... ]		// Arbitrary data for parameterized events or debug message text 
			}
		},
		"DSP":
		{
			"32423423":
			{
				"type": 1,
				"data": [ ... ]
			}
		}
	}
}
```

## Events Flow

![EventLogFlow](EventLogFlow.png)
