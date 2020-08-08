#include "pch.h"

int main(int argc, char **argv)
{
	// Check parameters

	if (argc < 2)
	{
		printf("Use: DolwinPlayground <file>\n");
		return -1;
	}

	// Add UI methods

	UI::Jdi.JdiAddNode(UI_JDI_JSON, UIReflector);

	// Say hello

	printf("Dolwin Playground, emulator version %s\n", UI::Jdi.GetVersion().c_str());

	// Load file and run

	printf("Press any key to stop emulation...\n\n");

	UI::Jdi.LoadFile(argv[1]);
	UI::Jdi.Run();
	DebugStart();

	// Wait key press..

#ifdef _WINDOWS
	_getch();
#endif

#ifdef _LINUX
	getc(stdin);
#endif

	// Unload

	UI::Jdi.Unload();
	UI::Jdi.JdiRemoveNode(UI_JDI_JSON);
	DebugStop();

	printf("\nThank you for flying DolwinPlayground airlines!\n");
	return 0;
}
