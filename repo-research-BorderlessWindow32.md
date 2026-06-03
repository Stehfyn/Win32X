---
type: repository-research
repository: Stehfyn/BorderlessWindow32
focus: general — conceptual contribution & novel mechanisms
status: complete
---

# Repository Research: BorderlessWindow32

## Thesis

BorderlessWindow32 is a maximalist proof that a pixel-perfect, DWM-integrated,
DirectComposition-backed, dark-mode-aware "immersive" custom title-bar window can
be built in **pure C** with **zero C++ COM runtime**, **zero ATL/WIL/WTL object
layer**, and **zero documented-only API surface** — by hand-binding every COM
vtable slot it touches and reverse-binding undocumented `user32`/`uxtheme`/`gdi32`
exports by ordinal. It is less a "library" in the reusable-package sense than a
**reference implementation of a technique**: how to talk to the modern Windows
compositor (Direct2D, DirectWrite, DXGI immersive swapchains, DComp, D3DKMT) from
C at the metal, indexing function pointers out of interface vtables directly. The
README is empty by design — the contribution is the source, not the prose.

## Conversation Context

- **Domain/problem:** Custom-chrome ("borderless"/"immersive") top-level windows on
  Windows — drawing your own title bar + caption buttons while keeping native
  resize, snap, Aero Snap, shadow, and DWM frame behavior. The perennial Win32
  pain point that spawned a thousand Stack Overflow answers and the
  `WM_NCCALCSIZE` lore.
- **Conventional baseline:** Either (a) high-level frameworks (WPF
  `WindowChrome`, WinUI, Electron `frame:false`, Qt `FramelessWindowHint`) that
  hide the compositor behind a managed runtime, or (b) the canonical C++ samples
  (melak47/BorderlessWindow, Rectify11, the `DwmExtendFrameIntoClientArea` +
  `WM_NCCALCSIZE` recipe) that still lean on the C++ COM machinery and GDI.
- **Repo's intervention:** Strip *all* of that. No C++, no `<d2d1.h>` C++ classes,
  no `Microsoft::WRL`, no GDI for the caption. Bind COM as raw C vtable thunks,
  render the caption with Direct2D/DirectWrite onto a DXGI **immersive swapchain**
  composited via DirectComposition into a `WS_EX_NOREDIRECTIONBITMAP` window,
  pace frames against the actual scanout via D3DKMT, and reach undocumented
  immersive/dark-mode APIs by ordinal.
- **What the repo makes visible:** The *actual* shape of the modern Windows
  compositor stack once you remove every convenience layer — and how thin that
  stack really is when you call it directly. It also exposes the seam where
  Microsoft's own apps (the "immersive" / MDL2 caption-glyph world) live versus
  what the documented SDK exposes.

## Evidence Map

| Area | Files | Why it matters |
|---|---|---|
| Public control API | `BorderlessWindow32/BorderlessWindow32.h`, `ImmersiveWindow/ImmersiveWindow.h` | Models the lib as a *Win32 control* (`BORDERLESS_CLASS`, `BCS_*` styles, `Borderless_CreateEx` macro) — deliberately mimics `commctrl.h` conventions, even leaving `begin_r_commctrl`/`end_r_commctrl` resource-compiler markers in. |
| Window proc core | `BorderlessWindow32/BorderlessWindow32.c` (3487 LOC), `ImmersiveWindow/ImmersiveWindow.c` (3741 LOC) | The whole message pump: `OnNCCalcSize`, `OnNCHittest`, immersive paint, caption-button state machine. |
| Hand-bound COM-in-C | `cd2d.h` (4465 LOC, **2396 vtable thunks**), `cdwrite.h` (3636 LOC), `cdcomp.h`, `cd3dkmt.h` | The technique itself. Every D2D/DWrite/DComp/DXGI method is a `static inline` that casts `this->v->tbl[N]` to a function-pointer type and calls it. |
| Undocumented API bindings | `cdwm.h`, `cuxtheme.h`, `immersivedef.h` | `SetWindowCompositionAttribute` (acrylic/blur accent policy), `SetPreferredAppMode`/`RefreshImmersiveColorPolicyState` by ordinal (135/104), private NC hit-test + caption-button enums. |
| Frame pacing | `WaitForVerticalBlank` in `BorderlessWindow32.c:1630`, `cd3dkmt.h` | Direct kernel-mode-thunk vblank sync, bypassing `DwmFlush`/`IDXGIOutput::WaitForVBlank`. |
| Code-gen thunks | `AllocateThunks` / `Borderless_Thunk` macros, AeonProfiler dep | Uses ATL's `AtlThunk` runtime-codegen to bind per-instance callbacks without a hashmap or `GWLP_USERDATA` indirection. |
| Demos / intent | `main.c`, `ImmersizeImGuiDemo/demo.cpp`, `ImmersiveWindowDemo/demo.c`, `video.c` | Shows the endgame: host Dear ImGui / D3D11 / bicubic-upscaled video *inside* the immersive client area sharing the same swapchain. |

## Significant Identifiers

| Identifier | Kind | Location | Significance |
|---|---|---|---|
| `IDCompositionDevice_CreateVisual` (et al.) | `static inline` fn | `cdcomp.h:73` | Canonical example of the whole repo's move: `((HRESULT(WINAPI*)(...))this->v->tbl[7])(this,...)` — COM call by raw vtable index, no headers. Note the magic indices (2=Release, 3=Commit, 6/7=Create*) are hard-coded ABI knowledge. |
| `BeginImmersivePaint` / `EndImmersivePaint` | exported CALLBACK | `BorderlessWindow32.c:3144` / `:3342` | The repo's rendering verb pair. Names assert the thesis: painting *is* "immersive," i.e. you own the whole frame, caption included. `End` takes `fRestart`, `fVsync` — paint is a restartable, vblank-gated transaction, not a `WM_PAINT` reaction. |
| `IDXGIImmersiveSwapchain` / `DXGIImmersiveSwapchain` | struct typedef | `BorderlessWindow32.c:267` | The god-object: bundles D2D device context, DWrite font faces, MDL2 glyph-index tables, dark-mode flag, frame-latency waitable, brushes. The author's entire per-window render state in one struct stored in `cbWndExtra`. |
| `WaitForVerticalBlank` | EXTERN_C CALLBACK | `BorderlessWindow32.c:1630` | Opens a D3DKMT adapter from the window's HDC, waits the vblank event, then **spins on `D3DKMTGetScanLine` until out of vblank** — a hand-rolled present-pacing primitive most apps never touch. |
| `lie to dwm` | code comment | `BorderlessWindow32.c:2261` | `lpcsp->rgrc[1] = lpcsp->rgrc[2];` — the entire borderless trick in one assignment, honestly labeled. Names the deception that makes DWM keep the frame while the client eats the caption. |
| `MA_CLIENT`/`MA_NONCLIENT`, `CB_*`, `NCHIT_*` | private enums | `immersivedef.h` | Re-derived names mirroring Windows' internal immersive vocabulary (`CAPTIONBUTTON`, `MOUSEAREA`). Evidence the author reverse-engineered Microsoft's own mental model rather than inventing one. |
| `SetPreferredAppMode(PAM_ALLOWDARK)` | ordinal-bound fn | `cuxtheme.h` + `:1469` | Undocumented uxtheme ordinal #135. Names + enum (`PAM_FORCEDARK`...) reconstruct the private dark-mode contract apps like Explorer use. |
| `AllocateThunks` / `BCSTHUNK_NCHITPROC` | fn + window-long offset | `BorderlessWindow32.c:1505` | Per-window ATL thunks stored in window-long slots — runtime machine-code generation to bind callbacks, an exotic choice vs the usual subclass/userdata pattern. |
| `HwndServerWindow32` / `CreateWindowServer` / `ServerThread` | class + fns | `BorderlessWindow32.c:1577`, `.h` HWND-server section | A second subsystem: an async "HWND server" thread that owns window creation on behalf of callers (`WM_SERVERCREATEWINDOW`, marshalled `CREATEWINDOWPARAMS`). Encodes the rule that HWNDs belong to their creating thread, and routes around it. |
| `GetShit` / `SetShit` / `hol_up` / `what up ma thunka` | fns + comments | `.h` / `BorderlessWindow32.c:1397`, `:2873` | Placeholder/jocular names — marks this as a personal research sandbox, not a productized API. The *evolved* `ImmersiveWindow.h` renames these to `GetDeviceAndSwapchain` / `SetDeviceDrawCallback`, showing the maturation path. |

## Novel Snippets

### COM-in-C by raw vtable index (the core move)
- Location: `cdcomp.h:73`, scaled to 2396 entries in `cd2d.h`
- Mechanism:
  ```c
  static inline HRESULT IDCompositionDevice_CreateVisual(IDCompositionDevice* this, IDCompositionVisual** visual)
  { return ((HRESULT(WINAPI*)(IDCompositionDevice*, IDCompositionVisual**))this->v->tbl[7])(this, visual); }
  ```
  The interface is declared as `struct { struct { void* tbl[]; }*v; }` — i.e. just
  "a pointer to a pointer to a function-pointer array." Each method is a typed cast
  of the right slot.
- Why interesting: This is the *de-sugared* truth of COM that C++ and `<d2d1_1.h>`'s
  C macros (`lpVtbl->Method`) hide. The author skips even the generated C vtable
  structs and indexes by integer — accepting total ABI fragility (slot 7 must stay
  slot 7 across Windows versions) in exchange for needing *no SDK headers at all*
  for these interfaces.
- Transferable insight: You can bind any COM/WinRT interface from a freestanding C
  TU if you know the vtable layout; the SDK headers are a convenience, not a
  requirement. Also a cautionary tale about versioning brittleness.

### Branchless NC hit-test as bit-flag arithmetic
- Location: `BorderlessWindow32.c:2301` (`OnNCHittest`)
- Mechanism:
  ```c
  result = NCHIT_LEFT   * (cursor.x == CLAMP(cursor.x, rcWindow.left, rcWindow.left + ((border.cx + 5) * !fIsMaximized)))
         | NCHIT_TOP    * (cursor.y == CLAMP(cursor.y, rcWindow.top - 2, ...))
         | NCHIT_RIGHT  * (...) | NCHIT_BOTTOM * (...);
  switch (result) { case NCHIT_TOP|NCHIT_LEFT: return HTTOPLEFT; ... }
  ```
  Each edge test is `flag * (point == clamp(point, lo, hi))` — a predicate that is 0
  or the flag bit. OR them, then `switch` on the combined corner mask. `!fIsMaximized`
  arithmetically zeroes the resize border when maximized instead of branching.
- Why interesting: Replaces the usual nested `if`-ladder hit-test with a single
  branch-collapsed expression where corners *fall out for free* as bit unions. The
  `* !fIsMaximized` idiom is a recurring style — boolean-as-multiplier to elide
  conditionals.
- Transferable insight: Edge/corner classification (resize handles, 9-slice,
  collision quadrants) composes cleanly as OR-able flag bits when each axis test is
  independent.

### Self-priming vblank pacer via D3DKMT
- Location: `BorderlessWindow32.c:1630`
- Mechanism: `static D3DKMT_WAITFORVERTICALBLANKEVENT vbe = INIT_ONCE_STATIC_INIT;`
  — first call opens the adapter from `GetDC(hWnd)` and caches it in the static; every
  later call waits the vblank event then **busy-spins on `D3DKMTGetScanLine` until
  `!InVerticalBlank`**, returning right as scanout exits the blank.
- Why interesting: Lazy one-time init folded into the wait function via a static
  sentinel, and a precision present-timing primitive built straight on the
  kernel-mode thunk layer — below DXGI, below DWM. The spin guarantees you return at
  a *consistent phase* of the blank, not just "sometime during."
- Transferable insight: When you need sub-frame present timing on Windows and
  `DwmFlush`/`WaitForVBlank` are too coarse, D3DKMT scanline polling is the floor.

### The honest "lie to dwm"
- Location: `BorderlessWindow32.c:2252` (`OnNCCalcSize`)
- Mechanism: `lpcsp->rgrc[1] = lpcsp->rgrc[2]; /* lie to dwm */` then, when maximized,
  clamp client to the *monitor work area* (`MonitorWorkRectFromWindow`) so a
  maximized borderless window doesn't bleed under the taskbar — the bug that breaks
  most naive borderless implementations.
- Why interesting: Compresses the single most-asked-about Win32 borderless gotcha
  (maximized window covering the taskbar) into a two-line fix, and names the trick.
- Transferable insight: Borderless maximize must special-case the work-area rect;
  returning 0 from `WM_NCCALCSIZE` alone is insufficient.

### Per-window callbacks via runtime code-gen thunks
- Location: `AllocateThunks` `BorderlessWindow32.c:1505`, `Borderless_Thunk` macro in `.h`
- Mechanism: `SetWindowLongPtr(hwnd, BCSTHUNK_NCHITPROC, (LONG_PTR)AtlThunk_AllocateData())`
  allocates an ATL thunk (executable stub that injects a fixed first argument) per
  window, stored in a window-long slot, later invoked via `AtlThunk_DataToCode(...)`.
- Why interesting: Instead of the standard "stash `this` in `GWLP_USERDATA` and look
  it up," it generates a tiny machine-code trampoline per instance — the same trick
  ATL/MFC use internally to make member functions look like raw `WNDPROC`s, here
  applied in plain C.
- Transferable insight: `AtlThunk_*` is a usable C-callable facility for
  context-binding callbacks without a lookup table; rarely seen outside C++ ATL.

## Conceptual Moves

- **Headers as ABI ledgers, not API contracts.** `cd2d.h`/`cdwrite.h` aren't
  wrappers — they're a transcribed memory of vtable slot numbers. The repo treats
  the Windows binary interface as the real interface and the SDK as optional.
- **"Immersive" as the organizing metaphor.** Class names, the paint verbs, and the
  god-struct all use Microsoft's internal "immersive" vocabulary. The author
  deliberately aligns with the OS's own model of system-drawn chrome rather than the
  app-developer-facing one.
- **Control-ification of a window.** Packaging this as `BORDERLESS_CLASS` + `BCS_*`
  styles + `Borderless_CreateEx` (with leftover `r_commctrl` markers) frames a custom
  compositor window as if it were a `BUTTON` or `LISTVIEW` — a deliberate stylistic
  claim that this *should* be a first-class common control.
- **Render-as-transaction, not as-event.** `Begin/EndImmersivePaint(fRestart, fVsync)`
  plus explicit `WaitForVerticalBlank` replaces the reactive `WM_PAINT`/`InvalidateRect`
  model with an imperative, vblank-gated, restartable present loop driven from the
  message pump in `main.c`.
- **Threads own windows, so build a window server.** The `HwndServerWindow32`
  subsystem reifies the Win32 rule that an HWND is bound to its creating thread, then
  provides an async marshalling layer (`WM_SERVERCREATEWINDOW` + `CREATEWINDOWPARAMS`)
  to create windows off-thread — a structural answer to a threading invariant.
- **Evolution toward a real API.** `BorderlessWindow32` → `ImmersiveWindow` is a
  visible maturation: `GetShit`→`GetDeviceAndSwapchain`, `SetShit`→
  `SetDeviceDrawCallback`/`SetDevicePreDrawCallback`, plus `DEVICEDRAWPROC` callbacks
  and `WaitForNextFrameResource` — the sandbox hardening into an embeddable surface
  that hosts ImGui/D3D11/video (the demos).

## Tensions and Limits

- **ABI brittleness is total and unguarded.** Hard-coded vtable indices
  (`tbl[7]`, ordinal #135) assume a fixed binary layout. Microsoft can resequence a
  vtable or move an ordinal on any Windows update; there are no GUID checks, no
  feature detection, no fallback. This is research-grade, not ship-grade.
- **Undocumented surface.** `SetWindowCompositionAttribute`, `SetPreferredAppMode`,
  the D3DKMT private structs, immersive NC enums — all off-contract. Acceptable for
  a personal compositor study; disqualifying for anything that must survive Patch
  Tuesday unattended.
- **Not a packaged library.** Empty README, `GetShit`/`hol_up` placeholder names,
  vendored ImGui + AeonProfiler binaries, duplicated source trees between
  `BorderlessWindow32/` and `ImmersiveWindow/`. It's a workbench, and reads like one.
- **Correctness debt visible in-source.** `OnNCCalcSize` carries a `static int i`
  counter gating a paint-on-third-calc hack; `BeginImmersivePaint` is full of
  commented-out experiments and duplicated rects (`rcLightDarkBtn` vs
  `rcLightDarkkBtn`). Live R&D, not settled code.
- **What it refuses to solve:** portability (Win32-only, x64 ABI-specific),
  forward-compat, and packaging. The refusal reveals the goal was *understanding and
  control of the compositor*, not distribution.

## What To Learn From It

- **The COM-in-C vtable-thunk pattern** (`cd2d.h`) is the headline takeaway: a
  complete, copyable demonstration that any COM interface is callable from
  freestanding C via `this->v->tbl[N]` casts. Study `cdcomp.h` first (small), then
  see it scaled in `cd2d.h`.
- **The boolean-as-multiplier, OR-the-flags hit-test** (`OnNCHittest`) — a reusable
  idiom for branchless edge/corner classification.
- **D3DKMT scanline pacing** (`WaitForVerticalBlank`) — the sub-`DwmFlush` floor for
  present timing on Windows, rarely shown in full.
- **The borderless invariants, named:** `lie to dwm` (`rgrc[1]=rgrc[2]`) + work-area
  clamp on maximize. If you ever write a custom title bar, these two lines are the
  ones that matter.
- **AtlThunk for per-instance C callbacks** — context-binding without a userdata
  lookup, usable from plain C.
- **Vocabulary worth adopting:** the "immersive" framing, render-as-transaction
  (`Begin/End...Paint(fRestart, fVsync)`), and the HWND-server pattern for crossing
  the thread-owns-window boundary.
- **Warning to internalize:** every technique here trades forward-compatibility for
  directness. Copy the *ideas*; do not ship the *ordinals*.

## Ecosystem Note

This repo is the ancestor of the analyst's current working project (`C:\dev\Win32X`,
`dwmframex.c`, commits "in-process DWM caption compositor", "Mica/Acrylic backdrops",
"DWM caption crossfade") — same author (Stehfyn), same immersive-caption thesis,
evidently carried forward into a DWM-frame-extension line of work. BorderlessWindow32
is where the COM-in-C + immersive-paint vocabulary was first worked out.

## Sources Read

- `README.md` (empty)
- `BorderlessWindow32/main.c`
- `BorderlessWindow32/BorderlessWindow32.h`
- `BorderlessWindow32/BorderlessWindow32.c` (key ranges: 1455–1535, 1630–1683, 2252–2392, 2865–2960, 3144–3230)
- `BorderlessWindow32/cdcomp.h`, `cdwm.h`, `cd3dkmt.h`, `cuxtheme.h`, `immersivedef.h`
- `BorderlessWindow32/cd2d.h` (vtable-thunk pattern, sampled)
- `ImmersiveWindow/ImmersiveWindow.h` (evolved API surface)
- File/LOC census across the tree
