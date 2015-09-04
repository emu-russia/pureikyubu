/*++

Copyright (c)

Module Name:

    Listutils.h

Abstract:

    Double-linked lists API

--*/

#pragma once

BOOLEAN IsListEmpty ( PLIST_ENTRY ListHead );

VOID InitializeListHead ( PLIST_ENTRY ListHead );

VOID InsertHeadList ( PLIST_ENTRY ListHead, PLIST_ENTRY Entry );

VOID InsertTailList ( PLIST_ENTRY ListHead, PLIST_ENTRY Entry );

VOID RemoveEntryList ( PLIST_ENTRY Entry );
