#ifdef UNICODE
#undef UNICODE
#endif
#ifdef _UNICODE
#undef _UNICODE
#endif

#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "WinBaseX.h"
#include "WinBaseXText.inl" /* UNICODE undefined for this TU -> WinBaseXRunA */

void __cdecl WinMainCRTStartup(void)
{
    ExitProcess((UINT)WinBaseXRun(_tWinMainEx, &WinBaseXRegistration));
}