# 02 - Win10 uxtheme.dll semantics (10.0.19041.2193)

Role: how the real Win10 `uxtheme.dll` behaves, from its decompilation. Research only; no code was run on Windows.
Everything below is tagged:

* **[V]** verified fact read from the Win10 decompilation in the corpus (cited as `fn @RVA`, RVA relative to image base `0x171b00000`).
* **[V-reg]** value read from the module bytes via `/api/bytes/uxtheme?rva=...`.
* **[INF]** inference from decomp structure (flagged because the decomp is naming-poor in places or a path is only partly read).
* **[SRC]** read from Wine source (`/home/iceisfun/work/wine/wine/dlls/uxtheme`), only used for naming/cross-check, never as Win10 truth.
* **UNKNOWN** not established; do not invent a value.

Method: `http://docker1.dev.moonlightcompanies.com:8765` (corpus explorer). Exports: `/api/modules/uxtheme/exports?limit=1000` (193 total: 85 named, 108 ordinal-only, 34 of those 108 are `SHUNIMPL.#n` forwarders). Decompiled with `/api/function/uxtheme/<rva>/decomp`, callees with `/callees`; all 837 uxtheme functions within 4 call-levels of the 156 distinct export bodies were pulled and read. The PDB symbol names the corpus carries (e.g. `CRenderObj::GetValueIndex`, `CAppInfo::OpenWindowThemeFile`) are the names used here. HRESULT values in the decomp print as negatives (`-0x7ff8fffa`); they are converted to unsigned form here.

Module facts [V]: `/api/modules/uxtheme` -> `version 10.0.19041.2193`, image base `0x171b00000`, `entry_rva 166768`, `.data` has 12116 virtual bytes but only 5120 raw bytes, i.e. most globals (e.g. `g_preferredAppMode` at RVA 0x934a4, `g_fAppOptInHighContrast` 0x92f44, `g_eThemeHookState`) start as zero.

HRESULT legend used throughout: `E_HANDLE`=0x80070006, `E_POINTER`=0x80004003, `E_INVALIDARG`=0x80070057, `E_FAIL`=0x80004005, `E_NOTFOUND`=`HRESULT_FROM_WIN32(ERROR_NOT_FOUND)`=0x80070490, `E_OUTOFMEMORY`=0x8007000E, `E_UNEXPECTED`-ish "uninitialized" = 0x8000FFFF (that is `E_UNEXPECTED`), ERROR_MORE_DATA-style buffer = 0x800700EA (`HRESULT_FROM_WIN32(ERROR_MORE_DATA)`), 0x8007000B = `HRESULT_FROM_WIN32(ERROR_BAD_FORMAT)`, 0x8007041D = `HRESULT_FROM_WIN32(ERROR_SERVICE_REQUEST_TIMEOUT)`, 0x8007054F = `HRESULT_FROM_WIN32(ERROR_INTERNAL_ERROR)`.

Important style fact [V]: many functions pass an **HRESULT to `SetLastError`** (not a Win32 code), so `GetLastError()` after a failed `OpenThemeData` is e.g. `0x80070490`, not `1168`.

---

## 1. The big picture (read this first)

Win10 uxtheme is a thin **client** over data that somebody else loads. Three layers:

1. **msstyles file** (`aero.msstyles`, `aerolite.msstyles`; the string `aerolite.msstyles` occurs in `LoadTheme @0x4108c`; `_IsDWMAtlas @0x4a890` and `_GetNcThemeMetrics @0x20cc0` reference `DWMWINDOW`/`Tab` class names). It is a PE resource DLL. It is opened only by the *theme manager side* and by a handful of explicitly file-based exports (section 3).
2. **Per-session shared sections**. The loader (winlogon/themes service side: `LoaderLoadTheme @0x4c1b0`, `ApplyThemeSections @0x25650`, `SendAcquireOrAddTheme`) parses the msstyles into two kernel sections and publishes their names in a `ROOTSECTION` block named `\Sessions\%d\Windows\ThemeSection` (session id taken from `PEB+0x2c0`; session 0 uses `\Windows\ThemeSection`) [V: `CRootSection::CRootSection @0x9db4`]. Every ordinary client call reads only these sections.
3. **Theme service ALPC port `\ThemeApiPort`**, connection request text `ThemeApiConnectionRequest` [V: `ConnectedToService @0xca84`]. Used for change numbers, acquire/release bookkeeping and for *loading* a global theme; **not** needed to read property values once the sections exist.

So "which functions touch msstyles" is a short list; "which need the sections" is almost everything; "which need the service" is the setter/loader side. The tables are in section 3.

Consequence for a themeless-by-design implementation: any consumer-visible number (`GetThemePartSize`, `GetThemeMetric`, `GetThemeInt`, ...) ultimately comes out of a section whose layout is the *binary form of msstyles* (class -> part -> state -> property chain). A recorded-constants table replaces layer 2 and 3; the lookup *rules* (section 5-6) are what must be copied.

---

## 2. Process-global state and gating [V unless stated]

### 2.1 Start-up (`DllMain @0x297e4` -> `ProcessStartUp`)

* `DLL_PROCESS_ATTACH` -> `DisableThreadLibraryCalls`, `ProcessStartUp`: creates its own heap (falls back to process heap), `GlobalsStartup` (render list, `CAppInfo`, bitmap caches, `g_fUxthemeInitialized = 1`), `ThemeHookStartup` (critical sections, `CThemeMenu::StaticInitialize`), `UxCtrlStaticInitialize`. `GlobalsStartup` also runs an app-compat shim hook `SbSelectProcedure(0xabababab, ...)` that may set `g_fAppOptInHighContrast`; default 0.
* If `g_fUxthemeInitialized == 0` the app-level calls return **0x8000FFFF** via `SetLastError` (`IsThemeActive`, `GetThemeAppProperties`, `SetThemeAppProperties`, `IsAppThemed`, `GetCurrentThemeName` returns it as HRESULT, `SetSystemVisualStyle` returns it). `OpenThemeData*`: returns NULL with LastError **unchanged by that branch** (the function body is skipped entirely when uninitialised: `_OpenThemeData @0x192c4` only does `SetLastError(0)` *then* the `if (g_fUxthemeInitialized)`; so NULL, LastError 0).
* `uxtheme` also carries a private activation context that loads **comctl32 v6** itself (`LoadUxCtrl @0x64c0`, `CreateActCtxW` with resource id 1 of uxtheme then `LoadLibraryW(L"comctl32")`), used for HIMAGELIST helpers (`DrawThemeIcon`). It does not depend on the host app manifest.

### 2.2 `CAppInfo` defaults (`CAppInfo::CAppInfo @0x37ac`)

| field offset | init | meaning |
|---|---|---|
| +0x00 | 0 | `CustomAppTheme` flag (set only by `InstallPrivateThemeFileForTesting`) |
| +0x08 | `-1` (0xFFFF...) | current app theme file; `-1` = not loaded yet |
| +0x10 | `-1` | change number of the loaded theme |
| +0x18 | 1 | (unlabelled) |
| +0x20 | **3** | **app theme flags = STAP_ALLOW_NONCLIENT(1) \| STAP_ALLOW_CONTROLS(2)**; STAP_ALLOW_WEBCONTENT(4) is not set |
| +0x24 | 0 | "theme override while hooks off" flag (nonzero only via `InstallPrivateThemeFileForTesting @0x36b80`) |
| +0x50 | critical section | guards the above |

So **`GetThemeAppProperties()` on a fresh Win10 process returns 3, not 7** [V: `CAppInfo ctor @0x37ac`; only other writer is `SetThemeAppProperties @0x512b0`]. (The Industrial7 `w10-uxtheme-off` crate uses `DEFAULT_APP_FLAGS = 7`, commented "stock default not read [U]"; this is a divergence, see section 14.)

`SetThemeAppProperties(dwFlags)` stores `dwFlags` verbatim (no masking) under the app critical section; `SetLastError(0)`; if `dwFlags == 0` it fires `DisableThemeForProcessTelemetryEvent`. Returns void. `GetThemeAppProperties()` returns the stored DWORD, `SetLastError(0)`.

### 2.3 Hook state `g_eThemeHookState`

Zero at load (BSS). `ThemeInitApiHook @0xcda0` (ordinal 193, exported by name; user32 calls it through `RegisterUserApiHook`'s `ThemeInitApiHook` string, built in `InstallThemeHooks @0x26578`): command 0 installs the theme versions of ~28 user32 API hooks (`ThemeDefWindowProcW`, `ThemeGetSystemMetrics`, `ThemePreWndProc`, `ThemeDrawFrameControl`, ...); commands 1/2 flip the state `0 -> 1 -> 2` ("hooks disabled", runs `OnHooksDisabled`: `CAppInfo::ResetAppTheme`, `CThemeWnd::DetachAll`, `CThemeMenuMetrics::FlushAll`, `CRenderList::FreeRenderObjects(-1)`, frees the hook module reference). `ThemeInitApiHook` called before `g_fUxthemeInitialized` only records `g_fEarlyHookRequest = 1` and returns 0. [INF: which argument values user32 passes for the three commands is not re-derived here.]

Hooks are **not** installed (via `ApiHandlerInit @0xd980`) in processes whose image name matches this list [V-reg, strings read at RVA 0x69798.. via `/api/bytes`]: `wininit, winlogon, lsass, services, svchost, dfssvc, winmgmt, spoolsv, msdtc, ntvdm`, nor in non-interactive window stations or Win16 apps.

Effect on the Open path: `_OpenThemeData` proceeds only if `g_eThemeHookState == 0` **or** `CAppInfo+0x24 != 0`; otherwise it sets LastError **0x80070490** and returns NULL [V: `_OpenThemeData @0x192c4`, `AppIsThemed @0x1bdd0`].

### 2.4 High contrast gate

`IsHighContrastMode @0xe900`: `SystemParametersInfoW(SPI_GETHIGHCONTRAST=0x42, 0x10, &HIGHCONTRAST)`; true iff the call succeeds and `HCF_HIGHCONTRASTON` (bit 0 of `dwFlags`). A test override global `g_fForceHighContrastModeUnderTest` exists. When true **and** `g_fAppOptInHighContrast == 0`:

* `OpenThemeData`, `OpenThemeDataEx`, `OpenThemeDataForDpi`, `OpenNcThemeData` return **NULL and do not touch LastError** (no `SetLastError` in the wrapper) [V: `0x1b590`, `0x50dc0`, `0xb9e0`, `0xbb20`].
* `IsThemeActive` = FALSE, LastError 0 [V: `0x224e0`]; `IsCompositionActive` = FALSE; `GetWindowTheme` returns NULL; `EnableThemeDialogTexture` returns 0 without storing; `IsThemeDialogTextureEnabled` returns FALSE; `GetCurrentThemeName` returns 0x8000FFFF [V: `0x24f80`]; `GetThemeSysColor/GetThemeSysColorBrush` fall through to `GetSysColor/GetSysColorBrush`.

### 2.5 DPI

* `GetScreenDpi @0x13db8`: `GetDeviceCaps(GetDC(NULL), LOGPIXELSX=0x58)`, cached in `g_nScreenDpi`, re-read when `IsProcessDPIAware()` changes; forced value `g_fForcedDpi` for tests.
* `OpenThemeDataForDpi(hwnd, classes, dpi)`: `dpi > 0x1e0 (480)` -> LastError **0x57** (plain `ERROR_INVALID_PARAMETER`, *not* an HRESULT) and NULL [V: `0xb9e0`]. Otherwise `OpenThemeDataExInternal(hwnd, classes, 0, ..., dpi)`.
* `CRenderList::OpenRenderObject @0x19e40`: if `dpi == 0`: when `hwnd != NULL && ThemeHasPerWindowDPI(hwnd, ...)` the object dpi is `user32!GetWindowDPI`(ordinal 2707, name from `/api/resolve ref=user32!ordinal:2707`) and the object is marked per-window-DPI; otherwise `GetScreenDpi()`. `ThemeHasPerWindowDPI @0x1a544` is false if window prop `0xa91f` is set; for top-level windows it asks user32 ordinals 2703/2706 (labelled `NtUserIsNonClientDpiScalingEnabled` / `NtUserIsWindowBroadcastingDpiToChildren` by the corpus), for child windows ordinal 2705 (`NtUserIsChildWindowDpiMessageEnabled`). [INF on exact arg roles.]
* `_OpenThemeData` ORs **OTD_FORCE_RECT_SIZING (1)** into the flags whenever `GetScreenDpi() != 96`.
* Scaling rules: all `GetThemeMetric` / `GetThemeSysSize` / theme font heights are stored at 96 dpi and scaled with `MulDiv(v, dpi, 96)`: `ScaleThemeSize @0x20904`: dpi = `hdc==NULL ? obj.dpi : (obj is per-window-DPI && IsScreenDC(hdc)) ? obj.dpi : GetDeviceCaps(hdc, LOGPIXELSX)`; `GetThemeSysSize` uses `ScaleSizeForScreenDpi @0x4adac` = `MulDiv(v, GetScreenDpi(), 96)`. Image-based sizes (`GetThemePartSize`, `GetThemeMargins`) instead *select a DPI-matched image variant* (`CImageFile::SelectCorrectImageFile @0x16ed0`, the "plateaus"/`ScaledBackground` records), they do not just multiply.

---

## 3. What touches what

### 3.1 msstyles resource layout (as far as the code shows)

`LoadThemeLibrary @0x52468`: `LoadLibraryExW(path, NULL, LOAD_LIBRARY_AS_DATAFILE=2)`, then the resource `PACKTHEM_VERSION` (type string, id 1) must be a 2-byte value equal to **4**; `<4` or `>4` -> `E_FAIL` internally, then collapsed to **0x8007000B**; any load failure -> `MakeErrorLast()` (the Win32 error as HRESULT). The out-param `*version` receives the WORD [V].

Resource-name strings referenced by the loader/readers (collected from all 837 decompiled functions): `CMAP, VMAP, RMAP, AMAP, BCMAP, STBL, STBL%02d, packthem, PACKTHEM_VERSION, VARIANT, default, globals, sysmetrics, SysMetrics, documentation, colorvariant, sizevariant, ENUMVAL, DUPCLASS, MINCOLORDEPTH, TEXTGLOW, timingfunction, DWMWINDOW, DWMTOUCH`, and the path format `%s\%s\%s.msstyles` (EnumThemes: `<dir>\<theme>\<theme>.msstyles`). The task-brief names CMAP/NMAP/SMAP are only partially confirmed: **CMAP, RMAP, AMAP, VMAP, BCMAP exist; the literal strings "NMAP" and "SMAP" do not occur** in the decompiled uxtheme (UNKNOWN whether the brief's NMAP/SMAP are CSS/size-name variants of the above; the doc-visible sub-resources are `colorvariant`/`sizevariant`).

Class lookup in the *binary section header* [V]: the section starts with magic `BEGINTHM` and dword version **0x10007** (checked in `CUxThemeFile::ValidateThemeData @0x9c54`, otherwise 0x8007000B). A class index lives at `hdr+0x3c` with byte length at `hdr+0x40`; entries are **0x18 bytes**: `{appNameStr, classNameStr, classDataOffset, ...}`; string offsets are relative to the section base (`ThemeString @0x1a5c8`: `base + off`, `""` for off<=0). `MatchThemeClass @0x1a5f0` scans entries with `appNameStr == 0`, compares `classNameStr` with an ASCII case-fold (`'A'..'Z'` -> `+0x20`), returns the class data offset; none -> **0x80070490**. `MatchThemeApp @0x1a710` scans entries with `appNameStr != 0` matching the app string case-insensitively, then compares the class string; success also returns the app string offset; failure falls back to `MatchThemeClass` when allowed.

Per-class property stream (decoded from `CRenderObj::GetValueIndex @0x1b260` and `GetPropertyOrigin @0x18d40`): a chain of records `{u16 id; u8 type; u8 flag; u32 size/next; payload}` starting at the class data. Special record ids: `0x0a` = **part jump table** (`u8 count` at +0x14; `u32` offsets indexed by part id, `-1`/out of range -> falls back to the entry at +0x18 = "default part"), `0x0b` = **state jump table** (`u8 count` at +0x8; `u32` offsets by state id with the same fallback), type byte `0x0c` on a record = continuation pointer to the base-class chain (`-1` = end). A property request `(part, state, propId)` walks: class stream -> part table -> state table, remembering the level it ended on; value found at that level -> returns the byte offset of the payload; not found anywhere -> `-1` (-> 0x80070490). `propId == 0` always misses. Origin values returned by `GetThemePropertyOrigin` are `PO_STATE=0, PO_PART=1, PO_CLASS=2, PO_GLOBAL=3, PO_NOTFOUND=4` (state-table default -> PART, part-table default -> CLASS, base-class continuation +1). [INF for the exact record header byte positions; the traversal logic is [V].]

Value encodings read by the getters [V]: COLORREF/int/enum = 4 bytes at payload; BOOL = 1 byte; POINT = 2x int; RECT/MARGINS = 4x int (RECT order l,t,r,b; MARGINS order cxLeft,cxRight,cyTop,cyBottom); INTLIST = count (<= **0x192 = 402**, else 0x80070490) then ints; STRING/FILENAME = UTF-16 with a size dword at payload-4 (copy is `min(cch, size/2)`); FONT = a 2-byte **font index** into the theme's LOGFONT table (`CUxThemeFile::GetFontByIndex`), optional `ScaleThemeFont`; STREAM = pointer/length into the section (TMT_DISKSTREAM 213 resolves a resource in `hInst`, and under high contrast for DWM atlases is re-colourised via WIC PNG decode/encode); bitmaps are built on demand from packed DIBs in the section (`GetBitmap @0x1f14`, `CreateUnscaledBitmap`, `ReturnBitmap`).

### 3.2 Classification of the 85 named exports + key ordinals

Legend: **F** reads an msstyles *file* directly (LoadLibraryEx data-file); **S** reads only the mapped theme sections via an `HTHEME`; **R** needs the root section (`ROOTSECTION`) / app theme file but no `HTHEME`; **A** needs the ALPC theme service; **U** user32/gdi32/window-property state only; **-** pure/stub.

| group | functions | class | notes |
|---|---|---|---|
| open/close | OpenThemeData, OpenThemeDataEx, OpenThemeDataForDpi, OpenNcThemeData(49), CloseThemeData | R+S | opens `CAppInfo::OpenWindowThemeFile` -> `CUxThemeFile::GetGlobalTheme` -> two `NtOpenSection` via `CRootSection`; no ALPC on this path |
| property getters | GetThemeBool/Color/Int/Enum/Position/Rect/IntList/String/Filename/Font/Margins/Metric/Stream/Bitmap/PropertyOrigin, IsThemePartDefined, GetThemeTransitionDuration, GetThemeAnimation*/TimingFunction, GetThemeClass(74), IsThemeClassDefined(50) | S | |
| part geometry/draw | GetThemePartSize, GetThemeBackground{ContentRect,Extent,Region}, IsThemeBackgroundPartiallyTransparent, HitTestThemeBackground, DrawThemeBackground(Ex), DrawThemeText(Ex), DrawThemeEdge, DrawThemeIcon(Ex), GetThemeTextExtent/Metrics | S (+GDI) | |
| parent bg / texture | DrawThemeParentBackground(Ex), EnableThemeDialogTexture, IsThemeDialogTextureEnabled | U | window messages + window properties |
| sys metrics | GetThemeSysColor/ColorBrush/Size/Bool/Int/Font/String | S (if hTheme) else U | `hTheme==NULL` falls back to GetSysColor/GetSystemMetrics/SPI |
| app/window state | IsThemeActive, IsAppThemed, GetThemeAppProperties, SetThemeAppProperties, EnableTheming, SetWindowTheme, GetWindowTheme, SetWindowThemeAttribute, IsCompositionActive | R / U | |
| file-based | GetThemeDocumentationProperty, GetCurrentThemeName(R), GetThemeDefaults(7), EnumThemes(8), EnumThemeColors(9), EnumThemeSizes(10), EnumThemeProperties(69), OpenThemeDataFromFile(16) | **F** (all but GetCurrentThemeName, which is R) | `LoadThemeLibrary` callers [V, via `/callers`]: Client_LoadGlobalTheme, EnumThemeColors, EnumThemeProperties, EnumThemeSizes, EnumThemes, GetThemeDefaults, GetThemeDocumentationProperty, LoadTheme, LoadThemeForTesting |
| service/loader | QueryThemeServices(1), SetSystemVisualStyle(65), ApplyThemeSections(93), SendAcquireOrAddTheme(91), SendReleaseTheme(116), LoaderLoadTheme(92), Remote_ReestablishServerConnection(33), EnableServiceConnection(76), ClearTheme(84) | A / loader | not called by applications |
| buffered paint / animation | BufferedPaint*, BeginBuffered*, EndBuffered*, GetBufferedPaint*, BufferedPaintRenderAnimation, BufferedPaintStopAllAnimations | U (per-thread TLS pool) | no theme data |
| dark mode / immersive | ordinals 94-139 subset | U/registry/shared memory | section 11 |
| panning feedback | Begin/Update/EndPanningFeedback | U | section 12 |
| COM | DllCanUnloadNow, DllGetClassObject, DllGetActivationFactory | - | WRL module; not themed APIs |

### 3.3 ALPC theme service ops [V: callers of `CallThemeServer @0xc988`]

`CallThemeServer(op, API_THEMES_SPECIFIC*)` wraps `NtRequestWaitReplyPort(s_hAPIPort)` with a message of 0x2b0 bytes (header word `0x780050`, op code at `+0x28`); parameter block is 0x48 bytes.

| op | caller | purpose |
|---|---|---|
| 1 | `QueryThemeServices @0x50f70` | returns a DWORD "services status" (`local_58[0]`) |
| 2 | `GetCurrentChangeNumber @0xc5d8` | theme change number; **if the service is not connected it instead reads the number from `ROOTSECTION+0x410`** (no error) |
| 3 | `Client_LoadGlobalTheme @0x4addc` | load a theme by file/colour/size name (flags `|8`) |
| 4 | `SendProcessAssignSection @0x4b2d0` | assign section handles to a process |
| 0x3ed | `SendAcquireOrAddTheme` | acquire/add a theme section for the client |
| 0x3ee | `SendReleaseTheme` | release |
| 0x3ef | `ReestablishServerConnection @0xc8d0` | reconnect probe; failure closes the port and re-allows connection attempts |

Failure policy [V]: status `0xC0000037` (invalid port handle) or `0xC0000253` (connection reset) -> `CloseConnection`; negative NTSTATUS results are returned `| 0x10000000` (HRESULT-ish facility-NT mapping). `QueryThemeServices` with no connection returns 0 and LastError **0x8007041D**. `ConnectedToService` connects once (`NtConnectPort(\ThemeApiPort)`); a failure sets `s_fAllowConnectionAttempts = false` so no more attempts until `ReestablishServerConnection`/`EnableServiceConnection(TRUE)`.

**Service absent**: ordinary reads still work if the sections exist (layer 2); `IsCompositionActive` returns FALSE when `s_fAllowConnectionAttempts` is false or hooks are off [V: `IsCompositionActiveInternal @0xe8d0`]; global-theme *changes* (`SetSystemVisualStyle` with a file) return 0x8007041D [V: `Client_LoadGlobalTheme`]. If the *sections* are absent, `OpenWindowThemeFile` fails (0x80070490 or the NTSTATUS-derived error from `CSection::OpenSection`) and every Open returns NULL.

---

## 4. HTHEME model, cache and lifetime [V]

* `HTHEME` is **not a pointer**: `(generation << 16) | slot`, 16 bits each [V: `CThemeApiHelper::OpenHandle @0x18888`, `OpenRenderObject @0x19e40`]. `g_pRenderList` holds an array of 0x28-byte entries `{CRenderObj*, generation(+8), openCount(+0xc), refCount(+0x10), dead(+0x18), hwnd(+0x20)}` under a critical section. Handle validation: slot < count, `CRenderObj*` non-NULL, `dead == 0`, generation equal -> else **0x80070006 (E_HANDLE)**. A fresh slot starts at generation 1; reuse increments it (`0xFFFF` wraps to 1).
* **Each API call takes a temporary ref** (`CThemeApiHelper` ctor/`OpenHandle` bumps refCount, `CloseHandle` drops it), so a `CloseThemeData` racing a draw call is safe; the object is freed when both `openCount` and `refCount` reach 0 (`DeleteCheck`).
* **Cache / identity**: `OpenRenderObject` reuses an existing entry when `(theme file, class data offset, base-class id, object dpi, per-window-dpi flag)` match and no custom draw objects are supplied and not (flags other than OTD_NONCLIENT while screen dpi is 96); the reuse path just increments `openCount` and returns the **same HTHEME value** [V structure; INF on the exact flag condition]. Therefore two `OpenThemeData(hwnd, L"Button")` calls on the same dpi return identical handles, and each needs its own `CloseThemeData`.
* **`CloseThemeData(hTheme)`**: `CRenderList::CloseRenderObject @0x19048`: valid handle -> decrement `openCount` if > 0, `DeleteCheck`, return S_OK(0). Invalid -> **0x80070006**. **`CloseThemeData(NULL)` -> E_HANDLE** [INF from generation check: handle 0 is slot 0/generation 0, generations start at 1; Industrial7's crate comment also records "CloseThemeData is E_HANDLE for every handle (corpus 0x190f0 -> 0x19048)"]. It does **not** touch LastError.
* **`_OpenThemeDataFromFile @0x196b4`**: unless `dwFlags & OTD_NONCLIENT(2)` and `hwnd != NULL`, stores the new HTHEME in window prop `0xa912` (`SetPropW`) -> this is what `GetWindowTheme` returns. `_OpenThemeData` first does `RemovePropW(hwnd, 0xa912)` (the previous "current theme" association is dropped, the old handle is *not* closed). The function ends with `SetLastError(hr)` for both success (0) and failure.
* `GetWindowTheme(NULL)` -> NULL with LastError 0x80070006; non-window hwnd likewise (`IsWindow` check) [V `0x6a10`].
* Window property atoms (integer atoms used as `LPCWSTR`): `0xa910` = pszSubIdList atom, `0xa911` = pszSubAppName atom, `0xa912` = HTHEME, `0xa914` = non-client attribute bits, `0xa915` = "in DrawThemeParentBackground" re-entrancy guard, `0xa916` = dialog-texture flags, `0xa91e` = dark-mode-allowed marker, `0xa91f` = disable per-window DPI, `0xa91a` = shake-window marker. These match the constants in the Industrial7 crate for 0xa910/11/12/14/16.

---

## 5. OpenThemeData(Ex/ForDpi) / OpenNcThemeData in detail

Call chain [V]: `OpenThemeData @0x1b590` / `OpenThemeDataEx @0x50dc0` / `OpenThemeDataForDpi @0xb9e0` -> HC gate -> `OpenThemeDataExInternal @0x1a470(hwnd, classList, flags, ?, dpi)` -> `_OpenThemeData @0x192c4(hwnd, classList, dpi, flags, ncFlag)`. `OpenNcThemeData @0xbb20` -> `OpenNcThemeDataInternal @0xbc2c` -> `_OpenThemeData(hwnd, classes, 0, flags=2, true)`.

Ordered checks / outcomes of `OpenThemeDataExInternal`:

1. `hwnd != NULL` and `!IsWindow(hwnd)` -> `SetLastError(0x80070006)`, return NULL.
2. `pszClassList == NULL` -> `SetLastError(0x80004003)`, return NULL. (NULL hwnd is legal.) `OpenNcThemeDataInternal` has the same `IsWindow` rule but passes a NULL class list straight to `_OpenThemeData`, where it hits the empty/NULL check below.
3. `_OpenThemeData`: `SetLastError(0)`; (uninitialised -> NULL, LastError 0); `RemovePropW(hwnd,0xa912)`; hook-state / `+0x24` gate -> else LastError **0x80070490**; app-flag gate: `flags = CAppInfo+0x20`; `if (!(dwFlags & OTD_NONCLIENT(2))) flags >>= 1`; `if (!(flags & 1))` -> LastError **0x80070490**, NULL. i.e. plain `OpenThemeData` needs `STAP_ALLOW_CONTROLS(2)`, `OpenNcThemeData`/`OTD_NONCLIENT` needs `STAP_ALLOW_NONCLIENT(1)`. **`SetThemeAppProperties(0)` therefore makes every Open fail with 0x80070490** (it does not change IsThemeActive/IsAppThemed, see 9).
4. `pszClassList == NULL || *pszClassList == 0` -> `E_INVALIDARG` 0x80070057 (LastError).
5. `CAppInfo::OpenWindowThemeFile(hwnd, &file)` (section 2/3): tracks the window (`TrackForeignWindow`), reuses the cached app theme file (`CAppInfo+8`) if its header flag byte has bit0 set, else `CUxThemeFile::GetGlobalTheme` -> `OpenThemeFile` (opens the two sections, refcounts them in a table keyed by the two section handles). Failure `-> 0x80070490` or the section-open error; LastError = that HRESULT, NULL returned.
6. `_OpenThemeDataFromFile(file, hwnd, classList, dpi, flags|(screenDpi!=96 ? 1:0), nc)` -> `MatchThemeClassList` -> `CRenderList::OpenRenderObject`; `SetLastError(hr)` at the end (0 on success), set `0xa912` prop.

### 5.1 Class list parsing [V `MatchThemeClassList @0x197c0`, `_ParseClassName @0x572ec`]

* `NULL` list -> E_INVALIDARG(0x80070057) returned as HRESULT (cannot be reached through the public entry points, which reject NULL earlier with E_POINTER).
* If `hwnd != NULL`: window prop `0xa910` (SetWindowTheme's `pszSubIdList`), if set, **replaces the whole class list** (`GetAtomNameW`, 0x104 chars). Prop `0xa911` (`pszSubAppName`) supplies the app name.
* The (possibly replaced) list is copied (max 0x7ffffffe chars) and split on **`;`**. Each element is tried in order; the first success wins; if all fail the **last** failure HRESULT is returned (so for "A;B" where neither exists you get 0x80070490).
* Within an element, if no app name is known, a **`::`** splits `App::Class` (first `::` only; `StringCchCopyN` of the prefix as app name; remainder is the class). So `Button::Window`... is **app = `Button`, class = `Window`**, i.e. the task-brief spelling `'Button::Window'` is an *app-qualified class lookup*, not a part path. The app-qualified form is matched with `MatchThemeApp`; the plain form with `MatchThemeClass`. Matching is ASCII case-insensitive, whole string, no wildcards; there is no `Class(Part)` syntax in Win10.
* **Dark mode**: if `ShouldAppsUseDarkMode()` and `!GetIsImmersiveColorUsingHighContrast(0)` and `IsDarkModeAllowedForWindow(hwnd)`, the app name is mapped by `MapAppNameToDarkMode @0x3e820` to `"DarkMode"` or `"DarkMode_" + app` (e.g. `DarkMode_Explorer`) and looked up first with `MatchThemeApp(..., allowFallback=0)`; on failure the normal path runs. This is how `SetWindowTheme(h, L"DarkMode_Explorer", NULL)` works.
* Errors: `HeapAlloc` fail -> 0x8007000E; string too long -> 0x80070057 / 0x8007007A.

---

## 6. Property getters: validation order and errors [V]

All take `hTheme` first. Order of checks (this ordering is observable and worth probing):

| function | order | errors |
|---|---|---|
| GetThemeBool/Color/Int/Enum/Position/Rect/IntList/Stream/Bitmap/PropertyOrigin | **(1) output pointer NULL -> E_POINTER, (2) handle -> E_HANDLE, (3) lookup** | miss -> 0x80070490 |
| GetThemeInt | same function body as GetThemeColor (identical RVA 0x1cf80, ordinals 154/159) | |
| GetThemeMetric(hTheme,hdc,part,state,prop,piVal) | piVal NULL -> E_POINTER, handle -> E_HANDLE, then it is **GetThemeColor lookup followed by `ScaleThemeSize(hdc)`** (so the stored int is scaled by dpi) | 0x80070490 on miss |
| GetThemeString / GetThemeFilename | `pszBuff == NULL && cch != 0` -> E_POINTER (note: `cch==0` with NULL buffer is allowed), handle, lookup, `SafeStringCchCopyW` (truncation -> its HRESULT) | |
| GetThemeFont(hTheme,hdc,part,state,prop,pFont) | pFont NULL -> E_POINTER; handle; lookup miss -> **0x80070490**; property found but its record type byte is not `0xD2` (font) -> **E_INVALIDARG**; font LOGFONT then scaled with `ScaleThemeFont(hdc)` | ETW start/stop events |
| GetThemeMargins(hTheme,hdc,part,state,prop,prc,pMargins) | pMargins NULL -> E_POINTER; handle; `iPropId == 10000` is a pseudo id (not in Wine's `vssym32.h`; public name UNKNOWN): with `prc != NULL` -> E_INVALIDARG, else the property is rewritten to **0xe12 (3602 = TMT_CONTENTMARGINS)** and the result additionally scaled by the object's dpi via `ScaleMargins`; lookup; **if the part is an image-file part with zero margins stored, margins are re-computed from the image** (`SelectCorrectImageFile`+`GetDrawnImageSize`+`ScaleMargins`) | |
| GetThemeIntList | pIntList NULL -> E_POINTER; handle; list longer than 402 -> 0x80070490 | |
| GetThemeBitmap(hTheme,part,state,prop,flags,phBitmap) | phBitmap NULL -> E_POINTER; handle; part object; if the part is a *border-fill* part (type 1) -> **E_FAIL**; else `CImageFile::GetBitmap` | |
| GetThemePropertyOrigin | origin NULL -> E_POINTER; handle; `propId==0` or no stream -> E_FAIL | |
| IsThemePartDefined(hTheme,part,state) -> BOOL | handle error -> returns 0 and **LastError = 0x80070006**; else `SetLastError(hr of GetPropertyOrigin)`; TRUE iff origin == PO_PART (state 0) / PO_STATE (state != 0) | ETW event on entry |
| IsThemeBackgroundPartiallyTransparent -> BOOL | handle error -> FALSE + LastError; border-fill: TRUE iff the part fields at `+8` or `+0xc` are non-zero (field meaning not labelled in the decomp); image: the stored `transparent` flag | |
| GetThemeTransitionDuration(hTheme,part,from,to,prop,pdw) | handle; `pdw==NULL`, `from<=0`, `to<=0` -> E_INVALIDARG; property is an INTLIST (matrix: `iValues[0]`=N states, entry at `[(from-1)*N+to]`); `from`/`to` > N -> E_INVALIDARG; list count==1 -> duration 0 | miss -> 0x80070490 |
| GetThemeAnimationProperty / Transform / TimingFunction | `hTheme` NULL is tolerated until the NULL-object check -> E_INVALIDARG; buffer too small -> **0x800700EA** with required size returned; `memcpy_s` failure E_FAIL | undocumented in SDK headers |

Hierarchy fallback is *inside* `GetValueIndex`: requested `(part, state)` out of the part table range uses the part table's default; same for state; then the base-class chain. `GetThemeColor(part=0,state=0,prop)` is the class-level value.

### 6.1 `GetThemePartSize(hTheme, hdc, part, state, prc, eSize, psz)` [V `0x14a20`, `CImageFile::GetPartSize @0x14edc`]

1. Handle invalid -> **E_HANDLE**.
2. `psz == NULL` -> **E_POINTER** (note: handle check is *before* the pointer check here, the opposite of the property getters).
3. `GetPartObject<CDrawBase>(part,state)`; failure -> E_FAIL (0x80004005); part/state ids are clamped `<0 -> 0`, out-of-range falls back to the table default.
4. **Border-fill part** (`type==1`): `TS_MIN(0)` -> `cx=cy=max(1, 2*borderSize)`; `TS_TRUE(1)` -> `cx=cy=2*borderSize+1`; `TS_DRAW(2)` (or any other) -> **E_INVALIDARG** (borderSize = `part+0x14`).
5. **Image-file part**: `TS_MIN`: `GetScaledContentMargins` -> `cx = max(1, left+right)`, `cy = max(1, top+bottom)` (margins re-scaled to the DPI-selected image); `TS_TRUE`: the selected DIB's *frame* width/height (`DIBINFO+0xc/+0x10`) unless a true-size stretch is active; `TS_DRAW`: `GetDrawnImageSize` = the size the image would be drawn into `prc` (requires `prc`; the image is scaled by `prc` when "sizing type" stretch; `*(this+0xa4)` aspect-preserve option); other enum -> E_INVALIDARG. The DIB chosen depends on `prc`, `hdc` DPI and the available plateaus (`SelectCorrectImageFile`). **A theme without images for the part therefore cannot answer TS_TRUE.**

So HEADERITEM size in a themed Win10 comes from the image strip / content margins in aero.msstyles, selected for the DPI of the process/window; the consumer (comctl32 `Header_NewFont @0x86174` calls `GetThemeTextExtent(hTheme, hdc, 1, 0, L"M", -1, 0x800, ...)` and falls back to `GetTextExtentPointW(L"...")` when the theme text-extent call fails [V from comctl32 decomp via API]). Whether the height difference 24 vs 20 comes from text extent, content margins or `GetThemePartSize` is the survey/probe role's job; this document only fixes the uxtheme side behaviour.

### 6.2 Draw/measure group [V]

| function | checks (in order) |
|---|---|
| DrawThemeBackground(hTheme,hdc,part,state,pRect,pClip) | handle -> `hdc==NULL` -> **E_HANDLE** -> `pRect==NULL` -> E_POINTER -> if `pClip` and `!IntersectRect(rect,clip)` -> returns S_OK (nothing to draw) -> part object (E_FAIL if no part) -> border-fill / image draw. Builds a `DTBGOPTS{dwSize=0x18, dwFlags=1 if clip, rcClip}` |
| DrawThemeBackgroundEx(...,pOptions) | handle -> hdc NULL **E_HANDLE** -> rect NULL E_POINTER -> `pOptions != NULL && dwSize != 0x18` -> **E_FAIL** |
| DrawThemeText(hTheme,hdc,part,state,pszText,cch,flags,flags2,pRect) | `hdc NULL` -> E_HANDLE; `pszText==NULL` with `cch == -1 || cch != 0` or `pRect==NULL` -> E_POINTER; handle; part object (text-draw class `CTextDraw`) -> `CTextDraw::DrawTextW` |
| DrawThemeTextEx(...,pRect,pOptions) | same as above; `pOptions != NULL` requires `dwSize` in **[0x38, 0x48]** else **E_FAIL**; `DTTOPTS` |
| DrawThemeEdge(hTheme,hdc,part,state,pDestRect,uEdge,uFlags,pContentRect) | hdc NULL -> E_HANDLE; pDestRect NULL -> E_POINTER; handle; part -> `_DrawEdge` with the part's 5 colours (`CTextDraw` +8..+0x18) |
| DrawThemeIcon(hTheme,hdc,part,state,pRect,himl,idx) | builds `DrawThemeIconEx` request (`cbSize 0x58`, flags 1) -> `DrawThemeIconEx` (ordinal 72); the HIMAGELIST helpers go through `g_pfnsUxCtrl` (comctl32 v6 via uxtheme's own activation context) |
| GetThemeBackgroundContentRect(hTheme,hdc,part,state,pBounding,pContent) | handle; either rect NULL -> E_POINTER; border-fill: `content = bounding` inset by the part's border margins `+0x2c,+0x30,+0x34,+0x38`; image: `bounding` inset by the (scaled) `contentMargins` |
| GetThemeBackgroundExtent | the inverse; both pointers checked **before** the handle |
| GetThemeBackgroundRegion(hTheme,hdc,part,state,pRect,phrgn) | rect or region NULL -> E_POINTER; **empty rect -> `*phrgn = NULL`, returns S_FALSE (1)**; handle; border-fill/image region |
| HitTestThemeBackground(hTheme,hdc,part,state,opts,pRect,hrgn,ptTest,pwHit) | handle first; `pRect==NULL || pwHit==NULL` -> E_POINTER; border-fill uses `HitTestRect` over the 4 border margins |
| GetThemeTextExtent / GetThemeTextMetrics | hdc NULL -> E_HANDLE; text NULL (cch != 0) / extent NULL -> E_POINTER; handle; part (text class) -> `CTextDraw::GetTextExtent`; text metrics selects the part's font into a temporary |
| DrawThemeParentBackground(hwnd,hdc,prc) / Ex(flags) | `!IsWindow(hwnd) || hdc==NULL` -> E_HANDLE; if parent prop `0xa915` set -> returns **S_FALSE (1)** (re-entrancy guard); else maps coordinates, sets prop, sends `WM_ERASEBKGND (0x14)` then `WM_PRINTCLIENT (0x318, PRF_ERASEBKGND=4)` to the parent. `dwFlags & 1` = use window rect (DTPB_WINDOWDC), `& 4` = DTPB_USECTLCOLORSTATIC bounds-rect probing |

---

## 7. GetThemeSys* (no part/state; "system" values) [V]

IDs (cross-checked with Wine's `include/vssym32.h`: `TMT_FLATMENUS=1001`, `TMT_SIZINGBORDERWIDTH=1201..TMT_PADDEDBORDERWIDTH=1210`, `TMT_MINCOLORDEPTH=1301`, `TMT_CSSNAME=1401..ALIAS=1404`, `TMT_SCROLLBAR=1601`, fonts `801..809`).

| function | `hTheme==NULL` | valid handle | errors |
|---|---|---|---|
| GetThemeSysColor(hTheme, COLOR_x) -> COLORREF | `GetSysColor` | colour from the section's metric block `[(id+0x641)*4]` (ids 0..30 only; out-of-range ids read colour 0 = the 1601/TMT_SCROLLBAR slot); **under high contrast: `GetSysColor`** | invalid handle -> 0 with LastError E_HANDLE; success `SetLastError(0)` |
| GetThemeSysColorBrush | `GetSysColorBrush` (stock brush, not owned) | `CreateSolidBrush(themed colour)` -> **the caller owns it** (leaks if caller never `DeleteObject`s) | |
| GetThemeSysSize(hTheme, SM_x) -> int | `ClassicGetSystemMetrics` | maps SM_ id via `GetThemeSizeId @0x523c8`: `SM_CXVSCROLL(2)->1202, SM_CYHSCROLL(3)->1203, SM_CXSIZE(30)->1204, SM_CYSIZE(31)->1205, SM_CXFRAME(32)/SM_CYFRAME(33)->1201, SM_CXSMSIZE(52)->1206, SM_CYSMSIZE(53)->1207, SM_CXMENUSIZE(54)->1208, SM_CYMENUSIZE(55)->1209, SM_CXPADDEDBORDER(92)->1210`; value = `ScaleSizeForScreenDpi(metric[(id-1201)])` | unsupported SM id -> returns 0, LastError **0x80070057**; invalid handle -> 0, LastError E_HANDLE |
| GetThemeSysBool(hTheme, id) | only `TMT_FLATMENUS (1001=0x3e9)`: `SystemParametersInfoW(SPI_GETFLATMENU=0x1022)` | stored bool at metric block `+0x3e0` | other ids -> FALSE, LastError 0 |
| GetThemeSysInt(hTheme, id, piValue) | **handle is always opened first: NULL -> E_HANDLE** | only `TMT_MINCOLORDEPTH(1301=0x515)` -> metric block `+0x3f4` | `piValue==NULL` -> E_POINTER; other id -> E_INVALIDARG |
| GetThemeSysString(hTheme, id, buf, cch) | handle required (NULL -> E_HANDLE) | ids `1401..1404` (CSSNAME, XMLNAME, LASTUPDATED, ALIAS) via string-table offsets in the metric block | buf NULL & cch != 0 -> E_POINTER; other id -> E_INVALIDARG |
| GetThemeSysFont(hTheme, id, plf) | classic fallback: ids `801..805,807..809` from `SystemParametersInfoW(SPI_GETNONCLIENTMETRICS 0x29, 0x1f8)` fields (caption/smcaption/menu/status/message), id 806 (`TMT_ICONTITLEFONT`) from `SPI_GETICONTITLELOGFONT (0x1f)` | font table in the metric block, stride **0x5c**, `(id-0x321)*0x5c` | `plf==NULL` -> E_POINTER; id outside `0x321..0x329` -> E_INVALIDARG; SPI failure -> `MakeErrorLast()` |

Everything above reads the "sysmetrics" block that the loader built (colours from the user's `Control Panel\Colors`, metrics from `Control Panel\Desktop\WindowMetrics` at load time; strings `sysmetrics`, `Control Panel\Colors`, `...WindowMetrics` are present in `LOADTHEMEMETRICS`/`InitThemeMetrics @0x40e90`) -> **not obtainable without a loaded theme**.

---

## 8. App- and window-level calls [V]

| call | behaviour |
|---|---|
| `IsThemeActive()` (`@0x224e0`) | uninit -> FALSE + LastError 0x8000FFFF. Else `SetLastError(0)`; HC gate -> FALSE. Then under the app lock: TRUE iff the app's cached theme file exists and its header flag byte has **both bit0 and bit1** set, **or** the root section exists with both stored names non-empty (`name1` at `ROOTSECTION+0`, `name2` at `+0x208`). It does **not** consult `SetThemeAppProperties`, `EnableTheming`, or the hook state. |
| `IsAppThemed()` (`@0x224b0`) | `AppIsThemed && IsSystemThemeActive` (hook state == 0 or `+0x24`; same root-section test as above) **and** `IsThemeActive()`. So `IsAppThemed => IsThemeActive`. Also ignores the app flags. |
| `GetThemeAppProperties()` | returns `CAppInfo+0x20` (default **3**). |
| `SetThemeAppProperties(f)` | stores `f` (section 2.2). Subsequent `Open*` fail per 5 step 3. Existing HTHEMEs are unaffected. |
| `EnableTheming(BOOL)` (`@0x4f7c0`, ordinals 36 and 129 share the body) | **no-op, returns S_OK (0)**; reads nothing, writes nothing. (Wine's version persists `ThemeActive=0` in the registry and flips an internal flag: divergent. [SRC `system.c:564`]) |
| `SetWindowTheme(hwnd, pszSubAppName, pszSubIdList)` (`@0x3bb0`) | `!IsWindow(hwnd)` -> **E_HANDLE** (this includes NULL). Else: both strings NULL/empty -> telemetry; `ApplyStringProp(hwnd, pszSubAppName, 0xa911)` and `(pszSubIdList, 0xa910)`: each deletes the old atom and prop, then if the new string is non-NULL `AddAtomW` and `SetPropW` (an *empty string is stored as the atom for `"$"`*). Because a stored `0xa910` string **replaces the class list** in `MatchThemeClassList`, `SetWindowTheme(h, L"", L"")` makes every later `OpenThemeData(h, ...)` look up the class `$`, which does not exist -> de-themed window. An empty app name alone only fails the app-qualified match and falls back to the plain class. Then `SendMessageW(hwnd, WM_THEMECHANGED (0x31a), wParam = -1, lParam = 0x80000001)`. Returns S_OK. No pointer validation, no HC gate. |
| `GetWindowTheme(hwnd)` | section 4. |
| `SetWindowThemeAttribute(hwnd, eAttribute, pv, cb)` | only `WTA_NONCLIENT (0)` with `cb == 8` accepted, else **E_INVALIDARG**. `pv` = `WTA_OPTIONS{dwFlags, dwMask}`: `new = old ^ ((flags ^ old) & mask)` (old read from prop `0xa914`); if changed: `SetPropW` (fail -> E_FAIL), if composition active `SetWindowCompositionAttribute(hwnd, {10, &new, 4})`, then `SetWindowPos(NOSIZE|NOMOVE|NOZORDER|NOACTIVATE|FRAMECHANGED = 0x37)`. `pv == NULL` is **not** checked (crashes on NULL, [INF]). |
| `EnableThemeDialogTexture(hwnd, dwFlags)` | HC gate passes: `dwFlags & ETDT_DISABLE (1)` -> `RemovePropW(0xa916)`; else `SetPropW(0xa916, (dwFlags & 0xf) | old)`; `SetPropW` failure -> `GetLastError()` mapped to `0x8007xxxx` (or E_FAIL). Returns S_OK otherwise. |
| `IsThemeDialogTextureEnabled(hwnd)` | `GetPropW(0xa916) != NULL`. |
| `GetCurrentThemeName(file,cchFile,color,cchColor,size,cchSize)` | `file==NULL && cchFile != 0` -> E_POINTER; clears `*file`; uninit or HC -> 0x8000FFFF; else opens the app theme file and `GetThemeNameId` copies the three strings stored in the section header (`hdr+0x18/+0x1c/+0x20` string offsets); each copy returns its `SafeStringCchCopyW` HRESULT. Needs the root section; no ALPC. |
| `IsCompositionActive()` | HC gate; else `IsThreadDesktopComposited` (user32) only if connections are allowed and hooks on, else FALSE. |
| `GetThemeDocumentationProperty(themeFile, propName, buf, cch)` | any NULL (or buf NULL with cch != 0) -> E_POINTER; `LoadThemeLibrary(themeFile)` (file read); `GetVSDocProperty` looks in the `documentation` resource; cleanup `FreeLibrary`. |

---

## 9. Buffered paint and animation [V]

Per-thread state: TLS slot (`CThreadObjectManager<CPaintBufferPool>`) holding `{CPaintBufferPool*, refcount}`; a process-wide `CGlobalBufferPool` caches DIB sections and is pruned by tick count.

* `BufferedPaintInit()` -> `CBufferedPaintThreadManager::ThreadAddRef` (creates the TLS data and pool if needed; refcount++); returns S_OK, or **E_FAIL** when the TLS data cannot be allocated (`CThreadObjectManager::ThreadAddRef @0x2061c`). `BufferedPaintUnInit()` -> `ThreadRelease`: no TLS data -> **E_FAIL (0x80004005)**; refcount to 0 destroys the pool; returns S_OK.
* `BeginBufferedPaint(hdcTarget, prcTarget, dwFormat, pPaintParams, phdc)`: `phdc == NULL` -> NULL; `*phdc = NULL`; **implicitly does `ThreadAddRef`** (a missing `BufferedPaintInit` is tolerated; on failure it `ThreadRelease`s and returns NULL). Validation in `CPaintBufferPool::Impl::BeginAnimation @0x1d7d0`: `pPaintParams != NULL && cbSize != 0x18` -> NULL (error 0x80070057 path); `hdc NULL`, `prc NULL`, `IsRectEmpty(prc)`, `dwFormat > 3` (valid `BPBF_COMPATIBLEBITMAP 0, DIB 1, TOPDOWNDIB 2, TOPDOWNMONODIB 3`), `pPaintParams->prcExclude` present and empty -> NULL. The returned `HPAINTBUFFER` is looked up by pointer in a per-thread table.
* `BeginBufferedAnimation(hwnd, hdcTarget, prc, fmt, pPaintParams, pAnimParams, phdcFrom, phdcTo)` -> `CPaintBufferPool::BeginAnimation @0x202e8`: **`pAnimParams == NULL` -> NULL**; `StopConflicts(hwnd, rc)`; `pAnimParams->cbSize != 0x10` or `style > 3` (`BPAS_NONE 0, LINEAR 1, CUBIC 2, SINE 3`) -> NULL.
* `EndBufferedPaint(h, fUpdate)`: **`h == NULL` -> E_INVALIDARG**; no TLS pool or `h` not in the thread's table -> E_FAIL; else paints/updates and releases (the decomp then conditionally removes the buffer from the table when `buffer+0x20 == 1`; whether that drops the implicit `ThreadAddRef` is [INF], not read).
* `EndBufferedAnimation`, `GetBufferedPaintDC/TargetDC/TargetRect/Bits`, `BufferedPaintClear/SetAlpha`: all look up `h` in the **calling thread's** table; unknown handle, other thread's handle or `h==NULL` -> E_FAIL (HDC getters return NULL). `GetBufferedPaintTargetRect(h, prc==NULL)` -> E_POINTER. `GetBufferedPaintBits(h, ppb, pcx)` -> `GetBufferedPaintBitsEx(h, ppb, pcx, NULL)` (ordinal 131): **`ppb == NULL || pcx == NULL` -> E_POINTER (before the handle lookup)**, unknown/NULL handle -> E_FAIL; the Ex form additionally returns a format DWORD through the 4th parameter via a virtual call.
* `BufferedPaintStopAllAnimations(hwnd)`: no TLS pool -> E_FAIL; else stops animations targeting hwnd; `BufferedPaintRenderAnimation(hwnd, hdc)` returns BOOL (Wine spec signature; body not read in detail - [INF] TRUE when an animation frame was rendered).
* Theme-driven animation data: `GetThemeTransitionDuration` (TMT_TRANSITIONDURATIONS=6000 matrix, section 6) and the undocumented `GetThemeAnimationProperty/Transform/TimingFunction` (ordinals 146/147/178) read `animation`/`timingfunction` resources; a descriptor table of 9 `{propertyId, structOffset, size}` triples sits at RVA 0x693f0 [V-reg]: ids 0,1,2,3,4,5 -> offsets 0,4,8,12,16,20, and ids 256,257,258 -> offsets 24,28,32, each size 4 (so `GetThemeAnimationProperty` copies 4-byte fields of an `AnimationProperty` record; other ids leave `*pcbSize = 0` and return the E_INVALIDARG initial value).

---

## 10. Dark mode and immersive colour ordinals (all undocumented) [V]

Correction to the task brief: in 19041 **ordinal 133 = AllowDarkModeForWindow**, 135 = `SetPreferredAppMode` (the older `AllowDarkModeForApp(BOOL)`; now takes the enum), as in Wine's spec (`uxtheme.spec`, 132/133/135/136/137/138).

| ordinal | name | behaviour |
|---|---|---|
| 132 `ShouldAppsUseDarkMode()` | returns `g_preferredAppMode != ForceLight(3) && (g_preferredAppMode == ForceDark(2) \|\| !g_isSystemAppModeLight)`. Note it does **not** require `AllowDark`; it answers "would apps be dark" from the registry (`...\Themes\Personalize\AppsUseLightTheme`, fallback `...\Themes\UWPAppsUseLightTheme`, default light) |
| 133 `AllowDarkModeForWindow(hwnd, BOOL)` | `GetPropW(0xa91e)` previous; `ApplyStringProp(hwnd, allow ? L"Enabled" (string at RVA 0x67d60 [V-reg]) : NULL, 0xa91e)`; returns previous state (BOOL). No IsWindow check. |
| 135 `SetPreferredAppMode(mode)` | atomic swap of `g_preferredAppMode`, returns the previous value. Enum: 0 Default, 1 AllowDark, 2 ForceDark, 3 ForceLight, (4 Max) [names from public reverse-engineering, not from this binary; the numeric tests in 132/137/139 confirm 1,2,3 roles]. No range check. Initial value 0 [V-reg: BSS]. |
| 136 `FlushMenuThemes()` | clears the per-window/DPI `CThemeMenuMetrics` map |
| 137 `IsDarkModeAllowedForWindow(hwnd)` | mode 1 or 2: if `hwnd != NULL` -> prop `0xa91e != NULL`, else TRUE; **any other mode (0, 3, 4) -> FALSE** |
| 138 `ShouldSystemUseDarkMode()` | `HKCU\...\Personalize\SystemUsesLightTheme` else `HKLM\...\Themes\SystemUsesLightTheme` else `IsOS(OS_PERSONAL)`; returns `value == 0` |
| 139 `IsDarkModeAllowedForApp()` | `g_preferredAppMode - 1 < 2` i.e. mode 1 or 2 |
| 104 `RefreshImmersiveColorPolicyState()` | re-reads policy `Software\Policies\Microsoft\Windows\Personalization\PersonalColors_Background`, `NoChangingStartMenuBackground`; may broadcast `WM_SETTINGCHANGE (0x1a)` with `"ImmersiveColorSet"`; refreshes `g_isSystemAppModeLight`; flushes menu metrics if mode 1/2 |
| 106 `GetIsImmersiveColorUsingHighContrast(int refresh)` | cached SPI_GETHIGHCONTRAST |
| 94/117 `GetImmersiveColorSetCount()` | constant **0x32 (50)** |
| 95 `GetImmersiveColorFromColorSetEx(set, type, ignoreHC, hcMode)` | set `< 0x32` -> preset; else the user preference; `GetColorFromPreferenceImpl` |
| 96 `GetImmersiveColorTypeFromName(wstr)` | strings shorter than 10 chars -> `0xFFFFFFFF`; else bsearch of `name+9` chars (skips `Immersive`) in a 0x4cb(1227)-entry table |
| 98 `GetImmersiveUserColorSetPreference(bForceCheck, bSkipCheck)` | returns **100** after filling the preference |
| 100 `GetImmersiveColorNamedTypeByIndex(i)` | `i < 0x4cb` -> table entry ptr else NULL |
| 119/120/121/122/118 | `GetDefaultColorPreference`, `GetUserColorPreference`, `GetColorFromPreference`, `SetUserColorPreference`, `GetColorPreferencePreset`; shared memory `Local\SessionImmersiveColorPreference` (8 bytes) + mutex `Local\SessionImmersiveColorMutex` [V `EnsureInitialized @0xd5f8`] |

None of these require a theme file; they need registry, a session shared-memory mapping, and (for window state) user32 window properties.

---

## 11. Other ordinal groups worth knowing [V]

* **Panning feedback (5, 6, 12)**: one thread/window at a time (`DAT_171b93f1c` owner thread id, `DAT_171b93f20` hwnd). `BeginPanningFeedback(hwnd)` -> BOOL (0 if another thread owns it or alloc fails); `UpdatePanningFeedback(hwnd, totalX, totalY, bInertia)` and `EndPanningFeedback(hwnd, bAnimateBack)` act only if the *calling thread* is the owner and `hwnd` matches, else return 0. Slides the window with `SetWindowPos`-style displacement (`CWindowFeedBackAnimation`).
* **Classic-metrics trampolines (43, 44, 45, 46)**: call the saved user32 hook function if hooks are on (`g_eThemeHookState == 0` and the saved table pointer non-null), otherwise `GetSystemMetrics` / `SystemParametersInfoW` between `ENTER/LEAVE_CLASSICSYSMETCALL`. They exist so themed code gets the *unthemed* metric.
* **App-compat**: `AddThemeAppCompatFlags(f1,f2)` ORs two globals, `ResetThemeAppCompatFlags()` zeroes them.
* **File-based (7-10, 69)**: `GetThemeDefaults(file, colorBuf, cchColor, sizeBuf, cchSize)` (file NULL -> E_POINTER) reads the default colour/size variant names from the msstyles `VARIANT` data; `EnumThemes(dir, cb, lp)` walks `<dir>\*.*` for `<name>\<name>.msstyles`.
* `OpenThemeFileFromData(hTheme, CUxThemeFile** out)` (17): handle check then `*out = renderObj->themeFile`; NULL out -> E_POINTER. `OpenThemeDataFromFile(themeFile, hwnd, classList NULL->"globals", int ncFlag)` (16): NULL file -> E_POINTER (SetLastError), non-window hwnd -> E_HANDLE.
* `DumpLoadedThemeToTextFile` (15) returns 0x80004001 (E_NOTIMPL).
* `RegisterDefaultTheme(file, int)` (14) writes `HKLM\Software\Microsoft\Windows\CurrentVersion\ThemeManager` values `LoadedBefore=0, LMVersion=0x69, LMOverRide, ThemeActive=1, DllName`.
* `IsThemeActiveByPolicy` (73): `HKCU...\ThemeManager\ThemeActive` combined with a group-policy check (`CThemeServices::ThemeEnforcedByPolicy`).
* `GetThemeParseErrorInfo(PARSE_ERROR_INFO*)` (48): NULL -> E_POINTER else copies last parser error.
* `IsValidShakeWindow` (86), `ThemeForwardCapturedMouseMessage` (75), `ClearDpiInfo` (128): internal helpers for shell shake/caption-button capture; not API surface.

---

## 12. Complete ordinal inventory (108 ordinal-only exports)

Names are the PDB function names resolved from the export RVA (`/api/modules/uxtheme/exports`, `/api/function/uxtheme/<rva>`), so they are real symbol names, not guesses. Cross-checked against Wine's `uxtheme.spec` XP-era numbering: Win10 agrees for 1, 7-10, 36, 95, 96, 98, 100, 104, 132-138 and disagrees elsewhere (Wine's 2-4, 11, 13, 15-35 are XP names that are SHUNIMPL forwarders in Win10). Ordinals with no description in the last column were read only as far as their name; **their signature is UNKNOWN** unless stated.

| ord | RVA | symbol | notes |
|---|---|---|---|
| 1 | 0x50f70 | `QueryThemeServices` | QueryThemeServices() -> DWORD; ALPC op 1 |
| 2 | 0x8daf4 | SHUNIMPL forwarder `SHUNIMPL.#440` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 3 | 0x8da84 | SHUNIMPL forwarder `SHUNIMPL.#441` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 4 | 0x8da68 | SHUNIMPL forwarder `SHUNIMPL.#445` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 7 | 0x4fec0 | `GetThemeDefaults` | GetThemeDefaults(themeFile,colorBuf,cch,sizeBuf,cch) |
| 8 | 0x4fad0 | `EnumThemes` | EnumThemes(dir,cb,lp) |
| 9 | 0x4f7f0 | `EnumThemeColors` | EnumThemeColors(file,sizeName,idx,out) |
| 10 | 0x4f9f0 | `EnumThemeSizes` | EnumThemeSizes(file,colorName,idx,out) |
| 11 | 0x8db02 | SHUNIMPL forwarder `SHUNIMPL.#190` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 13 | 0x8da92 | SHUNIMPL forwarder `SHUNIMPL.#444` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 14 | 0x51030 | `RegisterDefaultTheme` | RegisterDefaultTheme(file,int): writes HKLM `...\\ThemeManager` LoadedBefore=0, LMVersion=0x69, LMOverRide, ThemeActive=1, DllName (decomp 0x51030) |
| 15 | 0x4f7b0 | `DumpLoadedThemeToTextFile` | DumpLoadedThemeToTextFile: returns 0x80004001 (E_NOTIMPL) unconditionally (decomp 0x4f7b0) |
| 16 | 0x50e20 | `OpenThemeDataFromFile` | OpenThemeDataFromFile(void* themeFile, HWND, class (NULL->"globals"), int) |
| 17 | 0x50ef0 | `OpenThemeFileFromData` | OpenThemeFileFromData(hTheme, CUxThemeFile** out): E_POINTER if out NULL, else `*out` = the theme file of the render object |
| 18 | 0x8dabc | SHUNIMPL forwarder `SHUNIMPL.#189` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 19 | 0x8daae | SHUNIMPL forwarder `SHUNIMPL.#188` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 20 | 0x8dbaa | SHUNIMPL forwarder `SHUNIMPL.#395` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 21 | 0x8dbb8 | SHUNIMPL forwarder `SHUNIMPL.#396` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 22 | 0x8dbe2 | SHUNIMPL forwarder `SHUNIMPL.#397` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 23 | 0x8dbd4 | SHUNIMPL forwarder `SHUNIMPL.#398` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 24 | 0x8db56 | SHUNIMPL forwarder `SHUNIMPL.#399` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 25 | 0x8db80 | SHUNIMPL forwarder `SHUNIMPL.#400` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 26 | 0x8daa0 | SHUNIMPL forwarder `SHUNIMPL.#215` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 27 | 0x8dbc6 | SHUNIMPL forwarder `SHUNIMPL.#401` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 28 | 0x8db8e | SHUNIMPL forwarder `SHUNIMPL.#402` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 29 | 0x8da76 | SHUNIMPL forwarder `SHUNIMPL.#213` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 30 | 0x8db72 | SHUNIMPL forwarder `SHUNIMPL.#403` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 31 | 0x8db2c | SHUNIMPL forwarder `SHUNIMPL.#431` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 32 | 0x8db1e | SHUNIMPL forwarder `SHUNIMPL.#406` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 33 | 0x287f0 | `Remote_ReestablishServerConnection` | Remote_ReestablishServerConnection (ALPC op 0x3ef) |
| 34 | 0x8db3a | SHUNIMPL forwarder `SHUNIMPL.#407` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 35 | 0x8db48 | SHUNIMPL forwarder `SHUNIMPL.#408` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 36 | 0x4f7c0 | `EnableTheming` | alias of EnableTheming (same RVA 0x4f7c0) |
| 43 | 0x18780 | `ClassicGetSystemMetrics` | ClassicGetSystemMetrics(int) |
| 44 | 0x30f0 | `ClassicSystemParametersInfoA` | ClassicSystemParametersInfoA |
| 45 | 0x2da0 | `ClassicSystemParametersInfoW` | ClassicSystemParametersInfoW |
| 46 | 0x4b7e0 | `ClassicAdjustWindowRectEx` | ClassicAdjustWindowRectEx |
| 48 | 0x50210 | `GetThemeParseErrorInfo` | GetThemeParseErrorInfo(PARSE_ERROR_INFO*) |
| 49 | 0xbb20 | `OpenNcThemeData` | OpenNcThemeData(hwnd,classes) -> OpenThemeData with OTD_NONCLIENT path |
| 50 | 0x50bf0 | `IsThemeClassDefined` | IsThemeClassDefined(hTheme,appName,className,flag) |
| 60 | 0x4f3b0 | `CreateThemeDataFromObjects` | CreateThemeDataFromObjects(CDrawBase*,CTextDraw*,flags): builds an HTHEME with no theme file around caller-supplied draw objects (decomp 0x4f3b0) |
| 62 | 0x8db10 | SHUNIMPL forwarder `SHUNIMPL.#409` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 63 | 0x8db9c | SHUNIMPL forwarder `SHUNIMPL.#404` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 64 | 0x4b500 | `Process_LoadTheme_RunDLLW` | Process_LoadTheme_RunDLLW (rundll32 entry) |
| 65 | 0x511a0 | `SetSystemVisualStyle` | SetSystemVisualStyle(file,color,size,flags) |
| 66 | 0x8db64 | SHUNIMPL forwarder `SHUNIMPL.#405` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 67 | 0x4f390 | `AddThemeAppCompatFlags` | AddThemeAppCompatFlags(ulong,ulong) |
| 68 | 0x51180 | `ResetThemeAppCompatFlags` | ResetThemeAppCompatFlags() |
| 69 | 0x4f920 | `EnumThemeProperties(ctx): LoadThemeLibrary(file)+VSEnum` | EnumThemeProperties(ctx): LoadThemeLibrary(file)+VSEnum |
| 72 | 0x4f560 | `DrawThemeIconEx` | DrawThemeIconEx |
| 73 | 0x50b90 | `IsThemeActiveByPolicy` | IsThemeActiveByPolicy |
| 74 | 0x4fe30 | `GetThemeClass` | GetThemeClass(hTheme,buf,cch) |
| 75 | 0x4ba40 | `ThemeForwardCapturedMouseMessage` | ThemeForwardCapturedMouseMessage(hwnd,msg,wp,lp) |
| 76 | 0x286a0 | `EnableServiceConnection` | EnableServiceConnection(int) |
| 77 | 0x8dad8 | SHUNIMPL forwarder `SHUNIMPL.#432` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 78 | 0x8dc28 | SHUNIMPL forwarder `SHUNIMPL.#433` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 79 | 0x8dbf0 | SHUNIMPL forwarder `SHUNIMPL.#434` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 80 | 0x8dc36 | SHUNIMPL forwarder `SHUNIMPL.#435` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 81 | 0x8dbfe | SHUNIMPL forwarder `SHUNIMPL.#430` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 82 | 0x8dc0c | SHUNIMPL forwarder `SHUNIMPL.#436` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 83 | 0x8dc1a | SHUNIMPL forwarder `SHUNIMPL.#437` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 84 | 0x273e0 | `ClearTheme` | ClearTheme |
| 85 | 0x42c10 | `ClassicWndProc` | ClassicWndProc |
| 86 | 0x4bd80 | `IsValidShakeWindow` | IsValidShakeWindow(hwnd) |
| 87 | 0x8daca | SHUNIMPL forwarder `SHUNIMPL.#438` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 88 | 0x8dae6 | SHUNIMPL forwarder `SHUNIMPL.#439` | export is a forwarder string to module `SHUNIMPL`, not code in uxtheme (module SHUNIMPL itself not examined; treat as unimplemented placeholder) |
| 90 | 0x42920 | `SetSystemMetrics` | SetSystemMetrics |
| 91 | 0x25460 | `SendAcquireOrAddTheme` | SendAcquireOrAddTheme (ALPC 0x3ed) |
| 92 | 0x4c1b0 | `LoaderLoadTheme` | LoaderLoadTheme |
| 93 | 0x25650 | `ApplyThemeSections` | ApplyThemeSections |
| 94 | 0x3e440 | `GetImmersiveColorSetCount` | GetImmersiveColorSetCount() -> 0x32 |
| 96 | 0x6c50 | `GetImmersiveColorTypeFromName` | GetImmersiveColorTypeFromName(wstr) |
| 97 | 0x3e540 | `SetImmersiveUserColorSetPreferenceEx` | SetImmersiveUserColorSetPreferenceEx |
| 99 | 0x287d0 | `InitializeImmersiveColorSetSharedMemory` | InitializeImmersiveColorSetSharedMemory |
| 100 | 0x3e450 | `GetImmersiveColorNamedTypeByIndex` | GetImmersiveColorNamedTypeByIndex(uint) |
| 101 | 0x28800 | `SetImmersiveColorSetSharedMemoryACLforUser` | SetImmersiveColorSetSharedMemoryACLforUser |
| 102 | 0x3e510 | `SetImmersiveColorSetInSharedMemory` | SetImmersiveColorSetInSharedMemory |
| 104 | 0x7570 | `RefreshImmersiveColorPolicyState` | RefreshImmersiveColorPolicyState() |
| 105 | 0x3e490 | `GetImmersiveDefaultColorSet` | GetImmersiveDefaultColorSet |
| 106 | 0xd3f0 | `GetIsImmersiveColorUsingHighContrast` | GetIsImmersiveColorUsingHighContrast(int) |
| 107 | 0x3e5d0 | `GetCustomColorSet` | GetCustomColorSet |
| 108 | 0x3e5a0 | `GetCustomAccentPath` | GetCustomAccentPath |
| 109 | 0x3e600 | `GetIsImmersiveExpression` | GetIsImmersiveExpression |
| 110 | 0x23440 | `BufferChanged` | BufferChanged |
| 111 | 0x3e570 | `GetCurrentExpressionId` | GetCurrentExpressionId |
| 112 | 0x3e5c0 | `GetCustomAccentTopRatio` | GetCustomAccentTopRatio |
| 113 | 0x3e590 | `GetCustomAccentLoadStatus` | GetCustomAccentLoadStatus |
| 115 | 0x4b860 | `InvalidateAppTheme` | InvalidateAppTheme |
| 116 | 0x4c390 | `SendReleaseTheme` | SendReleaseTheme (ALPC 0x3ee) |
| 117 | 0x3e440 | `GetImmersiveColorSetCount` | alias of 94 (same RVA 0x3e440) |
| 118 | 0x3e410 | `GetColorPreferencePreset` | GetColorPreferencePreset |
| 119 | 0xba40 | `GetDefaultColorPreference` | GetDefaultColorPreference(ulong*) |
| 122 | 0x273d0 | `SetUserColorPreference` | SetUserColorPreference |
| 123 | 0x3e4c0 | `SetImmersiveColorPreferenceInSharedMemory` | SetImmersiveColorPreferenceInSharedMemory |
| 124 | 0x3e480 | `GetImmersiveColorXamlInfoByIndex` | GetImmersiveColorXamlInfoByIndex |
| 125 | 0x26940 | `GetGroupPolicyColors` | GetGroupPolicyColors |
| 126 | 0x1a9e0 | `DrawTextWithGlow` | DrawTextWithGlow |
| 127 | 0x4c320 | `LoaderLoadThemeForTesting` | LoaderLoadThemeForTesting |
| 128 | 0x25470 | `ClearDpiInfo` | ClearDpiInfo |
| 130 | 0x3e4b0 | `OverrideColorCurrentUser` | OverrideColorCurrentUser |
| 131 | 0x1c9d0 | `GetBufferedPaintBitsEx` | GetBufferedPaintBitsEx |
| 132 | 0x19e10 | `ShouldAppsUseDarkMode` | ShouldAppsUseDarkMode() |
| 133 | 0x25180 | `AllowDarkModeForWindow` | AllowDarkModeForWindow(hwnd,BOOL) -> prev BOOL |
| 135 | 0x252b0 | `SetPreferredAppMode` | SetPreferredAppMode(PreferredAppMode) -> previous |
| 136 | 0x24f70 | `FlushMenuThemes` | FlushMenuThemes() |
| 137 | 0x24e40 | `IsDarkModeAllowedForWindow` | IsDarkModeAllowedForWindow(hwnd) |
| 138 | 0xd1a0 | `ShouldSystemUseDarkMode` | ShouldSystemUseDarkMode() |
| 139 | 0x76a0 | `IsDarkModeAllowedForApp` | IsDarkModeAllowedForApp() |

Duplicated RVAs (one body, several ordinals): 36 = 129 (`EnableTheming`), 94 = 117 (`GetImmersiveColorSetCount`), 154 = 159 (`GetThemeColor` = `GetThemeInt`).

---

## 13. Scenario matrix (what real Win10 returns)

`T` = normal themed Win10 session, service + sections present. `NT` = no theme sections for the session. `HC` = high contrast on, app not opted in. `SP0` = `SetThemeAppProperties(0)`. `NS` = ALPC service absent, sections present. `UM` = comctl32 v5 / app not manifested for v6 (see note).

| call | T | NT | HC | SP0 | NS |
|---|---|---|---|---|---|
| IsThemeActive | TRUE, LE 0 | FALSE, LE 0 | FALSE, LE 0 | TRUE | TRUE |
| IsAppThemed | TRUE | FALSE | FALSE | TRUE | TRUE |
| GetThemeAppProperties | 3 | 3 | 3 | 0 | 3 |
| OpenThemeData(h,L"Button") | handle, LE 0 | NULL, LE 0x80070490 (or section-open error) | NULL, LE unchanged | NULL, LE 0x80070490 | handle |
| OpenThemeData(h,L"NoSuchClass") | NULL, LE 0x80070490 | NULL | NULL | NULL | NULL, 0x80070490 |
| OpenThemeData(h,NULL) | NULL, LE 0x80004003 | same (checked first) | NULL (HC gate first, LE untouched) | same | same |
| OpenThemeData(h,L"") | NULL, LE 0x80070057 | NULL, LE 0x80070057 (empty-list check precedes any section access) | NULL, LE untouched | NULL, LE 0x80070490 (flag gate first) | NULL, LE 0x80070057 |
| OpenThemeData(badHwnd,...) | NULL, LE 0x80070006 | same | NULL | same | same |
| CloseThemeData(valid / NULL / stale) | S_OK / E_HANDLE / E_HANDLE | | | | |
| any getter with stale/NULL HTHEME | E_HANDLE (after the E_POINTER check for property getters) | | | | |
| GetThemeSysColor(NULL,i) | GetSysColor(i) | GetSysColor | GetSysColor | | |
| EnableTheming(any) | S_OK, no effect | S_OK | S_OK | S_OK | S_OK |
| IsCompositionActive | DWM state | FALSE | FALSE | | FALSE |
| QueryThemeServices | status DWORD | LE 0x8007041D if no connection | | | returns 0, LE 0x8007041D |

**UM (comctl32 v5 / unmanifested app)** is *not decided inside uxtheme*: uxtheme does not look at the host manifest. comctl32 v5 never calls `OpenThemeData` at all (its controls draw classic); comctl32 v6 calls it from each control's `InitTheme` and keeps `NULL` on failure, falling back to classic drawing. The only manifest-related behaviour in uxtheme is its own private activation context for the HIMAGELIST helpers (section 2.1). [V for uxtheme side; the comctl32 side belongs to the consumer survey.]

**Classic theme**: Windows 10 has no classic visual style; the "classic" experience is high contrast (HC row) or `SetWindowTheme(h, L"", L"")` per window (`SetWindowTheme(h, L"", L"")` -> prop `0xa910` = atom `"$"` replaces the class list -> class lookup fails -> `OpenThemeData` returns NULL with LastError 0x80070490 for that window).

**Cache / handle lifetime across theme change**: `WM_THEMECHANGED (0x31a)` is sent to windows by `SendThemeChangedMsg`/`SetWindowTheme` (and `ThemeHooksOn @0x26388` enumerates windows with message code 0x31b, identity of which was not established); `NewThemeCheck(-1)` / `CAppInfo::ResetAppTheme` invalidates `CAppInfo+8` and the sections-refcounted file; **existing HTHEME values stay valid** (they hold their own ref on the old theme file via `BumpRefCount`) until `CloseThemeData`. Consumers must re-open on `WM_THEMECHANGED`. [V structure]

---

## 14. Themeless-by-design: what to return and where it diverges

"Themeless by design" in the Industrial7 sense (`/home/iceisfun/work/industrial7/userland/guest/uxtheme-w10`, crate `w10-uxtheme-off`, per its `glue.rs` comments): `IsThemeActive/IsAppThemed` FALSE with LastError 0; `OpenThemeData*` NULL with LastError `0x80004003` for a NULL class list else `0x80070490`; `CloseThemeData` and every handle-taking HRESULT call return E_HANDLE; `IsCompositionActive` FALSE.

Divergences from real Win10 (each is checked against the decomp above):

1. **Default `GetThemeAppProperties`: crate uses 7; Win10 is 3** (`CAppInfo::CAppInfo @0x37ac`). Probe it.
2. **`EnableTheming`**: Win10 is a no-op S_OK; a themeless implementation must not model disabling (matches the existing "off" answer; do not copy Wine's registry write).
3. **`IsThemeActive/IsAppThemed`** are FALSE for a themeless implementation but TRUE for every ordinary Win10 desktop process; this is *the* divergence that makes comctl32 v6 skip its themed paths (header height 20 vs 24 etc.).
4. **Uninitialised vs initialised**: Win10 returns `LastError 0x8000FFFF` only before DLL init; irrelevant after load.
5. **`OpenThemeData` LastError on success is 0** (decomp always `SetLastError(hr)`), and on the HC path LastError is untouched; the crate sets 0x80070490 for the "no theme" case, which matches Win10's NT row but not its HC row.
6. **`GetThemeSysColor/Size/Bool/Int/String/Font(hTheme=NULL, ...)`** fall back to the classic user32 values on Win10; for a *non-NULL* handle they read the section's sysmetrics block. A themeless DLL never has a non-NULL handle, so the NULL-handle behaviour is the only live one and must forward to `GetSysColor/GetSystemMetrics/SPI` (with `GetThemeSysInt/SysString` returning E_HANDLE for NULL, which is **asymmetric** with SysColor/Size/Font/Bool).
7. **Value-bearing getters**: `GetThemePartSize/GetThemeMetric/GetThemeInt/...` have no answer without a theme (E_HANDLE). Real Win10 answers from aero.msstyles. This is the gap the "armed" personality fills.
8. **`SetWindowTheme`** on Win10 stores atoms (`$` for empty) and sends WM_THEMECHANGED(-1, 0x80000001); returning S_OK without that message leaves comctl32 controls un-notified. The crate already implements the property and message path.
9. **`DrawThemeParentBackground`** semantic on Win10 depends on parent window messages (WM_ERASEBKGND then WM_PRINTCLIENT with flag 4) and returns S_FALSE(1) on re-entry; a themeless impl that returns E_HANDLE for a *valid hwnd* diverges (Win10 only returns E_HANDLE for bad hwnd/hdc).
10. **`GetThemeAnimation*`, `GetThemeTimingFunction`**: need theme resources; E_HANDLE/E_INVALIDARG pattern as in section 6 table.

What an **armed-Win10 personality** must reproduce (design input for the constants-table role, from the facts above): (a) a `CRenderObj`-like handle table with the `(gen<<16)|slot` rule and E_HANDLE/E_POINTER ordering; (b) class-list parsing (`;` list, `App::Class`, SubIdList override, `DarkMode_` mapping) over a *static class directory*; (c) the three-level `(class, part, state, propId)` lookup with part/state default fallbacks and base-class chain (`GetValueIndex` rules), typed values (int/color/enum/bool 4 or 1 bytes, margins, rect, position, intlist<=402, string, font index); (d) per-part draw type (border-fill vs image) with `GetPartSize` TS_MIN/TS_TRUE/TS_DRAW formulas; (e) DPI plateau selection (96 / 120 / 144 / 192 ...) for image parts and `MulDiv(v, dpi, 96)` for metrics and sysmetrics; (f) sysmetrics block: colours 0..30, 9 size slots (1201..1210), flat-menus bool, min-colour-depth int, 4 strings, 9 fonts; (g) transition-duration matrices. None of the **numbers** are in this document: values must be recorded on Win10 (see the probe list from the probe role) or extracted by a reader of the msstyles `CMAP/VMAP/...` resources.

---

## 15. Open items / UNKNOWN

* Exact binary layout of a property record header and of `DIBINFO`/`CImageFile` fields beyond those used above (offsets quoted are as observed in the decomp of the accessors; the struct is not published).
* Which user32 command codes `ThemeInitApiHook` receives for install/uninstall (1 and 2 disable the theme state) and the argument roles of user32 ordinals 2703/2705/2706 (names in the corpus are `NtUserIs*Dpi*`, may be an alias of a shared stub).
* The literal mapping of `NMAP`/`SMAP` from the task brief to real resources (not present as strings).
* Real-system confirmation (nothing here was run on Windows): default `GetThemeAppProperties`=3, `CloseThemeData(NULL)`=E_HANDLE, `OpenThemeData(h,L"")`=0x80070057, `GetThemeSysInt(NULL,...)`=E_HANDLE, `IsAppThemed` ignoring `SetThemeAppProperties(0)`, ordering of E_POINTER vs E_HANDLE in `GetThemePartSize` vs `GetThemeColor`. All are stated from decomp; they are the highest-value first probes.
* Behaviour of the ALPC server side (themeservice) is outside this DLL and not in the corpus modules listed; only the client protocol (op codes) is documented.
* Whether `EnableTheming` is truly dead on every path: ordinal 36 is the same RVA; no other caller exists in the 837 functions read.

## Verification (adversarial re-check)

Re-fetched from `http://docker1.dev.moonlightcompanies.com:8765/api/function/uxtheme/<name>/decomp` and compared with the text above. No errors found, no edits needed to the claims. Checked:

* `CAppInfo::CAppInfo @0x37ac`: `*(this+0x20) = 3`. Default app flags are 3 (CONFIRMED, from decomp only).
* `SetThemeAppProperties @0x512b0`: stores `dwFlags` unmasked at `CAppInfo+0x20`. `SetLastError(0)` is called only when initialised (CONFIRMED).
* `EnableTheming @0x4f7c0` (ordinals 36 and 129): returns 0 and only touches a CThemeApiHelper handle. It is a no-op. Wine `system.c:557-575` writes `ThemeActive=0` to the registry (CONFIRMED).
* `OpenThemeDataExInternal @0x1a470`: invalid hwnd gives 0x80070006, NULL class list gives 0x80004003, in that order (CONFIRMED).
* `_OpenThemeData @0x192c4`: the hook-state and `+0x24` gate, the flag gate (`>>1` unless flags&2), and the empty string giving 0x80070057 (CONFIRMED). The empty-string check comes after the flag gate (CONFIRMED).
* `OpenThemeData @0x1b590` and `OpenThemeDataForDpi @0xb9e0`: the high-contrast gate leaves LastError untouched, and dpi above 0x1e0 (480) sets LastError 0x57 (CONFIRMED).
* `GetThemeColor`: one body at 0x1cf80 serving ordinals 154 and 159 (GetThemeInt). A NULL output gives 0x80004003 (-0x7fffbffd) before the handle lookup (CONFIRMED). `GetThemeMetric @0x20980` has the same ordering (CONFIRMED).
* `GetThemePartSize @0x14a20`: handle first, then psz NULL gives E_POINTER. For border-fill parts (type 1): TS_MIN = max(1, 2*border), TS_TRUE = 2*border+1, anything else = 0x80070057. Image parts go to `CImageFile::GetPartSize` (CONFIRMED). The handle slot is stride 0x28 and the generation is the high 16 bits (CONFIRMED).
* `IsAppThemed @0x224b0` calls `IsAppThemedInternal @0x22670`, which calls `AppIsThemed` (hook state and `+0x24`) and `IsSystemThemeActive`. App flags are not consulted, so `SetThemeAppProperties(0)` does not change IsAppThemed (CONFIRMED).
* `CloseThemeData @0x190f0` just returns `CRenderList::CloseRenderObject`. The CloseThemeData(NULL)=E_HANDLE claim stays INFERENCE (not read in the CloseRenderObject body).
* Ordinals from the exports API: 133 is `AllowDarkModeForWindow`, 135 `SetPreferredAppMode`, 132 `ShouldAppsUseDarkMode`, 136 `FlushMenuThemes`, 137 `IsDarkModeAllowedForWindow`, 138 `ShouldSystemUseDarkMode`, 139 `IsDarkModeAllowedForApp`. Of 193 exports, 108 are ordinal-only and 34 are SHUNIMPL forwarders (all CONFIRMED by script).
* `GetThemeSizeId @0x523c8` gives 0x4b2..0x4b9 (1202..1209) for SM ids 2, 3, 0x1e, 0x1f, 0x34..0x37, and 0x4b1 (1201) for 0x20/0x21. The ids match `vssym32.h` (1201..1210; `TMT_FLATMENUS`=1001; `TMT_CSSNAME`=1401..`TMT_ALIAS`=1404; `TMT_SCROLLBAR`=1601; fonts 801+). SM_CXPADDEDBORDER(92)->1210 was not re-read (UNVERIFIED).
* comctl32 `Header_NewFont`: `GetThemeTextExtent(..., 1, 0, L"M", -1, 0x800, ...)` with a `GetTextExtentPointW(L"...")` fallback (CONFIRMED).

Not re-verified (still as the author labelled them): the property-record byte layout, the ALPC opcodes, the process-name exclusion list, buffered-paint cbSize values, and the DarkMode_ app-name mapping. Treat these as UNVERIFIED until a live Win10 probe.

Note on `OpenNcThemeDataInternal` (section 5): the claim that a NULL class list goes straight to `_OpenThemeData` and hits the empty check is consistent with `_OpenThemeData`, which returns 0x80070057 for NULL or empty. It is not E_POINTER on that path.
