#!/usr/bin/env bash
# vm_run.sh - run test exes on the local Win10 22H2 VM (~/w10lab) and collect results/win10*/<exe>.txt
#
#   tools/vm_run.sh [options] EXE[,EXE...] [EXE...]
#   tools/vm_run.sh [options] --all
#
# EXE: a path, or an area name resolved to build/<name>.exe.  --all = every build/*.exe except dx_* (and
# probe_record), dx_* only with --destructive.
# options:
#   --filter F          pass F (test-name substring) to every exe
#   --timeout S         per-exe limit in the guest (default 180); the runner kills the exe's whole job after S
#   --config NAME[@M]   theme configuration from tools/theme_config.sh (default aero); results go to
#                       results/win10-NAME/ unless --out is given (aero: results/win10/)
#   --out DIR           result directory
#   --probe "ARGS"      also run build/probe_record.exe ARGS --out <tmp>.csv (repeatable); the CSV comes back as
#                       <out>/probe_<n>.csv (needs make -f tools/Makefile.probe)
#   --per-boot N        exes per VM boot (default 40); a boot is capped at ORACLE_TIMEOUT (default 900 s)
#   --no-gate           do not wait for IsThemeActive before the tests
#   --env-only          only the environment report (<out>/__env.txt)
#   --keep              keep the run directory (payload, serial.log) under ~/w10lab/runs
# Exit status: 0 all OK, 1 test failures, 3 infrastructure problems (missing/truncated/timeout/no SUMMARY/boot).
# One guest at a time: win10_run.sh holds ~/w10lab/oracle.lock (other corpora queue on it).  See docs/VM_RUN.md.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LAB="${W10LAB:-$HOME/w10lab}"
RUNNER="$LAB/tools/win10_run.sh"
FILTER=""; TIMEOUT=180; CONFIG=aero; OUTDIR=""; PERBOOT=40; GATE=1; ENVONLY=0; KEEP=0; ALL=0; DX=0
PROBES=(); EXES=()
while [ $# -gt 0 ]; do
  case "$1" in
    --filter) FILTER="$2"; shift 2;;
    --timeout) TIMEOUT="$2"; shift 2;;
    --config) CONFIG="$2"; shift 2;;
    --out) OUTDIR="$2"; shift 2;;
    --probe) PROBES+=("$2"); shift 2;;
    --per-boot) PERBOOT="$2"; shift 2;;
    --no-gate) GATE=0; shift;;
    --env-only) ENVONLY=1; shift;;
    --keep) KEEP=1; shift;;
    --all) ALL=1; shift;;
    --destructive) DX=1; shift;;
    -h|--help) sed -n '2,24p' "$0"; exit 0;;
    -*) echo "vm_run: unknown option $1" >&2; exit 2;;
    *) IFS=',' read -r -a parts <<< "$1"; EXES+=("${parts[@]}"); shift;;
  esac
done
MODE="$("$ROOT/tools/theme_config.sh" mode "$CONFIG")"; EXPECT="$("$ROOT/tools/theme_config.sh" expect "$CONFIG")"
CNAME="${CONFIG%@*}"
[ -n "$OUTDIR" ] || { if [ "$CNAME" = aero ]; then OUTDIR="$ROOT/results/win10"; else OUTDIR="$ROOT/results/win10-$CNAME"; fi; }
mkdir -p "$OUTDIR"; OUTDIR="$(cd "$OUTDIR" && pwd)"

if [ "$ALL" = 1 ]; then
  for f in "$ROOT"/build/*.exe; do
    n="$(basename "$f" .exe)"
    case "$n" in probe_record) continue;; dx_*) [ "$DX" = 1 ] || { echo "skip $n (destructive; --destructive)"; continue; };; esac
    EXES+=("$f")
  done
fi
RES=()
for e in ${EXES[@]+"${EXES[@]}"}; do
  if [ -f "$e" ]; then RES+=("$(cd "$(dirname "$e")" && pwd)/$(basename "$e")")
  elif [ -f "$ROOT/build/$e.exe" ]; then RES+=("$ROOT/build/$e.exe")
  elif [ -f "$ROOT/build/$e" ]; then RES+=("$ROOT/build/$e")
  else echo "vm_run: no such exe: $e (make first?)" >&2; exit 2; fi
done
[ "$ENVONLY" = 1 ] && RES=()
[ ${#RES[@]} -gt 0 ] || [ ${#PROBES[@]} -gt 0 ] || [ "$ENVONLY" = 1 ] || { echo "vm_run: nothing to run" >&2; exit 2; }
make -s -C "$ROOT" -f tools/Makefile.vm >/dev/null
if [ ${#PROBES[@]} -gt 0 ]; then make -s -C "$ROOT" -f tools/Makefile.probe >/dev/null; fi
[ -x "$RUNNER" ] || { echo "vm_run: $RUNNER missing" >&2; exit 2; }
[ -f "$LAB/LAB_READY" ] || echo "vm_run: warning: $LAB/LAB_READY absent (lab not validated)" >&2

RUNROOT="$LAB/runs/theme-$(date +%m%d-%H%M%S)-$$"   # short: qemu's qmp socket path must stay < 108 bytes
mkdir -p "$RUNROOT"
cleanup() { [ "$KEEP" = 1 ] || rm -rf "$RUNROOT"; }
trap cleanup EXIT

# --- build one payload: run.cmd + helpers + the given exes ---------------------------------------------------
make_payload() {   # $1 = payload dir, $2 = probes? (1/0), rest = exe paths
  local pay="$1" withprobe="$2"; shift 2
  mkdir -p "$pay"
  cp "$ROOT"/build/vm/{vmrun.exe,themeenv.exe,themeenv6.exe,themeset.exe,ctl6.manifest} "$pay/"
  local names=() e n
  for e in "$@"; do cp "$e" "$pay/"; n="$(basename "$e")"; names+=("$n"); done
  if [ "$withprobe" = 1 ]; then cp "$ROOT/build/probe_record.exe" "$ROOT/build/probe_table.csv" "$pay/"; [ -f "$ROOT/tools/probe_table_dpi.csv" ] && cp "$ROOT/tools/probe_table_dpi.csv" "$pay/"; fi
  {
    echo '@echo off'
    echo 'setlocal'
    echo 'cd /d %~dp0'
    echo 'set W=C:\i7w'
    echo 'if not exist C:\i7state mkdir C:\i7state'
    echo "echo @@I7 CFG run config=$CONFIG mode=$MODE %date% %time% > COM1"
    if [ "$MODE" != none ]; then
      echo "if exist C:\\i7state\\cfg_$CNAME.done goto cfgdone"
      echo "echo @@I7 CFG apply $CONFIG %time% > COM1"
      "$ROOT/tools/theme_config.sh" cmd "$CONFIG"
      echo "echo done > C:\\i7state\\cfg_$CNAME.done"
      case "$MODE" in
        logoff) echo 'rem AutoAdminLogon alone logs on only at boot; ForceAutoLogon=1 makes winlogon log on again after a logoff'
                echo 'reg add "HKLM\Software\Microsoft\Windows NT\CurrentVersion\Winlogon" /v ForceAutoLogon /t REG_SZ /d 1 /f > COM1 2>&1'
                echo 'echo @@I7 CFG logoff > COM1'; echo 'shutdown /l /f'
                echo 'rem the logoff kills this script; block so the hook does not power off first'
                echo 'ping -n 300 127.0.0.1 > nul'; echo 'exit /b 0';;
        reboot) echo 'echo @@I7 CFG reboot-pending > COM1'; echo 'exit /b 0';;
      esac
      echo ':cfgdone'
      echo "echo @@I7 CFG active $CONFIG %time% > COM1"
    fi
    echo 'if not exist %W% mkdir %W%'
    echo 'xcopy /y /q *.exe %W%\ > nul'
    echo 'xcopy /y /q *.manifest %W%\ > nul'
    [ "$withprobe" = 1 ] && echo 'xcopy /y /q probe_table*.csv %W%\ > nul'
    echo 'cd /d %W%'
    if [ "$GATE" = 1 ] && [ "$EXPECT" = themed ]; then
      echo 'vmrun.exe -t 200 -n __env -- themeenv6.exe --wait 150'
    else
      echo 'vmrun.exe -t 60 -n __env -- themeenv6.exe'
    fi
    echo 'vmrun.exe -t 60 -n __env_v5 -- themeenv.exe --actctx %W%\ctl6.manifest'
    for n in ${names[@]+"${names[@]}"}; do
      echo "vmrun.exe -t $TIMEOUT -n ${n%.exe} -- %W%\\$n${FILTER:+ \"$FILTER\"}"
    done
    if [ "$withprobe" = 1 ]; then
      local i=0 a
      for a in "${PROBES[@]}"; do
        i=$((i+1))
        echo "vmrun.exe -t 1500 -n probe_${i}_log -- %W%\\probe_record.exe --table %W%\\probe_table.csv $a --out %W%\\probe_$i.csv"
        echo "vmrun.exe -t 120 -n probe_$i.csv -- cmd /c type %W%\\probe_$i.csv"
      done
    fi
    echo 'echo @@I7 DONE %time% > COM1'
  } > "$pay/run.cmd"
}

# runner OUTDIR [env...]: one win10_run.sh call with a watchdog. Seen once (2026-11-09): the guest reached the
# desktop but the logon hook never wrote COM1 and the display froze; without the watchdog that costs the whole
# ORACLE_TIMEOUT.  If serial.log is still empty SILENT_S (default 240) s after the oracle lock was taken, the
# guest is stopped through QMP "quit" and the boot is retried once.
qmp_quit() { python3 - "$1" <<'PY' 2>/dev/null || true
import socket, json, sys
s = socket.socket(socket.AF_UNIX); s.settimeout(5); s.connect(sys.argv[1]); f = s.makefile('rw'); f.readline()
for c in ({"execute": "qmp_capabilities"}, {"execute": "quit"}):
    f.write(json.dumps(c) + "\n"); f.flush(); f.readline()
PY
}
runner1() {   # $1 payload $2 outdir
  local log="$2.runner.log" t0="" now
  mkdir -p "$2"; : > "$log"
  "$RUNNER" "$1" "$2" > "$log" 2>&1 &
  local pid=$!
  while kill -0 $pid 2>/dev/null; do
    sleep 5
    if [ -z "$t0" ]; then grep -q "lock held" "$log" && t0=$(date +%s); continue; fi
    now=$(date +%s)
    if [ ! -s "$2/serial.log" ] && [ $((now - t0)) -gt "${SILENT_S:-240}" ]; then
      echo "vm_run: no serial output ${SILENT_S:-240}s after boot start: stopping the hung guest" >&2
      qmp_quit "$2/qmp.sock"; echo HUNG > "$2/hung"; break
    fi
  done
  wait $pid || true
  tail -1 "$log"
}
runner() {   # $1 payload $2 outdir; env passes through; one retry for a hung (silent) boot
  runner1 "$1" "$2"
  if [ -f "$2/hung" ]; then
    echo "vm_run: retrying the boot once" >&2
    if [ -n "${W10_OVERLAY:-}" ]; then echo "vm_run: (reusing overlay $W10_OVERLAY)" >&2; fi
    mv "$2" "$2.hung"; [ -f "$2.runner.log" ] && mv "$2.runner.log" "$2.hung/runner.log"
    runner1 "$1" "$2"
  fi
}
boot() {   # $1 = payload, $2 = outdir (serial.log)
  local t="${ORACLE_TIMEOUT:-900}"
  if [ "$MODE" = reboot ]; then
    echo "== boot 1/2 (apply $CONFIG, reboot mode)"
    ORACLE_TIMEOUT=300 W10_KEEPDISK=1 runner "$1" "$2.stage1"
    [ -f "$2.stage1/disk.qcow2" ] || { echo "vm_run: stage-1 overlay missing" >&2; return 1; }
    echo "== boot 2/2"
    ORACLE_TIMEOUT="$t" W10_OVERLAY="$2.stage1/disk.qcow2" runner "$1" "$2"
    cat "$2.stage1/serial.log" "$2/serial.log" > "$2/serial.all"
    rm -f "$2.stage1/disk.qcow2"
  else
    ORACLE_TIMEOUT="$t" runner "$1" "$2"
    cp "$2/serial.log" "$2/serial.all"
  fi
}

names_of() { local e; for e in "$@"; do basename "$e" .exe; done; }
rc=0; QUEUE=("${RES[@]+"${RES[@]}"}"); bootno=0; retried=0; first=1; ALLNAMES=(__env __env_v5)
: > "$RUNROOT/serial.combined"
while :; do
  CHUNK=("${QUEUE[@]:0:$PERBOOT}"); QUEUE=("${QUEUE[@]:$PERBOOT}")
  bootno=$((bootno+1)); pay="$RUNROOT/p$bootno"; out="$RUNROOT/o$bootno"
  wp=0; [ "$first" = 1 ] && [ ${#PROBES[@]} -gt 0 ] && wp=1
  make_payload "$pay" "$wp" ${CHUNK[@]+"${CHUNK[@]}"}
  echo "== boot $bootno: ${#CHUNK[@]} exe(s)$([ $wp = 1 ] && echo " + ${#PROBES[@]} probe pass(es)") config=$CONFIG mode=$MODE -> $OUTDIR"
  boot "$pay" "$out" || { rc=3; break; }
  NAMES=(__env __env_v5 $(names_of ${CHUNK[@]+"${CHUNK[@]}"}))
  if [ "$wp" = 1 ]; then i=0; for a in "${PROBES[@]}"; do i=$((i+1)); NAMES+=("probe_${i}_log" "probe_$i.csv"); done; fi
  for n in "${NAMES[@]:2}"; do ALLNAMES+=("$n"); done
  cat "$out/serial.all" >> "$RUNROOT/serial.combined"
  cp "$out/serial.all" "$OUTDIR/_serial_boot$bootno.log"
  python3 "$ROOT/tools/vm_split.py" "$out/serial.all" "$out/split" "${NAMES[@]}" --missing "$out/missing" > /dev/null || true
  first=0
  # exes that never started (boot cut short) get one more boot
  if [ -s "$out/missing" ] && [ "$retried" = 0 ]; then
    retried=1
    while read -r m; do for e in ${CHUNK[@]+"${CHUNK[@]}"}; do [ "$(basename "$e" .exe)" = "$m" ] && QUEUE+=("$e"); done; done < "$out/missing"
    [ ${#QUEUE[@]} -gt 0 ] && echo "vm_run: re-queueing $(wc -l < "$out/missing") missing exe(s)"
  fi
  [ ${#QUEUE[@]} -gt 0 ] || break
done
if [ "$rc" = 0 ]; then
  echo "== results (config=$CONFIG)"
  set +e; python3 "$ROOT/tools/vm_split.py" "$RUNROOT/serial.combined" "$OUTDIR" "${ALLNAMES[@]}"; rc=$?; set -e
fi
echo "results: $OUTDIR (_summary.txt, _env: __env.txt) exit=$rc"
exit $rc
