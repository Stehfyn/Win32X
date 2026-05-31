#ifndef PROCESSENVX_H
#define PROCESSENVX_H

#include <windows.h>

#ifdef __cplusplus
extern "C"
{
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

/*
 * GetCommandLineArguments -- the argument tail of the process command line: GetCommandLine with the
 * argv[0] token (quoted or bare) and the whitespace after it skipped. Returns a pointer within the
 * command line; never NULL (an empty string when there are no arguments). Defined here, so each
 * includer's compile fixes the charset (static, internal to each translation unit).
 */
static LPWSTR GetCommandLineArgumentsW(void)
{
    LPWSTR pszCmd;

    pszCmd = GetCommandLineW();
    if (L'"' == (*pszCmd))
    {
        pszCmd++;
        while ((*pszCmd) && (L'"' != (*pszCmd)))
        {
            pszCmd++;
        }
        if (L'"' == (*pszCmd))
        {
            pszCmd++;
        }
    }
    else
    {
        while ((*pszCmd) && (L' ' < (*pszCmd)))
        {
            pszCmd++;
        }
    }
    while ((L' ' == (*pszCmd)) || (L'\t' == (*pszCmd)))
    {
        pszCmd++;
    }
    return pszCmd;
}

static LPSTR GetCommandLineArgumentsA(void)
{
    LPSTR pszCmd;

    pszCmd = GetCommandLineA();
    if ('"' == (*pszCmd))
    {
        pszCmd++;
        while ((*pszCmd) && ('"' != (*pszCmd)))
        {
            pszCmd++;
        }
        if ('"' == (*pszCmd))
        {
            pszCmd++;
        }
    }
    else
    {
        while ((*pszCmd) && (' ' < (*pszCmd)))
        {
            pszCmd++;
        }
    }
    while ((' ' == (*pszCmd)) || ('\t' == (*pszCmd)))
    {
        pszCmd++;
    }
    return pszCmd;
}
#ifdef UNICODE
#define GetCommandLineArguments GetCommandLineArgumentsW
#else
#define GetCommandLineArguments GetCommandLineArgumentsA
#endif

#ifdef __cplusplus
}
#endif

#endif /* PROCESSENVX_H */
