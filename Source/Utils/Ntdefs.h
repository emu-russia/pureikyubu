/*++

Copyright (c)

Module Name:

    Ntdefs.h

Abstract:

    Some defines from Ntoskrnl.

--*/

#pragma once

//
// Base data types
//

#define NTAPI   __stdcall

#define CONST   const

#define VOID void
typedef char CHAR, *PCHAR;
typedef short SHORT;
#if defined(_WIN32)
typedef long LONG;
#else
typedef int LONG;
#endif

typedef VOID *PVOID;

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
#if defined(_WIN32)
typedef unsigned long ULONG;
#else
typedef unsigned int ULONG;
#endif

typedef UCHAR *PUCHAR;
typedef USHORT *PUSHORT;
typedef ULONG *PULONG;

//
// long long is supported almost everywhere now
//

typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;

typedef long long LONG64, *PLONG64;

typedef signed char         INT8, *PINT8;
typedef signed short        INT16, *PINT16;
typedef signed int          INT32, *PINT32;
typedef signed long long    INT64, *PINT64;
typedef unsigned char       UINT8, *PUINT8;
typedef unsigned short      UINT16, *PUINT16;
typedef unsigned int        UINT32, *PUINT32;
typedef unsigned long long  UINT64, *PUINT64;

typedef PVOID HANDLE;
typedef HANDLE *PHANDLE;

typedef LONG NTSTATUS;

#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

typedef UCHAR BOOLEAN;
typedef BOOLEAN *PBOOLEAN;

#define FALSE   0
#define TRUE    1

#define ARGUMENT_PRESENT(ArgumentPointer)    (\
    (CHAR *)((PVOID)(ArgumentPointer) != 0 ) )

#ifndef _WIN32
#define CONTAINING_RECORD(address, type, field) ((type *)( \
                                                  (PCHAR)(address) - \
                                                  (PCHAR)(&((type *)0)->field)))
#endif

#ifndef _WIN32
typedef struct _LIST_ENTRY {
   struct _LIST_ENTRY *Flink;
   struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY;
#endif

//
// Parameter prefixes
//

#define IN
#define OUT
#define INOUT
#define OPTIONAL

//
// Status
//

#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED           ((NTSTATUS)0xC0000002L)
#ifndef _WIN32
#define STATUS_ACCESS_VIOLATION          ((NTSTATUS)0xC0000005L)
#endif
#ifndef _WIN32
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#endif
#define STATUS_INSUFFICIENT_RESOURCES    ((NTSTATUS)0xC000009AL)
#define STATUS_CONNECTION_DISCONNECTED   ((NTSTATUS)0xC000020CL)
#define STATUS_NOT_FOUND                 ((NTSTATUS)0xC0000225L)
#define STATUS_DATATYPE_MISALIGNMENT_ERROR ((NTSTATUS)0xC00002C5L)
#define STATUS_HEAP_CORRUPTION           ((NTSTATUS)0xC0000374L)
