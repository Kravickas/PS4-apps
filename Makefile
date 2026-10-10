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

# ---- Optional stack-size variant: set MAIN_STACK (bytes) to give the main thread that stack.
#      It builds under its own title id with its own process parameters (src/procparam.S).
#      Empty = the normal build with OpenOrbis' default process parameters. ----
MAIN_STACK  :=
OBJCOPY     := objcopy
ifneq ($(MAIN_STACK),)
TITLE       := DmemTestStack
EXTRAFLAGS  += -DDMEM_STACK_VARIANT
TITLE_ID    := SHAD00091
CONTENT_ID  := IV0000-SHAD00091_00-SHADDMEMTEST0001
CRT_OBJS     = $(INTDIR)/crt1_noparam.o $(INTDIR)/procparam.o
else
CRT_OBJS    := $(OO_PS4_TOOLCHAIN)/lib/crt1.o
endif

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
               $(CRT_OBJS)

all: $(CONTENT_ID).pkg

$(INTDIR):
	mkdir -p $(INTDIR)

$(INTDIR)/%.o: $(SRCDIR)/%.c | $(INTDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.o: $(SRCDIR)/%.cpp | $(INTDIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(INTDIR)/%.o: $(SRCDIR)/%.s | $(INTDIR)
	$(CC) $(CFLAGS) -o $@ $<
# crt1.o without its process parameters; the three blocks they point to become visible to
# src/procparam.S, which supplies the parameters with the stack size filled in.
$(INTDIR)/crt1_noparam.o: $(TOOLCHAIN)/lib/crt1.o | $(INTDIR)
	$(OBJCOPY) --globalize-symbol=_sceLibcParam --globalize-symbol=_sceKernelMemParam \
	           --globalize-symbol=_sceKernelFsParam --remove-section=.rela.data.sce_process_param \
	           --remove-section=.data.sce_process_param $< $@
$(INTDIR)/procparam.o: $(SRCDIR)/procparam.S | $(INTDIR)
	$(CC) $(CFLAGS) -DMAIN_STACK_SIZE=$(MAIN_STACK) -o $@ $<

eboot.bin: $(OBJS) $(filter $(INTDIR)/%,$(CRT_OBJS))
	$(LD) $(OBJS) -o $(OUT_ELF) $(LDFLAGS)
	$(PKG)/create-fself -in=$(OUT_ELF) -out=$(OUT_OELF) --eboot "eboot.bin" --paid 0x3100000000000001

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
pkg.gp4: eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx $(LIBMODULES)
	python3 tools/make_gp4.py --out $@ --content-id $(CONTENT_ID) $^

$(CONTENT_ID).pkg: pkg.gp4
	$(PKG)/PkgTool.Core pkg_build $< .

clean:
	rm -rf $(INTDIR) eboot.bin pkg.gp4 sce_sys/param.sfo *.pkg

.PHONY: all clean
