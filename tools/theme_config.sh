#!/usr/bin/env bash
# theme_config.sh - Win10 VM theme/DPI configurations for multi-stage boots (used by tools/vm_run.sh --config).
#
#   theme_config.sh list                     configurations, default stage mode, what was verified (docs/VM_RUN.md)
#   theme_config.sh mode    NAME[@MODE]      prints runtime|logoff|reboot|none
#   theme_config.sh expect  NAME[@MODE]      prints themed|unthemed  (vm_run.sh's IsThemeActive gate)
#   theme_config.sh cmd     NAME[@MODE]      prints the guest cmd lines that apply the configuration (run.cmd fragment;
#                                            each line writes its output to COM1; themeset.exe must be in the cwd)
#
# Stage modes:
#   none     nothing to do (aero = the base image as booted)
#   runtime  applied inside the same logon session right before the tests (no restart)
#   logoff   applied, then 'shutdown /l'; the autologon brings the session back and run.cmd continues (marker file)
#   reboot   applied in boot 1 (VM powers off), tests run in boot 2 on the same overlay (W10_KEEPDISK/W10_OVERLAY)
# NAME@MODE overrides the default mode (e.g. dpi120@reboot).  Everything here changes the disposable overlay only.
set -eu

PERS='HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize'
DESK='HKCU\Control Panel\Desktop'

# name | default mode | expect | description
CONFIGS='aero|none|themed|Aero light (base image default: aero.msstyles NormalColor, light apps+system, 96 dpi, 1024x768)
dark|runtime|themed|Windows 10 dark: AppsUseLightTheme=0 SystemUsesLightTheme=0 + WM_SETTINGCHANGE ImmersiveColorSet
light|runtime|themed|explicit light: AppsUseLightTheme=1 SystemUsesLightTheme=1 + WM_SETTINGCHANGE ImmersiveColorSet
hc|runtime|themed|high contrast on (SPI_SETHIGHCONTRAST HCF_HIGHCONTRASTON, scheme High Contrast Black)
hcwhite|runtime|themed|high contrast on, scheme High Contrast White
classic|runtime|unthemed|visual styles off: sc stop Themes (Themes service stopped for the session)
classic_disabled|reboot|unthemed|Themes service start=disabled, then reboot (no theme from logon on)
dpi96|logoff|themed|LogPixels=96 Win8DpiScaling=1
dpi120|logoff|themed|LogPixels=120 Win8DpiScaling=1 (125%)
dpi144|logoff|themed|LogPixels=144 Win8DpiScaling=1 (150%)
dpi192|logoff|themed|LogPixels=192 Win8DpiScaling=1 (200%)'

die() { echo "theme_config: $*" >&2; exit 2; }
row() { local n="${1%@*}"; echo "$CONFIGS" | awk -F'|' -v n="$n" '$1==n' | head -1; }
mode_of() { local r; r="$(row "$1")"; [ -n "$r" ] || die "unknown configuration ${1%@*} (theme_config.sh list)"
  case "$1" in *@*) echo "${1#*@}";; *) echo "$r" | cut -d'|' -f2;; esac; }
expect_of() { local r; r="$(row "$1")"; [ -n "$r" ] || die "unknown configuration $1"; echo "$r" | cut -d'|' -f3; }

dpi_cmd() {
  echo "reg add \"$DESK\" /v LogPixels /t REG_DWORD /d $1 /f > COM1 2>&1"
  echo "reg add \"$DESK\" /v Win8DpiScaling /t REG_DWORD /d 1 /f > COM1 2>&1"
}
cmd_of() {
  local n="${1%@*}"
  case "$n" in
    aero) echo "rem aero: nothing to apply";;
    dark|light)
      local v=0; [ "$n" = light ] && v=1
      echo "reg add \"$PERS\" /v AppsUseLightTheme /t REG_DWORD /d $v /f > COM1 2>&1"
      echo "reg add \"$PERS\" /v SystemUsesLightTheme /t REG_DWORD /d $v /f > COM1 2>&1"
      echo "themeset.exe broadcast ImmersiveColorSet > COM1 2>&1";;
    hc) echo "themeset.exe hc on \"High Contrast Black\" > COM1 2>&1";;
    hcwhite) echo "themeset.exe hc on \"High Contrast White\" > COM1 2>&1";;
    classic) echo "sc stop Themes > COM1 2>&1"; echo "ping -n 4 127.0.0.1 > nul"; echo "sc query Themes > COM1 2>&1";;
    classic_disabled) echo "sc config Themes start= disabled > COM1 2>&1"; echo "sc stop Themes > COM1 2>&1";;
    dpi96) dpi_cmd 96;; dpi120) dpi_cmd 120;; dpi144) dpi_cmd 144;; dpi192) dpi_cmd 192;;
    *) die "unknown configuration $n";;
  esac
}

case "${1:-list}" in
  list) echo "$CONFIGS" | awk -F'|' '{printf "%-17s %-8s %-9s %s\n",$1,$2,$3,$4}';;
  mode) mode_of "${2:?name}";;
  expect) expect_of "${2:?name}";;
  cmd) mode_of "${2:?name}" >/dev/null; cmd_of "$2";;
  *) die "usage: theme_config.sh list|mode|expect|cmd NAME[@MODE]";;
esac
