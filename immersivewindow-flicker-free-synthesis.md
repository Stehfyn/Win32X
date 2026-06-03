---
type: synthesis
subject: Why ImmersiveWindow renders flicker-free — implementation cross-referenced to Microsoft docs
repository: Stehfyn/BorderlessWindow32 (ImmersiveWindow subproject)
status: complete
---

# Why & How ImmersiveWindow Is Flicker-Free — Documentation-Grounded Synthesis

This file draws explicit lines from **what ImmersiveWindow's code does** (file:line in
`ImmersiveWindow\ImmersiveWindow.c`) to **the Microsoft documentation sentence that
explains why it works**. Every doc claim is a verbatim quote with URL.

---

## 0. The central thesis in one paragraph

ImmersiveWindow is flicker-free because it **removes every surface that could hold a
stale frame** and then **never does the one operation that creates a blank frame
during resize**. Concretely: (1) it kills the GDI redirection bitmap
(`WS_EX_NOREDIRECTIONBITMAP`), so DWM has no opaque copy to show; (2) it feeds DWM a
**composition swapchain** whose flip-model back buffers DWM composes from directly,
with **no extra copy**; (3) it allocates that back buffer at **full desktop resolution
with `DXGI_SCALING_STRETCH`**, so a window resize is just a DComp transform — it
**never calls `ResizeBuffers`**, therefore the D2D render target is **never unbound**,
therefore there is **no window of time in which a black/empty buffer can be presented**.
Everything else (double-present, D3DKMT vblank spin, `DwmFlush`, the move/size timer)
is redundant pacing insurance that pushes the residual resize-race failure below ~1%.

The subtle part — and the answer to the cited "compositor sync failure" passage — is
in §6: ImmersiveWindow **opts out of the DirectComposition animation/sampling model
entirely**, which is the model that has the documented sync-failure mode. So that
failure mode mostly cannot occur here by construction.

---

## 1. Kill the redirection bitmap → DWM has nothing stale to show

**Code:** Window created `WS_EX_NOREDIRECTIONBITMAP` (`ImmersiveWindowDemo\demo.c:202`,
`ImmersizeImGuiDemo\demo.cpp:51`). Swapchain via `CreateSwapChainForComposition`
(`ImmersiveWindow.c:898`), bound to a DComp visual `SetContent`→`SetRoot`→`Commit`
(`:989`–`:1007`).

**Why it works (docs):**

> "As of Windows 8, you can now create a top-level window and request that it be
> created without a redirection surface. … you need to use the
> WS_EX_NOREDIRECTIONBITMAP extended window style that tells the composition engine
> not to allocate a redirection surface for the window."
> — Kenny Kerr, *High-Performance Window Layering Using the Windows Composition Engine*,
> learn.microsoft.com/archive/msdn-magazine/2014/june/…

> "In this case, the window has no redirection surface, so the DXGI factory's
> CreateSwapChainForHwnd method can't be used. … That's what the DXGI factory's
> CreateSwapChainForComposition method is for. … presenting this swap chain doesn't
> copy the bits to the redirection surface (which doesn't exist), but instead makes
> it available to the composition engine directly."
> — same article

> "A video memory bitmap is not subject to tearing because the application can only
> read from the surfaces that DirectComposition textures from."
> — learn.microsoft.com/windows/win32/directcomp/bitmap-surfaces, *Video memory bitmaps*

**Line drawn:** No redirection surface = no opaque intermediate buffer that DWM can
display half-updated. This is the structural precondition; the `lpcsp->rgrc[1] =
lpcsp->rgrc[2]` "lie to dwm" in `OnNCCalcSize` (`:2505`) is the matching move that
stops DWM from blitting old client bits.

---

## 2. Flip-model composition swapchain → DWM composes with zero copy

**Code:** `DXGI_SWAP_CHAIN_DESC1` (`:766`–`:797`): `SwapEffect =
DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL` (`:777`), `AlphaMode = PREMULTIPLIED` (`:778`),
`Scaling = STRETCH` (`:779`), `Format = B8G8R8A8_UNORM`. D3D11 device created
`D3D11_CREATE_DEVICE_BGRA_SUPPORT` (`:842`–`:846`) for D2D interop.

**Why it works (docs):**

> "In the flip model … all back buffers are shared with the DWM. Therefore, the DWM
> can compose straight from those back buffers without any additional copy
> operations."
> — learn.microsoft.com/windows/win32/api/dxgi/ne-dxgi-dxgi_swap_effect, *Remarks*

> "Composition swap chains only support the flip-sequential swap effect. … In the
> flip model, all buffers are shared directly with the composition engine. The
> composition engine can then compose the desktop directly from the swap chain back
> buffer without additional copying."
> — Kenny Kerr article

> "You must specify the DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL value … because
> CreateSwapChainForComposition supports only flip presentation model." / "You must
> also specify the DXGI_SCALING_STRETCH value…"
> — learn.microsoft.com/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforcomposition

> "Premultiplied alpha values typically provide the best performance, and it's also
> the only option supported by the flip model."
> — Kenny Kerr article

**Line drawn:** ImmersiveWindow's exact desc fields are not stylistic — `FLIP_SEQUENTIAL`
+ `STRETCH` + `PREMULTIPLIED` are **API-mandated** for a composition swapchain, and
each one is the field the docs name. The payoff is the no-copy compose: the only
pixels on screen are the ones ImmersiveWindow just drew, presented by a flip, never
copied through a redirection blit that could tear.

---

## 3. Desktop-sized buffer + STRETCH → no `ResizeBuffers` → no black resize frame

**Code:** Back buffer allocated at `DESKTOPHORZRES × DESKTOPVERTRES` (`:782`–`:783`),
`Scaling = STRETCH` (`:779`). **No `ResizeBuffers`/`ResizeBuffers1` exists anywhere in
the tree** (exhaustive search). The D2D target bitmap is created once
(`GetBuffer`→`CreateBitmapFromDxgiSurface`→`SetTarget`, `:942`–`:963`) with
`D2D1_BITMAP_OPTIONS_TARGET | CANNOT_DRAW` (`:809`) and **never rebound** for a window
resize (only released at shutdown, `:1407`).

**Why this is the crux (docs):** The normal flip-model resize path is a minefield the
code deliberately never enters —

> "After Present calls, the back buffer needs to explicitly be re-bound to the D3D11
> immediate context before it can be used again."
> — learn.microsoft.com/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model

> "In contrast to some flags, this flag can't be added or removed using ResizeBuffers.
> DXGI returns an error code if this flag is set differently from when the swap chain
> was created."  (re: FRAME_LATENCY_WAITABLE_OBJECT)
> — learn.microsoft.com/windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains

> "When your app calls IDXGISwapChain1::Present1 with dirty rectangles … If you aren't
> completely re-rendering the whole area … you must copy some data from the previous
> fully coherent back buffer to the current, stale back buffer before you start
> rendering."
> — learn.microsoft.com/windows/win32/direct3ddxgi/dxgi-1-2-presentation-improvements

**Line drawn:** Every documented resize hazard — re-binding the back buffer, releasing
and recreating the D2D target (the classic source of a black flash), reconciling stale
rotated buffers, re-passing flags through `ResizeBuffers` — is **avoided by never
resizing the buffers at all**. The buffer is always desktop-sized; the DComp visual
`STRETCH`-composes it into whatever the current window rect is. There is no instant
when the render target is unbound, so there is no instant when an empty buffer can be
presented. This is the single most important reason resize is flicker-free, and it is
why ImmersiveWindow trades GPU memory (a full-desktop B8G8R8A8 buffer, ×5) for
correctness.

---

## 4. The present path: cancel stale frames, then align to vblank

**Code:** `EndImmersivePaint` (`:3588`) issues **two** presents:
1. `Present(0, ALLOW_TEARING | DO_NOT_WAIT | (fRestart*PRESENT_RESTART))` (`:3601`–`:3608`)
2. then either `Present(1, DO_NOT_SEQUENCE)` (sync-to-vblank, `:3616`–`:3620`) or
   `Present(0, ALLOW_TEARING | DO_NOT_WAIT)` (`:3625`–`:3629`).

**Why it works (docs):**

> "When you call IDXGISwapChain1::Present1 on a flip model swap chain … with 0
> specified in the SyncInterval parameter … the runtime not only presents the next
> frame instead of any previously queued frames, it also terminates any remaining
> time left on the previously queued frames."
> — ne-dxgi-dxgi_swap_effect, *Remarks*

> "DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING can enable even lower latency than the waitable
> object…" / "For an application using sync interval 0, we do not recommend switching
> to flip model unless … DXGI_FEATURE_PRESENT_ALLOW_TEARING [is supported]."
> — for-best-performance--use-dxgi-flip-model

**Line drawn:** Present #1 (sync 0 + `PRESENT_RESTART`) does exactly what the doc says —
slams the just-drawn frame to the front and **terminates any queued stale frame**
(critical during resize, where a buffered old-size frame is the thing that would
flicker). Present #2 (sync 1) then re-aligns the steady state to the vertical blank.
`ALLOW_TEARING` is legal because the swapchain is FLIP_* (doc requirement satisfied at
`:791`–`:793`). This double-present is belt-and-suspenders (see §7) but each half maps
to a documented behavior.

---

## 5. Pacing: ImmersiveWindow ignores the canonical waitable object and rolls its own

**Code:** `FRAME_LATENCY_WAITABLE_OBJECT` flag is set (`:791`) and
`GetFrameLatencyWaitableObject` is retrieved (`:1009`), but it is **waited on with
timeout 0** (`WaitForNextFrameResource`, `:3117`) — a non-blocking poll, not a gate.
`SetMaximumFrameLatency` is **commented out** (`:876`–`:881`). The real cadence comes
from kernel-mode `D3DKMTWaitForVerticalBlankEvent` + a `D3DKMTGetScanLine` spin
(`WaitForVerticalBlank`, `:1851`–`:1900`) plus `DwmFlush` in the consumer loops.

**What the canonical mechanism is (docs), i.e. what ImmersiveWindow chose *not* to use:**

> "For every frame it renders, the app should wait on this handle before starting any
> rendering operations. Note that this requirement includes the first frame…"
> — getframelatencywaitableobject

> "It's better to wait until the system is ready to accept a new frame, then render
> the frame based on current data and queue the frame immediately."
> — reduce-latency-with-dxgi-1-3-swap-chains

> "The maximum number of back buffer frames that will be queued for the swap chain.
> This value is 1 by default."
> — setmaximumframelatency

**Line drawn:** This is the clearest evidence for the user's intuition that
ImmersiveWindow **does more work than necessary**. The DXGI-blessed low-latency path is
"wait on the latency object before rendering, latency=1." ImmersiveWindow instead
hand-rolls vblank sync at the D3DKMT layer *and* presents at sync-interval-1 *and*
calls `DwmFlush` — three independent vblank-alignment mechanisms stacked in one
iteration, while the actual waitable object sits unused (timeout 0). Any **one** of the
three would pace the loop; the stack is harmless but redundant. The `BufferCount=5`
queue (`:786`) is similarly over-provisioned given `PRESENT_RESTART`/`DO_NOT_SEQUENCE`
cancel queued frames every present and no latency cap is set.

---

## 6. The cited "compositor sync failure" passage — and why it barely applies

The user cited this passage and observed failure is <1% on resize:

> "For example, at the beginning of the application batch … the application can query
> the composition engine to determine the exact presentation time of the next frame.
> … to determine whether the application can complete the current batch before the
> next vertical blank. Therefore, the application uses the frame presentation time as
> the sampling time for its own animations. **If the application determines that it is
> unlikely to complete its work in the current vertical blank, the application can use
> the subsequent frame time as the sampling time instead, using the frame rate
> information returned by the composition engine to compute that time.**"
> — learn.microsoft.com/windows/win32/directcomp/architecture-and-components,
>   §*Composition engine* (final paragraph)

Read it carefully: this failure mode is about **animations whose values the composition
engine samples at the estimated next-vblank presentation time**. It is the DirectComposition
*animation/sampling* model. The supporting machinery:

> "The composition engine produces one frame for each vertical blank in the display.
> The frame is started at a vertical blank and targets the subsequent vertical blank."

> "The composition engine publishes the frame presentation times and the current frame
> rate. … enables applications to estimate the presentation time for their own batches,
> which in turns enables animations to be synchronized."

> "The composition engine does not reflect any changes that the application makes to
> the visual tree until the application calls Commit, at which point all changes since
> the last Commit are processed as a single transaction."
> — all architecture-and-components, §*Composition engine*

**Why ImmersiveWindow is largely immune.** It **commits the visual tree exactly once**
(`:1003`–`:1007`) and **never again** — there is no per-frame `Commit`, no DComp
animation object, no engine-sampled property in the tree. Confirmed by the design and
by Kerr:

> "For this particular application, where the visual tree doesn't change, I only need
> to call Commit once at the beginning of the application and never again. I originally
> assumed the Commit method needed to be called after presenting the swap chain, but
> this isn't the case because swap chain presentation isn't synchronized with changes
> to the visual tree."
> — Kenny Kerr article

So **all motion in ImmersiveWindow is the swapchain content changing** (app-thread D2D
redraw + flip Present), **not** an engine-sampled animation on a committed batch. The
"did I finish my batch before the vblank the engine will sample at?" question — the
exact thing the cited passage is about — **does not arise**, because ImmersiveWindow
never asks the engine to sample anything time-dependent. The engine just composes a
static visual whose content buffer was flipped underneath it.

**What the residual <1% actually is.** It is **not** a DComp sampling miss. It is a
race between a *resize geometry change* (DWM finalizing the new window rect / the
STRETCH source-dest mapping) and a *present landing in that same frame*. ImmersiveWindow
closes that window with the synchronous, vblank-locked repaint **inside**
`OnNCCalcSize`:

```c
// ImmersiveWindow.c ~:2516–2535, only when !ri.fMoving (genuine resize)
WaitForNextFrameResource(hWnd);
if (pPreProc) pPreProc(hWnd);     // app redraws content at the new size
WaitForVerticalBlank(hWnd);       // D3DKMT vblank
BeginImmersivePaint(hWnd);
EndImmersivePaint(hWnd, 1, 0);    // fRestart=1 → PRESENT_RESTART cancels stale frame
if (gti.hwndMoveSize) DwmFlush(); // serialize to the compositor while in modal size loop
```

This forces a fresh, vblank-aligned, stale-frame-cancelling present **before DWM
completes the geometry change**, so the newly exposed region is already filled. Plus
`OnTimer(0x69)` (`:2880`) and `OnWindowPosChanged` (`:3068`) each repaint again. Three
overlapping repaint drivers per resize step is why the race almost never resolves to a
visible blank — and also why the user is right that it is over-built. The
`DwmFlush` here is doing the documented job of the cited passage *manually*: instead of
trusting the engine to sample at the right frame, it **blocks until the compositor has
actually presented**, eliminating the timing guess.

> "PresentRefreshCount is equal to SyncRefreshCount when the app presents on every
> vsync."  — dxgi-flip-model, *Frame synchronization* (the steady-state ImmersiveWindow
> drives the loop toward via `WaitForVerticalBlank` + sync-1 present + `DwmFlush`).

**Net:** ImmersiveWindow swaps the DComp animation/sampling contract (which owns the
documented sync-failure mode) for a swapchain-flip contract it paces itself. That
trade *eliminates* the cited failure class and replaces it with a much smaller
resize-geometry race, which the synchronous NCCALCSIZE repaint + `DwmFlush` drives
under ~1%.

---

## 7. Where ImmersiveWindow does more work than necessary (flagged, evidence-based)

| # | Over-work | Evidence | Why redundant |
|---|---|---|---|
| 1 | Two `Present` calls per frame | `:3601` + `:3616`/`:3625` | One flip suffices; #1's `PRESENT_RESTART` already cancels stale frames |
| 2 | `BufferCount=5` with **no** `SetMaximumFrameLatency` | `:786`, `:876`–881 | Deep queue is pointless when every present cancels the queue (`RESTART`/`DO_NOT_SEQUENCE`) and default latency is 1 anyway |
| 3 | Frame-latency waitable object polled with timeout **0** | `:3117` | The whole `FRAME_LATENCY_WAITABLE_OBJECT` flag (`:791`) provides no back-pressure; it is a dead gate |
| 4 | Three stacked vblank mechanisms | `WaitForVerticalBlank` `:1880` + sync-1 Present `:3619` + `DwmFlush` (consumers) | Any one serializes to vblank; all three is insurance |
| 5 | Three repaint drivers per resize | NCCALCSIZE `:2532`, timer `:2897`, WINDOWPOSCHANGED `:3075` | NCCALCSIZE already paints synchronously; the others re-present the same geometry |
| 6 | Per-frame `EnableBlurBehind` | `:3633` | Re-sets an unchanged DWM accent attribute every present |
| 7 | `WaitForVerticalBlank` first-call returns undefined / does not wait | `:1858`–1875 | Harmless first frame, but evidence the path is under-verified |

None of these are *load-bearing* for flicker-freedom. The load-bearing set is §1–§3
plus the `PRESENT_RESTART` half of §4 and the synchronous NCCALCSIZE repaint of §6.

---

## 8. Minimal flicker-free recipe (distilled, doc-justified)

1. **`WS_EX_NOREDIRECTIONBITMAP` + `CreateSwapChainForComposition` + one DComp visual,
   `Commit` once.** No redirection surface → DWM has no stale copy; static tree → no
   sampling-time failure mode. *(Kerr; architecture-and-components §Composition engine.)*
2. **`FLIP_SEQUENTIAL` + `STRETCH` + `PREMULTIPLIED` desc**, mandated by
   `CreateSwapChainForComposition`; DWM composes from the back buffer with no copy.
   *(createswapchainforcomposition; ne-dxgi-dxgi_swap_effect.)*
3. **Allocate the back buffer desktop-sized; never call `ResizeBuffers`; never rebind
   the D2D target.** Resize becomes a DComp transform, so no unbound-target black frame.
   *(Avoids every hazard in for-best-performance and dxgi-1-2-presentation-improvements.)*
4. **On resize, repaint synchronously inside `WM_NCCALCSIZE`**, vblank-aligned, with
   `PRESENT_RESTART` to cancel the queued stale frame, and `DwmFlush` to block until
   the compositor has presented — manually doing the "use the right frame time" job the
   cited passage describes. Gate on `!fMoving` so moves stay cheap.
5. **Pace with one vblank mechanism** (the waitable object the way the docs intend, or
   D3DKMT, or sync-1 present — not all three). ImmersiveWindow uses all three; you need
   one.

---

## Sources

Repo: `ImmersiveWindow\ImmersiveWindow.c`, `…\cdcomp.h`, demos `ImmersiveWindowDemo\demo.c`,
`ImmersizeImGuiDemo\demo.cpp` (clone at `C:\Users\stehf\AppData\Local\Temp\BorderlessWindow32`).

Docs (all learn.microsoft.com):
- /windows/win32/directcomp/architecture-and-components (§Composition engine — the cited passage)
- /windows/win32/directcomp/basic-concepts (Transactional composition)
- /windows/win32/directcomp/bitmap-surfaces
- /archive/msdn-magazine/2014/june/windows-with-c-high-performance-window-layering-using-the-windows-composition-engine (Kenny Kerr)
- /windows/win32/api/dxgi/ne-dxgi-dxgi_swap_effect
- /windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforcomposition
- /windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model
- /windows/win32/direct3ddxgi/dxgi-flip-model
- /windows/win32/direct3ddxgi/dxgi-1-2-presentation-improvements
- /windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-getframelatencywaitableobject
- /windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-setmaximumframelatency
- /windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains
- /windows/win32/api/dxgi/ne-dxgi-dxgi_swap_chain_flag (FRAME_LATENCY_WAITABLE_OBJECT, ALLOW_TEARING)
