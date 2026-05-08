# SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build vdec2_capture for PS4 using the OpenOrbis SDK.
# Requires: OO_PS4_TOOLCHAIN environment variable. The CI workflow handles this.
#
# Local build (Linux/WSL/macOS):
#   export OO_PS4_TOOLCHAIN=/path/to/openorbis
#   make
#
# Output: build/eboot.bin
# Wrap that into a .pkg with your PKG tool of choice and install via Remote
# PKG Installer on a jailbroken PS4.

ifndef OO_PS4_TOOLCHAIN
$(error OO_PS4_TOOLCHAIN environment variable not set)
endif

# Project
TARGET    := vdec2_capture
BUILDDIR  := build
SRCS      := $(TARGET).cpp
OBJS      := $(SRCS:%.cpp=$(BUILDDIR)/%.o)

# Tools
CXX       := clang++
LD        := ld.lld
FSELF     := $(OO_PS4_TOOLCHAIN)/bin/linux/create-fself

TARGETFLAGS := --target=x86_64-pc-freebsd12-elf

CXXFLAGS  := $(TARGETFLAGS) -fPIC -funwind-tables \
             -O2 -ffunction-sections -fdata-sections \
             -fno-rtti -fno-exceptions -std=c++17 \
             -Wall -Wno-narrowing \
             -isysroot $(OO_PS4_TOOLCHAIN) \
             -isystem $(OO_PS4_TOOLCHAIN)/include \
             -isystem $(OO_PS4_TOOLCHAIN)/include/c++/v1 \
             -D__PS4__ -D__ORBIS__

LDFLAGS   := -m elf_x86_64 -pie --eh-frame-hdr --gc-sections \
             --script $(OO_PS4_TOOLCHAIN)/link.x \
             -L$(OO_PS4_TOOLCHAIN)/lib

# Always-required PS4 system libs that ship in OpenOrbis
LIBS      := -lkernel -lc -lc++

# libSceVideodec2 stub may or may not be in the toolchain depending on
# version. The workflow detects this and sets VDEC2_STUB accordingly:
#   - VDEC2_STUB=present  -> link with -lSceVideodec2 normally
#   - VDEC2_STUB=absent   -> resolve sceVideodec2QueryDecoderMemoryInfo at
#                            runtime; tell the linker to ignore the
#                            unresolved symbol (PS4 dynamic linker will
#                            satisfy it via the actual libSceVideodec2.sprx).
ifeq ($(VDEC2_STUB),present)
  LIBS += -lSceVideodec2
else ifeq ($(VDEC2_STUB),absent)
  LDFLAGS += --unresolved-symbols=ignore-all
else
  # Default for local builds: assume present, fail loudly if not
  LIBS += -lSceVideodec2
endif

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
