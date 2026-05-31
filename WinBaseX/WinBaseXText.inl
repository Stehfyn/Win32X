/*
 * WinBaseXText.inl -- generic-text client-facing run path.
 *
 * Written in plain generic-text using the SDK's own identifiers (GetCommandLine, GetStartupInfo,
 * STARTUPINFO, GetModuleHandle, TCHAR, TEXT). Compiled twice -- WinBaseXStartupW.c with UNICODE,
 * WinBaseXStartupA.c without -- so one source yields WinBaseXRunW and WinBaseXRunA. The wide-only
 * machinery (COM server, registry, state, the wide->ANSI client bridge) lives in WinBaseX.c.
 */

/* Implemented (wide) in WinBaseX.c. */
BOOL StateInit(void);
int  RunCommon(BOOL* pfProceed);
void StoreClientA(WBX_PFN_WINMAINEXA pfnWinMainEx);
void StoreClientW(WBX_PFN_WINMAINEXW pfnWinMainEx);
BOOL LoadRegistrationA(const WINBASEX_REGISTRATION_PROPERTIESA* lpRegistrationProperties);
BOOL LoadRegistrationW(const WINBASEX_REGISTRATION_PROPERTIESW* lpRegistrationProperties);

#ifdef UNICODE
#define StoreClient      StoreClientW
#define LoadRegistration LoadRegistrationW
#else
#define StoreClient      StoreClientA
#define LoadRegistration LoadRegistrationA
#endif

/* Command-line tail: skip argv[0] (quoted or bare), then leading whitespace. */
static LPTSTR CommandLineTail(void)
{
    LPTSTR pszCmd;

    pszCmd = GetCommandLine();
    if (TEXT('"') == (*pszCmd))
    {
        pszCmd++;
        while ((*pszCmd) && (TEXT('"') != (*pszCmd)))
        {
            pszCmd++;
        }
        if (TEXT('"') == (*pszCmd))
        {
            pszCmd++;
        }
    }
    else
    {
        while ((*pszCmd) && (TEXT(' ') < (*pszCmd)))
        {
            pszCmd++;
        }
    }
    while ((TEXT(' ') == (*pszCmd)) || (TEXT('\t') == (*pszCmd)))
    {
        pszCmd++;
    }
    return pszCmd;
}

static int ShowCmd(const STARTUPINFO* psi)
{
    BOOL fUseShow;

    fUseShow = IsFlagSet(psi->dwFlags, STARTF_USESHOWWINDOW);
    RETURN_VALUE_IF(fUseShow, (int)psi->wShowWindow);
    return SW_SHOWDEFAULT;
}

int __cdecl WinBaseXRun(WBX_PFN_WINMAINEX pfnWinMainEx, const WINBASEX_REGISTRATION_PROPERTIES* pReg)
{
    STARTUPINFO si;
    BOOL        fProceed;
    int         rc;

    RETURN_VALUE_IF_NOT(StateInit(), 3);
    StoreClient(pfnWinMainEx);
    RETURN_VALUE_IF_NOT(LoadRegistration(pReg), 3);
    rc = RunCommon(&fProceed);
    RETURN_VALUE_IF_NOT(fProceed, rc);
    GetStartupInfo(&si);
    return pfnWinMainEx(GetModuleHandle(NULL), NULL, CommandLineTail(), ShowCmd(&si), &si);
}
