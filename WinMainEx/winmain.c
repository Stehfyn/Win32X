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
#include "result.h"

DEFINE_GUID(CLSID_WinMainEx, 0xE5F1A9C2, 0x8B7D, 0x4E3F, 0xA1, 0x5C, 0x9D, 0x2E, 0x7B, 0x6F, 0x4A, 0x83);

static ATOM             MyRegisterClass(HINSTANCE hInstance);
static BOOL             InitInstance(HINSTANCE hInstance);
static void    CALLBACK OnDestroy(HWND hwnd);
static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

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
    RETURN_FALSE_IF_NULL(hwnd);

    ShowWindowEx(hwnd, SWX_SHOWSTARTUP);
    UpdateWindow(hwnd);

    /* A COM-server (shell DelegateExecute) activation only needs the window shown; WinBaseX pumps
       messages for the embedding, so the client must not run its own loop. */
    RETURN_FALSE_IF(IsWinBaseXComServer());
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
    UNREFERENCED_PARAMETER(nShowCmd);
    UNREFERENCED_PARAMETER(lpStartupInfo);

    /* RegisterClass returns 0 on a repeated activation (ERROR_CLASS_ALREADY_EXISTS); harmless --
       the class stays registered, so InitInstance's CreateWindowEx succeeds regardless. */
    MyRegisterClass(hInstance);
    RETURN_ZERO_IF_NOT(InitInstance(hInstance));

    while (0 < GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
