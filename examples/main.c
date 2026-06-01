/*
 * main.c -- WindowsProject: the classic Win32 desktop application (menu, accelerators, About
 * dialog, icons, string table) reborn as a Win32X client. It is the spiritual successor to the
 * Visual Studio "Windows Desktop Application" template -- same shape, but CRT-free, started through
 * Win32X's _tWinMainEx entry, and brought up with the DPI-aware ShowWindowEx(SWX_SHOWSTARTUP) first
 * show in place of the template's plain ShowWindow(nCmdShow).
 */

#include "framework.h"
#include "WindowsProject.h"

/* CRT-free codegen: this TU links /NODEFAULTLIB, so the runtime checks that would emit calls into an
   absent C runtime are turned off. */
#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#define MAX_LOADSTRING 100

/* Globals (file-scope; zero-initialized by the loader -- no CRT startup needed). */
static HINSTANCE g_hInst;                       /* current instance                 */
static WCHAR     g_szTitle[MAX_LOADSTRING];     /* title bar text                   */
static WCHAR     g_szWindowClass[MAX_LOADSTRING]; /* main window class name         */

/* Forward declarations. */
static ATOM             MyRegisterClass(HINSTANCE hInstance);
static BOOL             InitInstance(HINSTANCE hInstance);
static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK About(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

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

    /* Initialize global strings from the string table. */
    LoadString(hInstance, IDS_APP_TITLE, g_szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_WINDOWSPROJECT, g_szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    /* RegisterClass returns 0 on a repeated activation (ERROR_CLASS_ALREADY_EXISTS, harmless); the
       launch broker never calls in here -- a DelegateExecute activation spawns a fresh process that
       re-enters on the direct path -- so there is no launch mode to branch on. */
    if (!InitInstance(hInstance))
    {
        return 0;
    }

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSPROJECT));

    /* Main message loop. */
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

/* Registers the window class. */
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
    wcx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
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

    /* Win32X's DPI-aware first show: size to three-quarters of the launch monitor's work area and
       position per STARTUPINFO -- the successor's upgrade over the template's ShowWindow(nCmdShow). */
    ShowWindowEx(hwnd, SWX_SHOWSTARTUP);
    UpdateWindow(hwnd);
    return TRUE;
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

/* WM_PAINT. */
static FORCEINLINE void OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC         hdc;

    hdc = BeginPaint(hwnd, &ps);
    UNREFERENCED_PARAMETER(hdc); /* TODO: drawing code that uses hdc goes here. */
    EndPaint(hwnd, &ps);
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
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
        HANDLE_MSG(hwnd, WM_PAINT,   OnPaint);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/* Message handler for the About box. */
static INT_PTR CALLBACK About(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (uMsg)
    {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;

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
