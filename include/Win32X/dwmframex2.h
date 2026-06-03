/*
 * dwmframex2.h -- side-by-side V2 of the in-process caption compositor.
 *
 * Same public surface as dwmframex.h, symbols suffixed "2", so both implementations can live in one
 * static lib and be A/B compared. V2 is the ImmersiveWindow-style FLIP-MODEL COMPOSITION SWAPCHAIN port
 * (CreateSwapChainForComposition + persistent D2D target + Present, monitor-sized buffer never resized,
 * vblank-paced) -- see dwmframex-immersivewindow-port-plan.md. V1 (dwmframex.h) is the original
 * IDCompositionSurface-per-resize compositor.
 */
#ifndef DWMFRAMEX2_H
#define DWMFRAMEX2_H

#include "Win32X/dwmframex.h"   /* shared macros: DWMFRAME_BACKDROP_*, HTLIGHTDARKBTN */

#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI DwmFrameInit2(HWND hwnd);
void WINAPI DwmFrameRender2(HWND hwnd, BOOL fDark);
void WINAPI DwmFrameResize2(HWND hwnd);
void WINAPI DwmFrameDestroy2(HWND hwnd);
BOOL WINAPI DwmFrameActive2(HWND hwnd);
BOOL WINAPI DwmFrameIsBusy2(HWND hwnd);   /* V2: TRUE while frames remain (anim/hover/size) -> loop keeps rendering */
void WINAPI DwmFrameWaitFrame2(HWND hwnd); /* V2: block until swapchain ready (~vblank); call before render in the loop */
void WINAPI DwmFrameSetDark2(HWND hwnd, BOOL fDark);
void WINAPI DwmFrameAnimateTheme2(HWND hwnd, BOOL fDark);
void WINAPI DwmFrameSetBackdrop2(HWND hwnd, int iType);
BOOL WINAPI DwmFrameHandleMessage2(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                   void (WINAPI* pfnToggle)(HWND), LRESULT* plr);

#ifdef __cplusplus
}
#endif

#endif /* DWMFRAMEX2_H */
