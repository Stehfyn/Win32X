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

typedef int(WINAPI* WBX_PFN_WINMAINEXA)(HINSTANCE, HINSTANCE, LPSTR, int, const STARTUPINFOA*);
typedef int(WINAPI* WBX_PFN_WINMAINEXW)(HINSTANCE, HINSTANCE, LPWSTR, int, const STARTUPINFOW*);

#ifdef UNICODE
typedef WINBASEX_REGISTRATION_PROPERTIESW  WINBASEX_REGISTRATION_PROPERTIES;
typedef PWINBASEX_REGISTRATION_PROPERTIESW PWINBASEX_REGISTRATION_PROPERTIES;
#define WBX_PFN_WINMAINEX WBX_PFN_WINMAINEXW
#else
typedef WINBASEX_REGISTRATION_PROPERTIESA  WINBASEX_REGISTRATION_PROPERTIES;
typedef PWINBASEX_REGISTRATION_PROPERTIESA PWINBASEX_REGISTRATION_PROPERTIES;
#define WBX_PFN_WINMAINEX WBX_PFN_WINMAINEXA
#endif

/* Client-supplied registration data; the startup passes &WinBaseXRegistration to WinBaseXRun. */
extern const WINBASEX_REGISTRATION_PROPERTIESA WinBaseXRegistrationA;
extern const WINBASEX_REGISTRATION_PROPERTIESW WinBaseXRegistrationW;
#ifdef UNICODE
#define WinBaseXRegistration WinBaseXRegistrationW
#else
#define WinBaseXRegistration WinBaseXRegistrationA
#endif

/*
 * WinMainEx product identity. Defined in the shared header so the client references one definition;
 * the library still receives the registration strings as data (WinBaseXRegistration), never by
 * referencing these macros.
 */
#define WC_WINMAINEX      TEXT("WinMainEx")
#define WMX_WND_TITLE     TEXT("WinMainEx")
#define WMX_FRIENDLY_NAME TEXT("WinMainEx")
#define WMX_LIST_KEY      TEXT("Software\\WinMainEx\\Launched")

BOOL WINAPI IsWinBaseXComServer(void);

/* Bounded wide->ANSI conversion into pszBufA (cchBufA chars). Returns pszBufA, or NULL for a NULL or
   unconvertible source. */
_Success_(return != NULL)
LPSTR SafeWideCharToMultiByte(_In_opt_ LPCWSTR pszW, _Out_writes_(cchBufA) LPSTR pszBufA, _In_ int cchBufA);

int WINAPI WinMainEx(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPSTR lpCmdLine,
                     _In_ int nShowCmd,
                     _In_ const STARTUPINFOA* lpStartupInfo);

int WINAPI wWinMainEx(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine,
                      _In_ int nShowCmd,
                      _In_ const STARTUPINFOW* lpStartupInfo);

#ifdef UNICODE
#define _tWinMainEx wWinMainEx
#else
#define _tWinMainEx WinMainEx
#endif

int __cdecl WinBaseXRunA(WBX_PFN_WINMAINEXA pfnWinMainEx, const WINBASEX_REGISTRATION_PROPERTIESA* lpRegistrationProperties);
int __cdecl WinBaseXRunW(WBX_PFN_WINMAINEXW pfnWinMainEx, const WINBASEX_REGISTRATION_PROPERTIESW* lpRegistrationProperties);
#ifdef UNICODE
#define WinBaseXRun WinBaseXRunW
#else
#define WinBaseXRun WinBaseXRunA
#endif

#ifdef __cplusplus
}
#endif

#endif /* WINBASEX_H */
