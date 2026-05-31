/*
 * WinBaseXText.inl -- generic-text client-facing run path.
 *
 * Written in plain generic-text using the SDK's own identifiers (GetCommandLine, GetStartupInfo,
 * STARTUPINFO, GetModuleHandle, TCHAR, TEXT). Compiled twice -- WinBaseXStartupW.c with UNICODE,
 * WinBaseXStartupA.c without -- so one source yields WinBaseXRunW and WinBaseXRunA. The wide-only
 * machinery (COM server, registry, state, the wide->ANSI client bridge) lives in WinBaseX.c.
 */

#include "processenvx.h"

/* Implemented (wide) in WinBaseX.c. */
BOOL StateInit(void);
int  RunCommon(BOOL* pfProceed);
void StoreClientA(WBX_PFN_WINMAINEXA pfnWinMainEx);
void StoreClientW(WBX_PFN_WINMAINEXW pfnWinMainEx);

#ifdef UNICODE
#define StoreClient StoreClientW
#else
#define StoreClient StoreClientA
#endif

static int GetShowCmd(const STARTUPINFO* psi)
{
    BOOL fUseShow;

    fUseShow = IsFlagSet(psi->dwFlags, STARTF_USESHOWWINDOW);
    RETURN_VALUE_IF(fUseShow, (int)psi->wShowWindow);
    return SW_SHOWDEFAULT;
}

int __cdecl WinBaseXRun(WBX_PFN_WINMAINEX pfnWinMainEx)
{
    STARTUPINFO si;
    BOOL        fProceed;
    int         rc;

    RETURN_VALUE_IF_NOT(StateInit(), 3);
    StoreClient(pfnWinMainEx);
    rc = RunCommon(&fProceed);
    RETURN_VALUE_IF_NOT(fProceed, rc);
    GetStartupInfo(&si);
    return pfnWinMainEx(GetModuleHandle(NULL), NULL, GetCommandLineArguments(), GetShowCmd(&si), &si);
}
