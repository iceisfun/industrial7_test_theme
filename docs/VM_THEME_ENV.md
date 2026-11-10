# Theme environment of the local Win10 VM (measured)

Measured 2026-11-09 with `tools/vm/themeenv.c` (`themeenv6.exe` = comctl32 v6 manifest, `themeenv.exe` = no
Common-Controls dependency + activation-context check) and `tools/probe_record.exe --prio 0` through
`tools/vm_run.sh`. Raw reports: `docs/vm_evidence/env_<config>.txt`, probe CSVs `docs/vm_evidence/probe_*.csv`.
Everything below is an observation of real Windows, not an inference.

## Guest

| item | value |
|---|---|
| OS | Windows 10 **Home** 22H2, 10.0.19045.2965 (UBR 0xB95), EditionID Core |
| uxtheme.dll | **10.0.19041.2193** (exactly the reference build of the decomp server) |
| comctl32 v6 | C:\Windows\WinSxS\amd64_microsoft.windows.common-controls_..._6.0.19041.1110_...\comctl32.dll, file 6.10.19041.1110, DllGetVersion **6.16.10586** |
| comctl32 v5 | ..._5.82.19041.1110_...\comctl32.dll, file 5.82.19041.1110, DllGetVersion 5.82.10586 |
| user32 / gdi32 / win32u | 10.0.19041.2788 / 10.0.19041.2913 / 10.0.19041.2913 |
| dwmapi | 10.0.19041.746 |
| user | Administrator (autologon, built-in account); computer I7-WIN10 |
| how run.cmd starts | HKLM Run key `i7boot` = `C:\i7\boot.cmd` -> runs `\i7run\run.cmd` from the payload CD, at every logon |
| session | SessionId 1 = active console session; WindowStation **WinSta0**, thread + input desktop **Default** |
| shell | explorer.exe and dwm.exe running when run.cmd starts; LogonUI not running; uptime ~33 s |
| display | 1024x768x32 (std VGA), 96 dpi (LOGPIXELSX 96, GetDpiForSystem 96, AppliedDPI 0x60, LogPixels absent, Win8DpiScaling 0) |
| high contrast | off (SPI_GETHIGHCONTRAST flags 0x7E, HKCU Accessibility\HighContrast Flags "126") |
| dark mode | off: AppsUseLightTheme=1, SystemUsesLightTheme=1; uxtheme #132 ShouldAppsUseDarkMode=0, #138 ShouldSystemUseDarkMode=0 |
| theme registry | HKCU Themes CurrentTheme = C:\Windows\resources\Themes\light.theme; ThemeManager ThemeActive "1", DllName %SystemRoot%\resources\themes\Aero\Aero.msstyles; HKLM InstallTheme aero.theme |

## Is the session themed when run.cmd runs? **Yes, immediately.**

* Themes service (svchost -k netsvcs, AUTO_START) is RUNNING at the first line of run.cmd.
* `IsThemeActive()=1`, `IsAppThemed()=1`, `IsCompositionActive()=1`, `GetThemeAppProperties()=0x3`
  (STAP_ALLOW_NONCLIENT|STAP_ALLOW_CONTROLS) in the first process; the `--wait` gate returns after 0 ms / 1 poll
  in every boot so far (8 boots).
* `GetCurrentThemeName` = S_OK, `C:\Windows\resources\themes\Aero\Aero.msstyles`, `NormalColor`, `NormalSize`.
  (Note: the theme *file* is light.theme, the msstyles is Aero.msstyles.)
* No waiting, no `sc start themes`, no explorer delay is needed. vm_run.sh still runs the gate
  (`themeenv6.exe --wait 150`) so a slow boot would be noticed in `__env.txt` (`WaitThemeActive after_ms`).

## Header (CC30) - the number that motivated the corpus

| process / context | SysHeader32 HDM_LAYOUT cy (DEFAULT_GUI_FONT) | Segoe UI 9 (-12) | GetWindowTheme |
|---|---|---|---|
| comctl32 v6 by process manifest | **24** | 24 | non-NULL |
| comctl32 v5 (no manifest) | 17 | 19 | NULL |
| v5 process + v6 activation context (CreateActCtx/ActivateActCtx, LoadLibrary comctl32 -> v6 module) | **24** | 24 | non-NULL |

* `OpenThemeData(NULL, L"Header")` succeeds in both v5 and v6 processes.
* `GetThemePartSize(HP_HEADERITEM=1, HIS_NORMAL=1 (and state 0), NULL rect, TS_TRUE)` = S_OK **cx=6 cy=24**,
  identical with hdc NULL and with the screen DC.
* `GetThemePosition(HEADER, HP_HEADERITEM, 0, TMT_MINSIZE)` = **0x80070490** (E_PROP_ID_UNSUPPORTED/ not found):
  so on Win10 Aero the 24 comes from the TS_TRUE part size, not from TMT_MINSIZE.
* probe_record rows (v6 context): P01204 themed header 24 (font tmHeight 15), P01203 same header with the theme
  removed 19 (= 15 + 2*SM_CYEDGE), P01205-07 ListView report header 24, P01208 24.

## Configurations (tools/theme_config.sh) and what they really do

| config | stage | IsThemeActive | msstyles | HP_HEADERITEM TS_TRUE | header cy (v6, gui font) | other |
|---|---|---|---|---|---|---|
| aero | - | 1 | Aero.msstyles | 6x24 | 24 | |
| dark | runtime | 1 | Aero.msstyles | 6x24 | 24 | #132=1, #138=1 (read by the next process; registry + ImmersiveColorSet broadcast suffice, no logoff) |
| hc (High Contrast Black) | runtime + 10 s settle | 1 | **AeroLite.msstyles** NormalColor | **3x3** | **17** (19 in probe P01205-07: text-based) | HC flags 0x7F. The switch is asynchronous: a process started ~0.2 s after SPI_SETHIGHCONTRAST still got Aero values (24), one started a little later got AeroLite (17) - hence the settle |
| classic (`sc stop Themes`) | runtime | **1** | Aero.msstyles | 6x24 | 24 | service STOPPED but the session stays themed |
| classic_disabled (start=disabled + reboot) | reboot | **1** | Aero.msstyles | 6x24 | 24 | service stopped from boot on, session still themed |
| dpi120 | logoff | 1 | Aero | 6x24 (unaware process) | 24 (unaware) / probe (system aware): themed 26, unthemed 26, LV 26 | LOGPIXELSX 120 even for the DPI-unaware themeenv (XP-style scaling at 125%), AppliedDPI 0x78 |
| dpi144 | logoff | 1 | Aero | 6x24 (unaware process sees 96) | probe (system aware, LOGPIXELSY 144): themed 31, unthemed 31, LV 31 | DPI-unaware processes are virtualized to 96 at 150% (LOGPIXELSX 96); AppliedDPI 0x90; SM_CYEDGE stays 2 |
| dpi192 | logoff | 1 | Aero | 6x24 (unaware process sees 96, screen 512x384) | probe (system aware, LOGPIXELSY 192): themed 40, unthemed 40, LV 40, P01208 33 | AppliedDPI 0xC0; works at 1024x768 despite the small display |

Consequences:
* **There is no system-wide "classic" (unthemed) mode on Win10** reachable by stopping/disabling the Themes
  service: IsThemeActive stays TRUE. An unthemed state is per-process (`SetThemeAppProperties(0)`, a v5 process
  for comctl32 controls, `SetWindowTheme(h, L"", L"")` per window) or high contrast (which is still themed, with
  AeroLite.msstyles). Tests of "theme inactive" behaviour therefore cannot be produced on Win10 by configuration;
  their expectations must come from the decomp, and Industrial7's themeless mode is NOT a Win10 configuration.
* Dark mode does not change any msstyles value seen through the classic APIs; it only flips the #132/#138
  answers (and what dark-aware apps choose, e.g. `SetWindowTheme(h, L"DarkMode_Explorer", NULL)`).
* DPI: the measured value depends on the process DPI awareness. Areas recording DPI-dependent numbers must
  state awareness (probe_record runs system-aware by its manifest/argument).
* Logoff is sufficient for DPI (LogPixels/Win8DpiScaling); it requires ForceAutoLogon=1, which vm_run.sh sets.

## Practical rules for test authors

* The VM's default state is themed Aero, 96 dpi, light, comctl32 v6 available. Hard CHECKs of "themed" values
  run in the default configuration; t_note anything that changes with configuration.
* A test needing comctl32 v6 controls: `tests/<area>/app.rc` (process manifest) or an activation context
  (docs/VM_RUN.md). Without it the controls are v5 and unthemed even though uxtheme is active.
* Tests run **elevated**: TokenElevation elevated=1, TokenElevationType 1 (built-in Administrator, no split
  token). Behaviour that differs for a limited user is not observable here.
* Repeatability: two probe_record `--prio 0` runs in different boots (aero) differ in exactly one of 509 keys,
  GetSystemMetrics(93) (undocumented index, 596 vs 600); every theme value was identical.
