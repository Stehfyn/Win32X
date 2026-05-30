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
#define STARTF_HASSHELLDATA  0x00000400
#endif

#define WMX_WND_CLASS        L"WinMainEx"
#define WMX_WND_TITLE        L"WinMainEx"
#define WMX_FRIENDLY_NAME    L"WinMainEx"
#define WMX_LIST_KEY         L"Software\\WinMainEx\\Launched"
#define WMX_WND_PCT          50
#define WMX_PCT_DENOM        100

DEFINE_GUID(CLSID_WinMainEx, 0xE5F1A9C2, 0x8B7D, 0x4E3F, 0xA1, 0x5C, 0x9D, 0x2E, 0x7B, 0x6F, 0x4A, 0x83);

static void ShowGui(const STARTUPINFOW *psi);

BOOL WINAPI GetWinBaseXRegistrationProperties(_Out_ PWINBASEX_REGISTRATION_PROPERTIESW pRegistrationProperties)
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

static LRESULT CALLBACK WinMainEx_WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_DESTROY, WinMainEx_OnDestroy);
    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
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
    fGotInfo  = GetMonitorInfoW(hMon, &mi);
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
    SetWindowPos(hwnd, NULL, nX, nY, nWidth, nHeight, SWP_NOZORDER | SWP_NOACTIVATE);
}

static void ShowGui(const STARTUPINFOW *psi)
{
    static BOOL s_fClassRegistered = FALSE;
    WNDCLASSW   wc;
    HINSTANCE   hInstance;
    HMONITOR    hMon;
    HWND        hwnd;
    int         nCmdShow;
    BOOL        fUseShow;
    BOOL        fHasShellData;

    hInstance = GetModuleHandleW(NULL);
    fUseShow  = !!(STARTF_USESHOWWINDOW & psi->dwFlags);
    if (fUseShow)
    {
        nCmdShow = (int)psi->wShowWindow;
    }
    else
    {
        nCmdShow = SW_SHOWDEFAULT;
    }

    if (!s_fClassRegistered)
    {
        SecureZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc   = WinMainEx_WndProc;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = WMX_WND_CLASS;
        RegisterClassW(&wc);
        s_fClassRegistered = TRUE;
    }

    hwnd = CreateWindowExW(0, WMX_WND_CLASS, WMX_WND_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                           CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
    if (NULL == hwnd)
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

int WINAPI wWinMainEx(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd,
    _In_ const STARTUPINFOW *lpStartupInfo
    )
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

    while (0 < GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
