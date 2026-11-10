#!/usr/bin/env bash
# dhc_run.sh CONFIG RUN  - one full recording boot for area dark_hc_classic -> data/win10/dark_hc_classic/CONFIG/RUN
# probe_1 = v6 complete (--sweep full), probe_2 = v5 unmanifested (std), probe_3 = v6 --dark force, probe_4 = v6 app-props 0
set -u
cd "$(dirname "$0")/.."
cfg="$1"; run="$2"; out="data/win10/dark_hc_classic/$cfg/$run"
tools/vm_run.sh --config "$cfg" --out "$out" --timeout 600 \
  --probe "--label ${cfg}_v6 --prio 3 --sweep full --comctl v6" \
  --probe "--label ${cfg}_v5 --prio 3 --sweep std --comctl v5" \
  --probe "--label ${cfg}_darkforce --prio 2 --sweep std --comctl v6 --dark force" \
  --probe "--label ${cfg}_appprops0 --prio 2 --sweep std --comctl v6 --app-props 0" \
  filehash probe_draw probe_draw_nomanifest
