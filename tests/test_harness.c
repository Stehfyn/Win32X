/*
 * test_harness.c -- single-binary test runner for WinBaseX (no CRT, no PowerShell; any Windows version).
 *
 * Provides the harness (byte output, pass/fail accounting, entry point) and includes tests.inl, which
 * holds the test cases. Links WinBaseX.lib directly, exercising the real library. Prints [PASS]/[FAIL]/
 * [SKIP] per case and exits with the failure count.
 *
 * No CRT: linking WinBaseX.lib pulls /NODEFAULTLIB, so only Win32 and compiler intrinsics are used. The
 * output stream is a deliberate narrow byte format (console), so the A-form string calls are pinned.
 */

#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(linker,                                                                                 \
                "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' "          \
                "version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' "        \
                "language='*'\"")

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <commctrl.h>
#include <shlwapi.h>
#include "Win32X/win32x.h"
#include "Win32X/uxthemex.h"

#ifndef DPI_AWARENESS_CONTEXT_UNAWARE
#define DPI_AWARENESS_CONTEXT_UNAWARE ((DPI_AWARENESS_CONTEXT)-1)
#endif
#ifndef DPI_AWARENESS_CONTEXT_SYSTEM_AWARE
#define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE ((DPI_AWARENESS_CONTEXT)-2)
#endif
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

#define SECONDS_TO_MILLISECONDS 1000
#define FMT_CCH                 512
#define CMD_CCH                 (MAX_PATH + 16)
#define PROBE_INSET             10
#define LAUNCH_INSET            80
#define WAIT_MS                 (2 * SECONDS_TO_MILLISECONDS)
#define MULTIMON_MIN            2
#define CENTER_SLACK            1
#define EXIT_OK                 0
#define EXIT_FAIL               1
#define THEME_TEST_DEFERRED_MSG (WM_APP + 91)
#define THEME_CAPTURE_FRAMES    360u
#define THEME_ENCODE_QUEUE_FRAMES 12u
#define THEME_DXGI_TIMEOUT_MS   WAIT_MS
#define THEME_DRAIN_MS          1000u
/* Capture/settle window: the immersive crossfade runs well under a second (transition + DWM caption +
   slack). 1100ms captures the whole transition with margin without padding each run with a needless
   extra second of plateau -- decoupled from WAIT_MS (the 2s hang timeout) so timeouts stay generous. */
#define THEME_RECORD_MS         1000u

typedef struct
{
    RECT rcWork;
    BOOL fFound;
} SECOND_MON;

typedef struct
{
    RECT  rc;
    LONG  cx;
    LONG  cy;
    LONG  cbFrame;
    BYTE* pbFrames;
    ID3D11Texture2D** ppFrames;
    ID3D11Texture2D** ppEncodeFrames;
    HANDLE hEncodeReady;
    HANDLE hEncodeStarted;
    volatile LONG cReadyFrames;
    volatile LONG cEncodedFrames;
    volatile LONG fCaptureComplete;
    BOOL  fEncodeOverflow;
    UINT  cQueueFrames;
    UINT  cFrames;
    UINT  cCaptured;
    UINT  iDroppedFrame;
    UINT  cMaxAccumulated;
    UINT  iFirstMixedFrame;
    UINT  iLastMixedFrame;
    UINT  iFirstTargetFrame;
    UINT  iFirstIntermediateFrame;
    UINT  uMixedMask;
    UINT  uIntermediateMask;
    BOOL  fDropped;
} THEME_CAPTURE;

typedef struct
{
    ID3D11Device*        pDevice;
    ID3D11DeviceContext* pContext;
    IDXGIOutputDuplication* pDup;
    ID3D11Texture2D*     pStaging;
    LONG                 xOutput;
    LONG                 yOutput;
    UINT                 cxDesktop;
    UINT                 cyDesktop;
    UINT                 uRefreshNumerator;
    UINT                 uRefreshDenominator;
} THEME_DXGI;

typedef struct
{
    THEME_DXGI*    pDxgi;
    THEME_CAPTURE* pCap;
    HANDLE         hDone;
    DWORD          dwThreadId;
    BOOL           fOk;
} THEME_CAPTURE_RUN;

typedef struct
{
    THEME_DXGI*    pDxgi;
    THEME_CAPTURE* pCap;
    HANDLE         hDone;
    HRESULT        hr;
    UINT           uStage;
    HRESULT        hrDecode;
    DWORD          dwThreadId;
    BOOL           fOk;
} THEME_ENCODE_RUN;

typedef void (*PFN_TEST)(void);

static HANDLE g_out;
static int    g_fail;

/* Generic-text in; convert to bytes only here, at the console sink (the one narrow external edge). */
static void Out(LPCTSTR psz)
{
#ifdef UNICODE
    CHAR szBytes[FMT_CCH];
    int  cbBytes;
#endif
    DWORD cbWritten;

#ifdef UNICODE
    cbBytes = WideCharToMultiByte(CP_UTF8, 0, psz, -1, szBytes, FMT_CCH, NULL, NULL);
    if (IsPositive(cbBytes))
    {
        WriteFile(g_out, szBytes, (DWORD)(cbBytes - 1), &cbWritten, NULL);
    }
#else
    WriteFile(g_out, psz, (DWORD)lstrlenA(psz), &cbWritten, NULL);
#endif
}

static void OutF(LPCTSTR pszFmt, int nValue)
{
    TCHAR szBuf[FMT_CCH];
    wnsprintf(szBuf, ARRAYSIZE(szBuf), pszFmt, nValue);
    Out(szBuf);
}

static void Check(BOOL fOk, LPCTSTR pszName)
{
    LPCTSTR pszTag;

    pszTag = TEXT("[FAIL] ");
    if (fOk)
    {
        pszTag = TEXT("[PASS] ");
    }
    Out(pszTag);
    Out(pszName);
    Out(TEXT("\n"));
    if (!fOk)
    {
        g_fail++;
    }
}

static void Skip(LPCTSTR pszName, LPCTSTR pszWhy)
{
    Out(TEXT("[SKIP] "));
    Out(pszName);
    Out(TEXT(" -- "));
    Out(pszWhy);
    Out(TEXT("\n"));
}

#include "tests.inl"

static PFN_TEST volatile g_rgTests[] =
{
    T_ThreeQuarters,
    T_Thunks,
    T_StartupRect,
    T_Hardening,
    T_Position,
    T_ThemeTransitionAppsOnly,
    T_ThemeTransitionAppsAndSystem
};

void __cdecl TestEntry(void)
{
    BOOL fChild;
    UINT i;

    g_out  = GetStdHandle(STD_OUTPUT_HANDLE);
    fChild = HasArg(TEXT("--child"));
    if (fChild)
    {
        RunPositionChild();
    }
    for (i = 0u; i < ARRAYSIZE(g_rgTests); ++i)
    {
        g_rgTests[i]();
    }
    OutF(TEXT("\n%d failed\n"), g_fail);
    ExitProcess((UINT)g_fail);
}
