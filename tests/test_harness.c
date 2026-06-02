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
/* Record window: ONE straight capture spanning the whole transition (flip -> crossfade -> settle), so
   T6 runs a single recording instead of two. Decoupled from WAIT_MS (the 2s hang timeout) so timeouts
   stay generous, and from THEME_SETTLE_MS (the brief post-restore settle) so restoring the original
   theme does not pad the run with a second full-length record. */
#define THEME_RECORD_MS         3000u
/* Forward leg: how long the worker holds the target theme before flipping back, so the forward
   transition fully settles on the target plateau before the restore begins. RECORD_MS covers both
   legs (forward + restore) so the single video spans the whole round-trip. */
#define THEME_LEG_MS            1400u
/* Post-restore settle: after broadcasting the theme restore we only need the desktop to begin
   repainting before the app is torn down -- not a whole transition -- so this is short, not RECORD_MS. */
#define THEME_SETTLE_MS         300u
/* WindowsProject launch size in physical px, handed to it via STARTF_USESIZE (the app honors
   STARTUPINFO size in CalculateWindowStartupPosition, falling back to 3/4 work area otherwise). Small
   enough to keep the single straight recording's frame region and decode light; large enough that the
   main client stays visible around the centered modal About dialog so every sampled surface is
   unoccluded (client sampled at (24,96), dialog caption needs >=190px width). */
#define THEME_APP_WIDTH         800u
#define THEME_APP_HEIGHT        600u
/* Max sampled surfaces the reduction compute shader handles per frame (must equal THEME_MAX_SURF in
   theme_reduce.hlsl). Half-extent of the square pixel patch averaged per surface sample point: a patch
   (vs a single texel) is the discrete box-convolution that makes each surface's per-frame color robust
   to AA/subpixel noise -- and the GPU sums it exactly in integers, so the result is deterministic. */
#define THEME_MAX_SURF          8u
#define THEME_SURF_HALF         6

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
    ID3D11Texture2D** ppAnalysisFrames;   /* independent blit set the compute thread reduces, so the
                                             reduction never waits on the encoder for a shared texture */
    HANDLE hEncodeReady;
    HANDLE hEncodeStarted;
    HANDLE hReduceReady;                   /* signalled as each analysis frame is blitted ready         */
    volatile LONG cReadyFrames;
    volatile LONG cEncodedFrames;
    volatile LONG cReducedFrames;          /* frames the compute thread has reduced so far              */
    volatile LONG fCaptureComplete;
    UINT     uPhaseFrames;                 /* caption-band phase window (frames), scaled to refresh rate */
    UINT     cSurf;                        /* surfaces populated in rgSurfPt (<= THEME_MAX_SURF)         */
    POINT    rgSurfPt[THEME_MAX_SURF];     /* per-surface sample center, SCREEN space                   */
    BOOL     rgSurfActive[THEME_MAX_SURF]; /* surface present this run (absent dialog/children -> FALSE)*/
    UINT     rgSurfBand[THEME_MAX_SURF];   /* per-surface worst caption-band deviation, both legs (%)   */
    BOOL     rgSurfStarted[THEME_MAX_SURF];/* per-surface: began the forward leg                        */
    BOOL     rgSurfReturned[THEME_MAX_SURF];/* per-surface: completed the restore leg                   */
    COLORREF* pSurfColor;                  /* [cFrames*THEME_MAX_SURF] GPU-reduced mean colors (readback)*/
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
    ID3D11ComputeShader*       pReduceCS;        /* theme_reduce.hlsl, embedded bytecode             */
    ID3D11Buffer*              pReduceParams;    /* dynamic CB: surf count, frame slot, surface rects */
    ID3D11Buffer*              pReduceResults;   /* structured RWBuffer<uint4>, one slot per frame*surf*/
    ID3D11UnorderedAccessView* pReduceResultsUAV;
    ID3D11Buffer*              pReduceStaging;   /* CPU-read copy of pReduceResults                   */
    BOOL                       fComputeReady;    /* compute path initialized (FL11+, buffers created) */
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

/* Mirrors theme_reduce.hlsl's cbuffer ThemeReduceParams exactly (16B header + uint4[8] = 144B). */
typedef struct
{
    UINT gSurfCount;
    UINT gFrameSlot;
    UINT gPad0;
    UINT gPad1;
    UINT gRects[THEME_MAX_SURF][4];   /* per surface: x, y, w, h (capture-space px) */
} THEME_REDUCE_PARAMS;

typedef struct
{
    THEME_DXGI*    pDxgi;
    THEME_CAPTURE* pCap;
    HANDLE         hDone;
    DWORD          dwThreadId;
    BOOL           fOk;
} THEME_REDUCE_RUN;

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

/* Build-generated: const BYTE g_themeReduceCS[] -- compiled bytecode of theme_reduce.hlsl. */
#include "theme_reduce_cs.h"

#include "tests.inl"

static PFN_TEST volatile g_rgTests[] =
{
    T_ThreeQuarters,
    T_Thunks,
    T_StartupRect,
    T_Hardening,
    T_Position,
    T_ThemeTransition
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
