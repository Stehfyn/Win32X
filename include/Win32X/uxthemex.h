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

/* Custom non-client frame (DWM custom-frame technique, learn.microsoft.com/windows/win32/dwm/customframe).
   A window opted in with ThemeEnableCustomFrame removes the standard NC frame (WM_NCCALCSIZE), extends the
   DWM frame for the shadow + resize borders, and owner-draws the caption, the four caption buttons, and --
   because the NC menu band and its UAH draw messages die with the standard frame -- the menu bar in the
   client. HTLIGHTDARK is the private WM_NCHITTEST result for the fourth (light/dark) caption button, which
   sits directly left of Minimize, sized and sequenced exactly like the other three. */
#define HTLIGHTDARK   0x0000B001
#define HTMENUITEM0   0x0000C000   /* WM_NCHITTEST result for in-client menu top-level item i = HTMENUITEM0 + i */

typedef enum THEME_OS_POLICY
{
    ThemePolicyClassic = 0,
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

#ifdef __cplusplus
extern "C"
{
#endif

BOOL   WINAPI ThemeCanUseDarkMode(void);
void   WINAPI ThemeRefresh(void);
BOOL   WINAPI ThemeAppsUseDarkMode(void);
BOOL   WINAPI ThemeEffectiveDarkMode(void);
BOOL   WINAPI ThemeIsDarkMode(void);
void   WINAPI ThemeStartup(void);
HBRUSH WINAPI ThemeBackgroundBrush(BOOL fDark);
void   WINAPI ThemeRegisterWindow(HWND hwnd);
void   WINAPI ThemeUnregisterWindow(HWND hwnd);
void   WINAPI ThemeRegisterDialog(HWND hwnd);
void   WINAPI ThemeUnregisterDialog(HWND hwnd);
void   WINAPI ThemeApplyFrame(HWND hwnd, BOOL fDark);
void   WINAPI ThemeApplyWindow(HWND hwnd, BOOL fDark);
void   WINAPI ThemeApplyTopLevel(HWND hwnd, BOOL fDark);
void   WINAPI ThemeApplyDialog(HWND hwnd, BOOL fDark);
void   WINAPI ThemeApplyControl(HWND hCtl, BOOL fDark);
void   WINAPI ThemeApplyDialogTree(HWND hwnd, BOOL fDark);
HBRUSH WINAPI ThemeCtlColorBrush(HDC hdc, BOOL fDark);
HBRUSH WINAPI ThemeCtlColor(HDC hdc, BOOL fDark);
BOOL   WINAPI ThemeEraseBackground(HWND hwnd, HDC hdc, BOOL fDark);
BOOL   WINAPI ThemeHandleWindowMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT uDeferredMsg, LRESULT* plr);
BOOL   WINAPI ThemeOnThemeBroadcast(LPCTSTR pszSection);
BOOL   WINAPI ThemeOnSettingChange(HWND hwndPost, UINT uDeferredMsg, LPCTSTR pszSection);
BOOL   WINAPI ThemeOnDeferredThemeChange(void);
void   WINAPI ThemeDiagnostics(THEME_DIAGNOSTICS* pDiag);
void   WINAPI MenuBarPalette(BOOL fDark, MENUBAR_PALETTE* pPalette);
void   WINAPI MenuBarOnDrawMenu(HWND hwnd, const UAHMENU* pUDM, const MENUBAR_PALETTE* pPalette);
void   WINAPI MenuBarOnDrawMenuItem(HWND hwnd, const UAHDRAWMENUITEM* pUDMI, const MENUBAR_PALETTE* pPalette);
void   WINAPI MenuBarPaintSeam(HWND hwnd, const MENUBAR_PALETTE* pPalette);
void   WINAPI ThemeEnableCustomFrame(HWND hwnd, BOOL fEnable);
BOOL   WINAPI ThemeCustomFrameHandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plr);
void   WINAPI ThemeToggleDarkMode(HWND hwnd);

#ifdef __cplusplus
}
#endif

#endif /* UXTHEMEX_H */
