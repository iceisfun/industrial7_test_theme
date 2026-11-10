# data/win10/dpi_scaled - live Windows 10 recording, Aero Light at 120 / 144 / 192 DPI

Recorder `tools/probe_record.exe` (tables `probe_table.csv` = main, 7647 rows; `probe_table_dpi.csv` = DPI supplement,
12668 rows with `OpenThemeDataForDpi` 96/120/144/192, copies here), run by `tools/dpiscaled_run.sh DPI RUN` through
`tools/vm_run.sh --config dpiN` on the local Win10 VM. Each DPI was recorded in two independent fresh boots (`boot1`, `boot2`).

## Machine / state (measured, `env_v6.txt`, `env_v5.txt`, `guestinfo.txt`)
Windows 10 Home 22H2 10.0.19045.2965 x64; uxtheme.dll 10.0.19041.2193 (sha256 1fef44e2...); comctl32 v6 6.10.19041.1110 (activation context),
v5 5.82.19041.1110; theme = `Aero.msstyles` NormalColor/NormalSize (sha256 33ec7c86...), CurrentTheme light.theme, light apps+system,
IsThemeActive/IsAppThemed/IsCompositionActive = 1, GetThemeAppProperties 0x3, Themes service running, no high contrast, session 2 WinSta0\Default
(elevated Administrator, autologon after the logoff stage). Stage: `reg add HKCU\Control Panel\Desktop LogPixels=N, Win8DpiScaling=1`, `shutdown /l`,
ForceAutoLogon, tests after the second logon (marker `C:\i7state`). Registry AppliedDPI = 0x78 / 0x90 / 0xC0.

| config | LogPixels | system-aware process sees | DPI-unaware process sees | display |
|---|---|---|---|---|
| dpi120 (125%) | 120 | GetDpiForSystem 120 | **120 too** (XP-style scaling: LOGPIXELS 120, no virtualization) | 1024x768 |
| dpi144 (150%) | 144 | 144 | **96** (bitmap virtualised) | unaware sees 683x512 |
| dpi192 (200%) | 192 | 192 | **96** | unaware sees 512x384 |

## Files per `dpiN/`
`run_R_<pass>.csv.gz` (R = boot 1/2), `joined_<pass>.csv.gz` (run 1 joined with the table columns), `env_v6.txt`, `env_v5.txt`, `guestinfo.txt`
(sha256 of uxtheme, themeui, msstyles, .theme files), `boot1/ boot2/` (`_summary.txt`, `__env*`, `probe_draw*.txt.gz`, logs), `MANIFEST.sha256`.

| pass | args | rows (lines) |
|---|---|---|
| `sys_v6` | main table, `--prio 3 --sweep full --dpi-aware system`, comctl32 v6 | ~156k |
| `unaware_v6` | same, `--dpi-aware unaware` | ~156k |
| `sys_dpitable`, `unaware_dpitable` | `probe_table_dpi.csv` `--sweep std` (every catalogued class/part/state: TS_MIN/TRUE/DRAW, DrawThemeBackground 32x24 hashes + 10x10 raw + 64x48, content rect, extent, margins, IsThemePartDefined, class font and text metrics at 96/120/144/192) | 12674 |
| `pmv2_v6` | main table `--sweep std --dpi-aware pmv2` | ~33k |
| `sys_v5` | main table `--sweep std --comctl v5` (unmanifested comctl32 5.82), system-aware | ~33k |
| `boot*/probe_draw*.txt.gz` | tools/probe_draw (manifested / unmanifested) DIB hashes + key raw bitmaps | |

Columns as in `data/win10/aero_light_96/README.md` (`id,sub,hr,lasterr,ret,v0..v5,txt`).

## Repeatability (boot1 vs boot2, all passes, all three DPIs)
Raw probe CSVs identical line for line (0 differing lines after removing `RUN,time_utc` and `P00006 SM93`, which did not even differ this time).
probe_draw / filehash outputs differ only in the `@@I7 END ... ms=` timing. No CRASH / HANG / OPENFAIL / NOAPI / NODIB line anywhere.
(`filehash` is reported NOSUMMARY by vm_split: it is not a harness exe, not an error.)

## Findings / surprises
1. **Header (CC30) minimum is not DPI scaled.** `GetThemePartSize(HP_HEADERITEM, TS_TRUE)` = 6x24 at every DPI (explicit 96/120/144/192, every
   system DPI, aware or not) and HDM_LAYOUT cy floors at **24** at 120/144/192. Above the floor the height is text based:
   cy = tmHeight + 4 (dpi96), +6 (dpi120 and dpi144), +8 (dpi192): e.g. tm 20 -> 26 at 120/144, 28 at 192; tm 25 -> 31 (144), 33 (192); tm 41 -> 47 (120);
   tm 65 -> 73 (192). Consequence: a DPI-aware app with a DEFAULT_GUI_FONT-sized header gets 24 at 120 (tm16), 26 at 144 (tm20), 28 at 192 -
   it is NOT 24*scale. (Rows P01218-P01237.) With HDS_FILTERBAR roughly doubles (53 for tm20 at 120).
2. **`OpenThemeDataForDpi(dpi=N)` results are not independent of the process/system DPI.** Between dpi120 and dpi144 runs, 750 of 12668 dpi-table
   rows differ although the requested explicit DPI is the same (e.g. HP_HEADERSORTARROW TS_TRUE at explicit 120: 16x6 under system 120, 13x5 under system 144;
   at explicit 144/192 under system 144: 20x7; Scrollbar SBP_GRIPPERHORZ at explicit 96: 12x11 vs 16x14). Image plateau selection plus a scale by
   system DPI. Constants for these parts must be keyed by (object DPI, system DPI). Explicit-DPI numbers recorded at dpi96 (aero_light_96) are therefore not
   the explicit-DPI numbers of a scaled machine.
3. **pmv2 (per-monitor-v2) == system-aware** in every row of the std pass (only the DpiAwareness row differs): single-monitor VM.
4. **dpi120 unaware == system-aware** (identical but for the awareness row): 125% is not virtualized on Win10 with LogPixels 120. dpi144/192 unaware
   processes are virtualised to 96: ~7.6k rows still differ from the 96-DPI recording (mostly the smaller virtual screen/system-metrics rows and a few DPI-dependent values; not yet categorised).
5. Env facts matching docs/VM_THEME_ENV.md: theme active immediately, header 24 for the default v6 header at all DPIs for the *unscaled* font (tm16); nothing contradicts the 24 px derivation
   (TS_TRUE image height 24, MINSIZE E_PROP_ID_UNSUPPORTED); the new fact is that it stays 24 at high DPI instead of scaling.
