/*++

Copyright (c)

Module Name:

    Strutils.c

Abstract:

    Convenient string helpers.

--*/

#include "dolphin.h"

//
// ANSI Strings saving
//

NTSTATUS AllocateString ( IN PCHAR String, OUT PCHAR *Out)
{
    LONG Length;
    PCHAR Buffer;

    if ( !ARGUMENT_PRESENT(Out) )
    {
        return STATUS_INVALID_PARAMETER;
    }

    Length = (LONG)strlen ( String ) + 1;
    
    Buffer = (PCHAR)malloc ( Length );
    if ( Buffer == NULL )
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    strcpy ( Buffer, String );

    *Out = Buffer;

    return STATUS_SUCCESS;
}

NTSTATUS FreeString (IN PCHAR String)
{
    if ( !ARGUMENT_PRESENT(String) )
    {
        return STATUS_INVALID_PARAMETER;
    }

    free ( String );

    return STATUS_SUCCESS;
}

//
// TCHAR Strings saving
//

NTSTATUS AllocateTcharString ( IN PTCHAR String, OUT PTCHAR *Out)
{
    LONG Length;
    PTCHAR Buffer;

    if ( !ARGUMENT_PRESENT(Out) )
    {
        return STATUS_INVALID_PARAMETER;
    }

    Length = (LONG) _tcslen ( String ) * sizeof (TCHAR);
    
    Buffer = (PTCHAR)malloc ( Length + sizeof(TCHAR) );
    if ( Buffer == NULL )
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    _tcscpy ( Buffer, String );

    *Out = Buffer;

    return STATUS_SUCCESS;
}

NTSTATUS FreeTcharString (IN PTCHAR String)
{
    if ( !ARGUMENT_PRESENT(String) )
    {
        return STATUS_INVALID_PARAMETER;
    }

    free ( String );

    return STATUS_SUCCESS;
}

//
// ANSI <-> TCHAR String conversion
//

VOID CharStringToTcharString ( OUT PTCHAR Tstring, IN LONG MaxLength, IN PCHAR String )
{
    LONG Length;
    LONG i;

    if ( sizeof(TCHAR) == sizeof(CHAR) )
        strncpy ( (PCHAR)Tstring, String, MaxLength) ;
    else
    {
        Length = min ( MaxLength, (LONG)strlen ( String ) + 1 );

        for (i=0; i<Length; i++)
        {
            Tstring[i] = String[i];
        }

        Tstring[i] = 0;
    }
}

VOID TcharStringToCharString ( OUT PCHAR String, IN LONG MaxLength, IN PTCHAR Tstring )
{
    LONG Length;
    LONG i;

    if ( sizeof(TCHAR) == sizeof(CHAR) )
        strncpy ( String, (PCHAR)Tstring, MaxLength) ;
    else
    {
        Length = min ( MaxLength, (LONG)_tcslen ( Tstring ) + 1 );

        for (i=0; i<Length; i++)
        {
            String[i] = Tstring[i] & 0xff;
        }

        String[i] = 0;
    }
}

//
// Non-portable string ops.
//

int Stricmp ( char * str1, char * str2 )
{
    char c1, c2;
    int v;

    do {
        c1 = *str1++;
        c2 = *str2++;
        v = (unsigned)tolower(c1) - (unsigned)tolower(c2);
    } while ( (v == 0) && (c1 != '\0') && (c2 != '\0') );

    return v;
}
