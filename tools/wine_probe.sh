#!/usr/bin/env bash
# Run build/probe_record.exe under Wine inside a private Xvfb (never touches the live X session).
#   tools/wine_probe.sh [-w WINE_BIN] [-o results.csv] -- [probe_record args...]
# - throw-away copy of the base prefix, private Xvfb on a free unique display number, hard timeout
# - Xvfb binary: $XVFB, else `Xvfb` in PATH
# - everything (Xvfb, wineserver, prefix copy) is torn down on exit; DISPLAY of the caller is ignored
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(dirname "$HERE")"
WINE="${WINE:-$HOME/work/wine/proton-ge-custom/build/dist/files/bin/wine}"
BASE="${TP_BASE_PREFIX:-$HOME/.cache/test_pattern/prefix}"
OUT="$ROOT/build/results_wine.csv"; EXE="$ROOT/build/probe_record.exe"; TMO="${TP_TIMEOUT:-900}"
while [ $# -gt 0 ]; do case "$1" in -w) WINE="$2"; shift 2;; -o) OUT="$2"; shift 2;; -e) EXE="$2"; shift 2;; --) shift; break;; *) break;; esac; done
XV="${XVFB:-$(command -v Xvfb || true)}"
[ -n "$XV" ] || { echo "no Xvfb (set XVFB=/path/to/Xvfb)" >&2; exit 2; }
[ -d "$BASE/drive_c" ] || "$HERE/make_base_prefix.sh" >&2
RUN="$(mktemp -d "${TMPDIR:-/tmp}/probe_prefix.XXXXXX")"; cp -a --reflink=auto "$BASE/." "$RUN/"
XN=$((200 + RANDOM % 700)); while [ -e "/tmp/.X11-unix/X$XN" ]; do XN=$((XN + 1)); done
"$XV" ":$XN" -screen 0 1280x1024x24 -nolisten tcp >/dev/null 2>&1 & XPID=$!
cleanup() {
  WINEPREFIX="$RUN" "$(dirname "$WINE")/wineserver" -k 2>/dev/null; sleep 0.3
  kill "$XPID" 2>/dev/null; rm -rf "$RUN"; rm -f "/tmp/.X11-unix/X$XN" "/tmp/.X$XN-lock"
}
trap cleanup EXIT
for _ in $(seq 60); do [ -S "/tmp/.X11-unix/X$XN" ] && break; sleep 0.1; done
OUT="$(realpath -m "$OUT")"; mkdir -p "$(dirname "$OUT")"; : > "$OUT"
env -u WAYLAND_DISPLAY DISPLAY=":$XN" WINEPREFIX="$RUN" WINEARCH=win64 WINEDEBUG="${WINEDEBUG:--all}" \
  timeout "$TMO" "$WINE" "$EXE" --out "Z:${OUT//\//\\}" "$@"
echo "exit=$? results=$OUT" >&2
