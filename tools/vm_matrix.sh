#!/usr/bin/env bash
# vm_matrix.sh - run the same exes under several Win10 theme configurations (one tools/vm_run.sh call each) and
# print a status matrix (exe x config).
#
#   tools/vm_matrix.sh [--configs aero,dark,hc,dpi120] [vm_run.sh options...] EXE[,EXE...] | --all
#
# Default configs: aero,dark,hc (runtime-switchable, one boot each).  logoff/reboot configurations (dpi*,
# classic_disabled) cost one extra logon or one extra boot each; see tools/theme_config.sh list.
# Results: results/win10[-<config>]/ per config, matrix in results/win10-matrix.txt.
# Exit status: worst vm_run.sh status (0 ok, 1 test failures, 3 infrastructure).
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONFIGS="aero,dark,hc"; ARGS=()
while [ $# -gt 0 ]; do
  case "$1" in
    --configs) CONFIGS="$2"; shift 2;;
    --out) echo "vm_matrix: --out is per config (results/win10-<config>)" >&2; exit 2;;
    -h|--help) sed -n '2,11p' "$0"; exit 0;;
    *) ARGS+=("$1"); shift;;
  esac
done
IFS=',' read -r -a CFG <<< "$CONFIGS"
worst=0; DIRS=()
for c in "${CFG[@]}"; do
  n="${c%@*}"; d="$ROOT/results/win10"; [ "$n" = aero ] || d="$ROOT/results/win10-$n"
  echo "######## config $c"
  "$ROOT/tools/vm_run.sh" --config "$c" --out "$d" "${ARGS[@]}"; r=$?
  [ $r -gt $worst ] && worst=$r
  DIRS+=("$c=$d")
done
python3 - "${DIRS[@]}" > "$ROOT/results/win10-matrix.txt" <<'EOF'
import sys, re, os
cols, st = [], {}
for a in sys.argv[1:]:
    c, d = a.split('=', 1); cols.append(c)
    p = os.path.join(d, '_summary.txt')
    if not os.path.exists(p): continue
    for l in open(p):
        m = re.match(r'(\S+)\s+(OK|FAIL|TIMEOUT|SPAWNFAIL|TRUNCATED|NOSUMMARY|BADSUMMARY|MISSING)\s', l)
        if m: st.setdefault(m.group(1), {})[c] = m.group(2)
w = max([len(k) for k in st] + [10])
print('%-*s ' % (w, 'exe') + ' '.join('%-10s' % c for c in cols))
for k in sorted(st):
    print('%-*s ' % (w, k) + ' '.join('%-10s' % st[k].get(c, '-') for c in cols))
EOF
cat "$ROOT/results/win10-matrix.txt"
exit $worst
