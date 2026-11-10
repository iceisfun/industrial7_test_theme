#!/usr/bin/env bash
# dpiscaled_run.sh DPI RUN  - one full recording boot (logoff stage) for area dpi_scaled -> data/win10/dpi_scaled/dpiN/RUN
set -u
cd "$(dirname "$0")/.."
d="$1"; run="$2"; out="data/win10/dpi_scaled/dpi$d/$run"
D='--table %W%\probe_table_dpi.csv'
tools/vm_run.sh --config "dpi$d" --out "$out" --timeout 300 \
  --probe "--label dpi${d}_sys_v6 --prio 3 --sweep full --dpi-aware system" \
  --probe "--label dpi${d}_unaware_v6 --prio 3 --sweep full --dpi-aware unaware" \
  --probe "--label dpi${d}_sys_dpitable $D --prio 3 --sweep std --dpi-aware system" \
  --probe "--label dpi${d}_unaware_dpitable $D --prio 3 --sweep std --dpi-aware unaware" \
  --probe "--label dpi${d}_pmv2_v6 --prio 3 --sweep std --dpi-aware pmv2" \
  --probe "--label dpi${d}_sys_v5 --prio 3 --sweep std --comctl v5 --dpi-aware system" \
  filehash probe_draw probe_draw_nomanifest
