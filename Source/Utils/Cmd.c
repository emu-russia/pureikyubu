/*++

Copyright (c)

Module Name:

    Cmd.c

Abstract:

    Command processor.

    Command handlers are located all around host application modules.

--*/

#include "dolphin.h"

//
// Private context
//

typedef struct _COMMAND_ENTRY
{
    LIST_ENTRY Entry;
    PCHAR Name;
    COMMAND_HANDLER Handler;
} COMMAND_ENTRY, *PCOMMAND_ENTRY;

static LIST_ENTRY CmdHead = { &CmdHead, &CmdHead };

PCOMMAND_ENTRY CmdGetCommandByName ( PCHAR Name )
{
    PLIST_ENTRY Entry;
    PCOMMAND_ENTRY CmdEntry;

    Entry = CmdHead.Flink;

    while ( Entry != &CmdHead )
    {
        CmdEntry = (PCOMMAND_ENTRY) Entry;

        if ( !Stricmp ( CmdEntry->Name, Name ) )
            return CmdEntry;

        Entry = Entry->Flink;
    }

    return NULL;
}

PCHAR CmdNextToken ( PCHAR * Source, BOOLEAN TrimSpaces )
{
    PCHAR Token;
    CHAR Temp[1024];
    LONG DestIndex;
    LONG MaxChars;
    CHAR Char;
    BOOLEAN Quotes;
    BOOLEAN DQuotes;
    BOOLEAN TrimQuotes;
    PCHAR TempPtr;

    Token = NULL;

    DestIndex = 0;
    MaxChars = sizeof(Temp) - 1;

    //
    // Skip whitespaces
    //

    while ( **Source <= ' ' && **Source ) (*Source)++;

    if ( **Source == 0 ) return NULL;

    //
    // Collect chars in token
    //

    Quotes = DQuotes = FALSE;

    while ( DestIndex < MaxChars )
    {
        Char = **Source;

        if ( Char == 0 ) break;
        
        if ( Char == '\'' && !DQuotes ) Quotes ^= 1;

        if ( Char == '\"' && !Quotes ) DQuotes ^= 1;

        if ( Quotes || DQuotes ) TrimQuotes = TRUE;

        if ( Char <= ' ' && !(Quotes || DQuotes) )
        {
            break;
        }
        
        Temp[DestIndex++] = Char;

        (*Source)++;
    }

    Temp[DestIndex] = 0;
    
    if ( DestIndex )
    {
        TempPtr = Temp;

        //
        // Remove quotes (if needed). Take care of open quotes.
        //

        if ( TrimQuotes )
        {
            if ( Temp[0] == '\'' || Temp[0] == '\"' ) TempPtr++;

            if ( Temp[DestIndex - 1] == '\'' || Temp[DestIndex - 1] == '\"' ) Temp[--DestIndex] = 0;
        }

        //
        // Remove spaces from the left and right (if requested)
        //

        if ( TrimSpaces )
        {
            while ( *TempPtr <= ' ' && *TempPtr ) TempPtr++;

            while ( Temp[DestIndex - 1] <= ' ' && Temp[DestIndex - 1] ) Temp[--DestIndex] = 0;
        }

        if ( *TempPtr ) AllocateString ( TempPtr, &Token );
    }

    return Token;
}

//
// Public API
//

NTSTATUS CmdAddCommand ( IN PCHAR CommandName, IN COMMAND_HANDLER Handler )
{
    NTSTATUS Status;
    PCOMMAND_ENTRY Cmd;

    if ( !ARGUMENT_PRESENT(CommandName) || !ARGUMENT_PRESENT(Handler) )
    {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Check if such command is already registered - just replace its command handler
    //

    Cmd = CmdGetCommandByName ( CommandName );

    if ( Cmd )
    {
        Cmd->Handler = Handler;
        return STATUS_SUCCESS;
    }

    //
    // Register command handler
    //

    Cmd = (PCOMMAND_ENTRY) malloc ( sizeof(COMMAND_ENTRY) );
    if ( Cmd == NULL )
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = AllocateString ( CommandName, &Cmd->Name );
    if ( !NT_SUCCESS(Status) )
    {
        free (Cmd);
        return Status;
    }
    Cmd->Handler = Handler;

    InsertTailList ( &CmdHead, (PLIST_ENTRY)Cmd );

    return STATUS_SUCCESS;
}

NTSTATUS CmdRemoveCommand ( IN PCHAR CommandName )
{
    NTSTATUS Status;
    PCOMMAND_ENTRY Cmd;

    if ( !ARGUMENT_PRESENT(CommandName) )
    {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Check if such command is registered
    //

    Cmd = CmdGetCommandByName (CommandName);

    if ( Cmd )
    {
        RemoveEntryList ( (PLIST_ENTRY)Cmd );

        Status = FreeString (Cmd->Name);
        if ( !NT_SUCCESS(Status) )
            return Status;

        free (Cmd);

        return STATUS_SUCCESS;
    }

    return STATUS_NOT_FOUND;
}

NTSTATUS CmdExecute ( IN PCHAR CommandLine )
{
    PCOMMAND_ENTRY Command;
    PCHAR CommandName;
    PCHAR NextArg;
    PCHAR ArgList[CMD_MAXARGS];
    LONG NumArgs;
    LONG MaxArgs;
    LONG Count;
    NTSTATUS Status;

    if ( !ARGUMENT_PRESENT(CommandLine) )
    {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Get command name (first token)
    // Exit, if there is no such command handler, or command line is empty
    //

    CommandName = CmdNextToken ( &CommandLine, TRUE );

    if ( CommandName == NULL )
    {
        return STATUS_NOT_FOUND;
    }

    Command = CmdGetCommandByName ( CommandName );

    if ( Command == NULL )
    {
        return STATUS_NOT_FOUND;
    }

    //
    // Build arguments list
    //

    NumArgs = 0;
    MaxArgs = sizeof(ArgList) / sizeof(PTCHAR);
    memset ( ArgList, 0, sizeof(ArgList) );

    while ( NumArgs < MaxArgs )
    {
        NextArg = CmdNextToken ( &CommandLine, FALSE );

        if ( NextArg ) ArgList[NumArgs++] = NextArg;
        else break;
    }

    //
    // Pass arguments list to handler
    //

    Status = Command->Handler ( ArgList, NumArgs );

    //
    // Clean-up
    //

    if ( CommandName ) FreeString (CommandName);

    for ( Count=0; Count<NumArgs; Count++ )
    {
        if ( ArgList[Count] ) FreeString ( ArgList[Count] );
    }

    return Status;
}
