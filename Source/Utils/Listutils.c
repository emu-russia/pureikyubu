/*++

Copyright (c)

Module Name:

    Listutils.c

Abstract:

    Double-linked lists API.

--*/

#include "dolphin.h"

BOOLEAN IsListEmpty ( PLIST_ENTRY ListHead )
{
    return ( ListHead->Flink == ListHead );
}

VOID InitializeListHead ( PLIST_ENTRY ListHead )
{
    ListHead->Flink = ListHead->Blink = ListHead;
}

VOID InsertHeadList ( PLIST_ENTRY ListHead, PLIST_ENTRY Entry )
{
    PLIST_ENTRY Flink;

    Flink = ListHead->Flink;
    Entry->Flink = Flink;
    Entry->Blink = ListHead;
    Flink->Blink = Entry;
    ListHead->Flink = Entry;
}

VOID InsertTailList ( PLIST_ENTRY ListHead, PLIST_ENTRY Entry )
{
    PLIST_ENTRY Backup;

    Backup = ListHead->Blink;
    Entry->Flink = ListHead;
    Entry->Blink = Backup;
    Backup->Flink = Entry;
    ListHead->Blink = Entry;
}

VOID RemoveEntryList ( PLIST_ENTRY Entry )
{
    Entry->Blink->Flink = Entry->Flink;
    Entry->Flink->Blink = Entry->Blink;
}
