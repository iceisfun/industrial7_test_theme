/* themeenv.c - one-shot report of the theme environment a process sees (tools/vm_run.sh, docs/VM_THEME_ENV.md).
 *
 *   themeenv.exe [--wait SEC] [--actctx MANIFEST] [--quiet]
 *
 * --wait SEC   poll IsThemeActive() every 250 ms for up to SEC seconds before reporting; exit 0 when the theme
 *              is active at the end, 1 when not (vm_run.sh uses this as its "themed session ready" gate).
 * --actctx M   additionally activate the manifest file M (comctl32 v6 dependency) and report the comctl32 module
 *              loaded under that context and the header height there.
 * Output lines:  ENV key=value   (one per item, easy to grep).  Built twice by tools/vm/build.sh:
 *   themeenv.exe   no comctl32 dependency in its embedded manifest (process default = comctl32 v5)
 *   themeenv6.exe  embedded tools/probe_ctl6.manifest-style dependency (process default = comctl32 v6)
 * uxtheme/comctl32 are loaded with LoadLibrary/GetProcAddress, nothing theme-related is linked.
 */
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { DWORD cbSize, dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformID; } DLLVER;
static BOOL (WINAPI *pIsThemeActive)(void), (WINAPI *pIsAppThemed)(void), (WINAPI *pIsCompositionActive)(void);
static DWORD (WINAPI *pGetThemeAppProperties)(void);
static HRESULT (WINAPI *pGetCurrentThemeName)(LPWSTR, int, LPWSTR, int, LPWSTR, int);
static HTHEME (WINAPI *pOpenThemeData)(HWND, LPCWSTR);
static HRESULT (WINAPI *pCloseThemeData)(HTHEME);
static HRESULT (WINAPI *pGetThemePartSize)(HTHEME, HDC, int, int, LPCRECT, THEMESIZE, SIZE *);
static HRESULT (WINAPI *pGetThemePosition)(HTHEME, int, int, int, POINT *);
static HTHEME (WINAPI *pGetWindowTheme)(HWND);

static void P(const char *k, const char *fmt, ...) { va_list a; printf("ENV %s=", k); va_start(a, fmt); vprintf(fmt, a); va_end(a); printf("\n"); }
static const char *A(const wchar_t *w) { static char b[4][520]; static int i; char *o = b[i = (i + 1) & 3]; WideCharToMultiByte(CP_UTF8, 0, w, -1, o, 520, NULL, NULL); return o; }

static const char *regs(HKEY root, const char *key, const char *val)
{
    static char b[8][300]; static int i; char *o = b[i = (i + 1) & 7]; DWORD type = 0, sz = sizeof b[0] - 1; HKEY k; BYTE d[300];
    if (RegOpenKeyExA(root, key, 0, KEY_READ | KEY_WOW64_64KEY, &k)) return "(nokey)";
    memset(d, 0, sizeof d);
    if (RegQueryValueExA(k, val, NULL, &type, d, &sz)) { RegCloseKey(k); return "(absent)"; }
    RegCloseKey(k);
    if (type == REG_DWORD) sprintf(o, "0x%lX", *(DWORD *)d);
    else if (type == REG_SZ || type == REG_EXPAND_SZ) snprintf(o, 300, "\"%s\"", (char *)d);
    else if (type == REG_BINARY) { int n = 0, j; for (j = 0; j < (int)sz && j < 24; j++) n += sprintf(o + n, "%02X", d[j]); if (!sz) strcpy(o, "bin:"); }
    else sprintf(o, "type%lu", type);
    return o;
}

static const char *fver(const wchar_t *path)
{
    static char b[64]; DWORD h, n; BYTE *d; VS_FIXEDFILEINFO *fi; UINT l; strcpy(b, "?");
    HMODULE m = LoadLibraryA("version.dll"); if (!m) return b;
    DWORD (WINAPI *gsz)(LPCWSTR, LPDWORD) = (void *)GetProcAddress(m, "GetFileVersionInfoSizeW");
    BOOL (WINAPI *gi)(LPCWSTR, DWORD, DWORD, LPVOID) = (void *)GetProcAddress(m, "GetFileVersionInfoW");
    BOOL (WINAPI *vq)(LPCVOID, LPCWSTR, LPVOID *, PUINT) = (void *)GetProcAddress(m, "VerQueryValueW");
    if (!gsz || !gi || !vq || !(n = gsz(path, &h)) || !(d = malloc(n))) return b;
    if (gi(path, 0, n, d) && vq(d, L"\\", (LPVOID *)&fi, &l) && fi)
        sprintf(b, "%u.%u.%u.%u", HIWORD(fi->dwFileVersionMS), LOWORD(fi->dwFileVersionMS), HIWORD(fi->dwFileVersionLS), LOWORD(fi->dwFileVersionLS));
    free(d); return b;
}

static void comctl_report(const char *tag)
{
    char k[96]; WCHAR p[MAX_PATH] = L""; HMODULE m = LoadLibraryA("comctl32.dll"); DLLVER v; HRESULT (WINAPI *gv)(DLLVER *);
    if (!m) { snprintf(k, sizeof k, "%s.comctl32", tag); P(k, "LOADFAIL err=%lu", GetLastError()); return; }
    GetModuleFileNameW(m, p, MAX_PATH);
    gv = (void *)GetProcAddress(m, "DllGetVersion"); memset(&v, 0, sizeof v); v.cbSize = sizeof v;
    HRESULT hr = gv ? gv(&v) : E_NOTIMPL;
    snprintf(k, sizeof k, "%s.comctl32.path", tag); P(k, "%s", A(p));
    snprintf(k, sizeof k, "%s.comctl32.filever", tag); P(k, "%s", fver(p));
    snprintf(k, sizeof k, "%s.comctl32.DllGetVersion", tag); P(k, "hr=0x%08lX %lu.%lu.%lu", hr, v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
    BOOL (WINAPI *icc)(const INITCOMMONCONTROLSEX *) = (void *)GetProcAddress(m, "InitCommonControlsEx");
    INITCOMMONCONTROLSEX ic = { sizeof ic, ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
    if (icc) icc(&ic);
}

/* header height via HDM_LAYOUT, plus the themed window handle of the control */
static void header_report(const char *tag, HWND parent)
{
    char k[96]; RECT rc = {0, 0, 400, 300}; WINDOWPOS wp; HDLAYOUT hl = {&rc, &wp}; RECT wr;
    HWND h = CreateWindowExW(0, L"SysHeader32", L"", WS_CHILD | WS_VISIBLE | HDS_BUTTONS | HDS_HORZ, 0, 0, 400, 30, parent, NULL, GetModuleHandleW(NULL), NULL);
    snprintf(k, sizeof k, "%s.header", tag);
    if (!h) { P(k, "CREATEFAIL err=%lu", GetLastError()); return; }
    SendMessageW(h, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
    memset(&wp, 0, sizeof wp);
    LRESULT r = SendMessageW(h, HDM_LAYOUT, 0, (LPARAM)&hl);
    GetWindowRect(h, &wr);
    HTHEME th = pGetWindowTheme ? pGetWindowTheme(h) : NULL;
    P(k, "layout_ret=%ld cy=%d cx=%d window_theme=%s", (long)r, wp.cy, wp.cx, th ? "nonnull" : "NULL");
    { HFONT f = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI");
      SendMessageW(h, WM_SETFONT, (WPARAM)f, 0); rc.top = 0; memset(&wp, 0, sizeof wp); SendMessageW(h, HDM_LAYOUT, 0, (LPARAM)&hl);
      snprintf(k, sizeof k, "%s.header.segoe9", tag); P(k, "cy=%d", wp.cy); }
    DestroyWindow(h);
}

static void theme_report(void)
{
    WCHAR f[MAX_PATH] = L"", c[128] = L"", s[128] = L"";
    P("IsThemeActive", "%d", pIsThemeActive ? pIsThemeActive() : -1);
    P("IsAppThemed", "%d", pIsAppThemed ? pIsAppThemed() : -1);
    P("IsCompositionActive", "%d", pIsCompositionActive ? pIsCompositionActive() : -1);
    P("GetThemeAppProperties", "0x%lX", pGetThemeAppProperties ? pGetThemeAppProperties() : 0xFFFFFFFF);
    HRESULT hr = pGetCurrentThemeName ? pGetCurrentThemeName(f, MAX_PATH, c, 128, s, 128) : E_NOTIMPL;
    P("GetCurrentThemeName", "hr=0x%08lX file=\"%s\" color=\"%s\" size=\"%s\"", hr, A(f), A(c), A(s));
    { HMODULE ux = GetModuleHandleA("uxtheme.dll");
      BOOLEAN (WINAPI *o132)(void) = (void *)GetProcAddress(ux, (LPCSTR)132), (WINAPI *o138)(void) = (void *)GetProcAddress(ux, (LPCSTR)138);
      P("ShouldAppsUseDarkMode.132", "%d", o132 ? o132() : -1);
      P("ShouldSystemUseDarkMode.138", "%d", o138 ? o138() : -1); }
    HTHEME t = pOpenThemeData ? pOpenThemeData(NULL, L"Header") : NULL;
    P("OpenThemeData.Header", "%s err=%lu", t ? "nonnull" : "NULL", GetLastError());
    if (t) {
        SIZE sz = {-1, -1}; POINT pt = {-1, -1};
        HDC dc = GetDC(NULL);
        HRESULT h1 = pGetThemePartSize(t, NULL, HP_HEADERITEM, HIS_NORMAL, NULL, TS_TRUE, &sz);
        P("HP_HEADERITEM.TS_TRUE.nulldc", "hr=0x%08lX cx=%ld cy=%ld", h1, sz.cx, sz.cy);
        sz.cx = sz.cy = -1; h1 = pGetThemePartSize(t, dc, HP_HEADERITEM, HIS_NORMAL, NULL, TS_TRUE, &sz);
        P("HP_HEADERITEM.TS_TRUE.screendc", "hr=0x%08lX cx=%ld cy=%ld", h1, sz.cx, sz.cy);
        sz.cx = sz.cy = -1; h1 = pGetThemePartSize(t, NULL, HP_HEADERITEM, 0, NULL, TS_TRUE, &sz);
        P("HP_HEADERITEM.state0.TS_TRUE", "hr=0x%08lX cx=%ld cy=%ld", h1, sz.cx, sz.cy);
        h1 = pGetThemePosition(t, HP_HEADERITEM, 0, TMT_MINSIZE, &pt);
        P("HP_HEADERITEM.TMT_MINSIZE", "hr=0x%08lX x=%ld y=%ld", h1, pt.x, pt.y);
        ReleaseDC(NULL, dc); pCloseThemeData(t);
    }
}

static void session_report(void)
{
    DWORD sid = 0; char n[256]; DWORD l;
    ProcessIdToSessionId(GetCurrentProcessId(), &sid); P("SessionId", "%lu", sid);
    P("ActiveConsoleSessionId", "%lu", WTSGetActiveConsoleSessionId());
    n[0] = 0; GetUserObjectInformationA(GetProcessWindowStation(), UOI_NAME, n, sizeof n, &l); P("WindowStation", "%s", n);
    n[0] = 0; GetUserObjectInformationA(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME, n, sizeof n, &l); P("ThreadDesktop", "%s", n);
    HDESK in = OpenInputDesktop(0, FALSE, GENERIC_READ); n[0] = 0;
    if (in) { GetUserObjectInformationA(in, UOI_NAME, n, sizeof n, &l); CloseDesktop(in); P("InputDesktop", "%s", n); } else P("InputDesktop", "OPENFAIL err=%lu", GetLastError());
    l = sizeof n; n[0] = 0; GetUserNameA(n, &l); P("UserName", "%s", n);
    P("UptimeMs", "%lu", GetTickCount());
    { HANDLE tk; TOKEN_ELEVATION el = {0}; TOKEN_ELEVATION_TYPE et = 0; DWORD r;
      if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tk)) {
        GetTokenInformation(tk, TokenElevation, &el, sizeof el, &r); GetTokenInformation(tk, TokenElevationType, &et, sizeof et, &r);
        P("TokenElevation", "elevated=%lu type=%d (1=default 2=full 3=limited)", el.TokenIsElevated, (int)et); CloseHandle(tk); } }
    /* explorer / dwm / winlogon presence */
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32 pe = { sizeof pe };
    int ex = 0, dwm = 0, logonui = 0;
    if (snap != INVALID_HANDLE_VALUE) { if (Process32First(snap, &pe)) do {
        if (!_stricmp(pe.szExeFile, "explorer.exe")) ex++; else if (!_stricmp(pe.szExeFile, "dwm.exe")) dwm++; else if (!_stricmp(pe.szExeFile, "LogonUI.exe")) logonui++;
    } while (Process32Next(snap, &pe)); CloseHandle(snap); }
    P("Processes", "explorer=%d dwm=%d logonui=%d", ex, dwm, logonui);
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT), sv = scm ? OpenServiceA(scm, "Themes", SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG) : NULL;
    SERVICE_STATUS ss; memset(&ss, 0, sizeof ss);
    if (sv && QueryServiceStatus(sv, &ss)) P("Service.Themes", "state=%lu (4=running 1=stopped)", ss.dwCurrentState); else P("Service.Themes", "QUERYFAIL err=%lu", GetLastError());
    if (sv) CloseServiceHandle(sv);
    if (scm) CloseServiceHandle(scm);
}

static void display_report(void)
{
    HDC dc = GetDC(NULL);
    P("LOGPIXELSX", "%d", GetDeviceCaps(dc, LOGPIXELSX));
    UINT (WINAPI *gds)(void) = (void *)GetProcAddress(GetModuleHandleA("user32.dll"), "GetDpiForSystem");
    P("GetDpiForSystem", "%d", gds ? (int)gds() : -1);
    P("Screen", "%dx%d bpp=%d", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), GetDeviceCaps(dc, BITSPIXEL));
    P("SM_CYEDGE", "%d", GetSystemMetrics(SM_CYEDGE));
    ReleaseDC(NULL, dc);
    HIGHCONTRASTA hc = { sizeof hc }; SystemParametersInfoA(SPI_GETHIGHCONTRAST, sizeof hc, &hc, 0);
    P("HighContrast", "flags=0x%lX on=%d scheme=\"%s\"", hc.dwFlags, !!(hc.dwFlags & HCF_HIGHCONTRASTON), hc.lpszDefaultScheme ? hc.lpszDefaultScheme : "");
    const char *pk = "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    P("HKCU.AppsUseLightTheme", "%s", regs(HKEY_CURRENT_USER, pk, "AppsUseLightTheme"));
    P("HKCU.SystemUsesLightTheme", "%s", regs(HKEY_CURRENT_USER, pk, "SystemUsesLightTheme"));
    P("HKCU.Themes.CurrentTheme", "%s", regs(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes", "CurrentTheme"));
    P("HKCU.ThemeManager.ThemeActive", "%s", regs(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\ThemeManager", "ThemeActive"));
    P("HKCU.ThemeManager.DllName", "%s", regs(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\ThemeManager", "DllName"));
    P("HKLM.Themes.InstallTheme", "%s", regs(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes", "InstallTheme"));
    P("HKCU.Desktop.LogPixels", "%s", regs(HKEY_CURRENT_USER, "Control Panel\\Desktop", "LogPixels"));
    P("HKCU.Desktop.Win8DpiScaling", "%s", regs(HKEY_CURRENT_USER, "Control Panel\\Desktop", "Win8DpiScaling"));
    P("HKCU.WindowMetrics.AppliedDPI", "%s", regs(HKEY_CURRENT_USER, "Control Panel\\Desktop\\WindowMetrics", "AppliedDPI"));
    P("HKCU.HighContrast.Flags", "%s", regs(HKEY_CURRENT_USER, "Control Panel\\Accessibility\\HighContrast", "Flags"));
}

int main(int argc, char **argv)
{
    int wait = 0, i; const char *actmf = NULL;
    setvbuf(stdout, NULL, _IONBF, 0);
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--wait") && i + 1 < argc) wait = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--actctx") && i + 1 < argc) actmf = argv[++i];
    }
    HMODULE ux = LoadLibraryA("uxtheme.dll");
    if (!ux) { P("uxtheme", "LOADFAIL err=%lu", GetLastError()); return 2; }
#define G(n) p##n = (void *)GetProcAddress(ux, #n)
    G(IsThemeActive); G(IsAppThemed); G(IsCompositionActive); G(GetThemeAppProperties); G(GetCurrentThemeName); G(OpenThemeData);
    G(CloseThemeData); G(GetThemePartSize); G(GetThemePosition); G(GetWindowTheme);
    if (wait > 0) {
        DWORD t0 = GetTickCount(); int first = -1, polls = 0;
        while (GetTickCount() - t0 < (DWORD)wait * 1000) { polls++; if (pIsThemeActive()) { first = (int)(GetTickCount() - t0); break; } Sleep(250); }
        P("WaitThemeActive", "after_ms=%d polls=%d limit_s=%d", first, polls, wait);
    }
    session_report();
    display_report();
    theme_report();
    /* process default comctl32 (decided by the embedded manifest) */
    WNDCLASSW wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"themeenv_top";
    RegisterClassW(&wc);
    HWND top = CreateWindowExW(0, L"themeenv_top", L"themeenv", WS_OVERLAPPEDWINDOW, 0, 0, 500, 400, NULL, NULL, wc.hInstance, NULL);
    P("TopWindow", "%s", top ? "ok" : "CREATEFAIL");
    comctl_report("default");
    if (top) header_report("default", top);
    if (actmf) {
        ACTCTXA ac; memset(&ac, 0, sizeof ac); ac.cbSize = sizeof ac; ac.lpSource = actmf;
        HANDLE h = CreateActCtxA(&ac); ULONG_PTR cookie = 0;
        if (h == INVALID_HANDLE_VALUE) P("actctx", "CREATEFAIL err=%lu", GetLastError());
        else if (!ActivateActCtx(h, &cookie)) P("actctx", "ACTIVATEFAIL err=%lu", GetLastError());
        else {
            P("actctx", "active");
            comctl_report("actctx");
            if (top) header_report("actctx", top);
            DeactivateActCtx(0, cookie);
        }
    }
    if (top) DestroyWindow(top);
    printf("ENVDONE\n");
    return (pIsThemeActive && pIsThemeActive()) ? 0 : 1;
}
