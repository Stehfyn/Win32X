/*
 * WinBaseXText.inl -- generic-text client-facing run path.
 *
 * Written in plain generic-text using the SDK's own identifiers (GetCommandLine, GetStartupInfo,
 * STARTUPINFO, GetModuleHandle, TCHAR, TEXT). Compiled twice -- WinBaseXStartupW.c with UNICODE,
 * WinBaseXStartupA.c without -- so one source yields WinBaseXRunW and WinBaseXRunA. The wide-only
 * machinery (COM server, registry, state, the wide->ANSI client bridge) lives in WinBaseX.c.
 */

/* Implemented (wide) in WinBaseX.c. */
BOOL WbxStateInit(void);
int  WbxRunCommon(BOOL* pfProceed);
void WbxStoreClientA(WBX_PFN_WINMAINEXA pfnWinMainEx);
void WbxStoreClientW(WBX_PFN_WINMAINEXW pfnWinMainEx);
BOOL WbxLoadRegistrationA(const WINBASEX_REGISTRATION_PROPERTIESA* lpRegistrationProperties);
BOOL WbxLoadRegistrationW(const WINBASEX_REGISTRATION_PROPERTIESW* lpRegistrationProperties);

#ifdef UNICODE
#define WbxStoreClient      WbxStoreClientW
#define WbxLoadRegistration WbxLoadRegistrationW
#else
#define WbxStoreClient      WbxStoreClientA
#define WbxLoadRegistration WbxLoadRegistrationA
#endif

/* Command-line tail: skip argv[0] (quoted or bare), then leading whitespace. */
static LPTSTR WbxCommandLineTail(void)
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

static int WbxShowCmd(const STARTUPINFO* psi)
{
    BOOL fUseShow;

    fUseShow = !!(STARTF_USESHOWWINDOW & psi->dwFlags);
    if (fUseShow)
    {
        return (int)psi->wShowWindow;
    }
    return SW_SHOWDEFAULT;
}

int __cdecl WinBaseXRun(WBX_PFN_WINMAINEX pfnWinMainEx, const WINBASEX_REGISTRATION_PROPERTIES* pReg)
{
    STARTUPINFO si;
    BOOL        fProceed;
    int         rc;

    if (!WbxStateInit())
    {
        return 3;
    }
    WbxStoreClient(pfnWinMainEx);
    if (!WbxLoadRegistration(pReg))
    {
        return 3;
    }
    rc = WbxRunCommon(&fProceed);
    if (!fProceed)
    {
        return rc;
    }
    GetStartupInfo(&si);
    return pfnWinMainEx(GetModuleHandle(NULL), NULL, WbxCommandLineTail(), WbxShowCmd(&si), &si);
}
