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
#include <d3d11.h>
#include <d2d1.h>
#include <d2d1_1.h>
/* NOT <dwrite.h>: it is C++-only (enum-class underlying-type ':' syntax, static_cast). We forward-declare
   IDWriteFactory/IDWriteTextFormat and pass its enums as plain UINT constants (defined below).
   NOT <dcomp.h>: its C ("CINTERFACE") path is broken -- IDCompositionVisual has overloaded methods
   (SetOffsetX(float) vs SetOffsetX(IDCompositionAnimation*)) and C++ reference params, which cannot be
   expressed in a C struct. So we hand-declare exactly the DirectComposition interfaces we use, as C
   vtable structs, with slot indices read straight out of dcomp.h. */

#include "Win32X/dwmframex.h"

/* CRT-free: the compiler emits a reference to _fltused as soon as a translation unit uses floating
   point (the D2D colors / 3x2 matrix here). /NODEFAULTLIB drops the CRT that would define it, so we
   supply it ourselves -- one definition for the whole image. */
extern int _fltused;
int _fltused = 0x9875;

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
    void*   rsvd4[16];  /* slots 31..46: GetTransform..PopAxisAlignedClip */
    void    (STDMETHODCALLTYPE* Clear)(ID2D1DeviceContext*, const D2D1_COLOR_F*);             /* slot 47 */
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
#define DWF_DWMWCP_ROUND                   2
#define DWF_DWMWA_COLOR_DEFAULT            0xFFFFFFFFu

static HMODULE         g_dwfDwmapi;
static PFN_DWF_EXTEND  g_dwfExtend;
static PFN_DWF_SETATTR g_dwfSetAttr;

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
    IDCompositionSurface* pSurface;
    IDWriteFactory*       pDWrite;
    IDWriteTextFormat*    pTextFormat;
    IDWriteTextFormat*    pIconFormat;
    UINT                  cxSurface;
    UINT                  cySurface;
    BOOL                  fActive;
    int                   idHot;        /* DWF_BTN under the cursor, or DWB_NONE */
    int                   idPressed;    /* DWF_BTN currently pressed (captured), or DWB_NONE */
    BOOL                  fTracking;    /* TME_NONCLIENT leave tracking armed */
    BOOL                  fCapturing;   /* a button press holds the mouse capture */
    BOOL                  fDark;        /* last rendered dark state (for re-render on hover/press) */
} DWF_STATE;

static DWF_STATE g_dwf;

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

/* One caption button: hover/press flat fill (Close goes red, like the shell) + the glyph. State comes
   from g_dwf.idHot/idPressed. */
static DECLSPEC_NOINLINE void DwfDrawButton(ID2D1DeviceContext* pDC, const RECT* prc, int id, WCHAR glyph,
                                            BOOL fDark, BOOL fActive)
{
    BOOL                  fHot;
    BOOL                  fPressed;
    BOOL                  fFill;
    COLORREF              crFill;
    COLORREF              crGlyph;
    D2D1_COLOR_F          cf;
    D2D1_RECT_F           rf;
    ID2D1SolidColorBrush* pb;

    fHot     = (g_dwf.idHot == id) && (g_dwf.idPressed == DWB_NONE);
    fPressed = (g_dwf.idPressed == id) && (g_dwf.idHot == id);
    crGlyph  = fActive ? (fDark ? RGB(255, 255, 255) : RGB(0, 0, 0))
                       : (fDark ? RGB(0xAA, 0xAA, 0xAA) : RGB(0x64, 0x64, 0x64));
    crFill   = 0;
    fFill    = FALSE;
    if (DWB_CLOSE == id)
    {
        if (fPressed) { crFill = RGB(0xC8, 0x3C, 0x2F); crGlyph = RGB(255, 255, 255); fFill = TRUE; }
        else if (fHot) { crFill = RGB(0xC4, 0x2B, 0x1C); crGlyph = RGB(255, 255, 255); fFill = TRUE; }
    }
    else
    {
        if (fPressed) { crFill = fDark ? RGB(0x50, 0x50, 0x50) : RGB(0xCC, 0xCC, 0xCC); fFill = TRUE; }
        else if (fHot) { crFill = fDark ? RGB(0x3D, 0x3D, 0x3D) : RGB(0xE9, 0xE9, 0xE9); fFill = TRUE; }
    }

    rf.left   = (FLOAT)prc->left;
    rf.top    = (FLOAT)prc->top;
    rf.right  = (FLOAT)prc->right;
    rf.bottom = (FLOAT)prc->bottom;
    if (fFill)
    {
        cf = DwfColor(crFill);
        pb = NULL;
        (void)CCALL(pDC, CreateSolidColorBrush, &cf, NULL, &pb);
        if (pb)
        {
            CCALL(pDC, FillRectangle, &rf, (ID2D1Brush*)pb);
            DwfRelease((IUnknown**)&pb);
        }
    }
    cf = DwfColor(crGlyph);
    pb = NULL;
    (void)CCALL(pDC, CreateSolidColorBrush, &cf, NULL, &pb);
    if (pb)
    {
        DwfDrawGlyph(pDC, (ID2D1Brush*)pb, prc, glyph);
        DwfRelease((IUnknown**)&pb);
    }
}

/* ---- surface (re)creation ----------------------------------------------------------------------- */

static DECLSPEC_NOINLINE BOOL DwfCreateSurface(UINT cx, UINT cy)
{
    DwfRelease((IUnknown**)&g_dwf.pSurface);
    (void)CCALL(g_dwf.pDComp, CreateSurface, cx, cy,
                DXGI_FORMAT_B8G8R8A8_UNORM, DWF_ALPHA_MODE_PREMULTIPLIED, &g_dwf.pSurface);
    if (!g_dwf.pSurface)
    {
        return FALSE;
    }
    (void)CCALL(g_dwf.pVisual, SetContent, (IUnknown*)g_dwf.pSurface);
    g_dwf.cxSurface = cx;
    g_dwf.cySurface = cy;
    return TRUE;
}

/* Re-enable the DWM-drawn frame (shadow, rounded corners, border, extended bounds) that NC removal stripped.
   Idempotent; safe to call repeatedly. dwmapi is loaded on first use. */
static DECLSPEC_NOINLINE void DwfApplyDwmFrame(HWND hwnd)
{
    union { FARPROC fp; PFN_DWF_EXTEND ex; PFN_DWF_SETATTR sa; } u;
    DWF_MARGINS m;
    UINT        corner;

    if (!g_dwfDwmapi)
    {
        g_dwfDwmapi = LoadLibraryW(L"dwmapi.dll");
        if (g_dwfDwmapi)
        {
            u.fp = GetProcAddress(g_dwfDwmapi, "DwmExtendFrameIntoClientArea"); g_dwfExtend  = u.ex;
            u.fp = GetProcAddress(g_dwfDwmapi, "DwmSetWindowAttribute");        g_dwfSetAttr = u.sa;
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

BOOL WINAPI DwmFrameInit(HWND hwnd)
{
    UINT flags;
    UINT cx;
    UINT cy;

    if (g_dwf.fActive && (g_dwf.hwnd == hwnd))
    {
        return TRUE;
    }
    DwmFrameDestroy(hwnd);
    SecureZeroMemory(&g_dwf, sizeof(g_dwf));
    g_dwf.hwnd = hwnd;

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
        /* DComp device built ON the D2D device, so its surfaces back BeginDraw(ID2D1DeviceContext). */
        (void)DCompositionCreateDevice2((IUnknown*)g_dwf.pD2DDevice, &DWF_IID_IDCompositionDesktopDevice,
                                        (void**)&g_dwf.pDComp);
    }
    if (g_dwf.pDComp)
    {
        (void)CCALL(g_dwf.pDComp, CreateTargetForHwnd, hwnd, TRUE, &g_dwf.pTarget);
        (void)CCALL(g_dwf.pDComp, CreateVisual, &g_dwf.pVisual);
    }
    if (!g_dwf.pD2DContext || !g_dwf.pTarget || !g_dwf.pVisual)
    {
        DwmFrameDestroy(hwnd);
        return FALSE;
    }

    DwfClientSize(hwnd, &cx, &cy);
    if (!DwfCreateSurface(cx, cy))
    {
        DwmFrameDestroy(hwnd);
        return FALSE;
    }

    /* DWrite for the caption title (non-fatal if it fails -- the bar still composites). */
    (void)DWriteCreateFactory(DWF_FACTORY_TYPE_SHARED, &DWF_IID_IDWriteFactory, (IUnknown**)&g_dwf.pDWrite);
    DwfCreateTextFormat();
    DwfCreateIconFormat();

    (void)CCALL(g_dwf.pTarget, SetRoot, g_dwf.pVisual);
    (void)CCALL0(g_dwf.pDComp, Commit);

    /* Restore the DWM-drawn frame (shadow / rounded corners / border / extended bounds). */
    DwfApplyDwmFrame(hwnd);

    g_dwf.fActive = TRUE;
    return TRUE;
}

void WINAPI DwmFrameRender(HWND hwnd, BOOL fDark)
{
    ID2D1DeviceContext*   pDC;
    ID2D1SolidColorBrush* pBrush;
    POINT                 offset;
    D2D1_COLOR_F          col;
    D2D1_COLOR_F          crText;
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

    /* Keep the surface matched to the CURRENT client size every render -- otherwise an init/stale-size
       surface renders the caption + button positions for the wrong size (the "applies only after resize"
       symptom). */
    {
        UINT cx;
        UINT cy;

        DwfClientSize(hwnd, &cx, &cy);
        if (!g_dwf.pSurface || (cx != g_dwf.cxSurface) || (cy != g_dwf.cySurface))
        {
            if (!DwfCreateSurface(cx, cy))
            {
                return;
            }
        }
    }

    pDC = NULL;
    offset.x = 0;
    offset.y = 0;
    (void)CCALL(g_dwf.pSurface, BeginDraw, NULL, &DWF_IID_ID2D1DeviceContext, (void**)&pDC, &offset);
    if (!pDC)
    {
        return;
    }

    /* BeginDraw hands back a sub-rect of an atlas surface at 'offset'; translate so (0,0) is our origin. */
    xform._11 = 1.0f; xform._12 = 0.0f;
    xform._21 = 0.0f; xform._22 = 1.0f;
    xform._31 = (FLOAT)offset.x;
    xform._32 = (FLOAT)offset.y;
    CCALL(pDC, SetTransform, &xform);

    fActive      = (GetActiveWindow() == hwnd);
    g_dwf.fDark  = fDark;   /* remembered so hover/press re-renders use the same shade */
    capH         = (int)DwfCaptionHeight(hwnd);

    /* Whole window = client color; caption band painted on top. */
    col = DwfClientColor(fDark);
    CCALL(pDC, Clear, &col);
    {
        ID2D1SolidColorBrush* pCap;
        D2D1_RECT_F           rcCap;

        col  = DwfCaptionColor(fDark, fActive);
        pCap = NULL;
        (void)CCALL(pDC, CreateSolidColorBrush, &col, NULL, &pCap);
        if (pCap)
        {
            rcCap.left   = 0.0f;
            rcCap.top    = 0.0f;
            rcCap.right  = (FLOAT)g_dwf.cxSurface;
            rcCap.bottom = (FLOAT)capH;
            CCALL(pDC, FillRectangle, &rcCap, (ID2D1Brush*)pCap);
            DwfRelease((IUnknown**)&pCap);
        }
    }

    /* Caption title (DWrite, caption font). cch is not range-checked before the draw (szTitle[0] gates),
       so no Spectre index reaches the DrawText call. */
    szTitle[0] = 0;
    cch = GetWindowTextW(hwnd, szTitle, ARRAYSIZE(szTitle));
    if (g_dwf.pTextFormat && szTitle[0])
    {
        crText = DwfColor(fDark ? RGB(255, 255, 255) : RGB(0, 0, 0));
        if (!fActive)
        {
            crText.a = 0.60f;
        }
        pBrush = NULL;
        (void)CCALL(pDC, CreateSolidColorBrush, &crText, NULL, &pBrush);
        if (pBrush)
        {
            leftPad       = GetSystemMetrics(SM_CXSMICON) + GetSystemMetrics(SM_CXFRAME) * 2;
            rcText.left   = (FLOAT)leftPad;
            rcText.top    = 0.0f;
            rcText.right  = (FLOAT)g_dwf.cxSurface;
            rcText.bottom = (FLOAT)capH;
            CCALL(pDC, DrawText, szTitle, (UINT32)cch, g_dwf.pTextFormat, &rcText,
                  (ID2D1Brush*)pBrush, D2D1_DRAW_TEXT_OPTIONS_NONE, DWF_MEASURING_MODE_NATURAL);
            DwfRelease((IUnknown**)&pBrush);
        }
    }

    /* Caption buttons: light/dark (sun/moon), Minimize, Maximize/Restore, Close. */
    {
        RECT rcClose;
        RECT rcMax;
        RECT rcMin;
        RECT rcLD;

        if (DwfButtonRects(hwnd, &rcClose, &rcMax, &rcMin, &rcLD))
        {
            DwfDrawButton(pDC, &rcLD,    DWB_LIGHTDARK, (WCHAR)(fDark ? 0xE706 : 0xE708), fDark, fActive);
            DwfDrawButton(pDC, &rcMin,   DWB_MIN,       (WCHAR)0xE921,                    fDark, fActive);
            DwfDrawButton(pDC, &rcMax,   DWB_MAX,       (WCHAR)(IsZoomed(hwnd) ? 0xE923 : 0xE922), fDark, fActive);
            DwfDrawButton(pDC, &rcClose, DWB_CLOSE,     (WCHAR)0xE8BB,                    fDark, fActive);
        }
    }

    (void)CCALL0(g_dwf.pSurface, EndDraw);
    (void)CCALL0(g_dwf.pDComp, Commit);
}

void WINAPI DwmFrameResize(HWND hwnd)
{
    UINT cx;
    UINT cy;

    if (!g_dwf.fActive || (g_dwf.hwnd != hwnd))
    {
        return;
    }
    DwfClientSize(hwnd, &cx, &cy);
    if ((cx == g_dwf.cxSurface) && (cy == g_dwf.cySurface))
    {
        return;
    }
    if (DwfCreateSurface(cx, cy))
    {
        (void)CCALL0(g_dwf.pDComp, Commit);
    }
}

DECLSPEC_NOINLINE void WINAPI DwmFrameDestroy(HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);
    DwfRelease((IUnknown**)&g_dwf.pTextFormat);
    DwfRelease((IUnknown**)&g_dwf.pIconFormat);
    DwfRelease((IUnknown**)&g_dwf.pDWrite);
    DwfRelease((IUnknown**)&g_dwf.pSurface);
    DwfRelease((IUnknown**)&g_dwf.pVisual);
    DwfRelease((IUnknown**)&g_dwf.pTarget);
    DwfRelease((IUnknown**)&g_dwf.pDComp);
    DwfRelease((IUnknown**)&g_dwf.pD2DContext);
    DwfRelease((IUnknown**)&g_dwf.pD2DDevice);
    DwfRelease((IUnknown**)&g_dwf.pD2DFactory);
    DwfRelease((IUnknown**)&g_dwf.pDxgiDevice);
    DwfRelease((IUnknown**)&g_dwf.pD3DDevice);
    g_dwf.fActive   = FALSE;
    g_dwf.cxSurface = 0u;
    g_dwf.cySurface = 0u;
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

BOOL WINAPI DwmFrameHandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                  void (WINAPI* pfnToggle)(HWND), LRESULT* plr)
{
    int   id;
    POINT pt;

    if (!plr || !DwmFrameActive(hwnd))
    {
        return FALSE;
    }
    switch (uMsg)
    {
        case WM_NCCALCSIZE:
            if (wParam)
            {
                if (IsZoomed(hwnd))
                {
                    NCCALCSIZE_PARAMS* p = (NCCALCSIZE_PARAMS*)lParam;
                    UINT dpi = DwfDpi(hwnd);
                    int  pad = DwfMetric(SM_CXPADDEDBORDER, dpi);
                    int  cx  = DwfMetric(SM_CXFRAME, dpi) + pad;
                    int  cy  = DwfMetric(SM_CYFRAME, dpi) + pad;
                    p->rgrc[0].left   += cx;
                    p->rgrc[0].top    += cy;
                    p->rgrc[0].right  -= cx;
                    p->rgrc[0].bottom -= cy;
                }
                *plr = 0;
                return TRUE;
            }
            return FALSE;

        case WM_NCHITTEST:
            *plr = DwfHitTest(hwnd, lParam);
            return TRUE;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            (void)BeginPaint(hwnd, &ps);
            DwmFrameRender(hwnd, g_dwf.fDark);
            (void)EndPaint(hwnd, &ps);
            *plr = 0;
            return TRUE;
        }

        case WM_NCPAINT:
            *plr = 0;
            return TRUE;

        case WM_NCACTIVATE:
            DwmFrameRender(hwnd, g_dwf.fDark);
            *plr = (LRESULT)TRUE;
            return TRUE;

        case WM_ACTIVATE:
            DwmFrameRender(hwnd, g_dwf.fDark);
            return FALSE;

        case WM_NCMOUSEMOVE:
            id = DwfButtonFromHit(wParam);
            DwfTrackLeave(hwnd);
            if (g_dwf.idHot != id)
            {
                g_dwf.idHot = id;
                DwmFrameRender(hwnd, g_dwf.fDark);
            }
            return FALSE;

        case WM_NCMOUSELEAVE:
            g_dwf.fTracking = FALSE;
            if (g_dwf.idHot != DWB_NONE)
            {
                g_dwf.idHot = DWB_NONE;
                DwmFrameRender(hwnd, g_dwf.fDark);
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
                DwmFrameRender(hwnd, g_dwf.fDark);
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
                    DwmFrameRender(hwnd, g_dwf.fDark);
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
                DwmFrameRender(hwnd, g_dwf.fDark);
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
                DwmFrameRender(hwnd, g_dwf.fDark);
            }
            return FALSE;

        default:
            return FALSE;
    }
}

DECLSPEC_NOINLINE BOOL WINAPI DwmFrameActive(HWND hwnd)
{
    return g_dwf.fActive && (g_dwf.hwnd == hwnd);
}

/* Seed the caption shade without painting. At creation we publish the frame change (SWP_FRAMECHANGED) and
   invalidate the whole window once; the resulting WM_PAINT renders with g_dwf.fDark, so it must already
   hold the app's theme -- otherwise the first paint is light in dark mode. */
DECLSPEC_NOINLINE void WINAPI DwmFrameSetDark(HWND hwnd, BOOL fDark)
{
    if (g_dwf.fActive && (g_dwf.hwnd == hwnd))
    {
        g_dwf.fDark = fDark;
    }
}
