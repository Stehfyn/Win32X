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

/* Forward declarations. */
static ATOM             MyRegisterClass(HINSTANCE hInstance);
static BOOL             InitInstance(HINSTANCE hInstance);
static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK About(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

static BOOL (WINAPI* volatile pfnAppThemeIsDarkMode)(void) = ThemeIsDarkMode;
static void (WINAPI* volatile pfnAppThemeApplyDialogTree)(HWND hwnd, BOOL fDark) = ThemeApplyDialogTree;
static void (WINAPI* volatile pfnAppThemeUnregisterWindow)(HWND hwnd) = ThemeUnregisterWindow;
static void (WINAPI* volatile pfnAppThemeUnregisterDialog)(HWND hwnd) = ThemeUnregisterDialog;
static BOOL (WINAPI* volatile pfnAppThemeHandleWindowMessage)(
    HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT uDeferredMsg, LRESULT* plr) = ThemeHandleWindowMessage;
static void (WINAPI* volatile pfnAppThemeEnableCustomFrame)(HWND hwnd, BOOL fEnable) = ThemeEnableCustomFrame;
static BOOL (WINAPI* volatile pfnAppThemeCustomFrameHandleMessage)(
    HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plr) = ThemeCustomFrameHandleMessage;
static BOOL (WINAPI* volatile pfnAppDwmFrameInit)(HWND hwnd) = DwmFrameInit;
static void (WINAPI* volatile pfnAppDwmFrameRender)(HWND hwnd, BOOL fDark) = DwmFrameRender;
static void (WINAPI* volatile pfnAppDwmFrameResize)(HWND hwnd) = DwmFrameResize;
static BOOL (WINAPI* volatile pfnAppDwmFrameHandleMessage)(
    HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, void (WINAPI* pfnToggle)(HWND), LRESULT* plr) = DwmFrameHandleMessage;
static void (WINAPI* volatile pfnAppMenuBarPalette)(BOOL fDark, MENUBAR_PALETTE* pPalette) = MenuBarPalette;
static void (WINAPI* volatile pfnAppMenuBarOnDrawMenu)(
    HWND hwnd, const UAHMENU* pUDM, const MENUBAR_PALETTE* pPalette) = MenuBarOnDrawMenu;
static void (WINAPI* volatile pfnAppMenuBarOnDrawMenuItem)(
    HWND hwnd, const UAHDRAWMENUITEM* pUDMI, const MENUBAR_PALETTE* pPalette) = MenuBarOnDrawMenuItem;
static void (WINAPI* volatile pfnAppMenuBarPaintSeam)(
    HWND hwnd, const MENUBAR_PALETTE* pPalette) = MenuBarPaintSeam;

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
    ThemeStartup();
    g_fDark = ThemeEffectiveDarkMode();

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
    wcx.hbrBackground = ThemeBackgroundBrush(g_fDark);
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

    hwnd = CreateWindowEx(WS_EX_NOREDIRECTIONBITMAP,   /* DComp paints the whole window; no GDI redirection surface to occlude it */
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

    /* Register and theme the top-level window before the first show, so first paint is coherent. */
    ThemeApplyTopLevel(hwnd, g_fDark);

    /* Stand up the in-process DirectComposition caption compositor -- the same D3D11/D2D/DComp/DWrite
       stack uDWM uses, on a DCompositionTarget we own for this hwnd. Then force the WM_NCCALCSIZE that
       removes the standard non-client area (handled in DwmFrameHandleMessage) so our caption owns it,
       and render. */
    if (pfnAppDwmFrameInit(hwnd))
    {
        /* Publish the frame change ONCE at creation: SWP_FRAMECHANGED forces the WM_NCCALCSIZE that removes
           the standard NC (so our DComp caption owns the whole window) and settles the DWM extended frame
           set up in DwmFrameInit. Seed the caption shade first, then invalidate the whole window a single
           time -- the WM_PAINT it raises (after ShowWindow) renders the caption at the right moment, in the
           right theme. Rendering here, before the frame change is published and before the window is shown,
           is the mistimed paint that left the caption stale until a resize. */
        DwmFrameSetDark(hwnd, g_fDark);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, NULL, TRUE);
    }

    /* Win32X's DPI-aware first show: size to three-quarters of the launch monitor's work area and
       position per STARTUPINFO -- the successor's upgrade over the template's ShowWindow(nCmdShow). */
    ShowWindowEx(hwnd, SWX_SHOWSTARTUP);
    UpdateWindow(hwnd);
    return TRUE;
}

/* WM_UAHDRAWMENU: paint the whole menu-bar background dark. */
static FORCEINLINE void OnUahDrawMenu(HWND hwnd, const UAHMENU* pUDM)
{
    MENUBAR_PALETTE pal;

    pfnAppMenuBarPalette(pfnAppThemeIsDarkMode(), &pal);
    pfnAppMenuBarOnDrawMenu(hwnd, pUDM, &pal);
}

/* WM_UAHDRAWMENUITEM: paint one bar item dark, by state. */
static FORCEINLINE void OnUahDrawMenuItem(HWND hwnd, const UAHDRAWMENUITEM* pUDMI)
{
    MENUBAR_PALETTE pal;

    pfnAppMenuBarPalette(pfnAppThemeIsDarkMode(), &pal);
    pfnAppMenuBarOnDrawMenuItem(hwnd, pUDMI, &pal);
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
            return;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            return;
    }

    FORWARD_WM_COMMAND(hwnd, id, hwndCtl, codeNotify, DefWindowProc);
}

/* Apply the dark/light dress to a dialog: frame title bar (DWM), the child-control visual style (so
   the OK button -- which paints its own face -- transitions), and a full repaint. Shared by the
   About box's WM_INITDIALOG and the live theme switch, so a dialog already on screen at the moment
   the system theme flips transitions too. */
static FORCEINLINE void ThemeDialog(HWND hDlg, BOOL fDark)
{
    pfnAppThemeApplyDialogTree(hDlg, fDark);
}

/* WM_DESTROY: end whichever pump is running -- WinBaseXRun's on a direct launch, RunComServer's
   under a DelegateExecute embedding. A transient COM launch dies with its window; that is correct. */
static FORCEINLINE void OnDestroy(HWND hwnd)
{
    pfnAppThemeUnregisterWindow(hwnd);
    PostQuitMessage(0);
}

/* Light/dark caption-button click handler. ThemeToggleDarkMode flips the app theme state and invalidates
   the registered windows, but it does NOT touch g_fDark or re-render the DComp caption -- so without this
   the caption keeps the old shade until the next event that calls DwmFrameRender (a resize). Refresh the
   truth (g_fDark) from the now-effective theme and recolor the DComp caption immediately. */
static void WINAPI AppToggleDark(HWND hwnd)
{
    ThemeToggleDarkMode(hwnd);
    g_fDark = pfnAppThemeIsDarkMode();
    DwmFrameAnimateTheme(hwnd, g_fDark);   /* 160ms crossfade to the new shade (vs. an instant recolor) */
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT              lr;

    /* DComp caption frame first: WM_NCCALCSIZE removes the standard NC so our DirectComposition caption
       (uDWM-stack reproduction) owns it, WM_NCHITTEST follows FindNCHit's order, and the caption buttons
       (incl. the light/dark toggle, which calls ThemeToggleDarkMode) take their input here. Returns TRUE
       only for the frame messages it owns; everything else falls through to DefWindowProc below. */
    if (pfnAppDwmFrameHandleMessage(hwnd, uMsg, wParam, lParam, AppToggleDark, &lr))
    {
        return lr;
    }

    switch (uMsg)
    {
        /* Keep the DComp caption surface sized to the window and re-render it. */
        case WM_SIZE:
            pfnAppDwmFrameResize(hwnd);
            pfnAppDwmFrameRender(hwnd, g_fDark);
            break;

        /* Route the non-client frame messages (WM_NCACTIVATE/WM_NCPAINT) through the theme handler too:
           it lets DefWindowProc render the frame, then repaints the owner-drawn menu band -- the FULL
           bar cross-faded off the shared snapshot clock while a transition is live, the 1px seam
           otherwise. Handling them here (instead of the app painting only the seam) is what keeps the
           menu bar tracking the caption's progress band during the fade instead of snapping to the
           target shade on the first frame. The animation timer's per-tick WM_NCPAINT drives it. */
        case WM_SETTINGCHANGE:
        case WM_ERASEBKGND:
        case WM_PAINT:
        case WM_NCACTIVATE:
        case WM_NCPAINT:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WMAPP_THEMECHANGED:
            if (pfnAppThemeHandleWindowMessage(hwnd, uMsg, wParam, lParam, WMAPP_THEMECHANGED, &lr))
            {
                g_fDark = pfnAppThemeIsDarkMode();
                DwmFrameAnimateTheme(hwnd, g_fDark);   /* crossfade the DComp caption to the new theme */
                return lr;
            }
            break;

        /* Owner-draw the menu bar dark -- but only in dark mode; otherwise fall through so
           DefWindowProc paints the stock light bar. */
        case WM_UAHDRAWMENU:
            return HANDLE_WM_UAHDRAWMENU(hwnd, wParam, lParam, OnUahDrawMenu);

        case WM_UAHDRAWMENUITEM:
            return HANDLE_WM_UAHDRAWMENUITEM(hwnd, wParam, lParam, OnUahDrawMenuItem);

        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/* Message handler for the About box. Themed to match the window. */
static INT_PTR CALLBACK About(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    HWND hwndOwner;
    LRESULT lr;

    switch (uMsg)
    {
        case WM_INITDIALOG:
            ThemeDialog(hDlg, pfnAppThemeIsDarkMode());
            return (INT_PTR)TRUE;

        /* The owner coalesces the broadcast and rethemes registered windows on the next message-loop
           turn, after the shell broadcast has returned. */
        case WM_SETTINGCHANGE:
            hwndOwner = GetWindow(hDlg, GW_OWNER);
            if (!hwndOwner)
            {
                hwndOwner = hDlg;
            }
            if (pfnAppThemeHandleWindowMessage(hwndOwner, WM_SETTINGCHANGE, wParam, lParam, WMAPP_THEMECHANGED, &lr))
            {
                return (INT_PTR)TRUE;
            }
            return (INT_PTR)FALSE;

        case WM_ERASEBKGND:
            if (pfnAppThemeHandleWindowMessage(hDlg, WM_ERASEBKGND, wParam, lParam, WMAPP_THEMECHANGED, &lr))
            {
                return (INT_PTR)lr;
            }
            return (INT_PTR)FALSE;

        case WM_PAINT:
            if (pfnAppThemeHandleWindowMessage(hDlg, WM_PAINT, wParam, lParam, WMAPP_THEMECHANGED, &lr))
            {
                return (INT_PTR)lr;
            }
            return (INT_PTR)FALSE;

        case WM_CTLCOLORDLG:
            if (pfnAppThemeHandleWindowMessage(hDlg, WM_CTLCOLORDLG, wParam, lParam, WMAPP_THEMECHANGED, &lr))
            {
                return (INT_PTR)lr;
            }
            break;

        case WM_CTLCOLORSTATIC:
            if (pfnAppThemeHandleWindowMessage(hDlg, WM_CTLCOLORSTATIC, wParam, lParam, WMAPP_THEMECHANGED, &lr))
            {
                return (INT_PTR)lr;
            }
            break;

        case WM_COMMAND:
            if ((IDOK == LOWORD(wParam)) || (IDCANCEL == LOWORD(wParam)))
            {
                pfnAppThemeUnregisterDialog(hDlg);
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;

        case WM_NCDESTROY:
            pfnAppThemeUnregisterDialog(hDlg);
            break;

        default:
            break;
    }
    return (INT_PTR)FALSE;
}
