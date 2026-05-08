# SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build vdec2_capture.pkg using OpenOrbis SDK.
# Requires: OO_PS4_TOOLCHAIN environment variable pointing to OpenOrbis install.
# Tested with OpenOrbis SDK as of 2024 (relies on standard layout under
# $OO_PS4_TOOLCHAIN with bin/linux, lib, include, link.x).

ifndef OO_PS4_TOOLCHAIN
$(error OO_PS4_TOOLCHAIN environment variable not set)
endif

TARGET     := vdec2_capture
TITLE_ID   := SHAD00001
CONTENT_ID := IV0000-$(TITLE_ID)_00-VDEC2CAPTURE0000
PKG_VER    := 01.00

OUTDIR := build

# Tools
CXX     := clang++ --target=x86_64-pc-freebsd12-elf
LD      := ld.lld
FSELF   := $(OO_PS4_TOOLCHAIN)/bin/linux/create-fself
PKGCMD  := $(OO_PS4_TOOLCHAIN)/bin/linux/create-pkg

# Flags
CXXFLAGS := -O2 -ffunction-sections -fdata-sections -Wno-narrowing -fno-rtti \
            -fno-exceptions -std=c++17 -fPIC -funwind-tables \
            -isysroot $(OO_PS4_TOOLCHAIN) \
            -isystem $(OO_PS4_TOOLCHAIN)/include \
            -isystem $(OO_PS4_TOOLCHAIN)/include/c++/v1 \
            -D__PS4__ -D__ORBIS__

LDFLAGS := -m elf_x86_64 -pie --eh-frame-hdr --gc-sections \
           --script $(OO_PS4_TOOLCHAIN)/link.x \
           -L$(OO_PS4_TOOLCHAIN)/lib

LIBS := -lkernel -lc -lc++ -lSceVideodec2

OBJS := $(OUTDIR)/$(TARGET).o
ELF  := $(OUTDIR)/$(TARGET).elf
EBT  := $(OUTDIR)/eboot.bin
PKG  := $(OUTDIR)/$(TARGET).pkg

.PHONY: all clean
all: $(PKG)

$(OUTDIR):
	@mkdir -p $@

$(OUTDIR)/%.o: %.cpp | $(OUTDIR)
	$(CXX) -c $(CXXFLAGS) -o $@ $<

$(ELF): $(OBJS)
	$(LD) $(LDFLAGS) $(OO_PS4_TOOLCHAIN)/lib/crt1.o $(OBJS) $(LIBS) -o $@

$(EBT): $(ELF)
	$(FSELF) -in=$< -out=$@ --paid 0x3800000000000011

$(PKG): $(EBT) sce_sys/param.sfo
	$(PKGCMD) --content_id=$(CONTENT_ID) \
	          --files="eboot.bin sce_sys/param.sfo" \
	          --output=$@

clean:
	rm -rf $(OUTDIR)
