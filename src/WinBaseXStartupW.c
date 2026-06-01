#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Win32X/WinBaseX.h"
#include "Win32X/windefx.h"
#include "result.h"
#include "WinBaseXText.inl" /* UNICODE defined for this TU -> WinBaseXRunW */

void __cdecl wWinMainCRTStartup(void)
{
    ExitProcess((UINT)WinBaseXRun(_tWinMainEx));
}