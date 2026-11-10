/* probe_record.c - records Windows theme (uxtheme / comctl32 v6) answers, driven by tools/probe_table.csv.
 *
 *   probe_record.exe [--table probe_table.csv] [--out results.csv] [--label NAME] [--prio N]
 *                    [--grp a,b] [--sweep std|full|none] [--comctl v6|v5] [--dpi-aware system|pm|pmv2|unaware]
 *                    [--dark off|allow|force|light] [--app-props N] [--timeout SEC] [--show] [--list]
 *                    [--no-supervise] [--no-nc]   (internal: --child --from N --append)
 *
 * No judgment is needed: every row of the table is executed and one row of results.csv is written per probe
 * (sweeps write several).  A crash or hang of a row is logged (hr=CRASH / HANG) and the run continues with
 * the next row because the exe re-launches itself as a child (supervisor mode).
 *
 * uxtheme.dll and comctl32.dll are loaded dynamically (LoadLibrary/GetProcAddress); nothing is linked to them.
 * Plain C, mingw-w64.  results.csv columns:
 *     id,sub,hr,lasterr,ret,v0,v1,v2,v3,v4,v5,txt
 *  id    = table id (P00001 ...) or RUN / ENV.* ; sub = sub-measurement (sweeps, env items)
 *  hr    = 0xHHHHHHHH HRESULT, or "-" (API returns no HRESULT), OPENFAIL (the class did not open), NOAPI, CRASH, HANG, NA
 *  lasterr = GetLastError() right after the call ("0x%08X")
 *  ret   = return value of non-HRESULT APIs (BOOL/int/COLORREF; nonnull flag for HTHEME) ; v0..v5 = output values
 *  txt   = strings (never contains a comma).  Unwritten outputs keep the sentinel -7777.
 * Semantics of every api / column of the table: docs/research/06_probe_list.md.
 */
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define SENT (-7777)

/* ---------------------------------------------------------------- dynamic API table */
typedef HTHEME  (WINAPI *fOpen)(HWND, LPCWSTR);
typedef HTHEME  (WINAPI *fOpenDpi)(HWND, LPCWSTR, UINT);
typedef HTHEME  (WINAPI *fOpenEx)(HWND, LPCWSTR, DWORD);
typedef HRESULT (WINAPI *fClose)(HTHEME);
typedef HRESULT (WINAPI *fDrawBg)(HTHEME, HDC, int, int, LPCRECT, LPCRECT);
typedef HRESULT (WINAPI *fPartSize)(HTHEME, HDC, int, int, LPCRECT, int, SIZE *);
typedef HRESULT (WINAPI *fInt)(HTHEME, int, int, int, int *);
typedef HRESULT (WINAPI *fMetric)(HTHEME, HDC, int, int, int, int *);
typedef HRESULT (WINAPI *fBool)(HTHEME, int, int, int, BOOL *);
typedef HRESULT (WINAPI *fColor)(HTHEME, int, int, int, COLORREF *);
typedef HRESULT (WINAPI *fMargins)(HTHEME, HDC, int, int, int, LPCRECT, MARGINS *);
typedef HRESULT (WINAPI *fPos)(HTHEME, int, int, int, POINT *);
typedef HRESULT (WINAPI *fRect)(HTHEME, int, int, int, RECT *);
typedef HRESULT (WINAPI *fFont)(HTHEME, HDC, int, int, int, LOGFONTW *);
typedef HRESULT (WINAPI *fStr)(HTHEME, int, int, int, LPWSTR, int);
typedef HRESULT (WINAPI *fIntList)(HTHEME, int, int, int, INTLIST *);
typedef HRESULT (WINAPI *fOrigin)(HTHEME, int, int, int, int *);
typedef HRESULT (WINAPI *fRectOut)(HTHEME, HDC, int, int, LPCRECT, RECT *);
typedef HRESULT (WINAPI *fTextExt)(HTHEME, HDC, int, int, LPCWSTR, int, DWORD, LPCRECT, RECT *);
typedef HRESULT (WINAPI *fTextMet)(HTHEME, HDC, int, int, TEXTMETRICW *);
typedef BOOL    (WINAPI *fDefined)(HTHEME, int, int);
typedef BOOL    (WINAPI *fTransp)(HTHEME, int, int);
typedef HRESULT (WINAPI *fRegion)(HTHEME, HDC, int, int, LPCRECT, HRGN *);
typedef HRESULT (WINAPI *fTrans)(HTHEME, int, int, int, int, DWORD *);
typedef COLORREF (WINAPI *fSysColor)(HTHEME, int);
typedef HBRUSH  (WINAPI *fSysBrush)(HTHEME, int);
typedef int     (WINAPI *fSysSize)(HTHEME, int);
typedef BOOL    (WINAPI *fSysBool)(HTHEME, int);
typedef HRESULT (WINAPI *fSysInt)(HTHEME, int, int *);
typedef HRESULT (WINAPI *fSysFont)(HTHEME, int, LOGFONTW *);
typedef HRESULT (WINAPI *fSysStr)(HTHEME, int, LPWSTR, int);
typedef BOOL    (WINAPI *fBOOL0)(void);
typedef DWORD   (WINAPI *fDW0)(void);
typedef HRESULT (WINAPI *fCurName)(LPWSTR, int, LPWSTR, int, LPWSTR, int);
typedef HRESULT (WINAPI *fSetWT)(HWND, LPCWSTR, LPCWSTR);
typedef HTHEME  (WINAPI *fGetWT)(HWND);
typedef void    (WINAPI *fSetProps)(DWORD);
typedef BOOL    (WINAPI *fDlgTex)(HWND);
typedef int     (WINAPI *fOrd1)(int);
typedef BOOL    (WINAPI *fInitCC)(const INITCOMMONCONTROLSEX *);
typedef struct { DWORD cbSize, dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformID; } DLLVER;
typedef HRESULT (WINAPI *fDllGetVersion)(DLLVER *);

static struct {
    fDrawBg DrawBg; fOpen Open; fOpenDpi OpenDpi; fOpenEx OpenEx; fOpen OpenNc; fClose Close;
    fPartSize PartSize; fInt Int; fMetric Metric; fBool Bool; fInt Enum; fColor Color; fMargins Margins; fPos Pos; fRect Rect;
    fFont Font; fStr Str; fStr File; fIntList IntList; fOrigin Origin; fRectOut Content; fRectOut Extent; fTextExt TextExt; fTextMet TextMet;
    fDefined Defined; fTransp Transp; fRegion Region; fTrans Trans;
    fSysColor SysColor; fSysBrush SysBrush; fSysSize SysSize; fSysBool SysBool; fSysInt SysInt; fSysFont SysFont; fSysStr SysStr;
    fBOOL0 IsActive, IsAppThemed, IsComp; fDW0 GetProps; fCurName CurName; fSetWT SetWT; fGetWT GetWT; fSetProps SetProps; fDlgTex DlgTex;
    fDW0 QueryServices; fOrd1 SetPreferredMode; BOOL (WINAPI *AllowDarkWin)(HWND, BOOL);
    fInitCC InitCC; fDllGetVersion CcVersion;
} X;

/* ---------------------------------------------------------------- options / state */
static struct {
    char table[MAX_PATH], out[MAX_PATH], label[64], grp[256], comctl[8], dpiaw[16], dark[16], sweep[8];
    int prio, appprops, timeout, show, list, child, from, append, nosup, nonc, to;
} O = {"", "results.csv", "unlabelled", "", "v6", "system", "off", "std", 2, -1, 120, 0, 0, 0, 0, 0, 0, 0, -1};

static HMODULE hUx, hCc;
static HWND g_hwnd;
static HDC g_dc, g_mdc;
static FILE *g_out;
static char g_id[32] = "RUN";
static int g_idx;
static volatile DWORD g_rowstart;
static volatile int g_active;
static char g_crashfile[MAX_PATH];
static int g_cur_nc;
static char g_sub[64];       /* sub-app currently set on g_hwnd */
static ULONG_PTR g_cookie;
static HANDLE g_actctx = INVALID_HANDLE_VALUE;

/* ---------------------------------------------------------------- helpers */
static const wchar_t *W(const char *s)
{
    static wchar_t b[8][512]; static int i; wchar_t *o = b[i = (i + 1) & 7];
    MultiByteToWideChar(CP_UTF8, 0, s, -1, o, 512); return o;
}
static void clean(char *s)
{
    for (; *s; s++) if (*s == ',' || *s == '"' || (unsigned char)*s < 32 || (unsigned char)*s > 126) *s = (*s == ',') ? ';' : '?';
}
static const char *A(const wchar_t *w)
{
    static char b[8][1024]; static int i; char *o = b[i = (i + 1) & 7];
    if (!WideCharToMultiByte(CP_ACP, 0, w, -1, o, 1024, NULL, NULL)) o[0] = 0;
    clean(o); return o;
}
static const char *HR(HRESULT h) { static char b[8][16]; static int i; i = (i + 1) & 7; sprintf(b[i], "0x%08lX", (unsigned long)h); return b[i]; }

static int aux_find(const char *aux, const char *key, char *val, int n)
{
    size_t kl = strlen(key); const char *p = aux;
    while (p && *p) {
        const char *e = strchr(p, ';'); size_t l = e ? (size_t)(e - p) : strlen(p);
        if (l >= kl && !strncmp(p, key, kl) && (l == kl || p[kl] == '=')) {
            if (val) { size_t vl = (l > kl) ? l - kl - 1 : 0; if (vl >= (size_t)n) vl = n - 1; memcpy(val, p + kl + 1, vl); val[vl] = 0; }
            return 1;
        }
        p = e ? e + 1 : NULL;
    }
    return 0;
}
static long aux_int(const char *aux, const char *key, long def)
{
    char v[64]; if (!aux_find(aux, key, v, sizeof v)) return def; return strtol(v, NULL, 0);
}
static int aux_rect(const char *aux, RECT *r)
{
    char v[64]; if (!aux_find(aux, "rect", v, sizeof v)) return 0;
    return sscanf(v, "%ld:%ld:%ld:%ld", &r->left, &r->top, &r->right, &r->bottom) == 4;
}

static void emit(const char *sub, const char *hr, DWORD le, long ret, int nv, const long *v, const char *txt)
{
    char t[1200]; int i;
    snprintf(t, sizeof t, "%s", txt ? txt : ""); clean(t);
    fprintf(g_out, "%s,%s,%s,0x%08lX,%ld", g_id, sub ? sub : "", hr, (unsigned long)le, ret);
    for (i = 0; i < 6; i++) { if (i < nv) fprintf(g_out, ",%ld", v[i]); else fprintf(g_out, ","); }
    fprintf(g_out, ",%s\n", t);
}
static void em(const char *sub, const char *hr, DWORD le, long ret, const char *fmt, ...)
{   /* text-only row */
    char b[900]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    emit(sub, hr, le, ret, 0, NULL, b);
}
static void emit_hr(const char *sub, HRESULT hr, DWORD le, int nv, long a, long b, long c, long d, const char *txt)
{ long v[4] = {a, b, c, d}; emit(sub, HR(hr), le, 0, nv, v, txt); }

/* ---------------------------------------------------------------- fonts */
static HFONT mkfont(const char *name)
{
    static struct { char n[24]; HFONT f; } cache[12]; static int nc; int i;
    int dpi = GetDeviceCaps(g_dc, LOGPIXELSY);
    if (!name || !*name || !strcmp(name, "none")) return NULL;
    for (i = 0; i < nc; i++) if (!strcmp(cache[i].n, name)) return cache[i].f;
    HFONT f = NULL;
    if (!strcmp(name, "gui")) f = GetStockObject(DEFAULT_GUI_FONT);
    else if (!strcmp(name, "sys")) f = GetStockObject(SYSTEM_FONT);
    else if (!strcmp(name, "segoe9")) f = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    else if (!strcmp(name, "msshell8")) f = CreateFontW(-MulDiv(8, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"MS Shell Dlg");
    else if (!strcmp(name, "lg")) f = CreateFontW(-MulDiv(18, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    else if (!strcmp(name, "msg")) { NONCLIENTMETRICSW n; n.cbSize = sizeof n; SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof n, &n, 0); f = CreateFontIndirectW(&n.lfMessageFont); }
    if (nc < 12) { snprintf(cache[nc].n, sizeof cache[nc].n, "%s", name); cache[nc++].f = f; }
    return f;
}
/* dc for text-affecting calls: memory DC with the row's font (default: stock SYSTEM_FONT) */
static HDC textdc(const char *aux)
{
    char fn[32] = ""; HFONT f;
    aux_find(aux, "font", fn, sizeof fn);
    f = mkfont(fn);
    SelectObject(g_mdc, f ? f : GetStockObject(SYSTEM_FONT));
    return g_mdc;
}
static HDC pickdc(const char *aux, int def)   /* hdc=0 NULL, 1 screen, m memory; def used when absent */
{
    char v[8]; int m = def;
    if (aux_find(aux, "hdc", v, sizeof v)) m = (v[0] == 'm') ? 2 : atoi(v);
    return m == 0 ? NULL : (m == 2 ? textdc(aux) : g_dc);
}

/* ---------------------------------------------------------------- theme handles */
typedef struct { char key[320]; HTHEME h; DWORD le; } TC;
static TC tc[4096]; static int ntc;

static void set_sub(const char *sub)
{
    if (!strcmp(g_sub, sub)) return;
    if (!sub[0]) X.SetWT(g_hwnd, NULL, NULL);
    else if (!strcmp(sub, "(empty)")) X.SetWT(g_hwnd, L"", NULL);
    else X.SetWT(g_hwnd, W(sub), NULL);
    snprintf(g_sub, sizeof g_sub, "%s", sub);
}
static LPCWSTR clsw(const char *c)
{
    if (!strcmp(c, "@NULL")) return NULL;
    if (!strcmp(c, "@EMPTY")) return L"";
    return W(c);
}
static HTHEME do_open(const char *cls, HWND hw, int dpi, int nc, DWORD flags, int useex, DWORD *le)
{
    HTHEME h;
    SetLastError(0x5A5A5A5A);
    if (useex && X.OpenEx) h = X.OpenEx(hw, clsw(cls), flags);
    else if (nc && X.OpenNc) h = X.OpenNc(hw, clsw(cls));
    else if (dpi && X.OpenDpi) h = X.OpenDpi(hw, clsw(cls), dpi);
    else h = X.Open(hw, clsw(cls));
    *le = GetLastError();
    return h;
}
static HTHEME open_cached(const char *cls, int dpi, const char *sub, int nc, int usehw, DWORD *le)
{
    char key[320]; int i;
    snprintf(key, sizeof key, "%s|%d|%s|%d|%d", cls, dpi, sub, nc, usehw);
    for (i = 0; i < ntc; i++) if (!strcmp(tc[i].key, key)) { *le = tc[i].le; return tc[i].h; }
    set_sub(sub);
    HTHEME h = do_open(cls, usehw ? g_hwnd : NULL, dpi, nc, 0, 0, le);
    if (ntc < 4096) { snprintf(tc[ntc].key, sizeof tc[ntc].key, "%s", key); tc[ntc].h = h; tc[ntc].le = *le; ntc++; }
    return h;
}

/* ---------------------------------------------------------------- row context */
typedef struct {
    const char *api, *cls, *sub, *aux; int part, state, prop, ts, dpi, nc, usehw;
    HTHEME h; const char *grp;
} Ctx;

/* returns 1 if a handle (possibly NULL/forged on purpose) is available */
static int get_handle(Ctx *c)
{
    DWORD le;
    if (!strcmp(c->cls, "-") || !strcmp(c->cls, "@NULLH")) { c->h = NULL; return 1; }
    if (!strcmp(c->cls, "@BOGUS")) { c->h = (HTHEME)(ULONG_PTR)0x7fff0001; return 1; }
    if (!strcmp(c->cls, "@STALE")) {
        c->h = X.OpenDpi ? X.OpenDpi(g_hwnd, L"Button", 97) : X.Open(g_hwnd, L"Button");
        if (c->h) X.Close(c->h);
        return 1;
    }
    c->h = open_cached(c->cls, c->dpi, c->sub, c->nc, c->usehw, &le);
    if (!c->h) { long v[1] = {0}; emit("", "OPENFAIL", le, 0, 0, v, ""); return 0; }
    return 1;
}

/* ---------------------------------------------------------------- handlers: theme getters */
#define NOOUT(c) ((int)aux_int((c)->aux, "noout", 0))
static void h_PartSize(Ctx *c)
{
    SIZE s = {SENT, SENT}; RECT rc; HRESULT hr; HDC dc = pickdc(c->aux, 0);
    int hr_rect = aux_rect(c->aux, &rc);
    SetLastError(0x5A5A5A5A);
    hr = X.PartSize(c->h, dc, c->part, c->state, hr_rect ? &rc : NULL, c->ts, NOOUT(c) ? NULL : &s);
    emit_hr("", hr, GetLastError(), 2, s.cx, s.cy, 0, 0, "");
}
static void h_Int(Ctx *c)
{
    int v = SENT; HRESULT hr; SetLastError(0x5A5A5A5A);
    hr = X.Int(c->h, c->part, c->state, c->prop, NOOUT(c) ? NULL : &v); emit_hr("", hr, GetLastError(), 1, v, 0, 0, 0, "");
}
static void h_Enum(Ctx *c)
{
    int v = SENT; HRESULT hr; SetLastError(0x5A5A5A5A);
    hr = X.Enum(c->h, c->part, c->state, c->prop, NOOUT(c) ? NULL : &v); emit_hr("", hr, GetLastError(), 1, v, 0, 0, 0, "");
}
static void h_Metric(Ctx *c)
{
    int v = SENT; HRESULT hr; SetLastError(0x5A5A5A5A);
    hr = X.Metric(c->h, pickdc(c->aux, 1), c->part, c->state, c->prop, NOOUT(c) ? NULL : &v); emit_hr("", hr, GetLastError(), 1, v, 0, 0, 0, "");
}
static void h_Bool(Ctx *c)
{
    BOOL v = SENT; HRESULT hr; SetLastError(0x5A5A5A5A);
    hr = X.Bool(c->h, c->part, c->state, c->prop, NOOUT(c) ? NULL : &v); emit_hr("", hr, GetLastError(), 1, v, 0, 0, 0, "");
}
static void h_Color(Ctx *c)
{
    COLORREF v = 0xFFFFFFFF; HRESULT hr; char t[24]; SetLastError(0x5A5A5A5A);
    hr = X.Color(c->h, c->part, c->state, c->prop, NOOUT(c) ? NULL : &v);
    sprintf(t, "0x%06lX", (unsigned long)(v & 0xFFFFFF));  /* COLORREF 0x00BBGGRR */
    emit_hr("", hr, GetLastError(), 1, (long)v, 0, 0, 0, SUCCEEDED(hr) ? t : "");
}
static void h_Margins(Ctx *c)
{
    MARGINS m = {SENT, SENT, SENT, SENT}; RECT rc; int hr_rect = aux_rect(c->aux, &rc); HRESULT hr;
    SetLastError(0x5A5A5A5A);
    hr = X.Margins(c->h, pickdc(c->aux, 1), c->part, c->state, c->prop, hr_rect ? &rc : NULL, NOOUT(c) ? NULL : &m);
    emit_hr("", hr, GetLastError(), 4, m.cxLeftWidth, m.cxRightWidth, m.cyTopHeight, m.cyBottomHeight, "");  /* l r t b */
}
static void h_Position(Ctx *c)
{
    POINT p = {SENT, SENT}; HRESULT hr; SetLastError(0x5A5A5A5A);
    hr = X.Pos(c->h, c->part, c->state, c->prop, NOOUT(c) ? NULL : &p); emit_hr("", hr, GetLastError(), 2, p.x, p.y, 0, 0, "");
}
static void h_Rect(Ctx *c)
{
    RECT r = {SENT, SENT, SENT, SENT}; HRESULT hr; SetLastError(0x5A5A5A5A);
    hr = X.Rect(c->h, c->part, c->state, c->prop, NOOUT(c) ? NULL : &r); emit_hr("", hr, GetLastError(), 4, r.left, r.top, r.right, r.bottom, "");
}
static void lf_emit(const char *sub, HRESULT hr, DWORD le, const LOGFONTW *lf)
{
    long v[6] = {lf->lfHeight, lf->lfWeight, lf->lfItalic, lf->lfCharSet, lf->lfQuality, lf->lfPitchAndFamily};
    emit(sub, HR(hr), le, 0, 6, v, A(lf->lfFaceName));
}
static void h_Font(Ctx *c)
{
    LOGFONTW lf; HRESULT hr; memset(&lf, 0, sizeof lf); lf.lfHeight = SENT; SetLastError(0x5A5A5A5A);
    hr = X.Font(c->h, pickdc(c->aux, 1), c->part, c->state, c->prop, NOOUT(c) ? NULL : &lf); lf_emit("", hr, GetLastError(), &lf);
}
static void h_String(Ctx *c)
{
    WCHAR b[260]; HRESULT hr; b[0] = 0; SetLastError(0x5A5A5A5A);
    hr = (!strcmp(c->api, "GetThemeFilename") ? X.File : X.Str)(c->h, c->part, c->state, c->prop, NOOUT(c) ? NULL : b, 260);
    emit_hr("", hr, GetLastError(), 0, 0, 0, 0, 0, A(b));
}
static void h_IntList(Ctx *c)
{
    INTLIST il; HRESULT hr; char t[200] = ""; int i; memset(&il, 0, sizeof il); il.iValueCount = SENT; SetLastError(0x5A5A5A5A);
    hr = X.IntList(c->h, c->part, c->state, c->prop, NOOUT(c) ? NULL : &il);
    if (SUCCEEDED(hr)) for (i = 0; i < il.iValueCount && i < 24; i++) sprintf(t + strlen(t), "%s%d", i ? " " : "", il.iValues[i]);
    emit_hr("", hr, GetLastError(), 1, il.iValueCount, 0, 0, 0, t);
}
static void h_Origin(Ctx *c)
{
    int o = SENT; HRESULT hr; SetLastError(0x5A5A5A5A);
    hr = X.Origin(c->h, c->part, c->state, c->prop, NOOUT(c) ? NULL : &o);
    emit_hr("", hr, GetLastError(), 1, o, 0, 0, 0, o == 0 ? "PO_STATE" : o == 1 ? "PO_PART" : o == 2 ? "PO_CLASS" : o == 3 ? "PO_GLOBAL" : o == 4 ? "PO_NOTFOUND" : "");
}
static void h_Content(Ctx *c)   /* ContentRect / Extent */
{
    RECT in = {0, 0, 100, 30}, out = {SENT, SENT, SENT, SENT}; HRESULT hr; aux_rect(c->aux, &in); SetLastError(0x5A5A5A5A);
    hr = (!strcmp(c->api, "GetThemeBackgroundExtent") ? X.Extent : X.Content)(c->h, pickdc(c->aux, 1), c->part, c->state, &in, NOOUT(c) ? NULL : &out);
    emit_hr("", hr, GetLastError(), 4, out.left, out.top, out.right, out.bottom, "");
}

/* DrawThemeBackground into a small top-down 32bpp DIB prefilled with 0x00FF00FF (aux size=w:h, optional dump=1 -> raw BGRA hex
 * of the whole bitmap in txt, only for w*h <= 100).  v0 = FNV-1a of the greyscale ((299R+587G+114B)/1000) bytes,
 * v1 = pixels differing from the fill, v2 = pixels with alpha != 0, v3 = centre pixel 0xAARRGGBB (as signed long),
 * v4 = top-left pixel, v5 = bottom-right pixel.  txt = 'grey=<hex of up to 32 evenly spaced greyscale samples>[ raw=<hex>]' */
static void h_Draw(Ctx *c)
{
    int w = 32, h = 24, dump = (int)aux_int(c->aux, "dump", 0), x, y, n; char sz[64]; BITMAPINFO bi; void *bits = NULL; HBITMAP bm, old; HRESULT hr;
    unsigned long hash = 2166136261u; long changed = 0, alpha = 0, v[6]; DWORD *px; RECT rc; char t[1200]; size_t o = 0;
    if (aux_find(c->aux, "size", sz, sizeof sz)) sscanf(sz, "%d:%d", &w, &h);
    memset(&bi, 0, sizeof bi); bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    bm = CreateDIBSection(g_dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bm) { em("", "NODIB", GetLastError(), 0, "CreateDIBSection"); return; }
    px = bits; for (n = 0; n < w * h; n++) px[n] = 0x00FF00FF;
    old = SelectObject(g_mdc, bm); SetRect(&rc, 0, 0, w, h); SetLastError(0x5A5A5A5A);
    hr = X.DrawBg(c->h, g_mdc, c->part, c->state, &rc, NULL); { DWORD le = GetLastError(); GdiFlush();
    for (n = 0; n < w * h; n++) {
        DWORD p = px[n]; unsigned r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255; unsigned gr = (299 * r + 587 * g + 114 * b) / 1000;
        hash = (hash ^ gr) * 16777619u; if (p != 0x00FF00FF) changed++; if (p >> 24) alpha++;
    }
    v[0] = (long)hash; v[1] = changed; v[2] = alpha; v[3] = (long)px[(h / 2) * w + w / 2]; v[4] = (long)px[0]; v[5] = (long)px[w * h - 1];
    o += snprintf(t + o, sizeof t - o, "grey=");
    for (n = 0; n < 32; n++) { DWORD p = px[(n * (w * h - 1)) / 31]; unsigned gr = (299 * ((p >> 16) & 255) + 587 * ((p >> 8) & 255) + 114 * (p & 255)) / 1000; o += snprintf(t + o, sizeof t - o, "%02x", gr); }
    if (dump && w * h <= 100) { o += snprintf(t + o, sizeof t - o, " raw="); for (n = 0; n < w * h; n++) o += snprintf(t + o, sizeof t - o, "%08lx", (unsigned long)px[n]); }
    (void)x; (void)y; emit("", HR(hr), le, 0, 6, v, t); }
    SelectObject(g_mdc, old); DeleteObject(bm);
}
static void h_TextExtent(Ctx *c)
{
    char t[128] = "M", fl[32]; DWORD flags = 0; RECT b, ext = {SENT, SENT, SENT, SENT}, *pb = NULL; HRESULT hr; HDC dc = textdc(c->aux);
    aux_find(c->aux, "text", t, sizeof t); if (aux_find(c->aux, "flags", fl, sizeof fl)) flags = strtoul(fl, NULL, 0);
    if (aux_rect(c->aux, &b)) pb = &b;
    SetLastError(0x5A5A5A5A);
    hr = X.TextExt(c->h, dc, c->part, c->state, W(t), -1, flags, pb, NOOUT(c) ? NULL : &ext);
    emit_hr("", hr, GetLastError(), 4, ext.left, ext.top, ext.right, ext.bottom, "");
}
static void h_TextMetrics(Ctx *c)
{
    TEXTMETRICW tm; HRESULT hr; HDC dc = textdc(c->aux); memset(&tm, 0, sizeof tm); tm.tmHeight = SENT; SetLastError(0x5A5A5A5A);
    hr = X.TextMet(c->h, dc, c->part, c->state, NOOUT(c) ? NULL : &tm);
    long v[6] = {tm.tmHeight, tm.tmAscent, tm.tmDescent, tm.tmInternalLeading, tm.tmExternalLeading, tm.tmAveCharWidth};
    emit("", HR(hr), GetLastError(), 0, 6, v, "tmHeight tmAscent tmDescent tmInternalLeading tmExternalLeading tmAveCharWidth");
}
static void h_Defined(Ctx *c)
{
    BOOL r; SetLastError(0x5A5A5A5A); r = !strcmp(c->api, "IsThemePartDefined") ? X.Defined(c->h, c->part, c->state) : X.Transp(c->h, c->part, c->state);
    emit("", "-", GetLastError(), r, 0, NULL, "");
}
static void h_Region(Ctx *c)
{
    RECT in = {0, 0, 100, 30}, bb = {SENT, SENT, SENT, SENT}; HRGN rg = NULL; HRESULT hr; aux_rect(c->aux, &in); SetLastError(0x5A5A5A5A);
    hr = X.Region(c->h, pickdc(c->aux, 1), c->part, c->state, &in, NOOUT(c) ? NULL : &rg);
    if (rg) { GetRgnBox(rg, &bb); DeleteObject(rg); }
    emit_hr("", hr, GetLastError(), 4, bb.left, bb.top, bb.right, bb.bottom, "");
}
static void h_Transition(Ctx *c)
{
    DWORD d = SENT; HRESULT hr; int to = (int)aux_int(c->aux, "to", 1); SetLastError(0x5A5A5A5A);
    hr = X.Trans(c->h, c->part, c->state, to, c->prop, NOOUT(c) ? NULL : &d); emit_hr("", hr, GetLastError(), 1, (long)d, 0, 0, 0, "");
}


/* ---------------------------------------------------------------- GetThemeSys* (prop = id; class "-" = NULL theme) */
static void h_Sys(Ctx *c)
{
    const char *a = c->api; int id = c->prop; DWORD le; long v[4]; WCHAR b[260]; LOGFONTW lf; int iv = SENT;
    SetLastError(0x5A5A5A5A);
    if (!strcmp(a, "GetThemeSysColor")) { COLORREF r = X.SysColor(c->h, id); char t[24]; le = GetLastError(); sprintf(t, "0x%06lX", (unsigned long)(r & 0xFFFFFF)); emit("", "-", le, (long)r, 0, NULL, t); }
    else if (!strcmp(a, "GetThemeSysSize")) {
        int r = X.SysSize(c->h, id); le = GetLastError(); emit("", "-", le, r, 0, NULL, "");
    }
    else if (!strcmp(a, "GetThemeSysBool")) { BOOL r = X.SysBool(c->h, id); le = GetLastError(); emit("", "-", le, r, 0, NULL, ""); }
    else if (!strcmp(a, "GetThemeSysFont")) { memset(&lf, 0, sizeof lf); lf.lfHeight = SENT; HRESULT hr = X.SysFont(c->h, id, &lf); le = GetLastError(); lf_emit("", hr, le, &lf); }
    else if (!strcmp(a, "GetThemeSysString")) { b[0] = 0; HRESULT hr = X.SysStr(c->h, id, b, 260); le = GetLastError(); emit_hr("", hr, le, 0, 0, 0, 0, 0, A(b)); }
    else if (!strcmp(a, "GetThemeSysColorBrush")) {
        HBRUSH br = X.SysBrush(c->h, id); LOGBRUSH lb; char t[24] = ""; le = GetLastError(); memset(&lb, 0, sizeof lb);
        if (br) { GetObjectW(br, sizeof lb, &lb); sprintf(t, "0x%06lX", (unsigned long)(lb.lbColor & 0xFFFFFF)); if (c->h) DeleteObject(br); }
        emit("", "-", le, br != NULL, 0, NULL, t);
    }
}

/* ---------------------------------------------------------------- sweeps */
static void sweep(Ctx *c, int part, int state, const char *pre)
{
    int id; char sub[96]; HDC dc = g_dc;
    for (id = 1; id <= 8100; id++) {
        int o = SENT; HRESULT hr = X.Origin(c->h, part, state, id, &o);
        if (FAILED(hr) || o == 4) continue;
        sprintf(sub, "%s%d:origin", pre, id); emit(sub, HR(hr), 0, 0, 1, (long[]){o}, o == 0 ? "PO_STATE" : o == 1 ? "PO_PART" : o == 2 ? "PO_CLASS" : "PO_GLOBAL");
        { int v = SENT; if (SUCCEEDED(hr = X.Int(c->h, part, state, id, &v))) { sprintf(sub, "%s%d:Int", pre, id); emit(sub, HR(hr), 0, 0, 1, (long[]){v}, ""); } }
        { BOOL v = SENT; if (SUCCEEDED(hr = X.Bool(c->h, part, state, id, &v))) { sprintf(sub, "%s%d:Bool", pre, id); emit(sub, HR(hr), 0, 0, 1, (long[]){v}, ""); } }
        { int v = SENT; if (SUCCEEDED(hr = X.Enum(c->h, part, state, id, &v))) { sprintf(sub, "%s%d:Enum", pre, id); emit(sub, HR(hr), 0, 0, 1, (long[]){v}, ""); } }
        { COLORREF v = 0; if (SUCCEEDED(hr = X.Color(c->h, part, state, id, &v))) { char t[16]; sprintf(sub, "%s%d:Color", pre, id); sprintf(t, "0x%06lX", (unsigned long)(v & 0xFFFFFF)); emit(sub, HR(hr), 0, (long)v, 0, NULL, t); } }
        { int v = SENT; if (SUCCEEDED(hr = X.Metric(c->h, dc, part, state, id, &v))) { sprintf(sub, "%s%d:Metric", pre, id); emit(sub, HR(hr), 0, 0, 1, (long[]){v}, ""); } }
        { MARGINS m = {SENT, SENT, SENT, SENT}; if (SUCCEEDED(hr = X.Margins(c->h, dc, part, state, id, NULL, &m))) { sprintf(sub, "%s%d:Margins", pre, id); emit(sub, HR(hr), 0, 0, 4, (long[]){m.cxLeftWidth, m.cxRightWidth, m.cyTopHeight, m.cyBottomHeight}, "lrtb"); } }
        { POINT p = {SENT, SENT}; if (SUCCEEDED(hr = X.Pos(c->h, part, state, id, &p))) { sprintf(sub, "%s%d:Position", pre, id); emit(sub, HR(hr), 0, 0, 2, (long[]){p.x, p.y}, "xy"); } }
        { RECT r = {SENT, SENT, SENT, SENT}; if (SUCCEEDED(hr = X.Rect(c->h, part, state, id, &r))) { sprintf(sub, "%s%d:Rect", pre, id); emit(sub, HR(hr), 0, 0, 4, (long[]){r.left, r.top, r.right, r.bottom}, "ltrb"); } }
        { WCHAR b[260]; b[0] = 0; if (SUCCEEDED(hr = X.Str(c->h, part, state, id, b, 260))) { sprintf(sub, "%s%d:String", pre, id); emit(sub, HR(hr), 0, 0, 0, NULL, A(b)); } }
        { WCHAR b[260]; b[0] = 0; if (SUCCEEDED(hr = X.File(c->h, part, state, id, b, 260))) { sprintf(sub, "%s%d:Filename", pre, id); emit(sub, HR(hr), 0, 0, 0, NULL, A(b)); } }
        { LOGFONTW lf; memset(&lf, 0, sizeof lf); if (SUCCEEDED(hr = X.Font(c->h, dc, part, state, id, &lf))) { sprintf(sub, "%s%d:Font", pre, id); lf_emit(sub, hr, 0, &lf); } }
        { INTLIST il; memset(&il, 0, sizeof il); if (SUCCEEDED(hr = X.IntList(c->h, part, state, id, &il))) {
              char t[200] = ""; int i; for (i = 0; i < il.iValueCount && i < 24; i++) sprintf(t + strlen(t), "%s%d", i ? " " : "", il.iValues[i]);
              sprintf(sub, "%s%d:IntList", pre, id); emit(sub, HR(hr), 0, 0, 1, (long[]){il.iValueCount}, t); } }
    }
}
static void h_PropSweep(Ctx *c) { sweep(c, c->part, c->state, ""); }

static void grid(Ctx *c, int do_props)
{
    int pmax = (int)aux_int(c->aux, "pmax", 40), smax = (int)aux_int(c->aux, "smax", 20), p, s, ts;
    for (p = 0; p <= pmax; p++)
        for (s = 0; s <= smax; s++) {
            char sub[96]; BOOL d = X.Defined(c->h, p, s);
            if (!d) continue;
            sprintf(sub, "%d:%d:defined", p, s); emit(sub, "-", 0, 1, 0, NULL, "");
            for (ts = 0; ts < 3; ts++) {
                SIZE sz = {SENT, SENT}; RECT rc = {0, 0, 100, 30}; HRESULT hr = X.PartSize(c->h, NULL, p, s, ts == 2 ? &rc : NULL, ts, &sz);
                sprintf(sub, "%d:%d:%s", p, s, ts == 0 ? "MIN" : ts == 1 ? "TRUE" : "DRAW");
                emit_hr(sub, hr, 0, 2, sz.cx, sz.cy, 0, 0, "");
            }
            if (do_props) { sprintf(sub, "%d:%d/", p, s); sweep(c, p, s, sub); }
        }
}
static void h_PartGrid(Ctx *c) { grid(c, 0); }
static void h_PropGrid(Ctx *c) { grid(c, 1); }

/* ---------------------------------------------------------------- Open / lifetime / app-level rows */
static void h_Open(Ctx *c)
{
    char v[32]; HWND hw = g_hwnd; DWORD le, flags = 0; int useex = 0; HTHEME h, h2 = NULL; long vals[6]; int n = 0;
    if (aux_find(c->aux, "hwnd", v, sizeof v)) hw = !strcmp(v, "bad") ? (HWND)(ULONG_PTR)0xDEAD0 : (HWND)(ULONG_PTR)strtoul(v, NULL, 0);
    if (aux_find(c->aux, "flags", v, sizeof v)) { flags = strtoul(v, NULL, 0); useex = 1; }
    set_sub(c->sub);
    h = do_open(c->cls, hw, c->dpi, c->nc, flags, useex, &le);
    vals[n++] = h != NULL;
    if (hw == g_hwnd && h) vals[n++] = X.GetWT(g_hwnd) != NULL; else vals[n++] = SENT;
    if (aux_int(c->aux, "twice", 0)) { DWORD l2; h2 = do_open(c->cls, hw, c->dpi, c->nc, flags, useex, &l2); vals[n++] = (h2 == h); }
    emit("", "-", le, h != NULL, n, vals, "v0=nonnull v1=GetWindowTheme_nonnull v2=second_open_same_value");
    if (h2) X.Close(h2);
    if (h) X.Close(h);
}
static void h_Close(Ctx *c)
{
    HRESULT hr; DWORD le;
    if (!strcmp(c->cls, "@NULLH")) { SetLastError(0x5A5A5A5A); hr = X.Close(NULL); emit_hr("", hr, GetLastError(), 0, 0, 0, 0, 0, ""); return; }
    if (!strcmp(c->cls, "@BOGUS")) { SetLastError(0x5A5A5A5A); hr = X.Close((HTHEME)(ULONG_PTR)0x7fff0001); emit_hr("", hr, GetLastError(), 0, 0, 0, 0, 0, ""); return; }
    HTHEME h = do_open(c->cls, g_hwnd, 98, 0, 0, 0, &le);
    if (!h) { emit("", "OPENFAIL", le, 0, 0, NULL, ""); return; }
    SetLastError(0x5A5A5A5A); hr = X.Close(h); emit_hr("close1", hr, GetLastError(), 0, 0, 0, 0, 0, "");
    SetLastError(0x5A5A5A5A); hr = X.Close(h); emit_hr("close2", hr, GetLastError(), 0, 0, 0, 0, 0, "");
}
static void h_SetProps(Ctx *c)
{
    DWORD orig = X.GetProps(), le; long val = aux_int(c->aux, "value", 3); HTHEME h;
    X.SetProps((DWORD)val);
    SetLastError(0x5A5A5A5A); long a = X.IsActive(); le = GetLastError(); emit("IsThemeActive", "-", le, a, 0, NULL, "");
    a = X.IsAppThemed(); emit("IsAppThemed", "-", GetLastError(), a, 0, NULL, "");
    emit("GetThemeAppProperties", "-", 0, X.GetProps(), 0, NULL, "");
    h = do_open("Button", g_hwnd, 99, 0, 0, 0, &le); emit("OpenButton", "-", le, h != NULL, 0, NULL, ""); if (h) X.Close(h);
    h = do_open("Button", g_hwnd, 0, 1, 0, 0, &le);  emit("OpenNcButton", "-", le, h != NULL, 0, NULL, ""); if (h) X.Close(h);
    X.SetProps(orig);
}
static void h_SetWindowTheme(Ctx *c)
{
    char sub[96], ids[96]; HWND hw = g_hwnd; HRESULT hr; HTHEME h; DWORD le;
    if (!aux_int(c->aux, "hwnd", 1)) { SetLastError(0x5A5A5A5A); hr = X.SetWT(NULL, L"Explorer", NULL); emit_hr("", hr, GetLastError(), 0, 0, 0, 0, 0, ""); return; }
    int hs = aux_find(c->aux, "sub", sub, sizeof sub), hi = aux_find(c->aux, "ids", ids, sizeof ids);
    SetLastError(0x5A5A5A5A); hr = X.SetWT(hw, hs ? W(sub) : NULL, hi ? W(ids) : NULL);
    emit_hr("", hr, GetLastError(), 0, 0, 0, 0, 0, "");
    emit("GetWindowTheme", "-", 0, X.GetWT(hw) != NULL, 0, NULL, "");
    h = do_open("Header", hw, 0, 0, 0, 0, &le); emit("OpenHeader", "-", le, h != NULL, 0, NULL, ""); if (h) X.Close(h);
    X.SetWT(hw, NULL, NULL); g_sub[0] = 0;
}

/* ---------------------------------------------------------------- control-level probes */
static void pump(void) { MSG m; while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); } }
static HWND mk(const wchar_t *cls, DWORD style, DWORD ex, int x, int y, int w, int h)
{
    return CreateWindowExW(ex, cls, L"", WS_CHILD | style, x, y, w, h, g_hwnd, (HMENU)(ULONG_PTR)1, GetModuleHandleW(NULL), NULL);
}
static void ctl_setup(HWND w, const char *aux)
{
    char v[32]; HFONT f;
    if (aux_find(aux, "theme", v, sizeof v)) {
        if (!strcmp(v, "off")) X.SetWT(w, L"", L"");
        else if (!strcmp(v, "explorer")) X.SetWT(w, L"Explorer", NULL);
    }
    if (aux_find(aux, "font", v, sizeof v) && (f = mkfont(v)) != NULL) SendMessageW(w, WM_SETFONT, (WPARAM)f, TRUE);
}
static int fonth(HWND w)
{
    HFONT f = (HFONT)SendMessageW(w, WM_GETFONT, 0, 0); TEXTMETRICW tm; HGDIOBJ o;
    o = SelectObject(g_mdc, f ? f : GetStockObject(SYSTEM_FONT)); GetTextMetricsW(g_mdc, &tm); SelectObject(g_mdc, o); return tm.tmHeight;
}
static int rect_h(HWND w) { RECT r; GetWindowRect(w, &r); return r.bottom - r.top; }
static int rect_w(HWND w) { RECT r; GetWindowRect(w, &r); return r.right - r.left; }

static void h_Ctl(Ctx *c)
{
    const char *k = c->cls, *aux = c->aux; HWND w = NULL; long v[6] = {SENT, SENT, SENT, SENT, SENT, SENT}; int nv = 6; char txt[220] = "";
    int i, items = (int)aux_int(aux, "items", 0); DWORD style = (DWORD)aux_int(aux, "style", 0);
    pump();
    if (!strcmp(k, "header")) {
        RECT rc = {0, 0, 400, 200}; WINDOWPOS wp; HDLAYOUT hl; memset(&wp, 0, sizeof wp);
        w = mk(L"SysHeader32", HDS_BUTTONS | HDS_HORZ | style, 0, 0, 0, 400, 30);
        if (!w) goto fail;
        ctl_setup(w, aux);
        for (i = 0; i < items; i++) { HDITEMW it; memset(&it, 0, sizeof it); it.mask = HDI_TEXT | HDI_WIDTH | HDI_FORMAT; it.cxy = 100; it.pszText = (LPWSTR)L"Name"; it.fmt = HDF_LEFT | HDF_STRING; SendMessageW(w, HDM_INSERTITEMW, i, (LPARAM)&it); }
        hl.prc = &rc; hl.pwpos = &wp; SendMessageW(w, HDM_LAYOUT, 0, (LPARAM)&hl);
        v[0] = wp.cy; v[1] = X.GetWT(w) != NULL; v[2] = fonth(w); v[3] = (long)SendMessageW(w, HDM_GETITEMCOUNT, 0, 0); v[4] = wp.y; v[5] = wp.cx;
        strcpy(txt, "v0=HDM_LAYOUT_cy v1=GetWindowTheme_nonnull v2=font_tmHeight v3=items v4=wp.y v5=wp.cx");
    } else if (!strcmp(k, "lvheader")) {
        HWND hd; w = mk(L"SysListView32", LVS_REPORT, 0, 0, 0, 400, 200); if (!w) goto fail;
        ctl_setup(w, aux);
        for (i = 0; i < (items ? items : 1); i++) { LVCOLUMNW col; memset(&col, 0, sizeof col); col.mask = LVCF_TEXT | LVCF_WIDTH; col.cx = 100; col.pszText = (LPWSTR)L"Col"; SendMessageW(w, LVM_INSERTCOLUMNW, i, (LPARAM)&col); }
        { LVITEMW it; memset(&it, 0, sizeof it); it.mask = LVIF_TEXT; it.pszText = (LPWSTR)L"Item"; SendMessageW(w, LVM_INSERTITEMW, 0, (LPARAM)&it); }
        hd = (HWND)SendMessageW(w, LVM_GETHEADER, 0, 0);
        v[0] = hd ? rect_h(hd) : SENT; v[1] = hd ? X.GetWT(hd) != NULL : SENT;
        { RECT r; r.left = LVIR_BOUNDS; v[2] = SendMessageW(w, LVM_GETITEMRECT, 0, (LPARAM)&r) ? r.bottom - r.top : SENT; r.left = LVIR_LABEL; v[3] = SendMessageW(w, LVM_GETITEMRECT, 0, (LPARAM)&r) ? r.bottom - r.top : SENT; }
        v[4] = X.GetWT(w) != NULL; v[5] = fonth(w);
        strcpy(txt, "v0=header_height v1=header_themed v2=item_bounds_h v3=item_label_h v4=lv_themed v5=font_tmHeight");
    } else if (!strcmp(k, "status")) {
        int b[3] = {SENT, SENT, SENT}; w = mk(STATUSCLASSNAMEW, SBARS_SIZEGRIP, 0, 0, 0, 0, 0); if (!w) goto fail;
        ctl_setup(w, aux); SendMessageW(w, WM_SIZE, 0, 0); SendMessageW(w, SB_GETBORDERS, 0, (LPARAM)b);
        v[0] = rect_h(w); v[1] = b[0]; v[2] = b[1]; v[3] = b[2]; v[4] = X.GetWT(w) != NULL; v[5] = fonth(w);
        strcpy(txt, "v0=height v1..v3=SB_GETBORDERS v4=themed v5=font_tmHeight");
    } else if (!strcmp(k, "tab")) {
        RECT r = {0, 0, 0, 0}, adj = {0, 0, 300, 200}; w = mk(L"SysTabControl32", 0, 0, 0, 0, 300, 200); if (!w) goto fail;
        ctl_setup(w, aux);
        for (i = 0; i < (items ? items : 1); i++) { TCITEMW it; memset(&it, 0, sizeof it); it.mask = TCIF_TEXT; it.pszText = (LPWSTR)L"Tab"; SendMessageW(w, TCM_INSERTITEMW, i, (LPARAM)&it); }
        SendMessageW(w, TCM_GETITEMRECT, 0, (LPARAM)&r); SendMessageW(w, TCM_ADJUSTRECT, FALSE, (LPARAM)&adj);
        v[0] = r.right - r.left; v[1] = r.bottom - r.top; v[2] = adj.left; v[3] = adj.top; v[4] = adj.right; v[5] = adj.bottom;
        strcpy(txt, "v0,v1=item w,h v2..v5=TCM_ADJUSTRECT(FALSE;0 0 300 200) l t r b");
    } else if (!strcmp(k, "toolbar")) {
        w = mk(TOOLBARCLASSNAMEW, TBSTYLE_FLAT | CCS_TOP, 0, 0, 0, 0, 0); if (!w) goto fail;
        ctl_setup(w, aux); SendMessageW(w, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
        for (i = 0; i < (items ? items : 1); i++) { TBBUTTON b; memset(&b, 0, sizeof b); b.iBitmap = I_IMAGENONE; b.idCommand = 100 + i; b.fsState = TBSTATE_ENABLED; b.fsStyle = BTNS_AUTOSIZE | BTNS_SHOWTEXT; b.iString = (INT_PTR)L"Button"; SendMessageW(w, TB_ADDBUTTONSW, 1, (LPARAM)&b); }
        SendMessageW(w, TB_AUTOSIZE, 0, 0); { LRESULT s = SendMessageW(w, TB_GETBUTTONSIZE, 0, 0), p = SendMessageW(w, TB_GETPADDING, 0, 0);
            v[0] = rect_h(w); v[1] = LOWORD(s); v[2] = HIWORD(s); v[3] = LOWORD(p); v[4] = HIWORD(p); v[5] = X.GetWT(w) != NULL; }
        strcpy(txt, "v0=height v1,v2=TB_GETBUTTONSIZE cx cy v3,v4=TB_GETPADDING v5=themed");
    } else if (!strcmp(k, "combo")) {
        COMBOBOXINFO ci; memset(&ci, 0, sizeof ci); ci.cbSize = sizeof ci; w = mk(L"ComboBox", CBS_DROPDOWNLIST, 0, 0, 0, 150, 200); if (!w) goto fail;
        ctl_setup(w, aux); GetComboBoxInfo(w, &ci);
        v[0] = rect_h(w); v[1] = (long)SendMessageW(w, CB_GETITEMHEIGHT, (WPARAM)-1, 0); v[2] = (long)SendMessageW(w, CB_GETITEMHEIGHT, 0, 0);
        v[3] = ci.rcButton.right - ci.rcButton.left; v[4] = ci.rcItem.bottom - ci.rcItem.top; v[5] = X.GetWT(w) != NULL;
        strcpy(txt, "v0=window h v1=CB_GETITEMHEIGHT(-1) v2=item0 v3=button w v4=rcItem h v5=themed");
    } else if (!strcmp(k, "edit")) {
        RECT r = {0, 0, 0, 0}, cr; w = mk(L"Edit", ES_AUTOHSCROLL | WS_BORDER, WS_EX_CLIENTEDGE, 0, 0, 120, 30); if (!w) goto fail;
        ctl_setup(w, aux); SendMessageW(w, EM_GETRECT, 0, (LPARAM)&r); GetClientRect(w, &cr);
        v[0] = r.left; v[1] = r.top; v[2] = cr.right - r.right; v[3] = cr.bottom - r.bottom; v[4] = cr.right; v[5] = cr.bottom;
        strcpy(txt, "v0,v1=EM_GETRECT l t v2,v3=client minus rect r b v4,v5=client w h");
    } else if (!strcmp(k, "button")) {
        char kind[16] = "push"; DWORD bs = BS_PUSHBUTTON; SIZE s = {SENT, SENT}; RECT m = {SENT, SENT, SENT, SENT};
        aux_find(aux, "kind", kind, sizeof kind);
        if (!strcmp(kind, "check")) bs = BS_AUTOCHECKBOX; else if (!strcmp(kind, "radio")) bs = BS_AUTORADIOBUTTON; else if (!strcmp(kind, "group")) bs = BS_GROUPBOX;
        w = mk(L"Button", bs, 0, 0, 0, 100, 30); if (!w) goto fail; SetWindowTextW(w, L"OK");
        ctl_setup(w, aux); SendMessageW(w, 0x1601 /*BCM_GETIDEALSIZE*/, 0, (LPARAM)&s); SendMessageW(w, 0x1606 /*BCM_GETTEXTMARGIN*/, 0, (LPARAM)&m);
        v[0] = s.cx; v[1] = s.cy; v[2] = m.left; v[3] = m.top; v[4] = m.right; v[5] = m.bottom;
        strcpy(txt, "v0,v1=BCM_GETIDEALSIZE v2..v5=BCM_GETTEXTMARGIN l t r b (sentinel when unsupported)");
    } else if (!strcmp(k, "trackbar")) {
        RECT t = {SENT, SENT, SENT, SENT}, ch = {SENT, SENT, SENT, SENT}; int vert = (style & TBS_VERT) != 0;
        w = mk(L"msctls_trackbar32", style, 0, 0, 0, vert ? 45 : 200, vert ? 200 : 45); if (!w) goto fail;
        ctl_setup(w, aux); SendMessageW(w, TBM_GETTHUMBRECT, 0, (LPARAM)&t); SendMessageW(w, TBM_GETCHANNELRECT, 0, (LPARAM)&ch);
        v[0] = t.right - t.left; v[1] = t.bottom - t.top; v[2] = ch.left; v[3] = ch.top; v[4] = ch.right; v[5] = ch.bottom;
        strcpy(txt, "v0,v1=thumb w h v2..v5=channel l t r b");
    } else if (!strcmp(k, "updown")) {
        HWND e = mk(L"Edit", WS_BORDER, WS_EX_CLIENTEDGE, 0, 0, 80, 24); w = mk(L"msctls_updown32", UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_ARROWKEYS, 0, 0, 0, 0, 0);
        if (!w || !e) goto fail;
        ctl_setup(e, aux); ctl_setup(w, aux); SendMessageW(w, UDM_SETBUDDY, (WPARAM)e, 0);
        v[0] = rect_w(w); v[1] = rect_h(w); v[2] = rect_w(e); v[3] = rect_h(e); v[4] = X.GetWT(w) != NULL; v[5] = SENT;
        strcpy(txt, "v0,v1=updown w h v2,v3=buddy w h v4=themed");
        DestroyWindow(e);
    } else if (!strcmp(k, "listbox")) {
        w = mk(L"ListBox", LBS_NOTIFY, WS_EX_CLIENTEDGE, 0, 0, 100, 100); if (!w) goto fail; ctl_setup(w, aux);
        SendMessageW(w, LB_ADDSTRING, 0, (LPARAM)L"Item"); v[0] = (long)SendMessageW(w, LB_GETITEMHEIGHT, 0, 0); v[1] = fonth(w); nv = 2;
        strcpy(txt, "v0=LB_GETITEMHEIGHT v1=font_tmHeight");
    } else { emit("", "NA", 0, 0, 0, NULL, "unknown ctl kind"); return; }
    emit("", "0x00000000", 0, 0, nv, v, txt);
    if (w) DestroyWindow(w);
    pump();
    return;
fail:
    emit("", "CREATEFAIL", GetLastError(), 0, 0, NULL, "CreateWindowEx failed (class missing or comctl32 not initialised)");
}

/* ---------------------------------------------------------------- environment rows */
static const char *vstr(const wchar_t *path)
{
    static char b[64]; DWORD h, n; BYTE *d; VS_FIXEDFILEINFO *fi; UINT l; b[0] = 0;
    static BOOL (WINAPI *gsz)(LPCWSTR, LPDWORD); static BOOL (WINAPI *gi)(LPCWSTR, DWORD, DWORD, LPVOID); static BOOL (WINAPI *vq)(LPCVOID, LPCWSTR, LPVOID *, PUINT);
    if (!gsz) { HMODULE m = LoadLibraryW(L"version.dll"); if (!m) return "?"; gsz = (void *)GetProcAddress(m, "GetFileVersionInfoSizeW"); gi = (void *)GetProcAddress(m, "GetFileVersionInfoW"); vq = (void *)GetProcAddress(m, "VerQueryValueW"); }
    if (!gsz || !gi || !vq) return "?";
    n = gsz(path, &h); if (!n) return "noversion";
    d = malloc(n); if (!d) return "?";
    if (gi(path, 0, n, d) && vq(d, L"\\", (LPVOID *)&fi, &l) && fi)
        sprintf(b, "%u.%u.%u.%u", HIWORD(fi->dwFileVersionMS), LOWORD(fi->dwFileVersionMS), HIWORD(fi->dwFileVersionLS), LOWORD(fi->dwFileVersionLS));
    free(d); return b;
}
static const char *regs(HKEY root, const wchar_t *key, const wchar_t *val)
{
    static char b[8][300]; static int i; char *o = b[i = (i + 1) & 7]; DWORD type = 0, sz = 0; HKEY k; o[0] = 0;
    if (RegOpenKeyExW(root, key, 0, KEY_READ | KEY_WOW64_64KEY, &k)) { strcpy(o, "(nokey)"); return o; }
    if (RegQueryValueExW(k, val, NULL, &type, NULL, &sz)) { strcpy(o, "(absent)"); RegCloseKey(k); return o; }
    if (type == REG_DWORD) { DWORD d = 0; sz = 4; RegQueryValueExW(k, val, NULL, NULL, (BYTE *)&d, &sz); sprintf(o, "dword:0x%08lX", (unsigned long)d); }
    else if (type == REG_SZ || type == REG_EXPAND_SZ) { WCHAR w[260]; sz = sizeof w - 2; if (RegQueryValueExW(k, val, NULL, NULL, (BYTE *)w, &sz) == 0) { w[259] = 0; strcpy(o, A(w)); } }
    else sprintf(o, "type%lu", (unsigned long)type);
    RegCloseKey(k); clean(o); return o;
}
static const char *SMN(int i)
{
    static const char *n[] = {"CXSCREEN", "CYSCREEN", "CXVSCROLL", "CYHSCROLL", "CYCAPTION", "CXBORDER", "CYBORDER", "CXDLGFRAME", "CYDLGFRAME", "CYVTHUMB", "CXHTHUMB", "CXICON", "CYICON", "CXCURSOR", "CYCURSOR", "CYMENU", "CXFULLSCREEN", "CYFULLSCREEN", "CYKANJIWINDOW", "MOUSEPRESENT", "CYVSCROLL", "CXHSCROLL", "DEBUG", "SWAPBUTTON", "R1", "R2", "R3", "R4", "CXMIN", "CYMIN", "CXSIZE", "CYSIZE", "CXFRAME", "CYFRAME", "CXMINTRACK", "CYMINTRACK", "CXDOUBLECLK", "CYDOUBLECLK", "CXICONSPACING", "CYICONSPACING", "MENUDROPALIGNMENT", "PENWINDOWS", "DBCSENABLED", "CMOUSEBUTTONS", "SECURE", "CXEDGE", "CYEDGE", "CXMINSPACING", "CYMINSPACING", "CXSMICON", "CYSMICON", "CYSMCAPTION", "CXSMSIZE", "CYSMSIZE", "CXMENUSIZE", "CYMENUSIZE", "ARRANGE", "CXMINIMIZED", "CYMINIMIZED", "CXMAXTRACK", "CYMAXTRACK", "CXMAXIMIZED", "CYMAXIMIZED", "NETWORK", "R5", "R6", "R7", "CLEANBOOT", "CXDRAG", "CYDRAG", "SHOWSOUNDS", "CXMENUCHECK", "CYMENUCHECK", "SLOWMACHINE", "MIDEASTENABLED", "MOUSEWHEELPRESENT", "XVIRTUALSCREEN", "YVIRTUALSCREEN", "CXVIRTUALSCREEN", "CYVIRTUALSCREEN", "CMONITORS", "SAMEDISPLAYFORMAT", "IMMENABLED", "CXFOCUSBORDER", "CYFOCUSBORDER", "R8", "TABLETPC", "MEDIACENTER", "STARTER", "SERVERR2", "R9", "R10", "R11", "R12", "R13", "R14", "CXPADDEDBORDER", "R15", "R16", "R17", "R18", "R19", "R20"};
    return (i >= 0 && i < (int)(sizeof n / sizeof *n)) ? n[i] : "?";
}
static void env(Ctx *c)
{
    const char *k = c->cls; char sub[96]; int i;
    if (!strcmp(k, "ThemeState")) {
        WCHAR f[300], co[100], sz[100]; HRESULT hr; HWND hw = g_hwnd; DWORD le;
        SetLastError(0x5A5A5A5A); long r = X.IsActive(); em("IsThemeActive", "-", GetLastError(), r, "");
        SetLastError(0x5A5A5A5A); r = X.IsAppThemed(); em("IsAppThemed", "-", GetLastError(), r, "");
        SetLastError(0x5A5A5A5A); r = X.IsComp ? X.IsComp() : -1; em("IsCompositionActive", X.IsComp ? "-" : "NOAPI", GetLastError(), r, "");
        em("GetThemeAppProperties", "-", 0, X.GetProps ? X.GetProps() : -1, "");
        f[0] = co[0] = sz[0] = 0; SetLastError(0x5A5A5A5A); hr = X.CurName(f, 300, co, 100, sz, 100); le = GetLastError();
        em("GetCurrentThemeName", HR(hr), le, 0, "file=%s|color=%s|size=%s", A(f), A(co), A(sz));
        em("IsThemeDialogTextureEnabled", "-", 0, X.DlgTex ? X.DlgTex(hw) : -1, "");
        if (X.QueryServices) { SetLastError(0x5A5A5A5A); r = X.QueryServices(); em("QueryThemeServices", "-", GetLastError(), r, "ordinal 1"); }
        em("SetPreferredAppMode.arg", "-", 0, 0, "dark=%s", O.dark);
        em("app-props.arg", "-", 0, O.appprops, "");
    } else if (!strcmp(k, "Modules")) {
        static const wchar_t *mods[] = {L"uxtheme.dll", L"comctl32.dll", L"user32.dll", L"gdi32.dll", L"win32u.dll", L"ntdll.dll", L"kernel32.dll", L"shell32.dll", L"dwmapi.dll"};
        for (i = 0; i < (int)(sizeof mods / sizeof *mods); i++) {
            WCHAR p[MAX_PATH]; HMODULE m = GetModuleHandleW(mods[i]); if (!m) m = LoadLibraryW(mods[i]);
            p[0] = 0; if (m) GetModuleFileNameW(m, p, MAX_PATH);
            sprintf(sub, "%s", A(mods[i])); em(sub, m ? "-" : "NA", 0, m != NULL, "path=%s|version=%s", A(p), m ? vstr(p) : "-");
        }
        if (X.CcVersion) { DLLVER v; memset(&v, 0, sizeof v); v.cbSize = sizeof v; HRESULT hr = X.CcVersion(&v); em("comctl32.DllGetVersion", HR(hr), 0, 0, "%lu.%lu.%lu platform=%lu", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber, v.dwPlatformID); }
        { LONG (WINAPI *rtl)(OSVERSIONINFOW *) = (void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"); OSVERSIONINFOW vi; memset(&vi, 0, sizeof vi); vi.dwOSVersionInfoSize = sizeof vi;
          if (rtl && rtl(&vi) == 0) em("RtlGetVersion", "-", 0, 0, "%lu.%lu.%lu", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber); }
        { const wchar_t *k2 = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
          em("CurrentVersion", "-", 0, 0, "ProductName=%s|DisplayVersion=%s|CurrentBuild=%s|UBR=%s|EditionID=%s", regs(HKEY_LOCAL_MACHINE, k2, L"ProductName"), regs(HKEY_LOCAL_MACHINE, k2, L"DisplayVersion"), regs(HKEY_LOCAL_MACHINE, k2, L"CurrentBuild"), regs(HKEY_LOCAL_MACHINE, k2, L"UBR"), regs(HKEY_LOCAL_MACHINE, k2, L"EditionID")); }
        em("exe.comctl", "-", 0, 0, "requested=%s actctx=%s", O.comctl, g_actctx != INVALID_HANDLE_VALUE ? "active" : "none");
    } else if (!strcmp(k, "Display")) {
        UINT (WINAPI *gds)(void) = (void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForSystem");
        UINT (WINAPI *gdw)(HWND) = (void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
        DPI_AWARENESS_CONTEXT (WINAPI *gtc)(void) = (void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetThreadDpiAwarenessContext");
        int (WINAPI *gaw)(DPI_AWARENESS_CONTEXT) = (void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetAwarenessFromDpiAwarenessContext");
        em("GetDpiForSystem", gds ? "-" : "NOAPI", 0, gds ? gds() : -1, "");
        em("GetDpiForWindow", gdw ? "-" : "NOAPI", 0, gdw ? gdw(g_hwnd) : -1, "");
        em("DpiAwareness", gtc && gaw ? "-" : "NOAPI", 0, gtc && gaw ? gaw(gtc()) : -1, "0=unaware 1=system 2=permonitor; requested=%s", O.dpiaw);
        em("LOGPIXELSX", "-", 0, GetDeviceCaps(g_dc, LOGPIXELSX), ""); em("LOGPIXELSY", "-", 0, GetDeviceCaps(g_dc, LOGPIXELSY), "");
        em("screen", "-", 0, GetSystemMetrics(SM_CXSCREEN), "cx=%d|cy=%d|bpp=%d", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), GetDeviceCaps(g_dc, BITSPIXEL));
        em("remote", "-", 0, GetSystemMetrics(SM_REMOTESESSION), "SM_REMOTESESSION");
    } else if (!strcmp(k, "HighContrast")) {
        HIGHCONTRASTW hc; memset(&hc, 0, sizeof hc); hc.cbSize = sizeof hc; BOOL ok = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof hc, &hc, 0);
        em("SPI_GETHIGHCONTRAST", ok ? "-" : "FAIL", GetLastError(), hc.dwFlags, "flags=0x%lX (HCF_HIGHCONTRASTON=1)|scheme=%s", (unsigned long)hc.dwFlags, hc.lpszDefaultScheme ? A(hc.lpszDefaultScheme) : "");
    } else if (!strcmp(k, "Personalize")) {
        const wchar_t *pk = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
        const wchar_t *tm = L"Software\\Microsoft\\Windows\\CurrentVersion\\ThemeManager";
        em("AppsUseLightTheme", "-", 0, 0, "%s", regs(HKEY_CURRENT_USER, pk, L"AppsUseLightTheme"));
        em("SystemUsesLightTheme", "-", 0, 0, "%s", regs(HKEY_CURRENT_USER, pk, L"SystemUsesLightTheme"));
        em("EnableTransparency", "-", 0, 0, "%s", regs(HKEY_CURRENT_USER, pk, L"EnableTransparency"));
        em("ColorPrevalence", "-", 0, 0, "%s", regs(HKEY_CURRENT_USER, pk, L"ColorPrevalence"));
        em("ThemeManager.ThemeActive", "-", 0, 0, "%s", regs(HKEY_CURRENT_USER, tm, L"ThemeActive"));
        em("ThemeManager.DllName", "-", 0, 0, "%s", regs(HKEY_CURRENT_USER, tm, L"DllName"));
        em("ThemeManager.ColorName", "-", 0, 0, "%s", regs(HKEY_CURRENT_USER, tm, L"ColorName"));
        em("ThemeManager.SizeName", "-", 0, 0, "%s", regs(HKEY_CURRENT_USER, tm, L"SizeName"));
        em("CurrentTheme", "-", 0, 0, "%s", regs(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes", L"CurrentTheme"));
        em("DWM.ColorizationColor", "-", 0, 0, "%s", regs(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"ColorizationColor"));
        em("DWM.AccentColor", "-", 0, 0, "%s", regs(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor"));
        em("WindowMetrics.AppliedDPI", "-", 0, 0, "%s", regs(HKEY_CURRENT_USER, L"Control Panel\\Desktop\\WindowMetrics", L"AppliedDPI"));
        em("Desktop.LogPixels", "-", 0, 0, "%s", regs(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"LogPixels"));
    } else if (!strcmp(k, "SysMetrics")) {
        static const int dpis[] = {96, 120, 144, 168, 192};
        static const int scal[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 30, 31, 32, 33, 34, 35, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 71, 72, 75, 76, 77, 78, 92};
        int (WINAPI *gsm)(int, UINT) = (void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi"); int d, j;
        for (i = 0; i < 100; i++) { sprintf(sub, "SM%d.%s", i, SMN(i)); SetLastError(0); em(sub, "-", GetLastError(), GetSystemMetrics(i), ""); }
        if (gsm) for (d = 0; d < 5; d++) for (j = 0; j < (int)(sizeof scal / sizeof *scal); j++) { sprintf(sub, "SMdpi%d.%d.%s", dpis[d], scal[j], SMN(scal[j])); em(sub, "-", 0, gsm(scal[j], dpis[d]), ""); }
        else em("GetSystemMetricsForDpi", "NOAPI", 0, 0, "");
    } else if (!strcmp(k, "NonClientMetrics")) {
        static const int dpis[] = {0, 96, 120, 144, 168, 192}; int d;
        BOOL (WINAPI *spf)(UINT, UINT, PVOID, UINT, UINT) = (void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SystemParametersInfoForDpi");
        for (d = 0; d < 6; d++) {
            NONCLIENTMETRICSW n; BOOL ok; char p[32]; memset(&n, 0, sizeof n); n.cbSize = sizeof n;
            if (dpis[d] == 0) ok = SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof n, &n, 0); else if (spf) ok = spf(SPI_GETNONCLIENTMETRICS, sizeof n, &n, 0, dpis[d]); else continue;
            sprintf(p, dpis[d] ? "dpi%d." : "sys.", dpis[d]);
            if (!ok) { em(p, "FAIL", GetLastError(), 0, ""); continue; }
#define NCM(f) do { sprintf(sub, "%sncm.%s", p, #f); em(sub, "-", 0, n.f, ""); } while (0)
            NCM(iBorderWidth); NCM(iScrollWidth); NCM(iScrollHeight); NCM(iCaptionWidth); NCM(iCaptionHeight); NCM(iSmCaptionWidth); NCM(iSmCaptionHeight); NCM(iMenuWidth); NCM(iMenuHeight); NCM(iPaddedBorderWidth);
#define NLF(f) do { sprintf(sub, "%sncm.%s", p, #f); lf_emit(sub, 0, 0, &n.f); } while (0)
            NLF(lfCaptionFont); NLF(lfSmCaptionFont); NLF(lfMenuFont); NLF(lfStatusFont); NLF(lfMessageFont);
        }
        { ICONMETRICSW im; LOGFONTW lf; memset(&im, 0, sizeof im); im.cbSize = sizeof im;
          if (SystemParametersInfoW(SPI_GETICONMETRICS, sizeof im, &im, 0)) { em("iconmetrics", "-", 0, 0, "iHorzSpacing=%d|iVertSpacing=%d|iTitleWrap=%d", im.iHorzSpacing, im.iVertSpacing, im.iTitleWrap); lf_emit("iconmetrics.lfFont", 0, 0, &im.lfFont); }
          if (SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof lf, &lf, 0)) lf_emit("icontitlelogfont", 0, 0, &lf); }
        { BOOL fm = 0; SystemParametersInfoW(0x1022 /*SPI_GETFLATMENU*/, 0, &fm, 0); em("SPI_GETFLATMENU", "-", 0, fm, ""); }
    } else if (!strcmp(k, "Fonts")) {
        TEXTMETRICW tm; LOGFONTW lf; HFONT f; const char *nm[] = {"msg", "gui", "sys"}; int j;
        for (j = 0; j < 3; j++) {
            f = mkfont(nm[j]); if (!f) continue; GetObjectW(f, sizeof lf, &lf); sprintf(sub, "%s.logfont", nm[j]); lf_emit(sub, 0, 0, &lf);
            SelectObject(g_mdc, f); GetTextMetricsW(g_mdc, &tm); sprintf(sub, "%s.textmetric", nm[j]);
            em(sub, "-", 0, tm.tmHeight, "tmHeight=%ld|tmAscent=%ld|tmDescent=%ld|tmInternalLeading=%ld|tmExternalLeading=%ld|tmAveCharWidth=%ld", tm.tmHeight, tm.tmAscent, tm.tmDescent, tm.tmInternalLeading, tm.tmExternalLeading, tm.tmAveCharWidth);
        }
    } else if (!strcmp(k, "SysColors")) {
        for (i = 0; i <= 30; i++) { char t[16]; DWORD v = GetSysColor(i); sprintf(sub, "COLOR_%d", i); sprintf(t, "0x%06lX", (unsigned long)v); em(sub, "-", 0, (long)v, "%s", t); }
    } else em("", "NA", 0, 0, "unknown env item");
}

/* ---------------------------------------------------------------- dispatcher */
typedef struct { const char *name; void (*fn)(Ctx *); int need; } Api;   /* need: 1 = theme handle required */
static const Api APIS[] = {
    {"GetThemePartSize", h_PartSize, 1}, {"GetThemeInt", h_Int, 1}, {"GetThemeEnumValue", h_Enum, 1}, {"GetThemeMetric", h_Metric, 1},
    {"GetThemeBool", h_Bool, 1}, {"GetThemeColor", h_Color, 1}, {"GetThemeMargins", h_Margins, 1}, {"GetThemePosition", h_Position, 1},
    {"GetThemeRect", h_Rect, 1}, {"GetThemeFont", h_Font, 1}, {"GetThemeString", h_String, 1}, {"GetThemeFilename", h_String, 1},
    {"GetThemeIntList", h_IntList, 1}, {"GetThemePropertyOrigin", h_Origin, 1}, {"GetThemeBackgroundContentRect", h_Content, 1},
    {"GetThemeBackgroundExtent", h_Content, 1}, {"GetThemeTextExtent", h_TextExtent, 1}, {"GetThemeTextMetrics", h_TextMetrics, 1},
    {"IsThemePartDefined", h_Defined, 1}, {"IsThemeBackgroundPartiallyTransparent", h_Defined, 1}, {"GetThemeBackgroundRegion", h_Region, 1},
    {"GetThemeTransitionDuration", h_Transition, 1}, {"DrawThemeBackground", h_Draw, 1},
    {"GetThemeSysColor", h_Sys, 1}, {"GetThemeSysColorBrush", h_Sys, 1}, {"GetThemeSysSize", h_Sys, 1}, {"GetThemeSysBool", h_Sys, 1},
    {"GetThemeSysInt", h_Sys, 1}, {"GetThemeSysFont", h_Sys, 1}, {"GetThemeSysString", h_Sys, 1},
    {"PropSweep", h_PropSweep, 1}, {"PartGrid", h_PartGrid, 1}, {"PropGrid", h_PropGrid, 1},
    {"OpenTheme", h_Open, 0}, {"CloseThemeData", h_Close, 0}, {"SetThemeAppProperties", h_SetProps, 0}, {"SetWindowTheme", h_SetWindowTheme, 0},
    {"Ctl", h_Ctl, 0}, {"Env", env, 0},
};

/* GetThemeSysInt(hTheme, id, &value): id in 'prop'; dispatched before the table because it prints the out value */
static void fix_sysint(Ctx *c)
{
    int iv = SENT; HRESULT hr; SetLastError(0x5A5A5A5A);
    hr = X.SysInt(c->h, c->prop, &iv); emit_hr("", hr, GetLastError(), 1, iv, 0, 0, 0, "");
}

#define CSV_MAX 40
static char *colv[CSV_MAX]; static int ncol; static char *colname[CSV_MAX];
static const char *F(char **f, const char *name)
{
    int i; for (i = 0; i < ncol; i++) if (!strcmp(colname[i], name)) return f[i]; return "";
}
static int split(char *line, char **f)
{
    int n = 0; char *p = line; f[n++] = p;
    while (*p) { if (*p == ',') { *p = 0; if (n < CSV_MAX) f[n++] = p + 1; } p++; }
    return n;
}
static void run_row(char **f)
{
    Ctx c; int i; memset(&c, 0, sizeof c);
    c.api = F(f, "api"); c.cls = F(f, "class"); c.sub = F(f, "subapp"); c.aux = F(f, "aux"); c.grp = F(f, "grp");
    c.part = atoi(F(f, "part")); c.state = atoi(F(f, "state")); c.prop = atoi(F(f, "prop")); c.ts = atoi(F(f, "ts")); c.dpi = atoi(F(f, "dpi")); c.nc = atoi(F(f, "nc"));
    c.usehw = (int)aux_int(c.aux, "hwnd", 1) != 0;
    g_cur_nc = c.nc;
    if (c.nc && O.nonc) { emit("", "SKIPPED", 0, 0, 0, NULL, "--no-nc: ordinal 49 (OpenNcThemeData) disabled after a crash"); return; }
    if (!strcmp(c.sub, "(none)")) c.sub = "";
    for (i = 0; i < (int)(sizeof APIS / sizeof *APIS); i++) if (!strcmp(APIS[i].name, c.api)) break;
    if (i == (int)(sizeof APIS / sizeof *APIS)) { emit("", "NOAPI", 0, 0, 0, NULL, c.api); return; }
    if (APIS[i].need && !get_handle(&c)) return;
    if (!strcmp(c.api, "GetThemeSysInt")) { fix_sysint(&c); return; }
    APIS[i].fn(&c);
}

/* ---------------------------------------------------------------- run control */
static LONG WINAPI crash_filter(EXCEPTION_POINTERS *e)
{
    FILE *cf;
    fprintf(g_out, "%s,,CRASH,0x00000000,0,,,,,,,exception 0x%08lX at table row %d%s\n", g_id, (unsigned long)e->ExceptionRecord->ExceptionCode, g_idx, g_cur_nc ? " (nc=1 row: OpenNcThemeData ordinal 49)" : "");
    fflush(g_out);
    if ((cf = fopen(g_crashfile, "w")) != NULL) { fprintf(cf, "%d %d\n", g_idx, g_cur_nc); fclose(cf); }
    TerminateProcess(GetCurrentProcess(), 3);   /* not ExitProcess: DLL detach can deadlock on locks held by the crashed call */
    return EXCEPTION_EXECUTE_HANDLER;
}
static DWORD WINAPI watchdog(LPVOID p)
{
    (void)p;
    for (;;) {
        Sleep(1000);
        if (g_active && (GetTickCount() - g_rowstart) > (DWORD)O.timeout * 1000) {
            FILE *cf; fprintf(g_out, "%s,,HANG,0x00000000,0,,,,,,,row exceeded %d s\n", g_id, O.timeout); fflush(g_out);
            if ((cf = fopen(g_crashfile, "w")) != NULL) { fprintf(cf, "%d %d\n", g_idx, g_cur_nc); fclose(cf); }
            TerminateProcess(GetCurrentProcess(), 4);
        }
    }
    return 0;
}
static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

static void setup_dpi(void)
{
    BOOL (WINAPI *sp)(DPI_AWARENESS_CONTEXT) = (void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
    DPI_AWARENESS_CONTEXT ctx;
    if (!strcmp(O.dpiaw, "unaware")) ctx = (DPI_AWARENESS_CONTEXT)-1; else if (!strcmp(O.dpiaw, "pm")) ctx = (DPI_AWARENESS_CONTEXT)-3;
    else if (!strcmp(O.dpiaw, "pmv2")) ctx = (DPI_AWARENESS_CONTEXT)-4; else ctx = (DPI_AWARENESS_CONTEXT)-2;
    if (sp) sp(ctx);
}

static int child_main(void)
{
    HANDLE th; WNDCLASSW wc; char *buf, *p, *line; long len; FILE *tf; int line_no = 0, n, i, ran = 0; char *hdr;
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    setup_dpi();
    g_out = fopen(O.out, O.append ? "ab" : "wb");
    if (!g_out) { fprintf(stderr, "cannot write %s\n", O.out); return 2; }
    snprintf(g_crashfile, sizeof g_crashfile, "%s.crash", O.out);
    if (!O.append || ftell(g_out) == 0) fprintf(g_out, "id,sub,hr,lasterr,ret,v0,v1,v2,v3,v4,v5,txt\n");
    SetUnhandledExceptionFilter(crash_filter);
    if (!strcmp(O.comctl, "v6")) {
        ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
        a.hModule = GetModuleHandleW(NULL); a.lpResourceName = MAKEINTRESOURCEW(2);
        g_actctx = CreateActCtxW(&a);
        if (g_actctx != INVALID_HANDLE_VALUE) { if (!ActivateActCtx(g_actctx, &g_cookie)) g_actctx = INVALID_HANDLE_VALUE; }
    }
    hUx = LoadLibraryW(L"uxtheme.dll"); hCc = LoadLibraryW(L"comctl32.dll");
    if (!hUx || !hCc) { fprintf(stderr, "LoadLibrary uxtheme/comctl32 failed\n"); return 2; }
#define L_(f, n) X.f = (void *)GetProcAddress(hUx, n)
    L_(DrawBg, "DrawThemeBackground"); L_(Open, "OpenThemeData"); L_(OpenDpi, "OpenThemeDataForDpi"); L_(OpenEx, "OpenThemeDataEx"); X.OpenNc = (void *)GetProcAddress(hUx, MAKEINTRESOURCEA(49)); L_(Close, "CloseThemeData");
    L_(PartSize, "GetThemePartSize"); L_(Int, "GetThemeInt"); L_(Metric, "GetThemeMetric"); L_(Bool, "GetThemeBool"); L_(Enum, "GetThemeEnumValue");
    L_(Color, "GetThemeColor"); L_(Margins, "GetThemeMargins"); L_(Pos, "GetThemePosition"); L_(Rect, "GetThemeRect"); L_(Font, "GetThemeFont");
    L_(Str, "GetThemeString"); L_(File, "GetThemeFilename"); L_(IntList, "GetThemeIntList"); L_(Origin, "GetThemePropertyOrigin");
    L_(Content, "GetThemeBackgroundContentRect"); L_(Extent, "GetThemeBackgroundExtent"); L_(TextExt, "GetThemeTextExtent"); L_(TextMet, "GetThemeTextMetrics");
    L_(Defined, "IsThemePartDefined"); L_(Transp, "IsThemeBackgroundPartiallyTransparent"); L_(Region, "GetThemeBackgroundRegion"); L_(Trans, "GetThemeTransitionDuration");
    L_(SysColor, "GetThemeSysColor"); L_(SysBrush, "GetThemeSysColorBrush"); L_(SysSize, "GetThemeSysSize"); L_(SysBool, "GetThemeSysBool"); L_(SysInt, "GetThemeSysInt");
    L_(SysFont, "GetThemeSysFont"); L_(SysStr, "GetThemeSysString"); L_(IsActive, "IsThemeActive"); L_(IsAppThemed, "IsAppThemed"); L_(IsComp, "IsCompositionActive");
    L_(GetProps, "GetThemeAppProperties"); L_(CurName, "GetCurrentThemeName"); L_(SetWT, "SetWindowTheme"); L_(GetWT, "GetWindowTheme");
    L_(SetProps, "SetThemeAppProperties"); L_(DlgTex, "IsThemeDialogTextureEnabled");
    X.QueryServices = (void *)GetProcAddress(hUx, MAKEINTRESOURCEA(1)); X.SetPreferredMode = (void *)GetProcAddress(hUx, MAKEINTRESOURCEA(135));
    X.AllowDarkWin = (void *)GetProcAddress(hUx, MAKEINTRESOURCEA(133));
    X.InitCC = (void *)GetProcAddress(hCc, "InitCommonControlsEx"); X.CcVersion = (void *)GetProcAddress(hCc, "DllGetVersion");
    if (!X.Open || !X.Close || !X.PartSize || !X.SetWT || !X.GetWT || !X.IsActive) { fprintf(stderr, "uxtheme exports missing\n"); return 2; }
    if (X.InitCC) { INITCOMMONCONTROLSEX ic; ic.dwSize = sizeof ic; ic.dwICC = 0x3FFF; X.InitCC(&ic); }
    memset(&wc, 0, sizeof wc); wc.lpfnWndProc = wndproc; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"ProbeRecordWnd"; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(0, L"ProbeRecordWnd", L"probe", WS_OVERLAPPEDWINDOW, 50, 50, 600, 400, NULL, NULL, wc.hInstance, NULL);
    if (!g_hwnd) { fprintf(stderr, "CreateWindow failed\n"); return 2; }
    if (O.show) ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    g_dc = GetDC(NULL); g_mdc = CreateCompatibleDC(g_dc);
    if (X.SetPreferredMode) { int m = !strcmp(O.dark, "allow") ? 1 : !strcmp(O.dark, "force") ? 2 : !strcmp(O.dark, "light") ? 3 : 0; if (m) X.SetPreferredMode(m); if (m == 1 || m == 2) { if (X.AllowDarkWin) X.AllowDarkWin(g_hwnd, TRUE); } }
    if (O.appprops >= 0 && X.SetProps) X.SetProps((DWORD)O.appprops);
    th = CreateThread(NULL, 0, watchdog, NULL, 0, NULL); (void)th;

    tf = fopen(O.table, "rb");
    if (!tf) { fprintf(stderr, "cannot open table %s\n", O.table); return 2; }
    fseek(tf, 0, SEEK_END); len = ftell(tf); fseek(tf, 0, SEEK_SET);
    buf = malloc(len + 2); if (!buf || fread(buf, 1, len, tf) != (size_t)len) return 2; buf[len] = 0; fclose(tf);

    if (O.from == 0) {
        char t[64]; SYSTEMTIME st; GetSystemTime(&st); sprintf(t, "%04d-%02d-%02dT%02d:%02d:%02dZ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        strcpy(g_id, "RUN"); em("label", "-", 0, 0, "%s", O.label); em("time_utc", "-", 0, 0, "%s", t);
        em("args", "-", 0, 0, "comctl=%s dpi-aware=%s dark=%s sweep=%s prio=%d grp=%s app-props=%d", O.comctl, O.dpiaw, O.dark, O.sweep, O.prio, O.grp, O.appprops);
        em("table", "-", 0, 0, "%s", O.table);
    }
    for (p = buf; p && *p; ) {
        char *nl = strchr(p, '\n'); char *f[CSV_MAX];
        line = p; if (nl) { *nl = 0; p = nl + 1; } else p = NULL;
        n = (int)strlen(line); if (n && line[n - 1] == '\r') line[n - 1] = 0;
        if (!line[0] || line[0] == '#') continue;
        if (line_no == 0) { hdr = line; ncol = split(hdr, colname); line_no++; continue; }
        line_no++;
        g_idx = line_no - 2;
        if (g_idx < O.from) continue;
        if (O.to >= 0 && g_idx > O.to) break;
        n = split(line, f); (void)n;
        if (atoi(F(f, "prio")) > O.prio) continue;
        if (O.grp[0]) { char gl[256]; snprintf(gl, sizeof gl, ",%s,", O.grp); char g1[96]; snprintf(g1, sizeof g1, ",%s,", F(f, "grp")); if (!strstr(gl, g1)) continue; }
        { int heavy = (int)aux_int(F(f, "aux"), "heavy", 0); if (heavy && strcmp(O.sweep, "full")) continue; if (!heavy && !strcmp(O.sweep, "none") && (!strcmp(F(f, "api"), "PropSweep") || !strcmp(F(f, "api"), "PartGrid"))) continue; }
        snprintf(g_id, sizeof g_id, "%s", F(f, "id"));
        if (O.list) { printf("%s %s %s\n", g_id, F(f, "api"), F(f, "class")); continue; }
        g_rowstart = GetTickCount(); g_active = 1;
        run_row(f);
        g_active = 0; ran++;
        fflush(g_out);
    }
    strcpy(g_id, "RUN"); em("rows_run", "-", 0, ran, "");
    for (i = 0; i < ntc; i++) if (tc[i].h) X.Close(tc[i].h);
    fclose(g_out);
    return 0;
}

static int supervise(int argc, char **argv)
{
    char cmd[2048], crash[MAX_PATH]; int from = 0, tries = 0, nonc = O.nonc; FILE *f;
    f = fopen(O.out, "wb"); if (!f) { fprintf(stderr, "cannot write %s\n", O.out); return 2; } fclose(f);
    snprintf(crash, sizeof crash, "%s.crash", O.out);
    for (;;) {
        STARTUPINFOA si; PROCESS_INFORMATION pi; DWORD code = 0;
        { int k; snprintf(cmd, sizeof cmd, "\"%s\" --child --append %s--from %d", argv[0], nonc ? "--no-nc " : "", from);
          for (k = 1; k < argc; k++) { strcat(cmd, " \""); strcat(cmd, argv[k]); strcat(cmd, "\""); } }
        memset(&si, 0, sizeof si); si.cb = sizeof si; remove(crash);
        if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { fprintf(stderr, "CreateProcess failed (%lu)\n", GetLastError()); return 2; }
        WaitForSingleObject(pi.hProcess, INFINITE); GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        if (code == 0) { fprintf(stderr, "done: %s\n", O.out); remove(crash); return 0; }
        if (code == 3 || code == 4) {
int idx = -1, cnc = 0; f = fopen(crash, "r"); if (f) { if (fscanf(f, "%d %d", &idx, &cnc) < 1) idx = -1; fclose(f); }
            fprintf(stderr, "row index %d %s; continuing at %d\n", idx, code == 3 ? "crashed" : "hung", idx + 1);
            if (cnc && !nonc) { nonc = 1; fprintf(stderr, "  nc=1 row crashed: skipping all nc=1 rows from now on\n"); }
            if (idx < from || ++tries > 3000) return 3;
            from = idx + 1; continue;
        }
        fprintf(stderr, "child failed with exit code %lu (setup error)\n", (unsigned long)code); return (int)code;
    }
}

static void defaults(char *argv0)
{
    char *s; snprintf(O.table, sizeof O.table, "%s", argv0);
    s = strrchr(O.table, '\\'); if (!s) s = strrchr(O.table, '/');
    if (s) { s[1] = 0; strcat(O.table, "probe_table.csv"); } else strcpy(O.table, "probe_table.csv");
    { FILE *f = fopen(O.table, "rb"); if (f) fclose(f); else strcpy(O.table, "probe_table.csv"); }
}
int main(int argc, char **argv)
{
    int i;
    defaults(argv[0]);
    for (i = 1; i < argc; i++) {
        const char *a = argv[i]; const char *v = (i + 1 < argc) ? argv[i + 1] : "";
#define OPT(n, dst) if (!strcmp(a, n)) { snprintf(dst, sizeof dst, "%s", v); i++; continue; }
        OPT("--table", O.table) OPT("--out", O.out) OPT("--label", O.label) OPT("--grp", O.grp) OPT("--comctl", O.comctl)
        OPT("--dpi-aware", O.dpiaw) OPT("--dark", O.dark) OPT("--sweep", O.sweep)
        if (!strcmp(a, "--prio")) { O.prio = atoi(v); i++; } else if (!strcmp(a, "--app-props")) { O.appprops = atoi(v); i++; }
        else if (!strcmp(a, "--timeout")) { O.timeout = atoi(v); i++; } else if (!strcmp(a, "--from")) { O.from = atoi(v); i++; } else if (!strcmp(a, "--to")) { O.to = atoi(v); i++; }
        else if (!strcmp(a, "--show")) O.show = 1; else if (!strcmp(a, "--list")) O.list = 1; else if (!strcmp(a, "--child")) O.child = 1;
        else if (!strcmp(a, "--no-nc")) O.nonc = 1; else if (!strcmp(a, "--append")) O.append = 1; else if (!strcmp(a, "--no-supervise")) O.nosup = 1;
        else if (!strcmp(a, "--help") || !strcmp(a, "-h")) { puts("see the header comment of tools/probe_record.c and docs/research/06_probe_list.md"); return 0; }
        else { fprintf(stderr, "unknown option %s\n", a); return 2; }
    }
    if (O.child || O.nosup || O.list) return child_main();
    return supervise(argc, argv);
}
