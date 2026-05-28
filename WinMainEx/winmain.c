#include <windows.h>
#include <tchar.h>
#include <dwmapi.h>

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

LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, Msg, wParam, lParam);
}

int main(void)
{
    BOOL first = TRUE;

    FreeConsole();
    LockSetForegroundWindow(LSFW_LOCK);

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = TEXT("DummyWindowClass");
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, wc.lpszClassName, TEXT("DummyWindow"),
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
    while (GetMessage(&msg, 0, 0, 0) > 0)
    {
        if (hwnd != GetForegroundWindow() && first)
        {
            SetForegroundWindow(hwnd);
            if (hwnd == GetForegroundWindow())
            {
                first = FALSE;
                EnableWindow(hwnd, TRUE);
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
