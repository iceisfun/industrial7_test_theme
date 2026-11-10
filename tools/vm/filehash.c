/* filehash.c - prints SHA-256 + size of the theme-related files in the guest (README data). No args needed. */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
static void h(const char *p)
{
    HANDLE f = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    HCRYPTPROV pv; HCRYPTHASH hh; static BYTE buf[65536]; DWORD n, sz = 0; BYTE d[32]; DWORD dl = 32; int i;
    if (f == INVALID_HANDLE_VALUE) { printf("FILEHASH %s MISSING err=%lu\n", p, GetLastError()); return; }
    CryptAcquireContextA(&pv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT); CryptCreateHash(pv, CALG_SHA_256, 0, 0, &hh);
    while (ReadFile(f, buf, sizeof buf, &n, NULL) && n) { CryptHashData(hh, buf, n, 0); sz += n; }
    CryptGetHashParam(hh, HP_HASHVAL, d, &dl, 0); printf("FILEHASH %s size=%lu sha256=", p, sz); for (i = 0; i < 32; i++) printf("%02x", d[i]); printf("\n");
    CryptDestroyHash(hh); CryptReleaseContext(pv, 0); CloseHandle(f);
}
int main(void)
{
    const char *l[] = {"C:\\Windows\\resources\\themes\\Aero\\Aero.msstyles", "C:\\Windows\\resources\\themes\\Aero\\AeroLite.msstyles",
        "C:\\Windows\\resources\\Ease of Access Themes\\hcblack.theme", "C:\\Windows\\resources\\Ease of Access Themes\\hcwhite.theme",
        "C:\\Windows\\resources\\Themes\\aero.theme", "C:\\Windows\\resources\\Themes\\dark.theme", "C:\\Windows\\resources\\Themes\\light.theme",
        "C:\\Windows\\System32\\uxtheme.dll", "C:\\Windows\\System32\\themeui.dll", "C:\\Windows\\System32\\themeservice.dll", NULL}; int i;
    setvbuf(stdout, NULL, _IONBF, 0);
    for (i = 0; l[i]; i++) h(l[i]);
    return 0;
}
