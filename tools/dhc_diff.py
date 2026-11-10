#!/usr/bin/env python3
"""dhc_diff.py RUN_A RUN_B : compare two recording runs (probe_N.csv, probe_draw*.txt, __env.txt) ignoring volatile values.
Volatile: RUN rows, UptimeMs, time_utc, SM93 (undocumented metric), lasterr garbage is kept. Exit 1 on any non-volatile difference."""
import csv, sys, os, re, glob
VOL_ENV = ("UptimeMs", "WaitThemeActive", "ms=")
def load_csv(p):
    d = {}
    with open(p, newline="", errors="replace") as f:
        for r in csv.DictReader(f):
            if r.get("id") == "RUN": continue
            if r["id"] == "P00006" and re.search(r"\.(R12|)$", r["sub"]) and "SM93" in r["sub"]: continue
            d[(r["id"], r["sub"])] = tuple(r[c] for c in ("hr","lasterr","ret","v0","v1","v2","v3","v4","v5","txt"))
    return d
def load_txt(p, env=False):
    out = []
    for l in open(p, errors="replace").read().splitlines():
        if l.startswith("@@I7") or not l.strip(): continue
        if env and any(l.startswith("ENV " + v) for v in VOL_ENV): continue
        out.append(l)
    return out
def main():
    a, b = sys.argv[1:3]; bad = 0
    for pa in sorted(glob.glob(a + "/probe_[0-9].csv")):
        pb = pa.replace(a, b, 1)
        if not os.path.exists(pb): print("MISSING", pb); bad += 1; continue
        x, y = load_csv(pa), load_csv(pb)
        diffs = [k for k in set(x) | set(y) if x.get(k) != y.get(k)]
        print(f"{os.path.basename(pa)}: {len(x)} keys / {len(y)} keys, {len(diffs)} differences"); bad += len(diffs)
        for k in sorted(diffs)[:15]: print("   ", k, x.get(k), "|", y.get(k))
    for n in ("probe_draw.txt", "probe_draw_nomanifest.txt", "__env.txt", "__env_v5.txt", "filehash.txt"):
        pa, pb = os.path.join(a, n), os.path.join(b, n)
        if not (os.path.exists(pa) and os.path.exists(pb)): print("MISSING", n); bad += 1; continue
        x, y = load_txt(pa, n.startswith("__env")), load_txt(pb, n.startswith("__env"))
        dd = [(i, p, q) for i, (p, q) in enumerate(zip(x, y)) if p != q] + ([("len", len(x), len(y))] if len(x) != len(y) else [])
        print(f"{n}: {len(x)} lines, {len(dd)} differences"); bad += len(dd)
        for t in dd[:8]: print("   ", str(t)[:200])
    sys.exit(1 if bad else 0)
main()
