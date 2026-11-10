#!/usr/bin/env python3
"""Supplement table for DPI recorders: every catalogued (class, part, state) at explicit DPIs
(OpenThemeDataForDpi 96/120/144/192): part sizes, DrawThemeBackground hashes (+ tiny raw), content rects,
margins, class fonts.  Output tools/probe_table_dpi.csv (ids D00001...), same columns as probe_table.csv."""
import os, sys
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
import gen_probe_table as g

g.ROWS.clear()
DPIS = [96, 120, 144, 192]
for cls, parts in g.CAT.items():
    grp = "dpi_" + cls.lower()
    nc = 1 if cls == "Scrollbar" else 0
    for pn_, num, sts in parts:
        if pn_.startswith("("): continue
        states = g.parse_states(sts)
        z = ("0", 0)
        first = states[0] if states else z
        def A(api, st, d, **kw):
            g.add(1, grp, api, cls, num, st[1], dpi=d, nc=nc, partname=pn_, statename=st[0], **kw)
        for d in DPIS:
            for st in [z] + states:
                for ts_i in (0, 1, 2):
                    A("GetThemePartSize", st, d, ts=ts_i, aux=g.RECT_STD if ts_i == 2 else ("hdc=0" if ts_i == 1 else ""), propname=g.TSN[ts_i])
            for st in states or [z]:
                A("DrawThemeBackground", st, d, aux="size=32:24", propname="draw32x24")
            A("DrawThemeBackground", first, d, aux="size=10:10;dump=1", propname="draw10x10raw")
            A("DrawThemeBackground", first, d, aux="size=64:48", propname="draw64x48")
            A("GetThemeBackgroundContentRect", first, d, aux=g.RECT_STD)
            A("GetThemeBackgroundExtent", first, d, aux=g.RECT_STD)
            for nm, n in g.MARGIN_PROPS:
                A("GetThemeMargins", first, d, prop=n, propname=nm)
            A("IsThemePartDefined", first, d)
    for d in DPIS:
        g.add(1, grp, "GetThemeFont", cls, 0, 0, prop=210, dpi=d, nc=nc, propname="TMT_FONT")
        g.add(1, grp, "GetThemeTextMetrics", cls, 0, 0, dpi=d, nc=nc)
n = g.write(os.path.join(here, "probe_table_dpi.csv"))
# distinct ids so the two tables can be merged
p = os.path.join(here, "probe_table_dpi.csv")
s = open(p).read().replace("\nP", "\nD")
open(p, "w", newline="\n").write(s)
print("wrote %d rows" % n)
