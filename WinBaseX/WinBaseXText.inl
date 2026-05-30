/*
 * WinBaseXText.inl -- charset-agnostic template for the client-facing layer.
 *
 * Included twice by WinBaseX.c (once with the W macros, once with the A macros) to generate the
 * wide and ANSI variants from one source -- the SDK generic-text pattern. Our own function names
 * take the W/A suffix via WBXNAME()/WBXCAT(); the SDK identifiers that are themselves macros
 * (GetCommandLine, GetStartupInfo, STARTUPINFO) cannot be suffix-pasted, so the includer aliases
 * them per pass to their real ...W/...A names (WBX_GETCMDLINE, WBX_GETSTARTUP, WBX_STARTUPINFO).
 * The includer also defines WBXSUF, WBXSTR, WBXTEXT(), WBX_RUN, WBX_UNICODE.
 */

static WBXSTR WBXNAME(WbxWinMainCommandLine)(void)
{
    WBXSTR pszCmd;

    pszCmd = WBX_GETCMDLINE();
    if (WBXTEXT('"') == (*pszCmd))
    {
        pszCmd++;
        while ((*pszCmd) && (WBXTEXT('"') != (*pszCmd)))
        {
            pszCmd++;
        }
        if (WBXTEXT('"') == (*pszCmd))
        {
            pszCmd++;
        }
    }
    else
    {
        while ((*pszCmd) && (WBXTEXT(' ') < (*pszCmd)))
        {
            pszCmd++;
        }
    }
    while ((WBXTEXT(' ') == (*pszCmd)) || (WBXTEXT('\t') == (*pszCmd)))
    {
        pszCmd++;
    }
    return pszCmd;
}

static int WBXNAME(WbxShowWindowFromStartup)(const WBX_STARTUPINFO* psi)
{
    BOOL fUseShow;

    fUseShow = !!(STARTF_USESHOWWINDOW & psi->dwFlags);
    if (fUseShow)
    {
        return (int)psi->wShowWindow;
    }
    return SW_SHOWDEFAULT;
}

static int WBXNAME(WbxCallClient)(WBXSTR pszCmdLine, const WBX_STARTUPINFO* psi)
{
    WBX_STATE* pState;
    HINSTANCE  hInstance;
    int        nShowCmd;

    pState    = WbxState();
    hInstance = GetModuleHandleW(NULL);
    nShowCmd  = WBXNAME(WbxShowWindowFromStartup)(psi);
    return pState->WBXCAT(pfnWinMainEx, WBXSUF)(hInstance, NULL, pszCmdLine, nShowCmd, psi);
}

int __cdecl WBX_RUN(WBXCAT(WBX_PFN_WINMAINEX, WBXSUF) pfnWinMainEx)
{
    WBX_STATE*      pState;
    WBX_STARTUPINFO si;
    BOOL            fProceed;
    int             rc;

    if (!WbxStateInit())
    {
        return 3;
    }
    pState                               = WbxState();
    pState->fIsUnicode                   = WBX_UNICODE;
    pState->WBXCAT(pfnWinMainEx, WBXSUF) = pfnWinMainEx;

    rc = WbxRunCommon(&fProceed);
    if (!fProceed)
    {
        return rc;
    }
    WBX_GETSTARTUP(&si);
    return WBXNAME(WbxCallClient)(WBXNAME(WbxWinMainCommandLine)(), &si);
}
