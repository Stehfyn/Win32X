/*
 * uxthemex.c -- shared runtime state + implementation for Win32X theming helpers.
 */

#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)
#pragma inline_depth(4)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Win32X/windefx.h"
#include "Win32X/versionhelpersx.h"
#include "Win32X/uxthemex.h"

#pragma check_stack(off)

typedef HRESULT(WINAPI* PFN_DWMSETWINDOWATTRIBUTE)(HWND, DWORD, LPCVOID, DWORD);
typedef HRESULT(WINAPI* PFN_DWMFLUSH)(void);
typedef UINT(WINAPI* PFN_TIMEPERIOD)(UINT);
typedef HRESULT(WINAPI* PFN_SETWINDOWTHEME)(HWND, LPCWSTR, LPCWSTR);
typedef BOOL(WINAPI* PFN_ALLOWDARKMODEFORWINDOW)(HWND, BOOL);
typedef BOOL(WINAPI* PFN_ALLOWDARKMODEFORAPP_1809)(BOOL);
typedef PREFERRED_APP_MODE(WINAPI* PFN_SETPREFERREDAPPMODE)(PREFERRED_APP_MODE);
typedef BOOL(WINAPI* PFN_SHOULDAPPSUSEDARKMODE)(void);
typedef void(WINAPI* PFN_FLUSHMENUTHEMES)(void);
typedef void(WINAPI* PFN_REFRESHIMMERSIVECOLORPOLICYSTATE)(void);

typedef LSTATUS(WINAPI* PFN_REGOPENKEYEXW)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
typedef LSTATUS(WINAPI* PFN_REGQUERYVALUEEXW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef LSTATUS(WINAPI* PFN_REGCLOSEKEY)(HKEY);

typedef HANDLE HPAINTBUFFER;
typedef HPAINTBUFFER(WINAPI* PFN_BEGINBUFFEREDPAINT)(HDC, const RECT*, int, void*, HDC*);
typedef HRESULT(WINAPI* PFN_ENDBUFFEREDPAINT)(HPAINTBUFFER, BOOL);
typedef HRESULT(WINAPI* PFN_BUFFEREDPAINTINIT)(void);
typedef BOOL(WINAPI* PFN_BUFFEREDPAINTRENDERANIMATION)(HWND, HDC);
typedef HANDLE HTHEME;
typedef HTHEME(WINAPI* PFN_OPENTHEMEDATA)(HWND, LPCWSTR);
typedef HRESULT(WINAPI* PFN_CLOSETHEMEDATA)(HTHEME);
typedef HRESULT(WINAPI* PFN_GETTHEMETRANSITIONDURATION)(HTHEME, int, int, int, int, DWORD*);
typedef struct BP_ANIMATIONPARAMS
{
    DWORD cbSize;
    DWORD dwFlags;
    int   style;
    DWORD dwDuration;
} BP_ANIMATIONPARAMS;
typedef struct BP_PAINTPARAMS
{
    DWORD cbSize;
    DWORD dwFlags;
    const RECT* prcExclude;
    const BLENDFUNCTION* pBlendFunction;
} BP_PAINTPARAMS;
typedef HANDLE HANIMATIONBUFFER;
typedef HANIMATIONBUFFER(WINAPI* PFN_BEGINBUFFEREDANIMATION)(
    HWND, HDC, const RECT*, int, BP_PAINTPARAMS*, BP_ANIMATIONPARAMS*, HDC*, HDC*);
typedef HRESULT(WINAPI* PFN_ENDBUFFEREDANIMATION)(HANIMATIONBUFFER, BOOL);
typedef UINT(WINAPI* PFN_GETIMMERSIVEUSERCOLORSETPREFERENCE)(BOOL, BOOL);
typedef UINT(WINAPI* PFN_GETIMMERSIVECOLORTYPEFROMNAME)(LPCWSTR);
typedef DWORD(WINAPI* PFN_GETIMMERSIVECOLORFROMCOLORSETEX)(UINT, UINT, BOOL, UINT);
typedef struct DTTOPTS
{
    DWORD    dwSize;
    DWORD    dwFlags;
    COLORREF crText;
    COLORREF crBorder;
    COLORREF crShadow;
    int      iTextShadowType;
    POINT    ptShadowOffset;
    int      iBorderSize;
    int      iFontPropId;
    int      iColorPropId;
    int      iStateId;
    BOOL     fApplyOverlay;
    int      iGlowSize;
    void*    pfnDrawTextCallback;
    LPARAM   lParam;
} DTTOPTS;
typedef HRESULT(WINAPI* PFN_DRAWTHEMETEXTEX)(
    HTHEME, HDC, int, int, LPCWSTR, int, DWORD, LPRECT, const DTTOPTS*);
#define MENU_BARITEM  8
#define MBI_NORMAL    1
#define MBI_HOT       2
#define MBI_PUSHED    3
#define MBI_DISABLED  4
#define BPBF_COMPATIBLEBITMAP 0
#define BPAS_LINEAR           1
#define BPPF_ERASE            0x0001
#define TMT_TRANSITIONDURATIONS 6000
#define DTT_TEXTCOLOR 0x00000001

#define THEME_ANIMATION_TIMER_ID ((UINT_PTR)0x57A2)
#define THEME_ANIMATION_TICK_MS  5u
#define THEME_MAX_MENU_ITEMS     32
#define THEME_ANIMATION_SLACK_MS 90u

typedef struct THEME_STATE
{
    PFN_DWMSETWINDOWATTRIBUTE            pfnDwmSetWindowAttribute;
    PFN_DWMFLUSH                         pfnDwmFlush;
    PFN_SETWINDOWTHEME                   pfnSetWindowTheme;
    PFN_ALLOWDARKMODEFORWINDOW           pfnAllowDarkModeForWindow;
    PFN_ALLOWDARKMODEFORAPP_1809         pfnAllowDarkModeForApp1809;
    PFN_SETPREFERREDAPPMODE              pfnSetPreferredAppMode;
    PFN_SHOULDAPPSUSEDARKMODE            pfnShouldAppsUseDarkMode;
    PFN_FLUSHMENUTHEMES                  pfnFlushMenuThemes;
    PFN_REFRESHIMMERSIVECOLORPOLICYSTATE pfnRefreshImmersiveColorPolicyState;
    PFN_REGOPENKEYEXW                    pfnRegOpenKeyExW;
    PFN_REGQUERYVALUEEXW                 pfnRegQueryValueExW;
    PFN_REGCLOSEKEY                      pfnRegCloseKey;
    PFN_BEGINBUFFEREDPAINT               pfnBeginBufferedPaint;
    PFN_ENDBUFFEREDPAINT                 pfnEndBufferedPaint;
    PFN_BUFFEREDPAINTINIT                pfnBufferedPaintInit;
    PFN_BEGINBUFFEREDANIMATION           pfnBeginBufferedAnimation;
    PFN_ENDBUFFEREDANIMATION             pfnEndBufferedAnimation;
    PFN_BUFFEREDPAINTRENDERANIMATION     pfnBufferedPaintRenderAnimation;
    PFN_OPENTHEMEDATA                    pfnOpenThemeData;
    PFN_CLOSETHEMEDATA                   pfnCloseThemeData;
    PFN_GETTHEMETRANSITIONDURATION       pfnGetThemeTransitionDuration;
    PFN_GETIMMERSIVEUSERCOLORSETPREFERENCE pfnGetImmersiveUserColorSetPreference;
    PFN_GETIMMERSIVECOLORTYPEFROMNAME      pfnGetImmersiveColorTypeFromName;
    PFN_GETIMMERSIVECOLORFROMCOLORSETEX    pfnGetImmersiveColorFromColorSetEx;
    PFN_DRAWTHEMETEXTEX                  pfnDrawThemeTextEx;
    PFN_TIMEPERIOD                            pfnTimeBeginPeriod;
    PFN_TIMEPERIOD                            pfnTimeEndPeriod;
    HMODULE                                   hUxtheme;
    HMODULE                                   hDwmapi;
    HMODULE                                   hAdvapi32;
    HMODULE                                   hWinmm;
    HBRUSH                                    hbrDarkBg;
    COLORREF                                  crDarkBg;
    DWORD                                     dwBackgroundTransitionMs;
    DWORD                                     dwMajorVersion;
    DWORD                                     dwMinorVersion;
    DWORD                                     dwBuildNumber;
    THEME_OS_POLICY                      policy;
    BOOL                                      fResolved;
    BOOL                                      fDarkCapable;
    BOOL                                      fRequestedDark;
    BOOL                                      fEffectiveDark;
    BOOL                                      fPendingThemeChange;
    BOOL                                      fBufferedPaintInitialized;
    BOOL                                      fAnimatingBackground;
    BOOL                                      fAnimationFromDark;
    BOOL                                      fAnimationToDark;
    BOOL                                      fTimerPeriodRaised;
    HWND                                      rgTopLevels[THEME_MAX_TOPLEVELS];
    HWND                                      rgDialogs[THEME_MAX_DIALOGS];
    HWND                                      rgAnimationWindows[THEME_MAX_TOPLEVELS + THEME_MAX_DIALOGS];
    HWND                                      rgAnimationStarted[THEME_MAX_TOPLEVELS + THEME_MAX_DIALOGS];
    HWND                                      rgMenuAnimationStarted[THEME_MAX_TOPLEVELS + THEME_MAX_DIALOGS];
    HWND                                      hwndAnimationTimer;
    UINT                                      cTopLevels;
    UINT                                      cDialogs;
    UINT                                      cAnimationWindows;
    DWORD                                     dwAnimationStartTick;
    DWORD                                     dwAnimationSnapTick;
    DWORD                                     dwAnimationReserved;
} THEME_STATE;

typedef union THEME_PROC
{
    FARPROC fp;
    PFN_DWMSETWINDOWATTRIBUTE pfnDwmSetWindowAttribute;
    PFN_DWMFLUSH pfnDwmFlush;
    PFN_TIMEPERIOD pfnTimePeriod;
    PFN_SETWINDOWTHEME pfnSetWindowTheme;
    PFN_ALLOWDARKMODEFORWINDOW pfnAllowDarkModeForWindow;
    PFN_ALLOWDARKMODEFORAPP_1809 pfnAllowDarkModeForApp1809;
    PFN_SETPREFERREDAPPMODE pfnSetPreferredAppMode;
    PFN_SHOULDAPPSUSEDARKMODE pfnShouldAppsUseDarkMode;
    PFN_FLUSHMENUTHEMES pfnFlushMenuThemes;
    PFN_REFRESHIMMERSIVECOLORPOLICYSTATE pfnRefreshImmersiveColorPolicyState;
    PFN_REGOPENKEYEXW pfnRegOpenKeyExW;
    PFN_REGQUERYVALUEEXW pfnRegQueryValueExW;
    PFN_REGCLOSEKEY pfnRegCloseKey;
    PFN_BEGINBUFFEREDPAINT pfnBeginBufferedPaint;
    PFN_ENDBUFFEREDPAINT pfnEndBufferedPaint;
    PFN_BUFFEREDPAINTINIT pfnBufferedPaintInit;
    PFN_BEGINBUFFEREDANIMATION pfnBeginBufferedAnimation;
    PFN_ENDBUFFEREDANIMATION pfnEndBufferedAnimation;
    PFN_BUFFEREDPAINTRENDERANIMATION pfnBufferedPaintRenderAnimation;
    PFN_GETTHEMETRANSITIONDURATION pfnGetThemeTransitionDuration;
    PFN_OPENTHEMEDATA pfnOpenThemeData;
    PFN_CLOSETHEMEDATA pfnCloseThemeData;
    PFN_GETIMMERSIVEUSERCOLORSETPREFERENCE pfnGetImmersiveUserColorSetPreference;
    PFN_GETIMMERSIVECOLORTYPEFROMNAME pfnGetImmersiveColorTypeFromName;
    PFN_GETIMMERSIVECOLORFROMCOLORSETEX pfnGetImmersiveColorFromColorSetEx;
    PFN_DRAWTHEMETEXTEX pfnDrawThemeTextEx;
} THEME_PROC;


static THEME_STATE g_theme;

static FORCEINLINE HMODULE ThemeLoadSystemDll(LPCTSTR pszDll)
{
    return LoadLibrary(pszDll);
}

static FORCEINLINE void ThemeReadVersion(void)
{
    OSVERSIONINFOEXW osvi;

    SecureZeroMemory(&osvi, sizeof(osvi));
    osvi.dwOSVersionInfoSize = (DWORD)sizeof(osvi);
    if (IsNonNegative(RtlGetVersionEx(&osvi)))
    {
        g_theme.dwMajorVersion = osvi.dwMajorVersion;
        g_theme.dwMinorVersion = osvi.dwMinorVersion;
        g_theme.dwBuildNumber  = osvi.dwBuildNumber;
    }
}

static FORCEINLINE THEME_OS_POLICY ThemeClassifyPolicy(void)
{
    BOOL fWindows10OrGreater;
    BOOL fWin10_1903OrGreater;
    BOOL fWin10_1809OrGreater;

    fWindows10OrGreater  = (10u <= g_theme.dwMajorVersion);
    fWin10_1809OrGreater = (10u < g_theme.dwMajorVersion) ||
                            (fWindows10OrGreater && (BUILD_WIN10_1809 <= g_theme.dwBuildNumber));
    fWin10_1903OrGreater = (10u < g_theme.dwMajorVersion) ||
                            (fWindows10OrGreater && (BUILD_WIN10_1903 <= g_theme.dwBuildNumber));

    if (!fWin10_1809OrGreater)
    {
        return ThemePolicyClassic;
    }
    if (fWin10_1903OrGreater)
    {
        return ThemePolicyWin10_1903Plus;
    }
    return ThemePolicyWin10_1809;
}

static FORCEINLINE void ThemeResolve(void)
{
    THEME_PROC u;

    if (g_theme.fResolved)
    {
        return;
    }
    g_theme.fResolved = TRUE;

    ThemeReadVersion();
    g_theme.policy = ThemeClassifyPolicy();
    g_theme.hUxtheme  = ThemeLoadSystemDll(TEXT("uxtheme.dll"));
    g_theme.hDwmapi   = ThemeLoadSystemDll(TEXT("dwmapi.dll"));
    g_theme.hAdvapi32 = ThemeLoadSystemDll(TEXT("advapi32.dll"));
    g_theme.hWinmm    = ThemeLoadSystemDll(TEXT("winmm.dll"));

    if (IsNonNull(g_theme.hWinmm))
    {
        u.fp = GetProcAddress(g_theme.hWinmm, "timeBeginPeriod");
        g_theme.pfnTimeBeginPeriod = u.pfnTimePeriod;
        u.fp = GetProcAddress(g_theme.hWinmm, "timeEndPeriod");
        g_theme.pfnTimeEndPeriod = u.pfnTimePeriod;
    }

    if (IsNonNull(g_theme.hAdvapi32))
    {
        u.fp = GetProcAddress(g_theme.hAdvapi32, "RegOpenKeyExW");
        g_theme.pfnRegOpenKeyExW = u.pfnRegOpenKeyExW;
        u.fp = GetProcAddress(g_theme.hAdvapi32, "RegQueryValueExW");
        g_theme.pfnRegQueryValueExW = u.pfnRegQueryValueExW;
        u.fp = GetProcAddress(g_theme.hAdvapi32, "RegCloseKey");
        g_theme.pfnRegCloseKey = u.pfnRegCloseKey;
    }

    if (IsNonNull(g_theme.hUxtheme))
    {
        u.fp = GetProcAddress(g_theme.hUxtheme, "SetWindowTheme");
        g_theme.pfnSetWindowTheme = u.pfnSetWindowTheme;
        u.fp = GetProcAddress(g_theme.hUxtheme, MAKEINTRESOURCEA(UXTHEME_ORD_ALLOW_DARK_FOR_WINDOW));
        g_theme.pfnAllowDarkModeForWindow = u.pfnAllowDarkModeForWindow;
        u.fp = GetProcAddress(g_theme.hUxtheme, MAKEINTRESOURCEA(UXTHEME_ORD_SHOULD_APPS_USE_DARK));
        g_theme.pfnShouldAppsUseDarkMode = u.pfnShouldAppsUseDarkMode;
        u.fp = GetProcAddress(g_theme.hUxtheme, MAKEINTRESOURCEA(UXTHEME_ORD_FLUSH_MENU_THEMES));
        g_theme.pfnFlushMenuThemes = u.pfnFlushMenuThemes;
        u.fp = GetProcAddress(g_theme.hUxtheme, MAKEINTRESOURCEA(UXTHEME_ORD_REFRESH_IMMERSIVE));
        g_theme.pfnRefreshImmersiveColorPolicyState = u.pfnRefreshImmersiveColorPolicyState;

        if (IsEqual(ThemePolicyWin10_1809, g_theme.policy))
        {
            u.fp = GetProcAddress(g_theme.hUxtheme, MAKEINTRESOURCEA(UXTHEME_ORD_APP_MODE_135));
            g_theme.pfnAllowDarkModeForApp1809 = u.pfnAllowDarkModeForApp1809;
        }
        else if (IsEqual(ThemePolicyWin10_1903Plus, g_theme.policy))
        {
            u.fp = GetProcAddress(g_theme.hUxtheme, MAKEINTRESOURCEA(UXTHEME_ORD_APP_MODE_135));
            g_theme.pfnSetPreferredAppMode = u.pfnSetPreferredAppMode;
        }

        u.fp = GetProcAddress(g_theme.hUxtheme, "BeginBufferedPaint");
        g_theme.pfnBeginBufferedPaint = u.pfnBeginBufferedPaint;
        u.fp = GetProcAddress(g_theme.hUxtheme, "EndBufferedPaint");
        g_theme.pfnEndBufferedPaint = u.pfnEndBufferedPaint;
        u.fp = GetProcAddress(g_theme.hUxtheme, "BufferedPaintInit");
        g_theme.pfnBufferedPaintInit = u.pfnBufferedPaintInit;
        u.fp = GetProcAddress(g_theme.hUxtheme, "BeginBufferedAnimation");
        g_theme.pfnBeginBufferedAnimation = u.pfnBeginBufferedAnimation;
        u.fp = GetProcAddress(g_theme.hUxtheme, "EndBufferedAnimation");
        g_theme.pfnEndBufferedAnimation = u.pfnEndBufferedAnimation;
        u.fp = GetProcAddress(g_theme.hUxtheme, "BufferedPaintRenderAnimation");
        g_theme.pfnBufferedPaintRenderAnimation = u.pfnBufferedPaintRenderAnimation;
        u.fp = GetProcAddress(g_theme.hUxtheme, "GetThemeTransitionDuration");
        g_theme.pfnGetThemeTransitionDuration = u.pfnGetThemeTransitionDuration;
        u.fp = GetProcAddress(g_theme.hUxtheme, "OpenThemeData");
        g_theme.pfnOpenThemeData = u.pfnOpenThemeData;
        u.fp = GetProcAddress(g_theme.hUxtheme, "CloseThemeData");
        g_theme.pfnCloseThemeData = u.pfnCloseThemeData;
        u.fp = GetProcAddress(g_theme.hUxtheme, "GetImmersiveUserColorSetPreference");
        g_theme.pfnGetImmersiveUserColorSetPreference = u.pfnGetImmersiveUserColorSetPreference;
        u.fp = GetProcAddress(g_theme.hUxtheme, MAKEINTRESOURCEA(96));
        g_theme.pfnGetImmersiveColorTypeFromName = u.pfnGetImmersiveColorTypeFromName;
        u.fp = GetProcAddress(g_theme.hUxtheme, "GetImmersiveColorFromColorSetEx");
        g_theme.pfnGetImmersiveColorFromColorSetEx = u.pfnGetImmersiveColorFromColorSetEx;
        u.fp = GetProcAddress(g_theme.hUxtheme, "DrawThemeTextEx");
        g_theme.pfnDrawThemeTextEx = u.pfnDrawThemeTextEx;
    }

    if (IsNonNull(g_theme.hDwmapi))
    {
        u.fp = GetProcAddress(g_theme.hDwmapi, "DwmSetWindowAttribute");
        g_theme.pfnDwmSetWindowAttribute = u.pfnDwmSetWindowAttribute;
        u.fp = GetProcAddress(g_theme.hDwmapi, "DwmFlush");
        g_theme.pfnDwmFlush = u.pfnDwmFlush;
    }

    if (IsEqual(ThemePolicyWin10_1809, g_theme.policy))
    {
        g_theme.fDarkCapable =
            g_theme.pfnAllowDarkModeForApp1809 &&
            g_theme.pfnAllowDarkModeForWindow &&
            g_theme.pfnRefreshImmersiveColorPolicyState &&
            g_theme.pfnFlushMenuThemes &&
            g_theme.pfnSetWindowTheme &&
            g_theme.pfnDwmSetWindowAttribute;
    }
    else if (IsEqual(ThemePolicyWin10_1903Plus, g_theme.policy))
    {
        g_theme.fDarkCapable =
            g_theme.pfnSetPreferredAppMode &&
            g_theme.pfnAllowDarkModeForWindow &&
            g_theme.pfnRefreshImmersiveColorPolicyState &&
            g_theme.pfnFlushMenuThemes &&
            g_theme.pfnSetWindowTheme &&
            g_theme.pfnDwmSetWindowAttribute;
    }
    else
    {
        g_theme.fDarkCapable = FALSE;
    }
}

FORCEINLINE BOOL WINAPI ThemeCanUseDarkMode(void)
{
    ThemeResolve();
    return g_theme.fDarkCapable;
}

static FORCEINLINE void ThemeSetProcessDarkModeAllowed(BOOL fAllow)
{
    PREFERRED_APP_MODE appMode;
    BOOL                    fPolicy1809;
    BOOL                    fPolicy1903Plus;

    ThemeResolve();
    if (!g_theme.fDarkCapable)
    {
        return;
    }
    fPolicy1809     = (ThemePolicyWin10_1809 == g_theme.policy);
    fPolicy1903Plus = (ThemePolicyWin10_1903Plus == g_theme.policy);
    if (fPolicy1809 && g_theme.pfnAllowDarkModeForApp1809)
    {
        (void)g_theme.pfnAllowDarkModeForApp1809(fAllow);
    }
    else if (fPolicy1903Plus && g_theme.pfnSetPreferredAppMode)
    {
        appMode = AppModeDefault;
        if (fAllow)
        {
            appMode = AppModeAllowDark;
        }
        (void)g_theme.pfnSetPreferredAppMode(appMode);
    }
}

FORCEINLINE void WINAPI ThemeRefresh(void)
{
    ThemeResolve();
    if (g_theme.pfnRefreshImmersiveColorPolicyState)
    {
        g_theme.pfnRefreshImmersiveColorPolicyState();
    }
}

static FORCEINLINE BOOL ThemeReadPersonalizeDarkMode(BOOL* pfDark)
{
    HKEY   hKey;
    DWORD  dwValue;
    DWORD  dwType;
    DWORD  cbValue;
    LSTATUS lStatus;
    BOOL   fMissingRegFns;
    BOOL   fBadRead;

    ThemeResolve();
    fMissingRegFns =
        (!g_theme.pfnRegOpenKeyExW) || (!g_theme.pfnRegQueryValueExW) || (!g_theme.pfnRegCloseKey);
    if (fMissingRegFns)
    {
        return FALSE;
    }

    hKey = NULL;
    lStatus = g_theme.pfnRegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0u,
        KEY_QUERY_VALUE,
        &hKey);
    if (ERROR_SUCCESS != lStatus)
    {
        return FALSE;
    }

    dwValue = 1u;
    dwType  = 0u;
    cbValue = (DWORD)sizeof(dwValue);
    lStatus = g_theme.pfnRegQueryValueExW(
        hKey, L"AppsUseLightTheme", NULL, &dwType, (LPBYTE)&dwValue, &cbValue);
    (void)g_theme.pfnRegCloseKey(hKey);
    fBadRead = (ERROR_SUCCESS != lStatus) || (REG_DWORD != dwType) || ((DWORD)sizeof(dwValue) != cbValue);
    if (fBadRead)
    {
        return FALSE;
    }

    *pfDark = (0u == dwValue);
    return TRUE;
}

FORCEINLINE BOOL WINAPI ThemeAppsUseDarkMode(void)
{
    BOOL fDark;

    ThemeResolve();
    if (g_theme.pfnShouldAppsUseDarkMode)
    {
        return g_theme.pfnShouldAppsUseDarkMode();
    }
    fDark = FALSE;
    if (ThemeReadPersonalizeDarkMode(&fDark))
    {
        return fDark;
    }
    return FALSE;
}

FORCEINLINE BOOL WINAPI ThemeEffectiveDarkMode(void)
{
    ThemeResolve();
    g_theme.fRequestedDark = ThemeAppsUseDarkMode();
    g_theme.fEffectiveDark = g_theme.fRequestedDark && g_theme.fDarkCapable;
    return g_theme.fEffectiveDark;
}

static FORCEINLINE BOOL ThemeReadEffectiveDarkMode(BOOL* pfRequestedDark)
{
    BOOL fRequestedDark;

    ThemeResolve();
    fRequestedDark = ThemeAppsUseDarkMode();
    if (pfRequestedDark)
    {
        *pfRequestedDark = fRequestedDark;
    }
    return fRequestedDark && g_theme.fDarkCapable;
}

static FORCEINLINE COLORREF ThemeColorFromImmersive(DWORD dwArgb)
{
    return RGB((BYTE)(dwArgb & 0xFFu), (BYTE)((dwArgb >> 8) & 0xFFu), (BYTE)((dwArgb >> 16) & 0xFFu));
}

static FORCEINLINE COLORREF ThemeResolveDarkBackgroundColor(void)
{
    UINT  uColorSet;
    UINT  uType;
    DWORD dwColor;

    ThemeResolve();
    if (g_theme.pfnGetImmersiveUserColorSetPreference &&
        g_theme.pfnGetImmersiveColorTypeFromName &&
        g_theme.pfnGetImmersiveColorFromColorSetEx)
    {
        uColorSet = g_theme.pfnGetImmersiveUserColorSetPreference(FALSE, FALSE);
        uType = g_theme.pfnGetImmersiveColorTypeFromName(L"ImmersiveDarkChromeMedium");
        if (0xFFFFFFFFu != uType)
        {
            dwColor = g_theme.pfnGetImmersiveColorFromColorSetEx(uColorSet, uType, FALSE, 0u);
            if (0xFFFF00FFu != dwColor)
            {
                return ThemeColorFromImmersive(dwColor);
            }
        }
    }
    return DARK_BG;
}

static FORCEINLINE COLORREF ThemeBackgroundColor(BOOL fDark)
{
    if (fDark)
    {
        return ThemeResolveDarkBackgroundColor();
    }
    return GetSysColor(COLOR_WINDOW);
}

static FORCEINLINE HBRUSH ThemeBackgroundBrushForEffectiveMode(BOOL fDark)
{
    COLORREF crDark;

    if (!fDark)
    {
        return (HBRUSH)(COLOR_WINDOW + 1);
    }
    crDark = ThemeResolveDarkBackgroundColor();
    if (!g_theme.hbrDarkBg || (g_theme.crDarkBg != crDark))
    {
        if (g_theme.hbrDarkBg)
        {
            DeleteObject(g_theme.hbrDarkBg);
        }
        g_theme.hbrDarkBg = CreateSolidBrush(crDark);
        g_theme.crDarkBg = crDark;
    }
    return g_theme.hbrDarkBg;
}

static FORCEINLINE void ThemePaintBackgroundColor(HDC hdc, const RECT* prc, BOOL fDark)
{
    HBRUSH  hbr;
    HGDIOBJ hbrPrev;

    if (fDark)
    {
        hbr = ThemeBackgroundBrushForEffectiveMode(TRUE);
    }
    else
    {
        hbr = GetSysColorBrush(COLOR_WINDOW);
    }
    if (!hbr)
    {
        return;
    }
    hbrPrev = SelectObject(hdc, hbr);
    if (hbrPrev)
    {
        (void)PatBlt(hdc, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, PATCOPY);
        SelectObject(hdc, hbrPrev);
    }
}

static FORCEINLINE COLORREF ThemeLerpColor(COLORREF crFrom, COLORREF crTo, DWORD dwNum, DWORD dwDen)
{
    int rf;
    int gf;
    int bf;
    int rt;
    int gt;
    int bt;
    int num;
    int den;

    rf = (int)GetRValue(crFrom);
    gf = (int)GetGValue(crFrom);
    bf = (int)GetBValue(crFrom);
    rt = (int)GetRValue(crTo);
    gt = (int)GetGValue(crTo);
    bt = (int)GetBValue(crTo);
    num = (int)dwNum;
    den = (int)dwDen;

    /* Branchless clamp: den>=1 (a zero duration collapses to "fully crTo"), and num in [0,den].
     * No conditional means no control-dependent load, so the optimized build stays C5045-clean.
     * den += (0==den): folds a zero denominator up to 1.  num += (den-num) & ((den-num)>>31): the
     * arithmetic shift is all-ones exactly when num>den, selecting the (den-num) correction that
     * pins num to den; otherwise it is zero and num is untouched. */
    den += (int)(0 == den);
    num += (den - num) & ((den - num) >> 31);
    return RGB(rf + (((rt - rf) * num) / den),
              gf + (((gt - gf) * num) / den),
              bf + (((bt - bf) * num) / den));
}

static FORCEINLINE void ThemePaintSolidColor(HDC hdc, const RECT* prc, COLORREF cr)
{
    HBRUSH  hbr;
    HGDIOBJ hbrPrev;

    hbr = GetStockObject(DC_BRUSH);
    if (!hbr)
    {
        return;
    }
    hbrPrev = SelectObject(hdc, hbr);
    if (hbrPrev)
    {
        SetDCBrushColor(hdc, cr);
        (void)PatBlt(hdc, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, PATCOPY);
        SelectObject(hdc, hbrPrev);
    }
}

static FORCEINLINE DWORD ThemeBackgroundTransitionMs(void);

static FORCEINLINE DWORD ThemeBackgroundTransitionMs(void)
{
    HTHEME hTheme;
    DWORD  dwDuration;

    ThemeResolve();
    if (g_theme.dwBackgroundTransitionMs)
    {
        return g_theme.dwBackgroundTransitionMs;
    }
    dwDuration = 0u;
    if (g_theme.pfnOpenThemeData && g_theme.pfnGetThemeTransitionDuration && g_theme.pfnCloseThemeData)
    {
        hTheme = g_theme.pfnOpenThemeData(NULL, L"Button");
        if (hTheme)
        {
            if (FAILED(g_theme.pfnGetThemeTransitionDuration(hTheme, 1, 1, 2, TMT_TRANSITIONDURATIONS, &dwDuration)))
            {
                dwDuration = 0u;
            }
            g_theme.pfnCloseThemeData(hTheme);
        }
    }
    g_theme.dwBackgroundTransitionMs = dwDuration;
    return dwDuration;
}

/*
 * DWM fades the immersive caption on a mildly front-loaded (ease-out) curve, well approximated by the
 * rational p = 4t/(3+t). The client/menu are linear in elapsed time, so without this they trail the
 * caption through the body of the transition; mapping elapsed time through the same ease-out makes the
 * body track the caption to within the band. (The residual is a ~1-frame phase offset at the steep
 * entry/exit -- inherent because the app's wall-clock timer cannot be frame-locked to DWM's separately
 * composited caption -- which the analyzer's per-frame band check tolerates with a +/-1 frame window.)
 * Returned as the effective elapsed the linear lerp consumes: eff = 4*e*d / (3d + e) (integer,
 * branchless, C5045-safe -- no control-dependent load).
 */
static FORCEINLINE DWORD ThemeEaseElapsed(DWORD dwElapsed, DWORD dwDuration)
{
    int       e;
    int       d;
    DWORDLONG ullNum;
    DWORDLONG ullDen;

    e = (int)dwElapsed;
    d = (int)dwDuration;
    /* Branchless clamp of e to [0,d] and fold of a zero duration to 1 (ThemeLerpColor's idiom):
     * d += (0==d) lifts a zero denominator to 1; (d-e)>>31 is all-ones exactly when e>d, selecting the
     * (d-e) correction that pins e to d; ~(e>>31) pins a (defensive) negative e to 0. den = 3d+e >= 3. */
    d += (int)(0 == d);
    e += (d - e) & ((d - e) >> 31);
    e &= ~(e >> 31);
    ullNum = (DWORDLONG)4 * (DWORDLONG)(DWORD)e * (DWORDLONG)(DWORD)d;
    ullDen = (DWORDLONG)3 * (DWORDLONG)(DWORD)d + (DWORDLONG)(DWORD)e;
    return (DWORD)(ullNum / ullDen);
}

/*
 * Client background color at the current point of the transition, interpolated on the same clock
 * (dwAnimationStartTick + ThemeBackgroundTransitionMs) the menu bar uses, so the client and the
 * owner-drawn menu cross-fade in lockstep instead of drifting on independent animation timelines.
 */
static FORCEINLINE COLORREF ThemeClientAnimationColor(void)
{
    COLORREF crFrom;
    COLORREF crTo;
    DWORD    dwDuration;
    DWORD    dwElapsed;

    crFrom = ThemeBackgroundColor(g_theme.fAnimationFromDark);
    crTo = ThemeBackgroundColor(g_theme.fAnimationToDark);
    dwDuration = ThemeBackgroundTransitionMs();
    /* Use the per-tick clock snapshot, not a fresh GetTickCount, so the client and the menu bar
       (which is painted a moment later in the same tick) interpolate at the EXACT same t -- on a
       fast fade an independent read a few ms apart is several percent of progress, which would drift
       the two surfaces out of the caption's band. */
    dwElapsed = g_theme.dwAnimationSnapTick - g_theme.dwAnimationStartTick;
    return ThemeLerpColor(crFrom, crTo, ThemeEaseElapsed(dwElapsed, dwDuration), dwDuration);
}

static FORCEINLINE void ThemeArmBackgroundAnimationWindows(void);
static FORCEINLINE void ThemeCompletePendingThemeChange(void);
static FORCEINLINE void ThemeStopAnimationTimer(void);
static void CALLBACK ThemeAnimationTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
static FORCEINLINE COLORREF ThemeMenuAnimationColor(void);
static FORCEINLINE COLORREF ThemeMenuTextAnimationColor(void);
static FORCEINLINE void ThemePaintMenuBarColor(HWND hwnd, COLORREF crBar, COLORREF crText);
static BOOL CALLBACK ThemeApplyControlProc(HWND hChild, LPARAM lParam);
FORCEINLINE void WINAPI MenuBarPalette(BOOL fDark, MENUBAR_PALETTE* pPalette);
static FORCEINLINE void MenuBarPaintSeamToHdc(HWND hwnd, HDC hdc, const MENUBAR_PALETTE* pPalette);
static FORCEINLINE void MenuBarPaintSeamToHdcEx(HWND hwnd, HDC hdc, const MENUBAR_PALETTE* pPalette, int dx, int dy);
FORCEINLINE void WINAPI MenuBarPaintSeam(HWND hwnd, const MENUBAR_PALETTE* pPalette);

/*
 * Returns TRUE only when this call actually (re)armed a transition -- the caller gates its
 * clock-stamp/caption-flip/invalidate tail on that, so a duplicate broadcast that arms nothing also
 * repaints nothing. Returns FALSE for a no-op (from==to) or a duplicate of a live same-direction fade.
 */
static FORCEINLINE BOOL ThemeStartBackgroundTransition(BOOL fFromDark, BOOL fToDark)
{
    if (fFromDark == fToDark)
    {
        return FALSE;
    }
    /* Idempotent: a theme switch delivers more than one ImmersiveColorSet broadcast (the app's own
       re-broadcast plus the system's on the registry write). ThemeArmBackgroundAnimationWindows
       re-stamps dwAnimationStartTick and re-arms the timer, so honoring a duplicate broadcast mid-fade
       would reset the shared clock -- surfaces already painted on the first clock (e.g. the menu bar)
       would then read tens of percent ahead of surfaces repainted against the reset clock. While a
       transition in this same direction is live, ignore the duplicate and let the running clock ride. */
    if (g_theme.fAnimatingBackground &&
        (g_theme.fAnimationFromDark == fFromDark) &&
        (g_theme.fAnimationToDark == fToDark))
    {
        return FALSE;
    }
    g_theme.fAnimationFromDark = fFromDark;
    g_theme.fAnimationToDark = fToDark;
    ThemeArmBackgroundAnimationWindows();
    return TRUE;
}

static FORCEINLINE BOOL ThemeCanPaintBackgroundAnimation(void)
{
    ThemeResolve();
    return g_theme.pfnBeginBufferedAnimation &&
           g_theme.pfnEndBufferedAnimation &&
           g_theme.pfnBufferedPaintRenderAnimation;
}

static FORCEINLINE void ThemeListAddUnique(HWND* prg, UINT* pc, UINT cMax, HWND hwnd)
{
    HWND* phwnd;
    UINT  c;

    if (!hwnd)
    {
        return;
    }
    phwnd = prg;
    c = *pc;
    while (0u != c)
    {
        if (*phwnd == hwnd)
        {
            return;
        }
        ++phwnd;
        --c;
    }
    if (*pc < cMax)
    {
        prg[*pc] = hwnd;
        *pc = *pc + 1u;
    }
}

static FORCEINLINE void ThemeArmBackgroundAnimationWindows(void)
{
    HWND* phwnd;
    HWND  hwnd;
    UINT  c;

    g_theme.cAnimationWindows = 0u;
    SecureZeroMemory(g_theme.rgAnimationStarted, sizeof(g_theme.rgAnimationStarted));
    SecureZeroMemory(g_theme.rgMenuAnimationStarted, sizeof(g_theme.rgMenuAnimationStarted));

    phwnd = g_theme.rgDialogs;
    c = g_theme.cDialogs;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            ThemeListAddUnique(g_theme.rgAnimationWindows,
                               &g_theme.cAnimationWindows,
                               ARRAYSIZE(g_theme.rgAnimationWindows),
                               hwnd);
        }
        ++phwnd;
        --c;
    }

    phwnd = g_theme.rgTopLevels;
    c = g_theme.cTopLevels;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            ThemeListAddUnique(g_theme.rgAnimationWindows,
                               &g_theme.cAnimationWindows,
                               ARRAYSIZE(g_theme.rgAnimationWindows),
                               hwnd);
        }
        ++phwnd;
        --c;
    }

    g_theme.fAnimatingBackground = (0u != g_theme.cAnimationWindows);

    ThemeStopAnimationTimer();
    if (g_theme.fAnimatingBackground)
    {
        g_theme.dwAnimationStartTick = GetTickCount();
        /* Arm with a TIMERPROC, not a NULL callback. A NULL callback posts WM_TIMER to the window's
           queue, which only fires if the app's WndProc routes WM_TIMER into the theme handler -- a
           hidden cooperation requirement the app silently failed, leaving the transition (and with
           it the DWM caption swap) never completed. With a TIMERPROC the tick is delivered straight
           to the callback by the app's normal DispatchMessage loop, so the engine drives the whole
           synchronized transition -- client, menu, and caption -- with no WndProc cooperation. */
        if (IsWindow(g_theme.rgAnimationWindows[0]) &&
            SetTimer(g_theme.rgAnimationWindows[0], THEME_ANIMATION_TIMER_ID, THEME_ANIMATION_TICK_MS,
                     ThemeAnimationTimerProc))
        {
            g_theme.hwndAnimationTimer = g_theme.rgAnimationWindows[0];
        }
    }
}

static FORCEINLINE BOOL ThemeWindowHasBackgroundAnimation(HWND hwnd)
{
    HWND* phwnd;
    UINT  c;

    phwnd = g_theme.rgAnimationWindows;
    c = g_theme.cAnimationWindows;
    while (0u != c)
    {
        if (*phwnd == hwnd)
        {
            return TRUE;
        }
        ++phwnd;
        --c;
    }
    return FALSE;
}

static FORCEINLINE void ThemeWindowFinishBackgroundAnimation(HWND hwnd)
{
    UINT i;

    i = 0u;
    while (i < g_theme.cAnimationWindows)
    {
        if (g_theme.rgAnimationWindows[i] == hwnd)
        {
            --g_theme.cAnimationWindows;
            g_theme.rgAnimationWindows[i] = g_theme.rgAnimationWindows[g_theme.cAnimationWindows];
            g_theme.rgAnimationWindows[g_theme.cAnimationWindows] = NULL;
            g_theme.rgAnimationStarted[i] = g_theme.rgAnimationStarted[g_theme.cAnimationWindows];
            g_theme.rgAnimationStarted[g_theme.cAnimationWindows] = NULL;
            g_theme.rgMenuAnimationStarted[i] = g_theme.rgMenuAnimationStarted[g_theme.cAnimationWindows];
            g_theme.rgMenuAnimationStarted[g_theme.cAnimationWindows] = NULL;
            g_theme.fAnimatingBackground = (0u != g_theme.cAnimationWindows);
            if (!g_theme.fAnimatingBackground)
            {
                ThemeCompletePendingThemeChange();
            }
            return;
        }
        ++i;
    }
}

static FORCEINLINE BOOL ThemeWindowStartedBackgroundAnimation(HWND hwnd)
{
    HWND* phwnd;
    UINT  c;

    phwnd = g_theme.rgAnimationStarted;
    c = g_theme.cAnimationWindows;
    while (0u != c)
    {
        if (*phwnd == hwnd)
        {
            return TRUE;
        }
        ++phwnd;
        --c;
    }
    return FALSE;
}

static FORCEINLINE void ThemeWindowMarkBackgroundAnimationStarted(HWND hwnd)
{
    UINT i;

    i = 0u;
    while (i < g_theme.cAnimationWindows)
    {
        if (g_theme.rgAnimationWindows[i] == hwnd)
        {
            g_theme.rgAnimationStarted[i] = hwnd;
            return;
        }
        ++i;
    }
}

static FORCEINLINE BOOL ThemeWindowStartedMenuAnimation(HWND hwnd)
{
    HWND* phwnd;
    UINT  c;

    phwnd = g_theme.rgMenuAnimationStarted;
    c = g_theme.cAnimationWindows;
    while (0u != c)
    {
        if (*phwnd == hwnd)
        {
            return TRUE;
        }
        ++phwnd;
        --c;
    }
    return FALSE;
}

static FORCEINLINE void ThemeWindowMarkMenuAnimationStarted(HWND hwnd)
{
    UINT i;

    i = 0u;
    while (i < g_theme.cAnimationWindows)
    {
        if (g_theme.rgAnimationWindows[i] == hwnd)
        {
            g_theme.rgMenuAnimationStarted[i] = hwnd;
            return;
        }
        ++i;
    }
}

static FORCEINLINE void ThemeInvalidateAnimationWindows(void)
{
    HWND* phwnd;
    HWND  hwnd;
    MENUBAR_PALETTE pal;
    UINT  c;

    phwnd = g_theme.rgAnimationWindows;
    c = g_theme.cAnimationWindows;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
        }
        ++phwnd;
        --c;
    }

    phwnd = g_theme.rgTopLevels;
    c = g_theme.cTopLevels;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd) && GetMenu(hwnd) && !ThemeWindowHasBackgroundAnimation(hwnd))
        {
            MenuBarPalette(ThemeIsDarkMode(), &pal);
            DrawMenuBar(hwnd);
            RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOCHILDREN);
            DrawMenuBar(hwnd);
            MenuBarPaintSeam(hwnd, &pal);
        }
        ++phwnd;
        --c;
    }
}

static FORCEINLINE LPCWSTR ThemeSubAppName(BOOL fDark)
{
    if (fDark && ThemeCanUseDarkMode())
    {
        return L"DarkMode_Explorer";
    }
    return L"Explorer";
}

static FORCEINLINE void ThemeApplyClientTheme(HWND hwnd, BOOL fDark)
{
    BOOL fEffective;

    fEffective = fDark && ThemeCanUseDarkMode();
    if (g_theme.pfnAllowDarkModeForWindow)
    {
        (void)g_theme.pfnAllowDarkModeForWindow(hwnd, fEffective);
    }
    if (g_theme.pfnSetWindowTheme)
    {
        (void)g_theme.pfnSetWindowTheme(hwnd, ThemeSubAppName(fEffective), NULL);
    }
}

static BOOL CALLBACK ThemeApplyClientThemeProc(HWND hChild, LPARAM lParam)
{
    ThemeApplyClientTheme(hChild, 0 != lParam);
    return TRUE;
}

static BOOL CALLBACK ThemeInvalidateWindowProc(HWND hwnd, LPARAM lParam)
{
    BOOL fErase;

    fErase = 0 != lParam;
    InvalidateRect(hwnd, NULL, fErase);
    if (fErase)
    {
        UpdateWindow(hwnd);
    }
    return TRUE;
}

static FORCEINLINE void ThemeSetRegisteredClassBrushes(BOOL fDark)
{
    HWND* phwnd;
    HWND  hwnd;
    UINT  c;

    phwnd = g_theme.rgTopLevels;
    c = g_theme.cTopLevels;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)ThemeBackgroundBrush(fDark));
        }
        ++phwnd;
        --c;
    }

    phwnd = g_theme.rgDialogs;
    c = g_theme.cDialogs;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)ThemeBackgroundBrush(fDark));
        }
        ++phwnd;
        --c;
    }
}

static FORCEINLINE void ThemeSetRegisteredTopLevelThemes(BOOL fDark)
{
    HWND* phwnd;
    HWND  hwnd;
    UINT  c;

    phwnd = g_theme.rgTopLevels;
    c = g_theme.cTopLevels;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)ThemeBackgroundBrush(fDark));
            ThemeApplyWindow(hwnd, fDark);
        }
        ++phwnd;
        --c;
    }
}

static FORCEINLINE void ThemeSetRegisteredDialogThemes(BOOL fDark)
{
    HWND* phwnd;
    HWND  hwnd;
    UINT  c;

    phwnd = g_theme.rgDialogs;
    c = g_theme.cDialogs;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)ThemeBackgroundBrush(fDark));
            ThemeApplyDialog(hwnd, fDark);
            EnumChildWindows(hwnd, ThemeApplyClientThemeProc, (LPARAM)fDark);
        }
        ++phwnd;
        --c;
    }
}

static FORCEINLINE void ThemeInvalidateRegisteredWindows(void)
{
    HWND* phwnd;
    HWND  hwnd;
    UINT  c;
    BOOL  fErase;

    fErase = !ThemeCanPaintBackgroundAnimation() || !g_theme.fAnimatingBackground;

    phwnd = g_theme.rgDialogs;
    c = g_theme.cDialogs;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            InvalidateRect(hwnd, NULL, fErase);
            EnumChildWindows(hwnd, ThemeInvalidateWindowProc, (LPARAM)fErase);
            if (fErase)
            {
                UpdateWindow(hwnd);
            }
        }
        ++phwnd;
        --c;
    }

    phwnd = g_theme.rgTopLevels;
    c = g_theme.cTopLevels;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            InvalidateRect(hwnd, NULL, fErase);
            if (fErase)
            {
                UpdateWindow(hwnd);
            }
        }
        ++phwnd;
        --c;
    }
}

static FORCEINLINE void ThemeRepaintRegisteredMenus(void)
{
    HWND* phwnd;
    HWND  hwnd;
    UINT  c;

    phwnd = g_theme.rgTopLevels;
    c = g_theme.cTopLevels;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd) && GetMenu(hwnd))
        {
            DrawMenuBar(hwnd);
        }
        ++phwnd;
        --c;
    }
}

static FORCEINLINE void ThemeApplyRegisteredVisualState(BOOL fDark)
{
    HWND* phwnd;
    HWND  hwnd;
    UINT  c;

    ThemeSetProcessDarkModeAllowed(TRUE);
    ThemeRefresh();

    phwnd = g_theme.rgDialogs;
    c = g_theme.cDialogs;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            ThemeApplyDialog(hwnd, fDark);
            EnumChildWindows(hwnd, ThemeApplyControlProc, (LPARAM)fDark);
        }
        ++phwnd;
        --c;
    }

    phwnd = g_theme.rgTopLevels;
    c = g_theme.cTopLevels;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            ThemeApplyWindow(hwnd, fDark);
        }
        ++phwnd;
        --c;
    }
    ThemeRepaintRegisteredMenus();
}

static FORCEINLINE void ThemeCompletePendingThemeChange(void)
{
    if (!g_theme.fPendingThemeChange)
    {
        return;
    }
    g_theme.fPendingThemeChange = FALSE;
    ThemeApplyRegisteredVisualState(g_theme.fEffectiveDark);
}

static FORCEINLINE void ThemeStopAnimationTimer(void)
{
    if (g_theme.hwndAnimationTimer)
    {
        (void)KillTimer(g_theme.hwndAnimationTimer, THEME_ANIMATION_TIMER_ID);
        g_theme.hwndAnimationTimer = NULL;
    }
}

static FORCEINLINE void ThemeFinishAllBackgroundAnimation(void)
{
    g_theme.cAnimationWindows = 0u;
    SecureZeroMemory(g_theme.rgAnimationWindows, sizeof(g_theme.rgAnimationWindows));
    SecureZeroMemory(g_theme.rgAnimationStarted, sizeof(g_theme.rgAnimationStarted));
    SecureZeroMemory(g_theme.rgMenuAnimationStarted, sizeof(g_theme.rgMenuAnimationStarted));
    g_theme.fAnimatingBackground = FALSE;
    ThemeCompletePendingThemeChange();
    ThemeSetRegisteredClassBrushes(g_theme.fEffectiveDark);
    ThemeInvalidateRegisteredWindows();
}

/*
 * Apply only the non-client frame (DWM immersive-dark caption) to every registered window. The
 * caption is a binary DWM state -- it cannot crossfade like the client -- so flipping it at either
 * end of the transition leaves a window where the caption is one shade and the already-/not-yet-
 * crossfaded client is the other: a mixed frame. Flipping it at the transition midpoint, while the
 * client is in intermediate (neither-dark-nor-light) color, hides the snap and keeps every surface
 * coherent across the whole transition.
 */
static FORCEINLINE void ThemeApplyRegisteredFrames(BOOL fDark)
{
    HWND* phwnd;
    HWND  hwnd;
    UINT  c;

    phwnd = g_theme.rgTopLevels;
    c = g_theme.cTopLevels;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            ThemeApplyFrame(hwnd, fDark);
        }
        ++phwnd;
        --c;
    }

    phwnd = g_theme.rgDialogs;
    c = g_theme.cDialogs;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            ThemeApplyFrame(hwnd, fDark);
        }
        ++phwnd;
        --c;
    }
}

/*
 * The buffered-paint crossfade auto-invalidates only while the animation is live, so it never
 * delivers a terminal WM_PAINT to finalize the swap, and the owner-drawn menu bar is never redrawn
 * mid-transition. This timer pumps repaint of the client (driving the client crossfade) and the
 * menu bar (driving the elapsed-time menu crossfade) every tick, flips the DWM caption at the
 * transition midpoint, then deterministically completes the theme swap -- menu and control themes --
 * once the transition duration elapses.
 */
static FORCEINLINE void ThemeOnAnimationTick(void)
{
    HWND* phwnd;
    HWND  hwnd;
    UINT  c;
    DWORD dwDuration;
    DWORD dwElapsed;

    if (!g_theme.fAnimatingBackground)
    {
        ThemeStopAnimationTimer();
        return;
    }
    /* One clock read per tick, shared by the client and menu paints this tick. */
    g_theme.dwAnimationSnapTick = GetTickCount();

    phwnd = g_theme.rgAnimationWindows;
    c = g_theme.cAnimationWindows;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            /* Repaint the client SYNCHRONOUSLY every tick (UpdateWindow forces the WM_PAINT now)
               rather than leaving an async InvalidateRect to be coalesced -- otherwise the client
               crossfade lags the menu bar (which is redrawn synchronously below) by however long the
               paint sat in the queue, and the two surfaces drift out of step. */
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
            if (GetMenu(hwnd))
            {
                /* Drive the menu bar repaint every tick by SENDING WM_NCPAINT synchronously, not via
                   RedrawWindow(RDW_FRAME): the frame's own dirty tracking coalesces RDW_FRAME, so the
                   handler does not re-run every 5ms tick and the bar updates in visible stair-steps
                   (holding a shade for 8-12 composition frames while the client advances smoothly),
                   throwing the bar 10-14% off the caption's per-frame progress. SendMessage bypasses
                   the dirty-region gate, so the WM_NCPAINT handler -- which paints the bar off the
                   shared snapshot clock -- runs unconditionally each tick, pinning the bar to the
                   client's curve frame-for-frame. The paint stays in the handler's own stack frame,
                   not inlined into this timer path. wParam==1 means "whole window region". */
                SendMessage(hwnd, WM_NCPAINT, (WPARAM)1, 0);
            }
        }
        ++phwnd;
        --c;
    }

    dwDuration = ThemeBackgroundTransitionMs();
    dwElapsed = GetTickCount() - g_theme.dwAnimationStartTick;
    if ((0u == dwDuration) || (dwElapsed >= (dwDuration + THEME_ANIMATION_SLACK_MS)))
    {
        ThemeStopAnimationTimer();
        ThemeFinishAllBackgroundAnimation();
    }
}

static void CALLBACK ThemeAnimationTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(dwTime);
    if (THEME_ANIMATION_TIMER_ID == idEvent)
    {
        ThemeOnAnimationTick();
    }
}

static FORCEINLINE BOOL ThemePublishBroadcastPaintState(void)
{
    BOOL fRequestedDark;
    BOOL fEffectiveDark;
    BOOL fOldEffectiveDark;
    BOOL fStarted;

    ThemeRefresh();
    fOldEffectiveDark = g_theme.fEffectiveDark;
    fEffectiveDark = ThemeReadEffectiveDarkMode(&fRequestedDark);
    fStarted = ThemeStartBackgroundTransition(fOldEffectiveDark, fEffectiveDark);
    ThemeSetProcessDarkModeAllowed(TRUE);
    g_theme.fRequestedDark = fRequestedDark;
    g_theme.fEffectiveDark = fEffectiveDark;
    /* A theme switch re-broadcasts ImmersiveColorSet, so this runs more than once per switch. Only the
       call that actually (re)armed a transition owns the surfaces; a duplicate that armed nothing must
       not touch them. Re-flipping the caption, re-stamping the shared clock back to t=0, or
       re-invalidating mid-fade would repaint every crossfading surface (client AND menu, which read the
       same snapshot clock) at the from-color for a frame -- the visible flash. Leave the live fade be. */
    if (!fStarted)
    {
        return fEffectiveDark;
    }
    /* Flip the DWM caption attribute at the START of the transition, together with the class-brush
       flip, so DWM's own caption crossfade runs concurrently with the client/menu crossfade instead
       of starting late and lagging ~30 frames behind. Caption and client then animate in sync. */
    ThemeApplyRegisteredFrames(fEffectiveDark);
    /* Stamp the client/menu clock at the caption flip so both share t=0. (DwmFlush here was a
       mistake: it blocks a whole composition, starting our clock a frame LATE so the caption is
       already ~20% in before the group moves.) */
    if (g_theme.fAnimatingBackground)
    {
        g_theme.dwAnimationStartTick = GetTickCount();
        g_theme.dwAnimationSnapTick = g_theme.dwAnimationStartTick;
    }
    ThemeSetRegisteredClassBrushes(fEffectiveDark);
    ThemeInvalidateRegisteredWindows();
    return fEffectiveDark;
}

FORCEINLINE BOOL WINAPI ThemeIsDarkMode(void)
{
    ThemeResolve();
    return g_theme.fEffectiveDark;
}

FORCEINLINE void WINAPI ThemeStartup(void)
{
    BOOL fNeedsBufferedPaintInit;

    ThemeResolve();
    ThemeSetProcessDarkModeAllowed(TRUE);
    ThemeRefresh();
    fNeedsBufferedPaintInit = g_theme.pfnBufferedPaintInit && (!g_theme.fBufferedPaintInitialized);
    if (fNeedsBufferedPaintInit)
    {
        if (SUCCEEDED(g_theme.pfnBufferedPaintInit()))
        {
            g_theme.fBufferedPaintInitialized = TRUE;
        }
    }
    (void)ThemeEffectiveDarkMode();
}

FORCEINLINE HBRUSH WINAPI ThemeBackgroundBrush(BOOL fDark)
{
    return ThemeBackgroundBrushForEffectiveMode(fDark);
}

static FORCEINLINE void ThemeListAdd(HWND* prg, UINT* pc, UINT cMax, HWND hwnd)
{
    UINT i;

    if (!hwnd)
    {
        return;
    }
    for (i = 0u; i < *pc; ++i)
    {
        if (prg[i] == hwnd)
        {
            return;
        }
    }
    if (*pc < cMax)
    {
        prg[*pc] = hwnd;
        *pc = *pc + 1u;
    }
}

static FORCEINLINE void ThemeListRemove(HWND* prg, UINT* pc, HWND hwnd)
{
    UINT i;

    for (i = 0u; i < *pc; ++i)
    {
        if (prg[i] == hwnd)
        {
            while ((i + 1u) < *pc)
            {
                prg[i] = prg[i + 1u];
                ++i;
            }
            *pc = *pc - 1u;
            prg[*pc] = NULL;
            return;
        }
    }
}

FORCEINLINE void WINAPI ThemeRegisterWindow(HWND hwnd)
{
    ThemeResolve();
    ThemeListAdd(g_theme.rgTopLevels, &g_theme.cTopLevels, THEME_MAX_TOPLEVELS, hwnd);
}

FORCEINLINE void WINAPI ThemeUnregisterWindow(HWND hwnd)
{
    ThemeListRemove(g_theme.rgTopLevels, &g_theme.cTopLevels, hwnd);
}

FORCEINLINE void WINAPI ThemeRegisterDialog(HWND hwnd)
{
    ThemeResolve();
    ThemeListAdd(g_theme.rgDialogs, &g_theme.cDialogs, THEME_MAX_DIALOGS, hwnd);
}

FORCEINLINE void WINAPI ThemeUnregisterDialog(HWND hwnd)
{
    ThemeListRemove(g_theme.rgDialogs, &g_theme.cDialogs, hwnd);
}

FORCEINLINE void WINAPI ThemeApplyFrame(HWND hwnd, BOOL fDark)
{
    BOOL fAttr;
    BOOL fEffective;

    ThemeResolve();
    fEffective = fDark && g_theme.fDarkCapable;
    if (g_theme.pfnAllowDarkModeForWindow)
    {
        (void)g_theme.pfnAllowDarkModeForWindow(hwnd, fEffective);
    }
    if (g_theme.pfnDwmSetWindowAttribute)
    {
        fAttr = fEffective;
        (void)g_theme.pfnDwmSetWindowAttribute(
            hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &fAttr, (DWORD)sizeof(fAttr));
        (void)g_theme.pfnDwmSetWindowAttribute(
            hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, &fAttr, (DWORD)sizeof(fAttr));
    }
}

FORCEINLINE void WINAPI ThemeApplyWindow(HWND hwnd, BOOL fDark)
{
    ThemeApplyFrame(hwnd, fDark);
    if (g_theme.pfnFlushMenuThemes)
    {
        g_theme.pfnFlushMenuThemes();
    }
}

FORCEINLINE void WINAPI ThemeApplyTopLevel(HWND hwnd, BOOL fDark)
{
    BOOL fEffective;

    fEffective = fDark && ThemeCanUseDarkMode();
    g_theme.fRequestedDark = fDark;
    g_theme.fEffectiveDark = fEffective;
    ThemeRegisterWindow(hwnd);
    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)ThemeBackgroundBrush(fEffective));
    ThemeApplyWindow(hwnd, fEffective);
}

FORCEINLINE void WINAPI ThemeApplyDialog(HWND hwnd, BOOL fDark)
{
    BOOL    fEffective;

    fEffective = fDark && ThemeCanUseDarkMode();
    g_theme.fRequestedDark = fDark;
    g_theme.fEffectiveDark = fEffective;
    ThemeApplyFrame(hwnd, fEffective);
    ThemeApplyClientTheme(hwnd, fEffective);
}

FORCEINLINE void WINAPI ThemeApplyControl(HWND hCtl, BOOL fDark)
{
    ThemeApplyClientTheme(hCtl, fDark);
}

static BOOL CALLBACK ThemeApplyControlProc(HWND hChild, LPARAM lParam)
{
    ThemeApplyControl(hChild, 0 != lParam);
    return TRUE;
}

FORCEINLINE void WINAPI ThemeApplyDialogTree(HWND hwnd, BOOL fDark)
{
    BOOL fEffective;

    fEffective = fDark && ThemeCanUseDarkMode();
    ThemeRegisterDialog(hwnd);
    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)ThemeBackgroundBrush(fEffective));
    ThemeApplyDialog(hwnd, fEffective);
    EnumChildWindows(hwnd, ThemeApplyControlProc, (LPARAM)fEffective);
}

FORCEINLINE HBRUSH WINAPI ThemeCtlColorBrush(HDC hdc, BOOL fDark)
{
    BOOL fEffective;

    fEffective = fDark && ThemeCanUseDarkMode();
    if (!fEffective)
    {
        return NULL;
    }
    SetTextColor(hdc, DARK_TEXT);
    SetBkColor(hdc, ThemeBackgroundColor(TRUE));
    SetBkMode(hdc, OPAQUE);
    return ThemeBackgroundBrush(TRUE);
}

FORCEINLINE HBRUSH WINAPI ThemeCtlColor(HDC hdc, BOOL fDark)
{
    return ThemeCtlColorBrush(hdc, fDark);
}

static FORCEINLINE BOOL ThemeIsImmersiveColorSet(LPCTSTR pszSection);

static FORCEINLINE BOOL ThemeBeginBackgroundPaintAnimation(HWND hwnd, HDC hdc, const RECT* prc)
{
    BP_ANIMATIONPARAMS anim;
    HDC                hdcFrom;
    HDC                hdcTo;
    HANIMATIONBUFFER   hAnimation;
    DWORD              dwDuration;

    if (!ThemeWindowHasBackgroundAnimation(hwnd))
    {
        return FALSE;
    }

    dwDuration = ThemeBackgroundTransitionMs();
    if ((0u == dwDuration) || !ThemeCanPaintBackgroundAnimation())
    {
        ThemePaintBackgroundColor(hdc, prc, g_theme.fAnimationToDark);
        ThemeWindowFinishBackgroundAnimation(hwnd);
        return TRUE;
    }

    SecureZeroMemory(&anim, sizeof(anim));
    anim.cbSize = (DWORD)sizeof(anim);
    anim.style = BPAS_LINEAR;
    anim.dwDuration = dwDuration;
    hdcFrom = NULL;
    hdcTo = NULL;
    hAnimation = g_theme.pfnBeginBufferedAnimation(
        hwnd, hdc, prc, BPBF_COMPATIBLEBITMAP, NULL, &anim, &hdcFrom, &hdcTo);
    if (!hAnimation)
    {
        return FALSE;
    }
    if (hdcFrom)
    {
        ThemePaintBackgroundColor(hdcFrom, prc, g_theme.fAnimationFromDark);
    }
    if (hdcTo)
    {
        ThemePaintBackgroundColor(hdcTo, prc, g_theme.fAnimationToDark);
    }
    (void)g_theme.pfnEndBufferedAnimation(hAnimation, TRUE);
    ThemeWindowMarkBackgroundAnimationStarted(hwnd);
    if (g_theme.pfnBufferedPaintRenderAnimation(hwnd, hdc))
    {
        return TRUE;
    }
    return TRUE;
}

FORCEINLINE BOOL WINAPI ThemeEraseBackground(HWND hwnd, HDC hdc, BOOL fDark)
{
    RECT rc;
    BOOL fEffective;

    GetClientRect(hwnd, &rc);
    if (g_theme.fAnimatingBackground && ThemeWindowHasBackgroundAnimation(hwnd))
    {
        ThemePaintSolidColor(hdc, &rc, ThemeClientAnimationColor());
        return TRUE;
    }
    fEffective = fDark && ThemeCanUseDarkMode();
    ThemePaintBackgroundColor(hdc, &rc, fEffective);
    return TRUE;
}

/* The menu item text color, cross-faded on the same clock as the bar. */
static FORCEINLINE COLORREF ThemeMenuTextAnimationColor(void)
{
    MENUBAR_PALETTE palFrom;
    MENUBAR_PALETTE palTo;
    DWORD           dwDuration;
    DWORD           dwElapsed;

    MenuBarPalette(g_theme.fAnimationFromDark, &palFrom);
    MenuBarPalette(g_theme.fAnimationToDark, &palTo);
    dwDuration = ThemeBackgroundTransitionMs();
    dwElapsed = g_theme.dwAnimationSnapTick - g_theme.dwAnimationStartTick;
    return ThemeLerpColor(palFrom.clrText, palTo.clrText, ThemeEaseElapsed(dwElapsed, dwDuration), dwDuration);
}

static BOOL ThemeHandlePaintMessage(HWND hwnd, LRESULT* plr)
{
    PAINTSTRUCT ps;
    HDC         hdcPaint;
    RECT        rc;

    if (!g_theme.fAnimatingBackground || !ThemeWindowHasBackgroundAnimation(hwnd))
    {
        return FALSE;
    }
    hdcPaint = BeginPaint(hwnd, &ps);
    if (!hdcPaint)
    {
        return FALSE;
    }
    GetClientRect(hwnd, &rc);
    ThemePaintSolidColor(hdcPaint, &rc, ThemeClientAnimationColor());
    EndPaint(hwnd, &ps);
    *plr = 0;
    return TRUE;
}

/*
 * Process-lifetime cache of the menu font. SPI_GETNONCLIENTMETRICS fills a ~500-byte NONCLIENTMETRICS;
 * keeping that buffer (and the CreateFontIndirect/DeleteObject pair) out of ThemePaintMenuBarColor
 * matters twice over: the paint runs every 5ms animation tick (creating/destroying a font each tick
 * churns GDI handles), and a NONCLIENTMETRICS-sized frame on a function the timer path inlines pushes
 * past the one-page stack-probe threshold (__chkstk, which this no-CRT build cannot resolve).
 */
static HFONT g_hThemeMenuBarFont;

static NONCLIENTMETRICS g_themeNcmScratch;

static FORCEINLINE HFONT ThemeMenuBarFont(void)
{
    if (g_hThemeMenuBarFont)
    {
        return g_hThemeMenuBarFont;
    }
    /* g_themeNcmScratch is a file-static (not a stack local): a ~500-byte NONCLIENTMETRICS on the
       stack of a FORCEINLINE function would, once expanded into the timer path, push the frame past
       the one-page stack-probe threshold and emit __chkstk, which this /NODEFAULTLIB build cannot
       resolve. Keeping the buffer off-stack lets the font fetch inline cleanly. GUI thread only. */
    SecureZeroMemory(&g_themeNcmScratch, sizeof(g_themeNcmScratch));
    g_themeNcmScratch.cbSize = (DWORD)sizeof(g_themeNcmScratch);
    if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, (UINT)sizeof(g_themeNcmScratch), &g_themeNcmScratch, 0))
    {
        g_hThemeMenuBarFont = CreateFontIndirect(&g_themeNcmScratch.lfMenuFont);
    }
    return g_hThemeMenuBarFont;
}

/*
 * Paint the whole owner-drawn menu bar -- background fill plus each top-level label -- directly
 * through the window DC at the supplied cross-fade colors (adzm's UAHDrawMenuNCBottomLine technique:
 * GetWindowDC over the non-client menu band). WM_UAHDRAWMENU, which DefWindowProc drives, does NOT
 * re-fire on every animation tick, so a UAH-only bar holds a stale paint -- one computed against the
 * pre-caption-flip start tick, before that tick was re-stamped forward -- and reads ~30% ahead of the
 * client. Painting it here, every tick, off the same snapshot clock the client uses, pins the bar to
 * the client's progress so every surface stays inside the caption's band.
 */
static FORCEINLINE void ThemePaintMenuBarColor(HWND hwnd, COLORREF crBar, COLORREF crText)
{
    MENUBARINFO      mbi;
    RECT             rcWindow;
    RECT             rcBar;
    RECT             rcItem;
    HDC              hdc;
    HMENU            hMenu;
    HFONT            hFont;
    HGDIOBJ          hOldFont;
    WCHAR            szText[64];
    int              i;
    int              cch;

    hMenu = GetMenu(hwnd);
    if (!hMenu)
    {
        return;
    }
    SecureZeroMemory(&mbi, sizeof(mbi));
    mbi.cbSize = (DWORD)sizeof(mbi);
    if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
    {
        return;
    }
    GetWindowRect(hwnd, &rcWindow);
    rcBar = mbi.rcBar;
    OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top);
    rcBar.left = 0;
    rcBar.right = rcWindow.right - rcWindow.left;
    rcBar.bottom = rcBar.bottom + 1;
    hdc = GetWindowDC(hwnd);
    if (!hdc)
    {
        return;
    }
    ThemePaintSolidColor(hdc, &rcBar, crBar);

    /* The fill erased the item labels DefWindowProc drew; redraw each top-level label over it in the
       menu font at the interpolated text color so "File"/"Help" stay visible and cross-fade with the
       bar instead of vanishing until the swap completes. */
    hFont = ThemeMenuBarFont();
    hOldFont = hFont ? SelectObject(hdc, hFont) : NULL;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, crText);
    /* Constant loop bound (a bounded pre-tested loop is C5045-safe); GetMenuItemRect fails past the
       real top-level item count, so the body simply skips. DrawTextW is called unconditionally -- a
       zero-length GetMenuStringW result draws nothing -- so no comparison-checked value feeds a call. */
    for (i = 0; i < THEME_MAX_MENU_ITEMS; ++i)
    {
        if (!GetMenuItemRect(hwnd, hMenu, (UINT)i, &rcItem))
        {
            continue;
        }
        OffsetRect(&rcItem, -rcWindow.left, -rcWindow.top);
        szText[0] = 0;
        cch = GetMenuStringW(hMenu, (UINT)i, szText, (int)ARRAYSIZE(szText) - 1, MF_BYPOSITION);
        (void)DrawTextW(hdc, szText, cch, &rcItem,
                        DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_HIDEPREFIX);
    }
    if (hOldFont)
    {
        SelectObject(hdc, hOldFont);
    }
    /* hFont is the process-lifetime cached menu font (ThemeMenuBarFont); not deleted here. */
    ReleaseDC(hwnd, hdc);
}

/*
 * Non-client repaint of the owner-drawn menu bar, the DwmDefWindowProc way: the caller lets
 * DefWindowProc render the frame first, then this repaints the menu band. While the background is
 * cross-fading we paint the whole bar ourselves off the shared snapshot clock (so it tracks the client
 * to the frame, not the stale WM_UAHDRAWMENU cadence); otherwise the system's WM_UAHDRAWMENU has filled
 * the bar and we only restamp the 1px seam DefWindowProc drew in the stock shade.
 */
static FORCEINLINE void ThemeNcRepaintMenuBar(HWND hwnd)
{
    MENUBAR_PALETTE pal;

    if (!GetMenu(hwnd))
    {
        return;
    }
    if (g_theme.fAnimatingBackground && ThemeWindowHasBackgroundAnimation(hwnd))
    {
        ThemePaintMenuBarColor(hwnd, ThemeMenuAnimationColor(), ThemeMenuTextAnimationColor());
        return;
    }
    MenuBarPalette(ThemeIsDarkMode(), &pal);
    MenuBarPaintSeam(hwnd, &pal);
}

static BOOL ThemeHandleSettingChangeMessage(HWND hwnd, LPARAM lParam, UINT uDeferredMsg, LRESULT* plr)
{
    if (!ThemeIsImmersiveColorSet((LPCTSTR)lParam))
    {
        return FALSE;
    }
    (void)ThemeOnSettingChange(hwnd, uDeferredMsg, (LPCTSTR)lParam);
    *plr = 0;
    return TRUE;
}

static BOOL ThemeHandleDeferredMessage(LRESULT* plr)
{
    (void)ThemeOnDeferredThemeChange();
    *plr = 0;
    return TRUE;
}

BOOL WINAPI ThemeHandleWindowMessage(
    HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT uDeferredMsg, LRESULT* plr)
{
    BOOL fDark;

    if (!plr)
    {
        return FALSE;
    }
    switch (uMsg)
    {
        case WM_PAINT:
            return ThemeHandlePaintMessage(hwnd, plr);

        case WM_TIMER:
            if (THEME_ANIMATION_TIMER_ID == wParam)
            {
                ThemeOnAnimationTick();
                *plr = 0;
                return TRUE;
            }
            break;

        case WM_SETTINGCHANGE:
            return ThemeHandleSettingChangeMessage(hwnd, lParam, uDeferredMsg, plr);

        case WM_NCACTIVATE:
        case WM_NCPAINT:
            /* DwmDefWindowProc-style: let DefWindowProc render the frame, then repaint the owner-drawn
               menu band over it (full bar while cross-fading, otherwise the 1px seam). The literal
               message id -- not the switch-narrowed uMsg -- feeds DefWindowProc so no range-checked
               value reaches it (C5045-safe). */
            if (WM_NCACTIVATE == uMsg)
            {
                *plr = DefWindowProc(hwnd, WM_NCACTIVATE, wParam, lParam);
            }
            else
            {
                *plr = DefWindowProc(hwnd, WM_NCPAINT, wParam, lParam);
            }
            ThemeNcRepaintMenuBar(hwnd);
            return TRUE;

        case WM_ERASEBKGND:
            *plr = (LRESULT)ThemeEraseBackground(hwnd, (HDC)wParam, ThemeIsDarkMode());
            return TRUE;

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
            fDark = ThemeIsDarkMode();
            if (fDark)
            {
                *plr = (LRESULT)ThemeCtlColorBrush((HDC)wParam, fDark);
                return TRUE;
            }
            break;

        default:
            if (uMsg == uDeferredMsg)
            {
                return ThemeHandleDeferredMessage(plr);
            }
            break;
    }
    return FALSE;
}
static FORCEINLINE BOOL ThemeIsImmersiveColorSet(LPCTSTR pszSection)
{
    BOOL fHasSection;
    BOOL fIsImmersiveColorSet;

    fHasSection = IsNonNull(pszSection);
    fIsImmersiveColorSet = FALSE;
    if (fHasSection)
    {
        fIsImmersiveColorSet = (0 == lstrcmpi(pszSection, TEXT("ImmersiveColorSet")));
    }
    return fIsImmersiveColorSet;
}

FORCEINLINE BOOL WINAPI ThemeOnThemeBroadcast(LPCTSTR pszSection)
{
    if (!ThemeIsImmersiveColorSet(pszSection))
    {
        return FALSE;
    }
    ThemeResolve();
    (void)ThemePublishBroadcastPaintState();
    return TRUE;
}

FORCEINLINE BOOL WINAPI ThemeOnSettingChange(HWND hwndPost, UINT uDeferredMsg, LPCTSTR pszSection)
{
    if (!ThemeOnThemeBroadcast(pszSection))
    {
        return FALSE;
    }
    if (!g_theme.fPendingThemeChange)
    {
        g_theme.fPendingThemeChange = TRUE;
        (void)PostMessage(hwndPost, uDeferredMsg, 0, 0);
    }
    return TRUE;
}

FORCEINLINE BOOL WINAPI ThemeOnDeferredThemeChange(void)
{
    BOOL  fNewEffective;
    BOOL  fNewRequested;
    BOOL  fHadPendingThemeChange;

    ThemeResolve();
    fHadPendingThemeChange = g_theme.fPendingThemeChange;
    g_theme.fPendingThemeChange = FALSE;

    ThemeRefresh();
    fNewEffective = ThemeReadEffectiveDarkMode(&fNewRequested);
    g_theme.fRequestedDark = fNewRequested;
    g_theme.fEffectiveDark = fNewEffective;
    if (g_theme.fAnimatingBackground)
    {
        g_theme.fPendingThemeChange = fHadPendingThemeChange;
        return fNewEffective;
    }
    ThemeSetRegisteredClassBrushes(fNewEffective);
    if (fHadPendingThemeChange)
    {
        g_theme.fPendingThemeChange = TRUE;
        ThemeCompletePendingThemeChange();
    }
    return fNewEffective;
}

FORCEINLINE void WINAPI ThemeDiagnostics(THEME_DIAGNOSTICS* pDiag)
{
    if (!pDiag)
    {
        return;
    }
    ThemeResolve();
    pDiag->dwMajorVersion      = g_theme.dwMajorVersion;
    pDiag->dwMinorVersion      = g_theme.dwMinorVersion;
    pDiag->dwBuildNumber       = g_theme.dwBuildNumber;
    pDiag->policy              = g_theme.policy;
    pDiag->fDarkCapable        = g_theme.fDarkCapable;
    pDiag->fRequestedDark      = g_theme.fRequestedDark;
    pDiag->fEffectiveDark      = g_theme.fEffectiveDark;
    pDiag->fPendingThemeChange = g_theme.fPendingThemeChange;
    pDiag->cTopLevels          = g_theme.cTopLevels;
    pDiag->cDialogs            = g_theme.cDialogs;
}

FORCEINLINE void WINAPI MenuBarPalette(BOOL fDark, MENUBAR_PALETTE* pPalette)
{
    BOOL fEffective;

    fEffective = fDark && ThemeCanUseDarkMode();
    if (fEffective)
    {
        pPalette->clrBar        = DARK_BG;
        pPalette->clrText       = DARK_TEXT;
        pPalette->clrTextDim    = DARK_TEXT_DIM;
        pPalette->clrItemHot    = DARK_BG_HOT;
        pPalette->clrItemPushed = DARK_BG_PUSHED;
    }
    else
    {
        /* Light, ACTIVE chrome: the owner-drawn aero menu bar matches the window/caption surface
           (COLOR_WINDOW, ~white), not the legacy COLOR_MENUBAR (~#F0/medium gray) which renders the
           bar visibly darker than the active light caption and client it sits between. */
        pPalette->clrBar        = GetSysColor(COLOR_WINDOW);
        pPalette->clrText       = GetSysColor(COLOR_MENUTEXT);
        pPalette->clrTextDim    = GetSysColor(COLOR_GRAYTEXT);
        pPalette->clrItemHot    = GetSysColor(COLOR_MENUHILIGHT);
        pPalette->clrItemPushed = GetSysColor(COLOR_HIGHLIGHT);
    }
}

/*
 * A window can host only one buffered-paint animation at a time, and the client background already
 * owns it, so the menu bar cannot run its own BeginBufferedAnimation on the same hwnd. Instead the
 * menu crossfades by interpolating its bar color along the same transition clock the client uses,
 * which keeps the two in sync without contending for the buffered-animation slot.
 */
static FORCEINLINE COLORREF ThemeMenuAnimationColor(void)
{
    MENUBAR_PALETTE palFrom;
    MENUBAR_PALETTE palTo;
    DWORD           dwDuration;
    DWORD           dwElapsed;

    MenuBarPalette(g_theme.fAnimationFromDark, &palFrom);
    MenuBarPalette(g_theme.fAnimationToDark, &palTo);
    dwDuration = ThemeBackgroundTransitionMs();
    dwElapsed = g_theme.dwAnimationSnapTick - g_theme.dwAnimationStartTick;
    return ThemeLerpColor(palFrom.clrBar, palTo.clrBar, ThemeEaseElapsed(dwElapsed, dwDuration), dwDuration);
}

FORCEINLINE void WINAPI MenuBarOnDrawMenu(HWND hwnd, const UAHMENU* pUDM, const MENUBAR_PALETTE* pPalette)
{
    MENUBARINFO mbi;
    RECT        rcWindow;
    RECT        rcBar;

    SecureZeroMemory(&mbi, sizeof(mbi));
    mbi.cbSize = sizeof(mbi);
    if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
    {
        return;
    }
    GetWindowRect(hwnd, &rcWindow);
    rcBar = mbi.rcBar;
    OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top);
    rcBar.left = 0;
    rcBar.right = rcWindow.right - rcWindow.left;
    rcBar.bottom = rcBar.bottom + 1;
    if (g_theme.fAnimatingBackground)
    {
        ThemeWindowMarkMenuAnimationStarted(hwnd);
        ThemePaintSolidColor(pUDM->hdc, &rcBar, ThemeMenuAnimationColor());
        return;
    }
    ThemePaintSolidColor(pUDM->hdc, &rcBar, pPalette->clrBar);
}

FORCEINLINE void WINAPI MenuBarOnDrawMenuItem(HWND                        hwnd,
                                                  const UAHDRAWMENUITEM*      pUDMI,
                                                  const MENUBAR_PALETTE* pPalette)
{
    WCHAR         szText[64];
    MENUITEMINFOW mii;
    RECT          rcItem;
    COLORREF      clrBg;
    COLORREF      clrText;
    UINT          uFormat;
    BOOL          fHot;
    BOOL          fPushed;
    BOOL          fDisabled;
    BOOL          fNoAccel;
    BOOL          fCanDrawThemeText;

    fHot      = !!(ODS_HOTLIGHT & pUDMI->dis.itemState);
    fPushed   = !!(ODS_SELECTED & pUDMI->dis.itemState);
    fDisabled = !!((ODS_GRAYED | ODS_DISABLED) & pUDMI->dis.itemState);
    fNoAccel  = !!(ODS_NOACCEL & pUDMI->dis.itemState);

    clrBg   = pPalette->clrBar;
    clrText = pPalette->clrText;
    uFormat = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
    if (fHot)
    {
        clrBg = pPalette->clrItemHot;
    }
    if (fPushed)
    {
        clrBg = pPalette->clrItemPushed;
    }
    if (fDisabled)
    {
        clrText = pPalette->clrTextDim;
    }
    if (fNoAccel)
    {
        uFormat |= DT_HIDEPREFIX;
    }
    if (g_theme.fAnimatingBackground)
    {
        /* Mid cross-fade: draw the item background and its label at the interpolated colors -- on the
           same clock as the bar -- so "File"/"Help" stay visible and fade with it instead of being
           skipped (which left a blank animated bar) and snapping back at the end. No hover/pushed
           state participates during the fade; draw every item in its normal state. */
        clrBg   = ThemeMenuAnimationColor();
        clrText = ThemeMenuTextAnimationColor();
        fHot = FALSE;
        fPushed = FALSE;
        fDisabled = FALSE;
    }

    szText[0] = 0;
    SecureZeroMemory(&mii, sizeof(mii));
    mii.cbSize     = sizeof(mii);
    mii.fMask      = MIIM_STRING;
    mii.dwTypeData = szText;
    mii.cch        = (UINT)(ARRAYSIZE(szText) - 1);
    (void)GetMenuItemInfoW(pUDMI->um.hmenu, (UINT)pUDMI->umi.iPosition, TRUE, &mii);

    rcItem = pUDMI->dis.rcItem;
    ThemePaintSolidColor(pUDMI->um.hdc, &rcItem, clrBg);

    fCanDrawThemeText = g_theme.pfnOpenThemeData && g_theme.pfnDrawThemeTextEx;
    if (fCanDrawThemeText)
    {
        HTHEME hTheme;

        hTheme = g_theme.pfnOpenThemeData(hwnd, L"Menu");
        if (hTheme)
        {
            DTTOPTS opts;
            int          iState;

            iState = MBI_NORMAL;
            if (fHot)
            {
                iState = MBI_HOT;
            }
            if (fPushed)
            {
                iState = MBI_PUSHED;
            }
            if (fDisabled)
            {
                iState = MBI_DISABLED;
            }
            SecureZeroMemory(&opts, sizeof(opts));
            opts.dwSize  = (DWORD)sizeof(opts);
            opts.dwFlags = DTT_TEXTCOLOR;
            opts.crText  = clrText;
            (void)g_theme.pfnDrawThemeTextEx(hTheme, pUDMI->um.hdc, MENU_BARITEM, iState,
                                                 szText, -1, uFormat, &rcItem, &opts);
            g_theme.pfnCloseThemeData(hTheme);
            return;
        }
    }

    SetBkMode(pUDMI->um.hdc, TRANSPARENT);
    SetTextColor(pUDMI->um.hdc, clrText);
    DrawTextW(pUDMI->um.hdc, szText, -1, &rcItem, uFormat);
}

static FORCEINLINE void MenuBarPaintSeamToHdcEx(HWND hwnd, HDC hdc, const MENUBAR_PALETTE* pPalette, int dx, int dy)
{
    MENUBARINFO mbi;
    RECT        rcClient;
    RECT        rcWindow;
    RECT        rcLine;

    SecureZeroMemory(&mbi, sizeof(mbi));
    mbi.cbSize = sizeof(mbi);
    if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
    {
        return;
    }
    GetClientRect(hwnd, &rcClient);
    MapWindowPoints(hwnd, NULL, (POINT*)&rcClient, 2);
    GetWindowRect(hwnd, &rcWindow);
    OffsetRect(&rcClient, -rcWindow.left, -rcWindow.top);
    OffsetRect(&rcClient, dx, dy);

    rcLine        = rcClient;
    rcLine.bottom = rcLine.top;
    rcLine.top    = rcLine.top - 1;

    ThemePaintSolidColor(hdc, &rcLine, pPalette->clrBar);
}

static FORCEINLINE void MenuBarPaintSeamToHdc(HWND hwnd, HDC hdc, const MENUBAR_PALETTE* pPalette)
{
    MenuBarPaintSeamToHdcEx(hwnd, hdc, pPalette, 0, 0);
}

FORCEINLINE void WINAPI MenuBarPaintSeam(HWND hwnd, const MENUBAR_PALETTE* pPalette)
{
    HDC hdc;

    hdc = GetWindowDC(hwnd);
    if (!hdc)
    {
        return;
    }
    if (!g_theme.fAnimatingBackground)
    {
        MenuBarPaintSeamToHdc(hwnd, hdc, pPalette);
    }
    ReleaseDC(hwnd, hdc);
}

typedef BOOL (WINAPI* PFN_THEMECANUSEDARKMODE)(void);
static PFN_THEMECANUSEDARKMODE volatile pfnKeepThemeCanUseDarkMode = ThemeCanUseDarkMode;
typedef void (WINAPI* PFN_THEMEREFRESH)(void);
static PFN_THEMEREFRESH volatile pfnKeepThemeRefresh = ThemeRefresh;
typedef BOOL (WINAPI* PFN_THEMEAPPSUSEDARKMODE)(void);
static PFN_THEMEAPPSUSEDARKMODE volatile pfnKeepThemeAppsUseDarkMode = ThemeAppsUseDarkMode;
typedef BOOL (WINAPI* PFN_THEMEEFFECTIVEDARKMODE)(void);
static PFN_THEMEEFFECTIVEDARKMODE volatile pfnKeepThemeEffectiveDarkMode = ThemeEffectiveDarkMode;
typedef BOOL (WINAPI* PFN_THEMEISDARKMODE)(void);
static PFN_THEMEISDARKMODE volatile pfnKeepThemeIsDarkMode = ThemeIsDarkMode;
typedef void (WINAPI* PFN_THEMESTARTUP)(void);
static PFN_THEMESTARTUP volatile pfnKeepThemeStartup = ThemeStartup;
typedef HBRUSH (WINAPI* PFN_THEMEBACKGROUNDBRUSH)(BOOL fDark);
static PFN_THEMEBACKGROUNDBRUSH volatile pfnKeepThemeBackgroundBrush = ThemeBackgroundBrush;
typedef void (WINAPI* PFN_THEMEREGISTERWINDOW)(HWND hwnd);
static PFN_THEMEREGISTERWINDOW volatile pfnKeepThemeRegisterWindow = ThemeRegisterWindow;
typedef void (WINAPI* PFN_THEMEUNREGISTERWINDOW)(HWND hwnd);
static PFN_THEMEUNREGISTERWINDOW volatile pfnKeepThemeUnregisterWindow = ThemeUnregisterWindow;
typedef void (WINAPI* PFN_THEMEREGISTERDIALOG)(HWND hwnd);
static PFN_THEMEREGISTERDIALOG volatile pfnKeepThemeRegisterDialog = ThemeRegisterDialog;
typedef void (WINAPI* PFN_THEMEUNREGISTERDIALOG)(HWND hwnd);
static PFN_THEMEUNREGISTERDIALOG volatile pfnKeepThemeUnregisterDialog = ThemeUnregisterDialog;
typedef void (WINAPI* PFN_THEMEAPPLYFRAME)(HWND hwnd, BOOL fDark);
static PFN_THEMEAPPLYFRAME volatile pfnKeepThemeApplyFrame = ThemeApplyFrame;
typedef void (WINAPI* PFN_THEMEAPPLYWINDOW)(HWND hwnd, BOOL fDark);
static PFN_THEMEAPPLYWINDOW volatile pfnKeepThemeApplyWindow = ThemeApplyWindow;
typedef void (WINAPI* PFN_THEMEAPPLYTOPLEVEL)(HWND hwnd, BOOL fDark);
static PFN_THEMEAPPLYTOPLEVEL volatile pfnKeepThemeApplyTopLevel = ThemeApplyTopLevel;
typedef void (WINAPI* PFN_THEMEAPPLYDIALOG)(HWND hwnd, BOOL fDark);
static PFN_THEMEAPPLYDIALOG volatile pfnKeepThemeApplyDialog = ThemeApplyDialog;
typedef void (WINAPI* PFN_THEMEAPPLYCONTROL)(HWND hCtl, BOOL fDark);
static PFN_THEMEAPPLYCONTROL volatile pfnKeepThemeApplyControl = ThemeApplyControl;
typedef void (WINAPI* PFN_THEMEAPPLYDIALOGTREE)(HWND hwnd, BOOL fDark);
static PFN_THEMEAPPLYDIALOGTREE volatile pfnKeepThemeApplyDialogTree = ThemeApplyDialogTree;
typedef HBRUSH (WINAPI* PFN_THEMECTLCOLORBRUSH)(HDC hdc, BOOL fDark);
static PFN_THEMECTLCOLORBRUSH volatile pfnKeepThemeCtlColorBrush = ThemeCtlColorBrush;
typedef HBRUSH (WINAPI* PFN_THEMECTLCOLOR)(HDC hdc, BOOL fDark);
static PFN_THEMECTLCOLOR volatile pfnKeepThemeCtlColor = ThemeCtlColor;
typedef BOOL (WINAPI* PFN_THEMEERASEBACKGROUND)(HWND hwnd, HDC hdc, BOOL fDark);
static PFN_THEMEERASEBACKGROUND volatile pfnKeepThemeEraseBackground = ThemeEraseBackground;
typedef BOOL (WINAPI* PFN_THEMEHANDLEWINDOWMESSAGE)(
    HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT uDeferredMsg, LRESULT* plr);
static PFN_THEMEHANDLEWINDOWMESSAGE volatile pfnKeepThemeHandleWindowMessage = ThemeHandleWindowMessage;
typedef BOOL (WINAPI* PFN_THEMEONTHEMEBROADCAST)(LPCTSTR pszSection);
static PFN_THEMEONTHEMEBROADCAST volatile pfnKeepThemeOnThemeBroadcast = ThemeOnThemeBroadcast;
typedef BOOL (WINAPI* PFN_THEMEONSETTINGCHANGE)(HWND hwndPost, UINT uDeferredMsg, LPCTSTR pszSection);
static PFN_THEMEONSETTINGCHANGE volatile pfnKeepThemeOnSettingChange = ThemeOnSettingChange;
typedef BOOL (WINAPI* PFN_THEMEONDEFERREDTHEMECHANGE)(void);
static PFN_THEMEONDEFERREDTHEMECHANGE volatile pfnKeepThemeOnDeferredThemeChange = ThemeOnDeferredThemeChange;
typedef void (WINAPI* PFN_THEMEDIAGNOSTICS)(THEME_DIAGNOSTICS* pDiag);
static PFN_THEMEDIAGNOSTICS volatile pfnKeepThemeDiagnostics = ThemeDiagnostics;
typedef void (WINAPI* PFN_MENUBARPALETTE)(BOOL fDark, MENUBAR_PALETTE* pPalette);
static PFN_MENUBARPALETTE volatile pfnKeepMenuBarPalette = MenuBarPalette;
typedef void (WINAPI* PFN_MENUBARONDRAWMENU)(HWND hwnd, const UAHMENU* pUDM, const MENUBAR_PALETTE* pPalette);
static PFN_MENUBARONDRAWMENU volatile pfnKeepMenuBarOnDrawMenu = MenuBarOnDrawMenu;
typedef void (WINAPI* PFN_MENUBARONDRAWMENUITEM)(HWND hwnd, const UAHDRAWMENUITEM* pUDMI, const MENUBAR_PALETTE* pPalette);
static PFN_MENUBARONDRAWMENUITEM volatile pfnKeepMenuBarOnDrawMenuItem = MenuBarOnDrawMenuItem;
typedef void (WINAPI* PFN_MENUBARPAINTSEAM)(HWND hwnd, const MENUBAR_PALETTE* pPalette);
static PFN_MENUBARPAINTSEAM volatile pfnKeepMenuBarPaintSeam = MenuBarPaintSeam;
