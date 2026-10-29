/* A2M_TEST: alpha-to-mask (DB_ALPHA_TO_MASK, GCN "alpha to coverage") on the PS4 GPU, run once
 * before the main loop, then the visual pages (A2M visual) over the frame.
 *
 * Every pass is one command buffer on a 64 x 64 target of ns samples (RGBA16F colour, tile index
 * 13, no FMASK / CMASK, and Z_32_FLOAT depth, ARRAY_1D_TILED_THIN1, no HTILE - the scene's MSAA
 * layout):
 *   clear draw   all samples: colour 0.25 (ps clr), depth 1.0 (DB_RENDER_CONTROL DEPTH_CLEAR)
 *   test draw    the test's state and pixel shader (ps_a2m.h t_*), z 0.5, ZFUNC ALWAYS;
 *                ZPASS_DONE before / after it; COPY_DATA of the registers it set
 *   readback     ps rd: image_load of every sample of the colour and the depth, stored raw
 *   isolation    (first pass) the test draw again per sample s with PA_SC_AA_MASK = 1 << s,
 *                ZPASS_DONE around each
 *   end          PS_PARTIAL_FLUSH, ACQUIRE_MEM (CB, DB, TC, TCL1), two EVENT_WRITE_EOP (Mesa
 *                ac_emit_cp_release_mem: GFX7 needs two for all engines to be idle)
 * Every pixel has its own alpha A and second value B (tables at the V#). A pixel's colour mask has
 * bit s set when sample s reads back 1.0 (the test colour), its depth mask when it reads 0.5 (the
 * test z); any other raw value is unclassified, and an unwritten readback (0xA5A5A5A5) repeats the
 * pass. Every pass is checked against the model below (one or two hypotheses per test), the
 * ZPASS_DONE counts against the readback and the model, the isolation counts per sample likewise.
 *
 * Model (derived from the a2m-1 run on hardware, checked on every pixel here):
 *   p = (x & 1) + 2 (y & 1); off = 2 OFFSET_p + OFFSET_ROUND (eighths of a sample);
 *   n = min(1 << ALPHA_TO_MASK_NUM_SAMPLES, samples); a = alpha clamped to [0, 1], NaN 0;
 *   c(k) = min(k, floor(a k + off / 8));
 *   EQAA: for k = n, n / 2, .., 2 set bits [k / 2, k / 2 + c(k) - c(k / 2)); bit 0 if c(1);
 *   ALPHA_TO_MASK_EQAA_DISABLE: (1 << c(n)) - 1; the n-bit mask repeats: bit s = bit s % n.
 *
 * Results: ShadCube4 a2m.log next to the trace log. A hang (fence timeout) ends the run; the
 * state file a2m.state makes the next launch log it and continue after it. If R0 (the register
 * readback probe) hangs, the later tests run without COPY_DATA. A new A2_BUILD starts over (log
 * truncated); after the last test the run is not repeated until a2m.state is deleted. The visual
 * captures run on every launch. */
#include "ps_a2m.h"

#define A2_BUILD "a2m-5"
#define A2_TIMEOUT_US 2000000u
#define A2_W 64
#define A2_PX (A2_W * A2_W)
#define A2_QUADS 1024
#define A2_SPECIALS 128
#define A2_MAX_PASSES 12
#define A2_RETRIES 3
#define A2_OBMAX (A2_QUADS * (A2_MAX_PASSES + 1))
#define A2_DCB_DW 0x4000u
#define A2_OUT_DEPTH 0x8000u /* depth raw values, in dwords from the readback base */
#define A2_LOG_BUF 0x40000u
#define A2_ZP_STRIDE 256u /* ZPASS_DONE block: 16 bytes per RB (begin, end) */
#define A2_ZP_RBS 16
#define A2_ZP_BLOCKS 17 /* the test draw + 16 isolation draws */
#define A2_UNWRITTEN 0xA5A5A5A5u
#define A2_VIS_SLOTS 18
#define A2_TEX_W 256
#define A2_TEX_LEVELS 4 /* 256 .. 32: LINEAR_ALIGNED levels >= 32 px pack without padding */
#define A2_NOT_SHADED 0xFFFFFFFFu

/* CP_COHER_CNTL (AMD gfx7 register table) */
#define A2_COHER_DEST_BASE_0 (1u << 0)
#define A2_COHER_DB_DEST_BASE (1u << 14)
#define A2_COHER_TCL1 (1u << 22)
#define A2_COHER_TC (1u << 23)
#define A2_COHER_CB (1u << 25)
#define A2_COHER_DB (1u << 26)
#define A2_COHER_KCACHE (1u << 27)
#define A2_COHER_ICACHE (1u << 29)

/* Context register indices (register - 0x28000) / 4, AMD gfx7 register table. */
#define A2_DB_RENDER_CONTROL 0x000
#define A2_DB_COUNT_CONTROL 0x001
#define A2_DB_DEPTH_CLEAR 0x00B
#define A2_DB_DEPTH_CONTROL 0x200
#define A2_DB_EQAA 0x201
#define A2_DB_SHADER_CONTROL 0x203
#define A2_PA_SC_MODE_CNTL_1 0x293
#define A2_DB_ALPHA_TO_MASK 0x2DC
#define A2_CB_BLEND0_CONTROL 0x1E0
#define A2_PA_SC_WINDOW_OFFSET 0x080
#define A2_PA_SU_SC_MODE_CNTL 0x205

/* DB_ALPHA_TO_MASK: ENABLE [0], OFFSET0..3 [9:8] [11:10] [13:12] [15:14], OFFSET_ROUND [16]. */
#define A2M(en, o0, o1, o2, o3, r)                                                                 \
    ((uint32_t)(en) | ((uint32_t)(o0) << 8) | ((uint32_t)(o1) << 10) | ((uint32_t)(o2) << 12) |    \
     ((uint32_t)(o3) << 14) | ((uint32_t)(r) << 16))
#define A2_D0 A2M(1, 2, 2, 2, 2, 0) /* radeonsi / RADV without dithering */
#define A2_D1 A2M(1, 3, 1, 0, 2, 1) /* radeonsi / RADV dithered */
#define A2_OFF0 A2M(0, 2, 2, 2, 2, 0)

/* DB_SHADER_CONTROL bits */
#define A2_ZEXP (1u << 0)
#define A2_KILL (1u << 6)
#define A2_COV2MASK (1u << 7)
#define A2_MASKEXP (1u << 8)
#define A2_EXEC_NOOP (1u << 10)
#define A2_A2M_DIS (1u << 11)
#define A2_ZORDER(z) ((uint32_t)(z) << 4)

/* CB_BLEND0_CONTROL: SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ADD for colour and alpha, ENABLE. */
#define A2_BLEND_SRC_ALPHA (4u | (5u << 8) | (4u << 16) | (5u << 24) | (1u << 30))

enum {
    V_F32,
    V_F16,
    V_U16,
    V_RAW,
    V_ZA,
    V_AZ,
    V_ZAB,
    V_ZA0,
    V_MASK,
    V_KILL,
    V_M1,
    V_M1O,
    V_ITER,
    V_NOA,
    V_TEX,
    V_CLR,
    V_RD,
    V_VIS,
    A2_NSH
};
static const char* const k_a2_vname[A2_NSH] = {"t_f32 (MRT0 c, c, c, A)",
                                               "t_f16 (MRT0 pkrtz)",
                                               "t_u16 (MRT0 pknorm_u16)",
                                               "t_raw (MRT0 compressed, alpha = A[15:0])",
                                               "t_za (MRTZ.a = A, MRT0.a = table 4)",
                                               "t_az (MRTZ.a = table 5, MRT0.a = A)",
                                               "t_zab (MRTZ.a = B, MRT0.a = A)",
                                               "t_za0 (MRTZ.a = A, no MRT0 export)",
                                               "t_mask (MRTZ sample mask = B, MRT0.a = A)",
                                               "t_kill (kill where B & 1, MRT0.a = A)",
                                               "t_m1 (MRT0.a = table 4, MRT1.a = A)",
                                               "t_m1o (MRT1.a = A only)",
                                               "t_iter (MRT0.a = A + sample id x table 11)",
                                               "t_noa (MRT0 c, c, c, alpha not exported)",
                                               "t_tex (net texture alpha, perspective plane)",
                                               "clr",
                                               "rd",
                                               "vis"};
static const struct {
    const uint32_t* bin;
    uint32_t size, rsrc1;
} k_a2_sh[A2_NSH] = {
    {a2m_t_f32_bin, sizeof(a2m_t_f32_bin), A2M_T_F32_RSRC1},
    {a2m_t_f16_bin, sizeof(a2m_t_f16_bin), A2M_T_F16_RSRC1},
    {a2m_t_u16_bin, sizeof(a2m_t_u16_bin), A2M_T_U16_RSRC1},
    {a2m_t_raw_bin, sizeof(a2m_t_raw_bin), A2M_T_RAW_RSRC1},
    {a2m_t_za_bin, sizeof(a2m_t_za_bin), A2M_T_ZA_RSRC1},
    {a2m_t_az_bin, sizeof(a2m_t_az_bin), A2M_T_AZ_RSRC1},
    {a2m_t_zab_bin, sizeof(a2m_t_zab_bin), A2M_T_ZAB_RSRC1},
    {a2m_t_za0_bin, sizeof(a2m_t_za0_bin), A2M_T_ZA0_RSRC1},
    {a2m_t_mask_bin, sizeof(a2m_t_mask_bin), A2M_T_MASK_RSRC1},
    {a2m_t_kill_bin, sizeof(a2m_t_kill_bin), A2M_T_KILL_RSRC1},
    {a2m_t_m1_bin, sizeof(a2m_t_m1_bin), A2M_T_M1_RSRC1},
    {a2m_t_m1o_bin, sizeof(a2m_t_m1o_bin), A2M_T_M1O_RSRC1},
    {a2m_t_iter_bin, sizeof(a2m_t_iter_bin), A2M_T_ITER_RSRC1},
    {a2m_t_noa_bin, sizeof(a2m_t_noa_bin), A2M_T_NOA_RSRC1},
    {a2m_t_tex_bin, sizeof(a2m_t_tex_bin), A2M_T_TEX_RSRC1},
    {a2m_clr_bin, sizeof(a2m_clr_bin), A2M_CLR_RSRC1},
    {a2m_rd_bin, sizeof(a2m_rd_bin), A2M_RD_RSRC1},
    {a2m_vis_bin, sizeof(a2m_vis_bin), A2M_VIS_RSRC1},
};

/* Colour target formats: CB_COLOR0_INFO (FORMAT [6:2], NUMBER_TYPE [10:8]) and the readback T#
   (DATA_FORMAT, NUM_FORMAT); cmp: the colour mask is derived (the test colour reads 1.0). */
enum { CBF_F16, CBF_NONE, CBF_U8, CBF_R32F, CBF_U16I };
static const struct {
    const char* name;
    uint32_t info, dfmt, nfmt, cmp;
} k_a2_cbf[5] = {
    {"16_16_16_16 FLOAT", (0xCu << 2) | (7u << 8), 12, 7, 1},
    {"none", 0, 12, 7, 1},
    {"8_8_8_8 UNORM", (0xAu << 2) | (0u << 8), 10, 0, 1},
    {"32 FLOAT", (0x4u << 2) | (7u << 8), 4, 7, 1},
    {"16_16_16_16 UINT", (0xCu << 2) | (4u << 8), 12, 4, 0},
};

/* Alpha tables: the coarse table (128 specials, then 896 steps over [0, 1], one value per 2 x 2
   quad), one value everywhere, the visual ramp (quad q: q / 1023) and net, random values, or the
   16-bit sweep (pass j, quad q: raw value 1024 j + q). */
enum { AK_COARSE, AK_UNIFORM, AK_RAMP, AK_NET, AK_RANDOM, AK_SWEEP16, AK_TEX };
/* The second table B: none, a random alpha, a random sample mask, a random kill flag (bit 0). */
enum { BM_NONE, BM_ALPHA, BM_MASK, BM_KILL };
/* Hypotheses (h0 / h1) where the a2m-1 run left a question open. */
enum {
    H_NONE,
    H_COMBINE, /* MRT0.a and MRTZ.a both on: h0 AND of the masks, h1 mask of min(alpha) */
    H_EN0,     /* COVERAGE_TO_MASK with ENABLE 0 / DISABLE 1: h0 MRT0 path off, h1 on (AND) */
    H_ITER,    /* per-sample shading, alpha per sample: h0 bit s from its own alpha, h1 sample 0 */
    H_WIN,     /* window offset: h0 dither from screen x, y, h1 from window x, y */
    H_U16ONE,  /* UNORM16 export: h0 v / 65536, h1 the same but 65535 = 1.0 */
    H_SNORM,   /* SNORM16 export: h0 v / 32768, h1 v / 32767 (both: v <= 0 is 0) */
    H_MRT1,    /* h0 MRT0's alpha only, h1 MRT1's alpha */
    H_NOMODEL, /* UINT16 / SINT16 export: no model, the runs are logged */
    H_NOALPHA, /* MRT0 exported without alpha: h0 no alpha-to-mask, h1 alpha read as 0 */
};
static const char* const k_a2_hname[][2] = {
    {"model", "-"},
    {"AND of masks", "mask of min(alpha)"},
    {"MRT0 path off", "MRT0 path on"},
    {"own alpha per sample", "sample 0's alpha"},
    {"screen x, y", "window x, y"},
    {"v / 65536", "v / 65536, 65535 = 1"},
    {"v / 32768", "v / 32767"},
    {"MRT0 only", "MRT1's alpha"},
    {"no model", "-"},
    {"no alpha-to-mask", "alpha read as 0"},
};

struct A2Test {
    const char* id;
    const char* what;
    uint8_t kind;     /* AK_* */
    uint8_t ns;       /* coverage = colour = depth samples (16: coverage only, no targets) */
    uint8_t a2m_ns;   /* DB_EQAA.ALPHA_TO_MASK_NUM_SAMPLES; 0xFF: log2(ns) */
    uint8_t eqaa_dis; /* DB_EQAA.ALPHA_TO_MASK_EQAA_DISABLE */
    uint8_t msaa_en;  /* PA_SC_MODE_CNTL_0.MSAA_ENABLE; 2: ns > 1 */
    uint8_t ps_iter;  /* DB_EQAA.PS_ITER_SAMPLES (log2) + PA_SC_MODE_CNTL_1.PS_ITER_SAMPLE */
    uint8_t ps;       /* V_* */
    uint8_t cbf;      /* CBF_* */
    uint8_t nodepth;  /* no depth target */
    uint8_t refine;
    uint8_t show;  /* log the alpha table even when the model matches */
    uint8_t bmode; /* BM_* */
    uint8_t hyp;   /* H_* */
    uint8_t nocol; /* the colour plane is not modelled (blend, format conversions) */
    uint8_t vis;   /* visual slot + 1 */
    uint8_t risky;
    uint32_t colfmt;  /* SPI_SHADER_COL_FORMAT; 0: 9 (32_ABGR) for MRT0 */
    uint32_t cbsm;    /* CB_SHADER_MASK; 0: 0xF */
    uint32_t cbtm;    /* CB_TARGET_MASK; 0: 0xF */
    uint32_t a2m;     /* DB_ALPHA_TO_MASK; 0: A2_D0 */
    uint32_t dbsc;    /* DB_SHADER_CONTROL bits besides the variant's */
    uint32_t aa_mask; /* PA_SC_AA_MASK per pixel (16 bits); 0: all */
    uint32_t uni_alpha, mrt0a, mrtza, iter_delta, blend, wofs, raw_col, seed;
};
#define A2_ZERO_COL 0xFFFFFFFFu  /* colfmt for "COL0 ZERO" (0 means the default) */
#define A2_ZERO_MASK 0xFFFFFFFFu /* cbsm / cbtm for "0" */

#define A2_T(...)                                                                                  \
    {.ns = 4, .a2m_ns = 0xFF, .msaa_en = 2, .mrt0a = 0x3F800000u, .mrtza = 0x3F800000u, __VA_ARGS__}

#define A2_O0123 A2M(1, 0, 1, 2, 3, 0)
#define A2T16(i, w, fmt, col, h, m)                                                                \
    A2_T(.id = i, .what = w, .a2m = m, .ps = V_RAW, .colfmt = fmt, .kind = AK_SWEEP16,             \
         .raw_col = col, .nocol = 1, .hyp = h)

/* Designated initializers after A2_T's defaults override them (C11 6.7.9). */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winitializer-overrides"
static const struct A2Test k_a2_tests[] = {
    A2_T(.id = "R0", .what = "register readback probe (COPY_DATA); base state, 4x, D0"),
    /* gating, 4x, D0, t_f32 */
    A2_T(.id = "G01", .what = "ENABLE 0, ALPHA_TO_MASK_DISABLE 0", .a2m = A2_OFF0),
    A2_T(.id = "G02", .what = "ENABLE 1, ALPHA_TO_MASK_DISABLE 0 (base)", .refine = 1, .show = 1),
    A2_T(.id = "G03", .what = "ENABLE 0, ALPHA_TO_MASK_DISABLE 1", .a2m = A2_OFF0,
         .dbsc = A2_A2M_DIS),
    A2_T(.id = "G04", .what = "ENABLE 1, ALPHA_TO_MASK_DISABLE 1", .dbsc = A2_A2M_DIS),
    A2_T(.id = "G05", .what = "CB_SHADER_MASK MRT0 = 0x7 (no alpha)", .cbsm = 0x7),
    A2_T(.id = "G06", .what = "CB_TARGET_MASK MRT0 = 0x7 (no alpha)", .cbtm = 0x7),
    A2_T(.id = "G07", .what = "CB_TARGET_MASK MRT0 = 0 (no colour writes)", .cbtm = A2_ZERO_MASK,
         .nocol = 1),
    A2_T(.id = "G08", .what = "COL0 32_AR", .colfmt = 3, .nocol = 1),
    A2_T(.id = "G09", .what = "COL0 32_GR (no alpha exported)", .colfmt = 2, .nocol = 1),
    A2_T(.id = "G10", .what = "COL0 32_R (no alpha exported)", .colfmt = 1, .nocol = 1),
    A2_T(.id = "G11", .what = "COL0 ZERO (MRT0 export dropped), colour target bound",
         .colfmt = A2_ZERO_COL, .cbsm = A2_ZERO_MASK, .nocol = 1),
    A2_T(.id = "G12", .what = "no colour target, COL0 32_ABGR", .cbf = CBF_NONE),
    A2_T(.id = "G13", .what = "no colour target, COL0 ZERO", .cbf = CBF_NONE, .colfmt = A2_ZERO_COL,
         .cbsm = A2_ZERO_MASK),
    A2_T(.id = "G14", .what = "no depth target", .nodepth = 1),
    A2_T(.id = "G15", .what = "colour 8_8_8_8 UNORM", .cbf = CBF_U8, .refine = 1),
    A2_T(.id = "G16", .what = "colour 32 FLOAT (no alpha channel)", .cbf = CBF_R32F),
    A2_T(.id = "G17", .what = "colour 16_16_16_16 UINT (integer MRT0; colour raw only)",
         .cbf = CBF_U16I),
    A2_T(.id = "G18", .what = "export FP16_ABGR (pkrtz)", .ps = V_F16, .colfmt = 4, .refine = 1),
    A2_T(.id = "G19", .what = "export UNORM16_ABGR (pknorm_u16)", .ps = V_U16, .colfmt = 5,
         .refine = 1, .nocol = 1, .hyp = H_U16ONE),
    A2_T(.id = "G27", .what = "MRT0 exported without its alpha (exp en 0x7), COL0 32_ABGR",
         .ps = V_NOA, .hyp = H_NOALPHA),
    A2_T(.id = "G28", .what = "as G27, 8x D1", .ns = 8, .a2m = A2_D1, .ps = V_NOA,
         .hyp = H_NOALPHA),
    A2_T(.id = "G26", .what = "export UNORM16_ABGR (pknorm_u16), 4x offsets 0123 r0", .ps = V_U16,
         .colfmt = 5, .a2m = A2_O0123, .nocol = 1, .hyp = H_U16ONE),
    A2_T(.id = "G20", .what = "CB_SHADER_MASK MRT0 = 0x8 (alpha only)", .cbsm = 0x8, .nocol = 1),
    A2_T(.id = "G21", .what = "CB_SHADER_MASK MRT0 = 0x9 (red + alpha)", .cbsm = 0x9),
    A2_T(.id = "G22", .what = "no colour target, COL0 32_AR", .cbf = CBF_NONE, .colfmt = 3),
    A2_T(.id = "G23", .what = "CB_SHADER_MASK MRT0 = 0x1 (red only)", .cbsm = 0x1),
    A2_T(.id = "G24", .what = "blend SRC_ALPHA / ONE_MINUS_SRC_ALPHA on", .blend = 1, .nocol = 1),
    A2_T(.id = "G25", .what = "COL0 32_AR, CB_SHADER_MASK 0x9", .colfmt = 3, .cbsm = 0x9,
         .nocol = 1),
    /* sample counts, D0 / D1 */
    A2_T(.id = "S01", .what = "1x, MSAA_ENABLE 0, D0", .ns = 1, .refine = 1),
    A2_T(.id = "S02", .what = "1x, MSAA_ENABLE 1, D0", .ns = 1, .msaa_en = 1, .refine = 1),
    A2_T(.id = "S03", .what = "2x, D0", .ns = 2, .refine = 1),
    A2_T(.id = "S05", .what = "8x, D0", .ns = 8, .refine = 1),
    A2_T(.id = "S06", .what = "1x, D1", .ns = 1, .a2m = A2_D1, .refine = 1),
    A2_T(.id = "S07", .what = "2x, D1", .ns = 2, .a2m = A2_D1, .refine = 1),
    A2_T(.id = "S08", .what = "4x, D1", .a2m = A2_D1, .refine = 1),
    A2_T(.id = "S09", .what = "8x, D1", .ns = 8, .a2m = A2_D1, .refine = 1),
    A2_T(.id = "S10", .what = "4x, MSAA_ENABLE 0, D0", .msaa_en = 0, .refine = 1),
    A2_T(.id = "S11", .what = "8x, MSAA_ENABLE 0, D1", .ns = 8, .msaa_en = 0, .a2m = A2_D1),
    A2_T(.id = "S12", .what = "1x, MSAA_ENABLE 1, D1", .ns = 1, .msaa_en = 1, .a2m = A2_D1,
         .refine = 1),
    /* offsets */
    A2_T(.id = "O4a", .what = "4x, offsets 0000 round 0", .a2m = A2M(1, 0, 0, 0, 0, 0),
         .refine = 1),
    A2_T(.id = "O4b", .what = "4x, offsets 0000 round 1", .a2m = A2M(1, 0, 0, 0, 0, 1),
         .refine = 1),
    A2_T(.id = "O4c", .what = "4x, offsets 1111 round 0", .a2m = A2M(1, 1, 1, 1, 1, 0),
         .refine = 1),
    A2_T(.id = "O4d", .what = "4x, offsets 1111 round 1", .a2m = A2M(1, 1, 1, 1, 1, 1),
         .refine = 1),
    A2_T(.id = "O4e", .what = "4x, offsets 2222 round 1", .a2m = A2M(1, 2, 2, 2, 2, 1),
         .refine = 1),
    A2_T(.id = "O4f", .what = "4x, offsets 3333 round 0", .a2m = A2M(1, 3, 3, 3, 3, 0),
         .refine = 1),
    A2_T(.id = "O4g", .what = "4x, offsets 3333 round 1", .a2m = A2M(1, 3, 3, 3, 3, 1),
         .refine = 1),
    A2_T(.id = "O4h", .what = "4x, offsets 0123 round 0", .a2m = A2M(1, 0, 1, 2, 3, 0),
         .refine = 1),
    A2_T(.id = "O4i", .what = "4x, offsets 0123 round 1", .a2m = A2M(1, 0, 1, 2, 3, 1),
         .refine = 1),
    A2_T(.id = "O4j", .what = "4x, offsets 3102 round 0", .a2m = A2M(1, 3, 1, 0, 2, 0),
         .refine = 1),
    A2_T(.id = "O8a", .what = "8x, offsets 0000 round 0", .ns = 8, .a2m = A2M(1, 0, 0, 0, 0, 0),
         .refine = 1),
    A2_T(.id = "O8b", .what = "8x, offsets 0000 round 1", .ns = 8, .a2m = A2M(1, 0, 0, 0, 0, 1),
         .refine = 1),
    A2_T(.id = "O8c", .what = "8x, offsets 1111 round 0", .ns = 8, .a2m = A2M(1, 1, 1, 1, 1, 0),
         .refine = 1),
    A2_T(.id = "O8d", .what = "8x, offsets 1111 round 1", .ns = 8, .a2m = A2M(1, 1, 1, 1, 1, 1),
         .refine = 1),
    A2_T(.id = "O8e", .what = "8x, offsets 2222 round 1", .ns = 8, .a2m = A2M(1, 2, 2, 2, 2, 1),
         .refine = 1),
    A2_T(.id = "O8f", .what = "8x, offsets 3333 round 0", .ns = 8, .a2m = A2M(1, 3, 3, 3, 3, 0),
         .refine = 1),
    A2_T(.id = "O8g", .what = "8x, offsets 3333 round 1", .ns = 8, .a2m = A2M(1, 3, 3, 3, 3, 1),
         .refine = 1),
    A2_T(.id = "O8h", .what = "8x, offsets 0123 round 0", .ns = 8, .a2m = A2M(1, 0, 1, 2, 3, 0),
         .refine = 1),
    A2_T(.id = "O2h", .what = "2x, offsets 0123 round 0", .ns = 2, .a2m = A2M(1, 0, 1, 2, 3, 0),
         .refine = 1),
    A2_T(.id = "O1h", .what = "1x, offsets 0123 round 0", .ns = 1, .a2m = A2M(1, 0, 1, 2, 3, 0),
         .refine = 1),
    /* EQAA */
    A2_T(.id = "E4a", .what = "4x, ALPHA_TO_MASK_NUM_SAMPLES 0 (1), D1", .a2m = A2_D1, .a2m_ns = 0,
         .refine = 1),
    A2_T(.id = "E4b", .what = "4x, ALPHA_TO_MASK_NUM_SAMPLES 1 (2), D1", .a2m = A2_D1, .a2m_ns = 1,
         .refine = 1),
    A2_T(.id = "E4c", .what = "4x, ALPHA_TO_MASK_NUM_SAMPLES 3 (8), D1", .a2m = A2_D1, .a2m_ns = 3,
         .refine = 1),
    A2_T(.id = "E4d", .what = "4x, ALPHA_TO_MASK_EQAA_DISABLE 1, D1", .a2m = A2_D1, .eqaa_dis = 1,
         .refine = 1),
    A2_T(.id = "E4e", .what = "4x, EQAA_DISABLE 1, NUM_SAMPLES 1 (2), D1", .a2m = A2_D1,
         .eqaa_dis = 1, .a2m_ns = 1, .refine = 1),
    A2_T(.id = "E8a", .what = "8x, ALPHA_TO_MASK_NUM_SAMPLES 0 (1), D1", .ns = 8, .a2m = A2_D1,
         .a2m_ns = 0, .refine = 1),
    A2_T(.id = "E8b", .what = "8x, ALPHA_TO_MASK_NUM_SAMPLES 1 (2), D1", .ns = 8, .a2m = A2_D1,
         .a2m_ns = 1, .refine = 1),
    A2_T(.id = "E8c", .what = "8x, ALPHA_TO_MASK_NUM_SAMPLES 2 (4), D1", .ns = 8, .a2m = A2_D1,
         .a2m_ns = 2, .refine = 1),
    A2_T(.id = "E8d", .what = "8x, ALPHA_TO_MASK_EQAA_DISABLE 1, D1", .ns = 8, .a2m = A2_D1,
         .eqaa_dis = 1, .refine = 1),
    A2_T(.id = "E8e", .what = "8x, EQAA_DISABLE 1, NUM_SAMPLES 2 (4), D1", .ns = 8, .a2m = A2_D1,
         .eqaa_dis = 1, .a2m_ns = 2, .refine = 1),
    A2_T(.id = "P4", .what = "4x, PS_ITER_SAMPLES 2 (per-sample shading), D1", .a2m = A2_D1,
         .ps_iter = 2),
    A2_T(.id = "P8", .what = "8x, PS_ITER_SAMPLES 3 (per-sample shading), D1", .ns = 8,
         .a2m = A2_D1, .ps_iter = 3),
    /* the alpha source: MRT0.a or MRTZ.a */
    A2_T(.id = "M01", .what = "COVERAGE_TO_MASK 1; MRTZ.a = A, MRT0.a = 1.0", .ps = V_ZA,
         .dbsc = A2_COV2MASK, .refine = 1),
    A2_T(.id = "M02", .what = "COVERAGE_TO_MASK 1; MRTZ.a = 1.0, MRT0.a = A", .ps = V_AZ,
         .dbsc = A2_COV2MASK),
    A2_T(.id = "M03", .what = "COVERAGE_TO_MASK 0; MRTZ.a = A, MRT0.a = 1.0", .ps = V_ZA),
    A2_T(.id = "M04", .what = "COVERAGE_TO_MASK 0; MRTZ.a = 1.0, MRT0.a = A", .ps = V_AZ),
    A2_T(.id = "M05", .what = "COVERAGE_TO_MASK 1; MRTZ.a = A only, no colour target, COL0 ZERO",
         .ps = V_ZA0, .dbsc = A2_COV2MASK, .cbf = CBF_NONE, .colfmt = A2_ZERO_COL,
         .cbsm = A2_ZERO_MASK),
    A2_T(.id = "M06", .what = "COVERAGE_TO_MASK 1, ENABLE 0; MRTZ.a = A, MRT0.a = 1.0", .ps = V_ZA,
         .dbsc = A2_COV2MASK, .a2m = A2_OFF0),
    A2_T(.id = "M07", .what = "COVERAGE_TO_MASK 1; MRTZ.a = A, MRT0.a = 0.0", .ps = V_ZA,
         .dbsc = A2_COV2MASK, .mrt0a = 0),
    A2_T(.id = "M08", .what = "COVERAGE_TO_MASK 1, ALPHA_TO_MASK_DISABLE 1; MRTZ.a = A", .ps = V_ZA,
         .dbsc = A2_COV2MASK | A2_A2M_DIS),
    A2_T(.id = "M09", .what = "COVERAGE_TO_MASK 0; MRTZ.a = A only, no colour target, COL0 ZERO",
         .ps = V_ZA0, .cbf = CBF_NONE, .colfmt = A2_ZERO_COL, .cbsm = A2_ZERO_MASK),
    A2_T(.id = "M10", .what = "COVERAGE_TO_MASK 1, 8x D1; MRTZ.a = A, MRT0.a = 1.0", .ns = 8,
         .a2m = A2_D1, .ps = V_ZA, .dbsc = A2_COV2MASK, .refine = 1),
    /* both alphas vary (A, B random per pixel) */
    A2_T(.id = "C01", .what = "COVERAGE_TO_MASK 1, 4x D1; MRT0.a = A, MRTZ.a = B", .a2m = A2_D1,
         .ps = V_ZAB, .dbsc = A2_COV2MASK, .kind = AK_RANDOM, .bmode = BM_ALPHA, .hyp = H_COMBINE,
         .seed = 0xC01u),
    A2_T(.id = "C02", .what = "COVERAGE_TO_MASK 1, 8x D1; MRT0.a = A, MRTZ.a = B", .ns = 8,
         .a2m = A2_D1, .ps = V_ZAB, .dbsc = A2_COV2MASK, .kind = AK_RANDOM, .bmode = BM_ALPHA,
         .hyp = H_COMBINE, .seed = 0xC02u),
    A2_T(.id = "C03", .what = "COVERAGE_TO_MASK 1, ENABLE 0, 4x D1; MRT0.a = A, MRTZ.a = B",
         .a2m = A2_D1 & ~1u, .ps = V_ZAB, .dbsc = A2_COV2MASK, .kind = AK_RANDOM, .bmode = BM_ALPHA,
         .hyp = H_EN0, .seed = 0xC03u),
    A2_T(.id = "C04", .what = "COVERAGE_TO_MASK 1, ALPHA_TO_MASK_DISABLE 1, 4x D1; A, B",
         .a2m = A2_D1, .ps = V_ZAB, .dbsc = A2_COV2MASK | A2_A2M_DIS, .kind = AK_RANDOM,
         .bmode = BM_ALPHA, .hyp = H_EN0, .seed = 0xC04u),
    A2_T(.id = "C05", .what = "COVERAGE_TO_MASK 1, COL0 32_GR (no MRT0 alpha), 4x D1; A, B",
         .a2m = A2_D1, .ps = V_ZAB, .dbsc = A2_COV2MASK, .colfmt = 2, .kind = AK_RANDOM,
         .bmode = BM_ALPHA, .nocol = 1, .seed = 0xC05u),
    A2_T(.id = "C06", .what = "COVERAGE_TO_MASK 1, ENABLE 0, 8x D0; MRT0.a = A, MRTZ.a = B",
         .ns = 8, .a2m = A2_OFF0, .ps = V_ZAB, .dbsc = A2_COV2MASK, .kind = AK_RANDOM,
         .bmode = BM_ALPHA, .hyp = H_EN0, .seed = 0xC06u),
    /* MRT1 */
    A2_T(.id = "T01", .what = "MRT0.a = 1.0, MRT1.a = A (COL0 / COL1 32_ABGR)", .ps = V_M1,
         .colfmt = 0x99, .cbsm = 0xFF, .cbtm = 0xF, .hyp = H_MRT1),
    A2_T(.id = "T02", .what = "MRT1.a = A only (COL0 ZERO, COL1 32_ABGR)", .ps = V_M1O,
         .colfmt = 0x90, .cbsm = 0xF0, .nocol = 1, .hyp = H_MRT1),
    A2_T(.id = "T03", .what = "MRT0.a = 0.0, MRT1.a = A", .ps = V_M1, .colfmt = 0x99, .cbsm = 0xFF,
         .mrt0a = 0, .hyp = H_MRT1),
    /* per-sample shading with a per-sample alpha */
    A2_T(.id = "I01", .what = "4x D1, PS_ITER_SAMPLES 2, alpha = A + 0.125 sample id", .a2m = A2_D1,
         .ps = V_ITER, .ps_iter = 2, .iter_delta = 0x3E000000u, .hyp = H_ITER),
    A2_T(.id = "I02", .what = "8x D1, PS_ITER_SAMPLES 3, alpha = A + 0.0625 sample id", .ns = 8,
         .a2m = A2_D1, .ps = V_ITER, .ps_iter = 3, .iter_delta = 0x3D800000u, .hyp = H_ITER),
    A2_T(.id = "I03", .what = "2x D0, PS_ITER_SAMPLES 1, alpha = A + 0.25 sample id", .ns = 2,
         .ps = V_ITER, .ps_iter = 1, .iter_delta = 0x3E800000u, .hyp = H_ITER),
    /* the dither position with a window offset (uniform alpha 0.37890625: 4x 0123 round 0 gives
       0x4, 0x2, 0x6, 0x5 at p0..p3) */
    A2_T(.id = "W00", .what = "4x offsets 0123 r0, alpha 0.37890625, no window offset",
         .a2m = A2M(1, 0, 1, 2, 3, 0), .kind = AK_UNIFORM, .uni_alpha = 0x3EC20000u, .hyp = H_WIN),
    A2_T(.id = "W01", .what = "as W00, PA_SC_WINDOW_OFFSET (1, 0)", .a2m = A2M(1, 0, 1, 2, 3, 0),
         .kind = AK_UNIFORM, .uni_alpha = 0x3EC20000u, .wofs = 1, .hyp = H_WIN),
    A2_T(.id = "W02", .what = "as W00, PA_SC_WINDOW_OFFSET (0, 1)", .a2m = A2M(1, 0, 1, 2, 3, 0),
         .kind = AK_UNIFORM, .uni_alpha = 0x3EC20000u, .wofs = 1u << 16, .hyp = H_WIN),
    A2_T(.id = "W03", .what = "as W00, PA_SC_WINDOW_OFFSET (1, 1)", .a2m = A2M(1, 0, 1, 2, 3, 0),
         .kind = AK_UNIFORM, .uni_alpha = 0x3EC20000u, .wofs = 1u | (1u << 16), .hyp = H_WIN),
    A2_T(.id = "W04", .what = "as W00, PA_SC_WINDOW_OFFSET (3, 6)", .a2m = A2M(1, 0, 1, 2, 3, 0),
         .kind = AK_UNIFORM, .uni_alpha = 0x3EC20000u, .wofs = 3u | (6u << 16), .hyp = H_WIN),
    /* sample mask export and kill with random per-pixel values */
    A2_T(.id = "K01", .what = "MASK_EXPORT B (random per pixel) + alpha-to-mask", .ps = V_MASK,
         .bmode = BM_MASK, .seed = 0x01u),
    A2_T(.id = "K02", .what = "MASK_EXPORT B, ENABLE 0 (reference)", .ps = V_MASK, .a2m = A2_OFF0,
         .bmode = BM_MASK, .seed = 0x02u),
    A2_T(.id = "K03", .what = "KILL where B & 1 (random) + alpha-to-mask", .ps = V_KILL,
         .bmode = BM_KILL, .seed = 0x03u),
    A2_T(.id = "K04", .what = "Z_ORDER EARLY_Z_THEN_LATE_Z", .dbsc = A2_ZORDER(1)),
    A2_T(.id = "K05", .what = "Z_ORDER RE_Z", .dbsc = A2_ZORDER(2)),
    A2_T(.id = "K06", .what = "Z_ORDER EARLY_Z_THEN_RE_Z", .dbsc = A2_ZORDER(3)),
    A2_T(.id = "K07", .what = "PA_SC_AA_MASK 0x5 per pixel + alpha-to-mask", .aa_mask = 0x5),
    A2_T(.id = "K08", .what = "Z_ORDER EARLY_Z_THEN_LATE_Z, 8x D1", .ns = 8, .a2m = A2_D1,
         .dbsc = A2_ZORDER(1)),
    A2_T(.id = "K09", .what = "MASK_EXPORT B + alpha-to-mask, 8x D1", .ns = 8, .a2m = A2_D1,
         .ps = V_MASK, .bmode = BM_MASK, .seed = 0x09u),
    /* one alpha for every quad */
    A2_T(.id = "U01", .what = "uniform alpha 0.3, 4x D1", .a2m = A2_D1, .kind = AK_UNIFORM,
         .uni_alpha = 0x3E99999Au),
    A2_T(.id = "U02", .what = "uniform alpha 0.6, 4x D1", .a2m = A2_D1, .kind = AK_UNIFORM,
         .uni_alpha = 0x3F19999Au),
    A2_T(.id = "U03", .what = "uniform alpha 0.45, 8x D1", .ns = 8, .a2m = A2_D1,
         .kind = AK_UNIFORM, .uni_alpha = 0x3EE66666u),
    A2_T(.id = "U04", .what = "uniform alpha 0.55, 8x D0", .ns = 8, .kind = AK_UNIFORM,
         .uni_alpha = 0x3F0CCCCDu),
    A2_T(.id = "U05", .what = "uniform alpha 0.4, 2x D1", .ns = 2, .a2m = A2_D1, .kind = AK_UNIFORM,
         .uni_alpha = 0x3ECCCCCDu),
    A2_T(.id = "U06", .what = "uniform alpha 0.5, 1x D1", .ns = 1, .a2m = A2_D1, .kind = AK_UNIFORM,
         .uni_alpha = 0x3F000000u),
    /* every 16-bit export value (raw, 64 passes): 4x offsets 0123 round 0 (offsets 0, 2, 4, 6 -
       offset 0 tells 65535 = 1.0 apart), and D1 (1, 3, 5, 7) */
    A2T16("X16f", "every FP16_ABGR alpha (raw half), 4x 0123 r0", 4, 0x3C00u, H_NONE, A2_O0123),
    A2T16("X16g", "every FP16_ABGR alpha (raw half), 4x D1", 4, 0x3C00u, H_NONE, A2_D1),
    A2T16("X16u", "every UNORM16_ABGR alpha (raw), 4x 0123 r0", 5, 0xFFFFu, H_U16ONE, A2_O0123),
    A2T16("X16v", "every UNORM16_ABGR alpha (raw), 4x D1", 5, 0xFFFFu, H_U16ONE, A2_D1),
    A2T16("X16s", "every SNORM16_ABGR alpha (raw), 4x 0123 r0", 6, 0x7FFFu, H_SNORM, A2_O0123),
    A2T16("X16t", "every SNORM16_ABGR alpha (raw), 4x D1", 6, 0x7FFFu, H_SNORM, A2_D1),
    A2T16("X16i", "every UINT16_ABGR alpha (raw), 4x 0123 r0", 7, 1u, H_NOMODEL, A2_O0123),
    A2T16("X16j", "every SINT16_ABGR alpha (raw), 4x 0123 r0", 8, 1u, H_NOMODEL, A2_O0123),
    /* the textured net (t_tex): the model takes the alpha the GPU sampled (also visual) */
    A2_T(.id = "N01", .what = "textured net, 4x D1", .a2m = A2_D1, .ps = V_TEX, .kind = AK_TEX,
         .vis = 1),
    A2_T(.id = "N02", .what = "textured net, 4x D1, A2M_NUM_SAMPLES 0 (Gnm default DB_EQAA)",
         .a2m = A2_D1, .a2m_ns = 0, .ps = V_TEX, .kind = AK_TEX, .vis = 2),
    A2_T(.id = "N03", .what = "textured net, 4x D0 (Gnm AlphaToMaskControl::init offsets)",
         .ps = V_TEX, .kind = AK_TEX, .vis = 3),
    A2_T(.id = "N04", .what = "textured net, 8x D1", .ns = 8, .a2m = A2_D1, .ps = V_TEX,
         .kind = AK_TEX, .vis = 4),
    A2_T(.id = "N05", .what = "textured net, 8x D1, A2M_NUM_SAMPLES 0 (Gnm default DB_EQAA)",
         .ns = 8, .a2m = A2_D1, .a2m_ns = 0, .ps = V_TEX, .kind = AK_TEX, .vis = 5),
    A2_T(.id = "N06", .what = "textured net, 1x D1", .ns = 1, .a2m = A2_D1, .ps = V_TEX,
         .kind = AK_TEX, .vis = 6),
    /* the visual pages' captures (also logged here) */
    A2_T(.id = "V01", .what = "ramp, 4x D1", .a2m = A2_D1, .kind = AK_RAMP, .vis = 7),
    A2_T(.id = "V02", .what = "ramp, 4x D0", .kind = AK_RAMP, .vis = 8),
    A2_T(.id = "V03", .what = "ramp, 8x D1", .ns = 8, .a2m = A2_D1, .kind = AK_RAMP, .vis = 9),
    A2_T(.id = "V04", .what = "ramp, 2x D1", .ns = 2, .a2m = A2_D1, .kind = AK_RAMP, .vis = 10),
    A2_T(.id = "V05", .what = "ramp, 1x D1", .ns = 1, .a2m = A2_D1, .kind = AK_RAMP, .vis = 11),
    A2_T(.id = "V06", .what = "ramp, 4x D1, EQAA_DISABLE", .a2m = A2_D1, .eqaa_dis = 1,
         .kind = AK_RAMP, .vis = 12),
    A2_T(.id = "V07", .what = "ramp, 4x D1, COVERAGE_TO_MASK (MRTZ.a only, no colour)",
         .a2m = A2_D1, .ps = V_ZA0, .dbsc = A2_COV2MASK, .cbf = CBF_NONE, .colfmt = A2_ZERO_COL,
         .cbsm = A2_ZERO_MASK, .kind = AK_RAMP, .vis = 13),
    A2_T(.id = "V08", .what = "ramp, 8x D1, ALPHA_TO_MASK_NUM_SAMPLES 2 (4)", .ns = 8, .a2m = A2_D1,
         .a2m_ns = 2, .kind = AK_RAMP, .vis = 14),
    A2_T(.id = "V09", .what = "net, 4x D1", .a2m = A2_D1, .kind = AK_NET, .vis = 15),
    A2_T(.id = "V10", .what = "net, 4x D0", .kind = AK_NET, .vis = 16),
    A2_T(.id = "V11", .what = "net, 8x D1", .ns = 8, .a2m = A2_D1, .kind = AK_NET, .vis = 17),
    A2_T(.id = "V12", .what = "net, 1x D1", .ns = 1, .a2m = A2_D1, .kind = AK_NET, .vis = 18),
    /* random configurations and alphas (RND, generated from the seed), then 16 coverage samples
       without targets (counted by ZPASS_DONE only; may hang) */
    A2_T(.id = "RND", .what = "random configurations (48), random alphas", .kind = AK_RANDOM,
         .seed = 0x5EEDu),
    A2_T(.id = "E16a", .what = "16 coverage samples, no targets, EXEC_ON_NOOP, D1", .ns = 16,
         .a2m = A2_D1, .cbf = CBF_NONE, .nodepth = 1, .dbsc = A2_EXEC_NOOP, .risky = 1),
    A2_T(.id = "E16b", .what = "16 coverage samples, no targets, EXEC_ON_NOOP, D0", .ns = 16,
         .cbf = CBF_NONE, .nodepth = 1, .dbsc = A2_EXEC_NOOP, .risky = 1),
    A2_T(.id = "E16c", .what = "16 coverage samples, EQAA_DISABLE, no targets, D1", .ns = 16,
         .a2m = A2_D1, .eqaa_dis = 1, .cbf = CBF_NONE, .nodepth = 1, .dbsc = A2_EXEC_NOOP,
         .risky = 1),
};
#pragma clang diagnostic pop
#define A2_NTESTS ((int)(sizeof(k_a2_tests) / sizeof(k_a2_tests[0])))
#define A2_RND_CONFIGS 48

/* ---- log ---- */
static int g_a2_fd = -1;
static char g_a2_buf[A2_LOG_BUF];
static unsigned g_a2_len;
static char g_a2_log_path[160], g_a2_state_path[160];

static void a2_paths(void) {
    const char* t = g_trace_path ? g_trace_path : "";
    int dir = 0;
    for (int i = 0; t[i] && i < 120; i++)
        if (t[i] == '/')
            dir = i + 1;
    static const char log_name[] = "ShadCube4 a2m.log", state_name[] = "a2m.state";
    for (int i = 0; i < dir; i++)
        g_a2_log_path[i] = g_a2_state_path[i] = t[i];
    for (int i = 0; i < (int)sizeof(log_name); i++)
        g_a2_log_path[dir + i] = log_name[i];
    for (int i = 0; i < (int)sizeof(state_name); i++)
        g_a2_state_path[dir + i] = state_name[i];
}
static void a2_flush(void) {
    if (g_a2_fd >= 0 && g_a2_len) {
        sceKernelWrite(g_a2_fd, g_a2_buf, g_a2_len);
        sceKernelFsync(g_a2_fd);
    }
    g_a2_len = 0;
}
static void a2_s(const char* s) {
    while (*s) {
        if (g_a2_len >= A2_LOG_BUF - 1)
            a2_flush();
        g_a2_buf[g_a2_len++] = *s++;
    }
}
static void a2_u(unsigned long long v) {
    char t[24];
    t[lg_u64(t, v)] = 0;
    a2_s(t);
}
static void a2_x(unsigned long long v, int digits) {
    static const char h[] = "0123456789abcdef";
    char t[20];
    int n = 0;
    t[n++] = '0';
    t[n++] = 'x';
    for (int i = digits - 1; i >= 0; i--)
        t[n++] = h[(v >> (i * 4)) & 0xf];
    t[n] = 0;
    a2_s(t);
}
static float a2_ftof(uint32_t b) {
    union {
        uint32_t u;
        float f;
    } c;
    c.u = b;
    return c.f;
}
static uint32_t a2_fbits(float f) {
    union {
        uint32_t u;
        float f;
    } c;
    c.f = f;
    return c.u;
}
/* A float as "0x3f000000 (0.500000000)": 9 decimals; NaN / inf / huge by name. */
static void a2_f(uint32_t b) {
    a2_x(b, 8);
    a2_s(" (");
    uint32_t e = (b >> 23) & 0xFF;
    if (e == 0xFF) {
        a2_s((b & 0x7FFFFF) ? "nan" : (b >> 31) ? "-inf" : "+inf");
    } else {
        double v = (double)a2_ftof(b & 0x7FFFFFFFu);
        if (b >> 31)
            a2_s("-");
        if (v >= 1e9) {
            a2_s(">=1e9");
        } else {
            unsigned long long ip = (unsigned long long)v;
            unsigned long long fr = (unsigned long long)((v - (double)ip) * 1e9 + 0.5);
            if (fr >= 1000000000ull) {
                ip++;
                fr -= 1000000000ull;
            }
            a2_u(ip);
            a2_s(".");
            char t[10];
            for (int i = 8; i >= 0; i--) {
                t[i] = (char)('0' + fr % 10);
                fr /= 10;
            }
            t[9] = 0;
            a2_s(t);
            if (v != 0.0 && v < 1e-6)
                a2_s(" tiny");
        }
    }
    a2_s(")");
}

/* ---- state file ---- */
/* inflight: the test running (or, | A2_CAPTURE, the visual capture) when the app ended;
   bad_variants: pixel shader variants that crashed or hung (A2_GROUP of the variant). */
#define A2_CAPTURE 0x10000
struct A2State {
    char build[16];
    int32_t next, inflight, regread_bad;
    uint32_t bad_variants;
};
static void a2_state_save(const struct A2State* st) {
    int fd = sceKernelOpen(g_a2_state_path, 0x601, 0x1FF);
    if (fd < 0)
        return;
    sceKernelWrite(fd, st, sizeof(*st));
    sceKernelFsync(fd);
    sceKernelClose(fd);
}
static int a2_state_load(struct A2State* st) {
    int fd = sceKernelOpen(g_a2_state_path, 0, 0);
    if (fd < 0)
        return 0;
    long n = sceKernelRead(fd, st, sizeof(*st));
    sceKernelClose(fd);
    if (n != (long)sizeof(*st))
        return 0;
    for (int i = 0; A2_BUILD[i] || st->build[i]; i++)
        if (A2_BUILD[i] != st->build[i])
            return 0;
    return 1;
}

static int a2_pop(uint32_t m) {
    int n = 0;
    for (; m; m &= m - 1)
        n++;
    return n;
}
static int a2_streq(const char* a, const char* b) {
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* ---- effective test fields ---- */
static uint32_t a2_colfmt(const struct A2Test* t) {
    return t->colfmt == A2_ZERO_COL ? 0u : t->colfmt ? t->colfmt : 9u;
}
static uint32_t a2_cbsm(const struct A2Test* t) {
    return t->cbsm == A2_ZERO_MASK ? 0u : t->cbsm ? t->cbsm : 0xFu;
}
static uint32_t a2_cbtm(const struct A2Test* t) {
    return t->cbtm == A2_ZERO_MASK ? 0u : t->cbtm ? t->cbtm : 0xFu;
}
static uint32_t a2_a2m(const struct A2Test* t) {
    return t->a2m ? t->a2m : A2_D0;
}
static uint32_t a2_aa(const struct A2Test* t) {
    return t->aa_mask ? t->aa_mask : 0xFFFFu;
}
static uint32_t a2_log2(uint32_t n) {
    uint32_t l = 0;
    while ((1u << l) < n)
        l++;
    return l;
}
static uint32_t a2_full(uint32_t ns) {
    return ns >= 32 ? 0xFFFFFFFFu : (1u << ns) - 1u;
}
static int a2_has_readback(const struct A2Test* t) {
    return t->ns <= 8;
}

/* ---- GPU memory ---- */
struct A2Ctx {
    uint32_t* dcb;
    volatile uint32_t* fence;
    uint32_t fv;
    uint32_t* alpha; /* A: 4096 values (x + 64 y), then B at byte 0x4000 */
    uint32_t* tab;   /* shader table (gen_ps_a2m.py) */
    uint32_t* out;   /* readback: colour raw, then depth raw at A2_OUT_DEPTH */
    uint32_t* zp;    /* ZPASS_DONE blocks: 0 the test draw, 1 + s isolation of sample s */
    uint32_t* regs;  /* COPY_DATA of the test draw's registers */
    void* color;     /* ns-sample colour */
    void* depth;     /* ns-sample depth */
    void* scratch;   /* the readback pass's 1-sample target */
    void* sh[A2_NSH];
    void* vs;
    void* vs_net; /* the textured net's vertex shader */
    void* tex;    /* the net texture: RGBA8 256 x 256, 4 levels, LINEAR_ALIGNED */
    float* verts; /* the net plane: 3 vertices (x, y, z, w, u, v, -, -) */
};
static struct A2Ctx g_a2c;
static int g_a2c_ok;

/* Registers the test draw sets, also read back with COPY_DATA (context index, name). The a2m-1
   run read constant garbage for the three marked unreliable while the draws used the written
   values. */
static const struct {
    uint16_t idx;
    uint8_t unreliable;
    const char* name;
} k_a2_rr[] = {
    {0x2DC, 0, "DB_ALPHA_TO_MASK"},
    {0x203, 1, "DB_SHADER_CONTROL"},
    {0x201, 1, "DB_EQAA"},
    {0x2F8, 0, "PA_SC_AA_CONFIG"},
    {0x292, 0, "PA_SC_MODE_CNTL_0"},
    {0x293, 0, "PA_SC_MODE_CNTL_1"},
    {0x001, 0, "DB_COUNT_CONTROL"},
    {0x1C5, 0, "SPI_SHADER_COL_FORMAT"},
    {0x1C4, 0, "SPI_SHADER_Z_FORMAT"},
    {0x08F, 0, "CB_SHADER_MASK"},
    {0x08E, 0, "CB_TARGET_MASK"},
    {0x31C, 0, "CB_COLOR0_INFO"},
    {0x31D, 0, "CB_COLOR0_ATTRIB"},
    {0x010, 0, "DB_Z_INFO"},
    {0x30E, 0, "PA_SC_AA_MASK_X0Y0_X1Y0"},
    {0x30F, 0, "PA_SC_AA_MASK_X0Y1_X1Y1"},
    {0x000, 0, "DB_RENDER_CONTROL"},
    {0x200, 1, "DB_DEPTH_CONTROL"},
    {0x1B3, 0, "SPI_PS_INPUT_ENA"},
    {0x1E0, 0, "CB_BLEND0_CONTROL"},
    {0x080, 0, "PA_SC_WINDOW_OFFSET"},
    {0x205, 0, "PA_SU_SC_MODE_CNTL"},
};
#define A2_NRR ((int)(sizeof(k_a2_rr) / sizeof(k_a2_rr[0])))
static uint32_t g_a2_written[A2_NRR], g_a2_wtest[A2_NRR]; /* last written; at the test draw */

static void a2_reg(struct PM4Builder* b, uint32_t idx, uint32_t v) {
    pm4_set_context_reg(b, idx, v);
    for (int i = 0; i < A2_NRR; i++)
        if (k_a2_rr[i].idx == idx)
            g_a2_written[i] = v;
}

/* PA_SC_AA_SAMPLE_LOCS nibble pairs (radeonsi si_state_msaa.c FILL_SREG). */
static uint32_t a2_locs(int s0x, int s0y, int s1x, int s1y, int s2x, int s2y, int s3x, int s3y) {
    return ((uint32_t)s0x & 0xF) | (((uint32_t)s0y & 0xF) << 4) | (((uint32_t)s1x & 0xF) << 8) |
           (((uint32_t)s1y & 0xF) << 12) | (((uint32_t)s2x & 0xF) << 16) |
           (((uint32_t)s2y & 0xF) << 20) | (((uint32_t)s3x & 0xF) << 24) |
           (((uint32_t)s3y & 0xF) << 28);
}

/* Rasterizer / DB sample state (radeonsi si_emit_msaa_config / si_state_msaa.c; the 4x pattern is
   the scene's, emit_msaa_state in main.c). */
static void a2_msaa(struct PM4Builder* b, uint32_t ns, uint32_t msaa_en, uint32_t a2m_ns,
                    uint32_t eqaa_dis, uint32_t ps_iter, uint32_t aa_mask) {
    static const uint32_t max_dist[5] = {0, 4, 6, 7, 7};
    uint32_t lg = a2_log2(ns), locs[16] = {0}, cp[2] = {0, 0};
    if (ns == 2) {
        uint32_t l = a2_locs(-4, -4, 4, 4, 0, 0, 0, 0);
        locs[0] = locs[4] = locs[8] = locs[12] = l;
        cp[0] = cp[1] = 0x10101010u;
    } else if (ns == 4) {
        uint32_t l = a2_locs(-2, -6, 6, -2, -6, 2, 2, 6);
        locs[0] = locs[4] = locs[8] = locs[12] = l;
        cp[0] = cp[1] = 0x32103210u;
    } else if (ns == 8) {
        uint32_t l0 = a2_locs(-3, -5, 5, 1, -1, 3, 7, -7), l1 = a2_locs(-7, -1, 3, 7, -5, 5, 1, -3);
        for (int i = 0; i < 4; i++) {
            locs[4 * i] = l0;
            locs[4 * i + 1] = l1;
        }
        cp[0] = cp[1] = 0x35460127u;
    } else if (ns == 16) {
        uint32_t l[4] = {a2_locs(-5, -2, 5, 3, -2, 6, 3, -5), a2_locs(-4, -6, 1, 1, -6, 4, 7, -4),
                         a2_locs(-1, -3, 6, 7, -3, 2, 0, -7), a2_locs(-7, -7, 2, 5, -7, 0, 4, -1)};
        for (int i = 0; i < 16; i++)
            locs[i] = l[i & 3];
        cp[0] = 0x31d0fa85u;
        cp[1] = 0xc97e64b2u;
    }
    a2_reg(b, CTX_AA_CONFIG, ns > 1 ? (lg | (max_dist[lg] << 13) | (lg << 20)) : 0u);
    a2_reg(b, CTX_MODE_CONTROL, msaa_en | ((ns > 1 || msaa_en) ? 2u : 0u));
    a2_reg(b, A2_PA_SC_MODE_CNTL_1, 0x06000000u | (ps_iter ? (1u << 16) : 0u));
    a2_reg(b, A2_DB_EQAA,
           (ns > 1 ? (lg | (lg << 8)) : 0u) | (ps_iter << 4) | (a2m_ns << 12) | (1u << 16) |
               (1u << 17) | (1u << 20) | (eqaa_dis << 21));
    a2_reg(b, CTX_PA_SC_AA_MASK_X0Y0_X1Y0, aa_mask | (aa_mask << 16));
    a2_reg(b, CTX_PA_SC_AA_MASK_X0Y0_X1Y0 + 1, aa_mask | (aa_mask << 16));
    pm4_set_context_regs(b, CTX_PA_SC_CENTROID_PRIORITY_0, cp, 2);
    pm4_set_context_regs(b, CTX_PA_SC_AA_SAMPLE_LOCS_X0Y0_0, locs, 16);
    pm4_set_context_reg(b, CTX_DB_RENDER_OVERRIDE2, ns > 2 ? (1u << 8) : 0u);
}

/* Colour (cbf, CBF_NONE: no target) and depth (on) targets of ns <= 8 samples, 64 x 64, the
   scene's layout (main.c build_dcb: tile index 13 without FMASK / CMASK, FMASK fields = the
   colour's; depth ARRAY_1D_TILED_THIN1, PIPE_CONFIG 12). */
static void a2_targets(struct PM4Builder* b, struct A2Ctx* c, uint32_t ns, uint32_t cbf,
                       uint32_t on) {
    uint32_t lg = a2_log2(ns);
    if (on) {
        uint32_t z = (uint32_t)((uint64_t)(uintptr_t)c->depth >> 8);
        uint32_t d[4] = {z, 0, z, 0};
        a2_reg(b, CTX_DB_Z_INFO, 3u | (lg << 2));
        pm4_set_context_reg(b, CTX_DB_DEPTH_INFO, (1u << 0) | (2u << 4) | (12u << 8));
        pm4_set_context_reg(b, CTX_DB_STENCIL_INFO, 0);
        pm4_set_context_regs(b, CTX_DB_Z_READ_BASE, d, 4);
        pm4_set_context_reg(b, CTX_DB_DEPTH_SIZE, ((A2_W / 8) - 1) | (((A2_W / 8) - 1) << 11));
        pm4_set_context_reg(b, CTX_DB_DEPTH_SLICE, (A2_PX / 64) - 1);
        pm4_set_context_reg(b, CTX_DEPTH_VIEW, 0);
        pm4_set_context_reg(b, CTX_DEPTH_RENDER_OVERRIDE, 0);
    } else {
        a2_reg(b, CTX_DB_Z_INFO, 0);
    }
    uint32_t r[14] = {0};
    if (cbf != CBF_NONE) {
        uint32_t cb = (uint32_t)((uint64_t)(uintptr_t)c->color >> 8);
        uint32_t pt = (A2_W / 8) - 1, sl = (A2_PX / 64) - 1;
        r[0] = cb;
        r[1] = pt | (pt << 20);
        r[2] = sl;
        r[4] = k_a2_cbf[cbf].info;
        r[5] = 13u | (13u << 5) | (lg << 12) | (lg << 15);
        r[9] = cb;
        r[10] = sl;
    }
    pm4_set_context_regs(b, CTX_CB_COLOR0_BASE, r, 14);
    for (int i = 0; i < A2_NRR; i++) {
        if (k_a2_rr[i].idx == CTX_CB_COLOR0_INFO)
            g_a2_written[i] = r[4];
        if (k_a2_rr[i].idx == CTX_CB_COLOR0_ATTRIB)
            g_a2_written[i] = r[5];
    }
    pm4_emit(b, 0xC0001000u);
    pm4_emit(b, (uint32_t)A2_W | ((uint32_t)A2_W << 16));
}

/* State shared by every draw: 64 x 64 viewport and scissors, vs_a2m (z = zbits), one parameter,
   PERSP_CENTER + POS_X / POS_Y_FLOAT (v2, v3), + ANCILLARY (v4) for t_iter; window offset wofs
   (PA_SU_SC_MODE_CNTL.VTX_WINDOW_OFFSET_ENABLE when set). */
static void a2_common(struct PM4Builder* b, struct A2Ctx* c, uint32_t zbits, int v, uint32_t wofs) {
    uint32_t ena = v == V_ITER ? 0x2302u : 0x302u;
    if (v == V_TEX) { /* vs_net: the V# of the plane's vertices in VS_USER_DATA_0..3 */
        uint64_t va = (uint64_t)(uintptr_t)c->vs_net;
        uint32_t vr[4] = {(uint32_t)(va >> 8), (uint32_t)(va >> 40), A2M_VS_NET_RSRC1, 4u << 1};
        uint32_t vb[4];
        build_vsharp_strided(vb, c->verts, 3 * 32, 32);
        pm4_set_sh_regs(b, SH_VS_PGM_LO, vr, 4);
        pm4_set_sh_regs(b, SH_VS_USER_DATA_0, vb, 4);
    } else {
        uint64_t va = (uint64_t)(uintptr_t)c->vs;
        uint32_t vr[4] = {(uint32_t)(va >> 8), (uint32_t)(va >> 40), A2M_VS_A2M_RSRC1, 1u << 1};
        pm4_set_sh_regs(b, SH_VS_PGM_LO, vr, 4);
        pm4_set_sh_reg(b, SH_VS_USER_DATA_0, zbits);
    }
    a2_reg(b, A2_PA_SC_WINDOW_OFFSET, wofs);
    pm4_set_context_reg(b, CTX_VGT_GS_ONCHIP_CNTL, VGT_GS_ONCHIP_CNTL_SAFE);
    {
        uint32_t sc[2] = {0, (uint32_t)A2_W | ((uint32_t)A2_W << 16)};
        pm4_set_context_regs(b, CTX_SCREEN_SCISSOR, sc, 2);
        pm4_set_context_regs(b, CTX_VIEWPORT_SCISSOR0, sc, 2);
        sc[0] = 1u << 31; /* WINDOW_OFFSET_DISABLE */
        pm4_set_context_regs(b, CTX_GENERIC_SCISSOR, sc, 2);
        pm4_set_context_regs(b, CTX_WINDOW_SCISSOR, sc, 2);
    }
    pm4_emit(b, pm4_type3(PM4_SET_CONTEXT_REG, 7));
    pm4_emit(b, CTX_VIEWPORT0);
    pm4_emit_f(b, A2_W * 0.5f);
    pm4_emit_f(b, A2_W * 0.5f);
    pm4_emit_f(b, A2_W * -0.5f);
    pm4_emit_f(b, A2_W * 0.5f);
    pm4_emit_f(b, 1.0f);
    pm4_emit_f(b, 0.0f);
    {
        uint32_t z[2] = {0x00000000u, 0x3F800000u};
        pm4_set_context_regs(b, CTX_VIEWPORT_ZMIN0, z, 2);
    }
    pm4_set_context_reg(b, CTX_INDEX_OFFSET, 0);
    pm4_set_context_reg(b, CTX_PS_INPUT_CNTL_0, 0);
    pm4_set_context_reg(b, CTX_VS_OUTPUT_CONFIG, 0);
    a2_reg(b, CTX_PS_INPUT_ENA, ena);
    pm4_set_context_reg(b, CTX_PS_INPUT_ADDR, ena);
    pm4_set_context_reg(b, CTX_NUM_INTERP, 1);
    pm4_set_context_reg(b, CTX_SHADER_POS_FORMAT, 4);
    pm4_set_context_reg(b, CTX_COLOR_CONTROL, 0x00CC0010u);
    pm4_set_context_reg(b, CTX_SPI_BARYC_CNTL, 0);
    pm4_set_context_reg(b, CTX_CLIPPER_CONTROL, 1u << 19);
    pm4_set_context_reg(b, CTX_VIEWPORT_CONTROL, 0x43F);
    pm4_set_context_reg(b, CTX_VS_OUTPUT_CONTROL, 0);
    a2_reg(b, A2_CB_BLEND0_CONTROL, 0);
    a2_reg(b, A2_PA_SU_SC_MODE_CNTL, wofs ? (1u << 16) : 0u);
    pm4_set_uconfig_reg(b, UCFG_PRIMITIVE_TYPE, 4);
    pm4_set_uconfig_reg(b, UCFG_NUM_INSTANCES, 1);
}

static void a2_ps(struct PM4Builder* b, struct A2Ctx* c, int v) {
    uint64_t pa = (uint64_t)(uintptr_t)c->sh[v], t = (uint64_t)(uintptr_t)c->tab;
    uint32_t pr[4] = {(uint32_t)(pa >> 8), (uint32_t)(pa >> 40), k_a2_sh[v].rsrc1, 2u << 1};
    pm4_set_sh_regs(b, SH_PS_PGM_LO, pr, 4);
    uint32_t ud[2] = {(uint32_t)t, (uint32_t)(t >> 32)};
    pm4_set_sh_regs(b, SH_PS_USER_DATA_0, ud, 2);
}

/* SPI_SHADER_Z_FORMAT and DB_SHADER_CONTROL bits of a test variant (ac_get_spi_shader_z_format:
   Z + alpha 32_AR, Z + mask 32_ABGR). */
static uint32_t a2_zfmt(int v) {
    if (v == V_TEX)
        return 1u; /* 32_R: Z only */
    return (v == V_ZA || v == V_AZ || v == V_ZAB || v == V_ZA0) ? 3u : v == V_MASK ? 9u : 0u;
}
static uint32_t a2_vdbsc(int v) {
    if (v == V_ZA || v == V_AZ || v == V_ZAB || v == V_ZA0 || v == V_TEX)
        return A2_ZEXP;
    if (v == V_MASK)
        return A2_ZEXP | A2_MASKEXP;
    return v == V_KILL ? A2_KILL : 0u;
}

/* The clear draw: every sample, colour 0.25 (the test's colour format; RGBA16F when the test
   has no colour target), depth 1.0. */
static void a2_clear_draw(struct PM4Builder* b, struct A2Ctx* c, const struct A2Test* t) {
    a2_common(b, c, 0x3F000000u, V_CLR, 0);
    a2_targets(b, c, t->ns, t->cbf == CBF_NONE ? CBF_F16 : t->cbf, 1);
    a2_msaa(b, t->ns, t->ns > 1, a2_log2(t->ns), 0, 0, 0xFFFF);
    a2_reg(b, A2_DB_ALPHA_TO_MASK, 0);
    a2_reg(b, A2_DB_SHADER_CONTROL, 0);
    a2_reg(b, A2_DB_COUNT_CONTROL, 0);
    pm4_set_context_reg(b, A2_DB_DEPTH_CLEAR, 0x3F800000u);
    a2_reg(b, A2_DB_RENDER_CONTROL, 1u); /* DEPTH_CLEAR_ENABLE */
    a2_reg(b, A2_DB_DEPTH_CONTROL, (1u << 1) | (1u << 2) | (7u << 4));
    a2_reg(b, CTX_Z_EXPORT_FORMAT, 0);
    a2_reg(b, CTX_COLOR_EXPORT_FORMAT, 9);
    a2_reg(b, CTX_COLOR_TARGET_MASK, 0xF);
    a2_reg(b, CTX_COLOR_SHADER_MASK, 0xF);
    a2_ps(b, c, V_CLR);
    pm4_draw_index_auto(b, 3);
}

/* The test draw's state (aa_mask: PA_SC_AA_MASK per pixel). */
static void a2_test_state(struct PM4Builder* b, struct A2Ctx* c, const struct A2Test* t,
                          uint32_t aa_mask) {
    uint32_t lg = a2_log2(t->ns);
    int tg = a2_has_readback(t);
    a2_common(b, c, 0x3F000000u, t->ps, t->wofs);
    a2_targets(b, c, t->ns, tg ? t->cbf : CBF_NONE, tg && !t->nodepth);
    a2_msaa(b, t->ns, t->msaa_en == 2 ? (t->ns > 1) : t->msaa_en,
            t->a2m_ns == 0xFF ? lg : t->a2m_ns, t->eqaa_dis, t->ps_iter, aa_mask);
    a2_reg(b, A2_DB_ALPHA_TO_MASK, a2_a2m(t));
    a2_reg(b, A2_DB_SHADER_CONTROL, t->dbsc | a2_vdbsc(t->ps));
    /* occlusion counting (radeonsi si_emit_db_render_state, GFX7): PERFECT_ZPASS_COUNTS,
       SAMPLE_RATE log2(samples), ZPASS_ENABLE 1, SLICE_EVEN / ODD_ENABLE */
    a2_reg(b, A2_DB_COUNT_CONTROL, (1u << 1) | (lg << 4) | (1u << 8) | (1u << 24) | (1u << 28));
    a2_reg(b, A2_DB_RENDER_CONTROL, 0);
    a2_reg(b, A2_DB_DEPTH_CONTROL, (tg && !t->nodepth) ? ((1u << 1) | (1u << 2) | (7u << 4)) : 0u);
    a2_reg(b, CTX_Z_EXPORT_FORMAT, a2_zfmt(t->ps));
    a2_reg(b, CTX_COLOR_EXPORT_FORMAT, a2_colfmt(t));
    a2_reg(b, CTX_COLOR_TARGET_MASK, a2_cbtm(t));
    a2_reg(b, CTX_COLOR_SHADER_MASK, a2_cbsm(t));
    a2_reg(b, A2_CB_BLEND0_CONTROL, t->blend ? A2_BLEND_SRC_ALPHA : 0u);
    a2_ps(b, c, t->ps);
}

/* ZPASS_DONE (EVENT_WRITE, event 0x15, index 1): each RB writes its 64-bit count (bit 63 valid)
   at addr + 16 rb (radeonsi si_query.c). */
static void a2_zpass(struct PM4Builder* b, void* addr) {
    uint64_t a = (uint64_t)(uintptr_t)addr;
    pm4_emit(b, pm4_type3(0x46, 3));
    pm4_emit(b, 0x15u | (1u << 8));
    pm4_emit(b, (uint32_t)a);
    pm4_emit(b, (uint32_t)(a >> 32));
}
/* EVENT_WRITE with index 4 (PS_PARTIAL_FLUSH 0x10: wait for the pixel shaders, Mesa
   ac_barrier.c on GFX6-8 before the SURFACE_SYNC). */
static void a2_event(struct PM4Builder* b, uint32_t type, uint32_t index) {
    pm4_emit(b, pm4_type3(0x46, 1));
    pm4_emit(b, type | (index << 8));
}
/* EVENT_WRITE_EOP CACHE_FLUSH_AND_INV_TS (0x14, index 5), data 32-bit value, no interrupt (sid.h
   EOP_* fields; pm4_event_write_eop's address and data layout). */
static void a2_eop(struct PM4Builder* b, volatile uint32_t* addr, uint32_t v) {
    uint64_t a = (uint64_t)(uintptr_t)addr;
    pm4_emit(b, pm4_type3(0x47, 5));
    pm4_emit(b, 0x14u | (5u << 8));
    pm4_emit(b, (uint32_t)(a & 0xFFFFFFFCu));
    pm4_emit(b, ((uint32_t)(a >> 32) & 0xFFFFu) | (1u << 29));
    pm4_emit(b, v);
    pm4_emit(b, 0);
}
/* COPY_DATA register -> memory (src_sel 0, dst_sel 5, WR_CONFIRM), as ordcnt's oc_copy_reg. */
static void a2_copy_reg(struct PM4Builder* b, uint32_t reg, void* dst) {
    uint64_t a = (uint64_t)(uintptr_t)dst;
    pm4_emit(b, pm4_type3(0x40, 5));
    pm4_emit(b, (5u << 8) | (1u << 20));
    pm4_emit(b, reg);
    pm4_emit(b, 0);
    pm4_emit(b, (uint32_t)a);
    pm4_emit(b, (uint32_t)(a >> 32));
}

/* The readback pass: 1 sample, the scratch RGBA16F target, no depth. */
static void a2_readback(struct PM4Builder* b, struct A2Ctx* c) {
    a2_common(b, c, 0x3F000000u, V_RD, 0);
    {
        uint32_t s = (uint32_t)((uint64_t)(uintptr_t)c->scratch >> 8);
        uint32_t r[14] = {
            s, (A2_W / 8) - 1, (A2_PX / 64) - 1, 0, CB_INFO_RGBA16F, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        pm4_set_context_regs(b, CTX_CB_COLOR0_BASE, r, 14);
        pm4_emit(b, 0xC0001000u);
        pm4_emit(b, (uint32_t)A2_W | ((uint32_t)A2_W << 16));
    }
    pm4_set_context_reg(b, CTX_DB_Z_INFO, 0);
    emit_msaa_state(b, 1);
    pm4_set_context_reg(b, A2_PA_SC_MODE_CNTL_1, 0x06000000u);
    pm4_set_context_reg(b, A2_DB_ALPHA_TO_MASK, 0);
    pm4_set_context_reg(b, A2_DB_SHADER_CONTROL, 0);
    pm4_set_context_reg(b, A2_DB_COUNT_CONTROL, 0);
    pm4_set_context_reg(b, A2_DB_RENDER_CONTROL, 0);
    pm4_set_context_reg(b, A2_DB_DEPTH_CONTROL, 0);
    pm4_set_context_reg(b, CTX_Z_EXPORT_FORMAT, 0);
    pm4_set_context_reg(b, CTX_COLOR_EXPORT_FORMAT, 9);
    pm4_set_context_reg(b, CTX_COLOR_TARGET_MASK, 0xF);
    pm4_set_context_reg(b, CTX_COLOR_SHADER_MASK, 0xF);
    a2_ps(b, c, V_RD);
    pm4_draw_index_auto(b, 3);
}

/* Readback T#s (table 16: colour, 24: depth): TYPE 2D_MSAA (0xE) with LAST_LEVEL log2(ns), or 2D
   (9) for one sample; TILING_INDEX as the target (13 colour, 5 depth: build_tsharp_depth_msaa). */
static void a2_tsharps(struct A2Ctx* c, const struct A2Test* t) {
    uint32_t lg = a2_log2(t->ns), type = t->ns > 1 ? 0xEu : 9u;
    uint32_t cbf = t->cbf == CBF_NONE ? CBF_F16 : t->cbf;
    uint64_t a = (uint64_t)(uintptr_t)c->color;
    uint32_t* r = c->tab + 16;
    my_memset(r, 0, 64);
    r[0] = (uint32_t)(a >> 8);
    r[1] = (uint32_t)(a >> 40) | (k_a2_cbf[cbf].dfmt << 20) | (k_a2_cbf[cbf].nfmt << 26);
    r[2] = (uint32_t)(A2_W - 1) | ((uint32_t)(A2_W - 1) << 14);
    r[3] = 4u | (5u << 3) | (6u << 6) | (7u << 9) | (lg << 16) | (13u << 20) | (type << 28);
    r[4] = (uint32_t)(A2_W - 1) << 13;
    build_tsharp_depth_msaa(r + 8, c->depth, A2_W, A2_W);
    r[11] = (r[11] & ~((0xFu << 16) | (0xFu << 28))) | (lg << 16) | (type << 28);
}

/* One pass: clear, test draw, readback, and (iso) the isolation draws; then the end-of-pass
   synchronisation (file comment). */
static void a2_build(struct PM4Builder* b, struct A2Ctx* c, const struct A2Test* t, int iso,
                     int regread) {
    uint32_t avail = b->cap - b->off;
    int w = sceGnmDrawInitDefaultHardwareState350(b->buf + b->off, avail);
    if (w > 0)
        b->off += (uint32_t)w;
    pm4_acquire_mem(b, A2_COHER_ICACHE | A2_COHER_KCACHE | A2_COHER_TCL1 | A2_COHER_TC);
    pm4_context_control(b);
    int rb = a2_has_readback(t);
    if (rb)
        a2_clear_draw(b, c, t);
    a2_test_state(b, c, t, a2_aa(t));
    for (int i = 0; i < A2_NRR; i++)
        g_a2_wtest[i] = g_a2_written[i];
    if (regread)
        for (int i = 0; i < A2_NRR; i++)
            a2_copy_reg(b, 0xA000u + k_a2_rr[i].idx, c->regs + i);
    a2_zpass(b, c->zp);
    pm4_draw_index_auto(b, 3);
    a2_zpass(b, c->zp + 2);
    if (rb) {
        /* DB / CB to memory before they are read as textures (emit_post: DB_DEST_BASE_ENA and
           DEST_BASE_0_ENA, gfx6Barrier.cpp) */
        pm4_acquire_mem(b, COHER_RT_TO_TEXTURE | A2_COHER_DB_DEST_BASE | A2_COHER_DEST_BASE_0);
        a2_readback(b, c);
    }
    for (uint32_t s = 0; iso && s < t->ns; s++) {
        a2_test_state(b, c, t, (1u << s) & a2_aa(t));
        a2_zpass(b, c->zp + (1 + s) * (A2_ZP_STRIDE / 4));
        pm4_draw_index_auto(b, 3);
        a2_zpass(b, c->zp + (1 + s) * (A2_ZP_STRIDE / 4) + 2);
    }
    a2_event(b, 0x10, 4); /* PS_PARTIAL_FLUSH */
    pm4_acquire_mem(b, A2_COHER_TC | A2_COHER_TCL1 | A2_COHER_CB | A2_COHER_DB |
                           A2_COHER_DEST_BASE_0 | A2_COHER_DB_DEST_BASE);
    c->fv++;
    a2_eop(b, c->fence + 1, c->fv);
    a2_eop(b, c->fence, c->fv);
}

/* Returns 0 done, -1 fence timeout, -2 could not submit. */
static int a2_submit(struct A2Ctx* c, const struct A2Test* t, int iso, int regread, uint64_t* us) {
    struct PM4Builder b;
    my_memset(c->out, 0xA5, 2 * A2_OUT_DEPTH * 4);
    my_memset(c->zp, 0, A2_ZP_BLOCKS * A2_ZP_STRIDE);
    for (int i = 0; i < A2_NRR; i++)
        c->regs[i] = 0xDEADBEEFu;
    pm4_init(&b, c->dcb, A2_DCB_DW);
    a2_build(&b, c, t, iso, regread);
    if (b.overflow)
        return -2;
    uint64_t t0 = sceKernelGetProcessTime();
    uint32_t sz = b.off * 4;
    void* a[1] = {c->dcb};
    if (sceGnmSubmitCommandBuffers(1, a, &sz, 0, 0) != 0)
        return -2;
    sceGnmSubmitDone();
    while (*c->fence < c->fv) {
        if (sceKernelGetProcessTime() - t0 > A2_TIMEOUT_US)
            return -1;
        sceKernelUsleep(100);
    }
    *us = sceKernelGetProcessTime() - t0;
    return 0;
}

/* ---- the model ---- */
/* An alpha as the DB sees it: zero, full, a double in (0, 1) or the ratio num / den. */
struct A2A {
    uint8_t k; /* 0 zero, 1 full, 2 double, 3 ratio */
    double d;
    uint64_t num, den;
};
static struct A2A a2a_zero(void) {
    struct A2A a = {0, 0.0, 0, 1};
    return a;
}
static struct A2A a2a_fullv(void) {
    struct A2A a = {1, 1.0, 1, 1};
    return a;
}
static struct A2A a2a_ratio(uint64_t num, uint64_t den) {
    if (!num)
        return a2a_zero();
    if (num >= den)
        return a2a_fullv();
    struct A2A a = {3, 0.0, num, den};
    return a;
}
static struct A2A a2a_f32(uint32_t b) {
    uint32_t e = (b >> 23) & 0xFF;
    if (e == 0xFF)
        return (b & 0x7FFFFFu) || (b >> 31) ? a2a_zero() : a2a_fullv(); /* NaN / -inf / +inf */
    if ((b >> 31) || !(b & 0x7FFFFFFFu))
        return a2a_zero();
    double d = (double)a2_ftof(b);
    if (d >= 1.0)
        return a2a_fullv();
    struct A2A a = {2, d, 0, 1};
    return a;
}
static struct A2A a2a_half(uint32_t h) {
    uint32_t e = (h >> 10) & 0x1F, m = h & 0x3FF;
    if (e == 0x1F)
        return (m || (h & 0x8000u)) ? a2a_zero() : a2a_fullv();
    if ((h & 0x8000u) || !(h & 0x7FFFu))
        return a2a_zero();
    /* value = m 2^-24 (denormal) or (1024 + m) 2^(e - 25) */
    uint64_t num = e ? (1024u + m) : m;
    int sh = e ? (int)e - 25 : -24;
    if (sh >= 0)
        return a2a_ratio(num << sh, 1);
    return a2a_ratio(num, 1ull << (-sh));
}
/* v_cvt_pkrtz_f16_f32: round toward zero. */
static uint32_t a2_f2h_rtz(uint32_t b) {
    uint32_t s = (b >> 16) & 0x8000u, e = (b >> 23) & 0xFF, m = b & 0x7FFFFFu;
    if (e == 0xFF)
        return s | 0x7C00u | (m ? 0x200u : 0u);
    int he = (int)e - 127 + 15;
    if (he >= 31)
        return s | 0x7BFFu;
    if (he <= 0) {
        int shift = 14 - he; /* to a denormal: (1.m) 2^(e - 127) / 2^-24 */
        if (!e || shift >= 32)
            return s;
        return s | (uint32_t)(((uint64_t)(0x800000u | m) >> shift) & 0x3FFu);
    }
    return s | ((uint32_t)he << 10) | (m >> 13);
}
/* v_cvt_pknorm_u16_f32: clamp to [0, 1], x 65535, round to nearest even (the a2m-1 run's
   boundaries are all at .5 and fit this as well as round-half-up). */
static uint32_t a2_pknorm_u16(uint32_t b) {
    struct A2A a = a2a_f32(b);
    if (a.k == 0)
        return 0;
    if (a.k == 1)
        return 65535;
    double x = a.d * 65535.0;
    uint32_t f = (uint32_t)x;
    double fr = x - (double)f;
    if (fr > 0.5 || (fr == 0.5 && (f & 1)))
        f++;
    return f;
}
/* A raw 16-bit export value by SPI_SHADER_COL_FORMAT (4 FP16, 5 UNORM16, 6 SNORM16). */
static struct A2A a2a_raw(uint32_t v, uint32_t fmt, int h) {
    v &= 0xFFFFu;
    if (fmt == 4)
        return a2a_half(v);
    if (fmt == 5)
        return (h && v == 0xFFFFu) ? a2a_fullv() : a2a_ratio(v, 65536);
    if (fmt == 6) {
        int16_t s = (int16_t)v;
        if (s <= 0)
            return a2a_zero();
        return a2a_ratio((uint64_t)s, h ? 32767u : 32768u);
    }
    return a2a_zero();
}
static double a2a_value(struct A2A a) {
    return a.k == 0 ? 0.0 : a.k == 1 ? 1.0 : a.k == 2 ? a.d : (double)a.num / (double)a.den;
}
/* floor(a k8) for k8 = 8 k (exact: a double times a power of two, or integer arithmetic). */
static uint32_t a2_floor8(struct A2A a, uint32_t k8) {
    if (a.k == 0)
        return 0;
    if (a.k == 1)
        return k8;
    if (a.k == 2)
        return (uint32_t)(a.d * (double)k8);
    return (uint32_t)((a.num * k8) / a.den);
}
static uint32_t a2_cnt(struct A2A a, uint32_t k, uint32_t off) {
    uint32_t c = (a2_floor8(a, 8 * k) + off) >> 3;
    return c > k ? k : c;
}
static uint32_t a2_fmask(struct A2A a, uint32_t n, uint32_t off, uint32_t dis) {
    if (dis)
        return a2_full(a2_cnt(a, n, off));
    uint32_t m = 0;
    for (uint32_t k = n; k > 1; k >>= 1) {
        uint32_t h = a2_cnt(a, k, off) - a2_cnt(a, k >> 1, off);
        m |= a2_full(h) << (k >> 1);
    }
    return m | (a2_cnt(a, 1, off) ? 1u : 0u);
}
/* The ns-bit alpha-to-mask coverage of alpha a at quad pixel p. */
static uint32_t a2_amask(const struct A2Test* t, struct A2A a, uint32_t p) {
    uint32_t r = a2_a2m(t), off = 2 * ((r >> (8 + 2 * p)) & 3) + ((r >> 16) & 1);
    uint32_t lgn = t->a2m_ns == 0xFF ? a2_log2(t->ns) : t->a2m_ns;
    uint32_t n = 1u << lgn;
    if (n > t->ns)
        n = t->ns;
    uint32_t m = a2_fmask(a, n, off, t->eqaa_dis), out = 0;
    for (uint32_t s = 0; s < t->ns; s++)
        out |= ((m >> (s % n)) & 1u) << s;
    return out;
}
static int a2_fmt_alpha(uint32_t f) {
    return f >= 3 && f <= 9; /* 32_AR, FP16, UNORM16, SNORM16, UINT16, SINT16, 32_ABGR */
}
static int a2_exports_mrt0(int v) {
    return v != V_ZA0 && v != V_M1O;
}
/* MRT0's alpha as exported by the variant. */
static struct A2A a2_alpha0(const struct A2Test* t, uint32_t A, int h) {
    switch (t->ps) {
    case V_F16:
        return a2a_half(a2_f2h_rtz(A));
    case V_U16:
        return a2a_raw(a2_pknorm_u16(A), 5, h);
    case V_RAW:
        return a2a_raw(A, a2_colfmt(t) & 0xF, h);
    case V_ZA:
    case V_M1:
        return a2a_f32(t->mrt0a);
    default:
        return a2a_f32(A);
    }
}
/* The expected sample mask of pixel (x, y) under hypothesis h; -1: no model. */
static int64_t a2_expect(const struct A2Test* t, int x, int y, uint32_t A, uint32_t B, int h) {
    if (t->hyp == H_NOMODEL)
        return -1;
    uint32_t full = a2_full(t->ns), m = full, col0 = a2_colfmt(t) & 0xF,
             col1 = (a2_colfmt(t) >> 4) & 0xF, cbsm = a2_cbsm(t), r = a2_a2m(t);
    int px = x, py = y;
    /* The triangle starts at window (0, 0): with VTX_WINDOW_OFFSET_ENABLE it starts at screen
       (wx, wy), so the strips x < wx and y < wy are not drawn. */
    if (x < (int)(t->wofs & 0xFFFF) || y < (int)(t->wofs >> 16))
        return 0;
    if (t->hyp == H_WIN && h) {
        px -= (int)(t->wofs & 0xFFFF);
        py -= (int)(t->wofs >> 16);
    }
    uint32_t p = (uint32_t)(px & 1) + 2u * (uint32_t)(py & 1);
    int gate0 = a2_exports_mrt0(t->ps) && a2_fmt_alpha(col0) && (cbsm & 8);
    int path0 = gate0 && (r & 1) && !(t->dbsc & A2_A2M_DIS);
    if (t->hyp == H_EN0 && h)
        path0 = gate0;
    int pathz = (t->dbsc & A2_COV2MASK) &&
                (t->ps == V_ZA || t->ps == V_AZ || t->ps == V_ZAB || t->ps == V_ZA0);
    struct A2A a0 = a2_alpha0(t, A, h), az = a2a_zero();
    if (t->ps == V_ZA || t->ps == V_ZA0)
        az = a2a_f32(A);
    else if (t->ps == V_AZ)
        az = a2a_f32(t->mrtza);
    else if (t->ps == V_ZAB)
        az = a2a_f32(B);
    if (t->ps == V_NOA) {
        if (h)
            a0 = a2a_zero();
        else
            path0 = 0;
    }
    if (t->hyp == H_MRT1 && h) {
        path0 = (r & 1) && !(t->dbsc & A2_A2M_DIS) && a2_fmt_alpha(col1) && (cbsm & 0x80);
        a0 = a2a_f32(A);
    }
    if (t->ps == V_ITER && path0) {
        uint32_t it = 0;
        for (uint32_t s = 0; s < t->ns; s++) {
            float as = a2_ftof(A) + (float)s * a2_ftof(t->iter_delta);
            struct A2A a = a2a_f32(h ? A : a2_fbits(as));
            it |= a2_amask(t, a, p) & (1u << s);
        }
        m &= it;
    } else if (path0 && pathz) {
        if (t->hyp == H_COMBINE && h)
            m &= a2_amask(t, a2a_value(a0) < a2a_value(az) ? a0 : az, p);
        else
            m &= a2_amask(t, a0, p) & a2_amask(t, az, p);
    } else if (path0) {
        m &= a2_amask(t, a0, p);
    } else if (pathz) {
        m &= a2_amask(t, az, p);
    }
    if (t->ps == V_MASK)
        m &= B;
    if (t->ps == V_KILL && (B & 1))
        m = 0;
    return (int64_t)(m & a2_aa(t) & full);
}
/* The colour plane is modelled (equal to the depth plane) when MRT0's red is written. */
static int a2_colour_modelled(const struct A2Test* t) {
    return !t->nocol && t->cbf != CBF_NONE && k_a2_cbf[t->cbf].cmp && (a2_cbtm(t) & 1) &&
           (a2_cbsm(t) & 1) && (a2_colfmt(t) & 0xF) && a2_exports_mrt0(t->ps);
}
/* The variants a crash or hang marks bad: all four MRTZ.a exporters together (one shader
   recompiler path), otherwise the variant alone. */
static uint32_t a2_group(int v) {
    uint32_t z = (1u << V_ZA) | (1u << V_AZ) | (1u << V_ZAB) | (1u << V_ZA0);
    return (z >> v) & 1 ? z : 1u << v;
}
static int a2_nh(const struct A2Test* t) {
    return (t->hyp == H_NONE || t->hyp == H_NOMODEL) ? 1 : 2;
}

/* ---- alpha tables ---- */
static uint32_t g_a2_rng;
static uint32_t a2_rand(void) {
    uint32_t x = g_a2_rng ? g_a2_rng : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_a2_rng = x;
    return x;
}
static void a2_seed(uint32_t s) {
    g_a2_rng = s * 2654435761u + 0x6A09E667u;
    for (int i = 0; i < 4; i++)
        a2_rand();
}
/* A random alpha: [0, 1) (half the time), on the 1 / 64 grid +- 1 ulp, or any bit pattern. */
static uint32_t a2_rand_alpha(void) {
    uint32_t r = a2_rand(), k = a2_rand();
    switch (r & 3) {
    case 0:
    case 1:
        return a2_fbits((float)(k >> 8) * (1.0f / 16777216.0f));
    case 2: {
        uint32_t v = a2_fbits((float)(k % 65) / 64.0f);
        int d = (int)((r >> 2) % 3) - 1;
        return (v == 0 && d < 0) ? 0x80000000u : v + (uint32_t)d;
    }
    default:
        return k;
    }
}

/* Quad alphas (q = (x >> 1) + 32 (y >> 1)) and their per-pixel tables. */
static uint32_t g_a2_qa[A2_QUADS];

static void a2_coarse_alpha(uint32_t* al) {
    static const uint32_t sp[] = {
        0x7FC00000u, 0xFFC00000u, 0x7F800001u, 0x7FBFFFFFu, 0xFFBFFFFFu, 0x7F800000u, 0xFF800000u,
        0x80000000u, 0x00000000u, 0x00000001u, 0x007FFFFFu, 0x00800000u, 0x80000001u, 0x807FFFFFu,
        0xBF800000u, 0xBF000000u, 0xBDCCCCCDu, 0xC0000000u, 0xFF7FFFFFu, 0x3F800000u, 0x3F800001u,
        0x3F7FFFFFu, 0x3F8CCCCDu, 0x3FC00000u, 0x40000000u, 0x40800000u, 0x7F7FFFFFu, 0x2EDBE6FFu,
        0x33800000u, 0x3B800000u, 0x3B808081u, 0x37800080u};
    int n = 0;
    for (; n < (int)(sizeof(sp) / sizeof(sp[0])); n++)
        al[n] = sp[n];
    for (int k = 0; k <= 16 && n + 3 <= A2_SPECIALS; k++) {
        uint32_t v = a2_fbits((float)k / 16.0f);
        if (k)
            al[n++] = v - 1;
        al[n++] = v;
        al[n++] = v + 1;
    }
    for (int k = 1; k < 32 && n < A2_SPECIALS; k += 2)
        al[n++] = a2_fbits((float)k / 32.0f);
    for (int k = 1; n < A2_SPECIALS; k += 2)
        al[n++] = a2_fbits((float)k / 64.0f);
    for (int i = 0; i < A2_QUADS - A2_SPECIALS; i++)
        al[A2_SPECIALS + i] = a2_fbits((float)i / (float)(A2_QUADS - A2_SPECIALS - 1));
}
static void a2_expand_quads(uint32_t* A) {
    for (int y = 0; y < A2_W; y++)
        for (int x = 0; x < A2_W; x++)
            A[x + A2_W * y] = g_a2_qa[(x >> 1) + 32 * (y >> 1)];
}
/* The net: strands every 8 pixels (alpha 1.0 on, 0.6 / 0.25 beside), fading with y to 0.45. */
static uint32_t a2_net_alpha(int x, int y) {
    int dx = x & 7, dy = y & 7;
    int d = dx < 8 - dx ? dx : 8 - dx;
    int e = dy < 8 - dy ? dy : 8 - dy;
    if (e < d)
        d = e;
    float base = d == 0 ? 1.0f : d == 1 ? 0.6f : d == 2 ? 0.25f : 0.0f;
    float fade = 1.0f - 0.55f * (float)y / 63.0f;
    return a2_fbits(base * fade);
}
/* Fill A and B for pass `pass` of test t (B from the test's seed and the pass). */
static void a2_fill(struct A2Ctx* c, const struct A2Test* t, int pass) {
    uint32_t* A = c->alpha;
    uint32_t* B = c->alpha + A2_PX;
    a2_seed(t->seed * 977u + (uint32_t)pass * 131u + 7u);
    switch (t->kind) {
    case AK_COARSE:
        if (pass == 0)
            a2_coarse_alpha(g_a2_qa);
        a2_expand_quads(A);
        break;
    case AK_UNIFORM:
        for (int i = 0; i < A2_PX; i++)
            A[i] = t->uni_alpha;
        break;
    case AK_RAMP:
        for (int q = 0; q < A2_QUADS; q++)
            g_a2_qa[q] = a2_fbits((float)q / 1023.0f);
        a2_expand_quads(A);
        break;
    case AK_NET:
        for (int y = 0; y < A2_W; y++)
            for (int x = 0; x < A2_W; x++)
                A[x + A2_W * y] = a2_net_alpha(x, y);
        break;
    case AK_RANDOM:
        for (int i = 0; i < A2_PX; i++)
            A[i] = a2_rand_alpha();
        break;
    case AK_SWEEP16:
        for (int q = 0; q < A2_QUADS; q++)
            g_a2_qa[q] = (uint32_t)pass * A2_QUADS + (uint32_t)q;
        a2_expand_quads(A);
        break;
    case AK_TEX: /* t_tex stores the sampled alpha to B; A gets it after the pass */
        for (int i = 0; i < A2_PX; i++)
            A[i] = B[i] = A2_NOT_SHADED;
        return;
    }
    for (int i = 0; i < A2_PX; i++)
        B[i] = t->bmode == BM_ALPHA  ? a2_rand_alpha()
               : t->bmode == BM_MASK ? (a2_rand() & 0xFFu)
               : t->bmode == BM_KILL ? (a2_rand() & 1u)
                                     : 0u;
}

/* ---- results of one pass ---- */
static uint8_t g_a2_cm[A2_PX], g_a2_dm[A2_PX], g_a2_an[A2_PX];
static uint32_t g_a2_clear_raw, g_a2_nclear_raw, g_a2_nanom, g_a2_unwritten;
static uint32_t g_a2_anom[8][4]; /* x, y, s | plane << 8, raw */

static void a2_classify(struct A2Ctx* c, const struct A2Test* t) {
    const volatile uint32_t* o = c->out;
    int cmp = k_a2_cbf[t->cbf].cmp;
    g_a2_nclear_raw = 0;
    g_a2_nanom = 0;
    g_a2_unwritten = 0;
    for (int px = 0; px < A2_PX; px++) {
        uint32_t cm = 0, dm = 0, an = 0;
        for (uint32_t s = 0; s < t->ns; s++) {
            uint32_t cr = o[px * 8 + s], dr = o[A2_OUT_DEPTH + px * 8 + s];
            if (cr == A2_UNWRITTEN || dr == A2_UNWRITTEN)
                g_a2_unwritten++;
            if (cmp) {
                if (cr == 0x3F800000u) {
                    cm |= 1u << s;
                } else if (!g_a2_nclear_raw && cr != A2_UNWRITTEN) {
                    g_a2_clear_raw = cr;
                    g_a2_nclear_raw = 1;
                } else if (cr != g_a2_clear_raw) {
                    an |= 1;
                    if (g_a2_nanom < 8) {
                        uint32_t* a = g_a2_anom[g_a2_nanom];
                        a[0] = (uint32_t)(px & 63), a[1] = (uint32_t)(px >> 6), a[2] = s, a[3] = cr;
                    }
                    g_a2_nanom++;
                }
            }
            if (dr == 0x3F000000u) {
                dm |= 1u << s;
            } else if (dr != 0x3F800000u) {
                an |= 2;
                if (g_a2_nanom < 8) {
                    uint32_t* a = g_a2_anom[g_a2_nanom];
                    a[0] = (uint32_t)(px & 63), a[1] = (uint32_t)(px >> 6), a[2] = s | (1u << 8),
                    a[3] = dr;
                }
                g_a2_nanom++;
            }
        }
        g_a2_cm[px] = (uint8_t)cm;
        g_a2_dm[px] = (uint8_t)dm;
        g_a2_an[px] = (uint8_t)an;
    }
}

/* Totals of a test over its passes. */
struct A2Tot {
    uint32_t passes, retries, px, dmis[2], cmis[2], dchk, cchk, unclass, unwritten, cdiff;
    uint32_t zp_bad, iso_bad, zp_none, nex, not_shaded;
    uint64_t pass_us;
    uint32_t ex[6][8]; /* x, y, A, B, measured depth, measured colour, h0, h1 */
};
static struct A2Tot g_tot;
static uint32_t g_model_pop[2], g_model_s[2][16]; /* this pass: model popcounts */
static uint8_t g_model0[A2_PX];                   /* this pass: h0 mask per pixel */

static void a2_check(struct A2Ctx* c, const struct A2Test* t) {
    const uint32_t* A = c->alpha;
    const uint32_t* B = c->alpha + A2_PX;
    int nh = a2_nh(t), dchk = a2_has_readback(t) && !t->nodepth, cchk = a2_colour_modelled(t);
    g_model_pop[0] = g_model_pop[1] = 0;
    my_memset(g_model_s, 0, sizeof(g_model_s));
    for (int px = 0; px < A2_PX; px++) {
        int x = px & 63, y = px >> 6;
        int64_t e[2] = {0, 0};
        for (int h = 0; h < nh; h++) {
            e[h] = a2_expect(t, x, y, A[px], B[px], h);
            if (e[h] < 0)
                continue;
            g_model_pop[h] += (uint32_t)a2_pop((uint32_t)e[h]);
            for (uint32_t s = 0; s < t->ns; s++)
                g_model_s[h][s] += (uint32_t)((e[h] >> s) & 1);
        }
        g_model0[px] = (uint8_t)(e[0] < 0 ? 0 : e[0]);
        if (!a2_has_readback(t) || e[0] < 0)
            continue;
        g_tot.px++;
        if (g_a2_an[px])
            g_tot.unclass++;
        if (cchk && dchk && g_a2_cm[px] != g_a2_dm[px])
            g_tot.cdiff++;
        int bad0 = 0;
        for (int h = 0; h < nh; h++) {
            if (dchk && g_a2_dm[px] != (uint8_t)e[h]) {
                g_tot.dmis[h]++;
                if (!h)
                    bad0 = 1;
            }
            if (cchk && g_a2_cm[px] != (uint8_t)e[h]) {
                g_tot.cmis[h]++;
                if (!h)
                    bad0 = 1;
            }
        }
        if (dchk)
            g_tot.dchk++;
        if (cchk)
            g_tot.cchk++;
        if (bad0 && g_tot.nex < 6) {
            uint32_t* r = g_tot.ex[g_tot.nex++];
            r[0] = (uint32_t)x, r[1] = (uint32_t)y, r[2] = A[px], r[3] = B[px];
            r[4] = g_a2_dm[px], r[5] = g_a2_cm[px], r[6] = (uint32_t)e[0];
            r[7] = nh > 1 ? (uint32_t)e[1] : 0;
        }
    }
}

/* ZPASS_DONE: the sum over RBs whose begin and end have bit 63; -1 when none. */
static int64_t a2_zp_sum(struct A2Ctx* c, uint32_t blk, uint32_t* vmask) {
    const volatile uint32_t* zb = c->zp + blk * (A2_ZP_STRIDE / 4);
    uint64_t sum = 0;
    int valid = 0;
    *vmask = 0;
    for (int rb = 0; rb < A2_ZP_RBS; rb++) {
        uint64_t b0 = (uint64_t)zb[rb * 4] | ((uint64_t)zb[rb * 4 + 1] << 32);
        uint64_t e0 = (uint64_t)zb[rb * 4 + 2] | ((uint64_t)zb[rb * 4 + 3] << 32);
        if ((b0 >> 63) && (e0 >> 63)) {
            valid++;
            *vmask |= 1u << rb;
            sum += (e0 & 0x7FFFFFFFFFFFFFFFull) - (b0 & 0x7FFFFFFFFFFFFFFFull);
        }
    }
    return valid ? (int64_t)sum : -1;
}

/* ---- reports ---- */
static void a2_report_header(const struct A2Test* t) {
    a2_s("### ");
    a2_s(t->id);
    a2_s(": ");
    a2_s(t->what);
    a2_s("\n");
    a2_u(t->ns);
    a2_s(" samples, ");
    a2_s(k_a2_vname[t->ps]);
    a2_s(", colour ");
    a2_s(k_a2_cbf[t->cbf].name);
    a2_s(t->nodepth || !a2_has_readback(t) ? ", no depth target" : ", depth Z_32_FLOAT");
    a2_s(", DB_ALPHA_TO_MASK ");
    a2_x(a2_a2m(t), 8);
    if (t->hyp != H_NONE) {
        a2_s(", hypotheses h0 ");
        a2_s(k_a2_hname[t->hyp][0]);
        if (a2_nh(t) > 1) {
            a2_s(" / h1 ");
            a2_s(k_a2_hname[t->hyp][1]);
        }
    }
    a2_s("\n");
}

static void a2_report_regs(struct A2Ctx* c, int regread, int full) {
    if (!full) {
        a2_s("registers:");
        for (int i = 0; i < A2_NRR; i++) {
            uint32_t r = ((volatile uint32_t*)c->regs)[i];
            a2_s(" ");
            a2_s(k_a2_rr[i].name);
            a2_s("=");
            a2_x(g_a2_wtest[i], 8);
            if (regread && !k_a2_rr[i].unreliable && r != g_a2_wtest[i]) {
                a2_s(r == 0xDEADBEEFu ? "(not read)" : "(read back ");
                if (r != 0xDEADBEEFu) {
                    a2_x(r, 8);
                    a2_s(")");
                }
            }
        }
        a2_s("\n");
        return;
    }
    a2_s("\n| register | written | read back (COPY_DATA) |\n|---|---|---|\n");
    for (int i = 0; i < A2_NRR; i++) {
        a2_s("| ");
        a2_s(k_a2_rr[i].name);
        a2_s(" | ");
        a2_x(g_a2_wtest[i], 8);
        a2_s(" | ");
        if (!regread) {
            a2_s("not read");
        } else {
            uint32_t r = ((volatile uint32_t*)c->regs)[i];
            a2_x(r, 8);
            if (r == 0xDEADBEEFu)
                a2_s(" (not written)");
            else if (r != g_a2_wtest[i])
                a2_s(k_a2_rr[i].unreliable ? " differs (unreliable readback)" : " DIFFERS");
        }
        a2_s(" |\n");
    }
}

static void a2_report_zpass(struct A2Ctx* c, const struct A2Test* t) {
    int nh = a2_nh(t);
    uint64_t pop = 0, popc = 0;
    uint32_t per_s[16] = {0}, per_sc[16] = {0};
    if (a2_has_readback(t))
        for (int px = 0; px < A2_PX; px++) {
            pop += (uint64_t)a2_pop(g_a2_dm[px]);
            popc += (uint64_t)a2_pop(g_a2_cm[px]);
            for (uint32_t s = 0; s < t->ns; s++) {
                per_s[s] += (g_a2_dm[px] >> s) & 1;
                per_sc[s] += (g_a2_cm[px] >> s) & 1;
            }
        }
    a2_s("\nZPASS_DONE (RBs with bit 63 on both; model h0 / h1; readback depth / colour):\n\n| "
         "draw | valid RBs | samples passed | model h0 | model h1 | readback depth | readback "
         "colour |\n|---|---|---|---|---|---|---|\n");
    for (uint32_t blk = 0; blk <= t->ns; blk++) {
        uint32_t vmask;
        int64_t sum = a2_zp_sum(c, blk, &vmask);
        uint32_t m0 = blk ? g_model_s[0][blk - 1] : g_model_pop[0];
        uint32_t m1 = blk ? g_model_s[1][blk - 1] : g_model_pop[1];
        a2_s("| ");
        if (blk == 0) {
            a2_s("test draw");
        } else {
            a2_s("AA_MASK 1 << ");
            a2_u(blk - 1);
        }
        a2_s(" | ");
        a2_u((unsigned long long)a2_pop(vmask));
        a2_s(" (");
        a2_x(vmask, 4);
        a2_s(") | ");
        if (sum < 0) {
            a2_s("none");
            g_tot.zp_none++;
        } else {
            a2_u((unsigned long long)sum);
            if (t->hyp != H_NOMODEL && (uint64_t)sum != m0) {
                if (blk)
                    g_tot.iso_bad++;
                else
                    g_tot.zp_bad++;
            }
        }
        a2_s(" | ");
        if (t->hyp == H_NOMODEL)
            a2_s("-");
        else
            a2_u(m0);
        a2_s(" | ");
        if (nh > 1)
            a2_u(m1);
        else
            a2_s("-");
        a2_s(" | ");
        if (a2_has_readback(t)) {
            a2_u(blk == 0 ? pop : per_s[blk - 1]);
            a2_s(" | ");
            a2_u(blk == 0 ? popc : per_sc[blk - 1]);
        } else {
            a2_s("- | -");
        }
        a2_s(" |\n");
    }
}

static void a2_masks(int px, int cmp) {
    if (cmp)
        a2_x(g_a2_cm[px], 2);
    else
        a2_s("--");
    a2_s("/");
    a2_x(g_a2_dm[px], 2);
    if (g_a2_an[px])
        a2_s(" !");
}
static int a2_qpx(int q, int p) {
    return (2 * (q & 31) + (p & 1)) + A2_W * (2 * (q >> 5) + (p >> 1));
}
static uint32_t a2_key(int px) {
    return (uint32_t)g_a2_cm[px] | ((uint32_t)g_a2_dm[px] << 8) | ((uint32_t)g_a2_an[px] << 16);
}

/* The quad table (coarse kinds): masks per quad pixel and the model's h0, run-length over the
   linear part. */
static void a2_report_quads(const struct A2Test* t) {
    int cmp = k_a2_cbf[t->cbf].cmp;
    a2_s("\ncolour / depth mask per quad pixel (p0 = x even y even, p1 = x odd, p2 = y odd, p3 = "
         "both), then the model (h0); ! = unclassified sample\n\n| slots | alpha | p0 | p1 | p2 | "
         "p3 | model p0..p3 |\n|---|---|---|---|---|---|---|\n");
    for (int q = 0; q < A2_QUADS;) {
        int e = q + 1;
        if (q >= A2_SPECIALS)
            while (e < A2_QUADS) {
                int same = 1;
                for (int p = 0; p < 4; p++)
                    same &= a2_key(a2_qpx(e, p)) == a2_key(a2_qpx(q, p)) &&
                            g_model0[a2_qpx(e, p)] == g_model0[a2_qpx(q, p)];
                if (!same)
                    break;
                e++;
            }
        a2_s("| ");
        a2_u((unsigned long long)q);
        if (e - 1 > q) {
            a2_s("..");
            a2_u((unsigned long long)(e - 1));
        }
        a2_s(" | ");
        a2_f(g_a2_qa[q]);
        if (e - 1 > q) {
            a2_s(" .. ");
            a2_f(g_a2_qa[e - 1]);
        }
        for (int p = 0; p < 4; p++) {
            a2_s(" | ");
            a2_masks(a2_qpx(q, p), cmp);
        }
        a2_s(" |");
        for (int p = 0; p < 4; p++) {
            a2_s(" ");
            a2_x(g_model0[a2_qpx(q, p)], 2);
        }
        a2_s(" |\n");
        q = e;
    }
}

static void a2_report_uniform(const struct A2Test* t) {
    int cmp = k_a2_cbf[t->cbf].cmp;
    a2_s("\nuniform alpha ");
    a2_f(t->uni_alpha);
    a2_s(": per quad pixel p, the masks of quad 0 (model h0), and the quads that differ\n\n");
    for (int p = 0; p < 4; p++) {
        int px0 = a2_qpx(0, p), nd = 0;
        a2_s("p");
        a2_u((unsigned long long)p);
        a2_s(": ");
        a2_masks(px0, cmp);
        a2_s(" (model ");
        a2_x(g_model0[px0], 2);
        a2_s(")");
        for (int q = 1; q < A2_QUADS; q++)
            if (a2_key(a2_qpx(q, p)) != a2_key(px0)) {
                if (nd < 8) {
                    a2_s(", q");
                    a2_u((unsigned long long)q);
                    a2_s(" ");
                    a2_masks(a2_qpx(q, p), cmp);
                }
                nd++;
            }
        a2_s("; quads differing: ");
        a2_u((unsigned long long)nd);
        a2_s("\n");
    }
}

static void a2_report_totals(const struct A2Test* t) {
    int nh = a2_nh(t);
    a2_s("\npasses ");
    a2_u(g_tot.passes);
    a2_s(", repeated for unwritten readback ");
    a2_u(g_tot.retries);
    a2_s(", unwritten samples left ");
    a2_u(g_tot.unwritten);
    a2_s(", pixels with an unclassified sample ");
    a2_u(g_tot.unclass);
    a2_s(", colour != depth ");
    a2_u(g_tot.cdiff);
    if (t->ps == V_TEX) {
        a2_s(", pixels t_tex did not shade ");
        a2_u(g_tot.not_shaded);
    }
    a2_s(", GPU time ");
    a2_u(g_tot.pass_us);
    a2_s(" us\n");
    if (t->hyp == H_NOMODEL) {
        a2_s("model: none for this export format (the runs below are the result)\n");
    } else {
        for (int h = 0; h < nh; h++) {
            a2_s("model h");
            a2_u((unsigned long long)h);
            a2_s(" (");
            a2_s(k_a2_hname[t->hyp][h]);
            a2_s("): depth mismatches ");
            a2_u(g_tot.dmis[h]);
            a2_s(" of ");
            a2_u(g_tot.dchk);
            a2_s(" px, colour mismatches ");
            a2_u(g_tot.cmis[h]);
            a2_s(" of ");
            a2_u(g_tot.cchk);
            a2_s(" px\n");
        }
    }
    a2_s("ZPASS_DONE: test draws differing from the model (h0) ");
    a2_u(g_tot.zp_bad);
    a2_s(", isolation draws differing ");
    a2_u(g_tot.iso_bad);
    a2_s(", draws without any count ");
    a2_u(g_tot.zp_none);
    a2_s("\n");
    for (uint32_t i = 0; i < g_tot.nex; i++) {
        uint32_t* r = g_tot.ex[i];
        a2_s("  mismatch x ");
        a2_u(r[0]);
        a2_s(" y ");
        a2_u(r[1]);
        a2_s(" A ");
        a2_f(r[2]);
        a2_s(" B ");
        a2_x(r[3], 8);
        a2_s(": depth ");
        a2_x(r[4], 4);
        a2_s(" colour ");
        a2_x(r[5], 4);
        a2_s(", model h0 ");
        a2_x(r[6], 4);
        if (nh > 1) {
            a2_s(" h1 ");
            a2_x(r[7], 4);
        }
        a2_s("\n");
    }
    a2_s("summary ");
    a2_s(t->id);
    a2_s(": ");
    if (t->hyp == H_NOMODEL) {
        a2_s("no model");
    } else if (!a2_has_readback(t)) {
        a2_s(g_tot.zp_none                     ? "NO COUNTS"
             : (g_tot.zp_bad || g_tot.iso_bad) ? "COUNTS DIFFER"
                                               : "counts match");
    } else {
        for (int h = 0; h < nh; h++) {
            if (h)
                a2_s(", ");
            a2_s("h");
            a2_u((unsigned long long)h);
            a2_s(g_tot.dmis[h] + g_tot.cmis[h] ? " DIFFERS" : " matches");
        }
        if (g_tot.zp_bad || g_tot.iso_bad)
            a2_s(", ZPASS differs");
        if (g_tot.unwritten)
            a2_s(", UNWRITTEN READBACK");
    }
    a2_s("\n\n");
}

/* ---- refinement (bisection on the float bit pattern, per quad pixel) ---- */
static uint32_t g_ob_bits[4][A2_OBMAX], g_ob_key[4][A2_OBMAX];
static int g_ob_n[4];
static uint32_t g_tmp_bits[A2_OBMAX], g_tmp_key[A2_OBMAX];
static uint32_t g_new_bits[A2_QUADS], g_new_key[A2_QUADS];
static uint32_t g_a2_conflicts;

static void a2_ob_add(const uint32_t* al, int nused) {
    for (int p = 0; p < 4; p++) {
        int nn = 0;
        for (int q = 0; q < nused; q++) {
            uint32_t v = al[q];
            if (v > 0x40000000u)
                continue;
            uint32_t k = a2_key(a2_qpx(q, p));
            int i = nn++;
            while (i > 0 && g_new_bits[i - 1] > v) {
                g_new_bits[i] = g_new_bits[i - 1];
                g_new_key[i] = g_new_key[i - 1];
                i--;
            }
            g_new_bits[i] = v;
            g_new_key[i] = k;
        }
        int i = 0, j = 0, m = 0, n = g_ob_n[p];
        while ((i < n || j < nn) && m < A2_OBMAX) {
            uint32_t b, k;
            if (j >= nn || (i < n && g_ob_bits[p][i] <= g_new_bits[j])) {
                b = g_ob_bits[p][i];
                k = g_ob_key[p][i++];
            } else {
                b = g_new_bits[j];
                k = g_new_key[j++];
            }
            if (m && g_tmp_bits[m - 1] == b) {
                if (g_tmp_key[m - 1] != k)
                    g_a2_conflicts++;
                continue;
            }
            g_tmp_bits[m] = b;
            g_tmp_key[m++] = k;
        }
        for (int k = 0; k < m; k++) {
            g_ob_bits[p][k] = g_tmp_bits[k];
            g_ob_key[p][k] = g_tmp_key[k];
        }
        g_ob_n[p] = m;
    }
}
static int a2_refine_alpha(uint32_t* al) {
    int nint = 0;
    for (int p = 0; p < 4; p++)
        for (int i = 0; i + 1 < g_ob_n[p]; i++)
            if (g_ob_key[p][i] != g_ob_key[p][i + 1] && g_ob_bits[p][i + 1] - g_ob_bits[p][i] > 1)
                nint++;
    if (!nint)
        return 0;
    int per = A2_QUADS / nint, n = 0;
    if (per < 1)
        per = 1;
    for (int p = 0; p < 4; p++)
        for (int i = 0; i + 1 < g_ob_n[p] && n < A2_QUADS; i++) {
            uint32_t lo = g_ob_bits[p][i], hi = g_ob_bits[p][i + 1];
            if (g_ob_key[p][i] == g_ob_key[p][i + 1] || hi - lo <= 1)
                continue;
            uint32_t k = (uint32_t)per;
            if (k > hi - lo - 1)
                k = hi - lo - 1;
            for (uint32_t j = 1; j <= k && n < A2_QUADS; j++)
                al[n++] = lo + (uint32_t)((uint64_t)(hi - lo) * j / (k + 1));
        }
    for (int q = n; q < A2_QUADS; q++)
        al[q] = al[0];
    return n;
}
static void a2_report_refined(const struct A2Test* t, int passes, int converged) {
    int cmp = k_a2_cbf[t->cbf].cmp;
    a2_s("\nrefined (");
    a2_u((unsigned long long)passes);
    a2_s(" passes, ");
    a2_s(converged ? "converged" : "NOT converged");
    a2_s(", same alpha with different masks: ");
    a2_u(g_a2_conflicts);
    a2_s("): alpha runs in [0, 2] with the same colour / depth mask; \"exact\": the next run "
         "starts at the next float\n");
    for (int p = 0; p < 4; p++) {
        a2_s("\np");
        a2_u((unsigned long long)p);
        a2_s(":\n\n| from | to | colour/depth | boundary |\n|---|---|---|---|\n");
        for (int i = 0; i < g_ob_n[p];) {
            int e = i;
            while (e + 1 < g_ob_n[p] && g_ob_key[p][e + 1] == g_ob_key[p][i])
                e++;
            uint32_t k = g_ob_key[p][i];
            a2_s("| ");
            a2_f(g_ob_bits[p][i]);
            a2_s(" | ");
            a2_f(g_ob_bits[p][e]);
            a2_s(" | ");
            if (cmp)
                a2_x(k & 0xFF, 2);
            else
                a2_s("--");
            a2_s("/");
            a2_x((k >> 8) & 0xFF, 2);
            if (k >> 16)
                a2_s(" !");
            a2_s(" | ");
            a2_s(e + 1 >= g_ob_n[p]                           ? "end"
                 : g_ob_bits[p][e + 1] == g_ob_bits[p][e] + 1 ? "exact"
                                                              : "open");
            a2_s(" |\n");
            i = e + 1;
        }
    }
}

/* ---- the 16-bit sweep: masks per raw value and quad pixel ---- */
static uint8_t g_sw[4][65536];
static void a2_sweep_store(int pass) {
    for (int q = 0; q < A2_QUADS; q++)
        for (int p = 0; p < 4; p++) {
            int px = a2_qpx(q, p);
            g_sw[p][pass * A2_QUADS + q] = (uint8_t)(g_a2_dm[px] | (g_a2_an[px] ? 0x80u : 0u));
        }
}
static void a2_report_sweep(void) {
    a2_s("\nruns over the raw 16-bit alpha (depth mask; 80 = unclassified):\n");
    for (int p = 0; p < 4; p++) {
        a2_s("\np");
        a2_u((unsigned long long)p);
        a2_s(":");
        for (uint32_t v = 0; v < 65536;) {
            uint32_t e = v;
            while (e + 1 < 65536 && g_sw[p][e + 1] == g_sw[p][v])
                e++;
            a2_s(" ");
            a2_x(v, 4);
            if (e > v) {
                a2_s("..");
                a2_x(e, 4);
            }
            a2_s(":");
            a2_x(g_sw[p][v], 2);
            v = e + 1;
        }
        a2_s("\n");
    }
}

/* ---- passes ---- */
static uint32_t g_a2_bad; /* A2State.bad_variants of this launch */

static void a2_params(struct A2Ctx* c, const struct A2Test* t) {
    my_memset(c->tab, 0, 64 * 4);
    c->tab[4] = t->mrt0a;
    c->tab[5] = t->mrtza;
    c->tab[8] = 0x3F000000u;
    c->tab[9] = 0x3F800000u;
    c->tab[10] = t->ns;
    c->tab[11] = t->iter_delta;
    c->tab[32] = (t->raw_col & 0xFFFFu) | (t->raw_col << 16);
    c->tab[33] = t->raw_col & 0xFFFFu;
    build_tsharp_levels(c->tab + 48, c->tex, A2_TEX_W, A2_TEX_W, A2_TEX_LEVELS);
    build_ssharp_height(c->tab + 56); /* wrap, bilinear, linear mips */
    build_vsharp(c->tab, c->alpha, 2 * A2_PX * 4);
    build_vsharp(c->tab + 12, c->out, 2 * A2_OUT_DEPTH * 4);
    a2_tsharps(c, t);
}

/* One pass with its checks: submit (repeated up to A2_RETRIES times while the readback has
   unwritten samples), classify, check against the model. Returns a2_submit's code. */
static int a2_run_pass(struct A2Ctx* c, const struct A2Test* t, int iso, int regread) {
    uint64_t us = 0;
    int r = 0;
    for (int k = 0; k <= A2_RETRIES; k++) {
        r = a2_submit(c, t, iso, regread, &us);
        if (r)
            return r;
        g_tot.pass_us += us;
        if (!a2_has_readback(t))
            break;
        a2_classify(c, t);
        if (!g_a2_unwritten)
            break;
        if (k < A2_RETRIES)
            g_tot.retries++;
    }
    g_tot.passes++;
    if (a2_has_readback(t))
        g_tot.unwritten += g_a2_unwritten;
    if (t->ps == V_TEX) { /* the model takes the alpha t_tex sampled */
        for (int i = 0; i < A2_PX; i++) {
            c->alpha[i] = c->alpha[A2_PX + i];
            if (c->alpha[i] == A2_NOT_SHADED)
                g_tot.not_shaded++;
        }
    }
    a2_check(c, t);
    return 0;
}

/* A random configuration k of RND. */
static void a2_rnd_test(struct A2Test* o, const struct A2Test* t, int k) {
    a2_seed(t->seed + (uint32_t)k * 7919u);
    *o = *t;
    uint32_t r = a2_rand();
    o->ns = (uint8_t)(1u << (r & 3));
    o->a2m = A2M(1, (r >> 2) & 3, (r >> 4) & 3, (r >> 6) & 3, (r >> 8) & 3, (r >> 10) & 1);
    o->eqaa_dis = ((r >> 11) & 7) == 0;
    o->a2m_ns = ((r >> 14) & 3) == 0 ? (uint8_t)((r >> 16) % (a2_log2(o->ns) + 1)) : 0xFF;
    o->ps = V_F32;
    o->dbsc = 0;
    if (((r >> 18) & 7) == 0) { /* MRTZ.a through COVERAGE_TO_MASK, MRT0.a 1.0 */
        o->ps = V_ZA;
        o->dbsc = A2_COV2MASK;
    }
    o->seed = t->seed * 31u + (uint32_t)k;
}

static void a2_rnd_line(const struct A2Test* o, int k) {
    a2_s("| ");
    a2_u((unsigned long long)k);
    a2_s(" | ");
    a2_u(o->ns);
    a2_s(" | ");
    a2_x(a2_a2m(o), 8);
    a2_s(" | ");
    a2_u(o->eqaa_dis);
    a2_s(" | ");
    if (o->a2m_ns == 0xFF)
        a2_s("auto");
    else
        a2_u(o->a2m_ns);
    a2_s(" | ");
    a2_s(o->ps == V_ZA ? "MRTZ.a" : "MRT0.a");
    a2_s(" | ");
    a2_u(g_tot.dmis[0]);
    a2_s(" / ");
    a2_u(g_tot.dchk);
    a2_s(" | ");
    a2_u(g_tot.cmis[0]);
    a2_s(" / ");
    a2_u(g_tot.cchk);
    a2_s(" | ");
    a2_u(g_tot.zp_bad + g_tot.iso_bad);
    a2_s(" | ");
    a2_u(g_tot.retries);
    a2_s(" |\n");
}

/* Runs test t; returns -1 on a GPU hang. */
static int a2_run_test(struct A2Ctx* c, const struct A2Test* t, int regread, int k_index) {
    my_memset(&g_tot, 0, sizeof(g_tot));
    a2_report_header(t);
    if (a2_streq(t->id, "RND")) {
        a2_s("\n| # | samples | DB_ALPHA_TO_MASK | EQAA_DISABLE | A2M_NUM_SAMPLES | alpha | depth "
             "mismatches | colour mismatches | ZPASS differing | repeats |\n"
             "|---|---|---|---|---|---|---|---|---|---|\n");
        uint32_t bad = 0;
        for (int k = 0; k < A2_RND_CONFIGS; k++) {
            struct A2Test o;
            a2_rnd_test(&o, t, k);
            my_memset(&g_tot, 0, sizeof(g_tot));
            if ((g_a2_bad >> o.ps) & 1) {
                a2_s("| ");
                a2_u((unsigned long long)k);
                a2_s(" | skipped: its pixel shader variant crashed or hung earlier |\n");
                continue;
            }
            a2_params(c, &o);
            for (int pass = 0; pass < 2; pass++) {
                a2_fill(c, &o, pass);
                a2_flush();
                int r = a2_run_pass(c, &o, pass == 0, 0);
                if (r == -1)
                    return -1;
                if (r == -2)
                    break;
                if (pass == 0) {
                    uint32_t vm;
                    for (uint32_t blk = 0; blk <= o.ns; blk++) {
                        int64_t s = a2_zp_sum(c, blk, &vm);
                        uint32_t m = blk ? g_model_s[0][blk - 1] : g_model_pop[0];
                        if (s >= 0 && (uint64_t)s != m)
                            g_tot.zp_bad++;
                    }
                }
            }
            a2_rnd_line(&o, k);
            bad += g_tot.dmis[0] + g_tot.cmis[0] + g_tot.zp_bad;
        }
        a2_s("summary RND: ");
        a2_s(bad ? "DIFFERS" : "all match");
        a2_s("\n\n");
        return 0;
    }
    a2_params(c, t);
    int npass = t->kind == AK_SWEEP16 ? 64 : t->kind == AK_RANDOM ? 4 : 1;
    for (int pass = 0; pass < npass; pass++) {
        a2_fill(c, t, pass);
        a2_flush();
        int r = a2_run_pass(c, t, pass == 0, pass == 0 && regread);
        if (r == -1)
            return -1;
        if (r == -2) {
            a2_s("NOT SUBMITTED (command buffer overflow or submit error)\n\n");
            return 0;
        }
        if (pass == 0) {
            a2_report_regs(c, regread, k_index == 0);
            a2_report_zpass(c, t);
        }
        if (t->kind == AK_SWEEP16)
            a2_sweep_store(pass);
    }
    int show =
        t->show || (a2_has_readback(t) && t->hyp != H_NOMODEL && g_tot.dmis[0] + g_tot.cmis[0] &&
                    (a2_nh(t) == 1 || g_tot.dmis[1] + g_tot.cmis[1]));
    if (t->kind == AK_UNIFORM)
        a2_report_uniform(t);
    else if ((t->kind == AK_COARSE || t->kind == AK_RAMP) && show)
        a2_report_quads(t);
    if (t->kind == AK_SWEEP16)
        a2_report_sweep();
    if (t->refine && t->kind == AK_COARSE) {
        g_ob_n[0] = g_ob_n[1] = g_ob_n[2] = g_ob_n[3] = 0;
        g_a2_conflicts = 0;
        a2_ob_add(g_a2_qa, A2_QUADS); /* pass 0: the coarse table, still classified */
        int passes = 0, conv = 0;
        while (passes < A2_MAX_PASSES) {
            int n = a2_refine_alpha(g_a2_qa);
            if (!n) {
                conv = 1;
                break;
            }
            a2_expand_quads(c->alpha);
            a2_flush();
            int r = a2_run_pass(c, t, 0, 0);
            if (r == -1)
                return -1;
            if (r == -2)
                break;
            a2_ob_add(g_a2_qa, n);
            passes++;
        }
        a2_report_refined(t, passes, conv);
    }
    a2_report_totals(t);
    return 0;
}

/* ---- the visual pages ---- */
#define A2V_TOUCH_PAD 0x100000u /* ORBIS_PAD_BUTTON_TOUCH_PAD (as OPCODE_TEST 6) */
#define A2V_ROWS 3
#define A2V_PAGES ((A2_VIS_SLOTS + A2V_ROWS - 1) / A2V_ROWS)
#define A2V_Y0 120 /* gen_ps_a2m.py VIS_Y0, VIS_PITCH, VIS_H, VIS_X */
#define A2V_PITCH 312
#define A2V_H 256
static const int k_a2v_x[4] = {520, 792, 1064, 1336};

static uint32_t* g_a2v_tab;               /* vis's table */
static uint32_t* g_a2v_res[A2_VIS_SLOTS]; /* per pixel: [7:0] GPU, [15:8] model (h0) */
static const struct A2Test* g_a2v_test[A2_VIS_SLOTS];
static uint32_t g_a2v_mis[A2_VIS_SLOTS], g_a2v_unwritten[A2_VIS_SLOTS];
static int g_a2v_ok, g_a2v_page, g_a2v_mx = 24, g_a2v_my = 24, g_a2v_ui_key = -1;

static void a2v_set_rows(void) {
    for (int r = 0; r < A2V_ROWS; r++) {
        uint32_t* w = g_a2v_tab + 32 + 16 * r;
        int slot = g_a2v_page * A2V_ROWS + r;
        my_memset(w, 0, 64);
        if (slot >= A2_VIS_SLOTS || !g_a2v_test[slot])
            continue;
        uint32_t ns = g_a2v_test[slot]->ns;
        w[0] = 1;
        w[1] = ns;
        w[2] = (uint32_t)g_a2v_mx;
        w[3] = (uint32_t)g_a2v_my;
        w[4] = ns >= 8 ? 2u : ns >= 2 ? 1u : 0u; /* sample grid: 4 x 2, 2 x 2, 2 x 1, 1 */
        w[5] = ns >= 4 ? 1u : 0u;
        w[6] = a2_fbits(1.0f / (float)ns);
        build_vsharp(w + 8, g_a2v_res[slot], A2_PX * 4);
    }
}

/* The visual captures (every launch): one pass per V test, its result next to the model. */
static void a2_state_save(const struct A2State* st);
static int a2v_capture(struct A2Ctx* c, struct A2State* st) {
    for (int k = 0; k < A2_NTESTS; k++) {
        const struct A2Test* t = &k_a2_tests[k];
        if (!t->vis || ((g_a2_bad >> t->ps) & 1))
            continue;
        int slot = t->vis - 1;
        st->inflight = k | A2_CAPTURE;
        a2_state_save(st);
        my_memset(&g_tot, 0, sizeof(g_tot));
        a2_params(c, t);
        a2_fill(c, t, 0);
        if (a2_run_pass(c, t, 0, 0) == -1)
            return -1;
        uint32_t mis = 0;
        for (int px = 0; px < A2_PX; px++) {
            uint32_t gpu = t->nodepth ? g_a2_cm[px] : g_a2_dm[px];
            g_a2v_res[slot][px] = gpu | ((uint32_t)g_model0[px] << 8);
            mis += gpu != g_model0[px];
        }
        g_a2v_test[slot] = t;
        g_a2v_mis[slot] = mis;
        g_a2v_unwritten[slot] = g_a2_unwritten;
    }
    st->inflight = -1;
    a2_state_save(st);
    return 0;
}

static void a2v_input(uint32_t pressed) {
    if (!g_a2v_ok)
        return;
    if (pressed & A2V_TOUCH_PAD)
        g_a2v_page = (g_a2v_page + 1) % A2V_PAGES;
    if (pressed & PAD_LEFT)
        g_a2v_mx = (g_a2v_mx + 48) & 63;
    if (pressed & PAD_RIGHT)
        g_a2v_mx = (g_a2v_mx + 16) & 63;
    if (pressed & PAD_UP)
        g_a2v_my = (g_a2v_my + 56) & 63;
    if (pressed & PAD_DOWN)
        g_a2v_my = (g_a2v_my + 8) & 63;
    if (pressed & (A2V_TOUCH_PAD | PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN))
        a2v_set_rows();
}

static int a2v_cat(char* o, const char* s) {
    int n = 0;
    while (s[n]) {
        o[n] = s[n];
        n++;
    }
    return n;
}

/* Text at (x, y) wrapped at spaces to maxw pixels; returns the y of the next line. */
static int a2v_wrap(unsigned char* b, int x, int y, const char* s, int maxw,
                    const unsigned char* col) {
    char line[200];
    int n = 0, last_sp = -1;
    for (int i = 0;; i++) {
        char ch = s[i];
        line[n] = 0;
        if (ch == 0) {
            if (n)
                ui_text(b, x, y, line, col);
            return y + 26;
        }
        line[n++] = ch;
        line[n] = 0;
        if (ch == ' ')
            last_sp = n - 1;
        if (ui_text_width(line) > maxw && last_sp > 0) {
            line[last_sp] = 0;
            ui_text(b, x, y, line, col);
            y += 26;
            int rest = n - last_sp - 1;
            for (int j = 0; j < rest; j++)
                line[j] = line[last_sp + 1 + j];
            n = rest;
            last_sp = -1;
            for (int j = 0; j < n; j++)
                if (line[j] == ' ')
                    last_sp = j;
        }
    }
}

/* The page's text in a UI buffer (only when the page or the microscope moves). The panels stay
   hidden; clock mode is off. */
static void a2v_ui(void) {
    static const unsigned char white[3] = {255, 255, 255}, grey[3] = {190, 190, 196},
                               red[3] = {255, 110, 110}, green[3] = {130, 230, 130};
    g_clock_mode = 0;
    int key = g_a2v_page * 4096 + g_a2v_mx * 64 + g_a2v_my;
    if (!g_ui.ok || !g_a2v_ok || g_a2v_ui_key == key)
        return;
    int nb = (g_ui.cur + 1) % UI_BUFS;
    unsigned char* b = g_ui.buf[nb];
    g_ui.drawing = 0;
    g_ui.nclip = 0;
    ui_clear(b, 0, 0, UI_W, UI_H);
    g_ui.st[nb].valid = 0;
    g_ui.dirty[nb][0] = g_ui.dirty[nb][1] = g_ui.dirty[nb][2] = g_ui.dirty[nb][3] = 0;
    char t[200], *p = t;
    p += a2v_cat(p, "Alpha-to-mask (DB_ALPHA_TO_MASK), build " A2_BUILD "     page ");
    p += ui_fmt_int(p, g_a2v_page + 1);
    *p++ = '/';
    p += ui_fmt_int(p, A2V_PAGES);
    p += a2v_cat(p, " (touch pad)     microscope x ");
    p += ui_fmt_int(p, g_a2v_mx);
    p += a2v_cat(p, ", y ");
    p += ui_fmt_int(p, g_a2v_my);
    p += a2v_cat(p, " (d-pad)");
    *p = 0;
    ui_text(b, 40, 34, t, white);
    ui_text(b, 40, 62,
            "GPU: this console's (or emulator's) result, 64 x 64 at 4 px. Model: the PS4 rule. "
            "Grey = covered samples / samples.",
            grey);
    ui_text(b, 40, 86,
            "Diff: green equal, red different. Microscope: 16 x 8 pixels, one cell per sample: "
            "white both, red GPU only, blue model only.",
            grey);
    static const char* col[4] = {"GPU", "model", "diff", "microscope (samples)"};
    for (int r = 0; r < A2V_ROWS; r++) {
        int slot = g_a2v_page * A2V_ROWS + r, y0 = A2V_Y0 + A2V_PITCH * r;
        if (slot >= A2_VIS_SLOTS || !g_a2v_test[slot])
            continue;
        const struct A2Test* v = g_a2v_test[slot];
        for (int k = 0; k < 4; k++)
            ui_text(b, k_a2v_x[k], y0 - 6, col[k], grey);
        p = t;
        p += a2v_cat(p, v->id);
        p += a2v_cat(p, "  ");
        p += a2v_cat(p, v->what);
        *p = 0;
        int yy = a2v_wrap(b, 40, y0 + 22, t, k_a2v_x[0] - 60, white);
        p = t;
        p += ui_fmt_int(p, v->ns);
        p += a2v_cat(p, " samples, DB_ALPHA_TO_MASK 0x");
        {
            static const char h[] = "0123456789ABCDEF";
            uint32_t a = a2_a2m(v);
            for (int i = 0; i < 8; i++)
                *p++ = h[(a >> (28 - 4 * i)) & 15];
        }
        *p = 0;
        ui_text(b, 40, yy, t, grey);
        p = t;
        p += a2v_cat(p, "pixels differing from the model: ");
        p += ui_fmt_int(p, (long)g_a2v_mis[slot]);
        p += a2v_cat(p, " of 4096");
        *p = 0;
        ui_text(b, 40, yy + 26, t, g_a2v_mis[slot] ? red : green);
        if (g_a2v_unwritten[slot])
            ui_text(b, 40, yy + 52, "readback incomplete (unwritten samples)", red);
    }
    for (int i = 0; i < 4; i++)
        g_ui.rect[i] = g_ui.pill[i] = -1e6f;
    g_ui.cur = nb;
    g_a2v_ui_key = key;
    /* ps_ui composites the UI buffer inside the panels only: vis composites the labels */
    build_tsharp(g_a2v_tab + 16, g_ui.buf[nb], UI_W, UI_H);
    build_ssharp_clamp(g_a2v_tab + 24, 0);
}

/* After the final pass: the page over the whole frame (a post pass, as OPCODE_TEST 6). */
static void a2v_pass(struct PM4Builder* b, const uint32_t* bg_v) {
    if (!g_a2v_ok)
        return;
    post_pass(b, g_frame, DISPLAY_W, DISPLAY_W, DISPLAY_H, CB_INFO_RGBA16F, g_a2c.sh[V_VIS],
              A2M_VIS_RSRC1, g_a2v_tab, bg_v);
}

static int a2v_init(void) {
    g_a2v_tab = (uint32_t*)gpu_alloc_typed(0x400, 0x100, MEM_TYPE_ONION);
    if (!g_a2v_tab)
        return 0;
    my_memset(g_a2v_tab, 0, 0x400);
    for (int i = 0; i < A2_VIS_SLOTS; i++) {
        g_a2v_res[i] = (uint32_t*)gpu_alloc_typed(A2_PX * 4, 0x100, MEM_TYPE_ONION);
        if (!g_a2v_res[i])
            return 0;
        my_memset(g_a2v_res[i], 0, A2_PX * 4);
    }
    /* The labels' T# and S#: ps_ui's until a2v_ui draws a page (OPCODE_TEST 6's opt_init). */
    if (g_post_tab)
        my_memcpy(g_a2v_tab + 16, g_post_tab + UI_BLOCK * 32 + 36, 12 * 4);
    return 1;
}

/* ---- the textured net ---- */
/* The net texture: white strands every 32 texels (8 cells across 256), alpha 1 within 1.5
   texels of a strand's centre, falling to 0 over the next texel; mips by 2 x 2 box filter
   (alpha rounded), packed (LINEAR_ALIGNED, levels >= 32 px: no padding, dds_loader.h). The
   plane: view depth z = 3 + 0.8 x, projected with focal length 1.5 (clip x = 1.5 x, y = 1.5 y,
   w = z), one triangle (-1.4, -14), (-1.4, 14), (8, 0) whose edges all lie outside the viewport:
   every pixel is shaded once, with all its samples covered. UV = (x, y) / 4: the far side shows
   cells of about 4 pixels (small mips, fractional alpha), the near side about 12. */
static void a2_make_net(struct A2Ctx* c) {
    unsigned char* t = (unsigned char*)c->tex;
    for (int y = 0; y < A2_TEX_W; y++)
        for (int x = 0; x < A2_TEX_W; x++) {
            float fx = (float)(x & 31) + 0.5f, fy = (float)(y & 31) + 0.5f;
            float dx = fx < 32.0f - fx ? fx : 32.0f - fx, dy = fy < 32.0f - fy ? fy : 32.0f - fy;
            float d = dx < dy ? dx : dy;
            float a = 2.5f - d;
            a = a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a;
            unsigned char* p = t + 4 * (x + A2_TEX_W * y);
            p[0] = p[1] = p[2] = 255;
            p[3] = (unsigned char)(a * 255.0f + 0.5f);
        }
    unsigned char* src = t;
    for (int l = 1, w = A2_TEX_W / 2; l < A2_TEX_LEVELS; l++, w /= 2) {
        unsigned char* dst = src + 4 * (2 * w) * (2 * w);
        for (int y = 0; y < w; y++)
            for (int x = 0; x < w; x++) {
                const unsigned char* s0 = src + 4 * (2 * x + 2 * w * 2 * y);
                const unsigned char* s1 = s0 + 4 * 2 * w;
                unsigned a = s0[3] + s0[7] + s1[3] + s1[7];
                unsigned char* p = dst + 4 * (x + w * y);
                p[0] = p[1] = p[2] = 255;
                p[3] = (unsigned char)((a + 2) / 4);
            }
        src = dst;
    }
    static const float corner[3][2] = {{-1.4f, -14.0f}, {-1.4f, 14.0f}, {8.0f, 0.0f}};
    for (int i = 0; i < 3; i++) {
        float x = corner[i][0], y = corner[i][1], z = 3.0f + 0.8f * x;
        float* v = c->verts + 8 * i;
        v[0] = 1.5f * x;
        v[1] = 1.5f * y;
        v[2] = 0.5f * z; /* the shader exports Z 0.5 itself */
        v[3] = z;
        v[4] = 0.25f * x;
        v[5] = 0.25f * y;
        v[6] = v[7] = 0.0f;
    }
}

/* ---- the run ---- */
static int a2_alloc(struct A2Ctx* c) {
    my_memset(c, 0, sizeof(*c));
    c->dcb = (uint32_t*)gpu_alloc_typed(A2_DCB_DW * 4, 0x4000, MEM_TYPE_ONION);
    c->fence = (volatile uint32_t*)gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
    c->alpha = (uint32_t*)gpu_alloc_typed(2 * A2_PX * 4, 0x1000, MEM_TYPE_ONION);
    c->tab = (uint32_t*)gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
    c->out = (uint32_t*)gpu_alloc_typed(2 * A2_OUT_DEPTH * 4, 0x10000, MEM_TYPE_ONION);
    c->zp = (uint32_t*)gpu_alloc_typed(A2_ZP_BLOCKS * A2_ZP_STRIDE, 0x1000, MEM_TYPE_ONION);
    c->regs = (uint32_t*)gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
    c->color = gpu_alloc_typed(A2_PX * 8 * 8, 0x10000, MEM_TYPE_ONION);
    c->depth = gpu_alloc_typed(A2_PX * 4 * 8, 0x10000, MEM_TYPE_ONION);
    c->scratch = gpu_alloc_typed(A2_PX * 8, 0x10000, MEM_TYPE_ONION);
    c->vs = gpu_alloc_typed(sizeof(a2m_vs_a2m_bin) + 256, 0x1000, MEM_TYPE_ONION);
    c->vs_net = gpu_alloc_typed(sizeof(a2m_vs_net_bin) + 256, 0x1000, MEM_TYPE_ONION);
    c->tex = gpu_alloc_typed(A2_TEX_W * A2_TEX_W * 4 * 2, 0x10000, MEM_TYPE_ONION);
    c->verts = (float*)gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
    int ok = c->dcb && c->fence && c->alpha && c->tab && c->out && c->zp && c->regs && c->color &&
             c->depth && c->scratch && c->vs && c->vs_net && c->tex && c->verts &&
             !((uint64_t)(uintptr_t)c->vs >> 40) && !((uint64_t)(uintptr_t)c->vs_net >> 40);
    if (ok) {
        my_memcpy(c->vs, a2m_vs_a2m_bin, sizeof(a2m_vs_a2m_bin));
        my_memcpy(c->vs_net, a2m_vs_net_bin, sizeof(a2m_vs_net_bin));
        a2_make_net(c);
    }
    for (int i = 0; ok && i < A2_NSH; i++) {
        c->sh[i] = gpu_alloc_typed(k_a2_sh[i].size + 256, 0x1000, MEM_TYPE_ONION);
        if (!c->sh[i] || ((uint64_t)(uintptr_t)c->sh[i] >> 40))
            ok = 0;
        else
            my_memcpy(c->sh[i], k_a2_sh[i].bin, k_a2_sh[i].size);
    }
    if (ok) {
        c->fence[0] = c->fence[1] = 0;
        c->fv = 0;
    }
    return ok;
}

static int a2m_run(void) {
    a2_paths();
    struct A2State st;
    int resumed = a2_state_load(&st), tests_done = 0;
    if (!resumed) {
        my_memset(&st, 0, sizeof(st));
        for (int i = 0; A2_BUILD[i]; i++)
            st.build[i] = A2_BUILD[i];
        st.inflight = -1;
    }
    /* The app ended during a test or a capture: skip that pixel shader variant from now on (an
       emulator that cannot translate it crashes; RND's variants are its own). */
    if (st.inflight >= 0) {
        const struct A2Test* t = &k_a2_tests[st.inflight & 0xFFFF];
        if (!a2_streq(t->id, "RND"))
            st.bad_variants |= a2_group(t->ps);
        if (st.inflight & A2_CAPTURE) {
            trace_msg("a2m: the app ended during a visual capture; its variant is skipped now\n");
            st.inflight = -1;
            a2_state_save(&st);
        }
    }
    g_a2_bad = st.bad_variants;
    if (resumed && st.next >= A2_NTESTS && st.inflight < 0)
        tests_done = 1;
    g_a2c_ok = a2_alloc(&g_a2c);
    if (!g_a2c_ok) {
        trace_msg("a2m: allocation failed\n");
        return 0;
    }
    struct A2Ctx* c = &g_a2c;
    if (!tests_done) {
        g_a2_fd = sceKernelOpen(g_a2_log_path, resumed ? 0x209 : 0x601, 0x1FF);
        if (g_a2_fd < 0) {
            trace_msg("a2m: log open failed\n");
            return 0;
        }
        a2_s(resumed ? "\n## resumed at "
                     : "# Alpha-to-mask on PS4 hardware, build " A2_BUILD "\n\n## start at ");
        a2_s(st.next < A2_NTESTS ? k_a2_tests[st.next].id : "(end)");
        a2_s("\n\n");
        if (st.inflight >= 0) {
            a2_s("### ");
            a2_s(k_a2_tests[st.inflight].id);
            a2_s(": HUNG - the app ended before its fence (crash or GPU hang); its pixel shader "
                 "variant is skipped from now on\nsummary ");
            a2_s(k_a2_tests[st.inflight].id);
            a2_s(": HUNG\n\n");
            if (st.inflight == 0)
                st.regread_bad = 1;
            st.next = st.inflight + 1;
            st.inflight = -1;
            a2_state_save(&st);
        }
        a2_flush();
        for (int k = st.next; k < A2_NTESTS; k++) {
            const struct A2Test* t = &k_a2_tests[k];
            if ((st.bad_variants >> t->ps) & 1) {
                a2_s("### ");
                a2_s(t->id);
                a2_s(": ");
                a2_s(t->what);
                a2_s("\nSKIPPED: its pixel shader variant ");
                a2_s(k_a2_vname[t->ps]);
                a2_s(" crashed or hung earlier\nsummary ");
                a2_s(t->id);
                a2_s(": SKIPPED\n\n");
                st.next = k + 1;
                a2_state_save(&st);
                a2_flush();
                continue;
            }
            st.inflight = k;
            st.next = k;
            a2_state_save(&st);
            if (a2_run_test(c, t, !st.regread_bad, k) == -1) {
                a2_s("TIMEOUT: no fence after 2 s - GPU hung; close the app, relaunch to "
                     "continue\nsummary ");
                a2_s(t->id);
                a2_s(": HUNG\n\n");
                if (k == 0)
                    st.regread_bad = 1;
                if (!a2_streq(t->id, "RND"))
                    st.bad_variants |= a2_group(t->ps);
                st.inflight = -1;
                st.next = k + 1;
                a2_state_save(&st);
                a2_flush();
                return -1;
            }
            st.inflight = -1;
            st.next = k + 1;
            a2_state_save(&st);
            a2_flush();
        }
        a2_s("## all tests done\n");
        a2_flush();
        sceKernelClose(g_a2_fd);
        g_a2_fd = -1;
    } else {
        trace_msg("a2m: all tests done earlier; delete a2m.state to run them again\n");
    }
    if (!a2v_init()) {
        trace_msg("a2m: visual pages unavailable (allocation)\n");
        return 0;
    }
    if (a2v_capture(c, &st) < 0)
        return -1;
    a2v_set_rows();
    g_a2v_ok = 1;
    return 0;
}
