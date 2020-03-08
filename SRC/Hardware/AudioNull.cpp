// Null audio
#include "pch.h"

bool AXOpen()
{
	return true;
}

void AXClose()
{ }

// play DMA audio buffer (big-endian, 16-bit), AXPlayAudio(0, 0) - to stop.
void AXPlayAudio(void* buffer, long length)
{ }

// set DMA sample rate (32000/48000), stream sample rate is always 48000.
void AXSetRate(long rate)
{ }

// play stream data (raw data, read from DVD), AXPlayStream(0, 0) - to stop.
void AXPlayStream(void* buffer, long length)
{ }

// set stream volume (0..255), you cant set DMA volume in hardware.
void AXSetVolume(unsigned char left, unsigned char right)
{ }
