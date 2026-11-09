# 01 - Consumers of uxtheme.dll (Windows 10 22H2, uxtheme 10.0.19041.2193)

Role: consumer surveyor. Scope: who imports uxtheme, what every themed control in comctl32 v6 asks of it,
where the CC30 header height (24 px on Win10, 20 px in Industrial7) comes from, and what Industrial7 does today.

Labels used below: **[V-api]** read from the corpus API (a URL or an RVA is given), **[V-cmd]** the output of a command
that was run here, **[V-src]** file:line in Wine or Industrial7, **[INF]** inference (not read directly), **UNKNOWN**
(no data in hand; a probe is needed, see section 7).

Corpus API base: `B=http://docker1.dev.moonlightcompanies.com:8765`. All RVAs are `module!0xRVA`.
Property and part names come from Wine's `include/vssym32.h` and `include/vsstyle.h` (the numeric values are the public
ones; the Win10 decomp uses the same numbers, checked by the `GetThemeInt/Margins/Color/Font` call sites below).

---------------------------------------------------------------------------------------------------------------------

## 0. Method (reproducible)

1. Static importers: for every module with `nfuncs>0` (2863 of 3007 in `/api/modules`) ran
   `GET $B/api/modules/<mod>/imports?dll=uxtheme.dll&limit=1000`. The `dll=` filter works (comctl32 returns `total:52`).
   Result of the `dll=uxtheme.dll` scan alone: 76 modules, 518 entries, 107 distinct exports [V-cmd, script `agg.py`; re-run and reproduced in the fact-check].
   **CORRECTION (fact-check): this filter misses imports made through the API-set stub `ext-ms-win-uxtheme-themes-l1-1-0.dll`**
   (`/api/modules/conhost/imports?dll=ext-ms-win-uxtheme-themes-l1-1-0.dll` -> `SetWindowTheme`, `delay:true`, `target uxtheme.dll!0x3bb0`).
   Re-scanning all 2863 modules with that dll name adds 17 modules (conhost, msftedit, edgehtml, udwm, windows.ui.xaml, twinui.pcshell, ...) and 76 entries:
   **corrected totals: 93 modules, 594 import entries, still 107 distinct exports** (the API-set names `GetUserColorPreference`, `GetColorFromPreference`,
   `GetImmersiveColorFromColorSetEx`, `GetImmersiveUserColorSetPreference` are the same exports as ordinals 120, 121, 95, 98). See the corrected ranking in 1.2a.
   Delay-load and normal imports are both in the table (`"delay":true` flag); nothing here sees `GetProcAddress` users.
2. Ordinal names: `GET $B/api/modules/uxtheme/exports?limit=1000` (193 exports: 34 are `SHUNIMPL.#n` forwarders, 91 have no
   name) and `GET $B/api/resolve?ref=uxtheme!<rva>` for each unnamed ordinal gives the PDB function name. Wine's
   `dlls/uxtheme/uxtheme.spec` is used only as a cross-check (it lacks several ordinals).
3. comctl32 theme call sites: downloaded all 8531 comctl32 decompilations
   (`GET $B/api/function/comctl32/0x<rva>/decomp`; NOTE the path takes a **hex** RVA, a decimal one is silently misread),
   grepped every call to an OpenThemeData/GetTheme*/DrawTheme*/IsTheme*/SetWindowTheme/BeginBufferedPaint/ordinal import:
   231 functions, ~616 statements, ~300 distinct API call sites [V-cmd, script `ext.py`]. Every control row in section 3 cites
   the function and RVA it was read from.
4. Industrial7: grep of `/home/iceisfun/work/industrial7` (docs, crates, userland/guest).

---------------------------------------------------------------------------------------------------------------------

## 1. Who imports uxtheme

### 1.1 Headline facts

* **user32, gdi32, gdi32full, win32u do not import uxtheme** [V-api: `/api/modules/<m>/imports?dll=uxtheme.dll` -> `total:0` for each].
  user32's only `Theme` functions are `__xxxClientBroadcastThemeChange` (0x2bc30) and the `NtUserBroadcastThemeChangeEvent` syscall stub (0x33a40) [V-api search `Theme` in user32, total 5 hits].
  Windows chrome (caption, borders, scrollbars, menus) is themed through the **UserApiHook** path instead, see 1.4.
* **notepad.exe does not import uxtheme** (`total:0`); it reaches uxtheme only through comctl32 v6.
  **conhost.exe DOES delay-import `SetWindowTheme`**, but via the API-set stub `ext-ms-win-uxtheme-themes-l1-1-0.dll` (conhost!`DelayLoad_SetWindowTheme` 0x22504, IAT 0xd05f8,
  schema target `uxtheme.dll!0x3bb0`) [V-api, fact-check]. The original text of this survey ("not supported by the corpus") was WRONG: it came from filtering on `dll=uxtheme.dll`.
  So `docs/design/win10/UXTHEME-OFF-W10-LINK.md` (conhost delay-imports SetWindowTheme) is correct on this point. Consequence: an armed uxtheme must also be reachable as the
  `ext-ms-win-uxtheme-themes-l1-1-0` schema host (Industrial7 API-set map), otherwise conhost/udwm/msftedit/XAML imports do not bind.
* **comctl32 is the dominant consumer**: it imports **52 names** (47 by name + ordinals 47, 49, 72, 75, 131), all as delay-loads
  through `DelayLoad_*` thunks (RVAs 0x9aa09 ... in `.text`, e.g. `DelayLoad_GetThemePartSize` = 0x9aa09, 12 bytes) [V-api].
  Because they are delay-loaded, comctl32 survives a uxtheme that lacks an export only until the first call of it.
* Callers cannot be found with `/callers` or `/xrefs` for these: the thunk is reached through the IAT, and
  `GET $B/api/function/comctl32/DelayLoad_GetThemePartSize/callers` returns an empty edge list [V-api]. Call sites were found by grepping decomps instead.

### 1.2 Ranking of exports by number of importing modules (static imports only)

**Superseded counts (fact-check, direct `uxtheme.dll` imports only; the API-set importers add):** corrected module counts, direct + `ext-ms-win-uxtheme-themes-l1-1-0`:
SetWindowTheme 29, OpenThemeData 30, CloseThemeData 30, DrawThemeBackground 23, GetThemeColor 20, GetThemeFont 15, IsAppThemed 14, IsThemeActive 12, GetThemeMargins 8,
GetThemeInt 5, #120 GetUserColorPreference 40, #121 GetColorFromPreference 38, #106 15, #104 13, GetThemeTimingFunction 8, GetThemeAnimationTransform 9, GetThemeAnimationProperty 7;
unchanged: DrawThemeTextEx 13, Begin/EndBufferedPaint 13, BufferedPaintInit/UnInit 11, GetThemePartSize 8, GetThemeMetric 5, GetThemeBackgroundExtent 7. Largest per-module totals unchanged (comctl32 52, shell32 37, explorerframe 35).
Ranks 1-3 are therefore OpenThemeData/CloseThemeData (30), SetWindowTheme (29) after the immersive-colour ordinals #120/#121. The table below is the original direct-import count.

(count = modules; ordinal names resolved from the Win10 PDB via `/api/resolve`. "#N" = imported by ordinal.)

| rank | modules | export | notable importers |
|---|---|---|---|
| 1 | 27 | SetWindowTheme | comctl32, comdlg32, shell32, explorerframe, ieframe, dxp, devmgr, aclui, mshtml, wmp ... |
| 2 | 25 | #120 GetUserColorPreference | applicationframe, authui, explorerframe, idctrls, inputswitch, twinui ... (immersive color; not a control API) |
| 2 | 25 | CloseThemeData | comctl32, dui70, shell32, explorerframe, ieframe, ... |
| 2 | 25 | OpenThemeData | comctl32, dui70, shell32, explorerframe, ieframe, ... |
| 5 | 23 | #121 GetColorFromPreference | same set as #120 |
| 6 | 20 | DrawThemeBackground | comctl32, shell32, explorerframe, dsquery, dot3ui, dxp, mshtml, ieframe, ... |
| 7 | 18 | GetThemeColor | comctl32, dui70, explorerframe, shell32, mshtml, uiribbon, twinui ... |
| 8 | 13 | DrawThemeTextEx | comctl32, dui70, shell32, explorerframe, ieframe, inputswitch, uiribbon, wmp ... |
| 8 | 13 | BeginBufferedPaint / EndBufferedPaint | comctl32, dui70, duser, dxp, explorerframe, ieframe, fontext, uiribbon, wmp ... |
| 8 | 13 | IsAppThemed | comctl32, comdlg32, dui70, shell32, explorerframe, ieframe, mshtml, uiribbon, wmp ... |
| 8 | 13 | GetThemeFont | comctl32, dui70, shell32, explorerframe, ieframe, mshtml, twinui ... |
| 12 | 11 | IsThemeActive | comctl32, explorerframe, ieframe, mshtml, shell32 ... |
| 12 | 11 | BufferedPaintInit / BufferedPaintUnInit | comctl32, dui70, duser, dxp, explorerframe, fontext ... |
| 14 | 10 | #106 GetIsImmersiveColorUsingHighContrast | applicationframe, comdlg32, dui70, explorerframe ... |
| 15 | 9 | #104 RefreshImmersiveColorPolicyState | applicationframe, comdlg32, dui70, explorerframe ... |
| 16 | 8 | GetCurrentThemeName, EnableThemeDialogTexture, DrawThemeText, **GetThemePartSize**, IsCompositionActive, DrawThemeParentBackground, GetThemeBackgroundContentRect | GetThemePartSize: comctl32, dsquery, dui70, explorerframe, ieframe, shell32, uiribbon, wcnwiz |
| 23 | 7 | GetThemeMargins | comctl32, dui70, ieframe, mshtml, shell32, uiribbon, wmp |
| 23 | 7 | GetThemeBackgroundExtent | comctl32, dui70, ieframe, mshtml, shell32, wmp, ... |
| 25 | 6 | #132 ShouldAppsUseDarkMode, #122 SetUserColorPreference | comdlg32, explorerframe, ntshrui, shell32, themeui, windows.ui.fileexplorer |
| 27 | 5 | GetThemeTextExtent, GetThemeAnimationTransform, GetBufferedPaintBits, GetThemeAppProperties, **GetThemeMetric**, BufferedPaintClear, #47 DrawThemeBackgroundEx | GetThemeMetric: comctl32, dui70, ieframe, shell32, wcnwiz |
| 34 | 4 | GetThemeTimingFunction, SetWindowThemeAttribute, GetThemeAnimationProperty, **GetThemeInt**, #133 AllowDarkModeForWindow | GetThemeInt: comctl32, mshtml, twinui, uiribbon |
| 39 | 3 | BufferedPaintSetAlpha, GetThemePosition, IsThemePartDefined, #135 SetPreferredAppMode, #139 IsDarkModeAllowedForApp, #138 ShouldSystemUseDarkMode | |
| 45 | 2 | GetThemeEnumValue, BufferedPaintStopAllAnimations, GetThemeBool, OpenThemeDataForDpi, IsThemeBackgroundPartiallyTransparent, GetBufferedPaintDC, GetBufferedPaintTargetDC, Begin/End/UpdatePanningFeedback, GetThemeSysFont, GetWindowTheme, #95, #98, #1 QueryThemeServices, #65 SetSystemVisualStyle, #7 GetThemeDefaults, ... | |
| 61+ | 1 | GetThemeTextMetrics, GetThemePropertyOrigin, GetThemeBackgroundRegion, Begin/EndBufferedAnimation, BufferedPaintRenderAnimation, DrawThemeParentBackgroundEx, DrawThemeEdge, GetThemeTransitionDuration, #49 OpenNcThemeData, #72 DrawThemeIconEx, #75 ThemeForwardCapturedMouseMessage, #131 GetBufferedPaintBitsEx (all comctl32 only); GetThemeSysColor (explorerframe), OpenThemeDataEx + GetThemeBitmap (mshtml), GetThemeDocumentationProperty / EnumThemes / EnumThemeColors / EnumThemeSizes / ClassicGetSystemMetrics / ClassicSystemParametersInfoW (themeui), the theme-service ordinals (uxinit) | |

Module totals worth knowing [V-cmd]: comctl32 52, shell32 37, explorerframe 35, ieframe 30, dui70 27, wmp 22, themeui 19,
uiribbon 18, mshtml 15, twinui 14, applicationframe 13, windows.ui.fileexplorer 13, comdlg32 9, duser 6.

**Imported by nobody in the 2863 indexed modules (52 non-forwarder exports)** [V-cmd]:
RegisterDefaultTheme, DumpLoadedThemeToTextFile, OpenThemeDataFromFile, OpenThemeFileFromData, EnableTheming (#36, #129),
ClassicSystemParametersInfoA, ClassicAdjustWindowRectEx, GetThemeParseErrorInfo, IsThemeClassDefined, DllCanUnloadNow,
DllGetActivationFactory, DllGetClassObject, DrawThemeIcon, CreateThemeDataFromObjects, Process_LoadTheme_RunDLLW,
AddThemeAppCompatFlags, ResetThemeAppCompatFlags, GetThemeClass, ClassicWndProc, IsValidShakeWindow, most immersive-color
internals (#94, 97, 100, 102, 105, 107-113, 117, 124, 127, 130), GetBufferedPaintTargetRect, GetThemeFilename, GetThemeIntList,
GetThemeRect, GetThemeStream, GetThemeString, GetThemeSysBool, GetThemeSysColorBrush, GetThemeSysInt, GetThemeSysSize,
GetThemeSysString, HitTestThemeBackground, IsThemeDialogTextureEnabled, SetThemeAppProperties, **ThemeInitApiHook**
(the last is called by user32, see 1.4). The other 34 exports are `SHUNIMPL` forwarders. 107 + 52 + 34 = 193.

### 1.3 Win10 ordinal names (differ from Wine in several places; Industrial7's older table is Win7-derived)

From `/api/resolve?ref=uxtheme!<rva>` (PDB names) and decomp headers [V-api]. Ordinals comctl32 imports:

| ord | Win10 name | rva | evidence |
|---|---|---|---|
| 47 | DrawThemeBackgroundEx (named export) | 0x5200 | export table |
| 49 | OpenNcThemeData | 0xbb20 | decomp: `IsHighContrastMode()` gate then `OpenNcThemeDataInternal(hwnd, class)` |
| 72 | DrawThemeIconEx | 0x4f560 | decomp header; args (htheme, hdc, part, state, rect, himl, index, params) |
| 75 | ThemeForwardCapturedMouseMessage | 0x4ba40 | decomp: `CThemeWnd::FromHwndAddRef` ... returns 1 if no window object |
| 131 | GetBufferedPaintBitsEx | 0x1c9d0 | decomp header (4 args); comctl32 calls it from `_UpdateTextAlpha` 0x1e0810 |

Other ordinals other modules import (resolved names): 1 QueryThemeServices, 7 GetThemeDefaults, 8 EnumThemes, 9 EnumThemeColors,
10 EnumThemeSizes, 33 Remote_ReestablishServerConnection, 43 ClassicGetSystemMetrics, 45 ClassicSystemParametersInfoW,
65 SetSystemVisualStyle, 69 EnumThemeProperties, 73 IsThemeActiveByPolicy, 76 EnableServiceConnection, 84 ClearTheme,
90 SetSystemMetrics, 91 SendAcquireOrAddTheme, 92 LoaderLoadTheme, 93 ApplyThemeSections, 95 GetImmersiveColorFromColorSetEx,
96 GetImmersiveColorTypeFromName, 98 GetImmersiveUserColorSetPreference, 99 InitializeImmersiveColorSetSharedMemory,
101 SetImmersiveColorSetSharedMemoryACLforUser, 104 RefreshImmersiveColorPolicyState, 106 GetIsImmersiveColorUsingHighContrast,
115 InvalidateAppTheme, 116 SendReleaseTheme, 118 GetColorPreferencePreset, 119 GetDefaultColorPreference, 120 GetUserColorPreference,
121 GetColorFromPreference, 122 SetUserColorPreference, 123 SetImmersiveColorPreferenceInSharedMemory, 125 GetGroupPolicyColors,
126 DrawTextWithGlow, 128 ClearDpiInfo, 132 ShouldAppsUseDarkMode, 133 AllowDarkModeForWindow, 135 SetPreferredAppMode,
136 FlushMenuThemes, 137 IsDarkModeAllowedForWindow, 138 ShouldSystemUseDarkMode, 139 IsDarkModeAllowedForApp.

Wine cross-check [V-src `dlls/uxtheme/uxtheme.spec`; fact-check re-extracted: 1 2 3 4 7 8 9 10 11 13-36 43-49 60 61 62 63 95 96 98 100 104 132 133 135 136 137 138]: Wine exports only those ordinals (note: no 12, 134 either). **Wine has no ordinal 72, 75, 106, 119-123, 125, 126, 128, 131, 139**, and its ordinal 49 is an unnamed stub
(`49 stub @`). So a stock Win10 comctl32 6.10.19041 bound to Wine's uxtheme loses DrawThemeIconEx, GetBufferedPaintBitsEx and
ThemeForwardCapturedMouseMessage (the comctl32 delay-loader would fail on first use); a real Win10 shell stack additionally
needs 120/121 (25 and 23 importers).

### 1.4 How user32 chrome gets themed (no uxtheme import)

* user32 exports `RegisterUserApiHook` (0x2c5b0) and has `InitUserApiHook(hinst, fn)` (0x22ce0). Its only caller besides itself is
  `__ClientLoadLibrary` [V-api `/callers`]. `InitUserApiHook` calls the hook DLL's init (which is
  `uxtheme!ThemeInitApiHook`, ordinal 193, 0xcda0) with a `tagUSERAPIHOOK` and copies back a table of replacement entry points:
  `DefWindowProcA/W`, `GetScrollInfo`, `SetScrollInfo`, `EnableScrollBar`, `AdjustWindowRectEx`, `SetWindowRgn`,
  pre-dispatch message masks for Dwp/Wnd/Dlg, `GetSystemMetrics(ForDpi)`, `SystemParametersInfoA/W/ForDpi`, `DrawFrameControl`,
  `DrawCaption`, `MDIRedrawFrame` [V-api decomp of user32 0x22ce0; the field names are the `PTR_Real*` globals it stores].
* uxtheme side: `ThemeInitApiHook` 0xcda0 returns 0 and records `g_fEarlyHookRequest = 1` if uxtheme is not yet initialized,
  otherwise (no debugger, hooks on) it copies user32's table into its own globals and installs its hooks [V-api decomp 0xcda0].
* The themed window parts that hook produces are uxtheme-internal: `_LoadNcThemeMetrics` 0x1f37c opens class `"Window"` via
  `OpenNcThemeDataInternal` and reads `GetThemeMargins(Window, parts 1,5,3,2, state 1, TMT_CAPTIONMARGINS 3603)`,
  `IsThemePartDefined(Window, 0x1e..0x25)` (sizing templates 30-37), `GetThemePartSize(Window, 0x12=WP_CLOSEBUTTON, 0)` and
  `(0x13=WP_SMALLCLOSEBUTTON, 0)`, with classic SM_CXSIZE etc. fallbacks when the part size call fails; also opens
  `"CompositedWindow::Window"` for `GetThemeColor(0,0,0x979)`. `_GetNcThemeMetrics`/ChangeTheme open `"Tab"` and `"Window"`;
  `CThemeMenuMetrics` (`Initialize` 0x22cbc) opens class `"MENU"` and reads `GetThemePartSize(parts 0xb, 0xf, 0x10)`,
  `GetThemeColor(parts 8, 9, 0xe, TMT 0x963)`, `GetThemeMargins(parts 8, 0xb, 0xc, 0xe, 0x10, TMT_CONTENTMARGINS)`.
  Drawing uses `NcPaintCaption` 0x20f0c (Window parts from `_NCTHEMEMET`; close/min/max button parts = 0x12 + state
  groups of 4), `TrackFrameButton` 0x24ee4 (`DrawThemeBackgroundEx`), `DrawClientArea` 0x3940 (MENU part 7).
  [V-api decomps; part names from `vsstyle.h` WINDOWPARTS/MENUPARTS.]
* Consequence: to theme Win10 scrollbars, menus and caption exactly as stock, an owned uxtheme must ALSO implement this
  hook, not just the getters. comctl32 v6 additionally carries its own flat/themed `ScrollBar` implementation (section 3).

---------------------------------------------------------------------------------------------------------------------

## 2. The comctl32 v6 theme layer (shared plumbing)

* **OpenTheme wrappers.** `ResetThemeData(hwnd, HTHEME* slot, LPCWSTR cls)` 0x86118 = close old handle, `OpenThemeData(hwnd, cls)`,
  store [V-api decomp]. `Open` 0x5eec0 chooses `OpenThemeData` or `OpenThemeDataForDpi(hwnd, cls, dpi)` (when a DPI is given; also used by
  `CreateCheckBoxImagelistEx` 0xf3e58 with hwnd NULL). A control that gets `NULL` simply keeps `hTheme == 0` and every drawing/measuring
  path tests `hTheme != 0` first (the "effect when theme absent" column), with the exceptions listed in 2.3.
* **Class strings opened by comctl32** (decomp string literals at OpenThemeData/ResetThemeData sites, counted) [V-cmd]:
  Combobox 7+1 ("ComboBox" in ComboEx), Link 5, Tooltip 4, MonthCal 4, TreeView 3, Edit 3, Button 3 (+ table, 2.2), TrackBar 2,
  Toolbar 2, Status 2, Static 2, Spin 2, ScrollBar 2 (one via ordinal 49 `OpenNcThemeData`), Listbox 2, DatePicker 2, and one each:
  Tab, Rebar, ListView, Header, EmptyMarkup, AeroWizard, DWMWindow, TryHarder, Progress / Indeterminate::Progress (chosen in
  `Progress_ResetThemeData` 0x32374 by style bit 8).
* **Per-control SetWindowTheme use** (11 sites) is for sub-class strings: ComboBox -> `L"Combobox"` (0x10894, 0x44fa8), tree/list label
  edit -> `L"EditComposited"` (0x1a750, 0x11b18c), ToolTip/Toolbar/Rebar WndProc forwarding of a WM_SETTHEME-like message with a
  caller-supplied sub-app (0x661a0, 0x67390, 0x710e0), ComboEx (0x42200).
* **Call-site tallies in comctl32** (statements in decomps, approximate; includes DirectUI/TaskDialog code) [V-cmd]:
  DrawThemeBackground 99, OpenThemeData 39, GetThemePartSize 27, ResetThemeData 23, DrawThemeText 21, GetThemeColor 21,
  GetThemeTextExtent 17, GetThemeInt 13, BeginBufferedPaint 13, DrawThemeTextEx 12, SetWindowTheme 11, GetThemeMargins 10,
  GetThemeFont 10, IsThemePartDefined 9, GetThemeBool 9, IsThemeBackgroundPartiallyTransparent 8, GetThemeTransitionDuration 8,
  GetThemeTextMetrics 8, DrawThemeParentBackground 7, GetThemeBackgroundExtent 3, GetThemeBackgroundRegion 3, GetThemeEnumValue 2,
  GetThemeMetric 1 (generic, `ParseGTMetInt` 0x2d720: property id comes from markup), GetThemePosition 1 (header),
  GetThemePropertyOrigin 1, DrawThemeEdge 1, plus ordinals 47 (6 sites), 49 (2), 72 (2), 131 (1).

### 2.1 Property IDs seen at comctl32 GetTheme{Int,Bool,Color,Font,Margins,Position,EnumValue} sites (decimal, name per vssym32.h)

| id (hex) | name | used by (function, RVA) |
|---|---|---|
| 204 (0xcc) | TMT_COLOR | TrackBar `DrawTic` 0xecbcc (parts 9/10 TICS, state 1); TV_InitThemeMetrics 0x3aaf4 (TreeView part 3 BRANCH) |
| 210 (0xd2) | TMT_FONT | TV_OnSetFont 0x3ac94 (TreeView part 1,1); Edit_SetupThemeInfo 0x45dc0 (Edit part 1,8); GrowToolbar 0x64b10 (Toolbar 0,0); tooltip NewFont/RenderTitledTip |
| 809 (0x329), 808 (0x328) | TMT_BODYFONT / 808 (not in Wine's header: UNKNOWN name) | ListView group metrics `SetMetrics` 0x13c198 (part 6 GROUPHEADER) |
| 1201 (0x4b1) | TMT_SIZINGBORDERWIDTH | `CCDrawNonClientTheme` 0x4b5c4, `ThemeDrawEdge` 0xe8f40 (part 1 state N) |
| 2204 (0x89c) | TMT_COMPOSITED | Button_OnCreate 0x62810, InitializeCompositedDraw 0x84b34, ComboEx_CheckIsThemed 0x44cfc (part 0,0) |
| 2215 (0x8a7) | TMT_NOETCHEDEFFECT | Toolbar `DrawButton` 0x6bca0 (part 1, state 4) |
| 2219 (0x8ab) | TMT_COMPOSITEDOPAQUE | Button_OnCreate, InitializeCompositedDraw, ListView `OpenTheme` 0x85f2c |
| 2403 (0x963) | TMT_BORDERSIZE | Combobox `ComboBox_CalcControlRects` 0x101f4, `ComboBox_GetInfo` 0x56a1c, `ComboBox_SetEditItemHeight` 0x57380 (part 4 CP_BORDER, state 1; and part 0/0) |
| 2418 (0x972) | TMT_GLYPHINDEX | `EditSL_Create` 0x51e84 (Edit part 4 EP_PASSWORD, state 0) |
| 2426 / 2427 / 2428 (0x97a/0x97b/0x97c) | FRAMESPERSECOND / PIXELSPERFRAME / ANIMATIONDELAY | Progress `Progress_AnimateInitialize` 0x322cc, `Progress_PaintThemed` 0x11d914, `Progress_SetHighlightTimer` 0x11e120 (part 0, state 0) |
| 2601 (0xa29) | TMT_GLYPHFONT | EditSL_Create (EP_PASSWORD) |
| 3601 (0xe11) | TMT_SIZINGMARGINS | `TB_OnSize` 0x64260 (Toolbar 0,0); `Edit_ClientEdgePaint` 0x122004; 0xcc12a |
| 3602 (0xe12) | TMT_CONTENTMARGINS | Combobox 0x10cd0 (part 0,0); Edit_Size 0x136a8 (part 5,1); Rebar `_CacheThemeInfo` 0x8cfa4 (part 3,0); ListView `OpenTheme` (part 1,3); ListView `SetMetrics` (part 6,0); tooltip `UpdateTheme` 0x635b8 (parts 1/3) |
| 3802 / 3803 (0xeda/0xedb) | TMT_FILLCOLOR / TMT_TEXTCOLOR | Edit_GetBrush 0x40384 (1,0 TEXTCOLOR), cue-banner colors (Edit 1,8; Combobox CP_CUEBANNER 8,1), TV_InitThemeMetrics (part 0), Static OnPaint, tooltip, ComboEx |
| 5006 (0x138e) | TMT_ANIMATIONDURATION | Progress_AnimateInitialize / Progress_UpdateAnimation 0x32488 (part 0, state 0) |
| 3403 (0xd4b) | TMT_MINSIZE (POSITION) | `_Header_InitTheme` 0x85d98 (Header part 1, state 0) - **the header-height input**, see section 4 |
| 4005/4007/4009 (0xfa5/0xfa7/0xfa9) | HALIGN / VALIGN / ICONEFFECT | Progress_PaintThemedHighlight 0x11db14 (enum), `DrawThemeIconEx` 0x4f560 |

### 2.2 Dialog button class table

`Button_GetTheme` 0x62bfc [V-api]: `class = table[GetDlgCtrlID(hwnd)]` for ids 0..12, i.e. the class list passed to `OpenThemeData` is
(table at RVA 0x1f3410, strings read with `/api/bytes/comctl32`):
`Button` (id 0 and out of range), `Button-OK;Button` (1), `Button-CANCEL;Button` (2), `Button-ABORT;Button` (3),
`Button-RETRY;Button` (4), `Button-IGNORE;Button` (5), `Button-YES;Button` (6), `Button-NO;Button` (7), `Button-CLOSE;Button` (8),
`Button-HELP;Button` (9), `Button-TRYAGAIN;Button` (10), `Button-CONTINUE;Button` (11), `Button-APPLY;Button` (12) [fact-check: id 12 was missing; table re-read from the 13 pointers at RVA 0x1f3410 via `/api/bytes`; the `> 0xc` guard at 0x62bfc line 25 confirms 0..12]. It also calls
`EnableThemeDialogTexture(GetParent(hwnd), 2)` and reads `SystemParametersInfoW(0x42 = SPI_GETHIGHCONTRAST)` to set a high-contrast bit (0x10 at +0x58).
An armed uxtheme must therefore accept semicolon-separated class lists and resolve to the first defined class.

### 2.3 Hazards found while reading (matter for an "armed" implementation)

1. **Header sizes are read without checking the result.** `_Header_InitTheme` 0x85d98 calls
   `GetThemePartSize(h, 0, 1 /*HP_HEADERITEM*/, 0, 0, TS_TRUE, &local_28)` and discards the HRESULT; if it fails (e.g. a
   handle-valid theme that returns an error for the part), `local_28` is uninitialized stack and its `.cy` becomes the header height
   input (`*(HD+0x160) = local_28.cy`). Later `GetThemePosition(... 0xd4b ...)` is checked (`-1 < HVar2`) and only raises the value.
   The other `GetThemePartSize` calls (parts 5, 4, 7) also ignore the HRESULT, but write straight into HD fields.
   So: once `OpenThemeData("Header")` returns non-NULL, `GetThemePartSize` for HEADER parts 1/4/5/7 **must succeed and fill `*psz`**.
2. `OpenThemeData` non-NULL commits the control to the themed paths: e.g. `Tab_RefreshTheme` 0x8384 sets control flag 0x40
   and `+0x3c |= 2` as soon as the handle is non-NULL (`TC` struct), then every `VertDrawEdge` goes through `DrawThemeBackground`
   instead of `DrawEdge`; if `DrawThemeBackground` fails the tab simply draws nothing for that edge (0xa284 does not fall back).
   (Other draw sites such as `Header_DrawItem`, `TV_DrawItem`, `Button_DrawThemed`, `ThemeDrawEdge` do check `HVar < 0` and fall back to classic.)
3. Buffered paint is used by themed controls even when no theme is drawn (`BeginDoubleBuffer` 0x1ab44, `_PaintWorkArea` 0x75bf8,
   `_FillWithAlpha` 0x1680ec): BufferedPaintInit/UnInit/Begin/End are required independent of theme-on/off. They are already served
   by the Industrial7 stub (section 5).
4. `GetThemeSysSize/GetThemeSysInt/GetThemeSysBool` have **no comctl32 callers** (0 call sites) but `Industrial7` documents them; they are
   used by shell/dui (GetThemeSysFont: fvewiz, scansetting; GetThemeSysColor: explorerframe).

---------------------------------------------------------------------------------------------------------------------

## 3. Control-by-control table

Part/state names are from `vsstyle.h`. "T-off effect" = what the comctl32 code does when `OpenThemeData` returns NULL (read from the
cited decomp unless marked [INF]). RVAs are comctl32.dll!0x... unless noted.

| control | class string (opened at) | parts / states used (function) | theme functions needed | effect when theme absent |
|---|---|---|---|---|
| **Header** | `Header` (`_Header_InitTheme` 0x85d98, via ResetThemeData; from `Header_OnCreate` 0x85c50 and WM_THEMECHANGED) | root part 0 states 1/2 = HBG_DETAILS/HBG_ICON for the area right of the last item (`Header_Draw` 0x5009c); HP_HEADERITEM 1 with states 1..12 = HIS_NORMAL/HOT/PRESSED/SORTED*(4-6)/ICON*(7-12), picked from a 2x6 int table at RVA 0x2045c0 = {1,2,3,4,5,6 / 7..12} indexed by header-style word `HD+0x128` (details vs icon mode) (`Header_DrawItem` 0x5037c); HP_HEADERSORTARROW 4 states 1/2 (`_Header_DrawThemedSortArrow` 0x11a708); HP_HEADERDROPDOWN 5 and HP_HEADERDROPDOWNFILTER 6 via ordinal 47 `DrawThemeBackgroundEx` (state 1-3, `Header_DrawItem`); HP_HEADEROVERFLOW 7 states 1/2 (`Header_Draw`); item with state 7 or 8 on part 1 for the overflow background | `OpenThemeData`, `GetThemePartSize(1,0,TS_TRUE)`, `GetThemePosition(1,0,TMT_MINSIZE)`, `GetThemePartSize(5/4/7,0,TS_TRUE)`, `GetThemeTextExtent(1,0,"M",DT_NOPREFIX)` (`Header_NewFont` 0x86174), `GetThemeBackgroundContentRect(1,state)`, `GetThemeBackgroundExtent(1,0)` (`Header_ComputeItemWidth` 0x98a0c), `DrawThemeBackground(Ex)`, `DrawThemeText` via `SHThemeDrawText` 0x8a07c | classic: item height = text height + 2*edge; items drawn with `Header_DrawButtonEdges` (DrawEdge) and `GetSysColor` fills; sort arrow via `DrawScrollArrow`/Marlett font; no dropdown part sizes (`HD+0x114/0x118` = SM_CXVSCROLL-based defaults from `Header_OnCreate`). See section 4 for the derivation of 24 vs 20 |
| **Button / Check / Radio / Group / CommandLink** | `Button`, or `Button-OK;Button` ... per dialog id (`Button_GetTheme` 0x62bfc; also `Button` in ComboEx/DatePicker/NormalizeDUIButtonWidths 0xfcd20) | BP_PUSHBUTTON 1, BP_RADIOBUTTON 2, BP_CHECKBOX 3 (checked states +4/+8..), BP_GROUPBOX 4, BP_COMMANDLINK 6 (`GetThemeTextMetrics(6,0)`), BP_COMMANDLINKGLYPH 7 (`GetThemePartSize(7,state,TS_DRAW)`) (`DrawContent` 0x41b40); part/state computed by caller and passed to `Button_DrawThemed` 0x4319c / `Paint` 0x41750; check box glyph size `GetCheckBoxSize` 0x44734 = `GetThemePartSize(part=(type+2),state 1,TS_DRAW)`; ListView check-box imagelist `CreateCheckBoxImagelistEx` 0xf3e58 draws states 1,5,9,0xd,0x11 via ordinal 47 with `OpenThemeDataForDpi(0,"Button",dpi)` | `OpenThemeData`, `GetThemeBool(COMPOSITED 2204, COMPOSITEDOPAQUE 2219)`, `GetThemePartSize(TS_DRAW)`, `GetThemeTextExtent`, `GetThemeBackgroundExtent`, `GetThemeTextMetrics`, `DrawThemeBackground`, `DrawThemeText(Ex)` (DT flags 0x8010/0x820 etc.), `DrawThemeParentBackground` (`Button_PaintImpl` 0x43e10), `GetThemeTransitionDuration(6000)` (`Button_SoftFadeDuration` 0x445ec), `EnableThemeDialogTexture(parent,2)`, `IsThemeBackgroundPartiallyTransparent` | classic `DrawFrameControl`/`DrawEdge` rendering with fixed metric-derived glyph sizes (the "ideal size" path `Button_CalculateIdealSize` 0x413d8 uses GDI text metrics only); no soft-fade transitions |
| **Tab (SysTabControl32)** | `Tab` (`Tab_RefreshTheme` 0x8384; skipped when style bits 0x2180 set) | `Tab_Paint` 0x95f4 sets part in {1 TABITEM, 2 LEFTEDGE, 3/4 RIGHT/BOTH edge, 5-8 TOPTABITEM*, 9 TABP_PANE}, state {1 NORMAL, 2 HOT, 3 SELECTED} into `TC+0xf0/0xf4`; every edge goes `Tab_DrawEdge` -> `VertDrawEdge` 0xa284 -> `DrawThemeBackground(h, part, state)`; text via `Tab_DrawTextEx` 0xeb6ac / `Tab_ExtTextOut` 0xa3b8 (`DrawThemeText`), extents `Tab_GetTextExtentPoint` 0x56040 (`GetThemeTextExtent`), font metrics `Tab_GetTabFontMetrics` 0x578e0 (`GetThemeTextMetrics(part,state)`) | `OpenThemeData`, `DrawThemeBackground`, `DrawThemeText`, `GetThemeTextExtent`, `GetThemeTextMetrics` | `DrawEdge(BDR_* flags)` with 3D face colors; padding `GetScaledEdgeWidth*3` and `GetScaledEdgeHeight*3/2` (0x8384 sets `+0x8c/+0x90` from those) |
| **ListView** | `ListView` (`OpenTheme` 0x85f2c, DUI-style `CLVThemesManager`); also `Link`, `EmptyMarkup` for group/empty-text markup; header child uses `Header`; Explorer look needs `SetWindowTheme(lv,"Explorer",NULL)` by the app | LVP_LISTITEM 1 states 1 NORMAL/2 HOT/3 SELECTED/5 SELECTEDNOTFOCUS/6 HOTSELECTED (`DrawCustomTheme` 0x913b8, per `crates/w10-comctl32-lv/src/lv/paint_plan.rs:4`); `IsThemePartDefined(LISTITEM,0)` as gate; `GetThemeMargins(1,3,CONTENTMARGINS)` and `GetThemePartSize(LVP_COLUMNDETAIL 10,0,TS_TRUE)` in `OpenTheme`; group view (`SetMetrics` 0x13c198): `GetThemeMargins(6 GROUPHEADER,0)`, `GetThemePartSize(7 GROUPHEADERLINE,1,TS_TRUE)`, `(9 COLLAPSEBUTTON,1,TS_DRAW)`, `GetThemeFont(6,0,210/808/809)`; paint `_PaintHeader` 0x13d5c0 (parts 6, 7), `_PaintCollapse` 0x13d120 (parts 8/9 by expanded bit); `GetThemePropertyOrigin(6,..)`; `GetThemeBackgroundExtent(1,1)` in `GetContentSize` 0x1692e0; `DrawThemedColumnEdge` 0x3c5cc (part 10 COLUMNDETAIL) | OpenThemeData, IsThemePartDefined, GetThemeBool(COMPOSITEDOPAQUE), GetThemeMargins, GetThemePartSize, GetThemeFont, GetThemePropertyOrigin, GetThemeBackgroundExtent, DrawThemeBackground, DrawThemeTextEx | classic list: highlight fills via `GetSysColor`, group rendering falls back to classic group header; Explorer-style selection colors must come from Industrial7's own palette (`EXPLORER_SEL_*` in `paint_plan.rs`) |
| **TreeView** | `TreeView` + `Tooltip` (`TV_OnCreate` 0x3aef4, `TV_WndProc` 0x16d40 WM_THEMECHANGED) | TVP_TREEITEM 1 (`DrawThemeBackground(1,state)` `TV_DrawItem` 0x1576c; `DrawThemeTextEx(1,state, DT 0xc00/(flags&0x80|2)<<10)`, `TV_GetItemTextWidth` 0x18660 (1,3)), TVP_GLYPH 2 (`GetThemePartSize(2,2,TS_DRAW)` `TV_GetRealPlusSize` 0x15654; `DrawThemeBackground(2,state 1/2)` `TV_RealDrawPlusMinus` 0x54b30; `IsThemeBackgroundPartiallyTransparent(2,1)` `TV_HasTransparentImage` 0x18b8c), TVP_BRANCH 3 (`GetThemeColor(3,0,204)` `TV_InitThemeMetrics` 0x3aaf4), `IsThemePartDefined(1,0)`, `(4 HOTGLYPH,0)`, `GetThemeColor(0,0,3802)`, `GetThemeFont(1,1,210)` (`TV_OnSetFont`); expand fade uses BeginBufferedPaint (`TV_ExpandoFadeCreateDCs` 0x5497c) | OpenThemeData, GetThemePartSize, GetThemeColor, GetThemeFont, IsThemePartDefined, IsThemeBackgroundPartiallyTransparent, DrawThemeBackground, DrawThemeTextEx, BeginBufferedPaint | classic +/- bitmaps with fixed size, `GetSysColor` selection |
| **Toolbar** | `Toolbar` (`TB_CreateToolbar` 0x848c8, `TB_UpdateTheme` 0x63898, `TBUpdateTheme` 0xecf68) | background part 0 state 0 (`TB_DrawBackground` 0x757b0) / part 6 vertical separator (`DrawToolbarV` 0xf906c); buttons `DrawButton` 0x6bca0: TP_BUTTON 1 / DROPDOWNBUTTON 2 / SPLITBUTTON 3 / SPLITBUTTONDROPDOWN 4 / TP_DROPDOWNBUTTONGLYPH 7 with TS_NORMAL..TS_OTHERSIDEHOT; separators 5/6 (`DrawToolbarH` 0x6b638); `GetThemeBool(1,4,NOETCHEDEFFECT)`, `GetThemeColor(0,0,3803)`; metrics `TB_OnSize` 0x64260 `GetThemeMargins(0,0,SIZINGMARGINS)`, `TBRecalc` 0x6473c `GetThemeTextMetrics(0,0)`, `GrowToolbar` 0x64b10 `GetThemeFont(0,0,210)`, `_SoftFadeDuration` 0x64464 | OpenThemeData, GetThemeMargins, GetThemeTextMetrics, GetThemeFont, GetThemeBool, GetThemeColor, GetThemeTransitionDuration, DrawThemeBackground, DrawThemeParentBackground | classic flat/3D button via `DrawEdge`/ `DrawFrameControl`; button height from GDI font metrics only |
| **Rebar** | `Rebar` (`_CacheThemeInfo` 0x8cfa4) | RP_GRIPPER 1 (vert: +1... via `(style&0x80)+1`) and RP_SPLITTER 7/8 sizes `GetThemePartSize(.,0,TS_TRUE)`; RP_BAND 3 (`GetThemeMargins(3,0,CONTENTMARGINS)`, `_DrawBand` 0x71e50 part 3), RP_BACKGROUND 6 (`_EraseBkgnd` 0x72da0 with `IsThemeBackgroundPartiallyTransparent(6,0)` + `DrawThemeParentBackground`), RP_GRIPPER draw `_DrawGripper` 0x111320, RP_SPLITTER draw `_DrawSplitter` 0x111420, RP_CHEVRON 4/5 `DrawChevron` 0x112b58; text `GetThemeTextMetrics(0,0)` / `GetThemeTextExtent(0,0,flag 0x400)` | OpenThemeData, GetThemeMargins, GetThemePartSize, GetThemeTextMetrics, GetThemeTextExtent, DrawThemeBackground, IsThemeBackgroundPartiallyTransparent, DrawThemeParentBackground | classic gripper/edges; band sizes from fixed edge multiples |
| **Progress** | `Progress` or `Indeterminate::Progress` (`Progress_ResetThemeData` 0x32374; style bit 8) | PP_BAR 1 / PP_BARVERT 2 background via `uVar2 + 0xb` = PP_TRANSPARENTBAR 11 / VERT 12 (`Progress_PaintThemed` 0x11d914, `IsThemeBackgroundPartiallyTransparent`), PP_FILL 5 / VERT 6 (state from the PBM state), PP_PULSEOVERLAY 7 / MOVEOVERLAY 8 and verticals 9/10 (`Progress_PaintThemedHighlight` 0x11db14: `GetThemePartSize(.,.,TS_TRUE)` + `GetThemeEnumValue(HALIGN 4005 / VALIGN 4007)` + BeginBufferedPaint(COMPATIBLEBITMAP)); animation props `GetThemeInt(0,0,2426/5006/2427/2428)` | OpenThemeData, DrawThemeBackground, GetThemePartSize, GetThemeEnumValue, GetThemeInt, IsThemeBackgroundPartiallyTransparent, BeginBufferedPaint | classic block-chunk bar (`PBS_SMOOTH` etc.); no highlight/pulse animation |
| **TrackBar** | `TrackBar` (`TrackOnCreate` 0xaf10, `TBUpdateTheme` 0xecf68) | thumb `GetThumbThemePart/State` -> TKP_THUMB 3 / THUMBBOTTOM 4 / THUMBTOP 5 / THUMBVERT 6 / THUMBLEFT 7 / THUMBRIGHT 8, `TBResize` 0xb0c0 `GetThemePartSize(thumb part,state,TS_TRUE)` -> thumb cx,cy (clamped to `SM_CXEDGE*2 <= len < cy`); tics `GetThemeColor(9/10 TICS,1,TMT_COLOR 204)` (`DrawTic` 0xecbcc); `DrawChannel` 0x9fe8, `DrawThumb` 0xa080 via `VertDrawEdge` and `VertPatBlt` 0xed0b0 | OpenThemeData, GetThemePartSize (TS_TRUE), GetThemeColor, DrawThemeBackground | thumb size = `SM_CXVSCROLL(3)` scaled: `((GetSystemMetricsForDpi(3,dpi)<<2)/3)` rounded, then `ValidateThumbHeight`, and `len - 3*SM_CYEDGE` (x3/4 for TBS_NOTHUMB-ish bit 0x20) (0xb0c0, read) |
| **UpDown (msctls_updown32)** | `Spin` (`UpDownWndProc` 0x452c0; `UpdateTheme` 0xe9168) + buddy `Edit` (`SetBuddy` 0x45714) | SPNP_UP 1 / DOWN 2 / UPHORZ 3 / DOWNHORZ 4 via `_DrawButton` 0x8fe48 (iPartId, iStateId passed in) | OpenThemeData, DrawThemeBackground | classic `DrawFrameControl(DFC_SCROLL)` arrows |
| **Edit** | `Edit` (`Edit_NcCreate` 0x45bb4, `Edit_UpdateTheme` 0x45a78, `Edit_SetupThemeInfo` 0x45dc0) | EP_BACKGROUNDWITHBORDER 5 / EP_BACKGROUND 3 with states EPSN_* (`EditSL_DrawBackground` 0x40248, `Edit_ClientEdgePaintImpl` 0x122474, border parts EP_EDITBORDER_* 6-9), `IsThemePartDefined(5,0)`, `(3,0)`, EP_EDITTEXT 1 text (`EditSL_LpkEditDrawText` 0x40088), EP_PASSWORD 4 glyph font/index (`EditSL_Create` 0x51e84: `GetThemeFont(4,0,GLYPHFONT)`, `GetThemeInt(4,0,GLYPHINDEX)`), cue banner `GetThemeFont(1,8,FONT)` + `GetThemeColor(1,8,TEXTCOLOR)`, `Edit_Size` 0x136a8 `GetThemeMargins(5,1,CONTENTMARGINS)` | OpenThemeData, IsThemePartDefined, GetThemeMargins, GetThemeFont, GetThemeColor, GetThemeInt, DrawThemeBackground, DrawThemeTextEx, IsThemeBackgroundPartiallyTransparent, GetThemeTransitionDuration | classic sunken client edge (WS_EX_CLIENTEDGE via `DrawEdge`), margins from `GetSystemMetrics` |
| **ComboBox / ComboBoxEx** | `Combobox` (`ComboBox_SetupThemeInfo` 0x10cd0, `ComboBox_CreateHandler` 0x10894 uses SetWindowTheme), `ComboBox` in `ComboEx_CheckIsThemed` 0x44cfc | CP_DROPDOWNBUTTON 1 / CP_BORDER 4 / CP_READONLY 5 / CP_BACKGROUND 2 / CP_TRANSPARENTBACKGROUND 3 / CP_CUEBANNER 8 / CP_DROPDOWNITEM 9 (`ComboBox_PaintImpl` 0xc2cc: state from style bits, `IsThemeBackgroundPartiallyTransparent`); `IsThemePartDefined(3,0)`; `GetThemeMargins(0,0,CONTENTMARGINS)`; `GetThemeInt(4,1,BORDERSIZE)` and `(0,0,BORDERSIZE)` for the edit-field inset (`ComboBox_CalcControlRects` 0x101f4, `ComboBox_GetInfo` 0x56a1c); `GetThemeBool(0,0,COMPOSITED)`; text `DrawThemeText(.. state ((flags>>?)&3)+1 ..)` `ComboBox_InternalUpdateEditWindow` 0x8ad0; `ComboEx_OnDrawItem` 0x106468 `GetThemeColor(9,state,TEXT/FILL)` | OpenThemeData, IsThemePartDefined, GetThemeMargins, GetThemeInt, GetThemeBool, GetThemeColor, DrawThemeBackground, DrawThemeText, GetThemeTransitionDuration, IsThemeBackgroundPartiallyTransparent | classic button (DFCS_SCROLLCOMBOBOX) and 3D edge, fixed 2*SM_CYEDGE insets |
| **Listbox** | `Listbox` (`ListBox_Create` 0xd104, `ListBox_UpdateTheme` 0xd0c8) | border parts LBCP_BORDER_* 1-4 and LBCP_ITEM 5 [INF: only the open call is a theme call in the decomp; drawing is via Edit-style border routine] | OpenThemeData (+ DrawThemeBackground [INF]) | classic client edge |
| **Status bar** | `Status` (`InitStatusWnd` 0x38f9c, `StatusUpdateTheme` 0xe7c84) | SP_PANE 1 / SP_GRIPPERPANE 2 state `(sunken!=0)+1` (`DrawStatusTextEx` 0x3a1b4), SP_GRIPPER 3 (`PaintStatusWnd` 0x39ed4 state 1; size `Status_ComputeGripperSize` 0x3a688 = `GetThemePartSize(3,0,TS_DRAW)` initialised from `GetGripSize`), `GetThemeTextMetrics(0,0)` -> bar height = `tmHeight + tmInternalLeading` (+2*SM_CYBORDER if internal leading 0) (`GetNewMetrics` 0x3a894), `GetThemeTextExtent/DrawThemeText(0,0, flags 0x420/0x820)` | OpenThemeData, GetThemePartSize, GetThemeTextMetrics, GetThemeTextExtent, DrawThemeText, DrawThemeBackground | `GetTextMetricsW` fallback inside GetNewMetrics (read), `GetGripSize` gripper, classic `DrawEdge` panes |
| **Tooltip** | `Tooltip` (`LoadTheme` 0x6312c, `v_WndProc` 0x66fe0, TV_OnCreate), `Link` for markup | TTP_STANDARD 1, TTP_STANDARDTITLE 2, TTP_BALLOON 3, TTP_BALLOONTITLE 4, TTP_CLOSE 5 (`CalculateSize` 0x112fc4 `GetThemePartSize(5,1,TS_DRAW)`), TTP_BALLOONSTEM 6 (`CalculateBalloonStemRect` 0x112ee8 `GetThemePartSize(6,state,TS_TRUE)`), TTP_WRENCH 7 (0x113050 `(7,1,TS_DRAW)`); `DrawTipBackground` 0x4d38c (3 and part/state), `CreateToolTipRgn` 0x4e2f8 / `CreateBalloonRgn` 0x11334c use `GetThemeBackgroundRegion(1/3/6)`; text colors/fonts `RenderTitledTip` 0x4d2f4 (`GetThemeColor`, `GetThemeFont`, `GetThemeTextMetrics` for parts 2/4); `UpdateTheme` 0x635b8 `GetThemeColor`, `GetThemeMargins(1/3,0,CONTENTMARGINS)`; `NewFont` 0x63240 `GetThemeFont` | OpenThemeData, GetThemePartSize, GetThemeBackgroundRegion, GetThemeColor, GetThemeFont, GetThemeMargins, GetThemeTextMetrics, DrawThemeBackground | classic yellow tip / `GetSysColor(COLOR_INFOBK)`, rectangular or non-themed balloon region |
| **MonthCal / DatePicker** | `MonthCal`, `DatePicker`, `Button`, `Combobox` (`_CreateHandler` 0x143d40, `UpdateTheme` 0x142bb8, `_CreateHandler` 0x14995c) | DatePicker DP_DATETEXT 1 (`_DrawSubedits` 0x1441f8, `_CalcGetTextExtent` 0x143b94), DP_DATEBORDER 2 (`_Paint` 0x146ca8), DP_SHOWCALENDARBUTTONRIGHT 3 (button drawn with `Button` class part 3?) ; MonthCal parts 1..10 (`_PaintBackground` 0x14f504 parts 1,2; `_PaintCalendar` 0x14f7b4 parts 3,4,5; `_PaintCalHeader` 0x14f600 part 6; `_PaintArrowBtn` 0x14f380; `_DrawTodayCircle` 0x14a5c0 (5,state 5/6); `_CalcOnStyleChange` 0x148a90 `GetThemePartSize(10,0,TS_TRUE)`) | OpenThemeData, GetThemePartSize, GetThemeTextExtent, DrawThemeBackground, DrawThemeText | classic flat calendar |
| **Static** | `Static` (`WndProc` 0x826c0, `UpdateTheme` 0x121b5c) | `GetThemeColor(0,0,TEXTCOLOR/FILLCOLOR)` only (`OnPaint` 0x82e68) | OpenThemeData, GetThemeColor | `GetSysColor` text colors |
| **Link (SysLink)** | `Link` (`WndProc` 0x53de0; ResetThemeData on WM_THEMECHANGED) | `DrawThemeParentBackground` in `Paint` 0x54180; link colors via `GetThemeColor` [INF] | OpenThemeData, DrawThemeParentBackground | `COLOR_*` defaults |
| **Hotkey / IPAddress** | `Combobox` (`HotKeyWndProc` 0xed470, `IPAddressWndFn` 0xee740; `HKUpdateTheme`, `IPAddress_UpdateTheme`) | only themes the client edge (Combobox border) [INF] | OpenThemeData | classic client edge |
| **TaskDialog / DirectUI (markup)** | generic: `ParseGTPartSize` 0x5ed80 / `ParseGTMetInt` 0x2d720 / `ParseGTMarRect` 0x2aa90 / `ParseGTFStr` 0x2b120 / `ParseGTCColor` 0x2b420, class from XML (`GetThemeMetric`, `GetThemePartSize`, `GetThemeMargins`, `GetThemeFont`, `GetThemeColor` with parts from the markup); `PaintStringContent` 0x2e8a8 (BeginBufferedPaint + `DrawThemeTextEx(TEXT_* parts)`); wizard `AeroWizard` (`UpdateTitleBar` 0x158a80 `GetThemeColor(2,0,FILLCOLOR)`), `DWMWindow` (`UpdateWizardTitleColorMaxActive` 0x158c3c `GetThemeColor(0x2e,3,TEXTCOLOR)`); `TryHarder` (class opened with NULL hwnd by 0x1d9afc, a degenerate "try again" button) | parts/states are data-driven from the dialog markup, not from code | all of the Get* family, driven by markup | markup falls back to GDI text with default sizes [INF] |
| **ScrollBar (flat/themed)** | `ScrollBar` (`Attach` 0x73460 via `OpenNcThemeData`=ordinal 49; `ChangeTheme` 0x2708 also ordinal 49) | SBP_ARROWBTN 1 states (`xxxDrawSB2` 0x74730 uses `GetThemeTransitionDuration`), SBP_THUMBBTNHORZ/VERT 2/3, track parts via `iVar6`, SBP_GRIPPERHORZ/VERT 8/9 (`DrawThumb2` 0x73840: `GetThemePartSize((param!=0)+8,state,TS_TRUE)` = gripper size), SBP_LOWER/UPPERTRACK*, `InvertScrollHilite` 0xf0948 (parts 4/5), `zzzDrawInvertScrollArea` 0xf1c48 (part 1), size box SBP_SIZEBOX 10 (`DrawSizeBoxEx` 0x8f80c, `GetThemeColor(0xb,0,FILLCOLOR)`) | OpenNcThemeData (#49), DrawThemeBackground, GetThemePartSize, GetThemeTransitionDuration | non-theme scrollbars are user32's (themed via the UserApiHook); comctl32 `ScrollBar` object is only used when `OpenNcThemeData` succeeds [INF] |
| **user32 non-client / scrollbar / menu / MDI** | not by comctl32. `Window`, `MENU`, `Tab`, `CompositedWindow::Window` opened by uxtheme itself (section 1.4) | WP_CAPTION 1 / SMALLCAPTION 2 / MINCAPTION 3 / MAXCAPTION 5 etc., WP_CLOSEBUTTON 18 (size read), MENU_BARITEM 8 ... | `GetThemeMargins`, `GetThemePartSize`, `GetThemeColor`, `DrawThemeBackground(Ex)`, UserApiHook installation | classic caption/frame/menu (stock user32 drawing; this is Industrial7's current mode: docs/USER32.md:273 "classic drawing only, kept exact") |
| **Misc common** | `CCDrawNonClientTheme` 0x4b5c4 / `ThemeDrawEdge` 0xe8f40 / `CCThemeDrawEdge` 0x585f0 | generic edge helper: `GetThemeInt(h,part,state,SIZINGBORDERWIDTH 1201)` then `DrawThemeBackground`; `DrawThemeEdge(h,dc,3,0,rc,6,flags)` | GetThemeInt, DrawThemeBackground, DrawThemeEdge | `DrawEdge` |

Notes on rows marked [INF]: the decomp showed only the open/get call, not the drawing code, because the draw path goes through a
helper whose theme call was not in the grep set (e.g. Listbox paints its border via the Edit-shared helper). They are low risk for the
header work but should be re-read before the Listbox and Link rows are used as probe inputs.

---------------------------------------------------------------------------------------------------------------------

## 4. CC30: header 24 px vs 20 px - the exact derivation

Everything below is the Win10 comctl32 6.10.19041.1110 decomp, read with `curl $B/api/function/comctl32/<rva>/decomp`.
`HD` is the header's per-instance struct; offsets are bytes.

**4.1 Layout height (the number a ListView/parent applies).** `Header_OnLayout` 0x4b2a8 (`HDM_LAYOUT`):

```
edge   = DPISCALEINFO::GetLegacyScaledEdgeHeight(HD+0x34)      // = MulDiv(GetSystemMetrics(0x2e=SM_CYEDGE), dpiY, 96) [V-api decomp 0x4b384]; 2 at 96 dpi with stock metrics
height = HD[+0x4c] + 2*edge                                     // HD+0x4c = text height (4.2)
if (HD[+0x130] != 0 && height <= HD[+0x160])  height = HD[+0x160]   // themed AND theme minimum >= computed -> theme wins
if (style & HDS_FILTERBAR /*0x100*/)  height += HD[+0x4c] + 1 + 2*edge
WINDOWPOS.cy = (style & HDS_HIDDEN /*8*/) ? 0 : height         // flags = 0x14 (SWP_NOZORDER|SWP_NOACTIVATE)
```

`HD+0x130` is the HTHEME slot: **the minimum only applies when a theme handle exists** - this is the gap the owner described.
`HD+0x160` is the theme minimum height from 4.3.

**4.2 Text height `HD+0x4c`.** `Header_NewFont` 0x86174 (called from `Header_OnCreate` 0x85c50 before `_Header_InitTheme`, and on WM_SETFONT):

* a `GetDC(NULL)` is made and the new font (if any) selected;
* **if `HD+0x130 != 0`**: `GetThemeTextExtent(hTheme, hdc, part 1 (HP_HEADERITEM), state 0, L"M", -1, flags 0x800 (DT_NOPREFIX), &bound(0,0,0,0), &extent)` -> `cx/cy = extent`; on failure (`HRESULT < 0`) falls through;
* otherwise `GetTextExtentPointW(hdc, L"...", 3, &size)`;
* `HD+0x44 = size.cx; HD+0x4c = size.cy`. A Marlett font of height `((tmExternalLeading + GetSystemMetrics(6 = SM_CYBORDER) + tmHeight) & 0xfffe) - 1` is also created (classic sort arrows).

Note the order in `Header_OnCreate`: `Header_NewFont(hd, NULL)` runs **before** `_Header_InitTheme`, when `HD+0x130` is still 0 (the code
stores 0 there right after), so at creation the text height is the GDI `"..."` height. `WM_THEMECHANGED` (0x31a, `_Header_WndProc` 0x689b0 line 51)
only calls `_Header_InitTheme` (re-opens the theme, recomputes `+0x160`) and does **not** re-run `Header_NewFont`. The themed `"M"` measurement
therefore only happens on `WM_SETFONT` with a font different from `HD+0x58` (0x689b0 line ~228: `if (font == HD[+0x58]) return 0; Header_NewFont(hd, font)`), which is
what a ListView sends when it creates/updates its header [INF: the lv call was not traced]. [V-api decomps 0x85c50, 0x689b0.]

**4.3 Theme minimum `HD+0x160`.** `_Header_InitTheme` 0x85d98:

```
ResetThemeData(hwnd, &HD[+0x130], L"Header")
if (HD[+0x130] != 0) {
   GetThemePartSize(h, NULL, HP_HEADERITEM=1, state 0, NULL, TS_TRUE, &sz)   // HRESULT ignored
   if (GetThemePosition(h, 1, 0, TMT_MINSIZE=3403 (0xd4b), &pt) >= 0) { sz.cx = max(sz.cx, pt.x); sz.cy = max(sz.cy, pt.y); }
   HD[+0x160] = sz.cy                                                         // <- header minimum height
   GetThemePartSize(h, NULL, HP_HEADERDROPDOWN=5, 0, NULL, TS_TRUE, &sz); HD[+0x114] = max(HD[+0x114], sz.cx)   // dropdown width
   HD[+0x118] = GetSystemMetrics(0x2d = SM_CXEDGE)                                                              // dropdown pad
   GetThemePartSize(h, NULL, HP_HEADERSORTARROW=4, 0, NULL, TS_TRUE, &HD[+0x158])                                // sort arrow size
   GetThemePartSize(h, NULL, HP_HEADEROVERFLOW=7, 0, NULL, TS_TRUE, &HD[+0x144])                                 // overflow size
}
```

`TS_TRUE` for an image-file part: `CImageFile::GetPartSize` 0x14edc returns the per-state **image cell size**
(`DIBINFO+0xc/+0x10`) unless the true-size stretch info is present, in which case the stretched size; `TS_MIN` returns the sum of the
content margins (min 1); `TS_DRAW` returns `GetDrawnImageSize`. For border-fill parts `GetThemePartSize` (0x14a20) returns
`2*BORDERSIZE+1` for TS_TRUE and `2*BORDERSIZE` for TS_MIN [V-api decomp 0x14a20/0x14edc].

**4.4 The two numbers.**

* **20 (classic, Industrial7 today):** `HD[+0x130] == 0` -> `height = textcy + 2*edge`. With a 16 px font and a 2 px edge that is 20
  - this is exactly what Industrial7's `Header::layout` computes (`crates/w10-comctl32-lv/src/hdr/layout.rs:57-70` (fact-check: line range corrected; the code is `height = text_cy + 2*SM_CYEDGE; if theme.active && height <= theme.min_height { height = min_height }`, same shape as the decomp), test
  `tests/hdr_layout.rs:15-21` "layout.pos 0,0,400,20 ... 16 px font and 2 px edge").
* **24 (Win10 themed):** `HD[+0x130] != 0`, the text path gives `textcy + 2*edge` <= `HD[+0x160]`, so `height = HD[+0x160] =
  max(GetThemePartSize(HEADER, HP_HEADERITEM=1, 0, TS_TRUE).cy, GetThemePosition(HEADER, 1, 0, TMT_MINSIZE).y)`.
  The Win10 value of that max is **UNKNOWN** here: the inputs are theme data in `aero.msstyles` (Win10 default theme) and the
  decompile cannot give them. The owner's observation (24 on Win10) implies `HD+0x160 >= 24` unless the text arm alone gives
  >= 24 (a 15 px Segoe UI 9pt line + 4 = 19 makes that unlikely) [INF].
  Industrial7's model already encodes this: `Theme { min_height: 24, .. }` with the test comment "the Win7 VM recorded 24 for the same call"
  (`tests/hdr_layout.rs:51-55`) - but `Host::theme_info()` default returns `Theme::default()` (`src/hdr/mod.rs:382-384`), i.e. `active:false`,
  and nothing supplies a real value. **So the missing piece is exactly a data source for four numbers**:
  `HP_HEADERITEM` TS_TRUE size, `HP_HEADERITEM` TMT_MINSIZE position, `HP_HEADERDROPDOWN` (5) TS_TRUE cx, `HP_HEADERSORTARROW` (4)/`HP_HEADEROVERFLOW` (7) TS_TRUE sizes.
* DPI: `Header_OnCreate` 0x85c50 seeds `HD+0x114 = GetSystemMetricsForDpi(0x48 = 72 = SM_CYMENUCHECK, dpi = HD+0x38)` (the dropdown width before a theme raises it with
  `max(HD+0x114, part-5 cx)`), `HD+0x48 = SM_CXBORDER*8`, `HD+0xc4/0xc8 = SM_CXEDGE*3` (text/bitmap margins).

**4.5 What else the header asks once themed** (drawn by `Header_Draw` 0x5009c / `Header_DrawItem` 0x5037c):

| use | call | args (part, state) |
|---|---|---|
| fill right of last item | `DrawThemeBackground` | (0, HBG_DETAILS=1 or HBG_ICON=2 by `HD+0x128`) |
| each item | `DrawThemeBackground` | (1, state from table 0x2045c0: details row {1,2,3,4,5,6}; icon row {7,8,9,10,11,12}; offsets: +0 normal, +4 hot, +8 pressed, +0xc sorted normal, +0x10 sorted hot, +0x14 sorted pressed; sorted when HDF_SORTUP/DOWN (fmt & 0x600)) |
| item content rect | `GetThemeBackgroundContentRect` | (1, same state) -> text/bitmap rect |
| drop-down glyph | `DrawThemeBackgroundEx` (#47) | (5 HP_HEADERDROPDOWN or 6 HP_HEADERDROPDOWNFILTER (`(fmt&0x2000000)!=0)+5`), state 1/2/3 = NORMAL/SOFTHOT/HOT) |
| sort arrow | `DrawThemeBackground` | (4, 2 - (up!=0)) = HSAS_SORTEDDOWN 2 / SORTEDUP 1 |
| overflow | `DrawThemeBackground` x2 | (1, 7 or 8) and (7, 1 or 2) |
| item min width | `GetThemeBackgroundExtent` | (1, 0) in `Header_ComputeItemWidth` 0x98a0c |
| text | `DrawThemeText` via `SHThemeDrawText` 0x8a07c (`DrawThemeTextEx` with `GetThemeTextExtent` first) | (1, state) |

---------------------------------------------------------------------------------------------------------------------

## 5. How Industrial7 consumes uxtheme today

[V-src; paths under `/home/iceisfun/work/industrial7`.]

* **Policy: themeless by design.** `docs/COMCTL32.md:48` "uxtheme (v6; a theme-off seam answers 'no theme' until the theme layer is written)";
  `docs/USER32.md:273` "Themes (theme service, uxtheme UserApiHook) | missing | classic drawing only, kept exact";
  `docs/SHELL_READINESS.md:96` "Window regions / layered windows / themes | Partial | classic (non-themed) drawing";
  `docs/stage4/READINESS_CORE_GFX.md` marks `IsThemeActive` against a real consumer UNKNOWN and expects it to be FALSE;
  `docs/OWNED_STATUS.md:252` lists `uxtheme.dll | system dll | stock-only ... research only: docs/design/stockdll-ranking (rank 2)` and `:637` `R5 theme uxtheme/msstyles/Aero .theme ... NOT_SHIPPED`.
* **Pure decision crate `crates/w10-uxtheme-off`** (1100 lines, `no_std`): 61 named exports + 4 ordinals (`COMCTL32_V6_DELAY` pins the comctl32 half);
  "every behavioral row is Win7-derived" (header comment lib.rs:7-9). Design doc `docs/design/win10/UXTHEME-OFF-W10-LINK.md`
  (note: its claim "Win10 19041 uxtheme is not in the corpus" is **stale** - uxtheme 10.0.19041.2193 is in the corpus API now; and "conhost delay-imports SetWindowTheme" is not supported, see 1.1).
* **Guest DLL `userland/guest/uxtheme-w10`** (registry row `docs/personality/registry.tsv`: `uxtheme-w10 ... win10 win10 unwired`): exports all 193
  (`data/exports_manifest.tsv`): **109 `zero`/`ni` stubs, 49 `hr`/`ni` stubs, 25 `ehandle`/partial (every handle-taking getter, including `GetThemePartSize`, `GetThemeMetric`, `GetThemeInt`, `GetThemeMargins`: return `E_HANDLE 0x80070006`), 4 `open` (OpenThemeData, OpenThemeDataEx, OpenThemeDataForDpi, #49: return NULL with last error `0x80070490`), 2 `theme_query` (IsThemeActive, IsAppThemed: FALSE), 1 `set_theme`, 1 `set_attr`, 1 `dlg_texture`, 1 `comp_off`** [V-cmd awk over the manifest]. `GetThemeSysSize` and 108 other exports are `zero` stubs (`ERROR_CALL_NOT_IMPLEMENTED`).
  `glue.rs`: `NoWindows` hooks (every non-NULL hwnd treated as a window), `INITIALIZED = true`. Window-property side effects (SetWindowTheme etc.) are dropped in this image.
* **Consumers in Industrial7 that already speak "theme or classic":**
  * `crates/w10-comctl32-lv`: `trait Theme { open(hwnd,class) -> Option<usize>; part_defined(..) }` with `NoTheme` (`src/lv/paint_plan.rs:79-95`); the list paint plan runs the classic arm when `OpenThemeData("ListView")` is None or `LVP_LISTITEM` is undefined; `EXPLORER_*` colors are pixel-verified from a real Win10 Explorer list (`paint_plan.rs:44-`) so the Explorer-subapp look works without a theme.
  * `crates/w10-comctl32-lv/src/hdr/*`: header model with `Theme { active, min_height, dropdown_width, overflow_w, overflow_h }` and `Host::theme_info()` defaulting to the classic arm (`mod.rs:210-222, 381-384`), tests with `min_height: 24` (`tests/hdr_layout.rs:51-58`). **This is the exact socket for the recorded constants.**
  * `userland/guest/comctl32-logic/src/theme.rs` (153 lines): `DrawThemeParentBackgroundEx` rules; `comctl32-logic::bp` reused by `w10-uxtheme-off` for buffered paint.
  * Other comctl32 pieces (`w10-comctl32-tab`, `-tb`, `-status`, `-rebar`, `-tip`, `-tv`) exist as crates; grep found no `GetThemePartSize`-level theme source in them (only the lv/hdr crates reference theme).
* **Test evidence of themes elsewhere in the repo:** `docs/disasm/comctl32v6/decomp/*DelayLoad_*` is the **Win7** comctl32 (e.g. its `DelayLoad_GetThemePartSize` is at 0x18cec7, the Win10 one is at 0x9aa09), `docs/disasm/uxtheme/decomp` is Win7 RTM (7600). They are not interchangeable with the Win10 RVAs in this document.
* Chrome (not a stock Win10 file) delay-imports 6 names: CloseThemeData, DrawThemeBackground, GetThemeBackgroundContentRect, GetThemePartSize, OpenThemeData, ord#47 (`docs/stage4/chrome_imports.tsv`); all fail-soft when `OpenThemeData` returns NULL.

---------------------------------------------------------------------------------------------------------------------

## 6. Consumer-driven priorities for an armed (themed) uxtheme

[INF, from the counts above.] A theme data source that turns `OpenThemeData` non-NULL has to satisfy, in rough order of consumer weight:

1. **Core object**: `OpenThemeData` (+`ForDpi`, #49 `OpenNcThemeData` for ScrollBar/"Window"), `CloseThemeData`, `IsThemeActive`, `IsAppThemed`,
   `IsCompositionActive`, `SetWindowTheme`, `IsThemePartDefined`, `GetWindowTheme` - 25/25/27/13/11 importers.
2. **Measure getters comctl32 needs to size controls**: `GetThemePartSize` (27 sites; header, status, trackbar, tooltips, rebar, monthcal, listview groups, checkbox/radio glyphs),
   `GetThemeMargins` (10), `GetThemeInt` (13), `GetThemeTextMetrics`/`GetThemeTextExtent` (8/17), `GetThemeBackgroundContentRect/Extent`, `GetThemePosition` (header MINSIZE), `GetThemeFont`, `GetThemeColor`, `GetThemeBool`, `GetThemeEnumValue`.
3. **Drawing**: `DrawThemeBackground(Ex)`, `DrawThemeText(Ex)`, `DrawThemeParentBackground(Ex)`, `DrawThemeEdge`, buffered paint (already present), `GetThemeTransitionDuration` (8 sites; can return failure/0 to disable fades).
4. Immersive/dark-mode ordinals (#104, #106, #120, #121, #132-#139) are called by explorerframe/shell32/comdlg32 and are independent of control theming.

The header fix needs only item 1 + `GetThemePartSize`/`GetThemePosition`/`GetThemeTextExtent` for class `Header`.

---------------------------------------------------------------------------------------------------------------------

## 7. Probe inputs this survey hands to the probe-list role (class, part, state, property)

(Values to record on real Win10 at 96 DPI and, if the lane can, 120/144 DPI. `TS_TRUE`/`TS_DRAW`/`TS_MIN` as noted.)

* Header (the CC30 set): `GetThemePartSize(HEADER, 1,0,TS_TRUE)`, `(HEADER,1,0,TS_MIN)`, `(HEADER,1,0,TS_DRAW)`; `GetThemePosition(HEADER,1,0,TMT_MINSIZE 3403)`;
  `GetThemePartSize(HEADER, 4,0,TS_TRUE)`, `(5,0,TS_TRUE)`, `(7,0,TS_TRUE)`; `GetThemeTextExtent(HEADER,1,0,L"M",-1,0x800)` with a default 9pt font;
  `GetThemeBackgroundContentRect(HEADER,1,{1,2,3,4,5,6,7..12}, rect)` on a 100x24 rect; `GetThemeMargins(HEADER,1,1..,TMT_CONTENTMARGINS 3602)`; `GetThemeBackgroundExtent(HEADER,1,0, 0x0 rect)`; plus the real `HDM_LAYOUT` result (the 24 itself).
* Same tuple pattern for: Button 1/2/3 state 1 TS_DRAW (glyph sizes) and BP_COMMANDLINKGLYPH 7; Status SP_GRIPPER 3 TS_DRAW and `GetThemeTextMetrics(STATUS,0,0)`; TrackBar TKP_THUMB..THUMBRIGHT 3-8 state 1 TS_TRUE; Tooltip parts 5/6/7; Rebar 1/7 and `GetThemeMargins(REBAR,3,0)`; ListView 10 TS_TRUE, `GetThemeMargins(LISTVIEW,1,3)`, 6/0 margins, `GetThemePartSize(LISTVIEW,7,1,TS_TRUE)`, `(9,1,TS_DRAW)`; MonthCal 10; Combobox `GetThemeInt(COMBOBOX,4,1,BORDERSIZE 2403)`, `(0,0)`, `GetThemeMargins(COMBOBOX,0,0,3602)`; Edit `GetThemeMargins(EDIT,5,1,3602)`; Toolbar `GetThemeMargins(TOOLBAR,0,0,3601)`, `GetThemeTextMetrics(TOOLBAR,0,0)`; Progress `GetThemeInt(PROGRESS,0,0,{2426,5006,2427,2428})`; Window (NC) `GetThemeMargins(WINDOW,{1,2,3,5},1,3603)`, `GetThemePartSize(WINDOW,{18,19},0,TS_TRUE)`.
* Existence matrix: `IsThemePartDefined` over every (class, part, state 0) in the table of section 3 (the code gates on it for Combobox 3, Edit 3/5, TreeView 1/4, ListView 1).
* Class resolution: `OpenThemeData` with `"Button-OK;Button"` etc. (section 2.2), `"Indeterminate::Progress"`, `"CompositedWindow::Window"`, `"EditComposited"` (SetWindowTheme sub-app), `"Explorer"` (sub-app).

## 8. Known gaps in this survey

* UNKNOWN: any Win10 numeric theme value (HEADERITEM size/min size, margins, etc.): this role only locates the questions.
* The `[INF]` rows (Listbox, Link, Hotkey/IPAddress, TaskDialog markup, comctl32 ScrollBar vs user32 ScrollBar ownership) were not traced to their draw code.
* Static imports only: modules that `GetProcAddress` uxtheme (Chrome, Industrial7's own `w10-comctl32-lv`) or load it via the UserApiHook are invisible to the 107-name count. Modules not in the 3007-module corpus index were not scanned.
* `DPISCALEINFO::GetLegacyScaledEdgeHeight` (0x4b384) was decompiled in the fact-check: `MulDiv(GetSystemMetrics(0x2e = SM_CYEDGE), dpi, 0x60)`. SM_CYEDGE=2 at 96 dpi is the stock value but still a runtime metric, not recorded here (UNVERIFIED on the Win10 oracle; add `GetSystemMetrics(SM_CYEDGE)` to the probe list).
* The `DAT_171c045c0` header state table was read as raw bytes (`/api/bytes/comctl32?rva=0x2045c0&len=48` -> ints 1..12); the mapping of its columns to hot/pressed/sorted comes from the control-flow in `Header_DrawItem` and the HEADERITEMSTATES order, not from a symbol.

## 9. Verification (adversarial fact-check)

Checked by re-running API calls / commands (base `B=http://docker1.dev.moonlightcompanies.com:8765`):

| claim | result |
|---|---|
| `Header_OnLayout` 0x4b2a8 formula (+0x4c, 2*edge, +0x130 gate, +0x160, FILTERBAR 0x100, HIDDEN 8, flags 0x14) | CONFIRMED verbatim from decomp |
| `_Header_InitTheme` 0x85d98 (parts 1/5/4/7, TS_TRUE, `GetThemePosition(...,0xd4b)`, HRESULTs of PartSize ignored, `local_28` uninitialised) | CONFIRMED |
| `Header_NewFont` 0x86174 themed `"M"` extent flags 0x800 with `"..."` fallback; `Header_OnCreate` calls NewFont(NULL) before zeroing +0x130 and InitTheme; WM_THEMECHANGED (0x31a) only calls InitTheme; WM_SETFONT calls NewFont only when font differs | CONFIRMED (0x85c50 lines 42-48, 0x689b0 lines 50-52, 224-227) |
| TMT_MINSIZE=3403, SIZINGBORDERWIDTH 1201, COMPOSITED 2204, COMPOSITEDOPAQUE 2219, BORDERSIZE 2403, GLYPHINDEX 2418, GLYPHFONT 2601, CONTENTMARGINS 3602, SIZINGMARGINS 3601, CAPTIONMARGINS 3603, FILL/TEXTCOLOR 3802/3803, ANIMATIONDURATION 5006, TRANSITIONDURATIONS 6000, HALIGN/VALIGN/ICONEFFECT 4005/4007/4009; HP_*/HIS_*/HSAS_*/HDDS_* values; WP_ 18/19/30-37; MENU parts; SBP/TABP/TKP/SP/TTP/RP/PP/EP/CP/BP/LVP/TVP/TP/SPNP/DP | CONFIRMED against `/usr/x86_64-w64-mingw32/include/vsstyle.h`, `vssym32.h` |
| Header state table 0x2045c0 = ints 1..12, row stride 0x18, column offsets +0/+4/+8/+0xc/+0x10/+0x14 | CONFIRMED (`/api/bytes`, `Header_DrawItem` 0x5037c lines 151-170) |
| `GetThemePartSize` 0x14a20 (border-fill: TS_MIN 2*bs, TS_TRUE 2*bs+1) and `CImageFile::GetPartSize` 0x14edc | CONFIRMED |
| Spot-check of call sites: 0x98a0c (extent 1,0), 0x3a688 (SP_GRIPPER 3 TS_DRAW), 0xb0c0 (SM 3 / PartSize), 0xe8f40 (GetThemeInt 0x4b1), 0x112fc4 (TTP_CLOSE 5,1,TS_DRAW), 0x44734 (part+2,1,TS_DRAW), 0x148a90 (MonthCal part 10 TS_TRUE), 0x11a708 (sort arrow 4, 2-(up!=0)), 0x5009c (0,.. / 1,7|8 / 7,1|2), 0x8384 (Tab flags 0x2180, 0x40, +0x3c|2, +0x8c/+0x90) | CONFIRMED |
| Importer scan: comctl32 `total:52`, 47 named + ordinals 47,49,72,75,131, all `delay:true`; user32/gdi32/gdi32full/win32u/notepad `total:0`; 76 modules/518 entries/107 names for `dll=uxtheme.dll`; the per-name module counts | REPRODUCED for the direct scan, but **INCOMPLETE**: see corrections |
| ordinals 49/72/75/131 names (`/api/resolve`), ThemeInitApiHook ordinal 193 @0xcda0, `InitUserApiHook` 0x22ce0 sole caller `__ClientLoadLibrary` 0x22a50, 193 exports with 34 SHUNIMPL | CONFIRMED |
| Wine ordinal list | CONFIRMED, wording tightened (no 12, no 134) |
| Industrial7 manifest counts (109 zero, 49 hr, 25 ehandle, 4 open, 2 theme_query, 1 each set_theme/set_attr/dlg_texture/comp_off), registry row unwired, `E_HANDLE 0x80070006`, `Theme` struct and `Host::theme_info()` default, tests `min_height: 24`, docs line citations (OWNED_STATUS 252/637, USER32 273, COMCTL32 48, SHELL_READINESS 96), chrome_imports.tsv 6 names | CONFIRMED |

Errors found and fixed in place:

1. **conhost claim was wrong.** conhost delay-imports `SetWindowTheme` through the API set `ext-ms-win-uxtheme-themes-l1-1-0.dll`; the survey's `dll=uxtheme.dll` filter cannot see API-set imports. The i7 design doc was right. Importer totals corrected from 76 to 93 modules and 518 to 594 entries (still 107 distinct exports); ranking counts corrected (1.2). New importers via the API set: conhost, msftedit, edgehtml, udwm, windows.ui.xaml(+host), twinui.pcshell, applicationframe, and others. Practical consequence added: an armed uxtheme must be reachable under the schema name too.
2. The dialog button class table has **13** entries (id 12 = `Button-APPLY;Button`); the doc listed 12.
3. `GetLegacyScaledEdgeHeight` resolved (`MulDiv(GetSystemMetrics(SM_CYEDGE), dpi, 96)`), previously guessed.
4. user32 also contains `NtUserBroadcastThemeChangeEvent` (not just `__xxxClientBroadcastThemeChange`).
5. `layout.rs` citation range corrected (57-70).

Still UNVERIFIED (not re-checked): the full list of ~300 comctl32 call sites beyond the ~12 spot-checked, the `[INF]` rows (Listbox, Link, Hotkey/IPAddress), the 144 modules with `nfuncs=0` were scanned (no uxtheme imports) but modules outside the 3007-module index were not, the user32 `tagUSERAPIHOOK` field list (only the head of `InitUserApiHook` re-read), and the "24 px" origin itself (still UNKNOWN: needs the Win10 probe of HEADERITEM TS_TRUE / TMT_MINSIZE and the real HDM_LAYOUT result; the formula in 4.1-4.4 is verified, the numeric inputs are not).

Missing finding worth adding: because `_Header_InitTheme` takes `max(PartSize.cy, MINSIZE.y)` and `Header_OnLayout` only substitutes when `text_cy + 2*edge <= +0x160`, **any** recorded value >= 20 at 96 dpi yields the minimum; a value below 20 would be invisible. At 125%/150% DPI the same formula applies with a DPI-selected theme image, so probes must be taken per DPI (already in section 7).
