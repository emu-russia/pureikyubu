# DspAdpcmDecoder

This demo uses DspCore pieces from the emulator to test the DSP ADPCM decoder.

The input is encoded data (e.g. \\dvddata\\axdemo\\stream\\left.adpcm). The format of this file is:
- First byte: Predictor/Scale 
- Next 7 bytes: encoded data
- Repeat

Coefficients A0-A15 are taken from the code (not stored with the data).

The output is RAW Big-Endian Signed PCM16 (out.bin), which can then be checked in Audacity or another similar program for playing RAW samples.
