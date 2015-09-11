// bin2c.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

unsigned char buffer[65*1024];

int _tmain(int argc, _TCHAR* argv[])
{
	FILE *in;

	if (argc < 2) return -1;
	in = fopen(argv[1], "rb");
	if (in == NULL) return -1;
	int size = fread(buffer, 1, 10000, in);
	
	size = (size + 15) & ~0xf;
	int i, j;

	printf("unsigned int %s[] __attribute__ ((aligned (64)))  =\n", strtok(argv[1], "."));
	printf("{\n");
	for (i = 0 ; i < size ; i+=16)
	{
		printf("\t");
		for(j = i ; j < (i + 16) ; j += 4)
		{
			printf("0x%02x%02x%02x%02x", buffer[j], buffer[j + 1], buffer[j + 2], buffer[j + 3]);
			printf(", ");
		}
		printf("\n");
	}
	printf("};\n");

	fclose(in);
	return 0;
}

