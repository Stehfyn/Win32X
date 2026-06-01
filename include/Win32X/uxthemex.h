/*
 * uxthemex.h -- minimal, CRT-free dark-mode / theming helpers for Win32X clients.
 *
 * Windows' app dark mode lives behind a wall of undocumented uxtheme.dll ordinals plus a documented
 * DWM attribute. This header resolves them dynamically (no uxtheme.lib / dwmapi.lib import, nothing
 * the loader pulls a CRT through) and exposes a small, intention-named surface:
 *
 *   WinXThemeStartup()            -- once, early: opt the process into dark mode (PreferredAppMode).
 *   WinXThemeAppsUseDarkMode()    -- read the live system setting (HKCU Personalize\AppsUseLightTheme).
 *   WinXThemeApplyWindow()        -- dark/light a top-level window (frame title bar + menu theme).
 *   WinXThemeApplyDialog()        -- dark/light a dialog (DarkMode_Explorer control theme + frame).
 *   WinXThemeBackgroundBrush()    -- the brush to hang on the window class (GCLP_HBRBACKGROUND).
 *   WinXThemeCtlColorBrush()      -- WM_CTLCOLOR* reply: set dark text/bk on the HDC, return the brush.
 *   WinXMenuBarPalette() + WinXMenuBarOnDrawMenu()/OnDrawMenuItem()/PaintSeam()
 *                                 -- owner-draw a top-level menu bar dark, for the UAH messages
 *                                    cracked in windowsx2.h.
 *
 * State (resolved pointers, cached brush) is file-local: include this in exactly the translation unit
 * that themes the UI. The functions are static FORCEINLINE.
 */
#ifndef UXTHEMEX_H
#define UXTHEMEX_H

#include <windows.h>
#include "windowsx2.h"   /* UAHMENU / UAHDRAWMENUITEM for the menu-bar drawing helpers */

/* ---- dark palette (Windows 10/11 app-dark surfaces) ------------------------------------------- */
#define WINX_DARK_BG            RGB(32, 32, 32)
#define WINX_DARK_BG_HOT        RGB(64, 64, 64)
#define WINX_DARK_BG_PUSHED     RGB(80, 80, 80)
#define WINX_DARK_TEXT          RGB(240, 240, 240)
#define WINX_DARK_TEXT_DIM      RGB(150, 150, 150)

/* DWM frame attribute (documented, but the constant value shifted: 19 on early 1809, 20 since). */
#define WINX_DWMWA_USE_IMMERSIVE_DARK_MODE      20
#define WINX_DWMWA_USE_IMMERSIVE_DARK_MODE_OLD  19

/* uxtheme.dll dark-mode ordinals (undocumented, stable Win10 1809+ .. Win11). */
#define WINX_UXTHEME_ORD_REFRESH_IMMERSIVE      104
#define WINX_UXTHEME_ORD_ALLOW_DARK_FOR_WINDOW  133
#define WINX_UXTHEME_ORD_SET_PREFERRED_APP_MODE 135
#define WINX_UXTHEME_ORD_FLUSH_MENU_THEMES      136

typedef enum WINX_PREFERRED_APP_MODE
{
    WinXAppModeDefault   = 0,
    WinXAppModeAllowDark = 1,
    WinXAppModeForceDark = 2,
    WinXAppModeForceLight = 3
} WINX_PREFERRED_APP_MODE;

typedef struct WINX_MENUBAR_PALETTE
{
    COLORREF clrBar;        /* bar background + the 1px client seam */
    COLORREF clrText;       /* item text                           */
    COLORREF clrTextDim;    /* grayed/disabled item text           */
    COLORREF clrItemHot;    /* hot (hovered) item background        */
    COLORREF clrItemPushed; /* pressed/open item background         */
} WINX_MENUBAR_PALETTE;

typedef HRESULT(WINAPI* WINX_PFN_DWMSETWINDOWATTRIBUTE)(HWND, DWORD, LPCVOID, DWORD);
typedef HRESULT(WINAPI* WINX_PFN_SETWINDOWTHEME)(HWND, LPCWSTR, LPCWSTR);
typedef BOOL(WINAPI* WINX_PFN_ALLOWDARKMODEFORWINDOW)(HWND, BOOL);
typedef WINX_PREFERRED_APP_MODE(WINAPI* WINX_PFN_SETPREFERREDAPPMODE)(WINX_PREFERRED_APP_MODE);
typedef void(WINAPI* WINX_PFN_FLUSHMENUTHEMES)(void);
typedef void(WINAPI* WINX_PFN_REFRESHIMMERSIVECOLORPOLICYSTATE)(void);

typedef struct WINX_THEME_STATE
{
    WINX_PFN_DWMSETWINDOWATTRIBUTE            pfnDwmSetWindowAttribute;
    WINX_PFN_SETWINDOWTHEME                   pfnSetWindowTheme;
    WINX_PFN_ALLOWDARKMODEFORWINDOW           pfnAllowDarkModeForWindow;
    WINX_PFN_SETPREFERREDAPPMODE              pfnSetPreferredAppMode;
    WINX_PFN_FLUSHMENUTHEMES                  pfnFlushMenuThemes;
    WINX_PFN_REFRESHIMMERSIVECOLORPOLICYSTATE pfnRefreshImmersiveColorPolicyState;
    HMODULE                                   hUxtheme;
    HMODULE                                   hDwmapi;
    HBRUSH                                    hbrDarkBg;
    BOOL                                      fResolved;
    BOOL                                      _reserved; /* explicit tail pad */
} WINX_THEME_STATE;

static WINX_THEME_STATE g_winxTheme;

/* Resolve uxtheme + dwmapi entry points once. The unions launder FARPROC -> typed pointer without a
   function-pointer cast (avoids /Wall C4191). Idempotent. */
static FORCEINLINE void WinXThemeResolve(void)
{
    union { FARPROC fp; WINX_PFN_DWMSETWINDOWATTRIBUTE pfn; }            uDwm;
    union { FARPROC fp; WINX_PFN_SETWINDOWTHEME pfn; }                   uTheme;
    union { FARPROC fp; WINX_PFN_ALLOWDARKMODEFORWINDOW pfn; }           uAllow;
    union { FARPROC fp; WINX_PFN_SETPREFERREDAPPMODE pfn; }              uMode;
    union { FARPROC fp; WINX_PFN_FLUSHMENUTHEMES pfn; }                  uFlush;
    union { FARPROC fp; WINX_PFN_REFRESHIMMERSIVECOLORPOLICYSTATE pfn; } uRefresh;

    if (g_winxTheme.fResolved)
    {
        return;
    }
    g_winxTheme.fResolved = TRUE;

    g_winxTheme.hUxtheme = LoadLibraryEx(TEXT("uxtheme.dll"), NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    g_winxTheme.hDwmapi  = LoadLibraryEx(TEXT("dwmapi.dll"), NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (g_winxTheme.hUxtheme)
    {
        uTheme.fp   = GetProcAddress(g_winxTheme.hUxtheme, "SetWindowTheme");
        uAllow.fp   = GetProcAddress(g_winxTheme.hUxtheme, MAKEINTRESOURCEA(WINX_UXTHEME_ORD_ALLOW_DARK_FOR_WINDOW));
        uMode.fp    = GetProcAddress(g_winxTheme.hUxtheme, MAKEINTRESOURCEA(WINX_UXTHEME_ORD_SET_PREFERRED_APP_MODE));
        uFlush.fp   = GetProcAddress(g_winxTheme.hUxtheme, MAKEINTRESOURCEA(WINX_UXTHEME_ORD_FLUSH_MENU_THEMES));
        uRefresh.fp = GetProcAddress(g_winxTheme.hUxtheme, MAKEINTRESOURCEA(WINX_UXTHEME_ORD_REFRESH_IMMERSIVE));
        g_winxTheme.pfnSetWindowTheme                   = uTheme.pfn;
        g_winxTheme.pfnAllowDarkModeForWindow           = uAllow.pfn;
        g_winxTheme.pfnSetPreferredAppMode              = uMode.pfn;
        g_winxTheme.pfnFlushMenuThemes                  = uFlush.pfn;
        g_winxTheme.pfnRefreshImmersiveColorPolicyState = uRefresh.pfn;
    }
    if (g_winxTheme.hDwmapi)
    {
        uDwm.fp = GetProcAddress(g_winxTheme.hDwmapi, "DwmSetWindowAttribute");
        g_winxTheme.pfnDwmSetWindowAttribute = uDwm.pfn;
    }
}

/* Opt the whole process into dark mode and pre-resolve. Call once, before creating windows. */
static FORCEINLINE void WinXThemeStartup(void)
{
    WinXThemeResolve();
    if (g_winxTheme.pfnSetPreferredAppMode)
    {
        (void)g_winxTheme.pfnSetPreferredAppMode(WinXAppModeAllowDark);
    }
    if (g_winxTheme.pfnRefreshImmersiveColorPolicyState)
    {
        g_winxTheme.pfnRefreshImmersiveColorPolicyState();
    }
}

/* Re-read the immersive color policy into uxtheme's cached state. ShouldAppsUseDarkMode -- which the
   common controls consult when they repaint -- reads that cache, NOT the registry, so without this
   the controls paint the OLD shade on a theme flip and then correct (a visible flicker). Call once on
   each transition, before re-theming, so the very first repaint is already the new shade. */
static FORCEINLINE void WinXThemeRefresh(void)
{
    WinXThemeResolve();
    if (g_winxTheme.pfnRefreshImmersiveColorPolicyState)
    {
        g_winxTheme.pfnRefreshImmersiveColorPolicyState();
    }
}

/* The live system app theme: TRUE when apps should render dark. */
static FORCEINLINE BOOL WinXThemeAppsUseDarkMode(void)
{
    DWORD dwValue;
    DWORD cb;
    BOOL  fDark;

    dwValue = 1; /* default: light */
    cb      = (DWORD)sizeof(dwValue);
    fDark   = FALSE;
    if (ERROR_SUCCESS == RegGetValue(HKEY_CURRENT_USER,
                                     TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
                                     TEXT("AppsUseLightTheme"),
                                     RRF_RT_REG_DWORD,
                                     NULL,
                                     &dwValue,
                                     &cb))
    {
        fDark = (0 == dwValue);
    }
    return fDark;
}

static FORCEINLINE void WinXThemeApplyFrame(HWND hwnd, BOOL fDark)
{
    BOOL fAttr;

    WinXThemeResolve();
    if (g_winxTheme.pfnAllowDarkModeForWindow)
    {
        (void)g_winxTheme.pfnAllowDarkModeForWindow(hwnd, fDark);
    }
    if (g_winxTheme.pfnDwmSetWindowAttribute)
    {
        fAttr = fDark;
        /* Set the current (20) and legacy (19) attribute ids unconditionally -- whichever the OS
           understands takes effect, the other is a harmless no-op. No FAILED() branch gates the
           second call (CONV-4.12: a checked HRESULT must not be the only proof for a later call). */
        (void)g_winxTheme.pfnDwmSetWindowAttribute(
            hwnd, WINX_DWMWA_USE_IMMERSIVE_DARK_MODE, &fAttr, (DWORD)sizeof(fAttr));
        (void)g_winxTheme.pfnDwmSetWindowAttribute(
            hwnd, WINX_DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, &fAttr, (DWORD)sizeof(fAttr));
    }
}

/* Top-level window: dark/light the frame title bar + re-theme its menu. */
static FORCEINLINE void WinXThemeApplyWindow(HWND hwnd, BOOL fDark)
{
    WinXThemeApplyFrame(hwnd, fDark);
    if (g_winxTheme.pfnFlushMenuThemes)
    {
        g_winxTheme.pfnFlushMenuThemes();
    }
}

/* Dialog: dark/light the frame + route its controls through the DarkMode_Explorer visual style. */
static FORCEINLINE void WinXThemeApplyDialog(HWND hwnd, BOOL fDark)
{
    WinXThemeApplyFrame(hwnd, fDark);
    if (g_winxTheme.pfnSetWindowTheme)
    {
        (void)g_winxTheme.pfnSetWindowTheme(hwnd, fDark ? L"DarkMode_Explorer" : L"Explorer", NULL);
    }
}

/* Route a single child control through the dark/light visual style (DarkMode_Explorer vs Explorer).
   Push buttons, check boxes, etc. paint their own face and ignore WM_CTLCOLOR* for it -- this is what
   actually makes them dark. Call for each child (e.g. from an EnumChildWindows callback). */
static FORCEINLINE void WinXThemeApplyControl(HWND hCtl, BOOL fDark)
{
    WinXThemeResolve();
    if (g_winxTheme.pfnAllowDarkModeForWindow)
    {
        (void)g_winxTheme.pfnAllowDarkModeForWindow(hCtl, fDark);
    }
    if (g_winxTheme.pfnSetWindowTheme)
    {
        (void)g_winxTheme.pfnSetWindowTheme(hCtl, fDark ? L"DarkMode_Explorer" : L"Explorer", NULL);
    }
}

/* The class background brush for the theme. The dark brush is cached/owned here (never DeleteObject
   it); the light one is a system color brush (also not to be deleted). */
static FORCEINLINE HBRUSH WinXThemeBackgroundBrush(BOOL fDark)
{
    if (!fDark)
    {
        return (HBRUSH)(COLOR_WINDOW + 1);
    }
    if (!g_winxTheme.hbrDarkBg)
    {
        g_winxTheme.hbrDarkBg = CreateSolidBrush(WINX_DARK_BG);
    }
    return g_winxTheme.hbrDarkBg;
}

/* WM_CTLCOLORDLG / WM_CTLCOLORSTATIC / WM_CTLCOLORBTN reply for a dark dialog: paints text light on a
   dark background and returns the dark brush. (For light mode, return NULL and let DefDlgProc run.) */
static FORCEINLINE HBRUSH WinXThemeCtlColorBrush(HDC hdc, BOOL fDark)
{
    if (!fDark)
    {
        return NULL;
    }
    SetTextColor(hdc, WINX_DARK_TEXT);
    SetBkColor(hdc, WINX_DARK_BG);
    SetBkMode(hdc, OPAQUE);
    return WinXThemeBackgroundBrush(TRUE);
}

/* Fill a menu-bar palette for the theme. */
static FORCEINLINE void WinXMenuBarPalette(BOOL fDark, WINX_MENUBAR_PALETTE* pPalette)
{
    if (fDark)
    {
        pPalette->clrBar        = WINX_DARK_BG;
        pPalette->clrText       = WINX_DARK_TEXT;
        pPalette->clrTextDim    = WINX_DARK_TEXT_DIM;
        pPalette->clrItemHot    = WINX_DARK_BG_HOT;
        pPalette->clrItemPushed = WINX_DARK_BG_PUSHED;
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

/* WM_UAHDRAWMENU: fill the whole menu-bar background. */
static FORCEINLINE void WinXMenuBarOnDrawMenu(HWND hwnd, const UAHMENU* pUDM, const WINX_MENUBAR_PALETTE* pPalette)
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
    OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top); /* bar rect is screen-space; hdc is window-space */

    hbr = CreateSolidBrush(pPalette->clrBar);
    if (hbr)
    {
        FillRect(pUDM->hdc, &rcBar, hbr);
        DeleteObject(hbr);
    }
}

/* WM_UAHDRAWMENUITEM: paint one top-level bar item (background by state, then centered text). */
static FORCEINLINE void WinXMenuBarOnDrawMenuItem(HWND                        hwnd,
                                                  const UAHDRAWMENUITEM*      pUDMI,
                                                  const WINX_MENUBAR_PALETTE* pPalette)
{
    WCHAR        szText[128];
    MENUITEMINFOW mii;
    RECT         rcItem;
    HBRUSH       hbr;
    COLORREF     clrBg;
    COLORREF     clrText;
    UINT         uFormat;
    BOOL         fHot;
    BOOL         fPushed;
    BOOL         fDisabled;

    UNREFERENCED_PARAMETER(hwnd);

    fHot      = (0 != (pUDMI->dis.itemState & ODS_HOTLIGHT));
    fPushed   = (0 != (pUDMI->dis.itemState & ODS_SELECTED));
    fDisabled = (0 != (pUDMI->dis.itemState & (ODS_GRAYED | ODS_DISABLED)));

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
    if (0 != (pUDMI->dis.itemState & ODS_NOACCEL))
    {
        uFormat |= DT_HIDEPREFIX;
    }

    szText[0] = 0;
    SecureZeroMemory(&mii, sizeof(mii));
    mii.cbSize     = sizeof(mii);
    mii.fMask      = MIIM_STRING;
    mii.dwTypeData = szText;
    mii.cch        = (UINT)(ARRAYSIZE(szText) - 1); /* capacity; constant bound -> C5045-safe */
    (void)GetMenuItemInfoW(pUDMI->um.hmenu, (UINT)pUDMI->umi.iPosition, TRUE, &mii);

    rcItem = pUDMI->dis.rcItem;
    hbr    = CreateSolidBrush(clrBg);
    if (hbr)
    {
        FillRect(pUDMI->um.hdc, &rcItem, hbr);
        DeleteObject(hbr);
    }

    SetBkMode(pUDMI->um.hdc, TRANSPARENT);
    SetTextColor(pUDMI->um.hdc, clrText);
    /* -1: szText is nul-terminated by GetMenuItemInfo, so no checked length feeds the call. */
    DrawTextW(pUDMI->um.hdc, szText, -1, &rcItem, uFormat);
}

/* The 1px seam between the menu bar and the client area. DefWindowProc paints it light on WM_NCPAINT
   / WM_NCACTIVATE; call this afterward (with the bar color) to keep it dark. */
static FORCEINLINE void WinXMenuBarPaintSeam(HWND hwnd, const WINX_MENUBAR_PALETTE* pPalette)
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
