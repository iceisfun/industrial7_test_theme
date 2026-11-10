#!/usr/bin/env python3
"""join_probe.py TABLE RESULTS OUT.csv - join probe_record results with the probe table (one line per result row:
table columns id,grp,api,class,part,state,prop,ts,dpi,subapp,nc,aux,partname,statename,propname + hr,lasterr,ret,v0..v5,txt)."""
import csv, sys
tab = {r['id']: r for r in csv.DictReader(open(sys.argv[1], newline=''))}
keep = ['id', 'grp', 'api', 'class', 'part', 'state', 'prop', 'ts', 'dpi', 'subapp', 'nc', 'aux', 'partname', 'statename', 'propname']
out = csv.writer(open(sys.argv[3], 'w', newline=''), lineterminator='\n')
out.writerow(keep + ['sub', 'hr', 'lasterr', 'ret', 'v0', 'v1', 'v2', 'v3', 'v4', 'v5', 'txt'])
for i, l in enumerate(open(sys.argv[2], newline='')):
    if i == 0: continue
    x = l.rstrip('\n').split(',', 11)
    x += [''] * (12 - len(x))
    t = tab.get(x[0], {})
    out.writerow([t.get(k, '') for k in keep] + x[1:12] if t else [x[0]] + [''] * 14 + x[1:12])
