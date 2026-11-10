# Each directory under tests/ builds to build/<dir>.exe (all .c in it + common/t.c).
# The DLL under test is deliberately NOT linked; everything resolves via GetProcAddress.
CC      = x86_64-w64-mingw32-gcc
CFLAGS  ?= -O1 -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-format
AREAS   := $(notdir $(wildcard tests/*))
EXES    := $(addprefix build/,$(addsuffix .exe,$(AREAS)))
all: $(EXES)
.SECONDEXPANSION:
# An area that needs comctl32 v6 as its PROCESS default adds tests/<area>/app.rc containing
#   1 24 "app_v6.manifest"        (tools/vm/app_v6.manifest: Win10 compat + Common-Controls 6.0 dependency)
# Without it the exe gets mingw's default manifest and comctl32 5.82 (docs/VM_THEME_ENV.md).
WINDRES = x86_64-w64-mingw32-windres
build/%.exe: $$(wildcard tests/$$*/*.c) $$(wildcard tests/$$*/*.rc) common/t.c common/t.h common/modules.h
	@mkdir -p build
	$(if $(wildcard tests/$*/app.rc),$(WINDRES) -I tests/$* -I tools/vm tests/$*/app.rc build/$*.res.o)
	$(CC) $(CFLAGS) -Icommon -o $@ $(wildcard tests/$*/*.c) $(if $(wildcard tests/$*/app.rc),build/$*.res.o) common/t.c -static -lkernel32 -ladvapi32 -lntdll
clean: ; rm -rf build
# Validation under Wine in a headless sandbox (see tools/wine_run.sh). Windows is the authority.
wine-run: all
	@for e in $(EXES); do echo "== $$e"; tools/wine_run.sh $$e | grep -E '^(FAIL|SKIP|SUMMARY)|^  (FAIL|NOTE)'; done
.PHONY: all clean wine-run
