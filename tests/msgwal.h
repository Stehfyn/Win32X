/*
 * msgwal.h -- shared layout for the cross-process window-message write-ahead log (WAL).
 *
 * Tests.exe creates a named shared-memory section holding a single-producer/single-consumer ring of
 * fixed-size binary records, then installs WH_GETMESSAGE + WH_CALLWNDPROC hooks (msgwal_hook.c, built as
 * MsgWalHook.dll) onto the GUI thread of the launched WindowsProject.exe. The hook procedures run IN the
 * app's GUI thread context: they do nothing but stamp a record and publish it -- no disk, no allocation,
 * no lock -- so the message pump is never stalled (the "write-ahead": the record is committed BEFORE the
 * message is dispatched/processed). A logger thread in Tests.exe drains the ring, formats each record by
 * message id, and writes the text log to disk off the GUI thread.
 *
 * Pure Win32 types only: this header is included by the CRT-free /NODEFAULTLIB Tests.exe and by the hook
 * DLL alike.
 */
#ifndef MSGWAL_H
#define MSGWAL_H

#include <windows.h>

/* Session-local section + log path knobs. Same logon session for Tests.exe and the app it launches, so
   the Local\ namespace is visible to both. */
#define MSGWAL_MAPPING_NAME TEXT("Local\\Win32XMsgWal")
#define MSGWAL_CAPACITY     65536u   /* ring slots, power of two (index masked) */
#define MSGWAL_TEXT_CCH     32u      /* inline decoded-text bytes (e.g. WM_SETTINGCHANGE section)        */

/* Record source: which hook produced it. */
#define MSGWAL_SRC_GET  0u   /* WH_GETMESSAGE  -- a posted message pulled from the queue, pre-dispatch   */
#define MSGWAL_SRC_SENT 1u   /* WH_CALLWNDPROC -- a sent message, pre-window-procedure                   */

#pragma pack(push, 8)

/* One logged message. Fixed size so the ring is a flat array; 8-aligned. szText holds an in-process
   snapshot of a string-bearing lParam (only the producer, living in the app, can read that pointer). */
typedef struct
{
    LONGLONG  qpc;          /* QueryPerformanceCounter at capture                                       */
    DWORD     dwThreadId;   /* producing thread (the app GUI thread)                                    */
    DWORD     dwSource;     /* MSGWAL_SRC_*                                                             */
    ULONGLONG hwnd;         /* target HWND (as integer; cross-process, do not dereference)              */
    UINT      message;
    UINT      flags;        /* WH_GETMESSAGE: PM_REMOVE/PM_NOREMOVE; else 0                              */
    ULONGLONG wParam;
    ULONGLONG lParam;       /* raw value; pointers are in the app's address space                       */
    CHAR      szText[MSGWAL_TEXT_CCH]; /* NUL-terminated decoded text, or empty                          */
} MSGWAL_REC;

/* Ring header, at the base of the mapping; records[capacity] follow immediately after. writeIdx/readIdx
   are monotonic totals (never wrap); the slot is (idx & (capacity-1)). Single producer (the two hooks
   share one GUI thread), single consumer (the logger thread). */
typedef struct
{
    volatile LONGLONG writeIdx;   /* total records committed by the producer                            */
    volatile LONGLONG readIdx;    /* total records consumed by the logger                               */
    volatile LONGLONG dropped;    /* records discarded because the ring was full                        */
    LONGLONG qpcFreq;             /* QueryPerformanceFrequency (for the logger's ms conversion)         */
    DWORD    capacity;            /* == MSGWAL_CAPACITY                                                  */
    DWORD    recSize;             /* == sizeof(MSGWAL_REC), sanity across the two binaries              */
    DWORD    targetThreadId;      /* the app GUI thread the hooks are scoped to                          */
    DWORD    pad;
} MSGWAL_HDR;

#pragma pack(pop)

#define MSGWAL_TOTAL_BYTES (sizeof(MSGWAL_HDR) + (SIZE_T)MSGWAL_CAPACITY * sizeof(MSGWAL_REC))

/* Records array base, given a mapped header pointer. */
#define MSGWAL_RECS(pHdr) ((MSGWAL_REC*)((BYTE*)(pHdr) + sizeof(MSGWAL_HDR)))

#endif /* MSGWAL_H */
