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

static void (WINAPI* volatile pfnTestThemeStartup)(void) = ThemeStartup;
static BOOL (WINAPI* volatile pfnTestThemeCanUseDarkMode)(void) = ThemeCanUseDarkMode;
static BOOL (WINAPI* volatile pfnTestThemeEffectiveDarkMode)(void) = ThemeEffectiveDarkMode;
static BOOL (WINAPI* volatile pfnTestThemeIsDarkMode)(void) = ThemeIsDarkMode;
static HBRUSH (WINAPI* volatile pfnTestThemeBackgroundBrush)(BOOL fDark) = ThemeBackgroundBrush;
static void (WINAPI* volatile pfnTestThemeApplyTopLevel)(HWND hwnd, BOOL fDark) = ThemeApplyTopLevel;
static void (WINAPI* volatile pfnTestThemeApplyDialogTree)(HWND hwnd, BOOL fDark) = ThemeApplyDialogTree;
static BOOL (WINAPI* volatile pfnTestThemeOnSettingChange)(HWND hwndPost, UINT uDeferredMsg, LPCTSTR pszSection) =
    ThemeOnSettingChange;
static BOOL (WINAPI* volatile pfnTestThemeOnDeferredThemeChange)(void) = ThemeOnDeferredThemeChange;
static void (WINAPI* volatile pfnTestThemeDiagnostics)(THEME_DIAGNOSTICS* pDiag) = ThemeDiagnostics;
static void (WINAPI* volatile pfnTestThemeUnregisterDialog)(HWND hwnd) = ThemeUnregisterDialog;
static void (WINAPI* volatile pfnTestThemeUnregisterWindow)(HWND hwnd) = ThemeUnregisterWindow;

static BOOL g_fThemeExpectedDark;
static BOOL g_fThemeSawErase;
static BOOL g_fThemeSawCtlStatic;
static BOOL g_fThemeSawNcPaint;
static BOOL g_fThemePaintMismatch;
static DWORD g_dwThemeWorkerValue;
static BOOL  g_fThemeWorkerWrote;
static BOOL  g_fThemeWorkerBroadcast;
static BOOL  g_fThemeWorkerWriteSystem;
static LPCTSTR g_pszThemeTag = TEXT("");
static HANDLE g_hThemeStartEvent;
static HANDLE g_hThemeAppProcess;   /* launched WindowsProject.exe, terminated at teardown */
static HWND   g_hwndThemeTop;
static HWND   g_hwndThemeDialog;

#define THEME_IDM_ABOUT 104   /* IDM_ABOUT from examples/Resource.h: opens WindowsProject's About box */

typedef HRESULT(WINAPI* PFN_CREATEDXGIFACTORY1)(REFIID riid, void** ppFactory);
typedef HRESULT(WINAPI* PFN_D3D11CREATEDEVICE)(IDXGIAdapter*,
                                               D3D_DRIVER_TYPE,
                                               HMODULE,
                                               UINT,
                                               const D3D_FEATURE_LEVEL*,
                                               UINT,
                                               UINT,
                                               ID3D11Device**,
                                               D3D_FEATURE_LEVEL*,
                                               ID3D11DeviceContext**);
typedef BOOL(WINAPI* PFN_INITCOMMONCONTROLSEX)(const INITCOMMONCONTROLSEX*);
typedef HRESULT(WINAPI* PFN_MFSTARTUP)(ULONG Version, DWORD dwFlags);
typedef HRESULT(WINAPI* PFN_MFSHUTDOWN)(void);
typedef HRESULT(WINAPI* PFN_MFCREATEDXGISURFACEBUFFER)(REFIID riid,
                                                       IUnknown* punkSurface,
                                                       UINT uSubresourceIndex,
                                                       BOOL fBottomUpWhenLinear,
                                                       IMFMediaBuffer** ppBuffer);
typedef HRESULT(WINAPI* PFN_MFCREATEDXGIDEVICEMANAGER)(UINT* resetToken,
                                                       IMFDXGIDeviceManager** ppDeviceManager);
typedef HRESULT(WINAPI* PFN_MFCREATESAMPLE)(IMFSample** ppIMFSample);
typedef HRESULT(WINAPI* PFN_MFCREATEATTRIBUTES)(IMFAttributes** ppMFAttributes, UINT32 cInitialSize);
typedef HRESULT(WINAPI* PFN_MFCREATEMEDIATYPE)(IMFMediaType** ppMFType);
typedef HRESULT(WINAPI* PFN_MFCREATESINKWRITERFROMURL)(LPCWSTR pwszOutputURL,
                                                       IMFByteStream* pByteStream,
                                                       IMFAttributes* pAttributes,
                                                       IMFSinkWriter** ppSinkWriter);
typedef HRESULT(WINAPI* PFN_MFCREATESOURCEREADERFROMURL)(LPCWSTR pwszURL,
                                                         IMFAttributes* pAttributes,
                                                         IMFSourceReader** ppSourceReader);

typedef struct
{
    HMODULE                         hMfplat;
    HMODULE                         hMfreadwrite;
    PFN_MFSTARTUP                   pfnMFStartup;
    PFN_MFSHUTDOWN                  pfnMFShutdown;
    PFN_MFCREATEDXGISURFACEBUFFER   pfnMFCreateDXGISurfaceBuffer;
    PFN_MFCREATEDXGIDEVICEMANAGER   pfnMFCreateDXGIDeviceManager;
    PFN_MFCREATESAMPLE              pfnMFCreateSample;
    PFN_MFCREATEATTRIBUTES          pfnMFCreateAttributes;
    PFN_MFCREATEMEDIATYPE           pfnMFCreateMediaType;
    PFN_MFCREATESINKWRITERFROMURL   pfnMFCreateSinkWriterFromURL;
    PFN_MFCREATESOURCEREADERFROMURL pfnMFCreateSourceReaderFromURL;
} THEME_TEST_MF;

static THEME_TEST_MF g_mf;
static HRESULT g_hrThemeEncode;
static HRESULT g_hrThemeDecode;
static HRESULT g_hrThemeCapture;
static UINT g_uThemeCaptureStage;
static UINT g_uThemeEncodeStage;

static const GUID ThemeTestIID_IDXGIFactory1 = { 0x770aae78, 0xf26f, 0x4dba,
                                           { 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87 } };
static const GUID ThemeTestIID_IDXGIOutput1 = { 0x00cddea8, 0x939b, 0x4b83,
                                           { 0xa3, 0x40, 0xa6, 0x85, 0x22, 0x66, 0x66, 0xcc } };
static const GUID ThemeTestIID_ID3D11Texture2D = { 0x6f15aaf2, 0xd208, 0x4e89,
                                              { 0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c } };
static const GUID ThemeTestIID_IMF2DBuffer = { 0x7dc9d5f9, 0x9ed9, 0x44ec,
                                           { 0x9b, 0xbf, 0x06, 0x00, 0xbb, 0x58, 0x9f, 0xbb } };

static void ThemeTestRecordPaint(BOOL* pfSaw)
{
    *pfSaw = TRUE;
    if (g_fThemeExpectedDark != pfnTestThemeIsDarkMode())
    {
        g_fThemePaintMismatch = TRUE;
    }
}

static void ThemeTestOnUahDrawMenu(HWND hwnd, const UAHMENU* pUDM)
{
    MENUBAR_PALETTE pal;

    MenuBarPalette(pfnTestThemeIsDarkMode(), &pal);
    MenuBarOnDrawMenu(hwnd, pUDM, &pal);
}

static void ThemeTestOnUahDrawMenuItem(HWND hwnd, const UAHDRAWMENUITEM* pUDMI)
{
    MENUBAR_PALETTE pal;

    MenuBarPalette(pfnTestThemeIsDarkMode(), &pal);
    MenuBarOnDrawMenuItem(hwnd, pUDMI, &pal);
}

static LRESULT CALLBACK ThemeTestWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT lr;
    MENUBAR_PALETTE pal;

    if (WM_ERASEBKGND == uMsg)
    {
        ThemeTestRecordPaint(&g_fThemeSawErase);
    }
    else if (WM_CTLCOLORSTATIC == uMsg)
    {
        ThemeTestRecordPaint(&g_fThemeSawCtlStatic);
    }
    if (ThemeHandleWindowMessage(hwnd, uMsg, wParam, lParam, THEME_TEST_DEFERRED_MSG, &lr))
    {
        return lr;
    }

    switch (uMsg)
    {
        case WM_UAHDRAWMENU:
            return HANDLE_WM_UAHDRAWMENU(hwnd, wParam, lParam, ThemeTestOnUahDrawMenu);

        case WM_UAHDRAWMENUITEM:
            return HANDLE_WM_UAHDRAWMENUITEM(hwnd, wParam, lParam, ThemeTestOnUahDrawMenuItem);

        case WM_NCPAINT:
            ThemeTestRecordPaint(&g_fThemeSawNcPaint);
            lr = DefWindowProc(hwnd, WM_NCPAINT, wParam, lParam);
            MenuBarPalette(pfnTestThemeIsDarkMode(), &pal);
            MenuBarPaintSeam(hwnd, &pal);
            return lr;

        case WM_NCACTIVATE:
            lr = DefWindowProc(hwnd, WM_NCACTIVATE, wParam, lParam);
            MenuBarPalette(pfnTestThemeIsDarkMode(), &pal);
            MenuBarPaintSeam(hwnd, &pal);
            return lr;

        default:
            break;
    }
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
    wc.lpszClassName = TEXT("TestWnd");
    RegisterClass(&wc);

    prev = SetThreadDpiAwarenessContextEx(ambient);
    hwnd = CreateWindowEx(0, TEXT("TestWnd"), TEXT("t"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
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

/*
 * The two HKCU Personalize DWORDs a theme switch can touch: AppsUseLightTheme (app surfaces -- what
 * this app's caption/menu/client follow) and SystemUsesLightTheme (taskbar/system chrome; when it
 * flips, DWM also runs its own caption crossfade). The split transition tests exercise each path:
 * one writes AppsUseLightTheme alone, the other writes both (what Settings does for a full switch).
 * Regardless of which is written, BOTH originals are captured up front and BOTH are restored at the
 * end, so the developer's machine is never left in an inconsistent (apps != system) state.
 */
static BOOL g_fThemeHadSystemValue;
static DWORD g_dwThemeSystemValue;

static BOOL ThemeTestReadThemeValue(LPCTSTR pszValue, DWORD* pdwValue, BOOL* pfHadValue)
{
    HKEY    hKey;
    DWORD   dwType;
    DWORD   cbValue;
    LSTATUS lStatus;

    *pdwValue  = 1u;
    *pfHadValue = FALSE;
    hKey = NULL;
    lStatus = RegOpenKeyEx(HKEY_CURRENT_USER,
                           TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
                           0u,
                           KEY_QUERY_VALUE,
                           &hKey);
    if (ERROR_FILE_NOT_FOUND == lStatus)
    {
        return TRUE;
    }
    if (ERROR_SUCCESS != lStatus)
    {
        return FALSE;
    }

    dwType  = 0u;
    cbValue = (DWORD)sizeof(*pdwValue);
    lStatus = RegQueryValueEx(hKey, pszValue, NULL, &dwType, (LPBYTE)pdwValue, &cbValue);
    RegCloseKey(hKey);
    if (ERROR_FILE_NOT_FOUND == lStatus)
    {
        return TRUE;
    }
    if ((ERROR_SUCCESS != lStatus) || (REG_DWORD != dwType) || ((DWORD)sizeof(*pdwValue) != cbValue))
    {
        return FALSE;
    }

    *pfHadValue = TRUE;
    return TRUE;
}

static BOOL ThemeTestWriteThemeValue(LPCTSTR pszValue, DWORD dwValue)
{
    HKEY    hKey;
    DWORD   dwDisposition;
    LSTATUS lStatus;

    hKey = NULL;
    dwDisposition = 0u;
    lStatus = RegCreateKeyEx(HKEY_CURRENT_USER,
                             TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
                             0u,
                             NULL,
                             0u,
                             KEY_SET_VALUE,
                             NULL,
                             &hKey,
                             &dwDisposition);
    if (ERROR_SUCCESS != lStatus)
    {
        return FALSE;
    }
    lStatus = RegSetValueEx(hKey,
                            pszValue,
                            0u,
                            REG_DWORD,
                            (const BYTE*)&dwValue,
                            (DWORD)sizeof(dwValue));
    RegCloseKey(hKey);
    return ERROR_SUCCESS == lStatus;
}

static BOOL ThemeTestDeleteThemeValue(LPCTSTR pszValue)
{
    HKEY    hKey;
    LSTATUS lStatus;

    hKey = NULL;
    lStatus = RegOpenKeyEx(HKEY_CURRENT_USER,
                           TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
                           0u,
                           KEY_SET_VALUE,
                           &hKey);
    if (ERROR_SUCCESS != lStatus)
    {
        return ERROR_FILE_NOT_FOUND == lStatus;
    }
    lStatus = RegDeleteValue(hKey, pszValue);
    RegCloseKey(hKey);
    return (ERROR_SUCCESS == lStatus) || (ERROR_FILE_NOT_FOUND == lStatus);
}

static BOOL ThemeTestReadAppsUseLightTheme(DWORD* pdwValue, BOOL* pfHadValue)
{
    (void)ThemeTestReadThemeValue(TEXT("SystemUsesLightTheme"), &g_dwThemeSystemValue, &g_fThemeHadSystemValue);
    return ThemeTestReadThemeValue(TEXT("AppsUseLightTheme"), pdwValue, pfHadValue);
}

static BOOL ThemeTestWriteAppsUseLightTheme(DWORD dwValue, BOOL fWriteSystem)
{
    BOOL fApps;
    BOOL fSystem;

    fApps = ThemeTestWriteThemeValue(TEXT("AppsUseLightTheme"), dwValue);
    if (!fWriteSystem)
    {
        return fApps;
    }
    fSystem = ThemeTestWriteThemeValue(TEXT("SystemUsesLightTheme"), dwValue);
    return fApps && fSystem;
}

static BOOL ThemeTestRestoreAppsUseLightTheme(BOOL fHadValue, DWORD dwValue)
{
    BOOL fApps;
    BOOL fSystem;

    if (g_fThemeHadSystemValue)
    {
        fSystem = ThemeTestWriteThemeValue(TEXT("SystemUsesLightTheme"), g_dwThemeSystemValue);
    }
    else
    {
        fSystem = ThemeTestDeleteThemeValue(TEXT("SystemUsesLightTheme"));
    }
    if (fHadValue)
    {
        fApps = ThemeTestWriteThemeValue(TEXT("AppsUseLightTheme"), dwValue);
    }
    else
    {
        fApps = ThemeTestDeleteThemeValue(TEXT("AppsUseLightTheme"));
    }
    return fApps && fSystem;
}

static BOOL ThemeTestBroadcastImmersiveColorSet(void)
{
    DWORD dwRecipients;
    LONG  lResult;

    dwRecipients = BSM_APPLICATIONS;
    lResult = BroadcastSystemMessage(BSF_FORCEIFHUNG | BSF_NOTIMEOUTIFNOTHUNG,
                                     &dwRecipients,
                                     WM_SETTINGCHANGE,
                                     0,
                                     (LPARAM)TEXT("ImmersiveColorSet"));
    return 0 <= lResult;
}

static BOOL ThemeTestInitCommonControls(void)
{
    HMODULE hComctl;
    PFN_INITCOMMONCONTROLSEX pfnInitCommonControlsEx;
    INITCOMMONCONTROLSEX icc;

    hComctl = LoadLibrary(TEXT("comctl32.dll"));
    if (!hComctl)
    {
        return FALSE;
    }
    pfnInitCommonControlsEx = (PFN_INITCOMMONCONTROLSEX)GetProcAddress(hComctl, "InitCommonControlsEx");
    if (!pfnInitCommonControlsEx)
    {
        FreeLibrary(hComctl);
        return FALSE;
    }
    icc.dwSize = (DWORD)sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    return pfnInitCommonControlsEx(&icc);
}

static BOOL ThemeTestRegisterClass(LPCTSTR pszClass)
{
    WNDCLASS wc;

    SecureZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = ThemeTestWndProc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.lpszClassName = pszClass;
    wc.hbrBackground = pfnTestThemeBackgroundBrush(pfnTestThemeIsDarkMode());
    if (0 != RegisterClass(&wc))
    {
        return TRUE;
    }
    /* The window class is process-global; the second split-transition run reuses what the first
       registered, so an already-registered class is success, not failure. */
    return ERROR_CLASS_ALREADY_EXISTS == GetLastError();
}

static void ThemeTestClearDeferredMessage(HWND hwnd)
{
    MSG msg;

    while (PeekMessage(&msg, hwnd, THEME_TEST_DEFERRED_MSG, THEME_TEST_DEFERRED_MSG, PM_REMOVE))
    {
    }
}

static void ThemeTestPumpMessages(void)
{
    MSG msg;

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static void ThemeTestPumpForMs(DWORD dwMs)
{
    LARGE_INTEGER liFreq;
    LARGE_INTEGER liStart;
    LARGE_INTEGER liNow;

    if (!QueryPerformanceFrequency(&liFreq))
    {
        Sleep(dwMs);
        return;
    }
    QueryPerformanceCounter(&liStart);
    while (TRUE)
    {
        ThemeTestPumpMessages();
        QueryPerformanceCounter(&liNow);
        if (((liNow.QuadPart - liStart.QuadPart) * 1000) >= (liFreq.QuadPart * dwMs))
        {
            break;
        }
        Sleep(1u);
    }
}

static void ThemeTestCopyBytes(BYTE* pbDst, const BYTE* pbSrc, SIZE_T cb)
{
    while (0u != cb)
    {
        *pbDst = *pbSrc;
        ++pbDst;
        ++pbSrc;
        --cb;
    }
}

static UINT ThemeTestRefreshDelayMs(void)
{
    DEVMODE dm;
    DWORD   dwHz;
    UINT    uDelay;

    SecureZeroMemory(&dm, sizeof(dm));
    dm.dmSize = (WORD)sizeof(dm);
    dwHz = 60u;
    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm))
    {
        if ((30u <= dm.dmDisplayFrequency) && (240u >= dm.dmDisplayFrequency))
        {
            dwHz = dm.dmDisplayFrequency;
        }
    }
    uDelay = (UINT)((1000u + (dwHz - 1u)) / dwHz);
    if (1u > uDelay)
    {
        uDelay = 1u;
    }
    return uDelay;
}

static DWORD WINAPI ThemeTestTransitionThread(LPVOID pv)
{
    UNREFERENCED_PARAMETER(pv);
    WaitForSingleObject(g_hThemeStartEvent, WAIT_MS);
    g_fThemeWorkerWrote = ThemeTestWriteAppsUseLightTheme(g_dwThemeWorkerValue, g_fThemeWorkerWriteSystem);
    if (g_fThemeWorkerWrote)
    {
        g_fThemeWorkerBroadcast = ThemeTestBroadcastImmersiveColorSet();
    }
    return 0u;
}

static BOOL ThemeTestClassifyPixel(COLORREF cr, BOOL* pfDark)
{
    BYTE r;
    BYTE g;
    BYTE b;
    BOOL fDark;
    BOOL fLight;

    if (CLR_INVALID == cr)
    {
        return FALSE;
    }

    r = GetRValue(cr);
    g = GetGValue(cr);
    b = GetBValue(cr);
    fDark  = (80u >= r) && (80u >= g) && (80u >= b);
    fLight = (200u <= r) && (200u <= g) && (200u <= b);
    if (!fDark && !fLight)
    {
        return FALSE;
    }
    *pfDark = fDark;
    return TRUE;
}

static BOOL ThemeTestClassifyIntermediatePixel(COLORREF cr)
{
    BYTE r;
    BYTE g;
    BYTE b;
    BYTE cMax;
    BYTE cMin;

    if (CLR_INVALID == cr)
    {
        return FALSE;
    }

    r = GetRValue(cr);
    g = GetGValue(cr);
    b = GetBValue(cr);
    cMax = r;
    cMin = r;
    if (g > cMax) { cMax = g; }
    if (b > cMax) { cMax = b; }
    if (g < cMin) { cMin = g; }
    if (b < cMin) { cMin = b; }
    return (80u < cMin) && (200u > cMax) && ((cMax - cMin) <= 8u);
}

static BOOL ThemeTestSamplePoint(HWND hwnd, int x, int y, POINT* ppt)
{
    RECT rc;

    if (!GetClientRect(hwnd, &rc))
    {
        return FALSE;
    }
    ppt->x = rc.left + x;
    ppt->y = rc.top + y;
    MapWindowPoints(hwnd, NULL, ppt, 1u);
    return TRUE;
}

static BOOL ThemeTestMenuPoints(HWND hwnd, POINT* prgpt, UINT* pcpt)
{
    MENUBARINFO mbi;
    LONG        y;
    LONG        cx;

    SecureZeroMemory(&mbi, sizeof(mbi));
    mbi.cbSize = (DWORD)sizeof(mbi);
    if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
    {
        return FALSE;
    }
    y = mbi.rcBar.top + ((mbi.rcBar.bottom - mbi.rcBar.top) / 2);
    cx = mbi.rcBar.right - mbi.rcBar.left;
    if (120 > cx)
    {
        return FALSE;
    }
    prgpt[0].x = mbi.rcBar.right - 120;
    prgpt[0].y = y;
    prgpt[1].x = mbi.rcBar.right - 80;
    prgpt[1].y = y;
    prgpt[2].x = mbi.rcBar.right - 40;
    prgpt[2].y = y;
    *pcpt = 3u;
    return TRUE;
}

static BOOL ThemeTestCaptureRect(HWND hwndTop, HWND hwndDialog, RECT* prc)
{
    RECT rcTop;
    RECT rcDialog;

    if (!GetWindowRect(hwndTop, &rcTop))
    {
        return FALSE;
    }
    prc->left = rcTop.left;
    prc->top = rcTop.top;
    prc->right = rcTop.right;
    prc->bottom = rcTop.bottom;
    /* Union in the dialog only if one was opened (main-window-only capture passes NULL). */
    if (hwndDialog && GetWindowRect(hwndDialog, &rcDialog))
    {
        if (rcDialog.left < prc->left)
        {
            prc->left = rcDialog.left;
        }
        if (rcDialog.top < prc->top)
        {
            prc->top = rcDialog.top;
        }
        if (rcDialog.right > prc->right)
        {
            prc->right = rcDialog.right;
        }
        if (rcDialog.bottom > prc->bottom)
        {
            prc->bottom = rcDialog.bottom;
        }
    }
    prc->left -= 8;
    prc->top -= 8;
    prc->right += 8;
    prc->bottom += 8;
    return TRUE;
}

static BOOL ThemeTestCaptureInit(THEME_CAPTURE* pCap, const RECT* prc, UINT cFrames)
{
    SecureZeroMemory(pCap, sizeof(*pCap));
    pCap->rc = *prc;
    pCap->cx = prc->right - prc->left;
    pCap->cy = prc->bottom - prc->top;
    pCap->cbFrame = pCap->cx * pCap->cy * 4;
    pCap->cFrames = cFrames;
    pCap->cQueueFrames = THEME_ENCODE_QUEUE_FRAMES;
    pCap->ppFrames = (ID3D11Texture2D**)HeapAlloc(GetProcessHeap(),
                                                  HEAP_ZERO_MEMORY,
                                                  (SIZE_T)(sizeof(ID3D11Texture2D*) * pCap->cQueueFrames));
    pCap->ppEncodeFrames = (ID3D11Texture2D**)HeapAlloc(GetProcessHeap(),
                                                        HEAP_ZERO_MEMORY,
                                                        (SIZE_T)(sizeof(ID3D11Texture2D*) * pCap->cFrames));
    return IsNonNull(pCap->ppFrames) && IsNonNull(pCap->ppEncodeFrames);
}

static UINT ThemeTestNativeRecordFrameCount(const THEME_DXGI* pDxgi)
{
    UINT64 ullFrames;

    ullFrames = ((UINT64)pDxgi->uRefreshNumerator * (UINT64)THEME_RECORD_MS);
    ullFrames = (ullFrames + ((UINT64)pDxgi->uRefreshDenominator * 1000u) - 1u) /
                ((UINT64)pDxgi->uRefreshDenominator * 1000u);
    if (THEME_CAPTURE_FRAMES > ullFrames)
    {
        ullFrames = THEME_CAPTURE_FRAMES;
    }
    return (UINT)ullFrames;
}

static UINT ThemeTestExpectedNativeFrames(const THEME_DXGI* pDxgi)
{
    UINT64 ullFrames;

    ullFrames = ((UINT64)pDxgi->uRefreshNumerator * (UINT64)THEME_RECORD_MS);
    ullFrames = ullFrames / ((UINT64)pDxgi->uRefreshDenominator * 1000u);
    return (UINT)ullFrames;
}


static void ThemeTestCaptureFree(THEME_CAPTURE* pCap)
{
    UINT i;

    if (pCap->ppFrames)
    {
        for (i = 0u; i < pCap->cQueueFrames; ++i)
        {
            if (pCap->ppFrames[i])
            {
                ID3D11Texture2D_Release(pCap->ppFrames[i]);
            }
        }
        HeapFree(GetProcessHeap(), 0, pCap->ppFrames);
    }
    if (pCap->ppEncodeFrames)
    {
        for (i = 0u; i < pCap->cFrames; ++i)
        {
            if (pCap->ppEncodeFrames[i])
            {
                ID3D11Texture2D_Release(pCap->ppEncodeFrames[i]);
            }
        }
        HeapFree(GetProcessHeap(), 0, pCap->ppEncodeFrames);
    }
    if (pCap->pbFrames)
    {
        HeapFree(GetProcessHeap(), 0, pCap->pbFrames);
    }
    SecureZeroMemory(pCap, sizeof(*pCap));
}

static void ThemeTestDxgiFree(THEME_DXGI* pDxgi)
{
    if (pDxgi->pStaging)
    {
        ID3D11Texture2D_Release(pDxgi->pStaging);
    }
    if (pDxgi->pDup)
    {
        IDXGIOutputDuplication_Release(pDxgi->pDup);
    }
    if (pDxgi->pContext)
    {
        ID3D11DeviceContext_Release(pDxgi->pContext);
    }
    if (pDxgi->pDevice)
    {
        ID3D11Device_Release(pDxgi->pDevice);
    }
    SecureZeroMemory(pDxgi, sizeof(*pDxgi));
}

static BOOL ThemeTestDxgiInit(THEME_DXGI* pDxgi, HWND hwnd)
{
    HMODULE          hDxgi;
    HMODULE          hD3d11;
    PFN_CREATEDXGIFACTORY1 pfnCreateDXGIFactory1;
    PFN_D3D11CREATEDEVICE pfnD3D11CreateDevice;
    IDXGIFactory1*  pFactory;
    IDXGIAdapter1*  pAdapter;
    IDXGIOutput*    pOutput;
    IDXGIOutput1*   pOutput1;
    DXGI_OUTPUT_DESC desc;
    DEVMODEW        dm;
    HMONITOR        hMonitor;
    UINT            iAdapter;
    UINT            iOutput;
    HRESULT         hr;
    D3D_FEATURE_LEVEL rgLevels[3];
    D3D_FEATURE_LEVEL levelGot;
    BOOL            fFound;

    SecureZeroMemory(pDxgi, sizeof(*pDxgi));
    hDxgi = LoadLibrary(TEXT("dxgi.dll"));
    hD3d11 = LoadLibrary(TEXT("d3d11.dll"));
    if (!hDxgi || !hD3d11)
    {
        if (hDxgi)
        {
            FreeLibrary(hDxgi);
        }
        if (hD3d11)
        {
            FreeLibrary(hD3d11);
        }
        return FALSE;
    }
    pfnCreateDXGIFactory1 = (PFN_CREATEDXGIFACTORY1)GetProcAddress(hDxgi, "CreateDXGIFactory1");
    pfnD3D11CreateDevice = (PFN_D3D11CREATEDEVICE)GetProcAddress(hD3d11, "D3D11CreateDevice");
    if (!pfnCreateDXGIFactory1 || !pfnD3D11CreateDevice)
    {
        FreeLibrary(hD3d11);
        FreeLibrary(hDxgi);
        return FALSE;
    }
    rgLevels[0] = D3D_FEATURE_LEVEL_11_1;
    rgLevels[1] = D3D_FEATURE_LEVEL_11_0;
    rgLevels[2] = D3D_FEATURE_LEVEL_10_0;
    hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    pFactory = NULL;
    hr = pfnCreateDXGIFactory1(&ThemeTestIID_IDXGIFactory1, (void**)&pFactory);
    if (FAILED(hr))
    {
        FreeLibrary(hD3d11);
        FreeLibrary(hDxgi);
        return FALSE;
    }

    fFound = FALSE;
    for (iAdapter = 0u; !fFound; ++iAdapter)
    {
        pAdapter = NULL;
        if (DXGI_ERROR_NOT_FOUND == IDXGIFactory1_EnumAdapters1(pFactory, iAdapter, &pAdapter))
        {
            break;
        }
        for (iOutput = 0u; !fFound; ++iOutput)
        {
            pOutput = NULL;
            if (DXGI_ERROR_NOT_FOUND == IDXGIAdapter1_EnumOutputs(pAdapter, iOutput, &pOutput))
            {
                break;
            }
            SecureZeroMemory(&desc, sizeof(desc));
            hr = IDXGIOutput_GetDesc(pOutput, &desc);
            if (SUCCEEDED(hr) && (desc.Monitor == hMonitor))
            {
                pOutput1 = NULL;
                hr = IDXGIOutput_QueryInterface(pOutput, &ThemeTestIID_IDXGIOutput1, (void**)&pOutput1);
                if (SUCCEEDED(hr))
                {
                    hr = pfnD3D11CreateDevice((IDXGIAdapter*)pAdapter,
                                               D3D_DRIVER_TYPE_UNKNOWN,
                                               NULL,
                                               D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                               rgLevels,
                                               ARRAYSIZE(rgLevels),
                                               D3D11_SDK_VERSION,
                                               &pDxgi->pDevice,
                                               &levelGot,
                                               &pDxgi->pContext);
                    if (SUCCEEDED(hr))
                    {
                        hr = IDXGIOutput1_DuplicateOutput(pOutput1, (IUnknown*)pDxgi->pDevice, &pDxgi->pDup);
                        if (SUCCEEDED(hr))
                        {
                            pDxgi->xOutput = desc.DesktopCoordinates.left;
                            pDxgi->yOutput = desc.DesktopCoordinates.top;
                            pDxgi->cxDesktop =
                                (UINT)(desc.DesktopCoordinates.right - desc.DesktopCoordinates.left);
                            pDxgi->cyDesktop =
                                (UINT)(desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top);
                            SecureZeroMemory(&dm, sizeof(dm));
                            dm.dmSize = (WORD)sizeof(dm);
                            if (EnumDisplaySettingsW(desc.DeviceName, ENUM_CURRENT_SETTINGS, &dm) &&
                                (1u <= dm.dmDisplayFrequency))
                            {
                                pDxgi->uRefreshNumerator = dm.dmDisplayFrequency;
                                pDxgi->uRefreshDenominator = 1u;
                            }
                            else
                            {
                                pDxgi->uRefreshNumerator = 60u;
                                pDxgi->uRefreshDenominator = 1u;
                            }
                            fFound = TRUE;
                        }
                    }
                    IDXGIOutput1_Release(pOutput1);
                }
            }
            IDXGIOutput_Release(pOutput);
        }
        IDXGIAdapter1_Release(pAdapter);
    }
    IDXGIFactory1_Release(pFactory);
    if (!fFound)
    {
        ThemeTestDxgiFree(pDxgi);
        FreeLibrary(hD3d11);
        FreeLibrary(hDxgi);
    }
    return fFound;
}

static BOOL ThemeTestDxgiEnsureStaging(THEME_DXGI* pDxgi, ID3D11Texture2D* pTex)
{
    D3D11_TEXTURE2D_DESC desc;

    if (pDxgi->pStaging)
    {
        return TRUE;
    }
    ID3D11Texture2D_GetDesc(pTex, &desc);
    desc.BindFlags = 0u;
    desc.MiscFlags = 0u;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    return SUCCEEDED(ID3D11Device_CreateTexture2D(pDxgi->pDevice, &desc, NULL, &pDxgi->pStaging));
}

static BOOL ThemeTestDxgiCopyFrame(THEME_DXGI* pDxgi, THEME_CAPTURE* pCap, ID3D11Texture2D* pTex, UINT iFrame)
{
    D3D11_BOX box;
    LONG xSrcLong;
    LONG ySrcLong;
    LONG cEncoded;
    UINT xSrc;
    UINT ySrc;
    UINT iSlot;

    xSrcLong = pCap->rc.left - pDxgi->xOutput;
    ySrcLong = pCap->rc.top - pDxgi->yOutput;
    if ((0 > xSrcLong) || (0 > ySrcLong))
    {
        return FALSE;
    }
    xSrc = (UINT)xSrcLong;
    ySrc = (UINT)ySrcLong;
    if (((UINT)pCap->cx + xSrc) > pDxgi->cxDesktop || (((UINT)pCap->cy + ySrc) > pDxgi->cyDesktop))
    {
        return FALSE;
    }

    cEncoded = InterlockedCompareExchange(&pCap->cEncodedFrames, 0, 0);
    /* The encoder consumes pCap->ppEncodeFrames[iFrame] -- a distinct persistent slot per frame,
     * cFrames deep -- not the small staging ring, so a slow encoder can never cause the capture to
     * overwrite an unencoded frame. Bound real-time lag by that persistent depth (a transient
     * scheduling stall is not a dropped frame); the ring index below still wraps at cQueueFrames. */
    if (((LONG)iFrame - cEncoded) >= (LONG)pCap->cFrames)
    {
        pCap->fEncodeOverflow = TRUE;
    }
    iSlot = iFrame % pCap->cQueueFrames;
    if (!pCap->ppFrames[iSlot])
    {
        return FALSE;
    }
    box.left = xSrc;
    box.top = ySrc;
    box.front = 0u;
    box.right = xSrc + (UINT)pCap->cx;
    box.bottom = ySrc + (UINT)pCap->cy;
    box.back = 1u;
    ID3D11DeviceContext_CopySubresourceRegion(pDxgi->pContext,
                                              (ID3D11Resource*)pCap->ppFrames[iSlot],
                                              0u,
                                              0u,
                                              0u,
                                              0u,
                                              (ID3D11Resource*)pTex,
                                              0u,
                                              &box);
    ID3D11DeviceContext_CopyResource(pDxgi->pContext,
                                     (ID3D11Resource*)pCap->ppEncodeFrames[iFrame],
                                     (ID3D11Resource*)pCap->ppFrames[iSlot]);
    ID3D11DeviceContext_Flush(pDxgi->pContext);
    pCap->cCaptured = iFrame + 1u;
    InterlockedExchange(&pCap->cReadyFrames, (LONG)(iFrame + 1u));
    if (pCap->hEncodeReady)
    {
        SetEvent(pCap->hEncodeReady);
    }
    return TRUE;
}

static BOOL ThemeTestDxgiRepeatFrame(THEME_DXGI* pDxgi, THEME_CAPTURE* pCap, UINT iFrame)
{
    LONG cEncoded;
    UINT iSlot;
    UINT iPrevSlot;

    if (0u == iFrame)
    {
        return FALSE;
    }
    cEncoded = InterlockedCompareExchange(&pCap->cEncodedFrames, 0, 0);
    /* The encoder consumes pCap->ppEncodeFrames[iFrame] -- a distinct persistent slot per frame,
     * cFrames deep -- not the small staging ring, so a slow encoder can never cause the capture to
     * overwrite an unencoded frame. Bound real-time lag by that persistent depth (a transient
     * scheduling stall is not a dropped frame); the ring index below still wraps at cQueueFrames. */
    if (((LONG)iFrame - cEncoded) >= (LONG)pCap->cFrames)
    {
        pCap->fEncodeOverflow = TRUE;
    }
    iSlot = iFrame % pCap->cQueueFrames;
    iPrevSlot = (iFrame - 1u) % pCap->cQueueFrames;
    if (!pCap->ppFrames[iSlot] || !pCap->ppFrames[iPrevSlot])
    {
        return FALSE;
    }
    ID3D11DeviceContext_CopyResource(pDxgi->pContext,
                                     (ID3D11Resource*)pCap->ppFrames[iSlot],
                                     (ID3D11Resource*)pCap->ppFrames[iPrevSlot]);
    ID3D11DeviceContext_CopyResource(pDxgi->pContext,
                                     (ID3D11Resource*)pCap->ppEncodeFrames[iFrame],
                                     (ID3D11Resource*)pCap->ppFrames[iSlot]);
    ID3D11DeviceContext_Flush(pDxgi->pContext);
    pCap->cCaptured = iFrame + 1u;
    InterlockedExchange(&pCap->cReadyFrames, (LONG)(iFrame + 1u));
    if (pCap->hEncodeReady)
    {
        SetEvent(pCap->hEncodeReady);
    }
    return TRUE;
}

static BOOL ThemeTestAllocateFrameQueue(THEME_DXGI* pDxgi, THEME_CAPTURE* pCap)
{
    D3D11_TEXTURE2D_DESC desc;
    UINT i;

    SecureZeroMemory(&desc, sizeof(desc));
    desc.Width = (UINT)pCap->cx;
    desc.Height = (UINT)pCap->cy;
    desc.MipLevels = 1u;
    desc.ArraySize = 1u;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1u;
    desc.Usage = D3D11_USAGE_DEFAULT;
    for (i = 0u; i < pCap->cQueueFrames; ++i)
    {
        if (FAILED(ID3D11Device_CreateTexture2D(pDxgi->pDevice, &desc, NULL, &pCap->ppFrames[i])))
        {
            return FALSE;
        }
    }
    for (i = 0u; i < pCap->cFrames; ++i)
    {
        if (FAILED(ID3D11Device_CreateTexture2D(pDxgi->pDevice, &desc, NULL, &pCap->ppEncodeFrames[i])))
        {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL ThemeTestFramePixel(const THEME_CAPTURE* pCap, UINT iFrame, POINT ptScreen, COLORREF* pcr)
{
    LONG  x;
    LONG  y;
    BYTE* pb;

    x = ptScreen.x - pCap->rc.left;
    y = ptScreen.y - pCap->rc.top;
    if ((0 > x) || (0 > y) || (x >= pCap->cx) || (y >= pCap->cy))
    {
        return FALSE;
    }
    pb = pCap->pbFrames + (iFrame * pCap->cbFrame) + (((y * pCap->cx) + x) * 4);
    *pcr = RGB(pb[2], pb[1], pb[0]);
    return TRUE;
}

static void ThemeTestCountCapturedPixel(const THEME_CAPTURE* pCap,
                                        UINT iFrame,
                                        POINT pt,
                                        UINT* pcDark,
                                        UINT* pcLight,
                                        UINT* pcClassified,
                                        UINT* pcIntermediate)
{
    COLORREF cr;
    BOOL     fDark;

    if (!ThemeTestFramePixel(pCap, iFrame, pt, &cr))
    {
        return;
    }
    if (ThemeTestClassifyPixel(cr, &fDark))
    {
        *pcClassified = *pcClassified + 1u;
        if (fDark)
        {
            *pcDark = *pcDark + 1u;
        }
        else
        {
            *pcLight = *pcLight + 1u;
        }
    }
    else if (ThemeTestClassifyIntermediatePixel(cr))
    {
        *pcIntermediate = *pcIntermediate + 1u;
    }
}

static UINT ThemeTestCapturedPixelMask(const THEME_CAPTURE* pCap, UINT iFrame, POINT pt)
{
    COLORREF cr;
    BOOL     fDark;

    if (!ThemeTestFramePixel(pCap, iFrame, pt, &cr) || !ThemeTestClassifyPixel(cr, &fDark))
    {
        return 0u;
    }
    if (fDark)
    {
        return 1u;
    }
    return 2u;
}

static UINT ThemeTestCapturedPixelIntermediateMask(const THEME_CAPTURE* pCap, UINT iFrame, POINT pt)
{
    COLORREF cr;

    if (!ThemeTestFramePixel(pCap, iFrame, pt, &cr) || !ThemeTestClassifyIntermediatePixel(cr))
    {
        return 0u;
    }
    return 1u;
}

/*
 * Title-bar captions are classified by luminance, not the per-channel gray thresholds. A top-level
 * window's caption is a flat gray, but the active modal dialog's caption is the system ACCENT color
 * (e.g. a chromatic maroon in dark mode, a pale tint in light mode) which no gray threshold can
 * classify -- so a desynced dialog caption would otherwise be silently unclassifiable and slip past
 * the mixed-frame check. Rec.601 luma cleanly separates the dark accent (low luma) from the light
 * accent (high luma) while still classifying the gray top-level caption correctly.
 */
static BOOL ThemeTestClassifyCaptionPixel(COLORREF cr, BOOL* pfDark)
{
    UINT uLuma;

    if (CLR_INVALID == cr)
    {
        return FALSE;
    }
    uLuma = ((299u * GetRValue(cr)) + (587u * GetGValue(cr)) + (114u * GetBValue(cr))) / 1000u;
    if (110u >= uLuma)
    {
        *pfDark = TRUE;
        return TRUE;
    }
    if (150u <= uLuma)
    {
        *pfDark = FALSE;
        return TRUE;
    }
    return FALSE;
}

static BOOL ThemeTestClassifyCaptionIntermediate(COLORREF cr)
{
    UINT uLuma;

    if (CLR_INVALID == cr)
    {
        return FALSE;
    }
    uLuma = ((299u * GetRValue(cr)) + (587u * GetGValue(cr)) + (114u * GetBValue(cr))) / 1000u;
    return (110u < uLuma) && (150u > uLuma);
}

static void ThemeTestCountCapturedCaptionPixel(const THEME_CAPTURE* pCap,
                                               UINT iFrame,
                                               POINT pt,
                                               UINT* pcDark,
                                               UINT* pcLight,
                                               UINT* pcClassified,
                                               UINT* pcIntermediate)
{
    COLORREF cr;
    BOOL     fDark;

    if (!ThemeTestFramePixel(pCap, iFrame, pt, &cr))
    {
        return;
    }
    if (ThemeTestClassifyCaptionPixel(cr, &fDark))
    {
        *pcClassified = *pcClassified + 1u;
        if (fDark)
        {
            *pcDark = *pcDark + 1u;
        }
        else
        {
            *pcLight = *pcLight + 1u;
        }
    }
    else if (ThemeTestClassifyCaptionIntermediate(cr))
    {
        *pcIntermediate = *pcIntermediate + 1u;
    }
}

static UINT ThemeTestCapturedCaptionMask(const THEME_CAPTURE* pCap, UINT iFrame, POINT pt)
{
    COLORREF cr;
    BOOL     fDark;

    if (!ThemeTestFramePixel(pCap, iFrame, pt, &cr) || !ThemeTestClassifyCaptionPixel(cr, &fDark))
    {
        return 0u;
    }
    return fDark ? 1u : 2u;
}

static UINT ThemeTestCapturedCaptionIntermediateMask(const THEME_CAPTURE* pCap, UINT iFrame, POINT pt)
{
    COLORREF cr;

    if (!ThemeTestFramePixel(pCap, iFrame, pt, &cr) || !ThemeTestClassifyCaptionIntermediate(cr))
    {
        return 0u;
    }
    return 1u;
}

static void ThemeTestCountCapturedMenu(const THEME_CAPTURE* pCap,
                                       UINT iFrame,
                                       const POINT* prgpt,
                                       UINT cpt,
                                       UINT* pcDark,
                                       UINT* pcLight,
                                       UINT* pcClassified,
                                       UINT* pcIntermediate)
{
    COLORREF cr;
    BOOL     fDark;
    UINT     cMenuDark;
    UINT     cMenuLight;
    UINT     cMenuIntermediate;
    UINT     i;

    cMenuDark = 0u;
    cMenuLight = 0u;
    cMenuIntermediate = 0u;
    i = 0u;
    while (i < cpt)
    {
        if (ThemeTestFramePixel(pCap, iFrame, prgpt[i], &cr))
        {
            if (ThemeTestClassifyPixel(cr, &fDark))
            {
                if (fDark) { ++cMenuDark; } else { ++cMenuLight; }
            }
            else if (ThemeTestClassifyIntermediatePixel(cr))
            {
                ++cMenuIntermediate;
            }
        }
        ++i;
    }
    if ((cMenuDark + cMenuLight) >= 2u)
    {
        *pcClassified = *pcClassified + 1u;
        if (cMenuDark > cMenuLight)
        {
            *pcDark = *pcDark + 1u;
        }
        else
        {
            *pcLight = *pcLight + 1u;
        }
    }
    else if (2u <= cMenuIntermediate)
    {
        *pcIntermediate = *pcIntermediate + 1u;
    }
}

static UINT ThemeTestCapturedMenuMask(const THEME_CAPTURE* pCap, UINT iFrame, const POINT* prgpt, UINT cpt)
{
    COLORREF cr;
    BOOL     fDark;
    UINT     cMenuDark;
    UINT     cMenuLight;
    UINT     i;

    cMenuDark = 0u;
    cMenuLight = 0u;
    i = 0u;
    while (i < cpt)
    {
        if (ThemeTestFramePixel(pCap, iFrame, prgpt[i], &cr) && ThemeTestClassifyPixel(cr, &fDark))
        {
            if (fDark) { ++cMenuDark; } else { ++cMenuLight; }
        }
        ++i;
    }
    if ((cMenuDark + cMenuLight) < 2u)
    {
        return 0u;
    }
    if (cMenuDark > cMenuLight)
    {
        return 1u;
    }
    return 2u;
}

static UINT ThemeTestCapturedMenuIntermediateMask(const THEME_CAPTURE* pCap,
                                                 UINT iFrame,
                                                 const POINT* prgpt,
                                                 UINT cpt)
{
    COLORREF cr;
    UINT     cMenuIntermediate;
    UINT     i;

    cMenuIntermediate = 0u;
    i = 0u;
    while (i < cpt)
    {
        if (ThemeTestFramePixel(pCap, iFrame, prgpt[i], &cr) && ThemeTestClassifyIntermediatePixel(cr))
        {
            ++cMenuIntermediate;
        }
        ++i;
    }
    return (2u <= cMenuIntermediate) ? 1u : 0u;
}

static void ThemeTestCountCapturedButton(const THEME_CAPTURE* pCap,
                                         UINT iFrame,
                                         HWND hwndButton,
                                         UINT* pcDark,
                                         UINT* pcLight,
                                         UINT* pcClassified,
                                         UINT* pcIntermediate)
{
    POINT pt;
    COLORREF cr;
    BOOL fDark;
    UINT cButtonDark;
    UINT cButtonLight;
    UINT cButtonIntermediate;

    cButtonDark = 0u;
    cButtonLight = 0u;
    cButtonIntermediate = 0u;
    if (ThemeTestSamplePoint(hwndButton, 6, 6, &pt) &&
        ThemeTestFramePixel(pCap, iFrame, pt, &cr))
    {
        if (ThemeTestClassifyPixel(cr, &fDark))
        {
            if (fDark) { ++cButtonDark; } else { ++cButtonLight; }
        }
        else if (ThemeTestClassifyIntermediatePixel(cr)) { ++cButtonIntermediate; }
    }
    if (ThemeTestSamplePoint(hwndButton, 74, 6, &pt) &&
        ThemeTestFramePixel(pCap, iFrame, pt, &cr))
    {
        if (ThemeTestClassifyPixel(cr, &fDark))
        {
            if (fDark) { ++cButtonDark; } else { ++cButtonLight; }
        }
        else if (ThemeTestClassifyIntermediatePixel(cr)) { ++cButtonIntermediate; }
    }
    if (ThemeTestSamplePoint(hwndButton, 6, 18, &pt) &&
        ThemeTestFramePixel(pCap, iFrame, pt, &cr))
    {
        if (ThemeTestClassifyPixel(cr, &fDark))
        {
            if (fDark) { ++cButtonDark; } else { ++cButtonLight; }
        }
        else if (ThemeTestClassifyIntermediatePixel(cr)) { ++cButtonIntermediate; }
    }
    if (ThemeTestSamplePoint(hwndButton, 74, 18, &pt) &&
        ThemeTestFramePixel(pCap, iFrame, pt, &cr))
    {
        if (ThemeTestClassifyPixel(cr, &fDark))
        {
            if (fDark) { ++cButtonDark; } else { ++cButtonLight; }
        }
        else if (ThemeTestClassifyIntermediatePixel(cr)) { ++cButtonIntermediate; }
    }
    if ((cButtonDark + cButtonLight) >= 3u)
    {
        *pcClassified = *pcClassified + 1u;
        if (cButtonDark > cButtonLight)
        {
            *pcDark = *pcDark + 1u;
        }
        else
        {
            *pcLight = *pcLight + 1u;
        }
    }
    else if (cButtonIntermediate >= 3u)
    {
        *pcIntermediate = *pcIntermediate + 1u;
    }
}

static UINT ThemeTestCapturedButtonMask(const THEME_CAPTURE* pCap, UINT iFrame, HWND hwndButton)
{
    POINT pt;
    COLORREF cr;
    BOOL fDark;
    UINT cButtonDark;
    UINT cButtonLight;

    cButtonDark = 0u;
    cButtonLight = 0u;
    if (ThemeTestSamplePoint(hwndButton, 10, 8, &pt) &&
        ThemeTestFramePixel(pCap, iFrame, pt, &cr) &&
        ThemeTestClassifyPixel(cr, &fDark))
    {
        if (fDark) { ++cButtonDark; } else { ++cButtonLight; }
    }
    if (ThemeTestSamplePoint(hwndButton, 10, 16, &pt) &&
        ThemeTestFramePixel(pCap, iFrame, pt, &cr) &&
        ThemeTestClassifyPixel(cr, &fDark))
    {
        if (fDark) { ++cButtonDark; } else { ++cButtonLight; }
    }
    if (ThemeTestSamplePoint(hwndButton, 65, 8, &pt) &&
        ThemeTestFramePixel(pCap, iFrame, pt, &cr) &&
        ThemeTestClassifyPixel(cr, &fDark))
    {
        if (fDark) { ++cButtonDark; } else { ++cButtonLight; }
    }
    if (ThemeTestSamplePoint(hwndButton, 65, 16, &pt) &&
        ThemeTestFramePixel(pCap, iFrame, pt, &cr) &&
        ThemeTestClassifyPixel(cr, &fDark))
    {
        if (fDark) { ++cButtonDark; } else { ++cButtonLight; }
    }
    if ((cButtonDark + cButtonLight) < 3u)
    {
        return 0u;
    }
    if (cButtonDark > cButtonLight)
    {
        return 1u;
    }
    return 2u;
}

static UINT ThemeTestCapturedButtonIntermediateMask(const THEME_CAPTURE* pCap, UINT iFrame, HWND hwndButton)
{
    POINT pt;
    COLORREF cr;
    UINT cButtonIntermediate;

    cButtonIntermediate = 0u;
    if (ThemeTestSamplePoint(hwndButton, 10, 8, &pt) &&
        ThemeTestFramePixel(pCap, iFrame, pt, &cr) &&
        ThemeTestClassifyIntermediatePixel(cr))
    {
        ++cButtonIntermediate;
    }
    if (ThemeTestSamplePoint(hwndButton, 10, 16, &pt) &&
        ThemeTestFramePixel(pCap, iFrame, pt, &cr) &&
        ThemeTestClassifyIntermediatePixel(cr))
    {
        ++cButtonIntermediate;
    }
    if (ThemeTestSamplePoint(hwndButton, 65, 8, &pt) &&
        ThemeTestFramePixel(pCap, iFrame, pt, &cr) &&
        ThemeTestClassifyIntermediatePixel(cr))
    {
        ++cButtonIntermediate;
    }
    if (ThemeTestSamplePoint(hwndButton, 65, 16, &pt) &&
        ThemeTestFramePixel(pCap, iFrame, pt, &cr) &&
        ThemeTestClassifyIntermediatePixel(cr))
    {
        ++cButtonIntermediate;
    }
    return (3u <= cButtonIntermediate) ? 1u : 0u;
}

static int ThemeTestLumaAt(const THEME_CAPTURE* pCap, UINT iFrame, POINT pt)
{
    COLORREF cr;

    if (!ThemeTestFramePixel(pCap, iFrame, pt, &cr))
    {
        return -1;
    }
    return (int)(((299u * GetRValue(cr)) + (587u * GetGValue(cr)) + (114u * GetBValue(cr))) / 1000u);
}

static int ThemeTestMenuLumaAt(const THEME_CAPTURE* pCap, UINT iFrame, const POINT* prgpt, UINT cpt)
{
    int  iSum;
    UINT cValid;
    UINT i;
    int  iLuma;

    iSum = 0;
    cValid = 0u;
    for (i = 0u; i < cpt; ++i)
    {
        iLuma = ThemeTestLumaAt(pCap, iFrame, prgpt[i]);
        if (0 <= iLuma)
        {
            iSum += iLuma;
            ++cValid;
        }
    }
    if (0u == cValid)
    {
        return -1;
    }
    return iSum / (int)cValid;
}

/*
 * Coherence analysis -- color is irrelevant, synchrony is everything. The top-level DWM caption is
 * the transition time-base: at every captured frame its luma is the "where are we in the transition"
 * value, and every other surface (the modal dialog's caption, the owner-drawn menu bar, both client
 * areas, the static, the OK button) must sit in a tight band around it. A surface that starts late,
 * ends late, or jumps independently (flicker) leaves that band. Every frame is checked in sequence;
 * nothing is sampled away or skipped.
 */
#define THEME_SYNC_FRAMES  16   /* curve-independent: how far apart surfaces may start/end (frames) */
/* curve-dependent: normalized-progress band around the caption (%). The client/menu crossfade must
   follow the SAME ease-out curve DWM applies to the caption, so every surface sits within this tight
   band of the caption's progress at every frame. */
#define THEME_BAND_TOL      9
#define THEME_MIN_SPAN    120   /* a real transition moves the caption at least this much luma */

/* Luma of surface k at a frame. k: 0 ref caption, 1 dialog caption, 2 menu bar (avg of its points),
   3 top client, 4 dialog client, 5 static, 6 OK button. */
static int ThemeTestSurfaceLuma(const THEME_CAPTURE* pCap, UINT iFrame, UINT k,
                                POINT ptRef, const POINT* rgSurf, const POINT* rgMenu, UINT cMenu)
{
    static const UINT rgMap[7] = { 0u, 0u, 0u, 1u, 2u, 3u, 4u };

    if (0u == k)
    {
        return ThemeTestLumaAt(pCap, iFrame, ptRef);
    }
    if (2u == k)
    {
        return ThemeTestMenuLumaAt(pCap, iFrame, rgMenu, cMenu);
    }
    return ThemeTestLumaAt(pCap, iFrame, rgSurf[rgMap[k]]);
}

/* Where a surface sits in its OWN transition, 0..100, relative to its own dark/light endpoints --
   absolute color is normalized away (a gray menu bar and a near-white caption are both 100 when
   fully transitioned). The signed denominator carries the dark->light vs light->dark direction. */
static int ThemeTestProgress(int iLuma, int iStart, int iEnd)
{
    int iSpan;

    iSpan = iEnd - iStart;
    if (0 == iSpan)
    {
        return 100;
    }
    return ((iLuma - iStart) * 100) / iSpan;
}

static BOOL ThemeTestAnalyzeCapturedFrames(THEME_CAPTURE* pCap,
                                           HWND hwndTop,
                                           HWND hwndDialog,
                                           HWND hwndStatic,
                                           HWND hwndButton,
                                           BOOL* pfCoherent,
                                           BOOL* pfNoFlicker,
                                           BOOL* pfTransitioned)
{
    POINT rgMenu[3];
    POINT ptRef;
    POINT rgSurf[5];
    UINT  cMenu;
    UINT  i;
    UINT  k;
    UINT  iBase;
    UINT  iLast;
    RECT  rcTop;
    RECT  rcDialog;
    int   rgStart[7];
    int   rgEnd[7];
    UINT  rgStartFrame[7];
    UINT  rgEndFrame[7];
    BOOL  rgStarted[7];
    BOOL  rgEnded[7];
    BOOL  rgActive[7];
    BOOL  fAllStarted;
    BOOL  fAllEnded;
    UINT  uMinStart;
    UINT  uMaxStart;
    UINT  uMinEnd;
    UINT  uMaxEnd;
    int   iMaxBand;
    int   iBandSurf;
    int   iRefAmp;
    UINT  uBandFrame;
    BOOL  fFound;

    /* The main window's menu bar and client are always present; the dialog and its children are
       optional (main-window-only capture passes them NULL). Each surface carries an 'active' flag so
       absent surfaces are simply excluded from the band and the start/end-sync checks rather than
       failing the analysis. */
    for (k = 0u; k < 7u; ++k)
    {
        rgActive[k] = FALSE;
    }
    SecureZeroMemory(rgSurf, sizeof(rgSurf));
    if (!ThemeTestMenuPoints(hwndTop, rgMenu, &cMenu) ||
        !ThemeTestSamplePoint(hwndTop, 24, 96, &rgSurf[1]))
    {
        return FALSE;
    }
    if (!GetWindowRect(hwndTop, &rcTop))
    {
        return FALSE;
    }
    SecureZeroMemory(&rcDialog, sizeof(rcDialog));
    ptRef.x = rcTop.left + 120;
    ptRef.y = rcTop.top + 12;
    rgActive[0] = TRUE;   /* main caption (reference, always active -- no modal dialog dims it) */
    rgActive[2] = TRUE;   /* menu bar     */
    rgActive[3] = TRUE;   /* main client  */
    if (hwndDialog && GetWindowRect(hwndDialog, &rcDialog))
    {
        rgSurf[0].x = rcDialog.left + 190;
        rgSurf[0].y = rcDialog.top + 12;
        rgActive[1] = TRUE;
        if (ThemeTestSamplePoint(hwndDialog, 180, 84, &rgSurf[2]))
        {
            rgActive[4] = TRUE;
        }
    }
    if (hwndStatic && ThemeTestSamplePoint(hwndStatic, 8, 8, &rgSurf[3]))
    {
        rgActive[5] = TRUE;
    }
    if (hwndButton && ThemeTestSamplePoint(hwndButton, 12, 8, &rgSurf[4]))
    {
        rgActive[6] = TRUE;
    }

    /* Establish each surface's pre-transition plateau from a frame that is past the capture's black
       ramp-in and still before the theme flip (the worker only broadcasts well after capture starts,
       so the first stable, non-black caption frame is safely on the initial plateau). */
    fFound = FALSE;
    iBase = 0u;
    for (i = 4u; (i + 6u) < pCap->cCaptured; ++i)
    {
        int iA;
        int iB;
        int iD;

        iA = ThemeTestLumaAt(pCap, i, ptRef);
        iB = ThemeTestLumaAt(pCap, i + 6u, ptRef);
        iD = iA - iB;
        if (0 > iD) { iD = -iD; }
        if ((12 < iA) && (6 >= iD))
        {
            iBase = i;
            fFound = TRUE;
            break;
        }
    }
    if (!fFound)
    {
        return FALSE;
    }
    iLast = pCap->cCaptured - 1u;
    for (k = 0u; k < 7u; ++k)
    {
        rgStart[k] = ThemeTestSurfaceLuma(pCap, iBase, k, ptRef, rgSurf, rgMenu, cMenu);
        rgEnd[k] = ThemeTestSurfaceLuma(pCap, iLast, k, ptRef, rgSurf, rgMenu, cMenu);
        rgStartFrame[k] = 0u;
        rgEndFrame[k] = 0u;
        rgStarted[k] = FALSE;
        rgEnded[k] = FALSE;
    }

    iRefAmp = rgEnd[0] - rgStart[0];
    if (0 > iRefAmp) { iRefAmp = -iRefAmp; }
    *pfTransitioned = (iRefAmp >= THEME_MIN_SPAN);


    /* Single pass over every frame from the plateau on. For each surface compute its own normalized
       progress; record the frame it first crosses 15% (transition start) and 85% (transition end),
       and the worst gap between its progress and the caption's progress at the same frame. */
    iMaxBand = 0;
    iBandSurf = -1;
    uBandFrame = 0u;
    for (i = iBase; i < pCap->cCaptured; ++i)
    {
        int  iRefProg;
        int  iRefPrev;
        int  iRefNext;
        UINT iPrev;
        UINT iNext;

        /* Caption progress at this frame and at its two neighbours. The owner-drawn surfaces are
           painted on the app's wall-clock timer while DWM composites the caption on its own clock, so
           a +/-1 frame phase offset between them is inherent (and its sign flips between the apps-only
           and apps+system paths, depending on whether the app or the system drives the caption flip).
           The band is therefore measured against the closest of the caption's three adjacent frames:
           the curve-match within the band is still strictly enforced, only the unavoidable one-frame
           compositing phase is allowed -- never a larger lead/lag. */
        iPrev = (i > iBase) ? (i - 1u) : iBase;
        iNext = ((i + 1u) < pCap->cCaptured) ? (i + 1u) : i;
        iRefProg = ThemeTestProgress(ThemeTestLumaAt(pCap, i, ptRef), rgStart[0], rgEnd[0]);
        iRefPrev = ThemeTestProgress(ThemeTestLumaAt(pCap, iPrev, ptRef), rgStart[0], rgEnd[0]);
        iRefNext = ThemeTestProgress(ThemeTestLumaAt(pCap, iNext, ptRef), rgStart[0], rgEnd[0]);
        for (k = 0u; k < 7u; ++k)
        {
            int iLuma;
            int iProg;
            int iDev;

            if (!rgActive[k])
            {
                continue;
            }
            iLuma = ThemeTestSurfaceLuma(pCap, i, k, ptRef, rgSurf, rgMenu, cMenu);
            if (0 > iLuma)
            {
                continue;
            }
            iProg = ThemeTestProgress(iLuma, rgStart[k], rgEnd[k]);
            if (!rgStarted[k] && (15 <= iProg))
            {
                rgStarted[k] = TRUE;
                rgStartFrame[k] = i;
            }
            if (rgStarted[k] && !rgEnded[k] && (85 <= iProg))
            {
                rgEnded[k] = TRUE;
                rgEndFrame[k] = i;
            }
            /* The OK button (k==6) is a push button whose face is cross-faded by uxtheme's own
               internal state animation -- its own clock, like DWM's caption -- so it is held to the
               curve-independent duration/correctness checks (it starts and ends with the rest and
               reaches the target) but NOT to the tight curve-dependent color band, which governs the
               surfaces the theme engine itself paints. */
            if (6u != k)
            {
                int iDevPrev;
                int iDevNext;

                iDev     = iProg - iRefProg; if (0 > iDev)     { iDev = -iDev; }
                iDevPrev = iProg - iRefPrev; if (0 > iDevPrev) { iDevPrev = -iDevPrev; }
                iDevNext = iProg - iRefNext; if (0 > iDevNext) { iDevNext = -iDevNext; }
                /* Closest of the caption's three adjacent frames (+/-1 frame phase window). */
                if (iDevPrev < iDev) { iDev = iDevPrev; }
                if (iDevNext < iDev) { iDev = iDevNext; }
                if (iDev > iMaxBand)
                {
                    iMaxBand = iDev;
                    uBandFrame = i + 1u;
                    iBandSurf = (int)k;
                }
            }
        }
    }

    /* Curve-INDEPENDENT (duration): every surface must begin its transition together and finish it
       together -- same start frame, same end frame, within tolerance. Color and curve shape do not
       matter here, only when each surface is in motion. */
    fAllStarted = TRUE;
    fAllEnded = TRUE;
    uMinStart = 0xFFFFFFFFu;
    uMaxStart = 0u;
    uMinEnd = 0xFFFFFFFFu;
    uMaxEnd = 0u;
    for (k = 0u; k < 7u; ++k)
    {
        if (!rgActive[k])
        {
            continue;
        }
        if (!rgStarted[k])
        {
            fAllStarted = FALSE;
        }
        else
        {
            if (rgStartFrame[k] < uMinStart) { uMinStart = rgStartFrame[k]; }
            if (rgStartFrame[k] > uMaxStart) { uMaxStart = rgStartFrame[k]; }
        }
        if (!rgEnded[k])
        {
            fAllEnded = FALSE;
        }
        else
        {
            if (rgEndFrame[k] < uMinEnd) { uMinEnd = rgEndFrame[k]; }
            if (rgEndFrame[k] > uMaxEnd) { uMaxEnd = rgEndFrame[k]; }
        }
    }

    /* pfNoFlicker carries the curve-independent duration/sync verdict; pfCoherent carries the
       curve-dependent caption-banded verdict. */
    *pfNoFlicker = fAllStarted && fAllEnded &&
                   ((uMaxStart - uMinStart) <= THEME_SYNC_FRAMES) &&
                   ((uMaxEnd - uMinEnd) <= THEME_SYNC_FRAMES);
    *pfCoherent = (iMaxBand <= THEME_BAND_TOL);

    pCap->iFirstMixedFrame = uBandFrame;
    pCap->uMixedMask = (UINT)iMaxBand | ((UINT)(iBandSurf + 1) << 16);
    pCap->iLastMixedFrame = (UINT)(fAllStarted ? (uMaxStart - uMinStart) : 999u);
    pCap->uIntermediateMask = (UINT)(fAllEnded ? (uMaxEnd - uMinEnd) : 999u);
    pCap->iFirstTargetFrame = (UINT)(fAllStarted ? uMinStart : 0u);
    pCap->iFirstIntermediateFrame = (UINT)(fAllEnded ? uMaxEnd : 0u);

    if (0u < uBandFrame)
    {
        UINT bf = uBandFrame - 1u;
        OutF(TEXT("[BAND] frame=%d\n"), (int)uBandFrame);
        OutF(TEXT("[BAND] refprog=%d\n"), ThemeTestProgress(ThemeTestLumaAt(pCap, bf, ptRef), rgStart[0], rgEnd[0]));
        for (k = 0u; k < 7u; ++k)
        {
            OutF(TEXT("[BAND] prog=%d\n"),
                 ThemeTestProgress(ThemeTestSurfaceLuma(pCap, bf, k, ptRef, rgSurf, rgMenu, cMenu), rgStart[k], rgEnd[k]));
        }
        OutF(TEXT("[BAND] menustart=%d\n"), rgStart[2]);
        OutF(TEXT("[BAND] menuend=%d\n"), rgEnd[2]);
        OutF(TEXT("[BAND] menuluma=%d\n"), ThemeTestMenuLumaAt(pCap, bf, rgMenu, cMenu));
    }
    return TRUE;
}

static BOOL ThemeTestRecordCompositedFrames(THEME_DXGI* pDxgi, THEME_CAPTURE* pCap)
{
    DXGI_OUTDUPL_FRAME_INFO info;
    IDXGIResource*          pResource;
    ID3D11Texture2D*        pTex;
    HRESULT                 hr;
    UINT                    i;
    LARGE_INTEGER           liFreq;
    LARGE_INTEGER           liStart;
    LARGE_INTEGER           liRecordStart;
    LARGE_INTEGER           liNow;
    LONGLONG                llTarget;

    g_hrThemeCapture = S_OK;
    g_uThemeCaptureStage = 1u;
    if (!QueryPerformanceFrequency(&liFreq))
    {
        g_hrThemeCapture = E_FAIL;
        return FALSE;
    }
    QueryPerformanceCounter(&liStart);

    i = 0u;
    while (TRUE)
    {
        QueryPerformanceCounter(&liNow);
        if (((liNow.QuadPart - liStart.QuadPart) * 1000) >= (liFreq.QuadPart * THEME_DXGI_TIMEOUT_MS))
        {
            g_hrThemeCapture = DXGI_ERROR_WAIT_TIMEOUT;
            g_uThemeCaptureStage = 2u;
            return FALSE;
        }
        ThemeTestPumpMessages();
        SecureZeroMemory(&info, sizeof(info));
        pResource = NULL;
        hr = IDXGIOutputDuplication_AcquireNextFrame(pDxgi->pDup, 1u, &info, &pResource);
        if (DXGI_ERROR_WAIT_TIMEOUT == hr)
        {
            continue;
        }
        if (FAILED(hr))
        {
            g_hrThemeCapture = hr;
            g_uThemeCaptureStage = 3u;
            return FALSE;
        }
        pTex = NULL;
        hr = IDXGIResource_QueryInterface(pResource, &ThemeTestIID_ID3D11Texture2D, (void**)&pTex);
        if (SUCCEEDED(hr))
        {
            hr = ThemeTestDxgiCopyFrame(pDxgi, pCap, pTex, i) ? S_OK : E_FAIL;
            ID3D11Texture2D_Release(pTex);
        }
        IDXGIResource_Release(pResource);
        IDXGIOutputDuplication_ReleaseFrame(pDxgi->pDup);
        if (FAILED(hr))
        {
            g_hrThemeCapture = hr;
            g_uThemeCaptureStage = 4u;
            return FALSE;
        }
        break;
    }
    /* Hold the native-cadence record window shut until the encoder finishes Media Foundation
     * startup and is draining the queue; otherwise the first frames pile up during BeginWriting and
     * push the capture more than the queue depth ahead of the encoder, latching fEncodeOverflow. */
    if (pCap->hEncodeStarted)
    {
        (void)WaitForSingleObject(pCap->hEncodeStarted, THEME_DXGI_TIMEOUT_MS);
    }
    if (g_hThemeStartEvent)
    {
        SetEvent(g_hThemeStartEvent);
    }
    QueryPerformanceCounter(&liRecordStart);

    for (i = 0u; i < pCap->cFrames; ++i)
    {
        if (0u == i)
        {
            continue;
        }
        llTarget = ((LONGLONG)i * liFreq.QuadPart * (LONGLONG)pDxgi->uRefreshDenominator) /
                   (LONGLONG)pDxgi->uRefreshNumerator;
        QueryPerformanceCounter(&liNow);
        while ((liNow.QuadPart - liRecordStart.QuadPart) < llTarget)
        {
            Sleep(0);
            ThemeTestPumpMessages();
            QueryPerformanceCounter(&liNow);
        }
        ThemeTestPumpMessages();
        SecureZeroMemory(&info, sizeof(info));
        pResource = NULL;
        hr = IDXGIOutputDuplication_AcquireNextFrame(pDxgi->pDup, 0u, &info, &pResource);
        if (SUCCEEDED(hr))
        {
            if (1u < info.AccumulatedFrames)
            {
                pCap->fDropped = TRUE;
                if (0u == pCap->iDroppedFrame)
                {
                    pCap->iDroppedFrame = i + 1u;
                }
            }
            if (info.AccumulatedFrames > pCap->cMaxAccumulated)
            {
                pCap->cMaxAccumulated = info.AccumulatedFrames;
            }
            pTex = NULL;
            hr = IDXGIResource_QueryInterface(pResource, &ThemeTestIID_ID3D11Texture2D, (void**)&pTex);
            if (SUCCEEDED(hr))
            {
                hr = ThemeTestDxgiCopyFrame(pDxgi, pCap, pTex, i) ? S_OK : E_FAIL;
                ID3D11Texture2D_Release(pTex);
            }
            IDXGIResource_Release(pResource);
            IDXGIOutputDuplication_ReleaseFrame(pDxgi->pDup);
        }
        else if (DXGI_ERROR_WAIT_TIMEOUT != hr)
        {
            g_hrThemeCapture = hr;
            g_uThemeCaptureStage = 5u;
            return FALSE;
        }
        else if (!ThemeTestDxgiRepeatFrame(pDxgi, pCap, i))
        {
            g_hrThemeCapture = E_FAIL;
            g_uThemeCaptureStage = 6u;
            return FALSE;
        }
    }
    ThemeTestPumpMessages();
    return TRUE;
}

static DWORD WINAPI ThemeTestCaptureThread(LPVOID pv)
{
    THEME_CAPTURE_RUN* pRun;
    int                nOldPriority;

    pRun = (THEME_CAPTURE_RUN*)pv;
    pRun->dwThreadId = GetCurrentThreadId();
    nOldPriority = GetThreadPriority(GetCurrentThread());
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    pRun->fOk = ThemeTestRecordCompositedFrames(pRun->pDxgi, pRun->pCap);
    SetThreadPriority(GetCurrentThread(), nOldPriority);
    SetEvent(pRun->hDone);
    return pRun->fOk ? 0u : 1u;
}

static BOOL ThemeTestPumpUntilDone(HANDLE hDone, DWORD dwTimeoutMs)
{
    LARGE_INTEGER liFreq;
    LARGE_INTEGER liStart;
    LARGE_INTEGER liNow;
    DWORD         dwWait;

    if (!QueryPerformanceFrequency(&liFreq))
    {
        return FALSE;
    }
    QueryPerformanceCounter(&liStart);
    while (TRUE)
    {
        ThemeTestPumpMessages();
        dwWait = WaitForSingleObject(hDone, 1u);
        if (WAIT_OBJECT_0 == dwWait)
        {
            return TRUE;
        }
        if (WAIT_FAILED == dwWait)
        {
            return FALSE;
        }
        QueryPerformanceCounter(&liNow);
        if (((liNow.QuadPart - liStart.QuadPart) * 1000) >= (liFreq.QuadPart * dwTimeoutMs))
        {
            return FALSE;
        }
    }
}

static BOOL ThemeTestMaterializeCapturedFrames(THEME_DXGI* pDxgi, THEME_CAPTURE* pCap)
{
    D3D11_MAPPED_SUBRESOURCE map;
    UINT i;
    UINT y;
    BYTE* pbDst;
    BYTE* pbSrc;
    SIZE_T cbTotal;

    if (0u == pCap->cCaptured)
    {
        return FALSE;
    }
    cbTotal = (SIZE_T)pCap->cbFrame * pCap->cCaptured;
    pCap->pbFrames = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbTotal);
    if (!pCap->pbFrames)
    {
        return FALSE;
    }
    if (!ThemeTestDxgiEnsureStaging(pDxgi, pCap->ppFrames[0]))
    {
        return FALSE;
    }
    for (i = 0u; i < pCap->cCaptured; ++i)
    {
        ID3D11DeviceContext_CopyResource(pDxgi->pContext,
                                         (ID3D11Resource*)pDxgi->pStaging,
                                         (ID3D11Resource*)pCap->ppFrames[i]);
        if (FAILED(ID3D11DeviceContext_Map(pDxgi->pContext,
                                           (ID3D11Resource*)pDxgi->pStaging,
                                           0u,
                                           D3D11_MAP_READ,
                                           0u,
                                           &map)))
        {
            return FALSE;
        }
        pbDst = pCap->pbFrames + (i * pCap->cbFrame);
        pbSrc = (BYTE*)map.pData;
        for (y = 0u; y < (UINT)pCap->cy; ++y)
        {
            ThemeTestCopyBytes(pbDst + (y * pCap->cx * 4u), pbSrc + (y * map.RowPitch), (SIZE_T)(pCap->cx * 4u));
        }
        ID3D11DeviceContext_Unmap(pDxgi->pContext, (ID3D11Resource*)pDxgi->pStaging, 0u);
    }
    return TRUE;
}

static BOOL ThemeTestAppendPathLeafW(LPWSTR pszPath, UINT cchPath, LPCWSTR pszLeaf)
{
    UINT cch;
    UINT cchLeaf;

    cch = lstrlenW(pszPath);
    while ((0u < cch) && (L'\\' != pszPath[cch - 1u]) && (L'/' != pszPath[cch - 1u]))
    {
        --cch;
    }
    pszPath[cch] = 0;
    cchLeaf = lstrlenW(pszLeaf);
    if ((cch + cchLeaf + 1u) > cchPath)
    {
        return FALSE;
    }
    lstrcpyW(pszPath + cch, pszLeaf);
    return TRUE;
}

static BOOL ThemeTestMfResolve(void)
{
    if (g_mf.pfnMFStartup)
    {
        return TRUE;
    }
    g_mf.hMfplat = LoadLibrary(TEXT("mfplat.dll"));
    g_mf.hMfreadwrite = LoadLibrary(TEXT("mfreadwrite.dll"));
    if (!g_mf.hMfplat || !g_mf.hMfreadwrite)
    {
        return FALSE;
    }
    g_mf.pfnMFStartup = (PFN_MFSTARTUP)GetProcAddress(g_mf.hMfplat, "MFStartup");
    g_mf.pfnMFShutdown = (PFN_MFSHUTDOWN)GetProcAddress(g_mf.hMfplat, "MFShutdown");
    g_mf.pfnMFCreateDXGISurfaceBuffer =
        (PFN_MFCREATEDXGISURFACEBUFFER)GetProcAddress(g_mf.hMfplat, "MFCreateDXGISurfaceBuffer");
    g_mf.pfnMFCreateDXGIDeviceManager =
        (PFN_MFCREATEDXGIDEVICEMANAGER)GetProcAddress(g_mf.hMfplat, "MFCreateDXGIDeviceManager");
    g_mf.pfnMFCreateSample = (PFN_MFCREATESAMPLE)GetProcAddress(g_mf.hMfplat, "MFCreateSample");
    g_mf.pfnMFCreateAttributes = (PFN_MFCREATEATTRIBUTES)GetProcAddress(g_mf.hMfplat, "MFCreateAttributes");
    g_mf.pfnMFCreateMediaType = (PFN_MFCREATEMEDIATYPE)GetProcAddress(g_mf.hMfplat, "MFCreateMediaType");
    g_mf.pfnMFCreateSinkWriterFromURL =
        (PFN_MFCREATESINKWRITERFROMURL)GetProcAddress(g_mf.hMfreadwrite, "MFCreateSinkWriterFromURL");
    g_mf.pfnMFCreateSourceReaderFromURL =
        (PFN_MFCREATESOURCEREADERFROMURL)GetProcAddress(g_mf.hMfreadwrite, "MFCreateSourceReaderFromURL");
    return g_mf.pfnMFStartup &&
           g_mf.pfnMFShutdown &&
           g_mf.pfnMFCreateDXGISurfaceBuffer &&
           g_mf.pfnMFCreateDXGIDeviceManager &&
           g_mf.pfnMFCreateSample &&
           g_mf.pfnMFCreateAttributes &&
           g_mf.pfnMFCreateMediaType &&
           g_mf.pfnMFCreateSinkWriterFromURL &&
           g_mf.pfnMFCreateSourceReaderFromURL;
}

static HRESULT ThemeTestSetVideoTypeCommon(IMFMediaType* pType,
                                           REFGUID guidSubtype,
                                           UINT cx,
                                           UINT cy,
                                           UINT uRefreshNumerator,
                                           UINT uRefreshDenominator)
{
    HRESULT hr;

    hr = IMFMediaType_SetGUID(pType, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    if (SUCCEEDED(hr))
    {
        hr = IMFMediaType_SetGUID(pType, &MF_MT_SUBTYPE, guidSubtype);
    }
    if (SUCCEEDED(hr))
    {
        hr = IMFMediaType_SetUINT64(pType, &MF_MT_FRAME_SIZE, (((UINT64)cx) << 32) | (UINT64)cy);
    }
    if (SUCCEEDED(hr))
    {
        hr = IMFMediaType_SetUINT64(
            pType, &MF_MT_FRAME_RATE, (((UINT64)uRefreshNumerator) << 32) | (UINT64)uRefreshDenominator);
    }
    if (SUCCEEDED(hr))
    {
        hr = IMFMediaType_SetUINT64(pType, &MF_MT_PIXEL_ASPECT_RATIO, (((UINT64)1u) << 32) | (UINT64)1u);
    }
    if (SUCCEEDED(hr))
    {
        hr = IMFMediaType_SetUINT32(pType, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    }
    return hr;
}

static BOOL ThemeTestEncodeCapturedFrames(THEME_DXGI* pDxgi, THEME_CAPTURE* pCap)
{
    WCHAR                  szPath[MAX_PATH];
    HRESULT                hr;
    HRESULT                hrCo;
    BOOL                   fCoInit;
    UINT                   uResetToken;
    DWORD                  dwStream;
    LONGLONG               llTime;
    LONGLONG               llDuration;
    LONG                   cReady;
    UINT                   i;
    IMFAttributes*         pAttrs;
    IMFDXGIDeviceManager*  pManager;
    IMFSinkWriter*         pWriter;
    IMFMediaType*          pOutputType;
    IMFMediaType*          pInputType;
    IMFSample*             pSample;
    IMFMediaBuffer*        pBuffer;

    g_uThemeEncodeStage = 1u;
    if ((0u == pCap->cFrames) || (0u == pDxgi->uRefreshNumerator) || (0u == pDxgi->uRefreshDenominator))
    {
        return FALSE;
    }

    if (!GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath)) ||
        !ThemeTestAppendPathLeafW(szPath, ARRAYSIZE(szPath), L"T6-theme-transition.mp4"))
    {
        return FALSE;
    }
    DeleteFileW(szPath);

    fCoInit = FALSE;
    hrCo = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hrCo))
    {
        fCoInit = TRUE;
    }
    else if (RPC_E_CHANGED_MODE != hrCo)
    {
        return FALSE;
    }

    pAttrs = NULL;
    pManager = NULL;
    pWriter = NULL;
    pOutputType = NULL;
    pInputType = NULL;
    if (!ThemeTestMfResolve())
    {
        hr = E_FAIL;
    }
    else
    {
        hr = g_mf.pfnMFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    }
    if (SUCCEEDED(hr))
    {
        g_uThemeEncodeStage = 2u;
        hr = g_mf.pfnMFCreateDXGIDeviceManager(&uResetToken, &pManager);
    }
    if (SUCCEEDED(hr))
    {
        g_uThemeEncodeStage = 3u;
        hr = IMFDXGIDeviceManager_ResetDevice(pManager, (IUnknown*)pDxgi->pDevice, uResetToken);
    }
    if (SUCCEEDED(hr))
    {
        g_uThemeEncodeStage = 4u;
        hr = g_mf.pfnMFCreateAttributes(&pAttrs, 4u);
    }
    if (SUCCEEDED(hr))
    {
        hr = IMFAttributes_SetUINT32(pAttrs, &MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    }
    if (SUCCEEDED(hr))
    {
        hr = IMFAttributes_SetUINT32(pAttrs, &MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
    }
    if (SUCCEEDED(hr))
    {
        hr = IMFAttributes_SetUnknown(pAttrs, &MF_SINK_WRITER_D3D_MANAGER, (IUnknown*)pManager);
    }
    if (SUCCEEDED(hr))
    {
        g_uThemeEncodeStage = 8u;
        hr = g_mf.pfnMFCreateSinkWriterFromURL(szPath, NULL, pAttrs, &pWriter);
    }
    if (SUCCEEDED(hr))
    {
        g_uThemeEncodeStage = 9u;
        hr = g_mf.pfnMFCreateMediaType(&pOutputType);
    }
    if (SUCCEEDED(hr))
    {
        hr = ThemeTestSetVideoTypeCommon(pOutputType,
                                         &MFVideoFormat_H264,
                                         (UINT)pCap->cx,
                                         (UINT)pCap->cy,
                                         pDxgi->uRefreshNumerator,
                                         pDxgi->uRefreshDenominator);
    }
    if (SUCCEEDED(hr))
    {
        hr = IMFMediaType_SetUINT32(pOutputType, &MF_MT_AVG_BITRATE, 12000000u);
    }
    if (SUCCEEDED(hr))
    {
        g_uThemeEncodeStage = 10u;
        hr = IMFSinkWriter_AddStream(pWriter, pOutputType, &dwStream);
    }
    if (SUCCEEDED(hr))
    {
        g_uThemeEncodeStage = 11u;
        hr = g_mf.pfnMFCreateMediaType(&pInputType);
    }
    if (SUCCEEDED(hr))
    {
        hr = ThemeTestSetVideoTypeCommon(pInputType,
                                         &MFVideoFormat_RGB32,
                                         (UINT)pCap->cx,
                                         (UINT)pCap->cy,
                                         pDxgi->uRefreshNumerator,
                                         pDxgi->uRefreshDenominator);
    }
    if (SUCCEEDED(hr))
    {
        g_uThemeEncodeStage = 12u;
        hr = IMFSinkWriter_SetInputMediaType(pWriter, dwStream, pInputType, NULL);
    }
    if (SUCCEEDED(hr))
    {
        g_uThemeEncodeStage = 13u;
        hr = IMFSinkWriter_BeginWriting(pWriter);
    }
    /* Signal that Media Foundation startup is complete and the consume loop is about to run, so the
     * capture thread can open its native-cadence record window without overrunning the encoder. */
    if (pCap->hEncodeStarted)
    {
        SetEvent(pCap->hEncodeStarted);
    }
    llTime = 0;
    llDuration = (10000000LL * (LONGLONG)pDxgi->uRefreshDenominator) / (LONGLONG)pDxgi->uRefreshNumerator;
    if (0 >= llDuration)
    {
        hr = E_FAIL;
    }
    i = 0u;
    while (SUCCEEDED(hr) && (i < pCap->cFrames))
    {
        cReady = InterlockedCompareExchange(&pCap->cReadyFrames, 0, 0);
        while (SUCCEEDED(hr) && ((UINT)cReady <= i))
        {
            if (InterlockedCompareExchange(&pCap->fCaptureComplete, 0, 0) && ((UINT)cReady <= i))
            {
                i = pCap->cFrames;
                break;
            }
            if (WAIT_FAILED == WaitForSingleObject(pCap->hEncodeReady, THEME_DXGI_TIMEOUT_MS))
            {
                hr = E_FAIL;
            }
            cReady = InterlockedCompareExchange(&pCap->cReadyFrames, 0, 0);
        }
        if (i >= pCap->cFrames)
        {
            break;
        }
        if (FAILED(hr))
        {
            break;
        }

        cReady = InterlockedCompareExchange(&pCap->cReadyFrames, 0, 0);
        if ((UINT)cReady <= i)
        {
            continue;
        }
        pSample = NULL;
        pBuffer = NULL;
        g_uThemeEncodeStage = 20u;
        hr = g_mf.pfnMFCreateDXGISurfaceBuffer(&ThemeTestIID_ID3D11Texture2D,
                                               (IUnknown*)pCap->ppEncodeFrames[i],
                                               0u,
                                               FALSE,
                                               &pBuffer);
        if (SUCCEEDED(hr))
        {
            g_uThemeEncodeStage = 21u;
            hr = IMFMediaBuffer_SetCurrentLength(pBuffer, (DWORD)pCap->cbFrame);
        }
        if (SUCCEEDED(hr))
        {
            hr = g_mf.pfnMFCreateSample(&pSample);
        }
        if (SUCCEEDED(hr))
        {
            hr = IMFSample_AddBuffer(pSample, pBuffer);
        }
        if (SUCCEEDED(hr))
        {
            hr = IMFSample_SetSampleTime(pSample, llTime);
        }
        if (SUCCEEDED(hr))
        {
            hr = IMFSample_SetSampleDuration(pSample, llDuration);
        }
        if (SUCCEEDED(hr))
        {
            g_uThemeEncodeStage = 25u;
            hr = IMFSinkWriter_WriteSample(pWriter, dwStream, pSample);
        }
        if (pSample)
        {
            IMFSample_Release(pSample);
        }
        if (pBuffer)
        {
            IMFMediaBuffer_Release(pBuffer);
        }
        llTime += llDuration;
        ++i;
        InterlockedExchange(&pCap->cEncodedFrames, (LONG)i);
    }
    if (SUCCEEDED(hr))
    {
        g_uThemeEncodeStage = 30u;
        if (i == pCap->cFrames)
        {
            hr = IMFSinkWriter_Finalize(pWriter);
        }
        else
        {
            hr = E_FAIL;
        }
    }

    if (pInputType)
    {
        IMFMediaType_Release(pInputType);
    }
    if (pOutputType)
    {
        IMFMediaType_Release(pOutputType);
    }
    if (pWriter)
    {
        IMFSinkWriter_Release(pWriter);
    }
    if (pAttrs)
    {
        IMFAttributes_Release(pAttrs);
    }
    if (pManager)
    {
        IMFDXGIDeviceManager_Release(pManager);
    }
    if (g_mf.pfnMFShutdown)
    {
        g_mf.pfnMFShutdown();
    }
    if (fCoInit)
    {
        CoUninitialize();
    }
    g_hrThemeEncode = hr;
    return SUCCEEDED(hr);
}

static DWORD WINAPI ThemeTestEncodeThread(LPVOID pv)
{
    THEME_ENCODE_RUN* pRun;
    int               nOldPriority;

    pRun = (THEME_ENCODE_RUN*)pv;
    pRun->dwThreadId = GetCurrentThreadId();
    nOldPriority = GetThreadPriority(GetCurrentThread());
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    pRun->fOk = ThemeTestEncodeCapturedFrames(pRun->pDxgi, pRun->pCap);
    SetThreadPriority(GetCurrentThread(), nOldPriority);
    pRun->hr = g_hrThemeEncode;
    pRun->uStage = g_uThemeEncodeStage;
    SetEvent(pRun->hDone);
    return pRun->fOk ? 0u : 1u;
}

static BOOL ThemeTestDecodeCapturedVideo(THEME_DXGI* pDxgi, THEME_CAPTURE* pCap)
{
    WCHAR            szPath[MAX_PATH];
    HRESULT          hr;
    HRESULT          hrCo;
    BOOL             fCoInit;
    IMFAttributes*   pAttrs;
    IMFSourceReader* pReader;
    IMFMediaType*    pType;
    IMFSample*       pSample;
    IMFMediaBuffer*  pBuffer;
    IMF2DBuffer*     p2d;
    BYTE*            pbSrc;
    BYTE*            pbScan0;
    BYTE*            pbDst;
    LONG             lPitch;
    DWORD            dwStreamFlags;
    DWORD            cbMaxLength;
    DWORD            cbCurrentLength;
    UINT             i;
    UINT             y;
    SIZE_T           cbTotal;

    if ((0u == pCap->cCaptured) ||
        !GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath)) ||
        !ThemeTestAppendPathLeafW(szPath, ARRAYSIZE(szPath), L"T6-theme-transition.mp4"))
    {
        return FALSE;
    }

    fCoInit = FALSE;
    hrCo = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hrCo))
    {
        fCoInit = TRUE;
    }
    else if (RPC_E_CHANGED_MODE != hrCo)
    {
        return FALSE;
    }

    pAttrs = NULL;
    pReader = NULL;
    pType = NULL;
    if (!ThemeTestMfResolve())
    {
        hr = E_FAIL;
    }
    else
    {
        hr = g_mf.pfnMFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    }
    if (SUCCEEDED(hr))
    {
        hr = g_mf.pfnMFCreateAttributes(&pAttrs, 3u);
    }
    if (SUCCEEDED(hr))
    {
        hr = IMFAttributes_SetUINT32(pAttrs, &MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    }
    if (SUCCEEDED(hr))
    {
        hr = IMFAttributes_SetUINT32(pAttrs, &MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    }
    if (SUCCEEDED(hr))
    {
        hr = g_mf.pfnMFCreateSourceReaderFromURL(szPath, pAttrs, &pReader);
    }
    if (SUCCEEDED(hr))
    {
        hr = g_mf.pfnMFCreateMediaType(&pType);
    }
    if (SUCCEEDED(hr))
    {
        hr = IMFMediaType_SetGUID(pType, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    }
    if (SUCCEEDED(hr))
    {
        hr = IMFMediaType_SetGUID(pType, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    }
    if (SUCCEEDED(hr))
    {
        hr = IMFSourceReader_SetCurrentMediaType(pReader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
    }
    cbTotal = (SIZE_T)pCap->cbFrame * pCap->cCaptured;
    if (SUCCEEDED(hr))
    {
        pCap->pbFrames = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbTotal);
        if (!pCap->pbFrames)
        {
            hr = E_OUTOFMEMORY;
        }
    }

    i = 0u;
    while (SUCCEEDED(hr) && (i < pCap->cCaptured))
    {
        pSample = NULL;
        dwStreamFlags = 0u;
        hr = IMFSourceReader_ReadSample(pReader,
                                        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                        0u,
                                        NULL,
                                        &dwStreamFlags,
                                        NULL,
                                        &pSample);
        if (FAILED(hr) || (dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM))
        {
            break;
        }
        if (!pSample)
        {
            continue;
        }
        pBuffer = NULL;
        p2d = NULL;
        pbSrc = NULL;
        pbScan0 = NULL;
        cbMaxLength = 0u;
        cbCurrentLength = 0u;
        hr = IMFSample_ConvertToContiguousBuffer(pSample, &pBuffer);
        if (SUCCEEDED(hr) &&
            SUCCEEDED(IMFMediaBuffer_QueryInterface(pBuffer, &ThemeTestIID_IMF2DBuffer, (void**)&p2d)))
        {
            hr = IMF2DBuffer_Lock2D(p2d, &pbScan0, &lPitch);
        }
        else if (SUCCEEDED(hr))
        {
            hr = IMFMediaBuffer_Lock(pBuffer, &pbSrc, &cbMaxLength, &cbCurrentLength);
        }
        if (SUCCEEDED(hr))
        {
            if (pbScan0)
            {
                pbDst = pCap->pbFrames + (i * pCap->cbFrame);
                for (y = 0u; y < (UINT)pCap->cy; ++y)
                {
                    ThemeTestCopyBytes(pbDst + (y * pCap->cx * 4u),
                                       pbScan0 + ((LONG)y * lPitch),
                                       (SIZE_T)(pCap->cx * 4u));
                }
                ++i;
            }
            else
            {
                if (cbCurrentLength < (DWORD)pCap->cbFrame)
                {
                    hr = E_FAIL;
                }
                else
                {
                    pbDst = pCap->pbFrames + (i * pCap->cbFrame);
                    for (y = 0u; y < (UINT)pCap->cy; ++y)
                    {
                        ThemeTestCopyBytes(pbDst + (y * pCap->cx * 4u),
                                           pbSrc + (y * pCap->cx * 4u),
                                           (SIZE_T)(pCap->cx * 4u));
                    }
                    ++i;
                }
            }
        }
        if (pbScan0)
        {
            IMF2DBuffer_Unlock2D(p2d);
        }
        if (pbSrc)
        {
            IMFMediaBuffer_Unlock(pBuffer);
        }
        if (p2d)
        {
            IMF2DBuffer_Release(p2d);
        }
        if (pBuffer)
        {
            IMFMediaBuffer_Release(pBuffer);
        }
        IMFSample_Release(pSample);
    }
    if (0u == i)
    {
        hr = E_FAIL;
    }
    pCap->cCaptured = i;

    if (pType)
    {
        IMFMediaType_Release(pType);
    }
    if (pReader)
    {
        IMFSourceReader_Release(pReader);
    }
    if (pAttrs)
    {
        IMFAttributes_Release(pAttrs);
    }
    if (g_mf.pfnMFShutdown)
    {
        g_mf.pfnMFShutdown();
    }
    if (fCoInit)
    {
        CoUninitialize();
    }
    g_hrThemeDecode = hr;
    return SUCCEEDED(hr);
}

static BOOL ThemeTestDrainCompositedFrames(THEME_DXGI* pDxgi)
{
    DXGI_OUTDUPL_FRAME_INFO info;
    IDXGIResource*          pResource;
    HRESULT                 hr;
    LARGE_INTEGER           liFreq;
    LARGE_INTEGER           liStart;
    LARGE_INTEGER           liNow;

    if (!QueryPerformanceFrequency(&liFreq))
    {
        return FALSE;
    }
    QueryPerformanceCounter(&liStart);

    while (TRUE)
    {
        QueryPerformanceCounter(&liNow);
        if (((liNow.QuadPart - liStart.QuadPart) * 1000) >= (liFreq.QuadPart * THEME_DRAIN_MS))
        {
            return TRUE;
        }
        ThemeTestPumpMessages();
        SecureZeroMemory(&info, sizeof(info));
        pResource = NULL;
        hr = IDXGIOutputDuplication_AcquireNextFrame(pDxgi->pDup, 1u, &info, &pResource);
        if (DXGI_ERROR_WAIT_TIMEOUT == hr)
        {
            return TRUE;
        }
        if (FAILED(hr))
        {
            return FALSE;
        }
        if (pResource)
        {
            IDXGIResource_Release(pResource);
        }
        IDXGIOutputDuplication_ReleaseFrame(pDxgi->pDup);
    }
}

/*
 * The recorded scenario is WindowsProject.exe (the example app), the real theme-integrated process the
 * recording shows. The transition test drives and captures THAT process, not synthetic windows: it
 * CreateProcess's the sibling WindowsProject.exe, finds its main window, and opens its About dialog.
 * The Win32X state machine lives in the app's own process, so the test never calls theme APIs on these
 * cross-process windows -- it drives the switch exactly as Settings would, by flipping the Personalize
 * DWORD(s) and broadcasting WM_SETTINGCHANGE/ImmersiveColorSet, which the app's own WndProc reacts to.
 */
typedef struct THEME_FINDWND
{
    DWORD   dwPid;       /* match this process                                   */
    LPCTSTR pszClass;    /* NULL = any class                                     */
    BOOL    fOwned;      /* TRUE = owned popup (dialog); FALSE = top-level        */
    BOOL    fNeedMenu;   /* TRUE = window must have a menu (the main window)      */
    HWND    hwndFound;
} THEME_FINDWND;

static BOOL CALLBACK ThemeTestEnumFindProc(HWND hwnd, LPARAM lParam)
{
    THEME_FINDWND* pf;
    DWORD          dwPid;
    TCHAR          szClass[64];

    pf = (THEME_FINDWND*)lParam;
    dwPid = 0u;
    GetWindowThreadProcessId(hwnd, &dwPid);
    if (dwPid != pf->dwPid)
    {
        return TRUE;
    }
    if (!IsWindowVisible(hwnd))
    {
        return TRUE;
    }
    if (pf->fOwned != (NULL != GetWindow(hwnd, GW_OWNER)))
    {
        return TRUE;
    }
    if (pf->fNeedMenu && !GetMenu(hwnd))
    {
        return TRUE;
    }
    if (pf->pszClass)
    {
        szClass[0] = 0;
        GetClassName(hwnd, szClass, (int)ARRAYSIZE(szClass));
        if (0 != lstrcmp(szClass, pf->pszClass))
        {
            return TRUE;
        }
    }
    pf->hwndFound = hwnd;
    return FALSE;
}

static HWND ThemeTestWaitForWindow(DWORD dwPid, LPCTSTR pszClass, BOOL fOwned, BOOL fNeedMenu, DWORD dwTimeoutMs)
{
    THEME_FINDWND fw;
    DWORD         dwStart;

    dwStart = GetTickCount();
    while (TRUE)
    {
        SecureZeroMemory(&fw, sizeof(fw));
        fw.dwPid     = dwPid;
        fw.pszClass  = pszClass;
        fw.fOwned    = fOwned;
        fw.fNeedMenu = fNeedMenu;
        EnumWindows(ThemeTestEnumFindProc, (LPARAM)&fw);
        if (fw.hwndFound)
        {
            return fw.hwndFound;
        }
        if ((GetTickCount() - dwStart) >= dwTimeoutMs)
        {
            return NULL;
        }
        ThemeTestPumpMessages();
        Sleep(10u);
    }
}

static BOOL CALLBACK ThemeTestEnumChildProc(HWND hwnd, LPARAM lParam)
{
    THEME_FINDWND* pf;
    TCHAR          szClass[64];

    pf = (THEME_FINDWND*)lParam;
    szClass[0] = 0;
    GetClassName(hwnd, szClass, (int)ARRAYSIZE(szClass));
    if (0 == lstrcmpi(szClass, pf->pszClass))
    {
        pf->hwndFound = hwnd;
        return FALSE;
    }
    return TRUE;
}

static HWND ThemeTestFindChild(HWND hwndParent, LPCTSTR pszClass)
{
    THEME_FINDWND fw;

    SecureZeroMemory(&fw, sizeof(fw));
    fw.pszClass = pszClass;
    EnumChildWindows(hwndParent, ThemeTestEnumChildProc, (LPARAM)&fw);
    return fw.hwndFound;
}

/* Build the path of a sibling-of-parent executable: Tests.exe lives in bin\<Config>\, the example app
   in bin\, so WindowsProject.exe is "<dir of Tests.exe>\..\WindowsProject.exe". */
static BOOL ThemeTestWindowsProjectPath(LPTSTR pszOut, int cchOut)
{
    TCHAR  szDir[MAX_PATH];
    DWORD  cch;
    LPTSTR p;
    LPTSTR pszSlash;

    cch = GetModuleFileName(NULL, szDir, (DWORD)ARRAYSIZE(szDir));
    if ((0u == cch) || (cch >= ARRAYSIZE(szDir)))
    {
        return FALSE;
    }
    pszSlash = szDir;
    for (p = szDir; *p; ++p)
    {
        if ((TEXT('\\') == *p) || (TEXT('/') == *p))
        {
            pszSlash = p;
        }
    }
    *pszSlash = 0;
    /* WindowsProject.exe is emitted to the same output directory as Tests.exe. */
    return 0 < wnsprintf(pszOut, cchOut, TEXT("%s\\WindowsProject.exe"), szDir);
}

/* Tear the launched app down: ask it to close, then terminate if it lingers, and reap the handle so
   the developer's machine is left with no stray WindowsProject.exe from the test run. */
static void ThemeTestKillApp(void)
{
    if (!g_hThemeAppProcess)
    {
        return;
    }
    if (WAIT_OBJECT_0 != WaitForSingleObject(g_hThemeAppProcess, 0u))
    {
        TerminateProcess(g_hThemeAppProcess, 0u);
        WaitForSingleObject(g_hThemeAppProcess, WAIT_MS);
    }
    CloseHandle(g_hThemeAppProcess);
    g_hThemeAppProcess = NULL;
}

/* Launch WindowsProject.exe, find its main window, open its About dialog, and return all four windows
   the analysis samples: main window, About dialog, the dialog's OK button, and a dialog static. The
   About box is modal, which disables the owner and renders the MAIN caption inactive (dimmed) -- so
   the analyzer references the ACTIVE dialog caption and excludes the inactive main caption from the
   band (active-state awareness). Kept as its own function so its path/STARTUPINFO/PROCESS_INFORMATION
   buffers stay off ThemeTestRunTransition's already-large stack frame (a >1-page frame emits __chkstk,
   which this /NODEFAULTLIB build cannot resolve). */
static BOOL ThemeTestLaunchWindowsProject(HWND* phwndTop, HWND* phwndDialog, HWND* phwndStatic, HWND* phwndButton)
{
    TCHAR               szExe[MAX_PATH];
    STARTUPINFO         si;
    PROCESS_INFORMATION pi;

    *phwndTop    = NULL;
    *phwndDialog = NULL;
    *phwndStatic = NULL;
    *phwndButton = NULL;

    SecureZeroMemory(&si, sizeof(si));
    si.cb = (DWORD)sizeof(si);
    SecureZeroMemory(&pi, sizeof(pi));
    if (!ThemeTestWindowsProjectPath(szExe, (int)ARRAYSIZE(szExe)))
    {
        return FALSE;
    }
    if (!CreateProcess(szExe, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        return FALSE;
    }
    g_hThemeAppProcess = pi.hProcess;
    CloseHandle(pi.hThread);
    WaitForInputIdle(pi.hProcess, WAIT_MS);

    *phwndTop = ThemeTestWaitForWindow(pi.dwProcessId, NULL, FALSE, TRUE, WAIT_MS);
    if (!*phwndTop)
    {
        return FALSE;
    }
    /* Post (not Send) IDM_ABOUT: the app opens a MODAL About box; a SendMessage would block here. */
    PostMessage(*phwndTop, WM_COMMAND, (WPARAM)THEME_IDM_ABOUT, 0);
    *phwndDialog = ThemeTestWaitForWindow(pi.dwProcessId, TEXT("#32770"), TRUE, FALSE, WAIT_MS);
    if (!*phwndDialog)
    {
        return FALSE;
    }
    *phwndButton = GetDlgItem(*phwndDialog, IDOK);
    *phwndStatic = ThemeTestFindChild(*phwndDialog, TEXT("Static"));
    /* The modal dialog is already the active/foreground window; its caption transitions over the full
       active span and is the band reference. */
    BringWindowToTop(*phwndDialog);
    SetForegroundWindow(*phwndDialog);
    return (NULL != *phwndButton) && (NULL != *phwndStatic);
}

/* Tagged result emitters: prepend the active scenario tag (e.g. "[apps] ") to the result name so the
   apps-only and apps+system runs of the same transition body produce distinct, greppable lines. */
static void ThemeTestCheck(BOOL fOk, LPCTSTR pszName)
{
    TCHAR szBuf[192];
    wnsprintf(szBuf, ARRAYSIZE(szBuf), TEXT("%s%s"), g_pszThemeTag, pszName);
    Check(fOk, szBuf);
}

static void ThemeTestSkipNote(LPCTSTR pszName, LPCTSTR pszWhy)
{
    TCHAR szBuf[192];
    wnsprintf(szBuf, ARRAYSIZE(szBuf), TEXT("%s%s"), g_pszThemeTag, pszName);
    Skip(szBuf, pszWhy);
}

/*
 * The theme-transition body, parameterized over which Personalize DWORD(s) the worker flips:
 *   fWriteSystem == FALSE -> AppsUseLightTheme only (app theme; DWM caption follows the app's explicit
 *                            DWMWA_USE_IMMERSIVE_DARK_MODE, no system-driven caption crossfade).
 *   fWriteSystem == TRUE  -> AppsUseLightTheme and SystemUsesLightTheme (a full Settings switch; DWM
 *                            additionally runs its own caption crossfade).
 * pszTag labels the emitted results. Both originals are always restored (see helper comment above).
 */
static void ThemeTestRunTransition(BOOL fWriteSystem, LPCTSTR pszTag)
{
    DWORD             dwOriginalValue;
    DWORD             dwTargetValue;
    BOOL              fHadOriginalValue;
    BOOL              fCanUseDarkMode;
    BOOL              fInitialDark;
    BOOL              fTargetDark;
    BOOL              fReadRegistry;
    BOOL              fStartedWorker;
    BOOL              fCommonControls;
    BOOL              fMadeTopClass;
    BOOL              fMadeDialogClass;
    BOOL              fPublished;
    BOOL              fDiagnosticsPublished;
    BOOL              fClassBrushPublished;
    BOOL              fCapturedFrames;
    BOOL              fEncodedFrames;
    BOOL              fCaptureOffGuiThread;
    BOOL              fEncodeOffGuiThread;
    BOOL              fNoDroppedFrames;
    BOOL              fSawTargetFrame;
    BOOL              fSawIntermediateFrame;
    BOOL              fSawMenuIntermediateFrame;
    BOOL              fNoMixedFrames;
    BOOL              fAnalyzedFrames;
    BOOL              fDeferredStable;
    BOOL              fDiagnosticsDeferred;
    BOOL              fRestored;
    HWND              hwndTop;
    HWND              hwndDialog;
    HWND              hwndStatic;
    HWND              hwndButton;
    HMENU             hMenu;
    HANDLE            hThread;
    HANDLE            hCaptureThread;
    HANDLE            hCaptureDone;
    HANDLE            hEncodeThread;
    HANDLE            hEncodeDone;
    HANDLE            hMutex;
    DWORD             dwWait;
    DWORD             dwGuiThreadId;
    UINT              cCapturedFrames;
    UINT              cExpectedFrames;
    RECT              rcCapture;
    RECT              rcTopCreated;
    THEME_CAPTURE     capture;
    THEME_CAPTURE_RUN captureRun;
    THEME_ENCODE_RUN  encodeRun;
    THEME_DXGI        dxgi;
    THEME_DIAGNOSTICS diag;

    g_pszThemeTag = pszTag;
    g_fThemeWorkerWriteSystem = fWriteSystem;

    hMutex = CreateMutex(NULL, FALSE, TEXT("Local\\Win32XThemeTransitionTest"));
    if (!hMutex)
    {
        ThemeTestSkipNote(TEXT("T6 system theme transition contract"), TEXT("theme transition mutex cannot be created"));
        return;
    }
    dwWait = WaitForSingleObject(hMutex, 0u);
    if ((WAIT_OBJECT_0 != dwWait) && (WAIT_ABANDONED != dwWait))
    {
        CloseHandle(hMutex);
        ThemeTestCheck(FALSE, TEXT("T6 system theme transition contract is not run concurrently"));
        return;
    }

    pfnTestThemeStartup();
    dwGuiThreadId = GetCurrentThreadId();
    fCanUseDarkMode = pfnTestThemeCanUseDarkMode();
    if (!fCanUseDarkMode)
    {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        ThemeTestSkipNote(TEXT("T6 system theme transition contract"), TEXT("dark-mode uxtheme/DWM contract unavailable"));
        return;
    }

    fReadRegistry = ThemeTestReadAppsUseLightTheme(&dwOriginalValue, &fHadOriginalValue);
    if (!fReadRegistry)
    {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        ThemeTestSkipNote(TEXT("T6 system theme transition contract"), TEXT("AppsUseLightTheme cannot be read"));
        return;
    }

    fInitialDark  = pfnTestThemeEffectiveDarkMode();
    fTargetDark   = !fInitialDark;
    OutF(TEXT("[INFO] T6 initial dark: %u\n"), fInitialDark);
    OutF(TEXT("[INFO] T6 target dark: %u\n"), fTargetDark);
    dwTargetValue = 1u;
    if (fTargetDark)
    {
        dwTargetValue = 0u;
    }
    fCommonControls = ThemeTestInitCommonControls();
    if (!fCommonControls)
    {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        ThemeTestSkipNote(TEXT("T6 system theme transition contract"), TEXT("common controls v6 unavailable"));
        return;
    }

    fMadeTopClass = FALSE;
    fMadeDialogClass = FALSE;
    hwndTop = NULL;
    hwndDialog = NULL;
    hwndStatic = NULL;
    hwndButton = NULL;
    hMenu = NULL;
    hCaptureThread = NULL;
    hCaptureDone = NULL;
    hEncodeThread = NULL;
    hEncodeDone = NULL;
    SecureZeroMemory(&rcTopCreated, sizeof(rcTopCreated));

    /* Launch the real, theme-integrated WindowsProject.exe and drive the transition on IT. The Win32X
       state machine lives in the app's own process and its WndProc themes itself; the test only flips
       the Personalize DWORD(s) and broadcasts -- exactly a Settings theme switch. */
    (void)ThemeTestLaunchWindowsProject(&hwndTop, &hwndDialog, &hwndStatic, &hwndButton);

    if (!hwndTop || !hwndDialog || !hwndStatic || !hwndButton)
    {
        ThemeTestCheck(FALSE, TEXT("T6 launches WindowsProject.exe and opens its About dialog"));
        fRestored = ThemeTestRestoreAppsUseLightTheme(fHadOriginalValue, dwOriginalValue);
        ThemeTestCheck(fRestored, TEXT("T6 restores AppsUseLightTheme after launch failure"));
        ThemeTestKillApp();
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return;
    }
    ThemeTestCheck(TRUE, TEXT("T6 launches WindowsProject.exe and opens its About dialog"));

    GetWindowRect(hwndTop, &rcTopCreated);
    UpdateWindow(hwndTop);
    UpdateWindow(hwndDialog);
    ThemeTestPumpMessages();
    {
        HDC hdcProbe;
        COLORREF crProbe;

        hdcProbe = GetDC(hwndTop);
        crProbe = CLR_INVALID;
        if (hdcProbe)
        {
            crProbe = GetPixel(hdcProbe, 24, 96);
            ReleaseDC(hwndTop, hdcProbe);
        }
        OutF(TEXT("[INFO] T6 initial client red: %u\n"), GetRValue(crProbe));
        OutF(TEXT("[INFO] T6 initial client green: %u\n"), GetGValue(crProbe));
        OutF(TEXT("[INFO] T6 initial client blue: %u\n"), GetBValue(crProbe));
    }

    g_fThemeExpectedDark  = fTargetDark;
    g_fThemeSawErase      = FALSE;
    g_fThemeSawCtlStatic  = FALSE;
    g_fThemeSawNcPaint    = FALSE;
    g_fThemePaintMismatch = FALSE;
    g_dwThemeWorkerValue = dwTargetValue;
    g_fThemeWorkerWrote = FALSE;
    g_fThemeWorkerBroadcast = FALSE;
    g_hwndThemeTop = hwndTop;
    g_hwndThemeDialog = hwndDialog;
    g_hThemeStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    hThread = NULL;
    if (g_hThemeStartEvent)
    {
        hThread = CreateThread(NULL, 0, ThemeTestTransitionThread, NULL, 0, NULL);
    }
    fStartedWorker = IsNonNull(hThread);

    SecureZeroMemory(&capture, sizeof(capture));
    SecureZeroMemory(&dxgi, sizeof(dxgi));
    fCapturedFrames = FALSE;
    fEncodedFrames = FALSE;
    fCaptureOffGuiThread = FALSE;
    fEncodeOffGuiThread = FALSE;
    fNoDroppedFrames = FALSE;
    cExpectedFrames = 0u;
    fSawTargetFrame = FALSE;
    fSawIntermediateFrame = FALSE;
    fSawMenuIntermediateFrame = FALSE;
    fNoMixedFrames = FALSE;
    fAnalyzedFrames = FALSE;
    if (fStartedWorker &&
        ThemeTestCaptureRect(hwndTop, hwndDialog, &rcCapture) &&
        ThemeTestDxgiInit(&dxgi, hwndTop) &&
        ThemeTestCaptureInit(&capture, &rcCapture, ThemeTestNativeRecordFrameCount(&dxgi)) &&
        ThemeTestAllocateFrameQueue(&dxgi, &capture))
    {
        capture.hEncodeReady = CreateEvent(NULL, FALSE, FALSE, NULL);
        capture.hEncodeStarted = CreateEvent(NULL, TRUE, FALSE, NULL);
        hEncodeDone = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (capture.hEncodeReady && capture.hEncodeStarted && hEncodeDone)
        {
            SecureZeroMemory(&encodeRun, sizeof(encodeRun));
            encodeRun.pDxgi = &dxgi;
            encodeRun.pCap = &capture;
            encodeRun.hDone = hEncodeDone;
            hEncodeThread = CreateThread(NULL, 0, ThemeTestEncodeThread, &encodeRun, 0, NULL);
        }
        if (TRUE)
        {
            hCaptureDone = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (hCaptureDone && hEncodeThread)
            {
                SecureZeroMemory(&captureRun, sizeof(captureRun));
                captureRun.pDxgi = &dxgi;
                captureRun.pCap = &capture;
                captureRun.hDone = hCaptureDone;
                hCaptureThread = CreateThread(NULL, 0, ThemeTestCaptureThread, &captureRun, 0, NULL);
                if (hCaptureThread)
                {
                    if (ThemeTestPumpUntilDone(hCaptureDone, THEME_RECORD_MS + WAIT_MS))
                    {
                        fCapturedFrames = captureRun.fOk;
                    }
                    WaitForSingleObject(hCaptureThread, WAIT_MS);
                    fCaptureOffGuiThread = (0u != captureRun.dwThreadId) && (captureRun.dwThreadId != dwGuiThreadId);
                    CloseHandle(hCaptureThread);
                    hCaptureThread = NULL;
                }
                CloseHandle(hCaptureDone);
                hCaptureDone = NULL;
            }
        }
        InterlockedExchange(&capture.fCaptureComplete, 1);
        if (capture.hEncodeReady)
        {
            SetEvent(capture.hEncodeReady);
        }
        if (hEncodeThread)
        {
            if (ThemeTestPumpUntilDone(hEncodeDone, THEME_RECORD_MS + WAIT_MS))
            {
                fEncodedFrames = encodeRun.fOk;
            }
            WaitForSingleObject(hEncodeThread, WAIT_MS);
            fEncodeOffGuiThread = (0u != encodeRun.dwThreadId) && (encodeRun.dwThreadId != dwGuiThreadId);
            CloseHandle(hEncodeThread);
            hEncodeThread = NULL;
        }
        if (hEncodeDone)
        {
            CloseHandle(hEncodeDone);
            hEncodeDone = NULL;
        }
        if (capture.hEncodeReady)
        {
            CloseHandle(capture.hEncodeReady);
            capture.hEncodeReady = NULL;
        }
        if (capture.hEncodeStarted)
        {
            CloseHandle(capture.hEncodeStarted);
            capture.hEncodeStarted = NULL;
        }
        cExpectedFrames = ThemeTestExpectedNativeFrames(&dxgi);
        if (fEncodedFrames)
        {
            capture.cCaptured = capture.cFrames;
        }
        fNoDroppedFrames = fCapturedFrames &&
                           fEncodedFrames &&
                           (!capture.fEncodeOverflow) &&
                           (capture.cCaptured >= cExpectedFrames) &&
                           (0u < cExpectedFrames);
        fNoMixedFrames = FALSE;
        fSawMenuIntermediateFrame = FALSE;
        fSawTargetFrame = FALSE;
        fAnalyzedFrames = fCapturedFrames &&
                          fEncodedFrames &&
                          ThemeTestDecodeCapturedVideo(&dxgi, &capture) &&
                          ThemeTestAnalyzeCapturedFrames(&capture,
                                                         hwndTop,
                                                         hwndDialog,
                                                         hwndStatic,
                                                         hwndButton,
                                                         &fNoMixedFrames,
                                                         &fSawMenuIntermediateFrame,
                                                         &fSawTargetFrame);
        /* fNoMixedFrames == per-frame coherence, fSawMenuIntermediateFrame == no-flicker,
           fSawTargetFrame == transition-completed. */
        fNoMixedFrames = fAnalyzedFrames && fNoMixedFrames;
        fSawMenuIntermediateFrame = fAnalyzedFrames && fSawMenuIntermediateFrame;
        fSawTargetFrame = fAnalyzedFrames && fSawTargetFrame;
        fSawIntermediateFrame = fAnalyzedFrames;
    }
    if (g_hThemeStartEvent && !fCapturedFrames)
    {
        SetEvent(g_hThemeStartEvent);
    }
    if (hThread)
    {
        WaitForSingleObject(hThread, WAIT_MS);
        CloseHandle(hThread);
    }
    if (g_hThemeStartEvent)
    {
        CloseHandle(g_hThemeStartEvent);
        g_hThemeStartEvent = NULL;
    }
    cCapturedFrames = capture.cCaptured;
    if (0u == cExpectedFrames)
    {
        cExpectedFrames = cCapturedFrames;
    }
    /* Cross-process: the Win32X state machine (ThemeIsDarkMode, diagnostics, class brush, deferred
       reconciliation) lives in WindowsProject.exe's own process, not here, so those in-process
       contract checks cannot be read from the test. What the test owns and verifies is the driver side
       -- the Personalize DWORD(s) were written and ImmersiveColorSet broadcast -- plus the visual
       result captured off the wire (frames, transition completion, the curve band/sync). */
    fPublished = g_fThemeWorkerWrote && g_fThemeWorkerBroadcast;
    (void)diag;

    fRestored = ThemeTestRestoreAppsUseLightTheme(fHadOriginalValue, dwOriginalValue);
    if (fRestored)
    {
        /* Broadcast the restore so the app -- and the rest of the desktop -- returns to the original
           theme; let it settle before tearing the app down so no stale state is left behind. */
        (void)ThemeTestBroadcastImmersiveColorSet();
        ThemeTestPumpForMs(THEME_RECORD_MS);
    }

    /* These are another process's windows -- never DestroyWindow them; terminate the app instead. */
    ThemeTestKillApp();

    ThemeTestCheck(fPublished, TEXT("T6 writes the Personalize DWORD(s) and broadcasts ImmersiveColorSet"));
    ThemeTestCheck(fCaptureOffGuiThread, TEXT("T6 captures off the GUI thread"));
    ThemeTestCheck(fEncodeOffGuiThread, TEXT("T6 encodes off the GUI thread"));
    ThemeTestCheck(fCapturedFrames, TEXT("T6 records DXGI desktop-composited frames"));
    ThemeTestCheck(fEncodedFrames, TEXT("T6 encodes queued DXGI textures during capture"));
    ThemeTestCheck(fNoDroppedFrames, TEXT("T6 records at native monitor refresh without frame-count drops"));
    ThemeTestCheck(fSawIntermediateFrame, TEXT("T6 analyzes decoded transition frames in sequence"));
    ThemeTestCheck(fSawTargetFrame, TEXT("T6 transition completes: caption spans the full shade change"));
    ThemeTestCheck(fSawMenuIntermediateFrame, TEXT("T6 curve-independent: every surface starts and ends its transition together (same duration, synchronized)"));
    ThemeTestCheck(fNoMixedFrames, TEXT("T6 curve-dependent: every surface stays within a tight band of the DWM caption's progress, every frame"));
    if (!fSawTargetFrame || !fNoMixedFrames || !fSawMenuIntermediateFrame || !fNoDroppedFrames)
    {
        OutF(TEXT("[INFO] T6 captured frames: %u\n"), capture.cCaptured);
        OutF(TEXT("[INFO] T6 encode overflow: %u\n"), (UINT)capture.fEncodeOverflow);
        OutF(TEXT("[INFO] T6 expected frames: %u\n"), cExpectedFrames);
        OutF(TEXT("[INFO] T6 capture stage: %u\n"), g_uThemeCaptureStage);
        OutF(TEXT("[INFO] T6 capture hr: 0x%08X\n"), (int)g_hrThemeCapture);
        OutF(TEXT("[INFO] T6 encode stage: %u\n"), encodeRun.uStage);
        OutF(TEXT("[INFO] T6 encode hr: 0x%08X\n"), (int)encodeRun.hr);
        OutF(TEXT("[INFO] T6 decode hr: 0x%08X\n"), (int)g_hrThemeDecode);
        OutF(TEXT("[INFO] T6 dropped frame: %u\n"), capture.iDroppedFrame);
        OutF(TEXT("[INFO] T6 accumulated max: %u\n"), capture.cMaxAccumulated);
        OutF(TEXT("[INFO] T6 worst-deviation frame: %u\n"), capture.iFirstMixedFrame);
        OutF(TEXT("[INFO] T6 worst dev|surf: 0x%08X\n"), capture.uMixedMask);
        OutF(TEXT("[INFO] T6 worst-flicker frame: %u\n"), capture.iLastMixedFrame);
        OutF(TEXT("[INFO] T6 worst flicker delta: %u\n"), capture.uIntermediateMask);
        OutF(TEXT("[INFO] T6 caption luma min: %u\n"), capture.iFirstTargetFrame);
        OutF(TEXT("[INFO] T6 caption luma max: %u\n"), capture.iFirstIntermediateFrame);
    }
    ThemeTestCheck(fRestored, TEXT("T6 restores AppsUseLightTheme"));
    ThemeTestDxgiFree(&dxgi);
    ThemeTestCaptureFree(&capture);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
}

/* Split on registry toggling: T6a flips AppsUseLightTheme alone; T6b flips both AppsUseLightTheme
   and SystemUsesLightTheme. Each runs the full capture/analyze transition and restores both DWORDs. */
static void T_ThemeTransitionAppsOnly(void)
{
    ThemeTestRunTransition(FALSE, TEXT("[apps] "));
}

static void T_ThemeTransitionAppsAndSystem(void)
{
    ThemeTestRunTransition(TRUE, TEXT("[apps+system] "));
}
