/*++

Copyright (c)

Module Name:

    FileWrap.h

Abstract:

    File wrapper.
    
--*/

#pragma once

#define FILE_OPEN_READ TRUE
#define FILE_OPEN_WRITE FALSE

NTSTATUS FileOpen ( IN PCHAR FileName, IN BOOLEAN ForRead, OUT PVOID * Handle );

NTSTATUS FileClose ( IN PVOID Handle );

NTSTATUS FileRead ( IN PVOID Handle, IN PVOID Buffer, IN ULONG Length, OUT PULONG Readed OPTIONAL );

NTSTATUS FileWrite ( IN PVOID Handle, IN PVOID Buffer, IN ULONG Length, OUT PULONG Written OPTIONAL );

NTSTATUS FileSize ( IN PVOID Handle, OUT PULONG SizeOut );

NTSTATUS FileSeek ( IN PVOID Handle, IN ULONG Offset );

NTSTATUS FileDemo ( IN PCHAR FileName );
