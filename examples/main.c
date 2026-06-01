/*
 * main.c -- WindowsProject: the classic Win32 desktop application (menu, accelerators, About
 * dialog, icons, string table) reborn as a Win32X client, now theme-aware. It tracks the system
 * light/dark setting and dresses the whole window to match: the title-bar frame (DWM immersive dark
 * mode), the client background (theme-chosen class brush), the menu bar (owner-drawn via the
 * undocumented UAH messages), the drop-down menus (app dark-mode opt-in), and the About dialog
 * (DarkMode_Explorer control theme + dark WM_CTLCOLOR* brushes). Entered through Win32X's _tWinMainEx
 * and shown with the DPI-aware ShowWindowEx(SWX_SHOWSTARTUP) first show. CRT-free.
 */

#include "framework.h"   /* sets up the CRT-free environment (RTC/GS off) before any code */
#include "WindowsProject.h"

#define MAX_LOADSTRING 100
#define WMAPP_THEMECHANGED (WM_APP + 1) /* private: deferred retheme, posted from WM_SETTINGCHANGE */

/* Globals (file-scope; zero-initialized by the loader -- no CRT startup needed). */
static HINSTANCE g_hInst;                          /* current instance                  */
static WCHAR     g_szTitle[MAX_LOADSTRING];        /* title bar text                    */
static WCHAR     g_szWindowClass[MAX_LOADSTRING];  /* main window class name            */
static BOOL      g_fDark;                           /* current system app theme is dark  */
static HWND      g_hAboutDlg;                        /* the open About dialog, else NULL   */

/* Forward declarations. */
static ATOM             MyRegisterClass(HINSTANCE hInstance);
static BOOL             InitInstance(HINSTANCE hInstance);
static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK About(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static BOOL    CALLBACK AboutThemeChildProc(HWND hChild, LPARAM lParam);

DECLSPEC_NOINLINE int WINAPI _tWinMainEx(_In_ HINSTANCE          hInstance,
                                         _In_opt_ HINSTANCE      hPrevInstance,
                                         _In_ LPTSTR             lpCmdLine,
                                         _In_ int                nShowCmd,
                                         _In_ const STARTUPINFO* lpStartupInfo)
{
    HACCEL hAccelTable;
    MSG    msg;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);
    UNREFERENCED_PARAMETER(lpStartupInfo);

    /* Opt the process into dark mode and read the live system setting before the class is registered
       (the class background brush is chosen from it). */
    WinXThemeStartup();
    g_fDark = WinXThemeAppsUseDarkMode();

    LoadString(hInstance, IDS_APP_TITLE, g_szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_WINDOWSPROJECT, g_szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance))
    {
        return 0;
    }

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSPROJECT));

    while (0 < GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

/* Registers the window class. The background brush is the theme-chosen one (dark solid / light
   system), so plain client erases already paint the right color. */
static FORCEINLINE ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcx;

    SecureZeroMemory(&wcx, sizeof(wcx));
    wcx.cbSize        = sizeof(wcx);
    wcx.style         = CS_HREDRAW | CS_VREDRAW;
    wcx.lpfnWndProc   = WndProc;
    wcx.hInstance     = hInstance;
    wcx.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT));
    wcx.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcx.hbrBackground = WinXThemeBackgroundBrush(g_fDark);
    wcx.lpszMenuName  = MAKEINTRESOURCE(IDC_WINDOWSPROJECT);
    wcx.lpszClassName = g_szWindowClass;
    wcx.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcx);
}

/* Creates and displays the main window. */
static FORCEINLINE BOOL InitInstance(HINSTANCE hInstance)
{
    HWND hwnd;

    g_hInst = hInstance;

    hwnd = CreateWindowEx(0,
                          g_szWindowClass,
                          g_szTitle,
                          WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          NULL,
                          NULL,
                          hInstance,
                          NULL);
    if (!hwnd)
    {
        return FALSE;
    }

    /* Dark/light the frame title bar and re-theme the (drop-down) menus to match the class brush. */
    WinXThemeApplyWindow(hwnd, g_fDark);

    /* Win32X's DPI-aware first show: size to three-quarters of the launch monitor's work area and
       position per STARTUPINFO -- the successor's upgrade over the template's ShowWindow(nCmdShow). */
    ShowWindowEx(hwnd, SWX_SHOWSTARTUP);
    UpdateWindow(hwnd);
    return TRUE;
}

/* WM_UAHDRAWMENU: paint the whole menu-bar background dark. */
static FORCEINLINE void OnUahDrawMenu(HWND hwnd, const UAHMENU* pUDM)
{
    WINX_MENUBAR_PALETTE pal;

    WinXMenuBarPalette(TRUE, &pal);
    WinXMenuBarOnDrawMenu(hwnd, pUDM, &pal);
}

/* WM_UAHDRAWMENUITEM: paint one bar item dark, by state. */
static FORCEINLINE void OnUahDrawMenuItem(HWND hwnd, const UAHDRAWMENUITEM* pUDMI)
{
    WINX_MENUBAR_PALETTE pal;

    WinXMenuBarPalette(TRUE, &pal);
    WinXMenuBarOnDrawMenuItem(hwnd, pUDMI, &pal);
}

/* WM_COMMAND: the application menu. */
static FORCEINLINE void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    UNREFERENCED_PARAMETER(hwndCtl);
    UNREFERENCED_PARAMETER(codeNotify);

    switch (id)
    {
        case IDM_ABOUT:
            DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, About);
            g_hAboutDlg = NULL; /* DialogBox is modal: cleared once it returns */
            return;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            return;
    }

    FORWARD_WM_COMMAND(hwnd, id, hwndCtl, codeNotify, DefWindowProc);
}

/* WM_PAINT. The class brush already erased the client in the theme color. */
static FORCEINLINE void OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC         hdc;

    hdc = BeginPaint(hwnd, &ps);
    UNREFERENCED_PARAMETER(hdc); /* TODO: drawing code that uses hdc goes here. */
    EndPaint(hwnd, &ps);
}

/* WM_SETTINGCHANGE: the shell broadcasts "ImmersiveColorSet" when the light/dark setting flips. Do
   NOT retheme here: this message arrives as the system's blocking broadcast SendMessage while the
   theme and DWM services are mid-transition, and calling back into them (DwmSetWindowAttribute,
   FlushMenuThemes, SetWindowPos(SWP_FRAMECHANGED)) from inside it deadlocks -> the app hangs. Post a
   private message and do the work on the next message-loop turn, after the broadcast has returned. */
static FORCEINLINE void OnSettingChange(HWND hwnd, UINT flags, LPCTSTR pszSection)
{
    UNREFERENCED_PARAMETER(flags);

    if (pszSection && (0 == lstrcmpi(pszSection, TEXT("ImmersiveColorSet"))))
    {
        (void)PostMessage(hwnd, WMAPP_THEMECHANGED, 0, 0);
    }
}

/* Apply the dark/light dress to a dialog: frame title bar (DWM), the child-control visual style (so
   the OK button -- which paints its own face -- transitions), and a full repaint. Shared by the
   About box's WM_INITDIALOG and the live theme switch, so a dialog already on screen at the moment
   the system theme flips transitions too. */
static FORCEINLINE void ThemeDialog(HWND hDlg, BOOL fDark)
{
    /* Suspend painting while restyling -- the frame attribute and each child's visual-style change
       would otherwise each erase/repaint on their own and flicker the dialog through several frames.
       Hold them off with WM_SETREDRAW, then flush in one synchronous pass (RDW_UPDATENOW) so the
       earlier invalidations coalesce into a single erase+paint. The DWM dark title bar repaints
       itself when the attribute is set; RDW_FRAME covers the rest of the non-client. */
    SetWindowRedraw(hDlg, FALSE);
    WinXThemeApplyDialog(hDlg, fDark);
    EnumChildWindows(hDlg, AboutThemeChildProc, (LPARAM)fDark);
    SetWindowRedraw(hDlg, TRUE);
    RedrawWindow(hDlg, NULL, NULL,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME | RDW_UPDATENOW);
}

/* WMAPP_THEMECHANGED (deferred): re-read the system theme and, on a change, re-dress the window --
   swap the class brush, re-theme the frame and menus, force a full client + non-client repaint --
   and re-dress any About dialog open at the moment of the switch. Runs outside the broadcast
   SendMessage, so the theme/DWM calls below are safe. */
static FORCEINLINE void OnThemeChanged(HWND hwnd)
{
    BOOL fDark;

    fDark = WinXThemeAppsUseDarkMode();
    if (fDark == g_fDark)
    {
        return;
    }
    g_fDark = fDark;
    /* Refresh uxtheme's cached dark state FIRST: the controls' ShouldAppsUseDarkMode reads that cache
       (per disassembly), so the re-theme below paints the new shade in a single pass instead of
       flashing the old one. */
    WinXThemeRefresh();
    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)WinXThemeBackgroundBrush(fDark));
    WinXThemeApplyWindow(hwnd, fDark);
    InvalidateRect(hwnd, NULL, TRUE);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    DrawMenuBar(hwnd);
    if (g_hAboutDlg)
    {
        ThemeDialog(g_hAboutDlg, fDark);
    }
}

/* WM_DESTROY: end whichever pump is running -- WinBaseXRun's on a direct launch, RunComServer's
   under a DelegateExecute embedding. A transient COM launch dies with its window; that is correct. */
static FORCEINLINE void OnDestroy(HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);
    PostQuitMessage(0);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT              lr;
    WINX_MENUBAR_PALETTE pal;

    switch (uMsg)
    {
        /* Owner-draw the menu bar dark -- but only in dark mode; otherwise fall through so
           DefWindowProc paints the stock light bar. */
        case WM_UAHDRAWMENU:
            if (!g_fDark)
            {
                break;
            }
            return HANDLE_WM_UAHDRAWMENU(hwnd, wParam, lParam, OnUahDrawMenu);

        case WM_UAHDRAWMENUITEM:
            if (!g_fDark)
            {
                break;
            }
            return HANDLE_WM_UAHDRAWMENUITEM(hwnd, wParam, lParam, OnUahDrawMenuItem);

        /* DefWindowProc paints the 1px menu-bar/client seam light; repaint it dark afterward. Split
           per message with a literal id (not the switch-narrowed uMsg) so no range-checked value
           feeds the DefWindowProc call (CONV-4.12 C5045-safe). */
        case WM_NCACTIVATE:
            lr = DefWindowProc(hwnd, WM_NCACTIVATE, wParam, lParam);
            if (g_fDark)
            {
                WinXMenuBarPalette(TRUE, &pal);
                WinXMenuBarPaintSeam(hwnd, &pal);
            }
            return lr;

        case WM_NCPAINT:
            lr = DefWindowProc(hwnd, WM_NCPAINT, wParam, lParam);
            if (g_fDark)
            {
                WinXMenuBarPalette(TRUE, &pal);
                WinXMenuBarPaintSeam(hwnd, &pal);
            }
            return lr;

        case WMAPP_THEMECHANGED:
            OnThemeChanged(hwnd);
            return 0;

        HANDLE_MSG(hwnd, WM_SETTINGCHANGE, OnSettingChange);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hwnd, WM_PAINT, OnPaint);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/* EnumChildWindows callback: theme one About-box control to match. The OK button paints its own
   face, so WM_CTLCOLORBTN can't darken it -- the DarkMode_Explorer visual style does. */
static BOOL CALLBACK AboutThemeChildProc(HWND hChild, LPARAM lParam)
{
    WinXThemeApplyControl(hChild, 0 != lParam);
    return TRUE;
}

/* Message handler for the About box. Themed to match the window. */
static INT_PTR CALLBACK About(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
            g_hAboutDlg = hDlg;
            /* Double-buffer the dialog (children paint to an off-screen surface, blit once) so a live
               theme switch re-dresses every control as a single flicker-free composite rather than a
               cascade of individual control repaints. */
            SetWindowLongPtr(hDlg, GWL_EXSTYLE, GetWindowLongPtr(hDlg, GWL_EXSTYLE) | WS_EX_COMPOSITED);
            ThemeDialog(hDlg, g_fDark);
            return (INT_PTR)TRUE;

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            if (g_fDark)
            {
                return (INT_PTR)WinXThemeCtlColorBrush((HDC)wParam, TRUE);
            }
            break;

        case WM_COMMAND:
            if ((IDOK == LOWORD(wParam)) || (IDCANCEL == LOWORD(wParam)))
            {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;

        default:
            break;
    }
    return (INT_PTR)FALSE;
}
