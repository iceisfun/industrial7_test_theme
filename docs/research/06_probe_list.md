# 06 - Probe list for the Win10 oracle lane (uxtheme 10.0.19041.2193 / comctl32 6.10.19041.1110)

Role: PROBE-LIST AUTHOR. Research only. **Nothing in this document is a Windows 10 measurement**: the recorder
was built and validated for plumbing under Wine (section 10). Every row below is a *question* for the Win10 oracle
(laneg); no answer is claimed. Consumer needs come from `01_consumers.md` (sections 2-4, 7), the API behaviour to
verify from `02_win10_semantics.md` (sections 5-8, 13-15), the Wine comparison from `03_wine_evaluation.md`.

Deliverables (all committed):

| file | what |
|---|---|
| `tools/gen_probe_table.py` | generates the table; symbol ids are checked against the SDK headers (`vsstyle.h`, `vssym32.h` of mingw-w64) and generation aborts on a mismatch |
| `tools/probe_table.csv` | **the probe table** (7177 rows; input of the recorder) |
| `tools/probe_record.c` | the recorder (plain C, mingw-w64, `LoadLibrary`/`GetProcAddress` only; one exe) |
| `tools/probe_record.rc`, `probe_app.manifest`, `probe_ctl6.manifest` | embedded manifests (Win10 compat; comctl32 v6 activation-context manifest) |
| `tools/Makefile.probe` | `make -f tools/Makefile.probe` -> `build/probe_record.exe` + `build/probe_table.csv` |
| `tools/probe_run.bat` | the standard passes for one machine configuration, no judgment |
| `tools/probe_compare.py` | diff / determinism check of two or more `results.csv` |
| `tools/wine_probe.sh` | runs the recorder under Wine in a private Xvfb (never the live X session) |
| `tools/probe_groups.md`, `tools/probe_catalog.md` | generated statistics / class-part-state catalogue (spliced below) |

---

## 1. Quick start for the lane (Windows 10 x64, interactive desktop session)

```
# on the build host
make -f tools/Makefile.probe                 # needs x86_64-w64-mingw32-gcc + windres; python3 regenerates the table if needed
# copy build\probe_record.exe  build\probe_table.csv  tools\probe_run.bat  to one folder on the Win10 VM
# on Win10, cmd.exe, normal user (not elevated unless the run is labelled adm):
probe_run.bat aero-light-100                 # label = <theme>-<mode>-<scale>, see section 2
python tools\probe_compare.py results\run1.csv results\run2.csv results\run3.csv   # 3 repeats must be byte-identical (0 differences)
```

Minimal run (about a minute): `probe_record.exe --prio 0 --label aero-light-100 --out results.csv` - this is the
CC30 set (section 5). `--prio 2` (default) = everything except the very large `gridfull` discovery group;
`--sweep full --prio 3` additionally runs `PropGrid` (every defined part/state x every property id). `--list` prints the
rows that would run. The recorder re-launches itself as a child; a crash or a hang (default 120 s per row,
`--timeout`) is logged as a `CRASH`/`HANG` row and the run continues with the next row - nothing to babysit.

Command line (also in the header comment of `tools/probe_record.c`):

```
probe_record.exe [--table probe_table.csv] [--out results.csv] [--label NAME] [--prio N] [--grp a,b]
                 [--sweep std|full|none] [--comctl v6|v5] [--dpi-aware system|pm|pmv2|unaware]
                 [--dark off|allow|force|light] [--app-props N] [--timeout SEC] [--show] [--list]
                 [--no-supervise] [--no-nc]          (internal: --child --from N --to N --append)
```

* `--comctl v6` (default): the exe creates an **activation context from the embedded `probe_ctl6.manifest`**
  (`Microsoft.Windows.Common-Controls 6.0.0.0`) *before* loading comctl32 and creating any window, so `SysHeader32`,
  `SysListView32`, ... are the v6 (visual-styles) controls and `OpenThemeData` is reachable from them. `--comctl v5` skips the
  activation context (system32 comctl32 5.82, classic controls). The Env row `Modules` records the actual comctl32 path and
  version so the lane can see which one was used (a v6 context loads `C:\Windows\WinSxS\amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.19041...\comctl32.dll`).
* `--dpi-aware system` (default) = `SetProcessDpiAwarenessContext(SYSTEM_AWARE)`: the process sees the real system DPI. Use
  `unaware` to record what a legacy app sees (virtualised 96).
* `--dark allow|force|light`: calls uxtheme ordinal 135 (`SetPreferredAppMode(1|2|3)`, 02 section 10) and ordinal 133
  (`AllowDarkModeForWindow(hwnd,TRUE)`) before any open, so the normal rows then record the dark class resolution.
* `--app-props N`: `SetThemeAppProperties(N)` before the run (for a whole-run `STAP_*` experiment; the table also has rows that
  flip it temporarily).
* `--no-nc`: skip rows that use `OpenNcThemeData` (ordinal 49). The supervisor sets it automatically if an `nc=1` row crashes
  (this happens under Wine where ordinal 49 is a stub; on Win10 ordinal 49 is `OpenNcThemeData`, 02 section 5).

## 2. Run configurations (what "theme configuration" means: a property of the machine, set by the lane, named by `--label`)

The recorder cannot switch the system theme; it **records the configuration it ran in** (Env rows: `GetCurrentThemeName`,
registry `ThemeManager`, `Personalize\AppsUseLightTheme`, `SPI_GETHIGHCONTRAST`, DPI, awareness, versions, OS build/UBR,
`SM_REMOTESESSION`) so a result file is self-describing. Label convention `<theme>-<mode>-<scale>`.

| # | label | how to set the machine | recorder flags | what it answers | priority |
|---|---|---|---|---|---|
| C1 | `aero-light-100` | default Windows 10 "Windows" (light) theme, accent default, Settings > System > Display > Scale **100%**, sign out/in after changing scale | `probe_run.bat` (all passes) | the baseline: every number the first armed table needs (header 24 px, ...) | **P0 - do first** |
| C2 | `aero-light-125` `-150` `-175` `-200` | same, scale 125/150/175/200% (system DPI 120/144/168/192; **sign out and in** so the process starts at that DPI); `--dpi-aware system` | `probe_run.bat <label>` | DPI plateaus: part sizes, margins, metrics, sys metrics. The table's `dpi=N` rows (OpenThemeDataForDpi) answer part of this already at 100% (section 3.3), but hdc-scaled getters (`GetThemeMetric`, `GetThemeMargins`) follow the system DPI, so run C2 | P1 (125 and 150 first) |
| C3 | `aero-dark-100` | Settings > Personalization > Colors > "Choose your color": **Dark** | pass 5 of `probe_run.bat` (`--dark allow` and `--dark force`) plus the plain run | dark class resolution (`DarkMode_Explorer`...), dark sys colours, whether part sizes change | P2 |
| C4 | `hc-black-100` (and `hc-white`, `hc1`, `hc2`) | Settings > Ease of Access > High contrast > pick a high-contrast theme and Apply (or left-Alt+left-Shift+PrtScn) | `probe_run.bat` | HC gate (02 section 2.4): expect `OpenThemeData` NULL with LastError unchanged, `IsThemeActive` FALSE; `Env/HighContrast` row proves HC was on | P1 |
| C5 | `themessvc-off-100` | elevated: `sc stop Themes` (or `sc config Themes start= disabled` + reboot, then restore `start= auto`). The effect on an already running session is **UNKNOWN** and is exactly what the run records (02 scenario matrix columns NT/NS are inference) | `probe_run.bat` | "themes service off / no theme sections": does `OpenThemeData` return NULL, what does `IsThemeActive` say, does comctl32 v6 fall back to the 20 px header? | P1 |
| C6 | `aero-light-100-v5` | (same machine as C1) | pass 3 of `probe_run.bat` (`--comctl v5`) | classic comctl32 v5 control heights for comparison | in C1 |
| C7 | `aero-light-100-appprops0` | (same machine as C1) | pass 4 (`--app-props 0`) | `SetThemeAppProperties(0)` effect on opens | in C1 |
| C8 | `adm` | run C1 elevated | `probe_run.bat aero-light-100-adm` | the pipeline wants std x3 and adm x3 byte-identical (05 section 3.1) | P2 |
| C9 | `classic` | **Windows 10 has no classic theme** (02 section 13): "classic" = C4 (HC) or per-window `SetWindowTheme(h,L"",L"")` (rows `Ctl header theme=off`, `OpenTheme subapp="(empty)"`) | - | - | - |

Unknown at authoring time (so recorded, not assumed): which `.msstyles` the oracle VM runs (`aero.msstyles` vs `aerolite.msstyles`:
Env `GetCurrentThemeName` + `ThemeManager.DllName` rows), whether the VM is a remote session (`remote` row), and the display scale of
earlier CC30 fixtures (05 says dpiY 96).

## 3. Table and result formats

### 3.1 `probe_table.csv` (input, one probe per line; fields never contain a comma)

`id,prio,grp,api,class,part,state,prop,ts,dpi,subapp,nc,aux,partname,statename,propname,note`

| column | meaning |
|---|---|
| `id` | `P00001`...: stable only for one generated table; join results by `id` of the table that was used (the table is committed together with results) |
| `prio` | 0 = CC30 header-height set + environment (54 rows); 1 = consumer-required; 2 = remaining measurement + discovery; 3 = heavy (`--sweep full`) |
| `grp` | probe group (section 4); `--grp` filter |
| `api` | uxtheme API under test (`GetThemePartSize`, ...), or a recorder verb: `OpenTheme`, `Env`, `Ctl` (create the real control and measure it), `PropSweep`, `PartGrid`, `PropGrid`, `SetThemeAppProperties`, `SetWindowTheme`, `CloseThemeData` |
| `class` | the **class list string passed to OpenThemeData** (e.g. `Header`, `Button-OK;Button`, `Indeterminate::Progress`). Special: `-` or `@NULLH` = pass `hTheme = NULL`; `@BOGUS` = forged `HTHEME 0x7fff0001`; `@STALE` = a handle that was closed; `@NULL` / `@EMPTY` = `pszClassList` NULL / L"". For `Ctl` it is the control kind (`header`, `lvheader`, `status`, `tab`, `toolbar`, `combo`, `edit`, `button`, `trackbar`, `updown`, `listbox`); for `Env` the item |
| `part`, `state` | `iPartId`, `iStateId` (numbers; names in `partname`/`statename`, verified against the SDK headers) |
| `prop` | `iPropId` (TMT_* number; name in `propname`); for `GetThemeSys*` it is the **id** (COLOR_x index, SM_x, TMT_*) |
| `ts` | `THEMESIZE`: 0 `TS_MIN`, 1 `TS_TRUE`, 2 `TS_DRAW` (GetThemePartSize only) |
| `dpi` | 0 = `OpenThemeData(hwnd, cls)` (object DPI = window/system DPI); N = `OpenThemeDataForDpi(hwnd, cls, N)` (N in 96,120,144,168,192). **Caveat**: the `HDC` passed to margins/metric/font getters is a screen DC at the *system* DPI, so those getters mix object-DPI N with DC-DPI; true per-scale numbers need run C2 |
| `subapp` | non-empty = `SetWindowTheme(hwnd, subapp, NULL)` before the open (`Explorer`, `DarkMode_Explorer`, ...); `(empty)` = `L""` |
| `nc` | 1 = open through `OpenNcThemeData` (uxtheme ordinal 49, what comctl32's ScrollBar and `CCDrawNonClientTheme` use) |
| `aux` | `;`-separated `key=value`: `hdc=0\|1\|m` (NULL / screen DC / memory DC; **`GetThemePartSize` defaults to NULL like `_Header_InitTheme`**, everything else to the screen DC), `rect=l:t:r:b` (input rectangle), `text=`, `flags=` (DrawText flags), `font=none\|gui\|segoe9\|msshell8\|lg\|msg\|sys` (font selected into the DC for text probes / `WM_SETFONT` for `Ctl`; `segoe9` = 9 pt Segoe UI scaled by the system DPI, `msshell8` = 8 pt MS Shell Dlg), `noout=1` (pass NULL for the output pointer), `to=` (target state for `GetThemeTransitionDuration`), `hwnd=0\|bad`, `flags=` (OpenThemeDataEx), `twice=1`, `pmax=`/`smax=` (grid bounds), `items=`, `style=`, `kind=`, `theme=off\|explorer` (Ctl), `heavy=1` |

### 3.2 `results.csv` (output)

`id,sub,hr,lasterr,ret,v0,v1,v2,v3,v4,v5,txt` - one line per probe, several for env items and sweeps (`sub` distinguishes them).

| column | meaning |
|---|---|
| `hr` | `0xHHHHHHHH` HRESULT of the call; `-` for APIs without one (BOOL/int/COLORREF in `ret`); `OPENFAIL` the class did not open (`lasterr` = the LastError of the failed open, i.e. the 0x80070490 of the NT/HC cases); `NOAPI` the verb is unknown; `SKIPPED`; `CREATEFAIL` a `Ctl` window could not be created; `CRASH` / `HANG` the row killed the child (`txt` says exception code); `NA` |
| `lasterr` | `GetLastError()` immediately after the call. The recorder stores `0x5A5A5A5A` first, so **`0x5A5A5A5A` means "the call did not touch LastError"** (this matters: 02 says several uxtheme paths `SetLastError(hr)`, others leave it) |
| `ret` | return value of non-HRESULT APIs; for `OpenTheme` 1 = non-NULL handle (handle values are never recorded: they are not deterministic) |
| `v0..v5` | output values. **-7777 = output never written** (distinguishes "failed and left it alone" from "returned 0") |
| `txt` | strings / symbolic hints |

Output types per API (this is the "expected output type" column of the request):

| api | outputs |
|---|---|
| `GetThemePartSize` | `v0=cx v1=cy` (SIZE) |
| `GetThemeMargins` | `v0=left v1=right v2=top v3=bottom` (MARGINS order cxLeftWidth cxRightWidth cyTopHeight cyBottomHeight) |
| `GetThemePosition` | `v0=x v1=y` (POINT) |
| `GetThemeRect`, `GetThemeBackgroundContentRect`, `GetThemeBackgroundExtent`, `GetThemeTextExtent`, `GetThemeBackgroundRegion` (region box) | `v0..v3 = left top right bottom` |
| `GetThemeInt`, `GetThemeMetric`, `GetThemeEnumValue`, `GetThemeBool`, `GetThemePropertyOrigin` | `v0` (origin 0 `PO_STATE` 1 `PO_PART` 2 `PO_CLASS` 3 `PO_GLOBAL` 4 `PO_NOTFOUND`, `txt` names it) |
| `GetThemeColor`, `GetThemeSysColor`, `GetThemeSysColorBrush` | `v0`/`ret` = COLORREF decimal, `txt` = `0xBBGGRR` hex (COLORREF byte order, not RGB) |
| `GetThemeFont`, `GetThemeSysFont` | `v0=lfHeight v1=lfWeight v2=lfItalic v3=lfCharSet v4=lfQuality v5=lfPitchAndFamily`, `txt = lfFaceName` |
| `GetThemeString`, `GetThemeFilename`, `GetThemeSysString` | `txt` |
| `GetThemeIntList` | `v0 = count`, `txt` = up to 24 values |
| `GetThemeTextMetrics` | `v0=tmHeight v1=tmAscent v2=tmDescent v3=tmInternalLeading v4=tmExternalLeading v5=tmAveCharWidth` |
| `IsThemePartDefined`, `IsThemeBackgroundPartiallyTransparent` | `ret` BOOL (+`lasterr`) |
| `GetThemeSysSize`, `GetThemeSysBool` | `ret` ; `GetThemeSysInt`: `v0` |
| `GetThemeTransitionDuration` | `v0 = ms` |
| `OpenTheme` | `ret`/`v0` non-NULL, `v1` = `GetWindowTheme(hwnd) != NULL` after the open, `v2` = second open returned the same value (`twice=1`) |
| `PropSweep` / `PartGrid` / `PropGrid` | hit rows only, `sub` = `[part:state/]<propid>:<getter>` (e.g. `3403:Position`) or `<part>:<state>:defined|MIN|TRUE|DRAW` |
| `Ctl` | per kind, documented in `txt` of each result row |

### 3.3 Why `dpi` rows, and what they do not cover

Win10 `OpenThemeDataForDpi` chooses the image plateau and scales content margins for the *object* DPI (02 section 4, 6.1). The 96/120/144/168/192 rows
therefore give the per-DPI **part sizes and image selection** without changing the machine. Metrics that are scaled through the HDC (`GetThemeMetric`,
`GetThemeMargins` via `ScaleThemeSize`/`ScaleMargins`, 02 section 6) and every `GetThemeSys*` value follow the *system* DPI - those need the C2 machines.

## 4. Probe groups

| group | rows | prio0 | prio1 | prio2 | prio3 |
|---|---|---|---|---|---|
| env | 9 | 9 | 0 | 0 | 0 |
| open | 197 | 0 | 10 | 187 | 0 |
| err | 133 | 0 | 21 | 94 | 18 |
| header | 869 | 45 | 805 | 19 | 0 |
| button | 408 | 0 | 385 | 23 | 0 |
| tab | 616 | 0 | 609 | 7 | 0 |
| listview | 378 | 0 | 345 | 33 | 0 |
| treeview | 262 | 0 | 255 | 7 | 0 |
| toolbar | 132 | 0 | 83 | 49 | 0 |
| rebar | 302 | 0 | 276 | 26 | 0 |
| progress | 275 | 0 | 217 | 58 | 0 |
| trackbar | 532 | 0 | 510 | 22 | 0 |
| spin | 276 | 0 | 269 | 7 | 0 |
| edit | 308 | 0 | 255 | 53 | 0 |
| combobox | 131 | 0 | 73 | 58 | 0 |
| scrollbar | 443 | 0 | 407 | 36 | 0 |
| status | 147 | 0 | 140 | 7 | 0 |
| tooltip | 358 | 0 | 334 | 24 | 0 |
| window | 496 | 0 | 409 | 87 | 0 |
| menu | 82 | 0 | 0 | 82 | 0 |
| listbox | 43 | 0 | 0 | 43 | 0 |
| monthcal | 119 | 0 | 59 | 60 | 0 |
| datepicker | 185 | 0 | 178 | 7 | 0 |
| static | 12 | 0 | 5 | 7 | 0 |
| link | 16 | 0 | 0 | 16 | 0 |
| textstyle | 53 | 0 | 0 | 53 | 0 |
| globals | 12 | 0 | 0 | 12 | 0 |
| sys | 327 | 0 | 198 | 129 | 0 |
| ctl | 32 | 0 | 31 | 1 | 0 |
| gridfull | 24 | 0 | 0 | 0 | 24 |
| **total** | 7177 | 54 | 5874 | 1207 | 42 |

| api | rows |
|---|---|
| GetThemePartSize | 2429 |
| GetThemeMargins | 719 |
| GetThemeBackgroundContentRect | 502 |
| IsThemePartDefined | 488 |
| GetThemeInt | 370 |
| GetThemeMetric | 338 |
| GetThemeEnumValue | 273 |
| GetThemeColor | 222 |
| OpenTheme | 198 |
| GetThemePosition | 182 |
| IsThemeBackgroundPartiallyTransparent | 177 |
| GetThemeSysSize | 165 |
| GetThemeBool | 137 |
| GetThemeBackgroundExtent | 128 |
| GetThemeFont | 121 |
| GetThemeTextMetrics | 119 |
| GetThemeTextExtent | 101 |
| GetThemeSysColor | 93 |
| GetThemePropertyOrigin | 89 |
| PropSweep | 86 |
| Ctl | 58 |
| GetThemeSysFont | 33 |
| PartGrid | 24 |
| PropGrid | 24 |
| GetThemeSysString | 18 |
| GetThemeFilename | 12 |
| GetThemeTransitionDuration | 12 |
| Env | 9 |
| GetThemeSysInt | 9 |
| GetThemeIntList | 6 |
| GetThemeRect | 6 |
| GetThemeString | 6 |
| GetThemeSysBool | 6 |
| SetThemeAppProperties | 6 |
| CloseThemeData | 3 |
| GetThemeSysColorBrush | 3 |
| SetWindowTheme | 3 |
| GetThemeBackgroundRegion | 2 |


Priorities: **0** = what the header-height fix needs (section 5) and the environment; **1** = what comctl32 actually calls
(01 section 3/7, plus the error-ordering facts 02 calls the highest-value first probes); **2** = remaining parts/states/properties, class discovery,
overrides; **3** = heavy discovery.

## 5. Group `header` - the CC30 block (priority 0)

Why: `Header_OnLayout` (comctl32 0x4b2a8) gives `height = max(text_cy + 2*edge, HD+0x160)` only when `OpenThemeData("Header")` succeeded; `HD+0x160 =
max(GetThemePartSize(HEADER,1,0,TS_TRUE).cy, GetThemePosition(HEADER,1,0,TMT_MINSIZE 3403).y)` (`_Header_InitTheme` 0x85d98; 01 section 4). The HRESULT of every
`GetThemePartSize` there is **ignored** (01 section 2.3), so once a handle exists parts 1/4/5/7 must answer. The whole block records exactly those inputs, at every DPI, and the
control-level truth (the 24).

Exact priority-0 header rows (from the table; `hdc=0` means a NULL HDC like the real call):

| what | api (class Header) | part (state) | enum / prop | dpi | note |
|---|---|---|---|---|---|
| input A | GetThemePartSize hdc=NULL | HP_HEADERITEM 1 (0) | TS_TRUE | 0, 96, 120, 144, 168, 192 | `HD+0x160` cy part |
| input B | GetThemePosition | HP_HEADERITEM 1 (0) | TMT_MINSIZE 3403 | same | `max(...)` part; Wine returned 0x80070490 (absent), Win10 expected to exist or not: record |
| dropdown width | GetThemePartSize hdc=NULL | HP_HEADERDROPDOWN 5 (0) | TS_TRUE | same | `HD+0x114 = max(.., cx)` |
| sort arrow | GetThemePartSize hdc=NULL | HP_HEADERSORTARROW 4 (0) | TS_TRUE | same | `HD+0x158` |
| overflow | GetThemePartSize hdc=NULL | HP_HEADEROVERFLOW 7 (0) | TS_TRUE | same | `HD+0x144` |
| text height | GetThemeTextExtent text=`M` flags=0x800 | HP_HEADERITEM 1 (0) | font gui / segoe9 / msshell8 / lg | 0 | `Header_NewFont` 0x86174 (themed text arm) |
| result | Ctl header (HDM_LAYOUT cy, themed flag, font tmHeight) | - | font none/gui/segoe9/msshell8/lg x items 3, plus `theme=off`, `theme=explorer` | system | the 24 (expected, per CC30 fixtures) vs the 20 of a theme-less run |
| result | Ctl lvheader (ListView-owned header + row height) | - | font none/segoe9/msshell8, `theme=explorer` | system | header height seen by a ListView |

Priority 1 rows of the same group (all 7 header parts x all states x TS_MIN/TRUE/DRAW, 12 item states of `HP_HEADERITEM` for content rect / extent on three rectangles,
margins `TMT_SIZINGMARGINS 3601` / `CONTENTMARGINS 3602` / `CAPTIONMARGINS 3603` / the undocumented pseudo id 10000, text colours for 12 states, `TMT_IMAGECOUNT`, `TMT_IMAGEFILE`
per state, fonts, `IsThemePartDefined` for every state, `HBG_DETAILS/HBG_ICON` root states, `TS_DRAW` against four destination rectangles, transition durations,
`PropSweep` for (1,0)(1,1)(1,2)(1,4)(1,7)(0,0)(4,1)(5,1)(6,1)(7,1), and `PartGrid` 0..24 x 0..24) are in the CSV (`--grp header`).

## 6. Class / part / state catalogue (all priority tiers)

Every part below is also registered with `IsThemePartDefined` for state 0 and every listed state, so the *shape* of the theme (which parts/states exist) is recorded even
where no value is asked. "P1 lean" parts get: PartSize TS_MIN/TRUE/DRAW (state 0 and every state; DPI variants 120/144/192 for state 0 and the first state), content rect / extent,
the three margin properties, MINSIZE/MINSIZE1 positions, text metrics/extent, 4 ints, 2 metrics, 3 enums, transparency bool, text/fill colours, font, property origin, plus a `PropSweep`
on (part, first state). "P2 min" parts get TS_TRUE, defined, content margins. Every class also gets a class-level (0,0) text-metrics/font/margins/border-size/composited probe and a `PartGrid`.

| class | part (id) | states probed (id) | tier | measurement set |
|---|---|---|---|---|
| Header | HP_HEADERITEM (1) | HIS_NORMAL(1) HIS_HOT(2) HIS_PRESSED(3) HIS_SORTEDNORMAL(4) HIS_SORTEDHOT(5) HIS_SORTEDPRESSED(6) HIS_ICONNORMAL(7) HIS_ICONHOT(8) HIS_ICONPRESSED(9) HIS_ICONSORTEDNORMAL(10) HIS_ICONSORTEDHOT(11) HIS_ICONSORTEDPRESSED(12) | P0/P1 full | all |
| Header | HP_HEADERITEMLEFT (2) | HILS_NORMAL(1) HILS_HOT(2) HILS_PRESSED(3) | P0/P1 full | all |
| Header | HP_HEADERITEMRIGHT (3) | HIRS_NORMAL(1) HIRS_HOT(2) HIRS_PRESSED(3) | P0/P1 full | all |
| Header | HP_HEADERSORTARROW (4) | HSAS_SORTEDUP(1) HSAS_SORTEDDOWN(2) | P0/P1 full | all |
| Header | HP_HEADERDROPDOWN (5) | HDDS_NORMAL(1) HDDS_SOFTHOT(2) HDDS_HOT(3) | P0/P1 full | all |
| Header | HP_HEADERDROPDOWNFILTER (6) | HDDFS_NORMAL(1) HDDFS_SOFTHOT(2) HDDFS_HOT(3) | P0/P1 full | all |
| Header | HP_HEADEROVERFLOW (7) | HOFS_NORMAL(1) HOFS_HOT(2) | P0/P1 full | all |
| Button | BP_PUSHBUTTON (1) | PBS_NORMAL(1) PBS_HOT(2) PBS_PRESSED(3) PBS_DISABLED(4) PBS_DEFAULTED(5) | P1 lean | sizes+margins+props |
| Button | BP_RADIOBUTTON (2) | RBS_UNCHECKEDNORMAL(1) RBS_UNCHECKEDHOT(2) RBS_CHECKEDNORMAL(5) RBS_CHECKEDDISABLED(8) | P1 lean | sizes+margins+props |
| Button | BP_CHECKBOX (3) | CBS_UNCHECKEDNORMAL(1) CBS_UNCHECKEDHOT(2) CBS_CHECKEDNORMAL(5) CBS_MIXEDNORMAL(9) | P1 lean | sizes+margins+props |
| Button | BP_GROUPBOX (4) | GBS_NORMAL(1) GBS_DISABLED(2) | P2 min | TS_TRUE+defined+content margins |
| Button | BP_USERBUTTON (5) | - | P2 min | TS_TRUE+defined+content margins |
| Button | BP_COMMANDLINK (6) | CMDLS_NORMAL(1) CMDLS_HOT(2) CMDLS_PRESSED(3) CMDLS_DISABLED(4) | P1 lean | sizes+margins+props |
| Button | BP_COMMANDLINKGLYPH (7) | CMDLGS_NORMAL(1) CMDLGS_HOT(2) CMDLGS_PRESSED(3) CMDLGS_DISABLED(4) | P1 lean | sizes+margins+props |
| Tab | TABP_TABITEM (1) | TIS_NORMAL(1) TIS_HOT(2) TIS_SELECTED(3) TIS_DISABLED(4) TIS_FOCUSED(5) | P1 lean | sizes+margins+props |
| Tab | TABP_TABITEMLEFTEDGE (2) | TILES_NORMAL(1) TILES_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_TABITEMRIGHTEDGE (3) | TIRES_NORMAL(1) TIRES_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_TABITEMBOTHEDGE (4) | TIBES_NORMAL(1) TIBES_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_TOPTABITEM (5) | TTIS_NORMAL(1) TTIS_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_TOPTABITEMLEFTEDGE (6) | TTILES_NORMAL(1) TTILES_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_TOPTABITEMRIGHTEDGE (7) | TTIRES_NORMAL(1) TTIRES_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_TOPTABITEMBOTHEDGE (8) | TTIBES_NORMAL(1) TTIBES_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_PANE (9) | - | P1 lean | sizes+margins+props |
| Tab | TABP_BODY (10) | - | P1 lean | sizes+margins+props |
| ListView | LVP_LISTITEM (1) | LISS_NORMAL(1) LISS_HOT(2) LISS_SELECTED(3) LISS_DISABLED(4) LISS_SELECTEDNOTFOCUS(5) LISS_HOTSELECTED(6) | P1 lean | sizes+margins+props |
| ListView | LVP_LISTGROUP (2) | - | P2 min | TS_TRUE+defined+content margins |
| ListView | LVP_LISTDETAIL (3) | - | P2 min | TS_TRUE+defined+content margins |
| ListView | LVP_LISTSORTEDDETAIL (4) | - | P2 min | TS_TRUE+defined+content margins |
| ListView | LVP_EMPTYTEXT (5) | - | P2 min | TS_TRUE+defined+content margins |
| ListView | LVP_GROUPHEADER (6) | - | P1 lean | sizes+margins+props |
| ListView | LVP_GROUPHEADERLINE (7) | - | P1 lean | sizes+margins+props |
| ListView | LVP_EXPANDBUTTON (8) | - | P2 min | TS_TRUE+defined+content margins |
| ListView | LVP_COLLAPSEBUTTON (9) | - | P1 lean | sizes+margins+props |
| ListView | LVP_COLUMNDETAIL (10) | - | P1 lean | sizes+margins+props |
| TreeView | TVP_TREEITEM (1) | TREIS_NORMAL(1) TREIS_HOT(2) TREIS_SELECTED(3) TREIS_DISABLED(4) TREIS_SELECTEDNOTFOCUS(5) TREIS_HOTSELECTED(6) | P1 lean | sizes+margins+props |
| TreeView | TVP_GLYPH (2) | GLPS_CLOSED(1) GLPS_OPENED(2) | P1 lean | sizes+margins+props |
| TreeView | TVP_BRANCH (3) | - | P1 lean | sizes+margins+props |
| TreeView | TVP_HOTGLYPH (4) | HGLPS_CLOSED(1) HGLPS_OPENED(2) | P1 lean | sizes+margins+props |
| Toolbar | TP_BUTTON (1) | TS_NORMAL(1) TS_HOT(2) TS_PRESSED(3) TS_DISABLED(4) TS_CHECKED(5) TS_HOTCHECKED(6) | P1 lean | sizes+margins+props |
| Toolbar | TP_DROPDOWNBUTTON (2) | TS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Toolbar | TP_SPLITBUTTON (3) | TS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Toolbar | TP_SPLITBUTTONDROPDOWN (4) | TS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Toolbar | TP_SEPARATOR (5) | TS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Toolbar | TP_SEPARATORVERT (6) | TS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Toolbar | TP_DROPDOWNBUTTONGLYPH (7) | TS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Rebar | RP_GRIPPER (1) | - | P1 lean | sizes+margins+props |
| Rebar | RP_GRIPPERVERT (2) | - | P1 lean | sizes+margins+props |
| Rebar | RP_BAND (3) | - | P1 lean | sizes+margins+props |
| Rebar | RP_CHEVRON (4) | CHEVS_NORMAL(1) CHEVS_HOT(2) CHEVS_PRESSED(3) | P2 min | TS_TRUE+defined+content margins |
| Rebar | RP_CHEVRONVERT (5) | CHEVSV_NORMAL(1) CHEVSV_HOT(2) CHEVSV_PRESSED(3) | P2 min | TS_TRUE+defined+content margins |
| Rebar | RP_BACKGROUND (6) | - | P1 lean | sizes+margins+props |
| Rebar | RP_SPLITTER (7) | - | P1 lean | sizes+margins+props |
| Rebar | RP_SPLITTERVERT (8) | - | P1 lean | sizes+margins+props |
| Progress | PP_BAR (1) | - | P1 lean | sizes+margins+props |
| Progress | PP_BARVERT (2) | - | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_CHUNK (3) | - | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_CHUNKVERT (4) | - | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_FILL (5) | PBFS_NORMAL(1) PBFS_ERROR(2) PBFS_PAUSED(3) PBFS_PARTIAL(4) | P1 lean | sizes+margins+props |
| Progress | PP_FILLVERT (6) | PBFVS_NORMAL(1) PBFVS_ERROR(2) PBFVS_PAUSED(3) PBFVS_PARTIAL(4) | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_PULSEOVERLAY (7) | - | P1 lean | sizes+margins+props |
| Progress | PP_MOVEOVERLAY (8) | - | P1 lean | sizes+margins+props |
| Progress | PP_PULSEOVERLAYVERT (9) | - | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_MOVEOVERLAYVERT (10) | - | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_TRANSPARENTBAR (11) | PBBS_NORMAL(1) PBBS_PARTIAL(2) | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_TRANSPARENTBARVERT (12) | PBBVS_NORMAL(1) PBBVS_PARTIAL(2) | P2 min | TS_TRUE+defined+content margins |
| TrackBar | TKP_TRACK (1) | TRS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| TrackBar | TKP_TRACKVERT (2) | TRVS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| TrackBar | TKP_THUMB (3) | TUS_NORMAL(1) TUS_HOT(2) TUS_PRESSED(3) TUS_FOCUSED(4) TUS_DISABLED(5) | P1 lean | sizes+margins+props |
| TrackBar | TKP_THUMBBOTTOM (4) | TUBS_NORMAL(1) TUBS_HOT(2) | P1 lean | sizes+margins+props |
| TrackBar | TKP_THUMBTOP (5) | TUTS_NORMAL(1) TUTS_HOT(2) | P1 lean | sizes+margins+props |
| TrackBar | TKP_THUMBVERT (6) | TUVS_NORMAL(1) TUVS_HOT(2) | P1 lean | sizes+margins+props |
| TrackBar | TKP_THUMBLEFT (7) | TUVLS_NORMAL(1) TUVLS_HOT(2) | P1 lean | sizes+margins+props |
| TrackBar | TKP_THUMBRIGHT (8) | TUVRS_NORMAL(1) TUVRS_HOT(2) | P1 lean | sizes+margins+props |
| TrackBar | TKP_TICS (9) | TSS_NORMAL(1) | P1 lean | sizes+margins+props |
| TrackBar | TKP_TICSVERT (10) | TSVS_NORMAL(1) | P1 lean | sizes+margins+props |
| Spin | SPNP_UP (1) | UPS_NORMAL(1) UPS_HOT(2) UPS_PRESSED(3) UPS_DISABLED(4) | P1 lean | sizes+margins+props |
| Spin | SPNP_DOWN (2) | DNS_NORMAL(1) DNS_HOT(2) DNS_PRESSED(3) DNS_DISABLED(4) | P1 lean | sizes+margins+props |
| Spin | SPNP_UPHORZ (3) | UPHZS_NORMAL(1) UPHZS_HOT(2) | P1 lean | sizes+margins+props |
| Spin | SPNP_DOWNHORZ (4) | DNHZS_NORMAL(1) DNHZS_HOT(2) | P1 lean | sizes+margins+props |
| Edit | EP_EDITTEXT (1) | ETS_NORMAL(1) ETS_HOT(2) ETS_SELECTED(3) ETS_DISABLED(4) ETS_FOCUSED(5) ETS_READONLY(6) ETS_ASSIST(7) ETS_CUEBANNER(8) | P1 lean | sizes+margins+props |
| Edit | EP_CARET (2) | - | P2 min | TS_TRUE+defined+content margins |
| Edit | EP_BACKGROUND (3) | - | P1 lean | sizes+margins+props |
| Edit | EP_PASSWORD (4) | - | P1 lean | sizes+margins+props |
| Edit | EP_BACKGROUNDWITHBORDER (5) | EBWBS_NORMAL(1) EBWBS_HOT(2) EBWBS_DISABLED(3) EBWBS_FOCUSED(4) | P1 lean | sizes+margins+props |
| Edit | EP_EDITBORDER_NOSCROLL (6) | EPSN_NORMAL(1) EPSN_HOT(2) EPSN_FOCUSED(3) EPSN_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Edit | EP_EDITBORDER_HSCROLL (7) | EPSH_NORMAL(1) EPSH_HOT(2) EPSH_FOCUSED(3) EPSH_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Edit | EP_EDITBORDER_VSCROLL (8) | EPSV_NORMAL(1) EPSV_HOT(2) EPSV_FOCUSED(3) EPSV_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Edit | EP_EDITBORDER_HVSCROLL (9) | EPSHV_NORMAL(1) EPSHV_HOT(2) EPSHV_FOCUSED(3) EPSHV_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_DROPDOWNBUTTON (1) | CBXS_NORMAL(1) CBXS_HOT(2) CBXS_PRESSED(3) CBXS_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_BACKGROUND (2) | - | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_TRANSPARENTBACKGROUND (3) | - | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_BORDER (4) | CBB_NORMAL(1) CBB_HOT(2) CBB_FOCUSED(3) CBB_DISABLED(4) | P1 lean | sizes+margins+props |
| Combobox | CP_READONLY (5) | CBRO_NORMAL(1) CBRO_HOT(2) CBRO_PRESSED(3) CBRO_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_DROPDOWNBUTTONRIGHT (6) | CBXSR_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_DROPDOWNBUTTONLEFT (7) | CBXSL_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_CUEBANNER (8) | CBCB_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Scrollbar | SBP_ARROWBTN (1) | ABS_UPNORMAL(1) ABS_UPHOT(2) ABS_UPPRESSED(3) ABS_UPDISABLED(4) ABS_DOWNNORMAL(5) ABS_LEFTNORMAL(9) ABS_RIGHTNORMAL(13) | P1 lean | sizes+margins+props |
| Scrollbar | SBP_THUMBBTNHORZ (2) | SCRBS_NORMAL(1) SCRBS_HOT(2) SCRBS_PRESSED(3) SCRBS_DISABLED(4) | P1 lean | sizes+margins+props |
| Scrollbar | SBP_THUMBBTNVERT (3) | SCRBS_NORMAL(1) SCRBS_HOT(2) SCRBS_PRESSED(3) SCRBS_DISABLED(4) | P1 lean | sizes+margins+props |
| Scrollbar | SBP_LOWERTRACKHORZ (4) | SCRBS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Scrollbar | SBP_UPPERTRACKHORZ (5) | SCRBS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Scrollbar | SBP_LOWERTRACKVERT (6) | SCRBS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Scrollbar | SBP_UPPERTRACKVERT (7) | SCRBS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Scrollbar | SBP_GRIPPERHORZ (8) | SCRBS_NORMAL(1) | P1 lean | sizes+margins+props |
| Scrollbar | SBP_GRIPPERVERT (9) | SCRBS_NORMAL(1) | P1 lean | sizes+margins+props |
| Scrollbar | SBP_SIZEBOX (10) | SZB_RIGHTALIGN(1) SZB_LEFTALIGN(2) | P1 lean | sizes+margins+props |
| Status | SP_PANE (1) | - | P1 lean | sizes+margins+props |
| Status | SP_GRIPPERPANE (2) | - | P1 lean | sizes+margins+props |
| Status | SP_GRIPPER (3) | - | P1 lean | sizes+margins+props |
| Tooltip | TTP_STANDARD (1) | TTSS_NORMAL(1) TTSS_LINK(2) | P1 lean | sizes+margins+props |
| Tooltip | TTP_STANDARDTITLE (2) | TTSS_NORMAL(1) TTSS_LINK(2) | P2 min | TS_TRUE+defined+content margins |
| Tooltip | TTP_BALLOON (3) | TTBS_NORMAL(1) TTBS_LINK(2) | P1 lean | sizes+margins+props |
| Tooltip | TTP_BALLOONTITLE (4) | TTBS_NORMAL(1) TTBS_LINK(2) | P2 min | TS_TRUE+defined+content margins |
| Tooltip | TTP_CLOSE (5) | TTCS_NORMAL(1) TTCS_HOT(2) TTCS_PRESSED(3) | P1 lean | sizes+margins+props |
| Tooltip | TTP_BALLOONSTEM (6) | TTBSS_POINTINGUPLEFTWALL(1) TTBSS_POINTINGUPCENTERED(2) TTBSS_POINTINGUPRIGHTWALL(3) | P1 lean | sizes+margins+props |
| Tooltip | TTP_WRENCH (7) | TTWS_NORMAL(1) TTWS_HOT(2) TTWS_PRESSED(3) | P1 lean | sizes+margins+props |
| Window | WP_CAPTION (1) | CS_ACTIVE(1) CS_INACTIVE(2) CS_DISABLED(3) | P1 lean | sizes+margins+props |
| Window | WP_SMALLCAPTION (2) | CS_ACTIVE(1) CS_INACTIVE(2) | P1 lean | sizes+margins+props |
| Window | WP_MINCAPTION (3) | MXCS_ACTIVE(1) | P1 lean | sizes+margins+props |
| Window | WP_MAXCAPTION (5) | MXCS_ACTIVE(1) | P1 lean | sizes+margins+props |
| Window | WP_FRAMELEFT (7) | FS_ACTIVE(1) FS_INACTIVE(2) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_FRAMEBOTTOM (9) | FS_ACTIVE(1) FS_INACTIVE(2) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_SYSBUTTON (13) | SBS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_MINBUTTON (15) | MINBS_NORMAL(1) MINBS_HOT(2) MINBS_PUSHED(3) MINBS_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_MAXBUTTON (17) | MAXBS_NORMAL(1) MAXBS_HOT(2) MAXBS_PUSHED(3) MAXBS_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_CLOSEBUTTON (18) | CBS_NORMAL(1) CBS_HOT(2) CBS_PUSHED(3) CBS_DISABLED(4) | P1 lean | sizes+margins+props |
| Window | WP_SMALLCLOSEBUTTON (19) | CBS_NORMAL(1) CBS_HOT(2) CBS_PUSHED(3) CBS_DISABLED(4) | P1 lean | sizes+margins+props |
| Window | WP_RESTOREBUTTON (21) | RBS_NORMAL(1) RBS_HOT(2) RBS_PUSHED(3) RBS_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_HELPBUTTON (23) | HBS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_HORZSCROLL (25) | HSS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_VERTSCROLL (27) | VSS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_DIALOG (29) | - | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_BARBACKGROUND (7) | MB_ACTIVE(1) MB_INACTIVE(2) | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_BARITEM (8) | MBI_NORMAL(1) MBI_HOT(2) MBI_PUSHED(3) MBI_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPBACKGROUND (9) | - | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPBORDERS (10) | - | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPCHECK (11) | MC_CHECKMARKNORMAL(1) MC_CHECKMARKDISABLED(2) MC_BULLETNORMAL(3) | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPCHECKBACKGROUND (12) | MCB_DISABLED(1) MCB_NORMAL(2) MCB_BITMAP(3) | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPGUTTER (13) | - | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPITEM (14) | MPI_NORMAL(1) MPI_HOT(2) MPI_DISABLED(3) MPI_DISABLEDHOT(4) | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPSEPARATOR (15) | - | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPSUBMENU (16) | MSM_NORMAL(1) MSM_DISABLED(2) | P2 min | TS_TRUE+defined+content margins |
| Listbox | LBCP_BORDER_HSCROLL (1) | LBPSH_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Listbox | LBCP_BORDER_HVSCROLL (2) | LBPSHV_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Listbox | LBCP_BORDER_NOSCROLL (3) | LBPSN_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Listbox | LBCP_BORDER_VSCROLL (4) | LBPSV_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Listbox | LBCP_ITEM (5) | LBPSI_HOT(1) | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_BACKGROUND (1) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_BORDERS (2) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_GRIDBACKGROUND (3) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_COLHEADERSPLITTER (4) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_GRIDCELLBACKGROUND (5) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_GRIDCELL (6) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_GRIDCELLUPPER (7) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_TRAILINGGRIDCELL (8) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_TRAILINGGRIDCELLUPPER (9) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_NAVNEXT (10) | MCNN_NORMAL(1) | P1 lean | sizes+margins+props |
| MonthCal | MC_NAVPREV (11) | MCNP_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| DatePicker | DP_DATETEXT (1) | DPDT_NORMAL(1) | P1 lean | sizes+margins+props |
| DatePicker | DP_DATEBORDER (2) | DPDB_NORMAL(1) | P1 lean | sizes+margins+props |
| DatePicker | DP_SHOWCALENDARBUTTONRIGHT (3) | DPSCBR_NORMAL(1) | P1 lean | sizes+margins+props |
| Static | (class level) (0) | - | P1 lean | sizes+margins+props |
| Link | LP_HYPERLINK (1) | HLS_NORMALTEXT(1) HLS_LINKTEXT(2) | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_MAININSTRUCTION (1) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_INSTRUCTION (2) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_BODYTITLE (3) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_BODYTEXT (4) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_SECONDARYTEXT (5) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_HYPERLINKTEXT (6) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_EXPANDED (7) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_LABEL (8) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_CONTROLLABEL (9) | - | P2 min | TS_TRUE+defined+content margins |
| Globals | (class level) (0) | - | P2 min | TS_TRUE+defined+content margins |


Targeted extras (survey 01 section 7, all in the CSV): ComboBox `GetThemeInt(4,1,BORDERSIZE)` and (0,0); Edit `GetThemeMargins(5,1,3602)`, cue banner font/colour (1,8),
`EP_PASSWORD` `GLYPHFONT`/`GLYPHINDEX`; Toolbar `NOETCHEDEFFECT` (1,4), text colour (0,0), `SIZINGMARGINS` (0,0); Progress `FRAMESPERSECOND`/`PIXELSPERFRAME`/`ANIMATIONDELAY`/`ANIMATIONDURATION` (0,0) for
`Progress` and `Indeterminate::Progress`, `HALIGN`/`VALIGN` of `PP_PULSEOVERLAY`; TreeView `TMT_COLOR 204` of the branch, `FILLCOLOR` (0,0), item font; TrackBar tics colour (9,1)/(10,1);
ListView group fonts (props 808/809), `GetThemeMargins` (1,3,3602) and (6,0,3602), `GetThemePartSize` (7,1,TS_TRUE) and (9,1,TS_DRAW), the Explorer / DarkMode_Explorer sub-app list-item look for states 0..6,
transition durations; Window (non-client, `nc=1`) `TMT_SIZINGBORDERWIDTH` and caption margins of parts 1/2/3/5, `WP_CLOSEBUTTON`/`WP_SMALLCLOSEBUTTON` sizes; Button command-link text metrics, `COMPOSITED`, check box / radio
`TS_DRAW` for states 1..12 (`GetCheckBoxSize`, `CreateCheckBoxImagelistEx`), dialog-button class lists; Status `SP_GRIPPER` `TS_DRAW`; Tooltip close / wrench `TS_DRAW`, balloon stem `TS_TRUE` x3 states, background regions;
ScrollBar transition durations.

## 7. System values (group `sys`, 02 section 7)

For each of three handle kinds - `hTheme = NULL` (`class -`), a `Window` theme, a `Globals` theme: `GetThemeSysColor` ids 0..30, `GetThemeSysSize` for the eleven supported SM ids (2,3,30,31,32,33,52,53,54,55,92) and four unsupported ones (0,4,45,46), at DPI 0/96/120/144/192 for the
real handles, `GetThemeSysBool` (`TMT_FLATMENUS 1001` and an unrelated id), `GetThemeSysInt` (`TMT_MINCOLORDEPTH 1301`, a wrong-typed id, a bad id), `GetThemeSysFont` ids 800..810 (valid 801..809), `GetThemeSysString` ids 1400..1405 (valid 1401..1404),
`GetThemeSysColorBrush` COLOR_WINDOW (brush colour read through `GetObject`, brush deleted when the theme owns it). Environment rows (`P00006` SysMetrics: `GetSystemMetrics` 0..99 and `GetSystemMetricsForDpi` for 38 scaling metrics at 5 DPIs;
`NonClientMetrics` at the system DPI and via `SystemParametersInfoForDpi`; `Fonts`; `SysColors`) give the classic baseline each themed value is compared against.

## 8. Handle / error / lifetime semantics (group `open`, `err`)

These test the facts of 02 that were "stated from decomp, not confirmed on Windows" (02 section 15). Each row says what 02 predicts; **the prediction is a hypothesis, the recording decides**.

| selector (api / class / aux) | 02 prediction to confirm or refute |
|---|---|
| `Env ThemeState` | IsThemeActive TRUE (LE 0), IsAppThemed TRUE, GetThemeAppProperties **3** (the Industrial7 crate uses 7), GetCurrentThemeName fills the three names |
| `OpenTheme @NULL` | NULL, LastError **0x80004003** (E_POINTER) |
| `OpenTheme @EMPTY` | NULL, LastError **0x80070057** |
| `OpenTheme NoSuchClass`, `NoSuch;AlsoNoSuch` | NULL, LastError **0x80070490**; `NoSuch;Button` succeeds (list falls through) |
| `OpenTheme Header` hwnd=bad | NULL, LastError **0x80070006** |
| `OpenTheme Header;Button`, `header`, `HEADER` | first match wins; case-insensitive |
| `OpenTheme Button-OK;Button` ... 12 dialog classes | all resolve (01 section 2.2) |
| `OpenTheme Header::Foo` | `App::Class` form: app `Header`, class `Foo` -> expected NULL / 0x80070490 |
| `OpenTheme` with `(empty)` / `$` subapp | `SetWindowTheme(h,L"",NULL)`; `ids=` empty -> class `$` -> NULL (de-themed window) |
| `OpenTheme` `twice=1` | same HTHEME value on the second open at one DPI (cache), each needs its own close |
| `GetThemePartSize @NULLH/@BOGUS/@STALE` | **E_HANDLE** (0x80070006); with `noout=1` handle check first (E_HANDLE) whereas the property getters (`GetThemeColor`, `GetThemeInt`, ...) check the **output pointer first (E_POINTER)** |
| `GetThemeX Header` `noout=1` | `E_POINTER` 0x80004003 |
| `GetThemeX Header` part 99 state 99 | default-fallback rule: succeeds with the class defaults or 0x80070490 |
| `CloseThemeData @NULLH`, `@BOGUS` | E_HANDLE; `Header` close1 S_OK, close2 E_HANDLE |
| `SetThemeAppProperties` 0..7 | `0` -> every open fails 0x80070490 while `IsThemeActive` stays TRUE; `STAP_ALLOW_NONCLIENT` (1) / `STAP_ALLOW_CONTROLS` (2) gate `nc` vs plain opens |
| `SetWindowTheme sub= ids=` | WM_THEMECHANGED, then `OpenHeader` NULL; `sub=Explorer`; `hwnd=0` -> E_HANDLE |
| class list in `OPEN_CASES` marked "speculative" in the generator | **class discovery** (names the survey did not prove): `ItemsView CommandModule Navigation BreadcrumbBar SearchBox Taskband StartPanel TaskDialog ControlPanel Clock TrayNotify Flyout ExplorerBar Page` - an open that succeeds adds a class to the armed table's directory |

## 9. Control-level truth (group `ctl`) and how results feed the constants table

`Ctl` rows create the real comctl32 control inside a hidden window of the recorder and report the numbers a consumer ends up with (so the armed personality can be checked end to end
without trusting any single getter): status bar height + `SB_GETBORDERS`, tab `TCM_GETITEMRECT` + `TCM_ADJUSTRECT`, toolbar `TB_GETBUTTONSIZE`/`TB_GETPADDING`/height, combo `CB_GETITEMHEIGHT`/`rcButton`, edit `EM_GETRECT`, button
`BCM_GETIDEALSIZE`/`BCM_GETTEXTMARGIN` (push, check, radio, group), trackbar thumb + channel rect (horizontal, vertical, no ticks), updown, listbox item height, header and ListView header, each for
three fonts. Combined with `--comctl v5` (pass 3) they separate "comctl32 version" from "theme".

How a result line becomes a constants-table row (design input for 07):

| table field (armed personality) | recorded by |
|---|---|
| HEADER min height (`HD+0x160`) | `max(P(1,0,TS_TRUE).cy, MINSIZE.y)` from the two input-A/B rows; verify against `Ctl header` cy |
| per-(class,part,state) size TS_MIN/TRUE/DRAW | `GetThemePartSize` rows (+`dpi` variants, DRAW rows depend on the `rect=` in `aux`) |
| content margins / sizing margins / caption margins | `GetThemeMargins` rows, prop 3602/3601/3603 |
| min size / default pane size | `GetThemePosition` MINSIZE..MINSIZE5 / `GetThemeRect` |
| border size, image count, glyph index, animation values | `GetThemeInt`/`Metric` rows + `PropSweep` hits |
| text colours / fill colours / border colours | `GetThemeColor` rows (hex in `txt` is 0xBBGGRR) |
| fonts | `GetThemeFont`, `GetThemeSysFont` (LOGFONT height is DPI dependent: record at C1/C2) |
| exists/does-not-exist | `IsThemePartDefined`, `PartGrid` hit rows, `GetThemePropertyOrigin` |
| class directory | `OpenTheme` rows (`OPENFAIL` + `lasterr`) |
| sys colours / sizes / fonts | `Sys*` rows with the `-` handle (classic) vs `Window` handle (themed) |
| transition / animation | `GetThemeTransitionDuration` rows |

## 10. Wine validation of the plumbing (this host, not an oracle)

Hosts: Proton GE (Wine 11.0 build, `~/work/wine/proton-ge-custom`, reports Windows 10.0.19045) and system `wine` 9.0 (10.0.19043). Both ran **headless in a private Xvfb**
(`tools/wine_probe.sh`: throw-away copy of the base prefix, unique display number picked free in `/tmp/.X11-unix`, wineserver + Xvfb + prefix removed on exit; the Xvfb binary came from a user-space
`apt-get download` unpack because none is installed system-wide, selected with `XVFB=`). `ss -x | grep -c X11-unix` was 43 before and after every run, no `Xvfb`/`wineserver`/`probe_prefix` left behind; the live X session was never touched.

```
make -f tools/Makefile.probe                       # 0 errors, 0 warnings
XVFB=.../Xvfb tools/wine_probe.sh -o build/r_p2.csv -- --prio 2 --label wine-proton-full --timeout 20          # Proton:  24 s
TP_BASE_PREFIX=~/.cache/test_pattern/prefix_wine9 WINE=/usr/bin/wine tools/wine_probe.sh -o build/r_p2_wine9.csv -- --prio 2 --timeout 20   # wine 9: 24 s
tools/wine_probe.sh -o build/r_det_a.csv -- --prio 0 ; (same -> r_det_b.csv) ; python3 tools/probe_compare.py r_det_a r_det_b   # "509 keys, 0 differ"
```

Committed outputs: `docs/research/06_wine_run/{proton11,wine9}_prio0.csv` (the 54 priority-0 table rows incl. environment) and `*_prio2.csv.gz` (full default run, 12001 lines each).

What the run proved (plumbing, all [V] by the output files):

* Table: 7177 rows parsed; under `--prio 2` all 7135 rows with priority <= 2 produced at least one result line (checked by id; the 42 priority-3 rows are skipped, `gridfull` needs `--sweep full`). The final `RUN rows_run` line counts only the last child process, so it is smaller than the total.
* **Activation context works**: `comctl32.dll` was loaded from the WinSxS v6 location (`...common-controls_6595b64144ccf1df_6.0.2600.2982...`) with `exe.comctl = requested=v6 actctx=active` (Proton), and `SysHeader32`/`SysListView32`/... were created (`Ctl` rows `0x00000000`; 0 `CREATEFAIL`).
* Result types behave as specified: sentinel `-7777` for unwritten outputs (e.g. `GetThemePosition(Header,1,0,TMT_MINSIZE)` -> `0x80070490`, `-7777,-7777`), `lasterr` stays `0x5A5A5A5A` for calls that do not touch LastError, colours as `0xBBGGRR`, LOGFONT rows with face names, `PropSweep` hit rows (Header HP_HEADERITEM state 0: `2201 Bool=1`, `2401 Int=12`, `3001 Filename=blue_header_item.bmp`, `3601 Margins=2,3,9,14`, `3602 Margins=0,0,0,0`, `4001...`), `PartGrid` (`0:0 3x24`, `1:0 6x24`, `4:0 26x9` ... defined cells with MIN/TRUE/DRAW).
* **Robustness**: 31 rows killed the child (30 x access violation `0xC0000005` for NULL output pointers / handles that Wine does not validate - on Windows these are expected to be `E_POINTER`/`E_HANDLE` rows - and 1 x `nc=1` abort because Wine's uxtheme ordinal 49 is an unimplemented stub: `wine: Call ... to unimplemented function uxtheme.dll.49`, which made the supervisor skip all 462 `nc=1` rows with `SKIPPED`), and 1 row hung (`GetThemeTextMetrics` with a NULL HTHEME and NULL output right after a stale-handle call: `HANG` after the 20 s watchdog). The run always completed ("exit=0 ... done") and later rows were unaffected. A first version using `ExitProcess` in the crash filter deadlocked inside Wine; it now uses `TerminateProcess` (kept as a comment in the source).
* 385 `OPENFAIL` rows are classes Wine's built-in theme does not define (`DatePicker`, `MonthCal`, `TextStyle`, `Link`, `Static`): a real use of the "class directory" probes.
* Determinism: two identical priority-0 runs -> `0 differ` (the `RUN.*` meta rows are ignored by `probe_compare.py`).
* Proton vs wine 9 (`probe_compare.py`): 76 of 11964 keys differ - OS/file versions (21), DPI awareness value, text extent/metric rows that depend on the fallback font, the two system colours 16 and 26 that 03 section 4.2 already found, `Ctl header font=lg` 30 vs 31, 4 ListView enum rows. Proof that the diff tool sees real differences.

Excerpt of the real output (`docs/research/06_wine_run/proton11_prio0.csv`, Proton/Wine 11, 96 dpi; columns after the id: `hr,lasterr,ret,v0..v5,txt`):

```
P00950 GetThemePartSize  Header part=1 state=0 TS_TRUE hdc=0 -> 0x00000000,0x5A5A5A5A,0,6,24
P00951 GetThemePosition  Header part=1 state=0 TMT_MINSIZE   -> 0x80070490,0x5A5A5A5A,0,-7777,-7777
P00952 GetThemePartSize  Header part=5 state=0 TS_TRUE hdc=0 -> 0x00000000,0x5A5A5A5A,0,3,24
P00953 GetThemePartSize  Header part=4 state=0 TS_TRUE hdc=0 -> 0x00000000,0x5A5A5A5A,0,26,9
P00954 GetThemePartSize  Header part=7 state=0 TS_TRUE hdc=0 -> 0x00000000,0x5A5A5A5A,0,8,21
P00985 GetThemeTextExtent Header 1/0 "M" 0x800 font=segoe9   -> 0x00000000,0x5A5A5A5A,0,0,0,12,14        (l,t,r,b)
P01185 Ctl header font=none;items=3                           -> 0x00000000,0x00000000,0,20,1,16,3,0,400  (cy=20 themed=1 tmHeight=16 items=3 wp.y=0 wp.cx=400)
P01193 Ctl header font=segoe9;items=3                         -> 0x00000000,0x00000000,0,18,1,14,3,0,400
```

What Wine says about the questions (**NOT oracle data**, labelled Wine, for orientation only; the same numbers are in 03/seed_constants): HP_HEADERITEM TS_TRUE = 6 x 24 at every DPI (Wine does not scale: rows `dpi=96..192` are identical), `TMT_MINSIZE` absent, dropdown 3 x 24, sort arrow 26 x 9 (state 0), overflow 8 x 21;
`Ctl header`: HDM_LAYOUT cy = 20 (font none, tmHeight 16), 17 (gui / MS Shell Dlg 8 pt, tmHeight 13), 18 (Segoe-UI-9pt request, falls back to a 14-px face), 30/31 (18 pt), with `GetWindowTheme(header) != NULL` (`v1=1`) in every themed row, and **unchanged** by `theme=off` / `theme=explorer` and by `HDS_FILTERBAR` (Wine ignores both) - the opposite of Win10, where 02/01 predict a themed minimum of 24 and a changed height after de-theming. These are exactly the rows on which the oracle is expected to differ.

## 11. Not covered / follow-ups / known limits

* **No Win10 data exists yet.** Everything here is untested on Windows: the recorder is plain C against documented APIs, but its first run on Win10 may show compile-time-valid yet runtime-surprising behaviour (e.g. `NOAPI`/`OPENFAIL` rows); the harness is built so these degrade into rows, not aborts.
* **Out of scope here** (not theme *measurement* values): drawing output (`DrawThemeBackground*`, `DrawThemeText*`, `DrawThemeEdge`, pixels), `GetThemeBitmap`/`GetThemeStream`, `HitTestThemeBackground`, buffered paint / animation
  (`GetThemeAnimation*`, `GetThemeTimingFunction`), `GetThemeDocumentationProperty`, `EnumTheme*`, `OpenThemeFile`; the immersive-colour / dark-mode ordinals (94-139) other than the two the recorder calls to enter dark mode. A pixel probe (render a
  part into a DIB and hash/dump it) is the natural next stage if sizes alone do not pin the armed look; `DrawThemeBackground` on a memory DC with a fixed rect would give it with the same harness.
* Property sweep range is ids 1..8100 (the SDK's largest `TMT_*` is 8002); undocumented ids above that are not probed. `PropSweep` is gated by `GetThemePropertyOrigin`: a property whose origin lookup fails but whose typed getter would succeed is missed.
* Class names marked speculative in the generator are guesses to discover classes; a failing open costs one `OPENFAIL` row.
* `Ctl` windows are created hidden (`--show` shows the parent window; a few control metrics may differ for visible windows). Controls are created at the system DPI; per-monitor-DPI windows are not exercised.
* `hdc` DPI vs object DPI mixing is described in 3.3. Fonts `segoe9`/`msshell8`/`lg` are specified in points and therefore scale with the system DPI by design.
* `GetThemeSysColorBrush` for a real handle leaks if the caller never deletes it (02 section 7): the recorder deletes it.
* The unverified-name list of the generator is empty: every part/state/TMT symbol used was found in the SDK headers with the stated number; ids with no SDK name are labelled in `propname` (`pseudo-10000`, `prop808` (809 is `TMT_BODYFONT`), `(other id)`, `(bad id)`) and in the `note` column.
