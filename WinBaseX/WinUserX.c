/*
 * WinUserX.c -- charset-agnostic window-placement helpers complementing WinUser's window APIs:
 * launch-monitor resolution, startup-rectangle policy, and the startup show sequence. No CRT.
 */

#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "WinUserX.h"
#include "windefx.h"

#ifndef STARTF_HASSHELLDATA
#define STARTF_HASSHELLDATA 0x00000400
#endif

#define DEFAULT_PCT_NUM 50
#define DEFAULT_PCT_DEN 100

HMONITOR WINAPI MonitorFromStartupInfo(_In_opt_ const STARTUPINFO* psi, _In_ DWORD dwFlags)
{
    POINT pt;
    BOOL  fHasShellData;
    BOOL  fUsePosition;

    pt.x = 0;
    pt.y = 0;
    if (NULL == psi)
    {
        return MonitorFromPoint(pt, dwFlags);
    }

    fHasShellData = IsFlagSet(psi->dwFlags, STARTF_HASSHELLDATA);
    if (fHasShellData)
    {
        return (HMONITOR)psi->hStdOutput;
    }

    fUsePosition = IsFlagSet(psi->dwFlags, STARTF_USEPOSITION);
    if (fUsePosition)
    {
        pt.x = (LONG)psi->dwX;
        pt.y = (LONG)psi->dwY;
    }
    return MonitorFromPoint(pt, dwFlags);
}

_Success_(return != FALSE)
BOOL WINAPI CalculateWindowStartupPosition(_In_ const SIZE* pDefaultSize, _Out_ RECT* prcOut)
{
    STARTUPINFO si;
    HMONITOR    hMon;
    MONITORINFO mi;
    BOOL        fGotInfo;
    LONG        nWorkWidth;
    LONG        nWorkHeight;
    BOOL        fUseSize;
    SIZE        size;
    BOOL        fUsePosition;
    LONG        nLeft;
    LONG        nTop;
    LONG        nOffsetX;
    LONG        nOffsetY;
    BOOL        fOffsetXNonNeg;
    BOOL        fOffsetYNonNeg;

    if ((NULL == pDefaultSize) || (NULL == prcOut))
    {
        return FALSE;
    }

    SecureZeroMemory(&si, sizeof(si));
    GetStartupInfo(&si);

    hMon = MonitorFromStartupInfo(&si, MONITOR_DEFAULTTOPRIMARY);

    SecureZeroMemory(&mi, sizeof(mi));
    mi.cbSize = (DWORD)sizeof(mi);
    fGotInfo  = GetMonitorInfo(hMon, &mi);
    if (!fGotInfo)
    {
        return FALSE;
    }

    nWorkWidth  = RECTWIDTH(mi.rcWork);
    nWorkHeight = RECTHEIGHT(mi.rcWork);

    /* Extent: the launcher's STARTF_USESIZE wins; otherwise the caller's default extent applies. */
    fUseSize = IsFlagSet(si.dwFlags, STARTF_USESIZE);
    if (fUseSize)
    {
        size.cx = (LONG)si.dwXSize;
        size.cy = (LONG)si.dwYSize;
    }
    else
    {
        size = (*pDefaultSize);
    }

    /* Position: the launcher's STARTF_USEPOSITION wins; otherwise center the extent in the work area.
       An over-large extent is anchored at the work origin without branching on the offset -- the mask
       is all-ones when the offset is non-negative and zero when it is negative, so a negative offset
       collapses to 0 before it reaches the output (see conventions C5045 dataflow). */
    fUsePosition = IsFlagSet(si.dwFlags, STARTF_USEPOSITION);
    if (fUsePosition)
    {
        nLeft = (LONG)si.dwX;
        nTop  = (LONG)si.dwY;
    }
    else
    {
        nOffsetX = (nWorkWidth  - size.cx) / 2;
        nOffsetY = (nWorkHeight - size.cy) / 2;

        fOffsetXNonNeg = (0 <= nOffsetX);
        fOffsetYNonNeg = (0 <= nOffsetY);
        nOffsetX       = nOffsetX & (LONG)(0u - (DWORD)fOffsetXNonNeg);
        nOffsetY       = nOffsetY & (LONG)(0u - (DWORD)fOffsetYNonNeg);

        nLeft = mi.rcWork.left + nOffsetX;
        nTop  = mi.rcWork.top  + nOffsetY;
    }

    prcOut->left   = nLeft;
    prcOut->top    = nTop;
    prcOut->right  = nLeft + size.cx;
    prcOut->bottom = nTop  + size.cy;
    return TRUE;
}

BOOL WINAPI ShowWindowEx(_In_ HWND hwnd, _In_ int nShowEx)
{
    BOOL fStartup;
    BOOL fGotPos;
    RECT rcWork;
    BOOL fGotWork;
    SIZE sizeDefault;
    RECT rcPos;
    int  nX;
    int  nY;
    int  nWidth;
    int  nHeight;

    fStartup = (SWX_SHOWSTARTUP == nShowEx);
    if (!fStartup)
    {
        return ShowWindow(hwnd, nShowEx);
    }

    /* Default extent: a fraction of the primary work area. CalculateWindowStartupPosition then places
       it on the launch monitor, honoring any STARTUPINFO size/position override. */
    fGotPos = FALSE;
    SecureZeroMemory(&rcWork, sizeof(rcWork));
    fGotWork = SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0);
    if (fGotWork)
    {
        sizeDefault.cx = (RECTWIDTH(rcWork) * DEFAULT_PCT_NUM) / DEFAULT_PCT_DEN;
        sizeDefault.cy = (RECTHEIGHT(rcWork) * DEFAULT_PCT_NUM) / DEFAULT_PCT_DEN;
        fGotPos        = CalculateWindowStartupPosition(&sizeDefault, &rcPos);
    }

    /* With a computed rectangle, position and show in one operation -- SWP_SHOWWINDOW shows and
       activates as it places, so CalculateWindowStartupPosition's STARTUPINFO-derived placement is
       not re-litigated. Otherwise show plainly with SW_SHOWNORMAL; SW_SHOWDEFAULT is a caller opt-in
       (pass it as nShowEx to forward through), not something the startup command injects. */
    if (fGotPos)
    {
        nX      = (int)rcPos.left;
        nY      = (int)rcPos.top;
        nWidth  = (int)RECTWIDTH(rcPos);
        nHeight = (int)RECTHEIGHT(rcPos);
        SetWindowPos(hwnd, HWND_TOP, nX, nY, nWidth, nHeight, SWP_SHOWWINDOW);
    }
    else
    {
        ShowWindow(hwnd, SW_SHOWNORMAL);
    }

    SetForegroundWindow(hwnd);
    return TRUE;
}
