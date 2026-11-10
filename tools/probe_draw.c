/* probe_draw.c - records DrawThemeBackground / DrawThemeText output of the real Win10 uxtheme into small DIBs.
 *   probe_draw.exe [--out FILE|-] [--label NAME]
 * For every class in the list, every part 0..60 with IsThemePartDefined(part,s)==TRUE for some s 0..24 is drawn for states 0..16 into a
 * 32bpp top-down DIB (rect sizes 48x24 and 16x16) pre-filled with a sentinel colour.  One row per draw:
 *   class,part,state,w,h,hr,changed,bbox_l,bbox_t,bbox_r,bbox_b,grey_fnv1a,rgb_fnv1a,alpha_nonzero
 * grey_fnv1a hashes the greyscale byte (0.30R+0.59G+0.11B) of every pixel; changed = pixels != fill.
 * Key parts (table KEY below) additionally get a raw 24x24 RGB dump (hex, bmp.csv) and a DrawThemeText hash.
 * Plain C, mingw-w64; uxtheme is loaded dynamically.  No state is changed except a hidden window's sub-app.
 */
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef HTHEME (WINAPI *fOpen)(HWND, LPCWSTR);
typedef HRESULT (WINAPI *fClose)(HTHEME);
typedef BOOL (WINAPI *fDefined)(HTHEME, int, int);
typedef HRESULT (WINAPI *fDraw)(HTHEME, HDC, int, int, LPCRECT, LPCRECT);
typedef HRESULT (WINAPI *fDrawText)(HTHEME, HDC, int, int, LPCWSTR, int, DWORD, DWORD, LPCRECT);
typedef BOOL (WINAPI *fBool0)(void);

static const char *CLASSES[] = {
 "Header","Button","Tab","ListView","TreeView","Toolbar","Rebar","Status","Progress","TrackBar","Spin","ComboBox","Edit",
 "ScrollBar","Menu","ToolTip","Window","Listbox","MonthCal","DatePicker","Static","TaskDialog","Link","TextStyle",
 "Explorer::ListView","Explorer::TreeView","Explorer::ScrollBar","DarkMode_Explorer::ListView","DarkMode_Explorer::TreeView",
 "DarkMode_Explorer::ScrollBar","DarkMode_Explorer::Button","DarkMode_Explorer::Edit","DarkMode_Explorer::ComboBox",
 "DarkMode_CFD::Button","DarkMode_CFD::Edit","DarkMode_CFD::ComboBox","DarkMode_ItemsView::ListView","DarkMode_ItemsView::Header",
 "DarkMode_Explorer::Header","DarkMode_Explorer::Tab","DarkMode_Explorer::ToolTip","DarkMode_Explorer::Toolbar","DarkMode_Explorer::Menu",
 "CommandModule","Navigation","BreadcrumbBar","SearchBox","ExplorerBar","StartPanel","Taskband","Flyout","ItemsView","ControlPanel","Page",
 "AeroWizard","Clock","TrayNotify","Display","EmptyMarkup","HighContrast","Globals",
 NULL };
typedef struct { const char *cls; int part, state; } Key;
static const Key KEY[] = {
 {"Header",1,1},{"Header",1,2},{"Header",1,3},{"Header",4,1},{"Header",4,2},
 {"Button",1,1},{"Button",1,2},{"Button",1,3},{"Button",1,4},{"Button",1,5},{"Button",2,1},{"Button",2,5},{"Button",3,1},{"Button",3,5},{"Button",3,9},{"Button",4,1},
 {"Tab",1,1},{"Tab",1,2},{"Tab",1,3},{"Tab",9,0},{"Tab",10,0},
 {"ListView",1,1},{"ListView",1,2},{"ListView",1,3},{"TreeView",2,1},{"TreeView",2,2},{"TreeView",1,3},
 {"Toolbar",1,1},{"Toolbar",1,2},{"Toolbar",1,3},{"Rebar",4,1},{"Progress",1,0},{"Progress",5,1},{"TrackBar",1,1},{"TrackBar",3,1},
 {"Spin",1,1},{"Spin",2,1},{"ComboBox",1,1},{"ComboBox",2,1},{"Edit",1,1},{"Edit",6,1},{"ScrollBar",1,1},{"ScrollBar",2,1},{"ScrollBar",3,1},{"ScrollBar",4,1},{"ScrollBar",5,1},{"ScrollBar",6,1},
 {"Window",1,1},{"Window",1,2},{"Window",7,1},{"Window",13,1},{"Window",14,1},{"Window",15,1},{"Window",18,1},{"Window",19,1},{"Window",20,1},
 {"Menu",1,1},{"Menu",8,1},{"Menu",12,1},{"ToolTip",1,1},{"Status",1,0},{"Status",2,0},
 {NULL,0,0}};

static fOpen Open; static fClose Close; static fDefined Defined; static fDraw Draw; static fDrawText DrawTxt;
static FILE *fo, *fb;
static const unsigned FILL = 0x00FF00FFu;   /* magenta-ish sentinel, alpha 0 */

static void fnv(unsigned *h, unsigned char b) { *h ^= b; *h *= 16777619u; }
static const wchar_t *Wc(const char *s) { static wchar_t b[256]; MultiByteToWideChar(CP_UTF8, 0, s, -1, b, 256); return b; }

static void one(HTHEME th, const char *cls, int part, int state, int w, int h, HDC dc, unsigned *px, HBITMAP bm, int key)
{
    int i, n = w * h; RECT rc = {0, 0, w, h}; HRESULT hr; unsigned gh = 2166136261u, rh = 2166136261u; int changed = 0, an = 0, l = w, t = h, r = -1, b = -1;
    for (i = 0; i < n; i++) px[i] = FILL;
    hr = Draw(th, dc, part, state, &rc, NULL); GdiFlush();
    for (i = 0; i < n; i++) {
        unsigned p = px[i]; int R = (p >> 16) & 255, G = (p >> 8) & 255, B = p & 255; int g = (R * 30 + G * 59 + B * 11) / 100;
        fnv(&gh, (unsigned char)g); fnv(&rh, R); fnv(&rh, G); fnv(&rh, B); if (p >> 24) an++;
        if ((p & 0xFFFFFF) != (FILL & 0xFFFFFF)) { int x = i % w, y = i / w; changed++; if (x < l) l = x; if (x > r) r = x; if (y < t) t = y; if (y > b) b = y; }
    }
    if (!changed) { l = t = r = b = -1; }
    fprintf(fo, "%s,%d,%d,%d,%d,0x%08lX,%d,%d,%d,%d,%d,%08x,%08x,%d\n", cls, part, state, w, h, (unsigned long)hr, changed, l, t, r, b, gh, rh, an);
    if (key && w == 24 && h == 24 && fb) {
        fprintf(fb, "BMP,%s,%d,%d,0x%08lX,", cls, part, state, (unsigned long)hr);
        for (i = 0; i < n; i++) fprintf(fb, "%06x", px[i] & 0xFFFFFF);
        fprintf(fb, "\n");
    }
}

int main(int argc, char **argv)
{
    const char *outp = "-", *bmpp = "-", *label = "unlabelled"; int a, c, p, s, sz;
    HMODULE ux; fBool0 act; HDC sdc, mdc; BITMAPINFO bi; void *bits; HBITMAP bm; unsigned *px;
    for (a = 1; a < argc - 1; a++) { if (!strcmp(argv[a], "--out")) outp = argv[++a]; else if (!strcmp(argv[a], "--bitmaps")) bmpp = argv[++a]; else if (!strcmp(argv[a], "--label")) label = argv[++a]; }
    SetProcessDPIAware();
    ux = LoadLibraryA("uxtheme.dll"); if (!ux) return 2;
    Open = (fOpen)GetProcAddress(ux, "OpenThemeData"); Close = (fClose)GetProcAddress(ux, "CloseThemeData");
    Defined = (fDefined)GetProcAddress(ux, "IsThemePartDefined"); Draw = (fDraw)GetProcAddress(ux, "DrawThemeBackground");
    DrawTxt = (fDrawText)GetProcAddress(ux, "DrawThemeText"); act = (fBool0)GetProcAddress(ux, "IsThemeActive");
    if (!Open || !Draw || !Defined) return 2;
    setvbuf(stdout, NULL, _IOFBF, 1 << 16); fo = strcmp(outp, "-") ? fopen(outp, "wb") : stdout; fb = fo; (void)bmpp; if (!fo) return 2;
    fprintf(fo, "class,part,state,w,h,hr,changed,bbox_l,bbox_t,bbox_r,bbox_b,grey_fnv1a,rgb_fnv1a,alpha_nonzero\n");
    fprintf(fo, "#label=%s themeactive=%d\n", label, act ? act() : -1);
    if (fb) fprintf(fb, "BMPHEADER,class,part,state,hr,rgb_hex_24x24_row_major\n");
    sdc = GetDC(NULL); mdc = CreateCompatibleDC(sdc);
    for (sz = 0; sz < 2; sz++) {
        int w = sz ? 16 : 48, h = sz ? 16 : 24;
        memset(&bi, 0, sizeof bi); bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
        bm = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0); px = (unsigned *)bits; SelectObject(mdc, bm);
        for (c = 0; CLASSES[c]; c++) {
            HTHEME th = Open(NULL, Wc(CLASSES[c]));
            if (!th) { if (!sz) fprintf(fo, "%s,-1,-1,0,0,OPENFAIL,0,0,0,0,0,0,0,0\n", CLASSES[c]); continue; }
            for (p = 0; p <= 60; p++) { int any = 0; for (s = 0; s <= 24; s++) if (Defined(th, p, s)) any = 1;
                if (any) for (s = 0; s <= 16; s++) one(th, CLASSES[c], p, s, w, h, mdc, px, bm, 0); }
            Close(th);
        }
        DeleteObject(bm);
    }
    /* key parts: 24x24 raw dump (+ a 24x24 hash row) */
    memset(&bi, 0, sizeof bi); bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 24; bi.bmiHeader.biHeight = -24; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    bm = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0); px = (unsigned *)bits; SelectObject(mdc, bm);
    for (c = 0; KEY[c].cls; c++) {
        HTHEME th = Open(NULL, Wc(KEY[c].cls)); if (!th) continue;
        one(th, KEY[c].cls, KEY[c].part, KEY[c].state, 24, 24, mdc, px, bm, 1);
        if (DrawTxt) {   /* text hash (greyscale) in a 48x24 DIB is not possible with this 24x24 DIB: use the same bitmap, short text */
            RECT rc = {0, 0, 24, 24}; int i; unsigned gh = 2166136261u, ch = 0; HRESULT hr;
            for (i = 0; i < 576; i++) px[i] = FILL;
            SetBkMode(mdc, TRANSPARENT);
            hr = DrawTxt(th, mdc, KEY[c].part, KEY[c].state, L"Ag", 2, DT_SINGLELINE | DT_VCENTER | DT_CENTER, 0, &rc); GdiFlush();
            for (i = 0; i < 576; i++) { unsigned q = px[i]; int g = (((q >> 16) & 255) * 30 + ((q >> 8) & 255) * 59 + (q & 255) * 11) / 100; fnv(&gh, (unsigned char)g); if ((q & 0xFFFFFF) != (FILL & 0xFFFFFF)) ch++; }
            fprintf(fo, "TEXT:%s,%d,%d,24,24,0x%08lX,%u,0,0,0,0,%08x,00000000,0\n", KEY[c].cls, KEY[c].part, KEY[c].state, (unsigned long)hr, ch, gh);
        }
        Close(th);
    }
    fflush(fo); if (fo != stdout) fclose(fo); return 0;
}
