@echo off
rem probe_run.bat <label>  - the standard recorder passes for the CURRENT machine configuration.
rem Copy probe_record.exe, probe_table.csv and this file to one folder on the Win10 box, open cmd there, run:
rem     probe_run.bat aero-light-100
rem Results land in results\<label>*.csv.  Repeat 3x per configuration (std user) and 3x elevated if wanted;
rem compare with:  python tools\probe_compare.py results\a.csv results\b.csv results\c.csv   (expect 0 differences)
setlocal
set L=%1
if "%L%"=="" set L=unlabelled
if not exist results mkdir results
rem 1. quick CC30 set (header height first)
probe_record.exe --label %L% --prio 0 --out results\%L%.p0.csv
rem 2. everything priority 1-2 incl. property sweeps, comctl32 v6 context
probe_record.exe --label %L% --prio 2 --out results\%L%.csv
rem 3. same controls under comctl32 v5 (un-manifested app) - header/status/tab/... control heights
probe_record.exe --label %L%_v5 --comctl v5 --prio 1 --grp "env,header,ctl" --out results\%L%_v5.csv
rem 4. SetThemeAppProperties(0): do all opens fail with 0x80070490?
probe_record.exe --label %L%_appprops0 --app-props 0 --prio 1 --grp "env,open,header" --out results\%L%_appprops0.csv
rem 5. dark-mode opt-in variants (undocumented ordinal 135 SetPreferredAppMode 1=AllowDark 2=ForceDark)
probe_record.exe --label %L%_dark_allow --dark allow --prio 2 --grp "env,open,listview,header,button,sys" --out results\%L%_dark_allow.csv
probe_record.exe --label %L%_dark_force --dark force --prio 2 --grp "env,open,listview,header,button,sys" --out results\%L%_dark_force.csv
rem 6. optional, long (every defined part/state x every property id): uncomment
rem probe_record.exe --label %L%_full --sweep full --prio 3 --grp gridfull --timeout 1800 --out results\%L%_gridfull.csv
echo done - results\%L%*.csv
