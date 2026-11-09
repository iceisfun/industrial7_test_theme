# 05. Industrial7 current state: theme data sources, uxtheme-w10, header height, house style for recorded constants

Role: INDUSTRIAL7 STATE ANALYST. Read-only pass over `/home/iceisfun/work/industrial7` (master `25af4cbccd`, 2026-11-08; working tree has two unrelated modified scripts, nothing of ours). All paths below are relative to that repo unless they start with `/`. Line numbers were read on that tree.

Labels: [R] repo fact (file:line read in this pass); [bb] fact from a bulletin-board topic read with `bb show`/`bb search` (topic and reply numbers given); [corpus] read from `http://docker1.dev.moonlightcompanies.com:8765` in this pass; [inf] inference; UNKNOWN = not found. Nothing here was run on Windows or on a guest.

## 0. Bottom line

1. **There is no theme data source anywhere in Industrial7.** `uxtheme-w10` (193 exports: 35 `partial`, 158 `ni`) answers "no theme": `OpenThemeData*` returns NULL with last error 0x80070490, every handle call returns E_HANDLE 0x80070006, `IsThemeActive`/`IsAppThemed` return FALSE. `GetThemePartSize`, `GetThemeMetric`, `GetThemeInt`, `GetThemePosition` are all the E_HANDLE class. [R] `userland/guest/uxtheme-w10/data/exports_manifest.tsv`; `crates/w10-uxtheme-off/src/lib.rs`; `docs/design/win10/UXTHEME-DWMAPI-W10-LINK.md` section 3. The owner's one-line description in the task is accurate.
2. **The header-control 20-vs-24 gap is a one-field gap.** The pure header model only raises its height to a host-supplied `Theme.min_height` when `Theme.active` is true (`crates/w10-comctl32-lv/src/hdr/layout.rs:60-61`); the only production implementation of the host hook returns `Theme::default()` unconditionally (`userland/guest/comctl32-w10/src/hdr_host.rs:923-925`). The comctl32-w10 header image does not import uxtheme at all (`hdr_host.rs:5`). So the plug-in point is a host method, not a uxtheme call (section 4).
3. **A trap for the table design:** `Theme.active` is not a "height" switch. It also flips hot tracking (`hdr/msg.rs:240`, `hdr/drag.rs:274`), the overflow hit rectangle (`hdr/hit.rs:42-46`), sort-arrow width (`hdr/layout.rs:107`), focus rectangle (`:214`), split-button drawing (`:244`) and hot text colour (`:267`). Setting `active: true` just to get `min_height` would change all of those. See section 4.3.
4. **"Armed-Win10 personality" is not a named artifact in the repo.** "Armed" is a kernel predicate (a process with the Win10 personality whose syscall gate is armed). The closest existing thing to a recorded-constants table for it is the Win10 default system colour table (section 3). There is no existing Win10 oracle row for any uxtheme part size, metric or int. [R] grep of `userland/wintests` and `docs` found only the Win7/no-theme probe `userland/wintests/comctl32/comctl32_theme_v6.c`.
5. **The stock-DLL ranking doc and `docs/OWNED_STATUS.md` are Win7-side and stale for this question.** Row for uxtheme: `docs/OWNED_STATUS.md:252` ("stock-only ... win7-desktop ... research only: docs/design/stockdll-ranking (rank 2)"). It never mentions `uxtheme-w10`, `shell32-w10`, `propsys-w10` or `shcore-w10` (grep: no hits). The Win10 images are tracked in `docs/personality/registry.tsv`.
6. **The "wininet and sspicli research passes" are not research-only docs.** They are design docs that grew from research; the structure to copy is in section 6.

## 1. Existing uxtheme implementation (what each export returns)

### 1.1 Layout [R]

| piece | path | note |
|---|---|---|
| pure decision table | `crates/w10-uxtheme-off/src/lib.rs` (1100 lines, `no_std`) | Win7-RTM-derived; `EXPORTS` 61 rows at :103, `ORDINALS` 4 rows at :179, `COMCTL32_V6_DELAY` 47 names at :188; entry type `Uxtheme<H>` with one method per class (`theme_query` :377, `open_theme_data` :389, `close_theme_data` :408, `handle_call` :414, `sys_color` :434, `set_window_theme` :475, BufferedPaint :622-:673) |
| image | `userland/guest/uxtheme-w10/` (`src/{glue,guest,lib,mem,window}.rs`, `data/{corpus_exports.tsv,exports_manifest.tsv,uxtheme.def}`, `build.sh`, `tests/`) | additive crate, own workspace |
| design docs | `docs/design/win10/UXTHEME-OFF-W10-LINK.md` (106 lines, the Win7-derived decision table), `docs/design/win10/UXTHEME-DWMAPI-W10-LINK.md` (the Win10 image, 193 exports) | |
| registry | `docs/personality/registry.tsv:443` status `unwired`; `crates/w10-uxtheme-off` marked wired (commit `baf5fbfae1`) | |
| build/gates | `host/xtask/src/gates.rs:1226` (`Dll("uxtheme")`), `host/xtask/src/w10_images.rs:72` (`("uxtheme","user32.dll")` import exception), `:84` (`UNWIND_ENFORCED`), `host/xtask/src/w10_family.rs:33` (GENERIC image list; not in the `DEDICATED` default list at :14-15) | whether uxtheme is in the default staged family set is UNKNOWN; `host/xtask/src/gfx/comctl_subitems.rs:433` stages it for the comctl-subitems gate |

### 1.2 Export census [R, counted from `exports_manifest.tsv` and `host/i7dbg/data/owned-export-status.tsv`]

193 exports, ordinals 1..193; 85 named, 108 ordinal-only; 34 are `SHUNIMPL.#N` forwarders in stock (stubs here); `partial` 35, `ni` 158 (i7dbg status rows: `PARTIAL 35`, `STUB 158`). Image base 0x79100000. Corpus provenance: Win10 `uxtheme.dll` 10.0.19041.2193 [VERIFIED in the fact-check pass: `GET /api/modules/uxtheme` returns `"version":"10.0.19041.2193"`, size 627200, image_base 0x171..., pdb UxTheme.pdb; the `2965` that the repo docs mention is the user32 build, not uxtheme].

### 1.3 What the 35 `partial` exports return (verbatim behaviour classes) [R `UXTHEME-DWMAPI-W10-LINK.md` section 3]

| export(s) | answer | evidence label in doc |
|---|---|---|
| `IsThemeActive`, `IsAppThemed` | FALSE; last error 0 (0x8000FFFF before init) | V-corpus 0x224e0, 0x224b0 |
| `IsCompositionActive` | FALSE, last error untouched | V-corpus 0xe8b0; internal unread |
| `OpenThemeData`, `OpenThemeDataEx`, `OpenThemeDataForDpi`, ordinal 49 `OpenNcThemeData` | NULL, last error 0x80070490 (NULL class list: 0x80004003); hwnd validity check and prop clear NOT done | V-corpus 0xbb20, 0x1b590 |
| `CloseThemeData` + 25 handle calls (`DrawThemeBackground`, `DrawThemeBackgroundEx`, `GetThemePartSize`, `GetThemeBackgroundContentRect`, `GetTheme{Color,Int,Bool,Font,Metric,Rect,String,Margins,Position,TextExtent,TextMetrics,...}`, `DrawTheme{Text,Edge,Icon}`, `HitTestThemeBackground`) | E_HANDLE 0x80070006 | `CloseThemeData`, `GetThemePartSize`, `GetThemeBackgroundContentRect`, `DrawThemeBackground(Ex)` are V-corpus (0x190f0, 0x14a20, 0x17ca0, 0x15000, 0x5200); the other 21 are Win7 crate rules, Win10 argument-check order UNVERIFIED. CORRECTION (fact-check): for `GetThemePosition` the Win10 order IS readable (`GET /api/function/uxtheme/GetThemePosition/decomp`, RVA 0x6af0): a NULL `pPoint` returns 0x80004003 (E_POINTER, `-0x7fffbffd`) BEFORE the handle lookup; only a non-NULL `pPoint` reaches `CThemeApiHelper::OpenHandle`. So "every handle call returns E_HANDLE" holds only for non-NULL out pointers; header callers always pass one |
| `SetWindowTheme`, `EnableThemeDialogTexture`, `SetWindowThemeAttribute` | window-property calls (atoms 0xA910/0xA911/0xA914/0xA916), `WM_THEMECHANGED` | V-corpus 0x3bb0, 0x4ef0, 0x65e0 |

Everything else (158) is `ni`: HRESULT-typed ones return E_NOTIMPL, others FALSE/NULL/0 with `ERROR_CALL_NOT_IMPLEMENTED`. This includes `BufferedPaint*`, `GetWindowTheme`, `GetCurrentThemeName`, `GetThemeSys*`, `DrawThemeParentBackground*`, `IsThemePartDefined`, `IsThemeBackgroundPartiallyTransparent`, `GetThemeAppProperties`.

Note: the crate `w10-uxtheme-off` models more than the image exposes (e.g. `GetWindowTheme`, `GetCurrentThemeName`, BufferedPaint refcounts, `GetThemeSys*` NULL-handle arms); the image keeps those `ni` because they need user32/gdi32 imports it deliberately does not take (`UXTHEME-DWMAPI-W10-LINK.md` sections 3 and 5).

### 1.4 The Win7 comctl32 side [R]

The older owned Win7-line comctl32 has its own theme-off module: `userland/guest/comctl32/src/theme.rs:1-30` (`open` returns `None`; `docs/design/comctl32/DESIGN.md:545-575` section 5.3 is the table of uxtheme calls that have effects with no theme). The `no-import` rule forbids it importing uxtheme (`host/xtask/src/owned/comctl32.rs:390-404`). The Win10 comctl32-w10 differs: its list view binds uxtheme at run time (section 4.4).

## 2. Stock-DLL ranking doc and status tables

- `docs/design/stockdll-ranking/RESEARCH.md` (326 lines, branch `laneb/RESEARCH-UXTHEME-WINMM-SETUPAPI`, commit `1aaf259e89`, 2026-10-14). Verdict: winmm rank 1, **uxtheme rank 2** "theme-off stub set, cheapest and decomp-backed but low value" (:8-9), setupapi/cfgmgr32 rank 3; ranking table row :311. All data is **Win7 RTM 7600** (138 exports, :46-49; 52-name importer union, :64-81). It states in section 1.4 (:97-123) that stock uxtheme with no theme service answers exactly what the owned comctl32 hard-codes. Its value statement ("changes no behaviour on i7") predates the Win10 symptom and is superseded by the CC30 finding. It lists no Win10 numbers and no part sizes.
- `docs/OWNED_STATUS.md:252`: generated Win7 table (header at :289 area; rollup :8-20). It has an R5 resource row "theme: uxtheme/msstyles/Aero .theme ... 0 files ... NOT_SHIPPED" (:637). **No `.msstyles` is shipped in any image**, so there is no theme data to parse today.
- Win10 unowned/unfinished peers named in the task: `shell32-w10`, `propsys-w10`, `shcore-w10` all have `unwired` registry rows (`docs/personality/registry.tsv:410,425,426`); none is in OWNED_STATUS. A fresh count of "711 of 833 not implemented" for shell32 was NOT re-derived here.
- `docs/design/win10/RING2-PLAN.md` is referenced in bb #923 reply #9252 (weighted gap: uxtheme 20 rows / weight 30; "Open question: real theming (msstyles) and real IME are product decisions, not planned"). The file is on branch `lanec/ring2-plan`, not on master: not read.

## 3. How recorded / oracle constants are stored and consumed today (house style)

### 3.1 The recorded-probe pipeline [R]

1. **Probe**: one freestanding C file in `userland/wintests/win10/` (`// wintests: crt=mingw`, links only builder libs; optional DLLs via `LoadLibraryW`/`GetProcAddress`; every risky call on a guarded worker thread; measurement only, exit 0). Rows are `PFX <group>.<step> k=v ...`: no pids, handles, addresses or times, printable ASCII, LF only. Example header: `userland/wintests/win10/win10_dwmattr_probe.c:1-40` (`DWA ...`). Grammar and verdict vocabulary for the stricter differential form: `userland/wintests/include/oracle.h:1-60` (line `CONTEST <prefix><case>: <evidence> | <VERDICT>`; PASS/FAIL/STUB/UNIMPL/SKIP/OBSERVE). Family README: `userland/wintests/README.md`, `userland/wintests/win10/README.md`.
2. **Record**: committed under `userland/wintests/win10/expected/<probe>.win10.txt` (standard user) and `.win10.adm.txt` (administrator); six runs (std x3 + adm x3) must be byte-identical before a row is trusted (bb #1134 replies #15707, #16656; bb #1097 DONE-WHEN). Older/other layouts: `userland/wintests/win10/data/dlg_paint_probe.win10-oracle-19045.txt`, `userland/wintests/<family>/<name>.win10.txt` (fs, msvcrt, kernel32), Win7 columns as `comctl32/data/*.win7.tsv` with `# recorded-by:` header lines. 114 files in `win10/expected/` as of this tree. The recording seat is **laneg** (Win10 oracle VM runs, Depot tarballs, `bb attach`).
3. **Consumers**: host tests `include_str!` the record and replay every row against the model. Examples: `userland/guest/dwmapi-w10/tests/frame.rs:12` and `kernel/kmain-hosttest/tests/glass_rules.rs:54` both embed `userland/wintests/win10/expected/win10_dwmattr_probe.win10.txt`; `crates/w10-comctl32-lv/tests/hdr_trace_replay.rs` replays `comctl32/data/trace_hdr_*{,_v6}{,.win7}.tsv`. The implementation table is typed independently of the record and compared (`tests/manifest.rs` pattern in uxtheme-w10/dwmapi-w10).
4. **Corpus-derived tables**: generator script + digest-pinned TSV under the crate's `data/`: `scripts/w10_uxdwm_exports.py` (`--fetch`, `--check`, `--verify-facts`) -> `userland/guest/uxtheme-w10/data/{corpus_exports.tsv,exports_manifest.tsv,uxtheme.def}`; `scripts/w10_user32_metrics.py` -> `crates/w10-user32-metrics/data/w10_user32_metric_slots.tsv` (header `#sm_index slot source`, source column cites the RVAs). Pattern: `#` header lines with provenance, tab-separated, "do not edit, GENERATED by".
5. **Policy #550**: no Microsoft bytes committed; the corpus is an oracle; only names, ordinals, RVAs, "small constants and control-flow facts" are quoted (`UXTHEME-DWMAPI-W10-LINK.md` header; `COMCTL32-HEADER-W10.md` header). Whether a table of themed part sizes, margins and colours falls under "small constants" is NOT addressed anywhere I found: UNKNOWN, needs an owner decision before a table is committed (colours and bitmap-derived values are the risky ones; integer sizes are the same kind of datum as `SM_*` values).
6. **Labels** used everywhere: `[V-corpus]` (with RVA), `[oracle]`/`[obs]` (recorded row), `[V-impl]`, `[V-test]`, `[choice]`, `[hardening]`, `[unverified]`/`[U]`/`UNVERIFIED`. Honest failure over fake success; a row whose Win10 value is unknown stays `ni`/UNKNOWN.

### 3.2 Nearest precedent to an "armed-Win10 constants table" [R]

- `crates/w10-win32k-serverinfo/src/lib.rs:97` `DEFAULT_SYS_COLORS: [u32; 31]` (comment :87-96): "the Win10 22H2 default system colour table a Win10 session starts from"; **only index 15 (COLOR_BTNFACE 0x00F0F0F0) was observed** on a real Win10 guest (bb #983 oracle run); "every other entry is [unverified] until the dump row `color.table` of `win10_user32_misc` has been run on the oracle". `win10_default_color()` at :107 swaps the untouched Win7 value for the Win10 one. It is mirrored by `crates/user32-w10-dpi/src/colors.rs:21` `DEFAULT_COLORS` with a pinning host test. Kernel consumer: `kernel/kmain/src/win32k/user/w10_si.rs:90` `adopt_win10_default_palette`, called at first Win10 connect (`kernel/kmain/src/win32k/user/objects.rs:785`). The `color.table` row exists in `userland/wintests/win10/win10_user32_misc.c` but **no `win10_user32_misc*.win10.txt` record is committed** (`ls expected/`: no such file): so even this precedent is largely unrecorded.
- `crates/w10-user32-metrics` (rules read from user32 19041.2965 `RealGetSystemMetricsForDpi` 0x20520 etc.) and `crates/w10-win32k-serverinfo` (per-DPI metric cache, `aiSysMet` at 0x768).
- `docs/design/gdi32/D12-TRUETYPE-UI.md`: a recorded Segoe UI -12 TEXTMETRIC table (h15 a12 d3 il3 ave6) in a doc, knob `I7_GDI_TRUETYPE_UI` / ESP file `\industrial7\gdi-truetype-ui`, default OFF (:1-60; bb #1134 reply #16659). This is a **knob-gated personality table** and the model for "armed" switches: `docs/personality/KNOBS.md` (grammar of `\industrial7\personality`: `image <exe>=win7|win10`, `fsbrowse=`, `afd=` ... "ARMED Win10 callers").

### 3.3 What "armed" means [R]

`kernel/kmain/src/process/personality.rs:344` `caller_is_armed_win10()`: true only when some process has the Win10 personality **and** the calling thread's process passes the syscall gate (`gate_applies(rec, GATE_LATE, p.win10_gate_armed())`). Win7 processes, the test-only late-gate child and kernel threads are false. KNOBS.md uses "ARMED Win10 callers" for every Win10-only behaviour. Kernel-side Win10-only constants live in `kernel/kmain/src/win32k/user/*_w10*.rs` (`ncframe_w10_rules.rs`, `w10_si.rs`, ...). Userland DLL constants (this task) are not "armed" in that kernel sense; the Win10 family DLL set is selected by `I7_FAMILY_W10_DLLS` / `host/xtask/src/w10_family.rs`. [inf] "armed-Win10 personality table" most plausibly means: data in a `w10-*-tables` crate (or a TSV under it), applied only to Win10-family processes, and never to the Win7 default image.

## 4. How the header control decides its height today, and where a table plugs in

### 4.1 The decision [R]

`crates/w10-comctl32-lv/src/hdr/layout.rs:55-71` `Header::layout` (`HDM_LAYOUT`, corpus `Header_OnLayout` RVA 0x4b2a8):

```
edge   = host.metric(SM_CYEDGE)
height = self.text_cy + 2*edge
if self.theme.active && height <= self.theme.min_height { height = self.theme.min_height }   // :60-61
if style & HDS_FILTERBAR { height += text_cy + 1 + 2*edge }
cy = HDS_HIDDEN ? 0 : height
```

`text_cy` comes from `Host::measure_font` (`hdr/mod.rs:366`; image impl `hdr_host.rs:765-775`, "..." extent, **the themed "M" measurement is skipped**). `Theme` (the host-reported struct) is `hdr/mod.rs:210-222` {`active`, `min_height`, `dropdown_width`, `overflow_w`, `overflow_h`}. It is read at create (`hdr/mod.rs:516`, also `dd_width`/`dd_pad` :517-520) and on `WM_THEMECHANGED` (`hdr/msg.rs:573`). Default trait body `Host::theme_info() -> Theme::default()` at `hdr/mod.rs:381-384`; the only guest implementation `userland/guest/comctl32-w10/src/hdr_host.rs:923-925` also returns `Theme::default()`.

i7 result (bb #1134, lanec's CC30 i7 run, the reply just before #16650; attachment [1465]): `text_cy` 16 (8x16 stand-in font), edge 2 -> header 20, row 20. Win10 (laneg CC30 record, reply #16656, six boots byte-identical): header **24** in every fixture with a header (also with MS Shell Dlg -11 whose text height is 13), rows 19 (Segoe UI -12, tmHeight 15) / 17 (MS Shell Dlg -11). Because 24 does not follow the text height, [inf] it is the themed minimum, which matches the stock code in section 4.2. The model is already correct given the inputs; the Win7 trace-replay waiver says the same (`COMCTL32-HEADER-W10.md` section 9, waiver 1: a themed Win7 VM recorded 24, the model reproduces 24 only with `Theme.min_height = 24`; tests `hdr_layout.rs:53,58`, `hdr_trace_replay.rs:118`).

Separately, the 16 vs 15 text height and the 20-vs-19 row are the **font** problem, not the theme (bb #1134 reply #16659): knob `gdi-truetype-ui` is off by default; that is a different table and owner (win32k font / operator decision).

### 4.2 What stock does [corpus, read in this pass]

`GET /api/function/comctl32/_Header_InitTheme/decomp` (RVA 0x85d98): `ResetThemeData(hwnd, &HD+0x130, L"Header")`; if a theme handle exists:

- `GetThemePartSize(h, NULL, part 1, state 0, NULL, TS_TRUE, &sz)`; `GetThemePosition(h, 1, 0, 0xd4b, &pt)` (0xd4b = 3403 = TMT_MINSIZE [inf from the SDK value, not read from the corpus]); if the call succeeds, `sz.cx/cy` become max(sz, pt); `HD+0x160 = sz.cy` (the min height).
- `GetThemePartSize(h, NULL, 5, 0, NULL, TS_TRUE, &sz)`: `HD+0x114 = max(HD+0x114, sz.cx)`; `HD+0x118 = GetSystemMetrics(0x2d)` (SM_CXEDGE).
- `GetThemePartSize(h, NULL, 4, 0, ..., &HD+0x158)` and `(h, NULL, 7, 0, ..., &HD+0x144)`.

`Header_NewFont` (RVA 0x86174) with a theme: `GetThemeTextExtent(h, hdc, 1, 0, L"M", -1, 0x800, ...)` replaces the "..." extent (`HD+0x44/0x4c`), then `Header_OnLayout` (RVA 0x4b2a8) applies `height = max(text_cy + 2*edge, HD+0x160)` exactly as the Rust model. Part ids (verified against `/usr/x86_64-w64-mingw32/include/vsstyle.h` HEADERPARTS): 1 = HP_HEADERITEM, 4 = HP_HEADERSORTARROW, 5 = HP_HEADERDROPDOWN, 7 = HP_HEADEROVERFLOW (part 6, HP_HEADERDROPDOWNFILTER, is not queried at init). `TS_TRUE` is the 2nd-to-last enum arg (uxtheme.h:282); `0x800` in the `GetThemeTextExtent` call is `DT_NOPREFIX` (winuser.h:3455); `0xd4b` = 3403 = `TMT_MINSIZE` is now VERIFIED from `vssym32.h:342` (that it is a POSITION-typed property is still inference from the number range 3401..3407). Two further decomp facts the first draft missed: (a) `_Header_InitTheme` ignores the HRESULT of every `GetThemePartSize`; `local_28` is an uninitialised stack SIZE if the call fails, so a table that hands out a non-NULL handle MUST make part 1 succeed or the header min-height becomes garbage; only `GetThemePosition`'s HRESULT is tested; (b) `Header_OnLayout` and `Header_NewFont` gate on the theme HANDLE (`HD+0x130 != 0`) and `Header_NewFont` falls back to the `"..."` extent if `GetThemeTextExtent` fails (`HVar1 < 0`), whereas the Rust model gates on `Theme.active`. So the probes needed from Win10 for the header alone are GetThemePartSize parts 1, 4, 5, 7 state 0, GetThemePosition(1, 0, 3403), GetThemeTextExtent("M") (survey and probe list are other agents' files).

### 4.3 Where a table plugs in (three options, with the catch)

| option | change | risk |
|---|---|---|
| A. host-only, no uxtheme (smallest) | `hdr_host.rs:923` returns `Theme { active:?, min_height: <recorded>, dropdown_width, overflow_w, overflow_h }` from a recorded table crate | `active` is overloaded (list in section 0 item 3). Needs `Theme` split into layout inputs (`min_height`, `dropdown_width`, overflow size) and a paint/behaviour flag, otherwise hot tracking, hit rects and split-button paint change too. The classic painter cannot draw the themed header (`hdr_host.rs` classic cell painter only), so a Win10 height with a classic look is expected (the Win7 waiver shows the same hybrid). Header would still not match Win10 pixels |
| B. uxtheme answers (the stated goal) | `uxtheme-w10` `OpenThemeData(hwnd,"Header")` returns an opaque synthetic handle; `GetThemePartSize`/`GetThemePosition`/`GetThemeTextExtent`/`GetThemeMetric`/`GetThemeInt`/`IsThemePartDefined` answer from a table keyed by (class, part, state) | comctl32-w10 header does not import uxtheme (`hdr_host.rs:5`); it would need run-time binding like the list view, or the header would never see the table. Every consumer that gets a non-NULL handle takes its themed arm: list view calls `DrawThemeBackground` (`userland/guest/comctl32-w10/src/lv_ui.rs:84-175`) which is E_HANDLE today, so a half-implemented handle makes controls draw nothing. `IsThemeActive`/`IsAppThemed` flipping to TRUE has the same reach. Tab, toolbar, status, tree view, rebar have **no** themed arm at all in their pure crates (`w10-comctl32-tab/src/lib.rs:11`, `-tb/src/lib.rs:11`, `-status/src/lib.rs:13`, `-tv/src/lib.rs:13`, `-rebar/src/grip.rs:2`: "theme arms skipped, classic") |
| C. staged B | Table answers only metric/size/int/position queries and `IsThemePartDefined`=FALSE for parts the draw side cannot honour; `OpenThemeData` still NULL for classes whose consumers have no themed painter, non-NULL (size-only handle) for classes where only layout is wanted | needs an explicit rule for "handle that answers sizes but refuses drawing" so no consumer takes a draw arm; the Win10 reference does not distinguish them, so deviation rows are required |

[inf] recommended order matches the owner's: record sizes first (the table is useless without Win10 rows), then A for the header (one-field gap, host-testable) while B/C waits for the table and a decision on draw arms.

### 4.4 Other consumers' theme seams, as they exist now [R]

| consumer | seam | file:line |
|---|---|---|
| header | `Host::theme_info()` -> `Theme` | `hdr/mod.rs:381`, `hdr_host.rs:923` |
| list view painter | trait `Theme { open(class), part_defined(theme, part, state) }`, `NoTheme`, `open_theme()` asks class `ListView`, themed only when `IsThemePartDefined(LISTITEM,0)` | `crates/w10-comctl32-lv/src/lv/paint_plan.rs:79-95, 194-`; real implementation `RtTheme` binds `uxtheme.dll` at run time (`OpenThemeData`, `IsThemePartDefined`, `DrawThemeBackground`, `CloseThemeData`) in `userland/guest/comctl32-w10/src/lv_ui.rs:84-175` |
| ComboBoxEx | `Host::check_themed()`, `F_THEMED = 0x40`, `set_window_theme` | `crates/w10-comctl32-lv/src/cbex/mod.rs:125, 209-211` |
| task dialog | `button_floor(..., themed: bool)` | `crates/w10-comctl32-dialogs/src/td_layout.rs:56` |
| trackbar/progress | `GetThemeInt` property numbers TMT_ANIMATION_FPS 0x97a, TMT_ANIMATION_DURATION 0x138e, TMT_HIGHLIGHT_TIMER 0x97c | `crates/w10-comctl32-small/src/consts_bars.rs:122-125` |
| status bar, toolbar, tab, tree, rebar | classic arms only; stock theme calls named in comments (`GetThemePartSize` for the grip, `GetThemeBackgroundExtent`, `GetThemeTextMetrics`) | see 4.3 |
| Edit/Combo (user32-side, Win7 line) | `setup_theme_info` | `userland/guest/comctl32/src/stdctl/{combo.rs:270,edit/mod.rs:1537}` |

Other uxtheme importers on the i7 side: kernel `kernel/kmain/src/win32k/user/api_hook.rs` mentions uxtheme (user-api-hook registration, content not read); chrome.dll m109 imports 6 uxtheme names, `OpenThemeData CloseThemeData DrawThemeBackground GetThemeBackgroundContentRect GetThemePartSize` and ord 47 (`docs/stage4/chrome_imports.tsv`).

## 5. Existing tests that bear on this (inventory belongs to another file; only the i7 facts)

- Win7/no-theme: `userland/wintests/comctl32/comctl32_theme_v6.c` (DrawThemeParentBackground(Ex) via controls, buffered paint pixel hashes; no recorded `theme*.tsv` found in `comctl32/data`).
- Header/list view: `userland/wintests/comctl32/cc20_header_probe.c` (87 verdict lines, cross-built, not run per `COMCTL32-HEADER-W10.md` section 9), `cc28_lv_header_width_probe.c` (branch `lanea/comctl-lv-metrics`, Win10 record bb #1134 reply #15707), **CC30 list-view geometry** `cc30_lv_geometry_probe` (Depot m205; branch `origin/lanea/lv-geometry-oracle`, **not in the local ref set**, source not read here; Win10 std1 rows attached as bb #1134 attachment [1466] `cc30-m205-std1-out.txt`, i7 rows [1465]; neither attachment downloaded: `bb attach` was not tried).
- uxtheme-w10 host tests: `userland/guest/uxtheme-w10/tests/{manifest.rs,behavior.rs}` (bb #9692: 39 tests); `crates/w10-uxtheme-off` unit tests. No test pins any part size.
- `cargo xtask` gates: `uxtheme-w10-build` (export set equals `.def`, imports `user32` + atom API set only).

## 6. The templates to copy (survey / probe list / design)

None of the passes is a three-part research-only file; combine these:

| template | what to copy | path |
|---|---|---|
| `SHELL32-RESEARCH-R1.md` (202 lines) | the only explicitly "read-only research" doc: Evidence-labels block, `## 0 Bottom line`, demand tiers (T0/T1/T2), numbered Unknowns list, Acquisition list | `docs/design/win10/SHELL32-RESEARCH-R1.md` (sections 0, 7, 8, 9) |
| `SSPICLI-W10-LINK.md` (437 lines) | header "Status: RESEARCH + PURE CRATE, NOT WIRED"; labels paragraph; `## 0 Corrections` table; `## 1 Why this family` importer table with source column; `## 2 Facts per area`; `## 3 Design` (seam, modules, wiring proposal, deliberate divergences); `## 4 Test plan`; `## 5 Open items`; later rounds as numbered sections | `docs/design/win10/SSPICLI-W10-LINK.md` |
| `WININET-W10-LINK.md` (201 lines) | `## 1 What this is and is not`; `## 2 The export surface is the corpus table [V-corpus]` (slice/count/class/body/answer table); `## 3 contract per area`; `## 4 Layout and gates`; `## 5 Known limits` | `docs/design/win10/WININET-W10-LINK.md` |
| probe write-ups | probe list shape: file, prefix, rows, expectation source, "NOT RUN" disclosures | `docs/design/win10/W10APP-PROBES.md`, `FFX-IPC-PROBES.md`, `DLG-PAINT-PROBE.md` (oracle-vs-i7 table), `userland/wintests/win10/README.md` table |
| bb conventions | a research pass is announced on the topic, branch `lane*/<tag>`, "local only", explicit `NOT RUN`/`UNVERIFIED` lists | bb #923, #1134, #989 |

Suggested file name by house convention: `docs/design/win10/UXTHEME-THEMEDATA-RESEARCH.md` (UPPER-HYPHEN under `docs/design/win10/`, `docs/personality/NAMING.md` rule); probe `userland/wintests/win10/win10_uxtheme_parts_probe.c` (`win10_<area>_probe.c`, rows prefix three capital letters), record `userland/wintests/win10/expected/win10_uxtheme_parts_probe.win10.txt` (+ `.win10.adm.txt`); crate `crates/w10-uxtheme-tables` (kind `tables`, registry row required); generator `scripts/w10_uxtheme_tables.py` (`--check`).

## 7. Related bb context found (read-only)

- #1134 (7zFM comctl bugs): CC28/CC29/CC30 thread; replies from lanec's CC30 i7 run through #16659 contain the CC30 i7 and Win10 numbers used above and lanea's "(3) HEADER height 24 ... waits for a theme-size probe or lanef's uxtheme plan" (#16657). Owners: lanea comctl32-w10, laneg Win10 oracle, lanec i7 runs, lanef win32k font/caption.
- #923 SOW item (a) "uxtheme-w10 (control theming; visual correctness of every comctl control)" and #9252 (real msstyles theming "not planned").
- #989: proposes an `industrial7_test_<dll>` corpus per DLL (existing public: ntdll, kernel32_kernelbase, advapi32, user32, gdi32_gdifull, msvcrt, win32k, ws2, curl); item 2 is "comctl32 v6 + uxtheme + dwmapi + comdlg32 + shell32 ...". Method: test_pattern LoadLibrary/GetProcAddress/call, Win10 oracle first, determinism x3, privacy scan. Reply #10701 (dot) asks for a paired manifest/no-manifest comctl32 v6 fixture (v5 vs v6 class selection recorded separately from visual appearance): relevant to this corpus.
- #1097 G13 "uxtheme (1h)" probe item: `bb show 1097` shows the line only; no uxtheme probe file exists in the tree.

## 8. UNKNOWN / not found

- Any Win10 oracle row for GetThemePartSize/GetThemeMetric/GetThemeInt/GetThemePosition/GetThemeMargins/GetThemeTextExtent: none found in repo or bb searches (`bb search uxtheme`, `bb search CC30`).
- Whether Win10 `OpenThemeData(hwnd,"Header")` on the oracle returns non-NULL under the Aero Lite/default 19041 theme with 96 DPI and which `.msstyles` the oracle runs (high-contrast, dark mode, scale factor). The oracle's DPI/scale and colour scheme affect every number; CC30 fixtures say dpiY 96 (lanec/laneg rows).
- Composition of 24: part 1 size vs TMT_MINSIZE vs the "M" extent. Not separable from CC30. (Fact-check note: from the decomp, min height = max(part1.cy, MINSIZE.y) and the "M" extent only enters through `text_cy + 2*edge`, which the 24 px exceeds when text_cy is 13-15, so the 24 is most likely part1/MINSIZE; still inference.)
- Policy status of a committed themed-size table (section 3.1 item 6).
- `bb attach` content of CC30 rows (not fetched). Source of `cc30_lv_geometry_probe.c` (branch not local).
- Whether `uxtheme-w10` is in the default staged family image (only in GENERIC and the comctl-subitems gate was found).
- `docs/design/win10/OWNCHROME-ALPHA-PLAN.md`, `RING2-PLAN.md`, `CAPTION-7ZFM-FINDINGS.md`: referenced on bb, not present on master.

## 9. Verification (adversarial fact-check pass)

Re-run / re-read on master `25af4cbccd` of `/home/iceisfun/work/industrial7` and the corpus API. Result: the document is accurate on every load-bearing point; four corrections/additions were applied in place (version, GetThemePosition arg order, part-id names and uninitialised-SIZE hazard, composition-of-24 note).

CONFIRMED (read or re-ran):
- Header layout rule `hdr/layout.rs:55-71`, `Theme.active && height <= min_height` at :60-61: exact. `Theme` struct `hdr/mod.rs:210-222`, `theme_info` default `mod.rs:381-384`, create-time read `mod.rs:514-521`, `WM_THEMECHANGED` re-read `msg.rs:571-573`: exact.
- `hdr_host.rs:923-925` returns `Theme::default()`; module comment lines 4-5 say uxtheme/imm32 are NOT bound: exact.
- Overloading of `Theme.active` (grep): `msg.rs:240`, `drag.rs:274`, `hit.rs:42`, `layout.rs:60,107,214,244,267`: all exist as cited.
- Stock decomp via `GET /api/function/comctl32/_Header_InitTheme/decomp` (RVA 0x85d98): calls GetThemePartSize parts 1,5,4,7 (state 0, TS_TRUE), GetThemePosition(1,0,0xd4b), `HD+0x160 = max(part1.cy, pos.y)`, `HD+0x114 = max(.., part5.cx)`, SM 0x2d: exact. `Header_NewFont` GetThemeTextExtent(...,L"M",-1,0x800): exact. `Header_OnLayout` RVA 0x4b2a8 `height = max(HD+0x4c + 2*edge, HD+0x160)` plus filter-bar add: exact, matches the Rust model.
- uxtheme-w10 census: `exports_manifest.tsv` has 193 data rows; status partial 35 / ni 158; kind real 35 / stub 158; 108 `Ordinal_N` rows (so 85 named); 34 `SHUNIMPL` forwarder rows: all exact. Corpus module version 10.0.19041.2193 (see 1.2).
- `docs/OWNED_STATUS.md:252` is the Win7 uxtheme row (`research only ... rank 2`); `grep -c "uxtheme-w10|shell32-w10"` in OWNED_STATUS = 0; R5 theme row `0 files NOT_SHIPPED` at :637 (the file shows it at line 637; ok). `registry.tsv` lines 300 (w10-uxtheme-off wired), 410/425/426 (propsys/shcore/shell32 unwired), 443 (uxtheme-w10 unwired): exact.
- `DEFAULT_SYS_COLORS` at `w10-win32k-serverinfo/src/lib.rs:97` with the "only BTNFACE observed" comment at :87-96; `caller_is_armed_win10` at `personality.rs:344`: exact. `ls userland/wintests/win10/expected | wc -l` = 114, and no `win10_user32_misc*` or uxtheme record exists: exact.
- CC30 numbers: bb #1134 shows i7 header 20 (text_cy 16 + 2*2), Win10 header 24 / rows 19, `-11 MS Shell Dlg` guifont; attachments [1465] `cc30-i7-rows.txt` and [1466] `cc30-m205-std1-out.txt` exist (not downloaded). Another bb row confirms Win10 header-item rect top 24 bottom 43 vs ours 20/40 for the CC29 fixture.
- SDK constants: HEADERPARTS, `TMT_MINSIZE 3403`, `DT_NOPREFIX 0x800` checked against the mingw headers.

NOT RE-VERIFIED (kept as in the first draft, treat as UNVERIFIED): the 711/833 shell32 figure; line numbers of the tab/tb/status/tv/rebar "theme arms skipped" comments (tab `lib.rs:11` seen, others not opened); `lv_ui.rs:84-175` (seen only the start of the run-time uxtheme section at :84); `comctl32_theme_v6.c` contents (file exists); bb replies #16650/#16656/#16659 numbering (content confirmed, reply ids not re-matched); ranking doc details beyond lines 1-9; `comctl_subitems.rs`/`gates.rs`/`w10_family.rs` line cites; the claim about the dwmapi/glass_rules host tests.

MISSING FINDING added: see the correction rows above (uninitialised `SIZE` when a part-size call fails; GetThemePosition NULL-pPoint order).
