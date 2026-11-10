#!/usr/bin/env bash
# dhc_multi.sh GROUP RUN - several dark_hc_classic configurations recorded in ONE boot (runtime switches in sequence,
# undoing each one), to avoid one oracle-lock wait per configuration.  Output: data/win10/dark_hc_classic/<cfg>/<RUN>/
#   GROUP A: hc -> hcwhite -> (hc off) -> dark -> darkhc        GROUP B: classic -> classicdark -> (light) -> hcnothemes
# Per config: __env, __env_v5, filehash, probe_draw(+nomanifest), probe_record v6 full / v5 std / --dark force / --app-props 0.
set -eu
cd "$(dirname "$0")/.."
ROOT=$PWD; G="$1"; RUN="$2"; TC=tools/theme_config.sh
LAB=$HOME/w10lab; WORK=$LAB/runs/dhc-$(date +%m%d-%H%M%S)-$$; mkdir -p "$WORK/p" "$WORK/o"
P=$WORK/p
cp build/vm/{vmrun.exe,themeenv.exe,themeenv6.exe,themeset.exe,ctl6.manifest} build/{filehash,probe_draw,probe_draw_nomanifest,probe_record}.exe build/probe_table.csv "$P/"
[ -f tools/probe_table_dpi.csv ] && cp tools/probe_table_dpi.csv "$P/"
case "$G" in
  A) SEQ="hc hcwhite -hcoff dark darkhc";;
  B) SEQ="classic classicdark -light hcnothemes";;
  *) echo "group A|B"; exit 2;;
esac
CFGS=""; NAMES=(); 
{
  echo '@echo off'; echo 'setlocal'; echo 'cd /d %~dp0'; echo 'set W=C:\i7w'
  echo 'if not exist %W% mkdir %W%'
  echo 'xcopy /y /q *.exe %W%\ > nul'; echo 'xcopy /y /q *.manifest %W%\ > nul'; echo 'xcopy /y /q probe_table*.csv %W%\ > nul'
  echo 'cd /d %W%'
  for s in $SEQ; do
    case "$s" in
      -hcoff) echo "echo @@I7 CFG step hc-off %time% > COM1"; echo 'themeset.exe hc off > COM1 2>&1'; echo 'ping -n 11 127.0.0.1 > nul'; continue;;
      -light) echo "echo @@I7 CFG step light %time% > COM1"; $TC cmd light; continue;;
    esac
    CFGS="$CFGS $s"
    echo "echo @@I7 CFG apply $s %time% > COM1"
    # hcwhite applied while hc black is on: plain scheme switch; darkhc: dark first then hc
    $TC cmd "$s"
    echo "echo @@I7 CFG active $s %time% > COM1"
    echo "vmrun.exe -t 60 -n __${s}_env -- themeenv6.exe"
    echo "vmrun.exe -t 60 -n __${s}_env_v5 -- themeenv.exe --actctx %W%\\ctl6.manifest"
    echo "vmrun.exe -t 60 -n __${s}_filehash -- %W%\\filehash.exe"
    echo "vmrun.exe -t 300 -n probe_draw_$s -- %W%\\probe_draw.exe"
    echo "vmrun.exe -t 300 -n probe_drawnm_$s -- %W%\\probe_draw_nomanifest.exe"
    i=0
    for a in "--label ${s}_v6 --prio 3 --sweep full --comctl v6" "--label ${s}_v5 --prio 3 --sweep std --comctl v5" \
             "--label ${s}_darkforce --prio 2 --sweep std --comctl v6 --dark force" "--label ${s}_appprops0 --prio 2 --sweep std --comctl v6 --app-props 0"; do
      i=$((i+1))
      echo "vmrun.exe -t 600 -n probe_${s}_${i}_log -- %W%\\probe_record.exe --table %W%\\probe_table.csv $a --out %W%\\probe_${s}_$i.csv"
      echo "vmrun.exe -t 120 -n probe_${s}_$i.csv -- cmd /c type %W%\\probe_${s}_$i.csv"
    done
  done
  echo 'echo @@I7 DONE %time% > COM1'
} > "$P/run.cmd"
echo "$CFGS" > "$WORK/cfgs"
ORACLE_TIMEOUT=${ORACLE_TIMEOUT:-1500} "$LAB/tools/win10_run.sh" "$P" "$WORK/o" > "$WORK/runner.log" 2>&1 || true
tail -2 "$WORK/runner.log"
[ -s "$WORK/o/serial.log" ] || { echo "no serial output"; exit 3; }
rc=0
for s in $CFGS; do
  out=data/win10/dark_hc_classic/$s/$RUN; mkdir -p "$out"
  nm=(); for n in __${s}_env __${s}_env_v5 __${s}_filehash probe_draw_$s probe_drawnm_$s; do nm+=("$n"); done
  for i in 1 2 3 4; do nm+=("probe_${s}_${i}_log" "probe_${s}_$i.csv"); done
  python3 tools/vm_split.py "$WORK/o/serial.log" "$WORK/split_$s" "${nm[@]}" > /dev/null || rc=$?
  mv "$WORK/split_$s/__${s}_env.txt" "$out/__env.txt"; mv "$WORK/split_$s/__${s}_env_v5.txt" "$out/__env_v5.txt"
  mv "$WORK/split_$s/__${s}_filehash.txt" "$out/filehash.txt"
  mv "$WORK/split_$s/probe_draw_$s.txt" "$out/probe_draw.txt"; mv "$WORK/split_$s/probe_drawnm_$s.txt" "$out/probe_draw_nomanifest.txt"
  for i in 1 2 3 4; do mv "$WORK/split_$s/probe_${s}_${i}_log.txt" "$out/probe_${i}_log.txt"; mv "$WORK/split_$s/probe_${s}_$i.csv" "$out/probe_$i.csv"; done
  cp "$WORK/split_$s/_summary.txt" "$out/_summary.txt"
  grep -E "@@I7 CFG|THEMESET|I7VM" "$WORK/o/serial.log" | tr -d '\r' > "$out/_boot.txt"
  gzip -c "$WORK/o/serial.log" > "$out/_serial_group$G.log.gz"
done
echo "group $G run $RUN rc=$rc"
