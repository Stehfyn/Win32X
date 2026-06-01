/*
 * tests.inl -- WinBaseX test cases. Included once by test_harness.c, after the harness primitives
 * (Out/OutF/Check/Skip) and the shared declarations (defines, SECOND_MON, the HasArg macro). Holds only
 * function definitions; every #define and typedef lives in the harness preamble per the source-layout
 * rule. Callees precede callers so no forward prototypes are needed.
 */

static LRESULT CALLBACK TestWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static BOOL ShowExGivesThreeQuarters(DPI_AWARENESS_CONTEXT ambient)
{
    WNDCLASS              wc;
    HWND                  hwnd;
    DPI_AWARENESS_CONTEXT prev;
    RECT                  rc;
    HMONITOR              hMonitor;
    MONITORINFO           mi;
    LONG                  nWinWidth;
    LONG                  nWinHeight;
    LONG                  nWorkWidth;
    LONG                  nWorkHeight;
    BOOL                  fIsThreeQuarters;

    SecureZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = TestWndProc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.lpszClassName = TEXT("WmxTestWnd");
    RegisterClass(&wc);

    prev = SetThreadDpiAwarenessContextEx(ambient);
    hwnd = CreateWindowEx(0, TEXT("WmxTestWnd"), TEXT("t"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                          CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, wc.hInstance, NULL);
    ShowWindowEx(hwnd, SWX_SHOWSTARTUP);
    if (IsNonNull(prev))
    {
        SetThreadDpiAwarenessContextEx(prev);
    }

    SetThreadDpiAwarenessContextEx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    GetWindowRect(hwnd, &rc);
    hMonitor  = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    mi.cbSize = (DWORD)sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);
    DestroyWindow(hwnd);

    nWinWidth        = RECTWIDTH(rc);
    nWinHeight       = RECTHEIGHT(rc);
    nWorkWidth       = RECTWIDTH(mi.rcWork);
    nWorkHeight      = RECTHEIGHT(mi.rcWork);
    fIsThreeQuarters = (THREEQUARTERS(nWorkWidth) == nWinWidth) && (THREEQUARTERS(nWorkHeight) == nWinHeight);
    return fIsThreeQuarters;
}

static BOOL CALLBACK SecondMonProc(HMONITOR hMonitor, HDC hdc, LPRECT prc, LPARAM lParam)
{
    SECOND_MON* pCtx;
    MONITORINFO mi;
    BOOL        fPrimary;

    UNREFERENCED_PARAMETER(hdc);
    UNREFERENCED_PARAMETER(prc);
    pCtx      = (SECOND_MON*)lParam;
    mi.cbSize = (DWORD)sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);
    fPrimary = IsFlagSet(mi.dwFlags, MONITORINFOF_PRIMARY);
    if (!fPrimary)
    {
        pCtx->rcWork = mi.rcWork;
        pCtx->fFound = TRUE;
        return FALSE;
    }
    return TRUE;
}

static BOOL HasArg(LPCTSTR pszNeedle)
{
    LPCTSTR pszCmd;
    LPCTSTR pszScan;
    LPCTSTR pszN;
    BOOL    fMatching;

    pszCmd = GetCommandLine();
    while (IsNonZero((*pszCmd)))
    {
        pszScan   = pszCmd;
        pszN      = pszNeedle;
        fMatching = IsNonZero((*pszN)) && IsEqual((*pszScan), (*pszN));
        while (fMatching)
        {
            pszScan++;
            pszN++;
            fMatching = IsNonZero((*pszN)) && IsEqual((*pszScan), (*pszN));
        }
        if (IsZero((*pszN)))
        {
            return TRUE;
        }
        pszCmd++;
    }
    return FALSE;
}

static void RunPositionChild(void)
{
    RECT     rc;
    POINT    pt;
    POINT    ptOrigin;
    HMONITOR hMonitorGot;
    HMONITOR hMonitorPrimary;
    BOOL     fGotRect;
    BOOL     fOnSecond;
    UINT     uExitCode;

    fGotRect = CalculateWindowStartupPosition(&rc);
    if (!fGotRect)
    {
        ExitProcess(EXIT_FAIL);
    }
    pt.x            = rc.left + PROBE_INSET;
    pt.y            = rc.top + PROBE_INSET;
    ptOrigin.x      = 0;
    ptOrigin.y      = 0;
    hMonitorGot     = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL);
    hMonitorPrimary = MonitorFromPoint(ptOrigin, MONITOR_DEFAULTTOPRIMARY);
    fOnSecond       = IsNonNull(hMonitorGot) && (hMonitorGot != hMonitorPrimary);
    uExitCode       = EXIT_FAIL;
    if (fOnSecond)
    {
        uExitCode = EXIT_OK;
    }
    ExitProcess(uExitCode);
}

static void T_ThreeQuarters(void)
{
    BOOL fIsThreeQuarters;

    fIsThreeQuarters = (1920 == THREEQUARTERS(2560)) && (1280 == THREEQUARTERS(1707)) && (764 == THREEQUARTERS(1019));
    Check(fIsThreeQuarters, TEXT("T1 THREEQUARTERS == round(3/4 * dim)"));
}

static void T_Thunks(void)
{
    UINT                  uSysDpi;
    int                   nIconCx;
    DPI_AWARENESS_CONTEXT prev;
    BOOL                  fSysDpiOk;
    BOOL                  fIconOk;
    POINT                 ptOrigin;
    HMONITOR              hMonitor;
    UINT                  uDpiX;
    UINT                  uDpiY;
    HRESULT               hr;
    BOOL                  fMonDpiOk;

    uSysDpi = GetDpiForSystemEx();
    nIconCx = GetSystemMetricsForDpiEx(SM_CXICON, USER_DEFAULT_SCREEN_DPI);
    prev    = SetThreadDpiAwarenessContextEx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (IsNonNull(prev))
    {
        SetThreadDpiAwarenessContextEx(prev);
    }
    fSysDpiOk = (USER_DEFAULT_SCREEN_DPI <= uSysDpi);
    fIconOk   = IsPositive(nIconCx);
    Check(fSysDpiOk, TEXT("T2 GetDpiForSystemEx >= 96 (thunk + delay path resolves)"));
    Check(fIconOk, TEXT("T2 GetSystemMetricsForDpiEx > 0"));

    ptOrigin.x = 0;
    ptOrigin.y = 0;
    hMonitor   = MonitorFromPoint(ptOrigin, MONITOR_DEFAULTTOPRIMARY);
    uDpiX      = 0;
    uDpiY      = 0;
    hr         = GetDpiForMonitorEx(hMonitor, MDT_EFFECTIVE_DPI, &uDpiX, &uDpiY);
    fMonDpiOk  = FALSE;
    if (SUCCEEDED(hr))
    {
        fMonDpiOk = (USER_DEFAULT_SCREEN_DPI <= uDpiX);
    }
    else
    {
        fMonDpiOk = (E_NOTIMPL == hr);
    }
    Check(fMonDpiOk, TEXT("T2 GetDpiForMonitorEx (shcore delay-load) ok or E_NOTIMPL"));
}

static void T_StartupRect(void)
{
    RECT        rc;
    MONITORINFO mi;
    HMONITOR    hMonitor;
    BOOL        fGotRect;
    LONG        nWinWidth;
    LONG        nWinHeight;
    LONG        nWorkWidth;
    LONG        nWorkHeight;
    LONG        nLeftGap;
    LONG        nRightGap;
    LONG        nTopGap;
    LONG        nBottomGap;
    BOOL        fIsThreeQuarters;
    BOOL        fIsCentered;

    fGotRect = CalculateWindowStartupPosition(&rc);
    if (!fGotRect)
    {
        Check(FALSE, TEXT("T3 CalculateWindowStartupPosition returns TRUE"));
        return;
    }
    hMonitor  = GetStartupMonitor(MONITOR_DEFAULTTOPRIMARY);
    mi.cbSize = (DWORD)sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);

    nWinWidth        = RECTWIDTH(rc);
    nWinHeight       = RECTHEIGHT(rc);
    nWorkWidth       = RECTWIDTH(mi.rcWork);
    nWorkHeight      = RECTHEIGHT(mi.rcWork);
    fIsThreeQuarters = (THREEQUARTERS(nWorkWidth) == nWinWidth) && (THREEQUARTERS(nWorkHeight) == nWinHeight);
    Check(fIsThreeQuarters, TEXT("T3 startup extent == 3/4 of work area"));

    nLeftGap    = rc.left - mi.rcWork.left;
    nRightGap   = mi.rcWork.right - rc.right;
    nTopGap     = rc.top - mi.rcWork.top;
    nBottomGap  = mi.rcWork.bottom - rc.bottom;
    fIsCentered = (CENTER_SLACK >= ABS(nLeftGap - nRightGap)) && (CENTER_SLACK >= ABS(nTopGap - nBottomGap));
    Check(fIsCentered, TEXT("T3 startup rect centered in work area"));
}

static void T_Hardening(void)
{
    BOOL fUnawareOk;
    BOOL fSystemOk;
    BOOL fPerMonOk;

    fUnawareOk = ShowExGivesThreeQuarters(DPI_AWARENESS_CONTEXT_UNAWARE);
    fSystemOk  = ShowExGivesThreeQuarters(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
    fPerMonOk  = ShowExGivesThreeQuarters(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Check(fUnawareOk, TEXT("T4 ShowWindowEx 3/4 under UNAWARE thread"));
    Check(fSystemOk, TEXT("T4 ShowWindowEx 3/4 under SYSTEM thread"));
    Check(fPerMonOk, TEXT("T4 ShowWindowEx 3/4 under PMv2 thread"));
}

static void T_Position(void)
{
    int                 nMonitors;
    SECOND_MON          ctx;
    TCHAR               szSelf[MAX_PATH];
    TCHAR               szCmd[CMD_CCH];
    STARTUPINFO         si;
    PROCESS_INFORMATION pi;
    BOOL                fCreated;
    DWORD               dwExitCode;
    BOOL                fLandedSecond;

    nMonitors = GetSystemMetrics(SM_CMONITORS);
    if (MULTIMON_MIN > nMonitors)
    {
        Skip(TEXT("T5 positional dwX/dwY -> second monitor"), TEXT("single monitor"));
        return;
    }
    ctx.fFound = FALSE;
    EnumDisplayMonitors(NULL, NULL, SecondMonProc, (LPARAM)&ctx);
    if (!ctx.fFound)
    {
        Skip(TEXT("T5 positional dwX/dwY -> second monitor"), TEXT("no non-primary monitor found"));
        return;
    }

    GetModuleFileName(NULL, szSelf, MAX_PATH);
    wsprintf(szCmd, TEXT("\"%s\" --child"), szSelf);

    SecureZeroMemory(&si, sizeof(si));
    si.cb      = (DWORD)sizeof(si);
    si.dwFlags = STARTF_USEPOSITION;
    si.dwX     = (DWORD)(ctx.rcWork.left + LAUNCH_INSET);
    si.dwY     = (DWORD)(ctx.rcWork.top + LAUNCH_INSET);
    fCreated   = CreateProcess(szSelf, szCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (!fCreated)
    {
        Check(FALSE, TEXT("T5 positional: CreateProcess(self --child) succeeds"));
        return;
    }
    WaitForSingleObject(pi.hProcess, WAIT_MS);
    dwExitCode = EXIT_FAIL;
    GetExitCodeProcess(pi.hProcess, &dwExitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    fLandedSecond = IsZero(dwExitCode);
    Check(fLandedSecond, TEXT("T5 physical STARTF_USEPOSITION lands the startup rect on the second monitor"));
}
