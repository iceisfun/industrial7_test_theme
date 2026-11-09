# 04 - Existing-test inventory: uxtheme and themed-control behaviour

Scope: what tests exist today (Wine, our corpora on this host, the Industrial7 repo) for Windows 10 x64 `uxtheme.dll` (10.0.19041.2193) and for the themed behaviour of consumer controls (comctl32 6.10.19041.1110, user32, gdi32), and what a new corpus must add. Research only; nothing was run on Windows and no Wine process was started for this document (the work is source and repo inspection; commands below are the greps/reads that produced each claim).

Labels: **[V]** verified by reading the cited file/line or by a command whose output is quoted; **[I]** inference; **UNKNOWN** not established.

## 1. Bottom line

1. **Our own corpus is empty for uxtheme.** `/home/iceisfun/industrial7_test_theme/tests/` contains one area, `core_smoke` (11 lines, `smoke.c`: module loads, a kernel32 negative GetProcAddress, a child-process round trip). No call to any uxtheme export. [V: `ls tests; cat tests/core_smoke/smoke.c`]
2. **No other corpus on this host touches uxtheme behaviourally.** Of 13 `~/industrial7_test_*` / `~/test_*` directories, the only hit outside `industrial7_test_theme` is `test_win32k/tests/win32k_mitigation/sc_load.c:71`, which `LoadLibraryA("uxtheme.dll")` in a list of GUI-importing DLLs and only `t_note`s the result. `test_user32`, `test_gdi32_gdifull`, `test_ole`, `test_dwrite`, etc.: zero uxtheme / OpenThemeData / IsThemeActive / WM_THEMECHANGED hits. [V: grep, section 5]
3. **Wine's `dlls/uxtheme/tests/system.c` (2947 lines, one file) is the only third-party uxtheme test suite found.** It exercises 40 of the 193 Win10 exports (34 named exports plus 6 ordinals, 2 of which are SHUNIMPL forwarders on Win10) and is heavily weighted to *API error/argument semantics* and *window/property plumbing*. It reads almost no theme data: **one** `GetThemePartSize` combination (`BP_CHECKBOX / CBS_CHECKEDNORMAL / TS_DRAW`, expecting 13 px at 96 DPI), `GetThemeInt` only for `TMT_TRUESIZESTRETCHMARK`/`TMT_MINDPI*`, and no `GetThemeMetric`, `GetThemeColor`, `GetThemeMargins`, `GetThemePosition`, `GetThemeFont`, `GetThemeSys*`, `GetThemeRect`, `GetThemeString`, `DrawThemeBackground`, `GetThemeBackgroundContentRect`. Nothing for `HP_HEADERITEM`, tab, listview parts. [V, section 3]
4. **The CC30 symptom (header 24 px on Win10 vs 20 px) is covered by no test anywhere.** Wine `comctl32/tests/header.c` `test_hdm_layout` (line 911) checks only the `WINDOWPOS` plumbing and a message sequence, never a height value; the Wine header implementation does use `HP_HEADERITEM` (`dlls/comctl32/header.c:296,297,375,392,634`) so the theme path exists but is untested for size. In the Industrial7 repo the 24-vs-20 difference is recorded only as a *waiver* in `docs/design/win10/COMCTL32-HEADER-W10.md:163-165` ("Win10's themed minimum comes from GetThemePartSize/GetThemePosition [V-corpus 0x85d98] and is not computable on the host: UNVERIFIED"). The string "CC30" does not occur in the Industrial7 docs/code at all (only unrelated third-party hits). [V]
5. **Industrial7 has real uxtheme tests, but all are theme-OFF contract tests derived from Win7 decompilation, not Win10 oracle recordings.** `userland/guest/uxtheme-w10` (35 "real" exports, 158 stubs) and `crates/w10-uxtheme-off` (1100 lines, 26 `#[test]`s). Their header says: "Every row is Win7-derived: Win10 19041 uxtheme is not in the corpus." The new corpus is the first thing that can supply Win10 *recorded* values. [V, section 6]
6. **There is no recorded Win10 value anywhere for part sizes, metrics, ints, margins** - not in Wine tests (they test the checkbox 13 px only, and via Wine's own `light.msstyles` on Wine), not in our corpora, not in Industrial7 (`GetThemePartSize` appears only as a delay-import stub name and a "PIN"-level coverage row). Hence the probe list from the other research roles is entirely greenfield. [V]

## 2. Method (commands actually run)

```
ls /home/iceisfun/industrial7_test_theme /home/iceisfun/industrial7_test_theme/tests /home/iceisfun/industrial7_test_theme/tools
cat tests/core_smoke/smoke.c                        # 11 lines
python3: tools/uxtheme_exports.json                 # 193 entries: 85 named, 108 ordinal-only, 34 of them SHUNIMPL forwarders (all unnamed)
ls ~/work/wine/wine/dlls/{uxtheme,comctl32,user32}/tests ; wc -l dlls/uxtheme/tests/*   # system.c 2947, msg.h 408, v6util.h 131
grep -n "^static void test_\|^START_TEST" dlls/uxtheme/tests/system.c   # 25 test fns + 1 helper (test_buffer_dc_props) + START_TEST (corrected from "31" by fact-check)
python3 per-export regex over system.c              # hits per export (table in section 8)
grep -rl "OpenThemeData|IsThemeActive|IsAppThemed|uxtheme" ~/work/wine/wine/dlls/*/tests ~/work/wine/wine/programs
grep -rIl -i "uxtheme|OpenThemeData|IsThemeActive|GetThemePartSize|DrawThemeBackground|HEADERITEM|SetWindowTheme" ~/industrial7_test_* ~/test_*
grep -rIl uxtheme ~/work/industrial7 --exclude-dir=.git --exclude-dir=target
awk over ~/work/industrial7/docs/design/win10/owned-test-coverage.tsv (rows where crate==uxtheme)
find / -iname "*reactos*" -o -iname "uxtheme_test*" -o -name "*.msstyles"
```

Wine tree: `/home/iceisfun/work/wine/wine`, HEAD `4f4d68f6a784db76e66cf0f033f90b8288bba11e` (commit date Mon Sep 21 2026). Industrial7: `/home/iceisfun/work/industrial7`, HEAD `25af4cbccd`.

## 3. Wine `dlls/uxtheme/tests/system.c`

Build: `Makefile.in` `IMPORTS = advapi32 user32 gdi32 uxtheme`, single source `system.c`. Entry `START_TEST(system)` at line 2905. Most exports are linked directly; a few are `GET_PROC`'d (lines 79-105), and **six ordinals are fetched by number** (lines 85-90): `2` OpenThemeFile, `3` CloseThemeFile, `138` ShouldSystemUseDarkMode, `132` ShouldAppsUseDarkMode, `96` GetImmersiveColorTypeFromName, `100` GetImmersiveColorNamedTypeByIndex; line 2503 additionally asserts that ordinal `47` resolves to `DrawThemeBackgroundEx`. Win10 corpus check: `tools/uxtheme_exports.json` shows ordinals 2 and 3 as forwarders (`SHUNIMPL.#440`, `SHUNIMPL.#441`), so on Win10 `OpenThemeFile`/`CloseThemeFile` cannot work and `test_OpenThemeFile` is effectively dead there [V json; I: it either win_skips or fails on GetProcAddress of a SHUNIMPL forwarder - not run]. Ordinals 96, 100, 132, 138 are present local exports in the JSON [V].

Almost every theme-reading test starts with `OpenThemeData(...)` and `skip("Theming is inactive")` if NULL, so **on a machine with no visual style the whole data-reading half is skipped** - the opposite of what Industrial7 needs (its target is "armed" Win10 values). Order in `START_TEST`: IsThemed, IsThemePartDefined, GetWindowTheme, SetWindowTheme, OpenThemeData, OpenThemeDataEx, OpenThemeDataForDpi, OpenThemeFile, GetCurrentThemeName, GetThemePartSize, CloseThemeData, buffered_paint, GetThemeIntList, GetThemeTransitionDuration, DrawThemeParentBackground, DrawThemeBackgroundEx, GetThemeBackgroundRegion, theme(FALSE), dark-mode x2, immersive colors, DrawThemeEdge, DrawThemeTextEx; then (only if a comctl32 v6 activation context loads, `load_v6_module`) EnableThemeDialogTexture and theme(TRUE); finally EnableTheming (last because it can disable theming).

| Test fn (line) | uxtheme calls | Classes / parts / states / props checked | Version / theme / environment dependence |
|---|---|---|---|
| test_IsThemed (305) | IsThemeActive, IsAppThemed | none | none; just consistency |
| test_IsThemePartDefined (317) | OpenThemeData, IsThemePartDefined | Button/BP_PUSHBUTTON state 0 -> TRUE, state PBS_NORMAL / PBS_DEFAULTED_ANIMATING / +1 -> FALSE; Edit/EP_EDITTEXT state 0 TRUE, ETS_NORMAL/ETS_FOCUSED FALSE; NULL htheme -> FALSE + GLE E_HANDLE; bogus part 0xdeadbeef FALSE, GLE NO_ERROR | skipped for non-NULL class when theming inactive |
| test_GetWindowTheme (382), test_SetWindowTheme (417) | GetWindowTheme, SetWindowTheme, OpenThemeData | window property plumbing, WM_THEMECHANGED sequence (msg.h), NULL/invalid hwnd | theme-active vs not branches |
| test_OpenThemeData (499) | OpenThemeData | NULL/NULL -> GLE E_POINTER; invalid class "DEADBEEF" -> E_PROP_ID_UNSUPPORTED; class lists "Button;ListBox", case "bUtToN"; NULL hwnd vs real hwnd | explicit `bThemeActive` branches |
| test_OpenThemeDataEx (653) | OpenThemeDataEx | class-list forms "explorer::treeview", "deadbeef::treeview;dead::beef", OTD_* flags | theme-active gated |
| test_OpenThemeDataForDpi (765) | OpenThemeDataForDpi | DPI argument | needs Win10 (DPI APIs) |
| test_OpenThemeFile (894) | ord 2 / ord 3, GetThemePartSize on a *file* handle | NULL args -> E_POINTER (`todo_wine` on Wine); GetThemePartSize on file handle -> E_HANDLE (`todo_wine`) | reads `HKCU\...\ThemeManager\DllName`; `win_skip` if absent; **dead on Win10 (forwarders)** |
| test_GetCurrentThemeName (793) | GetCurrentThemeName | buffer length matrix, NULL out args | needs active theme |
| test_GetThemePartSize (1161) | OpenThemeData(Button), GetThemeInt, GetThemePartSize | **only** BP_CHECKBOX / CBS_CHECKEDNORMAL / TS_DRAW, hdc and NULL hdc; expected `MulDiv(13, dpi, 96)` +-1, 26 at 192 DPI; TMT_TRUESIZESTRETCHMARK, TMT_MINDPI1..7 | **DPI/theme dependent**: changes the primary monitor DPI via D3DKMT/DisplayConfig and sets `IgnorePerProcessSystemDPIToast` in HKCU; skip if no `SetThreadDpiAwarenessContext`. Do NOT port as-is (it mutates the host display) |
| test_CloseThemeData (882) | CloseThemeData | NULL and invalid handles | |
| test_buffered_paint (963) | BeginBufferedPaint, EndBufferedPaint, BufferedPaintClear, GetBufferedPaintBits/DC/TargetDC/TargetRect | arg validation matrix (NULL target, empty rect, formats BPBF_*), HRESULTs, DC properties | version-neutral on Vista+ (win_skip when absent) |
| test_GetThemeIntList (1491) | GetThemeIntList | PUSHBUTTON/PBS_NORMAL/TMT_TRANSITIONDURATIONS only; hr == S_OK on Vista+, E_PROP_ID_UNSUPPORTED on XP | `GetVersion()<6` branch |
| test_GetThemeTransitionDuration (1525) | GetThemeTransitionDuration, GetThemeIntList | full error matrix: NULL -> E_HANDLE, bad part -> E_PROP_ID_UNSUPPORTED, state range -> E_INVALIDARG (note `duration` out-param untouched 0xdeadbeef vs zeroed 0 depending on branch), TMT_BACKGROUND -> E_PROP_ID_UNSUPPORTED; compares duration with the int list | crash on Wine for garbage HTHEME (guarded by `winetest_platform`) |
| test_DrawThemeParentBackground (1706) | DrawThemeParentBackground | flags/clip/offset, parent WM_ERASEBKGND/WM_PRINTCLIENT sequence | |
| test_EnableThemeDialogTexture (1856) | EnableThemeDialogTexture, IsThemeDialogTextureEnabled | ETDT_* flag matrix | **requires comctl32 v6 activation context** (manifest written to cwd by `v6util.h`) |
| test_EnableTheming (1327) | EnableTheming | system colors, NONCLIENTMETRICS, gradient caption, flat menu side effects of toggling | mutates HKCU color strings and system parameters; runs last |
| test_DrawThemeBackgroundEx (2488) | ordinal 47, GetProcAddress identity, IsThemeBackgroundPartiallyTransparent, GetThemeBool(TMT_TRANSPARENT), GetThemeEnumValue(TMT_GLYPHTYPE), DrawThemeBackgroundEx | Spin / SPNP_UP / UPS_NORMAL: pixel alpha of glyph on opaque bg (10x10 DIB) | theme-active gated; asserts alpha behaviour only |
| test_GetThemeBackgroundRegion (2624) | GetThemeEnumValue(TMT_BGTYPE), GetThemeBackgroundRegion | Rebar/RP_BAND -> BT_NONE; region call -> E_UNEXPECTED (`broken(S_OK)` on < Win10) | Win10-specific expectation (useful: verified Win10 behaviour) |
| test_theme(v6) (2657) | GetThemeBool, IsThemeBackgroundPartiallyTransparent | ScrollBar SBP_ARROWBTN TMT_TRANSPARENT TRUE on Vista+, E_PROP_ID_UNSUPPORTED on XP; real scrollbar window pixel scan: < 4% (v6) / < 50% (v5) pure-white pixels in a 200x50 scrollbar | pixel check of real rendering; `GetVersion()` branch |
| test_ShouldSystemUseDarkMode / AppsUseDarkMode (2731/2760) | ord 138 / 132 | equals `!SystemUsesLightTheme` / `!AppsUseLightTheme` registry; GLE untouched | registry-dependent |
| test_GetImmersiveColors (2789) | GetImmersiveUserColorSetPreference, GetImmersiveColorFromColorSetEx, ord 96, ord 100 | type == ~0u for unknown name, name enumeration loop, color != 0; `todo_wine` on Wine | Win10-only (win_skip otherwise) |
| test_DrawThemeEdge (2828) | DrawThemeEdge | Button/BP_PUSHBUTTON/PBS_NORMAL, BF_ADJUST with NULL content rect, BF_DIAGONAL: returns S_OK | return-code only |
| test_DrawThemeTextEx (2861) | DrawThemeTextEx | NULL htheme -> E_HANDLE; DTT_FONTPROP/TMT_BODYFONT; DTT_TEXTCOLOR | return-code only |

Classes opened anywhere in the file (from `grep -o 'L"[A-Za-z:;_ ]*"'`): Button (13x), Scrollbar/ScrollBar (5), Edit (3), explorer::treeview forms, Tab (1), Spin (1), Rebar (1), Header (1; line 2581, HP_HEADERDROPDOWN only: IsThemePartDefined, TMT_TRANSPARENT/TMT_GLYPHTRANSPARENT, one DrawThemeBackgroundEx pixel/alpha check, lines 2581-2596), plus immersive names. Parts/props referenced (counts): BP_PUSHBUTTON 18, PBS_NORMAL 18, CBS_CHECKEDNORMAL 15, BP_CHECKBOX 15, TS_DRAW 13, TMT_TRANSITIONDURATIONS 17, TMT_TRANSPARENT 6, HP_HEADERDROPDOWN 5 (transparency/alpha checks inside test_DrawThemeBackgroundEx, not a size probe), SBP_SIZEBOX 4, SBP_ARROWBTN 3, EP_EDITTEXT 3, TMT_TRUESIZESTRETCHMARK 2, RP_BAND 2, BP_GROUPBOX 2, TABP_BODY 1, TS_TRUE 1. **No `HP_HEADERITEM`, `HIS_*`, `LVP_*`, `TABP_TABITEM`, `PP_*`, `SBP_THUMBBTN*`, `TMT_HEIGHT`/`TMT_WIDTH`/`TMT_MINSIZE`/`TMT_DEFAULTPANESIZE`.**

Quality note: Wine's value checks are "does Wine's own `light.msstyles` look right" (13 px checkbox) rather than "what does Win10 Aero return"; the only literal Win10 numbers it carries are 13/26 (checkbox), 4% / 50% white-pixel thresholds, and the error codes above. Checks marked `todo_wine` are places where *Wine deviates from Windows*, so each `todo_wine` is a documented Windows behaviour worth carrying over: lines 921 and 930 (OpenThemeFile E_POINTER / GetThemePartSize on a file handle -> E_HANDLE), 1562 (GetThemeTransitionDuration with garbage HTHEME -> E_HANDLE), 2813 (GetImmersiveColorNamedTypeByIndex returns a name), plus 583-629 and 819-830 in the OpenThemeData/GetCurrentThemeName tests (not read in detail).

## 4. Wine consumer-control tests that touch themes (comctl32, user32)

`grep -l "OpenThemeData|IsThemeActive|uxtheme" dlls/*/tests/*.c` gives exactly: `comctl32/tests/{misc,propsheet,toolbar,treeview}.c`, `user32/tests/{class,scroll,win}.c`, `uxtheme/tests/system.c`. No `win32u`, `shell32`, `gdi32` or `comdlg32` test touches uxtheme. [V]

| File:line | What | Uses | Notes |
|---|---|---|---|
| comctl32/tests/misc.c:627 `test_WM_THEMECHANGED` | for 25 classes (Animate, Button, ComboBox, ComboBoxEx, DateTimePick, Edit, SysHeader32, HotKey, IPAddress, ListBox, SysListView32, MonthCal, NativeFont, PageScroller, Progress, ReBar, Static, msctls_statusbar32, SysLink, SysTabControl32, ToolbarWindow32, tooltips, trackbar, SysTreeView32, UpDown, ScrollBar) sends WM_THEMECHANGED and checks the resulting child message sequence (paint+erase vs none) | none (message only) | v6 only. Good example of class-wide behaviour that does not need theme data |
| comctl32/tests/misc.c:898 `test_themed_background` | per class/style, parent `WM_PRINTCLIENT`/erase sequence and pixel colour of a themed child background; includes `SetWindowTheme` forms | `pSetThreadDpiAwarenessContext`, IsThemeActive | tests *behaviour*, hard-coded sequences |
| comctl32/tests/toolbar.c:2415, 2730-2800 | toolbar transparent drawing when theme active; "toolbar shouldn't use outside theme handles"; `OpenThemeData(toolbar, L"Rebar")` then pixel compare | GetWindowTheme, IsThemeActive, OpenThemeData, CloseThemeData | skip unless theme active |
| comctl32/tests/treeview.c:2387-2445, 3448 | `GetWindowTheme` + `IsThemeBackgroundPartiallyTransparent(theme, TVP_GLYPH, 0)` decides the expected glyph pixel | 2 uxtheme fns | pixel expectations depend on theme data |
| comctl32/tests/propsheet.c:1293-1302 | `IsThemeDialogTextureEnabled(sheet) == is_v6` "even when theming is off" | IsThemeActive, IsThemeDialogTextureEnabled | verified Windows behaviour carried by Wine tests |
| user32/tests/class.c:2325 `test_uxtheme` | uxtheme.dll is **not loaded** before the first window; after creating a window it is loaded iff `IsThemeActive` | GetModuleHandle, IsThemeActive | tests user32 -> uxtheme lazy load (ThemeHooksInstall path) |
| user32/tests/scroll.c:381,417,1074 | standard scrollbar paint size/arrow checks differ if theme active | IsThemeActive | |
| user32/tests/win.c:1476-1640 | scrollbar size-box pixel colour changes iff theme active (`broken()` for Win 10 1507 64-bit) | IsThemeActive | |
| comctl32/tests/header.c (whole file, 1949 lines) | header messages, items, hit-test, layout, custom draw, filter, order array. v6 section (line 1930): `test_hdf_fixedwidth`, `test_hds_nosizing`, `test_item_auto_format`, `test_hdm_layout` | **no uxtheme call, no theme gating** | `test_hdm_layout` (911) asserts only `WINDOWPOS.hwnd`/`hwndInsertAfter` untouched and a message sequence; **no height value is asserted** [V] |
| comctl32/tests/{button,tab,listview,progress,rebar,trackbar,updown,status,combo,edit,static,tooltips,monthcal,datetime,taskdialog,syslink}.c | v6 manifest activation (`load_v6_module`) for many; none calls a uxtheme function or compares a theme-derived size | - | e.g. `button.c` `test_bcm_get_ideal_size` uses `SM_CXEDGE`, `test_visual` compares DC bitmaps between button types (only identity of two renderings, no absolute values); `tab.c` `test_tab` (524) takes DPI and `TEXTMETRIC` inputs and checks `TCM_GETITEMRECT` against computed formulas [I: formula-based, no theme call]; `listview.c:5667-5790` uses the header height only relatively (`header_height`) |

Version/theme-dependence summary for these: skip-if-no-theme (`IsThemeActive`) in toolbar/treeview/class/scroll/win; `broken()` counts for tab 4, button 7, listview 6, status 11, treeview 2, rebar 1, header 1 (grep -c `broken(`), indicating tolerated cross-version differences that are exactly where a Win10 corpus must decide which side is correct [V counts; I interpretation].

## 5. Our other corpora on this host

`ls -d ~/industrial7_test_* ~/test_*`: industrial7_test_{crypt,ntdll,theme,webauthn}, test_{advapi32,dwrite,gdi32_gdifull,grab,industrial7_webauthn,kernel32_kernelbase,msvcrt-git-backup,ole,pattern,user32,win32k}. Command: `grep -rIl -i "uxtheme|OpenThemeData|IsThemeActive|GetThemePartSize|DrawThemeBackground|HEADERITEM|SetWindowTheme" <dir> --exclude-dir=.git --exclude-dir=build`.

| Corpus | uxtheme/theme hits | Relevant adjacent tests |
|---|---|---|
| industrial7_test_theme | `core_smoke/smoke.c` only (module load) + README/export lists | - |
| test_win32k | `tests/win32k_mitigation/sc_load.c:71` `load_gui_importing_dlls` loads uxtheme.dll among 8 DLLs, notes the result, then asserts the process is unconverted; `COVERAGE.md:805` lists `NtUserBroadcastThemeChangeEvent` as a (not behaviourally covered [I]) syscall | `dx_sysparams_colors_metrics` area (system colours/metrics, which uxtheme overrides when theming is on) |
| test_user32 | none | `tests/paint_gdi/sysmetrics.c` (SM_CXVSCROLL, SM_CYCAPTION, SM_CXPADDEDBORDER, NONCLIENTMETRICS consistency: only `> 0` / equality checks - **no absolute values**, so cannot distinguish themed vs classic), `tests/sysparams/metrics.c`, `tests/controls/{button,edge,scroll,static,combo,...}.c` (user32 classic controls: window class behaviour, no manifest, so comctl32 v5/classic visual path; button.c 31 tests: style bits, check cycles, owner draw; none themed), `tests/dialogs/dlg_create.c` mentions comctl32 once |
| test_gdi32_gdifull | none | nothing for `NtGdiDrawStream`; (see Industrial7 below for the gap row) |
| test_ole, test_dwrite, test_advapi32, test_kernel32_kernelbase, test_msvcrt-git-backup, test_grab, test_pattern, industrial7_test_{crypt,ntdll,webauthn}, test_industrial7_webauthn | none | - |
| manifests | grep for `manifest` / Common-Controls in Makefiles/rc/h of test_user32, industrial7_test_theme, test_gdi32_gdifull, test_win32k found no comctl32 v6 activation manifest (only `win_position_zorder/zo.h` and `win_create_destroy/wcd.h` mention the word) - **none of our corpora can currently obtain comctl32 v6 themed controls**; a new corpus needs an embedded or side-by-side manifest [V grep; I: the two .h hits are unrelated and were not read] |

## 6. The Industrial7 repo (`/home/iceisfun/work/industrial7`)

### 6.1 uxtheme-w10 (guest DLL) and w10-uxtheme-off (pure logic)

* `userland/guest/uxtheme-w10/data/exports_manifest.tsv`: 193 exports; `kind`: **35 real / 158 stub**, `status`: 35 partial / 158 ni. Real = DrawThemeBackgroundEx, Ordinal_49 (OpenNcThemeData), CloseThemeData, DrawThemeBackground, DrawThemeEdge, DrawThemeIcon, OpenThemeDataEx, DrawThemeText, EnableThemeDialogTexture, GetThemeBackgroundContentRect/Extent/Region, GetThemeBool/Color/EnumValue/Font/Int/Margins/Metric/PartSize/Position/PropertyOrigin/Rect/String/TextExtent/TextMetrics/TransitionDuration, HitTestThemeBackground, IsAppThemed, IsCompositionActive, IsThemeActive, OpenThemeData, OpenThemeDataForDpi, SetWindowTheme, SetWindowThemeAttribute. The `ehandle` class means "answer E_HANDLE"; `open` returns NULL. So `GetThemePartSize` today = E_HANDLE, as the owner stated. [V]
* Tests (all Rust, host-side, mock `Hooks`): `tests/manifest.rs` (7 tests: digest-pinned corpus table, export count/ordinals equal corpus, the 11 Chrome names are real, .def matches manifest, body symbols exist, stub bodies follow corpus return type, image imports only user32 + atoms API set), `tests/behavior.rs` (12 tests per grep -c, text lists 10 themes: theme queries false + LastError 0, "before initialization" 0x8000FFFF, composition inactive, OpenThemeData NULL with reason, every handle call E_HANDLE, ordinal 49 = OpenNcThemeData, CloseThemeData correction vs the Win7 crate, six corpus-verified handle calls carry decomp RVAs, state-changing calls are honest failures), `tests/window.rs` (19 tests per grep -c: SetWindowTheme property atoms 0xA910..0xA916 and WM_THEMECHANGED, EnableThemeDialogTexture flag fold, SetWindowThemeAttribute non-client merge truth table). `crates/w10-uxtheme-off/src/lib.rs`: 26 `#[test]`s over the decision table. [V: grep -c, `grep -n "#\[test\]"`]
* These are **model/PIN tests**, not Windows oracle runs: `crates/w10-uxtheme-off/src/lib.rs:1-8` "Every row is Win7-derived [V-decomp = Win7 RTM 7600]; Win10 19041 uxtheme is not in the corpus". `docs/design/win10/UXTHEME-OFF-W10-LINK.md:4-6` says the same. The behaviours verified there (OpenThemeData NULL + 0x80070490, E_HANDLE 0x80070006, last error 0x8000FFFF uninitialized, property atoms) are therefore **Win7 facts waiting for a Win10 confirmation run**; the new corpus can turn them into recorded Win10 expected values. Note the Wine suite contradicts nothing here but also does not check `0x80070490` (Wine tests expect `E_PROP_ID_UNSUPPORTED` for a bad class while theming is active).
* `docs/design/win10/owned-test-coverage.tsv` (generated by `scripts/w10_test_coverage.py`, bb #876), rows with crate=uxtheme, 193: **75 PIN, 8 GUEST (host level PIN), 2 BEHAVIOURAL, 108 NONE**. The 108 NONE are exactly the ordinal-only exports (the generator keys rows `@N` and finds no named evidence, even though `tests/manifest.rs` pins every ordinal by number [I]). The two BEHAVIOURAL rows: `OpenThemeData`, `IsCompositionActive`. GUEST rows (BeginBufferedPaint, BufferedPaintClear, BufferedPaintSetAlpha, DllCanUnloadNow, DllGetClassObject, EndBufferedPaint, GetBufferedPaintBits, IsThemeActive) have a guest-run test elsewhere (comctl32/chrome probes) - not enumerated here. Level semantics (NONE < PIN < BEHAVIOURAL < MODEL < GUEST < MUTANT-CHECKED) are defined in `OWNED-TEST-COVERAGE.md`. [V]
* `docs/OWNED_STATUS.md:252`: `uxtheme.dll | system dll | stock-only | ... research only: docs/design/stockdll-ranking (rank 2)` and line 637 `R5 theme uxtheme/msstyles/Aero .theme ... NOT_SHIPPED`. [V]

### 6.2 comctl32 / user32 / gdi32 theme-related tests in Industrial7

* Owned comctl32 (`userland/guest/comctl32/src/theme.rs`, 354 lines): `open()` always `None`, `is_app_themed()`/`is_theme_active()` false; `no-import` rule in `host/xtask/src/owned/comctl32.rs:390` forbids a uxtheme import. Tests `userland/guest/comctl32-w10/tests/{hdr_class,lv_class,lv_ui_surface,tv_class,tab}.rs` are *surface* tests asserting uxtheme is **not** a load-time import and that a failed bind or NULL theme means the classic arm (e.g. `lv_ui_surface.rs:16-33`, `hdr_class.rs:88-90`, `tab.rs:78`). They prove "no themed rendering", not any themed value. [V]
* `docs/design/win10/COMCTL32-HEADER-W10.md`: header `HDM_LAYOUT` height = text height + 2*edge (+ the theme minimum when themed); model gives 24 only with host input `Theme.min_height = 24`, otherwise 20 (font 16 + 2*2). Waiver 1 (lines 163-165) records the Win7 VM *was* themed (24 from the theme minimum) while the classic model gives 20, and states the Win10 value "is not computable on the host: UNVERIFIED" (`_Header_InitTheme` 0x85d98 reads `GetThemePartSize`/`GetThemePosition`). Probe `userland/wintests/comctl32/cc20_header_probe.c` (87 verdict lines, `HDM_LAYOUT` at lines 174-176) is "cross-built, NOT run" (doc lines 178-186, 220). The probe tree `userland/wintests/comctl32/cc10..cc25_*` has **no** uxtheme call (`grep -rIl uxtheme userland/wintests/comctl32` -> none). [V]
* `docs/design/comctl32/STAGES.md:331`: "`design/themed_paths.tsv` (DESIGN 5.3, P0) was never produced"; line 154 lists `IsAppThemed`/`IsThemeActive` LastError as needing a "theme probe (DESIGN 11.2 item 3)" - i.e. the theme probe was planned and **never written**; line 335 option (B) "themed Win7 column ... would need a themed Win7 column". `OWNED-STATUS.md:168`: "Not written: ... the theme layer". [V]
* `userland/wintests/gdi32/gdi32_bitmaps_pixel.c:390`: `g32_gap("K-BLT", "NtGdiDrawStream", "uxtheme's draw stream (Win7 only)")` - NtGdiDrawStream is the gdi32 side of DrawThemeBackground; `userland/wintests/gdi32/gdi32_chrome2.c:15`: "011 The blends uxtheme and Chrome's skia_utils reach: GdiAlphaBlend ...". So gdi32 coverage for themed drawing is a *known, recorded gap* ("Win7 only"; on Win10 not exercised). `userland/wintests/win10/win10_ldr_sweep.c:89` and `user32/data/attach_exit.tsv:66` only load/attach uxtheme (`uxtheme 1 exited 0x600d`). [V]
* `grep -rIl "oracle|CC30|HEADERITEM|GetThemePartSize"`: no CC30 anywhere in docs/code; `HEADERITEM`/`GetThemePartSize` appear only in import census tables, `owned-gap.tsv`/`gaps.tsv`, `CHROME-USERLAND-GAPS.md`, `UXTHEME-DWMAPI-W10-LINK.md`, `COMCTL32-HEADER-W10.md`, `listview/shared.rs` and `toolbar_rebar/rb/v6/state.rs` (themed branches never taken). No golden screenshots for themed controls were found (`find` for `*golden*`/`*.msstyles` in the repo returned only unrelated hardware/installer goldens). No oracle rows for part sizes. [V find/grep; the exact 'GetThemePartSize' lines in listview/shared.rs and rb/v6/state.rs were located but their content not read]

## 7. Other sources

* ReactOS: **not present locally** (`find / -iname "*reactos*"` returned only an unrelated icon). ReactOS's `modules/rostests/winetests/uxtheme` would be a copy of Wine's anyway [I].
* Prebuilt Wine test binaries exist outside the Wine source tree: `/home/iceisfun/diablo/testwine/wine/dlls/uxtheme/tests/x86_64-windows/uxtheme_test.exe` (and `-stripped`), `/home/iceisfun/diablo/testwine/wine/programs/winetest/x86_64-windows/winetest.exe` [V find]. Not run (another role evaluates Wine as an oracle). `/home/iceisfun/work/wine/wine/dlls/uxtheme/tests` itself holds only sources.
* Wine msstyles (the only theme file Wine can load): `light.msstyles` under many prefixes, e.g. `/home/iceisfun/.wine/drive_c/windows/resources/themes/light/light.msstyles`, `/home/iceisfun/diablo/testwine/wine/dlls/light.msstyles` [V find]. It is Wine's own dark-grey/light theme, not Aero or Win10's `aero.msstyles`; hence values read from it are not Win10 values.
* No Windows 10 machine/VM artifacts (`aero.msstyles`, themed screenshots, recorded .tsv with part sizes) were found on this host.

## 8. Coverage matrix: the 193 uxtheme exports

Columns: `ord` = ordinal; `name` = exported name, or `#ord (name)` where the export is ordinal-only and the parenthesised name is Wine's `uxtheme.spec` name for that ordinal (Wine ordinals follow the XP/Win7 numbering; **identity on Win10 is [I] unless it is one of 47, 96, 100, 132, 138, 49, 72, 75, where the Industrial7 research/Wine test corroborate it**) and `[fwd X]` marks a Win10 forwarder to SHUNIMPL (cannot be implemented usefully; 34 such, all ordinal-only). `Wine q` = quality of Wine `uxtheme/tests/system.c` coverage: A thorough (arg/error matrix + semantics), B solid but narrow, C a few return-value checks or one data point, D incidental/one call, `-` none (`B*` = test exists but dead on Win10). `other Wine` = coverage from comctl32/user32 tests. `our corpora` = **none for every row** (industrial7_test_theme has only core_smoke). `I7 level` = best level in `owned-test-coverage.tsv` (host-side model/pin tests, Win7-derived; NONE for every ordinal-only row is a generator artifact [I]). `I7 kind` = kind/status in `uxtheme-w10/data/exports_manifest.tsv` (real/stub, partial/ni).

| ord | name | Wine q | Wine system.c: where / what | other Wine | I7 level | I7 kind |
|---|---|---|---|---|---|---|
| 1 | #1 (QueryThemeServices) | - | - | - | NONE | stub/ni |
| 2 | #2 (OpenThemeFile) [fwd SHUNIMPL.#440] | B* | test_OpenThemeFile (Win10: SHUNIMPL fwd, skipped) | - | NONE | stub/ni |
| 3 | #3 (CloseThemeFile) [fwd SHUNIMPL.#441] | B* | test_OpenThemeFile (Win10: SHUNIMPL fwd, skipped) | - | NONE | stub/ni |
| 4 | #4 (ApplyTheme) [fwd SHUNIMPL.#445] | - | - | - | NONE | stub/ni |
| 5 | BeginPanningFeedback | - | - | - | PIN | stub/ni |
| 6 | EndPanningFeedback | - | - | - | PIN | stub/ni |
| 7 | #7 (GetThemeDefaults) | - | - | - | NONE | stub/ni |
| 8 | #8 (EnumThemes) | - | - | - | NONE | stub/ni |
| 9 | #9 (EnumThemeColors) | - | - | - | NONE | stub/ni |
| 10 | #10 (EnumThemeSizes) | - | - | - | NONE | stub/ni |
| 11 | #11 (ParseThemeIniFile) [fwd SHUNIMPL.#190] | - | - | - | NONE | stub/ni |
| 12 | UpdatePanningFeedback | - | - | - | PIN | stub/ni |
| 13 | #13 (DrawNCPreview) [fwd SHUNIMPL.#444] | - | - | - | NONE | stub/ni |
| 14 | #14 (RegisterDefaultTheme) | - | - | - | NONE | stub/ni |
| 15 | #15 (DumpLoadedThemeToTextFile) | - | - | - | NONE | stub/ni |
| 16 | #16 (OpenThemeDataFromFile) | - | - | - | NONE | stub/ni |
| 17 | #17 (OpenThemeFileFromData) | - | - | - | NONE | stub/ni |
| 18 | #18 (GetThemeSysSize96) [fwd SHUNIMPL.#189] | - | - | - | NONE | stub/ni |
| 19 | #19 (GetThemeSysFont96) [fwd SHUNIMPL.#188] | - | - | - | NONE | stub/ni |
| 20 | #20 (SessionAllocate) [fwd SHUNIMPL.#395] | - | - | - | NONE | stub/ni |
| 21 | #21 (SessionFree) [fwd SHUNIMPL.#396] | - | - | - | NONE | stub/ni |
| 22 | #22 (ThemeHooksOn) [fwd SHUNIMPL.#397] | - | - | - | NONE | stub/ni |
| 23 | #23 (ThemeHooksOff) [fwd SHUNIMPL.#398] | - | - | - | NONE | stub/ni |
| 24 | #24 (AreThemeHooksActive) [fwd SHUNIMPL.#399] | - | - | - | NONE | stub/ni |
| 25 | #25 (GetCurrentChangeNumber) [fwd SHUNIMPL.#400] | - | - | - | NONE | stub/ni |
| 26 | #26 (GetNewChangeNumber) [fwd SHUNIMPL.#215] | - | - | - | NONE | stub/ni |
| 27 | #27 (SetGlobalTheme) [fwd SHUNIMPL.#401] | - | - | - | NONE | stub/ni |
| 28 | #28 (GetGlobalTheme) [fwd SHUNIMPL.#402] | - | - | - | NONE | stub/ni |
| 29 | #29 (CheckThemeSignature) [fwd SHUNIMPL.#213] | - | - | - | NONE | stub/ni |
| 30 | #30 (LoadTheme) [fwd SHUNIMPL.#403] | - | - | - | NONE | stub/ni |
| 31 | #31 (InitUserTheme) [fwd SHUNIMPL.#431] | - | - | - | NONE | stub/ni |
| 32 | #32 (InitUserRegistry) [fwd SHUNIMPL.#406] | - | - | - | NONE | stub/ni |
| 33 | #33 (ReestablishServerConnection) | - | - | - | NONE | stub/ni |
| 34 | #34 (ThemeHooksInstall) [fwd SHUNIMPL.#407] | - | - | - | NONE | stub/ni |
| 35 | #35 (ThemeHooksRemove) [fwd SHUNIMPL.#408] | - | - | - | NONE | stub/ni |
| 36 | #36 (RefreshThemeForTS) | - | - | - | NONE | stub/ni |
| 37 | BeginBufferedAnimation | - | - | - | PIN | stub/ni |
| 38 | BeginBufferedPaint | A | test_buffered_paint (arg validation, formats, DC props) | - | GUEST | stub/ni |
| 39 | BufferedPaintClear | B | test_buffered_paint | - | GUEST | stub/ni |
| 40 | BufferedPaintInit | - | - | - | PIN | stub/ni |
| 41 | BufferedPaintRenderAnimation | - | - | - | PIN | stub/ni |
| 42 | BufferedPaintSetAlpha | - | - | - | GUEST | stub/ni |
| 43 | #43 (ClassicGetSystemMetrics) | - | - | - | NONE | stub/ni |
| 44 | #44 (ClassicSystemParametersInfoA) | - | - | - | NONE | stub/ni |
| 45 | #45 (ClassicSystemParametersInfoW) | - | - | - | NONE | stub/ni |
| 46 | #46 (ClassicAdjustWindowRectEx) | - | - | - | NONE | stub/ni |
| 47 | DrawThemeBackgroundEx | C | test_DrawThemeBackgroundEx: Spin SPNP_UP pixel/alpha handling; ordinal 47 identity check | - | PIN | real/partial |
| 48 | #48 (GetThemeParseErrorInfo) | - | - | - | NONE | stub/ni |
| 49 | #49 (OpenNcThemeData(i7 RESEARCH.md; Wine: stub @)) | - | - | - | NONE | real/partial |
| 50 | #50 | - | - | - | NONE | stub/ni |
| 51 | BufferedPaintStopAllAnimations | - | - | - | PIN | stub/ni |
| 52 | BufferedPaintUnInit | - | - | - | PIN | stub/ni |
| 53 | CloseThemeData | B | test_CloseThemeData; used as cleanup everywhere | comctl32 toolbar.c:2800 | PIN | real/partial |
| 54 | DllCanUnloadNow | - | - | - | GUEST | stub/ni |
| 55 | DllGetActivationFactory | - | - | - | PIN | stub/ni |
| 56 | DllGetClassObject | - | - | - | GUEST | stub/ni |
| 57 | DrawThemeBackground | - | - | - | PIN | real/partial |
| 58 | DrawThemeEdge | D | test_DrawThemeEdge: BF_ADJUST with NULL content rect only (2 calls) | - | PIN | real/partial |
| 59 | DrawThemeIcon | - | - | - | PIN | real/partial |
| 60 | #60 (CreateThemeDataFromObjects) | - | - | - | NONE | stub/ni |
| 61 | OpenThemeDataEx | A | test_OpenThemeDataEx (OTD_* flags, class-list forms) | - | PIN | real/partial |
| 62 | #62 (ServerClearStockObjects) [fwd SHUNIMPL.#409] | - | - | - | NONE | stub/ni |
| 63 | #63 (MarkSelection) [fwd SHUNIMPL.#404] | - | - | - | NONE | stub/ni |
| 64 | #64 | - | - | - | NONE | stub/ni |
| 65 | #65 | - | - | - | NONE | stub/ni |
| 66 | #66 [fwd SHUNIMPL.#405] | - | - | - | NONE | stub/ni |
| 67 | #67 | - | - | - | NONE | stub/ni |
| 68 | #68 | - | - | - | NONE | stub/ni |
| 69 | #69 | - | - | - | NONE | stub/ni |
| 70 | DrawThemeParentBackground | A | test_DrawThemeParentBackground (message sequences, flags, clip, offsets) | - | PIN | stub/ni |
| 71 | DrawThemeParentBackgroundEx | - | - | - | PIN | stub/ni |
| 72 | #72 (DrawThemeIconEx(i7 RESEARCH.md)) | - | - | - | NONE | stub/ni |
| 73 | #73 | - | - | - | NONE | stub/ni |
| 74 | #74 | - | - | - | NONE | stub/ni |
| 75 | #75 (ThemeForwardCapturedMouseMessage(i7 RESEARCH.md)) | - | - | - | NONE | stub/ni |
| 76 | #76 | - | - | - | NONE | stub/ni |
| 77 | #77 [fwd SHUNIMPL.#432] | - | - | - | NONE | stub/ni |
| 78 | #78 [fwd SHUNIMPL.#433] | - | - | - | NONE | stub/ni |
| 79 | #79 [fwd SHUNIMPL.#434] | - | - | - | NONE | stub/ni |
| 80 | #80 [fwd SHUNIMPL.#435] | - | - | - | NONE | stub/ni |
| 81 | #81 [fwd SHUNIMPL.#430] | - | - | - | NONE | stub/ni |
| 82 | #82 [fwd SHUNIMPL.#436] | - | - | - | NONE | stub/ni |
| 83 | #83 [fwd SHUNIMPL.#437] | - | - | - | NONE | stub/ni |
| 84 | #84 | - | - | - | NONE | stub/ni |
| 85 | #85 | - | - | - | NONE | stub/ni |
| 86 | #86 | - | - | - | NONE | stub/ni |
| 87 | #87 [fwd SHUNIMPL.#438] | - | - | - | NONE | stub/ni |
| 88 | #88 [fwd SHUNIMPL.#439] | - | - | - | NONE | stub/ni |
| 89 | DrawThemeText | - | - | - | PIN | real/partial |
| 90 | #90 | - | - | - | NONE | stub/ni |
| 91 | #91 | - | - | - | NONE | stub/ni |
| 92 | #92 | - | - | - | NONE | stub/ni |
| 93 | #93 | - | - | - | NONE | stub/ni |
| 94 | #94 | - | - | - | NONE | stub/ni |
| 95 | GetImmersiveColorFromColorSetEx | C | test_GetImmersiveColors | - | PIN | stub/ni |
| 96 | #96 (GetImmersiveColorTypeFromName) | B | test_GetImmersiveColors | - | NONE | stub/ni |
| 97 | #97 | - | - | - | NONE | stub/ni |
| 98 | GetImmersiveUserColorSetPreference | C | test_GetImmersiveColors | - | PIN | stub/ni |
| 99 | #99 | - | - | - | NONE | stub/ni |
| 100 | #100 (GetImmersiveColorNamedTypeByIndex) | B | test_GetImmersiveColors | - | NONE | stub/ni |
| 101 | #101 | - | - | - | NONE | stub/ni |
| 102 | #102 | - | - | - | NONE | stub/ni |
| 103 | DrawThemeTextEx | C | test_DrawThemeTextEx: NULL htheme -> E_HANDLE, DTT_FONTPROP/DTT_TEXTCOLOR, return codes only | - | PIN | stub/ni |
| 104 | #104 (RefreshImmersiveColorPolicyState) | - | - | - | NONE | stub/ni |
| 105 | #105 | - | - | - | NONE | stub/ni |
| 106 | #106 | - | - | - | NONE | stub/ni |
| 107 | #107 | - | - | - | NONE | stub/ni |
| 108 | #108 | - | - | - | NONE | stub/ni |
| 109 | #109 | - | - | - | NONE | stub/ni |
| 110 | #110 | - | - | - | NONE | stub/ni |
| 111 | #111 | - | - | - | NONE | stub/ni |
| 112 | #112 | - | - | - | NONE | stub/ni |
| 113 | #113 | - | - | - | NONE | stub/ni |
| 114 | EnableThemeDialogTexture | A | test_EnableThemeDialogTexture (v6 manifest only; flag matrix) | - | PIN | real/partial |
| 115 | #115 | - | - | - | NONE | stub/ni |
| 116 | #116 | - | - | - | NONE | stub/ni |
| 117 | #117 | - | - | - | NONE | stub/ni |
| 118 | #118 | - | - | - | NONE | stub/ni |
| 119 | #119 | - | - | - | NONE | stub/ni |
| 120 | GetUserColorPreference | - | - | - | PIN | stub/ni |
| 121 | GetColorFromPreference | - | - | - | PIN | stub/ni |
| 122 | #122 | - | - | - | NONE | stub/ni |
| 123 | #123 | - | - | - | NONE | stub/ni |
| 124 | #124 | - | - | - | NONE | stub/ni |
| 125 | #125 | - | - | - | NONE | stub/ni |
| 126 | #126 | - | - | - | NONE | stub/ni |
| 127 | #127 | - | - | - | NONE | stub/ni |
| 128 | #128 | - | - | - | NONE | stub/ni |
| 129 | EnableTheming | A | test_EnableTheming (deep: system colors/metrics/NONCLIENTMETRICS side effects; runs last because it disables theming) | - | PIN | stub/ni |
| 130 | #130 | - | - | - | NONE | stub/ni |
| 131 | #131 | - | - | - | NONE | stub/ni |
| 132 | #132 (ShouldAppsUseDarkMode) | B | test_ShouldAppsUseDarkMode | - | NONE | stub/ni |
| 133 | #133 (AllowDarkModeForWindow) | - | - | - | NONE | stub/ni |
| 134 | EndBufferedAnimation | - | - | - | PIN | stub/ni |
| 135 | #135 (SetPreferredAppMode) | - | - | - | NONE | stub/ni |
| 136 | #136 (FlushMenuThemes) | - | - | - | NONE | stub/ni |
| 137 | #137 (IsDarkModeAllowedForWindow) | - | - | - | NONE | stub/ni |
| 138 | #138 (ShouldSystemUseDarkMode) | B | test_ShouldSystemUseDarkMode | - | NONE | stub/ni |
| 139 | #139 | - | - | - | NONE | stub/ni |
| 140 | EndBufferedPaint | A | test_buffered_paint | - | GUEST | stub/ni |
| 141 | GetBufferedPaintBits | B | test_buffered_paint | - | GUEST | stub/ni |
| 142 | GetBufferedPaintDC | B | test_buffered_paint | - | PIN | stub/ni |
| 143 | GetBufferedPaintTargetDC | B | test_buffered_paint | - | PIN | stub/ni |
| 144 | GetBufferedPaintTargetRect | B | test_buffered_paint | - | PIN | stub/ni |
| 145 | GetCurrentThemeName | B | test_GetCurrentThemeName (buffer sizes, NULL args) | - | PIN | stub/ni |
| 146 | GetThemeAnimationProperty | - | - | - | PIN | stub/ni |
| 147 | GetThemeAnimationTransform | - | - | - | PIN | stub/ni |
| 148 | GetThemeAppProperties | - | - | - | PIN | stub/ni |
| 149 | GetThemeBackgroundContentRect | - | - | - | PIN | real/partial |
| 150 | GetThemeBackgroundExtent | - | - | - | PIN | real/partial |
| 151 | GetThemeBackgroundRegion | D | test_GetThemeBackgroundRegion: Rebar RP_BAND only (E_UNEXPECTED on Win10) | - | PIN | real/partial |
| 152 | GetThemeBitmap | - | - | - | PIN | stub/ni |
| 153 | GetThemeBool | C | TMT_TRANSPARENT on SBP_ARROWBTN, SPNP_UP; TMT_GLYPHTRANSPARENT | - | PIN | real/partial |
| 154 | GetThemeColor | - | - | - | PIN | real/partial |
| 155 | GetThemeDocumentationProperty | - | - | - | PIN | stub/ni |
| 156 | GetThemeEnumValue | D | TMT_BGTYPE (RP_BAND -> BT_NONE), TMT_GLYPHTYPE | - | PIN | real/partial |
| 157 | GetThemeFilename | - | - | - | PIN | stub/ni |
| 158 | GetThemeFont | - | - | - | PIN | real/partial |
| 159 | GetThemeInt | D | test_GetThemePartSize: only TMT_TRUESIZESTRETCHMARK and TMT_MINDPI1..7 on checkbox | - | PIN | real/partial |
| 160 | GetThemeIntList | C | test_GetThemeIntList: only TMT_TRANSITIONDURATIONS on PUSHBUTTON/NORMAL | - | PIN | stub/ni |
| 161 | GetThemeMargins | - | - | - | PIN | real/partial |
| 162 | GetThemeMetric | - | - | - | PIN | real/partial |
| 163 | GetThemePartSize | C | test_GetThemePartSize: ONLY BP_CHECKBOX/CBS_CHECKEDNORMAL TS_DRAW, DPI scaling 13/26 px; no other part | - | PIN | real/partial |
| 164 | GetThemePosition | - | - | - | PIN | real/partial |
| 165 | GetThemePropertyOrigin | - | - | - | PIN | real/partial |
| 166 | GetThemeRect | - | - | - | PIN | real/partial |
| 167 | GetThemeStream | - | - | - | PIN | stub/ni |
| 168 | GetThemeString | - | - | - | PIN | real/partial |
| 169 | GetThemeSysBool | - | - | - | PIN | stub/ni |
| 170 | GetThemeSysColor | - | - | - | PIN | stub/ni |
| 171 | GetThemeSysColorBrush | - | - | - | PIN | stub/ni |
| 172 | GetThemeSysFont | - | - | - | PIN | stub/ni |
| 173 | GetThemeSysInt | - | - | - | PIN | stub/ni |
| 174 | GetThemeSysSize | - | - | - | PIN | stub/ni |
| 175 | GetThemeSysString | - | - | - | PIN | stub/ni |
| 176 | GetThemeTextExtent | - | - | - | PIN | real/partial |
| 177 | GetThemeTextMetrics | - | - | - | PIN | real/partial |
| 178 | GetThemeTimingFunction | - | - | - | PIN | stub/ni |
| 179 | GetThemeTransitionDuration | A | test_GetThemeTransitionDuration (arg validation matrix, PBS_* state range, durations vs int list) | - | PIN | real/partial |
| 180 | GetWindowTheme | B | test_GetWindowTheme, test_SetWindowTheme | comctl32 toolbar.c:2769, treeview.c:2411/2444 | PIN | stub/ni |
| 181 | HitTestThemeBackground | - | - | - | PIN | real/partial |
| 182 | IsAppThemed | C | test_IsThemed single line 313 | - | PIN | real/partial |
| 183 | IsCompositionActive | - | - | - | BEHAVIOURAL | real/partial |
| 184 | IsThemeActive | B | test_IsThemed; gate for ~all other tests | comctl32 propsheet/toolbar/misc; user32 class.c:2325 (uxtheme lazily loaded iff theming), scroll.c:1074, win.c:1543 | GUEST | real/partial |
| 185 | IsThemeBackgroundPartiallyTransparent | C | SBP_ARROWBTN, SPNP_UP/UPS_NORMAL; treeview.c TVP_GLYPH | comctl32 treeview.c:2412 (TVP_GLYPH) | PIN | stub/ni |
| 186 | IsThemeDialogTextureEnabled | A | test_EnableThemeDialogTexture, propsheet.c:1294 | comctl32 propsheet.c:1294 (== is_v6 even when theming off) | PIN | stub/ni |
| 187 | IsThemePartDefined | A | test_IsThemePartDefined (Button/Edit parts, state 0 vs nonzero, NULL htheme -> E_HANDLE) | - | PIN | stub/ni |
| 188 | OpenThemeData | A | test_OpenThemeData, _OpenThemeDataEx, GetThemePartSize, IsThemePartDefined (NULL/invalid class, case, class lists, E_PROP_ID_UNSUPPORTED, GLE E_POINTER; theme-on vs off branches) | comctl32 toolbar.c:2786 | BEHAVIOURAL | real/partial |
| 189 | OpenThemeDataForDpi | B | test_OpenThemeDataForDpi | - | PIN | real/partial |
| 190 | SetThemeAppProperties | - | - | - | PIN | stub/ni |
| 191 | SetWindowTheme | B | test_SetWindowTheme (properties, WM_THEMECHANGED sequence) | - | PIN | real/partial |
| 192 | SetWindowThemeAttribute | - | - | - | PIN | real/partial |
| 193 | ThemeInitApiHook | - | - | - | PIN | stub/ni |
Totals [V from the generator output]: Wine `system.c` touches 40 of 193 exports (A 10, B 15, C 9, D 4, B* 2 = 40); 153 exports have no third-party test at all; our corpora cover 0 of 193; Industrial7 level = 75 PIN + 8 GUEST + 2 BEHAVIOURAL + 108 NONE. Of the 85 named exports, Wine tests 34 (plus 6 ordinal-only ones: 2, 3, 96, 100, 132, 138); the 51 named exports it never calls include the entire data-reading family: `GetThemeColor`, `GetThemeMetric`, `GetThemeMargins`, `GetThemePosition`, `GetThemeRect`, `GetThemeFont`, `GetThemeString`, `GetThemeFilename`, `GetThemeBitmap`, `GetThemeStream`, `GetThemePropertyOrigin`, `GetThemeDocumentationProperty`, all `GetThemeSys*` (Bool, Color, ColorBrush, Font, Int, Size, String), `GetThemeBackgroundContentRect`, `GetThemeBackgroundExtent`, `GetThemeTextExtent`, `GetThemeTextMetrics`, `HitTestThemeBackground`, `DrawThemeBackground`, `DrawThemeText`, `DrawThemeIcon`, `DrawThemeParentBackgroundEx`, `GetThemeAppProperties`, `SetThemeAppProperties`, `SetWindowThemeAttribute`, `IsCompositionActive`, `GetThemeAnimationProperty/Transform`, `GetThemeTimingFunction`, `BufferedPaintInit/UnInit/SetAlpha/RenderAnimation/StopAllAnimations`, `Begin/EndBufferedAnimation`, `Begin/End/UpdatePanningFeedback`, `GetUserColorPreference`, `GetColorFromPreference`, `ThemeInitApiHook`, the three COM entry points.

## 9. What Wine covers vs the gaps

**Wine covers (and the new corpus can port the *shape* of, rerecorded against Win10):**
* handle/argument validation: OpenThemeData(Ex) NULL/invalid/class-list/case rules and `GetLastError` values; IsThemePartDefined; GetThemeTransitionDuration full error matrix; CloseThemeData; buffered paint arg matrix; GetWindowTheme/SetWindowTheme property and WM_THEMECHANGED plumbing; EnableThemeDialogTexture/IsThemeDialogTextureEnabled (needs v6 manifest); EnableTheming side effects; DrawThemeParentBackground message sequences; dark-mode and immersive-colour queries; Win10-specific `GetThemeBackgroundRegion` -> E_UNEXPECTED on RP_BAND.
* Consumer behaviour: WM_THEMECHANGED repaint/erase sequence for 25 comctl32 classes; user32 lazy uxtheme load; scrollbar pixel sanity.

**Gaps (nothing tests these anywhere):**
1. Any *value* read from an armed Win10 theme: GetThemePartSize (all parts other than checkbox), GetThemeMetric, GetThemeInt (all props other than stretch/mindpi), GetThemeMargins, GetThemePosition, GetThemeRect, GetThemeColor, GetThemeFont, GetThemeEnumValue (other than BGTYPE), GetThemeBool (other than TRANSPARENT), GetThemeFilename/String/IntList (other than one prop), `GetThemeSys*`.
2. Header-control sizing: `HP_HEADERITEM` states HIS_NORMAL/HOT/PRESSED/SORTEDNORMAL..., HP_HEADERSORTARROW, HP_HEADERDROPDOWN(FILTER), `HDM_LAYOUT` result height under v6+theme (CC30), `HDM_GETITEMRECT` height, and `LVM_GETHEADER` height inside a report list.
3. Buttons (BP_PUSHBUTTON/CHECKBOX/RADIOBUTTON/GROUPBOX/COMMANDLINK sizes and content rects, `BCM_GETIDEALSIZE` with theme), tabs (TABP_*: item/body, `TCM_ADJUSTRECT`, `TCM_GETITEMRECT` themed), list view (LVP_*, header interplay), toolbar, rebar, progress (PP_*), trackbar (TKP_*), scrollbar (SBP_*), spin (SPNP_*), edit/combobox (EP_*, CP_*), menu (MENU_*), tooltips, status bar, treeview (TVP_*), taskdialog, window frame (WP_*) - 0 size/metric assertions across the board.
4. `GetThemeBackgroundContentRect`/`Extent` and text extent for any part (themed layout arithmetic), `DrawThemeBackground` pixels (golden bitmaps), `HitTestThemeBackground`.
5. Theme-OFF behaviour on a real Win10 (what we currently implement from Win7 decomp): exact HRESULT / `GetLastError` for each of the 193 exports when `IsThemeActive()` is FALSE or `SetWindowTheme(hwnd, L"", L"")` is used (Wine tests skip, not assert, these paths). Includes `0x80070490`, `0x80070006`, `0x8000FFFF` rows.
6. Ordinal-only internals (108 incl. 34 SHUNIMPL forwarders): Wine tests 6; no test of existence/forwarder resolution (a "forwarder-resolves-to-SHUNIMPL" and "export-table-shape" test is cheap and absent from our corpora).
7. Per-DPI and per-window-DPI theme data (OpenThemeDataForDpi at 96/120/144/192), which Wine tests only for the checkbox and by mutating system DPI.
8. Sys* and non-client: GetThemeSysColor/Size/Font/Int/Bool/String/ColorBrush(NULL and real htheme), `SetWindowThemeAttribute(WTA_NONCLIENT)`, `GetThemeAppProperties` / `SetThemeAppProperties` STAP_* flags and their effect on controls.
9. Animation/transition API: GetThemeTimingFunction, GetThemeAnimation*, BufferedPaintRenderAnimation, BeginBufferedAnimation (stubbed in i7, untested everywhere).
10. comctl32 v6 activation for *our* harness (no manifest exists in any of our corpora, section 5).

## 10. Prioritised gap list for the new corpus

Ranked by (value for the Industrial7 armed-Win10 personality) x (cost) with the tests' environment constraint that Win10 must be armed (a theme loaded) for data reads:

| Pri | Area (new test area name suggestion) | What to record / assert | Why / evidence |
|---|---|---|---|
| P0 | `theme_state` (theme ON and OFF) | IsThemeActive/IsAppThemed/IsCompositionActive/GetCurrentThemeName/GetThemeAppProperties/SetThemeAppProperties + GetLastError for each; OpenThemeData(NULL,NULL), (hwnd, bad), (hwnd, "Button"), class lists, case, OpenThemeDataEx flags, OpenThemeDataForDpi at 96..192; CloseThemeData(NULL/garbage) | Wine covers shape only; i7 has Win7-derived rows (0x80070490/0x80070006/0x8000FFFF) that need Win10 confirmation; also needed to know when *Win10 itself* hands out NULL |
| P0 | `parts_header` | GetThemePartSize/GetThemeMetric/GetThemeInt(TMT_HEIGHT, TMT_MINSIZE, TMT_DEFAULTPANESIZE...)/GetThemeMargins/GetThemePosition for HP_HEADERITEM (HIS_*), HP_HEADERITEMLEFT/RIGHT, HP_HEADERSORTARROW, HP_HEADERDROPDOWN* at 96/120/144/192 DPI; plus comctl32 v6 `HDM_LAYOUT`, `HDM_GETITEMRECT` height, `LVM_GETHEADER` height with/without theme | The CC30 symptom; zero existing coverage; i7 header doc waiver (`COMCTL32-HEADER-W10.md:163-165`) is waiting on exactly these numbers |
| P0 | `parts_core_controls` | Same probe set for Button (PUSHBUTTON, CHECKBOX, RADIOBUTTON, GROUPBOX, COMMANDLINK), Tab (TABITEM*, PANE, BODY), ListView (LVP_*, LISTITEM states, GROUPHEADER), Edit, ComboBox, ScrollBar, Spin, Progress, Trackbar, Toolbar, Rebar, Status, TreeView, ToolTip, Menu, Window | Wine tests only the 13 px checkbox; these are the controls the owner says "hit the same wall" |
| P1 | `theme_props_enum` | systematic walk: for each (class, part 0..N, state 0..M) call IsThemePartDefined and record which succeed - produces the "armed-Win10 recorded-constants table" skeleton; GetThemePropertyOrigin; GetThemeDocumentationProperty; GetThemeFilename | gives completeness proof of the table; none of these exist |
| P1 | `theme_errors` | HRESULT matrix for every handle-taking export with NULL, garbage and closed handles, NULL out-pointers, invalid part/state/prop (extend Wine's GetThemeTransitionDuration style matrix to all ~45 handle exports) | i7 `behavior.rs` pins E_HANDLE rows from Win7; need Win10 truth; cheap, deterministic, DPI/theme independent |
| P1 | `theme_sys` | GetThemeSysColor/Size/Font/Int/Bool/String/ColorBrush with NULL and real htheme across all COLOR_*/SM_*/TMT_* ids vs GetSysColor/GetSystemMetrics | i7 `Class::SysClassic` rows are Win7 [U]; no tests anywhere |
| P1 | `export_shape` | export table: 193 entries, 34 SHUNIMPL forwarders, ordinal 47/49/72/75/96/100/132/138 identities, DllGetClassObject/DllCanUnloadNow/DllGetActivationFactory behaviour | `tools/uxtheme_exports.json` already exists; 108 ordinal rows are 'NONE' in the i7 coverage table |
| P1 | `window_props` | SetWindowTheme/GetWindowTheme/SetWindowThemeAttribute/EnableThemeDialogTexture/IsThemeDialogTextureEnabled property atoms and WM_THEMECHANGED/WM_THEME... sequences (port Wine's shape, v6 manifest) | i7 `window.rs` has 17 Win7-derived tests; Wine confirms the shape; low cost |
| P2 | `draw_pixels` | DrawThemeBackground/Ex/Edge/Text/TextEx/Icon/ParentBackground(Ex) into a DIB: record pixel hashes or small golden bitmaps for a handful of parts (aero control states), GetThemeBackgroundRegion, HitTestThemeBackground, GetThemeTextExtent/Metrics | needed for rendering fidelity; heavier, DPI/font dependent, put behind a "pixel" flag; relates to the gdi32 `NtGdiDrawStream` gap |
| P2 | `consumer_sizes` | comctl32 v6 absolute sizes with theme on and off: `BCM_GETIDEALSIZE`, `TCM_ADJUSTRECT`, tab/header/listview item heights, toolbar/rebar band heights, status bar, progress, trackbar | ties the constants table to observable control geometry; Wine asserts none |
| P2 | `buffered_paint` | port Wine's matrix + BufferedPaintInit/UnInit/SetAlpha and animation APIs on Win10 | partially covered by Wine and by `comctl32_logic::bp`; animation API untested everywhere |
| P3 | `dark_mode_immersive` | ShouldAppsUseDarkMode/SystemUseDarkMode/GetImmersive* ordinal APIs (Wine test is registry-gated) | Win10 only; low priority for i7 |
| P3 | `wm_themechanged_matrix` | extend Wine misc.c `test_WM_THEMECHANGED` class list to all comctl32 v6 classes incl. new ones | cheap behaviour oracle for what controls do when theme appears/disappears |

Harness requirements that follow from the inventory: (a) comctl32 v6 manifest activation (none in any of our corpora; Wine's `v6util.h` writes `cc6.manifest` and uses an activation context, a model to follow, or embed a manifest resource); (b) a controlled theme state: record under the default Win10 Aero-lite (`aero.msstyles`/`Windows 10.theme`) and, as a separate pass, with theming forced off via `SetWindowTheme(hwnd, L"", L"")` / `EnableTheming(FALSE)`-**never** mutate DPI or system colours of the host as Wine's `test_GetThemePartSize`/`test_EnableTheming` do without a restore guard; (c) DPI handled by `OpenThemeDataForDpi` and per-window DPI awareness rather than by changing the display; (d) every recorded number written as NOTE lines for the constants table, not just pass/fail.

## 11. Unknowns and caveats

* **UNKNOWN**: whether `test_OpenThemeFile` on Win10 win_skips or fails (ordinal 2 is a SHUNIMPL forwarder; behaviour of GetProcAddress on an unresolvable forwarder not run).
* **UNKNOWN**: the Win10 19041 ordinal-to-name mapping for most ordinal-only exports; names in section 8 come from Wine's spec (XP/Win7-era) and are labelled [I].
* **UNKNOWN**: contents of the 8 GUEST-level rows' guest tests (not enumerated).
* `todo_wine` lines were taken from `grep -n todo_wine system.c`; only 921, 930, 1562 and 2813 were read in context.
* No Windows 10 run was performed; no Wine process was started for this document.

## 9. Verification (adversarial fact-check pass)

Re-checked by command against the live tree/corpus on 2026-11-09. Result: the document's load-bearing claims hold; two count errors were fixed in place.

**Confirmed [V, re-run]**
- `system.c` is 2947 lines, `START_TEST(system)` at 2905, ordinals 2/3/138/132/96/100 fetched by `GetProcAddress(MAKEINTRESOURCEA(n))` at lines 85-90, ordinal 47 identity assert at 2503-2504. `tools/uxtheme_exports.json`: 193 entries, 85 named, 34 with `SHUNIMPL.#...` forwarders; ordinals 2 (`SHUNIMPL.#440`) and 3 (`#441`) are forwarders; 47 is the named `DrawThemeBackgroundEx`; 49/72/75/96/100/132/138 are present local exports.
- No `GetThemeMetric/Color/Margins/Position/Font/Rect/String/Sys*`, `TMT_HEIGHT/WIDTH/MINSIZE` or `GetThemeBackgroundContentRect` in `system.c` (grep returned nothing). The only `GetThemePartSize` size expectation is the 13 px checkbox (`system.c:1219,1240,1288,1300`).
- Wine `comctl32/header.c` uses `HP_HEADERITEM` at lines 296, 297, 375, 392, 634. `test_hdm_layout` is at `tests/header.c:911` and asserts only `windowPos.hwnd`, `hwndInsertAfter` and a message sequence; no height value. `todo_wine` at system.c 921, 930, 1562, 2813 confirmed (additional todo_wine sites exist at 1048, 1052, 2179, 2182, 2246, which the main text does not describe).
- Wine tree HEAD `4f4d68f6...`; `broken(` counts per file (rebar 1, treeview 2, tab 4, button 7, listview 6, status 11, header 1) re-counted and match.
- Repo-wide grep (with `-E`, extended regex) of `~/industrial7_test_*` and `~/test_*` finds only `test_win32k/.../sc_load.c:71` outside `industrial7_test_theme` itself. `tests/` has only `core_smoke` (11 lines).
- `owned-test-coverage.tsv` uxtheme rows: 193 = 75 PIN + 8 GUEST + 2 BEHAVIOURAL + 108 NONE; the 108 NONE are exactly the 108 `@N` (ordinal-only) rows. `exports_manifest.tsv`: 35 real rows of 193 (194 non-comment lines include the header). `w10-uxtheme-off/src/lib.rs`: 26 `#[test]`, 1100 lines; header comment "Every row is Win7-derived ... Win10 19041 uxtheme is not in the corpus" present. `OWNED_STATUS.md:252` row and `STAGES.md:331` text confirmed. `COMCTL32-HEADER-W10.md:160-165` waiver confirmed. "CC30" has no relevant hit in Industrial7 (only hex-string noise).
- vsstyle.h: HP_HEADERITEM=1, ITEMLEFT=2, ITEMRIGHT=3, SORTARROW=4, DROPDOWN=5, DROPDOWNFILTER=6, OVERFLOW=7. vssym32.h: TMT_MINSIZE=3403 (0xd4b), TMT_DEFAULTPANESIZE=5002.

**Corrected**
- Section 2 said Wine `system.c` has "31 test fns": the grep yields 26 `static void test_*` (25 tests plus helper `test_buffer_dc_props`) plus `START_TEST`.
- Section 6.1 test counts for `uxtheme-w10/tests`: `grep -c '#[test]'` gives manifest.rs 7, behavior.rs 12 (doc said 10), window.rs 19 (doc said 17).

**Missing finding added (the CC30 mechanism, from the corpus; not previously in this document)**
`curl http://docker1.dev.moonlightcompanies.com:8765/api/function/comctl32/_Header_InitTheme/decomp` (RVA 0x85d98) shows the exact probes a header theme test needs, matching the earlier statement that the Win10 value is "not computable on the host":
- `ResetThemeData(hwnd, &theme, L"Header")`; if theme non-NULL:
- `GetThemePartSize(theme, NULL, 1 /*HP_HEADERITEM*/, 0, NULL, TS_TRUE, &size)` then `GetThemePosition(theme, 1, 0, 0xd4b /*TMT_MINSIZE*/, &pt)`; if it succeeds, size = max(size, pt) per axis; `size.cy` is stored at HD+0x160 (the themed header-height input).
- `GetThemePartSize` for part 5 (HP_HEADERDROPDOWN) -> cx, max-ed into HD+0x114; `GetSystemMetrics(0x2d)` stored at HD+0x118; part 4 (HP_HEADERSORTARROW) size into HD+0x158; part 7 (HP_HEADEROVERFLOW) size into HD+0x144; all state 0, TS_TRUE, NULL hdc.
So Wine's header tests plus Wine's uxtheme tests cover none of (HP 1/4/5/7, state 0, TS_TRUE) nor TMT_MINSIZE; these are the minimum P0 probes for CC30.

**UNVERIFIED / not re-checked**
- What GetProcAddress returns on Win10 for the SHUNIMPL-forwarded ordinals 2 and 3 (no Windows run).
- Wine ordinal-name mapping for ordinal-only exports other than those corroborated (inference, as labelled).
- Contents of the 8 GUEST rows, `listview/shared.rs` and `rb/v6/state.rs` GetThemePartSize lines, `cc20_header_probe.c` has never been run (its `layout.height` check only asserts `wp.cy >= classic`, line 175, so it cannot distinguish 20 from 24 even when run; added observation).
- The full 193-row matrix (section 8) was only spot-checked (rows 1-68 against the JSON); remaining rows not individually re-derived.
