/*
 * processenvxText.inl -- generic-text bodies for processenvx, compiled twice (UNICODE -> the W
 * symbols, not -> the A symbols), so the GetCommandLineArguments / GetCommandLineArgument A/W pairs
 * share one source instead of two hand-kept copies. The sole charset-divergent step -- whole-token
 * comparison -- is isolated below: CompareStringOrdinal has no ANSI form, so the A build uses the
 * LOCALE_INVARIANT + NORM_IGNORECASE equivalent (identical for the ASCII flags this serves).
 */

#ifdef UNICODE
#define ARG_TOKEN_EQUAL(p, arg, cch) \
    (CSTR_EQUAL == CompareStringOrdinal((p), (cch), (arg), (cch), TRUE))
#else
#define ARG_TOKEN_EQUAL(p, arg, cch) \
    (CSTR_EQUAL == CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, (p), (cch), (arg), (cch)))
#endif

DECLSPEC_NOINLINE LPTSTR WINAPI GetCommandLineArguments(void)
{
    LPTSTR pszCmd;

    pszCmd = GetCommandLine();
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
        while ((*pszCmd) && (!IsWhiteSpace(*pszCmd)))
        {
            pszCmd++;
        }
    }
    while (IsWhiteSpace(*pszCmd))
    {
        pszCmd++;
    }
    return pszCmd;
}

/* Search the argument tail only -- never argv[0] -- so the program path can't false-match a flag. */
DECLSPEC_NOINLINE LPTSTR WINAPI GetCommandLineArgument(_In_ LPCTSTR pszArgument)
{
    LPTSTR pszCmd;
    LPTSTR p;
    int    cchArgument;
    TCHAR  chPre;
    TCHAR  chPost;
    BOOL   fTokenStart;
    BOOL   fTokenEnd;

    pszCmd      = GetCommandLineArguments();
    cchArgument = lstrlen(pszArgument);
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
        if (!ARG_TOKEN_EQUAL(p, pszArgument, cchArgument))
        {
            continue;
        }
        chPost    = p[cchArgument];
        fTokenEnd = (!chPost) || (' ' == chPost) || ('"' == chPost);
        RETURN_VALUE_IF(fTokenEnd, p);
    }
    return NULL;
}

#undef ARG_TOKEN_EQUAL
