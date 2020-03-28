// DVD interface

#pragma once

// set current DVD image for read/seek/open file operations
// return 1 if no errors, and 0 if cannot use file
bool DVDSetCurrent(const TCHAR *file);

// seek and read operations on current DVD
void DVDSeek(int position);
void DVDRead(void *buffer, int length);

// open file in DVD root. return file position, or 0 if no such file.
// note : current DVD must be selected first!
// example use : s32 banner = DVDOpenFile("/opening.bnr");
long DVDOpenFile(const char *dvdfile);
