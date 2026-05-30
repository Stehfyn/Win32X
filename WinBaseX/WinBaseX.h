#ifndef WINBASEX_H
#define WINBASEX_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _WINBASEX_REGISTRATION_PROPERTIESW
{
    DWORD        cb;
    const CLSID *lpClsid;
    LPCWSTR      lpFriendlyName;
    LPCWSTR      lpLaunchHistoryKey;
    DWORD        dwFlags;
} WINBASEX_REGISTRATION_PROPERTIESW, *PWINBASEX_REGISTRATION_PROPERTIESW;

BOOL
WINAPI
GetWinBaseXRegistrationProperties(
    _Out_ PWINBASEX_REGISTRATION_PROPERTIESW lpRegistrationProperties
    );

BOOL
WINAPI
IsWinBaseXComServer(void);

int
WINAPI
WinMainEx(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd,
    _In_ const STARTUPINFOA *lpStartupInfo
    );

int
WINAPI
wWinMainEx(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd,
    _In_ const STARTUPINFOW *lpStartupInfo
    );

#ifdef __cplusplus
}
#endif

#endif /* WINBASEX_H */
