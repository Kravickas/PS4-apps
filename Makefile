# SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build vdec2_capture for PS4 using the OpenOrbis SDK and produce a .pkg
# that can be installed via Remote PKG Installer.
#
# Required: OO_PS4_TOOLCHAIN environment variable (set by toolchain-action).
# PkgTool.Core and create-gp4 are expected to be in $TOOLCHAIN/bin/linux/.
#
# Usage:
#   make            # build everything: eboot.bin -> sce_sys/* -> .gp4 -> .pkg
#   make clean      # remove build outputs

ifndef OO_PS4_TOOLCHAIN
$(error OO_PS4_TOOLCHAIN environment variable not set)
endif

# ---- Project metadata (edit if needed) ------------------------------------
TITLE       := vdec2 capture
TITLE_ID    := SHAD00001
VERSION     := 01.00
CONTENT_ID  := IV0000-$(TITLE_ID)_00-VDEC2CAPTURE0000

# ---- Tools ---------------------------------------------------------------
TOOLDIR  := $(OO_PS4_TOOLCHAIN)/bin/linux
CXX      := clang++
LD       := ld.lld
FSELF    := $(TOOLDIR)/create-fself
GP4GEN   := $(TOOLDIR)/create-gp4
PKGTOOL  := $(TOOLDIR)/PkgTool.Core

# ---- Source/build paths ---------------------------------------------------
SRC      := vdec2_capture.cpp
OBJ      := build/vdec2_capture.o
ELF      := build/vdec2_capture.elf
OELF     := build/vdec2_capture.oelf
EBOOT    := eboot.bin
SCE_SYS  := sce_sys
SFO      := $(SCE_SYS)/param.sfo
ICON     := $(SCE_SYS)/icon0.png
GP4      := pkg.gp4
PKG      := $(CONTENT_ID).pkg

# ---- Compile / link flags -------------------------------------------------
TARGETFLAGS := --target=x86_64-pc-freebsd12-elf
CXXFLAGS    := $(TARGETFLAGS) -fPIC -funwind-tables \
               -O2 -ffunction-sections -fdata-sections \
               -fno-rtti -fno-exceptions -std=c++17 \
               -Wall -Wno-narrowing \
               -isysroot $(OO_PS4_TOOLCHAIN) \
               -isystem $(OO_PS4_TOOLCHAIN)/include \
               -isystem $(OO_PS4_TOOLCHAIN)/include/c++/v1 \
               -D__PS4__ -D__ORBIS__

LDFLAGS  := -m elf_x86_64 -pie --eh-frame-hdr --gc-sections \
            --script $(OO_PS4_TOOLCHAIN)/link.x \
            -L$(OO_PS4_TOOLCHAIN)/lib

# Always-required PS4 system libs that ship in OpenOrbis
LIBS     := -lkernel -lc -lc++

# libSceVideodec2 stub may or may not be in the toolchain. The CI workflow
# detects this and sets VDEC2_STUB; for local builds the default is "present".
ifeq ($(VDEC2_STUB),absent)
  LDFLAGS += --unresolved-symbols=ignore-all
else
  LIBS += -lSceVideodec2
endif

ifeq ($(V),1)
Q :=
else
Q := @
endif

.PHONY: all clean
.DELETE_ON_ERROR:

all: $(PKG)

# ---- Compile + link + sign -> eboot.bin ----------------------------------
build:
	$(Q)mkdir -p build

$(OBJ): $(SRC) | build
	@echo "  CXX  $<"
	$(Q)$(CXX) $(CXXFLAGS) -c $< -o $@

$(ELF): $(OBJ)
	@echo "  LD   $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ \
	    $(OO_PS4_TOOLCHAIN)/lib/crt1.o $(OBJ) $(LIBS)

$(EBOOT): $(ELF)
	@echo "  FSELF $@"
	$(Q)$(FSELF) -in=$< -out=$(OELF) --eboot=$@ --paid 0x3800000000000011

# ---- sce_sys assets -------------------------------------------------------

# Minimal 256x256 placeholder icon (solid-colour PNG, generated on-the-fly).
# A real icon can be dropped into sce_sys/icon0.png to override this rule.
$(SCE_SYS):
	$(Q)mkdir -p $@

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

$(ICON): | $(SCE_SYS)
	@echo "  ICON $@"
	$(Q)python3 -c "$$GENICON_PY" $@

# param.sfo — generated via PkgTool.Core. The set of entries below is the
# minimum set that the PS4 Remote PKG Installer accepts for an FPKG (fake
# pkg) homebrew app. Order matches Al-Azif's ps4-payload-guest reference.
$(SFO): Makefile | $(SCE_SYS)
	@echo "  SFO  $@"
	$(Q)$(PKGTOOL) sfo_new $@
	$(Q)$(PKGTOOL) sfo_setentry $@ APP_TYPE   --type Integer --maxsize 4   --value 1
	$(Q)$(PKGTOOL) sfo_setentry $@ APP_VER    --type Utf8    --maxsize 8   --value '$(VERSION)'
	$(Q)$(PKGTOOL) sfo_setentry $@ ATTRIBUTE  --type Integer --maxsize 4   --value 0
	$(Q)$(PKGTOOL) sfo_setentry $@ CATEGORY   --type Utf8    --maxsize 4   --value 'gde'
	$(Q)$(PKGTOOL) sfo_setentry $@ CONTENT_ID --type Utf8    --maxsize 48  --value '$(CONTENT_ID)'
	$(Q)$(PKGTOOL) sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4   --value 0
	$(Q)$(PKGTOOL) sfo_setentry $@ TITLE      --type Utf8    --maxsize 128 --value '$(TITLE)'
	$(Q)$(PKGTOOL) sfo_setentry $@ TITLE_ID   --type Utf8    --maxsize 12  --value '$(TITLE_ID)'
	$(Q)$(PKGTOOL) sfo_setentry $@ VERSION    --type Utf8    --maxsize 8   --value '$(VERSION)'

# ---- pkg project + build --------------------------------------------------
$(GP4): $(EBOOT) $(SFO) $(ICON)
	@echo "  GP4  $@"
	$(Q)$(GP4GEN) -out $@ --content-id=$(CONTENT_ID) --files "$^"

$(PKG): $(GP4)
	@echo "  PKG  $@"
	$(Q)$(PKGTOOL) pkg_build $< .

clean:
	@echo "  CLEAN"
	$(Q)rm -rf build $(EBOOT) $(OELF) $(SCE_SYS) $(GP4) *.pkg
