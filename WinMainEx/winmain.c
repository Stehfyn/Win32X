/*
 * winmain.c -- sample WinBaseX client for the WinMainEx product.
 */

#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

#define WIN32_LEAN_AND_MEAN
#define INITGUID

#include <windows.h>
#include <windowsx.h>
#include <initguid.h>
#include "WinBaseX.h"

#ifndef STARTF_HASSHELLDATA
#define STARTF_HASSHELLDATA 0x00000400
#endif

#define WC_WINMAINEX     TEXT("WinMainEx")
#define WMX_WND_TITLE     TEXT("WinMainEx")
#define WMX_FRIENDLY_NAME TEXT("WinMainEx")
#define WMX_LIST_KEY      TEXT("Software\\WinMainEx\\Launched")
#define WMX_WND_PCT       50
#define WMX_PCT_DENOM     100

DEFINE_GUID(CLSID_WinMainEx, 0xE5F1A9C2, 0x8B7D, 0x4E3F, 0xA1, 0x5C, 0x9D, 0x2E, 0x7B, 0x6F, 0x4A, 0x83);

static void ShowGui(const STARTUPINFO* psi);

BOOL WINAPI GetWinBaseXRegistrationProperties(_Out_ PWINBASEX_REGISTRATION_PROPERTIES pRegistrationProperties)
{
    if (NULL == pRegistrationProperties)
    {
        return FALSE;
    }

    pRegistrationProperties->cb                 = (DWORD)sizeof((*pRegistrationProperties));
    pRegistrationProperties->lpClsid            = &CLSID_WinMainEx;
    pRegistrationProperties->lpFriendlyName     = WMX_FRIENDLY_NAME;
    pRegistrationProperties->lpLaunchHistoryKey = WMX_LIST_KEY;
    pRegistrationProperties->dwFlags            = 0;
    return TRUE;
}

/* void Cls_OnDestroy(HWND hwnd) */
static void WinMainEx_OnDestroy(HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);

    if (!IsWinBaseXComServer())
    {
        PostQuitMessage(0);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_DESTROY, WinMainEx_OnDestroy);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static void CenterOnMonitor(HWND hwnd, HMONITOR hMon)
{
    MONITORINFO mi;
    BOOL        fGotInfo;
    int         nWorkWidth;
    int         nWorkHeight;
    int         nWidth;
    int         nHeight;
    int         nX;
    int         nY;

    SecureZeroMemory(&mi, sizeof(mi));
    mi.cbSize = (DWORD)sizeof(mi);
    fGotInfo  = GetMonitorInfo(hMon, &mi);
    if (!fGotInfo)
    {
        return;
    }
    nWorkWidth  = (int)(mi.rcWork.right - mi.rcWork.left);
    nWorkHeight = (int)(mi.rcWork.bottom - mi.rcWork.top);
    nWidth      = nWorkWidth * WMX_WND_PCT / WMX_PCT_DENOM;
    nHeight     = nWorkHeight * WMX_WND_PCT / WMX_PCT_DENOM;
    nX          = mi.rcWork.left + (nWorkWidth - nWidth) / 2;
    nY          = mi.rcWork.top + (nWorkHeight - nHeight) / 2;
    SetWindowPos(hwnd, HWND_DESKTOP, nX, nY, nWidth, nHeight, SWP_NOZORDER | SWP_NOACTIVATE);
}

static void ShowGui(const STARTUPINFO* psi)
{
    WNDCLASS  wc;
    HINSTANCE hInstance;
    HMONITOR  hMon;
    HWND      hwnd;
    int       nCmdShow;
    BOOL      fUseShow;
    BOOL      fHasShellData;

    hInstance = GetModuleHandle(NULL);
    fUseShow  = !!(STARTF_USESHOWWINDOW & psi->dwFlags);
    if (fUseShow)
    {
        nCmdShow = (int)psi->wShowWindow;
    }
    else
    {
        nCmdShow = SW_SHOWDEFAULT;
    }

    /* Idempotent: a second RegisterClass fails with ERROR_CLASS_ALREADY_EXISTS and the class
       stays registered, so CreateWindowEx works regardless -- no need to track first-call state. */
    SecureZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WC_WINMAINEX;
    RegisterClass(&wc);

    hwnd = CreateWindowEx(0,
                           WC_WINMAINEX,
                           WMX_WND_TITLE,
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT,
                           CW_USEDEFAULT,
                           CW_USEDEFAULT,
                           CW_USEDEFAULT,
                           GetDesktopWindow(),
                           NULL,
                           hInstance,
                           NULL);
    if (!hwnd)
    {
        return;
    }

    fHasShellData = !!(STARTF_HASSHELLDATA & psi->dwFlags);
    if (fHasShellData)
    {
        hMon = (HMONITOR)psi->hStdOutput;
    }
    else
    {
        hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    }
    CenterOnMonitor(hwnd, hMon);
    ShowWindow(hwnd, nCmdShow);
    SetForegroundWindow(hwnd);
}

int WINAPI _tWinMainEx(_In_ HINSTANCE          hInstance,
                       _In_opt_ HINSTANCE      hPrevInstance,
                       _In_ LPTSTR             lpCmdLine,
                       _In_ int                nShowCmd,
                       _In_ const STARTUPINFO* lpStartupInfo)
{
    MSG msg;

    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);

    ShowGui(lpStartupInfo);
    if (IsWinBaseXComServer())
    {
        return 0;
    }

    while (0 < GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
