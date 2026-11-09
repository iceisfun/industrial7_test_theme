CC=x86_64-w64-mingw32-gcc
CF=-O1 -Wall -Wno-unused-variable -Wno-format
all: probe_v5.exe probe_v6.exe
probe_v5.exe: probe.c ; $(CC) $(CF) -o $@ $< -lcomctl32 -luser32 -lgdi32 -lkernel32
probe_v6.exe: probe.c probe_v6.rc probe.manifest ; x86_64-w64-mingw32-windres probe_v6.rc probe_v6.o && $(CC) $(CF) -o $@ $< probe_v6.o -lcomctl32 -luser32 -lgdi32 -lkernel32
clean: ; rm -f *.exe *.o
