/* Wine/Windows uxtheme + header-control probe. Text output "key = value" for diffing.
 * Build: see Makefile in this dir. Loads uxtheme dynamically (harness style). */
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>
#include <shlwapi.h>
#include <stdio.h>

#define U(name) ((__typeof__(&name))GetProcAddress(hUx, #name))
static HMODULE hUx;
static void P(const char *k, const char *fmt, ...) { va_list a; printf("%s = ", k); va_start(a, fmt); vprintf(fmt, a); va_end(a); printf("\n"); }

static const char *hr(HRESULT h) { static char b[4][32]; static int i; i = (i+1)&3; sprintf(b[i], "0x%08lx", (unsigned long)h); return b[i]; }

struct pr { const wchar_t *cls; const char *cn; int part; int state; const char *pn; };
static const struct pr parts[] = {
 {L"Header","Header",HP_HEADERITEM,HIS_NORMAL,"HP_HEADERITEM/NORMAL"},
 {L"Header","Header",HP_HEADERITEM,HIS_HOT,"HP_HEADERITEM/HOT"},
 {L"Header","Header",HP_HEADERITEMLEFT,1,"HP_HEADERITEMLEFT/1"},
 {L"Header","Header",HP_HEADERITEMRIGHT,1,"HP_HEADERITEMRIGHT/1"},
 {L"Header","Header",HP_HEADERSORTARROW,HSAS_SORTEDUP,"HP_HEADERSORTARROW/UP"},
 {L"Button","Button",BP_PUSHBUTTON,PBS_NORMAL,"BP_PUSHBUTTON/NORMAL"},
 {L"Button","Button",BP_CHECKBOX,CBS_UNCHECKEDNORMAL,"BP_CHECKBOX/UNCHK"},
 {L"Button","Button",BP_RADIOBUTTON,RBS_UNCHECKEDNORMAL,"BP_RADIOBUTTON/UNCHK"},
 {L"Button","Button",BP_GROUPBOX,GBS_NORMAL,"BP_GROUPBOX/NORMAL"},
 {L"Tab","Tab",TABP_TABITEM,TIS_NORMAL,"TABP_TABITEM/NORMAL"},
 {L"Tab","Tab",TABP_TABITEM,TIS_SELECTED,"TABP_TABITEM/SELECTED"},
 {L"Tab","Tab",TABP_PANE,0,"TABP_PANE/0"},
 {L"Tab","Tab",TABP_BODY,0,"TABP_BODY/0"},
 {L"ListView","ListView",LVP_LISTITEM,LISS_NORMAL,"LVP_LISTITEM/NORMAL"},
 {L"ListView","ListView",LVP_LISTDETAIL,0,"LVP_LISTDETAIL/0"},
 {L"ListView","ListView",LVP_EMPTYTEXT,0,"LVP_EMPTYTEXT/0"},
 {L"Scrollbar","Scrollbar",SBP_ARROWBTN,ABS_UPNORMAL,"SBP_ARROWBTN/UPNORMAL"},
 {L"Scrollbar","Scrollbar",SBP_THUMBBTNVERT,SCRBS_NORMAL,"SBP_THUMBBTNVERT/NORMAL"},
 {L"Edit","Edit",EP_EDITTEXT,ETS_NORMAL,"EP_EDITTEXT/NORMAL"},
 {L"Combobox","Combobox",CP_DROPDOWNBUTTON,CBXS_NORMAL,"CP_DROPDOWNBUTTON/NORMAL"},
 {L"Window","Window",WP_CAPTION,CS_ACTIVE,"WP_CAPTION/ACTIVE"},
 {L"Window","Window",WP_CLOSEBUTTON,CBS_NORMAL,"WP_CLOSEBUTTON/NORMAL"},
 {L"Toolbar","Toolbar",TP_BUTTON,TS_NORMAL,"TP_BUTTON/NORMAL"},
 {L"Rebar","Rebar",RP_BAND,0,"RP_BAND/0"},
 {L"Status","Status",SP_PANE,0,"SP_PANE/0"},
 {L"TreeView","TreeView",TVP_GLYPH,GLPS_CLOSED,"TVP_GLYPH/CLOSED"},
 {L"Menu","Menu",MENU_POPUPITEM,MPI_NORMAL,"MENU_POPUPITEM/NORMAL"},
 {L"Progress","Progress",PP_BAR,0,"PP_BAR/0"},
 {L"Spin","Spin",SPNP_UP,UPS_NORMAL,"SPNP_UP/NORMAL"},
};

static void probe_parts(HWND hwnd, HDC hdc)
{
  HRESULT (WINAPI *Open)(HWND,LPCWSTR) = (void*)GetProcAddress(hUx,"OpenThemeData");
  HRESULT (WINAPI *Close)(HTHEME) = (void*)GetProcAddress(hUx,"CloseThemeData");
  int i;
  for (i = 0; i < (int)(sizeof parts/sizeof parts[0]); i++) {
    const struct pr *p = &parts[i]; char k[96];
    HTHEME t; SetLastError(0xdeadbeef);
    t = U(OpenThemeData)(hwnd, p->cls);
    sprintf(k, "OpenThemeData(%s) lasterr", p->cn);
    if (i == 0 || strcmp(parts[i-1].cn, p->cn)) P(k, "%s %s", t ? "non-null" : "NULL", hr(GetLastError()));
    if (!t) continue;
    {
      SIZE s; HRESULT h; int v; RECT r; MARGINS m; int bg = -1, ix;
      int ts; static const char *tsn[] = {"MIN","TRUE","DRAW"};
      for (ts = 0; ts < 3; ts++) {
        s.cx = s.cy = -7;
        h = U(GetThemePartSize)(t, hdc, p->part, p->state, NULL, (THEMESIZE)ts, &s);
        sprintf(k, "PartSize[%s][%s]", p->pn, tsn[ts]);
        P(k, "hr=%s cx=%ld cy=%ld", hr(h), s.cx, s.cy);
      }
      h = U(GetThemeEnumValue)(t, p->part, p->state, TMT_BGTYPE, &bg);
      sprintf(k, "Enum TMT_BGTYPE[%s]", p->pn); P(k, "hr=%s v=%d", hr(h), bg);
      h = U(GetThemeEnumValue)(t, p->part, p->state, TMT_IMAGELAYOUT, &bg);
      sprintf(k, "Enum TMT_IMAGELAYOUT[%s]", p->pn); P(k, "hr=%s v=%d", hr(h), bg);
      v = -9; h = U(GetThemeInt)(t, p->part, p->state, TMT_BORDERSIZE, &v);
      sprintf(k, "Int TMT_BORDERSIZE[%s]", p->pn); P(k, "hr=%s v=%d", hr(h), v);
      v = -9; h = U(GetThemeInt)(t, p->part, p->state, TMT_IMAGECOUNT, &v);
      sprintf(k, "Int TMT_IMAGECOUNT[%s]", p->pn); P(k, "hr=%s v=%d", hr(h), v);
      v = -9; h = U(GetThemeInt)(t, p->part, p->state, TMT_MINDPI1, &v);
      sprintf(k, "Int TMT_MINDPI1[%s]", p->pn); P(k, "hr=%s v=%d", hr(h), v);
      v = -9; h = U(GetThemeMetric)(t, hdc, p->part, p->state, TMT_HEIGHT, &v);
      sprintf(k, "Metric TMT_HEIGHT[%s]", p->pn); P(k, "hr=%s v=%d", hr(h), v);
      v = -9; h = U(GetThemeMetric)(t, hdc, p->part, p->state, TMT_WIDTH, &v);
      sprintf(k, "Metric TMT_WIDTH[%s]", p->pn); P(k, "hr=%s v=%d", hr(h), v);
      memset(&m, 0xee, sizeof m); h = U(GetThemeMargins)(t, hdc, p->part, p->state, TMT_CONTENTMARGINS, NULL, &m);
      sprintf(k, "Margins TMT_CONTENTMARGINS[%s]", p->pn); P(k, "hr=%s l=%d r=%d t=%d b=%d", hr(h), m.cxLeftWidth, m.cxRightWidth, m.cyTopHeight, m.cyBottomHeight);
      memset(&m, 0xee, sizeof m); h = U(GetThemeMargins)(t, hdc, p->part, p->state, TMT_SIZINGMARGINS, NULL, &m);
      sprintf(k, "Margins TMT_SIZINGMARGINS[%s]", p->pn); P(k, "hr=%s l=%d r=%d t=%d b=%d", hr(h), m.cxLeftWidth, m.cxRightWidth, m.cyTopHeight, m.cyBottomHeight);
      { WCHAR fn[260] = L""; h = U(GetThemeFilename)(t, p->part, p->state, TMT_IMAGEFILE, fn, 260);
        sprintf(k, "Filename TMT_IMAGEFILE[%s]", p->pn); P(k, "hr=%s \"%ls\"", hr(h), fn); }
      { RECT in = {0,0,100,50}; memset(&r, 0xee, sizeof r);
        h = U(GetThemeBackgroundContentRect)(t, hdc, p->part, p->state, &in, &r);
        sprintf(k, "ContentRect(0,0,100,50)[%s]", p->pn); P(k, "hr=%s %ld,%ld,%ld,%ld", hr(h), r.left, r.top, r.right, r.bottom); }
      sprintf(k, "IsThemePartDefined[%s]", p->pn); P(k, "%d", U(IsThemePartDefined)(t, p->part, p->state));
    }
    Close(t);
  }
}

/* Exactly what Win10 comctl32 _Header_InitTheme (decomp) asks: GetThemePartSize(theme,NULL,part,0,NULL,TS_TRUE) for
 * parts 1,5,4,7 and GetThemePosition(theme,1,0,0xd4b=TMT_MINSIZE). */
static void probe_header_inittheme(HWND hwnd)
{
  HTHEME t = U(OpenThemeData)(hwnd, L"Header"); static const int ps[] = {1,5,4,7}; int i; char k[64];
  P("InitTheme OpenThemeData(Header)", "%s", t ? "non-null" : "NULL");
  if (!t) return;
  for (i = 0; i < 4; i++) { SIZE s = {-7,-7}; HRESULT h = U(GetThemePartSize)(t, NULL, ps[i], 0, NULL, TS_TRUE, &s); sprintf(k, "InitTheme PartSize(hdc=NULL,part=%d,state=0,TS_TRUE)", ps[i]); P(k, "hr=%s cx=%ld cy=%ld", hr(h), s.cx, s.cy); }
  { POINT pt = {-7,-7}; HRESULT h = U(GetThemePosition)(t, 1, 0, 0xd4b, &pt); P("InitTheme GetThemePosition(1,0,TMT_MINSIZE=0xd4b)", "hr=%s x=%ld y=%ld", hr(h), pt.x, pt.y); }
  U(CloseThemeData)(t);
}

static void probe_sys(HWND hwnd)
{
  int i; char k[64];
  HTHEME t = U(OpenThemeData)(hwnd, L"Window");
  static const int szs[] = { SM_CXVSCROLL, SM_CYHSCROLL, SM_CXSIZE, SM_CYSIZE, SM_CXFRAME, SM_CYFRAME, SM_CXSMSIZE, SM_CYSMSIZE, SM_CXMENUSIZE, SM_CYMENUSIZE };
  P("SysProbe theme", "%s", t ? "non-null" : "NULL");
  for (i = 0; i < 10; i++) { int v; SetLastError(0); v = U(GetThemeSysSize)(t, szs[i]);
    sprintf(k, "GetThemeSysSize[SM %d]", szs[i]); P(k, "%d (GetSystemMetrics=%d, lasterr=%lu)", v, GetSystemMetrics(szs[i]), GetLastError()); }
  { int v = -9; HRESULT h = U(GetThemeSysInt)(t, TMT_MINCOLORDEPTH, &v); P("GetThemeSysInt[TMT_MINCOLORDEPTH]", "hr=%s v=%d", hr(h), v); }
  { int v = -9; HRESULT h = U(GetThemeSysInt)(t, TMT_SCROLLBARWIDTH, &v); P("GetThemeSysInt[TMT_SCROLLBARWIDTH]", "hr=%s v=%d", hr(h), v); }
  P("GetThemeSysBool[TMT_FLATMENUS]", "%d", U(GetThemeSysBool)(t, TMT_FLATMENUS));
  for (i = 0; i < 31; i++) { COLORREF c = U(GetThemeSysColor)(t, i);
    sprintf(k, "GetThemeSysColor[%d]", i); P(k, "0x%06lx (GetSysColor=0x%06lx)", (unsigned long)c, (unsigned long)GetSysColor(i)); }
  { LOGFONTW lf; HRESULT h; memset(&lf,0,sizeof lf); h = U(GetThemeSysFont)(t, TMT_CAPTIONFONT, &lf); P("GetThemeSysFont[CAPTION]", "hr=%s h=%ld w=%ld face=%ls", hr(h), lf.lfHeight, lf.lfWeight, lf.lfFaceName); }
  { LOGFONTW lf; HRESULT h; memset(&lf,0,sizeof lf); h = U(GetThemeSysFont)(t, TMT_MSGBOXFONT, &lf); P("GetThemeSysFont[MSGBOX]", "hr=%s h=%ld w=%ld face=%ls", hr(h), lf.lfHeight, lf.lfWeight, lf.lfFaceName); }
  if (t) U(CloseThemeData)(t);
}

static void probe_header(const char *tag, DWORD style, BOOL withText, BOOL setfont, BOOL withImg)
{
  HWND par, h; RECT r, wr; HDLAYOUT hl; WINDOWPOS wp; HDITEMW it; char k[96]; int cy;
  par = CreateWindowExW(0, L"STATIC", L"p", WS_OVERLAPPEDWINDOW, 0, 0, 400, 200, NULL, NULL, NULL, NULL);
  if (!par) { P("Header", "parent create failed %lu", GetLastError()); return; }
  h = CreateWindowExW(0, WC_HEADERW, NULL, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, par, (HMENU)1, GetModuleHandleW(NULL), NULL);
  if (!h) { sprintf(k, "Header[%s]", tag); P(k, "CreateWindow failed %lu", GetLastError()); return; }
  if (setfont) SendMessageW(h, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
  memset(&it, 0, sizeof it); it.mask = HDI_WIDTH | (withText ? HDI_TEXT : 0) | HDI_FORMAT; it.cxy = 100; it.pszText = (WCHAR*)L"Name"; it.cchTextMax = 4; it.fmt = HDF_LEFT | (withText ? HDF_STRING : 0);
  SendMessageW(h, HDM_INSERTITEMW, 0, (LPARAM)&it);
  GetClientRect(par, &r); hl.prc = &r; hl.pwpos = &wp; memset(&wp, 0, sizeof wp);
  SendMessageW(h, HDM_LAYOUT, 0, (LPARAM)&hl);
  SetWindowPos(h, wp.hwndInsertAfter, wp.x, wp.y, wp.cx, wp.cy, wp.flags | SWP_SHOWWINDOW);
  GetWindowRect(h, &wr);
  sprintf(k, "Header[%s]", tag);
  P(k, "HDM_LAYOUT wpos.cy=%d wpos.cx=%d  GetWindowRect height=%ld  GetWindowTheme=%s", wp.cy, wp.cx, wr.bottom - wr.top, U(GetWindowTheme)(h) ? "set" : "NULL");
  DestroyWindow(par);
}

int main(int argc, char **argv)
{
  HWND w; HDC dc; WCHAR nm[260] = L"", col[64] = L"", sz[64] = L"";
  INITCOMMONCONTROLSEX ic = { sizeof ic, ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
  HMODULE hc;
  hUx = LoadLibraryW(L"uxtheme.dll");
  if (!hUx) { puts("no uxtheme"); return 1; }
  InitCommonControlsEx(&ic);
  P("wine_get_version", "%s", GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version") ? "present" : "absent");
  { typedef const char*(CDECL*gv)(void); gv v = (gv)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version"); if (v) P("wine version", "%s", v()); }
  { OSVERSIONINFOEXW vi = { sizeof vi }; typedef LONG(WINAPI*rv)(OSVERSIONINFOEXW*); rv f = (rv)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"); if (f) { f(&vi); P("RtlGetVersion", "%lu.%lu.%lu", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber); } }
  hc = GetModuleHandleW(L"comctl32.dll");
  { WCHAR path[260]; GetModuleFileNameW(hc, path, 260); P("comctl32 loaded from", "%ls", path); }
  { DLLVERSIONINFO vi = { sizeof vi }; HRESULT (CALLBACK *dv)(DLLVERSIONINFO*) = (void*)GetProcAddress(hc, "DllGetVersion"); if (dv) { dv(&vi); P("comctl32 DllGetVersion", "%lu.%lu.%lu", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber); } }
  P("IsThemeActive", "%d", U(IsThemeActive)());
  P("IsAppThemed", "%d", U(IsAppThemed)());
  P("IsCompositionActive", "%d", U(IsCompositionActive) ? U(IsCompositionActive)() : -1);
  { HRESULT h = U(GetCurrentThemeName)(nm, 260, col, 64, sz, 64); P("GetCurrentThemeName", "hr=%s name=\"%ls\" color=\"%ls\" size=\"%ls\"", hr(h), nm, col, sz); }
  P("GetSystemMetrics SM_CYCAPTION/SM_CXVSCROLL/SM_CYMENU", "%d/%d/%d", GetSystemMetrics(SM_CYCAPTION), GetSystemMetrics(SM_CXVSCROLL), GetSystemMetrics(SM_CYMENU));
  w = CreateWindowExW(0, L"STATIC", L"probe", WS_OVERLAPPEDWINDOW, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
  dc = GetDC(w);
  P("LOGPIXELSY", "%d", GetDeviceCaps(dc, LOGPIXELSY));
  { TEXTMETRICW tm; HFONT of = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT)); GetTextMetricsW(dc, &tm); SelectObject(dc, of); P("DEFAULT_GUI_FONT tmHeight", "%ld", tm.tmHeight); }
  { WCHAR cls[64]; HTHEME t; int i; static const WCHAR *cl[] = { L"Header", L"Button", L"Tab", L"ListView", L"Window", L"NoSuchClass", L"Header::Foo" };
    for (i = 0; i < 7; i++) { SetLastError(0xdeadbeef); t = U(OpenThemeData)(w, cl[i]); P("OpenThemeData class", "%ls -> %s lasterr=%s", cl[i], t ? "non-null" : "NULL", hr(GetLastError())); if (t) U(CloseThemeData)(t); }
    SetLastError(0xdeadbeef); t = U(OpenThemeData)(w, NULL); P("OpenThemeData(NULL class)", "%s lasterr=%s", t ? "non-null" : "NULL", hr(GetLastError())); (void)cls; }
  { HTHEME t = U(OpenThemeData)(w, L"Header"); SIZE s = {-7,-7}; HRESULT h;
    h = U(GetThemePartSize)(NULL, dc, HP_HEADERITEM, 1, NULL, TS_TRUE, &s); P("GetThemePartSize(NULL htheme)", "hr=%s cx=%ld cy=%ld", hr(h), s.cx, s.cy);
    if (t) U(CloseThemeData)(t); }
  probe_header_inittheme(w);
  probe_parts(w, dc);
  probe_sys(w);
  probe_header("HDS_BUTTONS|HORZ text, default font", HDS_BUTTONS | HDS_HORZ, TRUE, FALSE, FALSE);
  probe_header("HDS_BUTTONS|HORZ text, DEFAULT_GUI_FONT", HDS_BUTTONS | HDS_HORZ, TRUE, TRUE, FALSE);
  probe_header("HDS_HORZ (no buttons) text, DEFAULT_GUI_FONT", HDS_HORZ, TRUE, TRUE, FALSE);
  probe_header("HDS_BUTTONS|HORZ no text, DEFAULT_GUI_FONT", HDS_BUTTONS | HDS_HORZ, FALSE, TRUE, FALSE);
  ReleaseDC(w, dc);
  return 0;
}
