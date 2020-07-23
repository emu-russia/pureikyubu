#include "pch.h"

int main(int argc, char **argv)
{
	// Check parameters

	if (argc < 2)
	{
		printf("Use: DolwinPlayground <file>\n");
		return -1;
	}

	// Say hello

	printf("Dolwin Playground, emulator version %s\n", UI::Jdi.GetVersion().c_str());

	// Load file and run

	printf("Press any key to stop emulation...\n");

	UI::Jdi.LoadFile(argv[1]);
	UI::Jdi.Run();

	_getch();

	// Unload

	UI::Jdi.Unload();

	printf("Thank you for flying DolwinPlayground airlines!\n");
	return 0;
}
