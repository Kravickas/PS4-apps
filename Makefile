# ============================================================================
#  PS4 homebrew — reusable build (OpenOrbis). Drop source into src/, push.
#  Edit identity ONCE below; it stays fixed across builds (name / id / icon).
# ============================================================================

# ---- Package identity (fixed) ----
TITLE       := DmemTest
VERSION     := 01.00
TITLE_ID    := SHAD00090
CONTENT_ID  := IV0000-SHAD00090_00-SHADDMEMTEST0000


EXTRAFLAGS  :=

# ---- Toolchain / paths (don't usually need to touch) ----
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)

# ---- Libraries: link EVERY stub the SDK ships, so you never edit this per symbol.
#      Core libs are always linked; everything else is --as-needed, so only libs you
#      actually call become load-time deps (declaring unused modules can fail load).
CORE_LIBS   := -lc -lkernel -lc++ -lSceLibcInternal
ALL_STUBS   := $(sort $(patsubst $(TOOLCHAIN)/lib/lib%.so,-l%,$(wildcard $(TOOLCHAIN)/lib/lib*.so)) \
                      $(patsubst $(TOOLCHAIN)/lib/lib%.a,-l%,$(wildcard $(TOOLCHAIN)/lib/lib*.a)))
EXTRA_LIBS  := $(filter-out $(CORE_LIBS),$(ALL_STUBS))
SRCDIR      := src
INTDIR      := build
PROJ        := homebrew
OUT_ELF     := $(INTDIR)/$(PROJ).elf
OUT_OELF    := $(INTDIR)/$(PROJ).oelf

CC          := clang
CXX         := clang++
LD          := ld.lld
PKG         := $(TOOLCHAIN)/bin/linux

# ---- Sources: only the test, so other projects' files in src/ are never built ----
OBJS        := $(INTDIR)/dmemtest.o

# ---- Bundled .prx/.sprx modules (drop libc.prx / libSceFios2.prx here to ship them) ----
LIBMODULES  := $(wildcard sce_module/*)

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c $(EXTRAFLAGS) \
               -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr \
               -L$(TOOLCHAIN)/lib $(CORE_LIBS) --as-needed $(EXTRA_LIBS) --no-as-needed \
               $(TOOLCHAIN)/lib/crt1.o

all: $(CONTENT_ID).pkg

$(INTDIR):
	mkdir -p $(INTDIR)

$(INTDIR)/%.o: $(SRCDIR)/%.c | $(INTDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.o: $(SRCDIR)/%.cpp | $(INTDIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(INTDIR)/%.o: $(SRCDIR)/%.s | $(INTDIR)
	$(CC) $(CFLAGS) -o $@ $<
eboot.bin: $(OBJS)
	$(LD) $(OBJS) -o $(OUT_ELF) $(LDFLAGS)
	$(PKG)/create-fself -in=$(OUT_ELF) -out=$(OUT_OELF) --eboot "eboot.bin" --paid 0x3100000000000001 -sdkver $(EBOOT_SDK)

# The same program at every other SDK version of src/sdk_chain.h (the first is eboot.bin's default):
# each run hands over to the next with sceSystemServiceLoadExec. An executable has no module
# parameter: its SDK version is the process parameter only (create-fself -sdkver), written as in
# src/sdk_chain.h.
SDK_LIST    := $(shell grep -oE '0x[0-9A-F]{8}' $(SRCDIR)/sdk_chain.h)
SDK_CHAIN   := $(wordlist 2,999,$(SDK_LIST))
# eboot.bin: the first version of the list (make SDK=0x... for another one)
EBOOT_SDK   := $(if $(SDK),$(SDK),$(firstword $(SDK_LIST)))
# param.sfo, as Gran Turismo 7's (SDK 11.00, runs on FW 12.02): SYSTEM_VER (required system software)
# is the highest version in the package without its patch part (GT7: SDK 0x11008001, SYSTEM_VER
# 0x11008000), FORMAT "obs", PUBTOOLMINVER 0x02990000. GT7's flags that change how an app runs
# (ATTRIBUTE, ATTRIBUTE2, ...) are left out: they could change what DmemTest measures.
SDK_TOP     := $(lastword $(SDK_LIST))
SYSTEM_VER  := $(shell printf '%d' $$(( $(SDK_TOP) & 0xFFFFF000 )))
PUBTOOLMINVER := $(shell printf '%d' 0x02990000)
SDK_EBOOTS  := $(foreach v,$(SDK_CHAIN),sdk/eboot_$(v).bin)

# From SDK 10.00 the executable is linked with a copy of crt1.o whose process parameter has the
# layout of newer SDKs (0x60 bytes, 5 entries, as Gran Turismo 7's; tools/procparam60.py): FW 12.02
# refused the 10.00 executable with OpenOrbis' 0x50 bytes, 3 entries. The others keep crt1.o.
OUT_ELF_PP60 := $(INTDIR)/homebrew_pp60.elf

$(INTDIR)/crt1_pp60.o: $(TOOLCHAIN)/lib/crt1.o tools/procparam60.py | $(INTDIR)
	python3 tools/procparam60.py $< $@

$(OUT_ELF_PP60): $(OBJS) $(INTDIR)/crt1_pp60.o
	$(LD) $(OBJS) -o $@ $(subst $(TOOLCHAIN)/lib/crt1.o,$(INTDIR)/crt1_pp60.o,$(LDFLAGS))

sdk/eboot_%.bin: eboot.bin $(OUT_ELF_PP60)
	@mkdir -p sdk
	$(PKG)/create-fself -in=$(if $(filter 0x1%,$*),$(OUT_ELF_PP60),$(OUT_ELF)) -out=$(INTDIR)/sdk_$*.oelf --eboot "$@" --paid 0x3100000000000001 -sdkver $*

sce_sys/param.sfo: Makefile
	$(PKG)/PkgTool.Core sfo_new $@
	$(PKG)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(PKG)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(PKG)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(PKG)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(PKG)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(PKG)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(PKG)/PkgTool.Core sfo_setentry $@ FORMAT --type Utf8 --maxsize 4 --value 'obs'
	$(PKG)/PkgTool.Core sfo_setentry $@ PUBTOOLMINVER --type Integer --maxsize 4 --value $(PUBTOOLMINVER)
	$(PKG)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value $(SYSTEM_VER)
	$(PKG)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(PKG)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(PKG)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

# ---- No assets: the test draws its own text. ----
pkg.gp4: eboot.bin $(SDK_EBOOTS) sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx $(LIBMODULES)
	python3 tools/make_gp4.py --out $@ --content-id $(CONTENT_ID) $^

$(CONTENT_ID).pkg: pkg.gp4
	$(PKG)/PkgTool.Core pkg_build $< .

clean:
	rm -rf $(INTDIR) eboot.bin sdk pkg.gp4 sce_sys/param.sfo *.pkg

.PHONY: all clean
