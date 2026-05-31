/*
 * WinUserXThunks.inl -- delay-bound user32 DPI thunk bodies (WinUser.h origin). Included exactly once,
 * by WinUserX.c, so the wrapper definitions live in one translation unit (never in a multiply-included
 * header). user32 is resident by the time these run (the window already exists), so GetModuleHandle
 * resolution via DECLARE_DLL_THUNK suffices; each wrapper degrades to a legacy/constant fallback when
 * the *ForDpi export is absent (pre-Windows 10 1607).
 *
 * DECLARE_DLL_THUNK argument order: (_DllName, _RetType, _WrapperName, _ExportName, _Args1, _Args2,
 * _Fallback) -- one argument per line.
 */

DECLARE_DLL_THUNK(TEXT("user32.dll"),
                  UINT,
                  GetDpiForWindowEx,
                  GetDpiForWindow,
                  (HWND hwnd),
                  (hwnd),
                  USER_DEFAULT_SCREEN_DPI)

DECLARE_DLL_THUNK(TEXT("user32.dll"),
                  UINT,
                  GetDpiForSystemEx,
                  GetDpiForSystem,
                  (void),
                  (),
                  USER_DEFAULT_SCREEN_DPI)

DECLARE_DLL_THUNK(TEXT("user32.dll"),
                  int,
                  GetSystemMetricsForDpiEx,
                  GetSystemMetricsForDpi,
                  (int nIndex, UINT dpi),
                  (nIndex, dpi),
                  GetSystemMetrics(nIndex))

DECLARE_DLL_THUNK(TEXT("user32.dll"),
                  BOOL,
                  AdjustWindowRectExForDpiEx,
                  AdjustWindowRectExForDpi,
                  (LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi),
                  (lpRect, dwStyle, bMenu, dwExStyle, dpi),
                  AdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle))

DECLARE_DLL_THUNK(TEXT("user32.dll"),
                  BOOL,
                  SystemParametersInfoForDpiExW,
                  SystemParametersInfoForDpi,
                  (UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi),
                  (uiAction, uiParam, pvParam, fWinIni, dpi),
                  SystemParametersInfoW(uiAction, uiParam, pvParam, fWinIni))

DECLARE_DLL_THUNK(TEXT("user32.dll"),
                  BOOL,
                  SystemParametersInfoForDpiExA,
                  SystemParametersInfoForDpi,
                  (UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi),
                  (uiAction, uiParam, pvParam, fWinIni, dpi),
                  SystemParametersInfoA(uiAction, uiParam, pvParam, fWinIni))

DECLARE_DLL_THUNK(TEXT("user32.dll"),
                  DPI_AWARENESS_CONTEXT,
                  SetThreadDpiAwarenessContextEx,
                  SetThreadDpiAwarenessContext,
                  (DPI_AWARENESS_CONTEXT dpiContext),
                  (dpiContext),
                  NULL)
