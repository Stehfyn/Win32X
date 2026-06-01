/*
 * windowsx2.h -- message crackers and message-dispatch helpers that <windowsx.h> omits.
 *
 * Strictly follows the <windowsx.h> convention: for a message WM_FOO whose handler is
 * `T Cls_OnFoo(HWND, ...)`, this header provides
 *     HANDLE_WM_FOO(hwnd, wParam, lParam, fn)   -- unpacks (wParam,lParam) -> fn(...) -> LRESULT
 *     FORWARD_WM_FOO(hwnd, ..., fn)             -- packs fn-style args -> fn(hwnd, WM_FOO, wParam, lParam)
 * Each cracker carries the exact handler signature in a comment above it, and every macro is
 * #ifndef-guarded so this header composes with <windowsx.h> (include order independent).
 *
 * Covers two families <windowsx.h> never had:
 *   - the undocumented "UAH" (User Apphack) menu-paint messages (0x0090-0x0095) used to owner-draw
 *     a top-level menu bar (the only way to dark-theme it);
 *   - the theme/appearance broadcast messages (WM_SETTINGCHANGE/THEMECHANGED/SYSCOLORCHANGE/
 *     DWMCOLORIZATIONCOLORCHANGED) a window must crack to retheme itself live.
 */
#ifndef WINDOWSX2_H
#define WINDOWSX2_H

#include <windows.h>
#include <windowsx.h>

/* ------------------------------------------------------------------------------------------------
 * Undocumented UAH menu-bar messages. The window manager sends these to a top-level window that owns
 * a menu bar so the app can paint the bar itself (DefWindowProc paints the stock light bar). Values
 * and payload layouts are stable across Windows 10/11 but ship in no SDK header.
 * ---------------------------------------------------------------------------------------------- */
#ifndef WM_UAHDESTROYWINDOW
#define WM_UAHDESTROYWINDOW    0x0090
#endif
#ifndef WM_UAHDRAWMENU
#define WM_UAHDRAWMENU         0x0091  /* lParam -> UAHMENU:        paint the whole bar background   */
#endif
#ifndef WM_UAHDRAWMENUITEM
#define WM_UAHDRAWMENUITEM     0x0092  /* lParam -> UAHDRAWMENUITEM: paint one bar item              */
#endif
#ifndef WM_UAHINITMENU
#define WM_UAHINITMENU         0x0093
#endif
#ifndef WM_UAHMEASUREMENUITEM
#define WM_UAHMEASUREMENUITEM  0x0094  /* lParam -> UAHMEASUREMENUITEM: size one bar item            */
#endif
#ifndef WM_UAHNCPAINTMENUPOPUP
#define WM_UAHNCPAINTMENUPOPUP 0x0095
#endif

typedef union tagUAHMENUITEMMETRICS
{
    struct { DWORD cx; DWORD cy; } rgsizeBar[2];
    struct { DWORD cx; DWORD cy; } rgsizePopup[4];
} UAHMENUITEMMETRICS;

typedef struct tagUAHMENUPOPUPMETRICS
{
    DWORD rgcx[4];
    DWORD fUpdateMaxWidths : 2;
} UAHMENUPOPUPMETRICS;

typedef struct tagUAHMENU
{
    HMENU hmenu;
    HDC   hdc;
    DWORD dwFlags;
    DWORD dwReserved0;
} UAHMENU;

typedef struct tagUAHMENUITEM
{
    int                 iPosition;
    UAHMENUITEMMETRICS  umim;
    UAHMENUPOPUPMETRICS umpm;
} UAHMENUITEM;

typedef struct tagUAHDRAWMENUITEM
{
    DRAWITEMSTRUCT dis;
    UAHMENU        um;
    UAHMENUITEM    umi;
} UAHDRAWMENUITEM;

typedef struct tagUAHMEASUREMENUITEM
{
    MEASUREITEMSTRUCT mis;
    UAHMENU           um;
    UAHMENUITEM       umi;
} UAHMEASUREMENUITEM;

#ifndef HANDLE_WM_UAHDRAWMENU
/* void Cls_OnUahDrawMenu(HWND hwnd, const UAHMENU* pUDM) -- returns TRUE (handled). */
#define HANDLE_WM_UAHDRAWMENU(hwnd, wParam, lParam, fn)                                              \
    ((void)(wParam), (fn)((hwnd), (const UAHMENU*)(lParam)), (LRESULT)TRUE)
#endif

#ifndef HANDLE_WM_UAHDRAWMENUITEM
/* void Cls_OnUahDrawMenuItem(HWND hwnd, const UAHDRAWMENUITEM* pUDMI) -- returns TRUE (handled). */
#define HANDLE_WM_UAHDRAWMENUITEM(hwnd, wParam, lParam, fn)                                          \
    ((void)(wParam), (fn)((hwnd), (const UAHDRAWMENUITEM*)(lParam)), (LRESULT)TRUE)
#endif

#ifndef HANDLE_WM_UAHMEASUREMENUITEM
/* BOOL Cls_OnUahMeasureMenuItem(HWND hwnd, UAHMEASUREMENUITEM* pUMMI) */
#define HANDLE_WM_UAHMEASUREMENUITEM(hwnd, wParam, lParam, fn)                                       \
    ((void)(wParam), (LRESULT)(BOOL)(fn)((hwnd), (UAHMEASUREMENUITEM*)(lParam)))
#endif

/* ------------------------------------------------------------------------------------------------
 * Appearance / theme broadcast messages. <windowsx.h> cracks none of these.
 * ---------------------------------------------------------------------------------------------- */
#ifndef HANDLE_WM_SETTINGCHANGE
/* void Cls_OnSettingChange(HWND hwnd, UINT flags, LPCTSTR pszSection) */
#define HANDLE_WM_SETTINGCHANGE(hwnd, wParam, lParam, fn)                                            \
    ((fn)((hwnd), (UINT)(wParam), (LPCTSTR)(lParam)), 0L)
#endif
#ifndef FORWARD_WM_SETTINGCHANGE
/* void Cls_OnSettingChange(HWND hwnd, UINT flags, LPCTSTR pszSection) */
#define FORWARD_WM_SETTINGCHANGE(hwnd, flags, pszSection, fn)                                        \
    ((void)(fn)((hwnd), WM_SETTINGCHANGE, (WPARAM)(UINT)(flags), (LPARAM)(LPCTSTR)(pszSection)))
#endif

#ifndef HANDLE_WM_THEMECHANGED
/* void Cls_OnThemeChanged(HWND hwnd) */
#define HANDLE_WM_THEMECHANGED(hwnd, wParam, lParam, fn)                                             \
    ((void)(wParam), (void)(lParam), (fn)(hwnd), 0L)
#endif
#ifndef FORWARD_WM_THEMECHANGED
/* void Cls_OnThemeChanged(HWND hwnd) */
#define FORWARD_WM_THEMECHANGED(hwnd, fn) ((void)(fn)((hwnd), WM_THEMECHANGED, 0, 0))
#endif

#ifndef HANDLE_WM_SYSCOLORCHANGE
/* void Cls_OnSysColorChange(HWND hwnd) */
#define HANDLE_WM_SYSCOLORCHANGE(hwnd, wParam, lParam, fn)                                           \
    ((void)(wParam), (void)(lParam), (fn)(hwnd), 0L)
#endif
#ifndef FORWARD_WM_SYSCOLORCHANGE
/* void Cls_OnSysColorChange(HWND hwnd) */
#define FORWARD_WM_SYSCOLORCHANGE(hwnd, fn) ((void)(fn)((hwnd), WM_SYSCOLORCHANGE, 0, 0))
#endif

#ifndef WM_DWMCOLORIZATIONCOLORCHANGED
#define WM_DWMCOLORIZATIONCOLORCHANGED 0x0320
#endif
#ifndef HANDLE_WM_DWMCOLORIZATIONCOLORCHANGED
/* void Cls_OnDwmColorizationColorChanged(HWND hwnd, DWORD dwColor, BOOL fOpaqueBlend) */
#define HANDLE_WM_DWMCOLORIZATIONCOLORCHANGED(hwnd, wParam, lParam, fn)                              \
    ((fn)((hwnd), (DWORD)(wParam), (BOOL)(lParam)), 0L)
#endif
#ifndef FORWARD_WM_DWMCOLORIZATIONCOLORCHANGED
/* void Cls_OnDwmColorizationColorChanged(HWND hwnd, DWORD dwColor, BOOL fOpaqueBlend) */
#define FORWARD_WM_DWMCOLORIZATIONCOLORCHANGED(hwnd, dwColor, fOpaqueBlend, fn)                      \
    ((void)(fn)((hwnd),                                                                              \
                WM_DWMCOLORIZATIONCOLORCHANGED,                                                      \
                (WPARAM)(DWORD)(dwColor),                                                            \
                (LPARAM)(BOOL)(fOpaqueBlend)))
#endif

#endif /* WINDOWSX2_H */
