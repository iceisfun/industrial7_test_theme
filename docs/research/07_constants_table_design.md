# 07. Recorded-constants table for the armed-Win10 personality (design)

Role: CONSTANTS-TABLE DESIGNER. Design only, nothing was implemented in `/home/iceisfun/work/industrial7`. Companion files: `docs/research/seed_constants.csv` (247 rows, the seed) and `docs/research/07_seed/{gen_seed.py,probe_part_ids.txt}` (how the seed is produced, so every row can be re-derived).

Inputs read first: `01_consumers.md`, `02_win10_semantics.md`, `03_wine_evaluation.md` (+ `03_wine_probe/out/proton_v6.txt`), `05_i7_current_state.md` (house style, section 3 and 6).

Labels (same vocabulary as 05): [V] fact read from the Win10 decomp / a header / a file in this pass; [DOC] fact taken from another research file in this repo (which cites its own evidence); [WINE] value from a Wine run, NOT Win10; [INF] inference; [choice] a design decision; UNKNOWN = no source.

## 0. Bottom line

1. **The header only needs one recorded number to reach 24**: `HD+0x160 = max(GetThemePartSize(HEADER,1,0,TS_TRUE).cy, GetThemePosition(HEADER,1,0,3403).y)` [DOC 01 section 4.3, 05 section 4.2]. From the Win10 CC30 result (header = 24 for text heights 13..16 with edge 2, i.e. text arm 17..20) it follows that this max is exactly 24 [INF, section 12]. That is the single `serve=yes` Win10-derived row in the seed. Everything else the header asks for (parts 4, 5, 7, text extent "M", content margins) either only matters when larger than a value the header already has or only affects painting.
2. **The table is a lookup keyed by (class, part, state, property, size-enum, dpi)** with typed values AND recorded failure HRESULTs. Recording "this property does not exist, hr=0x80070490" matters as much as recording numbers, because `_Header_InitTheme` and ten other comctl32 sites branch on it (section 2).
3. **A non-NULL handle is a contract.** Once `OpenThemeData(L"Header")` returns non-NULL, parts 1/4/5/7 MUST succeed because `_Header_InitTheme` ignores their HRESULTs and uses an uninitialised `SIZE` otherwise [DOC 05 section 4.2, 01 section 2.3]. And `Tab_RefreshTheme` will draw nothing if `DrawThemeBackground` fails [DOC 01 section 2.3 item 2]. So handle capability must be declared per class (section 5/6), never globally.
4. **Seed honesty**: of 247 rows only 2 are Win10 observations (both second-hand) and 1 is an inference from them; 165 are Wine placeholders (`serve=no`), 15 decomp rules, 27 SDK ids, 36 UNKNOWN rows listing exactly what a Win10 probe still has to deliver. No numeric Win10 part size other than the 24 exists anywhere on this host.
5. **Recommended plumbing**: stage 1 is the host-method route (`Host::theme_info`) fed from the table crate directly, with `Theme` split so `active` is no longer the carrier of the height; the uxtheme-served route comes next and must answer the same tuple identically (equivalence test). See sections 7, 8, 10.

## 1. What the table has to answer (derived from the survey, not invented)

From 01 section 2.1/3/7 and the stock `_Header_InitTheme` decomp (`GET /api/function/comctl32/_Header_InitTheme/decomp`, RVA 0x85d98), the query kinds that real consumers issue, in order of need:

| kind | exports | consumers [DOC 01] | table row type |
|---|---|---|---|
| part size | `GetThemePartSize` (27 comctl32 sites) | header, status grip, trackbar, tooltips, rebar, monthcal, lv groups, check/radio glyphs | `size` with `esize` |
| position | `GetThemePosition` (1 site: header TMT_MINSIZE) | header | `position` |
| int / metric / bool / enum | `GetThemeInt` 13, `GetThemeMetric` 1 (markup-driven), `GetThemeBool` 9, `GetThemeEnumValue` 2 | combobox BORDERSIZE, progress animation, composited flags | `int`, `bool`, `enum` |
| margins | `GetThemeMargins` 10 | lv, edit, combo, rebar, tooltip, toolbar, window NC | `margins` (+ pseudo prop 10000 rewrite, see 4) |
| colour / font | `GetThemeColor` 21, `GetThemeFont` 10 | edit, tv, static, tooltip | `color`, `font` |
| text extent / metrics | `GetThemeTextExtent` 17, `GetThemeTextMetrics` 8 | header "M", toolbar, status | NOT table rows: computed from a font (section 6) |
| existence | `IsThemePartDefined` 9 | combobox, edit, tv, lv gate | `defined` (bool, also usable as the "capability" switch) |
| content rect / extent | `GetThemeBackgroundContentRect/Extent` | header, lv | derived from margins rows |
| class | `OpenThemeData*`, `CloseThemeData`, `IsThemeActive`, `IsAppThemed` | all | class directory + handle table |

## 2. Schema

### 2.1 Key

`(class, part, state, prop, esize, dpi)`

| field | type | notes |
|---|---|---|
| `class` | case-insensitive ASCII id | canonical class names (`Header`, `Button`, `ListView`, `ComboBox`, `ScrollBar`, ...). Aliases (`Button-OK;Button` family, app-qualified `Explorer::ListView`, `DarkMode_*`) live in a separate alias table (section 2.4). Note the probe prefix/case: Wine printed `Scrollbar`/`Combobox`; the seed generator normalises to the comctl32 spelling `ScrollBar`/`ComboBox` (01 section 2); matching on Win10 is ASCII case-insensitive [DOC 02 section 5.1] |
| `part`, `state` | int | `0` = "class/part default"; ids from `vsstyle.h` (numeric ids for the probe table are in `07_seed/probe_part_ids.txt`, generated by running a mingw exe that prints the enums) |
| `prop` | enum | `PARTSIZE`, or a `TMT_*` id from `vssym32.h` (seed section D lists ones verified by grep: 3403 MINSIZE, 3601 SIZINGMARGINS, 3602 CONTENTMARGINS, 2403 BORDERSIZE, 2401 IMAGECOUNT, 2416/2417 WIDTH/HEIGHT, ...). Pseudo/derived props start with a non-numeric tag (`min_height=...`, `HDM_LAYOUT...`) and are `type=derived`/`layout`: they are tests/oracles, not served |
| `esize` | `TS_MIN/TS_TRUE/TS_DRAW` | only for `PARTSIZE` |
| `dpi` | 96/120/144/192/... | see 2.3 |

### 2.2 Value and result

`type` in {`size`, `int`, `bool`, `enum`, `color`, `margins`, `position`, `rect`, `string`, `font`, `intlist`, `const`, `rule`, `derived`, `layout`}. `v1..v4` hold the payload (size: cx,cy; margins: l,r,t,b; position: x,y). `hr` is the recorded HRESULT: empty/`0x00000000` = S_OK, otherwise the recorded failure. A row whose `hr` is a failure serves that failure (this is what makes `GetThemePosition(...MINSIZE)` miss correctly). `relation` (`eq`/`ge`/`le`) lets the table store a bound when only an inequality is proven; a `ge` row is never served, only asserted.

`confidence` vocabulary (column in the CSV): `ORACLE` (recorded Win10 row, six boots identical, see 5 section 3.1 item 2) > `ORACLE-REPORTED` (Win10 value quoted second-hand from a bb post) > `ORACLE-INFERRED` > `DECOMP` (rule proven from decomp, no number) > `SDK-HEADER` > `WINE-TESTED-ON-WINDOWS` (Wine test that is written to pass on real Windows) > `WINE-RUN` (Wine placeholder) > `UNKNOWN`. `serve` is `yes` only for `ORACLE`, `ORACLE-INFERRED` (header min height, with a test-only flag until recorded), `DECOMP`, `SDK-HEADER`. Wine rows are `serve=no` always (03 section 7 item 4: Wine numbers are placeholders/cross-checks).

### 2.3 DPI

Win10 does two different things [DOC 02]:
* image parts: `SelectCorrectImageFile` picks a DIB by DPI plateau, then `TS_TRUE` returns that DIB frame size (02 section 6.1). So image-part sizes are **not** `MulDiv`-derivable; they need one recorded row per plateau (96, 120, 144, 192 at least). A missing plateau must not be interpolated silently: serve the nearest recorded plateau and mark the call as `dpi_fallback` in the debug counter, or fail (see 6). Wine's tested rule `expected = MulDiv(13, dpi, 96)` for the checkbox (`dlls/uxtheme/tests/system.c:1219`) is Windows-verified for that one glyph only.
* metrics/margins/ints: `GetThemeMetric` = lookup then `ScaleThemeSize(hdc)`; `GetThemeMargins` re-scales by object dpi (`ScaleMargins`) [DOC 02 section 6]. Store the 96-dpi value and apply `MulDiv(v, dpi, 96)`; exact rounding in `ScaleThemeSize` is UNKNOWN (not read in this pass): record a 120/144 probe row to pin it.
* The header also reads `SM_CYEDGE` through `DPISCALEINFO::GetLegacyScaledEdgeHeight = MulDiv(GetSystemMetrics(0x2e), dpi, 96)` (0x4b384) and seeds `HD+0x114` from `GetSystemMetricsForDpi(72)` [DOC 01 section 4.4]: those are user32 metrics, not table rows.

Seed holds 96-dpi rows plus UNKNOWN placeholders for 120/144/192 of Header part 1.

### 2.4 Class directory and aliases (not in the CSV yet)

`class_alias` rows: `Button-OK;Button` ... `Button-APPLY;Button` (13-entry table at comctl32 RVA 0x1f3410 [DOC 01 section 2.2]) resolve left to right to the first class that exists in the directory, so with a directory that has `Button` but not `Button-OK`, all 13 resolve to `Button`. Rule is the `MatchThemeClassList` rule (section 4). `Indeterminate::Progress`, `CompositedWindow::Window`, `EditComposited`, `Explorer`, `DarkMode_*` are app-qualified names: stage them as "resolve to the plain class, record deviation" until a probe says Win10's Explorer sub-app differs for the queried (part, state) tuples. Whether it does for Header is UNKNOWN.

### 2.5 Inheritance

Real uxtheme resolves a miss through `GetValueIndex`: out-of-range part/state use the part/state table default, then the base-class chain [DOC 02 section 6 last paragraph]; the precise precedence between state fallback and part fallback is not decoded. **[choice]** the table does not implement inheritance: it stores fully expanded rows for exactly the tuples consumers issue (the probe list) and answers any other tuple with `0x80070490` ("property absent") for getters. This is deliberately stricter than Win10 and can only make a consumer take its fallback arm, never garbage. A later stage may add `(part, 0)` / `(0, 0)` default rows once their semantics are probed (probe: same property at (p,s), (p,0), (0,0) for a part where the class sets only a class-level value).

## 3. File format and house style

Follow 05 section 3.1 items 3/4/6:
* A **corpus/oracle-derived TSV** under the new crate's `data/` (crate `crates/w10-uxtheme-tables`, kind `tables`, registry row needed [DOC 05 section 6]); tab separated; `#` header lines giving provenance and "do not edit, GENERATED by scripts/w10_uxtheme_tables.py"; a `--check` mode; digest pinned. The source column cites the oracle record file + row prefix, exactly like `w10_user32_metric_slots.tsv` (`#sm_index slot source`).
* The **recorded probe rows** are the input, not hand-typed TSV: probe `userland/wintests/win10/win10_uxtheme_parts_probe.c` (prefix e.g. `UXT`), rows `UXT <group>.<step> class=Header part=1 state=0 prop=PARTSIZE esize=TRUE dpi=96 hr=0x00000000 cx=.. cy=..` (no pids/handles/addresses; printable ASCII, LF) committed as `expected/win10_uxtheme_parts_probe.win10.txt` (+ `.win10.adm.txt`), six runs byte-identical before a row is trusted. The generator turns those rows into the TSV. This keeps the "implementation table typed independently of the record and compared" test pattern (`tests/manifest.rs` style).
* This repo's seed is **CSV** (the task asked for it and it carries free-text sources); the production artefact is the TSV above. The CSV columns map 1:1 onto the TSV (`class part state prop esize dpi type v1..v4 hr relation confidence serve source`).
* Rust access: a `no_std` crate that `include_str!`s the TSV and exposes `fn lookup(class: ClassId, part: u16, state: u16, prop: Prop, esize: Option<Esize>, dpi: u32) -> Lookup { Hit(Value), Fail(hr), Absent }`. `Absent` is distinct from `Fail`: `Fail` is a recorded Win10 failure, `Absent` is "never recorded", and the DLL layer maps `Absent` to `0x80070490` plus a debug counter so unrecorded tuples are visible.
* **Policy #550 open question** [DOC 05 section 3.1 item 5, 8]: whether a table of themed integer sizes/margins counts as "small constants" is UNKNOWN. This design keeps integer sizes/margins/ints/bools only; no bitmaps, no `.msstyles` bytes, no colour tables beyond what a probe shows. Colours are the riskiest datum and are deferred to a later stage and flagged for an owner decision.

## 4. How each export is served

"Table" = the `lookup` above. "T-off" = existing themeless answer from `w10-uxtheme-off` [DOC 05 section 1.3]. The error-order columns copy the decomp-proven order [DOC 02 section 6, 6.1, 6.2].

| export | armed behaviour | notes / deviations |
|---|---|---|
| `OpenThemeData`, `OpenThemeDataEx`, `OpenThemeDataForDpi`, `OpenNcThemeData`(#49) | keep the Win10 check order: `hwnd` bad -> NULL/0x80070006; NULL list -> NULL/0x80004003; `SetLastError(0)`; app-flag gate; empty list -> 0x80070057; then class-list match over the **class directory** (`;` list, first success, `App::Class`, SubIdList prop override) and, on a hit, `OpenHandle` (section 5). Success LastError 0; miss NULL + 0x80070490 | the class-directory membership is a per-class `capability` (below). A class not in the directory behaves exactly as today (NULL), so un-staged classes keep their classic arm for free |
| `CloseThemeData` | S_OK for a live handle (decrement openCount), E_HANDLE otherwise, LastError untouched | as 02 section 4 |
| `GetThemePartSize` | handle -> E_HANDLE; `psz==NULL` -> E_POINTER (note: handle check first here); then row `(class,part,state,PARTSIZE,esize,dpi)`: `Hit` -> S_OK; `Fail(hr)` -> hr; `Absent` -> **see 6.2** | border-fill parts: store `BORDERSIZE` and `bgtype=borderfill`; compute TS_MIN `max(1,2bs)`, TS_TRUE `2bs+1`, TS_DRAW `E_INVALIDARG` by the decomp rule (seed rows `PARTSIZE.borderfill.*`) instead of storing 3 sizes. Image parts: TS_MIN is `max(1, sum(content margins))` by the decomp rule but TS_TRUE needs the recorded frame size; TS_DRAW needs `prc`: recorded only for `prc==NULL`, anything else is a documented deviation (serve TS_TRUE) |
| `GetThemePosition` | `pPoint==NULL` -> E_POINTER **before** handle (05 correction); handle; row of type `position`; absent -> 0x80070490 | Header MINSIZE (3403) is the case that matters; a recorded "absent" is a valid and likely answer |
| `GetThemeInt`, `GetThemeBool`, `GetThemeEnumValue`, `GetThemeColor`, `GetThemeRect`, `GetThemeString`, `GetThemeFilename`, `GetThemeIntList` | out-pointer NULL -> E_POINTER; handle -> E_HANDLE; row; absent -> 0x80070490 | filename/string: serve only recorded rows; never invent a file name |
| `GetThemeMetric` | as above then `MulDiv(v, dpi, 96)` of the `hdc` dpi (or object dpi) | rounding UNKNOWN (2.3) |
| `GetThemeMargins` | as above, incl. `prop==10000` pseudo id (rewrite to 3602 when `prc==NULL`, else E_INVALIDARG [DOC 02 section 6]); if an image part has stored zero margins Win10 recomputes from the image: **table must therefore store the effective (post-recompute) margins** as recorded by the probe, never raw zero | Wine shows many parts with `hr=0x80070490 l=0 r=0 t=0 b=0` for CONTENT margins, i.e. absent -> the effective margins are 0 and the recompute path is what supplies them on Win10; do not copy Wine's zeros |
| `GetThemeFont` | row type `font` (LOGFONT) scaled by dpi; font ids are the 801..809 family | not needed for the header; later stage |
| `GetThemeTextExtent`, `GetThemeTextMetrics` | compute with GDI on a font chosen by the part (`TMT_FONT` row or the DC's selected font): `DrawText(DT_CALCRECT)`-style measurement via the user32/gdi32 the DLL already can import | **not table values**: depends on the live font. Header calls it with `L"M"`, flags `DT_NOPREFIX (0x800)`, and falls back to `"..."` GDI extent if it fails [DOC 01 section 4.2]. Stage 1 may simply return failure so the fallback runs (harmless: the 24 floor dominates, see 8) |
| `GetThemeBackgroundContentRect` / `Extent` | derived from effective margins rows (rect inset / outset) | exact decomp rules in 02 section 6.2 |
| `IsThemePartDefined` | TRUE iff a row exists AND the part's class has `draw` capability (6.1); `0` + LastError per 02 section 6 | **deviation**: Win10 says TRUE for every stock part; a size-only class must say FALSE for parts whose consumer uses it as the "draw themed?" gate (list view, combo, edit, tree view per 05 section 4.4) |
| `IsThemeActive` / `IsAppThemed` | see 7: stays FALSE until a draw-capable class set exists; invariant `IsAppThemed => IsThemeActive` kept [DOC 02 section 8] | flipping TRUE is global: 25/25 importers [DOC 01 section 6] |
| `GetThemeSys*` | `hTheme==NULL`: unchanged (forward to `GetSysColor`/`GetSystemMetrics`/SPI); handle non-NULL: serve the sysmetrics block rows (colours 0..30, ids 1201..1210, fonts 801..809, 1301, 1001, strings 1401..1404) | not needed for the header; the block is where `DEFAULT_SYS_COLORS` and the BTNFACE F0F0F0 observation belong, and 02 section 7 notes it is "not obtainable without a loaded theme", so it needs its own probe set |
| `GetCurrentThemeName`, `GetThemeDocumentationProperty` | return the recorded names (`C:\Windows\Resources\Themes\aero\aero.msstyles`-style strings are UNKNOWN: record them) | a string table of three names, no file access |
| `DrawTheme*`, `HitTestThemeBackground`, `GetThemeBackgroundRegion`, `GetThemeBitmap`, `BufferedPaint*`, animation | see section 6 | |
| `SetWindowTheme`, `EnableThemeDialogTexture`, `SetWindowThemeAttribute`, `EnableTheming` | keep the existing window-property implementation (`w10-uxtheme-off`) | these are orthogonal to the table |

## 5. Handle table

Copy the Win10 model [V 02 section 4]; it is observable and cheap:
* `HTHEME = (generation << 16) | slot`, 16 bits each; fresh slot generation 1; reuse increments; `0xFFFF` wraps to 1; a slot entry `{class_id, dpi, flags, generation, openCount, refCount, dead, hwnd}`. Handle 0 is never valid (`CloseThemeData(NULL)` = E_HANDLE [INF in 02]).
* Same `(class_id, dpi, nc flag)` reopen returns the **same** handle value with `openCount++` (02: "two OpenThemeData(hwnd, L"Button") calls on the same dpi return identical handles"); each needs its own Close.
* Every API call takes a temporary ref so a concurrent Close cannot free under it; free when `openCount` and `refCount` are 0.
* The slot stores `capability` copied from the class directory at open time (a theme "version change" must not change an open handle). `WM_THEMECHANGED` (0x31a) behaviour is unchanged: existing handles stay valid until closed; consumers re-open [DOC 02 section 13].
* Per-process table under a lock, like `g_pRenderList`. No ALPC, no section objects.

## 6. What stays themeless

### 6.1 Per-class capability (the "A/B" ladder)

Each class in the directory carries one of:

| level | meaning | handle returned | `IsThemePartDefined` | `DrawTheme*` |
|---|---|---|---|---|
| `off` | not in directory | NULL (today's behaviour) | n/a | n/a |
| `size` | metric/size/margin/int answers only | non-NULL | FALSE except where a consumer needs TRUE for sizing | fail (6.2) |
| `draw` | a painter exists (classic-look painter driven by the table, or real bitmaps later) | non-NULL | TRUE for staged parts | S_OK |

Stage 1 puts only `Header` at `size`. [choice] Rationale from the decomp: `Header_DrawItem`, `TV_DrawItem`, `Button_DrawThemed`, `ThemeDrawEdge` check the `DrawThemeBackground` HRESULT and fall back to classic drawing, while `Tab_RefreshTheme`/`VertDrawEdge` do not [DOC 01 section 2.3 item 2]. Header is therefore safe at `size`; Tab, and any class with an unchecked draw site, must stay `off` until `draw`.

### 6.2 Fallback rules for draw and unrecorded queries

* `DrawThemeBackground(Ex)`, `DrawThemeText(Ex)`, `DrawThemeEdge`, `DrawThemeIcon`, `DrawThemeParentBackground*` on a `size` handle: argument checks exactly as Win10 (section 6.2 of 02: `hdc==NULL` -> E_HANDLE etc.), then return **`E_FAIL` (0x80004005)** [choice; it is the value Win10 itself returns when no part object exists, 02 section 6.1 step 3]. This sends checked draw sites to their classic arm.
* `GetThemeBackgroundRegion` / `HitTestThemeBackground`: border-fill hit tests are derivable from margins (02 section 6.2); image parts: fail with E_FAIL on `size` handles.
* `IsThemeBackgroundPartiallyTransparent`: FALSE on `size` handles (draws nothing, so nothing to composite) [choice].
* `BufferedPaint*` unchanged: required independently of theming [DOC 01 section 2.3 item 3].
* `GetThemeTransitionDuration`: return a failure (e.g. 0x80070490) so animated fades are skipped; 8 sites [DOC 01 section 2].
* `Absent` for `GetThemePartSize` on a `size` handle: this is the uninitialised-`SIZE` hazard for the header. Rule: **a class may be set to `size` only if every `PARTSIZE` tuple its init code requests is `Hit`**. The generator `--check` must fail the build if the Header class lacks any of parts 1/4/5/7 state 0 `TS_TRUE`. For other classes (not ignoring HRESULTs) `Absent` -> 0x80070490.
* Pixel parity is not a goal of the table (see risks): a Win10 height with a classic look is the expected stage-1 result.

## 7. Personality switches

Layering follows `docs/personality/KNOBS.md` conventions as described in 05 section 3.2/3.3 (grammar `\industrial7\personality`, "ARMED Win10 callers", ESP file knobs like `I7_GDI_TRUETYPE_UI` / `\industrial7\gdi-truetype-ui`, default OFF). The existing "armed" predicate is a kernel one (`caller_is_armed_win10`), while uxtheme-w10 is a userland DLL, so the table is applied by **image selection** (the uxtheme-w10 family image only loads for Win10-family processes) plus a knob:

| switch | values | default | effect |
|---|---|---|---|
| A: personality image | Win7 `uxtheme`/owned comctl32 (themeless) vs `uxtheme-w10` | existing image selection (`I7_FAMILY_W10_DLLS`) | the table never reaches Win7 processes |
| B: `uxtheme-theme-level` knob (name is a [choice], not an existing knob) | `0` themeless (today), `1` header size-only, `2` + sizes for staged classes, `3` + draw capability for staged classes (and `IsThemeActive` TRUE) | `0` until the stage-1 gate passes, then `1` | selects which classes are in the directory and at which capability, so every stage is reversible without a rebuild |
| C: `dpi_fallback` | `nearest` / `fail` | `nearest` with a counter | 2.3 |
| D: per-class override list | e.g. `Header=size,Tab=off` | empty | bisecting a regression to one class |

The comctl32 side needs no switch of its own in the uxtheme-served route: it already does "handle NULL -> classic". In the host-method route (section 8, option A) the same knob value must be passed to `Host::theme_info`, otherwise two sources could disagree: single source of truth is the table crate's `level()`.

## 8. How the header picks up its 24 px

State today [DOC 05 section 4]: `Header::layout` (`crates/w10-comctl32-lv/src/hdr/layout.rs:55-71`) raises `height` to `Theme.min_height` only if `Theme.active`; `hdr_host.rs:923-925` returns `Theme::default()`; `comctl32-w10` does not import uxtheme. Stock does the same max keyed on the theme HANDLE (`HD+0x130 != 0`), not on a flag.

Two routes, and the order matters:

**Route A (stage 1): table consumed by the host, no uxtheme call.**
1. Split `Theme` into `ThemeMetrics { min_height, dropdown_width, overflow_w, overflow_h, sort_arrow_w, sort_arrow_h }` (layout inputs, valid whenever a handle would exist) and `Theme.active` (the behaviour/paint flag that today also flips hot-tracking `msg.rs:240`, `drag.rs:274`, hit rect `hit.rs:42-46`, sort arrow width `layout.rs:107`, focus rect `:214`, split-button `:244`, hot text colour `:267`). The layout rule becomes `if metrics.present && height <= metrics.min_height` — mirroring stock's handle gate. `active` stays false in stage 1, so none of the other behaviours change. This split is the single most important code decision: setting `active:true` just to get 24 would change seven other behaviours [DOC 05 section 0 item 3].
2. `hdr_host.rs:923` returns `ThemeMetrics` built from the table the way the decomp does: `min_height = max(lookup(Header,1,0,PARTSIZE,TS_TRUE).cy, lookup(Header,1,0,3403).y if Hit)`, `dropdown_width = max(HD+0x114 seed, lookup(Header,5,0,PARTSIZE,TS_TRUE).cx)`, etc. Fields whose Win10 value is UNKNOWN stay at `Theme::default()` (0): `max(...)` with 0 changes nothing, so an unrecorded part-5/4/7 value degrades gracefully to the classic seed instead of guessing.
3. Re-read on `WM_THEMECHANGED` (already done, `msg.rs:571-573`).
4. The themed `"M"` text extent is deliberately skipped (as `hdr_host.rs:765-775` already does): the 24 floor dominates the text arm for 13..16 px text, so skipping it cannot change the 24 [DOC 01 section 4.2 and the arithmetic in section 12]. It would matter for text taller than 20 px (e.g. 150% DPI), recorded as a stage-3 probe.

**Route B (stage 2/3): through uxtheme.** `OpenThemeData(hwnd, L"Header")` returns a `size` handle; `comctl32-w10` header binds uxtheme at run time (as the list view already does in `lv_ui.rs:84-175`) and runs the stock `_Header_InitTheme` call sequence verbatim. For this route to be correct the DLL must satisfy the "HRESULT ignored" hazard (6.2). Route A and B must produce identical `(min_height, dropdown_width, ...)` for the same table: that equivalence is a host test (section 9). After B is live, route A's table call can be deleted so there is one code path.

Expected result at 96 dpi: `text_cy 16, edge 2 -> 20 <= 24 -> 24`; `text_cy 13 -> 17 -> 24`; `text_cy 21 -> 25 > 24 -> 25` (rule from the decomp, unchanged by the table).

## 9. Test strategy

1. **Table self-consistency (host, no Win10)**: generator `--check`: every row has a source and confidence; no `serve=yes` row has confidence below `DECOMP`/`ORACLE-INFERRED`; Header parts 1/4/5/7 state 0 `TS_TRUE` present (6.2); `relation=ge` rows never served; digest pinned.
2. **Rule tests from the decomp**: border-fill TS_MIN/TRUE/DRAW formulas, error-order matrix (E_POINTER vs E_HANDLE per export, including the `GetThemePartSize` handle-first and `GetThemePosition` pointer-first inversions), class-list matching (`;`, `::`, case, SubIdList override), handle generation/reuse rules, `OpenThemeData` LastError matrix (02 section 13). These are exactly the scenario rows of 02 and need no new Win10 recording; they also form the test list for the corpus in this repo (which runs on Win10 and tells us if the decomp reading was wrong).
3. **Oracle replay**: for every recorded probe row, a host test `include_str!`s `expected/win10_uxtheme_parts_probe.win10.txt` and replays each `UXT` row through the served API (`tests/manifest.rs` pattern). Until recorded, only the `ORACLE-INFERRED` header row exists, so the replay is the header layout test: `layout.pos 0,0,W,24` for text heights 13/15/16 and edge 2, plus the "wins only if >=" row (text 21 -> 25), in `crates/w10-comctl32-lv/tests/hdr_layout.rs` next to the existing `min_height: 24` tests (lines 51-58 [DOC 05]).
4. **Differential against Win10 (this corpus)**: `tests/<area>` in this repo, run on real Win10 and on Industrial7: (a) `HDM_LAYOUT` cy on a header, the CC30 observable, (b) the exact `_Header_InitTheme` tuple set, (c) the existence matrix `IsThemePartDefined` and `OpenThemeData` returns for each class at each capability level. The probe output is the table's input AND the regression test (same file).
5. **No-surprise gate**: with level 0 the full existing uxtheme-w10 manifest tests (39, bb #9692 [DOC 05 section 5]) must stay byte-identical; with level 1 only the Header class changes (assert `OpenThemeData` for every other class is still NULL/0x80070490 and `IsThemeActive` still FALSE).
6. **Wine's role** (03 section 6/7): harness smoke only. Run the probe under `tools/wine_run.sh` to see that the test exes load and not crash; never assert Wine numbers. Note that Wine's header stays 20 (and 17) with or without theme in all 8 runs [DOC 03 section 4.1], so Wine cannot confirm the 24 either.
7. **Visual**: out of scope for the table; Industrial7 stage 1 intentionally paints classic at themed height (hybrid), as the Win7 waiver already does [DOC 05 section 4.3].

## 10. Staged plan

| stage | scope | needs recording | exit gate |
|---|---|---|---|
| 0 | this seed + the Win10 probe (header tuple set below) run by laneg | yes | six identical boots |
| 1 | `Header` size-only: `ThemeMetrics` split + table-fed `hdr_host`, level 1, `min_height=24`; uxtheme unchanged | only the 24 (already inferred); better: part-1 cy, MINSIZE, parts 4/5/7 | `HDM_LAYOUT` cy 24, CC30 row matches; nothing else in `hdr/*` changes behaviour |
| 2 | uxtheme-w10 serves `Header` through real handles (`OpenThemeData`, `CloseThemeData`, `GetThemePartSize`, `GetThemePosition`), handle table, class directory, error order; header binds at run time; route A deleted | no new rows | equivalence test A==B; manifest tests unchanged at level 0 |
| 3 | size/margin/int rows for the classes with unchecked-free layout use: Status (grip), TrackBar, Tooltip, Rebar, MonthCal, ListView groups, Button check/radio/commandlink glyph sizes; `GetThemeMargins`, `GetThemeInt`, `GetThemeBackgroundContentRect`; `IsThemePartDefined` FALSE | yes (01 section 7 list, per dpi 96/120/144) | each consumer's layout row matches Win10 CC-series records |
| 4 | text metrics/extents and fonts: `GetThemeTextExtent/Metrics/Font`, `GetThemeSys*` block, `GetCurrentThemeName` | yes (fonts, sysmetrics) | CC30 text rows |
| 5 | draw capability: classic-look painter per part driven by table colours/margins (or real bitmaps if #550 is decided), `IsThemeActive/IsAppThemed` TRUE, Tab/Button/ListView/Edit/Combo themed arms | yes (colours) and an owner decision on bitmaps | per-control pixel differential; **Tab must not be enabled before this stage** |
| 6 | DPI plateaus, dark mode/`DarkMode_*`, HC gate, `OpenThemeDataEx` flags | yes | scenario matrix of 02 section 13 |

## 11. Risks

1. **`IsThemeActive`/`IsAppThemed` blast radius**: global; do not flip before stage 5 (25 importers). The header does not need it (it keys on the handle).
2. **Non-NULL handle commits consumers to their themed arm** (hazards in 01 section 2.3): mitigated by per-class capability and the generator `--check`; residual risk: consumers in the 3000-module corpus that call `OpenThemeData` and have unchecked draw paths (Chrome m109 imports 6 names, all fail-soft [DOC 05 section 4.4]; third-party apps unknown).
3. **`Theme.active` overloading** (section 8): if the split is skipped, stage 1 silently changes hot tracking, hit rects, sort arrow width, focus rectangle and split-button paint.
4. **Wine contamination**: Wine's theme changed between Wine 9 and 11 (two sys colours) [DOC 03 section 4.2]; its header item is 6x24 which happens to equal the owner's 24, one data point only. Wine rows are `serve=no`; a generator flag must refuse to promote `WINE-RUN` rows.
5. **Frame vs strip ambiguity**: whether Win10 `TS_TRUE` returns the per-state frame or the whole strip for header items was not decoded (03 section 9 caveat). Only an on-Win10 probe settles it; the 24 itself is proven by the layout.
6. **Policy #550** (section 3).
7. **Composition of 24 unknown** (part size vs MINSIZE). Irrelevant for stage 1 (both feed a max), relevant if a different font/DPI changes only one of them (it does not: both are theme constants, but MINSIZE may be absent at some DPI plateau).
8. **DPI plateau and rounding**: per-plateau rows needed; rounding in `ScaleThemeSize` unread.
9. **Dark mode / HC / sub-apps** change class resolution; staged out (stage 6) but the alias table must not silently map `DarkMode_*` to light values without a counter.
10. **Table drift between two consumers** (host route and DLL route): one `lookup` crate, one `level()`.
11. **Window-lifetime cases not modelled**: `hwnd`-dependent behaviour (per-window DPI, `SetWindowTheme` replacing the class list) is only partly in the handle slot; keep the existing window-property code for that.

## 12. Recorded versus derivable

| datum | status | how |
|---|---|---|
| error orders, HRESULT values, LastError matrix, handle format, class-list grammar, border-fill size formulas, `TS_MIN` image formula, header consumer arithmetic, `GetThemeAppProperties` default 3 | **derivable from decomp** (seed `DECOMP` rows) | already read, 02/01 cite RVAs |
| HP_*/TMT_*/TS_* ids | derivable from SDK headers (seed `SDK-HEADER`) | grep |
| header minimum height 24 | inferred from the Win10 CC30 layout result | arithmetic below |
| every image-part frame size (`TS_TRUE`), `TMT_MINSIZE` presence/value, content/sizing margins, image counts, BORDERSIZE of border-fill parts, colours, fonts, sysmetrics block, theme names, transition matrices | **must be recorded on Win10** (decomp reads from `aero.msstyles`, which is not in the corpus) | probe list below, or direct extraction of `aero.msstyles` resources if a lane can dump the file (03 section 4.4 recommends parsing over feeding Wine) |
| `ScaleThemeSize` rounding | readable from decomp but NOT read in this pass | UNKNOWN; or a 120/144 probe |
| text extents | computed per font | not a constant |

The arithmetic for the one inferred row: Win10 `Header_OnLayout` gives `height = HD+0x4c + 2*edge`, replaced by `HD+0x160` when the theme handle exists and `height <= HD+0x160` [V, decomp RVA 0x4b2a8 via 01 section 4.1, 05 section 4.2]. CC30 measured 24 with fonts whose text arm is 17..20 (`tmHeight` 13 for MS Shell Dlg -11, 15 for Segoe UI -12, plus `2*SM_CYEDGE` = 4) [DOC 05 section 4.1]; the result can only be 24 if `HD+0x160 = 24` (the alternative, a text arm of exactly 24, needs a 20 px text height that none of the fixtures have). Not independent of the premise that SM_CYEDGE is 2 on the oracle (listed UNKNOWN in the seed); if the edge were larger the conclusion `HD+0x160 = 24` would still hold as long as the text arm stays below 24.

### Minimum Win10 probe for stage 1 (handed to the probe-list role)

`GetThemePartSize(Header,1,0,TS_TRUE|TS_MIN|TS_DRAW)`, `GetThemePosition(Header,1,0,3403)` (record hr), `GetThemePartSize(Header,{4,5,7},0,TS_TRUE)`, `GetSystemMetrics(SM_CYEDGE=0x2e)` and `GetSystemMetricsForDpi(72, dpi)`, `GetThemeTextExtent(Header,1,0,L"M",-1,0x800)` for the default 9 pt font, and `HDM_LAYOUT` cy; at 96/120/144/192 dpi where the lane can. These 15-20 numbers replace every `UNKNOWN` header row in the seed.

## 13. The seed: `seed_constants.csv`

Columns: `class,part,state,prop,esize,dpi,type,v1,v2,v3,v4,hr,relation,confidence,serve,source`. Counts: 247 rows = 165 `WINE-RUN`, 36 `UNKNOWN`, 27 `SDK-HEADER`, 15 `DECOMP`, 2 `ORACLE-REPORTED`, 1 `ORACLE-INFERRED`, 1 `WINE-TESTED-ON-WINDOWS`. Regenerate with `python3 docs/research/07_seed/gen_seed.py`.

* `WINE-RUN` rows are parsed mechanically from `docs/research/03_wine_probe/out/proton_v6.txt` (Proton 11.0 under Xvfb, 96 dpi; `source` carries `file:line`); numeric part/state ids come from `07_seed/probe_part_ids.txt`, produced here by cross-compiling a table-only copy of the probe's `parts[]` with `x86_64-w64-mingw32-gcc` and running it with `tools/wine_run.sh` (console exe, `DISPLAY` unset, null display driver; `ss -x | grep -c X11-unix` was 43 before and after; no wineserver left). All Wine rows are `serve=no`.
* The three Header sizes the stock init also asks for (parts 4, 5, 7) are in the CSV as Wine values (13x5, 3x24, 8x21) only. Part 5 cx=3 is almost certainly irrelevant (the code takes `max(HD+0x114, cx)` and `HD+0x114` is seeded from `SM_CYMENUCHECK`), but that is [INF] since the SM value was not recorded.
* Win10 rows: HDM_LAYOUT cy 24 (`ORACLE-REPORTED`, test-only: quoted from 05 which quotes bb #1134 reply #16656; not re-read from the bb here), the derived `min_height 24` (`ORACLE-INFERRED`, `serve=yes`), COLOR_BTNFACE F0F0F0 (`ORACLE-REPORTED`, a user32 colour, not a theme row), and BP_CHECKBOX TS_DRAW 13x13 at 96 dpi (`WINE-TESTED-ON-WINDOWS`, from `wine/dlls/uxtheme/tests/system.c:1219-1225`; the tested state is CBS_CHECKEDNORMAL=5 (vsstyle.h) while the seed row uses state 1, the part-level size is assumed equal: `serve=pending`).
* `DECOMP` rows hold rule identifiers in `v1` (no numbers) and cite RVAs: the table-driven DLL implements them as code, the CSV records them so each rule has a test row.
* `UNKNOWN` rows enumerate exactly what the Win10 probe must still supply (Header 1/4/5/7 + MINSIZE + text extent + margins, SM_CYEDGE, the 01 section 7 cross-control list, 120/144/192 dpi plateaus for the header). Property id 3603 in the Window row is `TMT_CAPTIONMARGINS` (vssym32.h:351).

## 14. Open items / UNKNOWN

* Everything in the UNKNOWN rows; in particular no Win10 number other than the header 24 exists.
* Whether the section-7 knob name exists/is acceptable to `docs/personality/KNOBS.md` (the doc was not read in this pass; grammar taken from 05).
* Whether a themed-size table passes policy #550 (05 section 3.1 item 5).
* `ScaleThemeSize` rounding; image `TS_TRUE` frame-vs-strip; precedence of part vs state fallback in `GetValueIndex`.
* Whether Win10's Explorer/`DarkMode_Explorer` sub-apps give different Header numbers.
* Win10 `TS_DRAW` with non-NULL `prc` for image parts (the table stores `prc==NULL` only).
* Not done: nothing was built or tested against Industrial7; the host-test names in section 9 are proposals, the existing ones (`tests/hdr_layout.rs:51-58`, `hdr_trace_replay.rs:118`) are quoted from 05.
