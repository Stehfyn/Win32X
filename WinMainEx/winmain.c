#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(linker, "/MERGE:.pdata=.rdata")

/* Reconstructed to match ConsoleApplication1-mt-win32.exe (x64 Release /MT).
   Uses the standard CRT entry (int main) so libcmt's real startup links in,
   reproducing the full-CRT import/section profile of the target binary. */

static const void* NtCurrentImage(void)
{
    return (unsigned short*)*(unsigned char**)((unsigned short*)(*(unsigned char**)((unsigned char*)__readgsqword(0x60) + 0x20) + 0x60) + 0x04);
}

static unsigned short NtStartupReserved2(void)
{
    return *(unsigned short*)((unsigned char*)NtCurrentImage() - 0x05A4);
}

LRESULT CALLBACK WndProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}

static WNDCLASSW wc = { 0 };

static HWND DummyWindow(void)
{
    wc.lpfnWndProc = WndProcW;
    wc.hInstance = GetModuleHandleW(0);
    wc.lpszClassName = L"DummyWindowClass";
    wc.hCursor = LoadCursorW(0, IDC_ARROW);
    RegisterClassW(&wc);
    return CreateWindowExW(0, wc.lpszClassName, L"DummyWindow", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        0, 0, wc.hInstance, 0);
}

#pragma pack(1)
typedef struct _SHARED_STARTUP_INFO_W {
    STARTUPINFOEXW StartupInfoEx;
    PROCESS_INFORMATION ProcessInfo;
    unsigned char attributeList[72];
} SHARED_STARTUP_INFO_W, * PSHARED_STARTUP_INFO_W;
#pragma pack()

int main(void)
{
    BOOL fDisable = TRUE;

    if (0x00 == NtStartupReserved2())
    {
        SHARED_STARTUP_INFO_W _;
        SecureZeroMemory(&_, sizeof(_));
        _.StartupInfoEx.StartupInfo.cb = sizeof(STARTUPINFOW);
        _.StartupInfoEx.StartupInfo.dwFlags = STARTF_FORCEOFFFEEDBACK;
        CreateProcessW(NtCurrentImage(), 0, 0, 0, 0,
            CREATE_NO_WINDOW | DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_IGNORE_SYSTEM_DEFAULT,
            0, 0, &_.StartupInfoEx.StartupInfo, &_.ProcessInfo);
        CloseHandle(_.ProcessInfo.hThread);
        CloseHandle(_.ProcessInfo.hProcess);
        ExitProcess(0);
    }
    else
    {
        FreeConsole();

        HWND dummy = DummyWindow();
        DwmSetWindowAttribute(dummy, DWMWA_TRANSITIONS_FORCEDISABLED, (LPCVOID)&fDisable, sizeof(fDisable));

        ShowWindow(dummy, SW_SHOWMINIMIZED);
        SetForegroundWindow(dummy);
        UpdateWindow(dummy);

        EnableWindow(dummy, TRUE);
        SetActiveWindow(dummy);
        LockSetForegroundWindow(LSFW_UNLOCK);
        if (GetForegroundWindow() != dummy)
            FlashWindow(dummy, TRUE);

        ShowWindow(dummy, SW_SHOWNORMAL);
        UpdateWindow(dummy);

        MessageBoxW(0, L"Yay, you re-executed yourself!", L"WinMainEx", 0);

        MSG msg;
        while (0 < GetMessageW(&msg, 0, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(0);
    }
    return 0;
}
