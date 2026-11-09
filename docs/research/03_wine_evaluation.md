# Wine as a reference / oracle for uxtheme (role: WINE EVALUATOR)

Scope: is Wine usable as an oracle for Windows 10 x64 `uxtheme.dll` 10.0.19041.2193 (and the comctl32 6.10.19041.1110
consumer behaviour that depends on it)? Everything below is labelled **VERIFIED** (command output / decomp / source
line I read), **INFERENCE** (my reasoning from verified facts), or **UNKNOWN**.

## 0. Bottom line

1. Wine is **not a Win10 oracle**. It is a useful *plumbing and sanity oracle*: it is the only thing on this host that
   exercises the full pipeline `comctl32 -> OpenThemeData -> GetThemePartSize -> msstyles` end to end headlessly, and its
   own test-suite encodes a number of real-Windows behaviours (return codes, last-error values, DPI behaviour) that were
   obtained on real Windows by Wine developers.
2. Wine's data source is **its own authored theme** (`light.msstyles` in Proton/Ubuntu, `aero.msstyles` in Wine >= 11.18
   tree), not Microsoft's `aero.msstyles`. Part sizes it returns are properties of that theme's images, not Win10 constants.
   Some match Win10 (HP_HEADERITEM height 24, BP_CHECKBOX 13x13 @96dpi), but none of them is *verified* against Win10 here.
3. The headline CC30 symptom (header 24 px on Win10 vs 20 px in Industrial7) **cannot be reproduced or explained by Wine's
   comctl32**: Wine's header height ignores the theme entirely (font height + 4). Measured 20 px (system font) / 17 px
   (DEFAULT_GUI_FONT) in every combination of theme on/off and comctl32 v5/v6 (section 4). Wine's *uxtheme* does return
   `GetThemePartSize(HP_HEADERITEM) = 6x24`, which is exactly the number Win10's `_Header_InitTheme` would consume.
4. Wine diverges from Win10 in `GetThemePartSize` semantics (TS_MIN, BorderFill TS_DRAW, NULL `psz`), in handle encoding,
   in High-Contrast handling, and in `IsAppThemed`. Real Windows 10 recordings (the lane's armed-Win10 probes) are
   mandatory for any constant that goes into the recorded-constants table.
5. "Themed under Wine" is possible headlessly and is the **default** in a fresh prefix (HKCU ThemeManager is pre-seeded by
   `wine.inf`). Switching a prefix to "themeless" is one `reg add`. Loading a *real* Windows 10 `aero.msstyles` into Wine is
   very probably not possible (loader accepts only the XP-era text-INI format, `PACKTHEM_VERSION == 3`; Win10's file format
   is UNKNOWN to me, and no Win10 msstyles is available on this host or in the corpus API).

## 1. Environment facts (VERIFIED)

| Item | Value | Evidence |
|---|---|---|
| System wine | `wine-9.0 (Ubuntu 9.0~repack-4build3)` | `wine --version` |
| Proton wine | `wine-11.0 (Staging)`, binary `~/work/wine/proton-ge-custom/build/dist/files/bin/wine`, source `proton-ge-custom/wine` VERSION `Wine version 11.0` | `wine --version`, `cat wine/VERSION` |
| Wine source tree read | `~/work/wine/wine` = **Wine 11.18** (`VERSION`), HEAD `4f4d68f6a78` | `cat VERSION`; `git log -1` |
| Reported OS | Proton: `RtlGetVersion = 10.0.19045`; wine 9: `10.0.19043` | probe output `out/proton_v6.txt`, `out/wine9_v6.txt` |
| Display | host shell has `DISPLAY=""`, only the user's live `/tmp/.X11-unix/X1`. **No Xvfb installed**; `Xephyr` exists but needs a host display ("Xephyr cannot open host display. Is DISPLAY set?") | `echo $DISPLAY; ls /tmp/.X11-unix`; Xephyr run |
| Proton build config | configured with `--disable-tests` => no `uxtheme_test.exe` shipped | `grep disable-tests build/obj-wine-x86_64/config.log` |
| Themes shipped | `lib/wine/x86_64-windows/light.msstyles` (7,585,792 B, Proton), `/usr/lib/x86_64-linux-gnu/wine/x86_64-windows/light.msstyles` (7,573,504 B, Ubuntu); Wine 11.18 source has `dlls/aero.msstyles/` (aero.rc + ~hundreds of .bmp/.svg) | `ls -la`; `ls ~/work/wine/wine/dlls/aero.msstyles` |
| Any Microsoft `.msstyles` on host or in corpus | **none** (`find / -xdev -iname '*.msstyles'` finds only Wine's `light.msstyles` copies; `/api/modules` has uxtheme/themeui/themecpl/themeservice but no aero.msstyles) | commands run |

### 1.1 The harness's "headless null driver" cannot create windows (VERIFIED, important)

`tools/wine_run.sh` runs with `DISPLAY`/`WAYLAND_DISPLAY` unset. Under that, Wine has no graphics driver, and
`CreateWindowEx` fails. A 4-line test (`CreateWindowExW(0, L"STATIC", ...)`) under system wine 9.0 with `WINEDEBUG=err+all`:

```
err:winediag:nodrv_CreateWindow Application tried to create a window, but no driver could be loaded.
err:winediag:nodrv_CreateWindow L"Make sure that your X server is running and that $DISPLAY is set correctly."
static hinst: 0000000000000000 err=0
```

(`err=0` because `GetLastError` is stale; the window handle is NULL; with Proton the visible error was 1400.)
So `wine_run.sh` is fine for uxtheme *API-only* probes (OpenThemeData with a NULL hwnd works, see below) but **any test that
creates a window (header height, GetWindowTheme, DrawThemeParentBackground, buttons, tabs) needs a real X server**.
Without Xvfb installed and with Xephyr unusable, I obtained Xvfb without root:

```
cd $SCRATCH/xvfb_dl && apt-get download xvfb                 # Fetched 877 kB
dpkg -x xvfb_*.deb $SCRATCH/xvfb_root                        # no root; ldd shows no missing libs
$SCRATCH/xvfb_root/usr/bin/Xvfb :NNN -screen 0 1280x1024x24 -nolisten tcp
```

`docs/research/03_wine_probe/run_probe.sh --xvfb` starts a private Xvfb on a random display number (checked free in
`/tmp/.X11-unix`), points only the Wine child at it (`env -u DISPLAY -u WAYLAND_DISPLAY ... DISPLAY=:N`), and tears it down
and removes its socket on exit. After all runs: `ss -x | grep -c X11-unix` = 43 before and 43 after, `pgrep -x Xvfb` = 0,
`pgrep -x wineserver` = 0, no `/tmp/probe_prefix.*` left. The user's X1 was never touched. (Recommendation: install `xvfb`
on the Windows-lab/CI host properly, or keep the apt-get-download recipe; `make wine-run` in the Makefile will otherwise
silently fail every window-creating test.)

## 2. How Wine's uxtheme works (VERIFIED from source, Wine 11.18 tree, `dlls/uxtheme/`)

* **Activation** is a registry value, not a system property. `system.c:44` `szThemeManager =
  Software\Microsoft\Windows\CurrentVersion\ThemeManager`; `UXTHEME_LoadTheme` (system.c:138-196) reads `ThemeActive`
  (`REG_SZ`, `tmp[0] != '0'`), `ColorName`, `SizeName`, `DllName`; if active it opens the msstyles file; on failure
  `bThemeActive = FALSE`. `IsThemeActive` (system.c:539) = `bThemeActive` with `SetLastError(ERROR_SUCCESS)`;
  `IsAppThemed` (system.c:531) = `IsThemeActive()`; `IsCompositionActive` (system.c:549) is a FIXME stub returning FALSE with
  `ERROR_CALL_NOT_IMPLEMENTED`.
* **OpenThemeData** (`open_theme_data`, system.c:~625): NULL class list => `SetLastError(E_POINTER)`, NULL; if
  `bThemeActive` it calls `MSSTYLES_OpenThemeClass(app, classlist, dpi)`; sets last error `ERROR_SUCCESS` or
  `E_PROP_ID_UNSUPPORTED` (0x80070490). Flags to `OpenThemeDataEx` are only FIXME-logged. DPI = `GetDpiForWindow(hwnd)`
  else `GetDpiForSystem()`.
* **The theme** is an ordinary PE resource DLL: `MSSTYLES_OpenThemeFile` (msstyles.c:73) does
  `LoadLibraryExW(file, LOAD_LIBRARY_AS_DATAFILE)` and requires resources `PACKTHEM_VERSION` (== `MSSTYLES_VERSION` 0x0003,
  msstyles.c:50), `COLORNAMES`, `SIZENAMES`, `FILERESNAMES`, then loads a *text INI* resource (msstyles.c:285-312) and
  parses `[Class.Part]` sections with `ImageFile`, `SizingMargins`, `ContentMargins`, etc. Images are `.bmp` resources.
  Wine's `light.msstyles` is built from `dlls/aero.msstyles/aero.rc`, which embeds that INI as a string (aero.rc:614-700 is
  the `[Header]`, `[Header.HeaderItem]`, `[Header.HeaderSortArrow]`, `[Header.HeaderDropDown]` sections).
* **GetThemePartSize** (draw.c:2126-2155):
  `GetThemeEnumValue(TMT_BGTYPE)` -> `BT_IMAGEFILE` => `get_image_part_size` (draw.c:650-740); `BT_BORDERFILL` =>
  `get_border_background_size` (draw.c:~2103: `TS_MIN -> 2*bordersize`, otherwise `2*bordersize+1`); NULL `hTheme` => `E_HANDLE`.
  For images: `TS_MIN` and `TS_TRUE` both return the source rect size; comment draw.c:731 *"FIXME: couldn't figure how native
  uxtheme computes min size"*. `TS_DRAW` returns the source size with DPI scaling for TrueSize parts and clamps to `prc`.
* **GetThemeSysSize** (metric.c:~150): if theme: map SM_* to a TMT_SIZE metric in the theme, else `GetSystemMetrics`.
  **GetThemeSysInt** takes a *TMT_INT* id only and returns `STG_E_INVALIDPARAMETER` for ids outside `TMT_FIRSTINT..LASTINT`.

## 3. Win10 comparison on the paths that matter (VERIFIED from corpus decomp)

Corpus base: `http://docker1.dev.moonlightcompanies.com:8765`.

### 3.1 `GetThemePartSize` — `/api/function/uxtheme/GetThemePartSize/decomp` (RVA 0x14a20)

* handle: `hTheme & 0xffff` = index into `g_pRenderList` table (0x28-byte entries), `hTheme >> 16 & 0xffff` = generation;
  invalid => `0x80070006` (E_HANDLE). Wine's HTHEME is a pointer. (Equal on E_HANDLE; representation differs, matters only
  for tests that print handles.)
* `psz == NULL` => `0x80004003` (E_POINTER). **Wine does not check** (it would dereference NULL).
* BorderFill (`bgtype == 1`): `TS_MIN` => `cx=cy=max(1, 2*borderSize)`; `TS_TRUE` => `2*borderSize+1`; **`TS_DRAW` =>
  `0x80070057` (E_INVALIDARG)**. Wine: `TS_MIN` = `2*bs` (no clamp to 1), `TS_TRUE`/`TS_DRAW` = `2*bs+1` (S_OK).
* Image parts go to `CImageFile::GetPartSize` (`/api/function/uxtheme/%3FGetPartSize%40CImageFile...`, RVA 0x14edc):
  `TS_MIN` => `max(1, left+right content margin)` x `max(1, top+bottom content margin)` of `GetScaledContentMargins`;
  `TS_TRUE` => selected image width/height (or stretched true-size result); `TS_DRAW` => `GetDrawnImageSize(...)`;
  other => `0x80070057`. **Wine's `TS_MIN` returns the image size, i.e. it is wrong for Win10.** This is the main semantic
  divergence for the header work: the probes below show `MIN == TRUE == DRAW` for every Wine image part, which cannot be
  the case on Win10 (`MIN` is margin-derived).

### 3.2 Header layout on Win10 comctl32 (`Header_OnLayout`, `_Header_InitTheme`, `Header_NewFont`)

`/api/function/comctl32/_Header_InitTheme/decomp`:

```
ResetThemeData(hwnd, pHD+0x130, L"Header");
if (theme) {
  GetThemePartSize(theme, NULL, 1 /*HP_HEADERITEM*/, 0 /*state*/, NULL, TS_TRUE, &sz);
  hr = GetThemePosition(theme, 1, 0, 0xd4b /*TMT_MINSIZE*/, &pt);   // if SUCCEEDED: sz = max(sz, pt)
  pHD->0x160 = sz.cy;                                              // themed min header-item height
  GetThemePartSize(theme, NULL, 5 /*HP_HEADERDROPDOWN*/, 0, NULL, TS_TRUE, &sz); pHD->0x114 = max(pHD->0x114, sz.cx);
  pHD->0x118 = GetSystemMetrics(0x2d /*SM_CXEDGE*/);
  GetThemePartSize(theme, NULL, 4 /*HP_HEADERSORTARROW*/, 0, NULL, TS_TRUE, pHD+0x158);
  GetThemePartSize(theme, NULL, 7 /*HP_HEADEROVERFLOW*/, 0, NULL, TS_TRUE, pHD+0x144);
}
```

`/api/function/comctl32/Header_OnLayout/decomp`:

```
h = textExtentCy(pHD+0x4c) + 2*GetLegacyScaledEdgeHeight();
if (theme != 0 && h <= pHD->0x160) h = pHD->0x160;      // themed minimum wins
if (style & HDS_FILTERBAR /*0x100*/) h += textExtentCy + 1 + 2*edge;
layout.cy = (style & HDS_HIDDEN /*8*/) ? 0 : h;
```

`Header_NewFont` (`/api/function/comctl32/Header_NewFont/decomp`) obtains `textExtentCy` from `GetThemeTextExtent(theme, hdc,
HP_HEADERITEM, 0, L"M", -1, DT_CALCRECT-ish 0x800, ...)` when a theme exists, else `GetTextExtentPointW(L"...")`.
So on Win10 the "24" is `max(textH + 2*edge, GetThemePartSize(HP_HEADERITEM,state 0,TS_TRUE).cy, TMT_MINSIZE.y)`.

Wine's comctl32 does none of this (header.c:1636 and :2095 `infoPtr->nHeight = tm.tmHeight + VERT_BORDER` with
`VERT_BORDER = 4`, header.c:93; `HEADER_Layout` header.c:1496-1506 returns `nHeight`). Wine's own test comments:
*"The height of the header control depends on the height of the system font ... 2 dot extra space are needed for the border"*
(`dlls/comctl32/tests/header.c:1756-1762`). **Industrial7's current 20 px equals Wine's value** (system font tmHeight 16 + 4,
see section 4); Win10 gets 24 from the theme. Wine therefore reproduces the *Industrial7* symptom, not the Win10 behaviour.

### 3.3 Other functions compared against decomp

| Function | Win10 (decomp) | Wine | Verdict |
|---|---|---|---|
| `IsThemeActive` (RVA 0x224e0) | `SetLastError(0)`; returns 0 if `SPI_GETHIGHCONTRAST` flag set and app has not opted in; else checks app-info/root-section state | registry `ThemeActive` + successful theme load; no HC | match in the default state only |
| `IsAppThemed` (0x224b0) | `IsAppThemedInternal()` AND `IsThemeActive()` | `IsThemeActive()` | diverges for non-themed apps (INFERENCE: Win10 returns FALSE for un-manifested legacy processes; not measured) |
| `OpenThemeData` / `OpenThemeDataEx` (0x1b590 / 0x50dc0) | return NULL if `IsHighContrastMode()` and not opted in; else `OpenThemeDataExInternal(hwnd, cls, flags, ..)` | no HC; flags ignored | diverges (HC, flags) |
| `OpenThemeData(NULL class)` | Wine tests assert `GLE == E_POINTER` (`tests/system.c:518-521`, run on Windows) | E_POINTER (probe: `lasterr=0x80004003`) | match |
| `OpenThemeData(unknown class)` | `E_PROP_ID_UNSUPPORTED` (tests/system.c:526-529) | `0x80070490` (probe: `NoSuchClass`, `Header::Foo`) | match |
| `GetThemeSysSize` (0x509f0) | NULL theme => `ClassicGetSystemMetrics`; else value from theme's cached metric table (+0x3b8) through `ScaleSizeForScreenDpi` | theme metric else `GetSystemMetrics` | match at 96 dpi with the system's metrics; DPI-scaling path unverified |
| `GetThemePosition` / `ExternalGetPosition` (0x6af0 / 0x6b94) | `psz==NULL` -> E_POINTER; property missing -> `0x80070490` | missing -> `0x80070490` (probe: TMT_MINSIZE for Header -> `0x80070490`) | match |
| `GetThemePartSize` | see 3.1 | see 3.1 | **diverges** (TS_MIN, BorderFill TS_DRAW, NULL psz) |

## 4. Experiments run (all headless, Xvfb :N private, throw-away prefix copies)

Sources and raw outputs are committed under `docs/research/03_wine_probe/` (`probe.c`, `probe.manifest`, `probe_v6.rc`,
`Makefile`, `run_probe.sh`, `out/*.txt`). Build and run:

```
cd docs/research/03_wine_probe
make                                        # x86_64-w64-mingw32-gcc; probe_v5.exe (no manifest), probe_v6.exe (comctl32 v6 manifest via windres)
export XVFB=$SCRATCH/xvfb_root/usr/bin/Xvfb
PW=~/work/wine/proton-ge-custom/build/dist/files/bin/wine ; PB=~/.cache/test_pattern/prefix        # Proton 11.0, prefix made by tools/make_base_prefix.sh
W9=~/.cache/test_pattern/prefix_wine9                                                               # created on first run: /usr/bin/wine wineboot -i
./run_probe.sh $PW  $PB probe_v6.exe --xvfb                  > out/proton_v6.txt        # themed, v6
./run_probe.sh $PW  $PB probe_v5.exe --xvfb                  > out/proton_v5.txt        # themed machine, un-manifested exe
./run_probe.sh $PW  $PB probe_v6.exe --xvfb --theme off      > out/proton_v6_off.txt    # reg add HKCU\...\ThemeManager /v ThemeActive /d 0
./run_probe.sh /usr/bin/wine $W9 probe_v6.exe --xvfb [--theme off]                       # system wine 9.0
```

`probe.c` loads `uxtheme.dll` dynamically (`LoadLibraryW` + `GetProcAddress` through `__typeof__(&Name)`, the harness style)
and prints `key = value` lines so runs can be diffed. It records: version/DPI/system metrics, `IsThemeActive`,
`IsAppThemed`, `GetCurrentThemeName`, `OpenThemeData` for 12 classes, `GetThemePartSize` (TS_MIN/TRUE/DRAW), `GetThemeEnumValue(BGTYPE,
IMAGELAYOUT)`, `GetThemeInt(BORDERSIZE, IMAGECOUNT, MINDPI1)`, `GetThemeMetric(HEIGHT, WIDTH)`, `GetThemeMargins(CONTENT,
SIZING)`, `GetThemeFilename(IMAGEFILE)`, `GetThemeBackgroundContentRect`, `IsThemePartDefined` for 28 class/part/state tuples,
the exact `_Header_InitTheme` call set (section 3.2), `GetThemeSys{Size,Int,Bool,Color,Font}`, and four header windows
(`CreateWindowExW(WC_HEADERW)`, `WM_SETFONT` optional, `HDM_INSERTITEMW`, `HDM_LAYOUT`, `SetWindowPos`, `GetWindowRect`).

### 4.1 Header control height — themed vs themeless, v5 vs v6 (the requested table)

Parent client width 392; header created with `HDS_BUTTONS|HDS_HORZ` (cy from `HDM_LAYOUT`, and confirmed with `GetWindowRect` after
`SetWindowPos`); "system font" = header's default (`SYSTEM_FONT`, tmHeight 16); "GUI font" = `WM_SETFONT(DEFAULT_GUI_FONT)` (tmHeight 13).

| Run (file in `out/`) | `IsThemeActive` | comctl32 loaded | `GetWindowTheme(hdr)` | header cy, system font | header cy, GUI font |
|---|---|---|---|---|---|
| Proton 11.0, v6 manifest, theme on (`proton_v6`) | 1 | `winsxs\...6.0.2600.2982...\COMCTL32.dll`, DllGetVersion 6.0.2600 | set | **20** | **17** |
| Proton 11.0, v6 manifest, theme **off** (`proton_v6_off`) | 0 | same (v6) | NULL | **20** | **17** |
| Proton 11.0, no manifest (`proton_v5`) | 1 | `system32\COMCTL32.dll`, 5.81.4704 | NULL (v5 does not open theme) | **20** | **17** |
| Proton 11.0, no manifest, theme off (`proton_v5_off`) | 0 | v5 | NULL | **20** | **17** |
| wine 9.0, v6 manifest, theme on (`wine9_v6`) | 1 | winsxs v6 file | set | **20** | **17** |
| wine 9.0, v6 manifest, theme off (`wine9_v6_off`) | 0 | winsxs v6 file | NULL | **20** | **17** |
| wine 9.0, no manifest (`wine9_v5`) | 1 | system32 | set | **20** | **17** |
| **Win10 10.0.19041 (per owner)** | 1 | comctl32 6.10.19041.1110 | set | **24** | not measured |

VERIFIED: Wine header height does not move with theme state or comctl32 version. Equal `20` in Industrial7 is the same
algorithm (font height 16 + 4). The `24` target requires the Win10 `Header_OnLayout` logic of 3.2 *and* a theme source returning
`GetThemePartSize(HP_HEADERITEM, 0, TS_TRUE).cy == 24`.

Same probe, `_Header_InitTheme` call set in Wine's themed run (`out/proton_v6.txt`):

```
InitTheme PartSize(hdc=NULL,part=1,state=0,TS_TRUE) = hr=0x00000000 cx=6  cy=24
InitTheme PartSize(hdc=NULL,part=5,state=0,TS_TRUE) = hr=0x00000000 cx=3  cy=24
InitTheme PartSize(hdc=NULL,part=4,state=0,TS_TRUE) = hr=0x00000000 cx=13 cy=5
InitTheme PartSize(hdc=NULL,part=7,state=0,TS_TRUE) = hr=0x00000000 cx=8  cy=21
InitTheme GetThemePosition(1,0,TMT_MINSIZE=0xd4b)   = hr=0x80070490 x=-7 y=-7     (property absent -> same HRESULT as Win10 ExternalGetPosition)
```

Why 24: Wine's `blue_header_item.bmp` is 6 x 288 (BMP header parse), `ImageCount = 12`, `ImageLayout = Vertical` =>
288/12 = 24 (aero.rc:623-631). **INFERENCE**: Wine's theme authors copied Win10's metrics; the match with the owner's "24 on Win10"
is encouraging but is one data point; the Win10 value must still be recorded on Windows (probe list: `HP_HEADERITEM` state 0
`TS_TRUE`, `TS_MIN`, `TS_DRAW`, `TMT_MINSIZE`, `TMT_CONTENTMARGINS`, `TMT_IMAGECOUNT`).

### 4.2 Wine-theme values for the controls in the survey (Proton 11.0, 96 dpi) — NOT Win10-verified

`GetThemePartSize(theme, hdc, part, state, NULL, TS_TRUE, &sz)` from `out/proton_v6.txt` (hr = S_OK for all). For image parts `TS_MIN`
and `TS_DRAW` returned the same value as `TS_TRUE`; for the BorderFill parts (listview, edit, progress) `MIN=2x2`, `TRUE=DRAW=3x3`.

| Class / part / state | TS_TRUE (cx x cy) | Contents margins (l,r,t,b) |
|---|---|---|
| Header HP_HEADERITEM / HIS_NORMAL (and HOT) | 6 x 24 | 0,0,0,0 (SizingMargins 2,3,9,14) |
| Header HP_HEADERITEMLEFT / RIGHT | 3 x 24 / 3 x 24 | - |
| Header HP_HEADERSORTARROW / HSAS_SORTEDUP | 13 x 5 | - |
| Button BP_PUSHBUTTON / PBS_NORMAL | 22 x 23 | 3,3,3,3 |
| Button BP_CHECKBOX / CBS_UNCHECKEDNORMAL | 13 x 13 | - |
| Button BP_RADIOBUTTON / RBS_UNCHECKEDNORMAL | 13 x 13 | - |
| Button BP_GROUPBOX / GBS_NORMAL | 22 x 20 | - |
| Tab TABP_TABITEM / TIS_NORMAL, TIS_SELECTED | 5 x 19 | 0,0,0,0 |
| Tab TABP_PANE / TABP_BODY | 48 x 48 / 10 x 600 | - |
| ListView LVP_LISTITEM / LISS_NORMAL | MIN 2x2, TRUE 3x3 (BorderFill, BORDERSIZE 1) | undefined |
| Scrollbar SBP_ARROWBTN UPNORMAL / SBP_THUMBBTNVERT NORMAL | 17 x 17 / 17 x 11 | - |
| Edit EP_EDITTEXT / ETS_NORMAL | MIN 2x2, TRUE 3x3 | - |
| Combobox CP_DROPDOWNBUTTON / CBXS_NORMAL | 7 x 21 | - |
| Window WP_CAPTION ACTIVE / WP_CLOSEBUTTON NORMAL | 66 x 29 / 28 x 17 | - |
| Toolbar TP_BUTTON / Status SP_PANE / TreeView TVP_GLYPH CLOSED / Spin SPNP_UP | 7x18 / 3x15 / 9x9 / 8x16 | - |

The full 522-line dump (including `GetThemeInt/Metric/Margins/Filename/ContentRect`) is `out/proton_v6.txt`. Notes:
`GetThemeMetric(TMT_HEIGHT/TMT_WIDTH)` returned `0x80070490` for every probed part (these ids are not present in Wine's theme; **UNKNOWN**
whether Win10 defines them), `GetThemeSysInt` for `TMT_SCROLLBARWIDTH` returned `0x80030057` because it is a TMT_SIZE id (Wine
source metric.c:~145-155). `GetThemeSysSize(SM_CXVSCROLL..SM_CYMENUSIZE)` equalled `GetSystemMetrics` for all ten ids (e.g.
SM 2->17, 3->17, 30->18, 31->25), and `GetThemeSysColor(i) == GetSysColor(i)` for all 31 (e.g. i=26 `0xfa9630`).
Wine 9.0 and Wine 11.0 produced identical probe output except two sys colours (index 17: `0xa6a6a6` vs `0x6a6a6a`; 26: `0xe0e0e0` vs `0xfa9630`),
i.e. Wine's theme itself changed between those versions: **a Wine-derived table is version-dependent, another reason it cannot be an oracle.**
BP_CHECKBOX 13x13 at 96 dpi agrees with Wine's *Windows-verified* test (section 5, `test_GetThemePartSize`: `expected = MulDiv(13, dpi, 96)`).

### 4.3 Themeless Wine (`ThemeActive=0`) — `out/proton_v6_off.txt`

```
IsThemeActive = 0 ; IsAppThemed = 0
GetCurrentThemeName = hr=0x80070490 name="" color="" size=""
OpenThemeData class = <all 12 classes> -> NULL lasterr=0x80070490
GetThemePartSize(NULL htheme) = hr=0x80070006 cx=-7 cy=-7        (E_HANDLE, output untouched)
GetThemeSysSize[SM n] = GetSystemMetrics(n)     (theme NULL)
```

This matches what Industrial7's themeless uxtheme does today (OpenThemeData NULL, GetThemePartSize error), so Wine-themeless is
a decent *comparison* for the "no theme" path (and also confirms `E_PROP_ID_UNSUPPORTED` as the last-error convention). Win10
`GetThemeSysSize(NULL, n)` uses `ClassicGetSystemMetrics` (decomp), the same shape.

### 4.4 Can "themed under Wine" be done headlessly? — Yes, and it is the default

* `loader/wine.inf.in:1029-1033` seeds `HKCU\...\ThemeManager` with `ThemeActive=1`, `DllName=%10%\resources\themes\aero\aero.msstyles`
  (Wine 11.18 tree; Proton/Ubuntu use `...\light\light.msstyles`, visible in `~/.cache/test_pattern/prefix/user.reg:571-575`),
  `ColorName=Blue`, `SizeName=NormalSize`. So every fresh prefix is themed; v6 comctl32 additionally attaches the theme to
  windows (`GetWindowTheme` non-NULL).
* To disable: `wine reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\ThemeManager' /v ThemeActive /t REG_SZ /d 0 /f`
  (what `run_probe.sh --theme off` does; effect shown in 4.3). `EnableTheming(FALSE)` also writes this value (system.c:564-577).
* To load a *different* theme: `reg add ... /v DllName /d <path-to-.msstyles>` (`run_probe.sh --theme path:<dll>`), the file must be
  Wine's text-INI/`PACKTHEM_VERSION 3` format. **No other msstyles exists on this host**, and a real Microsoft `aero.msstyles` is
  expected not to load (INFERENCE: Vista+ msstyles use a different, compiled layout; Wine's loader returns `ERROR_BAD_FORMAT`
  when `PACKTHEM_VERSION != 3`, msstyles.c:108-113). The corpus API has no msstyles. If a lane ever dumps Win10's `aero.msstyles` the
  cheaper path is to parse it directly, not to feed it to Wine.
* winetricks themes: not tried (they are XP-format Luna ports, would give XP numbers anyway).

## 5. Wine's own tests as a (partial) source of real-Windows facts

### 5.1 Building them (the Proton build was configured `--disable-tests`, so none are shipped)

Out-of-tree build of the Wine 11.18 tree into the scratchpad (no root, no changes to the user's trees):

```
# configure needs flex+bison, which are not installed: fetched with apt-get download + dpkg -x
export BISON_PKGDATADIR=$SCRATCH/flex_root/usr/share/bison PATH=$SCRATCH/flex_root/usr/bin:$PATH
mkdir $SCRATCH/wbuild && cd $SCRATCH/wbuild
~/work/wine/wine/configure --enable-win64 --with-mingw=x86_64-w64-mingw32-gcc --enable-tests --disable-win16 --without-x --without-wayland \
   --without-alsa --without-pulse --without-gstreamer --without-opengl --without-vulkan --without-ffmpeg --without-sdl --without-usb ...   # 'Finished.'
make -j28 dlls/uxtheme/tests/x86_64-windows/uxtheme_test.exe dlls/comctl32/tests/x86_64-windows/comctl32_test.exe     # real 4.7 s (tools+import libs+tests)
```

(An earlier attempt to compile `tests/system.c` standalone against mingw headers failed on Wine-header/mingw-header clashes; the
out-of-tree build is the working route and takes seconds.) The test exes are link-time Wine PE binaries; they were run under the
two Wine versions only, not on Windows.

### 5.2 Results (Proton 11.0, Xvfb, `out/uxtheme_test_proton.txt`, `out/cc_*.txt`)

| Binary | Result | Failures explained |
|---|---|---|
| `uxtheme_test.exe` | `22254 tests executed (16 marked as todo, 0 as flaky, 3 failures), 4 skipped` | (a) `system.c:876`: test expects theme file `aero.msstyles`, Proton ships `light.msstyles` — packaging, not behaviour. (b) `system.c:1756`: `DrawThemeParentBackground(parent, hdc, NULL)` sent `WM_ERASEBKGND(0x14)`+`WM_PRINTCLIENT(0x318)` where the test (written against Windows) expects an empty sequence — Wine divergence (cause under Xvfb-without-WM not isolated; **not** marked todo_wine). (c) `system.c:2799`: Immersive colour exports missing in uxtheme (`GetImmersiveColorFromColorSetEx` and `GetImmersiveUserColorSetPreference` GetProcAddress failures at system.c:109-110). |
| `uxtheme_test.exe` on system wine 9.0 | same 3 failures, then **crashes**: `wine: Unhandled page fault on read access to 0000000000000004` after the immersive-colour test (`out/uxtheme_test_wine9.txt`) | not investigated |
| `comctl32_test.exe header` | `1053 tests executed (9 marked as todo, 0 failures)` | - |
| `comctl32_test.exe button` | `11542 tests, 76 failures` | all `SetFocus(0) on a button` message-sequence mismatches => no window manager / focus under bare Xvfb (environmental) |
| `comctl32_test.exe tab` | `5671 tests, 11 failures` | focus-index and `TCS_BUTTONS` parent-sequence (focus/WM again; one rect off-by-one `(1,2)-(98,25)` at tab.c unmarked) |

Takeaway: the Wine tests are **runnable under Xvfb** with a recipe of a few minutes, but ~90 failures in button/tab are focus noise,
so they would need a window manager (e.g. a minimal `openbox`/`twm`, or `xdotool`) before being trusted as regression signal.

### 5.3 What the Wine tests *teach* about real Windows (the useful part)

These assertions pass on Windows (that is how Wine tests are validated) and are therefore reusable as expected values, **each
still needs re-recording in our lane before it enters the corpus**:

* `OpenThemeData(any, NULL)` -> NULL and `GetLastError() == E_POINTER`; unknown class -> `E_PROP_ID_UNSUPPORTED` (`tests/system.c:~516-529`, :556-560).
* `GetThemePartSize(BP_CHECKBOX, CBS_CHECKEDNORMAL, TS_DRAW)` is `13x13` at 96 dpi, 26 at 192 dpi when the theme was opened with a
  per-monitor-aware window, **and does not change when the system DPI changes afterwards unless the theme is reopened** (`system.c:1161-1325`).
* `GetThemePartSize(htf /*theme FILE handle*/, ...)` -> `E_HANDLE`; `OpenThemeFile(NULL,...)` -> `E_POINTER` (:915-930).
* `GetCurrentThemeName` on Windows returns a path whose leaf is `aero.msstyles` (:874-876) — for Win10 default theme (consistent with `themeui`).
* `GetThemeTransitionDuration` invalid handle -> `E_HANDLE` (:1562), `GetThemeIntList`, `EnableThemeDialogTexture`, buffered paint, etc.
* The 15 `todo_wine` markers in `tests/system.c` are the *known Wine-vs-Windows* gaps (lines 203, 583-586, 629, 819-832, 921, 930, 1048-1053,
  1562, 2179-2183, 2246, 2813). Each is a place where Windows behaviour is documented by an assertion and Wine fails it.

comctl32: `tests/header.c` only checks header height against `tmHeight + 2` *for the customdraw rects* (lines 1756-1762, uses the
SYSTEM_FONT); there is **no themed header height test** anywhere in Wine's tree (grep of `HP_HEADER` in `dlls/comctl32/tests`: none; in
`dlls/uxtheme/tests/system.c`: none). `tab.c` / `button.c` have `todo_wine` markers on tab rects and push-box content, none on theme part sizes.

## 6. Verdict per function / area

Legend: **S** = usable as a regression *sanity* check against Wine (cheap, headless, but only catches gross errors); **W** = must come from real Windows 10;
**X** = Wine is wrong/misleading, do not compare.

| Area | Wine usable? | Notes |
|---|---|---|
| `IsThemeActive`/`IsAppThemed` default-state return (TRUE) | S | Wine 1/1; Win10 depends on HC/app info; record both on Windows (W) for HC and un-manifested processes |
| `OpenThemeData` NULL-class / unknown-class / known-class return+last-error | S | matches Windows-verified Wine tests and probe |
| `OpenThemeData` class-list syntax (`app::class;other`), `OpenThemeDataEx` flags, HC mode | W / X | todo_wine at system.c:583-586; flags ignored; no HC |
| HTHEME handle format | X | Win10: 16-bit index + 16-bit generation (decomp); Wine: pointer |
| `CloseThemeData`, `GetWindowTheme`, `SetWindowTheme` | S (lightly) | Wine tests cover; Industrial7 is themeless so only error paths apply |
| `GetThemePartSize` TS_TRUE for image parts | W | Wine returns its own theme's image size; header 6x24, checkbox 13x13 agree with owner/Wine-tests but unverified for the rest |
| `GetThemePartSize` TS_MIN | X | Win10 = content-margin sum (min 1); Wine returns image size |
| `GetThemePartSize` BorderFill TS_DRAW | X | Win10 -> `0x80070057`; Wine -> `2*bs+1` S_OK |
| `GetThemePartSize` NULL psz / NULL htheme | S for E_HANDLE; X for NULL psz | Win10 E_POINTER, Wine crashes |
| `GetThemePosition`/`GetThemeInt`/`GetThemeMetric` on missing property | S | both `0x80070490` |
| `GetThemeMetric`/`GetThemeInt` values (TMT_HEIGHT, MINSIZE, BORDERSIZE ...) | W | Wine's theme defines different/fewer properties |
| `GetThemeMargins` (CONTENT/SIZING) | W | theme data |
| `GetThemeSysSize` with NULL theme | S | both fall back to system metrics |
| `GetThemeSysSize` with a theme at 96 dpi | S (weak) / W for DPI scaling | Win10 routes via `ScaleSizeForScreenDpi` |
| `GetThemeSysColor/Font/Bool/Int` | W | Wine = `GetSysColor`/SPI; Win10 reads theme tables; Wine 9 vs 11 already differ |
| `DrawTheme*` pixel output | X | different art, different compositor; use only for crash/no-op checks |
| `IsCompositionActive` | X | Wine stub FALSE |
| comctl32 header height | X | ignores theme (20/17) |
| comctl32 button/tab/listview themed metrics | X for numbers; S for message flow | needs a WM under Xvfb for the focus-sensitive tests |
| msstyles loading | X | Wine reads XP-style text-INI themes only |

## 7. Recommendations for the corpus

1. Treat Wine as a **harness smoke-test target**: `tools/wine_run.sh` + `--xvfb` (this directory's `run_probe.sh`) verifies the new
   test exes load, resolve exports and do not crash. Do not encode Wine's numbers as expected values; where a test passes on Wine
   but disagrees with the owner's Win10 observations (header 24), mark the check as Windows-authoritative.
2. Add `xvfb` (or the `apt-get download` recipe) to the lane setup; `tools/wine_run.sh`'s null-driver mode cannot run window-creating tests
   (section 1.1). Also consider a trivial window manager for focus-dependent Wine tests.
3. Probe additions that Wine data suggests (to be recorded on Win10): everything in `_Header_InitTheme` (parts 1,4,5,7 + TMT_MINSIZE 0xd4b,
   state 0, hdc NULL, TS_TRUE), plus `TS_MIN`/`TS_DRAW` per part to pin the margin-derived `TS_MIN` rule, BorderFill `TS_DRAW` E_INVALIDARG,
   `psz==NULL` E_POINTER, `OpenThemeData` under `SPI_SETHIGHCONTRAST`, `IsAppThemed` in an un-manifested process, and the
   DPI-reopen behaviour from `test_GetThemePartSize`. `docs/research/03_wine_probe/probe.c` is a ready skeleton: it already
   prints `key = value` for these calls, runs unmodified on Windows (mingw build), and its output can be diffed against `out/*.txt`.
4. The recorded-constants table for the armed-Win10 personality must be sourced from Windows 10 (probe run or `aero.msstyles` extraction).
   Wine's `light.msstyles` numbers may be used only as **placeholders/cross-checks**, flagged "Wine-derived, unverified".

## 8. Unknowns / not done

* Win10 `aero.msstyles` internal format and any Win10 part-size constant besides the owner's "header = 24": UNKNOWN (no Windows host, no file).
* Why `DrawThemeParentBackground(parent, hdc, NULL)` fails Wine's own test under Xvfb (genuine Wine divergence vs missing WM): not isolated.
* Wine 9 crash in `uxtheme_test` after the immersive-colour test: not investigated.
* Win10 `GetThemeInt/GetThemeMetric/GetThemeMargins/GetThemeFilename/GetThemeSysColor` decomps were not read end to end (only
  `GetThemePosition`/`ExternalGetPosition` and `GetThemeSysSize`).
* Not tried: winetricks themes, Xephyr (cannot run without a host display here), comctl32 listview/toolbar Wine tests, high-DPI (`LOGPIXELSY` stayed 96).
