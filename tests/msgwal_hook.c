/*
 * msgwal_hook.c -- MsgWalHook.dll: WH_GETMESSAGE + WH_CALLWNDPROC hook procedures injected by Tests.exe
 * into the launched WindowsProject.exe GUI thread. Each procedure stamps the message into the shared-
 * memory ring (msgwal.h) and returns immediately -- no disk, no heap, no lock -- so the app's message
 * pump is never stalled. The record is committed BEFORE the message is dispatched (WH_GETMESSAGE) or
 * before the window procedure runs (WH_CALLWNDPROC): the write-ahead guarantee.
 *
 * These procedures execute in the APP's address space, which is why string-bearing lParams (the
 * WM_SETTINGCHANGE section) are snapshotted here -- only the producer can dereference that pointer.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "msgwal.h"

#define MSGWAL_API __declspec(dllexport)

static HANDLE      g_hMap;
static MSGWAL_HDR* g_pHdr;
static LONG        g_lInitTried;

/* Lazily attach to the section Tests.exe created. Single-threaded (both hooks run on the one GUI thread
   the hook is scoped to), so the one-shot guard needs no fence beyond the interlocked flip. */
static void MsgWalEnsureMapped(void)
{
    if (g_pHdr)
    {
        return;
    }
    if (InterlockedExchange(&g_lInitTried, 1) != 0)
    {
        return;
    }
    g_hMap = OpenFileMapping(FILE_MAP_WRITE, FALSE, MSGWAL_MAPPING_NAME);
    if (!g_hMap)
    {
        return;
    }
    g_pHdr = (MSGWAL_HDR*)MapViewOfFile(g_hMap, FILE_MAP_WRITE, 0, 0, MSGWAL_TOTAL_BYTES);
    if (g_pHdr && (g_pHdr->recSize != (DWORD)sizeof(MSGWAL_REC)))
    {
        /* Layout mismatch between the two binaries: refuse to write rather than corrupt the ring. */
        UnmapViewOfFile(g_pHdr);
        g_pHdr = NULL;
    }
}

/* Snapshot a string lParam (WM_SETTINGCHANGE section) from the app's address space into the record.
   SEH-guarded: a malformed/NULL pointer leaves the field empty instead of faulting the GUI thread. */
static void MsgWalSnapshotText(UINT message, LPARAM lParam, CHAR* pszOut)
{
    pszOut[0] = '\0';
    if (message != WM_SETTINGCHANGE || lParam == 0)
    {
        return;
    }
    __try
    {
        (void)WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)lParam, -1, pszOut,
                                  (int)MSGWAL_TEXT_CCH, NULL, NULL);
        pszOut[MSGWAL_TEXT_CCH - 1] = '\0';
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        pszOut[0] = '\0';
    }
}

static void MsgWalPush(DWORD dwSource, UINT flags, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    MSGWAL_HDR*   pHdr;
    MSGWAL_REC*   pRec;
    LONGLONG      w0;
    LARGE_INTEGER qpc;

    MsgWalEnsureMapped();
    pHdr = g_pHdr;
    if (!pHdr)
    {
        return;
    }
    w0 = pHdr->writeIdx;
    if ((w0 - pHdr->readIdx) >= (LONGLONG)pHdr->capacity)
    {
        InterlockedIncrement64(&pHdr->dropped);
        return;
    }
    pRec = &MSGWAL_RECS(pHdr)[w0 & (pHdr->capacity - 1)];
    QueryPerformanceCounter(&qpc);
    pRec->qpc        = qpc.QuadPart;
    pRec->dwThreadId = GetCurrentThreadId();
    pRec->dwSource   = dwSource;
    pRec->hwnd       = (ULONGLONG)(ULONG_PTR)hwnd;
    pRec->message    = message;
    pRec->flags      = flags;
    pRec->wParam     = (ULONGLONG)wParam;
    pRec->lParam     = (ULONGLONG)lParam;
    MsgWalSnapshotText(message, lParam, pRec->szText);
    /* Commit: the record bytes must be visible before the index advance the consumer reads. */
    MemoryBarrier();
    InterlockedExchangeAdd64(&pHdr->writeIdx, 1);
}

/* WH_GETMESSAGE: posted messages, pre-dispatch. Log only PM_REMOVE retrievals (the ones that will be
   dispatched); PM_NOREMOVE peeks would double-count. */
MSGWAL_API LRESULT CALLBACK MsgWalGetMsgProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && wParam == PM_REMOVE)
    {
        const MSG* pMsg = (const MSG*)lParam;
        if (pMsg)
        {
            MsgWalPush(MSGWAL_SRC_GET, (UINT)wParam, pMsg->hwnd, pMsg->message, pMsg->wParam, pMsg->lParam);
        }
    }
    return CallNextHookEx(NULL, code, wParam, lParam);
}

/* WH_CALLWNDPROC: sent messages, before the window procedure runs. */
MSGWAL_API LRESULT CALLBACK MsgWalCallWndProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
        const CWPSTRUCT* pCwp = (const CWPSTRUCT*)lParam;
        if (pCwp)
        {
            MsgWalPush(MSGWAL_SRC_SENT, 0u, pCwp->hwnd, pCwp->message, pCwp->wParam, pCwp->lParam);
        }
    }
    return CallNextHookEx(NULL, code, wParam, lParam);
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID pReserved)
{
    UNREFERENCED_PARAMETER(pReserved);
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hInst);
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        if (g_pHdr)
        {
            UnmapViewOfFile(g_pHdr);
            g_pHdr = NULL;
        }
        if (g_hMap)
        {
            CloseHandle(g_hMap);
            g_hMap = NULL;
        }
    }
    return TRUE;
}
