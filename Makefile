# SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build vdec2_capture for PS4 using the OpenOrbis SDK.
# Requires: OO_PS4_TOOLCHAIN environment variable pointing to OpenOrbis install
# (e.g. extracted from https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain
#  releases). The CI workflow downloads it automatically.
#
# Local build (Linux/WSL/macOS):
#   export OO_PS4_TOOLCHAIN=/path/to/openorbis
#   make
#
# Output: build/eboot.bin
# Wrap that into a .pkg with your PKG tool of choice (PkgTool.Core etc.)
# and install via Remote PKG Installer on a jailbroken PS4.

ifndef OO_PS4_TOOLCHAIN
$(error OO_PS4_TOOLCHAIN environment variable not set)
endif

# Project
TARGET    := vdec2_capture
PROJDIR   := .
BUILDDIR  := build
SRCS      := $(TARGET).cpp
OBJS      := $(SRCS:%.cpp=$(BUILDDIR)/%.o)

# Tools (clang/clang++/lld must be on PATH; ubuntu-latest has them)
CXX       := clang++
LD        := ld.lld
FSELF     := $(OO_PS4_TOOLCHAIN)/bin/linux/create-fself

# PS4 target triple is FreeBSD 12 ELF
TARGETFLAGS := --target=x86_64-pc-freebsd12-elf

# Compile flags
CXXFLAGS  := $(TARGETFLAGS) -fPIC -funwind-tables \
             -O2 -ffunction-sections -fdata-sections \
             -fno-rtti -fno-exceptions -std=c++17 \
             -Wall -Wno-narrowing \
             -isysroot $(OO_PS4_TOOLCHAIN) \
             -isystem $(OO_PS4_TOOLCHAIN)/include \
             -isystem $(OO_PS4_TOOLCHAIN)/include/c++/v1 \
             -D__PS4__ -D__ORBIS__

# Link flags
LDFLAGS   := -m elf_x86_64 -pie --eh-frame-hdr --gc-sections \
             --script $(OO_PS4_TOOLCHAIN)/link.x \
             -L$(OO_PS4_TOOLCHAIN)/lib

# PS4 system libraries we link against
# NOTE: stubs are auto-generated from ps4libdoc; if libSceVideodec2 isn't
# present in your toolchain version, drop -lSceVideodec2 and the homebrew
# will pull the symbol weakly via libkernel's loader.
LIBS      := -lkernel -lc -lc++ -lSceVideodec2

# Verbose mode: invoke with `make V=1`
ifeq ($(V),1)
Q :=
else
Q := @
endif

.PHONY: all clean

all: $(BUILDDIR)/eboot.bin

$(BUILDDIR):
	$(Q)mkdir -p $@

$(BUILDDIR)/%.o: %.cpp | $(BUILDDIR)
	@echo "  CXX  $<"
	$(Q)$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILDDIR)/$(TARGET).elf: $(OBJS) | $(BUILDDIR)
	@echo "  LD   $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ \
	    $(OO_PS4_TOOLCHAIN)/lib/crt1.o $(OBJS) $(LIBS)

$(BUILDDIR)/eboot.bin: $(BUILDDIR)/$(TARGET).elf
	@echo "  FSELF eboot.bin"
	$(Q)$(FSELF) -in=$< -out=$(BUILDDIR)/$(TARGET).oelf \
	    --eboot=$@ --paid 0x3800000000000011

clean:
	@echo "  CLEAN"
	$(Q)rm -rf $(BUILDDIR)
