#ifndef WINBASEX_H
#define WINBASEX_H

#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef int(WINAPI* WBX_PFN_WINMAINEXA)(HINSTANCE, HINSTANCE, LPSTR, int, const STARTUPINFOA*);
typedef int(WINAPI* WBX_PFN_WINMAINEXW)(HINSTANCE, HINSTANCE, LPWSTR, int, const STARTUPINFOW*);

#ifdef UNICODE
#define WBX_PFN_WINMAINEX WBX_PFN_WINMAINEXW
#else
#define WBX_PFN_WINMAINEX WBX_PFN_WINMAINEXA
#endif

/*
 * Identity of the single, machine-wide exefile DelegateExecute launch broker. This is the library's
 * identity, not the client's: there is exactly one exefile\shell\open\command\DelegateExecute slot,
 * so one shared CLSID brokers launches for every WinBaseX-hosted exe. Declared here, defined once in
 * WinBaseX.c alongside the interface IIDs (the project links /NODEFAULTLIB, so there is no uuid.lib;
 * WinBaseX.c is the hand-written definition TU, in the spirit of a MIDL _i.c).
 */
extern const CLSID CLSID_WinBaseXLaunchBroker;

/* Bounded wide->ANSI conversion into pszBufA (cchBufA chars). Returns pszBufA, or NULL for a NULL or
   unconvertible source. */
_Success_(return != NULL)
LPSTR SafeWideCharToMultiByte(_In_opt_ LPCWSTR pszW, _Out_writes_(cchBufA) LPSTR pszBufA, _In_ int cchBufA);

/* Bounded ANSI->wide conversion into pszBufW (cchBufW chars). Returns pszBufW, or NULL for a NULL or
   unconvertible source. */
_Success_(return != NULL)
LPWSTR SafeMultiByteToWideChar(_In_opt_ LPCSTR pszA, _Out_writes_(cchBufW) LPWSTR pszBufW, _In_ int cchBufW);

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

int __cdecl WinBaseXRunA(WBX_PFN_WINMAINEXA pfnWinMainEx);
int __cdecl WinBaseXRunW(WBX_PFN_WINMAINEXW pfnWinMainEx);
#ifdef UNICODE
#define WinBaseXRun WinBaseXRunW
#else
#define WinBaseXRun WinBaseXRunA
#endif

#ifdef __cplusplus
}
#endif

#endif /* WINBASEX_H */
