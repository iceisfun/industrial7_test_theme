#!/usr/bin/env bash
# run_probe.sh <wine-bin> <base-prefix> <exe> [--args "a b"] [--theme off|on|path:<dll> ] [--xvfb]
# Throw-away prefix copy. Without --xvfb: DISPLAY unset (Wine null driver; NO windows can be created).
# With --xvfb: private Xvfb on a random unique display number (XVFB env = binary, default Xvfb), torn down on exit.
# The user live X session is never touched (DISPLAY is only ever set to our own :N).
set -uo pipefail
WINE="$1"; BASE="$2"; EXE="$3"; shift 3
THEME=default; XV=0; ARGS=""
while [ $# -gt 0 ]; do case "$1" in --theme) THEME="$2"; shift 2;; --xvfb) XV=1; shift;; --args) ARGS="$2"; shift 2;; *) shift;; esac; done
[ -d "$BASE/drive_c" ] || { env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$BASE" WINEDEBUG=-all "$WINE" wineboot -i >/dev/null 2>&1; env -u DISPLAY WINEPREFIX="$BASE" "$(dirname "$WINE")/wineserver" -w 2>/dev/null; }
RUN="$(mktemp -d "${TMPDIR:-/tmp}/probe_prefix.XXXXXX")"; cp -a --reflink=auto "$BASE/." "$RUN/"
cleanup() { WINEPREFIX="$RUN" "$(dirname "$WINE")/wineserver" -k 2>/dev/null; sleep 0.3; rm -rf "$RUN"; [ -n "${XEPID:-}" ] && kill "$XEPID" 2>/dev/null; [ -n "${XN:-}" ] && rm -f "/tmp/.X11-unix/X$XN" "/tmp/.X$XN-lock"; }
trap cleanup EXIT
E=(WINEPREFIX="$RUN" WINEDEBUG="${WINEDEBUG:--all}")
if [ $XV = 1 ]; then XN=$((200 + RANDOM % 700)); while [ -e "/tmp/.X11-unix/X$XN" ]; do XN=$((XN+1)); done; "${XVFB:-Xvfb}" ":$XN" -screen 0 1280x1024x24 -nolisten tcp >/dev/null 2>&1 & XEPID=$!; for i in $(seq 50); do [ -S "/tmp/.X11-unix/X$XN" ] && break; sleep 0.1; done; E+=(DISPLAY=":$XN"); fi
KEY='HKCU\Software\Microsoft\Windows\CurrentVersion\ThemeManager'
rw() { env -u DISPLAY -u WAYLAND_DISPLAY "${E[@]}" "$WINE" "$@" >/dev/null 2>&1; }
case "$THEME" in
  off) rw reg add "$KEY" /v ThemeActive /t REG_SZ /d 0 /f ;;
  path:*) rw reg add "$KEY" /v DllName /t REG_SZ /d "${THEME#path:}" /f ;;
esac
env -u DISPLAY -u WAYLAND_DISPLAY "${E[@]}" timeout 300 "$WINE" "$EXE" $ARGS
