/*++

Copyright (c)

Module Name:

    Cmd.h

Abstract:

    Command processor.

--*/

#pragma once

#define CMD_MAXARGS 256

//
// Command handler
//

typedef NTSTATUS (*COMMAND_HANDLER) ( IN PCHAR * Args, IN LONG NumArgs );

//
// API
//

NTSTATUS CmdAddCommand ( IN PCHAR CommandName, IN COMMAND_HANDLER Handler );

NTSTATUS CmdRemoveCommand ( IN PCHAR CommandName );

NTSTATUS CmdExecute ( IN PCHAR CommandLine );
