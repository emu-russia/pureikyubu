# DvdOpenFile

This demo is designed to test the operation of the DVDConvertPathToEntrynum method.

The DVDConvertPathToEntrynum method is used to convert the file path to the index of the FST table record, which contains service information about the DVD file system.

## Usage

DvdOpenFile "/path/to/dir/or/file" FST.bin

You will need a copy of FST from disk, the easiest way is to make it with the command from debugger:

```
FileSave FST.bin DumpFst"
```
