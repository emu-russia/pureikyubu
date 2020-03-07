// DVD interface

#pragma once

// return values are always 1 for good, and 0 for bad.

// set current DVD image for read/seek/open file operations
// return 1 if no errors, and 0 if cannot use file
long DVDSetCurrent(char *file);

// seek and read operations on current DVD
void DVDSeek(int position);
void DVDRead(void *buffer, int length);

// open file in DVD root. return file position, or 0 if no such file.
// note : current DVD must be selected first!
// example use : s32 banner = DVDOpenFile("/opening.bnr");
long DVDOpenFile(char *dvdfile);
