/*++

Copyright (c)

Module Name:

    Strutils.h

Abstract:

    Convenient string helpers.

--*/

#pragma once

NTSTATUS AllocateString ( IN PCHAR String, OUT PCHAR *Out);
NTSTATUS FreeString (IN PCHAR String);
NTSTATUS AllocateTcharString ( IN PTCHAR String, OUT PTCHAR *Out);
NTSTATUS FreeTcharString (IN PTCHAR String);
VOID CharStringToTcharString ( OUT PTCHAR Tstring, IN LONG MaxLength, IN PCHAR String );
VOID TcharStringToCharString ( OUT PCHAR String, IN LONG MaxLength, IN PTCHAR Tstring );
int Stricmp ( char * str1, char * str2 );
