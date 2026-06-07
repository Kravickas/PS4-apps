// PM4 command buffer builder for PS4 GFX ring
// Register offsets verified against shadPS4 regs.cpp static_asserts
#pragma once
#include <stdint.h>

// ============================================================================
// PM4 packet header helpers
// ============================================================================

// Type 3 packet: [31:30]=11, [29:16]=count-1, [15:8]=opcode, [7:0]=predicate
static inline uint32_t pm4_type3(uint32_t opcode, uint32_t count) {
    return (3u << 30) | (((count)-1) << 16) | ((opcode) << 8);
}

// ============================================================================
// PM4 opcodes (from shadPS4 pm4_opcodes.h)
// ============================================================================
#define PM4_CONTEXT_CONTROL     0x28
#define PM4_SET_CONTEXT_REG     0x69
#define PM4_SET_SH_REG          0x76
#define PM4_SET_UCONFIG_REG     0x79
#define PM4_DRAW_INDEX_AUTO     0x2D
#define PM4_NUM_INSTANCES       0x2F
#define PM4_EVENT_WRITE_EOP     0x47
#define PM4_NOP                 0x10
#define PM4_ACQUIRE_MEM         0x58

// Register bases
#define CTX_REG_BASE    0xA000u
#define SH_REG_BASE     0x2C00u
#define UCONFIG_REG_BASE 0xC000u

// ============================================================================
// Context register offsets (register - 0xA000)
// From shadPS4 regs.cpp static_asserts
// ============================================================================
#define CTX_DEPTH_RENDER_CONTROL    0x000
#define CTX_DEPTH_VIEW              0x002
#define CTX_DEPTH_RENDER_OVERRIDE   0x003
#define CTX_DEPTH_HTILE_DATA_BASE   0x005
#define CTX_SCREEN_SCISSOR          0x00C  // 2 dwords
#define CTX_DB_Z_INFO               0x010
#define CTX_DB_STENCIL_INFO         0x011
#define CTX_DB_Z_READ_BASE          0x012
#define CTX_DB_STENCIL_READ_BASE    0x013
#define CTX_DB_Z_WRITE_BASE         0x014
#define CTX_DB_STENCIL_WRITE_BASE   0x015
#define CTX_DB_DEPTH_SIZE           0x016
#define CTX_DB_DEPTH_SLICE          0x017
#define CTX_WINDOW_SCISSOR          0x081  // 2 dwords
#define CTX_COLOR_TARGET_MASK       0x08E
#define CTX_COLOR_SHADER_MASK       0x08F
#define CTX_GENERIC_SCISSOR         0x090  // 2 dwords
#define CTX_VIEWPORT_SCISSOR0       0x094  // 2 dwords
#define CTX_INDEX_OFFSET            0x102
#define CTX_STENCIL_CONTROL         0x10B
#define CTX_STENCIL_REF_FRONT       0x10C
#define CTX_STENCIL_REF_BACK        0x10D
#define CTX_VIEWPORT0               0x10F  // 6 dwords (xscale,xoff,yscale,yoff,zscale,zoff)
#define CTX_PS_INPUT_CNTL_0         0x191
#define CTX_VS_OUTPUT_CONFIG        0x1B1
#define CTX_PS_INPUT_ENA            0x1B3
#define CTX_PS_INPUT_ADDR           0x1B4
#define CTX_NUM_INTERP              0x1B6
#define CTX_SHADER_POS_FORMAT       0x1C3
#define CTX_Z_EXPORT_FORMAT         0x1C4
#define CTX_COLOR_EXPORT_FORMAT     0x1C5
#define CTX_BLEND_CONTROL0          0x1E0
#define CTX_DEPTH_CONTROL           0x200
#define CTX_COLOR_CONTROL           0x202
#define CTX_CLIPPER_CONTROL         0x204
#define CTX_VIEWPORT_CONTROL        0x206
#define CTX_VS_OUTPUT_CONTROL       0x207
#define CTX_MODE_CONTROL            0x292
#define CTX_INDEX_SIZE              0x29D
#define CTX_STAGE_ENABLE            0x2D5
#define CTX_AA_CONFIG               0x2F8
#define CTX_CB_COLOR0_BASE          0x318
#define CTX_CB_COLOR0_PITCH         0x319
#define CTX_CB_COLOR0_SLICE         0x31A
#define CTX_CB_COLOR0_VIEW          0x31B
#define CTX_CB_COLOR0_INFO          0x31C
#define CTX_CB_COLOR0_ATTRIB        0x31D

// SH register offsets (register - 0x2C00)
#define SH_PS_PGM_LO               0x08
#define SH_PS_PGM_HI               0x09
#define SH_PS_PGM_RSRC1            0x0A
#define SH_PS_PGM_RSRC2            0x0B
#define SH_PS_USER_DATA_0          0x0C
#define SH_VS_PGM_LO               0x48
#define SH_VS_PGM_HI               0x49
#define SH_VS_PGM_RSRC1            0x4A
#define SH_VS_PGM_RSRC2            0x4B
#define SH_VS_USER_DATA_0          0x4C

// UCONFIG register offsets (register - 0xC000)
#define UCFG_PRIMITIVE_TYPE         0x242
#define UCFG_NUM_INSTANCES          0x24D

// ============================================================================
// PM4 command buffer builder (simple linear writer)
// ============================================================================
struct PM4Builder {
    uint32_t* buf;
    uint32_t  off;
    uint32_t  cap;
};

static inline void pm4_init(struct PM4Builder* b, uint32_t* buffer, uint32_t capacity_dwords) {
    b->buf = buffer;
    b->off = 0;
    b->cap = capacity_dwords;
}

static inline void pm4_emit(struct PM4Builder* b, uint32_t val) {
    if (b->off < b->cap) b->buf[b->off++] = val;
}

static inline void pm4_emit_f(struct PM4Builder* b, float val) {
    union { float f; uint32_t u; } conv;
    conv.f = val;
    pm4_emit(b, conv.u);
}

// --- Packet emitters ---

static inline void pm4_context_control(struct PM4Builder* b) {
    pm4_emit(b, pm4_type3(PM4_CONTEXT_CONTROL, 2));
    pm4_emit(b, 0x80000000u); // load_control: LOAD_ENABLE
    pm4_emit(b, 0x80000000u); // shadow_enable: SHADOW_ENABLE
}

static inline void pm4_set_context_reg(struct PM4Builder* b, uint32_t offset, uint32_t value) {
    pm4_emit(b, pm4_type3(PM4_SET_CONTEXT_REG, 2));
    pm4_emit(b, offset);
    pm4_emit(b, value);
}

static inline void pm4_set_context_regs(struct PM4Builder* b, uint32_t offset,
                                        const uint32_t* values, uint32_t count) {
    pm4_emit(b, pm4_type3(PM4_SET_CONTEXT_REG, count + 1));
    pm4_emit(b, offset);
    for (uint32_t i = 0; i < count; i++) pm4_emit(b, values[i]);
}

static inline void pm4_set_sh_reg(struct PM4Builder* b, uint32_t offset, uint32_t value) {
    pm4_emit(b, pm4_type3(PM4_SET_SH_REG, 2));
    pm4_emit(b, offset);
    pm4_emit(b, value);
}

static inline void pm4_set_sh_regs(struct PM4Builder* b, uint32_t offset,
                                   const uint32_t* values, uint32_t count) {
    pm4_emit(b, pm4_type3(PM4_SET_SH_REG, count + 1));
    pm4_emit(b, offset);
    for (uint32_t i = 0; i < count; i++) pm4_emit(b, values[i]);
}

static inline void pm4_set_uconfig_reg(struct PM4Builder* b, uint32_t offset, uint32_t value) {
    pm4_emit(b, pm4_type3(PM4_SET_UCONFIG_REG, 2));
    pm4_emit(b, offset);
    pm4_emit(b, value);
}

static inline void pm4_draw_index_auto(struct PM4Builder* b, uint32_t index_count) {
    pm4_emit(b, pm4_type3(PM4_DRAW_INDEX_AUTO, 2));
    pm4_emit(b, index_count);
    pm4_emit(b, 2); // draw_initiator: source_select=AUTO
}

static inline void pm4_event_write_eop(struct PM4Builder* b,
                                       volatile uint32_t* fence_addr,
                                       uint32_t fence_value) {
    uint64_t addr = (uint64_t)(uintptr_t)fence_addr;
    pm4_emit(b, pm4_type3(PM4_EVENT_WRITE_EOP, 5));
    pm4_emit(b, 0x0504u);                              // BOTTOM_OF_PIPE(4), event_index=5
    pm4_emit(b, (uint32_t)(addr & 0xFFFFFFFFu));        // address_lo
    pm4_emit(b, (uint32_t)(addr >> 32) | 0x20000000u);  // address_hi | data_sel=1(Data32Low)
    pm4_emit(b, fence_value);                           // data_lo
    pm4_emit(b, 0);                                     // data_hi
}

static inline void pm4_nop(struct PM4Builder* b, uint32_t count) {
    if (count == 0) return;
    pm4_emit(b, pm4_type3(PM4_NOP, count));
    for (uint32_t i = 0; i < count; i++) pm4_emit(b, 0);
}
