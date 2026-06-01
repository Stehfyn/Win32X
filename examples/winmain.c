/*
 * winmain.c -- sample WinBaseX client for the WinMainEx product.
 */

#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include "win32x.h"

/* Client-owned window-class name and caption -- UI identity, distinct from the WinBaseX launch
   broker's COM identity (CLSID_WinBaseXLaunchBroker), which the library owns. */
#define WC_WINMAINEX  TEXT("WinMainEx")
#define WMX_WND_TITLE TEXT("WinMainEx")

static ATOM             MyRegisterClass(HINSTANCE hInstance);
static BOOL             InitInstance(HINSTANCE hInstance);
static void    CALLBACK OnDestroy(HWND hwnd);
static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI _tWinMainEx(_In_ HINSTANCE          hInstance,
                       _In_opt_ HINSTANCE      hPrevInstance,
                       _In_ LPTSTR             lpCmdLine,
                       _In_ int                nShowCmd,
                       _In_ const STARTUPINFO* lpStartupInfo)
{
    MSG msg;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);
    UNREFERENCED_PARAMETER(lpStartupInfo);

    /* A plain CRT-style WinMain: register the class (RegisterClass returns 0 on a repeated activation
       -- ERROR_CLASS_ALREADY_EXISTS, harmless), bring the window up, run the message pump, return.
       The launch broker never calls in here -- a DelegateExecute activation spawns a fresh process
       that re-enters on the direct path -- so there is no launch mode to branch on. */
    MyRegisterClass(hInstance);
    if (!InitInstance(hInstance))
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

/* Registers the window class. */
static ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASS wc;

    SecureZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WC_WINMAINEX;

    return RegisterClass(&wc);
}

/* Creates and displays the main window. */
static BOOL InitInstance(HINSTANCE hInstance)
{
    HWND hwnd;

    hwnd = CreateWindowEx(0,
                          WC_WINMAINEX,
                          WMX_WND_TITLE,
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

    ShowWindowEx(hwnd, SWX_SHOWSTARTUP);
    UpdateWindow(hwnd);
    return TRUE;
}

static void CALLBACK OnDestroy(HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);

    /* Ends whichever pump is running -- WinBaseXRun's on a direct launch, RunComServer's under a
       DelegateExecute embedding. A transient COM launch dies with its window; that is correct. */
    PostQuitMessage(0);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
