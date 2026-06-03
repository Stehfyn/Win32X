/*
 * dwmframex.h -- in-process reproduction of uDWM's caption compositor.
 *
 * uDWM draws the window caption with D3D11 -> Direct2D -> DirectComposition -> DWrite, as a visual tree
 * GPU-composited against the desktop (its import table: d2d1/d3d11/dcomp/dxgi/DWrite/dwmcore). An app
 * cannot inject into DWM's visual tree, but it CAN build its OWN DirectComposition target for its hwnd
 * (DCompositionCreateTargetForHwnd) and render the caption with the SAME stack -- which DWM then
 * composites identically. This module stands up that pipeline and renders the caption (bar + title +
 * the four caption buttons, the fourth being the light/dark toggle) the way uDWM does, driven by the
 * geometry/metrics/state reproduced from win32kfull + uDWM disassembly.
 *
 * Phase 1 (this file): device + DCompositionTarget(hwnd) + a composited D2D caption surface.
 */
#ifndef DWMFRAMEX_H
#define DWMFRAMEX_H

#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Stand up the GPU pipeline + DComp target for hwnd. Returns FALSE if the stack is unavailable
   (no D3D11 / pre-DComp OS); the caller should then fall back to the GDI caption path. */
BOOL WINAPI DwmFrameInit(HWND hwnd);

/* Render the caption (Phase 1: the caption bar fill at the current theme color) and Commit. */
void WINAPI DwmFrameRender(HWND hwnd, BOOL fDark);

/* Resize the composition surface to the window's current client width x caption height. */
void WINAPI DwmFrameResize(HWND hwnd);

/* Tear down the pipeline for hwnd. */
void WINAPI DwmFrameDestroy(HWND hwnd);

/* TRUE once DwmFrameInit has a live pipeline for hwnd. */
BOOL WINAPI DwmFrameActive(HWND hwnd);

/* Seed the caption shade (no paint). Call before publishing the frame change + the one whole-window
   invalidate at creation, so the first WM_PAINT renders in the correct theme. */
void WINAPI DwmFrameSetDark(HWND hwnd, BOOL fDark);

/* Non-client message handler reproducing DefWindowProc's frame: WM_NCCALCSIZE removes the standard NC so
   our DComp caption owns it; WM_NCHITTEST follows win32kfull!FindNCHit's region order; button hover/press
   + click (cancel-on-drag-off, SC_MINIMIZE/MAXIMIZE/RESTORE/CLOSE per xxxTrackCaptionButton; the light/
   dark button toggles via the supplied callback). Returns TRUE when it handled the message (fills *plr).
   pfnToggle is called when the light/dark button is clicked (may be NULL). */
BOOL WINAPI DwmFrameHandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                  void (WINAPI* pfnToggle)(HWND), LRESULT* plr);

/* Private WM_NCHITTEST result for the light/dark caption button. */
#define HTLIGHTDARKBTN  0x0000B002

#ifdef __cplusplus
}
#endif

#endif /* DWMFRAMEX_H */
