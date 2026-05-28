/*
 * WinMainEx / ConsoleApplication1-mt-win32.exe  (x64 Release /MT)
 *
 * Reconstructed from the disassembly of the original binary. The Win32 APIs are
 * hand-declared (no <windows.h>) with 32-bit `int` handle return types, exactly
 * as the original did -- this reproduces the binary's `movsxd` sign-extension of
 * handle returns and the 32-bit (`dword`) stores of int-typed window args, which
 * <windows.h>'s 64-bit pointer prototypes do not generate.
 */

#include <stddef.h>   /* wchar_t (no <windows.h>) */

#define WINAPI  __stdcall
#define CALLBACK __stdcall

typedef unsigned int   UINT;
typedef unsigned long  DWORD;          /* 32-bit on Windows */
typedef int            BOOL;
typedef __int64        LONG_PTR;
typedef __int64        LRESULT;
typedef __int64        LPARAM;
typedef unsigned __int64 WPARAM;
typedef void*          HANDLE;
typedef HANDLE         HWND;
typedef HANDLE         HINSTANCE;
typedef HANDLE         HICON;
typedef HANDLE         HCURSOR;
typedef HANDLE         HBRUSH;
typedef const wchar_t* LPCWSTR;

#define WM_DESTROY          0x0002
#define IDC_ARROW           0x7F00
#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define WS_DISABLED         0x08000000
#define CW_USEDEFAULT       ((int)0x80000000)
#define SW_SHOW             5
#define LSFW_LOCK           1
#define LSFW_UNLOCK         2
#define TRUE                1
#define FALSE               0

typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagWNDCLASSW {
    UINT      style;
    WNDPROC   lpfnWndProc;
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCWSTR   lpszMenuName;
    LPCWSTR   lpszClassName;
} WNDCLASSW;

typedef struct tagPOINT { long x, y; } POINT;

typedef struct tagMSG {
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
    DWORD  lPrivate;
} MSG;

/* Hand-declared imports. Handle returns are `int` (32-bit) on purpose. */
__declspec(dllimport) int     WINAPI FreeConsole(void);
__declspec(dllimport) BOOL    WINAPI LockSetForegroundWindow(UINT uLockCode);
__declspec(dllimport) void    WINAPI Sleep(DWORD dwMilliseconds);
__declspec(dllimport) int     WINAPI LoadCursorW(HINSTANCE hInstance, int lpCursorName);
__declspec(dllimport) int     WINAPI RegisterClassW(const WNDCLASSW* lpWndClass);
__declspec(dllimport) int     WINAPI CreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName,
    LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
    int hWndParent, int hMenu, HINSTANCE hInstance, int lpParam);
__declspec(dllimport) int     WINAPI ShowWindow(HWND hWnd, int nCmdShow);
__declspec(dllimport) int     WINAPI UpdateWindow(HWND hWnd);
__declspec(dllimport) int     WINAPI SetForegroundWindow(HWND hWnd);
__declspec(dllimport) int     WINAPI GetForegroundWindow(void);
__declspec(dllimport) BOOL    WINAPI EnableWindow(HWND hWnd, BOOL bEnable);
__declspec(dllimport) int     WINAPI GetMessageW(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
__declspec(dllimport) int     WINAPI TranslateMessage(const MSG* lpMsg);
__declspec(dllimport) int     WINAPI DispatchMessageW(const MSG* lpMsg);
__declspec(dllimport) int     WINAPI DefWindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
__declspec(dllimport) void    WINAPI PostQuitMessage(int nExitCode);

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(linker, "/MERGE:.pdata=.rdata")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

/* Forced imports: present in the IAT but never called (matches the original's thunk table). */
#pragma comment(linker, "/INCLUDE:CreateProcessW")
#pragma comment(linker, "/INCLUDE:ExitProcess")
#pragma comment(linker, "/INCLUDE:FreeConsole")
#pragma comment(linker, "/INCLUDE:GetModuleHandleW")
#pragma comment(linker, "/INCLUDE:Sleep")
#pragma comment(linker, "/INCLUDE:MessageBoxW")
#pragma comment(linker, "/INCLUDE:PostQuitMessage")
#pragma comment(linker, "/INCLUDE:DefWindowProcW")
#pragma comment(linker, "/INCLUDE:RegisterClassW")
#pragma comment(linker, "/INCLUDE:CreateWindowExW")
#pragma comment(linker, "/INCLUDE:LoadCursorW")
#pragma comment(linker, "/INCLUDE:GetMessageW")
#pragma comment(linker, "/INCLUDE:TranslateMessage")
#pragma comment(linker, "/INCLUDE:DispatchMessageW")
#pragma comment(linker, "/INCLUDE:ShowWindow")
#pragma comment(linker, "/INCLUDE:UpdateWindow")
#pragma comment(linker, "/INCLUDE:SetActiveWindow")
#pragma comment(linker, "/INCLUDE:LockSetForegroundWindow")
#pragma comment(linker, "/INCLUDE:SetForegroundWindow")
#pragma comment(linker, "/INCLUDE:GetForegroundWindow")
#pragma comment(linker, "/INCLUDE:FlashWindow")
#pragma comment(linker, "/INCLUDE:EnableWindow")
#pragma comment(linker, "/INCLUDE:DwmSetWindowAttribute")

LRESULT CALLBACK WndProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}

int main(void)
{
    BOOL first = TRUE;

    FreeConsole();
    LockSetForegroundWindow(LSFW_LOCK);

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProcW;
    wc.lpszClassName = L"DummyWindowClass";
    wc.hCursor = (HCURSOR)(LONG_PTR)LoadCursorW(0, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = (HWND)(LONG_PTR)CreateWindowExW(0, wc.lpszClassName, L"DummyWindow",
        WS_OVERLAPPEDWINDOW | WS_DISABLED,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        0, 0, wc.hInstance, 0);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    EnableWindow(hwnd, TRUE);
    LockSetForegroundWindow(LSFW_UNLOCK);
    Sleep(2);

    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0) > 0)
    {
        if (hwnd != (HWND)(LONG_PTR)GetForegroundWindow() && first)
        {
            SetForegroundWindow(hwnd);
            if (hwnd == (HWND)(LONG_PTR)GetForegroundWindow())
            {
                first = FALSE;
                EnableWindow(hwnd, TRUE);
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
