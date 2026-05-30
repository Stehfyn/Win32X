#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "WinBaseX.h"

int __cdecl WinBaseXRunAnsi(int(WINAPI* pfnWinMainEx)(HINSTANCE, HINSTANCE, LPSTR, int, const STARTUPINFOA*));

void __cdecl WinMainCRTStartup(void)
{
    int rc;

    rc = WinBaseXRunAnsi(WinMainEx);
    ExitProcess((UINT)rc);
}
