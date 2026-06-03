---
type: repository-research + surgical-plan
repository: C:\dev\Win32X (Win32X / dwmframex.c in-process DWM caption compositor)
goal: flicker-free resize + exactly-native-refresh FPS, surgically
status: complete
---

# dwmframex.c — Flicker-Free Resize + Native-Refresh FPS: Surgical Plan

## 0. Premise correction (read first)

**There is no swapchain in this repo.** Repo-wide search: zero `CreateSwapChain*`,
zero `Present`/`Present1`, zero `ResizeBuffers`, zero `GetFrameLatencyWaitableObject`,
zero `DwmFlush` in any `.c/.h/.cpp`. The presentation primitive is an
`IDCompositionSurface` (`src\dwmframex.c:581-594`, `DwfCreateSurface`) set as the content
of a single `IDCompositionVisual` on an `IDCompositionTarget` for the hwnd, drawn with
Direct2D via the surface's own `BeginDraw`/`EndDraw` (`:903`, `:1035`) and published to
DWM with `IDCompositionDesktopDevice::Commit` (`:1036`). The device is `D3D11CreateDevice`
with **no** swapchain out-params (`:646-654`).

So "surgically update the existing swapchain" has no referent. The two goals are
achievable on the **surface model** without introducing a swapchain — and doing so is
*more* surgical than a swapchain rewrite. A swapchain rewrite is the wrong move here
(see §5). Confidence: high.

---

## 1. Current resize path and the exact flicker source

`WM_SIZE` (example `main.c:248-251`):
```c
case WM_SIZE:
    pfnAppDwmFrameResize(hwnd);   /* releases + recreates surface, Commits EMPTY */
    pfnAppDwmFrameRender(hwnd, g_fDark);  /* draws, Commits AGAIN */
    break;
```

`DwmFrameResize` (`dwmframex.c:1083-1101`):
```c
DwfClientSize(hwnd, &cx, &cy);
if ((cx == g_dwf.cxSurface) && (cy == g_dwf.cySurface)) return;
if (DwfCreateSurface(cx, cy))          /* DwfRelease(old); CreateSurface(new); SetContent(new) */
    (void)CCALL0(g_dwf.pDComp, Commit);   /* <-- LINE 1099: publishes the UNDRAWN surface */
```

**The flicker is line 1099.** `DwfCreateSurface` releases the old surface and
`SetContent`s a brand-new, **undrawn** one (`:583-590`). The `Commit` at `:1099` then
publishes that empty surface to DWM as a complete frame. Only afterward does
`DwmFrameRender` draw into it and `Commit` again (`:1036`). Between those two commits DWM
can — and on ~resize does — composite the empty/garbage surface = the flash.

Aggravators:
- `CS_HREDRAW | CS_VREDRAW` (`main.c:102`) forces a full invalidate every size delta →
  a `WM_SIZE` + `WM_PAINT` per drag pixel → the release/recreate/empty-commit churn fires
  continuously during a drag.
- Surface is **released and reallocated on every size step** — GPU alloc + `SetContent`
  churn, independent of the flash.

This is the structural analogue of the ImmersiveWindow "unbound D2D target on resize"
hazard, except here the gap is published explicitly by an extra `Commit` rather than by
a `ResizeBuffers`.

---

## 2. Why DirectComposition makes the fix trivial (doc-grounded)

DComp commits are **atomic per frame**:

> "All changes within a single commit are guaranteed to be applied to a single frame."
> — learn.microsoft.com/windows/win32/directcomp/basic-concepts, *Transactional composition*

> "Remember that all changes are applied to the visual at once within the context of the
> same frame. This means that, from the user's perspective, the changes to the visual
> occur instantaneously."
> — same page

> "The composition engine does not reflect any changes that the application makes to the
> visual tree until the application calls Commit, at which point all changes since the
> last Commit are processed as a single transaction."
> — architecture-and-components, *Composition engine*

**Implication:** if the surface release + recreate + `SetContent` + full redraw all land
in **one** Commit, DWM never sees an intermediate empty surface. The flash exists only
because there are currently **two** commits per resize. Collapse to one → flash gone, by
the atomicity guarantee. No swapchain, no `ResizeBuffers`, no double-buffer needed.

---

## 3. The surgical changes

### Fix 1a — minimal: collapse resize to one atomic commit (1 line)

Delete the premature commit so the recreate is published only by the subsequent render's
single `Commit`.

`dwmframex.c:1096-1100` → replace:
```c
    if (DwfCreateSurface(cx, cy))
    {
        (void)CCALL0(g_dwf.pDComp, Commit);   /* DELETE this Commit */
    }
```
with:
```c
    (void)DwfCreateSurface(cx, cy);   /* recreate only; the following DwmFrameRender
                                         draws into it and Commits atomically (one frame) */
```
Flow after the edit: `WM_SIZE` → `DwmFrameResize` recreates the surface and updates
`cxSurface/cySurface` (no commit) → `DwmFrameRender`'s size guard (`:891`) now matches, so
it skips its own recreate, draws into the new surface, `EndDraw` + **single** `Commit`
(`:1035-1036`). DWM only ever sees the fully-drawn new-size surface.
Blast radius: one site. Confidence the flash disappears: high.

**Residual after 1a:** still releasing+reallocating the surface every size step (alloc
churn / possible stall on fast drags). Fix 1b removes that too.

### Fix 1b — robust: oversize the surface once, never realloc on resize (ImmersiveWindow parity)

Mirror ImmersiveWindow's "desktop-sized buffer, never `ResizeBuffers`" move, adapted to a
DComp surface. Allocate the surface once at the **current monitor's** pixel size; on
resize, redraw the client sub-rect and clip — never recreate.

Why it is safe: the visual lives on a `CreateTargetForHwnd` target, so content past the
window client is clipped to the window by DWM automatically — an oversized surface's
bottom-right excess is simply not shown.

Required edits (medium, but localized to `dwmframex.c`):

1. **Split the coupled size field.** Today `cxSurface/cySurface` means both *allocation
   size* and *layout size*. Introduce `cxClient/cyClient` (layout) distinct from
   `cxSurface/cySurface` (allocation). Audit every read of `cxSurface/cySurface` used for
   **layout** — at least `:957` (`rcCap.right`) and `:1010` (`rcText.right`), plus the
   button-rect math — and point those at `cxClient`.
2. **Allocate once at monitor max.** At init (`:688-689`) compute the window's monitor
   pixel size (`MonitorFromWindow` + `GetMonitorInfo`, or `GetSystemMetrics(SM_CXSCREEN/
   SM_CYSCREEN)` for the primary) and `DwfCreateSurface(maxCx, maxCy)` once.
3. **`DwmFrameResize` stops reallocating** — set `cxClient/cyClient` and trigger a render;
   recreate the surface *only* if the new client exceeds the current surface allocation
   (i.e. a monitor change), e.g.:
   ```c
   DwfClientSize(hwnd, &cx, &cy);
   if (cx == g_dwf.cxClient && cy == g_dwf.cyClient) return;
   g_dwf.cxClient = cx; g_dwf.cyClient = cy;
   if (cx > g_dwf.cxSurface || cy > g_dwf.cySurface)
       (void)DwfGrowSurface(...);   /* rare: only when client exceeds the allocation */
   /* no Commit here — the render does it */
   ```
4. **`DwmFrameRender`** draws only the client region: pass the client rect as the
   `BeginDraw` update rect (`:903`, currently `NULL` = whole surface) so you redraw just
   the live area, and use `cxClient/cyClient` for all caption/button layout. Drop its
   own size-mismatch recreate (`:891-897`) — it becomes "grow only."

Result: resize never frees the surface → no realloc gap, no alloc churn, atomic single
commit per frame. This is the true flicker-free + smooth-resize state.

> "The biggest difference between bitblt and flip presentation models is how back-buffer
> contents get to the … DWM … the DWM can compose straight from those back buffers without
> any additional copy operations." — for-best-performance--use-dxgi-flip-model.
> The DComp surface model already gives you this (no redirection bitmap, premultiplied,
> `WS_EX_NOREDIRECTIONBITMAP` is set at `main.c:122`). Keeping the surface bound across
> resize is the surface-model equivalent of "don't `ResizeBuffers`."

### Fix 2 — exactly native-refresh FPS via DwmFlush pacing (no refresh-rate query needed)

The current animation clock is `WM_TIMER` at 8 ms (`DWF_ANIM_INTERVAL`, `:298`), which
the OS coalesces to ~15.6 ms and which has **no** relationship to the panel refresh.

For a DComp/no-swapchain app the vblank gate is **`DwmFlush`**: it blocks until the
composition engine has produced the next frame, and the engine runs exactly once per
vertical blank —

> "The composition engine produces one frame for each vertical blank in the display. The
> frame is started at a vertical blank and targets the subsequent vertical blank."
> — architecture-and-components, *Composition engine*

> "Composition frames are scheduled to always start at a vertical blank."
> — same

So a `{ advance; DwmFrameRender (draw+Commit); DwmFlush }` loop emits **exactly one frame
per vblank = native refresh**, on any panel (60/120/144), **without querying the refresh
rate at all** — `DwmFlush` self-paces to the actual hardware. (The repo's existing DXGI
refresh-enumeration in `tests\tests.inl` is therefore unnecessary for this.)

Surgical shape — replace the coalesced timer with a paced loop during active periods only
(animation live OR in a size/move modal loop). Pseudocode for the active driver:
```c
/* entered when an animation starts or on WM_ENTERSIZEMOVE; left when fAnim ends / WM_EXITSIZEMOVE */
LARGE_INTEGER prev = now();
while (g_dwf.fAnim || g_dwf.fInSizeMove)
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    double dt = seconds_since(prev); prev = now();
    DwfAdvanceAnimation(dt);          /* time-based, not per-tick-count */
    DwmFrameRender(hwnd, g_dwf.fDark); /* draw + Commit */
    DwmFlush();                        /* block until composited ~ next vblank => native refresh */
}
```
This holds exactly native refresh while animating/resizing, and falls back to fully
event-driven (zero idle frames) when nothing is changing — you do **not** want to burn
native-refresh frames forever on a static caption.

Minimal alternative if you keep the `WM_TIMER` skeleton: it cannot hit "exactly native
refresh" because USER-timer granularity caps you below it. To get there you must either
the paced loop above, a multimedia/`CreateWaitableTimerEx` high-res timer, or a dedicated
render thread. The paced loop is the smallest correct change.

---

## 4. Tensions / red lines (must decide before coding)

1. **Coupled size field (state ownership).** Fix 1b only works if you split allocation
   size from layout size. Every `cxSurface/cySurface` read used for **layout** must move
   to `cxClient/cyClient`, or the caption/buttons render for the wrong size. Audit sites:
   `:957`, `:1010`, all button-rect math, and the `DwfClientSize` callers. Miss one →
   the exact "applies only after resize" bug the comment at `:883-885` already fights.
2. **DwmFlush blocks the UI thread (~1 vblank/frame).** In the paced loop with
   `PeekMessage` interleaved this is fine. Do **not** sprinkle `DwmFlush` into the
   per-`WM_TIMER` tick — it would stall input. If UI-thread blocking is unacceptable,
   the correct (non-surgical) answer is a dedicated render thread owning the DComp
   surface + `DwmFlush`; defer unless required.
3. **DComp gives no present-completion signal.** `Commit` is async; `DwmFlush` is the
   only sync. "Exactly native refresh" holds **only while you actively pace** — DComp
   will not self-maintain a frame rate.
4. **No `WM_DPICHANGED` handler exists.** Fix 1b's monitor-sized surface must be regrown
   on a monitor/DPI change; `DwmFlush` auto-adapts to the new refresh but the surface
   allocation and font metrics will not. Add a `WM_DPICHANGED` (and monitor-change) path
   that calls the grow + re-render, or the surface will be wrong-sized after a
   cross-monitor move. Out of the stated scope but a real gap.
5. **`CS_HREDRAW | CS_VREDRAW`** (`main.c:102`) double-drives resize repaint. Harmless
   after Fix 1a (every paint is atomic now) but wasteful; consider dropping it once the
   surface is no longer reallocated per step.

---

## 5. Why NOT switch to a swapchain (the counterargument)

A `CreateSwapChainForComposition` flip swapchain would also solve both goals, and it is
what ImmersiveWindow does. But here it is the **wrong** trade:
- It rewrites the entire render-target model: `IDCompositionSurface::BeginDraw` (which
  hands back a D2D `ID2D1DeviceContext` already bound to the surface, `:903`) would be
  replaced by `GetBuffer` → `CreateBitmapFromDxgiSurface` → `SetTarget` → `Present`, plus
  `ResizeBuffers` handling — i.e. the exact `ResizeBuffers` hazard surface you are trying
  to avoid, reintroduced.
- The DComp surface model already has the property the swapchain is prized for
  (atomic, no-copy, no redirection bitmap). The flash is **not** intrinsic to the surface
  model — it is one stray `Commit` (line 1099).
- Native-refresh pacing via `DwmFlush` needs no swapchain; the waitable-object machinery
  (which ImmersiveWindow set up and then left as a dead timeout-0 poll — see the
  flicker-free synthesis §5) would be net-new complexity for no gain here.

Recommendation: **Fix 1a now** (one line, removes the flash), then **Fix 1b** (oversize
surface, removes alloc churn and the realloc gap entirely), then **Fix 2** (DwmFlush
paced loop for exact native refresh during animation/resize). Keep the surface model.

---

## 6. Verified anchors

| Concern | Site |
|---|---|
| Surface create/recreate (the gap) | `dwmframex.c:581-594` `DwfCreateSurface` |
| Premature empty-surface Commit (the flash) | `dwmframex.c:1099` |
| Atomic draw+Commit (the good path) | `dwmframex.c:1035-1036` |
| Render-side recreate guard | `dwmframex.c:891-897` |
| Layout reads of alloc size (split these) | `dwmframex.c:957`, `:1010` + button rects |
| Coalesced animation timer (replace) | `dwmframex.c:298`, `:730/:738`, `WM_TIMER` `:1336-1368` |
| WM_SIZE routing | `examples\main.c:248-251` |
| WS_EX_NOREDIRECTIONBITMAP (already correct) | `examples\main.c:122` |
| CS_HREDRAW\|CS_VREDRAW (aggravator) | `examples\main.c:102` |
| No swapchain / device w/o swapchain | `dwmframex.c:646-654` |

Docs: directcomp/basic-concepts (Transactional composition), directcomp/architecture-and-components
(Composition engine), direct3ddxgi/for-best-performance--use-dxgi-flip-model. See
`immersivewindow-flicker-free-synthesis.md` in this repo for the full quote set.
