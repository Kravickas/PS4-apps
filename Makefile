# ============================================================================
#  PS4 homebrew — reusable build (OpenOrbis). Drop source into src/, push.
#  Edit identity ONCE below; it stays fixed across builds (name / id / icon).
# ============================================================================

# ---- Package identity (fixed) ----
TITLE       := Homebrew Template
VERSION     := 01.00
TITLE_ID    := BREW00001
CONTENT_ID  := IV0000-BREW00001_00-HOMEBREW00000000

# ---- Libraries linked into the ELF (libc + fios2 are the baseline) ----
LIBS        := -lc -lkernel -lc++ -lSceLibcInternal -lSceFios2 \
               -lSceVideoOut -lSceGnmDriver -lScePad -lSceUserService -lSceSystemService

EXTRAFLAGS  :=

# ---- Toolchain / paths (don't usually need to touch) ----
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
SRCDIR      := src
INTDIR      := build
PROJ        := homebrew
OUT_ELF     := $(INTDIR)/$(PROJ).elf
OUT_OELF    := $(INTDIR)/$(PROJ).oelf

CC          := clang
CXX         := clang++
LD          := ld.lld
PKG         := $(TOOLCHAIN)/bin/linux

# ---- Sources: any .c / .cpp / .s in src/ ----
CFILES      := $(wildcard $(SRCDIR)/*.c)
CPPFILES    := $(wildcard $(SRCDIR)/*.cpp)
SFILES      := $(wildcard $(SRCDIR)/*.s)
OBJS        := $(patsubst $(SRCDIR)/%.c,$(INTDIR)/%.o,$(CFILES)) \
               $(patsubst $(SRCDIR)/%.cpp,$(INTDIR)/%.o,$(CPPFILES)) \
               $(patsubst $(SRCDIR)/%.s,$(INTDIR)/%.o,$(SFILES))

# ---- Bundled .prx/.sprx modules (drop libc.prx / libSceFios2.prx here to ship them) ----
LIBMODULES  := $(wildcard sce_module/*)

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c $(EXTRAFLAGS) \
               -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr \
               -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

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
	$(PKG)/create-fself -in=$(OUT_ELF) -out=$(OUT_OELF) --eboot "eboot.bin" --paid 0x3800000000000011

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

pkg.gp4: eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx $(LIBMODULES)
	$(PKG)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

$(CONTENT_ID).pkg: pkg.gp4
	$(PKG)/PkgTool.Core pkg_build $< .

clean:
	rm -rf $(INTDIR) eboot.bin pkg.gp4 sce_sys/param.sfo *.pkg

.PHONY: all clean
