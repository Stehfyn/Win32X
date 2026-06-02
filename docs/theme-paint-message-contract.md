# Win32 Theme / Paint / Message Contract — Disassembly-Backed Reference

Scope: the real message/paint/theme contract for a CRT-free, manually-themed Win32 app
(`WindowsProject`) doing a live light↔dark transition. Every claim is backed by a disassembly
listing or a named source line. Evidence gathered from local module dumps:

| Module | Dump | Base | Symbols |
|---|---|---|---|
| ntdll.dll | `ntdll.disasm.txt` | 0x180000000 | export-only |
| user32.dll | `user32.disasm.txt` | 0x180000000 | export-only |
| win32kfull.sys | `win32kfull.disasm.txt` | 0x140000000 | none |
| dwmapi.dll | `dwmapi.disasm.txt` | 0x180000000 | export-only |
| uxtheme.dll | `uxtheme-disasm.txt` | 0x180000000 | export-only |
| comctl32.dll | `comctl32.disasm.txt` / `comctl32.imports.txt` | 0x180000000 | ordinal-only |

Dumps are `dumpbin /DISASM[:NOBYTES]`; `.rdata`/strings are NOT emitted, so per-class string/name
attribution is impossible without public PDBs (see §8). Listings normalized (whitespace collapsed).

---

## 0. Dispatch model — DefWindowProc is a kernel thunk

```
; dumpbin /EXPORTS user32.dll
  1676  AC          DefWindowProcW (forwarded to NTDLL.NtdllDefWindowProc_W)
; ntdll NtdllDefWindowProc_W and neighbours = a pfn table, jmps only:
18015FCF0: jmp qword ptr [1801E6200h]
18015FD30: jmp qword ptr [1801E6210h]   ; <- NtdllDefWindowProc_W
18015FD50: jmp qword ptr [1801E6218h]
```
Grep for any *writer* of `1E6200..1E6228` in ntdll = only the jmps → the slot is bound cross-module
at runtime; the real worker is in **win32u/win32kfull (kernel)**.

- **CONTRACT:** `DefWindowProc` NC/frame/menu/owner-draw handling is kernel-resident. You may act
  before forwarding, forward and repair after, or not forward — there is no third user-mode layer.
- **NEVER** assume a message you don't pass to `DefWindowProc` is handled (return 0 to `WM_NCPAINT`
  and the frame/menu never draw).

---

## 1. Menu + owner-draw — kernel sends, all CONDITIONAL (win32kfull)

The kernel menu/NC draw sends the undocumented UAH messages and `WM_DRAWITEM` via
`xxxSendMessage` wrappers (`call 14013C620` / `14003A6F0`); `mov edx,<id>` is the message id.
**Every send is gated** — none is unconditional, none is guaranteed per `WM_NCPAINT`/tick.

### WM_UAHDRAWMENU (0x91) @ 0x140082F7C
```
140082EFF: call 140082698        ; menu-state helper -> eax
140082F04: test eax,eax
140082F06: je   140082F60        ; eax==0 -> r15b=0 (suppress)
140082F08: mov  r15b,r12b        ; eax!=0 -> arm send
140082F71: test r15b,r15b        ; GATE
140082F74: je   140082F8E        ; r15b==0 -> SKIP send
140082F7C: mov  edx,91h          ; WM_UAHDRAWMENU
140082F84: call 14013C620        ; xxxSendMessage
```
Gate: fires only when helper `140082698` returned nonzero.

### WM_UAHDRAWMENUITEM (0x92) @ 0x140084CE4
```
140084C64: test cl,20h           ; menu-state bit 0x20 in [+10654h]
140084C67: jne  140084D36        ; set -> bail
140084C79: cmp  [rax+10654h],r12d
140084C80: jge  140084D36        ; counter past end -> bail
140084CE4: mov  edx,92h          ; WM_UAHDRAWMENUITEM
140084CEC: call 14003A6F0
```

### WM_UAHINITMENU (0x93) @ 0x1400826CA — gated on a resolvable menu object.
### WM_UAHMEASUREMENUITEM (0x94) @ 0x140083AA2 — gated on predicate `14009831C != 0` AND target!=0.
### WM_UAHNCPAINTMENUPOPUP (0x95) @ 0x14017B69E — gated on `14009831C != 0`; else legacy NC paint.
### WM_DRAWITEM (0x2B) @ 0x140089DAC
```
140089C61: test dl,dl            ; GATE: owner-draw selector (2nd arg)
140089C63: jne  140089D23        ; dl!=0 -> owner-draw send path
140089DAC: mov  edx,2Bh          ; WM_DRAWITEM
140089DB1: call 14033BA60
```
(`WM_UAHDESTROYWINDOW`/0x90: the `mov edx,90h @ 0x1400A5455` is a **metric selector**, not a send —
its call target is the DPI-scale helper `1400A434C`, not an xxxSendMessage wrapper. False positive.)

- **CONTRACT:** UAH/owner-draw callbacks fire only when the kernel decides to repaint the bar AND
  its state gate passes — **not every NCPAINT, not every animation tick.**
- **DO** during animation: `DefWindowProc(WM_NCPAINT)` first, then repaint the menu band yourself
  each tick off your clock (the UAH cadence is stale).
- **NEVER** rely on UAH alone mid-fade (removing the per-tick overpaint regressed the bar to ~120%
  off the caption). **NEVER** fill a parent client over child windows without `WS_CLIPCHILDREN`.

---

## 2. DWM — dwmapi (caption attribute + caption-button NC)

### DwmSetWindowAttribute @ 0x180004E20
```
180004E44: test r8,r8           ; pvAttribute != NULL
180004E47: jne  180004E6F       ; else E_INVALIDARG (80070057)
180004E6F: cmp  ebp,4           ; cbAttribute >= 4
180004E72: jb   180004E49       ; else E_INVALIDARG
180004E79: cmp  ebx,0Eh         ; special-case attr 0x0E (HOSTBACKDROPBRUSH); 0x0F,0x26 also
180004EA6: call qword ptr [18001A588h]  ; marshal to DWM service (client object), not inline LPC
```
- DWMWA_USE_IMMERSIVE_DARK_MODE (0x14) is **not** special-cased in the prologue → forwarded through
  the generic marshalling path to the DWM service.
- **CONTRACT:** the caption shade is a binary attribute; DWM composites its crossfade on the **vblank
  clock**, in a separate process. **DO** flip it at transition start so DWM's fade runs concurrently.
  **NEVER** pass `cbAttribute<4` or a null value (hard `E_INVALIDARG`).

### DwmDefWindowProc @ 0x180002E10 — handles ONLY 4 messages
```
180002E44: cmp edx,84h          ; WM_NCHITTEST  -> caption-button hit-test (service call 180004480)
180003063: cmp ebx,2A2h         ; WM_NCMOUSELEAVE -> hover/leave notify (180004558)
18000306B: cmp ebx,0A1h         ; WM_NCLBUTTONDOWN -> reuses hit-test gate @180002E50
180003079: cmp ebx,2A3h         ; WM_NCMOUSEHOVER -> hover/leave notify
180003081: mov eax,edi          ; default: return FALSE (not handled)
```
Accepted hit regions (the `sub rax,…` ladder): 8 HTMINBUTTON, 9 HTMAXBUTTON, 0x14 HTHELP, 0x15 HTCLOSE.
- **CONTRACT:** `DwmDefWindowProc` is **only** caption-button NC interaction (hit-test/press/hover/
  leave) — it does NOT touch `WM_NCCALCSIZE`/`WM_NCACTIVATE`/paint. Gated on DWM composition enabled
  (`[18001A550h]==1`). **NEVER** expect it to handle frame paint or sizing.

---

## 3. uxtheme / visual styles — the msstyles renderer

### OpenThemeData @ 0x18000CF40 — can return NULL (theming off / no activation context)
```
18000CF4A: mov  eax,[18009F434h]   ; g_fThemingDisabled (process)
18000CF58: jne  18000CFAA
18000CF74: call [1800715E0h]       ; activation-context / theming-state probe
18000CF84: test byte ptr [rsp+34h],1
18000CFAA: cmp  dword ptr [18009EB88h],0  ; g_fThemeActive
18000CFB1: jne  18000CF8B           ; live -> worker 18000CFC0 -> HTHEME
18000CFB3: xor  eax,eax             ; else return NULL
18000CF99: call 18000CFC0           ; real worker
```
- **CONTRACT:** a NULL return means *visual styles are off for this caller* (theming disabled OR the
  v6 activation context isn't active) — not an allocation failure. **NEVER** assume `OpenThemeData`
  succeeded; always null-check.

### DrawThemeTextEx @ 0x18001AFB0 — requires DTTOPTS; renders the msstyles part (incl. glow)
```
18001AFE3: test rdx,rdx            ; DTTOPTS required
18001AFE6: je   18001B250          ; null -> error (DrawThemeText has no options, different entry)
18001B02A: mov  eax,[rax]          ; DTTOPTS.dwSize
18001B032: ja   18001B257          ; dwSize must be 0x38..0x48
18001B03F: call 18001C3A0          ; normalize DTTOPTS (consumes dwFlags: DTT_TEXTCOLOR 0x1, DTT_GLOWSIZE 0x800)
18001B044: mov  rbp,[18009EBA0h]   ; g_pThemeClassDataTable; HTHEME = LOWORD index + HIWORD cookie
```
- **CONTRACT:** text is drawn through the msstyles part; `DTT_TEXTCOLOR` only recolors, the part's
  **glow still renders**. `DTT_GLOWSIZE` + `iGlowSize=0` suppresses it.
- **DO** render menu/themed text with `DrawThemeTextEx` (matches system AA); during a fade add
  `DTT_GLOWSIZE`/`iGlowSize=0` (the glow re-blending against a moving bar each tick = the shimmer,
  "a result of the msstyle"). **NEVER** mix raw GDI `DrawTextW` with themed draws of one surface
  (different rasterization → shimmer).

### DrawThemeBackground @ 0x18001DC90 — same HTHEME table/cookie validation, dispatches per part/state.

### BeginBufferedAnimation @ 0x180022A90 / BufferedPaintRenderAnimation @ 0x180022C40
```
; both call the per-thread animation registry resolver:
1800242C2: mov  ecx,[18009D6F0h]  ; TLS slot g_dwAnimationTls
1800242CF: call [180071C38h]      ; TlsGetValue -> per-thread active-animation list
; RenderAnimation scans the list for THIS hwnd:
180022C92: cmp  [rdx+0F8h],rdi    ; entry.hwnd == hwnd ?
180022C9B: cmp  byte ptr [rdx+100h],bl ; entry active flag set ?
180022CAA: call 180022138         ; render this window's frame
```
- **CONTRACT:** buffered-paint animations live in a TLS-keyed list keyed by `hwnd` (`[+0xF8]`), tag 3
  (`[+8]`), active flag (`[+0x100]`). A window has at most one. uxtheme advances it on the
  composition clock and auto-invalidates. **NEVER** start a second `BeginBufferedAnimation` on a
  window already animating its client (reviving it for the client regressed the round-trip to ~176%).

---

## 4. Common controls — custom-draw / owner-draw (comctl32)

**Two comctl32 exist.** `C:\Windows\System32\comctl32.dll` is **v5.82** — the legacy, *un-themed* SxS
stub: zero uxtheme binding (no uxtheme string/import), only ntdll/advapi32/gdi32/kernel32/user32.
The **themed** controls live in the **v6** assembly under `C:\Windows\WinSxS\...common-controls_..._6.0.*`
(activated by `app.manifest`, §6). The custom/owner-draw evidence below (addresses `0x18002…`) is from
the **v5** System32 dump; the theme/animation contract (§4b) is from **v6**. v6 imports `UxTheme.dll`
both statically AND delay-load (it has a `.didat` section), so its theme calls are real named imports —
recoverable (§4b), contrary to a "GetProcAddress-only" assumption.

### NM_CUSTOMDRAW (notify −12 = 0xFFFFFFF4) — single chokepoint @ 0x18002DF98
```
18002DF9C: bt   edx,10h          ; CDDS_ITEM (bit 16) ?
18002DFAC: mov  [r8+18h],edx     ; NMCUSTOMDRAW.dwDrawStage
18002DFB0: mov  edx,0FFFFFFF4h   ; NM_CUSTOMDRAW
18002DFB5: call 18002D08C        ; internal WM_NOTIFY dispatcher:
  18002D653: mov edx,4Eh         ;   WM_NOTIFY
  18002D658: call [18008FED8h]   ;   SendMessage(parent, WM_NOTIFY, id, &NMCUSTOMDRAW)
18002DFBA: test eax,0FE01h       ; mask returned CDRF_* bits (the one CDRF gate in the image)
```
CDDS stages feed the same helper: `mov edx,10001h` (CDDS_ITEMPREPAINT) ×14 sites, `mov edx,10002h`
(CDDS_ITEMPOSTPAINT) ×14 — the standard pre/post-item protocol, one shared sender. Call sites check
`cmp eax,4` (CDRF_SKIPDEFAULT) to skip default paint.

### Owner-draw sends
```
18004C60D: mov edx,2Bh           ; WM_DRAWITEM  -> call [180090168h] (SendMessage to parent)
180074FA6: mov edx,2Ch           ; WM_MEASUREITEM -> call [18008FED8h]
```
- **CONTRACT (custom-draw):** a themed common control asks the parent per item via
  `WM_NOTIFY/NM_CUSTOMDRAW`; the parent's `CDRF_*` return controls font/skip/post-paint. **DO**
  return `CDRF_DODEFAULT` unless intentionally overriding; honor the `0xFE01` CDRF flag set.
- **CONTRACT (owner-draw):** `WM_DRAWITEM`/`WM_MEASUREITEM` only fire for `*_OWNERDRAW`-styled
  controls. **NEVER** assume a control is owner-drawn unless its style requested it.

---

## 4b. Common controls v6 — the CANONICAL themed-control cross-fade (this is the pattern to copy)

v6 controls (incl. the dialog OK button) cross-fade a themed part between two states via uxtheme's
buffered animation. v6's uxtheme delay-IAT pfn map (image base 0x180000000, v6 SxS dll):

| uxtheme fn | pfn slot | | uxtheme fn | pfn slot |
|---|---|---|---|---|
| OpenThemeData | 0x180242450 | | BeginBufferedAnimation | 0x180242520 |
| DrawThemeBackground | 0x180242458 | | EndBufferedAnimation | 0x180242518 |
| BeginBufferedPaint | 0x1802424A8 | | BufferedPaintRenderAnimation | 0x180242528 |
| EndBufferedPaint | 0x1802424A0 | | BufferedPaintInit | 0x180242530 |
| GetThemeColor | 0x180242590 | | GetThemeTransitionDuration | 0x1802425A0 |

Four structurally-identical cross-fade routines (one per animated control class) — button cluster
@`0x99xxx` (others @0x6Dxxx, 0xB2xxx, 0xC75xx). Annotated (rbx=control instance, r12=HDC, r13=rect,
[rbx+0x38]=HWND, [rbx+0x15C]=stateId, [rbx+0x17C]=flags, [rbx+0x1C4/1C8]=last-state):

```
1800996B8: call qword ptr [180242528h]   ; (1) BufferedPaintRenderAnimation(hwnd,hdc) FIRST
1800996C6: jne  180099842                 ;     animation live -> it blitted this frame -> DONE
1800996CE: mov  dword ptr [rbp-21h],10h   ; BP_ANIMATIONPARAMS.cbSize
1800996DC: mov  eax,1                      ; BPAS_LINEAR
1800998C4: call qword ptr [1802425A0h]   ; (2) GetThemeTransitionDuration(...) on state-change
1800997D5: call qword ptr [180242520h]   ; (3) BeginBufferedAnimation -> hdcFrom, hdcTo
;          paint OLD state into hdcFrom, NEW state into hdcTo (DrawThemeBackground)
180099823: call qword ptr [180242518h]   ; (4) EndBufferedAnimation(hbp, TRUE) -> uxtheme owns timeline
180099835: mov  [rbx+1C8h],eax            ;     record last-painted state (won't re-trigger)
```
**The contract (DO — copy this for any per-tick cross-fade):**
1. lazily `BufferedPaintInit` once;
2. on each `WM_PAINT`, call `BufferedPaintRenderAnimation(hwnd,hdc)` **first** — if it returns nonzero
   the animation is in flight and it rendered the frame; **stop** (this is what continues the fade
   across paints — *not* a wall-clock timer);
3. only when no animation is running and the state changed: `GetThemeTransitionDuration` →
   `BeginBufferedAnimation` (captures from/to surfaces) → paint old into `hdcFrom`, new into `hdcTo` →
4. `EndBufferedAnimation(hbp, TRUE)` — uxtheme drives the timeline on the composition clock and
   re-invalidates; subsequent paints flow back through step 2.

This is the **composition-synced** cross-fade. The earlier failed attempt to revive it for the client
regressed (round-trip 176%) because it did NOT follow this exact shape (render-first, continue across
paints, state-gated single Begin). The OK button rides this — its "own clock" is *this* uxtheme
timeline, vblank-driven, not an independent wall clock.

- **NEVER** start a `BeginBufferedAnimation` without first letting `BufferedPaintRenderAnimation`
  continue an in-flight one. **NEVER** re-Begin every tick.

## 5. Per-class identity — still needs symbols

The 14/14 CDDS sites (§4) and the 4 v6 cross-fade clusters (§4b) map to distinct classes
(ListView/TreeView/Header/Toolbar/Tab/Trackbar/Rebar/Pager/Button) but **which cluster is which class
cannot be proven from symbol-less dumps** (class names are `lea reg,[rip+disp]` into `.rdata`). The
button is *one* of the four §4b clusters, inferred structurally. Definitive per-class attribution
needs `comctl32.pdb` (Microsoft Symbol Server). Scratch v6 disasm left at `C:\dev\Win32X\v6.disasm.txt`.

---

## 6. Activation context (cross-cutting)

- `examples/app.manifest:10` declares `Microsoft.Windows.Common-Controls 6.0.0.0`.
- `OpenThemeData`'s gate (§3) + comctl32's dynamic uxtheme binding mean **themed/UAH draw only
  happens with the v6 activation context active on the servicing thread.**
- **NEVER** run a themed draw or service a UAH/custom-draw callback without the v6 actctx active —
  without it the whole themed path collapses to flat (suspected cause of intermittent
  all-surfaces-together failures; still to be confirmed at runtime).

---

## 7. The contract in one table (DO / NEVER)

| Surface | DO | NEVER | Evidence |
|---|---|---|---|
| Dispatch | act-before / repair-after / don't-forward | assume unforwarded msg handled | §0 ntdll jmp table |
| Menu/owner-draw | overpaint bar each tick off your clock | rely on UAH alone mid-fade | §1 gates |
| Client paint | `WS_CLIPCHILDREN`; double-buffer children (`WM_PRINTCLIENT`+blit) | fill over children; `InvalidateRect` children mid-fade | §1, code |
| DWM caption | flip attribute at start | `DwmFlush` on GUI thread; expect ≤1-frame match @180Hz | §2 |
| Themed text | `DrawThemeTextEx`; `DTT_GLOWSIZE=0` during fade | mix `DrawTextW` w/ themed; assume `OpenThemeData` ok | §3 |
| Buffered anim | one per window, uxtheme-driven | second `BeginBufferedAnimation` on same hwnd | §3 |
| Common ctrls | target sub-theme from frame 1; honor `CDRF_*` | defer sub-theme to end; hold OK button to tight band | §4, code |
| Actctx | keep v6 context active for every themed/UAH/custom-draw call | service themed draw w/o it | §6 |

---

## 8. Limits of static evidence (what needs PDBs)

- Per-message `DefWindowProc` switch: **kernel-resident** (win32kfull `xxxDefWindowProcWorker`),
  symbol-less; only the `WM_NCPAINT`→menu path is anchored here (the UAH sends, §1).
- Per-control-class procs and comctl32's uxtheme/buffered-paint usage: **dynamically bound, symbol-
  less, `.rdata`-less** → not attributable from these dumps.
- To complete §1 (all DefWindowProc messages) and §5 (per-class): pull public PDBs
  (`win32kfull.pdb`, `comctl32.pdb`, `uxtheme.pdb`) and re-disassemble symbolized, and/or capture a
  runtime per-message trace of the transition.
