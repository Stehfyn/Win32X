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
#include "WinUserX.h"

#define WMX_PCT_NUM       50
#define WMX_PCT_DENOM     100

DEFINE_GUID(CLSID_WinMainEx, 0xE5F1A9C2, 0x8B7D, 0x4E3F, 0xA1, 0x5C, 0x9D, 0x2E, 0x7B, 0x6F, 0x4A, 0x83);

static void             PlaceStartupWindow(HWND hwnd);
static ATOM             MyRegisterClass(HINSTANCE hInstance);
static BOOL             InitInstance(HINSTANCE hInstance, int nCmdShow);
static void    CALLBACK OnDestroy(HWND hwnd);
static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static void PlaceStartupWindow(HWND hwnd)
{
    RECT rcWork;
    BOOL fGotWork;
    SIZE sizeDefault;
    RECT rcPos;
    BOOL fGotPos;
    int  nX;
    int  nY;
    int  nWidth;
    int  nHeight;

    /* This client's default-extent policy: 50% of the primary work area. CalculateWindowStartupPosition
       resolves the launch monitor, honors any launcher-specified STARTUPINFO position/size, and
       otherwise centers this default extent on the work area. */
    SecureZeroMemory(&rcWork, sizeof(rcWork));
    fGotWork = SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0);
    if (!fGotWork)
    {
        return;
    }

    sizeDefault.cx = (rcWork.right - rcWork.left) * WMX_PCT_NUM / WMX_PCT_DENOM;
    sizeDefault.cy = (rcWork.bottom - rcWork.top) * WMX_PCT_NUM / WMX_PCT_DENOM;

    fGotPos = CalculateWindowStartupPosition(&sizeDefault, &rcPos);
    if (!fGotPos)
    {
        return;
    }

    nX      = (int)rcPos.left;
    nY      = (int)rcPos.top;
    nWidth  = (int)(rcPos.right - rcPos.left);
    nHeight = (int)(rcPos.bottom - rcPos.top);
    SetWindowPos(hwnd, HWND_DESKTOP, nX, nY, nWidth, nHeight, SWP_NOZORDER | SWP_NOACTIVATE);
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
static BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
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
                          GetDesktopWindow(),
                          NULL,
                          hInstance,
                          NULL);
    if (!hwnd)
    {
        return FALSE;
    }

    PlaceStartupWindow(hwnd);
    ShowWindow(hwnd, nCmdShow);
    SetForegroundWindow(hwnd);

    /* A COM-server (shell DelegateExecute) activation only needs the window shown; WinBaseX pumps
       messages for the embedding, so the client must not run its own loop. */
    if (IsWinBaseXComServer())
    {
        return FALSE;
    }
    return TRUE;
}

const WINBASEX_REGISTRATION_PROPERTIES WinBaseXRegistration = {
    sizeof(WinBaseXRegistration), &CLSID_WinMainEx, WMX_FRIENDLY_NAME, WMX_LIST_KEY, 0};

static void CALLBACK OnDestroy(HWND hwnd)
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
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI _tWinMainEx(_In_ HINSTANCE          hInstance,
                       _In_opt_ HINSTANCE      hPrevInstance,
                       _In_ LPTSTR             lpCmdLine,
                       _In_ int                nShowCmd,
                       _In_ const STARTUPINFO* lpStartupInfo)
{
    MSG msg;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(lpStartupInfo);

    /* RegisterClass returns 0 on a repeated activation (ERROR_CLASS_ALREADY_EXISTS); harmless --
       the class stays registered, so InitInstance's CreateWindowEx succeeds regardless. */
    MyRegisterClass(hInstance);
    if (!InitInstance(hInstance, nShowCmd))
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
