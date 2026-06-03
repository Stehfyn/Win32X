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
#define DTT_GLOWSIZE  0x00000800

/* ---- custom non-client frame (DWM custom-frame technique) ------------------------------------ */
typedef struct THEME_MARGINS  /* layout-identical to uxtheme/dwmapi MARGINS (not included here) */
{
    int cxLeftWidth;
    int cxRightWidth;
    int cyTopHeight;
    int cyBottomHeight;
} THEME_MARGINS;
typedef HRESULT(WINAPI* PFN_DWMEXTENDFRAMEINTOCLIENTAREA)(HWND, const THEME_MARGINS*);
typedef BOOL(WINAPI* PFN_DWMDEFWINDOWPROC)(HWND, UINT, WPARAM, LPARAM, LRESULT*);
typedef HRESULT(WINAPI* PFN_DRAWTHEMEBACKGROUND)(HTHEME, HDC, int, int, const RECT*, const RECT*);
typedef HRESULT(WINAPI* PFN_GETTHEMEPARTSIZE)(HTHEME, HDC, int, int, const RECT*, int, SIZE*);
typedef UINT(WINAPI* PFN_GETDPIFORWINDOW)(HWND);
typedef int(WINAPI* PFN_GETSYSTEMMETRICSFORDPI)(int, UINT);

/* uxtheme "WINDOW" class parts/states used to render the caption buttons the way DefWindowProc's themed
   legacy caption path does (evidence: uxtheme.disasm.txt; the renderer DWM falls back to). */
#define WP_CAPTION        1
#define WP_MINBUTTON      15
#define WP_MAXBUTTON      17
#define WP_CLOSEBUTTON    18
#define WP_RESTOREBUTTON  21
#define CBTNS_NORMAL      1   /* shared caption-button state ladder: NORMAL/HOT/PUSHED/DISABLED/INACTIVE */
#define CBTNS_HOT         2
#define CBTNS_PUSHED      3
#define CBTNS_DISABLED    4
#define CBTNS_INACTIVE    5
#define TS_TRUE           1   /* THEMESIZE */
#ifndef TME_NONCLIENT
#define TME_NONCLIENT     0x00000010
#endif

/* The four caption buttons, left-to-right in the right cluster. The light/dark toggle is the new leftmost
   one, directly adjacent to Minimize, computed from the same cell so it is the others' exact size. */
enum FRAME_BUTTON
{
    FB_NONE = 0,
    FB_LIGHTDARK,
    FB_MIN,
    FB_MAX,
    FB_CLOSE,
    FB_COUNT
};

/* Per-window custom-frame interaction state (kept off the window's USERDATA, in g_theme, like the other
   per-window lists). idHot/idPressed are FRAME_BUTTON values; iHotMenu is a 0-based top-level menu index
   or -1. */
typedef struct FRAME_STATE
{
    HWND  hwnd;
    HMENU hMenu;     /* detached from the window (SetMenu NULL); we own its draw + popup tracking */
    int   idHot;
    int   idPressed;
    int   iHotMenu;
    int   iMenuActive; /* keyboard menu mode: hot top-level index, or -1 (not in menu mode)         */
    BOOL  fTracking;
    BOOL  fCapturing;  /* a caption button is pressed; we hold the mouse capture                    */
    BOOL  fShowAccel;  /* draw menu mnemonic underlines (Alt/F10 cue is up)                         */
    BOOL  fReserved;   /* keep FRAME_STATE 8-byte aligned (no C4820 pad)                            */
} FRAME_STATE;

#define THEME_MAX_MENU_TOPITEMS 16

#define THEME_ANIMATION_TIMER_ID ((UINT_PTR)0x57A2)
#define THEME_ANIMATION_TICK_MS  5u

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
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
    PFN_DWMEXTENDFRAMEINTOCLIENTAREA     pfnDwmExtendFrameIntoClientArea;
    PFN_DWMDEFWINDOWPROC                 pfnDwmDefWindowProc;
    PFN_DRAWTHEMEBACKGROUND             pfnDrawThemeBackground;
    PFN_GETTHEMEPARTSIZE                pfnGetThemePartSize;
    PFN_GETDPIFORWINDOW                 pfnGetDpiForWindow;
    PFN_GETSYSTEMMETRICSFORDPI          pfnGetSystemMetricsForDpi;
    PFN_TIMEPERIOD                            pfnTimeBeginPeriod;
    PFN_TIMEPERIOD                            pfnTimeEndPeriod;
    HMODULE                                   hUxtheme;
    HMODULE                                   hDwmapi;
    HMODULE                                   hAdvapi32;
    HMODULE                                   hWinmm;
    HMODULE                                   hUser32;
    HBRUSH                                    hbrDarkBg;
    HBRUSH                                    hbrAnimation;   /* solid brush for the current crossfade color */
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
    HWND                                      hwndAnimationTimer; /* GUI window the tick message is posted to     */
    HANDLE                                    hAnimationThread;   /* posts the tick; immune to WM_TIMER starvation */
    HANDLE                                    hAnimationStop;     /* manual-reset: tells the tick thread to exit    */
    HANDLE                                    hAnimationTimerObj; /* high-res periodic waitable timer (the clock)   */
    volatile LONG                             lTickPending;       /* 1 == a tick is queued/in-flight (coalescing)   */
    UINT                                      uAnimationTickMsg;  /* RegisterWindowMessage id for the posted tick   */
    UINT                                      cTopLevels;
    UINT                                      cDialogs;
    UINT                                      cAnimationWindows;
    DWORD                                     dwAnimationStartTick;
    DWORD                                     dwAnimationSnapTick;
    COLORREF                                  crAnimation;       /* color hbrAnimation was created for (was reserved) */
    DWORD                                     dwCaptionProgress; /* 0..1000: live caption progress this tick        */
    DWORD                                     dwReserved2;       /* keep THEME_STATE 8-byte aligned (no C4820 pad)  */
    FRAME_STATE                               rgFrames[THEME_MAX_TOPLEVELS]; /* custom-frame interaction state    */
    UINT                                      cFrames;
    BOOL                                      fManualOverrideActive; /* light/dark button drove the mode, not regs */
    BOOL                                      fManualDark;
    DWORD                                     dwReserved3;       /* keep THEME_STATE 8-byte aligned (no C4820 pad) */
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
    PFN_DWMEXTENDFRAMEINTOCLIENTAREA pfnDwmExtendFrameIntoClientArea;
    PFN_DWMDEFWINDOWPROC pfnDwmDefWindowProc;
    PFN_DRAWTHEMEBACKGROUND pfnDrawThemeBackground;
    PFN_GETTHEMEPARTSIZE pfnGetThemePartSize;
    PFN_GETDPIFORWINDOW pfnGetDpiForWindow;
    PFN_GETSYSTEMMETRICSFORDPI pfnGetSystemMetricsForDpi;
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
        u.fp = GetProcAddress(g_theme.hUxtheme, "DrawThemeBackground");
        g_theme.pfnDrawThemeBackground = u.pfnDrawThemeBackground;
        u.fp = GetProcAddress(g_theme.hUxtheme, "GetThemePartSize");
        g_theme.pfnGetThemePartSize = u.pfnGetThemePartSize;
    }

    if (IsNonNull(g_theme.hDwmapi))
    {
        u.fp = GetProcAddress(g_theme.hDwmapi, "DwmSetWindowAttribute");
        g_theme.pfnDwmSetWindowAttribute = u.pfnDwmSetWindowAttribute;
        u.fp = GetProcAddress(g_theme.hDwmapi, "DwmFlush");
        g_theme.pfnDwmFlush = u.pfnDwmFlush;
        u.fp = GetProcAddress(g_theme.hDwmapi, "DwmExtendFrameIntoClientArea");
        g_theme.pfnDwmExtendFrameIntoClientArea = u.pfnDwmExtendFrameIntoClientArea;
        u.fp = GetProcAddress(g_theme.hDwmapi, "DwmDefWindowProc");
        g_theme.pfnDwmDefWindowProc = u.pfnDwmDefWindowProc;
    }

    /* GetDpiForWindow / GetSystemMetricsForDpi are Win10 1607+; resolve dynamically (user32 is already
       loaded -- the app links it) so the library still loads on older systems. */
    g_theme.hUser32 = GetModuleHandle(TEXT("user32.dll"));
    if (IsNonNull(g_theme.hUser32))
    {
        u.fp = GetProcAddress(g_theme.hUser32, "GetDpiForWindow");
        g_theme.pfnGetDpiForWindow = u.pfnGetDpiForWindow;
        u.fp = GetProcAddress(g_theme.hUser32, "GetSystemMetricsForDpi");
        g_theme.pfnGetSystemMetricsForDpi = u.pfnGetSystemMetricsForDpi;
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
    /* Once the light/dark caption button is used, the manual choice wins over the system registry setting
       (the app's own preference); the same transition machinery then runs against this value. */
    if (g_theme.fManualOverrideActive)
    {
        return g_theme.fManualDark;
    }
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
 * DWM fades the immersive caption on a mildly front-loaded (ease-out) curve, p = 4t/(3+t) -- but it
 * does NOT fade symmetrically: the to-DARK caption fade completes markedly faster than the to-LIGHT
 * fade. A single curve therefore cannot track both directions: tuned for one, the app leads the caption
 * on the fast direction and lags it on the slow one. So the shape stays p = 4t/(3+t) but the effective
 * duration the lerp completes over is scaled per direction -- a shorter window to-dark (app was lagging),
 * a slightly longer one to-light (app was leading). The curve still reaches full at e == d_eff, then the
 * clamp holds it, so each direction's body tracks the caption within the band. Percentages are tuned
 * against T6's measured per-leg caption-band deviation.
 *
 * eff = 4*e*d / (3*d_eff + e), with d_eff = d * pct/100 and e clamped to [0, d_eff] (integer, branchless,
 * C5045-safe: pct is selected by arithmetic on a 0/1 flag, not a control-dependent load).
 */
#define THEME_EASE_TOLIGHT_PCT  108   /* to-light: mild stretch (app led the caption ~14% at pct=100)  */
#define THEME_EASE_TODARK_PCT    76   /* to-dark: compress (app lagged ~34% at pct=100, led ~37% at 52) */
/* Caption luma endpoints (the DWM immersive caption's dark/light shades, as composited) used to turn a
   live caption-pixel sample into transition progress. The owner-painted surfaces are driven by THIS
   measured progress (see ThemeSampleCaptionProgress) instead of a wall-clock ease, so they track the
   caption's actual curve/timing frame-for-frame -- no approximation, no lead/lag. */
#define THEME_CAPTION_DARK_LUMA   31
#define THEME_CAPTION_LIGHT_LUMA 243

static FORCEINLINE DWORD ThemeEaseElapsed(DWORD dwElapsed, DWORD dwDuration, BOOL fToDark)
{
    int       e;
    int       d;
    int       deff;
    int       pct;
    DWORDLONG ullNum;
    DWORDLONG ullDen;

    e = (int)dwElapsed;
    d = (int)dwDuration;
    d += (int)(0 == d);
    /* Branchless per-direction duration: pct = TOLIGHT + flag*(TODARK - TOLIGHT), flag in {0,1}. */
    pct = THEME_EASE_TOLIGHT_PCT + ((0 != fToDark) ? 1 : 0) * (THEME_EASE_TODARK_PCT - THEME_EASE_TOLIGHT_PCT);
    deff = (int)(((DWORDLONG)(DWORD)d * (DWORDLONG)(DWORD)pct) / 100u);
    deff += (int)(0 == deff);
    /* Clamp e to [0, deff]: (deff-e)>>31 is all-ones exactly when e>deff (pins e to deff); ~(e>>31)
     * pins a defensive negative e to 0. The lerp reaches full (eff==d) at e==deff, then holds. */
    e += (deff - e) & ((deff - e) >> 31);
    e &= ~(e >> 31);
    ullNum = (DWORDLONG)4 * (DWORDLONG)(DWORD)e * (DWORDLONG)(DWORD)d;
    ullDen = (DWORDLONG)3 * (DWORDLONG)(DWORD)deff + (DWORDLONG)(DWORD)e;
    return (DWORD)(ullNum / ullDen);
}

/*
 * Client background color at the current point of the transition. Interpolated by the LIVE caption
 * progress (g_theme.dwCaptionProgress, sampled off the DWM caption each tick), so the client renders at
 * exactly the caption's current shade -- in lockstep with the caption and with the menu/children, which
 * read the same value -- instead of an independent wall-clock approximation that drifts.
 */
static FORCEINLINE COLORREF ThemeClientAnimationColor(void)
{
    COLORREF crFrom;
    COLORREF crTo;

    crFrom = ThemeBackgroundColor(g_theme.fAnimationFromDark);
    crTo = ThemeBackgroundColor(g_theme.fAnimationToDark);
    return ThemeLerpColor(crFrom, crTo, g_theme.dwCaptionProgress, 1000u);
}

/* Control (static/dialog) text color at the current point of the transition, on the same live caption
   progress as the client background, so a dialog's labels cross-fade in lockstep with the body they sit
   on instead of snapping. Light mode uses the system window-text color; dark mode uses DARK_TEXT. */
static FORCEINLINE COLORREF ThemeClientTextColor(BOOL fDark)
{
    if (fDark)
    {
        return DARK_TEXT;
    }
    return GetSysColor(COLOR_WINDOWTEXT);
}

static FORCEINLINE COLORREF ThemeClientTextAnimationColor(void)
{
    COLORREF crFrom;
    COLORREF crTo;

    crFrom = ThemeClientTextColor(g_theme.fAnimationFromDark);
    crTo = ThemeClientTextColor(g_theme.fAnimationToDark);
    return ThemeLerpColor(crFrom, crTo, g_theme.dwCaptionProgress, 1000u);
}

/* Cached solid brush for the live crossfade color (recreated only when the interpolated color changes),
   mirroring the hbrDarkBg cache. Returned from WM_CTLCOLOR* during a transition so control backgrounds
   are filled with the same intermediate color the client erases to -- the dialog's static/edit faces
   then track the caption's progress band instead of jumping to the target shade on the first frame. */
static FORCEINLINE HBRUSH ThemeAnimationBrush(COLORREF cr)
{
    if (!g_theme.hbrAnimation || (g_theme.crAnimation != cr))
    {
        if (g_theme.hbrAnimation)
        {
            DeleteObject(g_theme.hbrAnimation);
        }
        g_theme.hbrAnimation = CreateSolidBrush(cr);
        g_theme.crAnimation = cr;
    }
    return g_theme.hbrAnimation;
}


static FORCEINLINE void ThemeArmBackgroundAnimationWindows(void);
static FORCEINLINE void ThemeCompletePendingThemeChange(void);
static FORCEINLINE void ThemeStopAnimationTimer(void);
static FORCEINLINE void ThemeStartAnimationThread(void);
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
        /* Clock the crossfade from a dedicated thread that POSTS a tick message, not SetTimer/WM_TIMER.
           WM_TIMER (with or without a TIMERPROC) is synthesized only when the message queue is otherwise
           empty, so a real Settings switch -- which floods the app with WM_THEMECHANGED/WM_SETTINGCHANGE
           and forced repaints for the whole transition -- starves it (measured: ticks ~700-1500ms apart
           instead of every 5ms), and the fade snaps to the target. A posted message is normal priority,
           dequeued ahead of WM_TIMER/WM_PAINT, so the storm cannot starve it. The thread only
           PostMessages; every paint stays on the GUI thread in the tick handler, routed through the app's
           WndProc exactly as the deferred theme-change message already is. */
        if (IsWindow(g_theme.rgAnimationWindows[0]))
        {
            ThemeStartAnimationThread();
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

/* Apply the TARGET control sub-theme (DarkMode_Explorer / Explorer) to every registered dialog's child
   controls at the START of a transition. Without this the sub-theme is only swapped at the end, so a
   dark->light fade runs with the controls still on DarkMode_Explorer the whole way -- they render dark
   and lag the lightening body (light->dark is fine because the controls start on Explorer). Reversing
   the sub-theme up front lets each control re-theme once (SetWindowTheme's own WM_THEMECHANGED repaint;
   no extra invalidation) and cross-fade toward the target from the first frame. */
static FORCEINLINE void ThemeSetRegisteredControlThemes(BOOL fDark)
{
    HWND* phwnd;
    HWND  hwnd;
    UINT  c;

    /* Every control under every registered window -- dialogs AND top-levels -- gets the target
       sub-theme, so none is left on the wrong (e.g. DarkMode_Explorer) theme during the fade. */
    phwnd = g_theme.rgDialogs;
    c = g_theme.cDialogs;
    while (0u != c)
    {
        hwnd = *phwnd;
        if (IsWindow(hwnd))
        {
            EnumChildWindows(hwnd, ThemeApplyClientThemeProc, (LPARAM)fDark);
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
            EnumChildWindows(hwnd, ThemeApplyClientThemeProc, (LPARAM)fDark);
        }
        ++phwnd;
        --c;
    }
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

/* Cross-fade one child control's background DOUBLE-BUFFERED, per animation tick. Plain InvalidateRect
   makes the control erase-then-draw on screen, so its text/icon shimmer against the changing background
   every tick (the flicker). Instead, render the control fully into a memory bitmap -- PRF_ERASEBKGND
   fills the background from the WM_CTLCOLOR brush (the crossfade color this tick), PRF_CLIENT draws the
   text/icon over it -- then blit the finished image to the screen in ONE BitBlt. The on-screen pixels
   change exactly once per tick, fully composited, so the background cross-fades smoothly and the
   text/icon stay crisp with no shimmer. Safe with the parent's WS_CLIPCHILDREN: the parent fill never
   touches these child rects, so nothing fights this blit. */
static BOOL CALLBACK ThemeRepaintChildProc(HWND hChild, LPARAM lParam)
{
    RECT    rc;
    HDC     hdc;
    HDC     hdcMem;
    HBITMAP hbm;
    HGDIOBJ hbmOld;

    UNREFERENCED_PARAMETER(lParam);
    if (!GetClientRect(hChild, &rc) || (0 >= rc.right) || (0 >= rc.bottom))
    {
        return TRUE;
    }
    hdc = GetDC(hChild);
    if (!hdc)
    {
        return TRUE;
    }
    hdcMem = CreateCompatibleDC(hdc);
    hbm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    if (hdcMem && hbm)
    {
        hbmOld = SelectObject(hdcMem, hbm);
        /* The control renders its own background (from WM_CTLCOLOR* -> the crossfade brush) and content
           into the off-screen DC; blit the composited result once. */
        SendMessage(hChild, WM_PRINTCLIENT, (WPARAM)hdcMem, (LPARAM)(PRF_CLIENT | PRF_ERASEBKGND));
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hbmOld);
    }
    if (hbm)
    {
        DeleteObject(hbm);
    }
    if (hdcMem)
    {
        DeleteDC(hdcMem);
    }
    ReleaseDC(hChild, hdc);
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
    if (g_theme.hAnimationThread)
    {
        if (g_theme.hAnimationStop)
        {
            (void)SetEvent(g_theme.hAnimationStop);
        }
        /* The tick thread only ever PostMessages (it never blocks on the GUI thread), so joining it from
           the GUI thread -- even from inside the tick handler that called us -- cannot deadlock; it exits
           within one tick interval. */
        (void)WaitForSingleObject(g_theme.hAnimationThread, INFINITE);
        CloseHandle(g_theme.hAnimationThread);
        g_theme.hAnimationThread = NULL;
    }
    if (g_theme.hAnimationStop)
    {
        CloseHandle(g_theme.hAnimationStop);
        g_theme.hAnimationStop = NULL;
    }
    if (g_theme.hAnimationTimerObj)
    {
        (void)CancelWaitableTimer(g_theme.hAnimationTimerObj);
        CloseHandle(g_theme.hAnimationTimerObj);
        g_theme.hAnimationTimerObj = NULL;
    }
    /* Destroy the tick window only after the thread has stopped posting to it. Safe to call from inside
       ThemeTickWndProc (a window may be destroyed from within its own procedure on the owning thread). */
    if (g_theme.hwndAnimationTimer)
    {
        DestroyWindow(g_theme.hwndAnimationTimer);
        g_theme.hwndAnimationTimer = NULL;
    }
    InterlockedExchange(&g_theme.lTickPending, 0);
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
static DECLSPEC_NOINLINE void ThemeOnAnimationTick(void)
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

    /* Per-tick transition progress (0..1000) every owner-painted surface lerps by, so they all render at
       the same point in the fade. Eased wall-clock progress: GetPixel-sampling the live caption proved
       unreliable (intermittently reads the composited caption wrong), so this is the deterministic path. */
    {
        DWORD dwDur;
        DWORD dwElap;

        dwDur = ThemeBackgroundTransitionMs();
        dwElap = g_theme.dwAnimationSnapTick - g_theme.dwAnimationStartTick;
        g_theme.dwCaptionProgress = (0u != dwDur)
                    ? ((ThemeEaseElapsed(dwElap, dwDur, g_theme.fAnimationToDark) * 1000u) / dwDur)
                    : 1000u;
    }

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
            /* Repaint child controls (a dialog's static text, the OK button) so they re-issue
               WM_CTLCOLOR* and cross-fade their background+text in step with the parent client. Targeted
               to the children only -- not the whole window tree -- and conflict-free because the parent
               is WS_CLIPCHILDREN (its fill never touches these child rects). */
            EnumChildWindows(hwnd, ThemeRepaintChildProc, 0);
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

/* Atom of the library's private tick-window class, registered once per process. */
static ATOM g_atomThemeTickClass;

/* The library's own message-only window procedure. The animation tick is delivered HERE, to a window the
   library owns, so it does NOT depend on the host app routing a new message id into
   ThemeHandleWindowMessage -- the cooperation requirement that silently broke the old WM_TIMER path
   (examples/main.c routes only a fixed set of messages). The app's normal GetMessage/DispatchMessage
   loop dispatches the posted tick straight to this proc. */
static LRESULT CALLBACK ThemeTickWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (g_theme.uAnimationTickMsg && (uMsg == g_theme.uAnimationTickMsg))
    {
        /* Clear pending first so the tick thread may queue the next tick while this one's paints run. */
        InterlockedExchange(&g_theme.lTickPending, 0);
        ThemeOnAnimationTick();
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static FORCEINLINE HWND ThemeCreateTickWindow(void)
{
    WNDCLASS  wc;
    HINSTANCE hInst;

    hInst = GetModuleHandle(NULL);
    if (0u == g_atomThemeTickClass)
    {
        SecureZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc   = ThemeTickWndProc;
        wc.hInstance     = hInst;
        wc.lpszClassName = TEXT("Win32XThemeTickWnd");
        g_atomThemeTickClass = RegisterClass(&wc);
    }
    if (0u == g_atomThemeTickClass)
    {
        return NULL;
    }
    /* HWND_MESSAGE: a message-only window -- never shown, just a tick sink on the GUI thread. */
    return CreateWindowEx(0, TEXT("Win32XThemeTickWnd"), NULL, 0, 0, 0, 0, 0,
                          HWND_MESSAGE, NULL, hInst, NULL);
}

/* The animation clock: a dedicated thread that posts uAnimationTickMsg to the library's tick window every
   THEME_ANIMATION_TICK_MS. It does no GUI work -- only PostMessage -- so it is safe off the GUI thread,
   and the posted message (normal priority) is never starved by the WM_THEMECHANGED/WM_PAINT storm a real
   Settings switch delivers, unlike the WM_TIMER it replaces. lTickPending coalesces: at most one tick is
   ever outstanding, so a slow GUI thread cannot accumulate a backlog of posts. */
static DWORD WINAPI ThemeAnimationTickThread(LPVOID pvParam)
{
    HANDLE rgWait[2];
    DWORD  dwWait;
    BOOL   fTick;

    UNREFERENCED_PARAMETER(pvParam);
    rgWait[0] = g_theme.hAnimationStop;
    rgWait[1] = g_theme.hAnimationTimerObj;
    for (;;)
    {
        if (rgWait[1])
        {
            /* The high-resolution periodic timer is the clock: it re-signals every tick interval to true
               5ms accuracy, independent of the ~15.6ms default system timer granularity that WaitForSingle-
               Object's timeout is clamped to (which made the tick ~3x too slow). */
            dwWait = WaitForMultipleObjects(2u, rgWait, FALSE, INFINITE);
            if (dwWait != (WAIT_OBJECT_0 + 1u))
            {
                break;   /* stop signalled, or wait failed */
            }
            fTick = TRUE;
        }
        else
        {
            /* Fallback when no waitable timer could be created: timeout poll (coarser). */
            if (WAIT_OBJECT_0 == WaitForSingleObject(g_theme.hAnimationStop, THEME_ANIMATION_TICK_MS))
            {
                break;
            }
            fTick = TRUE;
        }
        if (fTick && (0 == InterlockedExchange(&g_theme.lTickPending, 1)))
        {
            if (!PostMessage(g_theme.hwndAnimationTimer, g_theme.uAnimationTickMsg, 0, 0))
            {
                /* Window gone or queue full: drop the pending mark so the next interval retries. */
                InterlockedExchange(&g_theme.lTickPending, 0);
            }
        }
    }
    return 0u;
}

static FORCEINLINE void ThemeStartAnimationThread(void)
{
    if (0u == g_theme.uAnimationTickMsg)
    {
        g_theme.uAnimationTickMsg = RegisterWindowMessage(TEXT("Win32XThemeAnimationTick.v1"));
    }
    if (g_theme.hAnimationThread || (0u == g_theme.uAnimationTickMsg))
    {
        return;   /* already running, or no tick-message id available */
    }
    if (!g_theme.hwndAnimationTimer)
    {
        g_theme.hwndAnimationTimer = ThemeCreateTickWindow();
    }
    if (!g_theme.hwndAnimationTimer)
    {
        return;
    }
    if (!g_theme.hAnimationStop)
    {
        g_theme.hAnimationStop = CreateEvent(NULL, TRUE, FALSE, NULL);   /* manual-reset, nonsignaled */
    }
    else
    {
        (void)ResetEvent(g_theme.hAnimationStop);
    }
    if (!g_theme.hAnimationStop)
    {
        return;
    }
    /* High-resolution periodic clock. Falls back to a normal waitable timer, then (in the thread) to a
       timeout poll, if the high-res flag is unsupported. */
    if (!g_theme.hAnimationTimerObj)
    {
        g_theme.hAnimationTimerObj = CreateWaitableTimerEx(NULL, NULL,
                                        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!g_theme.hAnimationTimerObj)
        {
            g_theme.hAnimationTimerObj = CreateWaitableTimer(NULL, FALSE, NULL);
        }
    }
    if (g_theme.hAnimationTimerObj)
    {
        LARGE_INTEGER liDue;
        liDue.QuadPart = -((LONGLONG)THEME_ANIMATION_TICK_MS * 10000);   /* relative, 100ns units */
        (void)SetWaitableTimer(g_theme.hAnimationTimerObj, &liDue, (LONG)THEME_ANIMATION_TICK_MS,
                               NULL, NULL, FALSE);
    }
    InterlockedExchange(&g_theme.lTickPending, 0);
    g_theme.hAnimationThread = CreateThread(NULL, 0, ThemeAnimationTickThread, NULL, 0, NULL);
}

/* TRUE when this ImmersiveColorSet broadcast merely duplicates the switch already being animated: we are
   mid-transition AND the registry's effective mode -- read FRESH (ThemeReadPersonalizeDarkMode, not the
   cached ShouldAppsUseDarkMode) -- already equals the target we are fading to. A real Settings switch
   re-broadcasts ImmersiveColorSet several times, and a full switch broadcasts again for the system-theme
   half; running ThemePublishBroadcastPaintState's heavy RefreshImmersiveColorPolicyState flush on each one
   blocks the GUI thread through the fade window (the input path's 11 animation ticks vs the harness's ~74).
   Detecting the duplicate here -- via the always-fresh registry, BEFORE any flush -- lets those duplicates
   skip the flush. The harness's first, real broadcast is not yet animating, so it is never a duplicate and
   still flushes + arms; an opposite mid-fade switch differs from fAnimationToDark and is likewise not a
   duplicate, so it re-arms. DECLSPEC_NOINLINE keeps its local off the inlined publish/dispatch frame
   (__chkstk, unresolvable in this /NODEFAULTLIB build). */
static DECLSPEC_NOINLINE BOOL ThemeBroadcastDuplicatesLiveTransition(void)
{
    BOOL fRegDark;

    if (!g_theme.fAnimatingBackground)
    {
        return FALSE;
    }
    fRegDark = FALSE;
    if (!ThemeReadPersonalizeDarkMode(&fRegDark))
    {
        return FALSE;
    }
    return ((fRegDark && g_theme.fDarkCapable) == g_theme.fAnimationToDark);
}

static FORCEINLINE BOOL ThemePublishBroadcastPaintState(void)
{
    BOOL fRequestedDark;
    BOOL fEffectiveDark;
    BOOL fOldEffectiveDark;
    BOOL fStarted;

    /* Skip the heavy flush/republish for re-broadcasts of the switch already in flight -- see helper. */
    if (ThemeBroadcastDuplicatesLiveTransition())
    {
        return g_theme.fEffectiveDark;
    }
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
        /* Reset the shared progress to t=0 on every (re)arm. Otherwise the first paint triggered by
           the invalidate below runs BEFORE the next timer tick refreshes the value, and reuses the
           PREVIOUS leg's final progress (1000 = 100%). On the restore leg that paints the client
           instantly at lerp(target,initial,100%) = fully restored -> the client snaps while the caption
           is still at target. Whether the tick beats that first paint was the bimodal restore-leg race
           (forward leg starts from a 0/plateau so it was never hit). */
        g_theme.dwCaptionProgress = 0u;
    }
    ThemeSetRegisteredClassBrushes(fEffectiveDark);
    /* Reverse the dialog control sub-theme to the target now (DarkMode_Explorer -> Explorer on the way
       to light), so the controls cross-fade from the first frame instead of staying dark until the end
       -- the dark->light direction otherwise lags. */
    ThemeSetRegisteredControlThemes(fEffectiveDark);
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
    /* Clip child windows out of the parent's paint: the per-tick crossfade fills the whole client, and
       without WS_CLIPCHILDREN that fill lands under child controls and fights their own paint -- the
       flicker. Clipped, the parent fill never touches child pixels; each child crossfades itself. */
    SetWindowLongPtr(hwnd, GWL_STYLE, GetWindowLongPtr(hwnd, GWL_STYLE) | WS_CLIPCHILDREN);
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
    /* Clip children so the dialog's full-client crossfade fill never paints under the static/button
       child windows and fights their paint (the flicker). Each child crossfades its own background. */
    SetWindowLongPtr(hwnd, GWL_STYLE, GetWindowLongPtr(hwnd, GWL_STYLE) | WS_CLIPCHILDREN);
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

    MenuBarPalette(g_theme.fAnimationFromDark, &palFrom);
    MenuBarPalette(g_theme.fAnimationToDark, &palTo);
    return ThemeLerpColor(palFrom.clrText, palTo.clrText, g_theme.dwCaptionProgress, 1000u);
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

    /* The fill erased the item labels DefWindowProc drew; redraw each top-level label over it at the
       interpolated text color so "File"/"Help" stay visible and cross-fade with the bar. Render with
       uxtheme's DrawThemeTextEx -- the SAME themed text path DefWindowProc/WM_UAHDRAWMENUITEM uses (the
       Menu theme's MENU_BARITEM glyphs, anti-aliased the theme's way) -- not raw GDI DrawTextW. Plain
       DrawTextW rasterizes the label differently than the system's themed draw, so the two renderings
       disagree frame to frame and the text shimmers (fights the system draw). Themed text matches it. */
    hFont = ThemeMenuBarFont();
    hOldFont = hFont ? SelectObject(hdc, hFont) : NULL;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, crText);
    {
        HTHEME hThemeMenu;

        hThemeMenu = (g_theme.pfnOpenThemeData && g_theme.pfnDrawThemeTextEx && g_theme.pfnCloseThemeData)
                     ? g_theme.pfnOpenThemeData(hwnd, L"Menu") : NULL;
        /* Constant loop bound (a bounded pre-tested loop is C5045-safe); GetMenuItemRect fails past the
           real top-level item count, so the body simply skips. */
        for (i = 0; i < THEME_MAX_MENU_ITEMS; ++i)
        {
            if (!GetMenuItemRect(hwnd, hMenu, (UINT)i, &rcItem))
            {
                continue;
            }
            OffsetRect(&rcItem, -rcWindow.left, -rcWindow.top);
            szText[0] = 0;
            cch = GetMenuStringW(hMenu, (UINT)i, szText, (int)ARRAYSIZE(szText) - 1, MF_BYPOSITION);
            if (hThemeMenu)
            {
                DTTOPTS opts;

                SecureZeroMemory(&opts, sizeof(opts));
                opts.dwSize  = (DWORD)sizeof(opts);
                /* DTT_GLOWSIZE + iGlowSize 0 suppresses the msstyles MENU_BARITEM text glow. With only
                   DTT_TEXTCOLOR the part's glow still renders, and as the bar cross-fades the glow
                   re-blends against a changing background every tick -- the menu-text shimmer ("a result
                   of the msstyle"). Zeroing the glow keeps uxtheme's themed AA but removes the shimmer. */
                opts.dwFlags = DTT_TEXTCOLOR | DTT_GLOWSIZE;
                opts.crText  = crText;
                opts.iGlowSize = 0;
                (void)g_theme.pfnDrawThemeTextEx(hThemeMenu, hdc, MENU_BARITEM, MBI_NORMAL,
                                                 szText, cch,
                                                 DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_HIDEPREFIX,
                                                 &rcItem, &opts);
            }
            else
            {
                (void)DrawTextW(hdc, szText, cch, &rcItem,
                                DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_HIDEPREFIX);
            }
        }
        if (hThemeMenu)
        {
            g_theme.pfnCloseThemeData(hThemeMenu);
        }
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
 * DefWindowProc render the frame first, then this repaints the menu band. WM_UAHDRAWMENU does NOT
 * re-fire every animation tick, so a UAH-only bar holds a stale paint and reads far off the client --
 * therefore while cross-fading we paint the whole bar ourselves off the shared clock each tick; outside
 * a transition the system's WM_UAHDRAWMENU has filled the bar and we only restamp the 1px seam.
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

/*
 * WM_CTLCOLOR* during a live transition: fill the control's background with the same intermediate color
 * the client erases to and interpolate its text color on the same clock, so the dialog's statics
 * cross-fade in lockstep with the body. DECLSPEC_NOINLINE keeps this helper's color math (and its
 * inlined lerps) in its own stack frame rather than enlarging the ThemeHandleWindowMessage dispatcher
 * past the one-page stack-probe threshold (__chkstk, unresolvable in this /NODEFAULTLIB build). Returns
 * FALSE when no transition is live so the caller falls back to the steady-state brush.
 */
static DECLSPEC_NOINLINE BOOL ThemeCtlColorAnimation(HWND hwnd, HDC hdc, LRESULT* plr)
{
    COLORREF crBg;

    if (!g_theme.fAnimatingBackground || !ThemeWindowHasBackgroundAnimation(hwnd))
    {
        return FALSE;
    }
    crBg = ThemeClientAnimationColor();
    SetBkColor(hdc, crBg);
    SetTextColor(hdc, ThemeClientTextAnimationColor());
    SetBkMode(hdc, OPAQUE);
    *plr = (LRESULT)ThemeAnimationBrush(crBg);
    return TRUE;
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

/* Absorb a redundant WM_THEMECHANGED during a managed transition -- but only while the live (or pending)
   transition is heading to the SAME target this broadcast reflects, so a quick subsequent switch to a
   DIFFERENT target falls through and re-arms (see the WM_THEMECHANGED case for the full rationale).
   DECLSPEC_NOINLINE keeps its local off the ThemeHandleWindowMessage dispatcher frame, which is already
   at the one-page stack-probe threshold (__chkstk, unresolvable in this /NODEFAULTLIB build). */
static DECLSPEC_NOINLINE BOOL ThemeHandleThemeChangedMessage(LRESULT* plr)
{
    BOOL fRequestedDark;

    if ((g_theme.fAnimatingBackground || g_theme.fPendingThemeChange) &&
        (ThemeReadEffectiveDarkMode(&fRequestedDark) == g_theme.fAnimationToDark))
    {
        *plr = 0;
        return TRUE;
    }
    return FALSE;
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

        case WM_SETTINGCHANGE:
            return ThemeHandleSettingChangeMessage(hwnd, lParam, uDeferredMsg, plr);

        case WM_THEMECHANGED:
            /* A full Settings switch broadcasts WM_THEMECHANGED to every window. While we are running a
               managed dark-mode transition we already drive control/window theming ourselves (SetWindowTheme
               on the registered windows) and cross-fade the surfaces on the animation clock; letting
               DefWindowProc re-theme on each broadcast runs a redundant SYNCHRONOUS comctl re-theme that
               competes with the crossfade and -- in the real Settings (input) case -- stalls the GUI thread
               long enough that the first animation frame paints after the transition window has elapsed, so
               the fade snaps to the target. Absorb it while a change is in flight -- BUT only when the live
               (or pending) transition is heading to the SAME target this broadcast reflects. A quick
               subsequent switch to a DIFFERENT target must NOT be swallowed: fall through so the normal
               WM_SETTINGCHANGE path re-arms the fade to the new target (otherwise the fade keeps running to
               the now-stale target and controls never re-theme to the new one). Outside a transition,
               likewise fall through so controls reload normally. */
            if (ThemeHandleThemeChangedMessage(plr))
            {
                return TRUE;
            }
            break;

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
            /* While a transition is live, paint control backgrounds with the SAME intermediate color
               the client erases to (off the shared snapshot clock), so the dialog's statics cross-fade
               in band with the body instead of snapping to the target shade on the first frame. Kept in
               its own (non-inlined) frame so its color math does not enlarge this dispatcher's stack. */
            if (ThemeCtlColorAnimation(hwnd, (HDC)wParam, plr))
            {
                return TRUE;
            }
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
            if (g_theme.uAnimationTickMsg && (uMsg == g_theme.uAnimationTickMsg))
            {
                /* Posted animation tick (ThemeAnimationTickThread). Clear the pending mark first so the
                   thread may queue the next tick even while this one's paints run. */
                InterlockedExchange(&g_theme.lTickPending, 0);
                ThemeOnAnimationTick();
                *plr = 0;
                return TRUE;
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

    MenuBarPalette(g_theme.fAnimationFromDark, &palFrom);
    MenuBarPalette(g_theme.fAnimationToDark, &palTo);
    return ThemeLerpColor(palFrom.clrBar, palTo.clrBar, g_theme.dwCaptionProgress, 1000u);
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
            if (g_theme.fAnimatingBackground)
            {
                /* Suppress the msstyles text glow while cross-fading -- it re-blends against the moving
                   bar each tick and shimmers. Outside the fade keep the native glow. */
                opts.dwFlags |= DTT_GLOWSIZE;
                opts.iGlowSize = 0;
            }
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

/* ============================================================================================== *
 *  Custom non-client frame: caption + four caption buttons + in-client menu bar.
 *
 *  The window is opted in with ThemeEnableCustomFrame. WM_NCCALCSIZE removes the standard frame so the
 *  whole window becomes client; DwmExtendFrameIntoClientArea keeps the DWM drop shadow + resize borders;
 *  WM_NCHITTEST re-supplies the resize/move/button regions; WM_PAINT owner-draws the caption (icon,
 *  title via DrawThemeTextEx), the four buttons (the three standard ones through the uxtheme WINDOW
 *  parts -- the renderer DefWindowProc's themed legacy caption uses -- and the new light/dark one to the
 *  same cell), and the menu bar (its NC band and UAH draw messages die with the standard frame, so it is
 *  re-hosted in the client and rendered with the existing MenuBarOnDrawMenuItem). The bands fold onto the
 *  shared dwCaptionProgress clock, so they cross-fade with the client during a live light/dark switch.
 *
 *  Runtime-tuning surface (documented, not guessed): exact button cell pixels and the DWM shadow extent
 *  follow system metrics / a 1px sheet-of-glass and may want per-OS tuning to match uDWM's Win11 cell to
 *  the pixel (uDWM is not in the disassembly set -- see THEME-PAINT-MESSAGE-CONTRACT §2). The Win11
 *  snap-layout flyover (DwmDefWindowProc) and full native menu keyboard navigation are not wired in v1.
 * ============================================================================================== */

typedef struct FRAME_LAYOUT
{
    int  cxFrame;     /* side/bottom resize-border thickness used for hit-testing */
    int  cyFrame;     /* top resize-grab strip thickness                          */
    int  capH;        /* caption band height (client y: 0 .. capH)                */
    int  menuH;       /* menu band height   (client y: capH .. capH+menuH)        */
    int  btnW;        /* one caption-button cell width                            */
    int  btnH;        /* one caption-button cell height (== capH)                 */
    int  cxClient;    /* client width                                            */
    RECT rcButtons[FB_COUNT];
} FRAME_LAYOUT;

static HFONT g_hThemeCaptionFont;
static HFONT g_hThemeSymbolFont;
static int   g_cyThemeSymbolFont;

/* GUI-thread-only paint/hit-test scratch kept off-stack: a 256-byte title buffer and the menu-item
   rect table on the stack would, once the surrounding NOINLINE frames are laid out, push past the
   one-page stack-probe threshold and emit __chkstk, which this /NODEFAULTLIB build cannot resolve
   (same remedy as g_themeNcmScratch). */
static WCHAR g_themeFrameTitle[128];
static RECT  g_themeFrameMenuRc[THEME_MAX_MENU_TOPITEMS];

static FORCEINLINE UINT ThemeFrameDpi(HWND hwnd)
{
    UINT dpi;

    if (g_theme.pfnGetDpiForWindow)
    {
        dpi = g_theme.pfnGetDpiForWindow(hwnd);
        if (0u != dpi)
        {
            return dpi;
        }
    }
    return 96u;
}

static FORCEINLINE int ThemeFrameMetric(int index, UINT dpi)
{
    if (g_theme.pfnGetSystemMetricsForDpi)
    {
        return g_theme.pfnGetSystemMetricsForDpi(index, dpi);
    }
    return GetSystemMetrics(index);
}

static FORCEINLINE HFONT ThemeCaptionFont(void)
{
    if (g_hThemeCaptionFont)
    {
        return g_hThemeCaptionFont;
    }
    SecureZeroMemory(&g_themeNcmScratch, sizeof(g_themeNcmScratch));
    g_themeNcmScratch.cbSize = (DWORD)sizeof(g_themeNcmScratch);
    if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, (UINT)sizeof(g_themeNcmScratch), &g_themeNcmScratch, 0))
    {
        g_hThemeCaptionFont = CreateFontIndirect(&g_themeNcmScratch.lfCaptionFont);
    }
    return g_hThemeCaptionFont;
}

/* A symbol font (Segoe MDL2 Assets, present Win10+) sized to the caption, for the light/dark glyph. */
static FORCEINLINE HFONT ThemeSymbolFont(int cyButton)
{
    int cyGlyph;

    cyGlyph = (cyButton * 2) / 5;
    if (cyGlyph < 8)
    {
        cyGlyph = 8;
    }
    if (g_hThemeSymbolFont && (g_cyThemeSymbolFont == cyGlyph))
    {
        return g_hThemeSymbolFont;
    }
    if (g_hThemeSymbolFont)
    {
        DeleteObject(g_hThemeSymbolFont);
        g_hThemeSymbolFont = NULL;
    }
    g_hThemeSymbolFont = CreateFontW(cyGlyph, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     L"Segoe MDL2 Assets");
    g_cyThemeSymbolFont = cyGlyph;
    return g_hThemeSymbolFont;
}

static FORCEINLINE FRAME_STATE* ThemeFrameFind(HWND hwnd)
{
    UINT i;

    for (i = 0u; i < g_theme.cFrames; ++i)
    {
        if (g_theme.rgFrames[i].hwnd == hwnd)
        {
            return &g_theme.rgFrames[i];
        }
    }
    return NULL;
}

static FORCEINLINE HMENU ThemeFrameMenu(HWND hwnd)
{
    FRAME_STATE* pst;

    pst = ThemeFrameFind(hwnd);
    return pst ? pst->hMenu : NULL;
}

static DECLSPEC_NOINLINE void ThemeFrameComputeLayout(HWND hwnd, FRAME_LAYOUT* pL)
{
    RECT           rcClient;
    TITLEBARINFOEX tbi;
    UINT           dpi;
    int            pad;
    int            r;
    int            w;
    int            h;
    BOOL           fExact;

    SecureZeroMemory(pL, sizeof(*pL));
    GetClientRect(hwnd, &rcClient);
    dpi = ThemeFrameDpi(hwnd);
    pad = ThemeFrameMetric(SM_CXPADDEDBORDER, dpi);
    pL->cxFrame  = ThemeFrameMetric(SM_CXFRAME, dpi) + pad;
    pL->cyFrame  = ThemeFrameMetric(SM_CYFRAME, dpi) + pad;
    pL->menuH    = ThemeFrameMenu(hwnd) ? ThemeFrameMetric(SM_CYMENU, dpi) : 0;
    pL->cxClient = rcClient.right - rcClient.left;

    /* Exact caption-button geometry from the system -- WM_GETTITLEBARINFOEX returns the SAME rects
       uDWM!CWindowList::GetCaptionButtonBounds / DefWindowProc compute (rgrect[2]=min, [3]=max, [5]=close,
       screen coords). We map them to client and place the light/dark button one cell left of Minimize,
       so all four are the system's exact size and position. */
    SecureZeroMemory(&tbi, sizeof(tbi));
    tbi.cbSize = (DWORD)sizeof(tbi);
    (void)SendMessageW(hwnd, WM_GETTITLEBARINFOEX, 0, (LPARAM)&tbi);
    (void)MapWindowPoints(NULL, hwnd, (POINT*)&tbi.rgrect[2], 2);  /* minimize -> client */
    (void)MapWindowPoints(NULL, hwnd, (POINT*)&tbi.rgrect[3], 2);  /* maximize -> client */
    (void)MapWindowPoints(NULL, hwnd, (POINT*)&tbi.rgrect[5], 2);  /* close    -> client */

    fExact = (tbi.rgrect[5].right > tbi.rgrect[5].left) && (tbi.rgrect[5].bottom > tbi.rgrect[5].top);
    if (fExact)
    {
        pL->btnW = tbi.rgrect[5].right - tbi.rgrect[5].left;
        pL->capH = tbi.rgrect[5].bottom;                  /* caption runs from client top to button bottom */
        pL->btnH = pL->capH;
        pL->rcButtons[FB_CLOSE] = tbi.rgrect[5];
        pL->rcButtons[FB_MAX]   = tbi.rgrect[3];
        pL->rcButtons[FB_MIN]   = tbi.rgrect[2];
        pL->rcButtons[FB_LIGHTDARK].left   = tbi.rgrect[2].left - pL->btnW;
        pL->rcButtons[FB_LIGHTDARK].right  = tbi.rgrect[2].left;
        pL->rcButtons[FB_LIGHTDARK].top    = tbi.rgrect[2].top;
        pL->rcButtons[FB_LIGHTDARK].bottom = tbi.rgrect[2].bottom;
        /* the caption band fills the full width to the top edge */
        pL->rcButtons[FB_CLOSE].top = 0;
        pL->rcButtons[FB_MAX].top   = 0;
        pL->rcButtons[FB_MIN].top   = 0;
        pL->rcButtons[FB_LIGHTDARK].top = 0;
        return;
    }

    /* Fallback (e.g. WM_GETTITLEBARINFOEX returned nothing): metric-based right cluster, same cell. */
    pL->capH = ThemeFrameMetric(SM_CYCAPTION, dpi) + pL->cyFrame;
    pL->btnW = ThemeFrameMetric(SM_CXSIZE, dpi);
    pL->btnH = pL->capH;
    r = pL->cxClient;
    w = pL->btnW;
    h = pL->btnH;
    pL->rcButtons[FB_CLOSE].left = r - w;     pL->rcButtons[FB_CLOSE].right = r;       r -= w;
    pL->rcButtons[FB_MAX].left   = r - w;     pL->rcButtons[FB_MAX].right   = r;       r -= w;
    pL->rcButtons[FB_MIN].left   = r - w;     pL->rcButtons[FB_MIN].right   = r;       r -= w;
    pL->rcButtons[FB_LIGHTDARK].left = r - w; pL->rcButtons[FB_LIGHTDARK].right = r;
    pL->rcButtons[FB_CLOSE].top = 0;     pL->rcButtons[FB_CLOSE].bottom = h;
    pL->rcButtons[FB_MAX].top = 0;       pL->rcButtons[FB_MAX].bottom = h;
    pL->rcButtons[FB_MIN].top = 0;       pL->rcButtons[FB_MIN].bottom = h;
    pL->rcButtons[FB_LIGHTDARK].top = 0; pL->rcButtons[FB_LIGHTDARK].bottom = h;
}

static DECLSPEC_NOINLINE int ThemeFrameMenuItemWidth(HWND hwnd, HMENU hMenu, int i, HDC hdc)
{
    WCHAR         sz[64];
    MENUITEMINFOW mii;
    SIZE          size;
    HFONT         hFont;
    HGDIOBJ       hOld;
    int           pad;

    UNREFERENCED_PARAMETER(hwnd);
    sz[0] = 0;
    SecureZeroMemory(&mii, sizeof(mii));
    mii.cbSize     = sizeof(mii);
    mii.fMask      = MIIM_STRING;
    mii.dwTypeData = sz;
    mii.cch        = (UINT)(ARRAYSIZE(sz) - 1);
    /* Returns 0 past the real top-level item count (invalid position), so a constant-bound caller loop
       stops without a dynamic count comparison feeding the call (C5045-safe, the existing menu idiom). */
    if (!GetMenuItemInfoW(hMenu, (UINT)i, TRUE, &mii))
    {
        return 0;
    }

    hFont = ThemeMenuBarFont();
    hOld  = hFont ? SelectObject(hdc, hFont) : NULL;
    size.cx = 0;
    size.cy = 0;
    (void)GetTextExtentPoint32W(hdc, sz, lstrlenW(sz), &size);
    if (hOld)
    {
        SelectObject(hdc, hOld);
    }
    pad = GetSystemMetrics(SM_CXMENUCHECK);
    return size.cx + pad * 2;
}

static DECLSPEC_NOINLINE void ThemeFrameInvalidateBands(HWND hwnd)
{
    FRAME_LAYOUT L;
    RECT         rc;

    ThemeFrameComputeLayout(hwnd, &L);
    rc.left   = 0;
    rc.top    = 0;
    rc.right  = L.cxClient;
    rc.bottom = L.capH + L.menuH;
    InvalidateRect(hwnd, &rc, FALSE);
}

/* A caption button is shown only when the window's styles enable it (DefWindowProc parity): Minimize
   needs WS_MINIMIZEBOX, Maximize needs WS_MAXIMIZEBOX; Close and the light/dark toggle are always shown. */
static FORCEINLINE BOOL ThemeFrameButtonEnabled(HWND hwnd, int id)
{
    LONG_PTR style;

    style = GetWindowLongPtr(hwnd, GWL_STYLE);
    switch (id)
    {
        case FB_MIN: return (0 != (style & WS_MINIMIZEBOX)) ? TRUE : FALSE;
        case FB_MAX: return (0 != (style & WS_MAXIMIZEBOX)) ? TRUE : FALSE;
        default:     return TRUE;
    }
}

/* Segoe MDL2 Assets glyph for each caption button (the same icon font the Win10/11 shell caption uses);
   matches both light and dark mode, unlike the uxtheme WINDOW parts (light-classic only). */
static FORCEINLINE WCHAR ThemeFrameButtonGlyph(int id, BOOL fZoomed, BOOL fDark)
{
    switch (id)
    {
        case FB_MIN:       return (WCHAR)0xE921;                          /* ChromeMinimize          */
        case FB_MAX:       return (WCHAR)(fZoomed ? 0xE923 : 0xE922);     /* ChromeRestore / Maximize */
        case FB_CLOSE:     return (WCHAR)0xE8BB;                          /* ChromeClose             */
        case FB_LIGHTDARK: return (WCHAR)(fDark ? 0xE706 : 0xE708);       /* Brightness / QuietHours  */
        default:           return 0;
    }
}

/* All four buttons drawn uniformly: a flat hover/pressed fill (Close goes red, like the shell) plus the
   MDL2 glyph in the caption text color. Sized from the shared cell, so the light/dark button is exactly
   the others' size. */
static DECLSPEC_NOINLINE void ThemeFramePaintButton(HWND hwnd, HDC hdc, const FRAME_LAYOUT* pL,
                                                    const FRAME_STATE* pst, int id, BOOL fActive, BOOL fDark,
                                                    COLORREF crBg, COLORREF crText, COLORREF crDim)
{
    RECT     rc;
    COLORREF crFill;
    COLORREF crGlyph;
    HFONT    hFont;
    HGDIOBJ  hOld;
    WCHAR    glyph[2];
    BOOL     fHot;
    BOOL     fPushed;

    rc      = pL->rcButtons[id];
    /* Pressed visual only while the cursor is over the captured button (cancel-on-drag-off, like the
       shell); hover only when nothing is pressed. */
    fPushed = (pst && (pst->idPressed == id) && (pst->idHot == id)) ? TRUE : FALSE;
    fHot    = (pst && (pst->idHot == id) && (pst->idPressed == FB_NONE)) ? TRUE : FALSE;

    crFill  = crBg;
    crGlyph = fActive ? crText : crDim;
    if (FB_CLOSE == id)
    {
        if (fPushed) { crFill = RGB(193, 62, 47);  crGlyph = RGB(255, 255, 255); }
        else if (fHot) { crFill = RGB(196, 43, 28); crGlyph = RGB(255, 255, 255); }
    }
    else
    {
        if (fPushed) { crFill = fDark ? RGB(80, 80, 80) : RGB(204, 204, 204); }
        else if (fHot) { crFill = fDark ? RGB(64, 64, 64) : RGB(229, 229, 229); }
    }
    ThemePaintSolidColor(hdc, &rc, crFill);

    glyph[0] = ThemeFrameButtonGlyph(id, IsZoomed(hwnd), fDark);
    glyph[1] = 0;
    hFont = ThemeSymbolFont(rc.bottom - rc.top);
    hOld  = hFont ? SelectObject(hdc, hFont) : NULL;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, crGlyph);
    DrawTextW(hdc, glyph, 1, &rc, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    if (hOld)
    {
        SelectObject(hdc, hOld);
    }
}

static DECLSPEC_NOINLINE void ThemeFramePaintMenuBar(HWND hwnd, HDC hdc, const FRAME_LAYOUT* pL,
                                                     const FRAME_STATE* pst, const MENUBAR_PALETTE* pPal)
{
    HMENU    hMenu;
    int      i;
    int      x;
    RECT     rcBand;
    COLORREF crBar;

    hMenu = ThemeFrameMenu(hwnd);
    if (!hMenu)
    {
        return;
    }

    crBar = pPal->clrBar;
    if (g_theme.fAnimatingBackground && ThemeWindowHasBackgroundAnimation(hwnd))
    {
        crBar = ThemeMenuAnimationColor();
    }
    rcBand.left   = 0;
    rcBand.top    = pL->capH;
    rcBand.right  = pL->cxClient;
    rcBand.bottom = pL->capH + pL->menuH;
    ThemePaintSolidColor(hdc, &rcBand, crBar);

    /* Constant loop bound (C5045-safe); ThemeFrameMenuItemWidth returns 0 past the last item -> stop. */
    x = pL->cxFrame;
    for (i = 0; i < THEME_MAX_MENU_TOPITEMS; ++i)
    {
        RECT           rcItem;
        int            w;
        UAHDRAWMENUITEM udmi;

        w = ThemeFrameMenuItemWidth(hwnd, hMenu, i, hdc);
        if (0 == w)
        {
            break;
        }
        rcItem.left   = x;
        rcItem.right  = x + w;
        rcItem.top    = pL->capH;
        rcItem.bottom = pL->capH + pL->menuH;

        SecureZeroMemory(&udmi, sizeof(udmi));
        udmi.dis.CtlType   = ODT_MENU;
        udmi.dis.rcItem    = rcItem;
        udmi.dis.hDC       = hdc;
        /* Hide '&' mnemonics until the Alt/F10 keyboard cue is up (DefWindowProc parity). */
        udmi.dis.itemState = (pst && pst->fShowAccel) ? 0u : (UINT)ODS_NOACCEL;
        if (pst && ((pst->iHotMenu == i) || (pst->iMenuActive == i)))
        {
            udmi.dis.itemState |= ODS_HOTLIGHT;
        }
        udmi.um.hmenu      = hMenu;
        udmi.um.hdc        = hdc;
        udmi.umi.iPosition = i;
        MenuBarOnDrawMenuItem(hwnd, &udmi, pPal);
        x += w;
    }
}

static DECLSPEC_NOINLINE void ThemeFramePaint(HWND hwnd, HDC hdc)
{
    FRAME_LAYOUT    L;
    FRAME_STATE*    pst;
    MENUBAR_PALETTE pal;
    RECT            rc;
    COLORREF        crCaption;
    COLORREF        crText;
    BOOL            fDark;
    BOOL            fActive;
    HTHEME          hWin;
    int             i;
    int             leftPad;

    pst = ThemeFrameFind(hwnd);
    ThemeFrameComputeLayout(hwnd, &L);
    fDark   = ThemeIsDarkMode();
    fActive = (GetActiveWindow() == hwnd);
    MenuBarPalette(fDark, &pal);

    if (g_theme.fAnimatingBackground && ThemeWindowHasBackgroundAnimation(hwnd))
    {
        crCaption = ThemeMenuAnimationColor();
        crText    = ThemeMenuTextAnimationColor();
    }
    else
    {
        crCaption = pal.clrBar;
        crText    = pal.clrText;
    }

    /* Caption band background. */
    rc.left   = 0;
    rc.top    = 0;
    rc.right  = L.cxClient;
    rc.bottom = L.capH;
    ThemePaintSolidColor(hdc, &rc, crCaption);

    leftPad = L.cxFrame;

    /* Window small icon. */
    {
        HICON hIcon;
        int   cxIcon;
        int   cyIcon;
        int   y;

        hIcon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL2, 0);
        if (!hIcon)
        {
            hIcon = (HICON)(LONG_PTR)GetClassLongPtr(hwnd, GCLP_HICONSM);
        }
        cxIcon = GetSystemMetrics(SM_CXSMICON);
        cyIcon = GetSystemMetrics(SM_CYSMICON);
        y      = (L.capH - cyIcon) / 2;
        if (hIcon)
        {
            (void)DrawIconEx(hdc, leftPad, y, hIcon, cxIcon, cyIcon, 0, NULL, DI_NORMAL);
        }
    }

    /* Caption title (themed text, caption font, caption text color). The text is passed NUL-terminated
       (-1), not a range-checked length, so no length comparison feeds the draw call (C5045-safe). The
       title buffer is the off-stack g_themeFrameTitle (GUI thread only) to keep this frame small. */
    {
        RECT    rcTitle;
        HFONT   hFont;
        HGDIOBJ hOld;

        g_themeFrameTitle[0] = 0;
        (void)GetWindowTextW(hwnd, g_themeFrameTitle, ARRAYSIZE(g_themeFrameTitle));
        rcTitle.left   = leftPad + GetSystemMetrics(SM_CXSMICON) + leftPad;
        rcTitle.top    = 0;
        rcTitle.right  = L.rcButtons[FB_LIGHTDARK].left - leftPad;
        rcTitle.bottom = L.capH;
        if (g_themeFrameTitle[0] && (rcTitle.right > rcTitle.left))
        {
            hFont = ThemeCaptionFont();
            hOld  = hFont ? SelectObject(hdc, hFont) : NULL;
            hWin  = g_theme.pfnOpenThemeData ? g_theme.pfnOpenThemeData(hwnd, L"WINDOW") : NULL;
            if (hWin && g_theme.pfnDrawThemeTextEx)
            {
                DTTOPTS o;

                SecureZeroMemory(&o, sizeof(o));
                o.dwSize    = (DWORD)sizeof(o);
                o.dwFlags   = DTT_TEXTCOLOR | DTT_GLOWSIZE;
                o.crText    = crText;
                o.iGlowSize = 0;
                (void)g_theme.pfnDrawThemeTextEx(hWin, hdc, WP_CAPTION,
                                                 fActive ? CBTNS_NORMAL : CBTNS_INACTIVE,
                                                 g_themeFrameTitle, -1,
                                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS,
                                                 &rcTitle, &o);
                g_theme.pfnCloseThemeData(hWin);
            }
            else
            {
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, crText);
                DrawTextW(hdc, g_themeFrameTitle, -1, &rcTitle,
                          DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS);
            }
            if (hOld)
            {
                SelectObject(hdc, hOld);
            }
        }
    }

    /* Caption buttons -- hand-drawn uniformly (flat fill + MDL2 glyph), correct in dark and light. Only
       the buttons the window's styles enable are painted (gated below in the hit-test / here by style). */
    for (i = FB_LIGHTDARK; i <= FB_CLOSE; ++i)
    {
        if (ThemeFrameButtonEnabled(hwnd, i))
        {
            ThemeFramePaintButton(hwnd, hdc, &L, pst, i, fActive, fDark, crCaption, crText, pal.clrTextDim);
        }
    }

    /* Menu bar (re-hosted in the client). */
    ThemeFramePaintMenuBar(hwnd, hdc, &L, pst, &pal);
}

static DECLSPEC_NOINLINE BOOL ThemeFramePaintMessage(HWND hwnd, LRESULT* plr)
{
    PAINTSTRUCT ps;
    HDC         hdc;
    RECT        rcClient;

    hdc = BeginPaint(hwnd, &ps);
    if (!hdc)
    {
        return FALSE;
    }
    GetClientRect(hwnd, &rcClient);
    if (g_theme.fAnimatingBackground && ThemeWindowHasBackgroundAnimation(hwnd))
    {
        ThemePaintSolidColor(hdc, &rcClient, ThemeClientAnimationColor());
    }
    else
    {
        ThemePaintBackgroundColor(hdc, &rcClient, ThemeIsDarkMode());
    }
    ThemeFramePaint(hwnd, hdc);
    EndPaint(hwnd, &ps);
    *plr = 0;
    return TRUE;
}

static DECLSPEC_NOINLINE LRESULT ThemeFrameOnNcCalcSize(HWND hwnd, LPARAM lParam)
{
    NCCALCSIZE_PARAMS* p;
    RECT*              prc;
    UINT               dpi;
    int                cx;
    int                cy;
    int                pad;

    p   = (NCCALCSIZE_PARAMS*)lParam;
    prc = &p->rgrc[0];
    if (IsZoomed(hwnd))
    {
        /* Maximized: a top-level window's frame hangs cx/cy beyond the monitor; inset so the client is
           the visible work area (and the caption is not clipped off the top). */
        dpi = ThemeFrameDpi(hwnd);
        pad = ThemeFrameMetric(SM_CXPADDEDBORDER, dpi);
        cx  = ThemeFrameMetric(SM_CXFRAME, dpi) + pad;
        cy  = ThemeFrameMetric(SM_CYFRAME, dpi) + pad;
        prc->left   += cx;
        prc->top    += cy;
        prc->right  -= cx;
        prc->bottom -= cy;
    }
    /* Restored: leave rgrc[0] unchanged -> the entire window is client; resize borders come from the
       hit-test, the caption + buttons are flush to the window edges. */
    return 0;
}

/* Fill rg[0..N) with each top-level menu item's client rect; absent items get an empty rect. Constant
   loop bound + plain assignment + a single unconditional ReleaseDC -> no index range-check feeds a call
   (C5045-clean). */
static DECLSPEC_NOINLINE void ThemeFrameMenuRects(HWND hwnd, const FRAME_LAYOUT* pL, RECT* rg)
{
    HMENU hMenu;
    HDC   hdc;
    int   i;
    int   x;

    for (i = 0; i < THEME_MAX_MENU_TOPITEMS; ++i)
    {
        rg[i].left = 0; rg[i].top = 0; rg[i].right = 0; rg[i].bottom = 0;
    }
    hMenu = ThemeFrameMenu(hwnd);
    if (!hMenu)
    {
        return;
    }
    hdc = GetDC(hwnd);
    x   = pL->cxFrame;
    for (i = 0; i < THEME_MAX_MENU_TOPITEMS; ++i)
    {
        int w = ThemeFrameMenuItemWidth(hwnd, hMenu, i, hdc);
        if (0 == w)
        {
            break;
        }
        rg[i].left   = x;
        rg[i].right  = x + w;
        rg[i].top    = pL->capH;
        rg[i].bottom = pL->capH + pL->menuH;
        x += w;
    }
    ReleaseDC(hwnd, hdc);
}

static DECLSPEC_NOINLINE LRESULT ThemeFrameHitTest(HWND hwnd, LPARAM lParam)
{
    FRAME_LAYOUT L;
    POINT        ptScreen;
    POINT        ptClient;
    RECT         rcWin;
    RECT         rcIcon;
    LRESULT      border;
    LRESULT      menuCode;
    LONG_PTR     mask;
    LONG_PTR     style;
    BOOL         fSizable;
    int          i;
    int          found;
    int          row;
    int          col;
    BOOL         fOnResizeTop;

    ptScreen.x = (int)(short)LOWORD(lParam);
    ptScreen.y = (int)(short)HIWORD(lParam);
    GetWindowRect(hwnd, &rcWin);
    ThemeFrameComputeLayout(hwnd, &L);
    style    = GetWindowLongPtr(hwnd, GWL_STYLE);
    fSizable = (0 != (style & WS_THICKFRAME)) ? TRUE : FALSE;

    ptClient = ptScreen;
    ScreenToClient(hwnd, &ptClient);

    /* DefWindowProc / DefWndNCHitTest order: the SIZING FRAME is classified FIRST. Classify row/col +
       the top-strip flag, build the border code via a cmov ladder (no row/col-indexed array). */
    row          = 1;
    col          = 1;
    fOnResizeTop = FALSE;
    if ((ptScreen.y >= rcWin.top) && (ptScreen.y < rcWin.top + L.capH))
    {
        fOnResizeTop = (ptScreen.y < rcWin.top + L.cyFrame);
        row          = 0;
    }
    else if ((ptScreen.y < rcWin.bottom) && (ptScreen.y >= rcWin.bottom - L.cyFrame))
    {
        row = 2;
    }
    if ((ptScreen.x >= rcWin.left) && (ptScreen.x < rcWin.left + L.cxFrame))
    {
        col = 0;
    }
    else if ((ptScreen.x < rcWin.right) && (ptScreen.x >= rcWin.right - L.cxFrame))
    {
        col = 2;
    }
    border = (0 == row)
                 ? ((0 == col) ? (fSizable ? HTTOPLEFT : HTBORDER)
                               : (2 == col) ? (fSizable ? HTTOPRIGHT : HTBORDER)
                                            : ((fOnResizeTop && fSizable) ? HTTOP : HTCAPTION))
                 : (2 == row)
                       ? ((0 == col) ? (fSizable ? HTBOTTOMLEFT : HTBORDER)
                                     : (2 == col) ? (fSizable ? HTBOTTOMRIGHT : HTBORDER)
                                                  : (fSizable ? HTBOTTOM : HTBORDER))
                       : ((0 == col) ? (fSizable ? HTLEFT : HTBORDER)
                                     : (2 == col) ? (fSizable ? HTRIGHT : HTBORDER)
                                                  : HTCLIENT);

    /* Corners + the left/right/bottom edges win immediately -- THIS is why the top-right corner resizes
       even though the Close button is drawn there. (The top strip HTTOP is deferred below, so buttons /
       sysmenu still take priority over top-edge resizing.) */
    if ((HTTOPLEFT == border) || (HTTOPRIGHT == border) || (HTBOTTOMLEFT == border) ||
        (HTBOTTOMRIGHT == border) || (HTLEFT == border) || (HTRIGHT == border) ||
        (HTBOTTOM == border) || (HTBORDER == border))
    {
        return border;
    }

    /* Caption buttons (only those the styles enable). */
    for (i = FB_LIGHTDARK; i <= FB_CLOSE; ++i)
    {
        if (ThemeFrameButtonEnabled(hwnd, i) && PtInRect(&L.rcButtons[i], ptClient))
        {
            switch (i)
            {
                case FB_MIN:       return HTMINBUTTON;
                case FB_MAX:       return HTMAXBUTTON;
                case FB_CLOSE:     return HTCLOSE;
                case FB_LIGHTDARK: return HTLIGHTDARK;
                default:           break;
            }
        }
    }

    /* Window icon -> system menu (click opens the system menu, double-click closes). */
    rcIcon.left   = L.cxFrame;
    rcIcon.top    = 0;
    rcIcon.right  = L.cxFrame + GetSystemMetrics(SM_CXSMICON);
    rcIcon.bottom = L.capH;
    if (PtInRect(&rcIcon, ptClient))
    {
        return HTSYSMENU;
    }

    /* Top resize strip, after the buttons/sysmenu have had their say. */
    if (HTTOP == border)
    {
        return HTTOP;
    }

    /* Menu top-level items: accumulate the matched index by ARITHMETIC (no index range-check feeding a
       call/return -> C5045-clean); otherwise HTCAPTION/HTCLIENT (border). */
    ThemeFrameMenuRects(hwnd, &L, g_themeFrameMenuRc);
    found = -1;
    for (i = 0; i < THEME_MAX_MENU_TOPITEMS; ++i)
    {
        int in = (PtInRect(&g_themeFrameMenuRc[i], ptClient) != 0) ? 1 : 0;
        found += in * (i - found);   /* in==1 -> found=i; in==0 -> unchanged (branchless cmov) */
    }
    menuCode = (LRESULT)(HTMENUITEM0 + found);
    mask     = (LONG_PTR)found >> (int)(8 * sizeof(LONG_PTR) - 1);
    return (LRESULT)(((LONG_PTR)border & mask) | ((LONG_PTR)menuCode & ~mask));
}

static FORCEINLINE int ThemeFrameButtonFromHit(WPARAM hit)
{
    switch (hit)
    {
        case HTMINBUTTON: return FB_MIN;
        case HTMAXBUTTON: return FB_MAX;
        case HTCLOSE:     return FB_CLOSE;
        case HTLIGHTDARK: return FB_LIGHTDARK;
        default:          return FB_NONE;
    }
}

/* Map a WM_NCHITTEST result to a top-level menu index, or -1. Constant-bound equality scan (no range
   check on an index feeding a call) keeps it C5045-clean. */
static FORCEINLINE int ThemeFrameMenuFromHit(WPARAM hit)
{
    int k;

    for (k = 0; k < THEME_MAX_MENU_TOPITEMS; ++k)
    {
        if (hit == (WPARAM)(HTMENUITEM0 + k))
        {
            return k;
        }
    }
    return -1;
}

static DECLSPEC_NOINLINE void ThemeFrameButtonAction(HWND hwnd, int id)
{
    switch (id)
    {
        case FB_MIN:       (void)PostMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0); break;
        case FB_MAX:       (void)PostMessage(hwnd, WM_SYSCOMMAND, IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0); break;
        case FB_CLOSE:     (void)PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0); break;
        case FB_LIGHTDARK: ThemeToggleDarkMode(hwnd); break;
        default:           break;
    }
}

static DECLSPEC_NOINLINE void ThemeFrameTrackLeave(HWND hwnd, FRAME_STATE* pst)
{
    TRACKMOUSEEVENT tme;

    if (pst->fTracking)
    {
        return;
    }
    SecureZeroMemory(&tme, sizeof(tme));
    tme.cbSize    = (DWORD)sizeof(tme);
    tme.dwFlags   = TME_LEAVE | TME_NONCLIENT;
    tme.hwndTrack = hwnd;
    if (TrackMouseEvent(&tme))
    {
        pst->fTracking = TRUE;
    }
}

static DECLSPEC_NOINLINE void ThemeFrameOnNcMouseMove(HWND hwnd, WPARAM hit)
{
    FRAME_STATE* pst;
    int          idHot;
    int          iMenu;

    pst = ThemeFrameFind(hwnd);
    if (!pst)
    {
        return;
    }
    idHot = ThemeFrameButtonFromHit(hit);
    iMenu = ThemeFrameMenuFromHit(hit);
    ThemeFrameTrackLeave(hwnd, pst);
    if ((pst->idHot != idHot) || (pst->iHotMenu != iMenu))
    {
        pst->idHot    = idHot;
        pst->iHotMenu = iMenu;
        ThemeFrameInvalidateBands(hwnd);
    }
}

static DECLSPEC_NOINLINE void ThemeFrameOnNcMouseLeave(HWND hwnd)
{
    FRAME_STATE* pst;

    pst = ThemeFrameFind(hwnd);
    if (!pst)
    {
        return;
    }
    pst->fTracking = FALSE;
    if ((pst->idHot != FB_NONE) || (pst->idPressed != FB_NONE) || (pst->iHotMenu != -1))
    {
        pst->idHot     = FB_NONE;
        pst->idPressed = FB_NONE;
        pst->iHotMenu  = -1;
        ThemeFrameInvalidateBands(hwnd);
    }
}

static DECLSPEC_NOINLINE void ThemeFrameTrackMenu(HWND hwnd, int i)
{
    FRAME_LAYOUT L;
    FRAME_STATE* pst;
    HMENU        hMenu;
    HMENU        hSub;
    POINT        pt;
    HDC          hdc;
    int          x;
    int          j;

    hMenu = ThemeFrameMenu(hwnd);
    if (!hMenu)
    {
        return;
    }
    hSub = GetSubMenu(hMenu, i);
    if (!hSub)
    {
        return;
    }
    ThemeFrameComputeLayout(hwnd, &L);
    hdc = GetDC(hwnd);
    /* Sum item widths up to the clicked index. Constant loop bound + equality stop (C5045-safe). */
    x   = L.cxFrame;
    for (j = 0; j < THEME_MAX_MENU_TOPITEMS; ++j)
    {
        if (j == i)
        {
            break;
        }
        x += ThemeFrameMenuItemWidth(hwnd, hMenu, j, hdc);
    }
    ReleaseDC(hwnd, hdc);

    pt.x = x;
    pt.y = L.capH + L.menuH;
    ClientToScreen(hwnd, &pt);

    pst = ThemeFrameFind(hwnd);
    if (pst)
    {
        pst->iHotMenu = i;
    }
    ThemeFrameInvalidateBands(hwnd);
    (void)TrackPopupMenu(hSub, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    pst = ThemeFrameFind(hwnd);
    if (pst)
    {
        pst->iHotMenu = -1;
    }
    ThemeFrameInvalidateBands(hwnd);
}

/* Constant-bound count of top-level menu items (width 0 marks the end -> C5045-safe). */
static FORCEINLINE int ThemeFrameMenuCount(HWND hwnd)
{
    HMENU hMenu;
    HDC   hdc;
    int   i;
    int   n;

    hMenu = ThemeFrameMenu(hwnd);
    if (!hMenu)
    {
        return 0;
    }
    hdc = GetDC(hwnd);
    n   = 0;
    for (i = 0; i < THEME_MAX_MENU_TOPITEMS; ++i)
    {
        if (0 == ThemeFrameMenuItemWidth(hwnd, hMenu, i, hdc))
        {
            break;
        }
        n = i + 1;
    }
    ReleaseDC(hwnd, hdc);
    return n;
}

static FORCEINLINE LRESULT ThemeFrameHitScreen(HWND hwnd, int x, int y)
{
    LPARAM lp;

    lp = (LPARAM)((((DWORD)y & 0xFFFFu) << 16) | ((DWORD)x & 0xFFFFu));
    return ThemeFrameHitTest(hwnd, lp);
}

/* Open the window's system menu (DefWindowProc parity for the icon / Alt+Space / right-click caption). */
static DECLSPEC_NOINLINE void ThemeFrameSysMenu(HWND hwnd, int xScreen, int yScreen)
{
    HMENU hSys;
    int   cmd;

    hSys = GetSystemMenu(hwnd, FALSE);
    if (!hSys)
    {
        return;
    }
    cmd = (int)TrackPopupMenu(hSys, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                              xScreen, yScreen, 0, hwnd, NULL);
    if (0 != cmd)
    {
        (void)PostMessage(hwnd, WM_SYSCOMMAND, (WPARAM)cmd, 0);
    }
}

/* Top-level item index whose &-mnemonic matches ch, or -1. Returning the loop index directly from inside
   a constant-bound loop (no >=0 guard feeding a call) is C5045-clean. */
static DECLSPEC_NOINLINE int ThemeFrameMnemonic(HWND hwnd, WCHAR ch)
{
    HMENU         hMenu;
    WCHAR         sz[64];
    MENUITEMINFOW mii;
    WCHAR         up;
    int           i;
    int           j;

    hMenu = ThemeFrameMenu(hwnd);
    if (!hMenu)
    {
        return -1;
    }
    up = (WCHAR)(DWORD_PTR)CharUpperW((LPWSTR)(DWORD_PTR)ch);
    for (i = 0; i < THEME_MAX_MENU_TOPITEMS; ++i)
    {
        sz[0] = 0;
        SecureZeroMemory(&mii, sizeof(mii));
        mii.cbSize     = sizeof(mii);
        mii.fMask      = MIIM_STRING;
        mii.dwTypeData = sz;
        mii.cch        = (UINT)(ARRAYSIZE(sz) - 1);
        if (!GetMenuItemInfoW(hMenu, (UINT)i, TRUE, &mii))
        {
            break;
        }
        for (j = 0; (j + 1 < (int)ARRAYSIZE(sz)) && (0 != sz[j]); ++j)
        {
            if ((L'&' == sz[j]) && (0 != sz[j + 1]) && (L'&' != sz[j + 1]))
            {
                if ((WCHAR)(DWORD_PTR)CharUpperW((LPWSTR)(DWORD_PTR)sz[j + 1]) == up)
                {
                    return i;
                }
                break;
            }
        }
    }
    return -1;
}

/* While a keyboard-opened popup is up, Left/Right at the menu-bar level should move to the adjacent
   top-level menu. TrackPopupMenu does not do this, so a thread MSGF_MENU filter ends the current popup
   and records the direction; ThemeFrameMenuLoop reopens the neighbour. */
static int   g_themeMenuLoopDir;   /* GUI thread only: -1/0/+1 set by the filter */
static HHOOK g_themeMenuHook;

static LRESULT CALLBACK ThemeMenuMsgFilter(int code, WPARAM wParam, LPARAM lParam)
{
    if (MSGF_MENU == code)
    {
        const MSG* pm = (const MSG*)lParam;
        if (pm && (WM_KEYDOWN == pm->message))
        {
            if (VK_LEFT == pm->wParam)  { g_themeMenuLoopDir = -1; EndMenu(); return 1; }
            if (VK_RIGHT == pm->wParam) { g_themeMenuLoopDir =  1; EndMenu(); return 1; }
        }
    }
    return CallNextHookEx(g_themeMenuHook, code, wParam, lParam);
}

/* Open the currently-active top-level menu (FRAME_STATE.iMenuActive) and keep it open across Left/Right
   navigation. The active index is READ FRESH from state inside a constant-bound equality loop (the loop
   counter feeds TrackMenu; no range-checked local index reaches the call) -> C5045-clean. */
static DECLSPEC_NOINLINE void ThemeFrameOpenActiveMenu(HWND hwnd)
{
    FRAME_STATE* pst;
    int          n;
    int          i;

    pst = ThemeFrameFind(hwnd);
    if (!pst)
    {
        return;
    }
    n = ThemeFrameMenuCount(hwnd);
    if (n <= 0)
    {
        return;
    }
    for (;;)
    {
        g_themeMenuLoopDir = 0;
        g_themeMenuHook    = SetWindowsHookExW(WH_MSGFILTER, ThemeMenuMsgFilter, NULL, GetCurrentThreadId());
        for (i = 0; i < THEME_MAX_MENU_TOPITEMS; ++i)
        {
            if (i == pst->iMenuActive)
            {
                ThemeFrameTrackMenu(hwnd, i);
                break;
            }
        }
        if (g_themeMenuHook)
        {
            (void)UnhookWindowsHookEx(g_themeMenuHook);
            g_themeMenuHook = NULL;
        }
        if (0 == g_themeMenuLoopDir)
        {
            break;   /* a command was chosen or the popup was cancelled */
        }
        pst->iMenuActive = ((pst->iMenuActive + g_themeMenuLoopDir) % n + n) % n;  /* assignment, not a call */
    }
}

static DECLSPEC_NOINLINE BOOL ThemeFrameOnNcLButtonDown(HWND hwnd, WPARAM hit, LRESULT* plr)
{
    FRAME_STATE* pst;
    int          id;
    int          k;

    pst = ThemeFrameFind(hwnd);
    if (!pst)
    {
        return FALSE;
    }
    id = ThemeFrameButtonFromHit(hit);
    if (FB_NONE != id)
    {
        /* Press a caption button: capture the mouse so we can cancel on drag-off and fire on the up. */
        pst->idPressed  = id;
        pst->idHot      = id;
        pst->fCapturing = TRUE;
        (void)SetCapture(hwnd);
        ThemeFrameInvalidateBands(hwnd);
        *plr = 0;
        return TRUE;
    }
    if (HTSYSMENU == hit)
    {
        RECT         rcWin;
        FRAME_LAYOUT L;

        GetWindowRect(hwnd, &rcWin);
        ThemeFrameComputeLayout(hwnd, &L);
        ThemeFrameSysMenu(hwnd, rcWin.left + L.cxFrame, rcWin.top + L.capH);
        *plr = 0;
        return TRUE;
    }
    /* Menu top-level click -> open its popup. Constant-bound equality scan (no range-checked index
       feeding the call) keeps this C5045-clean. */
    for (k = 0; k < THEME_MAX_MENU_TOPITEMS; ++k)
    {
        if (hit == (WPARAM)(HTMENUITEM0 + k))
        {
            ThemeFrameTrackMenu(hwnd, k);
            *plr = 0;
            return TRUE;
        }
    }
    return FALSE;  /* HTCAPTION / borders -> DefWindowProc moves/sizes. */
}

static DECLSPEC_NOINLINE BOOL ThemeFrameOnNcLButtonDblClk(HWND hwnd, WPARAM hit, LRESULT* plr)
{
    if (HTSYSMENU == hit)
    {
        (void)PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);  /* icon double-click closes */
        *plr = 0;
        return TRUE;
    }
    return FALSE;  /* caption double-click -> DefWindowProc maximizes/restores */
}

static DECLSPEC_NOINLINE BOOL ThemeFrameOnNcRButtonUp(HWND hwnd, WPARAM hit, LPARAM lParam, LRESULT* plr)
{
    if ((HTCAPTION == hit) || (HTSYSMENU == hit))
    {
        ThemeFrameSysMenu(hwnd, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
        *plr = 0;
        return TRUE;
    }
    return FALSE;
}

static DECLSPEC_NOINLINE void ThemeFrameOnMouseMoveCapture(HWND hwnd, LPARAM lParam)
{
    FRAME_STATE* pst;
    POINT        pt;
    int          id;

    pst = ThemeFrameFind(hwnd);
    if (!pst || !pst->fCapturing)
    {
        return;
    }
    pt.x = (int)(short)LOWORD(lParam);
    pt.y = (int)(short)HIWORD(lParam);
    (void)ClientToScreen(hwnd, &pt);
    id = ThemeFrameButtonFromHit((WPARAM)ThemeFrameHitScreen(hwnd, pt.x, pt.y));
    if (pst->idHot != id)
    {
        pst->idHot = id;
        ThemeFrameInvalidateBands(hwnd);
    }
}

static DECLSPEC_NOINLINE BOOL ThemeFrameOnLButtonUpCapture(HWND hwnd, LPARAM lParam, LRESULT* plr)
{
    FRAME_STATE* pst;
    POINT        pt;
    int          id;
    int          pressed;

    pst = ThemeFrameFind(hwnd);
    if (!pst || !pst->fCapturing)
    {
        return FALSE;
    }
    pressed = pst->idPressed;
    pt.x    = (int)(short)LOWORD(lParam);
    pt.y    = (int)(short)HIWORD(lParam);
    (void)ClientToScreen(hwnd, &pt);
    id = ThemeFrameButtonFromHit((WPARAM)ThemeFrameHitScreen(hwnd, pt.x, pt.y));

    pst->fCapturing = FALSE;
    pst->idPressed  = FB_NONE;
    (void)ReleaseCapture();
    ThemeFrameInvalidateBands(hwnd);
    if ((FB_NONE != pressed) && (pressed == id))
    {
        ThemeFrameButtonAction(hwnd, pressed);
    }
    *plr = 0;
    return TRUE;
}

static DECLSPEC_NOINLINE void ThemeFrameOnCaptureChanged(HWND hwnd)
{
    FRAME_STATE* pst;

    pst = ThemeFrameFind(hwnd);
    if (!pst)
    {
        return;
    }
    if (pst->fCapturing)
    {
        pst->fCapturing = FALSE;
        pst->idPressed  = FB_NONE;
        ThemeFrameInvalidateBands(hwnd);
    }
}

static DECLSPEC_NOINLINE void ThemeFrameEnterMenuMode(HWND hwnd, int start)
{
    FRAME_STATE* pst;

    pst = ThemeFrameFind(hwnd);
    if (!pst || (ThemeFrameMenuCount(hwnd) <= 0))
    {
        return;
    }
    pst->iMenuActive = (start < 0) ? 0 : start;
    pst->fShowAccel  = TRUE;
    ThemeFrameInvalidateBands(hwnd);
}

static DECLSPEC_NOINLINE void ThemeFrameExitMenuMode(HWND hwnd)
{
    FRAME_STATE* pst;

    pst = ThemeFrameFind(hwnd);
    if (!pst)
    {
        return;
    }
    if ((pst->iMenuActive >= 0) || pst->fShowAccel)
    {
        pst->iMenuActive = -1;
        pst->fShowAccel  = FALSE;
        ThemeFrameInvalidateBands(hwnd);
    }
}

static DECLSPEC_NOINLINE BOOL ThemeFrameOnSysKeyDown(HWND hwnd, WPARAM vk, LRESULT* plr)
{
    FRAME_STATE* pst;

    if (VK_F10 != vk)
    {
        return FALSE;  /* Alt alone -> keyup; Alt+char -> WM_SYSCHAR */
    }
    pst = ThemeFrameFind(hwnd);
    if (pst && (pst->iMenuActive >= 0))
    {
        ThemeFrameExitMenuMode(hwnd);
    }
    else
    {
        ThemeFrameEnterMenuMode(hwnd, 0);
    }
    *plr = 0;
    return TRUE;
}

static DECLSPEC_NOINLINE BOOL ThemeFrameOnSysKeyUp(HWND hwnd, WPARAM vk, LRESULT* plr)
{
    FRAME_STATE* pst;

    if (VK_MENU != vk)
    {
        return FALSE;
    }
    pst = ThemeFrameFind(hwnd);
    if (pst && (pst->iMenuActive >= 0))
    {
        ThemeFrameExitMenuMode(hwnd);
    }
    else
    {
        ThemeFrameEnterMenuMode(hwnd, 0);
    }
    *plr = 0;
    return TRUE;
}

static DECLSPEC_NOINLINE BOOL ThemeFrameOnSysChar(HWND hwnd, WPARAM ch, LRESULT* plr)
{
    FRAME_STATE* pst;
    int          idx;

    idx = ThemeFrameMnemonic(hwnd, (WCHAR)ch);
    pst = ThemeFrameFind(hwnd);
    if (pst)
    {
        pst->iMenuActive = idx;          /* assignment (idx may be -1; OpenActiveMenu then no-ops) */
        pst->fShowAccel  = TRUE;
    }
    ThemeFrameOpenActiveMenu(hwnd);
    ThemeFrameExitMenuMode(hwnd);
    *plr = 0;
    /* Consume only when a mnemonic matched -- branchless (sign bit), so no range-checked index reaches
       the return -> C5045-clean. idx in [-1, N): (idx>>31)+1 == 1 iff idx>=0. */
    return (BOOL)((idx >> 31) + 1);
}

static DECLSPEC_NOINLINE BOOL ThemeFrameOnKeyDown(HWND hwnd, WPARAM vk, LRESULT* plr)
{
    FRAME_STATE* pst;
    int          n;
    int          active;

    pst = ThemeFrameFind(hwnd);
    if (!pst || (pst->iMenuActive < 0))
    {
        return FALSE;  /* only while the keyboard menu cue is up */
    }
    n = ThemeFrameMenuCount(hwnd);
    if (n <= 0)
    {
        ThemeFrameExitMenuMode(hwnd);
        return FALSE;
    }
    active = pst->iMenuActive;
    switch (vk)
    {
        case VK_LEFT:
            pst->iMenuActive = (active - 1 + n) % n;
            ThemeFrameInvalidateBands(hwnd);
            *plr = 0;
            return TRUE;
        case VK_RIGHT:
            pst->iMenuActive = (active + 1) % n;
            ThemeFrameInvalidateBands(hwnd);
            *plr = 0;
            return TRUE;
        case VK_DOWN:
        case VK_RETURN:
            /* iMenuActive is already the target; OpenActiveMenu reads it fresh (no index passed). */
            ThemeFrameOpenActiveMenu(hwnd);
            ThemeFrameExitMenuMode(hwnd);
            *plr = 0;
            return TRUE;
        case VK_ESCAPE:
            ThemeFrameExitMenuMode(hwnd);
            *plr = 0;
            return TRUE;
        default:
            return FALSE;
    }
}

static DECLSPEC_NOINLINE void ThemeFrameExtend(HWND hwnd)
{
    THEME_MARGINS m;

    if (!g_theme.pfnDwmExtendFrameIntoClientArea)
    {
        return;
    }
    /* A 1px top sheet-of-glass restores the DWM drop shadow + the top resize-border hairline while we own
       the rest of the caption. (Shadow extent is a tuning knob.) */
    m.cxLeftWidth    = 0;
    m.cxRightWidth   = 0;
    m.cyTopHeight    = 1;
    m.cyBottomHeight = 0;
    (void)g_theme.pfnDwmExtendFrameIntoClientArea(hwnd, &m);
}

void WINAPI ThemeEnableCustomFrame(HWND hwnd, BOOL fEnable)
{
    FRAME_STATE* pst;
    RECT         rc;
    UINT         i;

    ThemeResolve();
    if (fEnable)
    {
        pst = ThemeFrameFind(hwnd);
        if (!pst && (g_theme.cFrames < THEME_MAX_TOPLEVELS))
        {
            pst              = &g_theme.rgFrames[g_theme.cFrames++];
            pst->hwnd        = hwnd;
            pst->hMenu       = GetMenu(hwnd);
            pst->idHot       = FB_NONE;
            pst->idPressed   = FB_NONE;
            pst->iHotMenu    = -1;
            pst->iMenuActive = -1;
            pst->fTracking   = FALSE;
            pst->fCapturing  = FALSE;
            pst->fShowAccel  = FALSE;
            pst->fReserved   = FALSE;
            /* Detach the menu so the system reserves no NC band and never tracks a phantom bar; we own
               its draw and popup tracking from here (accelerators still post WM_COMMAND independently). */
            if (pst->hMenu)
            {
                (void)SetMenu(hwnd, NULL);
            }
        }
        ThemeFrameExtend(hwnd);
        GetWindowRect(hwnd, &rc);
        (void)SetWindowPos(hwnd, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                           SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, NULL, TRUE);
        return;
    }
    for (i = 0u; i < g_theme.cFrames; ++i)
    {
        if (g_theme.rgFrames[i].hwnd == hwnd)
        {
            if (g_theme.rgFrames[i].hMenu)
            {
                (void)SetMenu(hwnd, g_theme.rgFrames[i].hMenu);  /* re-attach on opt-out */
            }
            while ((i + 1u) < g_theme.cFrames)
            {
                g_theme.rgFrames[i] = g_theme.rgFrames[i + 1u];
                ++i;
            }
            --g_theme.cFrames;
            break;
        }
    }
    GetWindowRect(hwnd, &rc);
    (void)SetWindowPos(hwnd, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                       SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

DECLSPEC_NOINLINE void WINAPI ThemeToggleDarkMode(HWND hwnd)
{
    BOOL fOldEffective;
    BOOL fNewRequested;
    BOOL fNewEffective;
    BOOL fStarted;

    UNREFERENCED_PARAMETER(hwnd);
    ThemeResolve();
    fOldEffective = g_theme.fEffectiveDark;
    fNewRequested = !(g_theme.fManualOverrideActive ? g_theme.fManualDark : ThemeAppsUseDarkMode());
    g_theme.fManualOverrideActive = TRUE;
    g_theme.fManualDark           = fNewRequested;
    fNewEffective = fNewRequested && g_theme.fDarkCapable;

    fStarted = ThemeStartBackgroundTransition(fOldEffective, fNewEffective);
    ThemeSetProcessDarkModeAllowed(TRUE);
    g_theme.fRequestedDark = fNewRequested;
    g_theme.fEffectiveDark = fNewEffective;

    ThemeApplyRegisteredFrames(fNewEffective);
    if (fStarted && g_theme.fAnimatingBackground)
    {
        g_theme.dwAnimationStartTick = GetTickCount();
        g_theme.dwAnimationSnapTick  = g_theme.dwAnimationStartTick;
        g_theme.dwCaptionProgress    = 0u;
    }
    ThemeSetRegisteredClassBrushes(fNewEffective);
    ThemeSetRegisteredControlThemes(fNewEffective);
    ThemeInvalidateRegisteredWindows();
}

BOOL WINAPI ThemeCustomFrameHandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plr)
{
    if (!plr)
    {
        return FALSE;
    }
    if (!ThemeFrameFind(hwnd))
    {
        return FALSE;  /* not a custom-frame window */
    }
    switch (uMsg)
    {
        case WM_NCCALCSIZE:
            if (wParam)
            {
                *plr = ThemeFrameOnNcCalcSize(hwnd, lParam);
                return TRUE;
            }
            return FALSE;

        case WM_NCHITTEST:
            *plr = ThemeFrameHitTest(hwnd, lParam);
            return TRUE;

        case WM_NCPAINT:
            *plr = 0;            /* DWM draws the shadow + resize borders; nothing standard to paint. */
            return TRUE;

        case WM_NCACTIVATE:
            /* lParam == -1 means "do not change the non-client area" -> suppress the repaint (parity). */
            if ((LPARAM)-1 != lParam)
            {
                ThemeFrameInvalidateBands(hwnd);
            }
            *plr = (LRESULT)TRUE;  /* we own the caption; keep it active-looking */
            return TRUE;

        case WM_PAINT:
            return ThemeFramePaintMessage(hwnd, plr);

        /* Let DefWindowProc store the new title/icon, then repaint our caption. The LITERAL message id --
           not the switch-narrowed uMsg -- feeds DefWindowProc so no range-checked value reaches it
           (C5045-safe; the same idiom ThemeHandleWindowMessage uses for WM_NCACTIVATE/WM_NCPAINT). */
        case WM_SETTEXT:
            *plr = DefWindowProc(hwnd, WM_SETTEXT, wParam, lParam);
            ThemeFrameInvalidateBands(hwnd);
            return TRUE;

        case WM_SETICON:
            *plr = DefWindowProc(hwnd, WM_SETICON, wParam, lParam);
            ThemeFrameInvalidateBands(hwnd);
            return TRUE;

        case WM_ACTIVATE:
        case WM_DWMCOMPOSITIONCHANGED:
            ThemeFrameExtend(hwnd);
            ThemeFrameInvalidateBands(hwnd);
            return FALSE;  /* let default activation proceed too */

        case WM_NCMOUSEMOVE:
            ThemeFrameOnNcMouseMove(hwnd, wParam);
            return FALSE;

        case WM_NCMOUSELEAVE:
            ThemeFrameOnNcMouseLeave(hwnd);
            return FALSE;

        case WM_NCLBUTTONDOWN:
            return ThemeFrameOnNcLButtonDown(hwnd, wParam, plr);

        case WM_NCLBUTTONDBLCLK:
            return ThemeFrameOnNcLButtonDblClk(hwnd, wParam, plr);

        case WM_NCRBUTTONUP:
            return ThemeFrameOnNcRButtonUp(hwnd, wParam, lParam, plr);

        /* Caption-button press uses mouse capture, so the drag/release arrive as client messages. */
        case WM_MOUSEMOVE:
            ThemeFrameOnMouseMoveCapture(hwnd, lParam);
            return FALSE;

        case WM_LBUTTONUP:
            return ThemeFrameOnLButtonUpCapture(hwnd, lParam, plr);

        case WM_CAPTURECHANGED:
            ThemeFrameOnCaptureChanged(hwnd);
            return FALSE;

        /* Keyboard menu activation (Alt / F10 / mnemonics / arrows). */
        case WM_SYSKEYDOWN:
            return ThemeFrameOnSysKeyDown(hwnd, wParam, plr);

        case WM_SYSKEYUP:
            return ThemeFrameOnSysKeyUp(hwnd, wParam, plr);

        case WM_SYSCHAR:
            return ThemeFrameOnSysChar(hwnd, wParam, plr);

        case WM_KEYDOWN:
            return ThemeFrameOnKeyDown(hwnd, wParam, plr);

        default:
            return FALSE;
    }
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
