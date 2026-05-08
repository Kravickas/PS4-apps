# SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build vdec2_capture for PS4 and produce an installable .pkg.
# Modelled on OpenOrbis v0.5.2 samples/hello_world/Makefile (canonical
# reference): https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.2/samples/hello_world/Makefile
#
# Required env: OO_PS4_TOOLCHAIN (set by Al-Azif/toolchain-action).
# Tools used (all in $TOOLCHAIN/bin/linux/): create-fself, create-gp4, PkgTool.Core

# ---- Project metadata -----------------------------------------------------
TITLE       := vdec2 capture
VERSION     := 01.00
TITLE_ID    := SHAD00001
CONTENT_ID  := IV0000-$(TITLE_ID)_00-VDEC2CAPTURE0000

# Libraries linked into the ELF
LIBS        := -lc -lkernel -lc++

# ---- Toolchain plumbing (from OpenOrbis hello_world) ----------------------
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
PROJDIR     := $(shell basename $(CURDIR))
INTDIR      := $(PROJDIR)/x64/Debug

UNAME_S     := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CC      := clang
    CCX     := clang++
    LD      := ld.lld
    CDIR    := linux
endif
ifeq ($(UNAME_S),Darwin)
    CC      := /usr/local/opt/llvm/bin/clang
    CCX     := /usr/local/opt/llvm/bin/clang++
    LD      := /usr/local/opt/llvm/bin/ld.lld
    CDIR    := macos
endif

CFILES      := $(wildcard *.c)
CPPFILES    := $(wildcard *.cpp)
OBJS        := $(patsubst %.c,$(INTDIR)/%.o,$(CFILES)) \
               $(patsubst %.cpp,$(INTDIR)/%.o,$(CPPFILES))

# libSceVideodec2 stub may not ship in v0.5.2. CI workflow detects this
# and sets VDEC2_STUB; default for local builds is "absent" since most
# OpenOrbis releases predate Videodec2 stub generation.
ifeq ($(VDEC2_STUB),present)
    LIBS    += -lSceVideodec2
endif

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c \
               $(EXTRAFLAGS) -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1 -std=c++17
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr \
               -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o
ifeq ($(VDEC2_STUB),absent)
    LDFLAGS += --unresolved-symbols=ignore-all
endif

_unused     := $(shell mkdir -p $(INTDIR))

# ---- Build rules ----------------------------------------------------------
.PHONY: all clean

all: $(CONTENT_ID).pkg

# Final pkg
$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .

# pkg project file (XML); create-gp4 builds it from the file list
pkg.gp4: eboot.bin sce_sys/param.sfo sce_sys/icon0.png
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ \
	    --content-id=$(CONTENT_ID) --files "$^"

# param.sfo — built via PkgTool.Core
sce_sys/param.sfo: Makefile
	@mkdir -p sce_sys
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE   --type Integer --maxsize 4   --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER    --type Utf8    --maxsize 8   --value '$(VERSION)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE  --type Integer --maxsize 4   --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY   --type Utf8    --maxsize 4   --value 'gd'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8    --maxsize 48  --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4   --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE      --type Utf8    --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID   --type Utf8    --maxsize 12  --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION    --type Utf8    --maxsize 8   --value '$(VERSION)'

# Placeholder icon (256x256 solid colour PNG generated inline so we don't
# ship a binary asset in the repo). Override by dropping a real
# sce_sys/icon0.png next to the Makefile.
define GENICON_PY
import zlib, struct, sys
w = h = 256
raw = b"".join(b"\x00" + b"\x33\x66\x99" * w for _ in range(h))
def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
idat = zlib.compress(raw)
with open(sys.argv[1], "wb") as f:
    f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b""))
endef
export GENICON_PY

sce_sys/icon0.png:
	@mkdir -p sce_sys
	python3 -c "$$GENICON_PY" $@

# eboot.bin — link our objects, then sign with create-fself
eboot.bin: $(INTDIR) $(OBJS)
	$(LD) $(INTDIR)/*.o -o $(INTDIR)/$(PROJDIR).elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself \
	    -in=$(INTDIR)/$(PROJDIR).elf \
	    -out=$(INTDIR)/$(PROJDIR).oelf \
	    --eboot "eboot.bin" --paid 0x3800000000000011

$(INTDIR)/%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.o: %.cpp
	$(CCX) $(CXXFLAGS) -o $@ $<

$(INTDIR):
	@mkdir -p $@

clean:
	rm -f $(CONTENT_ID).pkg pkg.gp4 eboot.bin
	rm -rf sce_sys $(PROJDIR)
