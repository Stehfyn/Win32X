/*
 * uxthemex.h -- CRT-free dark-mode / theming helpers for Win32X clients.
 *
 * The important rule is that "dark mode" is not one Windows API. It is a versioned runtime contract:
 * old Windows versions have no app dark mode, Windows 10 1809 exposes one undocumented uxtheme ABI,
 * Windows 10 1903+ exposes a different ABI at the same ordinal, and DWM/non-client painting is a
 * separate documented-but-versioned attribute. This header classifies that contract once, resolves
 * every optional function dynamically, and only reports dark mode effective when the required pieces
 * are present for the running build.
 */
#ifndef UXTHEMEX_H
#define UXTHEMEX_H

#include <windows.h>
#include "windefx.h"
#include "versionhelpersx.h"
#include "windowsx2.h"

/* ---- palette ------------------------------------------------------------------------------- */
#define DARK_BG            RGB(32, 32, 32)
#define DARK_BG_HOT        RGB(64, 64, 64)
#define DARK_BG_PUSHED     RGB(80, 80, 80)
#define DARK_TEXT          RGB(240, 240, 240)
#define DARK_TEXT_DIM      RGB(150, 150, 150)

/* DWM frame attribute ids. Attribute 19 was used by early 1809 builds; 20 is the current id. */
#define DWMWA_USE_IMMERSIVE_DARK_MODE      20
#define DWMWA_USE_IMMERSIVE_DARK_MODE_OLD  19

/* Windows 10 dark-mode build boundaries. */
#define BUILD_WIN10_1809 17763u
#define BUILD_WIN10_1903 18362u
#define BUILD_WIN11     22000u

/* uxtheme.dll dark-mode ordinals. They are intentionally build-gated below. */
#define UXTHEME_ORD_REFRESH_IMMERSIVE      104
#define UXTHEME_ORD_SHOULD_APPS_USE_DARK   132
#define UXTHEME_ORD_ALLOW_DARK_FOR_WINDOW  133
#define UXTHEME_ORD_APP_MODE_135           135
#define UXTHEME_ORD_FLUSH_MENU_THEMES      136

#define THEME_MAX_TOPLEVELS 16u
#define THEME_MAX_DIALOGS   16u

typedef enum PREFERRED_APP_MODE
{
    AppModeDefault    = 0,
    AppModeAllowDark  = 1,
    AppModeForceDark  = 2,
    AppModeForceLight = 3
} PREFERRED_APP_MODE;

typedef struct MENUBAR_PALETTE
{
    COLORREF clrBar;
    COLORREF clrText;
    COLORREF clrTextDim;
    COLORREF clrItemHot;
    COLORREF clrItemPushed;
} MENUBAR_PALETTE;

typedef enum THEME_OS_POLICY
{
    ThemePolicyClassic = 0,  /* no immersive dark app contract */
    ThemePolicyWin10_1809,
    ThemePolicyWin10_1903Plus
} THEME_OS_POLICY;

typedef struct THEME_DIAGNOSTICS
{
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    THEME_OS_POLICY policy;
    BOOL  fDarkCapable;
    BOOL  fRequestedDark;
    BOOL  fEffectiveDark;
    BOOL  fPendingThemeChange;
    UINT  cTopLevels;
    UINT  cDialogs;
} THEME_DIAGNOSTICS;

typedef HRESULT(WINAPI* PFN_DWMSETWINDOWATTRIBUTE)(HWND, DWORD, LPCVOID, DWORD);
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
#define BPBF_COMPATIBLEBITMAP 0

typedef HANDLE HTHEME;
typedef HTHEME(WINAPI* PFN_OPENTHEMEDATA)(HWND, LPCWSTR);
typedef HRESULT(WINAPI* PFN_CLOSETHEMEDATA)(HTHEME);
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
#define DTT_TEXTCOLOR 0x00000001

typedef struct THEME_STATE
{
    PFN_DWMSETWINDOWATTRIBUTE            pfnDwmSetWindowAttribute;
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
    PFN_OPENTHEMEDATA                    pfnOpenThemeData;
    PFN_CLOSETHEMEDATA                   pfnCloseThemeData;
    PFN_DRAWTHEMETEXTEX                  pfnDrawThemeTextEx;
    HMODULE                                   hUxtheme;
    HMODULE                                   hDwmapi;
    HMODULE                                   hAdvapi32;
    HBRUSH                                    hbrDarkBg;
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
    HWND                                      rgTopLevels[THEME_MAX_TOPLEVELS];
    HWND                                      rgDialogs[THEME_MAX_DIALOGS];
    UINT                                      cTopLevels;
    UINT                                      cDialogs;
} THEME_STATE;

extern THEME_STATE g_theme;

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
    union { FARPROC fp; PFN_DWMSETWINDOWATTRIBUTE pfn; }             uDwm;
    union { FARPROC fp; PFN_SETWINDOWTHEME pfn; }                    uTheme;
    union { FARPROC fp; PFN_ALLOWDARKMODEFORWINDOW pfn; }            uAllowWindow;
    union { FARPROC fp; PFN_ALLOWDARKMODEFORAPP_1809 pfn; }          uAllowApp;
    union { FARPROC fp; PFN_SETPREFERREDAPPMODE pfn; }               uAppMode;
    union { FARPROC fp; PFN_SHOULDAPPSUSEDARKMODE pfn; }             uShould;
    union { FARPROC fp; PFN_FLUSHMENUTHEMES pfn; }                   uFlush;
    union { FARPROC fp; PFN_REFRESHIMMERSIVECOLORPOLICYSTATE pfn; }  uRefresh;
    union { FARPROC fp; PFN_REGOPENKEYEXW pfn; }                     uRegOpen;
    union { FARPROC fp; PFN_REGQUERYVALUEEXW pfn; }                  uRegQuery;
    union { FARPROC fp; PFN_REGCLOSEKEY pfn; }                       uRegClose;
    union { FARPROC fp; PFN_BEGINBUFFEREDPAINT pfn; }                uBegin;
    union { FARPROC fp; PFN_ENDBUFFEREDPAINT pfn; }                  uEnd;
    union { FARPROC fp; PFN_BUFFEREDPAINTINIT pfn; }                 uInit;
    union { FARPROC fp; PFN_OPENTHEMEDATA pfn; }                     uOpen;
    union { FARPROC fp; PFN_CLOSETHEMEDATA pfn; }                    uClose;
    union { FARPROC fp; PFN_DRAWTHEMETEXTEX pfn; }                   uDtt;

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

    if (IsNonNull(g_theme.hAdvapi32))
    {
        uRegOpen.fp  = GetProcAddress(g_theme.hAdvapi32, "RegOpenKeyExW");
        uRegQuery.fp = GetProcAddress(g_theme.hAdvapi32, "RegQueryValueExW");
        uRegClose.fp = GetProcAddress(g_theme.hAdvapi32, "RegCloseKey");
        g_theme.pfnRegOpenKeyExW    = uRegOpen.pfn;
        g_theme.pfnRegQueryValueExW = uRegQuery.pfn;
        g_theme.pfnRegCloseKey      = uRegClose.pfn;
    }

    if (IsNonNull(g_theme.hUxtheme))
    {
        uTheme.fp       = GetProcAddress(g_theme.hUxtheme, "SetWindowTheme");
        uAllowWindow.fp = GetProcAddress(g_theme.hUxtheme, MAKEINTRESOURCEA(UXTHEME_ORD_ALLOW_DARK_FOR_WINDOW));
        uShould.fp      = GetProcAddress(g_theme.hUxtheme, MAKEINTRESOURCEA(UXTHEME_ORD_SHOULD_APPS_USE_DARK));
        uFlush.fp       = GetProcAddress(g_theme.hUxtheme, MAKEINTRESOURCEA(UXTHEME_ORD_FLUSH_MENU_THEMES));
        uRefresh.fp     = GetProcAddress(g_theme.hUxtheme, MAKEINTRESOURCEA(UXTHEME_ORD_REFRESH_IMMERSIVE));
        g_theme.pfnSetWindowTheme                   = uTheme.pfn;
        g_theme.pfnAllowDarkModeForWindow           = uAllowWindow.pfn;
        g_theme.pfnShouldAppsUseDarkMode            = uShould.pfn;
        g_theme.pfnFlushMenuThemes                  = uFlush.pfn;
        g_theme.pfnRefreshImmersiveColorPolicyState = uRefresh.pfn;

        if (IsEqual(ThemePolicyWin10_1809, g_theme.policy))
        {
            uAllowApp.fp = GetProcAddress(g_theme.hUxtheme, MAKEINTRESOURCEA(UXTHEME_ORD_APP_MODE_135));
            g_theme.pfnAllowDarkModeForApp1809 = uAllowApp.pfn;
        }
        else if (IsEqual(ThemePolicyWin10_1903Plus, g_theme.policy))
        {
            uAppMode.fp = GetProcAddress(g_theme.hUxtheme, MAKEINTRESOURCEA(UXTHEME_ORD_APP_MODE_135));
            g_theme.pfnSetPreferredAppMode = uAppMode.pfn;
        }

        uBegin.fp = GetProcAddress(g_theme.hUxtheme, "BeginBufferedPaint");
        uEnd.fp   = GetProcAddress(g_theme.hUxtheme, "EndBufferedPaint");
        uInit.fp  = GetProcAddress(g_theme.hUxtheme, "BufferedPaintInit");
        uOpen.fp  = GetProcAddress(g_theme.hUxtheme, "OpenThemeData");
        uClose.fp = GetProcAddress(g_theme.hUxtheme, "CloseThemeData");
        uDtt.fp   = GetProcAddress(g_theme.hUxtheme, "DrawThemeTextEx");
        g_theme.pfnBeginBufferedPaint = uBegin.pfn;
        g_theme.pfnEndBufferedPaint   = uEnd.pfn;
        g_theme.pfnBufferedPaintInit  = uInit.pfn;
        g_theme.pfnOpenThemeData      = uOpen.pfn;
        g_theme.pfnCloseThemeData     = uClose.pfn;
        g_theme.pfnDrawThemeTextEx    = uDtt.pfn;
    }

    if (IsNonNull(g_theme.hDwmapi))
    {
        uDwm.fp = GetProcAddress(g_theme.hDwmapi, "DwmSetWindowAttribute");
        g_theme.pfnDwmSetWindowAttribute = uDwm.pfn;
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

static FORCEINLINE BOOL ThemeCanUseDarkMode(void)
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

static FORCEINLINE void ThemeRefresh(void)
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

static FORCEINLINE BOOL ThemeAppsUseDarkMode(void)
{
    BOOL fDark;

    fDark = FALSE;
    if (ThemeReadPersonalizeDarkMode(&fDark))
    {
        return fDark;
    }
    ThemeResolve();
    if (g_theme.pfnShouldAppsUseDarkMode)
    {
        return g_theme.pfnShouldAppsUseDarkMode();
    }
    return FALSE;
}

static FORCEINLINE BOOL ThemeEffectiveDarkMode(void)
{
    ThemeResolve();
    g_theme.fRequestedDark = ThemeAppsUseDarkMode();
    g_theme.fEffectiveDark = g_theme.fRequestedDark && g_theme.fDarkCapable;
    return g_theme.fEffectiveDark;
}

static FORCEINLINE BOOL ThemeIsDarkMode(void)
{
    ThemeResolve();
    return g_theme.fEffectiveDark;
}

static FORCEINLINE void ThemeStartup(void)
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

static FORCEINLINE HBRUSH ThemeBackgroundBrush(BOOL fDark)
{
    if (!fDark)
    {
        return (HBRUSH)(COLOR_WINDOW + 1);
    }
    if (!g_theme.hbrDarkBg)
    {
        g_theme.hbrDarkBg = CreateSolidBrush(DARK_BG);
    }
    return g_theme.hbrDarkBg;
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

static FORCEINLINE void ThemeRegisterWindow(HWND hwnd)
{
    ThemeResolve();
    ThemeListAdd(g_theme.rgTopLevels, &g_theme.cTopLevels, THEME_MAX_TOPLEVELS, hwnd);
}

static FORCEINLINE void ThemeUnregisterWindow(HWND hwnd)
{
    ThemeListRemove(g_theme.rgTopLevels, &g_theme.cTopLevels, hwnd);
}

static FORCEINLINE void ThemeRegisterDialog(HWND hwnd)
{
    ThemeResolve();
    ThemeListAdd(g_theme.rgDialogs, &g_theme.cDialogs, THEME_MAX_DIALOGS, hwnd);
}

static FORCEINLINE void ThemeUnregisterDialog(HWND hwnd)
{
    ThemeListRemove(g_theme.rgDialogs, &g_theme.cDialogs, hwnd);
}

static FORCEINLINE void ThemeRedrawRegistered(void)
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
            RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
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
            RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
        }
        ++phwnd;
        --c;
    }
}

static FORCEINLINE void ThemeApplyFrame(HWND hwnd, BOOL fDark)
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

static FORCEINLINE void ThemeApplyWindow(HWND hwnd, BOOL fDark)
{
    ThemeApplyFrame(hwnd, fDark);
    if (g_theme.pfnFlushMenuThemes)
    {
        g_theme.pfnFlushMenuThemes();
    }
}

static FORCEINLINE void ThemeApplyTopLevel(HWND hwnd, BOOL fDark)
{
    BOOL fEffective;

    fEffective = fDark && ThemeCanUseDarkMode();
    ThemeRegisterWindow(hwnd);
    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)ThemeBackgroundBrush(fEffective));
    ThemeApplyWindow(hwnd, fEffective);
}

static FORCEINLINE void ThemeApplyDialog(HWND hwnd, BOOL fDark)
{
    BOOL    fEffective;
    LPCWSTR pszSubAppName;

    fEffective = fDark && ThemeCanUseDarkMode();
    pszSubAppName = L"Explorer";
    if (fEffective)
    {
        pszSubAppName = L"DarkMode_Explorer";
    }
    ThemeApplyFrame(hwnd, fEffective);
    if (g_theme.pfnSetWindowTheme)
    {
        (void)g_theme.pfnSetWindowTheme(hwnd, pszSubAppName, NULL);
    }
}

static FORCEINLINE void ThemeApplyControl(HWND hCtl, BOOL fDark)
{
    BOOL    fEffective;
    LPCWSTR pszSubAppName;

    fEffective = fDark && ThemeCanUseDarkMode();
    pszSubAppName = L"Explorer";
    if (fEffective)
    {
        pszSubAppName = L"DarkMode_Explorer";
    }
    if (g_theme.pfnAllowDarkModeForWindow)
    {
        (void)g_theme.pfnAllowDarkModeForWindow(hCtl, fEffective);
    }
    if (g_theme.pfnSetWindowTheme)
    {
        (void)g_theme.pfnSetWindowTheme(hCtl, pszSubAppName, NULL);
    }
}

static BOOL CALLBACK ThemeApplyControlProc(HWND hChild, LPARAM lParam)
{
    ThemeApplyControl(hChild, 0 != lParam);
    return TRUE;
}

static FORCEINLINE void ThemeApplyDialogTree(HWND hwnd, BOOL fDark)
{
    BOOL fEffective;

    fEffective = fDark && ThemeCanUseDarkMode();
    ThemeRegisterDialog(hwnd);
    ThemeApplyDialog(hwnd, fEffective);
    EnumChildWindows(hwnd, ThemeApplyControlProc, (LPARAM)fEffective);
}

static FORCEINLINE HBRUSH ThemeCtlColorBrush(HDC hdc, BOOL fDark)
{
    BOOL fEffective;

    fEffective = fDark && ThemeCanUseDarkMode();
    if (!fEffective)
    {
        return NULL;
    }
    SetTextColor(hdc, DARK_TEXT);
    SetBkColor(hdc, DARK_BG);
    SetBkMode(hdc, OPAQUE);
    return ThemeBackgroundBrush(TRUE);
}

static FORCEINLINE HBRUSH ThemeCtlColor(HDC hdc, BOOL fDark)
{
    return ThemeCtlColorBrush(hdc, fDark);
}

static FORCEINLINE BOOL ThemeEraseBackground(HWND hwnd, HDC hdc, BOOL fDark)
{
    RECT              rc;
    RECT              rcChild;
    HWND              hChild;
    HBRUSH            hbr;
    HDC               hdcBuf;
    HPAINTBUFFER hpb;
    BOOL              fEffective;
    BOOL              fCanBuffer;
    BOOL              fHasBuffer;

    fEffective = fDark && ThemeCanUseDarkMode();
    GetClientRect(hwnd, &rc);

    hChild = GetWindow(hwnd, GW_CHILD);
    while (hChild)
    {
        if (IsWindowVisible(hChild))
        {
            GetWindowRect(hChild, &rcChild);
            MapWindowPoints(NULL, hwnd, (POINT*)&rcChild, 2);
            ExcludeClipRect(hdc, rcChild.left, rcChild.top, rcChild.right, rcChild.bottom);
        }
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }

    hbr = GetSysColorBrush(COLOR_3DFACE);
    if (fEffective)
    {
        hbr = ThemeBackgroundBrush(TRUE);
    }
    hdcBuf = NULL;
    hpb    = NULL;
    fCanBuffer = g_theme.pfnBeginBufferedPaint && g_theme.pfnEndBufferedPaint;
    if (fCanBuffer)
    {
        hpb = g_theme.pfnBeginBufferedPaint(hdc, &rc, BPBF_COMPATIBLEBITMAP, NULL, &hdcBuf);
    }
    fHasBuffer = hpb && hdcBuf;
    if (fHasBuffer)
    {
        FillRect(hdcBuf, &rc, hbr);
        (void)g_theme.pfnEndBufferedPaint(hpb, TRUE);
    }
    else
    {
        FillRect(hdc, &rc, hbr);
    }
    return TRUE;
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

static FORCEINLINE BOOL ThemeOnThemeBroadcast(LPCTSTR pszSection)
{
    if (!ThemeIsImmersiveColorSet(pszSection))
    {
        return FALSE;
    }
    ThemeResolve();
    return TRUE;
}

static FORCEINLINE BOOL ThemeOnSettingChange(HWND hwndPost, UINT uDeferredMsg, LPCTSTR pszSection)
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

static FORCEINLINE BOOL ThemeOnDeferredThemeChange(void)
{
    HWND* phwnd;
    HWND  hwnd;
    UINT  c;
    BOOL  fOldEffective;
    BOOL  fNewEffective;

    ThemeResolve();
    g_theme.fPendingThemeChange = FALSE;
    fOldEffective = g_theme.fEffectiveDark;

    ThemeRefresh();
    fNewEffective = ThemeEffectiveDarkMode();
    if (fNewEffective == fOldEffective)
    {
        ThemeRedrawRegistered();
        return fNewEffective;
    }

    ThemeSetProcessDarkModeAllowed(TRUE);
    ThemeRefresh();

    phwnd = g_theme.rgTopLevels;
    c = g_theme.cTopLevels;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)ThemeBackgroundBrush(fNewEffective));
            ThemeApplyWindow(hwnd, fNewEffective);
            SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            DrawMenuBar(hwnd);
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
            ThemeApplyDialog(hwnd, fNewEffective);
            EnumChildWindows(hwnd, ThemeApplyControlProc, (LPARAM)fNewEffective);
        }
        ++phwnd;
        --c;
    }

    ThemeRedrawRegistered();
    return fNewEffective;
}

static FORCEINLINE void ThemeDiagnostics(THEME_DIAGNOSTICS* pDiag)
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

static FORCEINLINE void MenuBarPalette(BOOL fDark, MENUBAR_PALETTE* pPalette)
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
        pPalette->clrBar        = GetSysColor(COLOR_MENUBAR);
        pPalette->clrText       = GetSysColor(COLOR_MENUTEXT);
        pPalette->clrTextDim    = GetSysColor(COLOR_GRAYTEXT);
        pPalette->clrItemHot    = GetSysColor(COLOR_MENUHILIGHT);
        pPalette->clrItemPushed = GetSysColor(COLOR_HIGHLIGHT);
    }
}

static FORCEINLINE void MenuBarOnDrawMenu(HWND hwnd, const UAHMENU* pUDM, const MENUBAR_PALETTE* pPalette)
{
    MENUBARINFO mbi;
    RECT        rcWindow;
    RECT        rcBar;
    HBRUSH      hbr;

    SecureZeroMemory(&mbi, sizeof(mbi));
    mbi.cbSize = sizeof(mbi);
    if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
    {
        return;
    }
    GetWindowRect(hwnd, &rcWindow);
    rcBar = mbi.rcBar;
    OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top);

    hbr = CreateSolidBrush(pPalette->clrBar);
    if (hbr)
    {
        FillRect(pUDM->hdc, &rcBar, hbr);
        DeleteObject(hbr);
    }
}

static FORCEINLINE void MenuBarOnDrawMenuItem(HWND                        hwnd,
                                                  const UAHDRAWMENUITEM*      pUDMI,
                                                  const MENUBAR_PALETTE* pPalette)
{
    WCHAR         szText[128];
    MENUITEMINFOW mii;
    RECT          rcItem;
    HBRUSH        hbr;
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

    szText[0] = 0;
    SecureZeroMemory(&mii, sizeof(mii));
    mii.cbSize     = sizeof(mii);
    mii.fMask      = MIIM_STRING;
    mii.dwTypeData = szText;
    mii.cch        = (UINT)(ARRAYSIZE(szText) - 1);
    (void)GetMenuItemInfoW(pUDMI->um.hmenu, (UINT)pUDMI->umi.iPosition, TRUE, &mii);

    rcItem = pUDMI->dis.rcItem;
    hbr    = CreateSolidBrush(clrBg);
    if (hbr)
    {
        FillRect(pUDMI->um.hdc, &rcItem, hbr);
        DeleteObject(hbr);
    }

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

static FORCEINLINE void MenuBarPaintSeam(HWND hwnd, const MENUBAR_PALETTE* pPalette)
{
    MENUBARINFO mbi;
    RECT        rcClient;
    RECT        rcWindow;
    RECT        rcLine;
    HBRUSH      hbr;
    HDC         hdc;

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

    rcLine        = rcClient;
    rcLine.bottom = rcLine.top;
    rcLine.top    = rcLine.top - 1;

    hdc = GetWindowDC(hwnd);
    if (!hdc)
    {
        return;
    }
    hbr = CreateSolidBrush(pPalette->clrBar);
    if (hbr)
    {
        FillRect(hdc, &rcLine, hbr);
        DeleteObject(hbr);
    }
    ReleaseDC(hwnd, hdc);
}

#endif /* UXTHEMEX_H */
