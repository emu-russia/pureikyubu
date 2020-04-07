// GC DVD ADPCM Decoder

#pragma once

void DvdAudioInitDecoder();
void DvdAudioDecode(uint8_t adpcmBuffer[32], uint16_t pcmBuffer[2 * 28]);
