# ============================================================================
#  PS4 homebrew — reusable build (OpenOrbis). Drop source into src/, push.
#  Edit identity ONCE below; it stays fixed across builds (name / id / icon).
# ============================================================================

# ---- Package identity (fixed) ----
TITLE       := DmemTest
VERSION     := 01.00
TITLE_ID    := SHAD00090
CONTENT_ID  := IV0000-SHAD00090_00-SHADDMEMTEST0000

# make BISECT=1: the SDK bisect package (its own title, results and state; only the sections whose
# lines change with the SDK version; versions from src/sdk_bisect.h, eboot.bin at the first one).
CHAIN_H     := sdk_chain.h
ifeq ($(BISECT),1)
TITLE       := DmemTest SDK bisect
TITLE_ID    := SHAD00095
CONTENT_ID  := IV0000-SHAD00095_00-SHADDMEMBISECT00
CHAIN_H     := sdk_bisect.h
SDK         ?= $(firstword $(shell grep -oE '0x[0-9A-F]{8}' src/sdk_bisect.h))
endif

EXTRAFLAGS  := $(if $(filter 1,$(BISECT)),-DDMEM_BISECT)

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
	$(PKG)/create-fself -in=$(OUT_ELF) -out=$(OUT_OELF) --eboot "eboot.bin" --paid 0x3100000000000001 $(if $(SDK),-sdkver $(SDK))

# The same program at every other SDK version of src/sdk_chain.h (the first is eboot.bin's default):
# each run hands over to the next with sceSystemServiceLoadExec. From SDK 10.00 the executable also
# gets its firmware version field (-fwversion, 0 otherwise) set to its SDK version: with it unset, FW
# 12.02 refused the 10.00 executable as corrupted.
SDK_CHAIN   := $(wordlist 2,999,$(shell grep -oE '0x[0-9A-F]{8}' $(SRCDIR)/$(CHAIN_H)))
SDK_EBOOTS  := $(foreach v,$(SDK_CHAIN),sdk/eboot_$(v).bin)

sdk/eboot_%.bin: eboot.bin
	@mkdir -p sdk
	$(PKG)/create-fself -in=$(OUT_ELF) -out=$(INTDIR)/sdk_$*.oelf --eboot "$@" --paid 0x3100000000000001 -sdkver $* $(if $(filter 0x1%,$*),-fwversion $*)

sce_sys/param.sfo: Makefile
	$(PKG)/PkgTool.Core sfo_new $@
	$(PKG)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(PKG)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(PKG)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(PKG)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(PKG)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(PKG)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(PKG)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
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
