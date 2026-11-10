/* themeset.c - runtime theme-configuration switches used by tools/theme_config.sh inside the Win10 VM.
 *
 *   themeset.exe hc on [SCHEME]   SPI_SETHIGHCONTRAST HCF_HIGHCONTRASTON (+SPIF_UPDATEINIFILE|SPIF_SENDCHANGE);
 *                                 SCHEME default "High Contrast Black"
 *   themeset.exe hc off           clears HCF_HIGHCONTRASTON
 *   themeset.exe broadcast STR    SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, STR) (e.g. ImmersiveColorSet)
 *   themeset.exe wait-themed SEC  poll IsThemeActive() until TRUE (exit 0) or SEC elapsed (exit 1)
 * Every action prints one "THEMESET ..." line with the result.  These change SESSION-WIDE state: only for
 * dx_* areas / tools/theme_config.sh configurations on the disposable VM.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc >= 3 && !strcmp(argv[1], "hc")) {
        static WCHAR scheme[128] = L"High Contrast Black";
        HIGHCONTRASTW hc; memset(&hc, 0, sizeof hc); hc.cbSize = sizeof hc;
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof hc, &hc, 0);
        DWORD before = hc.dwFlags; int on = !strcmp(argv[2], "on");
        if (argc >= 4) MultiByteToWideChar(CP_ACP, 0, argv[3], -1, scheme, 128);
        if (on) { hc.dwFlags |= HCF_HIGHCONTRASTON; hc.lpszDefaultScheme = scheme; } else hc.dwFlags &= ~HCF_HIGHCONTRASTON;
        DWORD t0 = GetTickCount();
        BOOL ok = SystemParametersInfoW(SPI_SETHIGHCONTRAST, sizeof hc, &hc, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
        DWORD err = GetLastError(), ms = GetTickCount() - t0;
        memset(&hc, 0, sizeof hc); hc.cbSize = sizeof hc; SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof hc, &hc, 0);
        printf("THEMESET hc %s ok=%d err=%lu ms=%lu flags_before=0x%lX flags_after=0x%lX\n", argv[2], ok, err, ms, before, hc.dwFlags);
        return ok ? 0 : 1;
    }
    if (argc >= 3 && !strcmp(argv[1], "broadcast")) {
        WCHAR w[256]; DWORD_PTR res = 0; MultiByteToWideChar(CP_ACP, 0, argv[2], -1, w, 256);
        LRESULT r = SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)w, SMTO_ABORTIFHUNG, 5000, &res);
        printf("THEMESET broadcast %s ret=%ld\n", argv[2], (long)r);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "wait-themed")) {
        HMODULE ux = LoadLibraryA("uxtheme.dll"); BOOL (WINAPI *act)(void) = ux ? (void *)GetProcAddress(ux, "IsThemeActive") : NULL;
        DWORD t0 = GetTickCount(), lim = atoi(argv[2]) * 1000;
        while (act && !act() && GetTickCount() - t0 < lim) Sleep(250);
        int on = act && act();
        printf("THEMESET wait-themed active=%d ms=%lu\n", on, GetTickCount() - t0);
        return on ? 0 : 1;
    }
    fprintf(stderr, "usage: themeset hc on [scheme] | hc off | broadcast STR | wait-themed SEC\n");
    return 2;
}
