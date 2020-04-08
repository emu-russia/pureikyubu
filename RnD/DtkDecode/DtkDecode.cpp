// Demonstration and verification of the GC DVD Audio decoder

#include <iostream>

#include "../../SRC/DVD/DvdAdpcmDecode.h"

int main(int argc, char **argv)
{
	FILE* adpcmFile;
	FILE* pcmFile;

	fopen_s(&adpcmFile, argv[1], "rb");
	if (!adpcmFile)
	{
		std::cout << "Specify input ADPCM file (*.adp)" << std::endl;
		return 0;
	}

	fopen_s(&pcmFile, "output.pcm", "wb");
	if (!adpcmFile)
	{
		std::cout << "Cannot create output.pcm" << std::endl;
		return 0;
	}

	DvdAudioInitDecoder();

	while (!feof(adpcmFile))
	{
		uint8_t adpcmData[32];
		uint16_t decodedPcmData[2 * 28];

		fread(adpcmData, 1, 32, adpcmFile);

		DvdAudioDecode(adpcmData, decodedPcmData);

		fwrite(decodedPcmData, 1, sizeof(decodedPcmData), pcmFile);
	}

	fclose(adpcmFile);
	fclose(pcmFile);
}
