// ============================================================================
// pm4.h — PM4 Command Buffer Builder for PS4 GPU
// ============================================================================
//
// WHAT IS PM4?
//   PM4 (Packet Manager 4) is AMD's protocol for sending commands to the GPU.
//   Instead of calling GPU functions directly, you build a command buffer
//   (a list of PM4 packets) and submit the whole thing at once.
//
//   Think of it like writing a to-do list for the GPU:
//     "Set this register to X, set that register to Y, now draw 36 triangles"
//
// HOW PM4 PACKETS WORK:
//   Every packet starts with a header dword:
//     Bits [31:30] = 3  (always, means "type 3 packet")
//     Bits [29:16] = count of body dwords minus 1
//     Bits [15:8]  = opcode (what command)
//     Bits [7:0]   = 0 (predicate, always 0 for us)
//
//   After the header come the body dwords (register offset + values).
//
// GPU REGISTER TYPES:
//   The PS4 GPU has thousands of registers, organized in 3 groups:
//
//   CONTEXT registers (0xA000-0xAFFF): Drawing state
//     Scissors, viewport, depth test, color buffer, blend mode, etc.
//     Changed between draw calls (e.g., switch depth mode mid-frame).
//
//   SH registers (0x2C00-0x2CFF): Shader state
//     Shader program address, resource descriptors, user data.
//     Set before each draw to configure which shaders to use.
//
//   UCONFIG registers (0xC000-0xCFFF): Global GPU config
//     Primitive type (triangles/lines), instance count.
//     Rarely changes.
//
// REGISTER OFFSETS:
//   All offsets below are relative to their base (already subtracted).
//   The PM4 helper functions add the base automatically.
//   Values verified against shadPS4's regs.cpp static_asserts.
//
// ============================================================================
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
#define PM4_WRITE_DATA          0x37

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

/* Default hardware-state init — the exact sequence sceGnmDrawInitDefaultHardwareState
   emits on real PS4. Copied verbatim from shadPS4's gnmdriver.cpp (ClearContextState)
   and gnmdriver_init.h (base InitSequence, the non-Neo / pre-1.75-SDK variant — correct
   for a base FAT PS4).

   Real PS4 firmware requires this register-defaults block before any draw: it puts the
   scan converter, viewport, clip, VGT, and color-buffer registers into a known-good
   state. Homebrew that skips it leaves those context registers undefined -> GPU hang /
   black screen on hardware. shadPS4 self-initializes its register tracking, so the
   omission is invisible there. Emitted once at command-buffer start, before draw state. */
static inline void pm4_init_default_hw_state(struct PM4Builder* b) {
    /* ClearContextState preamble (gnmdriver.cpp ClearStateSequence) */
    static const uint32_t clear_state[] = {
        0xc0012800u, 0x80000000u, 0x80000000u, 0xc0001200u, 0u, 0xc0055800u,
        0x2ec47fc0u, 0xffffffffu, 0u,          0u,          0u, 10u,
    };
    for (unsigned i = 0; i < sizeof(clear_state)/sizeof(clear_state[0]); i++)
        pm4_emit(b, clear_state[i]);

    /* Base InitSequence register defaults (gnmdriver_init.h), minus the 2-dword
       IT_CLEAR_STATE header which ClearContextState already covers above. */
    static const uint32_t init_seq[] = {
        0xc0017600u, 0x216u, 0xffffffffu,
        0xc0017600u, 0x217u, 0xffffffffu,
        0xc0017600u, 0x215u, 0u,
        0xc0016900u, 0x2f9u, 0x2du,
        0xc0016900u, 0x282u, 8u,
        0xc0016900u, 0x280u, 0x80008u,
        0xc0016900u, 0x281u, 0xffff0000u,
        0xc0016900u, 0x204u, 0u,
        0xc0016900u, 0x206u, 0x43fu,
        0xc0016900u, 0x83u,  0xffffu,
        0xc0016900u, 0x317u, 0x10u,
        0xc0016900u, 0x2fau, 0x3f800000u,
        0xc0016900u, 0x2fcu, 0x3f800000u,
        0xc0016900u, 0x2fbu, 0x3f800000u,
        0xc0016900u, 0x2fdu, 0x3f800000u,
        0xc0016900u, 0x202u, 0xcc0010u,
        0xc0016900u, 0x30eu, 0xffffffffu,
        0xc0016900u, 0x30fu, 0xffffffffu,
        0xc0002f00u, 1u,
        0xc0017600u, 7u,     0x1ffu,
        0xc0017600u, 0x46u,  0x1ffu,
        0xc0017600u, 0x87u,  0x1ffu,
        0xc0017600u, 0xc7u,  0x1ffu,
        0xc0017600u, 0x107u, 0u,
        0xc0017600u, 0x147u, 0x1ffu,
        0xc0016900u, 0x1b1u, 2u,
        0xc0016900u, 0x101u, 0u,
        0xc0016900u, 0x100u, 0xffffffffu,
        0xc0016900u, 0x103u, 0u,
        0xc0016900u, 0x284u, 0u,
        0xc0016900u, 0x290u, 0u,
        0xc0016900u, 0x2aeu, 0u,
        0xc0016900u, 0x292u, 0u,
        0xc0016900u, 0x293u, 0x6000000u,
        0xc0016900u, 0x2f8u, 0u,
        0xc0016900u, 0x2deu, 0x1e9u,
        0xc0036900u, 0x295u, 0x100u, 0x100u, 4u,
        0xc0017900u, 0x200u, 0xe0000000u,
    };
    for (unsigned i = 0; i < sizeof(init_seq)/sizeof(init_seq[0]); i++)
        pm4_emit(b, init_seq[i]);
}

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
    pm4_emit(b, 0x0504u);                              // CACHE_FLUSH_TS(4), event_index=5
    pm4_emit(b, (uint32_t)(addr & 0xFFFFFFFFu));        // address_lo
    pm4_emit(b, (uint32_t)(addr >> 32) | 0x22000000u);  // addr_hi | data_sel=1(Data32) | int_sel=2(IRQ on write)
    pm4_emit(b, fence_value);                           // data_lo
    pm4_emit(b, 0);                                     // data_hi
}

static inline void pm4_nop(struct PM4Builder* b, uint32_t count) {
    if (count == 0) return;
    pm4_emit(b, pm4_type3(PM4_NOP, count));
    for (uint32_t i = 0; i < count; i++) pm4_emit(b, 0);
}

/* PM4 DMA_DATA (opcode 0x50): fill GPU memory with a constant value.
   Unlike CPU memset, this goes through the rasterizer's FillBuffer path which
   invalidates any cached Vulkan image view for the target memory range. Required
   when the target is also sampled as a texture elsewhere in the frame (e.g.,
   shadow_depth: written by shadow CB, sampled by main floor PS). CPU memset
   would leave stale cached pixels in shadPS4's texture cache, producing ghost
   silhouettes from previous frames.
   Layout per PS4 PM4 DMA_DATA packet:
     header = type3(0x50, 6)
     dw1    = control: src_sel=2(Data) at [30:29], dst_sel=0(Memory) at [21:20]
     dw2    = src data (value to fill)
     dw3    = src_addr_hi (unused for Data source)
     dw4    = dst_addr_lo
     dw5    = dst_addr_hi
     dw6    = command: num_bytes at [20:0] */
static inline void pm4_dma_fill(struct PM4Builder* b, void* dst,
                                uint32_t num_bytes, uint32_t fill_value) {
    uint64_t daddr = (uint64_t)(uintptr_t)dst;
    pm4_emit(b, pm4_type3(0x50, 6));
    pm4_emit(b, (2u << 29) | (0u << 20));               /* src_sel=Data, dst_sel=Memory */
    pm4_emit(b, fill_value);                            /* fill data */
    pm4_emit(b, 0);                                     /* src_addr_hi (unused) */
    pm4_emit(b, (uint32_t)(daddr & 0xFFFFFFFFu));       /* dst_addr_lo */
    pm4_emit(b, (uint32_t)(daddr >> 32));               /* dst_addr_hi */
    pm4_emit(b, num_bytes & 0x1FFFFFu);                 /* command.num_bytes */
}
#define CTX_POLYGON_CONTROL         0x205  // PA_SU_SC_MODE_CNTL

/* IT_INDEX_TYPE (opcode 0x2A): set index buffer format */
static inline void pm4_index_type(struct PM4Builder* b, uint32_t type) {
    /* type: 0 = uint16, 1 = uint32 */
    pm4_emit(b, 0xC0002A00);
    pm4_emit(b, type);
}

/* IT_DRAW_INDEX_2 (opcode 0x27): indexed draw */
static inline void pm4_acquire_mem(struct PM4Builder* b, uint32_t coher_cntl) {
    /* Flush CB + invalidate TC between render-to-texture phases.
       GCN Sea Islands PM4 spec: ACQUIRE_MEM(0x58), 6 body dwords */
    pm4_emit(b, pm4_type3(PM4_ACQUIRE_MEM, 6));
    pm4_emit(b, coher_cntl);    /* CP_COHER_CNTL */
    pm4_emit(b, 0xFFFFFFFF);    /* CP_COHER_SIZE = full range */
    pm4_emit(b, 0);             /* CP_COHER_SIZE_HI */
    pm4_emit(b, 0);             /* CP_COHER_BASE_LO */
    pm4_emit(b, 0);             /* CP_COHER_BASE_HI */
    pm4_emit(b, 10);            /* POLL_INTERVAL */
}
static inline void pm4_draw_index_2(struct PM4Builder* b, uint32_t max_size,
                                     uint64_t index_base, uint32_t index_count) {
    pm4_emit(b, 0xC0042700);
    pm4_emit(b, max_size);
    pm4_emit(b, (uint32_t)(index_base & 0xFFFFFFFF));
    pm4_emit(b, (uint32_t)(index_base >> 32));
    pm4_emit(b, index_count);
    pm4_emit(b, 0); /* draw_initiator: SOURCE_SELECT=DMA (indexed) */
}
