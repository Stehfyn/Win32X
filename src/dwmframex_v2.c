/*
 * dwmframex.c -- in-process reproduction of uDWM's caption compositor (Phase 1: the GPU pipeline).
 *
 * Builds the SAME stack uDWM uses -- D3D11 -> Direct2D -> DirectComposition -> (later) DWrite -- but on
 * a DCompositionTarget the app owns for its own hwnd (DCompositionCreateTargetForHwnd). DWM composites
 * this visual tree against the desktop identically to its own caption, on the GPU. Phase 1 renders the
 * caption bar fill at the theme color to prove the pipeline in-process under the CRT-free /Wall /WX
 * build; later phases add the title (DWrite), the four caption-button sprites from a glyph atlas, and
 * the hover crossfades, mirroring CButton::DrawStateW / CTopLevelWindow::CreateNCButtons.
 *
 * COM here is called through OUR OWN vtable macros (CCALL/CCALL0) -- the D2D/DComp C headers expose the
 * lpVtbl layout (CINTERFACE) but not the MIDL `Interface_Method` wrappers, so the call site is just
 * vtable pointer arithmetic: p->lpVtbl->Method(p, args...).
 */
#pragma runtime_checks("", off)
#pragma check_stack(off)
#pragma strict_gs_check(off)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define CINTERFACE          /* expose the C lpVtbl struct layout for every COM interface */

#include <windows.h>
#include <dxgi.h>
#include <dxgi1_2.h>   /* V2: IDXGIFactory2::CreateSwapChainForComposition, DXGI_SWAP_CHAIN_DESC1 */
#include <dxgi1_3.h>   /* V2: IDXGISwapChain2: GetFrameLatencyWaitableObject/SetSourceSize/SetMaximumFrameLatency */
#include <d3d11.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <timeapi.h>   /* V2: timeBeginPeriod/timeEndPeriod (WIN32_LEAN_AND_MEAN drops mmsystem) */
/* NOT <dwrite.h>: it is C++-only (enum-class underlying-type ':' syntax, static_cast). We forward-declare
   IDWriteFactory/IDWriteTextFormat and pass its enums as plain UINT constants (defined below).
   NOT <dcomp.h>: its C ("CINTERFACE") path is broken -- IDCompositionVisual has overloaded methods
   (SetOffsetX(float) vs SetOffsetX(IDCompositionAnimation*)) and C++ reference params, which cannot be
   expressed in a C struct. So we hand-declare exactly the DirectComposition interfaces we use, as C
   vtable structs, with slot indices read straight out of dcomp.h. */

#include "Win32X/dwmframex2.h"

/* CRT-free: _fltused is defined once for the whole image by dwmframex.c (this V2 TU lives in the same
   static lib, so it must NOT redefine it -- that is LNK4006). Just reference the shared definition. */
extern int _fltused;

/* ---- our own COBJMACROS: a COM call is a vtable deref. ------------------------------------------ */
#define CCALL(p, m, ...)  ((p)->lpVtbl->m((p), __VA_ARGS__))
#define CCALL0(p, m)      ((p)->lpVtbl->m((p)))

/* ---- hand-declared DirectComposition interfaces (slot order per dcomp.h) ------------------------ */
typedef struct IDCompositionDesktopDevice IDCompositionDesktopDevice;
typedef struct IDCompositionTarget  IDCompositionTarget;
typedef struct IDCompositionVisual  IDCompositionVisual;
typedef struct IDCompositionSurface IDCompositionSurface;
typedef struct IDWriteFactory       IDWriteFactory;
typedef struct IDWriteTextFormat    IDWriteTextFormat;

/* DWrite enum values we pass (dwrite.h is unavailable in C). */
#define DWF_FACTORY_TYPE_SHARED         0u
#define DWF_FONT_STYLE_NORMAL           0u
#define DWF_FONT_STYLE_ITALIC           2u
#define DWF_FONT_STRETCH_NORMAL         5u
#define DWF_TEXT_ALIGNMENT_LEADING      0u
#define DWF_TEXT_ALIGNMENT_CENTER       2u
#define DWF_PARAGRAPH_ALIGNMENT_CENTER  2u
#define DWF_WORD_WRAPPING_NO_WRAP       1u
#define DWF_MEASURING_MODE_NATURAL      0u

/* IDCompositionDesktopDevice : IDCompositionDevice2 -- the device that backs D2D BeginDraw (created via
   DCompositionCreateDevice2 with the D2D device). Commit/CreateVisual/CreateSurface are inherited from
   IDCompositionDevice2 (slots 3/6/8); CreateTargetForHwnd is IDCompositionDesktopDevice's own (slot 24). */
typedef struct IDCompositionDesktopDeviceVtbl
{
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDCompositionDesktopDevice*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(IDCompositionDesktopDevice*);
    ULONG   (STDMETHODCALLTYPE* Release)(IDCompositionDesktopDevice*);
    HRESULT (STDMETHODCALLTYPE* Commit)(IDCompositionDesktopDevice*);                          /* slot 3 */
    void*   rsvd_a[2];   /* 4,5: WaitForCommitCompletion, GetFrameStatistics */
    HRESULT (STDMETHODCALLTYPE* CreateVisual)(IDCompositionDesktopDevice*, IDCompositionVisual**); /* slot 6 */
    void*   rsvd_b;      /* 7: CreateSurfaceFactory */
    HRESULT (STDMETHODCALLTYPE* CreateSurface)(IDCompositionDesktopDevice*, UINT, UINT, DXGI_FORMAT, UINT, IDCompositionSurface**); /* 8 */
    void*   rsvd_c[15];  /* 9..23: CreateVirtualSurface .. CreateAnimation */
    HRESULT (STDMETHODCALLTYPE* CreateTargetForHwnd)(IDCompositionDesktopDevice*, HWND, BOOL, IDCompositionTarget**); /* slot 24 */
} IDCompositionDesktopDeviceVtbl;
struct IDCompositionDesktopDevice { const IDCompositionDesktopDeviceVtbl* lpVtbl; };

typedef struct IDCompositionTargetVtbl
{
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDCompositionTarget*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(IDCompositionTarget*);
    ULONG   (STDMETHODCALLTYPE* Release)(IDCompositionTarget*);
    HRESULT (STDMETHODCALLTYPE* SetRoot)(IDCompositionTarget*, IDCompositionVisual*);          /* slot 3 */
} IDCompositionTargetVtbl;
struct IDCompositionTarget { const IDCompositionTargetVtbl* lpVtbl; };

typedef struct IDCompositionVisualVtbl
{
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDCompositionVisual*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(IDCompositionVisual*);
    ULONG   (STDMETHODCALLTYPE* Release)(IDCompositionVisual*);
    void*   rsvd[12];   /* slots 3..14: SetOffsetX/Y(anim,float), SetTransform(anim,matrix),
                           SetTransformParent, SetEffect, SetBitmapInterpolationMode, SetBorderMode,
                           SetClip(anim,rect) -- unused, present only to position SetContent at slot 15 */
    HRESULT (STDMETHODCALLTYPE* SetContent)(IDCompositionVisual*, IUnknown*);                  /* slot 15 */
} IDCompositionVisualVtbl;
struct IDCompositionVisual { const IDCompositionVisualVtbl* lpVtbl; };

typedef struct IDCompositionSurfaceVtbl
{
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDCompositionSurface*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(IDCompositionSurface*);
    ULONG   (STDMETHODCALLTYPE* Release)(IDCompositionSurface*);
    HRESULT (STDMETHODCALLTYPE* BeginDraw)(IDCompositionSurface*, const RECT*, REFIID, void**, POINT*); /* slot 3 */
    HRESULT (STDMETHODCALLTYPE* EndDraw)(IDCompositionSurface*);                               /* slot 4 */
} IDCompositionSurfaceVtbl;
struct IDCompositionSurface { const IDCompositionSurfaceVtbl* lpVtbl; };

/* ---- hand-declared Direct2D interfaces (d2d1.h forward-declares them but never binds lpVtbl in C;
        D2D is a C++-only header). Slot indices read from d2d1.h/d2d1_1.h:
        ID2D1Factory1::CreateDevice = 17, ID2D1Device::CreateDeviceContext = 4,
        ID2D1RenderTarget::SetTransform = 30, ::Clear = 47 (inherited by ID2D1DeviceContext). -------- */
typedef struct ID2D1Factory1Vtbl
{
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(ID2D1Factory1*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(ID2D1Factory1*);
    ULONG   (STDMETHODCALLTYPE* Release)(ID2D1Factory1*);
    void*   rsvd[14];   /* ID2D1Factory slots 3..16 */
    HRESULT (STDMETHODCALLTYPE* CreateDevice)(ID2D1Factory1*, IDXGIDevice*, ID2D1Device**);  /* slot 17 */
} ID2D1Factory1Vtbl;
struct ID2D1Factory1 { const ID2D1Factory1Vtbl* lpVtbl; };

typedef struct ID2D1DeviceVtbl
{
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(ID2D1Device*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(ID2D1Device*);
    ULONG   (STDMETHODCALLTYPE* Release)(ID2D1Device*);
    void*   rsvd_GetFactory;   /* slot 3 (ID2D1Resource::GetFactory) */
    HRESULT (STDMETHODCALLTYPE* CreateDeviceContext)(ID2D1Device*, D2D1_DEVICE_CONTEXT_OPTIONS, ID2D1DeviceContext**); /* slot 4 */
} ID2D1DeviceVtbl;
struct ID2D1Device { const ID2D1DeviceVtbl* lpVtbl; };

typedef struct ID2D1DeviceContextVtbl
{
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(ID2D1DeviceContext*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(ID2D1DeviceContext*);
    ULONG   (STDMETHODCALLTYPE* Release)(ID2D1DeviceContext*);
    void*   rsvd0[5];   /* slots 3..7: GetFactory, CreateBitmap(x3), CreateBitmapBrush */
    HRESULT (STDMETHODCALLTYPE* CreateSolidColorBrush)(ID2D1DeviceContext*, const D2D1_COLOR_F*, const D2D1_BRUSH_PROPERTIES*, ID2D1SolidColorBrush**); /* 8 */
    void*   rsvd1[8];   /* slots 9..16: gradient/layer/mesh/DrawLine/DrawRectangle */
    void    (STDMETHODCALLTYPE* FillRectangle)(ID2D1DeviceContext*, const D2D1_RECT_F*, ID2D1Brush*); /* slot 17 */
    void*   rsvd2[8];   /* slots 18..25: Draw/FillRoundedRect, Draw/FillEllipse, Draw/FillGeometry, FillMesh, FillOpacityMask */
    void    (STDMETHODCALLTYPE* DrawBitmap)(ID2D1DeviceContext*, ID2D1Bitmap*, const D2D1_RECT_F*, FLOAT, D2D1_BITMAP_INTERPOLATION_MODE, const D2D1_RECT_F*); /* 26 */
    void    (STDMETHODCALLTYPE* DrawText)(ID2D1DeviceContext*, const WCHAR*, UINT32, IDWriteTextFormat*, const D2D1_RECT_F*, ID2D1Brush*, D2D1_DRAW_TEXT_OPTIONS, UINT /*DWRITE_MEASURING_MODE*/); /* 27 */
    void*   rsvd3[2];   /* slots 28..29: DrawTextLayout, DrawGlyphRun */
    void    (STDMETHODCALLTYPE* SetTransform)(ID2D1DeviceContext*, const D2D1_MATRIX_3X2_F*); /* slot 30 */
    void*   rsvd4a[14]; /* slots 31..44: GetTransform..RestoreDrawingState */
    void    (STDMETHODCALLTYPE* PushAxisAlignedClip)(ID2D1DeviceContext*, const D2D1_RECT_F*, D2D1_ANTIALIAS_MODE); /* 45 */
    void    (STDMETHODCALLTYPE* PopAxisAlignedClip)(ID2D1DeviceContext*);                     /* slot 46 */
    void    (STDMETHODCALLTYPE* Clear)(ID2D1DeviceContext*, const D2D1_COLOR_F*);             /* slot 47 */
    void    (STDMETHODCALLTYPE* BeginDraw)(ID2D1DeviceContext*);                              /* slot 48 */
    HRESULT (STDMETHODCALLTYPE* EndDraw)(ID2D1DeviceContext*, void* /*D2D1_TAG**/, void*);    /* slot 49 */
    void*   rsvd5[12];  /* 50..61: GetPixelFormat,SetDpi,GetDpi,GetSize,GetPixelSize,GetMaximumBitmapSize,
                           IsSupported,CreateBitmap,CreateBitmapFromWicBitmap,CreateColorContext(x3) */
    HRESULT (STDMETHODCALLTYPE* CreateBitmapFromDxgiSurface)(ID2D1DeviceContext*, IDXGISurface*,
            const D2D1_BITMAP_PROPERTIES1*, ID2D1Bitmap1**);                                  /* slot 62 */
    void*   rsvd6[11];  /* 63..73: CreateEffect..GetDevice */
    void    (STDMETHODCALLTYPE* SetTarget)(ID2D1DeviceContext*, ID2D1Image*);                 /* slot 74 */
} ID2D1DeviceContextVtbl;
struct ID2D1DeviceContext { const ID2D1DeviceContextVtbl* lpVtbl; };

/* ---- hand-declared DWrite interfaces (dwrite.h is C++-only too). IDWriteFactory::CreateTextFormat=15. */
typedef struct IDWriteFactoryVtbl
{
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDWriteFactory*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(IDWriteFactory*);
    ULONG   (STDMETHODCALLTYPE* Release)(IDWriteFactory*);
    void*   rsvd[12];   /* slots 3..14 */
    HRESULT (STDMETHODCALLTYPE* CreateTextFormat)(IDWriteFactory*, const WCHAR*, void* /*fontCollection*/,
            UINT /*weight*/, UINT /*style*/, UINT /*stretch*/, FLOAT, const WCHAR*, IDWriteTextFormat**); /* 15 */
} IDWriteFactoryVtbl;
struct IDWriteFactory { const IDWriteFactoryVtbl* lpVtbl; };

typedef struct IDWriteTextFormatVtbl
{
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDWriteTextFormat*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(IDWriteTextFormat*);
    ULONG   (STDMETHODCALLTYPE* Release)(IDWriteTextFormat*);
    HRESULT (STDMETHODCALLTYPE* SetTextAlignment)(IDWriteTextFormat*, UINT);       /* slot 3 (DWRITE_TEXT_ALIGNMENT)      */
    HRESULT (STDMETHODCALLTYPE* SetParagraphAlignment)(IDWriteTextFormat*, UINT);  /* slot 4 (DWRITE_PARAGRAPH_ALIGNMENT) */
    HRESULT (STDMETHODCALLTYPE* SetWordWrapping)(IDWriteTextFormat*, UINT);        /* slot 5 (DWRITE_WORD_WRAPPING)       */
} IDWriteTextFormatVtbl;
struct IDWriteTextFormat { const IDWriteTextFormatVtbl* lpVtbl; };

/* Self-contained IIDs (no dependency on the lib-provided GUID symbols). */
static const GUID DWF_IID_IDCompositionDesktopDevice =                                     /* dcomp.h */
    { 0x5F4633FEu, 0x1E08u, 0x4CB8u, { 0x8Cu, 0x75u, 0xCEu, 0x24u, 0x33u, 0x3Fu, 0x56u, 0x02u } };
static const GUID DWF_IID_IDXGIDevice =                                                    /* dxgi.h  */
    { 0x54ec77fau, 0x1377u, 0x44e6u, { 0x8cu, 0x32u, 0x88u, 0xfdu, 0x5fu, 0x44u, 0xc8u, 0x4cu } };
static const GUID DWF_IID_ID2D1Factory1 =                                                  /* d2d1_1  */
    { 0xbb12d362u, 0xdaeeu, 0x4b9au, { 0xaau, 0x1du, 0x14u, 0xbau, 0x40u, 0x1cu, 0xfau, 0x1fu } };
static const GUID DWF_IID_ID2D1DeviceContext =                                             /* d2d1_1  */
    { 0xe8f7fe7au, 0x191cu, 0x466du, { 0xadu, 0x95u, 0x97u, 0x56u, 0x78u, 0xbdu, 0xa9u, 0x98u } };
static const GUID DWF_IID_IDWriteFactory =                                                 /* dwrite  */
    { 0xb859ee5au, 0xd838u, 0x4b5bu, { 0xa2u, 0xe8u, 0x1au, 0xdcu, 0x7du, 0x93u, 0xdbu, 0x48u } };

#define DWF_ALPHA_MODE_PREMULTIPLIED  1u   /* DXGI_ALPHA_MODE_PREMULTIPLIED */

/* Caption buttons, left-to-right adjacency: the light/dark toggle sits immediately left of Minimize. */
enum DWF_BTN { DWB_NONE = 0, DWB_LIGHTDARK, DWB_MIN, DWB_MAX, DWB_CLOSE };

EXTERN_C HRESULT WINAPI DCompositionCreateDevice2(IUnknown* /*renderingDevice (the D2D device)*/, REFIID, void**);
EXTERN_C HRESULT WINAPI DWriteCreateFactory(UINT /*DWRITE_FACTORY_TYPE*/, REFIID, IUnknown**);

/* DWM frame attributes -- the shadow, rounded corners, 1px border and extended frame bounds a native
   overlapped window gets are drawn by DWM, NOT by our surface. Removing the NC (WM_NCCALCSIZE->0) strips
   the frame DWM would render, so we re-enable it the documented way: DwmExtendFrameIntoClientArea (a 1px
   sheet-of-glass restores shadow + border + DWMWA_EXTENDED_FRAME_BOUNDS) and DWMWA_WINDOW_CORNER_PREFERENCE
   = ROUND. dwmapi is dynamically loaded (no link dep), hand-declared in this file's style. */
typedef struct DWF_MARGINS { int cxLeft; int cxRight; int cyTop; int cyBottom; } DWF_MARGINS; /* == MARGINS */
typedef HRESULT (WINAPI* PFN_DWF_EXTEND)(HWND, const DWF_MARGINS*);
typedef HRESULT (WINAPI* PFN_DWF_SETATTR)(HWND, DWORD, const void*, DWORD);
typedef HRESULT (WINAPI* PFN_DWF_GETATTR)(HWND, DWORD, void*, DWORD);
#define DWF_DWMWA_EXTENDED_FRAME_BOUNDS    9
#define DWF_DWMWA_WINDOW_CORNER_PREFERENCE 33
#define DWF_DWMWA_BORDER_COLOR             34
#define DWF_DWMWA_SYSTEMBACKDROP_TYPE      38
#define DWF_DWMWCP_ROUND                   2
#define DWF_DWMWA_COLOR_DEFAULT            0xFFFFFFFFu
/* DWM_SYSTEMBACKDROP_TYPE: composited by DWM (wuceffects luminosity-blend) when the app opts in. */
#define DWF_DWMSBT_NONE                    1
#define DWF_DWMSBT_MAINWINDOW             2  /* Mica  (static blurred wallpaper) */
#define DWF_DWMSBT_TRANSIENTWINDOW        3  /* Acrylic (live host blur) */

typedef HRESULT (WINAPI* PFN_DWF_FLUSH)(void);
static HMODULE         g_dwfDwmapi;
static PFN_DWF_EXTEND  g_dwfExtend;
static PFN_DWF_SETATTR g_dwfSetAttr;
static PFN_DWF_FLUSH   g_dwfFlush;

/* Single top-level window for the sample; one pipeline instance. */
typedef struct DWF_STATE
{
    HWND                  hwnd;
    ID3D11Device*         pD3DDevice;
    IDXGIDevice*          pDxgiDevice;
    ID2D1Factory1*        pD2DFactory;
    ID2D1Device*          pD2DDevice;
    ID2D1DeviceContext*   pD2DContext;
    IDCompositionDesktopDevice* pDComp;
    IDCompositionTarget*  pTarget;
    IDCompositionVisual*  pVisual;
    IDXGISwapChain2*      pSwapchain;   /* V2: visual content = flip-model composition swapchain */
    ID2D1Bitmap1*         pTargetBmp;   /* persistent D2D target wrapping back buffer 0 (created once) */
    HANDLE                hFrameWait;   /* GetFrameLatencyWaitableObject -- caller-owned; CloseHandle in destroy */
    IDWriteFactory*       pDWrite;
    IDWriteTextFormat*    pTextFormat;
    IDWriteTextFormat*    pIconFormat;
    ID2D1Bitmap1*         pIconBmp;     /* cached high-res caption icon (D3D11 texture -> D2D bitmap) */
    UINT                  cxBuffer;     /* back-buffer allocation (monitor px); regrown only on monitor change */
    UINT                  cyBuffer;
    UINT                  cxClient;     /* current client size (presented sub-rect via SetSourceSize, layout) */
    UINT                  cyClient;
    BOOL                  fInSizeMove;  /* inside WM_ENTERSIZEMOVE..EXITSIZEMOVE (main loop suspended) */
    DWORD                 dwLastTick;   /* GetTickCount() at the previous frame (time-based opacity step) */
    BOOL                  fActive;      /* pipeline alive (DwmFrameActive2), NOT window-activation */
    int                   idHot;        /* DWF_BTN under the cursor, or DWB_NONE */
    int                   idPressed;    /* DWF_BTN currently pressed (captured), or DWB_NONE */
    BOOL                  fTracking;    /* TME_NONCLIENT leave tracking armed */
    BOOL                  fCapturing;   /* a button press holds the mouse capture */
    BOOL                  fDark;        /* current (target) dark state */
    BOOL                  fWndActive;   /* current (target) window-activation state */
    /* Color crossfade (uDWM's 160ms linear timeline, reused for theme AND activation transitions). */
    BOOL                  fAnim;        /* a crossfade is in progress */
    DWORD                 dwAnimStart;  /* GetTickCount() at crossfade start */
    BOOL                  fDarkFrom;    /* dark state at crossfade start (the "from" color set) */
    BOOL                  fActiveFrom;  /* window-active state at crossfade start */
    float                 flAnimT;      /* current progress 0..1 (advanced by WM_TIMER) */
    /* Per-button hover/press highlight opacity (uDWM's CButton 160ms glyph-state crossfade). Indexed by
       DWB_* (DWB_NONE..DWB_CLOSE = 0..4); [0] unused. The same WM_TIMER advances these toward target. */
    float                 flBtnOpacity[5];
    int                   iBackdrop;    /* DWMSBT_*: 0/1=solid caption, 2=Mica, 3=Acrylic */
} DWF_STATE;

static DWF_STATE g_dwf;
static BOOL      g_dwfIconTried;       /* attempt the (high-res) caption icon build at most once */
static BOOL      g_dwfFramePublished;  /* SWP_FRAMECHANGED reported once, on the first WM_ACTIVATE */

/* ---- helpers ------------------------------------------------------------------------------------ */

static FORCEINLINE void DwfRelease(IUnknown** ppUnk)
{
    if (*ppUnk)
    {
        CCALL0(*ppUnk, Release);
        *ppUnk = NULL;
    }
}

static FORCEINLINE D2D1_COLOR_F DwfColor(COLORREF cr)
{
    D2D1_COLOR_F c;

    c.r = (FLOAT)GetRValue(cr) / 255.0f;
    c.g = (FLOAT)GetGValue(cr) / 255.0f;
    c.b = (FLOAT)GetBValue(cr) / 255.0f;
    c.a = 1.0f;
    return c;
}

/* uDWM caption crossfade: a 160ms LINEAR timeline (spec_input_anim.md: duration const 0x1801072b0 =
   0x3FC47AE140000000 = 0.16s; CTimeline<float> default/linear interpolation). The same clock drives the
   dark<->light theme change AND the active<->inactive transition. */
#define DWF_ANIM_TIMER_ID  ((UINT_PTR)0x0DF00001u)
#define DWF_ANIM_INTERVAL  1u    /* V2: 1ms tick (timeBeginPeriod defeats coalescing); Present(1) throttles to vblank */
#define DWF_ANIM_DURATION  160u  /* ms */

static FORCEINLINE D2D1_COLOR_F DwfLerp(D2D1_COLOR_F a, D2D1_COLOR_F b, float t)
{
    D2D1_COLOR_F c;

    c.r = a.r + (b.r - a.r) * t;
    c.g = a.g + (b.g - a.g) * t;
    c.b = a.b + (b.b - a.b) * t;
    c.a = a.a + (b.a - a.a) * t;
    return c;
}

/* Caption title text color for a (dark, active) state. Inactive dims to 60% alpha, like the shell. */
static FORCEINLINE D2D1_COLOR_F DwfTextColor(BOOL fDark, BOOL fActive)
{
    D2D1_COLOR_F c;

    c = DwfColor(fDark ? RGB(255, 255, 255) : RGB(0, 0, 0));
    if (!fActive)
    {
        c.a = 0.60f;
    }
    return c;
}

/* Caption-button glyph color for a (dark, active) state (matches DwfDrawButton's normal-state tint). */
static FORCEINLINE D2D1_COLOR_F DwfGlyphColor(BOOL fDark, BOOL fActive)
{
    return DwfColor(fActive ? (fDark ? RGB(255, 255, 255) : RGB(0, 0, 0))
                            : (fDark ? RGB(0xAA, 0xAA, 0xAA) : RGB(0x64, 0x64, 0x64)));
}

/* Target highlight opacity for a button: 1 when pressed, or hot with no other press; else 0. The
   per-button opacity ramps toward this over the 160ms timeline (uDWM's CButton glyph-state crossfade). */
static FORCEINLINE float DwfBtnTarget(int id)
{
    if (g_dwf.idPressed == id)
    {
        return 1.0f;
    }
    if ((g_dwf.idHot == id) && (g_dwf.idPressed == DWB_NONE))
    {
        return 1.0f;
    }
    return 0.0f;
}

/* Caption band height in client pixels -- the system's own number (GetTitleBarInfoEx close.bottom),
   with a metric fallback. Phase 1 keeps the surface this tall and the full client width. */
static DECLSPEC_NOINLINE UINT DwfCaptionHeight(HWND hwnd)
{
    TITLEBARINFOEX tbi;
    int            cy;

    SecureZeroMemory(&tbi, sizeof(tbi));
    tbi.cbSize = (DWORD)sizeof(tbi);
    (void)SendMessageW(hwnd, WM_GETTITLEBARINFOEX, 0, (LPARAM)&tbi);
    (void)MapWindowPoints(NULL, hwnd, (POINT*)&tbi.rgrect[5], 2);
    cy = tbi.rgrect[5].bottom;
    if (cy <= 0)
    {
        cy = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFRAME) +
             GetSystemMetrics(SM_CXPADDEDBORDER);
    }
    if (cy <= 0)
    {
        cy = 32;
    }
    return (UINT)cy;
}

static DECLSPEC_NOINLINE void DwfClientSize(HWND hwnd, UINT* pcx, UINT* pcy)
{
    RECT rc;

    GetClientRect(hwnd, &rc);
    *pcx = (UINT)(rc.right - rc.left);
    *pcy = (UINT)(rc.bottom - rc.top);   /* whole window: DComp paints caption + client (no redirection) */
    if (*pcx == 0u) { *pcx = 1u; }
    if (*pcy == 0u) { *pcy = 1u; }
}

/* Caption bar fill color. Phase 2 approximation of uDWM's colorization pipeline (active/inactive,
   dark/light); the exact accent path is refined in a later pass. */
static FORCEINLINE D2D1_COLOR_F DwfCaptionColor(BOOL fDark, BOOL fActive)
{
    COLORREF cr;

    if (fDark)
    {
        cr = fActive ? RGB(0x20, 0x20, 0x20) : RGB(0x2B, 0x2B, 0x2B);
    }
    else
    {
        cr = fActive ? RGB(0xF3, 0xF3, 0xF3) : RGB(0xFB, 0xFB, 0xFB);
    }
    return DwfColor(cr);
}

/* Client-area background below the caption (the window is DComp-only with WS_EX_NOREDIRECTIONBITMAP, so
   DComp paints the whole window -- caption band + client). */
static FORCEINLINE D2D1_COLOR_F DwfClientColor(BOOL fDark)
{
    return DwfColor(fDark ? RGB(0x20, 0x20, 0x20) : RGB(0xFF, 0xFF, 0xFF));
}

/* SPI_GETNONCLIENTMETRICS is ~500 bytes -> off-stack (GUI thread only), same remedy as the GDI path. */
static NONCLIENTMETRICSW g_dwfNcm;

/* Build the caption IDWriteTextFormat from the system caption font (lfCaptionFont = Segoe UI), the same
   font uDWM's CDWriteText uses for the title. */
static DECLSPEC_NOINLINE void DwfCreateTextFormat(void)
{
    FLOAT size;

    if (!g_dwf.pDWrite)
    {
        return;
    }
    DwfRelease((IUnknown**)&g_dwf.pTextFormat);
    SecureZeroMemory(&g_dwfNcm, sizeof(g_dwfNcm));
    g_dwfNcm.cbSize = (DWORD)sizeof(g_dwfNcm);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, (UINT)sizeof(g_dwfNcm), &g_dwfNcm, 0))
    {
        return;
    }
    size = (FLOAT)((g_dwfNcm.lfCaptionFont.lfHeight < 0) ? -g_dwfNcm.lfCaptionFont.lfHeight
                                                         : g_dwfNcm.lfCaptionFont.lfHeight);
    if (size < 1.0f)
    {
        size = 12.0f;
    }
    (void)CCALL(g_dwf.pDWrite, CreateTextFormat, g_dwfNcm.lfCaptionFont.lfFaceName, NULL,
                (UINT)g_dwfNcm.lfCaptionFont.lfWeight,
                g_dwfNcm.lfCaptionFont.lfItalic ? DWF_FONT_STYLE_ITALIC : DWF_FONT_STYLE_NORMAL,
                DWF_FONT_STRETCH_NORMAL, size, L"", &g_dwf.pTextFormat);
    if (g_dwf.pTextFormat)
    {
        (void)CCALL(g_dwf.pTextFormat, SetTextAlignment, DWF_TEXT_ALIGNMENT_LEADING);
        (void)CCALL(g_dwf.pTextFormat, SetParagraphAlignment, DWF_PARAGRAPH_ALIGNMENT_CENTER);
        (void)CCALL(g_dwf.pTextFormat, SetWordWrapping, DWF_WORD_WRAPPING_NO_WRAP);
    }
}

/* Icon text-format: Segoe Fluent Icons (Win11) / Segoe MDL2 Assets (Win10) -- the same glyphs uDWM bakes
   into its button atlas. Sized to the caption height, centered both ways for glyph centering. */
static DECLSPEC_NOINLINE void DwfCreateIconFormat(void)
{
    FLOAT size;

    if (!g_dwf.pDWrite)
    {
        return;
    }
    DwfRelease((IUnknown**)&g_dwf.pIconFormat);
    size = (FLOAT)DwfCaptionHeight(g_dwf.hwnd) * 0.36f;
    if (size < 8.0f)
    {
        size = 8.0f;
    }
    (void)CCALL(g_dwf.pDWrite, CreateTextFormat, L"Segoe Fluent Icons", NULL, 400u,
                DWF_FONT_STYLE_NORMAL, DWF_FONT_STRETCH_NORMAL, size, L"", &g_dwf.pIconFormat);
    if (!g_dwf.pIconFormat)
    {
        (void)CCALL(g_dwf.pDWrite, CreateTextFormat, L"Segoe MDL2 Assets", NULL, 400u,
                    DWF_FONT_STYLE_NORMAL, DWF_FONT_STRETCH_NORMAL, size, L"", &g_dwf.pIconFormat);
    }
    if (g_dwf.pIconFormat)
    {
        (void)CCALL(g_dwf.pIconFormat, SetTextAlignment, DWF_TEXT_ALIGNMENT_CENTER);
        (void)CCALL(g_dwf.pIconFormat, SetParagraphAlignment, DWF_PARAGRAPH_ALIGNMENT_CENTER);
        (void)CCALL(g_dwf.pIconFormat, SetWordWrapping, DWF_WORD_WRAPPING_NO_WRAP);
    }
}

/* The four caption-button rects in client/surface coords: Close/Max/Min from GetTitleBarInfoEx (the exact
   rects DefWindowProc/uDWM compute), and the light/dark button one cell left of Minimize. Returns 0 if
   the system reported no caption buttons. */
static DECLSPEC_NOINLINE int DwfButtonRects(HWND hwnd, RECT* prcClose, RECT* prcMax, RECT* prcMin, RECT* prcLD)
{
    RECT rc;
    UINT dpi;
    int  capH;
    int  btnW;
    int  r;

    /* WM_GETTITLEBARINFOEX returns nothing once the NC is removed, so compute the cluster directly: each
       cell is the native caption-button width (~47 DIP) scaled by DPI, right-aligned, Close..light/dark
       right-to-left. Height = caption-bar height. (uDWM!UpdateNCAreaButton's metric layout.) */
    GetClientRect(hwnd, &rc);
    dpi  = GetDpiForWindow(hwnd);
    if (0u == dpi) { dpi = 96u; }
    capH = (int)DwfCaptionHeight(hwnd);
    btnW = MulDiv(47, (int)dpi, 96);

    r = rc.right;
    prcClose->right = r; prcClose->left = r - btnW; r -= btnW;
    prcMax->right   = r; prcMax->left   = r - btnW; r -= btnW;
    prcMin->right   = r; prcMin->left   = r - btnW; r -= btnW;
    prcLD->right    = r; prcLD->left    = r - btnW;
    prcClose->top = 0; prcClose->bottom = capH;
    prcMax->top   = 0; prcMax->bottom   = capH;
    prcMin->top   = 0; prcMin->bottom   = capH;
    prcLD->top    = 0; prcLD->bottom    = capH;
    return 1;
}

static FORCEINLINE void DwfDrawGlyph(ID2D1DeviceContext* pDC, ID2D1Brush* pBrush, const RECT* prc, WCHAR glyph)
{
    IDWriteTextFormat* pf;
    D2D1_RECT_F        rc;
    WCHAR              s[1];

    pf = g_dwf.pIconFormat ? g_dwf.pIconFormat : g_dwf.pTextFormat;
    if (!pf)
    {
        return;
    }
    s[0]      = glyph;
    rc.left   = (FLOAT)prc->left;
    rc.top    = (FLOAT)prc->top;
    rc.right  = (FLOAT)prc->right;
    rc.bottom = (FLOAT)prc->bottom;
    CCALL(pDC, DrawText, s, 1u, pf, &rc, pBrush,
          D2D1_DRAW_TEXT_OPTIONS_NONE, DWF_MEASURING_MODE_NATURAL);
}

/* One caption button: hover/press highlight + the glyph. The highlight is alpha-faded by flHover (the
   per-button 160ms opacity from the WM_TIMER), so hover/press cross-fade in and out instead of snapping --
   uDWM's CButton glyph-state crossfade. cfGlyph is the caller's normal-state glyph color (itself crossfaded
   during a theme/activation change); Close's glyph cross-fades to white as its red highlight fades in.
   fDark selects the neutral hover/press fill shade; the shade switches by press state, the alpha animates. */
static DECLSPEC_NOINLINE void DwfDrawButton(ID2D1DeviceContext* pDC, const RECT* prc, int id, WCHAR glyph,
                                            BOOL fDark, D2D1_COLOR_F cfGlyph, float flHover)
{
    BOOL                  fPressed;
    COLORREF              crFill;
    D2D1_COLOR_F          cf;
    D2D1_RECT_F           rf;
    ID2D1SolidColorBrush* pb;

    fPressed = (g_dwf.idPressed == id);
    if (DWB_CLOSE == id)
    {
        crFill  = fPressed ? RGB(0xC8, 0x3C, 0x2F) : RGB(0xC4, 0x2B, 0x1C);
        cfGlyph = DwfLerp(cfGlyph, DwfColor(RGB(255, 255, 255)), flHover);  /* glyph -> white as red rises */
    }
    else
    {
        crFill = fPressed ? (fDark ? RGB(0x50, 0x50, 0x50) : RGB(0xCC, 0xCC, 0xCC))
                          : (fDark ? RGB(0x3D, 0x3D, 0x3D) : RGB(0xE9, 0xE9, 0xE9));
    }

    rf.left   = (FLOAT)prc->left;
    rf.top    = (FLOAT)prc->top;
    rf.right  = (FLOAT)prc->right;
    rf.bottom = (FLOAT)prc->bottom;
    if (flHover > 0.001f)
    {
        cf   = DwfColor(crFill);
        cf.a = flHover;                 /* fade the highlight in/out over the timeline */
        pb   = NULL;
        (void)CCALL(pDC, CreateSolidColorBrush, &cf, NULL, &pb);
        if (pb)
        {
            CCALL(pDC, FillRectangle, &rf, (ID2D1Brush*)pb);
            DwfRelease((IUnknown**)&pb);
        }
    }
    cf = cfGlyph;
    pb = NULL;
    (void)CCALL(pDC, CreateSolidColorBrush, &cf, NULL, &pb);
    if (pb)
    {
        DwfDrawGlyph(pDC, (ID2D1Brush*)pb, prc, glyph);
        DwfRelease((IUnknown**)&pb);
    }
}

/* ---- V2 swapchain (re)creation ------------------------------------------------------------------ */

/* Monitor pixel size of hwnd's monitor (physical px under per-monitor-DPI awareness). */
static DECLSPEC_NOINLINE void DfwMonitorSize(HWND hwnd, UINT* pcx, UINT* pcy)
{
    HMONITOR    hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi;

    SecureZeroMemory(&mi, sizeof(mi));
    mi.cbSize = (DWORD)sizeof(mi);
    if (GetMonitorInfoW(hMon, &mi))
    {
        *pcx = (UINT)(mi.rcMonitor.right  - mi.rcMonitor.left);
        *pcy = (UINT)(mi.rcMonitor.bottom - mi.rcMonitor.top);
    }
    else
    {
        *pcx = (UINT)GetSystemMetrics(SM_CXSCREEN);
        *pcy = (UINT)GetSystemMetrics(SM_CYSCREEN);
    }
    if (*pcx < 1u)      { *pcx = 1u; }
    if (*pcx > 16384u)  { *pcx = 16384u; }
    if (*pcy < 1u)      { *pcy = 1u; }
    if (*pcy > 16384u)  { *pcy = 16384u; }
}

/* Create (or regrow, on monitor-size growth) the composition swapchain at bufW x bufH, wrap back
   buffer 0 as the persistent D2D target, and bind the swapchain to the visual. Does NOT Commit (the
   caller commits once after SetRoot). SYSTEM-VERIFIED (tests/dxgi_probe.c): STRETCH/FLIP_SEQUENTIAL/
   PREMULTIPLIED/WAITABLE all S_OK; SCALING_NONE returns INVALID_CALL so STRETCH is forced (exact 1:1
   because the untransformed visual target == SetSourceSize == client size). Returns FALSE on failure. */
static DECLSPEC_NOINLINE BOOL DfwCreateSwapchain(HWND hwnd, UINT bufW, UINT bufH)
{
    IDXGIAdapter*           pAdapter = NULL;
    IDXGIFactory2*          pFactory = NULL;
    IDXGISwapChain1*        pSC1     = NULL;
    IDXGISurface*           pBB      = NULL;
    DXGI_SWAP_CHAIN_DESC1   scd;
    D2D1_BITMAP_PROPERTIES1 bp;

    UNREFERENCED_PARAMETER(hwnd);

    DwfRelease((IUnknown**)&g_dwf.pTargetBmp);
    if (g_dwf.pD2DContext) { CCALL(g_dwf.pD2DContext, SetTarget, NULL); }
    DwfRelease((IUnknown**)&g_dwf.pSwapchain);
    g_dwf.hFrameWait = NULL;   /* handle is owned by the released swapchain object */

    SecureZeroMemory(&scd, sizeof(scd));
    scd.Width              = bufW;
    scd.Height             = bufH;
    scd.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.Stereo             = FALSE;
    scd.SampleDesc.Count   = 1u;
    scd.SampleDesc.Quality = 0u;
    scd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount        = 3u + 1u + 1u;                     /* ImmersiveWindow TRIPLE_BUFFERED */
    scd.Scaling            = DXGI_SCALING_STRETCH;             /* required by ForComposition (verified) */
    scd.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;    /* ImmersiveWindow spec: discard, not sequential */
    scd.AlphaMode          = DXGI_ALPHA_MODE_PREMULTIPLIED;    /* Mica transparency + flip model */
    /* EXACT ImmersiveWindow flags: ALLOW_TEARING enables the immediate (sync-interval-0) present that
       pushes the just-drawn frame without blocking; the synced DO_NOT_SEQUENCE present then holds it at
       the vblank. Without ALLOW_TEARING the immediate present returns DXGI_ERROR_INVALID_CALL. */
    scd.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
                           | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
                           | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    (void)CCALL(g_dwf.pDxgiDevice, GetAdapter, &pAdapter);
    if (pAdapter) { (void)CCALL(pAdapter, GetParent, &IID_IDXGIFactory2, (void**)&pFactory); }
    if (pFactory) { (void)CCALL(pFactory, CreateSwapChainForComposition, (IUnknown*)g_dwf.pD3DDevice, &scd, NULL, &pSC1); }
    if (pSC1)     { (void)CCALL((IUnknown*)pSC1, QueryInterface, &IID_IDXGISwapChain2, (void**)&g_dwf.pSwapchain); }
    DwfRelease((IUnknown**)&pSC1);
    DwfRelease((IUnknown**)&pFactory);
    DwfRelease((IUnknown**)&pAdapter);
    if (!g_dwf.pSwapchain) { return FALSE; }

    g_dwf.hFrameWait = (HANDLE)CCALL0(g_dwf.pSwapchain, GetFrameLatencyWaitableObject);
    (void)CCALL(g_dwf.pSwapchain, SetMaximumFrameLatency, 1u);

    (void)CCALL(g_dwf.pSwapchain, GetBuffer, 0u, &IID_IDXGISurface, (void**)&pBB);
    if (pBB)
    {
        bp.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
        bp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
        bp.dpiX                  = 96.0f;
        bp.dpiY                  = 96.0f;
        bp.bitmapOptions         = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        bp.colorContext          = NULL;
        (void)CCALL(g_dwf.pD2DContext, CreateBitmapFromDxgiSurface, pBB, &bp, &g_dwf.pTargetBmp);
        DwfRelease((IUnknown**)&pBB);
    }
    if (!g_dwf.pTargetBmp) { DwfRelease((IUnknown**)&g_dwf.pSwapchain); g_dwf.hFrameWait = NULL; return FALSE; }

    CCALL(g_dwf.pD2DContext, SetTarget, (ID2D1Image*)g_dwf.pTargetBmp);   /* bound once, never rebound */
    (void)CCALL(g_dwf.pVisual, SetContent, (IUnknown*)g_dwf.pSwapchain);
    g_dwf.cxBuffer = bufW;
    g_dwf.cyBuffer = bufH;
    return TRUE;
}

/* Re-enable the DWM-drawn frame (shadow, rounded corners, border, extended bounds) that NC removal stripped.
   Idempotent; safe to call repeatedly. dwmapi is loaded on first use. */
static DECLSPEC_NOINLINE void DwfApplyDwmFrame(HWND hwnd)
{
    union { FARPROC fp; PFN_DWF_EXTEND ex; PFN_DWF_SETATTR sa; PFN_DWF_FLUSH fl; } u;
    DWF_MARGINS m;
    UINT        corner;

    if (!g_dwfDwmapi)
    {
        g_dwfDwmapi = LoadLibraryW(L"dwmapi.dll");
        if (g_dwfDwmapi)
        {
            u.fp = GetProcAddress(g_dwfDwmapi, "DwmExtendFrameIntoClientArea"); g_dwfExtend  = u.ex;
            u.fp = GetProcAddress(g_dwfDwmapi, "DwmSetWindowAttribute");        g_dwfSetAttr = u.sa;
            u.fp = GetProcAddress(g_dwfDwmapi, "DwmFlush");                      g_dwfFlush   = u.fl;
        }
    }
    if (g_dwfExtend)
    {
        /* 1px top sheet-of-glass: smallest extension that makes DWM render the drop shadow, the window
           border, and report DWMWA_EXTENDED_FRAME_BOUNDS -- our opaque surface paints over the 1px. */
        m.cxLeft = 0; m.cxRight = 0; m.cyTop = 1; m.cyBottom = 0;
        (void)g_dwfExtend(hwnd, &m);
    }
    if (g_dwfSetAttr)
    {
        corner = DWF_DWMWCP_ROUND;
        (void)g_dwfSetAttr(hwnd, DWF_DWMWA_WINDOW_CORNER_PREFERENCE, &corner, (DWORD)sizeof(corner));
    }
}

/* ---- public API --------------------------------------------------------------------------------- */

BOOL WINAPI DwmFrameInit2(HWND hwnd)
{
    UINT flags;
    UINT cx;
    UINT cy;

    if (g_dwf.fActive && (g_dwf.hwnd == hwnd))
    {
        return TRUE;
    }
    DwmFrameDestroy2(hwnd);
    SecureZeroMemory(&g_dwf, sizeof(g_dwf));
    g_dwf.hwnd          = hwnd;
    g_dwfIconTried      = FALSE;
    g_dwfFramePublished = FALSE;

    /* Each COM creation call NULLs its out-pointer on failure, so we gate the next step on the pointer
       rather than on an HRESULT integer (which the Spectre analyzer treats as a range-checked index
       feeding the following call). Full failure handling, no /wd suppressions. */
    flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;  /* required for D2D interop */
    (void)D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, NULL, 0,
                            D3D11_SDK_VERSION, &g_dwf.pD3DDevice, NULL, NULL);
    if (!g_dwf.pD3DDevice)
    {
        /* No hardware device (RDP / headless): fall back to the WARP software rasterizer. */
        (void)D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, NULL, 0,
                                D3D11_SDK_VERSION, &g_dwf.pD3DDevice, NULL, NULL);
    }
    if (g_dwf.pD3DDevice)
    {
        (void)CCALL((IUnknown*)g_dwf.pD3DDevice, QueryInterface, &DWF_IID_IDXGIDevice,
                    (void**)&g_dwf.pDxgiDevice);
    }
    if (g_dwf.pDxgiDevice)
    {
        (void)D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &DWF_IID_ID2D1Factory1, NULL,
                                (void**)&g_dwf.pD2DFactory);
    }
    if (g_dwf.pD2DFactory)
    {
        (void)CCALL(g_dwf.pD2DFactory, CreateDevice, g_dwf.pDxgiDevice, &g_dwf.pD2DDevice);
    }
    if (g_dwf.pD2DDevice)
    {
        (void)CCALL(g_dwf.pD2DDevice, CreateDeviceContext, D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                    &g_dwf.pD2DContext);
        /* V2: DComp on the DXGI device (like ImmersiveWindow), NOT the D2D device. The swapchain -- not a
           DComp surface -- is the visual content, so DComp must not share/recomposite through our D2D
           render context (that coupling let DWM sample mid-draw state -> flicker). */
        (void)DCompositionCreateDevice2((IUnknown*)g_dwf.pDxgiDevice, &DWF_IID_IDCompositionDesktopDevice,
                                        (void**)&g_dwf.pDComp);
    }
    if (g_dwf.pDComp)
    {
        (void)CCALL(g_dwf.pDComp, CreateTargetForHwnd, hwnd, TRUE, &g_dwf.pTarget);
        (void)CCALL(g_dwf.pDComp, CreateVisual, &g_dwf.pVisual);
    }
    if (!g_dwf.pD2DContext || !g_dwf.pTarget || !g_dwf.pVisual)
    {
        DwmFrameDestroy2(hwnd);
        return FALSE;
    }

    {
        UINT bw, bh;
        DfwMonitorSize(hwnd, &bw, &bh);
        if (!DfwCreateSwapchain(hwnd, bw, bh))
        {
            DwmFrameDestroy2(hwnd);
            return FALSE;
        }
    }
    DwfClientSize(hwnd, &cx, &cy);
    g_dwf.cxClient = cx;
    g_dwf.cyClient = cy;
    /* NO SetSourceSize: the full monitor-sized buffer presents 1:1; the window (hwnd target) clips to the
       client rect. SetSourceSize+STRETCH would instead blow the client-sized source up to the buffer size. */
    g_dwf.dwLastTick = GetTickCount();

    /* DWrite for the caption title (non-fatal if it fails -- the bar still composites). */
    (void)DWriteCreateFactory(DWF_FACTORY_TYPE_SHARED, &DWF_IID_IDWriteFactory, (IUnknown**)&g_dwf.pDWrite);
    DwfCreateTextFormat();
    DwfCreateIconFormat();

    (void)CCALL(g_dwf.pTarget, SetRoot, g_dwf.pVisual);
    (void)CCALL0(g_dwf.pDComp, Commit);   /* the ONLY Commit for the lifetime of the static visual tree */

    /* Restore the DWM-drawn frame (shadow / rounded corners / border / extended bounds). */
    DwfApplyDwmFrame(hwnd);

    g_dwf.fWndActive = TRUE;   /* window becomes foreground on the first show; WM_*ACTIVATE corrects it */
    g_dwf.fActive    = TRUE;
    return TRUE;
}

/* Start a color crossfade toward (fDarkTo, fActiveTo). Captures the current shown state as the origin,
   arms the 160ms timer, and renders frame 0. No-op if the target already matches the current state.
   Drives both the theme (dark<->light) and the activation (active<->inactive) transitions. */
static DECLSPEC_NOINLINE void DwfBeginTransition(HWND hwnd, BOOL fDarkTo, BOOL fActiveTo)
{
    if (!g_dwf.fActive || (g_dwf.hwnd != hwnd))
    {
        return;
    }
    if ((g_dwf.fDark == fDarkTo) && (g_dwf.fWndActive == fActiveTo))
    {
        return;   /* already at / heading to this state -- ignore the duplicate (e.g. NCACTIVATE+ACTIVATE) */
    }
    g_dwf.fDarkFrom   = g_dwf.fDark;        /* current shown colors become the crossfade origin */
    g_dwf.fActiveFrom = g_dwf.fWndActive;
    g_dwf.fWndActive  = fActiveTo;
    g_dwf.dwAnimStart = GetTickCount();
    g_dwf.flAnimT     = 0.0f;
    g_dwf.fAnim       = TRUE;
    g_dwf.dwLastTick  = GetTickCount();
    DwmFrameRender2(hwnd, fDarkTo);          /* frame 0; the main render loop continues the timeline */
}

/* A button hover/press state changed: arm the 160ms timer (idempotent) so the per-button highlight
   opacities animate toward their new targets, and paint the current frame now. */
static FORCEINLINE void DwfKickAnim(HWND hwnd)
{
    DwmFrameRender2(hwnd, g_dwf.fDark);   /* immediate feedback; the loop animates the opacity to target */
}

/* Build the cached caption icon ONCE, high-res, via a D3D11 texture: take the window's big icon, rasterize
   it at its native resolution into a premultiplied BGRA DIB, upload that as an ID3D11Texture2D, wrap the
   texture's DXGI surface as an ID2D1Bitmap1, and let DrawBitmap downscale it to the caption with linear
   filtering (crisp at any DPI, unlike the 16px small icon). Attempted at most once. */
static DECLSPEC_NOINLINE void DwfEnsureIcon(HWND hwnd)
{
    HICON                   hIcon;
    ICONINFO                ii;
    BITMAP                  bm;
    BITMAPINFO              biH;
    HDC                     hdcScreen;
    HDC                     hdcMem;
    HBITMAP                 hDib;
    HBITMAP                 hOld;
    void*                   pBits;
    BYTE*                   p;
    int                     n;
    int                     i;
    ID3D11Texture2D*        pTex;
    IDXGISurface*           pSurf;
    D3D11_TEXTURE2D_DESC    td;
    D3D11_SUBRESOURCE_DATA  sd;
    D2D1_BITMAP_PROPERTIES1 bp;

    if (g_dwf.pIconBmp || g_dwfIconTried || !g_dwf.pD3DDevice || !g_dwf.pD2DContext)
    {
        return;
    }
    g_dwfIconTried = TRUE;

    hIcon = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0);
    if (!hIcon) { hIcon = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL2, 0); }
    if (!hIcon) { hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICON); }
    if (!hIcon) { hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM); }
    if (!hIcon) { hIcon = LoadIconW(NULL, IDI_APPLICATION); }
    if (!hIcon) { return; }

    /* Native icon resolution (use the highest available frame -> high-res source). */
    n = 32;
    SecureZeroMemory(&ii, sizeof(ii));
    if (GetIconInfo(hIcon, &ii))
    {
        if (ii.hbmColor && (0 != GetObjectW(ii.hbmColor, (int)sizeof(bm), &bm))) { n = bm.bmWidth; }
        if (ii.hbmColor) { (void)DeleteObject(ii.hbmColor); }
        if (ii.hbmMask)  { (void)DeleteObject(ii.hbmMask); }
    }
    if (n < 16)  { n = 16; }
    if (n > 256) { n = 256; }

    SecureZeroMemory(&biH, sizeof(biH));
    biH.bmiHeader.biSize        = (DWORD)sizeof(BITMAPINFOHEADER);
    biH.bmiHeader.biWidth       = n;
    biH.bmiHeader.biHeight      = -n;            /* top-down */
    biH.bmiHeader.biPlanes      = 1;
    biH.bmiHeader.biBitCount    = 32;
    biH.bmiHeader.biCompression = BI_RGB;

    pBits     = NULL;
    pTex      = NULL;
    pSurf     = NULL;
    hdcScreen = GetDC(NULL);
    hdcMem    = CreateCompatibleDC(hdcScreen);
    hDib      = CreateDIBSection(hdcScreen, &biH, DIB_RGB_COLORS, &pBits, NULL, 0u);
    if (hdcScreen) { (void)ReleaseDC(NULL, hdcScreen); }

    if (hdcMem && hDib && pBits)
    {
        hOld = (HBITMAP)SelectObject(hdcMem, hDib);
        (void)DrawIconEx(hdcMem, 0, 0, hIcon, n, n, 0u, NULL, DI_NORMAL);
        (void)GdiFlush();
        (void)SelectObject(hdcMem, hOld);

        /* DrawIconEx gives straight-alpha BGRA; premultiply for the premultiplied D2D bitmap. */
        p = (BYTE*)pBits;
        for (i = 0; i < n * n; ++i)
        {
            UINT a = p[3];
            p[0] = (BYTE)(((UINT)p[0] * a) / 255u);
            p[1] = (BYTE)(((UINT)p[1] * a) / 255u);
            p[2] = (BYTE)(((UINT)p[2] * a) / 255u);
            p += 4;
        }

        SecureZeroMemory(&td, sizeof(td));
        td.Width            = (UINT)n;
        td.Height           = (UINT)n;
        td.MipLevels        = 1u;
        td.ArraySize        = 1u;
        td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1u;
        td.Usage            = D3D11_USAGE_DEFAULT;
        td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        sd.pSysMem          = pBits;
        sd.SysMemPitch      = (UINT)(n * 4);
        sd.SysMemSlicePitch = 0u;
        (void)CCALL(g_dwf.pD3DDevice, CreateTexture2D, &td, &sd, &pTex);
        if (pTex)
        {
            (void)CCALL((IUnknown*)pTex, QueryInterface, &IID_IDXGISurface, (void**)&pSurf);
        }
        if (pSurf)
        {
            bp.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
            bp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
            bp.dpiX                  = 96.0f;
            bp.dpiY                  = 96.0f;
            bp.bitmapOptions         = D2D1_BITMAP_OPTIONS_NONE;
            bp.colorContext          = NULL;
            (void)CCALL(g_dwf.pD2DContext, CreateBitmapFromDxgiSurface, pSurf, &bp, &g_dwf.pIconBmp);
        }
    }

    DwfRelease((IUnknown**)&pSurf);
    DwfRelease((IUnknown**)&pTex);
    if (hDib)   { (void)DeleteObject(hDib); }
    if (hdcMem) { (void)DeleteDC(hdcMem); }
}

/* V2: advance the crossfade + per-button opacities by wall-clock. The ImmersiveWindow-style render loop
   calls DfwRenderEx every frame, so the animation timeline is driven here, not by a WM_TIMER. */
static void DfwAdvance(void)
{
    DWORD now = GetTickCount();
    float dt  = (float)(now - g_dwf.dwLastTick) / (float)DWF_ANIM_DURATION;
    int   i;

    g_dwf.dwLastTick = now;
    if (g_dwf.fAnim)
    {
        float t = (float)(now - g_dwf.dwAnimStart) / (float)DWF_ANIM_DURATION;
        if (t >= 1.0f) { t = 1.0f; g_dwf.fAnim = FALSE; }
        g_dwf.flAnimT = t;
    }
    else
    {
        g_dwf.flAnimT = 1.0f;
    }
    for (i = DWB_LIGHTDARK; i <= DWB_CLOSE; ++i)
    {
        float tgt = DwfBtnTarget(i);
        float cur = g_dwf.flBtnOpacity[i];
        if (cur < tgt)      { cur += dt; if (cur > tgt) { cur = tgt; } }
        else if (cur > tgt) { cur -= dt; if (cur < tgt) { cur = tgt; } }
        g_dwf.flBtnOpacity[i] = cur;
    }
}

static DECLSPEC_NOINLINE void DfwRenderEx(HWND hwnd, BOOL fDark, BOOL fRestart, BOOL fVsync);
void WINAPI DwmFrameRender2(HWND hwnd, BOOL fDark) { DfwRenderEx(hwnd, fDark, TRUE, TRUE); }  /* ImmersiveWindow: EndImmersivePaint(1,1) */
static DECLSPEC_NOINLINE void DfwRenderEx(HWND hwnd, BOOL fDark, BOOL fRestart, BOOL fVsync)
{
    ID2D1DeviceContext*   pDC;
    ID2D1SolidColorBrush* pBrush;
    D2D1_COLOR_F          col;
    D2D1_COLOR_F          colClient;
    D2D1_COLOR_F          colCap;
    D2D1_COLOR_F          colText;
    D2D1_COLOR_F          colGlyph;
    D2D1_MATRIX_3X2_F     xform;
    D2D1_RECT_F           rcText;
    WCHAR                 szTitle[256];
    int                   cch;
    int                   leftPad;
    int                   capH;
    BOOL                  fActive;

    if (!g_dwf.fActive || (g_dwf.hwnd != hwnd))
    {
        return;
    }

    DfwAdvance();   /* advance the animation timeline (loop-driven, not WM_TIMER) */

    /* V2: monitor-sized buffer -- never ResizeBuffers. Regrow only if the client outgrew the buffer
       (moved to a larger monitor); otherwise track the client size + presented sub-rect. */
    {
        UINT cx;
        UINT cy;

        DwfClientSize(hwnd, &cx, &cy);
        if ((cx > g_dwf.cxBuffer) || (cy > g_dwf.cyBuffer))
        {
            UINT bw, bh;
            DfwMonitorSize(hwnd, &bw, &bh);
            if (bw < cx) { bw = cx; }
            if (bh < cy) { bh = cy; }
            if (!DfwCreateSwapchain(hwnd, bw, bh)) { return; }
            (void)CCALL0(g_dwf.pDComp, Commit);   /* re-publish SetContent(new swapchain) */
        }
        if ((cx != g_dwf.cxClient) || (cy != g_dwf.cyClient))
        {
            g_dwf.cxClient = cx;
            g_dwf.cyClient = cy;
        }
    }

    if (!g_dwf.pTargetBmp) { return; }
    pDC = g_dwf.pD2DContext;                   /* persistent target context (not a per-frame surface DC) */
    CCALL0(pDC, BeginDraw);
    xform._11 = 1.0f; xform._12 = 0.0f;
    xform._21 = 0.0f; xform._22 = 1.0f;
    xform._31 = 0.0f; xform._32 = 0.0f;         /* fixed full-size target: origin (0,0), identity */
    CCALL(pDC, SetTransform, &xform);
    {
        D2D1_RECT_F rcClip;
        rcClip.left = 0.0f; rcClip.top = 0.0f;
        rcClip.right = (FLOAT)g_dwf.cxClient; rcClip.bottom = (FLOAT)g_dwf.cyClient;
        CCALL(pDC, PushAxisAlignedClip, &rcClip, D2D1_ANTIALIAS_MODE_ALIASED);  /* bound Clear/draw to client */
    }

    g_dwf.fDark  = fDark;              /* target shade (repaint contract for hover/size renders) */
    fActive      = g_dwf.fWndActive;   /* window-activation is owned by the activation messages */
    capH         = (int)DwfCaptionHeight(hwnd);

    /* Effective colors. During a crossfade, lerp between the (from) state captured at transition start and
       the (to = current) state by flAnimT; otherwise just the current state. One path serves the theme
       (dark<->light) AND the activation (active<->inactive) transitions -- uDWM's shared 160ms timeline. */
    {
        BOOL  fDk1 = g_dwf.fAnim ? g_dwf.fDarkFrom   : fDark;
        BOOL  fAc1 = g_dwf.fAnim ? g_dwf.fActiveFrom : fActive;
        float t    = g_dwf.fAnim ? g_dwf.flAnimT     : 1.0f;

        colClient = DwfLerp(DwfClientColor(fDk1),         DwfClientColor(fDark),          t);
        colCap    = DwfLerp(DwfCaptionColor(fDk1, fAc1),  DwfCaptionColor(fDark, fActive), t);
        colText   = DwfLerp(DwfTextColor(fDk1, fAc1),     DwfTextColor(fDark, fActive),    t);
        colGlyph  = DwfLerp(DwfGlyphColor(fDk1, fAc1),    DwfGlyphColor(fDark, fActive),   t);
    }

    /* Backdrop. Solid (DWMSBT 0/1): fill the whole window with the client color, caption band on top --
       identical to a native overlapped window. Mica/Acrylic (DWMSBT 2/3): clear to TRANSPARENT and paint
       NO opaque fill, so DWM's system backdrop (composited beneath this premultiplied-alpha surface) shows
       through the whole window; only the title + buttons are drawn on top. */
    if (g_dwf.iBackdrop >= DWF_DWMSBT_MAINWINDOW)
    {
        col.r = 0.0f; col.g = 0.0f; col.b = 0.0f; col.a = 0.0f;
        CCALL(pDC, Clear, &col);
    }
    else
    {
        col = colClient;
        CCALL(pDC, Clear, &col);
        {
            ID2D1SolidColorBrush* pCap;
            D2D1_RECT_F           rcCap;

            pCap = NULL;
            (void)CCALL(pDC, CreateSolidColorBrush, &colCap, NULL, &pCap);
            if (pCap)
            {
                rcCap.left   = 0.0f;
                rcCap.top    = 0.0f;
                rcCap.right  = (FLOAT)g_dwf.cxClient;
                rcCap.bottom = (FLOAT)capH;
                CCALL(pDC, FillRectangle, &rcCap, (ID2D1Brush*)pCap);
                DwfRelease((IUnknown**)&pCap);
            }
        }
    }

    /* High-res caption system icon, placed EXACTLY as win32kfull!DrawCaptionIcon does: the icon sits in a
       caption-height square slot at the caption-left (rc.left = 0 here, no system frame), sized
       SM_CXSMICON x SM_CYSMICON for the window DPI and centered in that slot --
           X = rc.left + (capH - SM_CXSMICON)/2 + 1,   Y = rc.top + (capH - SM_CYSMICON)/2.
       The title text then starts at rc.left + capH (xxxDrawCaptionTemp: rc.left += capH). The D3D11-texture
       source is downscaled here with linear filtering for crispness. */
    {
        UINT dpi = GetDpiForWindow(hwnd);
        int  iw;
        int  ih;
        int  ix;
        int  iy;

        if (0u == dpi) { dpi = 96u; }
        iw = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
        ih = GetSystemMetricsForDpi(SM_CYSMICON, dpi);
        ix = (capH - iw) / 2 + 1;
        iy = (capH - ih) / 2;

        DwfEnsureIcon(hwnd);
        if (g_dwf.pIconBmp)
        {
            D2D1_RECT_F ri;
            ri.left   = (FLOAT)ix;
            ri.top    = (FLOAT)iy;
            ri.right  = (FLOAT)(ix + iw);
            ri.bottom = (FLOAT)(iy + ih);
            CCALL(pDC, DrawBitmap, (ID2D1Bitmap*)g_dwf.pIconBmp, &ri, 1.0f,
                  D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, NULL);
        }
    }

    /* Caption title (DWrite, caption font). cch is not range-checked before the draw (szTitle[0] gates),
       so no Spectre index reaches the DrawText call. */
    szTitle[0] = 0;
    cch = GetWindowTextW(hwnd, szTitle, ARRAYSIZE(szTitle));
    if (g_dwf.pTextFormat && szTitle[0])
    {
        pBrush = NULL;
        (void)CCALL(pDC, CreateSolidColorBrush, &colText, NULL, &pBrush);
        if (pBrush)
        {
            leftPad       = capH;   /* xxxDrawCaptionTemp: title starts one caption-height in (icon slot) */
            rcText.left   = (FLOAT)leftPad;
            rcText.top    = 0.0f;
            rcText.right  = (FLOAT)g_dwf.cxClient;
            rcText.bottom = (FLOAT)capH;
            CCALL(pDC, DrawText, szTitle, (UINT32)cch, g_dwf.pTextFormat, &rcText,
                  (ID2D1Brush*)pBrush, D2D1_DRAW_TEXT_OPTIONS_NONE, DWF_MEASURING_MODE_NATURAL);
            DwfRelease((IUnknown**)&pBrush);
        }
    }

    /* Caption buttons: light/dark (sun/moon), Minimize, Maximize/Restore, Close. Glyph tint is the
       crossfaded colGlyph; DwfDrawButton overrides it only for Close hover/press (red bg, white glyph). */
    {
        RECT rcClose;
        RECT rcMax;
        RECT rcMin;
        RECT rcLD;

        if (DwfButtonRects(hwnd, &rcClose, &rcMax, &rcMin, &rcLD))
        {
            DwfDrawButton(pDC, &rcLD,    DWB_LIGHTDARK, (WCHAR)(fDark ? 0xE706 : 0xE708), fDark, colGlyph, g_dwf.flBtnOpacity[DWB_LIGHTDARK]);
            DwfDrawButton(pDC, &rcMin,   DWB_MIN,       (WCHAR)0xE921,                    fDark, colGlyph, g_dwf.flBtnOpacity[DWB_MIN]);
            DwfDrawButton(pDC, &rcMax,   DWB_MAX,       (WCHAR)(IsZoomed(hwnd) ? 0xE923 : 0xE922), fDark, colGlyph, g_dwf.flBtnOpacity[DWB_MAX]);
            DwfDrawButton(pDC, &rcClose, DWB_CLOSE,     (WCHAR)0xE8BB,                    fDark, colGlyph, g_dwf.flBtnOpacity[DWB_CLOSE]);
        }
    }

    CCALL0(pDC, PopAxisAlignedClip);
    (void)CCALL(pDC, EndDraw, NULL, NULL);
    /* ImmersiveWindow order: the GPU draw is now queued; wait for the vertical blank, THEN present so the
       frame lands right at scanout (minimizes the wait-to-present gap). */
    if (g_dwf.hFrameWait) { (void)WaitForSingleObjectEx(g_dwf.hFrameWait, 100u, FALSE); }
    /* EXACT ImmersiveWindow double-present. (1) immediate, no-wait, tearing -- push this frame to the
       compositor now, cancelling any queued frame on a restart. (2) sync to the next vblank with
       DO_NOT_SEQUENCE so the CURRENT buffer is presented *instead of advancing* to the next back buffer
       (this is what stops the buffer-alternation flicker a plain Present(1) produces). */
    {
        UINT p1 = (UINT)(DXGI_PRESENT_ALLOW_TEARING | DXGI_PRESENT_DO_NOT_WAIT)
                | (fRestart ? (UINT)DXGI_PRESENT_RESTART : 0u);
        (void)CCALL(g_dwf.pSwapchain, Present, 0u, p1);
        if (fRestart || fVsync)
        {
            (void)CCALL(g_dwf.pSwapchain, Present, 1u, (UINT)DXGI_PRESENT_DO_NOT_SEQUENCE);
        }
        else
        {
            (void)CCALL(g_dwf.pSwapchain, Present, 0u,
                        (UINT)(DXGI_PRESENT_ALLOW_TEARING | DXGI_PRESENT_DO_NOT_WAIT));
        }
    }
    /* ImmersiveWindow: DwmFlush() after the present -- block until DWM has composited this frame so the
       render loop cannot race ahead and queue presents (that backlog is the "DWM fighting us" flicker). */
    if (g_dwfFlush) { (void)g_dwfFlush(); }
}

/* Public: crossfade the caption to a new theme shade (keeps the current activation state). Called on the
   light/dark button toggle and on a system theme change, in place of an instant DwmFrameRender2. */
DECLSPEC_NOINLINE void WINAPI DwmFrameAnimateTheme2(HWND hwnd, BOOL fDark)
{
    DwfBeginTransition(hwnd, fDark, g_dwf.fWndActive);
}

/* Select the caption/window backdrop. iType: 0/1 (DWMSBT_NONE) = solid caption (native overlapped-window
   look); 2 (DWMSBT_MAINWINDOW) = Mica; 3 (DWMSBT_TRANSIENTWINDOW) = Acrylic. For Mica/Acrylic the render
   clears the surface transparent and DWM composites the system backdrop beneath it (the app does not draw
   the blur/tint); the frame is extended fully so the backdrop spans the whole window. Both modes are fully
   supported and switchable at runtime. */
DECLSPEC_NOINLINE void WINAPI DwmFrameSetBackdrop2(HWND hwnd, int iType)
{
    DWF_MARGINS m;
    UINT        t;

    if (!g_dwf.fActive || (g_dwf.hwnd != hwnd))
    {
        return;
    }
    g_dwf.iBackdrop = iType;
    DwfApplyDwmFrame(hwnd);   /* make sure dwmapi is loaded + corner preference applied */
    if (g_dwfSetAttr)
    {
        t = (UINT)iType;
        (void)g_dwfSetAttr(hwnd, DWF_DWMWA_SYSTEMBACKDROP_TYPE, &t, (DWORD)sizeof(t));
    }
    if (g_dwfExtend)
    {
        if (iType >= DWF_DWMSBT_MAINWINDOW)
        {
            m.cxLeft = -1; m.cxRight = -1; m.cyTop = -1; m.cyBottom = -1;  /* full glass: backdrop everywhere */
        }
        else
        {
            m.cxLeft = 0; m.cxRight = 0; m.cyTop = 1; m.cyBottom = 0;      /* 1px: shadow/border only */
        }
        (void)g_dwfExtend(hwnd, &m);
    }
    DwmFrameRender2(hwnd, g_dwf.fDark);
}

void WINAPI DwmFrameResize2(HWND hwnd)
{
    UINT cx;
    UINT cy;

    if (!g_dwf.fActive || (g_dwf.hwnd != hwnd))
    {
        return;
    }
    DwfClientSize(hwnd, &cx, &cy);
    if ((cx == g_dwf.cxClient) && (cy == g_dwf.cyClient))
    {
        return;
    }
    /* V2: no surface realloc, no ResizeBuffers -- regrow the monitor-sized buffer only if the client
       outgrew it, then just move the presented sub-rect (SetSourceSize) and repaint atomically. */
    if ((cx > g_dwf.cxBuffer) || (cy > g_dwf.cyBuffer))
    {
        UINT bw, bh;
        DfwMonitorSize(hwnd, &bw, &bh);
        if (bw < cx) { bw = cx; }
        if (bh < cy) { bh = cy; }
        if (DfwCreateSwapchain(hwnd, bw, bh)) { (void)CCALL0(g_dwf.pDComp, Commit); }
    }
    g_dwf.cxClient = cx;
    g_dwf.cyClient = cy;
    DfwRenderEx(hwnd, g_dwf.fDark, TRUE, FALSE);   /* fRestart, no vsync: immediate during a resize */
}

DECLSPEC_NOINLINE void WINAPI DwmFrameDestroy2(HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);
    g_dwf.fAnim       = FALSE;
    g_dwf.fInSizeMove = FALSE;
    DwfRelease((IUnknown**)&g_dwf.pTextFormat);
    DwfRelease((IUnknown**)&g_dwf.pIconFormat);
    DwfRelease((IUnknown**)&g_dwf.pIconBmp);
    DwfRelease((IUnknown**)&g_dwf.pDWrite);
    if (g_dwf.pD2DContext) { CCALL(g_dwf.pD2DContext, SetTarget, NULL); }
    DwfRelease((IUnknown**)&g_dwf.pTargetBmp);
    DwfRelease((IUnknown**)&g_dwf.pSwapchain);
    if (g_dwf.hFrameWait) { (void)CloseHandle(g_dwf.hFrameWait); g_dwf.hFrameWait = NULL; }  /* caller-owned (docs) */
    DwfRelease((IUnknown**)&g_dwf.pVisual);
    DwfRelease((IUnknown**)&g_dwf.pTarget);
    DwfRelease((IUnknown**)&g_dwf.pDComp);
    DwfRelease((IUnknown**)&g_dwf.pD2DContext);
    DwfRelease((IUnknown**)&g_dwf.pD2DDevice);
    DwfRelease((IUnknown**)&g_dwf.pD2DFactory);
    DwfRelease((IUnknown**)&g_dwf.pDxgiDevice);
    DwfRelease((IUnknown**)&g_dwf.pD3DDevice);
    g_dwf.fActive   = FALSE;
    g_dwf.cxClient  = 0u;
    g_dwf.cyClient  = 0u;
    g_dwf.cxBuffer  = 0u;
    g_dwf.cyBuffer  = 0u;
    g_dwf.hwnd      = NULL;
}

/* ---- Phase 4: non-client hit-test + button input (reproducing FindNCHit / xxxTrackCaptionButton) ---- */

#ifndef TME_NONCLIENT
#define TME_NONCLIENT  0x00000010
#endif

static FORCEINLINE UINT DwfDpi(HWND hwnd)
{
    UINT d;

    d = GetDpiForWindow(hwnd);
    return d ? d : 96u;
}

static FORCEINLINE int DwfMetric(int index, UINT dpi)
{
    return GetSystemMetricsForDpi(index, dpi);
}

static FORCEINLINE int DwfButtonFromHit(WPARAM hit)
{
    switch (hit)
    {
        case HTCLOSE:        return DWB_CLOSE;
        case HTMAXBUTTON:    return DWB_MAX;
        case HTMINBUTTON:    return DWB_MIN;
        case HTLIGHTDARKBTN: return DWB_LIGHTDARK;
        default:             return DWB_NONE;
    }
}

static FORCEINLINE void DwfTrackLeave(HWND hwnd)
{
    TRACKMOUSEEVENT tme;

    if (g_dwf.fTracking)
    {
        return;
    }
    SecureZeroMemory(&tme, sizeof(tme));
    tme.cbSize    = (DWORD)sizeof(tme);
    tme.dwFlags   = TME_LEAVE | TME_NONCLIENT;
    tme.hwndTrack = hwnd;
    if (TrackMouseEvent(&tme))
    {
        g_dwf.fTracking = TRUE;
    }
}

static DECLSPEC_NOINLINE void DwfButtonAction(HWND hwnd, int id, void (WINAPI* pfnToggle)(HWND))
{
    switch (id)
    {
        case DWB_MIN:       (void)PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0); break;
        case DWB_MAX:       (void)PostMessageW(hwnd, WM_SYSCOMMAND, IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0); break;
        case DWB_CLOSE:     (void)PostMessageW(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0); break;
        case DWB_LIGHTDARK: if (pfnToggle) { pfnToggle(hwnd); } break;
        default:            break;
    }
}

/* FindNCHit order: caption buttons (inset by GetTitleBarInfoEx, so disjoint from the resize corners) ->
   system-menu icon -> resize ring/corners (outer frame) -> caption -> client. Returns HT* constants only,
   so no range-checked index reaches the return (C5045-clean). */
static DECLSPEC_NOINLINE LRESULT DwfHitTest(HWND hwnd, LPARAM lParam)
{
    POINT    ptScreen;
    POINT    ptClient;
    RECT     rc;
    RECT     rcClose;
    RECT     rcMax;
    RECT     rcMin;
    RECT     rcLD;
    RECT     rcIcon;
    LONG_PTR style;
    UINT     dpi;
    int      pad;
    int      cxF;
    int      cyF;
    int      capH;
    int      row;
    int      col;
    BOOL     fSizable;

    ptScreen.x = (int)(short)LOWORD(lParam);
    ptScreen.y = (int)(short)HIWORD(lParam);
    ptClient   = ptScreen;
    ScreenToClient(hwnd, &ptClient);
    GetClientRect(hwnd, &rc);
    dpi      = DwfDpi(hwnd);
    pad      = DwfMetric(SM_CXPADDEDBORDER, dpi);
    cxF      = DwfMetric(SM_CXFRAME, dpi) + pad;
    cyF      = DwfMetric(SM_CYFRAME, dpi) + pad;
    capH     = (int)DwfCaptionHeight(hwnd);
    style    = GetWindowLongPtr(hwnd, GWL_STYLE);
    fSizable = (0 != (style & WS_THICKFRAME)) ? TRUE : FALSE;

    if (DwfButtonRects(hwnd, &rcClose, &rcMax, &rcMin, &rcLD))
    {
        if (PtInRect(&rcLD, ptClient))    { return HTLIGHTDARKBTN; }
        if (PtInRect(&rcMin, ptClient))   { return HTMINBUTTON; }
        if (PtInRect(&rcMax, ptClient))   { return HTMAXBUTTON; }
        if (PtInRect(&rcClose, ptClient)) { return HTCLOSE; }
    }

    rcIcon.left   = cxF;
    rcIcon.top    = 0;
    rcIcon.right  = cxF + GetSystemMetrics(SM_CXSMICON);
    rcIcon.bottom = capH;
    if (PtInRect(&rcIcon, ptClient))
    {
        return HTSYSMENU;
    }

    row = 1;
    col = 1;
    if (ptClient.y < cyF)                  { row = 0; }
    else if (ptClient.y >= rc.bottom - cyF) { row = 2; }
    if (ptClient.x < cxF)                  { col = 0; }
    else if (ptClient.x >= rc.right - cxF) { col = 2; }

    if (0 == row)
    {
        if (0 == col) { return fSizable ? HTTOPLEFT  : HTCAPTION; }
        if (2 == col) { return fSizable ? HTTOPRIGHT : HTCAPTION; }
        return fSizable ? HTTOP : HTCAPTION;
    }
    if (2 == row)
    {
        if (0 == col) { return fSizable ? HTBOTTOMLEFT  : HTBORDER; }
        if (2 == col) { return fSizable ? HTBOTTOMRIGHT : HTBORDER; }
        return fSizable ? HTBOTTOM : HTBORDER;
    }
    if (0 == col) { return fSizable ? HTLEFT  : HTBORDER; }
    if (2 == col) { return fSizable ? HTRIGHT : HTBORDER; }
    if (ptClient.y < capH) { return HTCAPTION; }
    return HTCLIENT;
}

static FORCEINLINE LRESULT DwfHitScreen(HWND hwnd, int x, int y)
{
    LPARAM lp;

    lp = (LPARAM)((((DWORD)y & 0xFFFFu) << 16) | ((DWORD)x & 0xFFFFu));
    return DwfHitTest(hwnd, lp);
}

BOOL WINAPI DwmFrameHandleMessage2(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                  void (WINAPI* pfnToggle)(HWND), LRESULT* plr)
{
    int   id;
    POINT pt;

    if (!plr)
    {
        return FALSE;
    }

    /* Remove the standard non-client area on EVERY WM_NCCALCSIZE -- even before the DComp pipeline is up
       (the first one arrives during CreateWindowEx, before DwmFrameInit2). If we waited for DwmFrameActive2,
       DefWindowProc would draw the standard caption (title + sysmenu icon) for that first frame; it then
       composited UNDER our DComp caption, producing the doubled/ghosted title at startup. Strip it from the
       very first message so the system never draws a caption. */
    if (WM_NCCALCSIZE == uMsg)
    {
        if (wParam)
        {
            NCCALCSIZE_PARAMS* p = (NCCALCSIZE_PARAMS*)lParam;
            if (IsZoomed(hwnd))
            {
                UINT dpi = DwfDpi(hwnd);
                int  pad = DwfMetric(SM_CXPADDEDBORDER, dpi);
                int  fx  = DwfMetric(SM_CXFRAME, dpi) + pad;
                int  fy  = DwfMetric(SM_CYFRAME, dpi) + pad;
                p->rgrc[0].left   += fx;
                p->rgrc[0].top    += fy;
                p->rgrc[0].right  -= fx;
                p->rgrc[0].bottom -= fy;
            }
            /* V2: fill the resized region synchronously, before DWM finalizes the geometry. The proposed
               client is rgrc[0] (post maximize-inset). SetSourceSize + vblank-aligned restart present. */
            if (g_dwf.fActive && (g_dwf.hwnd == hwnd) && g_dwf.pSwapchain)
            {
                UINT cx = (UINT)(p->rgrc[0].right - p->rgrc[0].left);
                UINT cy = (UINT)(p->rgrc[0].bottom - p->rgrc[0].top);
                if (cx < 1u) { cx = 1u; }
                if (cy < 1u) { cy = 1u; }
                if (((cx != g_dwf.cxClient) || (cy != g_dwf.cyClient)) &&
                    (cx <= g_dwf.cxBuffer) && (cy <= g_dwf.cyBuffer))
                {
                    g_dwf.cxClient = cx;
                    g_dwf.cyClient = cy;
                    DfwRenderEx(hwnd, g_dwf.fDark, TRUE, FALSE);
                }
            }
            *plr = 0;
            return TRUE;
        }
        return FALSE;
    }

    if (!DwmFrameActive2(hwnd))
    {
        return FALSE;
    }
    switch (uMsg)
    {
        case WM_NCHITTEST:
            *plr = DwfHitTest(hwnd, lParam);
            return TRUE;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            (void)BeginPaint(hwnd, &ps);
            DwmFrameRender2(hwnd, g_dwf.fDark);
            (void)EndPaint(hwnd, &ps);
            *plr = 0;
            return TRUE;
        }

        case WM_NCPAINT:
            /* We own the whole window (NC removed); the caption lives in our composited surface, so an NC
               repaint must re-render it. Suppressing this (just *plr=0) left the caption stale at startup
               until a mouse-over the NC forced a render -- THIS is the non-client redraw that was missing. */
            DwmFrameRender2(hwnd, g_dwf.fDark);
            *plr = 0;
            return TRUE;

        case WM_NCACTIVATE:
            DwfBeginTransition(hwnd, g_dwf.fDark, (wParam != FALSE));
            /* Propagate to DefWindowProc with lParam -1 (update the window's activation state, do NOT
               repaint the standard NC -- we own the caption) so DWM TRACKS activation and renders the
               correct active/inactive frame. Returning TRUE without this froze DWM's frame, which only
               corrected when the real foreground changed (the "click the desktop to fix it" symptom). */
            *plr = DefWindowProcW(hwnd, WM_NCACTIVATE, wParam, (LPARAM)-1);
            return TRUE;

        case WM_ACTIVATE:
            /* Report the frame change in RESPONSE TO WM_ACTIVATE -- the documented DWM custom-frame contract.
               DwmExtendFrameIntoClientArea applied in DwmFrameInit2 (before the window was shown/activated)
               does not take effect; it must be (re-)reported here, on activation, for the DWM frame to
               settle. THEN redraw the non-client area so the result shows immediately. */
            DwfApplyDwmFrame(hwnd);
            (void)SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                               SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            (void)RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
            DwmFrameRender2(hwnd, g_dwf.fDark);
            if (g_dwfFlush) { (void)g_dwfFlush(); }   /* force DWM to present the commit now */
            DwfBeginTransition(hwnd, g_dwf.fDark, (LOWORD(wParam) != WA_INACTIVE));
            return FALSE;

        case WM_ENTERSIZEMOVE:
            if (g_dwf.fActive && (g_dwf.hwnd == hwnd)) { g_dwf.fInSizeMove = TRUE; }
            return FALSE;

        case WM_EXITSIZEMOVE:
            if (g_dwf.fActive && (g_dwf.hwnd == hwnd))
            {
                g_dwf.fInSizeMove = FALSE;
                DfwRenderEx(hwnd, g_dwf.fDark, FALSE, TRUE);
            }
            return FALSE;

        /* No WM_TIMER: the ImmersiveWindow-style main render loop drives the animation timeline (DfwAdvance
           inside every DfwRenderEx). Painting during the system modal size/move loop (when the main loop is
           suspended) comes from WM_SIZE -> DwmFrameResize2 and the WM_NCCALCSIZE synchronous repaint. */

        case WM_NCMOUSEMOVE:
            id = DwfButtonFromHit(wParam);
            DwfTrackLeave(hwnd);
            if (g_dwf.idHot != id)
            {
                g_dwf.idHot = id;
                DwfKickAnim(hwnd);
            }
            return FALSE;

        case WM_NCMOUSELEAVE:
            g_dwf.fTracking = FALSE;
            if (g_dwf.idHot != DWB_NONE)
            {
                g_dwf.idHot = DWB_NONE;
                DwfKickAnim(hwnd);
            }
            return FALSE;

        case WM_NCLBUTTONDOWN:
            id = DwfButtonFromHit(wParam);
            if (DWB_NONE != id)
            {
                g_dwf.idPressed  = id;
                g_dwf.idHot      = id;
                g_dwf.fCapturing = TRUE;
                (void)SetCapture(hwnd);
                DwfKickAnim(hwnd);
                *plr = 0;
                return TRUE;
            }
            return FALSE;

        case WM_MOUSEMOVE:
            if (g_dwf.fCapturing)
            {
                pt.x = (int)(short)LOWORD(lParam);
                pt.y = (int)(short)HIWORD(lParam);
                (void)ClientToScreen(hwnd, &pt);
                id = DwfButtonFromHit((WPARAM)DwfHitScreen(hwnd, pt.x, pt.y));
                if (g_dwf.idHot != id)
                {
                    g_dwf.idHot = id;
                    DwfKickAnim(hwnd);
                }
            }
            return FALSE;

        case WM_LBUTTONUP:
            if (g_dwf.fCapturing)
            {
                int pressed = g_dwf.idPressed;
                pt.x = (int)(short)LOWORD(lParam);
                pt.y = (int)(short)HIWORD(lParam);
                (void)ClientToScreen(hwnd, &pt);
                id = DwfButtonFromHit((WPARAM)DwfHitScreen(hwnd, pt.x, pt.y));
                g_dwf.fCapturing = FALSE;
                g_dwf.idPressed  = DWB_NONE;
                (void)ReleaseCapture();
                DwfKickAnim(hwnd);
                if ((DWB_NONE != pressed) && (pressed == id))
                {
                    DwfButtonAction(hwnd, pressed, pfnToggle);
                }
                *plr = 0;
                return TRUE;
            }
            return FALSE;

        case WM_CAPTURECHANGED:
            if (g_dwf.fCapturing)
            {
                g_dwf.fCapturing = FALSE;
                g_dwf.idPressed  = DWB_NONE;
                DwfKickAnim(hwnd);
            }
            return FALSE;

        default:
            return FALSE;
    }
}

DECLSPEC_NOINLINE BOOL WINAPI DwmFrameActive2(HWND hwnd)
{
    return g_dwf.fActive && (g_dwf.hwnd == hwnd);
}

/* TRUE while the caption still has frames to produce (crossfade in flight, a button opacity not yet at its
   target, or inside a size/move loop). The ImmersiveWindow-style render loop renders every iteration but
   only blocks on WaitMessage when this is FALSE and there is no pending input -- so idle costs nothing. */
DECLSPEC_NOINLINE BOOL WINAPI DwmFrameIsBusy2(HWND hwnd)
{
    int i;

    if (!g_dwf.fActive || (g_dwf.hwnd != hwnd)) { return FALSE; }
    if (g_dwf.fAnim || g_dwf.fInSizeMove)       { return TRUE; }
    for (i = DWB_LIGHTDARK; i <= DWB_CLOSE; ++i)
    {
        if (g_dwf.flBtnOpacity[i] != DwfBtnTarget(i)) { return TRUE; }
    }
    return FALSE;
}

/* Block until the swapchain is ready to accept the next frame (frame-latency waitable, MaxFrameLatency=1
   => roughly once per vertical blank). The render loop calls this BEFORE drawing, mirroring
   ImmersiveWindow's WaitForVerticalBlank-before-paint: it paces the loop to native refresh and keeps the
   present queue from backing up (which is what made a plain post-draw Present(1) judder). */
DECLSPEC_NOINLINE void WINAPI DwmFrameWaitFrame2(HWND hwnd)
{
    if (g_dwf.fActive && (g_dwf.hwnd == hwnd) && g_dwf.hFrameWait)
    {
        (void)WaitForSingleObjectEx(g_dwf.hFrameWait, 100u, FALSE);
    }
}

/* Seed the caption shade without painting. At creation we publish the frame change (SWP_FRAMECHANGED) and
   invalidate the whole window once; the resulting WM_PAINT renders with g_dwf.fDark, so it must already
   hold the app's theme -- otherwise the first paint is light in dark mode. */
DECLSPEC_NOINLINE void WINAPI DwmFrameSetDark2(HWND hwnd, BOOL fDark)
{
    if (g_dwf.fActive && (g_dwf.hwnd == hwnd))
    {
        g_dwf.fDark = fDark;
    }
}
