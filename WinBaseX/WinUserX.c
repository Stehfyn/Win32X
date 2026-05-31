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
#include "result.h"

#ifndef STARTF_HASSHELLDATA
#define STARTF_HASSHELLDATA 0x00000400
#endif

HMONITOR WINAPI GetStartupMonitor(_In_ DWORD dwFlags)
{
    STARTUPINFO si;
    POINT       pt;
    BOOL        fHasShellData;
    BOOL        fUsePosition;

    SecureZeroMemory(&si, sizeof(si));
    GetStartupInfo(&si);

    pt.x = 0;
    pt.y = 0;

    fHasShellData = IsFlagSet(si.dwFlags, STARTF_HASSHELLDATA);
    RETURN_VALUE_IF(fHasShellData, (HMONITOR)si.hStdOutput);

    fUsePosition = IsFlagSet(si.dwFlags, STARTF_USEPOSITION);
    if (fUsePosition)
    {
        pt.x = (LONG)si.dwX;
        pt.y = (LONG)si.dwY;
    }
    return MonitorFromPoint(pt, dwFlags);
}

_Success_(return != FALSE)
BOOL WINAPI CalculateWindowStartupPosition(_Out_ RECT* prcOut)
{
    STARTUPINFO si;
    HMONITOR    hMonitor;
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

    RETURN_FALSE_IF_NULL(prcOut);

    SecureZeroMemory(&si, sizeof(si));
    GetStartupInfo(&si);

    hMonitor = GetStartupMonitor(MONITOR_DEFAULTTOPRIMARY);

    SecureZeroMemory(&mi, sizeof(mi));
    mi.cbSize = (DWORD)sizeof(mi);
    fGotInfo  = GetMonitorInfo(hMonitor, &mi);
    RETURN_FALSE_IF_NOT(fGotInfo);

    nWorkWidth  = RECTWIDTH(mi.rcWork);
    nWorkHeight = RECTHEIGHT(mi.rcWork);

    /* Extent: the launcher's STARTF_USESIZE wins; otherwise the window manager's own default applies --
       three-quarters of this (launch) monitor's work area per axis, matching win32kfull!SetTiledRect.
       The app is per-monitor DPI-aware, so mi.rcWork is the target monitor's physical work area at its
       own DPI; the ratio is therefore taken against the monitor the window will actually appear on. */
    fUseSize = IsFlagSet(si.dwFlags, STARTF_USESIZE);
    if (fUseSize)
    {
        size.cx = (LONG)si.dwXSize;
        size.cy = (LONG)si.dwYSize;
    }
    else
    {
        size.cx = THREEQUARTERS(nWorkWidth);
        size.cy = THREEQUARTERS(nWorkHeight);
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

        fOffsetXNonNeg = IsNonNegative(nOffsetX);
        fOffsetYNonNeg = IsNonNegative(nOffsetY);
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

    /* Place per the launch policy: CalculateWindowStartupPosition sizes the window to three-quarters of
       the launch monitor's work area (matching win32kfull!SetTiledRect) and positions it there,
       honoring any STARTUPINFO size/position override. Because the app is per-monitor DPI-aware, that
       work area is the target monitor's physical extent at its own DPI -- so the default tracks the OS
       contract on whichever monitor the launch resolves to, not the primary. */
    fGotPos = CalculateWindowStartupPosition(&rcPos);

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

    /* Deliberately no SetForegroundWindow: the activating show above (SWP_SHOWWINDOW / SW_SHOWNORMAL)
       already brings a foreground-entitled launch to front. Forcing it would presume foreground rights
       we may not hold, and would steal focus if the user has moved on since launch -- the focus-steal
       the OS suppresses. An unentitled show flashes the taskbar instead, which is the correct cue. */
    return TRUE;
}

int WINAPI ErrorMessageBoxW(_In_opt_ HWND hwnd, _In_ LPCWSTR pszText, _In_ LPCWSTR pszCaption)
{
    return MessageBoxW(hwnd, pszText, pszCaption, MB_ICONERROR | MB_OK);
}

int WINAPI ErrorMessageBoxA(_In_opt_ HWND hwnd, _In_ LPCSTR pszText, _In_ LPCSTR pszCaption)
{
    return MessageBoxA(hwnd, pszText, pszCaption, MB_ICONERROR | MB_OK);
}
