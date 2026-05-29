/*
 * WinMainEx / ConsoleApplication1-mt-win32.exe  (x64 Release /MT)
 *
 * Win32 APIs are hand-declared (no <windows.h>) using the correct 64-bit handle
 * types (HWND/HMENU/HCURSOR/...). An earlier revision declared handle returns and
 * the CreateWindowExW hWndParent/hMenu/lpParam args as 32-bit `int`; passing 0 then
 * emitted a 32-bit store into a 64-bit stack slot, leaving the high 32 bits as
 * uninitialized stack garbage -> CreateWindowExW intermittently failed with
 * ERROR_INVALID_MENU_HANDLE/ERROR_INVALID_WINDOW_HANDLE (Debug always, Release by
 * luck). Correct handle types make those args full 64-bit NULLs.
 */

#pragma check_stack(off)
#pragma strict_gs_check(off)
#pragma runtime_checks("", off)

#include <stddef.h>   /* wchar_t (no <windows.h>); size_t for memset */

#define WINAPI  __stdcall
#define CALLBACK __stdcall
#define CFORCEINLINE __forceinline

typedef unsigned int   UINT;
typedef unsigned long  DWORD;          /* 32-bit on Windows */
typedef int            BOOL;
#ifdef _WIN64
typedef __int64          LONG_PTR;
typedef unsigned __int64 UINT_PTR;
#else
typedef long             LONG_PTR;
typedef unsigned int     UINT_PTR;
#endif
typedef LONG_PTR         LRESULT;
typedef LONG_PTR         LPARAM;
typedef UINT_PTR         WPARAM;
typedef void*          HANDLE;
typedef HANDLE         HWND;
typedef HANDLE         HINSTANCE;
typedef HANDLE         HICON;
typedef HANDLE         HCURSOR;
typedef HANDLE         HBRUSH;
typedef HANDLE         HMENU;
typedef void*          LPVOID;
typedef const wchar_t* LPCWSTR;

#define WM_DESTROY          0x0002
#define IDC_ARROW           0x7F00
#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define WS_DISABLED         0x08000000
#define CW_USEDEFAULT       ((int)0x80000000)
#define SW_SHOW             5
#define SPI_GETFOREGROUNDLOCKTIMEOUT 0x2000
#define SPI_SETFOREGROUNDLOCKTIMEOUT 0x2001
#define SPIF_SENDCHANGE             0x0002
#define LSFW_LOCK           1
#define LSFW_UNLOCK         2
#define TRUE                1
#define FALSE               0

typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagWNDCLASSW {
    UINT      style;
#ifdef _WIN64
    int       reserved0;   /* explicit ABI padding (x64): UINT before 8-byte ptr */
#endif
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
#ifdef _WIN64
    int    reserved0;      /* explicit ABI padding (x64): UINT before 8-byte WPARAM */
#endif
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
    DWORD  lPrivate;
} MSG;

/* Hand-declared imports using correct 64-bit handle types. */
__declspec(dllimport) void    WINAPI ExitProcess(UINT uExitCode);
__declspec(dllimport) BOOL    WINAPI FreeConsole(void);
__declspec(dllimport) BOOL    WINAPI LockSetForegroundWindow(UINT uLockCode);
__declspec(dllimport) void    WINAPI Sleep(DWORD dwMilliseconds);
__declspec(dllimport) HCURSOR WINAPI LoadCursorW(HINSTANCE hInstance, int lpCursorName);
__declspec(dllimport) int     WINAPI RegisterClassW(const WNDCLASSW* lpWndClass);
__declspec(dllimport) HWND    WINAPI CreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName,
    LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
__declspec(dllimport) BOOL    WINAPI ShowWindow(HWND hWnd, int nCmdShow);
__declspec(dllimport) BOOL    WINAPI UpdateWindow(HWND hWnd);
__declspec(dllimport) BOOL    WINAPI SetForegroundWindow(HWND hWnd);
__declspec(dllimport) HWND    WINAPI GetForegroundWindow(void);
__declspec(dllimport) BOOL    WINAPI EnableWindow(HWND hWnd, BOOL bEnable);
__declspec(dllimport) int     WINAPI GetMessageW(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
__declspec(dllimport) int     WINAPI TranslateMessage(const MSG* lpMsg);
__declspec(dllimport) int     WINAPI DispatchMessageW(const MSG* lpMsg);
__declspec(dllimport) int     WINAPI DefWindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
__declspec(dllimport) void    WINAPI PostQuitMessage(int nExitCode);
__declspec(dllimport) DWORD   WINAPI GetWindowThreadProcessId(HWND hWnd, DWORD* lpdwProcessId);
__declspec(dllimport) DWORD   WINAPI GetCurrentThreadId(void);
__declspec(dllimport) BOOL    WINAPI AttachThreadInput(DWORD idAttach, DWORD idAttachTo, BOOL fAttach);
__declspec(dllimport) BOOL    WINAPI BringWindowToTop(HWND hWnd);
__declspec(dllimport) HWND    WINAPI SetActiveWindow(HWND hWnd);
__declspec(dllimport) HWND    WINAPI SetFocus(HWND hWnd);
__declspec(dllimport) BOOL    WINAPI SystemParametersInfoW(UINT uiAction, UINT uiParam, LPVOID pvParam, UINT fWinIni);

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(linker, "/NODEFAULTLIB")
#pragma comment(linker, "/ENTRY:mainCRTStartup")
#pragma comment(linker, "/MERGE:.pdata=.rdata")


__declspec(safebuffers) LRESULT CALLBACK WndProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}

/* Deterministically take the foreground. SetForegroundWindow alone is subject to
   the anti-focus-steal policy; sharing input state with the current foreground
   thread (AttachThreadInput) plus clearing the foreground-lock timeout satisfies
   the documented conditions so the activation actually lands. */
static CFORCEINLINE __declspec(safebuffers) void ForceForeground(HWND hwnd)
{
    DWORD fgThread  = GetWindowThreadProcessId(GetForegroundWindow(), 0);
    DWORD myThread  = GetCurrentThreadId();
    DWORD oldTimeout = 0;

    SystemParametersInfoW(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &oldTimeout, 0);
    SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (LPVOID)0, SPIF_SENDCHANGE);

    AttachThreadInput(myThread, fgThread, TRUE);
    BringWindowToTop(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
    AttachThreadInput(myThread, fgThread, FALSE);

    SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (LPVOID)(UINT_PTR)oldTimeout, SPIF_SENDCHANGE);
}

__declspec(safebuffers) void mainCRTStartup(void)
{
    FreeConsole();
    LockSetForegroundWindow(LSFW_LOCK);

    WNDCLASSW wc;                       /* every field set explicitly: no memset emitted */
    wc.style = 0;
    wc.lpfnWndProc = WndProcW;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = 0;
    wc.hIcon = 0;
    wc.hCursor = LoadCursorW(0, IDC_ARROW);
    wc.hbrBackground = 0;
    wc.lpszMenuName = 0;
    wc.lpszClassName = L"DummyWindowClass";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"DummyWindow",
        WS_OVERLAPPEDWINDOW | WS_DISABLED,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        0, 0, wc.hInstance, 0);

    EnableWindow(hwnd, TRUE);              /* created WS_DISABLED; enable before activating */
    LockSetForegroundWindow(LSFW_UNLOCK);  /* release the startup lock so we can foreground */
    ForceForeground(hwnd);                 /* show + deterministically take the foreground */
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ExitProcess(0);
}
