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
/* CP_COHER_CNTL bits for ACQUIRE_MEM / surface-sync. */
#define COHER_TCL1_ACTION_ENA   (1u<<22)  /* invalidate vector L1 */
#define COHER_TC_ACTION_ENA     (1u<<23)  /* invalidate/flush L2  */
#define COHER_CB_ACTION_ENA     (1u<<25)  /* flush colour pipe    */
#define COHER_DB_ACTION_ENA     (1u<<26)  /* flush depth pipe     */
#define COHER_SH_KCACHE_ENA     (1u<<27)  /* scalar K$            */
#define COHER_SH_ICACHE_ENA     (1u<<28)  /* instruction I$       */
#define COHER_CB_DEST_BASE_ENA  (1u<<6)
/* Render-target -> texture barrier: TCL1 with TC (invalidating L2 without the vector L1 leaves
   stale texels there; Mesa's radeonsi sets TCL1 whenever it invalidates L2, and the firmware's
   CLEAR_STATE ACQUIRE_MEM uses coher_cntl 0x2ec47fc0: TCL1, TC, CB, DB, K$). */
#define COHER_RT_TO_TEXTURE   (COHER_CB_ACTION_ENA | COHER_DB_ACTION_ENA | \
                               COHER_TC_ACTION_ENA | COHER_TCL1_ACTION_ENA | \
                               COHER_SH_KCACHE_ENA | COHER_CB_DEST_BASE_ENA)
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
/* PA_SC_WINDOW_OFFSET (R_028200 -> index 0x080): no init function sets it, so it is zeroed here,
   and every scissor sets WINDOW_OFFSET_DISABLE. */
#define CTX_WINDOW_OFFSET           0x080
#define CTX_COLOR_TARGET_MASK       0x08E
#define CTX_COLOR_SHADER_MASK       0x08F
#define CTX_GENERIC_SCISSOR         0x090  // 2 dwords
#define CTX_VIEWPORT_SCISSOR0       0x094  // 2 dwords
/* PA_SC_VPORT_ZMIN_0 / ZMAX_0: the viewport depth range, two float registers the application owns
   (neither init function nor CLEAR_STATE sets them); unset, a stale or 0/0 range clips or flattens
   the scene. */
#define CTX_VIEWPORT_ZMIN0          0x0B4  // 2 dwords: ZMIN then ZMAX
/* R_0282D0 / R_0282D4 (Mesa's Sea Islands map) -> context indices 0x0B4 / 0x0B5; written like
   Mesa: ZMIN 0, ZMAX 1.0 (0x3f800000). */
#define WRITE_VIEWPORT_DEPTH_RANGE  1
#define CTX_INDEX_OFFSET            0x102
/* VGT_GS_ONCHIP_CNTL. Mesa/radv, guarded by chip_class >= CIK:
       "If this is 0, Bonaire can hang even if GS isn't being used.
        Other chips are unaffected. These are suboptimal values, but we
        don't use on-chip GS."
       R_028A44_VGT_GS_ONCHIP_CNTL = ES_VERTS_PER_SUBGRP(64) |
                                     GS_PRIMS_PER_SUBGRP(4)
   Byte 0x028A44 -> context index (0xA44)/4 = 0x291. Liverpool is CIK-family,
   we do not use GS either, and NEITHER gnm init function sets this register -
   the firmware writes 0x290 (VGT_GS_MODE) and stops. A documented
   hardware hang from a register nobody initialises is worth closing.
   ES_VERTS_PER_SUBGRP is bits [10:0], GS_PRIMS_PER_SUBGRP bits [21:11]. */
#define CTX_VGT_GS_ONCHIP_CNTL      0x291
#define VGT_GS_ONCHIP_CNTL_SAFE     (64u | (4u << 11))   /* = 0x2040 */
#define CTX_STENCIL_CONTROL         0x10B
#define CTX_STENCIL_REF_FRONT       0x10C
#define CTX_STENCIL_REF_BACK        0x10D
#define CTX_VIEWPORT0               0x10F  // 6 dwords (xscale,xoff,yscale,yoff,zscale,zoff)
#define CTX_PS_INPUT_CNTL_0         0x191
#define CTX_VS_OUTPUT_CONFIG        0x1B1
#define CTX_PS_INPUT_ENA            0x1B3
#define CTX_PS_INPUT_ADDR           0x1B4
#define CTX_NUM_INTERP              0x1B6
/* SPI_BARYC_CNTL. gnm's canonical PS setup (sub_0x24b0, the function
   sceGnmSetEmbeddedPsShader tail-jumps to) writes this register from the
   PsStageRegisters struct at +0x24, and NOTHING in
   DrawInitDefaultHardwareState350 or DrawInitToDefaultContextState400 sets it -
   gnm expects the app's PS setup to own it. We never wrote it, so it held
   whatever CLEAR_STATE left. It selects which barycentric sets the SPI
   provides (PERSP_CENTER/CENTROID/SAMPLE, LINEAR_*) and POS_FLOAT_LOCATION. */
#define CTX_SPI_BARYC_CNTL          0x1B8
#define CTX_DB_SHADER_CONTROL       0x203
#define CTX_SHADER_POS_FORMAT       0x1C3
#define CTX_Z_EXPORT_FORMAT         0x1C4
#define CTX_COLOR_EXPORT_FORMAT     0x1C5
#define CTX_BLEND_CONTROL0          0x1E0
#define CTX_DEPTH_CONTROL           0x200
#define CTX_COLOR_CONTROL           0x202
#define CTX_CLIPPER_CONTROL         0x204
#define CTX_VIEWPORT_CONTROL        0x206
#define CTX_VS_OUTPUT_CONTROL       0x207
#define CTX_MODE_CONTROL            0x292  // AMD: PA_SC_MODE_CNTL_0
#define CTX_INDEX_SIZE              0x29D  // AMD: VGT_DMA_SIZE
#define CTX_DB_DEPTH_INFO           0x00F  // AMD: DB_DEPTH_INFO
#define CTX_STAGE_ENABLE            0x2D5  // AMD: VGT_SHADER_STAGES_EN
#define CTX_AA_CONFIG               0x2F8
/* MSAA state (AMD gfx_7_2_d.h offsets - 0xA000; used by emit_msaa_state in main.c). */
#define CTX_DB_RENDER_OVERRIDE2 0x004
#define CTX_DB_EQAA 0x201
#define CTX_PA_SC_CENTROID_PRIORITY_0 0x2F5   /* _0, _1 */
#define CTX_PA_SC_AA_SAMPLE_LOCS_X0Y0_0 0x2FE /* 16 regs: X0Y0_0..3, X1Y0, X0Y1, X1Y1 */
#define CTX_PA_SC_AA_MASK_X0Y0_X1Y0 0x30E     /* and X0Y1_X1Y1 at 0x30F */
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
    uint32_t overflow;   /* dwords dropped because the buffer was full */
};

static inline void pm4_init(struct PM4Builder* b, uint32_t* buffer, uint32_t capacity_dwords) {
    b->buf = buffer;
    b->off = 0;
    b->cap = capacity_dwords;
    b->overflow = 0;
}

/* Room is checked before a completion packet, as the game does (no-flip: 6 dwords, flip: 0x40):
   pm4_emit would truncate it, and the flip marker must sit at dcb[size_dw - 0x40] for gnm's
   patcher. These report failure so the caller can refuse to submit. */
static inline int pm4_have_space(const struct PM4Builder* b, uint32_t dwords) {
    return (b->cap - b->off) >= dwords;
}


/* Silently dropping on overflow is dangerous here, not merely lossy: the flip
   marker MUST be the last 64 dwords of the DCB because gnm's patcher reads
   dcb[size_dw - 0x40]. If the buffer filled mid-build, prepare_flip's dwords
   would be dropped, b->off would stop advancing, and the patcher would rewrite
   whatever 64 dwords happen to sit at that offset - corrupting the command
   stream in a way that is invisible from the CPU side. So count the drops. */
static inline void pm4_emit(struct PM4Builder* b, uint32_t val) {
    if (b->off < b->cap) b->buf[b->off++] = val;
    else                 b->overflow++;
}

/* GPU-side checkpoint: the retail GPU status queries are stubs, so each checkpoint makes the CP
   store a distinct value to a CPU-visible dword; after a hang the last value is the last packet it
   retired. Header 0xc0033700 (the packet gnm's own marker patcher emits):
     [1] control: DST_SEL=5 (memory) << 8 | WR_CONFIRM (1<<20) | ENGINE ME
     [2] addr lo   [3] addr hi   [4] value */
static inline void pm4_write_data_dword(struct PM4Builder* b,
                                        volatile uint32_t* dst, uint32_t value) {
    uint64_t a = (uint64_t)(uintptr_t)dst;
    pm4_emit(b, pm4_type3(PM4_WRITE_DATA, 4));
    pm4_emit(b, (5u << 8) | (1u << 20));          /* DST_SEL=memory, WR_CONFIRM */
    pm4_emit(b, (uint32_t)(a & 0xFFFFFFFCu));
    pm4_emit(b, (uint32_t)(a >> 32) & 0xFFFFu);
    pm4_emit(b, value);
}

/* Set to 0 to remove all checkpoint packets from the command stream. */
#define GPU_CHECKPOINTS 1

static inline void pm4_emit_f(struct PM4Builder* b, float val) {
    union { float f; uint32_t u; } conv;
    conv.f = val;
    pm4_emit(b, conv.u);
}

// --- Packet emitters ---

/* Default hardware-state init: the sequence sceGnmDrawInitDefaultHardwareState emits (shadPS4
   gnmdriver.cpp ClearContextState, gnmdriver_init.h base InitSequence - the non-Neo variant for a
   base PS4). Required before any draw on hardware: undefined context registers hang the GPU.
   Emitted once at command-buffer start, before draw state. */
/* Declared locally so pm4.h does not depend on nid_resolve.h include order. */
extern int sceGnmDrawInitDefaultHardwareState350(uint32_t *cmd, uint32_t sizeInDwords);

/* The driver's own default hardware state (sceGnmDrawInitDefaultHardwareState350), correct by
   construction; a zero return means the driver refused (size < 0x100 dwords), seen as a short
   DCB. */
/* The leading tag NOP the game puts at the start of every command buffer: 0xc0031000 (TYPE3 NOP,
   4 payload dwords; the game pads to an 8-byte boundary), 0x68753000 (tag), 0xbadc0de (magic), the
   tag id, and the frame counter where the game has its fence counter - our fence lives in its own
   ONION allocation. gnm's marker gate accepts only 0x68750777..0x68750781, so the CP skips it. */
static inline void pm4_leading_tag(struct PM4Builder* b, uint32_t tag_id,
                                   uint32_t counter) {
    if (!pm4_have_space(b, 5)) { b->overflow++; return; }
    pm4_emit(b, pm4_type3(PM4_NOP, 4));   /* = 0xc0031000, align 0 */
    pm4_emit(b, 0x68753000u);             /* the game's tag */
    pm4_emit(b, 0x0badc0deu);             /* magic */
    pm4_emit(b, tag_id);
    pm4_emit(b, counter);
}

static int g_hw_state_done = 0;

/* INIT_STATE_ONCE: the full default hardware state only on the first frame - context state
   persists between submits (the game never calls sceGnmDrawInitDefaultHardwareState350). gnm's own
   preamble still issues CLEAR_STATE and a full-range cache invalidate once per frame. */
#define INIT_STATE_ONCE 1

static inline void pm4_init_default_hw_state_always(struct PM4Builder* b) {
    uint32_t avail = b->cap - b->off;
    if (avail < 0x100) return;
    int written = sceGnmDrawInitDefaultHardwareState350(b->buf + b->off, avail);
    if (written > 0) b->off += (uint32_t)written;
}

static inline void pm4_init_default_hw_state(struct PM4Builder* b) {
#if INIT_STATE_ONCE
    if (g_hw_state_done) return;
#endif
    pm4_init_default_hw_state_always(b);
}

/* A hand-written default-state table: the fallback if sceGnmDrawInitDefaultHardwareState350
   refuses (it differs from the firmware in nine registers). */
__attribute__((unused))
static void pm4_init_default_hw_state_local(struct PM4Builder* b) {
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
    if (!pm4_have_space(b, 6)) { b->overflow++; return; }   /* game checks < 6 */
    pm4_emit(b, pm4_type3(PM4_EVENT_WRITE_EOP, 5));
    pm4_emit(b, 0x0504u);                              // CACHE_FLUSH_TS(4), event_index=5
    /* The fence address is 4-byte aligned (the EOP address field's low two bits are reserved), as
       the game masks it. */
    pm4_emit(b, (uint32_t)(addr & 0xFFFFFFFCu));        // address_lo, 4-byte aligned
    /* addr_hi | data_sel Data32Low | int_sel None (shadPS4 PM4CmdEventWriteEop: address_hi [0:16],
       int_sel [24:26], data_sel [29:32]): write the 32-bit fence value, no interrupt - as the game.
     */
    pm4_emit(b, (uint32_t)(addr >> 32) | 0x20000000u);
    pm4_emit(b, fence_value);                           // data_lo
    pm4_emit(b, 0);                                     // data_hi
}

/* GPU timestamp: EVENT_WRITE_EOP, BOTTOM_OF_PIPE_TS (40, shadPS4 BottomOfPipeTs),
   EVENT_INDEX 5, DATA_SEL 3 = "send 64bit GPU counter value" (cikd.h), INT_SEL 0.
   The GPU writes its 64-bit clock to addr once all prior work has completed.
   addr must be 8-byte aligned. */
static inline void pm4_gpu_timestamp(struct PM4Builder* b, volatile uint64_t* ts_addr) {
    uint64_t addr = (uint64_t)(uintptr_t)ts_addr;
    if (!pm4_have_space(b, 6)) { b->overflow++; return; }
    pm4_emit(b, pm4_type3(PM4_EVENT_WRITE_EOP, 5));
    pm4_emit(b, 40u | (5u << 8));
    pm4_emit(b, (uint32_t)(addr & 0xFFFFFFF8u));
    pm4_emit(b, (uint32_t)(addr >> 32) | (3u << 29));
    pm4_emit(b, 0);
    pm4_emit(b, 0);
}

static inline void pm4_nop(struct PM4Builder* b, uint32_t count) {
    if (count == 0) return;
    pm4_emit(b, pm4_type3(PM4_NOP, count));
    for (uint32_t i = 0; i < count; i++) pm4_emit(b, 0);
}

/* prepareFlip marker block, the game's layout: 0xc03e1000 (64-dword TYPE3 NOP), 0x68750778 (flip
   marker), fence address lo / hi, fence value - and no EVENT_WRITE_EOP. gnm's patcher rewrites it
   in place into WRITE_DATA(vo_label[bufIdx] = 1) + WRITE_DATA(fence = value) +
   sceVideoOutSubmitEopFlip, so the fence write and the flip belong to the same submit. It must be
   the DCB's last 64 dwords: the patcher reads dcb[size_dw - 0x40]. */
/* Marker tags (gnm's patcher): 0x68750777 NOP, no EOP; 0x68750778 WRITE_DATA label + WRITE_DATA
   fence, no EOP, no interrupt (the game's, used here); 0x68750780 EOP IrqOnly, no data; 0x68750781
   EOP IrqWhenWriteConfirm, one datum - it writes the label only, so the fence never advances: keep
   FLIP_TAG_IRQ at 0. */
#define FLIP_TAG_IRQ 0
#if FLIP_TAG_IRQ
#define PM4_PREPARE_FLIP_TAG 0x68750781u
#else
#define PM4_PREPARE_FLIP_TAG 0x68750778u
#endif
static inline void pm4_prepare_flip(struct PM4Builder* b,
                                    volatile uint32_t* fence_addr,
                                    uint32_t fence_value) {
    uint64_t addr = (uint64_t)(uintptr_t)fence_addr;
    /* 64 dwords or nothing: a partial marker is worse than none, because the
       patcher would rewrite whatever sits at dcb[size_dw - 0x40] instead.
       The overflow counter makes the caller refuse to submit. */
    if (!pm4_have_space(b, 64)) { b->overflow++; return; }
    pm4_emit(b, pm4_type3(PM4_NOP, 63));                 // +0x00 = 0xc03e1000
    pm4_emit(b, PM4_PREPARE_FLIP_TAG);                   // +0x04 marker
    pm4_emit(b, (uint32_t)(addr & 0xFFFFFFFFu));         // +0x08 p0 = fence lo
    pm4_emit(b, (uint32_t)(addr >> 32));                 // +0x0c p1 = fence hi
    pm4_emit(b, fence_value);                            // +0x10 p2 = fence value
    for (uint32_t i = 0; i < 59; i++) pm4_emit(b, 0);    // 64 dwords total
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
/* DMA_DATA memory -> memory copy (as pm4_dma_fill, with src_sel Memory: dwords 2-3 are a source
   address). Batching uses it to copy each sub-frame's MVPs and cube vertices into the shared
   vertex buffer at GPU time. PM4DmaData: src_sel bits 29-30, dst_sel bits 20-21, cp_sync bit 31,
   num_bytes = command & 0x1fffff. cp_sync is required: without it the CP runs ahead of the DMA and
   the following draws can read stale contents. */
static inline void pm4_dma_copy(struct PM4Builder* b, void* dst, const void* src,
                                uint32_t num_bytes) {
    uint64_t daddr = (uint64_t)(uintptr_t)dst;
    uint64_t saddr = (uint64_t)(uintptr_t)src;
    pm4_emit(b, pm4_type3(0x50, 6));
    pm4_emit(b, (1u << 31) | (0u << 29) | (0u << 20));  /* cp_sync=1, src_sel=Memory, dst_sel=Memory */
    pm4_emit(b, (uint32_t)(saddr & 0xFFFFFFFFu));       /* src_addr_lo */
    pm4_emit(b, (uint32_t)(saddr >> 32));               /* src_addr_hi */
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
