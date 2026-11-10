#!/usr/bin/env bash
# dhc_run.sh CONFIG RUN  - one full recording boot for area dark_hc_classic -> data/win10/dark_hc_classic/CONFIG/RUN
set -u
cd "$(dirname "$0")/.."
cfg="$1"; run="$2"; out="data/win10/dark_hc_classic/$cfg/$run"
tools/vm_run.sh --config "$cfg" --out "$out" --timeout 300 \
  --probe "--label $cfg --prio 3" \
  --probe "--label ${cfg}_darkforce --prio 2 --dark force" \
  --probe "--label ${cfg}_appprops0 --prio 2 --app-props 0" \
  filehash probe_draw probe_draw_nomanifest
