/*++

Copyright (c)

Module Name:

    FileWrap.c

Abstract:

    File wrapper

--*/

#include "dolphin.h"

NTSTATUS FileOpen ( IN PCHAR FileName, IN BOOLEAN ForRead, OUT PVOID * Handle )
{
    FILE * File;

    if ( !ARGUMENT_PRESENT(Handle) )
        return STATUS_INVALID_PARAMETER;

    if ( ForRead ) File = fopen ( FileName, "rb" );
    else
    {
        File = fopen ( FileName, "rb+" );
        if ( File ) fseek ( File, 0, SEEK_SET );
        if ( File == NULL ) File = fopen ( FileName, "wb" );
    }

    if ( File == NULL )
        return STATUS_UNSUCCESSFUL;

    *Handle = (PVOID)File;

    return STATUS_SUCCESS;
}

NTSTATUS FileClose ( IN PVOID Handle )
{
    if ( !ARGUMENT_PRESENT(Handle) )
        return STATUS_INVALID_PARAMETER;

    fflush ( (FILE *)Handle );
    fclose ( (FILE *)Handle );

    return STATUS_SUCCESS;
}

NTSTATUS FileRead ( IN PVOID Handle, IN PVOID Buffer, IN ULONG Length, OUT PULONG Readed OPTIONAL )
{
    size_t Bytes;

    Bytes = fread ( Buffer, 1, Length, (FILE *)Handle );

    if ( Readed )
        *Readed = (ULONG)Bytes;

    return STATUS_SUCCESS;
}

NTSTATUS FileWrite ( IN PVOID Handle, IN PVOID Buffer, IN ULONG Length, OUT PULONG Written OPTIONAL )
{
    size_t Bytes;

    Bytes = fwrite ( Buffer, 1, Length, (FILE *)Handle );

    if ( Written ) 
        *Written = (ULONG) Bytes;

    return STATUS_SUCCESS;
}

NTSTATUS FileSize ( IN PVOID Handle, OUT PULONG SizeOut )
{
    int Old;
    long Size;

    if ( !ARGUMENT_PRESENT(SizeOut) )
        return STATUS_INVALID_PARAMETER;

    Old = ftell ( (FILE*)Handle );

    fseek ( (FILE*)Handle, 0, SEEK_END );
    Size = ftell ( (FILE*)Handle );

    fseek ( (FILE*)Handle, Old, SEEK_SET );

    *SizeOut = Size;

    return STATUS_SUCCESS;
}

NTSTATUS FileSeek ( IN PVOID Handle, IN ULONG Offset )
{
    fseek ( (FILE *)Handle, Offset, SEEK_SET);

    return STATUS_SUCCESS;
}
