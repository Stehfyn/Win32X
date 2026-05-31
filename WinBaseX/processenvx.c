/*
 * processenvx.c -- charset-agnostic command-line argument lookup complementing processenv's
 * GetCommandLine. No CRT.
 */

#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#pragma comment(lib, "kernel32.lib")

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "processenvx.h"
#include "result.h"

LPWSTR WINAPI GetCommandLineArgumentW(_In_ LPCWSTR pszArgument)
{
    LPWSTR pszCmd;
    LPWSTR p;
    int    cchArgument;
    WCHAR  chPre;
    WCHAR  chPost;
    BOOL   fTokenStart;
    BOOL   fTokenEnd;

    pszCmd      = GetCommandLineW();
    cchArgument = lstrlenW(pszArgument);
    for (p = pszCmd; (*p); p++)
    {
        if (p == pszCmd)
        {
            chPre = L' ';
        }
        else
        {
            chPre = p[-1];
        }
        fTokenStart = (L' ' == chPre) || (L'"' == chPre);
        if (!fTokenStart)
        {
            continue;
        }
        if (CSTR_EQUAL != CompareStringOrdinal(p, cchArgument, pszArgument, cchArgument, TRUE))
        {
            continue;
        }
        chPost    = p[cchArgument];
        fTokenEnd = (!chPost) || (L' ' == chPost) || (L'"' == chPost);
        RETURN_VALUE_IF(fTokenEnd, p);
    }
    return NULL;
}

/* No CompareStringOrdinalA exists; LOCALE_INVARIANT + NORM_IGNORECASE is the ANSI equivalent for the
   ASCII flags this serves -- identical case-insensitive result, no locale dependence. */
LPSTR WINAPI GetCommandLineArgumentA(_In_ LPCSTR pszArgument)
{
    LPSTR pszCmd;
    LPSTR p;
    int   cchArgument;
    CHAR  chPre;
    CHAR  chPost;
    BOOL  fTokenStart;
    BOOL  fTokenEnd;

    pszCmd      = GetCommandLineA();
    cchArgument = lstrlenA(pszArgument);
    for (p = pszCmd; (*p); p++)
    {
        if (p == pszCmd)
        {
            chPre = ' ';
        }
        else
        {
            chPre = p[-1];
        }
        fTokenStart = (' ' == chPre) || ('"' == chPre);
        if (!fTokenStart)
        {
            continue;
        }
        if (CSTR_EQUAL != CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, p, cchArgument, pszArgument, cchArgument))
        {
            continue;
        }
        chPost    = p[cchArgument];
        fTokenEnd = (!chPost) || (' ' == chPost) || ('"' == chPost);
        RETURN_VALUE_IF(fTokenEnd, p);
    }
    return NULL;
}
