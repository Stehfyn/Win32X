#ifndef WINBASEX_H
#define WINBASEX_H

#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _WINBASEX_REGISTRATION_PROPERTIESA
{
    DWORD        cb;
    const CLSID* lpClsid;
    LPCSTR       lpFriendlyName;
    LPCSTR       lpLaunchHistoryKey;
    DWORD        dwFlags;
} WINBASEX_REGISTRATION_PROPERTIESA, *PWINBASEX_REGISTRATION_PROPERTIESA;

typedef struct _WINBASEX_REGISTRATION_PROPERTIESW
{
    DWORD        cb;
    const CLSID* lpClsid;
    LPCWSTR      lpFriendlyName;
    LPCWSTR      lpLaunchHistoryKey;
    DWORD        dwFlags;
} WINBASEX_REGISTRATION_PROPERTIESW, *PWINBASEX_REGISTRATION_PROPERTIESW;

/* Client-implemented (one variant, matching the entry point the client links). The library converts
   an ANSI client's strings up to wide internally for the (wide) registry. */
BOOL WINAPI GetWinBaseXRegistrationPropertiesA(_Out_ PWINBASEX_REGISTRATION_PROPERTIESA lpRegistrationProperties);
BOOL WINAPI GetWinBaseXRegistrationPropertiesW(_Out_ PWINBASEX_REGISTRATION_PROPERTIESW lpRegistrationProperties);

#ifdef UNICODE
typedef WINBASEX_REGISTRATION_PROPERTIESW  WINBASEX_REGISTRATION_PROPERTIES;
typedef PWINBASEX_REGISTRATION_PROPERTIESW PWINBASEX_REGISTRATION_PROPERTIES;
#define GetWinBaseXRegistrationProperties GetWinBaseXRegistrationPropertiesW
#else
typedef WINBASEX_REGISTRATION_PROPERTIESA  WINBASEX_REGISTRATION_PROPERTIES;
typedef PWINBASEX_REGISTRATION_PROPERTIESA PWINBASEX_REGISTRATION_PROPERTIES;
#define GetWinBaseXRegistrationProperties GetWinBaseXRegistrationPropertiesA
#endif

BOOL WINAPI IsWinBaseXComServer(void);

int WINAPI  WinMainEx(_In_ HINSTANCE           hInstance,
                      _In_opt_ HINSTANCE       hPrevInstance,
                      _In_ LPSTR               lpCmdLine,
                      _In_ int                 nShowCmd,
                      _In_ const STARTUPINFOA* lpStartupInfo);

int WINAPI  wWinMainEx(_In_ HINSTANCE           hInstance,
                        _In_opt_ HINSTANCE       hPrevInstance,
                        _In_ LPWSTR              lpCmdLine,
                        _In_ int                 nShowCmd,
                        _In_ const STARTUPINFOW* lpStartupInfo);

/* Generic-text client entry, mirroring <tchar.h>'s _tWinMain: a charset-agnostic client implements
   _tWinMainEx with LPTSTR / const STARTUPINFO* and it resolves to wWinMainEx or WinMainEx. */
#ifdef UNICODE
#define _tWinMainEx wWinMainEx
#else
#define _tWinMainEx WinMainEx
#endif

#ifdef __cplusplus
}
#endif

#endif /* WINBASEX_H */
