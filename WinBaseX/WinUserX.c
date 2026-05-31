/*
 * WinUserX.c -- charset-agnostic window-placement helpers complementing WinUser's window APIs:
 * launch-monitor resolution and startup-rectangle policy, no CRT dependency.
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

#ifndef STARTF_HASSHELLDATA
#define STARTF_HASSHELLDATA 0x00000400
#endif

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

    fHasShellData = !!(STARTF_HASSHELLDATA & psi->dwFlags);
    if (fHasShellData)
    {
        return (HMONITOR)psi->hStdOutput;
    }

    fUsePosition = !!(STARTF_USEPOSITION & psi->dwFlags);
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

    nWorkWidth  = mi.rcWork.right  - mi.rcWork.left;
    nWorkHeight = mi.rcWork.bottom - mi.rcWork.top;

    /* Extent: the launcher's STARTF_USESIZE wins; otherwise the caller's default extent applies. */
    fUseSize = !!(STARTF_USESIZE & si.dwFlags);
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
    fUsePosition = !!(STARTF_USEPOSITION & si.dwFlags);
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
