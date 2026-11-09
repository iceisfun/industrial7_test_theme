#!/usr/bin/env python3
"""Generate docs/research/seed_constants.csv.
Inputs (all committed): 03_wine_probe/out/proton_v6.txt (Wine 11.0 / Proton run, 96 dpi),
07_seed/probe_part_ids.txt (numeric ids of the probe table, produced by running a mingw exe that prints
the vsstyle.h enum values under tools/wine_run.sh).
Every row carries confidence + source; Win10 values that no document proves are emitted as UNKNOWN.
"""
import csv, re, sys, os
R = os.path.dirname(os.path.abspath(__file__)) + "/.."
OUT = R + "/seed_constants.csv"
W = "03_wine_probe/out/proton_v6.txt"
cols = ["class","part","state","prop","esize","dpi","type","v1","v2","v3","v4","hr","relation","confidence","serve","source"]
rows = []
def add(**k):
    r = {c: "" for c in cols}; r.update(k); rows.append(r)

ids = {}
for l in open(R + "/07_seed/probe_part_ids.txt"):
    cls, nm, p, s = l.strip().split(",")
    ids[nm] = (cls.upper() if False else cls, int(p), int(s))
CLS = {"Scrollbar": "ScrollBar", "Combobox": "ComboBox"}
wl = open(R + "/" + W).read().split("\n")
def wsrc(i): return "%s:%d (Wine 11.0 Proton run, 96 dpi, NOT Win10)" % (W, i + 1)

# ---- A. Wine placeholder rows, parsed mechanically -------------------------------------------------
for i, l in enumerate(wl):
    m = re.match(r"PartSize\[(.+?)\]\[(MIN|TRUE|DRAW)\] = hr=(0x[0-9a-f]+) cx=(-?\d+) cy=(-?\d+)", l)
    if m and m.group(1) in ids:
        cls, p, s = ids[m.group(1)]
        add(**{"class": CLS.get(cls, cls), "part": p, "state": s, "prop": "PARTSIZE", "esize": "TS_" + m.group(2),
               "dpi": 96, "type": "size", "v1": m.group(4), "v2": m.group(5), "hr": m.group(3),
               "confidence": "WINE-RUN", "serve": "no", "source": wsrc(i) + " " + m.group(1)})
    m = re.match(r"Margins TMT_(CONTENT|SIZING)MARGINS\[(.+?)\] = hr=(0x[0-9a-f]+) l=(-?\d+) r=(-?\d+) t=(-?\d+) b=(-?\d+)", l)
    if m and m.group(2) in ids and m.group(3) == "0x00000000":
        cls, p, s = ids[m.group(2)]
        add(**{"class": CLS.get(cls, cls), "part": p, "state": s, "prop": "TMT_%sMARGINS" % m.group(1),
               "dpi": 96, "type": "margins", "v1": m.group(4), "v2": m.group(5), "v3": m.group(6), "v4": m.group(7),
               "hr": m.group(3), "confidence": "WINE-RUN", "serve": "no", "source": wsrc(i) + " " + m.group(2)})
    m = re.match(r"Int TMT_(BORDERSIZE|IMAGECOUNT)\[(.+?)\] = hr=(0x[0-9a-f]+) v=(-?\d+)", l)
    if m and m.group(2) in ids and m.group(3) == "0x00000000":
        cls, p, s = ids[m.group(2)]
        add(**{"class": CLS.get(cls, cls), "part": p, "state": s, "prop": "TMT_" + m.group(1), "dpi": 96,
               "type": "int", "v1": m.group(4), "hr": m.group(3), "confidence": "WINE-RUN", "serve": "no",
               "source": wsrc(i) + " " + m.group(2)})
# _Header_InitTheme call set (parts 5 and 7 are not in the probe table, only in the InitTheme block)
for i, l in enumerate(wl):
    m = re.match(r"InitTheme PartSize\(hdc=NULL,part=(\d),state=0,TS_TRUE\) = hr=(0x[0-9a-f]+) cx=(-?\d+) cy=(-?\d+)", l)
    if m and m.group(1) in "457":
        add(**{"class": "Header", "part": m.group(1), "state": 0, "prop": "PARTSIZE", "esize": "TS_TRUE", "dpi": 96,
               "type": "size", "v1": m.group(3), "v2": m.group(4), "hr": m.group(2), "confidence": "WINE-RUN",
               "serve": "no", "source": wsrc(i) + " _Header_InitTheme call tuple"})
    m = re.match(r"InitTheme GetThemePosition\(1,0,TMT_MINSIZE=0xd4b\) = hr=(0x[0-9a-f]+)", l)
    if m:
        add(**{"class": "Header", "part": 1, "state": 0, "prop": "TMT_MINSIZE(3403)", "dpi": 96, "type": "position",
               "hr": m.group(1), "confidence": "WINE-RUN", "serve": "no",
               "source": wsrc(i) + " property ABSENT in Wine theme (E_PROP_ID_UNSUPPORTED); Win10 value UNKNOWN"})

# ---- B. Win10 facts (second-hand oracle / decomp-proven) -------------------------------------------
add(**{"class": "Header", "part": "", "state": "", "prop": "HDM_LAYOUT.cy(text_cy=13..16,edge=2)", "dpi": 96,
       "type": "layout", "v1": 24, "confidence": "ORACLE-REPORTED", "serve": "test-only",
       "source": "Win10 CC30 record: bb #1134 reply #16656 (laneg, 6 boots byte-identical), via docs/research/05_i7_current_state.md section 4.1; not re-read from the bb in this pass"})
add(**{"class": "Header", "part": 1, "state": 0, "prop": "min_height=max(PARTSIZE.TS_TRUE.cy, TMT_MINSIZE.y)", "dpi": 96,
       "type": "derived", "v1": 24, "relation": "eq", "confidence": "ORACLE-INFERRED", "serve": "yes",
       "source": "Header_OnLayout 0x4b2a8: height=max(text_cy+2*edge, HD+0x160) when HD+0x130!=0 (01_consumers.md 4.1, 05 section 4.2). With Win10 result 24 > text arm (13..16 + 4 = 17..20) => HD+0x160 == 24. INFERENCE, composition (part-size vs MINSIZE) UNKNOWN"})
add(**{"class": "(sysmetric)", "prop": "COLOR_BTNFACE(15)", "dpi": 96, "type": "color", "v1": "0x00F0F0F0",
       "confidence": "ORACLE-REPORTED", "serve": "no",
       "source": "crates/w10-win32k-serverinfo/src/lib.rs:87-107 comment, bb #983 oracle run, via 05 section 3.2 (the only observed entry of the 31-colour table); GetThemeSysColor(NULL)=GetSysColor so not a theme row"})
add(**{"class": "Button", "part": 3, "state": 1, "prop": "PARTSIZE", "esize": "TS_DRAW", "dpi": 96, "type": "size",
       "v1": 13, "v2": 13, "relation": "eq", "confidence": "WINE-TESTED-ON-WINDOWS", "serve": "pending",
       "source": "wine dlls/uxtheme/tests/system.c:1219-1225 (test_GetThemePartSize: expected = MulDiv(13, dpi, 96) for BP_CHECKBOX/CBS_CHECKEDNORMAL(=5) TS_DRAW; the test is written to pass on real Windows). State 1 (UNCHECKEDNORMAL) differs from the tested state 5: image-strip frame size is per part in aero, assumption only"})

# ---- C. rules proven from the Win10 decomp (value columns hold the rule id, not a number) -----------
def rule(prop, v1, src, **k):
    add(**dict({"class": "*", "prop": prop, "type": "rule", "v1": v1, "confidence": "DECOMP", "serve": "yes", "source": src}, **k))
rule("PARTSIZE.borderfill.TS_MIN", "max(1,2*BORDERSIZE) square", "GET /api/function/uxtheme/GetThemePartSize/decomp RVA 0x14a20 (02 section 6.1)")
rule("PARTSIZE.borderfill.TS_TRUE", "2*BORDERSIZE+1 square", "same, RVA 0x14a20")
rule("PARTSIZE.borderfill.TS_DRAW", "E_INVALIDARG", "same, RVA 0x14a20", hr="0x80070057")
rule("PARTSIZE.image.TS_MIN", "max(1,sum scaled CONTENTMARGINS) per axis", "CImageFile::GetPartSize RVA 0x14edc (02 section 6.1)")
rule("PARTSIZE.image.TS_TRUE", "selected DIB frame cx,cy (DIBINFO+0xc/+0x10) unless true-size stretch", "RVA 0x14edc; frame-vs-strip meaning NOT decoded (03 section 9 caveat)")
rule("PARTSIZE.errors", "handle->E_HANDLE(0x80070006) then psz NULL->E_POINTER(0x80004003) then no part object->E_FAIL(0x80004005)", "RVA 0x14a20 (02 section 6.1)")
rule("GETTHEMEPOSITION.miss", "0x80070490 (E_PROP_ID_UNSUPPORTED); pPoint NULL->0x80004003 before handle", "ExternalGetPosition 0x6b94 / GetThemePosition RVA 0x6af0 (05 section 1.3 correction)", hr="0x80070490")
rule("GETTHEMEMETRIC", "GetThemeColor-style lookup then ScaleThemeSize(hdc)", "02 section 6 table row GetThemeMetric")
rule("PROPGETTER.order", "out-pointer NULL->E_POINTER, then handle->E_HANDLE, then lookup (miss 0x80070490)", "02 section 6 table (GetThemeInt body == GetThemeColor body, RVA 0x1cf80)")
rule("HANDLE.format", "(generation<<16)|slot; fresh slot gen 1; same (file,class,baseclass,dpi,flag) reopen returns same HTHEME", "CThemeApiHelper::OpenHandle 0x18888, OpenRenderObject 0x19e40 (02 section 4)")
rule("OPEN.lasterror", "success 0; unknown class 0x80070490; NULL class 0x80004003; empty 0x80070057; bad hwnd 0x80070006", "02 section 5 and 13")
rule("CLASSLIST", "';' separated, first success wins, 'App::Class' split on first '::', ASCII case-insensitive", "MatchThemeClassList 0x197c0 (02 section 5.1)")
rule("DEFAULT.GetThemeAppProperties", "3", "CAppInfo::CAppInfo 0x37ac (02 section 14 item 1)")
rule("HEADER.InitTheme.calls", "PARTSIZE(1,0,TS_TRUE) ; POSITION(1,0,3403) ; PARTSIZE(5,0,TS_TRUE) ; PARTSIZE(4,0,TS_TRUE) ; PARTSIZE(7,0,TS_TRUE) ; hdc NULL, prc NULL; HRESULTs of PARTSIZE ignored", "GET /api/function/comctl32/_Header_InitTheme/decomp RVA 0x85d98 (05 section 4.2, 01 section 4.3)")
rule("HEADER.consumers", "min_height=max(p1.cy,minsize.y); HD+0x114=max(HD+0x114,p5.cx); HD+0x158=p4 size; HD+0x144=p7 size", "same, RVA 0x85d98")
rows[-1]["class"] = "Header"

# ---- D. SDK constants (header-defined, checkable with grep) ----------------------------------------
H = "/usr/x86_64-w64-mingw32/include/"
for nm, v, f in [("HP_HEADERITEM",1,"vsstyle.h:647"),("HP_HEADERITEMLEFT",2,"vsstyle.h:648"),("HP_HEADERITEMRIGHT",3,"vsstyle.h:649"),
                 ("HP_HEADERSORTARROW",4,"vsstyle.h:650"),("HP_HEADERDROPDOWN",5,"vsstyle.h:651"),
                 ("HP_HEADERDROPDOWNFILTER",6,"vsstyle.h:652"),("HP_HEADEROVERFLOW",7,"vsstyle.h:653")]:
    add(**{"class": "Header", "part": v, "prop": nm, "type": "const", "v1": v, "confidence": "SDK-HEADER", "serve": "yes", "source": H + f})
for nm, v, f in [("TMT_MINSIZE",3403,"vssym32.h:342"),("TMT_SIZINGMARGINS",3601,"vssym32.h:349"),("TMT_CONTENTMARGINS",3602,"vssym32.h:350"),
                 ("TMT_BORDERSIZE",2403,"vssym32.h:300"),("TMT_IMAGECOUNT",2401,"vssym32.h:298"),("TMT_WIDTH",2416,"vssym32.h:313"),
                 ("TMT_HEIGHT",2417,"vssym32.h:314"),("TMT_IMAGEFILE",3001,"vssym32.h:331"),("TMT_FILLCOLOR",3802,"vssym32.h:353"),
                 ("TMT_TEXTCOLOR",3803,"vssym32.h:354"),("TMT_COMPOSITED",2204,"vssym32.h:281"),("TMT_ANIMATIONDURATION",5006,"vssym32.h:399"),
                 ("TMT_FRAMESPERSECOND",2426,"vssym32.h:323"),("TMT_SIZINGBORDERWIDTH",1201,"vssym32.h:204"),
                 ("TMT_SCROLLBARWIDTH",1202,"vssym32.h:205"),("TMT_CAPTIONBARHEIGHT",1205,"vssym32.h:208"),("TMT_GLYPHINDEX",2418,"vssym32.h:315")]:
    add(**{"class": "*", "prop": nm, "type": "const", "v1": v, "confidence": "SDK-HEADER", "serve": "yes", "source": H + f})
for nm, v in [("TS_MIN",0),("TS_TRUE",1),("TS_DRAW",2)]:
    add(**{"class": "*", "prop": nm, "type": "const", "v1": v, "confidence": "SDK-HEADER", "serve": "yes", "source": H + "uxtheme.h:280-284"})

# ---- E. UNKNOWN rows: the Win10 values the table must still receive ------------------------------------
def unk(cls, part, state, prop, esize="", typ="size", why=""):
    add(**{"class": cls, "part": part, "state": state, "prop": prop, "esize": esize, "dpi": 96, "type": typ, "v1": "UNKNOWN",
           "confidence": "UNKNOWN", "serve": "no", "source": "needs Win10 probe row; " + why if why else "needs Win10 probe row"})
for es in ("TS_TRUE","TS_MIN","TS_DRAW"):
    unk("Header",1,0,"PARTSIZE",es,why="stage 1 blocker" if es=="TS_TRUE" else "")
unk("Header",1,0,"TMT_MINSIZE(3403)",typ="position",why="stage 1 blocker: decides whether 24 comes from size or MINSIZE")
for p in (4,5,7): unk("Header",p,0,"PARTSIZE","TS_TRUE",why="stage 1 (stored in HD+0x158/0x114/0x144)")
unk("Header",1,0,"GetThemeTextExtent(L\"M\",DT_NOPREFIX)",typ="size",why="Header_NewFont themed text height, per font")
unk("Header",1,"1..12","TMT_CONTENTMARGINS",typ="margins",why="GetThemeBackgroundContentRect inset")
unk("Header",1,0,"GetThemeBackgroundExtent",typ="size",why="Header_ComputeItemWidth")
unk("Header",1,0,"IsThemePartDefined",typ="bool")
unk("Header",1,0,"TMT_BGTYPE/TMT_IMAGELAYOUT/TMT_IMAGECOUNT",typ="enum/int")
unk("(sysmetric)","","","SM_CYEDGE",typ="int",why="runtime metric, 2 at 96 dpi on stock Win10 is expected but unrecorded on the oracle (01 section 8)")
unk("(sysmetric)","","","SM_CYMENUCHECK(72)@96dpi",typ="int",why="seeds HD+0x114 dropdown width (01 section 4.4)")
for cls,p,s,es in [("Button",1,1,"TS_DRAW"),("Button",2,1,"TS_DRAW"),("Button",3,1,"TS_DRAW"),("Button",7,1,"TS_DRAW"),
                   ("Status",3,0,"TS_DRAW"),("TrackBar",3,1,"TS_TRUE"),("ListView",10,0,"TS_TRUE"),("MonthCal",10,0,"TS_TRUE"),
                   ("Window",18,0,"TS_TRUE"),("Window",19,0,"TS_TRUE"),("Tooltip",5,1,"TS_TRUE"),("Rebar",1,0,"TS_TRUE")]:
    unk(cls,p,s,"PARTSIZE",es,why="01_consumers.md section 7 probe input")
unk("ListView",1,3,"TMT_CONTENTMARGINS",typ="margins"); unk("ListView",6,0,"TMT_CONTENTMARGINS",typ="margins")
unk("ComboBox",4,1,"TMT_BORDERSIZE",typ="int"); unk("Edit",5,1,"TMT_CONTENTMARGINS",typ="margins")
unk("Progress",0,0,"TMT_FRAMESPERSECOND/ANIMATIONDURATION/PIXELSPERFRAME/ANIMATIONDELAY",typ="int")
unk("Toolbar",0,0,"TMT_SIZINGMARGINS",typ="margins")
unk("Window","1,2,3,5",1,"TMT_CAPTIONMARGINS(3603)",typ="margins",why="id 3603 = TMT_CAPTIONMARGINS, vssym32.h:351; 01 section 7 probe input")
for dpi in (120,144,192):
    unk("Header",1,0,"PARTSIZE","TS_TRUE",why="dpi plateau %d" % dpi); rows[-1]["dpi"] = dpi

with open(OUT, "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=cols, lineterminator="\n"); w.writeheader(); w.writerows(rows)
print(len(rows), "rows ->", OUT)
