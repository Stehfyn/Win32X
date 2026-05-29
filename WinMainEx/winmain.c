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
typedef unsigned short WORD;
typedef wchar_t*       LPWSTR;
typedef unsigned char* LPBYTE;
typedef long           LONG;
typedef HANDLE         HMONITOR;

#define WM_DESTROY          0x0002
#define IDC_ARROW           0x7F00
#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define WIN_W               960
#define WIN_H               600
#define SW_SHOWNORMAL       1
#define STARTF_USESHOWWINDOW 0x00000001
#define STARTF_USEPOSITION   0x00000004
#define STARTF_HASSHELLDATA  0x00000400
#define MONITOR_DEFAULTTONEAREST 0x00000002
#define SWP_NOSIZE          0x0001
#define SWP_NOZORDER        0x0004
#define SWP_NOACTIVATE      0x0010
#define SPI_GETFOREGROUNDLOCKTIMEOUT 0x2000
#define SPI_SETFOREGROUNDLOCKTIMEOUT 0x2001
#define LSFW_LOCK           1
#define LSFW_UNLOCK         2
#define TRUE                1
#define FALSE               0
#define WHERE_NOONE_CAN_SEE_ME ((int)-32000)

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
typedef struct tagRECT { LONG left, top, right, bottom; } RECT;            /* 4x LONG: no padding */
typedef struct tagMONITORINFO { DWORD cbSize; RECT rcMonitor; RECT rcWork; DWORD dwFlags; } MONITORINFO;

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

typedef struct _STARTUPINFOW {
    DWORD  cb;
#ifdef _WIN64
    int    reserved0;      /* explicit ABI padding (x64): DWORD before 8-byte ptr */
#endif
    LPWSTR lpReserved;
    LPWSTR lpDesktop;
    LPWSTR lpTitle;
    DWORD  dwX;
    DWORD  dwY;
    DWORD  dwXSize;
    DWORD  dwYSize;
    DWORD  dwXCountChars;
    DWORD  dwYCountChars;
    DWORD  dwFillAttribute;
    DWORD  dwFlags;
    WORD   wShowWindow;
    WORD   cbReserved2;
#ifdef _WIN64
    int    reserved1;      /* explicit ABI padding (x64): WORDs before 8-byte ptr */
#endif
    LPBYTE lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFOW;

/* Hand-declared imports using correct 64-bit handle types. */
__declspec(dllimport) void    WINAPI ExitProcess(UINT uExitCode);
__declspec(dllimport) void    WINAPI GetStartupInfoW(STARTUPINFOW* lpStartupInfo);
__declspec(dllimport) HWND    WINAPI GetConsoleWindow(void);
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
__declspec(dllimport) HMONITOR WINAPI MonitorFromPoint(POINT pt, DWORD dwFlags);
__declspec(dllimport) BOOL    WINAPI GetCursorPos(POINT* lpPoint);
__declspec(dllimport) BOOL    WINAPI GetMonitorInfoW(HMONITOR hMonitor, MONITORINFO* lpmi);
__declspec(dllimport) BOOL    WINAPI SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);

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

/* Compute the top-left that centers a WIN_W x WIN_H window in mon's work area. */
static CFORCEINLINE void CenteredPos(HMONITOR mon, int* px, int* py)
{
    MONITORINFO mi;
    mi.cbSize = (DWORD)sizeof(mi);
    GetMonitorInfoW(mon, &mi);
    *px = (int)(mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - WIN_W) / 2);
    *py = (int)(mi.rcWork.top  + (mi.rcWork.bottom - mi.rcWork.top - WIN_H) / 2);
}

/* Documented path only: we were started by the foreground process (Explorer),
   which per the SetForegroundWindow rules allows us to set the foreground --
   no AttachThreadInput / lock-timeout workarounds. */
static CFORCEINLINE __declspec(safebuffers) void ForceForeground(HWND hwnd, int nCmdShow)
{
    ShowWindow(hwnd, nCmdShow);
    SetForegroundWindow(hwnd);
}

__declspec(safebuffers) void mainCRTStartup(void)
{
    STARTUPINFOW si;
    int nCmdShow, x, y;
    POINT pt;

    /* First thing: shove the console offscreen. SWP_NOACTIVATE keeps it the
       foreground/active window (so foreground does NOT hand off to Explorer) --
       it's just no longer visible. SW_HIDE would deactivate it and cause the
       handoff; moving offscreen does not. Only the pre-main conhost frame remains. */
    SetWindowPos(GetConsoleWindow(), 0, WHERE_NOONE_CAN_SEE_ME, WHERE_NOONE_CAN_SEE_ME,
        0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    GetStartupInfoW(&si);
    nCmdShow = (si.dwFlags & STARTF_USESHOWWINDOW) ? (int)si.wShowWindow : SW_SHOWNORMAL;

    /* Resolve the final top-left up front -- no CW_USEDEFAULT, no create-then-move:
       1. STARTF_USEPOSITION  -> launcher's dwX/dwY.
       2. STARTF_HASSHELLDATA -> center on the shell's HMONITOR (hStdOutput).
       3. otherwise           -> center on the monitor under the cursor. */
    if (si.dwFlags & STARTF_USEPOSITION)
    {
        x = (int)si.dwX;
        y = (int)si.dwY;
    }
    else if (si.dwFlags & STARTF_HASSHELLDATA)
    {
        CenteredPos(si.hStdOutput, &x, &y);
    }
    else
    {
        GetCursorPos(&pt);
        CenteredPos(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &x, &y);
    }

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
        WS_OVERLAPPEDWINDOW,
        x, y, WIN_W, WIN_H,
        0, 0, wc.hInstance, 0);

    UpdateWindow(hwnd);
    ForceForeground(hwnd, nCmdShow);       /* show (per STARTUPINFO) + take foreground */
    FreeConsole();                         /* free only after our window owns foreground:
                                              the (now background) console can't hand off to Explorer */

    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ExitProcess(0);
}
