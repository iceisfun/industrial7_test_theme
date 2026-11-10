#!/usr/bin/env python3
"""Generate tools/probe_table.csv (the input of tools/probe_record.c) and tools/probe_groups.md.

    python3 tools/gen_probe_table.py [--out tools/probe_table.csv] [--md tools/probe_groups.md]

Symbolic names are checked against the mingw-w64 vsstyle.h / vssym32.h (the SDK enumerations): a
name whose number differs from the spec aborts generation; names the SDK header does not have are
kept and flagged 'name-not-in-sdk-header' in the CSV note column.
No Win10 *value* is produced here, only questions.  CSV fields never contain ',' '"' or newlines.
"""
import re, sys, os, collections, argparse

INC = os.environ.get("MINGW_INC", "/usr/x86_64-w64-mingw32/include")

def load_symbols():
    sym = collections.defaultdict(set)
    for fn in ("vsstyle.h", "vssym32.h"):
        txt = open(os.path.join(INC, fn), encoding="latin-1").read()
        for m in re.finditer(r"^\s*#define\s+([A-Z][A-Z0-9_]+)\s+(-?\d+)\b", txt, re.M):
            sym[m.group(1)].add(int(m.group(2)))
        for m in re.finditer(r"^\s*([A-Z][A-Z0-9_]+)\s*=\s*(-?\d+)\s*,?", txt, re.M):
            sym[m.group(1)].add(int(m.group(2)))
    return sym
SYM = load_symbols()
UNVERIFIED = set()
BAD = []

def chk(name, num):
    if name in SYM:
        if num not in SYM[name]:
            BAD.append("symbol %s: spec says %d, headers say %s" % (name, num, sorted(SYM[name])))
        return ""
    UNVERIFIED.add(name)
    return "name-not-in-sdk-header"

def tmt(name):
    if name not in SYM or len(SYM[name]) != 1:
        sys.exit("TMT name %s unknown/ambiguous in vssym32.h" % name)
    return next(iter(SYM[name]))

COLS = ["id", "prio", "grp", "api", "class", "part", "state", "prop", "ts", "dpi", "subapp", "nc",
        "aux", "partname", "statename", "propname", "note"]
ROWS = []

def add(prio, grp, api, cls="-", part=0, state=0, prop=0, ts="", dpi=0, subapp="", nc=0, aux="",
        partname="", statename="", propname="", note=""):
    r = dict(prio=prio, grp=grp, api=api, part=part, state=state, prop=prop, ts=ts, dpi=dpi, subapp=subapp,
             nc=nc, aux=aux, partname=partname, statename=statename, propname=propname, note=note)
    r["class"] = cls
    r["note"] = str(note).replace(",", ";").replace('"', "'")
    for k, v in r.items():
        s = str(v)
        assert "," not in s and '"' not in s and "\n" not in s, (k, s)
    ROWS.append(r)

# (partname, partnum, "STATE=n ...")  -- ids verified against vsstyle.h by chk(); survey: docs/research/01 sec 3
CAT = collections.OrderedDict()
CAT["Header"] = [
    ("HP_HEADERITEM", 1, "HIS_NORMAL=1 HIS_HOT=2 HIS_PRESSED=3 HIS_SORTEDNORMAL=4 HIS_SORTEDHOT=5 HIS_SORTEDPRESSED=6 HIS_ICONNORMAL=7 HIS_ICONHOT=8 HIS_ICONPRESSED=9 HIS_ICONSORTEDNORMAL=10 HIS_ICONSORTEDHOT=11 HIS_ICONSORTEDPRESSED=12"),
    ("HP_HEADERITEMLEFT", 2, "HILS_NORMAL=1 HILS_HOT=2 HILS_PRESSED=3"),
    ("HP_HEADERITEMRIGHT", 3, "HIRS_NORMAL=1 HIRS_HOT=2 HIRS_PRESSED=3"),
    ("HP_HEADERSORTARROW", 4, "HSAS_SORTEDUP=1 HSAS_SORTEDDOWN=2"),
    ("HP_HEADERDROPDOWN", 5, "HDDS_NORMAL=1 HDDS_SOFTHOT=2 HDDS_HOT=3"),
    ("HP_HEADERDROPDOWNFILTER", 6, "HDDFS_NORMAL=1 HDDFS_SOFTHOT=2 HDDFS_HOT=3"),
    ("HP_HEADEROVERFLOW", 7, "HOFS_NORMAL=1 HOFS_HOT=2"),
]
CAT["Button"] = [
    ("BP_PUSHBUTTON", 1, "PBS_NORMAL=1 PBS_HOT=2 PBS_PRESSED=3 PBS_DISABLED=4 PBS_DEFAULTED=5"),
    ("BP_RADIOBUTTON", 2, "RBS_UNCHECKEDNORMAL=1 RBS_UNCHECKEDHOT=2 RBS_CHECKEDNORMAL=5 RBS_CHECKEDDISABLED=8"),
    ("BP_CHECKBOX", 3, "CBS_UNCHECKEDNORMAL=1 CBS_UNCHECKEDHOT=2 CBS_CHECKEDNORMAL=5 CBS_MIXEDNORMAL=9"),
    ("BP_GROUPBOX", 4, "GBS_NORMAL=1 GBS_DISABLED=2"),
    ("BP_USERBUTTON", 5, ""),
    ("BP_COMMANDLINK", 6, "CMDLS_NORMAL=1 CMDLS_HOT=2 CMDLS_PRESSED=3 CMDLS_DISABLED=4"),
    ("BP_COMMANDLINKGLYPH", 7, "CMDLGS_NORMAL=1 CMDLGS_HOT=2 CMDLGS_PRESSED=3 CMDLGS_DISABLED=4"),
]
CAT["Tab"] = [
    ("TABP_TABITEM", 1, "TIS_NORMAL=1 TIS_HOT=2 TIS_SELECTED=3 TIS_DISABLED=4 TIS_FOCUSED=5"),
    ("TABP_TABITEMLEFTEDGE", 2, "TILES_NORMAL=1 TILES_SELECTED=3"),
    ("TABP_TABITEMRIGHTEDGE", 3, "TIRES_NORMAL=1 TIRES_SELECTED=3"),
    ("TABP_TABITEMBOTHEDGE", 4, "TIBES_NORMAL=1 TIBES_SELECTED=3"),
    ("TABP_TOPTABITEM", 5, "TTIS_NORMAL=1 TTIS_SELECTED=3"),
    ("TABP_TOPTABITEMLEFTEDGE", 6, "TTILES_NORMAL=1 TTILES_SELECTED=3"),
    ("TABP_TOPTABITEMRIGHTEDGE", 7, "TTIRES_NORMAL=1 TTIRES_SELECTED=3"),
    ("TABP_TOPTABITEMBOTHEDGE", 8, "TTIBES_NORMAL=1 TTIBES_SELECTED=3"),
    ("TABP_PANE", 9, ""),
    ("TABP_BODY", 10, ""),
]
CAT["ListView"] = [
    ("LVP_LISTITEM", 1, "LISS_NORMAL=1 LISS_HOT=2 LISS_SELECTED=3 LISS_DISABLED=4 LISS_SELECTEDNOTFOCUS=5 LISS_HOTSELECTED=6"),
    ("LVP_LISTGROUP", 2, ""), ("LVP_LISTDETAIL", 3, ""), ("LVP_LISTSORTEDDETAIL", 4, ""), ("LVP_EMPTYTEXT", 5, ""),
    ("LVP_GROUPHEADER", 6, ""), ("LVP_GROUPHEADERLINE", 7, ""), ("LVP_EXPANDBUTTON", 8, ""),
    ("LVP_COLLAPSEBUTTON", 9, ""), ("LVP_COLUMNDETAIL", 10, ""),
]
CAT["TreeView"] = [
    ("TVP_TREEITEM", 1, "TREIS_NORMAL=1 TREIS_HOT=2 TREIS_SELECTED=3 TREIS_DISABLED=4 TREIS_SELECTEDNOTFOCUS=5 TREIS_HOTSELECTED=6"),
    ("TVP_GLYPH", 2, "GLPS_CLOSED=1 GLPS_OPENED=2"),
    ("TVP_BRANCH", 3, ""),
    ("TVP_HOTGLYPH", 4, "HGLPS_CLOSED=1 HGLPS_OPENED=2"),
]
CAT["Toolbar"] = [
    ("TP_BUTTON", 1, "TS_NORMAL=1 TS_HOT=2 TS_PRESSED=3 TS_DISABLED=4 TS_CHECKED=5 TS_HOTCHECKED=6"),
    ("TP_DROPDOWNBUTTON", 2, "TS_NORMAL=1"), ("TP_SPLITBUTTON", 3, "TS_NORMAL=1"), ("TP_SPLITBUTTONDROPDOWN", 4, "TS_NORMAL=1"),
    ("TP_SEPARATOR", 5, "TS_NORMAL=1"), ("TP_SEPARATORVERT", 6, "TS_NORMAL=1"), ("TP_DROPDOWNBUTTONGLYPH", 7, "TS_NORMAL=1"),
]
CAT["Rebar"] = [
    ("RP_GRIPPER", 1, ""), ("RP_GRIPPERVERT", 2, ""), ("RP_BAND", 3, ""), ("RP_CHEVRON", 4, "CHEVS_NORMAL=1 CHEVS_HOT=2 CHEVS_PRESSED=3"),
    ("RP_CHEVRONVERT", 5, "CHEVSV_NORMAL=1 CHEVSV_HOT=2 CHEVSV_PRESSED=3"), ("RP_BACKGROUND", 6, ""),
    ("RP_SPLITTER", 7, ""), ("RP_SPLITTERVERT", 8, ""),
]
CAT["Progress"] = [
    ("PP_BAR", 1, ""), ("PP_BARVERT", 2, ""), ("PP_CHUNK", 3, ""), ("PP_CHUNKVERT", 4, ""),
    ("PP_FILL", 5, "PBFS_NORMAL=1 PBFS_ERROR=2 PBFS_PAUSED=3 PBFS_PARTIAL=4"),
    ("PP_FILLVERT", 6, "PBFVS_NORMAL=1 PBFVS_ERROR=2 PBFVS_PAUSED=3 PBFVS_PARTIAL=4"),
    ("PP_PULSEOVERLAY", 7, ""), ("PP_MOVEOVERLAY", 8, ""), ("PP_PULSEOVERLAYVERT", 9, ""), ("PP_MOVEOVERLAYVERT", 10, ""),
    ("PP_TRANSPARENTBAR", 11, "PBBS_NORMAL=1 PBBS_PARTIAL=2"), ("PP_TRANSPARENTBARVERT", 12, "PBBVS_NORMAL=1 PBBVS_PARTIAL=2"),
]
CAT["TrackBar"] = [
    ("TKP_TRACK", 1, "TRS_NORMAL=1"), ("TKP_TRACKVERT", 2, "TRVS_NORMAL=1"),
    ("TKP_THUMB", 3, "TUS_NORMAL=1 TUS_HOT=2 TUS_PRESSED=3 TUS_FOCUSED=4 TUS_DISABLED=5"),
    ("TKP_THUMBBOTTOM", 4, "TUBS_NORMAL=1 TUBS_HOT=2"), ("TKP_THUMBTOP", 5, "TUTS_NORMAL=1 TUTS_HOT=2"),
    ("TKP_THUMBVERT", 6, "TUVS_NORMAL=1 TUVS_HOT=2"), ("TKP_THUMBLEFT", 7, "TUVLS_NORMAL=1 TUVLS_HOT=2"),
    ("TKP_THUMBRIGHT", 8, "TUVRS_NORMAL=1 TUVRS_HOT=2"), ("TKP_TICS", 9, "TSS_NORMAL=1"), ("TKP_TICSVERT", 10, "TSVS_NORMAL=1"),
]
CAT["Spin"] = [
    ("SPNP_UP", 1, "UPS_NORMAL=1 UPS_HOT=2 UPS_PRESSED=3 UPS_DISABLED=4"), ("SPNP_DOWN", 2, "DNS_NORMAL=1 DNS_HOT=2 DNS_PRESSED=3 DNS_DISABLED=4"),
    ("SPNP_UPHORZ", 3, "UPHZS_NORMAL=1 UPHZS_HOT=2"), ("SPNP_DOWNHORZ", 4, "DNHZS_NORMAL=1 DNHZS_HOT=2"),
]
CAT["Edit"] = [
    ("EP_EDITTEXT", 1, "ETS_NORMAL=1 ETS_HOT=2 ETS_SELECTED=3 ETS_DISABLED=4 ETS_FOCUSED=5 ETS_READONLY=6 ETS_ASSIST=7 ETS_CUEBANNER=8"),
    ("EP_CARET", 2, ""), ("EP_BACKGROUND", 3, ""), ("EP_PASSWORD", 4, ""),
    ("EP_BACKGROUNDWITHBORDER", 5, "EBWBS_NORMAL=1 EBWBS_HOT=2 EBWBS_DISABLED=3 EBWBS_FOCUSED=4"),
    ("EP_EDITBORDER_NOSCROLL", 6, "EPSN_NORMAL=1 EPSN_HOT=2 EPSN_FOCUSED=3 EPSN_DISABLED=4"),
    ("EP_EDITBORDER_HSCROLL", 7, "EPSH_NORMAL=1 EPSH_HOT=2 EPSH_FOCUSED=3 EPSH_DISABLED=4"),
    ("EP_EDITBORDER_VSCROLL", 8, "EPSV_NORMAL=1 EPSV_HOT=2 EPSV_FOCUSED=3 EPSV_DISABLED=4"),
    ("EP_EDITBORDER_HVSCROLL", 9, "EPSHV_NORMAL=1 EPSHV_HOT=2 EPSHV_FOCUSED=3 EPSHV_DISABLED=4"),
]
CAT["Combobox"] = [
    ("CP_DROPDOWNBUTTON", 1, "CBXS_NORMAL=1 CBXS_HOT=2 CBXS_PRESSED=3 CBXS_DISABLED=4"),
    ("CP_BACKGROUND", 2, ""), ("CP_TRANSPARENTBACKGROUND", 3, ""),
    ("CP_BORDER", 4, "CBB_NORMAL=1 CBB_HOT=2 CBB_FOCUSED=3 CBB_DISABLED=4"),
    ("CP_READONLY", 5, "CBRO_NORMAL=1 CBRO_HOT=2 CBRO_PRESSED=3 CBRO_DISABLED=4"),
    ("CP_DROPDOWNBUTTONRIGHT", 6, "CBXSR_NORMAL=1"), ("CP_DROPDOWNBUTTONLEFT", 7, "CBXSL_NORMAL=1"),
    ("CP_CUEBANNER", 8, "CBCB_NORMAL=1"),
]
CAT["Scrollbar"] = [
    ("SBP_ARROWBTN", 1, "ABS_UPNORMAL=1 ABS_UPHOT=2 ABS_UPPRESSED=3 ABS_UPDISABLED=4 ABS_DOWNNORMAL=5 ABS_LEFTNORMAL=9 ABS_RIGHTNORMAL=13"),
    ("SBP_THUMBBTNHORZ", 2, "SCRBS_NORMAL=1 SCRBS_HOT=2 SCRBS_PRESSED=3 SCRBS_DISABLED=4"),
    ("SBP_THUMBBTNVERT", 3, "SCRBS_NORMAL=1 SCRBS_HOT=2 SCRBS_PRESSED=3 SCRBS_DISABLED=4"),
    ("SBP_LOWERTRACKHORZ", 4, "SCRBS_NORMAL=1"), ("SBP_UPPERTRACKHORZ", 5, "SCRBS_NORMAL=1"),
    ("SBP_LOWERTRACKVERT", 6, "SCRBS_NORMAL=1"), ("SBP_UPPERTRACKVERT", 7, "SCRBS_NORMAL=1"),
    ("SBP_GRIPPERHORZ", 8, "SCRBS_NORMAL=1"), ("SBP_GRIPPERVERT", 9, "SCRBS_NORMAL=1"),
    ("SBP_SIZEBOX", 10, "SZB_RIGHTALIGN=1 SZB_LEFTALIGN=2"),
]
CAT["Status"] = [("SP_PANE", 1, ""), ("SP_GRIPPERPANE", 2, ""), ("SP_GRIPPER", 3, "")]
CAT["Tooltip"] = [
    ("TTP_STANDARD", 1, "TTSS_NORMAL=1 TTSS_LINK=2"), ("TTP_STANDARDTITLE", 2, "TTSS_NORMAL=1 TTSS_LINK=2"),
    ("TTP_BALLOON", 3, "TTBS_NORMAL=1 TTBS_LINK=2"), ("TTP_BALLOONTITLE", 4, "TTBS_NORMAL=1 TTBS_LINK=2"),
    ("TTP_CLOSE", 5, "TTCS_NORMAL=1 TTCS_HOT=2 TTCS_PRESSED=3"),
    ("TTP_BALLOONSTEM", 6, "TTBSS_POINTINGUPLEFTWALL=1 TTBSS_POINTINGUPCENTERED=2 TTBSS_POINTINGUPRIGHTWALL=3"),
    ("TTP_WRENCH", 7, "TTWS_NORMAL=1 TTWS_HOT=2 TTWS_PRESSED=3"),
]
CAT["Window"] = [
    ("WP_CAPTION", 1, "CS_ACTIVE=1 CS_INACTIVE=2 CS_DISABLED=3"), ("WP_SMALLCAPTION", 2, "CS_ACTIVE=1 CS_INACTIVE=2"),
    ("WP_MINCAPTION", 3, "MXCS_ACTIVE=1"), ("WP_MAXCAPTION", 5, "MXCS_ACTIVE=1"),
    ("WP_FRAMELEFT", 7, "FS_ACTIVE=1 FS_INACTIVE=2"), ("WP_FRAMEBOTTOM", 9, "FS_ACTIVE=1 FS_INACTIVE=2"),
    ("WP_SYSBUTTON", 13, "SBS_NORMAL=1"),
    ("WP_MINBUTTON", 15, "MINBS_NORMAL=1 MINBS_HOT=2 MINBS_PUSHED=3 MINBS_DISABLED=4"),
    ("WP_MAXBUTTON", 17, "MAXBS_NORMAL=1 MAXBS_HOT=2 MAXBS_PUSHED=3 MAXBS_DISABLED=4"),
    ("WP_CLOSEBUTTON", 18, "CBS_NORMAL=1 CBS_HOT=2 CBS_PUSHED=3 CBS_DISABLED=4"),
    ("WP_SMALLCLOSEBUTTON", 19, "CBS_NORMAL=1 CBS_HOT=2 CBS_PUSHED=3 CBS_DISABLED=4"),
    ("WP_RESTOREBUTTON", 21, "RBS_NORMAL=1 RBS_HOT=2 RBS_PUSHED=3 RBS_DISABLED=4"),
    ("WP_HELPBUTTON", 23, "HBS_NORMAL=1"), ("WP_HORZSCROLL", 25, "HSS_NORMAL=1"), ("WP_VERTSCROLL", 27, "VSS_NORMAL=1"),
    ("WP_DIALOG", 29, ""),
]
CAT["Menu"] = [
    ("MENU_BARBACKGROUND", 7, "MB_ACTIVE=1 MB_INACTIVE=2"), ("MENU_BARITEM", 8, "MBI_NORMAL=1 MBI_HOT=2 MBI_PUSHED=3 MBI_DISABLED=4"),
    ("MENU_POPUPBACKGROUND", 9, ""), ("MENU_POPUPBORDERS", 10, ""),
    ("MENU_POPUPCHECK", 11, "MC_CHECKMARKNORMAL=1 MC_CHECKMARKDISABLED=2 MC_BULLETNORMAL=3"),
    ("MENU_POPUPCHECKBACKGROUND", 12, "MCB_DISABLED=1 MCB_NORMAL=2 MCB_BITMAP=3"), ("MENU_POPUPGUTTER", 13, ""),
    ("MENU_POPUPITEM", 14, "MPI_NORMAL=1 MPI_HOT=2 MPI_DISABLED=3 MPI_DISABLEDHOT=4"),
    ("MENU_POPUPSEPARATOR", 15, ""), ("MENU_POPUPSUBMENU", 16, "MSM_NORMAL=1 MSM_DISABLED=2"),
]
CAT["Listbox"] = [("LBCP_BORDER_HSCROLL", 1, "LBPSH_NORMAL=1"), ("LBCP_BORDER_HVSCROLL", 2, "LBPSHV_NORMAL=1"),
                  ("LBCP_BORDER_NOSCROLL", 3, "LBPSN_NORMAL=1"), ("LBCP_BORDER_VSCROLL", 4, "LBPSV_NORMAL=1"), ("LBCP_ITEM", 5, "LBPSI_HOT=1")]
CAT["MonthCal"] = [("MC_BACKGROUND", 1, ""), ("MC_BORDERS", 2, ""), ("MC_GRIDBACKGROUND", 3, ""), ("MC_COLHEADERSPLITTER", 4, ""),
                   ("MC_GRIDCELLBACKGROUND", 5, ""), ("MC_GRIDCELL", 6, ""), ("MC_GRIDCELLUPPER", 7, ""), ("MC_TRAILINGGRIDCELL", 8, ""),
                   ("MC_TRAILINGGRIDCELLUPPER", 9, ""), ("MC_NAVNEXT", 10, "MCNN_NORMAL=1"), ("MC_NAVPREV", 11, "MCNP_NORMAL=1")]
CAT["DatePicker"] = [("DP_DATETEXT", 1, "DPDT_NORMAL=1"), ("DP_DATEBORDER", 2, "DPDB_NORMAL=1"), ("DP_SHOWCALENDARBUTTONRIGHT", 3, "DPSCBR_NORMAL=1")]
CAT["Static"] = [("(class level)", 0, "")]
CAT["Link"] = [("LP_HYPERLINK", 1, "HLS_NORMALTEXT=1 HLS_LINKTEXT=2")]
CAT["TextStyle"] = [("TEXT_MAININSTRUCTION", 1, ""), ("TEXT_INSTRUCTION", 2, ""), ("TEXT_BODYTITLE", 3, ""), ("TEXT_BODYTEXT", 4, ""),
                    ("TEXT_SECONDARYTEXT", 5, ""), ("TEXT_HYPERLINKTEXT", 6, ""), ("TEXT_EXPANDED", 7, ""), ("TEXT_LABEL", 8, ""), ("TEXT_CONTROLLABEL", 9, "")]
CAT["Globals"] = [("(class level)", 0, "")]
# survey sec 3: parts comctl32 really needs (priority 1); None = all parts; rest priority 2
MUST = {
    "Header": None, "Button": {1, 2, 3, 6, 7}, "Status": {0, 1, 2, 3}, "TrackBar": {3, 4, 5, 6, 7, 8, 9, 10}, "Tooltip": {1, 3, 5, 6, 7},
    "Rebar": {1, 2, 3, 6, 7, 8}, "ListView": {1, 6, 7, 9, 10}, "MonthCal": {10}, "Combobox": {0, 4}, "Edit": {1, 3, 4, 5},
    "Toolbar": {0, 1}, "Progress": {0, 1, 5, 7, 8}, "Window": {1, 2, 3, 5, 18, 19}, "Tab": None, "TreeView": None,
    "Spin": None, "Scrollbar": {1, 2, 3, 8, 9, 10}, "DatePicker": None, "Static": None,
}

def parse_states(s):
    out = []
    for tok in s.split():
        n, v = tok.split("=")
        out.append((n, int(v)))
    return out

def is_must(cls, part):
    m = MUST.get(cls, set())
    return m is None or part in m

def P(name):
    return (name, tmt(name))
INT_PROPS = [P(n) for n in ("TMT_BORDERSIZE", "TMT_IMAGECOUNT", "TMT_WIDTH", "TMT_HEIGHT", "TMT_MINDPI1", "TMT_ROUNDCORNERWIDTH")]
METRIC_PROPS = [P(n) for n in ("TMT_BORDERSIZE", "TMT_WIDTH", "TMT_HEIGHT")]
ENUM_PROPS = [P(n) for n in ("TMT_BGTYPE", "TMT_IMAGELAYOUT", "TMT_SIZINGTYPE", "TMT_BORDERTYPE", "TMT_CONTENTALIGNMENT", "TMT_HALIGN", "TMT_VALIGN")]
BOOL_PROPS = [P(n) for n in ("TMT_TRANSPARENT", "TMT_COMPOSITED", "TMT_NOETCHEDEFFECT", "TMT_COMPOSITEDOPAQUE", "TMT_BORDERONLY")]
COLOR_PROPS = [P(n) for n in ("TMT_TEXTCOLOR", "TMT_FILLCOLOR", "TMT_BORDERCOLOR")]
MARGIN_PROPS = [P(n) for n in ("TMT_SIZINGMARGINS", "TMT_CONTENTMARGINS", "TMT_CAPTIONMARGINS")]
POS_PROPS = [P(n) for n in ("TMT_MINSIZE", "TMT_MINSIZE1", "TMT_OFFSET", "TMT_TEXTSHADOWOFFSET")]
ALL_DPIS = [96, 120, 144, 168, 192]
RECT_STD = "rect=0:0:100:30"
TSN = ("TS_MIN", "TS_TRUE", "TS_DRAW")

def part_rows(cls, grp, partname, pn, states, prio, dpis, nc=0, mode="lean"):
    """Measurement set for one (class, part).  mode: full (header) / lean (priority-1 parts) / min (rest)."""
    pnote = chk(partname, pn)
    first = states[0] if states else ("0", 0)
    z = ("0", 0)
    def A(api, st, dl=(0,), **kw):
        for d in dl:
            add(prio, grp, api, cls, pn, st[1], dpi=d, nc=nc, partname=partname, statename=st[0], note=pnote, **kw)
    # sizes: TS_MIN/TRUE/DRAW; dpi variants only for state 0 and the first state
    szstates = [z] + states if mode != "min" else [z] + states[:1]
    for st in szstates:
        for ts_i in ((0, 1, 2) if mode != "min" else (1,)):
            dl = ([0] + list(dpis)) if (st[1] in (0, first[1]) and mode != "min") else [0]
            A("GetThemePartSize", st, dl, ts=ts_i, aux=RECT_STD if ts_i == 2 else "", propname=TSN[ts_i])
    for st in [z] + states:
        A("IsThemePartDefined", st)
    A("IsThemeBackgroundPartiallyTransparent", first)
    A("GetThemeBackgroundContentRect", first, [0] + (list(dpis) if mode != "min" else []), aux=RECT_STD)
    if mode == "min":
        A("GetThemeMargins", first, prop=3602, propname="TMT_CONTENTMARGINS")
        return
    A("GetThemeBackgroundExtent", first, aux=RECT_STD)
    for nm, num in MARGIN_PROPS:
        A("GetThemeMargins", first, [0] + (list(dpis) if num == 3602 else []), prop=num, propname=nm)
    for nm, num in POS_PROPS[:2]: A("GetThemePosition", z, prop=num, propname=nm)
    A("GetThemeTextMetrics", first)
    A("GetThemeTextExtent", first, aux="text=Mg;flags=0x800")
    for nm, num in INT_PROPS[:4]: A("GetThemeInt", first, prop=num, propname=nm)
    for nm, num in METRIC_PROPS[1:]: A("GetThemeMetric", first, [0, 144], prop=num, propname=nm)
    for nm, num in ENUM_PROPS[:3]: A("GetThemeEnumValue", first, prop=num, propname=nm)
    A("GetThemeBool", first, prop=tmt("TMT_TRANSPARENT"), propname="TMT_TRANSPARENT")
    for nm, num in COLOR_PROPS[:2]: A("GetThemeColor", first, prop=num, propname=nm)
    A("GetThemeFont", first, prop=210, propname="TMT_FONT")
    A("GetThemePropertyOrigin", first, prop=3602, propname="TMT_CONTENTMARGINS")

# ================================================================ environment
for cls, note in (
    ("ThemeState", "IsThemeActive/IsAppThemed/IsCompositionActive/GetThemeAppProperties/GetCurrentThemeName/QueryThemeServices/dialog texture"),
    ("Modules", "paths+file versions of uxtheme comctl32 user32 gdi32 win32u ntdll; OS build/UBR"),
    ("Display", "system/window DPI, awareness, screen size, LOGPIXELS"),
    ("HighContrast", "SPI_GETHIGHCONTRAST"),
    ("Personalize", "registry: AppsUseLightTheme SystemUsesLightTheme ThemeManager ColorName SizeName DllName accent colour"),
    ("SysMetrics", "GetSystemMetrics 0..99 and GetSystemMetricsForDpi at 96 120 144 168 192"),
    ("NonClientMetrics", "SPI_GETNONCLIENTMETRICS (+ForDpi) all fields and LOGFONTs; ICONMETRICS; icon title font"),
    ("Fonts", "message font and DEFAULT_GUI_FONT LOGFONT + TEXTMETRIC"),
    ("SysColors", "GetSysColor 0..30")):
    add(1, "env", "Env", cls, note=note)

# ================================================================ open / error / lifetime semantics (02 sec 5,6,13,15)
OPEN_CASES = [
    ("@NULL", "OpenThemeData(h,NULL) -> E_POINTER expected"), ("@EMPTY", "empty class list"), ("NoSuchClass", ""), ("Header", ""),
    ("header", "case-insensitivity"), ("HEADER", ""), ("Header;Button", "first match wins"), ("NoSuch;Button", "list falls through"),
    ("NoSuch;AlsoNoSuch", "last failure HRESULT"), ("Header::Foo", "App::Class form"), ("Button-OK;Button", "dialog button table (01 sec 2.2)"),
    ("Button-CANCEL;Button", ""), ("Button-YES;Button", ""), ("Button-NO;Button", ""), ("Button-CLOSE;Button", ""), ("Button-HELP;Button", ""),
    ("Button-APPLY;Button", ""), ("Button-ABORT;Button", ""), ("Button-RETRY;Button", ""), ("Button-IGNORE;Button", ""),
    ("Button-TRYAGAIN;Button", ""), ("Button-CONTINUE;Button", ""),
    ("Indeterminate::Progress", "marquee progress"), ("CompositedWindow::Window", "uxtheme-internal class"), ("DWMWindow", "AeroWizard title"),
    ("AeroWizard", ""), ("TryHarder", ""), ("EmptyMarkup", ""), ("EditComposited", ""), ("Explorer::ListView", "sub-app qualified"),
    ("ItemsView", "Explorer item view (speculative name)"), ("CommandModule", "Explorer command bar (speculative)"), ("Navigation", "speculative"),
    ("BreadcrumbBar", "speculative"), ("SearchBox", "speculative"), ("Taskband", "speculative"), ("StartPanel", "speculative"),
    ("TaskDialog", "speculative"), ("TaskDialogStyle", "speculative"), ("ControlPanel", "speculative"),
    ("ToolTip", "alt spelling of Tooltip"), ("DarkMode_Explorer::ListView", "dark mapping form"), ("Globals", "global class"),
    ("Textstyle", ""), ("TextStyle", ""), ("Clock", "speculative"), ("TrayNotify", "speculative"), ("Flyout", "speculative"),
    ("ScrollBar", ""), ("Scrollbar", "case"), ("ComboBox", ""), ("Combobox", ""),
    ("Edit", ""), ("Page", "speculative"), ("ExplorerBar", "speculative"), ("Tab", ""), ("Window", ""), ("Menu", ""), ("Rebar", ""), ("Toolbar", ""),
    ("Status", ""), ("Tooltip", ""), ("TrackBar", ""), ("Spin", ""), ("Progress", ""), ("ListView", ""), ("TreeView", ""), ("Listbox", ""),
    ("MonthCal", ""), ("DatePicker", ""), ("Static", ""), ("Link", ""), ("Button", ""),
]
KEY_OPEN = {"@NULL", "@EMPTY", "NoSuchClass", "Header", "Button", "Header;Button", "NoSuch;Button"}
seen = set()
for c, note in OPEN_CASES:
    if c in seen: continue
    seen.add(c)
    add(1 if c in KEY_OPEN else 2, "open", "OpenTheme", c, note=note)
    add(2, "open", "OpenTheme", c, aux="hwnd=0", note="NULL hwnd")
add(1, "open", "OpenTheme", "Header", aux="hwnd=bad", note="invalid hwnd -> E_HANDLE (0x80070006)")
add(2, "open", "OpenTheme", "Header", aux="flags=1", note="OpenThemeDataEx OTD_FORCE_RECT_SIZING")
add(2, "open", "OpenTheme", "Header", aux="flags=2", note="OpenThemeDataEx OTD_NONCLIENT")
add(2, "open", "OpenTheme", "Header", aux="flags=3", note="OpenThemeDataEx both")
for c in ("ScrollBar", "Scrollbar", "Window", "Header", "Menu", "Button", "Tab", "ListView"):
    add(1 if c in ("ScrollBar", "Window") else 2, "open", "OpenTheme", c, nc=1, note="OpenNcThemeData (uxtheme ordinal 49)")
for d in ALL_DPIS:
    for c in ("Header", "Button", "ScrollBar", "Window", "Tab"):
        add(2, "open", "OpenTheme", c, dpi=d, note="OpenThemeDataForDpi")
for sub, cls in (("Explorer", "ListView"), ("Explorer", "TreeView"), ("DarkMode_Explorer", "ListView"), ("DarkMode_Explorer", "TreeView"),
                 ("DarkMode_Explorer", "ScrollBar"), ("DarkMode_CFD", "Button"), ("DarkMode", "Button"), ("InfoPaneToolbar", "Toolbar"),
                 ("PreviewPane", "Toolbar"), ("Explorer", "ScrollBar"), ("Explorer", "Header"), ("Explorer", "Tab"), ("(empty)", "Header"), ("$", "Header")):
    add(2, "open", "OpenTheme", cls, subapp=sub, note="SetWindowTheme(hwnd,subapp,NULL) then OpenThemeData")

ERR_APIS = [
    ("GetThemePartSize", dict(ts=1), 1), ("GetThemeColor", dict(prop=tmt("TMT_TEXTCOLOR")), 1), ("GetThemeInt", dict(prop=tmt("TMT_BORDERSIZE")), 1),
    ("GetThemeMetric", dict(prop=tmt("TMT_BORDERSIZE")), 2), ("GetThemeMargins", dict(prop=tmt("TMT_CONTENTMARGINS")), 2),
    ("GetThemePosition", dict(prop=tmt("TMT_MINSIZE")), 2), ("GetThemeFont", dict(prop=210), 2), ("GetThemeBool", dict(prop=tmt("TMT_TRANSPARENT")), 2),
    ("GetThemeEnumValue", dict(prop=tmt("TMT_BGTYPE")), 2), ("GetThemeRect", dict(prop=tmt("TMT_DEFAULTPANESIZE")), 2),
    ("GetThemePropertyOrigin", dict(prop=tmt("TMT_CONTENTMARGINS")), 2),
    ("GetThemeBackgroundContentRect", dict(aux=RECT_STD), 2), ("GetThemeBackgroundExtent", dict(aux=RECT_STD), 2),
    ("GetThemeTextExtent", dict(aux="text=M;flags=0x800"), 2), ("GetThemeTextMetrics", {}, 2), ("IsThemePartDefined", {}, 2),
    ("IsThemeBackgroundPartiallyTransparent", {}, 2), ("GetThemeString", dict(prop=tmt("TMT_CSSNAME")), 3),
    ("GetThemeFilename", dict(prop=tmt("TMT_IMAGEFILE")), 3), ("GetThemeIntList", dict(prop=tmt("TMT_TRANSITIONDURATIONS")), 3),
]
for api, extra, pr in ERR_APIS:
    for hc, note in (("@NULLH", "NULL HTHEME"), ("@BOGUS", "forged HTHEME 0x7fff0001"), ("@STALE", "closed HTHEME")):
        add(pr, "err", api, hc, 1, 1, note=note, **extra)
    ex2 = dict(extra); ex2["aux"] = (ex2.get("aux", "") + ";noout=1").lstrip(";")
    add(pr, "err", api, "Header", 1, 1, note="valid handle, NULL output pointer", **ex2)
    add(pr, "err", api, "@NULLH", 1, 1, note="NULL HTHEME and NULL output pointer: which error wins", **ex2)
    add(pr, "err", api, "Header", 99, 99, note="part/state outside the table (default-fallback rule)", **extra)
add(1, "err", "CloseThemeData", "@NULLH", note="CloseThemeData(NULL)")
add(1, "err", "CloseThemeData", "@BOGUS", note="CloseThemeData(forged)")
add(1, "err", "CloseThemeData", "Header", note="sub close1: first close of a private open; close2: second close of the same value")
add(2, "err", "OpenTheme", "Header", aux="twice=1", note="two opens at one dpi: same HTHEME value? (recorded as same/different)")
for p in (0, 1, 2, 3, 4, 7):
    add(2, "err", "SetThemeAppProperties", "-", aux="value=%d" % p, note="SetThemeAppProperties(v): then IsThemeActive + Open(Button); restored to the original afterwards")
add(2, "err", "SetWindowTheme", "-", aux="sub=;ids=", note="SetWindowTheme(hwnd,L\"\",L\"\") then Open(Header): de-themed window")
add(2, "err", "SetWindowTheme", "-", aux="sub=Explorer;ids=", note="SetWindowTheme Explorer then GetWindowTheme")
add(2, "err", "SetWindowTheme", "-", aux="hwnd=0", note="SetWindowTheme(NULL,...)")

# ================================================================ header (CC30)
G = "header"
for pn_, num, sts in CAT["Header"]:
    states = parse_states(sts)
    for n, v in states: chk(n, v)
    part_rows("Header", G, pn_, num, states, 1, ALL_DPIS, mode="full")
for nm, v in (("HBG_DETAILS", 1), ("HBG_ICON", 2)):
    chk(nm, v)
    for ts_i in range(3):
        add(1, G, "GetThemePartSize", "Header", 0, v, ts=ts_i, aux=RECT_STD if ts_i == 2 else "", partname="(root)", statename=nm, propname=TSN[ts_i])
    add(1, G, "IsThemePartDefined", "Header", 0, v, partname="(root)", statename=nm)
for d in [0] + ALL_DPIS:   # exactly _Header_InitTheme's calls (hdc=NULL, prc=NULL)
    add(1, G, "GetThemePartSize", "Header", 1, 0, ts=1, dpi=d, aux="hdc=0", partname="HP_HEADERITEM", propname="TS_TRUE", note="CC30 input A (_Header_InitTheme 0x85d98)")
    add(1, G, "GetThemePosition", "Header", 1, 0, prop=tmt("TMT_MINSIZE"), dpi=d, partname="HP_HEADERITEM", propname="TMT_MINSIZE", note="CC30 input B")
    for pp, nm in ((5, "HP_HEADERDROPDOWN"), (4, "HP_HEADERSORTARROW"), (7, "HP_HEADEROVERFLOW")):
        add(1, G, "GetThemePartSize", "Header", pp, 0, ts=1, dpi=d, aux="hdc=0", partname=nm, propname="TS_TRUE", note="_Header_InitTheme dropdown/sort/overflow")
for ts_i in (0, 1):
    add(1, G, "GetThemePartSize", "Header", 1, 0, ts=ts_i, aux="hdc=1", partname="HP_HEADERITEM", propname=TSN[ts_i], note="with a screen DC instead of NULL")
for fnt in ("gui", "segoe9", "msshell8", "lg"):
    add(1, G, "GetThemeTextExtent", "Header", 1, 0, aux="text=M;flags=0x800;font=%s" % fnt, partname="HP_HEADERITEM", note="Header_NewFont 0x86174: extent of M with DT_NOPREFIX")
    add(1, G, "GetThemeTextMetrics", "Header", 1, 0, aux="font=%s" % fnt, partname="HP_HEADERITEM")
    add(2, G, "GetThemeTextExtent", "Header", 1, 0, aux="text=...;flags=0x800;font=%s" % fnt, partname="HP_HEADERITEM", note="classic fallback string")
for st, nm in ((1, "HIS_NORMAL"), (2, "HIS_HOT"), (3, "HIS_PRESSED"), (4, "HIS_SORTEDNORMAL")):
    add(1, G, "GetThemeTextExtent", "Header", 1, st, aux="text=Name;flags=0x2800;font=segoe9", partname="HP_HEADERITEM", statename=nm)
for st in range(0, 13):
    for rc in ("0:0:100:24", "0:0:100:40", "0:0:0:0"):
        add(1, G, "GetThemeBackgroundContentRect", "Header", 1, st, aux="rect=" + rc, partname="HP_HEADERITEM", statename="state%d" % st, note="Header_DrawItem / ComputeItemWidth 0x98a0c")
        add(1, G, "GetThemeBackgroundExtent", "Header", 1, st, aux="rect=" + rc, partname="HP_HEADERITEM", statename="state%d" % st)
for rc in ("0:0:20:20", "0:0:16:5", "0:0:8:21"):
    for pp in (4, 5, 7):
        add(2, G, "GetThemeBackgroundContentRect", "Header", pp, 1, aux="rect=" + rc, partname="part%d" % pp)
for rc in ("0:0:100:24", "0:0:100:40", "0:0:6:24", "0:0:400:100"):
    add(1, G, "GetThemePartSize", "Header", 1, 0, ts=2, aux="rect=" + rc, partname="HP_HEADERITEM", propname="TS_DRAW")
for st in range(1, 13):
    add(1, G, "GetThemeColor", "Header", 1, st, prop=3803, partname="HP_HEADERITEM", propname="TMT_TEXTCOLOR", statename="state%d" % st)
for nm, num in (("TMT_SIZINGMARGINS", 3601), ("TMT_CONTENTMARGINS", 3602), ("TMT_CAPTIONMARGINS", 3603)):
    for st in range(0, 13):
        add(1, G, "GetThemeMargins", "Header", 1, st, prop=num, partname="HP_HEADERITEM", propname=nm, statename="state%d" % st)
add(1, G, "GetThemeMargins", "Header", 1, 0, prop=10000, partname="HP_HEADERITEM", propname="pseudo-10000", note="02 sec 6: pseudo id rewritten to 3602 + dpi scaling; public name UNKNOWN")
add(2, G, "GetThemeMargins", "Header", 1, 1, prop=10000, aux="rect=0:0:100:24", partname="HP_HEADERITEM", propname="pseudo-10000", note="with prc != NULL expected E_INVALIDARG")
for d in ALL_DPIS:
    for nm, num in (("TMT_CONTENTMARGINS", 3602), ("TMT_SIZINGMARGINS", 3601)):
        add(1, G, "GetThemeMargins", "Header", 1, 1, prop=num, dpi=d, partname="HP_HEADERITEM", propname=nm)
for nm, num in POS_PROPS:
    add(1, G, "GetThemePosition", "Header", 1, 1, prop=num, partname="HP_HEADERITEM", propname=nm)
add(1, G, "GetThemeFont", "Header", 1, 0, prop=210, partname="HP_HEADERITEM", propname="TMT_FONT")
add(1, G, "GetThemeFont", "Header", 0, 0, prop=210, partname="(root)", propname="TMT_FONT")
for nm in ("TMT_BGTYPE", "TMT_IMAGELAYOUT"):
    add(1, G, "GetThemeEnumValue", "Header", 1, 1, prop=tmt(nm), partname="HP_HEADERITEM", propname=nm)
add(1, G, "GetThemeInt", "Header", 1, 1, prop=tmt("TMT_IMAGECOUNT"), partname="HP_HEADERITEM", propname="TMT_IMAGECOUNT", note="Wine theme: 12 images, 288/12=24")
for st in (0, 1, 2, 4, 7, 10):
    add(1, G, "GetThemeFilename", "Header", 1, st, prop=tmt("TMT_IMAGEFILE"), partname="HP_HEADERITEM", statename="state%d" % st, propname="TMT_IMAGEFILE", note="which bitmap (information only)")
for a, b in ((1, 2), (2, 1), (1, 3), (2, 3), (1, 4)):
    add(2, G, "GetThemeTransitionDuration", "Header", 1, a, aux="to=%d" % b, prop=6000, partname="HP_HEADERITEM", propname="TMT_TRANSITIONDURATIONS")
for st in (0, 1, 2, 4, 7):
    add(1, G, "PropSweep", "Header", 1, st, partname="HP_HEADERITEM", note="every property id 1..8100 with origin != NOTFOUND, then each typed getter")
for pp in (0, 4, 5, 6, 7):
    add(1, G, "PropSweep", "Header", pp, 1 if pp else 0, partname="part%d" % pp)
add(1, G, "PartGrid", "Header", aux="pmax=24;smax=24", note="every defined (part,state) 0..24 with TS sizes")
for fnt in ("none", "gui", "segoe9", "msshell8", "lg"):
    for items in (0, 3):
        for stl in ("0x0", "0x100"):
            add(1, G, "Ctl", "header", aux="font=%s;items=%d;style=%s" % (fnt, items, stl), note="HDM_LAYOUT cy, window height, themed flag")
add(1, G, "Ctl", "header", aux="font=segoe9;items=3;style=0x0;theme=off", note="after SetWindowTheme(hwnd,L\"\",L\"\"): classic height")
add(1, G, "Ctl", "header", aux="font=segoe9;items=3;style=0x0;theme=explorer", note="after SetWindowTheme Explorer")
for fnt, th in (("none", ""), ("segoe9", ""), ("segoe9", ";theme=explorer"), ("msshell8", "")):
    add(1, G, "Ctl", "lvheader", aux="font=%s;items=3%s" % (fnt, th), note="ListView-owned header height + row height")

# ================================================================ every other class
for cls, parts in CAT.items():
    if cls == "Header": continue
    grp = cls.lower()
    nc = 1 if cls == "Scrollbar" else 0
    for pn_, num, sts in parts:
        states = parse_states(sts)
        for n, v in states: chk(n, v)
        prio = 1 if is_must(cls, num) else 2
        dpis = [120, 144, 192]
        if pn_.startswith("("):
            for nm in ("TMT_TEXTCOLOR", "TMT_FILLCOLOR"):
                add(prio, grp, "GetThemeColor", cls, 0, 0, prop=tmt(nm), partname=pn_, propname=nm)
            add(prio, grp, "GetThemeTextMetrics", cls, 0, 0, partname=pn_)
            add(prio, grp, "GetThemeFont", cls, 0, 0, prop=210, partname=pn_, propname="TMT_FONT")
            continue
        part_rows(cls, grp, pn_, num, states, prio, dpis, nc=nc, mode="lean" if prio == 1 else "min")
        if prio == 1:
            add(1, grp, "PropSweep", cls, num, states[0][1] if states else 0, nc=nc, partname=pn_, statename=states[0][0] if states else "0")
    add(1 if is_must(cls, 0) else 2, grp, "GetThemeTextMetrics", cls, 0, 0, note="class-level text metrics (status/toolbar/rebar bar height)")
    add(2, grp, "GetThemeFont", cls, 0, 0, prop=210, propname="TMT_FONT")
    add(2, grp, "GetThemeMargins", cls, 0, 0, prop=3602, propname="TMT_CONTENTMARGINS", note="class level (Combobox 0,0)")
    add(2, grp, "GetThemeMargins", cls, 0, 0, prop=3601, propname="TMT_SIZINGMARGINS")
    add(2, grp, "GetThemeInt", cls, 0, 0, prop=2403, propname="TMT_BORDERSIZE")
    for nm in ("TMT_COMPOSITED", "TMT_COMPOSITEDOPAQUE"):
        add(2, grp, "GetThemeBool", cls, 0, 0, prop=tmt(nm), propname=nm)
    add(2, grp, "PartGrid", cls, nc=nc, aux="pmax=40;smax=20")

# targeted extras straight from the survey section 7
add(1, "combobox", "GetThemeInt", "Combobox", 4, 1, prop=2403, propname="TMT_BORDERSIZE", partname="CP_BORDER", statename="CBB_NORMAL", note="ComboBox_CalcControlRects")
add(1, "edit", "GetThemeMargins", "Edit", 5, 1, prop=3602, propname="TMT_CONTENTMARGINS", partname="EP_BACKGROUNDWITHBORDER", statename="EBWBS_NORMAL", note="Edit_Size")
add(1, "edit", "GetThemeFont", "Edit", 1, 8, prop=210, propname="TMT_FONT", partname="EP_EDITTEXT", statename="ETS_CUEBANNER")
add(1, "edit", "GetThemeColor", "Edit", 1, 8, prop=3803, propname="TMT_TEXTCOLOR", partname="EP_EDITTEXT", statename="ETS_CUEBANNER")
add(1, "edit", "GetThemeFont", "Edit", 4, 0, prop=tmt("TMT_GLYPHFONT"), propname="TMT_GLYPHFONT", partname="EP_PASSWORD")
add(1, "edit", "GetThemeInt", "Edit", 4, 0, prop=tmt("TMT_GLYPHINDEX"), propname="TMT_GLYPHINDEX", partname="EP_PASSWORD")
add(1, "toolbar", "GetThemeBool", "Toolbar", 1, 4, prop=tmt("TMT_NOETCHEDEFFECT"), propname="TMT_NOETCHEDEFFECT", partname="TP_BUTTON", statename="TS_DISABLED")
add(1, "toolbar", "GetThemeColor", "Toolbar", 0, 0, prop=3803, propname="TMT_TEXTCOLOR")
add(1, "toolbar", "GetThemeMargins", "Toolbar", 0, 0, prop=3601, propname="TMT_SIZINGMARGINS", note="TB_OnSize")
for nm in ("TMT_FRAMESPERSECOND", "TMT_PIXELSPERFRAME", "TMT_ANIMATIONDELAY", "TMT_ANIMATIONDURATION"):
    add(1, "progress", "GetThemeInt", "Progress", 0, 0, prop=tmt(nm), propname=nm, note="Progress_AnimateInitialize")
add(1, "progress", "GetThemeInt", "Indeterminate::Progress", 0, 0, prop=tmt("TMT_ANIMATIONDURATION"), propname="TMT_ANIMATIONDURATION")
for nm in ("TMT_HALIGN", "TMT_VALIGN"):
    add(1, "progress", "GetThemeEnumValue", "Progress", 7, 0, prop=tmt(nm), propname=nm, partname="PP_PULSEOVERLAY")
add(1, "treeview", "GetThemeColor", "TreeView", 3, 0, prop=tmt("TMT_COLOR"), propname="TMT_COLOR", partname="TVP_BRANCH", note="TV_InitThemeMetrics")
add(1, "treeview", "GetThemeColor", "TreeView", 0, 0, prop=3802, propname="TMT_FILLCOLOR")
add(1, "treeview", "GetThemeFont", "TreeView", 1, 1, prop=210, propname="TMT_FONT", partname="TVP_TREEITEM", statename="TREIS_NORMAL")
add(1, "trackbar", "GetThemeColor", "TrackBar", 9, 1, prop=tmt("TMT_COLOR"), propname="TMT_COLOR", partname="TKP_TICS", note="DrawTic")
add(1, "trackbar", "GetThemeColor", "TrackBar", 10, 1, prop=tmt("TMT_COLOR"), propname="TMT_COLOR", partname="TKP_TICSVERT")
for pid in (808, 809):
    add(1, "listview", "GetThemeFont", "ListView", 6, 0, prop=pid, propname="prop%d" % pid, partname="LVP_GROUPHEADER", note="ListView SetMetrics reads 808/809 (TMT_BODYFONT=809)")
add(1, "listview", "GetThemeMargins", "ListView", 1, 3, prop=3602, propname="TMT_CONTENTMARGINS", partname="LVP_LISTITEM", statename="LISS_SELECTED", note="CLVThemesManager OpenTheme")
add(1, "listview", "GetThemeMargins", "ListView", 6, 0, prop=3602, propname="TMT_CONTENTMARGINS", partname="LVP_GROUPHEADER")
add(1, "listview", "GetThemePartSize", "ListView", 7, 1, ts=1, partname="LVP_GROUPHEADERLINE", propname="TS_TRUE", note="group view SetMetrics")
add(1, "listview", "GetThemePartSize", "ListView", 9, 1, ts=2, aux=RECT_STD, partname="LVP_COLLAPSEBUTTON", propname="TS_DRAW")
for sub in ("Explorer", "DarkMode_Explorer"):
    for st in range(0, 7):
        for api, kw in (("GetThemeColor", dict(prop=3801, propname="TMT_BORDERCOLOR")), ("GetThemeColor", dict(prop=3802, propname="TMT_FILLCOLOR")),
                        ("GetThemeEnumValue", dict(prop=tmt("TMT_BGTYPE"), propname="TMT_BGTYPE")), ("IsThemePartDefined", {}),
                        ("GetThemeBackgroundContentRect", dict(aux=RECT_STD))):
            add(1, "listview", api, "ListView", 1, st, subapp=sub, partname="LVP_LISTITEM", note="Explorer-subapp list item look", **kw)
for st in range(1, 7):
    add(1, "listview", "GetThemeTransitionDuration", "ListView", 1, 1, aux="to=%d" % st, prop=6000, partname="LVP_LISTITEM", propname="TMT_TRANSITIONDURATIONS")
add(1, "window", "GetThemeInt", "Window", 1, 1, prop=1201, nc=1, propname="TMT_SIZINGBORDERWIDTH", partname="WP_CAPTION", note="CCDrawNonClientTheme/ThemeDrawEdge (OpenNc)")
for pn_, nm_ in ((1, "WP_CAPTION"), (2, "WP_SMALLCAPTION"), (3, "WP_MINCAPTION"), (5, "WP_MAXCAPTION")):
    for num, nm in ((3603, "TMT_CAPTIONMARGINS"), (3601, "TMT_SIZINGMARGINS"), (3602, "TMT_CONTENTMARGINS")):
        add(1, "window", "GetThemeMargins", "Window", pn_, 1, prop=num, nc=1, propname=nm, partname=nm_)
for pn_, nm_ in ((18, "WP_CLOSEBUTTON"), (19, "WP_SMALLCLOSEBUTTON")):
    for ts_i in range(3):
        add(1, "window", "GetThemePartSize", "Window", pn_, 1, ts=ts_i, nc=1, aux=RECT_STD if ts_i == 2 else "", partname=nm_, propname=TSN[ts_i])
add(1, "button", "GetThemeTextMetrics", "Button", 6, 0, partname="BP_COMMANDLINK")
add(1, "button", "GetThemeBool", "Button", 0, 0, prop=tmt("TMT_COMPOSITED"), propname="TMT_COMPOSITED", note="Button_OnCreate")
for st in range(1, 13):
    add(1, "button", "GetThemePartSize", "Button", 3, st, ts=2, aux=RECT_STD, partname="BP_CHECKBOX", statename="state%d" % st, propname="TS_DRAW", note="GetCheckBoxSize / CreateCheckBoxImagelistEx")
    add(1, "button", "GetThemePartSize", "Button", 2, st, ts=2, aux=RECT_STD, partname="BP_RADIOBUTTON", statename="state%d" % st, propname="TS_DRAW")
for cn in ("Button-OK;Button", "Button-CANCEL;Button"):
    add(2, "button", "GetThemePartSize", cn, 1, 1, ts=1, partname="BP_PUSHBUTTON", propname="TS_TRUE", note="dialog button class list")
add(1, "status", "GetThemePartSize", "Status", 3, 0, ts=2, aux=RECT_STD, partname="SP_GRIPPER", propname="TS_DRAW", note="Status_ComputeGripperSize")
add(1, "tooltip", "GetThemePartSize", "Tooltip", 5, 1, ts=2, aux=RECT_STD, partname="TTP_CLOSE", propname="TS_DRAW")
add(1, "tooltip", "GetThemePartSize", "Tooltip", 7, 1, ts=2, aux=RECT_STD, partname="TTP_WRENCH", propname="TS_DRAW")
for st in (1, 2, 3):
    add(1, "tooltip", "GetThemePartSize", "Tooltip", 6, st, ts=1, partname="TTP_BALLOONSTEM", propname="TS_TRUE")
for pp, nm in ((1, "TTP_STANDARD"), (3, "TTP_BALLOON")):
    add(1, "tooltip", "GetThemeBackgroundRegion", "Tooltip", pp, 1, aux=RECT_STD, partname=nm, note="CreateToolTipRgn / CreateBalloonRgn (region bounding box)")
add(1, "scrollbar", "GetThemeTransitionDuration", "Scrollbar", 1, 1, nc=1, aux="to=2", prop=6000, partname="SBP_ARROWBTN", propname="TMT_TRANSITIONDURATIONS", note="xxxDrawSB2")

# ================================================================ GetThemeSys*  (02 sec 7); ids go in 'prop'
SYS_SM = ((2, "SM_CXVSCROLL"), (3, "SM_CYHSCROLL"), (30, "SM_CXSIZE"), (31, "SM_CYSIZE"), (32, "SM_CXFRAME"), (33, "SM_CYFRAME"),
          (52, "SM_CXSMSIZE"), (53, "SM_CYSMSIZE"), (54, "SM_CXMENUSIZE"), (55, "SM_CYMENUSIZE"), (92, "SM_CXPADDEDBORDER"),
          (0, "SM_CXSCREEN(unsupported id)"), (4, "SM_CYCAPTION(unsupported id)"), (45, "SM_CXEDGE(unsupported id)"), (46, "SM_CYEDGE(unsupported id)"))
for hc, note in (("-", "hTheme NULL (classic fallback)"), ("Window", "hTheme of class Window"), ("Globals", "hTheme of class Globals")):
    pr = 1 if hc != "Globals" else 2
    for i in range(0, 31):
        add(pr, "sys", "GetThemeSysColor", hc, prop=i, propname="COLOR_%d" % i, note=note if i == 0 else "")
    for sm, nm in SYS_SM:
        for d in ([0] if hc == "-" else [0, 96, 120, 144, 192]):
            add(pr, "sys", "GetThemeSysSize", hc, prop=sm, dpi=d, propname=nm, note=note if sm == 2 else "")
    add(pr, "sys", "GetThemeSysBool", hc, prop=tmt("TMT_FLATMENUS"), propname="TMT_FLATMENUS")
    add(pr, "sys", "GetThemeSysBool", hc, prop=1002, propname="(other id)")
    for pid, nm in ((tmt("TMT_MINCOLORDEPTH"), "TMT_MINCOLORDEPTH"), (tmt("TMT_FLATMENUS"), "TMT_FLATMENUS(wrong)"), (1, "(bad id)")):
        add(pr, "sys", "GetThemeSysInt", hc, prop=pid, propname=nm)
    for fid in range(800, 811):
        add(pr, "sys", "GetThemeSysFont", hc, prop=fid, propname="font%d" % fid)
    for sid in range(1400, 1406):
        add(pr, "sys", "GetThemeSysString", hc, prop=sid, propname="string%d" % sid)
    add(pr, "sys", "GetThemeSysColorBrush", hc, prop=5, propname="COLOR_WINDOW", note="brush colour via GetObject; brush deleted when the theme owns it")

# ================================================================ control-level truth
for fnt in ("none", "segoe9", "msshell8"):
    add(1, "ctl", "Ctl", "status", aux="font=%s" % fnt, note="status bar height + borders (GetNewMetrics 0x3a894)")
    add(1, "ctl", "Ctl", "tab", aux="font=%s;items=3" % fnt, note="TCM_GETITEMRECT, TCM_ADJUSTRECT")
    add(1, "ctl", "Ctl", "toolbar", aux="font=%s;items=3" % fnt, note="TB_GETBUTTONSIZE, bar height")
    add(1, "ctl", "Ctl", "combo", aux="font=%s" % fnt, note="CB_GETITEMHEIGHT, rcButton")
    add(1, "ctl", "Ctl", "edit", aux="font=%s" % fnt, note="EM_GETRECT inset (themed edge)")
    for t in ("push", "check", "radio", "group"):
        add(1, "ctl", "Ctl", "button", aux="font=%s;kind=%s" % (fnt, t), note="BCM_GETIDEALSIZE")
for stl, nm in (("0x0", "TBS_HORZ"), ("0x2", "TBS_VERT"), ("0x10", "TBS_NOTICKS")):
    add(1, "ctl", "Ctl", "trackbar", aux="style=" + stl, note=nm + " thumb/channel")
add(1, "ctl", "Ctl", "updown", aux="font=segoe9", note="spin width")
add(2, "ctl", "Ctl", "listbox", aux="font=segoe9", note="item height")

# ================================================================ heavy discovery (only with --sweep full)
for cls in CAT:
    add(3, "gridfull", "PropGrid", cls, nc=1 if cls == "Scrollbar" else 0, aux="pmax=40;smax=20;heavy=1", note="PropSweep over every defined (part,state): large output")

# priority 0 = the CC30 header-height set (quick first run: probe_record --prio 0)
for r in ROWS:
    if r["grp"] == "header" and (r["note"].startswith("CC30") or r["note"].startswith("_Header_InitTheme") or (r["api"] == "Ctl" and r["aux"].find("items=3") >= 0 and r["aux"].find("0x100") < 0)
                                or (r["api"] == "GetThemeTextExtent" and r["aux"].startswith("text=M;"))):
        r["prio"] = 0
    if r["grp"] == "env":
        r["prio"] = 0

def write(path):
    n = 0
    with open(path, "w", newline="\n") as f:
        f.write(",".join(COLS) + "\n")
        for r in ROWS:
            n += 1
            r["id"] = "P%05d" % n
            f.write(",".join(str(r[c]) for c in COLS) + "\n")
    return n

def groups_md(path):
    by = collections.OrderedDict()
    for r in ROWS:
        g = by.setdefault(r["grp"], collections.Counter())
        g[r["prio"]] += 1
        g["n"] += 1
    apis = collections.Counter(r["api"] for r in ROWS)
    with open(path, "w") as f:
        f.write("| group | rows | prio0 | prio1 | prio2 | prio3 |\n|---|---|---|---|---|---|\n")
        for g, c in by.items():
            f.write("| %s | %d | %d | %d | %d | %d |\n" % (g, c["n"], c[0], c[1], c[2], c[3]))
        f.write("| **total** | %d | %d | %d | %d | %d |\n\n" % (len(ROWS), sum(1 for r in ROWS if r["prio"] == 0), sum(1 for r in ROWS if r["prio"] == 1), sum(1 for r in ROWS if r["prio"] == 2), sum(1 for r in ROWS if r["prio"] == 3)))
        f.write("| api | rows |\n|---|---|\n")
        for a, c in sorted(apis.items(), key=lambda x: (-x[1], x[0])):
            f.write("| %s | %d |\n" % (a, c))

def catalog_md(path):
    with open(path, "w") as f:
        f.write("| class | part (id) | states probed (id) | tier | measurement set |\n|---|---|---|---|---|\n")
        for cls, parts in CAT.items():
            for pn_, num, sts in parts:
                st = parse_states(sts)
                tier = "P0/P1 full" if cls == "Header" else ("P1 lean" if is_must(cls, num) else "P2 min")
                f.write("| %s | %s (%d) | %s | %s | %s |\n" % (cls, pn_, num, " ".join("%s(%d)" % x for x in st) or "-", tier,
                        "all" if cls == "Header" else ("sizes+margins+props" if is_must(cls, num) else "TS_TRUE+defined+content margins")))

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument("--out", default=os.path.join(here, "probe_table.csv"))
    ap.add_argument("--md", default=os.path.join(here, "probe_groups.md"))
    a = ap.parse_args()
    if BAD:
        print("\n".join(sorted(set(BAD)))); sys.exit(1)
    n = write(a.out)
    groups_md(a.md)
    catalog_md(os.path.join(os.path.dirname(a.md), "probe_catalog.md"))
    print("wrote %d rows to %s" % (n, a.out))
    if UNVERIFIED:
        print("names not in SDK headers (kept, flagged):", " ".join(sorted(UNVERIFIED)))
