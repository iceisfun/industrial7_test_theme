# Running the corpus on the local Win10 VM

Win10 is the authority for expected values. `tools/vm_run.sh` runs test exes on the shared local Windows 10 22H2
oracle VM (`~/w10lab`, see `~/w10lab/README.md`) and brings results back over COM1.
The theme environment the tests see is described in `docs/VM_THEME_ENV.md`.

## Quick use

```sh
make                                   # build/<area>.exe
tools/vm_run.sh core_smoke             # one exe -> results/win10/core_smoke.txt
tools/vm_run.sh hdr_cc30,gt_partsize --filter hdr_   # several, with a test-name filter
tools/vm_run.sh --all                  # every build/*.exe except dx_* (add --destructive for dx_*)
tools/vm_run.sh --config dark hdr_cc30 # run under a theme configuration -> results/win10-dark/
tools/vm_run.sh --env-only             # only the environment report (results/win10/__env.txt)
tools/vm_run.sh --probe "--label aero --prio 0" core_smoke   # also run tools/probe_record.exe (CSV back)
tools/vm_matrix.sh --configs aero,dark,hc,dpi120 --all       # same exes under several configs + matrix
```

Exit status of `vm_run.sh`: 0 all OK, 1 test failures only, 3 infrastructure problem (an exe missing,
truncated, timed out, crashed without SUMMARY, SUMMARY inconsistent, or the boot was cut short).

## What it does

1. `make -f tools/Makefile.vm` builds the guest helpers in `build/vm/`:
   * `vmrun.exe` (`tools/vm/vmrun.c`) - runs one exe in a kill-on-close job object with a timeout, stdout+stderr
     piped and forwarded to `\\.\COM1` framed as `@@I7 BEGIN <name> <cmdline>` ... `@@I7 END <name> exit=0x..
     ms=.. timeout=0|1 bytes=..`. The child inherits run.cmd's interactive session (WinSta0\Default, session 1).
   * `themeenv6.exe` / `themeenv.exe` (`tools/vm/themeenv.c`) - environment report (comctl32 v6 / v5 process
     default), `--wait SEC` theme gate, `--actctx M` comctl32 v6 activation-context check.
   * `themeset.exe` (`tools/vm/themeset.c`) - runtime switches for `tools/theme_config.sh` (high contrast,
     WM_SETTINGCHANGE broadcast).
2. A payload directory gets the helpers, the exes and a generated `run.cmd`. In the guest run.cmd
   * applies the `--config` stage (see below) with a marker in `C:\i7state` so a logoff/reboot resumes,
   * copies everything to `C:\i7w` (the payload CD is read-only) and runs from there,
   * `__env`: `themeenv6.exe --wait 150` (gate: waits for IsThemeActive; in practice it is already 1),
     `__env_v5`: `themeenv.exe --actctx ctl6.manifest`,
   * each exe through `vmrun.exe -t <timeout>` (default 180 s), then each `--probe` pass (CSV typed back
     through vmrun), then `@@I7 DONE`.
3. `~/w10lab/tools/win10_run.sh` boots a throw-away overlay (waits on `~/w10lab/oracle.lock`: one guest per
   host, other corpora queue on the same lock). The run directory is `~/w10lab/runs/theme-<ts>-<pid>` (kept with
   `--keep`); it must stay short because qemu's QMP socket path is limited to 108 bytes.
4. `tools/vm_split.py` splits the serial log into `results/win10[-config]/<exe>.txt` and validates every frame:
   END present (else TRUNCATED), not timed out, SUMMARY present for harness exes, and SUMMARY's
   tests/failed/skipped equal the counted RUN/OK/FAIL/SKIP lines (BADSUMMARY otherwise). Data files
   (`probe_N.csv`) are written without the frame lines. Also written: `_summary.txt`, `_boot.txt` (everything
   outside frames: config-stage output, hook lines), `_serial_bootN.log` (raw).
5. Batching: at most `--per-boot` exes (default 40) per boot; a boot is capped by `ORACLE_TIMEOUT` (default
   900 s; keep a boot under ~15 min). Exes that never started (boot cut short) get one retry boot.

Measured costs: a boot with a handful of exes takes ~40 s wall time end to end (Windows is logged on ~30 s after
power-on); a logoff stage adds ~15 s, a reboot stage a second boot (~40 s). Serial throughput through vmrun is
about 250 KB/s, so even large probe CSVs are cheap.

## Theme configurations (tools/theme_config.sh)

`tools/theme_config.sh list` prints them; `--config NAME[@MODE]` selects one (`@logoff`/`@reboot` override the
default stage mode). Verified on the VM 2026-11-09 (details and numbers: docs/VM_THEME_ENV.md):

| config | stage | how | verified result |
|---|---|---|---|
| aero | none | base image | Aero.msstyles NormalColor, light, 96 dpi, header 24 |
| dark | runtime | HKCU Personalize AppsUseLightTheme=0, SystemUsesLightTheme=0, broadcast ImmersiveColorSet | ShouldAppsUseDarkMode(#132)=1, ShouldSystemUseDarkMode(#138)=1 in the next process; msstyles unchanged |
| light | runtime | same with 1 | (restores dark) |
| hc / hcwhite | runtime | `themeset hc on "<scheme>"` = SPI_SETHIGHCONTRAST + SPIF_SENDCHANGE, then 10 s settle | AeroLite.msstyles, HP_HEADERITEM TS_TRUE 3x3, header 17 (text based). Without the settle a process started <1 s later still saw Aero values |
| classic | runtime | `sc stop Themes` | NOT unthemed: service stops, IsThemeActive stays 1, Aero.msstyles, header 24 |
| classic_disabled | reboot | Themes start=disabled + reboot | NOT unthemed either (service stopped at logon, IsThemeActive 1) |
| dpi96/120/144/192 | logoff | HKCU Control Panel\Desktop LogPixels=N, Win8DpiScaling=1, logoff | see VM_THEME_ENV.md (dpi120 verified: LOGPIXELSX 120, AppliedDPI 0x78) |

Stage mechanics:
* runtime: commands run in run.cmd before the tests, same session.
* logoff: run.cmd applies, sets `HKLM\...\Winlogon ForceAutoLogon=1`, `shutdown /l /f`, and blocks; the
  autologon logs on again, the hook (HKLM Run `C:\i7\boot.cmd`) calls run.cmd again, the marker
  `C:\i7state\cfg_<name>.done` makes it skip to the tests. Without ForceAutoLogon the VM stays at the logon screen
  after the logoff (verified: hung until the qemu timeout).
* reboot: boot 1 applies and powers off (`W10_KEEPDISK=1`), boot 2 reuses the overlay (`W10_OVERLAY`) with the
  same payload; the marker skips the apply. Note: qemu runs with -no-reboot, so a guest `shutdown /r` would just
  end the run - that is why reboot is two host-side boots.

All of this modifies only the throw-away overlay. Tests themselves must not change system-wide state unless
they are dx_* areas (run only with `--destructive`).

## comctl32 v6 in test exes

An exe built by the plain Makefile has mingw's default manifest (no Common-Controls dependency) and gets
comctl32 **5.82** (C:\Windows\WinSxS\...5.82.19041.1110...); an un-manifested SysHeader32 is 17 px and never
themed. Two ways to get v6 (6.10.19041.1110), both verified with themeenv:
* process default: add `tests/<area>/app.rc` containing `1 24 "app_v6.manifest"`; the Makefile compiles it with
  windres (`-I tools/vm`) and links it;
* activation context: `CreateActCtx` on a v6 manifest (`tools/probe_ctl6.manifest`; file or RT_MANIFEST id 2 as
  probe_record does), `ActivateActCtx`, then `LoadLibrary("comctl32.dll")` loads the v6 module side by side
  with v5 and controls created in that context are v6 (header 24, themed).
uxtheme itself works in both: OpenThemeData/GetThemePartSize answer the same in a v5 process.

Pitfall found on the first run: an XML comment containing `--` makes the embedded manifest invalid and the exe
fails to start with ERROR_SXS_CANT_GEN_ACTCTX (14001) (`tools/probe_app.manifest` had this; fixed).
