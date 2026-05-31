#ifndef PROCESSENVX_H
#define PROCESSENVX_H

#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * GetCommandLineArguments -- the argument tail of the process command line: GetCommandLine with the
 * argv[0] token (quoted or bare) and the whitespace after it skipped. Returns a pointer within the
 * command line; never NULL (an empty string when there are no arguments).
 */
LPWSTR WINAPI GetCommandLineArgumentsW(void);
LPSTR  WINAPI GetCommandLineArgumentsA(void);
#ifdef UNICODE
#define GetCommandLineArguments GetCommandLineArgumentsW
#else
#define GetCommandLineArguments GetCommandLineArgumentsA
#endif

/*
 * GetCommandLineArgument -- whole-token, case-insensitive search of the process command line
 * (GetCommandLine) for pszArgument. Returns a pointer to the matched token within the command line,
 * or NULL when absent. A token is bounded by a space or double quote (or either end of the string),
 * so "/x" matches "/x" but not "/xyz" or "prefix/x". Both leaves always defined, like the SDK's own
 * A/W pairs; GetCommandLineArgument resolves by this build's charset.
 */
LPWSTR WINAPI GetCommandLineArgumentW(_In_ LPCWSTR pszArgument);
LPSTR  WINAPI GetCommandLineArgumentA(_In_ LPCSTR pszArgument);
#ifdef UNICODE
#define GetCommandLineArgument GetCommandLineArgumentW
#else
#define GetCommandLineArgument GetCommandLineArgumentA
#endif

#ifdef __cplusplus
}
#endif

#endif /* PROCESSENVX_H */
