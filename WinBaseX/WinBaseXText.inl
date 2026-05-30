/*
 * WinBaseXText.inl -- charset-agnostic template for the client-facing layer.
 *
 * Included twice by WinBaseX.c (once with the W macros, once with the A macros) to generate the
 * wide and ANSI variants from one source -- the SDK generic-text pattern. Our own function names
 * take the W/A suffix via WBXNAME()/WBXCAT(); the SDK identifiers that are themselves macros
 * (GetCommandLine, GetStartupInfo, STARTUPINFO) cannot be suffix-pasted, so the includer aliases
 * them per pass to their real ...W/...A names (WBX_GETCMDLINE, WBX_GETSTARTUP, WBX_STARTUPINFO).
 * The includer also defines WBXSUF, WBXSTR, WBXTEXT(), WBX_RUN, WBX_USE_WIDE.
 */

static WBXSTR WBXNAME(wbx_winmain_command_line)(void)
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

static int WBXNAME(wbx_show_window_from_startup)(const WBX_STARTUPINFO *psi)
{
    BOOL fUseShow;

    fUseShow = !!(STARTF_USESHOWWINDOW & psi->dwFlags);
    if (fUseShow)
    {
        return (int)psi->wShowWindow;
    }
    return SW_SHOWDEFAULT;
}

static int WBXNAME(wbx_call_client)(WBXSTR pszCmdLine, const WBX_STARTUPINFO *psi)
{
    WBX_STATE *pState;
    HINSTANCE  hInstance;
    int        nShowCmd;

    pState    = wbx_state();
    hInstance = GetModuleHandleW(NULL);
    nShowCmd  = WBXNAME(wbx_show_window_from_startup)(psi);
    return pState->WBXCAT(pfnWinMainEx, WBXSUF)(hInstance, NULL, pszCmdLine, nShowCmd, psi);
}

int __cdecl WBX_RUN(WBXCAT(WBX_PFN_WINMAINEX, WBXSUF) pfnWinMainEx)
{
    WBX_STATE      *pState;
    WBX_STARTUPINFO si;
    BOOL            fProceed;
    int             rc;

    if (!wbx_state_init())
    {
        return 3;
    }
    pState                               = wbx_state();
    pState->fUseWideCallback             = WBX_USE_WIDE;
    pState->WBXCAT(pfnWinMainEx, WBXSUF) = pfnWinMainEx;

    rc = wbx_run_common(&fProceed);
    if (!fProceed)
    {
        return rc;
    }
    WBX_GETSTARTUP(&si);
    return WBXNAME(wbx_call_client)(WBXNAME(wbx_winmain_command_line)(), &si);
}
