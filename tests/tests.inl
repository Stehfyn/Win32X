/*
 * tests.inl -- WinBaseX test cases. Included once by test_harness.c, after the harness primitives
 * (Out/OutF/Check/Skip) and the shared declarations (defines, SECOND_MON, the HasArg macro). Holds only
 * function definitions; every #define and typedef lives in the harness preamble per the source-layout
 * rule. Callees precede callers so no forward prototypes are needed.
 */

#include "msgwal.h"

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
static DWORD g_dwThemeWorkerRestore;   /* original AppsUseLightTheme value to flip back to in-recording */
static BOOL  g_fThemeWorkerHadValue;   /* whether AppsUseLightTheme existed before the test            */
static LPCTSTR g_pszThemeTag = TEXT("");
static HANDLE g_hThemeStartEvent;
static HANDLE g_hThemeAppProcess;   /* launched WindowsProject.exe, terminated at teardown */
static HWND   g_hwndThemeTop;
static HWND   g_hwndThemeDialog;

/* Cross-process message-WAL state (see msgwal.h). The hooks live in MsgWalHook.dll, injected into the
   app GUI thread; these handles + the logger thread live here in Tests.exe. */
static HANDLE     g_hMsgWalMap;
static MSGWAL_HDR* g_pMsgWalHdr;
static HMODULE    g_hMsgWalDll;
static HHOOK      g_hMsgWalHookGet;
static HHOOK      g_hMsgWalHookCwp;
static HANDLE     g_hMsgWalLogThread;
static HANDLE     g_hMsgWalFile;
static volatile LONG g_lMsgWalStop;

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

/* The exact real-Settings broadcast: disassembling uxtheme.dll and themeui.dll shows a light/dark switch
   flips the HKCU Personalize DWORD(s), then calls
   SendNotifyMessageW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, L"ImmersiveColorSet") -- an asynchronous,
   non-blocking fan-out to every top-level window. T7 (the inactive Settings-toggle test) uses this so it
   reproduces the user path from the screen recording, not the synchronous BroadcastSystemMessage above. */
static BOOL ThemeTestSendNotifyImmersiveColorSet(void)
{
    return SendNotifyMessage(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)TEXT("ImmersiveColorSet"));
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
    /* Drive the RESTORE flip from inside the recording too: let the forward transition fully play out,
       then flip the theme back to its original value and re-broadcast. The single capture then spans the
       complete round-trip (initial -> target -> initial) in one straight video, instead of cutting off
       at the target. The post-capture restore in the caller stays as an idempotent safety net. */
    Sleep(THEME_LEG_MS);
    (void)ThemeTestRestoreAppsUseLightTheme(g_fThemeWorkerHadValue, g_dwThemeWorkerRestore);
    (void)ThemeTestBroadcastImmersiveColorSet();
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
    pCap->ppAnalysisFrames = (ID3D11Texture2D**)HeapAlloc(GetProcessHeap(),
                                                          HEAP_ZERO_MEMORY,
                                                          (SIZE_T)(sizeof(ID3D11Texture2D*) * pCap->cFrames));
    pCap->pSurfColor = (COLORREF*)HeapAlloc(GetProcessHeap(),
                                            HEAP_ZERO_MEMORY,
                                            (SIZE_T)(sizeof(COLORREF) * pCap->cFrames * THEME_MAX_SURF));
    return IsNonNull(pCap->ppFrames) && IsNonNull(pCap->ppEncodeFrames) &&
           IsNonNull(pCap->ppAnalysisFrames) && IsNonNull(pCap->pSurfColor);
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
    if (pCap->ppAnalysisFrames)
    {
        for (i = 0u; i < pCap->cFrames; ++i)
        {
            if (pCap->ppAnalysisFrames[i])
            {
                ID3D11Texture2D_Release(pCap->ppAnalysisFrames[i]);
            }
        }
        HeapFree(GetProcessHeap(), 0, pCap->ppAnalysisFrames);
    }
    if (pCap->pSurfColor)
    {
        HeapFree(GetProcessHeap(), 0, pCap->pSurfColor);
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
    /* Independent blit for the reduction compute thread (see ppAnalysisFrames). */
    ID3D11DeviceContext_CopyResource(pDxgi->pContext,
                                     (ID3D11Resource*)pCap->ppAnalysisFrames[iFrame],
                                     (ID3D11Resource*)pCap->ppFrames[iSlot]);
    ID3D11DeviceContext_Flush(pDxgi->pContext);
    pCap->cCaptured = iFrame + 1u;
    InterlockedExchange(&pCap->cReadyFrames, (LONG)(iFrame + 1u));
    if (pCap->hEncodeReady)
    {
        SetEvent(pCap->hEncodeReady);
    }
    if (pCap->hReduceReady)
    {
        SetEvent(pCap->hReduceReady);
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
    /* Independent blit for the reduction compute thread (see ppAnalysisFrames). */
    ID3D11DeviceContext_CopyResource(pDxgi->pContext,
                                     (ID3D11Resource*)pCap->ppAnalysisFrames[iFrame],
                                     (ID3D11Resource*)pCap->ppFrames[iSlot]);
    ID3D11DeviceContext_Flush(pDxgi->pContext);
    pCap->cCaptured = iFrame + 1u;
    InterlockedExchange(&pCap->cReadyFrames, (LONG)(iFrame + 1u));
    if (pCap->hEncodeReady)
    {
        SetEvent(pCap->hEncodeReady);
    }
    if (pCap->hReduceReady)
    {
        SetEvent(pCap->hReduceReady);
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
    /* The analysis set is bound as a shader resource so the reduction compute shader can SRV-read it;
       otherwise identical to the encode set. Separate textures (vs reusing ppEncodeFrames) keep the
       compute thread off the encoder's frames so the two never serialize on a shared resource. */
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    for (i = 0u; i < pCap->cFrames; ++i)
    {
        if (FAILED(ID3D11Device_CreateTexture2D(pDxgi->pDevice, &desc, NULL, &pCap->ppAnalysisFrames[i])))
        {
            return FALSE;
        }
    }
    return TRUE;
}

/*
 * GPU reduction pipeline. Creates the reduction compute shader and its buffers; reduces each captured
 * analysis frame to a per-surface exact-mean color via Dispatch; reads the whole time series back once.
 * The device is created without D3D11_CREATE_DEVICE_SINGLETHREADED, so the immediate context is
 * internally synchronized -- the reduce thread shares it with the capture thread safely (they serialize
 * on D3D's lock, but operate on disjoint resources so neither blocks on the other's frames).
 */
static void ThemeTestComputeFree(THEME_DXGI* pDxgi)
{
    if (pDxgi->pReduceStaging)     { ID3D11Buffer_Release(pDxgi->pReduceStaging);          pDxgi->pReduceStaging = NULL; }
    if (pDxgi->pReduceResultsUAV)  { ID3D11UnorderedAccessView_Release(pDxgi->pReduceResultsUAV); pDxgi->pReduceResultsUAV = NULL; }
    if (pDxgi->pReduceResults)     { ID3D11Buffer_Release(pDxgi->pReduceResults);          pDxgi->pReduceResults = NULL; }
    if (pDxgi->pReduceParams)      { ID3D11Buffer_Release(pDxgi->pReduceParams);           pDxgi->pReduceParams = NULL; }
    if (pDxgi->pReduceCS)          { ID3D11ComputeShader_Release(pDxgi->pReduceCS);        pDxgi->pReduceCS = NULL; }
    pDxgi->fComputeReady = FALSE;
}

static BOOL ThemeTestComputeInit(THEME_DXGI* pDxgi, const THEME_CAPTURE* pCap)
{
    D3D11_BUFFER_DESC                desc;
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav;
    UINT                             cElems;

    /* cs_5_0 + structured-buffer UAV require feature level 11_0+. On a 10_0 device CreateComputeShader
       fails and fComputeReady stays FALSE; the caller then skips the GPU-reduced analysis with a note. */
    if (FAILED(ID3D11Device_CreateComputeShader(pDxgi->pDevice,
                                                g_themeReduceCS,
                                                (SIZE_T)sizeof(g_themeReduceCS),
                                                NULL,
                                                &pDxgi->pReduceCS)))
    {
        return FALSE;
    }

    cElems = pCap->cFrames * THEME_MAX_SURF;

    SecureZeroMemory(&desc, sizeof(desc));
    desc.ByteWidth = (UINT)sizeof(THEME_REDUCE_PARAMS);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(ID3D11Device_CreateBuffer(pDxgi->pDevice, &desc, NULL, &pDxgi->pReduceParams)))
    {
        ThemeTestComputeFree(pDxgi);
        return FALSE;
    }

    SecureZeroMemory(&desc, sizeof(desc));
    desc.ByteWidth = cElems * 16u;                 /* uint4 per (frame, surface) */
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = 16u;
    if (FAILED(ID3D11Device_CreateBuffer(pDxgi->pDevice, &desc, NULL, &pDxgi->pReduceResults)))
    {
        ThemeTestComputeFree(pDxgi);
        return FALSE;
    }

    SecureZeroMemory(&uav, sizeof(uav));
    uav.Format = DXGI_FORMAT_UNKNOWN;
    uav.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uav.Buffer.FirstElement = 0u;
    uav.Buffer.NumElements = cElems;
    if (FAILED(ID3D11Device_CreateUnorderedAccessView(pDxgi->pDevice,
                                                      (ID3D11Resource*)pDxgi->pReduceResults,
                                                      &uav,
                                                      &pDxgi->pReduceResultsUAV)))
    {
        ThemeTestComputeFree(pDxgi);
        return FALSE;
    }

    SecureZeroMemory(&desc, sizeof(desc));
    desc.ByteWidth = cElems * 16u;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(ID3D11Device_CreateBuffer(pDxgi->pDevice, &desc, NULL, &pDxgi->pReduceStaging)))
    {
        ThemeTestComputeFree(pDxgi);
        return FALSE;
    }

    pDxgi->fComputeReady = TRUE;
    return TRUE;
}

/* Capture-space sample patch for surface s: a (2*HALF)^2 box centered on the screen-space sample point,
   clamped to the captured region. Returned in p->gRects[s] = {x, y, w, h}. */
static void ThemeTestComputeSurfaceRect(const THEME_CAPTURE* pCap, UINT s, THEME_REDUCE_PARAMS* p)
{
    LONG x;
    LONG y;
    LONG w;
    LONG h;

    x = (pCap->rgSurfPt[s].x - pCap->rc.left) - THEME_SURF_HALF;
    y = (pCap->rgSurfPt[s].y - pCap->rc.top) - THEME_SURF_HALF;
    w = 2 * THEME_SURF_HALF;
    h = 2 * THEME_SURF_HALF;
    if (x < 0) { x = 0; }
    if (y < 0) { y = 0; }
    if ((x + w) > pCap->cx) { w = pCap->cx - x; }
    if ((y + h) > pCap->cy) { h = pCap->cy - y; }
    if (w < 0) { w = 0; }
    if (h < 0) { h = 0; }
    p->gRects[s][0] = (UINT)x;
    p->gRects[s][1] = (UINT)y;
    p->gRects[s][2] = (UINT)w;
    p->gRects[s][3] = (UINT)h;
}

static BOOL ThemeTestComputeReduceFrame(THEME_DXGI* pDxgi, const THEME_CAPTURE* pCap,
                                        THEME_REDUCE_PARAMS* pParams, UINT iFrame)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srv;
    ID3D11ShaderResourceView*       pSRV;
    ID3D11ShaderResourceView*       pNullSRV;
    ID3D11UnorderedAccessView*      pNullUAV;

    SecureZeroMemory(&srv, sizeof(srv));
    srv.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MostDetailedMip = 0u;
    srv.Texture2D.MipLevels = 1u;
    pSRV = NULL;
    if (FAILED(ID3D11Device_CreateShaderResourceView(pDxgi->pDevice,
                                                     (ID3D11Resource*)pCap->ppAnalysisFrames[iFrame],
                                                     &srv,
                                                     &pSRV)))
    {
        return FALSE;
    }

    pParams->gFrameSlot = iFrame;
    ID3D11DeviceContext_UpdateSubresource(pDxgi->pContext,
                                          (ID3D11Resource*)pDxgi->pReduceParams,
                                          0u, NULL, pParams, 0u, 0u);

    ID3D11DeviceContext_CSSetShader(pDxgi->pContext, pDxgi->pReduceCS, NULL, 0u);
    ID3D11DeviceContext_CSSetConstantBuffers(pDxgi->pContext, 0u, 1u, &pDxgi->pReduceParams);
    ID3D11DeviceContext_CSSetShaderResources(pDxgi->pContext, 0u, 1u, &pSRV);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(pDxgi->pContext, 0u, 1u, &pDxgi->pReduceResultsUAV, NULL);
    ID3D11DeviceContext_Dispatch(pDxgi->pContext, pCap->cSurf, 1u, 1u);

    /* Unbind so the SRV's texture is free for the next frame's blit and the UAV for readback. */
    pNullSRV = NULL;
    pNullUAV = NULL;
    ID3D11DeviceContext_CSSetShaderResources(pDxgi->pContext, 0u, 1u, &pNullSRV);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(pDxgi->pContext, 0u, 1u, &pNullUAV, NULL);
    ID3D11ShaderResourceView_Release(pSRV);
    return TRUE;
}

static BOOL ThemeTestComputeReadback(THEME_DXGI* pDxgi, THEME_CAPTURE* pCap)
{
    D3D11_MAPPED_SUBRESOURCE map;
    const UINT*              pu;
    UINT                     i;
    UINT                     cElems;
    UINT                     sumR;
    UINT                     sumG;
    UINT                     sumB;
    UINT                     cN;

    ID3D11DeviceContext_CopyResource(pDxgi->pContext,
                                     (ID3D11Resource*)pDxgi->pReduceStaging,
                                     (ID3D11Resource*)pDxgi->pReduceResults);
    if (FAILED(ID3D11DeviceContext_Map(pDxgi->pContext,
                                       (ID3D11Resource*)pDxgi->pReduceStaging,
                                       0u, D3D11_MAP_READ, 0u, &map)))
    {
        return FALSE;
    }
    pu = (const UINT*)map.pData;
    cElems = pCap->cCaptured * THEME_MAX_SURF;
    for (i = 0u; i < cElems; ++i)
    {
        sumR = pu[(i * 4u) + 0u];
        sumG = pu[(i * 4u) + 1u];
        sumB = pu[(i * 4u) + 2u];
        cN   = pu[(i * 4u) + 3u];
        if (0u == cN)
        {
            pCap->pSurfColor[i] = CLR_INVALID;
        }
        else
        {
            pCap->pSurfColor[i] = RGB((BYTE)(sumR / cN), (BYTE)(sumG / cN), (BYTE)(sumB / cN));
        }
    }
    ID3D11DeviceContext_Unmap(pDxgi->pContext, (ID3D11Resource*)pDxgi->pReduceStaging, 0u);
    return TRUE;
}

/*
 * Reduce-on-GPU worker: a peer of the encode thread. Drains captured analysis frames as they become
 * ready (off cReadyFrames / hReduceReady), dispatching one reduction per frame, then reads the whole
 * per-surface color time series back once capture completes. Independent ppAnalysisFrames slots mean it
 * never contends with the encoder for a texture.
 */
static BOOL ThemeTestReduceCapturedFrames(THEME_DXGI* pDxgi, THEME_CAPTURE* pCap)
{
    THEME_REDUCE_PARAMS params;
    UINT                s;
    LONG                iDone;
    LONG                cReady;

    if (!pDxgi->fComputeReady)
    {
        return FALSE;
    }
    SecureZeroMemory(&params, sizeof(params));
    params.gSurfCount = pCap->cSurf;
    for (s = 0u; s < pCap->cSurf; ++s)
    {
        ThemeTestComputeSurfaceRect(pCap, s, &params);
    }

    iDone = 0;
    while (TRUE)
    {
        cReady = InterlockedCompareExchange(&pCap->cReadyFrames, 0, 0);
        while (iDone < cReady)
        {
            if (!ThemeTestComputeReduceFrame(pDxgi, pCap, &params, (UINT)iDone))
            {
                return FALSE;
            }
            ++iDone;
            InterlockedExchange(&pCap->cReducedFrames, iDone);
        }
        if (InterlockedCompareExchange(&pCap->fCaptureComplete, 0, 0) &&
            (iDone >= InterlockedCompareExchange(&pCap->cReadyFrames, 0, 0)))
        {
            break;
        }
        if (pCap->hReduceReady)
        {
            (void)WaitForSingleObject(pCap->hReduceReady, THEME_DXGI_TIMEOUT_MS);
        }
    }
    return ThemeTestComputeReadback(pDxgi, pCap);
}

static DWORD WINAPI ThemeTestReduceThread(LPVOID pv)
{
    THEME_REDUCE_RUN* pRun;

    pRun = (THEME_REDUCE_RUN*)pv;
    pRun->dwThreadId = GetCurrentThreadId();
    pRun->fOk = ThemeTestReduceCapturedFrames(pRun->pDxgi, pRun->pCap);
    SetEvent(pRun->hDone);
    return pRun->fOk ? 0u : 1u;
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

/* Luma of surface k at a frame, sourced from the GPU-reduced per-surface mean color (pSurfColor),
   NOT a CPU pixel read -- the compute shader integer-averaged each surface's capture-space patch for
   every captured frame on the reduce thread. k: 0 ref caption, 1 dialog caption, 2 menu bar, 3 top
   client, 4 dialog client, 5 static, 6 OK button. Returns -1 when the patch had no pixels (CLR_INVALID).
   The trailing point/menu args are unused now (the GPU patch replaces them) but kept so the analyzer's
   many call sites stay untouched. */
static int ThemeTestSurfaceLuma(const THEME_CAPTURE* pCap, UINT iFrame, UINT k,
                                POINT ptRef, const POINT* rgSurf, const POINT* rgMenu, UINT cMenu)
{
    COLORREF cr;

    UNREFERENCED_PARAMETER(ptRef);
    UNREFERENCED_PARAMETER(rgSurf);
    UNREFERENCED_PARAMETER(rgMenu);
    UNREFERENCED_PARAMETER(cMenu);
    if (!pCap->pSurfColor || (k >= THEME_MAX_SURF) || (iFrame >= pCap->cFrames))
    {
        return -1;
    }
    cr = pCap->pSurfColor[(iFrame * THEME_MAX_SURF) + k];
    if (CLR_INVALID == cr)
    {
        return -1;
    }
    return (int)(((299u * GetRValue(cr)) + (587u * GetGValue(cr)) + (114u * GetBValue(cr))) / 1000u);
}

/*
 * Fill pCap->rgSurfPt / rgSurfActive with the per-surface SCREEN-space sample centers the reduction
 * shader patches -- run BEFORE capture so the reduce thread has them. Surface order matches
 * ThemeTestSurfaceLuma's k: 0 main caption, 1 dialog caption, 2 menu bar (its middle probe), 3 main
 * client, 4 dialog client, 5 dialog static, 6 OK button. The main window's caption/menu/client are
 * always present; the dialog and its children are optional. Must mirror the analyzer's old per-surface
 * sample points exactly so the GPU patch lands on the same place the CPU used to read.
 */
static void ThemeTestFillSurfacePoints(HWND hwndTop, HWND hwndDialog, HWND hwndStatic, HWND hwndButton,
                                       THEME_CAPTURE* pCap)
{
    POINT rgMenu[3];
    POINT pt;
    RECT  rcTop;
    RECT  rcDialog;
    RECT  rcCtl;
    UINT  cMenu;
    UINT  k;

    pCap->cSurf = 7u;
    for (k = 0u; k < THEME_MAX_SURF; ++k)
    {
        pCap->rgSurfActive[k] = FALSE;
        pCap->rgSurfPt[k].x = 0;
        pCap->rgSurfPt[k].y = 0;
    }
    if (!GetWindowRect(hwndTop, &rcTop))
    {
        return;
    }
    /* 0: main caption (reference). */
    pCap->rgSurfPt[0].x = rcTop.left + 120;
    pCap->rgSurfPt[0].y = rcTop.top + 12;
    pCap->rgSurfActive[0] = TRUE;
    /* 2: menu bar -- its middle probe (max-of-3 collapses to one robust box patch on the GPU). */
    if (ThemeTestMenuPoints(hwndTop, rgMenu, &cMenu))
    {
        pCap->rgSurfPt[2] = rgMenu[1];
        pCap->rgSurfActive[2] = TRUE;
    }
    /* 3: main client. */
    if (ThemeTestSamplePoint(hwndTop, 24, 96, &pt))
    {
        pCap->rgSurfPt[3] = pt;
        pCap->rgSurfActive[3] = TRUE;
    }
    if (hwndDialog && GetWindowRect(hwndDialog, &rcDialog))
    {
        /* 1: dialog caption. */
        pCap->rgSurfPt[1].x = rcDialog.left + 190;
        pCap->rgSurfPt[1].y = rcDialog.top + 12;
        pCap->rgSurfActive[1] = TRUE;
        /* 4: dialog client. */
        if (ThemeTestSamplePoint(hwndDialog, 180, 84, &pt))
        {
            pCap->rgSurfPt[4] = pt;
            pCap->rgSurfActive[4] = TRUE;
        }
    }
    /* 5: dialog static -- sample the RIGHT-edge BACKGROUND of the (shortest) text label. Left-aligned
       text leaves the right side as pure control background, which cross-fades cleanly dark->light; the
       glyphs themselves (dark-bg/light-text -> light-bg/dark-text) average to nearly the same mean, and
       the app icon is a fixed bitmap -- both give a degenerate span the band check cannot normalize. */
    if (hwndStatic && GetClientRect(hwndStatic, &rcCtl))
    {
        pt.x = rcCtl.right - THEME_SURF_HALF - 2;
        pt.y = (rcCtl.top + rcCtl.bottom) / 2;
        MapWindowPoints(hwndStatic, NULL, &pt, 1u);
        pCap->rgSurfPt[5] = pt;
        pCap->rgSurfActive[5] = TRUE;
    }
    /* 6: OK button. */
    if (hwndButton && ThemeTestSamplePoint(hwndButton, 12, 8, &pt))
    {
        pCap->rgSurfPt[6] = pt;
        pCap->rgSurfActive[6] = TRUE;
    }
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

/* DECLSPEC_NOINLINE: keep this large analyzer out of ThemeTestRunTransition's frame. Under the tests'
   LTCG, it would otherwise be inlined into that already-large one-shot caller, and the combined frame
   crosses the one-page stack-probe threshold (__chkstk, unresolvable in this /NODEFAULTLIB build). */
static DECLSPEC_NOINLINE BOOL ThemeTestAnalyzeCapturedFrames(THEME_CAPTURE* pCap,
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
    UINT  rgReturnFrame[7];
    BOOL  rgStarted[7];
    BOOL  rgEnded[7];
    BOOL  rgReturned[7];
    BOOL  rgActive[7];
    BOOL  fAllStarted;
    BOOL  fAllEnded;
    BOOL  fAllReturned;
    UINT  uMinStart;
    UINT  uMaxStart;
    UINT  uMinEnd;
    UINT  uMaxEnd;
    UINT  uMinReturn;
    UINT  uMaxReturn;
    int   iMaxBand;
    int   iBandSurf;
    int   iRefAmp;
    UINT  uBandFrame;
    BOOL  fFound;
    int   dbgCliFwd  = 0;
    int   dbgCliRst  = 0;
    int   dbgMenuFwd = 0;
    int   dbgMenuRst = 0;

    /* The main window's menu bar and client are always present; the dialog and its children are
       optional (main-window-only capture passes them NULL). Each surface carries an 'active' flag so
       absent surfaces are simply excluded from the band and the start/end-sync checks rather than
       failing the analysis. */
    UNREFERENCED_PARAMETER(hwndTop);
    UNREFERENCED_PARAMETER(hwndDialog);
    UNREFERENCED_PARAMETER(hwndStatic);
    UNREFERENCED_PARAMETER(hwndButton);
    /* Per-surface sample patches were computed pre-capture (ThemeTestFillSurfacePoints) and reduced on
       the GPU; the analyzer reads each surface's per-frame mean color from pCap->pSurfColor by index
       (ThemeTestSurfaceLuma), so it no longer recomputes screen points here. ptRef/rgSurf/rgMenu/cMenu
       survive only as now-ignored call arguments at the unchanged read sites below. */
    for (k = 0u; k < 7u; ++k)
    {
        rgActive[k] = pCap->rgSurfActive[k];
    }
    SecureZeroMemory(rgSurf, sizeof(rgSurf));
    SecureZeroMemory(rgMenu, sizeof(rgMenu));
    SecureZeroMemory(&ptRef, sizeof(ptRef));
    SecureZeroMemory(&rcTop, sizeof(rcTop));
    SecureZeroMemory(&rcDialog, sizeof(rcDialog));
    cMenu = 0u;
    if (!rgActive[0])
    {
        return FALSE;   /* no reference caption -> nothing to band against */
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

        iA = ThemeTestSurfaceLuma(pCap, i, 0u, ptRef, rgSurf, rgMenu, cMenu);
        iB = ThemeTestSurfaceLuma(pCap, i + 6u, 0u, ptRef, rgSurf, rgMenu, cMenu);
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
    for (k = 0u; k < 7u; ++k)
    {
        rgStart[k] = ThemeTestSurfaceLuma(pCap, iBase, k, ptRef, rgSurf, rgMenu, cMenu);
        rgStartFrame[k] = 0u;
        rgEndFrame[k] = 0u;
        rgReturnFrame[k] = 0u;
        rgStarted[k] = FALSE;
        rgEnded[k] = FALSE;
        rgReturned[k] = FALSE;
    }
    /* The recording spans the full round-trip (initial -> target -> initial). rgEnd is the TARGET
       plateau, found at the caption's turning point (the frame whose caption luma is farthest from the
       start plateau), NOT the last frame (which has restored to the initial shade -> zero span). With
       rgStart = initial and rgEnd = target, each surface's normalized progress runs 0 -> 100 -> 0 across
       the two legs, so the per-frame band check (below) tracks the caption over BOTH legs. */
    {
        int iBestDev;

        iBestDev = -1;
        iLast = iBase;
        for (i = iBase; i < pCap->cCaptured; ++i)
        {
            int iL;
            int iDevC;

            iL = ThemeTestSurfaceLuma(pCap, i, 0u, ptRef, rgSurf, rgMenu, cMenu);
            iDevC = iL - rgStart[0];
            if (0 > iDevC) { iDevC = -iDevC; }
            if (iDevC > iBestDev) { iBestDev = iDevC; iLast = i; }
        }
    }
    for (k = 0u; k < 7u; ++k)
    {
        rgEnd[k] = ThemeTestSurfaceLuma(pCap, iLast, k, ptRef, rgSurf, rgMenu, cMenu);
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
    for (i = iBase; i < pCap->cCaptured; ++i)   /* span BOTH legs: forward (0->100) and restore (100->0) */
    {
        int  iRefProg;
        int  rgRefWin[9];   /* caption progress over [i-W, i+W], W = pCap->uPhaseFrames (<= 4) */
        int  nWin;
        int  jj;
        int  iW;

        /* The owner-drawn surfaces are painted on the app's wall-clock timer while DWM composites the
           caption separately, so a small phase offset is inherent. It is bounded in REAL time (~one
           composition), so its size in FRAMES scales with refresh -- hence the +/-W window (W from the
           refresh rate), not a fixed +/-1. The band measures each surface against the CLOSEST caption
           frame in that window: the curve-match is still strictly enforced, only the unavoidable
           compositing phase is forgiven, never a larger lead/lag. */
        iW = (int)pCap->uPhaseFrames;
        nWin = 0;
        for (jj = (int)i - iW; jj <= (int)i + iW; ++jj)
        {
            UINT jc;
            jc = (jj < (int)iBase) ? iBase : (((UINT)jj >= pCap->cCaptured) ? (pCap->cCaptured - 1u) : (UINT)jj);
            rgRefWin[nWin++] = ThemeTestProgress(ThemeTestSurfaceLuma(pCap, jc, 0u, ptRef, rgSurf, rgMenu, cMenu), rgStart[0], rgEnd[0]);
        }
        iRefProg = ThemeTestProgress(ThemeTestSurfaceLuma(pCap, i, 0u, ptRef, rgSurf, rgMenu, cMenu), rgStart[0], rgEnd[0]);
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
            /* Restore leg: after reaching the target (>=85%) the surface falls back; record when it
               returns to the initial shade (<=15%). The sync check then verifies every surface not only
               starts/finishes the forward leg together but also completes the restore leg together. */
            if (rgEnded[k] && !rgReturned[k] && (15 >= iProg))
            {
                rgReturned[k] = TRUE;
                rgReturnFrame[k] = i;
            }
            {
                int w;
                int dd;

                /* Deviation = distance to the CLOSEST caption frame in the +/-W phase window. */
                iDev = 1000;
                for (w = 0; w < nWin; ++w)
                {
                    dd = iProg - rgRefWin[w];
                    if (0 > dd) { dd = -dd; }
                    if (dd < iDev) { iDev = dd; }
                }
                /* Per-surface worst deviation (both legs), so each surface gets its own test verdict. */
                if ((UINT)iDev > pCap->rgSurfBand[k])
                {
                    pCap->rgSurfBand[k] = (UINT)iDev;
                }
                /* Per-leg tuning diagnostics for the two key owner-painted surfaces. */
                if (3u == k) { if (i <= iLast) { if (iDev > dbgCliFwd)  { dbgCliFwd  = iDev; } } else if (iDev > dbgCliRst)  { dbgCliRst  = iDev; } }
                if (2u == k) { if (i <= iLast) { if (iDev > dbgMenuFwd) { dbgMenuFwd = iDev; } } else if (iDev > dbgMenuRst) { dbgMenuRst = iDev; } }
                /* The OK button (k==6) is a push button whose face is cross-faded by uxtheme's own
                   internal state animation -- its own clock -- so it is excluded from the AGGREGATE
                   tight band (its per-surface verdict below uses a wider tolerance). */
                if ((6u != k) && (iDev > iMaxBand))
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
    fAllReturned = TRUE;
    uMinStart = 0xFFFFFFFFu;
    uMaxStart = 0u;
    uMinEnd = 0xFFFFFFFFu;
    uMaxEnd = 0u;
    uMinReturn = 0xFFFFFFFFu;
    uMaxReturn = 0u;
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
        if (!rgReturned[k])
        {
            fAllReturned = FALSE;
        }
        else
        {
            if (rgReturnFrame[k] < uMinReturn) { uMinReturn = rgReturnFrame[k]; }
            if (rgReturnFrame[k] > uMaxReturn) { uMaxReturn = rgReturnFrame[k]; }
        }
    }

    /* pfNoFlicker carries the curve-independent duration/sync verdict; pfCoherent carries the
       curve-dependent caption-banded verdict. Both legs must sync: surfaces begin the forward leg
       together, finish it together, and complete the restore leg together. */
    *pfNoFlicker = fAllStarted && fAllEnded && fAllReturned &&
                   ((uMaxStart - uMinStart) <= THEME_SYNC_FRAMES) &&
                   ((uMaxEnd - uMinEnd) <= THEME_SYNC_FRAMES) &&
                   ((uMaxReturn - uMinReturn) <= THEME_SYNC_FRAMES);
    *pfCoherent = (iMaxBand <= THEME_BAND_TOL);

    pCap->iFirstMixedFrame = uBandFrame;
    pCap->uMixedMask = (UINT)iMaxBand | ((UINT)(iBandSurf + 1) << 16);
    pCap->iLastMixedFrame = (UINT)(fAllStarted ? (uMaxStart - uMinStart) : 999u);
    pCap->uIntermediateMask = (UINT)(fAllEnded ? (uMaxEnd - uMinEnd) : 999u);
    pCap->iFirstTargetFrame = (UINT)(fAllStarted ? uMinStart : 0u);
    pCap->iFirstIntermediateFrame = (UINT)(fAllEnded ? uMaxEnd : 0u);

    OutF(TEXT("[INFO] T6 dbg client fwd=%d\n"), dbgCliFwd);
    OutF(TEXT("[INFO] T6 dbg client rst=%d\n"), dbgCliRst);
    OutF(TEXT("[INFO] T6 dbg menu fwd=%d\n"),   dbgMenuFwd);
    OutF(TEXT("[INFO] T6 dbg menu rst=%d\n"),   dbgMenuRst);

    /* Per-surface verdicts: each surface must have moved on the forward leg AND returned on the restore
       leg. Exposed so the caller emits an individual test line per surface (menu, dialog text, button,
       ...) -- "is there a test for this surface" is then answerable by name, not just an aggregate. */
    for (k = 0u; k < 7u; ++k)
    {
        pCap->rgSurfStarted[k]  = rgStarted[k];
        pCap->rgSurfReturned[k] = rgReturned[k];
    }

    if (0u < uBandFrame)
    {
        UINT bf = uBandFrame - 1u;
        OutF(TEXT("[BAND] frame=%d\n"), (int)uBandFrame);
        OutF(TEXT("[BAND] refprog=%d\n"), ThemeTestProgress(ThemeTestSurfaceLuma(pCap, bf, 0u, ptRef, rgSurf, rgMenu, cMenu), rgStart[0], rgEnd[0]));
        for (k = 0u; k < 7u; ++k)
        {
            OutF(TEXT("[BAND] prog=%d\n"),
                 ThemeTestProgress(ThemeTestSurfaceLuma(pCap, bf, k, ptRef, rgSurf, rgMenu, cMenu), rgStart[k], rgEnd[k]));
        }
        OutF(TEXT("[BAND] menustart=%d\n"), rgStart[2]);
        OutF(TEXT("[BAND] menuend=%d\n"), rgEnd[2]);
        OutF(TEXT("[BAND] menuluma=%d\n"), ThemeTestSurfaceLuma(pCap, bf, 2u, ptRef, rgSurf, rgMenu, cMenu));
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
    int     iBest;       /* child finder: shortest window-text length kept so far */
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
        /* Keep the matching child with the SHORTEST non-empty window text. For the About box's "Static"
           children that skips the icon (no text -> cannot cross-fade) and prefers the shorter label
           ("Copyright (c) 2026" over the longer version string), whose left-aligned text leaves the most
           right-edge background for a clean dark->light sample. Enumerate all, do not stop early. */
        int iLen = GetWindowTextLength(hwnd);
        if ((0 < iLen) && (!pf->hwndFound || (iLen < pf->iBest)))
        {
            pf->hwndFound = hwnd;
            pf->iBest = iLen;
        }
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
static BOOL ThemeTestLaunchWindowsProject(HWND* phwndTop, HWND* phwndDialog, HWND* phwndStatic, HWND* phwndButton,
                                          BOOL fInteractive)
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
    /* Launch the app small via STARTUPINFO: it honors STARTF_USESIZE in CalculateWindowStartupPosition
       (falling back to 3/4 work area otherwise), so a smaller window means a smaller capture region and
       faster decode for the single straight recording, while staying large enough that the main client
       is not occluded by the centered modal About dialog. */
    si.dwFlags = STARTF_USESIZE;
    si.dwXSize = THEME_APP_WIDTH;
    si.dwYSize = THEME_APP_HEIGHT;
    if (!fInteractive)
    {
        /* T7: park the app in the top-left corner. Its custom startup show (ShowWindowEx -> SWP_SHOWWINDOW)
           always activates, which we cannot suppress; parking it left of the Settings combo (which sits
           right-of-center) at least guarantees it never OCCLUDES the combo, so the input-safety gate reads
           the real Settings window at the combo point and foreground can be handed back to Settings. */
        si.dwFlags |= STARTF_USEPOSITION;
        si.dwX = 0;
        si.dwY = 0;
    }
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
    if (!fInteractive)
    {
        /* T7 (real-input Settings path): do NOT open the modal About dialog and do NOT foreground the
           app. The canonical user path has the app INACTIVE while the Settings window holds the
           foreground. Foregrounding the app here would (a) be a focus change the user never made and
           (b) set a foreground lock that prevents the about-to-launch Settings from coming to the
           foreground -- which then trips the input-safety gate and falsely reports interrupted. Leave the
           main window where it lands; Settings becomes foreground on its own when launched next. */
        return TRUE;
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

/* ----- Cross-process window-message write-ahead log (see msgwal.h / msgwal_hook.c) ------------------ */

/* Names for the messages that matter to the theme/activation/paint sequence; everything else logs by
   hex id alone. ANSI: the log is written as bytes so it reads cleanly regardless of the UNICODE build. */
static LPCSTR MsgWalMsgNameA(UINT message)
{
    switch (message)
    {
        case WM_ACTIVATE:                 return "WM_ACTIVATE";
        case WM_SETFOCUS:                 return "WM_SETFOCUS";
        case WM_KILLFOCUS:                return "WM_KILLFOCUS";
        case WM_PAINT:                    return "WM_PAINT";
        case WM_ERASEBKGND:               return "WM_ERASEBKGND";
        case WM_SYSCOLORCHANGE:           return "WM_SYSCOLORCHANGE";
        case WM_SETTINGCHANGE:            return "WM_SETTINGCHANGE";
        case WM_ACTIVATEAPP:              return "WM_ACTIVATEAPP";
        case WM_MOUSEACTIVATE:            return "WM_MOUSEACTIVATE";
        case WM_NCCALCSIZE:               return "WM_NCCALCSIZE";
        case WM_NCHITTEST:                return "WM_NCHITTEST";
        case WM_NCPAINT:                  return "WM_NCPAINT";
        case WM_NCACTIVATE:               return "WM_NCACTIVATE";
        case WM_WINDOWPOSCHANGING:        return "WM_WINDOWPOSCHANGING";
        case WM_WINDOWPOSCHANGED:         return "WM_WINDOWPOSCHANGED";
        case WM_CTLCOLORMSGBOX:           return "WM_CTLCOLORMSGBOX";
        case WM_CTLCOLOREDIT:             return "WM_CTLCOLOREDIT";
        case WM_CTLCOLORBTN:              return "WM_CTLCOLORBTN";
        case WM_CTLCOLORDLG:              return "WM_CTLCOLORDLG";
        case WM_CTLCOLORSTATIC:           return "WM_CTLCOLORSTATIC";
        case WM_TIMER:                    return "WM_TIMER";
        case WM_COMMAND:                  return "WM_COMMAND";
        case WM_SYSCOMMAND:               return "WM_SYSCOMMAND";
        case 0x031A /*WM_THEMECHANGED*/:  return "WM_THEMECHANGED";
        case 0x0320 /*WM_DWMCOLORIZATIONCOLORCHANGED*/: return "WM_DWMCOLORIZATIONCOLORCHANGED";
        case 0x0090:                      return "WM_UAHINITMENU";
        case 0x0091:                      return "WM_UAHDRAWMENU";
        case 0x0092:                      return "WM_UAHDRAWMENUITEM";
        case 0x0093:                      return "WM_UAHMEASUREMENUITEM";
        case 0x0094:                      return "WM_UAHNCPAINTMENUPOPUP";
        default:                          return "WM_?";
    }
}

static void MsgWalWriteLine(const MSGWAL_REC* pRec, LONGLONG qpc0, LONGLONG qpcFreq)
{
    CHAR  szLine[256];
    int   cch;
    DWORD cbWritten;
    LONG  lMs;

    if (!g_hMsgWalFile || (0 == qpcFreq))
    {
        return;
    }
    lMs = (LONG)(((pRec->qpc - qpc0) * 1000) / qpcFreq);
    cch = wnsprintfA(szLine, (int)ARRAYSIZE(szLine),
                     "+%5ldms %-4hs h=%08lX%08lX %-22hs 0x%04X w=%08lX%08lX l=%08lX%08lX %hs\r\n",
                     lMs,
                     (MSGWAL_SRC_GET == pRec->dwSource) ? "GET" : "SENT",
                     (DWORD)(pRec->hwnd >> 32), (DWORD)pRec->hwnd,
                     MsgWalMsgNameA(pRec->message), pRec->message,
                     (DWORD)(pRec->wParam >> 32), (DWORD)pRec->wParam,
                     (DWORD)(pRec->lParam >> 32), (DWORD)pRec->lParam,
                     pRec->szText);
    if (cch > 0)
    {
        WriteFile(g_hMsgWalFile, szLine, (DWORD)cch, &cbWritten, NULL);
    }
}

/* Single consumer: drain every committed-but-unread record. qpc0 is latched off the first record so the
   timeline reads as +ms from the first logged message. */
static void MsgWalDrain(LONGLONG* pQpc0, BOOL* pfHaveQpc0)
{
    MSGWAL_HDR* pHdr;
    LONGLONG    w;

    pHdr = g_pMsgWalHdr;
    if (!pHdr)
    {
        return;
    }
    for (;;)
    {
        w = pHdr->writeIdx;
        MemoryBarrier();
        if (pHdr->readIdx >= w)
        {
            break;
        }
        {
            MSGWAL_REC* pRec = &MSGWAL_RECS(pHdr)[pHdr->readIdx & (pHdr->capacity - 1)];
            if (!*pfHaveQpc0)
            {
                *pQpc0 = pRec->qpc;
                *pfHaveQpc0 = TRUE;
            }
            MsgWalWriteLine(pRec, *pQpc0, pHdr->qpcFreq);
        }
        ++pHdr->readIdx;
    }
}

static DWORD WINAPI MsgWalLoggerThread(LPVOID pv)
{
    LONGLONG qpc0;
    BOOL     fHaveQpc0;
    BOOL     fStop;

    UNREFERENCED_PARAMETER(pv);
    qpc0 = 0;
    fHaveQpc0 = FALSE;
    for (;;)
    {
        fStop = (0 != InterlockedCompareExchange(&g_lMsgWalStop, 0, 0));
        MsgWalDrain(&qpc0, &fHaveQpc0);
        if (fStop)
        {
            MsgWalDrain(&qpc0, &fHaveQpc0);   /* final sweep after the producer has stopped */
            break;
        }
        Sleep(2u);
    }
    return 0u;
}

/* Stand up the shared ring, the log file, and the two hooks scoped to the app's GUI thread, then start
   the logger. The hook DLL is loaded from Tests.exe's own directory (SetWindowsHookEx hands that path to
   the OS to inject into the app). */
static BOOL MsgWalStart(HWND hwndTop, LPCTSTR pszLogName)
{
    TCHAR         szDir[MAX_PATH];
    TCHAR         szDll[MAX_PATH];
    DWORD         dwPid;
    DWORD         dwTid;
    LARGE_INTEGER freq;
    HOOKPROC      pfnGet;
    HOOKPROC      pfnCwp;

    dwTid = GetWindowThreadProcessId(hwndTop, &dwPid);
    if (0u == dwTid)
    {
        return FALSE;
    }
    g_hMsgWalMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                     (DWORD)((ULONGLONG)MSGWAL_TOTAL_BYTES >> 32),
                                     (DWORD)((ULONGLONG)MSGWAL_TOTAL_BYTES & 0xFFFFFFFFu),
                                     MSGWAL_MAPPING_NAME);
    if (!g_hMsgWalMap)
    {
        return FALSE;
    }
    g_pMsgWalHdr = (MSGWAL_HDR*)MapViewOfFile(g_hMsgWalMap, FILE_MAP_WRITE, 0, 0, MSGWAL_TOTAL_BYTES);
    if (!g_pMsgWalHdr)
    {
        return FALSE;
    }
    SecureZeroMemory(g_pMsgWalHdr, sizeof(MSGWAL_HDR));
    QueryPerformanceFrequency(&freq);
    g_pMsgWalHdr->qpcFreq        = freq.QuadPart;
    g_pMsgWalHdr->capacity       = MSGWAL_CAPACITY;
    g_pMsgWalHdr->recSize        = (DWORD)sizeof(MSGWAL_REC);
    g_pMsgWalHdr->targetThreadId = dwTid;

    g_hMsgWalFile = CreateFile(pszLogName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == g_hMsgWalFile)
    {
        g_hMsgWalFile = NULL;
    }

    GetModuleFileName(NULL, szDir, (DWORD)ARRAYSIZE(szDir));
    PathRemoveFileSpec(szDir);
    wnsprintf(szDll, (int)ARRAYSIZE(szDll), TEXT("%s\\MsgWalHook.dll"), szDir);
    g_hMsgWalDll = LoadLibrary(szDll);
    if (!g_hMsgWalDll)
    {
        return FALSE;
    }
    pfnGet = (HOOKPROC)GetProcAddress(g_hMsgWalDll, "MsgWalGetMsgProc");
    pfnCwp = (HOOKPROC)GetProcAddress(g_hMsgWalDll, "MsgWalCallWndProc");
    if (!pfnGet || !pfnCwp)
    {
        return FALSE;
    }
    g_hMsgWalHookGet = SetWindowsHookEx(WH_GETMESSAGE, pfnGet, g_hMsgWalDll, dwTid);
    g_hMsgWalHookCwp = SetWindowsHookEx(WH_CALLWNDPROC, pfnCwp, g_hMsgWalDll, dwTid);

    g_lMsgWalStop = 0;
    g_hMsgWalLogThread = CreateThread(NULL, 0, MsgWalLoggerThread, NULL, 0, NULL);
    return (NULL != g_hMsgWalHookGet) && (NULL != g_hMsgWalHookCwp);
}

static void MsgWalStop(void)
{
    if (g_hMsgWalHookGet)
    {
        UnhookWindowsHookEx(g_hMsgWalHookGet);
        g_hMsgWalHookGet = NULL;
    }
    if (g_hMsgWalHookCwp)
    {
        UnhookWindowsHookEx(g_hMsgWalHookCwp);
        g_hMsgWalHookCwp = NULL;
    }
    /* Let any hook calls already in flight on the app GUI thread complete before the producer is declared
       stopped, so the final sweep sees every committed record. */
    Sleep(50u);
    InterlockedExchange(&g_lMsgWalStop, 1);
    if (g_hMsgWalLogThread)
    {
        WaitForSingleObject(g_hMsgWalLogThread, WAIT_MS);
        CloseHandle(g_hMsgWalLogThread);
        g_hMsgWalLogThread = NULL;
    }
    if (g_hMsgWalFile)
    {
        CloseHandle(g_hMsgWalFile);
        g_hMsgWalFile = NULL;
    }
    if (g_pMsgWalHdr)
    {
        OutF(TEXT("[INFO] T6 msgwal records: %d\n"), (int)g_pMsgWalHdr->writeIdx);
        OutF(TEXT("[INFO] T6 msgwal dropped: %d\n"), (int)g_pMsgWalHdr->dropped);
        UnmapViewOfFile(g_pMsgWalHdr);
        g_pMsgWalHdr = NULL;
    }
    if (g_hMsgWalMap)
    {
        CloseHandle(g_hMsgWalMap);
        g_hMsgWalMap = NULL;
    }
    if (g_hMsgWalDll)
    {
        FreeLibrary(g_hMsgWalDll);
        g_hMsgWalDll = NULL;
    }
}

/* Tagged result emitters: prepend the active scenario tag (e.g. "[apps] ") to the result name so the
   apps-only and apps+system runs of the same transition body produce distinct, greppable lines. */
/* DECLSPEC_NOINLINE: each carries a 192-TCHAR scratch buffer. Under the tests' LTCG, inlining them
   into ThemeTestRunTransition (which calls ThemeTestCheck ~20 times) would stack a fresh buffer per
   call site and blow the frame past the one-page stack-probe threshold (__chkstk, unresolvable in this
   /NODEFAULTLIB build). Kept out-of-line so the buffer lives once, in their own frame. */
static DECLSPEC_NOINLINE void ThemeTestCheck(BOOL fOk, LPCTSTR pszName)
{
    TCHAR szBuf[192];
    wnsprintf(szBuf, ARRAYSIZE(szBuf), TEXT("%s%s"), g_pszThemeTag, pszName);
    Check(fOk, szBuf);
}

static DECLSPEC_NOINLINE void ThemeTestSkipNote(LPCTSTR pszName, LPCTSTR pszWhy)
{
    TCHAR szBuf[192];
    wnsprintf(szBuf, ARRAYSIZE(szBuf), TEXT("%s%s"), g_pszThemeTag, pszName);
    Skip(szBuf, pszWhy);
}

/* Distinct from pass/fail/skip: the user took over their own machine mid-run (closed a window, switched
   focus away from Settings). The test ends immediately and reports [INTR] -- not a failure -- because it
   could not (and must not) keep driving input. */
static DECLSPEC_NOINLINE void ThemeTestInterruptNote(LPCTSTR pszName, LPCTSTR pszWhy)
{
    Out(TEXT("[INTR] "));
    Out(g_pszThemeTag);
    Out(pszName);
    Out(TEXT(" -- "));
    Out(pszWhy);
    Out(TEXT("\n"));
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
    HANDLE            hReduceThread;
    HANDLE            hReduceDone;
    THEME_REDUCE_RUN  reduceRun;
    BOOL              fReduced;
    HANDLE            hMutex;
    DWORD             dwWait;
    DWORD             dwGuiThreadId;
    UINT              cCapturedFrames;
    UINT              cExpectedFrames;
    RECT              rcCapture;
    RECT              rcTopCreated;
    /* capture/dxgi are large (frame textures, the per-surface color series, the compute pipeline) and
       are shared by the capture/encode/reduce threads by pointer. Holding them in file-static storage
       (the T6 mutex serializes the whole run, so there is only ever one live instance) keeps
       ThemeTestRunTransition's stack frame under the one-page stack-probe threshold -- a larger frame
       would emit __chkstk, unresolvable in this /NODEFAULTLIB build. */
    static THEME_CAPTURE capture;
    static THEME_DXGI    dxgi;
    THEME_CAPTURE_RUN captureRun;
    THEME_ENCODE_RUN  encodeRun;
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
    hReduceThread = NULL;
    hReduceDone = NULL;
    fReduced = FALSE;
    SecureZeroMemory(&rcTopCreated, sizeof(rcTopCreated));

    /* Launch the real, theme-integrated WindowsProject.exe and drive the transition on IT. The Win32X
       state machine lives in the app's own process and its WndProc themes itself; the test only flips
       the Personalize DWORD(s) and broadcasts -- exactly a Settings theme switch. */
    (void)ThemeTestLaunchWindowsProject(&hwndTop, &hwndDialog, &hwndStatic, &hwndButton, TRUE);

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

    /* Install the cross-process message WAL on the app GUI thread before the capture thread fires the
       theme flip, so the full message sequence around the transition (WM_SETTINGCHANGE, WM_NCACTIVATE,
       WM_TIMER ticks, the UAH menu paints) is logged to T6_msgwal.log in dispatch order. */
    (void)MsgWalStart(hwndTop, TEXT("T6_msgwal.log"));

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
    g_dwThemeWorkerRestore = dwOriginalValue;
    g_fThemeWorkerHadValue = fHadOriginalValue;
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
        /* Stand up the GPU reduction alongside the encoder: fix the surface sample patches, init the
           compute pipeline, and spawn the reduce thread. It drains analysis-blit frames as they go
           ready (off capture.hReduceReady) into the per-surface color time series -- concurrent with,
           and on independent textures from, the encoder, so neither stalls the other. */
        capture.hReduceReady = CreateEvent(NULL, FALSE, FALSE, NULL);
        hReduceDone = CreateEvent(NULL, TRUE, FALSE, NULL);
        ThemeTestFillSurfacePoints(hwndTop, hwndDialog, hwndStatic, hwndButton, &capture);
        if (ThemeTestComputeInit(&dxgi, &capture) && capture.hReduceReady && hReduceDone)
        {
            SecureZeroMemory(&reduceRun, sizeof(reduceRun));
            reduceRun.pDxgi = &dxgi;
            reduceRun.pCap = &capture;
            reduceRun.hDone = hReduceDone;
            hReduceThread = CreateThread(NULL, 0, ThemeTestReduceThread, &reduceRun, 0, NULL);
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
        if (capture.hReduceReady)
        {
            SetEvent(capture.hReduceReady);   /* wake the reducer to drain the tail and finish */
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
        if (hReduceThread)
        {
            if (ThemeTestPumpUntilDone(hReduceDone, THEME_RECORD_MS + WAIT_MS))
            {
                fReduced = reduceRun.fOk;
            }
            WaitForSingleObject(hReduceThread, WAIT_MS);
            CloseHandle(hReduceThread);
            hReduceThread = NULL;
        }
        if (hReduceDone)
        {
            CloseHandle(hReduceDone);
            hReduceDone = NULL;
        }
        if (capture.hReduceReady)
        {
            CloseHandle(capture.hReduceReady);
            capture.hReduceReady = NULL;
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
        /* Caption-band phase window scaled to the monitor's refresh: the app paints owner-drawn surfaces
           on its wall-clock timer while DWM composites the caption separately, an offset bounded in REAL
           time (~one composition), so its size in FRAMES grows with refresh. A fixed +/-1 frame is right
           at 60Hz but far too strict at 180Hz (the same wall-clock phase spans ~3 frames). Scale it:
           round(hz/60), clamped to [1,4]. The curve match within the window stays strictly enforced. */
        {
            UINT uHz;
            uHz = dxgi.uRefreshDenominator ? (dxgi.uRefreshNumerator / dxgi.uRefreshDenominator) : 60u;
            capture.uPhaseFrames = (uHz + 30u) / 60u;
            if (capture.uPhaseFrames < 1u) { capture.uPhaseFrames = 1u; }
            if (capture.uPhaseFrames > 4u) { capture.uPhaseFrames = 4u; }
        }
        /* Analysis runs off the GPU-reduced per-surface color time series (fReduced), NOT a CPU decode
           of the MP4 -- the MP4 is still written for reference, just no longer the analysis source. */
        fAnalyzedFrames = fCapturedFrames &&
                          fReduced &&
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
        ThemeTestPumpForMs(THEME_SETTLE_MS);
    }

    /* Tear down the message WAL (unhook, drain, flush the log) before killing the app. */
    MsgWalStop();

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
    /* Per-surface verdicts (both legs: forward fade AND restore). Each surface that the theme engine
       paints must cross-fade within the caption band and complete both legs. The OK button rides
       uxtheme's own button-state clock, so it gets a wider band but must still start and return. An
       absent surface (no dialog) passes vacuously. These make the menu/dialog-text/client/button each
       individually testable by name rather than hidden inside the aggregate band verdict. */
    if (fAnalyzedFrames)
    {
        ThemeTestCheck(!capture.rgSurfActive[2] ||
            ((capture.rgSurfBand[2] <= THEME_BAND_TOL) && capture.rgSurfStarted[2] && capture.rgSurfReturned[2]),
            TEXT("T6 menu bar cross-fades in the caption band over both legs"));
        ThemeTestCheck(!capture.rgSurfActive[5] ||
            ((capture.rgSurfBand[5] <= THEME_BAND_TOL) && capture.rgSurfStarted[5] && capture.rgSurfReturned[5]),
            TEXT("T6 dialog static text cross-fades in the caption band over both legs"));
        ThemeTestCheck(!capture.rgSurfActive[4] ||
            ((capture.rgSurfBand[4] <= THEME_BAND_TOL) && capture.rgSurfStarted[4] && capture.rgSurfReturned[4]),
            TEXT("T6 dialog client cross-fades in the caption band over both legs"));
        ThemeTestCheck(!capture.rgSurfActive[1] ||
            ((capture.rgSurfBand[1] <= THEME_BAND_TOL) && capture.rgSurfStarted[1] && capture.rgSurfReturned[1]),
            TEXT("T6 dialog caption cross-fades in the caption band over both legs"));
        ThemeTestCheck(!capture.rgSurfActive[3] ||
            ((capture.rgSurfBand[3] <= THEME_BAND_TOL) && capture.rgSurfStarted[3] && capture.rgSurfReturned[3]),
            TEXT("T6 main client cross-fades in the caption band over both legs"));
        ThemeTestCheck(!capture.rgSurfActive[6] ||
            ((capture.rgSurfBand[6] <= 25u) && capture.rgSurfStarted[6] && capture.rgSurfReturned[6]),
            TEXT("T6 OK button tracks the transition over both legs (own clock, wide band)"));
        OutF(TEXT("[INFO] T6 surf band menu=%u\n"),       capture.rgSurfBand[2]);
        OutF(TEXT("[INFO] T6 surf band dlgstatic=%u\n"),  capture.rgSurfBand[5]);
        OutF(TEXT("[INFO] T6 surf band dlgclient=%u\n"),  capture.rgSurfBand[4]);
        OutF(TEXT("[INFO] T6 surf band dlgcaption=%u\n"), capture.rgSurfBand[1]);
        OutF(TEXT("[INFO] T6 surf band mainclient=%u\n"), capture.rgSurfBand[3]);
        OutF(TEXT("[INFO] T6 surf band button=%u\n"),     capture.rgSurfBand[6]);
    }
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
    ThemeTestComputeFree(&dxgi);
    ThemeTestDxgiFree(&dxgi);
    ThemeTestCaptureFree(&capture);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
}

/* One straight recording of the real-world switch: flip BOTH AppsUseLightTheme and SystemUsesLightTheme
   (a full Settings theme change -- DWM additionally runs its own caption crossfade, the hardest surface
   to keep in band), capture/analyze the single transition, and restore both DWORDs. The app-only
   variant (AppsUseLightTheme alone, no system caption crossfade) is a strict subset of this path and is
   dropped so T6 runs a single ~3.5s recording rather than two spaced runs. */
static void T_ThemeTransition(void)
{
    ThemeTestRunTransition(TRUE, TEXT("[apps+system] "));
}

/* ----- T7 real-input Settings driver: UIA to focus, SendInput keystrokes to actuate ---------------- */

/* One synthesized key transition through the real input pipeline (SendInput): not a posted message, so
   it drives the genuine WinUI input -> SelectionChanged -> apply path a literal click takes -- which is
   why UIA SelectionItem.Select gave different results than a real click. */
static void ThemeTestSendVk(WORD wVk, BOOL fUp)
{
    INPUT in;
    SecureZeroMemory(&in, sizeof(in));
    in.type       = INPUT_KEYBOARD;
    in.ki.wVk     = wVk;
    in.ki.dwFlags = fUp ? KEYEVENTF_KEYUP : 0u;
    SendInput(1u, &in, (int)sizeof(INPUT));
}

static void ThemeTestKeyPress(WORD wVk)
{
    ThemeTestSendVk(wVk, FALSE);
    Sleep(30u);
    ThemeTestSendVk(wVk, TRUE);
    Sleep(30u);
}

/* Find the Settings "Choose your mode" combo by its stable AutomationId (caller releases). */
static IUIAutomationElement* UiaFindColorModeCombo(IUIAutomation* pAuto)
{
    IUIAutomationElement*   pRoot;
    IUIAutomationElement*   pEl;
    IUIAutomationCondition* pCond;
    VARIANT                 v;

    if (!pAuto)
    {
        return NULL;
    }
    pRoot = NULL;
    if (FAILED(IUIAutomation_GetRootElement(pAuto, &pRoot)) || !pRoot)
    {
        return NULL;
    }
    pEl   = NULL;
    pCond = NULL;
    VariantInit(&v);
    v.vt      = VT_BSTR;
    v.bstrVal = SysAllocString(L"SystemSettings_Personalize_Color_ColorMode_ComboBox");
    if (SUCCEEDED(IUIAutomation_CreatePropertyCondition(pAuto, UIA_AutomationIdPropertyId, v, &pCond)) && pCond)
    {
        (void)IUIAutomationElement_FindFirst(pRoot, TreeScope_Descendants, pCond, &pEl);
        IUIAutomationCondition_Release(pCond);
    }
    VariantClear(&v);
    IUIAutomationElement_Release(pRoot);
    return pEl;
}

/* The combo's currently selected item name -> mode index (Light=0, Dark=1, Custom=2; -1 unknown). */
static int UiaColorModeSelectedIndex(IUIAutomation* pAuto)
{
    IUIAutomationElement*          pCombo;
    IUIAutomationSelectionPattern* pSel;
    IUIAutomationElementArray*     pArr;
    IUIAutomationElement*          pItem;
    BSTR                           bsName;
    int                            cItems;
    int                            idx;

    idx    = -1;
    pCombo = UiaFindColorModeCombo(pAuto);
    if (!pCombo)
    {
        return -1;
    }
    pSel = NULL;
    if (SUCCEEDED(IUIAutomationElement_GetCurrentPatternAs(pCombo, UIA_SelectionPatternId,
                                                           &IID_IUIAutomationSelectionPattern, (void**)&pSel)) && pSel)
    {
        pArr = NULL;
        if (SUCCEEDED(IUIAutomationSelectionPattern_GetCurrentSelection(pSel, &pArr)) && pArr)
        {
            cItems = 0;
            (void)IUIAutomationElementArray_get_Length(pArr, &cItems);
            pItem = NULL;
            if ((cItems > 0) && SUCCEEDED(IUIAutomationElementArray_GetElement(pArr, 0, &pItem)) && pItem)
            {
                bsName = NULL;
                if (SUCCEEDED(IUIAutomationElement_get_CurrentName(pItem, &bsName)) && bsName)
                {
                    if (0 == lstrcmpiW(bsName, L"Light"))       idx = 0;
                    else if (0 == lstrcmpiW(bsName, L"Dark"))   idx = 1;
                    else if (0 == lstrcmpiW(bsName, L"Custom")) idx = 2;
                    SysFreeString(bsName);
                }
                IUIAutomationElement_Release(pItem);
            }
            IUIAutomationElementArray_Release(pArr);
        }
        IUIAutomationSelectionPattern_Release(pSel);
    }
    IUIAutomationElement_Release(pCombo);
    return idx;
}

/* Select a mode by index via REAL input: UIA only to put keyboard focus on the combo (no cursor move),
   then synthesized Alt+Down (open) -> Home (top item) -> Down x index -> Enter (commit). */
/* TRUE only when the window physically at the combo's on-screen center is the same top-level as the
   foreground window -- i.e. the Settings window hosting the combo is foreground AND the combo is not
   occluded. This is the gate for ALL synthesized input: the user's machine, cursor, and keyboard are
   theirs, so if they closed Settings, switched away, or covered it, we must not SetFocus (steals focus)
   or SendInput (lands keystrokes in their window). Identity is by window-at-point, not by class/title:
   Settings is a UWP ApplicationFrameWindow whose title/host matching is unreliable. No cursor is moved --
   WindowFromPoint is a pure query. */
static BOOL ThemeTestComboHostIsForeground(IUIAutomationElement* pCombo)
{
    RECT  rc;
    POINT pt;
    HWND  hPt;
    HWND  hRootAtPt;
    HWND  hFg;

    if (!pCombo)
    {
        return FALSE;
    }
    SecureZeroMemory(&rc, sizeof(rc));
    if (FAILED(IUIAutomationElement_get_CurrentBoundingRectangle(pCombo, &rc)))
    {
        return FALSE;
    }
    if ((rc.right <= rc.left) || (rc.bottom <= rc.top))
    {
        return FALSE;   /* zero/offscreen rect -> not visible */
    }
    pt.x = (rc.left + rc.right) / 2;
    pt.y = (rc.top + rc.bottom) / 2;
    hPt = WindowFromPoint(pt);
    if (!hPt)
    {
        return FALSE;
    }
    hRootAtPt = GetAncestor(hPt, GA_ROOT);
    hFg       = GetForegroundWindow();
    return (NULL != hRootAtPt) && (NULL != hFg) && (hRootAtPt == GetAncestor(hFg, GA_ROOT));
}

/* Diagnostic: dump the input-gate state (combo rect, foreground window class, window-at-combo-point
   class, root match) so an interruption can be explained rather than guessed at. */
static DECLSPEC_NOINLINE void ThemeTestDumpInputGateDiag(IUIAutomation* pAuto)
{
    IUIAutomationElement* pCombo;
    RECT  rc;
    POINT pt;
    HWND  hFg;
    HWND  hPt;
    BOOL  fHadCombo;
    TCHAR szFg[64];
    TCHAR szPt[64];
    TCHAR szLine[256];

    SecureZeroMemory(&rc, sizeof(rc));
    pCombo = UiaFindColorModeCombo(pAuto);
    fHadCombo = (NULL != pCombo);
    if (pCombo)
    {
        (void)IUIAutomationElement_get_CurrentBoundingRectangle(pCombo, &rc);
        IUIAutomationElement_Release(pCombo);
    }
    pt.x = (rc.left + rc.right) / 2;
    pt.y = (rc.top + rc.bottom) / 2;
    hFg  = GetForegroundWindow();
    hPt  = WindowFromPoint(pt);
    szFg[0] = TEXT('\0');
    szPt[0] = TEXT('\0');
    if (hFg) { (void)GetClassName(hFg, szFg, (int)ARRAYSIZE(szFg)); }
    if (hPt) { (void)GetClassName(hPt, szPt, (int)ARRAYSIZE(szPt)); }
    wnsprintf(szLine, (int)ARRAYSIZE(szLine),
              TEXT("[INFO] T7 gate combo?=%d rc=(%d,%d,%d,%d) match=%d\n"),
              (int)fHadCombo, (int)rc.left, (int)rc.top, (int)rc.right, (int)rc.bottom,
              (int)(hFg && hPt && (GetAncestor(hFg, GA_ROOT) == GetAncestor(hPt, GA_ROOT))));
    Out(szLine);
    wnsprintf(szLine, (int)ARRAYSIZE(szLine), TEXT("[INFO] T7 gate fgCls=%s ptCls=%s\n"), szFg, szPt);
    Out(szLine);
}

/* Hand the foreground to the Settings window that hosts the combo (NOT our app). Our app's launch
   forcibly activates it; this reclaims foreground for the real target so the canonical "app inactive,
   Settings active" state holds. The combo is parked unoccluded (corner-launched app), so WindowFromPoint
   at its center yields the Settings top-level. AttachThreadInput to the current foreground thread makes
   the cross-process SetForegroundWindow take. Returns TRUE if Settings ends up foreground. */
static BOOL ThemeTestForegroundSettings(IUIAutomation* pAuto)
{
    IUIAutomationElement* pCombo;
    RECT  rc;
    POINT pt;
    HWND  hPt;
    HWND  hRoot;
    HWND  hFg;
    DWORD tidFg;
    DWORD tidMe;
    BOOL  fOk;

    fOk    = FALSE;
    pCombo = UiaFindColorModeCombo(pAuto);
    if (!pCombo)
    {
        return FALSE;
    }
    SecureZeroMemory(&rc, sizeof(rc));
    if (SUCCEEDED(IUIAutomationElement_get_CurrentBoundingRectangle(pCombo, &rc)) &&
        (rc.right > rc.left) && (rc.bottom > rc.top))
    {
        pt.x  = (rc.left + rc.right) / 2;
        pt.y  = (rc.top + rc.bottom) / 2;
        hPt   = WindowFromPoint(pt);
        hRoot = hPt ? GetAncestor(hPt, GA_ROOT) : NULL;
        if (hRoot)
        {
            hFg   = GetForegroundWindow();
            tidFg = GetWindowThreadProcessId(hFg, NULL);
            tidMe = GetCurrentThreadId();
            (void)AttachThreadInput(tidMe, tidFg, TRUE);
            (void)SetForegroundWindow(hRoot);
            (void)AttachThreadInput(tidMe, tidFg, FALSE);
            fOk = TRUE;
        }
    }
    IUIAutomationElement_Release(pCombo);
    return fOk;
}

/* Poll until the Settings window hosting the combo is genuinely foreground (and unoccluded), or time out.
   This is the precondition for driving any input. */
static BOOL ThemeTestWaitForComboForeground(IUIAutomation* pAuto, DWORD dwTimeoutMs)
{
    IUIAutomationElement* pCombo;
    BOOL                  fFg;
    DWORD                 dwWaited;

    dwWaited = 0u;
    for (;;)
    {
        pCombo = UiaFindColorModeCombo(pAuto);
        fFg    = FALSE;
        if (pCombo)
        {
            fFg = ThemeTestComboHostIsForeground(pCombo);
            IUIAutomationElement_Release(pCombo);
        }
        if (fFg)
        {
            return TRUE;
        }
        if (dwWaited >= dwTimeoutMs)
        {
            return FALSE;
        }
        Sleep(200u);
        dwWaited += 200u;
    }
}

/* Returns FALSE the instant it is unsafe to drive input (Settings not foreground / closed, or the combo
   is gone) -- the caller treats that as an interruption and ends the test. Never sends a keystroke nor
   takes focus unless Settings is genuinely foreground, and re-checks between key groups so the user
   taking over mid-toggle stops input immediately. */
static BOOL ThemeTestSettingsPickMode(IUIAutomation* pAuto, int index)
{
    IUIAutomationElement* pCombo;
    int                   k;

    if (index < 0)
    {
        return FALSE;
    }
    pCombo = UiaFindColorModeCombo(pAuto);
    if (!pCombo)
    {
        return FALSE;
    }
    if (!ThemeTestComboHostIsForeground(pCombo))
    {
        IUIAutomationElement_Release(pCombo);
        return FALSE;
    }
    (void)IUIAutomationElement_SetFocus(pCombo);
    Sleep(150u);
    if (!ThemeTestComboHostIsForeground(pCombo))
    {
        IUIAutomationElement_Release(pCombo);
        return FALSE;
    }
    ThemeTestSendVk(VK_MENU, FALSE);
    ThemeTestKeyPress(VK_DOWN);
    ThemeTestSendVk(VK_MENU, TRUE);
    Sleep(250u);
    ThemeTestKeyPress(VK_HOME);
    Sleep(100u);
    for (k = 0; k < index; ++k)
    {
        ThemeTestKeyPress(VK_DOWN);
        Sleep(80u);
    }
    if (!ThemeTestComboHostIsForeground(pCombo))
    {
        IUIAutomationElement_Release(pCombo);
        return FALSE;
    }
    ThemeTestKeyPress(VK_RETURN);
    Sleep(150u);
    IUIAutomationElement_Release(pCombo);
    return TRUE;
}

/* Poll for the Settings color-mode combo to appear after launch. */
static BOOL ThemeTestWaitForColorCombo(IUIAutomation* pAuto, DWORD dwTimeoutMs)
{
    IUIAutomationElement* pCombo;
    DWORD                 dwWaited;

    dwWaited = 0u;
    for (;;)
    {
        pCombo = UiaFindColorModeCombo(pAuto);
        if (pCombo)
        {
            IUIAutomationElement_Release(pCombo);
            return TRUE;
        }
        if (dwWaited >= dwTimeoutMs)
        {
            return FALSE;
        }
        Sleep(250u);
        dwWaited += 250u;
    }
}

/*
 * T7 -- the high-priority user path from the screen recording, driven through REAL input: launch the
 * real WindowsProject.exe (it goes inactive once Settings takes the foreground -- no focus-steal hack,
 * the mouse is never touched), open Settings to Personalization > Colors, and toggle the "Choose your
 * mode" combo light<->dark several times using SendInput keystrokes (UIA only locates + focuses the
 * combo). This goes through the genuine Settings/uxtheme/themeui/DWM path a literal click takes -- the
 * one that produced different results than the synthetic RegSetValue + broadcast. The cross-process
 * message WAL (msgwal.h) records the exact sequence the inactive app receives to T7_msgwal.log; the
 * original mode is restored through the same combo, with a registry restore as the safety net.
 */
#define THEME_SETTINGS_TOGGLES 4u

static void T_ThemeSettingsToggle(void)
{
    DWORD          dwOriginalValue;
    BOOL           fHadOriginalValue;
    BOOL           fCanUseDarkMode;
    UINT           i;
    UINT           cApplied;
    int            iOriginalMode;
    int            iNextMode;
    BOOL           fRestored;
    BOOL           fWalStarted;
    BOOL           fComboReady;
    BOOL           fSettingsForeground;
    BOOL           fInterrupted;
    HWND           hwndTop;
    HWND           hwndDialog;
    HWND           hwndStatic;
    HWND           hwndButton;
    HWND           hwndSettings;
    HRESULT        hrCo;
    BOOL           fCoInit;
    IUIAutomation* pAuto;
    HANDLE         hMutex;
    DWORD          dwWait;

    g_pszThemeTag = TEXT("[settings] ");

    hMutex = CreateMutex(NULL, FALSE, TEXT("Local\\Win32XThemeTransitionTest"));
    if (!hMutex)
    {
        ThemeTestSkipNote(TEXT("T7 settings-menu theme toggle"), TEXT("theme transition mutex cannot be created"));
        return;
    }
    dwWait = WaitForSingleObject(hMutex, WAIT_MS);
    if ((WAIT_OBJECT_0 != dwWait) && (WAIT_ABANDONED != dwWait))
    {
        CloseHandle(hMutex);
        ThemeTestCheck(FALSE, TEXT("T7 settings-menu theme toggle is not run concurrently"));
        return;
    }

    pfnTestThemeStartup();
    fCanUseDarkMode = pfnTestThemeCanUseDarkMode();
    if (!fCanUseDarkMode)
    {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        ThemeTestSkipNote(TEXT("T7 settings-menu theme toggle"), TEXT("dark-mode uxtheme/DWM contract unavailable"));
        return;
    }
    if (!ThemeTestReadAppsUseLightTheme(&dwOriginalValue, &fHadOriginalValue))
    {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        ThemeTestSkipNote(TEXT("T7 settings-menu theme toggle"), TEXT("AppsUseLightTheme cannot be read"));
        return;
    }
    if (!ThemeTestInitCommonControls())
    {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        ThemeTestSkipNote(TEXT("T7 settings-menu theme toggle"), TEXT("common controls v6 unavailable"));
        return;
    }

    hwndTop    = NULL;
    hwndDialog = NULL;
    hwndStatic = NULL;
    hwndButton = NULL;
    (void)ThemeTestLaunchWindowsProject(&hwndTop, &hwndDialog, &hwndStatic, &hwndButton, FALSE);
    if (!hwndTop)
    {
        ThemeTestCheck(FALSE, TEXT("T7 launches WindowsProject.exe (inactive, no foreground steal)"));
        (void)ThemeTestRestoreAppsUseLightTheme(fHadOriginalValue, dwOriginalValue);
        ThemeTestKillApp();
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return;
    }
    ThemeTestCheck(TRUE, TEXT("T7 launches WindowsProject.exe (inactive, no foreground steal)"));

    /* WAL on the app GUI thread BEFORE Settings takes the foreground, so the app's WM_NCACTIVATE(FALSE)
       (losing activation to Settings) and every per-toggle message land in T7_msgwal.log in order. */
    fWalStarted = MsgWalStart(hwndTop, TEXT("T7_msgwal.log"));

    hrCo    = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    fCoInit = (S_OK == hrCo);
    pAuto   = NULL;
    (void)CoCreateInstance(&CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void**)&pAuto);

    /* Open the real Settings to Personalization > Colors. It takes the foreground -> the app goes
       inactive, exactly as in the recording; the cursor is never moved. */
    (void)ShellExecute(NULL, TEXT("open"), TEXT("ms-settings:colors"), NULL, NULL, SW_SHOWNORMAL);
    fComboReady = ThemeTestWaitForColorCombo(pAuto, WAIT_MS * 4u);
    if (fComboReady)
    {
        /* Hand the foreground to Settings (the real target), undoing our app's launch activation, then
           wait until Settings genuinely holds it. Only then is it safe to drive input. */
        (void)ThemeTestForegroundSettings(pAuto);
    }
    fSettingsForeground = fComboReady && ThemeTestWaitForComboForeground(pAuto, WAIT_MS * 2u);
    ThemeTestDumpInputGateDiag(pAuto);   /* gate state after handing foreground to Settings */

    iOriginalMode = UiaColorModeSelectedIndex(pAuto);

    /* Toggle dark<->light through REAL keystrokes on the focused combo, app inactive throughout. Start
       opposite the current mode so the first toggle is an actual change. The moment the user takes their
       machine back -- closes the app window, closes Settings, or switches focus away -- detection fires,
       input stops, and the test ENDS as interrupted (it never drives input into the user's windows). */
    iNextMode    = (1 == iOriginalMode) ? 0 : 1;
    cApplied     = 0u;
    fInterrupted = !fSettingsForeground;
    if (fSettingsForeground)
    {
        for (i = 0u; i < THEME_SETTINGS_TOGGLES; ++i)
        {
            if (!IsWindow(hwndTop))                          /* user closed the app under test */
            {
                fInterrupted = TRUE;
                break;
            }
            if (!ThemeTestSettingsPickMode(pAuto, iNextMode)) /* Settings closed / not foreground */
            {
                fInterrupted = TRUE;
                break;
            }
            ++cApplied;
            Sleep(THEME_LEG_MS);                 /* let each leg play out before the next toggle */
            iNextMode = iNextMode ? 0 : 1;
        }
    }

    MsgWalStop();

    /* Restore the original mode through the same real path -- only while still safe; otherwise the
       registry safety net below restores it. */
    if (fComboReady && (iOriginalMode >= 0) && !fInterrupted)
    {
        (void)ThemeTestSettingsPickMode(pAuto, iOriginalMode);
        Sleep(THEME_SETTLE_MS);
    }
    /* Close only the Settings window we opened, and only if it is still there (the user may have already
       closed it -- that is exactly the interrupted case, and PostMessage to a gone window is harmless). */
    hwndSettings = FindWindow(TEXT("ApplicationFrameWindow"), TEXT("Settings"));
    if (hwndSettings && !fInterrupted)
    {
        (void)PostMessage(hwndSettings, WM_CLOSE, 0, 0);
    }
    if (pAuto)
    {
        IUIAutomation_Release(pAuto);
        pAuto = NULL;
    }
    if (fCoInit)
    {
        CoUninitialize();
    }

    /* Safety net: ALWAYS restore the developer's exact original Personalize DWORDs, interrupted or not,
       so the machine is never left on the wrong theme. */
    fRestored = ThemeTestRestoreAppsUseLightTheme(fHadOriginalValue, dwOriginalValue);
    if (fRestored)
    {
        (void)ThemeTestSendNotifyImmersiveColorSet();
        ThemeTestPumpForMs(THEME_SETTLE_MS);
    }

    ThemeTestKillApp();

    if (fInterrupted)
    {
        ThemeTestInterruptNote(TEXT("T7 settings-menu theme toggle"),
                               TEXT("user closed a window or switched focus from Settings; ended without driving input elsewhere"));
    }
    else
    {
        ThemeTestCheck(fWalStarted, TEXT("T7 installs the cross-process message WAL on the app GUI thread"));
        ThemeTestCheck(cApplied == THEME_SETTINGS_TOGGLES,
                       TEXT("T7 toggles the Settings mode combo via real input (UIA focus + SendInput)"));
    }
    ThemeTestCheck(fRestored, TEXT("T7 restores both Personalize DWORDs"));

    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
}
