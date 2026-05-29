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
#define NTAPI    __stdcall
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
#define WM_QUIT             0x0012
#define WM_TIMER            0x0113
#define PM_REMOVE           0x0001
#define IDC_ARROW           0x7F00
#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define WIN_W               960
#define WIN_H               600
#define SW_HIDE             0
#define SW_SHOWNORMAL       1
#define SW_SHOWNOACTIVATE   4
#define WS_EX_APPWINDOW     0x00040000
#define WS_EX_TOOLWINDOW    0x00000080
#define GWL_EXSTYLE         (-20)
#define SWP_FRAMECHANGED    0x0020
#define STARTF_USESHOWWINDOW 0x00000001
#define STARTF_USEPOSITION   0x00000004
#define STARTF_HASSHELLDATA  0x00000400
#define MONITOR_DEFAULTTONEAREST 0x00000002
#define SWP_NOSIZE          0x0001
#define SWP_NOZORDER        0x0004
#define SWP_NOACTIVATE      0x0010
#define DWMWA_TRANSITIONS_FORCEDISABLED 3
#define DWMWA_CLOAK         13
#define SPI_GETFOREGROUNDLOCKTIMEOUT 0x2000
#define SPI_SETFOREGROUNDLOCKTIMEOUT 0x2001
#define LSFW_LOCK           1
#define LSFW_UNLOCK         2
#define VK_SHIFT            0x10
#define KEYEVENTF_KEYUP     0x0002
#define MOUSEEVENTF_MOVE    0x0001
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
__declspec(dllimport) void    WINAPI keybd_event(unsigned char bVk, unsigned char bScan, DWORD dwFlags, UINT_PTR dwExtraInfo);
__declspec(dllimport) void    WINAPI mouse_event(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData, UINT_PTR dwExtraInfo);
__declspec(dllimport) HWND    WINAPI GetForegroundWindow(void);
__declspec(dllimport) int     WINAPI TranslateMessage(const MSG* lpMsg);
__declspec(dllimport) int     WINAPI DispatchMessageW(const MSG* lpMsg);
__declspec(dllimport) int     WINAPI DefWindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
__declspec(dllimport) void    WINAPI PostQuitMessage(int nExitCode);
__declspec(dllimport) UINT    WINAPI RegisterWindowMessageW(LPCWSTR lpString);
__declspec(dllimport) UINT_PTR WINAPI SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void* lpTimerFunc);
__declspec(dllimport) BOOL    WINAPI SetWindowTextW(HWND hWnd, LPCWSTR lpString);
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
__declspec(dllimport) LONG    WINAPI SetWindowLongW(HWND hWnd, int nIndex, LONG dwNewLong);
__declspec(dllimport) long    WINAPI DwmSetWindowAttribute(HWND hwnd, DWORD dwAttribute, const void* pvAttribute, DWORD cbAttribute);
__declspec(dllimport) long    NTAPI  NtQueryTimerResolution(DWORD* Min, DWORD* Max, DWORD* Cur);
__declspec(dllimport) long    NTAPI  NtSetTimerResolution(DWORD Desired, int Set, DWORD* Cur);
__declspec(dllimport) int     WINAPI PeekMessageW(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
__declspec(dllimport) BOOL    WINAPI WaitMessage(void);

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(linker, "/NODEFAULTLIB")
#pragma comment(linker, "/ENTRY:mainCRTStartup")
#pragma comment(linker, "/MERGE:.pdata=.rdata")

/* ITaskbarList (COM, hand-declared -- no <shobjidl.h>): DeleteTab removes a window's
   taskbar button cleanly (no SW_HIDE bounce, no FreeConsole churn); ActivateTab marks a
   button active. Co* live in ole32.lib (already on the link line). */
typedef struct _GUID { unsigned long Data1; unsigned short Data2; unsigned short Data3; unsigned char Data4[8]; } GUID;
typedef struct ITaskbarList ITaskbarList;
typedef struct ITaskbarListVtbl {
    long          (WINAPI* QueryInterface)(ITaskbarList*, const GUID*, void**);
    unsigned long (WINAPI* AddRef)(ITaskbarList*);
    unsigned long (WINAPI* Release)(ITaskbarList*);
    long          (WINAPI* HrInit)(ITaskbarList*);
    long          (WINAPI* AddTab)(ITaskbarList*, HWND);
    long          (WINAPI* DeleteTab)(ITaskbarList*, HWND);
    long          (WINAPI* ActivateTab)(ITaskbarList*, HWND);
    long          (WINAPI* SetActiveAlt)(ITaskbarList*, HWND);
} ITaskbarListVtbl;
struct ITaskbarList { ITaskbarListVtbl* lpVtbl; };
__declspec(dllimport) long WINAPI CoInitialize(void* pvReserved);
__declspec(dllimport) long WINAPI CoCreateInstance(const GUID* rclsid, void* pUnkOuter, unsigned long dwClsContext, const GUID* riid, void** ppv);
static const GUID CLSID_TaskbarList = {0x56FDF344,0xFD6D,0x11D0,{0x95,0x8A,0x00,0x60,0x97,0xC9,0xA0,0x90}};
static const GUID IID_ITaskbarList  = {0x56FDF342,0xFD6D,0x11D0,{0x95,0x8A,0x00,0x60,0x97,0xC9,0xA0,0x90}};
#define CLSCTX_INPROC_SERVER 1

/* RegisterWindowMessageW(L"TaskbarButtonCreated"): the shell SENDs this to a top-level
   window the moment it creates that window's taskbar button. */
static UINT g_wmTaskbarButtonCreated;
static int  g_nCmdShow;                 /* requested show-state (STARTUPINFO), applied at activation */

__declspec(safebuffers) LRESULT CALLBACK WndProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (g_wmTaskbarButtonCreated && Msg == g_wmTaskbarButtonCreated)
    {
        MSG dm;
        {
            ITaskbarList* tb;
            CoInitialize(0);
            if (CoCreateInstance(&CLSID_TaskbarList, 0, CLSCTX_INPROC_SERVER, &IID_ITaskbarList, (void**)&tb) >= 0)
            {
                tb->lpVtbl->HrInit(tb);
                tb->lpVtbl->ActivateTab(tb, hWnd);
                tb->lpVtbl->DeleteTab(tb, GetConsoleWindow());  /* remove the console's stray button */
        while (PeekMessageW(&dm, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&dm);
            DispatchMessageW(&dm);
        }
                Sleep(5);
                tb->lpVtbl->ActivateTab(tb, hWnd);
                //Sleep(500);
                //tb->lpVtbl->SetActiveAlt(tb, hWnd);
                tb->lpVtbl->Release(tb);
            }
        }
        return 0;
    }
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

__declspec(safebuffers) void mainCRTStartup(void)
{
    STARTUPINFOW si;
    int x, y;
    POINT pt;
    DWORD tMin, tMax, tCur;
    MSG msg;

    NtQueryTimerResolution(&tMin, &tMax, &tCur);
    NtSetTimerResolution(tMax, TRUE, &tCur);

    /* Hide the console without deactivating it (so it stays foreground
       and there's NO Explorer handoff). Disable transitions FIRST so the cloak and
       the offscreen move are instant (no fade/slide animation), then cloak
       (compositor hide) and shove it offscreen. The transition-disable synergizes
       with the SetWindowPos: the move doesn't animate. Only the pre-main conhost
       frame (painted by the OS before main runs) can still appear. */
    HWND con = GetConsoleWindow();
    BOOL dwmTrue = TRUE;
    SetWindowLongW(con, GWL_EXSTYLE, WS_EX_TOOLWINDOW);   /* off the taskbar, no button -- WITHOUT hiding
                                                            it (SW_HIDE bounced foreground to the shell
                                                            and back, desyncing our active state) */
    DwmSetWindowAttribute(con, DWMWA_TRANSITIONS_FORCEDISABLED, &dwmTrue, sizeof(dwmTrue));
    DwmSetWindowAttribute(con, DWMWA_CLOAK, &dwmTrue, sizeof(dwmTrue));
    SetWindowPos(con, 0, WHERE_NOONE_CAN_SEE_ME, WHERE_NOONE_CAN_SEE_ME, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    GetStartupInfoW(&si);
    g_nCmdShow = (si.dwFlags & STARTF_USESHOWWINDOW) ? (int)si.wShowWindow : SW_SHOWNORMAL;

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

    g_wmTaskbarButtonCreated = RegisterWindowMessageW(L"TaskbarButtonCreated");

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

    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"DummyWindow",
        WS_OVERLAPPEDWINDOW,
        x, y, WIN_W, WIN_H,
        0, 0, wc.hInstance, 0);

    ShowWindow(hwnd, SW_SHOWNOACTIVATE);   /* show but DON'T activate: creates our taskbar button while
                                              the (invisible) console stays foreground. The activation is
                                              deferred to the TaskbarButtonCreated handler so it happens
                                              AFTER the button exists and as a real foreground change. */
    UpdateWindow(hwnd);
    BOOL boosted = TRUE;

    for (;;)
    {
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                ExitProcess(0);
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (boosted)                       /* queue drained == loop idle: startup has settled */
        {
            NtSetTimerResolution(0, FALSE, &tCur);   /* drop the finer-timer boost here, not in WndProc */
            boosted = FALSE;
        }
        WaitMessage();                     /* block until the next message (no busy-spin) */
    }
}
