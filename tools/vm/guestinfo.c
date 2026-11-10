/* guestinfo.exe - prints OS/theme file facts of the guest (hashes, versions, registry) for data/win10/<config>/README */
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    const char *c[] = {
        "ver",
        "certutil -hashfile C:\\Windows\\resources\\Themes\\Aero\\Aero.msstyles SHA256",
        "certutil -hashfile C:\\Windows\\resources\\Themes\\aero.theme SHA256",
        "certutil -hashfile C:\\Windows\\resources\\Themes\\light.theme SHA256",
        "certutil -hashfile C:\\Windows\\System32\\uxtheme.dll SHA256",
        "dir C:\\Windows\\resources\\Themes\\Aero\\Aero.msstyles C:\\Windows\\resources\\Themes\\aero.theme C:\\Windows\\resources\\Themes\\light.theme",
        "dir C:\\Windows\\resources\\Themes\\Aero\\en-US",
        "reg query \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\" /s",
        "reg query \"HKCU\\Control Panel\\Desktop\" /v LogPixels",
        "reg query \"HKCU\\Control Panel\\Desktop\" /v Win8DpiScaling",
        "reg query \"HKCU\\Software\\Microsoft\\Windows\\DWM\"",
        "reg query \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\" /v BuildLabEx",
        "reg query \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\" /v UBR",
        "sc query Themes", "wmic path win32_videocontroller get name,currenthorizontalresolution,currentverticalresolution",
        "wmic os get Caption,Version,BuildNumber,OSArchitecture,Locale,MUILanguages /format:list",
        "reg query \"HKCU\\Control Panel\\International\" /v LocaleName", NULL };
    int i; for (i = 0; c[i]; i++) { printf("### %s\n", c[i]); fflush(stdout); system(c[i]); printf("\n"); fflush(stdout); }
    return 0;
}
