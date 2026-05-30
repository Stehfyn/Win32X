#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "WinBaseX.h"

int __cdecl WinBaseXRunWide(int(WINAPI* pfnWinMainEx)(HINSTANCE, HINSTANCE, LPWSTR, int, const STARTUPINFOW*));

void __cdecl wWinMainCRTStartup(void)
{
    int rc;

    rc = WinBaseXRunWide(wWinMainEx);
    ExitProcess((UINT)rc);
}
