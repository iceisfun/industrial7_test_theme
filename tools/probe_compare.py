#!/usr/bin/env python3
"""Compare probe_record results.csv files.

    python3 tools/probe_compare.py A.csv B.csv [C.csv ...] [--max 40] [--ignore-lasterr]

Rows are keyed by (id, sub); the RUN.* meta rows are ignored.  With more than two files every file is
compared to the first (determinism check: the lane should see 0 differences between repeated runs on
the same configuration).  Exit status 1 when any difference is found.
"""
import csv, sys

def load(path, ign):
    d = {}
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            if r["id"] == "RUN": continue
            k = (r["id"], r["sub"])
            v = tuple(r[c] for c in ("hr", "ret", "v0", "v1", "v2", "v3", "v4", "v5", "txt") + (() if ign else ("lasterr",)))
            d[k] = v
    return d

def main():
    a = sys.argv[1:]; mx = 40; ign = False
    if "--max" in a: i = a.index("--max"); mx = int(a[i + 1]); del a[i:i + 2]
    if "--ignore-lasterr" in a: a.remove("--ignore-lasterr"); ign = True
    if len(a) < 2: sys.exit(__doc__)
    base = load(a[0], ign); bad = 0
    for p in a[1:]:
        o = load(p, ign)
        keys = sorted(set(base) | set(o))
        diff = [k for k in keys if base.get(k) != o.get(k)]
        print("%s vs %s: %d keys, %d differ" % (a[0], p, len(keys), len(diff)))
        for k in diff[:mx]:
            print("  %s %s\n    A: %s\n    B: %s" % (k[0], k[1], base.get(k), o.get(k)))
        bad += len(diff)
    sys.exit(1 if bad else 0)
main()
