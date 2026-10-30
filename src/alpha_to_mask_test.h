/* ALPHA_TO_MASK_TEST: alpha-to-mask (DB_ALPHA_TO_MASK, GCN "alpha to coverage") on the PS4 GPU, run
 * once before the main loop, then the visual pages over the frame.
 *
 * Every pass is one command buffer on a 64 x 64 target of ns samples (RGBA16F colour, tile index
 * 13, no FMASK / CMASK, and Z_32_FLOAT depth, ARRAY_1D_TILED_THIN1, no HTILE - the scene's MSAA
 * layout):
 *   clear draw   all samples: colour 0.25 (ps clr), depth 1.0 (DB_RENDER_CONTROL DEPTH_CLEAR)
 *   readback     ps rd: image_load of every sample of the colour and the depth, stored raw
 *   test draw    the test's state and pixel shader (ps_alpha_to_mask.h t_*), z 0.5;
 *                ZPASS_DONE before / after it; COPY_DATA of the registers it set
 *   readback     again, after the test draw
 *   isolation    per sample s: the clear draw, then the test draw with PA_SC_AA_MASK = 1 << s,
 *                ZPASS_DONE around it
 *   end          PS_PARTIAL_FLUSH, ACQUIRE_MEM (CB, DB, TC, TCL1), two EVENT_WRITE_EOP (Mesa
 *                ac_emit_cp_release_mem: GFX7 needs two for all engines to be idle)
 * Every pixel has its own alpha A and values B, C (tables at the V#). A sample's colour counts as
 * written when its readback changed, its depth when it reads 0.5 after 1.0; any other depth is
 * unclassified, and an unwritten readback (0xA5A5A5A5) repeats the pass. Every pass is checked
 * against the model below (one or two hypotheses per test) and the ZPASS_DONE counts against the
 * readback and the model; the first pass of a test runs twice and must repeat bit for bit.
 *
 * Model (from the hardware runs, checked on every pixel here):
 *   p = (x & 1) + 2 (y & 1); off = 2 OFFSET_p + OFFSET_ROUND (eighths of a sample);
 *   n = 1 << ALPHA_TO_MASK_NUM_SAMPLES; a = alpha clamped to [0, 1], NaN 0;
 *   c(k) = min(k, floor(a k + off / 8));
 *   EQAA: for k = n, n / 2, .., 2 set bits [k / 2, k / 2 + c(k) - c(k / 2)); bit 0 if c(1);
 *   ALPHA_TO_MASK_EQAA_DISABLE: (1 << c(n)) - 1; sample s takes bit s % n (repeated when n is
 *   below the sample count, truncated above).
 *
 * Results: ShadCube4 alpha to mask.log next to the trace log. A hang (fence timeout) ends the run;
 * the state file alpha_to_mask.state makes the next launch log it and continue after it. If R0 (the
 * register readback probe) hangs, the later tests run without COPY_DATA. A new ALPHA_TO_MASK_BUILD
 * starts over (log truncated); after the last test the run is not repeated until
 * alpha_to_mask.state is deleted. The visual captures run on every launch. */
#include "ps_alpha_to_mask.h"

#define ALPHA_TO_MASK_BUILD "alpha-to-mask-14"
#define ALPHA_TO_MASK_TIMEOUT_US 2000000u
#define ALPHA_TO_MASK_W 64
#define ALPHA_TO_MASK_PX (ALPHA_TO_MASK_W * ALPHA_TO_MASK_W)
#define ALPHA_TO_MASK_QUADS 1024
#define ALPHA_TO_MASK_SPECIALS 128
#define ALPHA_TO_MASK_MAX_PASSES 12
#define ALPHA_TO_MASK_RETRIES 3
#define ALPHA_TO_MASK_OBMAX (ALPHA_TO_MASK_QUADS * (ALPHA_TO_MASK_MAX_PASSES + 1))
#define ALPHA_TO_MASK_DCB_DW 0x4000u
#define ALPHA_TO_MASK_OUT_DEPTH 0x8000u /* depth raw values, in dwords from the readback base */
#define ALPHA_TO_MASK_LOG_BUF 0x40000u
#define ALPHA_TO_MASK_ZP_STRIDE 256u /* ZPASS_DONE block: 16 bytes per RB (begin, end) */
#define ALPHA_TO_MASK_ZP_RBS 16
#define ALPHA_TO_MASK_ZP_BLOCKS 17 /* the test draw + 16 isolation draws */
#define ALPHA_TO_MASK_UNWRITTEN 0xA5A5A5A5u
#define ALPHA_TO_MASK_VIS_SLOTS 18
#define ALPHA_TO_MASK_TEX_W 256
#define ALPHA_TO_MASK_TEX_LEVELS                                                                   \
    4 /* 256 .. 32: LINEAR_ALIGNED levels >= 32 px pack without padding */
#define ALPHA_TO_MASK_NOT_SHADED 0xFFFFFFFFu

/* CP_COHER_CNTL (AMD gfx7 register table) */
#define ALPHA_TO_MASK_COHER_DEST_BASE_0 (1u << 0)
#define ALPHA_TO_MASK_COHER_DB_DEST_BASE (1u << 14)
#define ALPHA_TO_MASK_COHER_TCL1 (1u << 22)
#define ALPHA_TO_MASK_COHER_TC (1u << 23)
#define ALPHA_TO_MASK_COHER_CB (1u << 25)
#define ALPHA_TO_MASK_COHER_DB (1u << 26)
#define ALPHA_TO_MASK_COHER_KCACHE (1u << 27)
#define ALPHA_TO_MASK_COHER_ICACHE (1u << 29)

/* Context register indices (register - 0x28000) / 4, AMD gfx7 register table. */
#define ALPHA_TO_MASK_DB_RENDER_CONTROL 0x000
#define ALPHA_TO_MASK_DB_COUNT_CONTROL 0x001
#define ALPHA_TO_MASK_DB_DEPTH_CLEAR 0x00B
#define ALPHA_TO_MASK_DB_DEPTH_CONTROL 0x200
#define ALPHA_TO_MASK_DB_EQAA 0x201
#define ALPHA_TO_MASK_DB_SHADER_CONTROL 0x203
#define ALPHA_TO_MASK_PA_SC_MODE_CNTL_1 0x293
#define ALPHA_TO_MASK_DB_ALPHA_TO_MASK 0x2DC
#define ALPHA_TO_MASK_CB_BLEND0_CONTROL 0x1E0
#define ALPHA_TO_MASK_PA_SC_WINDOW_OFFSET 0x080
#define ALPHA_TO_MASK_PA_SU_SC_MODE_CNTL 0x205

/* DB_ALPHA_TO_MASK: ENABLE [0], OFFSET0..3 [9:8] [11:10] [13:12] [15:14], OFFSET_ROUND [16]. */
#define DB_ALPHA_TO_MASK_VALUE(en, o0, o1, o2, o3, r)                                              \
    ((uint32_t)(en) | ((uint32_t)(o0) << 8) | ((uint32_t)(o1) << 10) | ((uint32_t)(o2) << 12) |    \
     ((uint32_t)(o3) << 14) | ((uint32_t)(r) << 16))
#define ALPHA_TO_MASK_D0                                                                           \
    DB_ALPHA_TO_MASK_VALUE(1, 2, 2, 2, 2, 0) /* radeonsi / RADV without dithering */
#define ALPHA_TO_MASK_D1 DB_ALPHA_TO_MASK_VALUE(1, 3, 1, 0, 2, 1) /* radeonsi / RADV dithered */
#define ALPHA_TO_MASK_OFF0 DB_ALPHA_TO_MASK_VALUE(0, 2, 2, 2, 2, 0)

/* DB_SHADER_CONTROL bits */
#define ALPHA_TO_MASK_ZEXP (1u << 0)
#define ALPHA_TO_MASK_KILL (1u << 6)
#define ALPHA_TO_MASK_COV2MASK (1u << 7)
#define ALPHA_TO_MASK_AA_ZERO 0x10000u /* aa_mask: PA_SC_AA_MASK 0 for every pixel */
#define ALPHA_TO_MASK_DEPTH_BEFORE_SHADER (1u << 12)
#define ALPHA_TO_MASK_MASKEXP (1u << 8)
#define ALPHA_TO_MASK_EXEC_NOOP (1u << 10)
#define ALPHA_TO_MASK_DISABLE_BIT (1u << 11)
#define ALPHA_TO_MASK_ZORDER(z) ((uint32_t)(z) << 4)

/* CB_BLEND0_CONTROL: SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ADD for colour and alpha, ENABLE. */
#define ALPHA_TO_MASK_BLEND_SRC_ALPHA (4u | (5u << 8) | (4u << 16) | (5u << 24) | (1u << 30))
/* Dual source: ONE + SRC1_ALPHA (17) for colour and alpha, ADD, ENABLE. */
#define ALPHA_TO_MASK_BLEND_DUAL (1u | (17u << 8) | (1u << 16) | (17u << 24) | (1u << 30))

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
    V_EN8,
    V_EN0,
    V_EN9,
    V_EN1,
    V_C3,
    V_CC,
    V_C4,
    V_C8,
    V_ALL,
    V_KILL2,
    V_KILL3,
    V_KILL_NOVM,
    V_KP_E64,
    V_KP_F32,
    V_KP_GT,
    V_KP_VALU,
    V_KP_SMOV,
    V_KP_SAVEEXEC,
    V_KP_TWOEXP,
    V_KP_RESTORE,
    V_MARK,
    V_COV,
    V_PKNORM_U16,
    V_PKNORM_I16,
    V_PKRTZ,
    V_PK_U16,
    V_PK_I16,
    V_CLR,
    V_RD,
    V_VIS,
    ALPHA_TO_MASK_NSH
};
static const char* const k_alpha_to_mask_vname[ALPHA_TO_MASK_NSH] = {
    "t_f32 (MRT0 c, c, c, A)",
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
    "t_en8 (MRT0 enable mask 0x8: alpha only)",
    "t_en0 (MRT0 enable mask 0x0)",
    "t_en9 (MRT0 enable mask 0x9: red, alpha)",
    "t_en1 (MRT0 enable mask 0x1: red only)",
    "t_c3 (MRT0 FP16 compressed, enable mask 0x3)",
    "t_cc (MRT0 FP16 compressed, enable mask 0xC)",
    "t_c4 (MRT0 FP16 compressed, enable mask 0x4)",
    "t_c8 (MRT0 FP16 compressed, enable mask 0x8)",
    "t_all (kill B bit 8, MRTZ mask B, MRTZ.a C, MRT0.a A)",
    "t_kill2 (v_cmpx, branch to a null export where B & 1)",
    "t_kill3 (v_cmp, s_and_b64 exec where B & 1)",
    "t_kill_novm (v_cmpx where B & 1, export without the valid mask)",
    "t_kp_e64 (v_cmpx_eq_u32_e64 to s[24:25] where B & 1, exp vm)",
    "t_kp_f32 (v_cmpx_gt_f32 where B & 1, exp vm)",
    "t_kp_gt (v_cmpx_gt_u32 where B & 1, exp vm)",
    "t_kp_valu (v_cmpx, then VALU work where B & 1, exp vm)",
    "t_kp_smov (v_cmpx, exec copied through s[24:25] where B & 1, exp vm)",
    "t_kp_saveexec (v_cmp, s_and_saveexec_b64 where B & 1, exp vm)",
    "t_kp_twoexp (v_cmpx, MRT0 without vm, null export with vm where B & 1, exp vm)",
    "t_kp_restore (v_cmpx, then s_mov_b64 exec, -1 where B & 1, exp vm)",
    "t_mark (1 to B: the pixel shader ran)",
    "t_cov (the sample coverage input to B)",
    "t_pknorm_u16 (v_cvt_pknorm_u16_f32 A, C to B)",
    "t_pknorm_i16 (v_cvt_pknorm_i16_f32 A, C to B)",
    "t_pkrtz (v_cvt_pkrtz_f16_f32 A, C to B)",
    "t_pk_u16 (v_cvt_pk_u16_u32 A, C to B)",
    "t_pk_i16 (v_cvt_pk_i16_i32 A, C to B)",
    "clr",
    "rd",
    "vis"};
static const struct {
    const uint32_t* bin;
    uint32_t size, rsrc1;
} k_alpha_to_mask_sh[ALPHA_TO_MASK_NSH] = {
    {alpha_to_mask_t_f32_bin, sizeof(alpha_to_mask_t_f32_bin), ALPHA_TO_MASK_T_F32_RSRC1},
    {alpha_to_mask_t_f16_bin, sizeof(alpha_to_mask_t_f16_bin), ALPHA_TO_MASK_T_F16_RSRC1},
    {alpha_to_mask_t_u16_bin, sizeof(alpha_to_mask_t_u16_bin), ALPHA_TO_MASK_T_U16_RSRC1},
    {alpha_to_mask_t_raw_bin, sizeof(alpha_to_mask_t_raw_bin), ALPHA_TO_MASK_T_RAW_RSRC1},
    {alpha_to_mask_t_za_bin, sizeof(alpha_to_mask_t_za_bin), ALPHA_TO_MASK_T_ZA_RSRC1},
    {alpha_to_mask_t_az_bin, sizeof(alpha_to_mask_t_az_bin), ALPHA_TO_MASK_T_AZ_RSRC1},
    {alpha_to_mask_t_zab_bin, sizeof(alpha_to_mask_t_zab_bin), ALPHA_TO_MASK_T_ZAB_RSRC1},
    {alpha_to_mask_t_za0_bin, sizeof(alpha_to_mask_t_za0_bin), ALPHA_TO_MASK_T_ZA0_RSRC1},
    {alpha_to_mask_t_mask_bin, sizeof(alpha_to_mask_t_mask_bin), ALPHA_TO_MASK_T_MASK_RSRC1},
    {alpha_to_mask_t_kill_bin, sizeof(alpha_to_mask_t_kill_bin), ALPHA_TO_MASK_T_KILL_RSRC1},
    {alpha_to_mask_t_m1_bin, sizeof(alpha_to_mask_t_m1_bin), ALPHA_TO_MASK_T_M1_RSRC1},
    {alpha_to_mask_t_m1o_bin, sizeof(alpha_to_mask_t_m1o_bin), ALPHA_TO_MASK_T_M1O_RSRC1},
    {alpha_to_mask_t_iter_bin, sizeof(alpha_to_mask_t_iter_bin), ALPHA_TO_MASK_T_ITER_RSRC1},
    {alpha_to_mask_t_noa_bin, sizeof(alpha_to_mask_t_noa_bin), ALPHA_TO_MASK_T_NOA_RSRC1},
    {alpha_to_mask_t_tex_bin, sizeof(alpha_to_mask_t_tex_bin), ALPHA_TO_MASK_T_TEX_RSRC1},
    {alpha_to_mask_t_en8_bin, sizeof(alpha_to_mask_t_en8_bin), ALPHA_TO_MASK_T_EN8_RSRC1},
    {alpha_to_mask_t_en0_bin, sizeof(alpha_to_mask_t_en0_bin), ALPHA_TO_MASK_T_EN0_RSRC1},
    {alpha_to_mask_t_en9_bin, sizeof(alpha_to_mask_t_en9_bin), ALPHA_TO_MASK_T_EN9_RSRC1},
    {alpha_to_mask_t_en1_bin, sizeof(alpha_to_mask_t_en1_bin), ALPHA_TO_MASK_T_EN1_RSRC1},
    {alpha_to_mask_t_c3_bin, sizeof(alpha_to_mask_t_c3_bin), ALPHA_TO_MASK_T_C3_RSRC1},
    {alpha_to_mask_t_cc_bin, sizeof(alpha_to_mask_t_cc_bin), ALPHA_TO_MASK_T_CC_RSRC1},
    {alpha_to_mask_t_c4_bin, sizeof(alpha_to_mask_t_c4_bin), ALPHA_TO_MASK_T_C4_RSRC1},
    {alpha_to_mask_t_c8_bin, sizeof(alpha_to_mask_t_c8_bin), ALPHA_TO_MASK_T_C8_RSRC1},
    {alpha_to_mask_t_all_bin, sizeof(alpha_to_mask_t_all_bin), ALPHA_TO_MASK_T_ALL_RSRC1},
    {alpha_to_mask_t_kill2_bin, sizeof(alpha_to_mask_t_kill2_bin), ALPHA_TO_MASK_T_KILL2_RSRC1},
    {alpha_to_mask_t_kill3_bin, sizeof(alpha_to_mask_t_kill3_bin), ALPHA_TO_MASK_T_KILL3_RSRC1},
    {alpha_to_mask_t_kill_novm_bin, sizeof(alpha_to_mask_t_kill_novm_bin),
     ALPHA_TO_MASK_T_KILL_NOVM_RSRC1},
    {alpha_to_mask_t_kp_e64_bin, sizeof(alpha_to_mask_t_kp_e64_bin), ALPHA_TO_MASK_T_KP_E64_RSRC1},
    {alpha_to_mask_t_kp_f32_bin, sizeof(alpha_to_mask_t_kp_f32_bin), ALPHA_TO_MASK_T_KP_F32_RSRC1},
    {alpha_to_mask_t_kp_gt_bin, sizeof(alpha_to_mask_t_kp_gt_bin), ALPHA_TO_MASK_T_KP_GT_RSRC1},
    {alpha_to_mask_t_kp_valu_bin, sizeof(alpha_to_mask_t_kp_valu_bin),
     ALPHA_TO_MASK_T_KP_VALU_RSRC1},
    {alpha_to_mask_t_kp_smov_bin, sizeof(alpha_to_mask_t_kp_smov_bin),
     ALPHA_TO_MASK_T_KP_SMOV_RSRC1},
    {alpha_to_mask_t_kp_saveexec_bin, sizeof(alpha_to_mask_t_kp_saveexec_bin),
     ALPHA_TO_MASK_T_KP_SAVEEXEC_RSRC1},
    {alpha_to_mask_t_kp_twoexp_bin, sizeof(alpha_to_mask_t_kp_twoexp_bin),
     ALPHA_TO_MASK_T_KP_TWOEXP_RSRC1},
    {alpha_to_mask_t_kp_restore_bin, sizeof(alpha_to_mask_t_kp_restore_bin),
     ALPHA_TO_MASK_T_KP_RESTORE_RSRC1},
    {alpha_to_mask_t_mark_bin, sizeof(alpha_to_mask_t_mark_bin), ALPHA_TO_MASK_T_MARK_RSRC1},
    {alpha_to_mask_t_cov_bin, sizeof(alpha_to_mask_t_cov_bin), ALPHA_TO_MASK_T_COV_RSRC1},
    {alpha_to_mask_t_pknorm_u16_bin, sizeof(alpha_to_mask_t_pknorm_u16_bin),
     ALPHA_TO_MASK_T_PKNORM_U16_RSRC1},
    {alpha_to_mask_t_pknorm_i16_bin, sizeof(alpha_to_mask_t_pknorm_i16_bin),
     ALPHA_TO_MASK_T_PKNORM_I16_RSRC1},
    {alpha_to_mask_t_pkrtz_bin, sizeof(alpha_to_mask_t_pkrtz_bin), ALPHA_TO_MASK_T_PKRTZ_RSRC1},
    {alpha_to_mask_t_pk_u16_bin, sizeof(alpha_to_mask_t_pk_u16_bin), ALPHA_TO_MASK_T_PK_U16_RSRC1},
    {alpha_to_mask_t_pk_i16_bin, sizeof(alpha_to_mask_t_pk_i16_bin), ALPHA_TO_MASK_T_PK_I16_RSRC1},
    {alpha_to_mask_clr_bin, sizeof(alpha_to_mask_clr_bin), ALPHA_TO_MASK_CLR_RSRC1},
    {alpha_to_mask_rd_bin, sizeof(alpha_to_mask_rd_bin), ALPHA_TO_MASK_RD_RSRC1},
    {alpha_to_mask_vis_bin, sizeof(alpha_to_mask_vis_bin), ALPHA_TO_MASK_VIS_RSRC1},
};

/* Colour target formats: CB_COLOR0_INFO (FORMAT [6:2], NUMBER_TYPE [10:8]) and the readback T#
   (DATA_FORMAT, NUM_FORMAT); cmp: the colour mask is derived (the test colour reads 1.0). */
enum { CBF_F16, CBF_NONE, CBF_U8, CBF_R32F, CBF_U16I };
static const struct {
    const char* name;
    uint32_t info, dfmt, nfmt, cmp;
} k_alpha_to_mask_cbf[5] = {
    {"16_16_16_16 FLOAT", (0xCu << 2) | (7u << 8), 12, 7, 1},
    {"none", 0, 12, 7, 1},
    {"8_8_8_8 UNORM", (0xAu << 2) | (0u << 8), 10, 0, 1},
    {"32 FLOAT", (0x4u << 2) | (7u << 8), 4, 7, 1},
    {"16_16_16_16 UINT", (0xCu << 2) | (4u << 8), 12, 4, 0},
};

/* Alpha tables: the coarse table (128 specials, then 896 steps over [0, 1], one value per 2 x 2
   quad), one value everywhere, the visual ramp (quad q: q / 1023) and net, random values, or the
   16-bit sweep (pass j, quad q: raw value 1024 j + q). */
enum { AK_COARSE, AK_UNIFORM, AK_RAMP, AK_NET, AK_RANDOM, AK_SWEEP16, AK_TEX, AK_BOUND, AK_TIES };
/* The second table B: none, a random alpha, a random sample mask, a random kill flag (bit 0). */
enum { BM_NONE, BM_ALPHA, BM_MASK, BM_KILL, BM_ALL, BM_C };
/* Hypotheses (h0 / h1) where the first hardware run left a question open. */
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
    H_CLAMP,   /* NUM_SAMPLES above the sample count: h0 truncated mask, h1 NUM_SAMPLES clamped */
    H_COMPR,   /* compressed MRT0 alpha enabled by: h0 enable bit 2 (VSRC1), h1 bit 3 */
    H_NOMRT0,  /* no MRT0 export, COL0 with alpha: h0 no alpha-to-mask, h1 alpha read as 0 */
    H_VM,      /* export without vm after v_cmpx: h0 depth written, colour not, h1 both killed */
    H_KILLEN,  /* valid-mask kill without DB_SHADER_CONTROL.KILL_ENABLE: h0 killed, h1 not */
    H_ROUND,   /* v_cvt_pknorm: h0 exact product, half to even, h1 f32 product, half to even */
    H_ROUND2,  /* v_cvt_pknorm: h0 exact product, half to even, h1 f32 product, half away */
    H_SAT,     /* v_cvt_pk_u16_u32 / i16_i32: h0 saturate, h1 truncate to 16 bits */
    H_RESTORE, /* v_cmpx, then exec set to all lanes before exp vm: h0 not killed, h1 killed */
    H_TWOEXP,  /* v_cmpx, MRT0 without vm, null export with vm: h0 killed, h1 not killed */
    H_CAP,     /* NUM_SAMPLES 5..7 with EQAA_DISABLE: h0 16 samples, h1 2^NUM_SAMPLES */
    H_AAPS,    /* pixel with PA_SC_AA_MASK 0: h0 the pixel shader does not run, h1 it runs */
    H_COVIN,   /* sample coverage input: h0 includes PA_SC_AA_MASK, h1 does not */
    H_DBS,     /* DEPTH_BEFORE_SHADER: h0 shader kills miss depth and ZPASS, h1 they reach them */
    H_MASK_EXPORT, /* MASK_EXPORT_NUM_SAMPLES below the samples: h0 repeated, h1 rest cleared */
};
static const char* const k_alpha_to_mask_hname[][2] = {
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
    {"n = 2^NUM_SAMPLES, truncated", "n clamped to the samples"},
    {"enable bit 2", "enable bit 3"},
    {"no alpha-to-mask", "alpha read as 0"},
    {"depth written, colour not", "killed"},
    {"killed", "not killed"},
    {"exact product, half to even", "f32 product, half to even"},
    {"exact product, half to even", "f32 product, half away from zero"},
    {"saturate", "truncate"},
    {"not killed", "killed"},
    {"killed", "not killed"},
    {"16 samples", "2^NUM_SAMPLES samples"},
    {"pixel shader not run", "pixel shader runs"},
    {"coverage input includes AA_MASK", "coverage input without AA_MASK"},
    {"depth / ZPASS from coverage only", "shader kills reach depth / ZPASS"},
    {"low bits repeated", "only the low bits"},
};

struct AlphaToMaskTest {
    const char* id;
    const char* what;
    uint8_t kind; /* AK_* */
    uint8_t ns;   /* coverage = colour = depth samples (16: coverage only, no targets) */
    uint8_t alpha_to_mask_num_samples; /* DB_EQAA.ALPHA_TO_MASK_NUM_SAMPLES; 0xFF: log2(ns) */
    uint8_t mask_export_ns;            /* DB_EQAA.MASK_EXPORT_NUM_SAMPLES; 0xFF: log2(ns) */
    uint8_t eqaa_dis;                  /* DB_EQAA.ALPHA_TO_MASK_EQAA_DISABLE */
    uint8_t msaa_en;                   /* PA_SC_MODE_CNTL_0.MSAA_ENABLE; 2: ns > 1 */
    uint8_t ps_iter; /* DB_EQAA.PS_ITER_SAMPLES (log2) + PA_SC_MODE_CNTL_1.PS_ITER_SAMPLE */
    uint8_t ps;      /* V_* */
    uint8_t cbf;     /* CBF_* */
    uint8_t nodepth; /* no depth target */
    uint8_t refine;
    uint8_t show;  /* log the alpha table even when the model matches */
    uint8_t bmode; /* BM_* */
    uint8_t hyp;   /* H_* */
    uint8_t vis;   /* visual slot + 1 */
    uint8_t risky;
    uint32_t colfmt;           /* SPI_SHADER_COL_FORMAT; 0: 9 (32_ABGR) for MRT0 */
    uint32_t cbsm;             /* CB_SHADER_MASK; 0: 0xF */
    uint32_t cbtm;             /* CB_TARGET_MASK; 0: 0xF */
    uint32_t db_alpha_to_mask; /* DB_ALPHA_TO_MASK; 0: ALPHA_TO_MASK_D0 */
    uint32_t dbsc;             /* DB_SHADER_CONTROL bits besides the variant's */
    uint32_t aa_mask;          /* PA_SC_AA_MASK per pixel (16 bits); 0: all */
    uint8_t zfunc;             /* DB_DEPTH_CONTROL.ZFUNC + 1; 0: ALWAYS */
    uint32_t colour;           /* the test colour's bits; 0: 1.0 */
    uint64_t aa_quad;       /* PA_SC_AA_MASK per quad pixel (16 bits each); 0: aa_mask everywhere */
    uint8_t cb_disable;     /* CB_COLOR_CONTROL.MODE CB_DISABLE */
    uint8_t no_kill_enable; /* DB_SHADER_CONTROL.KILL_ENABLE off for a kill variant */
    uint8_t locs;           /* 1: every sample at the pixel centre */
    uint32_t uni_alpha, mrt0a, mrtza, iter_delta, blend, wofs, raw_col, seed;
};
#define ALPHA_TO_MASK_ZERO_COL 0xFFFFFFFFu  /* colfmt for "COL0 ZERO" (0 means the default) */
#define ALPHA_TO_MASK_ZERO_MASK 0xFFFFFFFFu /* cbsm / cbtm for "0" */

#define ALPHA_TO_MASK_CASE(...)                                                                    \
    {.ns = 4,                                                                                      \
     .alpha_to_mask_num_samples = 0xFF,                                                            \
     .mask_export_ns = 0xFF,                                                                       \
     .msaa_en = 2,                                                                                 \
     .mrt0a = 0x3F800000u,                                                                         \
     .mrtza = 0x3F800000u,                                                                         \
     __VA_ARGS__}

#define ALPHA_TO_MASK_O0123 DB_ALPHA_TO_MASK_VALUE(1, 0, 1, 2, 3, 0)
#define SWEEP16_CASE(i, w, fmt, col, h, m)                                                         \
    ALPHA_TO_MASK_CASE(.id = i, .what = w, .db_alpha_to_mask = m, .ps = V_RAW, .colfmt = fmt,      \
                       .kind = AK_SWEEP16, .raw_col = col, .hyp = h)

/* Designated initializers after ALPHA_TO_MASK_T's defaults override them (C11 6.7.9). */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winitializer-overrides"
static const struct AlphaToMaskTest k_alpha_to_mask_tests[] = {
    ALPHA_TO_MASK_CASE(.id = "R0",
                       .what = "register readback probe (COPY_DATA); base state, 4x, D0"),
    /* gating, 4x, D0, t_f32 */
    ALPHA_TO_MASK_CASE(.id = "G01", .what = "ENABLE 0, ALPHA_TO_MASK_DISABLE 0",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0),
    ALPHA_TO_MASK_CASE(.id = "G02", .what = "ENABLE 1, ALPHA_TO_MASK_DISABLE 0 (base)", .refine = 1,
                       .show = 1),
    ALPHA_TO_MASK_CASE(.id = "G03", .what = "ENABLE 0, ALPHA_TO_MASK_DISABLE 1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .dbsc = ALPHA_TO_MASK_DISABLE_BIT),
    ALPHA_TO_MASK_CASE(.id = "G04", .what = "ENABLE 1, ALPHA_TO_MASK_DISABLE 1",
                       .dbsc = ALPHA_TO_MASK_DISABLE_BIT),
    ALPHA_TO_MASK_CASE(.id = "G05", .what = "CB_SHADER_MASK MRT0 = 0x7 (no alpha)", .cbsm = 0x7),
    ALPHA_TO_MASK_CASE(.id = "G06", .what = "CB_TARGET_MASK MRT0 = 0x7 (no alpha)", .cbtm = 0x7),
    ALPHA_TO_MASK_CASE(.id = "G07", .what = "CB_TARGET_MASK MRT0 = 0 (no colour writes)",
                       .cbtm = ALPHA_TO_MASK_ZERO_MASK),
    ALPHA_TO_MASK_CASE(.id = "G08", .what = "COL0 32_AR", .colfmt = 3),
    ALPHA_TO_MASK_CASE(.id = "G09", .what = "COL0 32_GR (no alpha exported)", .colfmt = 2),
    ALPHA_TO_MASK_CASE(.id = "G10", .what = "COL0 32_R (no alpha exported)", .colfmt = 1),
    ALPHA_TO_MASK_CASE(.id = "G11", .what = "COL0 ZERO (MRT0 export dropped), colour target bound",
                       .colfmt = ALPHA_TO_MASK_ZERO_COL, .cbsm = ALPHA_TO_MASK_ZERO_MASK),
    ALPHA_TO_MASK_CASE(.id = "G12", .what = "no colour target, COL0 32_ABGR", .cbf = CBF_NONE),
    ALPHA_TO_MASK_CASE(.id = "G13", .what = "no colour target, COL0 ZERO", .cbf = CBF_NONE,
                       .colfmt = ALPHA_TO_MASK_ZERO_COL, .cbsm = ALPHA_TO_MASK_ZERO_MASK),
    ALPHA_TO_MASK_CASE(.id = "G14", .what = "no depth target", .nodepth = 1),
    ALPHA_TO_MASK_CASE(.id = "G15", .what = "colour 8_8_8_8 UNORM", .cbf = CBF_U8, .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "G16", .what = "colour 32 FLOAT (no alpha channel)", .cbf = CBF_R32F),
    ALPHA_TO_MASK_CASE(.id = "G17",
                       .what = "colour 16_16_16_16 UINT (integer MRT0, test colour bits 1)",
                       .cbf = CBF_U16I, .colour = 1u),
    ALPHA_TO_MASK_CASE(.id = "G18", .what = "export FP16_ABGR (pkrtz)", .ps = V_F16, .colfmt = 4,
                       .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "G19", .what = "export UNORM16_ABGR (pknorm_u16)", .ps = V_U16,
                       .colfmt = 5, .refine = 1, .hyp = H_U16ONE),
    ALPHA_TO_MASK_CASE(.id = "G27",
                       .what = "MRT0 exported without its alpha (exp en 0x7), COL0 32_ABGR",
                       .ps = V_NOA, .hyp = H_NOALPHA),
    ALPHA_TO_MASK_CASE(.id = "G28", .what = "as G27, 8x D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_NOA, .hyp = H_NOALPHA),
    ALPHA_TO_MASK_CASE(.id = "G26", .what = "export UNORM16_ABGR (pknorm_u16), 4x offsets 0123 r0",
                       .ps = V_U16, .colfmt = 5, .db_alpha_to_mask = ALPHA_TO_MASK_O0123,
                       .hyp = H_U16ONE),
    ALPHA_TO_MASK_CASE(.id = "G20", .what = "CB_SHADER_MASK MRT0 = 0x8 (alpha only)", .cbsm = 0x8),
    ALPHA_TO_MASK_CASE(.id = "G21", .what = "CB_SHADER_MASK MRT0 = 0x9 (red + alpha)", .cbsm = 0x9),
    ALPHA_TO_MASK_CASE(.id = "G22", .what = "no colour target, COL0 32_AR", .cbf = CBF_NONE,
                       .colfmt = 3),
    ALPHA_TO_MASK_CASE(.id = "G23", .what = "CB_SHADER_MASK MRT0 = 0x1 (red only)", .cbsm = 0x1),
    ALPHA_TO_MASK_CASE(.id = "G24", .what = "blend SRC_ALPHA / ONE_MINUS_SRC_ALPHA on", .blend = 1),
    ALPHA_TO_MASK_CASE(.id = "G25", .what = "COL0 32_AR, CB_SHADER_MASK 0x9", .colfmt = 3,
                       .cbsm = 0x9),
    /* sample counts, D0 / D1 */
    ALPHA_TO_MASK_CASE(.id = "S01", .what = "1x, MSAA_ENABLE 0, D0", .ns = 1, .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "S02", .what = "1x, MSAA_ENABLE 1, D0", .ns = 1, .msaa_en = 1,
                       .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "S03", .what = "2x, D0", .ns = 2, .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "S05", .what = "8x, D0", .ns = 8, .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "S06", .what = "1x, D1", .ns = 1, .db_alpha_to_mask = ALPHA_TO_MASK_D1,
                       .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "S07", .what = "2x, D1", .ns = 2, .db_alpha_to_mask = ALPHA_TO_MASK_D1,
                       .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "S08", .what = "4x, D1", .db_alpha_to_mask = ALPHA_TO_MASK_D1,
                       .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "S09", .what = "8x, D1", .ns = 8, .db_alpha_to_mask = ALPHA_TO_MASK_D1,
                       .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "S10", .what = "4x, MSAA_ENABLE 0, D0", .msaa_en = 0, .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "S11", .what = "8x, MSAA_ENABLE 0, D1", .ns = 8, .msaa_en = 0,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1),
    ALPHA_TO_MASK_CASE(.id = "S12", .what = "1x, MSAA_ENABLE 1, D1", .ns = 1, .msaa_en = 1,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .refine = 1),
    /* offsets */
    ALPHA_TO_MASK_CASE(.id = "O4a", .what = "4x, offsets 0000 round 0",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 0, 0, 0, 0), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O4b", .what = "4x, offsets 0000 round 1",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 0, 0, 0, 1), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O4c", .what = "4x, offsets 1111 round 0",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 1, 1, 1, 1, 0), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O4d", .what = "4x, offsets 1111 round 1",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 1, 1, 1, 1, 1), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O4e", .what = "4x, offsets 2222 round 1",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 2, 2, 2, 2, 1), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O4f", .what = "4x, offsets 3333 round 0",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 3, 3, 3, 3, 0), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O4g", .what = "4x, offsets 3333 round 1",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 3, 3, 3, 3, 1), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O4h", .what = "4x, offsets 0123 round 0",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 1, 2, 3, 0), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O4i", .what = "4x, offsets 0123 round 1",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 1, 2, 3, 1), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O4j", .what = "4x, offsets 3102 round 0",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 3, 1, 0, 2, 0), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O8a", .what = "8x, offsets 0000 round 0", .ns = 8,
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 0, 0, 0, 0), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O8b", .what = "8x, offsets 0000 round 1", .ns = 8,
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 0, 0, 0, 1), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O8c", .what = "8x, offsets 1111 round 0", .ns = 8,
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 1, 1, 1, 1, 0), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O8d", .what = "8x, offsets 1111 round 1", .ns = 8,
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 1, 1, 1, 1, 1), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O8e", .what = "8x, offsets 2222 round 1", .ns = 8,
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 2, 2, 2, 2, 1), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O8f", .what = "8x, offsets 3333 round 0", .ns = 8,
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 3, 3, 3, 3, 0), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O8g", .what = "8x, offsets 3333 round 1", .ns = 8,
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 3, 3, 3, 3, 1), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O8h", .what = "8x, offsets 0123 round 0", .ns = 8,
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 1, 2, 3, 0), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O2h", .what = "2x, offsets 0123 round 0", .ns = 2,
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 1, 2, 3, 0), .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "O1h", .what = "1x, offsets 0123 round 0", .ns = 1,
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 1, 2, 3, 0), .refine = 1),
    /* EQAA */
    ALPHA_TO_MASK_CASE(.id = "E4a", .what = "4x, ALPHA_TO_MASK_NUM_SAMPLES 0 (1), D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 0,
                       .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "E4b", .what = "4x, ALPHA_TO_MASK_NUM_SAMPLES 1 (2), D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 1,
                       .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "E4c", .what = "4x, ALPHA_TO_MASK_NUM_SAMPLES 3 (8), D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 3,
                       .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "E4d", .what = "4x, ALPHA_TO_MASK_EQAA_DISABLE 1, D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .eqaa_dis = 1, .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "E4e", .what = "4x, EQAA_DISABLE 1, NUM_SAMPLES 1 (2), D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .eqaa_dis = 1,
                       .alpha_to_mask_num_samples = 1, .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "E8a", .what = "8x, ALPHA_TO_MASK_NUM_SAMPLES 0 (1), D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 0,
                       .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "E8b", .what = "8x, ALPHA_TO_MASK_NUM_SAMPLES 1 (2), D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 1,
                       .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "E8c", .what = "8x, ALPHA_TO_MASK_NUM_SAMPLES 2 (4), D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 2,
                       .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "E8d", .what = "8x, ALPHA_TO_MASK_EQAA_DISABLE 1, D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .eqaa_dis = 1, .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "E8e", .what = "8x, EQAA_DISABLE 1, NUM_SAMPLES 2 (4), D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .eqaa_dis = 1,
                       .alpha_to_mask_num_samples = 2, .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "P4", .what = "4x, PS_ITER_SAMPLES 2 (per-sample shading), D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps_iter = 2),
    ALPHA_TO_MASK_CASE(.id = "P8", .what = "8x, PS_ITER_SAMPLES 3 (per-sample shading), D1",
                       .ns = 8, .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps_iter = 3),
    /* the alpha source: MRT0.a or MRTZ.a */
    ALPHA_TO_MASK_CASE(.id = "M01", .what = "COVERAGE_TO_MASK 1; MRTZ.a = A, MRT0.a = 1.0",
                       .ps = V_ZA, .dbsc = ALPHA_TO_MASK_COV2MASK, .refine = 1),
    ALPHA_TO_MASK_CASE(.id = "M02", .what = "COVERAGE_TO_MASK 1; MRTZ.a = 1.0, MRT0.a = A",
                       .ps = V_AZ, .dbsc = ALPHA_TO_MASK_COV2MASK),
    ALPHA_TO_MASK_CASE(.id = "M03", .what = "COVERAGE_TO_MASK 0; MRTZ.a = A, MRT0.a = 1.0",
                       .ps = V_ZA),
    ALPHA_TO_MASK_CASE(.id = "M04", .what = "COVERAGE_TO_MASK 0; MRTZ.a = 1.0, MRT0.a = A",
                       .ps = V_AZ),
    ALPHA_TO_MASK_CASE(.id = "M05",
                       .what = "COVERAGE_TO_MASK 1; MRTZ.a = A only, no colour target, COL0 ZERO",
                       .ps = V_ZA0, .dbsc = ALPHA_TO_MASK_COV2MASK, .cbf = CBF_NONE,
                       .colfmt = ALPHA_TO_MASK_ZERO_COL, .cbsm = ALPHA_TO_MASK_ZERO_MASK),
    ALPHA_TO_MASK_CASE(.id = "M06",
                       .what = "COVERAGE_TO_MASK 1, ENABLE 0; MRTZ.a = A, MRT0.a = 1.0", .ps = V_ZA,
                       .dbsc = ALPHA_TO_MASK_COV2MASK, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0),
    ALPHA_TO_MASK_CASE(.id = "M07", .what = "COVERAGE_TO_MASK 1; MRTZ.a = A, MRT0.a = 0.0",
                       .ps = V_ZA, .dbsc = ALPHA_TO_MASK_COV2MASK, .mrt0a = 0),
    ALPHA_TO_MASK_CASE(.id = "M08",
                       .what = "COVERAGE_TO_MASK 1, ALPHA_TO_MASK_DISABLE 1; MRTZ.a = A",
                       .ps = V_ZA, .dbsc = ALPHA_TO_MASK_COV2MASK | ALPHA_TO_MASK_DISABLE_BIT),
    ALPHA_TO_MASK_CASE(.id = "M09",
                       .what = "COVERAGE_TO_MASK 0; MRTZ.a = A only, no colour target, COL0 ZERO",
                       .ps = V_ZA0, .cbf = CBF_NONE, .colfmt = ALPHA_TO_MASK_ZERO_COL,
                       .cbsm = ALPHA_TO_MASK_ZERO_MASK),
    ALPHA_TO_MASK_CASE(.id = "M10", .what = "COVERAGE_TO_MASK 1, 8x D1; MRTZ.a = A, MRT0.a = 1.0",
                       .ns = 8, .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_ZA,
                       .dbsc = ALPHA_TO_MASK_COV2MASK, .refine = 1),
    /* both alphas vary (A, B random per pixel) */
    ALPHA_TO_MASK_CASE(.id = "C01", .what = "COVERAGE_TO_MASK 1, 4x D1; MRT0.a = A, MRTZ.a = B",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_ZAB,
                       .dbsc = ALPHA_TO_MASK_COV2MASK, .kind = AK_RANDOM, .bmode = BM_ALPHA,
                       .hyp = H_COMBINE, .seed = 0xC01u),
    ALPHA_TO_MASK_CASE(.id = "C02", .what = "COVERAGE_TO_MASK 1, 8x D1; MRT0.a = A, MRTZ.a = B",
                       .ns = 8, .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_ZAB,
                       .dbsc = ALPHA_TO_MASK_COV2MASK, .kind = AK_RANDOM, .bmode = BM_ALPHA,
                       .hyp = H_COMBINE, .seed = 0xC02u),
    ALPHA_TO_MASK_CASE(.id = "C03",
                       .what = "COVERAGE_TO_MASK 1, ENABLE 0, 4x D1; MRT0.a = A, MRTZ.a = B",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1 & ~1u, .ps = V_ZAB,
                       .dbsc = ALPHA_TO_MASK_COV2MASK, .kind = AK_RANDOM, .bmode = BM_ALPHA,
                       .hyp = H_EN0, .seed = 0xC03u),
    ALPHA_TO_MASK_CASE(.id = "C04",
                       .what = "COVERAGE_TO_MASK 1, ALPHA_TO_MASK_DISABLE 1, 4x D1; A, B",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_ZAB,
                       .dbsc = ALPHA_TO_MASK_COV2MASK | ALPHA_TO_MASK_DISABLE_BIT,
                       .kind = AK_RANDOM, .bmode = BM_ALPHA, .hyp = H_EN0, .seed = 0xC04u),
    ALPHA_TO_MASK_CASE(.id = "C05",
                       .what = "COVERAGE_TO_MASK 1, COL0 32_GR (no MRT0 alpha), 4x D1; A, B",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_ZAB,
                       .dbsc = ALPHA_TO_MASK_COV2MASK, .colfmt = 2, .kind = AK_RANDOM,
                       .bmode = BM_ALPHA, .seed = 0xC05u),
    ALPHA_TO_MASK_CASE(.id = "C06",
                       .what = "COVERAGE_TO_MASK 1, ENABLE 0, 8x D0; MRT0.a = A, MRTZ.a = B",
                       .ns = 8, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .ps = V_ZAB,
                       .dbsc = ALPHA_TO_MASK_COV2MASK, .kind = AK_RANDOM, .bmode = BM_ALPHA,
                       .hyp = H_EN0, .seed = 0xC06u),
    /* MRT1 */
    ALPHA_TO_MASK_CASE(.id = "T01", .what = "MRT0.a = 1.0, MRT1.a = A (COL0 / COL1 32_ABGR)",
                       .ps = V_M1, .colfmt = 0x99, .cbsm = 0xFF, .cbtm = 0xF, .hyp = H_MRT1),
    ALPHA_TO_MASK_CASE(.id = "T02", .what = "MRT1.a = A only (COL0 ZERO, COL1 32_ABGR)",
                       .ps = V_M1O, .colfmt = 0x90, .cbsm = 0xF0, .hyp = H_MRT1),
    ALPHA_TO_MASK_CASE(.id = "T03", .what = "MRT0.a = 0.0, MRT1.a = A", .ps = V_M1, .colfmt = 0x99,
                       .cbsm = 0xFF, .mrt0a = 0, .hyp = H_MRT1),
    /* per-sample shading with a per-sample alpha */
    ALPHA_TO_MASK_CASE(.id = "I01", .what = "4x D1, PS_ITER_SAMPLES 2, alpha = A + 0.125 sample id",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_ITER, .ps_iter = 2,
                       .iter_delta = 0x3E000000u, .hyp = H_ITER),
    ALPHA_TO_MASK_CASE(.id = "I02",
                       .what = "8x D1, PS_ITER_SAMPLES 3, alpha = A + 0.0625 sample id", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_ITER, .ps_iter = 3,
                       .iter_delta = 0x3D800000u, .hyp = H_ITER),
    ALPHA_TO_MASK_CASE(.id = "I03", .what = "2x D0, PS_ITER_SAMPLES 1, alpha = A + 0.25 sample id",
                       .ns = 2, .ps = V_ITER, .ps_iter = 1, .iter_delta = 0x3E800000u,
                       .hyp = H_ITER),
    /* the dither position with a window offset (uniform alpha 0.37890625: 4x 0123 round 0 gives
       0x4, 0x2, 0x6, 0x5 at p0..p3) */
    ALPHA_TO_MASK_CASE(.id = "W00",
                       .what = "4x offsets 0123 r0, alpha 0.37890625, no window offset",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 1, 2, 3, 0),
                       .kind = AK_UNIFORM, .uni_alpha = 0x3EC20000u, .hyp = H_WIN),
    ALPHA_TO_MASK_CASE(.id = "W01", .what = "as W00, PA_SC_WINDOW_OFFSET (1, 0)",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 1, 2, 3, 0),
                       .kind = AK_UNIFORM, .uni_alpha = 0x3EC20000u, .wofs = 1, .hyp = H_WIN),
    ALPHA_TO_MASK_CASE(.id = "W02", .what = "as W00, PA_SC_WINDOW_OFFSET (0, 1)",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 1, 2, 3, 0),
                       .kind = AK_UNIFORM, .uni_alpha = 0x3EC20000u, .wofs = 1u << 16,
                       .hyp = H_WIN),
    ALPHA_TO_MASK_CASE(.id = "W03", .what = "as W00, PA_SC_WINDOW_OFFSET (1, 1)",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 1, 2, 3, 0),
                       .kind = AK_UNIFORM, .uni_alpha = 0x3EC20000u, .wofs = 1u | (1u << 16),
                       .hyp = H_WIN),
    ALPHA_TO_MASK_CASE(.id = "W04", .what = "as W00, PA_SC_WINDOW_OFFSET (3, 6)",
                       .db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, 0, 1, 2, 3, 0),
                       .kind = AK_UNIFORM, .uni_alpha = 0x3EC20000u, .wofs = 3u | (6u << 16),
                       .hyp = H_WIN),
    /* sample mask export and kill with random per-pixel values */
    ALPHA_TO_MASK_CASE(.id = "K01", .what = "MASK_EXPORT B (random per pixel) + alpha-to-mask",
                       .ps = V_MASK, .bmode = BM_MASK, .seed = 0x01u),
    ALPHA_TO_MASK_CASE(.id = "K02", .what = "MASK_EXPORT B, ENABLE 0 (reference)", .ps = V_MASK,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_MASK, .seed = 0x02u),
    ALPHA_TO_MASK_CASE(.id = "K03", .what = "KILL where B & 1 (random) + alpha-to-mask",
                       .ps = V_KILL, .bmode = BM_KILL, .seed = 0x03u),
    ALPHA_TO_MASK_CASE(.id = "K04", .what = "Z_ORDER EARLY_Z_THEN_LATE_Z",
                       .dbsc = ALPHA_TO_MASK_ZORDER(1)),
    ALPHA_TO_MASK_CASE(.id = "K05", .what = "Z_ORDER RE_Z", .dbsc = ALPHA_TO_MASK_ZORDER(2)),
    ALPHA_TO_MASK_CASE(.id = "K06", .what = "Z_ORDER EARLY_Z_THEN_RE_Z",
                       .dbsc = ALPHA_TO_MASK_ZORDER(3)),
    ALPHA_TO_MASK_CASE(.id = "K07", .what = "PA_SC_AA_MASK 0x5 per pixel + alpha-to-mask",
                       .aa_mask = 0x5),
    ALPHA_TO_MASK_CASE(.id = "K08", .what = "Z_ORDER EARLY_Z_THEN_LATE_Z, 8x D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .dbsc = ALPHA_TO_MASK_ZORDER(1)),
    ALPHA_TO_MASK_CASE(.id = "K09", .what = "MASK_EXPORT B + alpha-to-mask, 8x D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_MASK, .bmode = BM_MASK,
                       .seed = 0x09u),
    /* one alpha for every quad */
    ALPHA_TO_MASK_CASE(.id = "U01", .what = "uniform alpha 0.3, 4x D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .kind = AK_UNIFORM,
                       .uni_alpha = 0x3E99999Au),
    ALPHA_TO_MASK_CASE(.id = "U02", .what = "uniform alpha 0.6, 4x D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .kind = AK_UNIFORM,
                       .uni_alpha = 0x3F19999Au),
    ALPHA_TO_MASK_CASE(.id = "U03", .what = "uniform alpha 0.45, 8x D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .kind = AK_UNIFORM,
                       .uni_alpha = 0x3EE66666u),
    ALPHA_TO_MASK_CASE(.id = "U04", .what = "uniform alpha 0.55, 8x D0", .ns = 8,
                       .kind = AK_UNIFORM, .uni_alpha = 0x3F0CCCCDu),
    ALPHA_TO_MASK_CASE(.id = "U05", .what = "uniform alpha 0.4, 2x D1", .ns = 2,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .kind = AK_UNIFORM,
                       .uni_alpha = 0x3ECCCCCDu),
    ALPHA_TO_MASK_CASE(.id = "U06", .what = "uniform alpha 0.5, 1x D1", .ns = 1,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .kind = AK_UNIFORM,
                       .uni_alpha = 0x3F000000u),
    /* every 16-bit export value (raw, 64 passes): 4x offsets 0123 round 0 (offsets 0, 2, 4, 6 -
       offset 0 tells 65535 = 1.0 apart), and D1 (1, 3, 5, 7) */
    SWEEP16_CASE("X16f", "every FP16_ABGR alpha (raw half), 4x 0123 r0", 4, 0x3C00u, H_NONE,
                 ALPHA_TO_MASK_O0123),
    SWEEP16_CASE("X16g", "every FP16_ABGR alpha (raw half), 4x D1", 4, 0x3C00u, H_NONE,
                 ALPHA_TO_MASK_D1),
    SWEEP16_CASE("X16u", "every UNORM16_ABGR alpha (raw), 4x 0123 r0", 5, 0xFFFFu, H_U16ONE,
                 ALPHA_TO_MASK_O0123),
    SWEEP16_CASE("X16v", "every UNORM16_ABGR alpha (raw), 4x D1", 5, 0xFFFFu, H_U16ONE,
                 ALPHA_TO_MASK_D1),
    SWEEP16_CASE("X16s", "every SNORM16_ABGR alpha (raw), 4x 0123 r0", 6, 0x7FFFu, H_SNORM,
                 ALPHA_TO_MASK_O0123),
    SWEEP16_CASE("X16t", "every SNORM16_ABGR alpha (raw), 4x D1", 6, 0x7FFFu, H_SNORM,
                 ALPHA_TO_MASK_D1),
    SWEEP16_CASE("X16i", "every UINT16_ABGR alpha (raw), 4x 0123 r0", 7, 1u, H_NOMODEL,
                 ALPHA_TO_MASK_O0123),
    SWEEP16_CASE("X16j", "every SINT16_ABGR alpha (raw), 4x 0123 r0", 8, 1u, H_NOMODEL,
                 ALPHA_TO_MASK_O0123),
    /* the textured net (t_tex): the model takes the alpha the GPU sampled (also visual) */
    ALPHA_TO_MASK_CASE(.id = "N01", .what = "textured net, 4x D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_TEX, .kind = AK_TEX, .vis = 1),
    ALPHA_TO_MASK_CASE(.id = "N02",
                       .what =
                           "textured net, 4x D1, ALPHA_TO_MASK_NUM_SAMPLES 0 (Gnm default DB_EQAA)",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 0,
                       .ps = V_TEX, .kind = AK_TEX, .vis = 2),
    ALPHA_TO_MASK_CASE(.id = "N03",
                       .what = "textured net, 4x D0 (Gnm AlphaToMaskControl::init offsets)",
                       .ps = V_TEX, .kind = AK_TEX, .vis = 3),
    ALPHA_TO_MASK_CASE(.id = "N04", .what = "textured net, 8x D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_TEX, .kind = AK_TEX, .vis = 4),
    ALPHA_TO_MASK_CASE(.id = "N05",
                       .what =
                           "textured net, 8x D1, ALPHA_TO_MASK_NUM_SAMPLES 0 (Gnm default DB_EQAA)",
                       .ns = 8, .db_alpha_to_mask = ALPHA_TO_MASK_D1,
                       .alpha_to_mask_num_samples = 0, .ps = V_TEX, .kind = AK_TEX, .vis = 5),
    ALPHA_TO_MASK_CASE(.id = "N06", .what = "textured net, 1x D1", .ns = 1,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_TEX, .kind = AK_TEX, .vis = 6),
    /* the visual pages' captures (also logged here) */
    ALPHA_TO_MASK_CASE(.id = "V01", .what = "ramp, 4x D1", .db_alpha_to_mask = ALPHA_TO_MASK_D1,
                       .kind = AK_RAMP, .vis = 7),
    ALPHA_TO_MASK_CASE(.id = "V02", .what = "ramp, 4x D0", .kind = AK_RAMP, .vis = 8),
    ALPHA_TO_MASK_CASE(.id = "V03", .what = "ramp, 8x D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .kind = AK_RAMP, .vis = 9),
    ALPHA_TO_MASK_CASE(.id = "V04", .what = "ramp, 2x D1", .ns = 2,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .kind = AK_RAMP, .vis = 10),
    ALPHA_TO_MASK_CASE(.id = "V05", .what = "ramp, 1x D1", .ns = 1,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .kind = AK_RAMP, .vis = 11),
    ALPHA_TO_MASK_CASE(.id = "V06", .what = "ramp, 4x D1, EQAA_DISABLE",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .eqaa_dis = 1, .kind = AK_RAMP,
                       .vis = 12),
    ALPHA_TO_MASK_CASE(.id = "V07",
                       .what = "ramp, 4x D1, COVERAGE_TO_MASK (MRTZ.a only, no colour)",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_ZA0,
                       .dbsc = ALPHA_TO_MASK_COV2MASK, .cbf = CBF_NONE,
                       .colfmt = ALPHA_TO_MASK_ZERO_COL, .cbsm = ALPHA_TO_MASK_ZERO_MASK,
                       .kind = AK_RAMP, .vis = 13),
    ALPHA_TO_MASK_CASE(.id = "V08", .what = "ramp, 8x D1, ALPHA_TO_MASK_NUM_SAMPLES 2 (4)", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 2,
                       .kind = AK_RAMP, .vis = 14),
    ALPHA_TO_MASK_CASE(.id = "V09", .what = "net, 4x D1", .db_alpha_to_mask = ALPHA_TO_MASK_D1,
                       .kind = AK_NET, .vis = 15),
    ALPHA_TO_MASK_CASE(.id = "V10", .what = "net, 4x D0", .kind = AK_NET, .vis = 16),
    ALPHA_TO_MASK_CASE(.id = "V11", .what = "net, 8x D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .kind = AK_NET, .vis = 17),
    ALPHA_TO_MASK_CASE(.id = "V12", .what = "net, 1x D1", .ns = 1,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .kind = AK_NET, .vis = 18),
    /* random configurations and alphas (RND, generated from the seed), then 16 coverage samples
       without targets (counted by ZPASS_DONE only; may hang) */
    /* MRT0 export enable masks (uncompressed and FP16 compressed) */
    ALPHA_TO_MASK_CASE(.id = "EN01", .what = "enable mask 0x8 (alpha only), COL0 32_ABGR",
                       .ps = V_EN8, .kind = AK_RANDOM, .seed = 0xE01u),
    ALPHA_TO_MASK_CASE(.id = "EN02", .what = "enable mask 0x0, COL0 32_ABGR", .ps = V_EN0,
                       .kind = AK_RANDOM, .hyp = H_NOALPHA, .seed = 0xE02u),
    ALPHA_TO_MASK_CASE(.id = "EN03", .what = "enable mask 0x9 (red, alpha), COL0 32_AR",
                       .ps = V_EN9, .colfmt = 3, .kind = AK_RANDOM, .seed = 0xE03u),
    ALPHA_TO_MASK_CASE(.id = "EN04", .what = "enable mask 0x1 (red only), COL0 32_AR", .ps = V_EN1,
                       .colfmt = 3, .kind = AK_RANDOM, .hyp = H_NOALPHA, .seed = 0xE04u),
    ALPHA_TO_MASK_CASE(.id = "EN05", .what = "enable mask 0x1 (red only), COL0 32_ABGR",
                       .ps = V_EN1, .kind = AK_RANDOM, .hyp = H_NOALPHA, .seed = 0xE05u),
    ALPHA_TO_MASK_CASE(.id = "EN06", .what = "FP16 compressed, enable mask 0x3 (red, green)",
                       .ps = V_C3, .colfmt = 4, .kind = AK_RANDOM, .hyp = H_NOALPHA,
                       .seed = 0xE06u),
    ALPHA_TO_MASK_CASE(.id = "EN07", .what = "FP16 compressed, enable mask 0xC (blue, alpha)",
                       .ps = V_CC, .colfmt = 4, .kind = AK_RANDOM, .seed = 0xE07u),
    ALPHA_TO_MASK_CASE(.id = "EN08", .what = "FP16 compressed, enable mask 0x4 (blue only)",
                       .ps = V_C4, .colfmt = 4, .kind = AK_RANDOM, .hyp = H_COMPR, .seed = 0xE08u),
    ALPHA_TO_MASK_CASE(.id = "EN09", .what = "FP16 compressed, enable mask 0x8 (alpha only)",
                       .ps = V_C8, .colfmt = 4, .kind = AK_RANDOM, .seed = 0xE09u, .hyp = H_COMPR),
    ALPHA_TO_MASK_CASE(.id = "EN10", .what = "enable mask 0x0, 8x D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_EN0, .kind = AK_RANDOM,
                       .hyp = H_NOALPHA, .seed = 0xE10u),
    ALPHA_TO_MASK_CASE(.id = "EN11", .what = "FP16 compressed, enable mask 0x3, 8x D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_C3, .colfmt = 4,
                       .kind = AK_RANDOM, .hyp = H_NOALPHA, .seed = 0xE11u),
    /* ALPHA_TO_MASK_NUM_SAMPLES above the sample count */
    ALPHA_TO_MASK_CASE(.id = "EQ01", .what = "2x, NUM_SAMPLES 2 (4), EQAA_DISABLE, D1", .ns = 2,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 2,
                       .eqaa_dis = 1, .kind = AK_RANDOM, .hyp = H_CLAMP, .seed = 0xE001u),
    ALPHA_TO_MASK_CASE(.id = "EQ02", .what = "4x, NUM_SAMPLES 3 (8), EQAA_DISABLE, D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 3,
                       .eqaa_dis = 1, .kind = AK_RANDOM, .hyp = H_CLAMP, .seed = 0xE002u),
    ALPHA_TO_MASK_CASE(.id = "EQ03", .what = "8x, NUM_SAMPLES 4 (16), EQAA_DISABLE, D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 4,
                       .eqaa_dis = 1, .kind = AK_RANDOM, .hyp = H_CLAMP, .seed = 0xE003u),
    ALPHA_TO_MASK_CASE(.id = "EQ04", .what = "1x, NUM_SAMPLES 1 (2), EQAA_DISABLE, D1", .ns = 1,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 1,
                       .eqaa_dis = 1, .kind = AK_RANDOM, .hyp = H_CLAMP, .seed = 0xE004u),
    ALPHA_TO_MASK_CASE(.id = "EQ05", .what = "2x, NUM_SAMPLES 2 (4), D1", .ns = 2,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 2,
                       .kind = AK_RANDOM, .hyp = H_CLAMP, .seed = 0xE005u),
    ALPHA_TO_MASK_CASE(.id = "EQ06", .what = "8x, NUM_SAMPLES 4 (16), D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 4,
                       .kind = AK_RANDOM, .hyp = H_CLAMP, .seed = 0xE006u),
    ALPHA_TO_MASK_CASE(.id = "EQ07", .what = "4x, NUM_SAMPLES 3 (8), EQAA_DISABLE, D0, refined",
                       .alpha_to_mask_num_samples = 3, .eqaa_dis = 1, .refine = 1, .hyp = H_CLAMP),
    /* NUM_SAMPLES 5..7 (Gnm's setAlphaToMaskSamples only reaches 0..4) */
    ALPHA_TO_MASK_CASE(.id = "EQ08", .what = "4x, NUM_SAMPLES 5, D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 5,
                       .show = 1, .kind = AK_RANDOM, .seed = 0xE008u),
    ALPHA_TO_MASK_CASE(.id = "EQ09", .what = "4x, NUM_SAMPLES 7, D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 7,
                       .show = 1, .kind = AK_RANDOM, .seed = 0xE009u),
    ALPHA_TO_MASK_CASE(.id = "EQ10", .what = "4x, NUM_SAMPLES 5, EQAA_DISABLE, D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 5,
                       .eqaa_dis = 1, .kind = AK_RANDOM, .hyp = H_CAP, .seed = 0xE010u),
    ALPHA_TO_MASK_CASE(.id = "EQ11", .what = "8x, NUM_SAMPLES 7, EQAA_DISABLE, D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 7,
                       .eqaa_dis = 1, .kind = AK_RANDOM, .hyp = H_CAP, .seed = 0xE011u),
    ALPHA_TO_MASK_CASE(.id = "EQ12", .what = "4x, NUM_SAMPLES 6, D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 6,
                       .kind = AK_RANDOM, .seed = 0xE012u),
    ALPHA_TO_MASK_CASE(.id = "EQ13", .what = "4x, NUM_SAMPLES 6, EQAA_DISABLE, D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 6,
                       .eqaa_dis = 1, .kind = AK_RANDOM, .hyp = H_CAP, .seed = 0xE013u),
    ALPHA_TO_MASK_CASE(.id = "EQ14", .what = "8x, NUM_SAMPLES 6, EQAA_DISABLE, D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 6,
                       .eqaa_dis = 1, .kind = AK_RANDOM, .hyp = H_CAP, .seed = 0xE014u),
    ALPHA_TO_MASK_CASE(.id = "EQ15", .what = "1x, NUM_SAMPLES 6, EQAA_DISABLE, D0", .ns = 1,
                       .alpha_to_mask_num_samples = 6, .eqaa_dis = 1, .kind = AK_RANDOM,
                       .hyp = H_CAP, .seed = 0xE015u),
    ALPHA_TO_MASK_CASE(.id = "EQ16", .what = "2x, NUM_SAMPLES 6, EQAA_DISABLE, D0", .ns = 2,
                       .alpha_to_mask_num_samples = 6, .eqaa_dis = 1, .kind = AK_RANDOM,
                       .hyp = H_CAP, .seed = 0xE016u),
    /* kill: valid-mask patterns, with alpha-to-mask off and on */
    ALPHA_TO_MASK_CASE(.id = "KX01", .what = "v_cmpx kill, exp vm, alpha-to-mask off", .ps = V_KILL,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_KILL, .kind = AK_RANDOM,
                       .seed = 0x1001u),
    ALPHA_TO_MASK_CASE(.id = "KX02",
                       .what = "v_cmpx kill, branch to a null export, alpha-to-mask off",
                       .ps = V_KILL2, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_KILL,
                       .kind = AK_RANDOM, .seed = 0x1002u),
    ALPHA_TO_MASK_CASE(.id = "KX03", .what = "v_cmp, s_and_b64 exec, exp vm, alpha-to-mask off",
                       .ps = V_KILL3, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_KILL,
                       .kind = AK_RANDOM, .seed = 0x1003u),
    ALPHA_TO_MASK_CASE(.id = "KX04", .what = "v_cmpx kill, export without vm, alpha-to-mask off",
                       .ps = V_KILL_NOVM, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_KILL,
                       .kind = AK_RANDOM, .hyp = H_VM, .seed = 0x1004u),
    ALPHA_TO_MASK_CASE(.id = "KX05",
                       .what = "v_cmpx kill, exp vm, KILL_ENABLE 0 (colour killed, depth "
                               "partly written on hardware: logged only)",
                       .ps = V_KILL, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .no_kill_enable = 1,
                       .bmode = BM_KILL, .kind = AK_RANDOM, .hyp = H_NOMODEL, .seed = 0x1005u),
    ALPHA_TO_MASK_CASE(.id = "KX06", .what = "branch to a null export + alpha-to-mask, 4x D1",
                       .ps = V_KILL2, .db_alpha_to_mask = ALPHA_TO_MASK_D1, .bmode = BM_KILL,
                       .kind = AK_RANDOM, .seed = 0x1006u),
    ALPHA_TO_MASK_CASE(.id = "KX07", .what = "s_and_b64 exec + alpha-to-mask, 4x D1", .ps = V_KILL3,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .bmode = BM_KILL, .kind = AK_RANDOM,
                       .seed = 0x1007u),
    ALPHA_TO_MASK_CASE(.id = "KX08", .what = "v_cmpx kill + alpha-to-mask, 1x D1", .ns = 1,
                       .ps = V_KILL, .db_alpha_to_mask = ALPHA_TO_MASK_D1, .bmode = BM_KILL,
                       .kind = AK_RANDOM, .seed = 0x1008u),
    /* kill patterns, alpha-to-mask off: which construct decides the valid mask */
    ALPHA_TO_MASK_CASE(.id = "KP01", .what = "v_cmpx_eq_u32_e64 to an SGPR pair, exp vm",
                       .ps = V_KP_E64, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_KILL,
                       .kind = AK_RANDOM, .seed = 0x8001u),
    ALPHA_TO_MASK_CASE(.id = "KP02", .what = "v_cmpx_gt_f32, exp vm", .ps = V_KP_F32,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_KILL, .kind = AK_RANDOM,
                       .seed = 0x8002u),
    ALPHA_TO_MASK_CASE(.id = "KP03", .what = "v_cmpx_gt_u32, exp vm", .ps = V_KP_GT,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_KILL, .kind = AK_RANDOM,
                       .seed = 0x8003u),
    ALPHA_TO_MASK_CASE(.id = "KP04", .what = "v_cmpx, VALU work, exp vm", .ps = V_KP_VALU,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_KILL, .kind = AK_RANDOM,
                       .seed = 0x8004u),
    ALPHA_TO_MASK_CASE(.id = "KP05", .what = "v_cmpx, exec copied through SGPRs, exp vm",
                       .ps = V_KP_SMOV, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_KILL,
                       .kind = AK_RANDOM, .seed = 0x8005u),
    ALPHA_TO_MASK_CASE(.id = "KP06", .what = "v_cmp, s_and_saveexec_b64, exp vm",
                       .ps = V_KP_SAVEEXEC, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0,
                       .bmode = BM_KILL, .kind = AK_RANDOM, .seed = 0x8006u),
    ALPHA_TO_MASK_CASE(.id = "KP07", .what = "v_cmpx, MRT0 without vm, null export with vm",
                       .ps = V_KP_TWOEXP, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_KILL,
                       .kind = AK_RANDOM, .hyp = H_TWOEXP, .seed = 0x8007u),
    ALPHA_TO_MASK_CASE(.id = "KP08", .what = "v_cmpx, s_mov_b64 exec -1, exp vm",
                       .ps = V_KP_RESTORE, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_KILL,
                       .kind = AK_RANDOM, .hyp = H_RESTORE, .seed = 0x8008u),
    ALPHA_TO_MASK_CASE(.id = "KP09", .what = "v_cmpx_eq_u32_e64 + alpha-to-mask, 4x D1",
                       .ps = V_KP_E64, .db_alpha_to_mask = ALPHA_TO_MASK_D1, .bmode = BM_KILL,
                       .kind = AK_RANDOM, .seed = 0x8009u),
    ALPHA_TO_MASK_CASE(.id = "KP10", .what = "v_cmp, s_and_saveexec_b64 + alpha-to-mask, 8x D1",
                       .ns = 8, .ps = V_KP_SAVEEXEC, .db_alpha_to_mask = ALPHA_TO_MASK_D1,
                       .bmode = BM_KILL, .kind = AK_RANDOM, .seed = 0x800Au),
    /* PA_SC_AA_MASK, also different per quad pixel */
    ALPHA_TO_MASK_CASE(.id = "AM01", .what = "AA_MASK 0x5, alpha-to-mask off",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .aa_mask = 0x5, .kind = AK_RANDOM,
                       .seed = 0x2001u),
    ALPHA_TO_MASK_CASE(.id = "AM02",
                       .what = "AA_MASK 0x1 / 0x2 / 0x4 / 0x8 per quad pixel, alpha-to-mask off",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .aa_quad = 0x0008000400020001ull,
                       .kind = AK_RANDOM, .seed = 0x2002u),
    ALPHA_TO_MASK_CASE(.id = "AM03", .what = "AA_MASK 0x3 / 0x6 / 0xC / 0x9 per quad pixel, 4x D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .aa_quad = 0x0009000C00060003ull,
                       .kind = AK_RANDOM, .seed = 0x2003u),
    ALPHA_TO_MASK_CASE(.id = "AM04",
                       .what = "AA_MASK 0x81 / 0x42 / 0x24 / 0x18 per quad pixel, 8x D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .aa_quad = 0x0018002400420081ull,
                       .kind = AK_RANDOM, .seed = 0x2004u),
    /* depth-only draws (no colour written), with alpha-to-mask off and on */
    ALPHA_TO_MASK_CASE(.id = "DO01", .what = "CB_TARGET_MASK 0, alpha-to-mask off",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .cbtm = ALPHA_TO_MASK_ZERO_MASK,
                       .kind = AK_RANDOM, .seed = 0x3001u),
    ALPHA_TO_MASK_CASE(.id = "DO02", .what = "COL0 ZERO, colour target bound, alpha-to-mask off",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .colfmt = ALPHA_TO_MASK_ZERO_COL,
                       .cbsm = ALPHA_TO_MASK_ZERO_MASK, .kind = AK_RANDOM, .seed = 0x3002u),
    ALPHA_TO_MASK_CASE(.id = "DO03", .what = "no colour target, COL0 ZERO, alpha-to-mask off",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .cbf = CBF_NONE,
                       .colfmt = ALPHA_TO_MASK_ZERO_COL, .cbsm = ALPHA_TO_MASK_ZERO_MASK,
                       .kind = AK_RANDOM, .seed = 0x3003u),
    ALPHA_TO_MASK_CASE(.id = "DO04", .what = "no colour target, COL0 32_AR, alpha-to-mask off",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .cbf = CBF_NONE, .colfmt = 3,
                       .kind = AK_RANDOM, .seed = 0x3004u),
    ALPHA_TO_MASK_CASE(.id = "DO05", .what = "no colour target, COL0 32_R, alpha-to-mask off",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .cbf = CBF_NONE, .colfmt = 1,
                       .kind = AK_RANDOM, .seed = 0x3005u),
    ALPHA_TO_MASK_CASE(.id = "DO06", .what = "no colour target, COL0 32_ABGR, alpha-to-mask off",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .cbf = CBF_NONE, .kind = AK_RANDOM,
                       .seed = 0x3006u),
    ALPHA_TO_MASK_CASE(.id = "DO07", .what = "CB_COLOR_CONTROL CB_DISABLE, alpha-to-mask off",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .cb_disable = 1, .kind = AK_RANDOM,
                       .seed = 0x3007u),
    ALPHA_TO_MASK_CASE(.id = "DO08", .what = "CB_COLOR_CONTROL CB_DISABLE, 4x D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .cb_disable = 1, .kind = AK_RANDOM,
                       .seed = 0x3008u),
    ALPHA_TO_MASK_CASE(.id = "DO09", .what = "1x, CB_TARGET_MASK 0, D1", .ns = 1,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .cbtm = ALPHA_TO_MASK_ZERO_MASK,
                       .kind = AK_RANDOM, .seed = 0x3009u),
    ALPHA_TO_MASK_CASE(.id = "DO10", .what = "1x, CB_DISABLE, NUM_SAMPLES 0, D1", .ns = 1,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 0,
                       .cb_disable = 1, .kind = AK_RANDOM, .seed = 0x300Au),
    ALPHA_TO_MASK_CASE(.id = "DO11", .what = "4x, CB_TARGET_MASK 0, NUM_SAMPLES 0, D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .alpha_to_mask_num_samples = 0,
                       .cbtm = ALPHA_TO_MASK_ZERO_MASK, .kind = AK_RANDOM, .seed = 0x300Bu),
    /* CB_SHADER_MASK and colour writes, alpha-to-mask off */
    ALPHA_TO_MASK_CASE(.id = "CS01", .what = "CB_SHADER_MASK 0x8",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .cbsm = 0x8, .kind = AK_RANDOM,
                       .seed = 0x4001u),
    ALPHA_TO_MASK_CASE(.id = "CS02", .what = "CB_SHADER_MASK 0",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .cbsm = ALPHA_TO_MASK_ZERO_MASK,
                       .kind = AK_RANDOM, .seed = 0x4002u),
    ALPHA_TO_MASK_CASE(.id = "CS03", .what = "CB_SHADER_MASK 0x7",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .cbsm = 0x7, .kind = AK_RANDOM,
                       .seed = 0x4003u),
    ALPHA_TO_MASK_CASE(.id = "CS04", .what = "CB_SHADER_MASK 0x1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .cbsm = 0x1, .kind = AK_RANDOM,
                       .seed = 0x4004u),
    /* MRT0 not exported while COL0 and CB_SHADER_MASK have alpha */
    ALPHA_TO_MASK_CASE(.id = "NX01",
                       .what = "MRTZ only (COVERAGE_TO_MASK 0), COL0 32_ABGR, CB_SHADER_MASK 0xF",
                       .ps = V_ZA0, .kind = AK_RANDOM, .hyp = H_NOMRT0, .seed = 0x5001u),
    ALPHA_TO_MASK_CASE(.id = "NX02", .what = "MRT1 only, COL0 / COL1 32_ABGR, CB_SHADER_MASK 0xFF",
                       .ps = V_M1O, .colfmt = 0x99, .cbsm = 0xFF, .kind = AK_RANDOM,
                       .hyp = H_NOMRT0, .seed = 0x5002u),
    /* window offset, alpha-to-mask off */
    ALPHA_TO_MASK_CASE(.id = "WO01", .what = "PA_SC_WINDOW_OFFSET (1, 0), alpha-to-mask off",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .wofs = 1, .kind = AK_RANDOM,
                       .seed = 0x6001u),
    ALPHA_TO_MASK_CASE(.id = "WO02", .what = "PA_SC_WINDOW_OFFSET (0, 1), alpha-to-mask off",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .wofs = 1u << 16, .kind = AK_RANDOM,
                       .seed = 0x6002u),
    /* conversion instructions that produce exported alpha (result stored to B) */
    ALPHA_TO_MASK_CASE(.id = "PK01", .what = "v_cvt_pknorm_u16_f32, random", .ps = V_PKNORM_U16,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_C, .kind = AK_RANDOM,
                       .hyp = H_ROUND, .seed = 0x7001u),
    ALPHA_TO_MASK_CASE(.id = "PK02", .what = "v_cvt_pknorm_u16_f32, every halfway point",
                       .ps = V_PKNORM_U16, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .kind = AK_TIES,
                       .hyp = H_ROUND),
    ALPHA_TO_MASK_CASE(.id = "PK03", .what = "v_cvt_pknorm_i16_f32, random", .ps = V_PKNORM_I16,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_C, .kind = AK_RANDOM,
                       .hyp = H_ROUND, .seed = 0x7003u),
    ALPHA_TO_MASK_CASE(.id = "PK04", .what = "v_cvt_pknorm_i16_f32, every halfway point",
                       .ps = V_PKNORM_I16, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .kind = AK_TIES,
                       .hyp = H_ROUND),
    ALPHA_TO_MASK_CASE(.id = "PK08", .what = "v_cvt_pknorm_u16_f32, every halfway point (rounding)",
                       .ps = V_PKNORM_U16, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .kind = AK_TIES,
                       .hyp = H_ROUND2),
    ALPHA_TO_MASK_CASE(.id = "PK09", .what = "v_cvt_pknorm_i16_f32, every halfway point (rounding)",
                       .ps = V_PKNORM_I16, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .kind = AK_TIES,
                       .hyp = H_ROUND2),
    ALPHA_TO_MASK_CASE(.id = "PK05", .what = "v_cvt_pkrtz_f16_f32, random", .ps = V_PKRTZ,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_C, .kind = AK_RANDOM,
                       .seed = 0x7005u),
    ALPHA_TO_MASK_CASE(.id = "PK06", .what = "v_cvt_pk_u16_u32, random", .ps = V_PK_U16,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_C, .kind = AK_RANDOM,
                       .hyp = H_SAT, .seed = 0x7006u),
    ALPHA_TO_MASK_CASE(.id = "PK07", .what = "v_cvt_pk_i16_i32, random", .ps = V_PK_I16,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_C, .kind = AK_RANDOM,
                       .hyp = H_SAT, .seed = 0x7007u),
    /* dual-source blending, alpha-to-mask off */
    ALPHA_TO_MASK_CASE(.id = "DS2", .what = "dual-source blend, 4x, alpha-to-mask off", .ps = V_M1,
                       .colfmt = 0x99, .cbsm = 0xFF, .blend = ALPHA_TO_MASK_BLEND_DUAL,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .kind = AK_RANDOM, .seed = 0xD52u),
    ALPHA_TO_MASK_CASE(.id = "DS3", .what = "dual-source blend, 1x, alpha-to-mask off", .ns = 1,
                       .ps = V_M1, .colfmt = 0x99, .cbsm = 0xFF, .blend = ALPHA_TO_MASK_BLEND_DUAL,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .kind = AK_RANDOM, .seed = 0xD53u),
    /* PA_SC_AA_MASK edge cases */
    ALPHA_TO_MASK_CASE(.id = "AQ01",
                       .what = "AA_MASK 0xFFF0 (sample bits 0-3 off, upper bits on), 4x",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .aa_mask = 0xFFF0, .kind = AK_RANDOM,
                       .seed = 0x9001u),
    ALPHA_TO_MASK_CASE(.id = "AQ02", .what = "AA_MASK 0x000F (not replicated), 4x",
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .aa_mask = 0x000F, .kind = AK_RANDOM,
                       .seed = 0x9002u),
    ALPHA_TO_MASK_CASE(.id = "AQ03", .what = "AA_MASK 0 / 1 / 1 / 0 per quad pixel, 1x", .ns = 1,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .aa_quad = 0x0000000100010000ull,
                       .kind = AK_RANDOM, .seed = 0x9003u),
    ALPHA_TO_MASK_CASE(.id = "AQ04",
                       .what = "AA_MASK 1 / 0 / 0 / 1 per quad pixel, 1x, MSAA_ENABLE 1", .ns = 1,
                       .msaa_en = 1, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0,
                       .aa_quad = 0x0001000000000001ull, .kind = AK_RANDOM, .seed = 0x9004u),
    ALPHA_TO_MASK_CASE(.id = "AQ06",
                       .what = "pixel shader runs for a pixel with AA_MASK 0? (0 / F / F / F), 4x",
                       .ps = V_MARK, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0,
                       .aa_quad = 0x000F000F000F0000ull, .bmode = BM_C, .kind = AK_RANDOM,
                       .hyp = H_AAPS, .seed = 0x9006u),
    ALPHA_TO_MASK_CASE(.id = "AQ07", .what = "pixel shader runs with AA_MASK 0 everywhere?, 4x",
                       .ps = V_MARK, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0,
                       .aa_mask = ALPHA_TO_MASK_AA_ZERO, .bmode = BM_C, .kind = AK_RANDOM,
                       .hyp = H_AAPS, .seed = 0x9007u),
    ALPHA_TO_MASK_CASE(.id = "AQ08",
                       .what =
                           "sample coverage input with AA_MASK 1 / 3 / 7 / F per quad pixel, 4x",
                       .ps = V_COV, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0,
                       .aa_quad = 0x000F000700030001ull, .bmode = BM_C, .kind = AK_RANDOM,
                       .hyp = H_COVIN, .seed = 0x9008u),
    ALPHA_TO_MASK_CASE(.id = "AQ09", .what = "sample coverage input with AA_MASK 0x5, 4x",
                       .ps = V_COV, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .aa_mask = 0x5,
                       .bmode = BM_C, .kind = AK_RANDOM, .hyp = H_COVIN, .seed = 0x9009u),
    ALPHA_TO_MASK_CASE(.id = "AQ10",
                       .what = "sample coverage input, AA_MASK 0x81 / 0x42 / 0x24 / 0x18, 8x",
                       .ns = 8, .ps = V_COV, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0,
                       .aa_quad = 0x0018002400420081ull, .bmode = BM_C, .kind = AK_RANDOM,
                       .hyp = H_COVIN, .seed = 0x900Au),
    ALPHA_TO_MASK_CASE(.id = "AQ11", .what = "pixel shader runs and coverage input, no AA_MASK, 4x",
                       .ps = V_COV, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .bmode = BM_C,
                       .kind = AK_RANDOM, .seed = 0x900Bu),
    /* DEPTH_BEFORE_SHADER: do the shader's kills reach depth and ZPASS_DONE? */
    ALPHA_TO_MASK_CASE(.id = "DB01", .what = "DEPTH_BEFORE_SHADER + alpha-to-mask, 4x D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1,
                       .dbsc = ALPHA_TO_MASK_DEPTH_BEFORE_SHADER, .kind = AK_RANDOM, .hyp = H_DBS,
                       .seed = 0xDB01u),
    ALPHA_TO_MASK_CASE(.id = "DB02", .what = "DEPTH_BEFORE_SHADER + alpha-to-mask, 8x D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1,
                       .dbsc = ALPHA_TO_MASK_DEPTH_BEFORE_SHADER, .kind = AK_RANDOM, .hyp = H_DBS,
                       .seed = 0xDB02u),
    ALPHA_TO_MASK_CASE(.id = "DB03", .what = "DEPTH_BEFORE_SHADER + kill, alpha-to-mask off",
                       .ps = V_KILL, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0,
                       .dbsc = ALPHA_TO_MASK_DEPTH_BEFORE_SHADER, .bmode = BM_KILL,
                       .kind = AK_RANDOM, .hyp = H_DBS, .seed = 0xDB03u),
    ALPHA_TO_MASK_CASE(.id = "DB04",
                       .what = "DEPTH_BEFORE_SHADER + sample mask export, alpha-to-mask off",
                       .ps = V_MASK, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0,
                       .dbsc = ALPHA_TO_MASK_DEPTH_BEFORE_SHADER, .bmode = BM_MASK,
                       .kind = AK_RANDOM, .hyp = H_DBS, .seed = 0xDB04u),
    ALPHA_TO_MASK_CASE(.id = "DB05", .what = "DEPTH_BEFORE_SHADER + kill + alpha-to-mask, 4x D1",
                       .ps = V_KILL, .db_alpha_to_mask = ALPHA_TO_MASK_D1,
                       .dbsc = ALPHA_TO_MASK_DEPTH_BEFORE_SHADER, .bmode = BM_KILL,
                       .kind = AK_RANDOM, .hyp = H_DBS, .seed = 0xDB05u),
    /* MASK_EXPORT_NUM_SAMPLES (Gnm's DepthEqaaControl default 0) and PS_ITER_SAMPLES below the
       samples */
    ALPHA_TO_MASK_CASE(.id = "ME01", .what = "mask export, MASK_EXPORT_NUM_SAMPLES 0, 4x",
                       .ps = V_MASK, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .mask_export_ns = 0,
                       .bmode = BM_MASK, .kind = AK_RANDOM, .hyp = H_MASK_EXPORT, .seed = 0xE101u),
    ALPHA_TO_MASK_CASE(.id = "ME02", .what = "mask export, MASK_EXPORT_NUM_SAMPLES 1, 4x",
                       .ps = V_MASK, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .mask_export_ns = 1,
                       .bmode = BM_MASK, .kind = AK_RANDOM, .hyp = H_MASK_EXPORT, .seed = 0xE102u),
    ALPHA_TO_MASK_CASE(.id = "ME03", .what = "mask export, MASK_EXPORT_NUM_SAMPLES 0, 8x", .ns = 8,
                       .ps = V_MASK, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .mask_export_ns = 0,
                       .bmode = BM_MASK, .kind = AK_RANDOM, .hyp = H_MASK_EXPORT, .seed = 0xE103u),
    ALPHA_TO_MASK_CASE(.id = "ME04", .what = "mask export, MASK_EXPORT_NUM_SAMPLES 2, 8x", .ns = 8,
                       .ps = V_MASK, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .mask_export_ns = 2,
                       .bmode = BM_MASK, .kind = AK_RANDOM, .hyp = H_MASK_EXPORT, .seed = 0xE104u),
    ALPHA_TO_MASK_CASE(.id = "ME05", .what = "mask export, MASK_EXPORT_NUM_SAMPLES 3 above 4x",
                       .ps = V_MASK, .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .mask_export_ns = 3,
                       .bmode = BM_MASK, .kind = AK_RANDOM, .seed = 0xE105u),
    ALPHA_TO_MASK_CASE(.id = "ME06",
                       .what = "mask export + alpha-to-mask, MASK_EXPORT_NUM_SAMPLES 0, 4x D1",
                       .ps = V_MASK, .db_alpha_to_mask = ALPHA_TO_MASK_D1, .mask_export_ns = 0,
                       .bmode = BM_MASK, .kind = AK_RANDOM, .hyp = H_MASK_EXPORT, .seed = 0xE106u),
    ALPHA_TO_MASK_CASE(.id = "I04",
                       .what = "4x D1, PS_ITER_SAMPLES 1 (2 of 4), alpha = A + 0.125 sample id",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_ITER, .ps_iter = 1,
                       .iter_delta = 0x3E000000u, .show = 1, .hyp = H_NOMODEL),
    ALPHA_TO_MASK_CASE(.id = "I05",
                       .what = "8x D1, PS_ITER_SAMPLES 1 (2 of 8), alpha = A + 0.0625 sample id",
                       .ns = 8, .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_ITER, .ps_iter = 1,
                       .iter_delta = 0x3D800000u, .show = 1, .hyp = H_NOMODEL),
    /* everything at once: MRT0.a, MRTZ.a, sample mask export, kill, PA_SC_AA_MASK */
    ALPHA_TO_MASK_CASE(.id = "ALL1",
                       .what = "MRT0.a A, MRTZ.a C, mask B, kill B bit 8, AA_MASK 0xB, 4x D1",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_ALL,
                       .dbsc = ALPHA_TO_MASK_COV2MASK, .bmode = BM_ALL, .aa_mask = 0xB,
                       .kind = AK_RANDOM, .seed = 0xA11u),
    ALPHA_TO_MASK_CASE(.id = "ALL2",
                       .what = "MRT0.a A, MRTZ.a C, mask B, kill B bit 8, AA_MASK 0xB6, 8x D1",
                       .ns = 8, .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_ALL,
                       .dbsc = ALPHA_TO_MASK_COV2MASK, .bmode = BM_ALL, .aa_mask = 0xB6,
                       .kind = AK_RANDOM, .seed = 0xA12u),
    ALPHA_TO_MASK_CASE(.id = "ALL3", .what = "as ALL1, ENABLE 0 (MRTZ.a path only)",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1 & ~1u, .ps = V_ALL,
                       .dbsc = ALPHA_TO_MASK_COV2MASK, .bmode = BM_ALL, .aa_mask = 0xB,
                       .kind = AK_RANDOM, .seed = 0xA13u),
    ALPHA_TO_MASK_CASE(.id = "ALL4", .what = "as ALL1, COVERAGE_TO_MASK 0 (MRT0.a path only)",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .ps = V_ALL, .bmode = BM_ALL,
                       .aa_mask = 0xB, .kind = AK_RANDOM, .seed = 0xA14u),
    ALPHA_TO_MASK_CASE(.id = "ALL5", .what = "as ALL1, 2x D0, AA_MASK 0x2", .ns = 2, .ps = V_ALL,
                       .dbsc = ALPHA_TO_MASK_COV2MASK, .bmode = BM_ALL, .aa_mask = 0x2,
                       .kind = AK_RANDOM, .seed = 0xA15u),
    /* dual-source blending (MRT1 = source 1), sample locations, Z test */
    ALPHA_TO_MASK_CASE(.id = "DS1",
                       .what = "dual-source blend ONE / SRC1_ALPHA; MRT0.a 1.0, MRT1.a A",
                       .ps = V_M1, .colfmt = 0x99, .cbsm = 0xFF, .blend = ALPHA_TO_MASK_BLEND_DUAL,
                       .kind = AK_RANDOM, .hyp = H_MRT1, .seed = 0xD51u),
    ALPHA_TO_MASK_CASE(.id = "SL1", .what = "4x D1, every sample location at the pixel centre",
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .locs = 1, .kind = AK_RANDOM,
                       .seed = 0x511u),
    ALPHA_TO_MASK_CASE(.id = "SL2", .what = "8x D1, every sample location at the pixel centre",
                       .ns = 8, .db_alpha_to_mask = ALPHA_TO_MASK_D1, .locs = 1, .kind = AK_RANDOM,
                       .seed = 0x512u),
    ALPHA_TO_MASK_CASE(.id = "Z01", .what = "ZFUNC NEVER", .zfunc = 1, .kind = AK_RANDOM,
                       .seed = 0x201u),
    ALPHA_TO_MASK_CASE(.id = "Z02", .what = "ZFUNC LESS (z 0.5 < 1.0)", .zfunc = 2,
                       .kind = AK_RANDOM, .seed = 0x202u),
    ALPHA_TO_MASK_CASE(.id = "Z03", .what = "ZFUNC EQUAL (fails)", .zfunc = 3, .kind = AK_RANDOM,
                       .seed = 0x203u),
    ALPHA_TO_MASK_CASE(.id = "Z04", .what = "ZFUNC GREATER (fails), 8x D1", .ns = 8,
                       .db_alpha_to_mask = ALPHA_TO_MASK_D1, .zfunc = 5, .kind = AK_RANDOM,
                       .seed = 0x204u),
    /* every level boundary of every configuration (families, one line per configuration) */
    ALPHA_TO_MASK_CASE(.id = "BU",
                       .what = "every level boundary, uniform offsets, 1x .. 8x, EQAA_DISABLE"),
    ALPHA_TO_MASK_CASE(.id = "BP",
                       .what = "every level boundary, a different offset per quad pixel"),
    ALPHA_TO_MASK_CASE(.id = "BN",
                       .what = "every level boundary, ALPHA_TO_MASK_NUM_SAMPLES != sample count"),
    ALPHA_TO_MASK_CASE(.id = "AA", .what = "every PA_SC_AA_MASK value at 1x, 2x, 4x and 8x",
                       .seed = 0xAA00u),
    ALPHA_TO_MASK_CASE(.id = "AAP",
                       .what = "random PA_SC_AA_MASK per quad pixel with alpha-to-mask, kill, mask "
                               "export, MRTZ alpha, the invocation marker and the coverage input",
                       .seed = 0xAA50u),
    ALPHA_TO_MASK_CASE(.id = "RND", .what = "random configurations (48), random alphas",
                       .kind = AK_RANDOM, .seed = 0x5EEDu),
    ALPHA_TO_MASK_CASE(.id = "E16a", .what = "16 coverage samples, no targets, EXEC_ON_NOOP, D1",
                       .ns = 16, .db_alpha_to_mask = ALPHA_TO_MASK_D1, .cbf = CBF_NONE,
                       .nodepth = 1, .dbsc = ALPHA_TO_MASK_EXEC_NOOP, .risky = 1),
    ALPHA_TO_MASK_CASE(.id = "E16b", .what = "16 coverage samples, no targets, EXEC_ON_NOOP, D0",
                       .ns = 16, .cbf = CBF_NONE, .nodepth = 1, .dbsc = ALPHA_TO_MASK_EXEC_NOOP,
                       .risky = 1),
    ALPHA_TO_MASK_CASE(.id = "AQ05",
                       .what = "AA_MASK 0x00FF / 0xFF00 / 0x0F0F / 0xF0F0 per quad pixel, 16x",
                       .ns = 16, .cbf = CBF_NONE, .nodepth = 1, .dbsc = ALPHA_TO_MASK_EXEC_NOOP,
                       .db_alpha_to_mask = ALPHA_TO_MASK_OFF0, .aa_quad = 0xF0F00F0FFF0000FFull,
                       .risky = 1),
    ALPHA_TO_MASK_CASE(.id = "AA16", .what = "PA_SC_AA_MASK at 16 samples (ZPASS_DONE only)",
                       .ns = 16, .cbf = CBF_NONE, .nodepth = 1, .dbsc = ALPHA_TO_MASK_EXEC_NOOP,
                       .risky = 1, .seed = 0xAA16u),
    ALPHA_TO_MASK_CASE(.id = "B16",
                       .what = "every level boundary, 16 coverage samples (ZPASS_DONE only)",
                       .ns = 16, .cbf = CBF_NONE, .nodepth = 1, .dbsc = ALPHA_TO_MASK_EXEC_NOOP,
                       .risky = 1),
    ALPHA_TO_MASK_CASE(.id = "E16c", .what = "16 coverage samples, EQAA_DISABLE, no targets, D1",
                       .ns = 16, .db_alpha_to_mask = ALPHA_TO_MASK_D1, .eqaa_dis = 1,
                       .cbf = CBF_NONE, .nodepth = 1, .dbsc = ALPHA_TO_MASK_EXEC_NOOP, .risky = 1),
};
#pragma clang diagnostic pop
#define ALPHA_TO_MASK_NTESTS                                                                       \
    ((int)(sizeof(k_alpha_to_mask_tests) / sizeof(k_alpha_to_mask_tests[0])))
#define ALPHA_TO_MASK_RND_CONFIGS 48

/* ---- log ---- */
static int g_alpha_to_mask_fd = -1;
static char g_alpha_to_mask_buf[ALPHA_TO_MASK_LOG_BUF];
static unsigned g_alpha_to_mask_len;
static char g_alpha_to_mask_log_path[160], g_alpha_to_mask_state_path[160];

static void alpha_to_mask_paths(void) {
    const char* t = g_trace_path ? g_trace_path : "";
    int dir = 0;
    for (int i = 0; t[i] && i < 120; i++)
        if (t[i] == '/')
            dir = i + 1;
    static const char log_name[] = "ShadCube4 alpha to mask.log",
                      state_name[] = "alpha_to_mask.state";
    for (int i = 0; i < dir; i++)
        g_alpha_to_mask_log_path[i] = g_alpha_to_mask_state_path[i] = t[i];
    for (int i = 0; i < (int)sizeof(log_name); i++)
        g_alpha_to_mask_log_path[dir + i] = log_name[i];
    for (int i = 0; i < (int)sizeof(state_name); i++)
        g_alpha_to_mask_state_path[dir + i] = state_name[i];
}
static void alpha_to_mask_flush(void) {
    if (g_alpha_to_mask_fd >= 0 && g_alpha_to_mask_len) {
        sceKernelWrite(g_alpha_to_mask_fd, g_alpha_to_mask_buf, g_alpha_to_mask_len);
        sceKernelFsync(g_alpha_to_mask_fd);
    }
    g_alpha_to_mask_len = 0;
}
static void alpha_to_mask_s(const char* s) {
    while (*s) {
        if (g_alpha_to_mask_len >= ALPHA_TO_MASK_LOG_BUF - 1)
            alpha_to_mask_flush();
        g_alpha_to_mask_buf[g_alpha_to_mask_len++] = *s++;
    }
}
static void alpha_to_mask_u(unsigned long long v) {
    char t[24];
    t[lg_u64(t, v)] = 0;
    alpha_to_mask_s(t);
}
static void alpha_to_mask_x(unsigned long long v, int digits) {
    static const char h[] = "0123456789abcdef";
    char t[20];
    int n = 0;
    t[n++] = '0';
    t[n++] = 'x';
    for (int i = digits - 1; i >= 0; i--)
        t[n++] = h[(v >> (i * 4)) & 0xf];
    t[n] = 0;
    alpha_to_mask_s(t);
}
static float alpha_to_mask_ftof(uint32_t b) {
    union {
        uint32_t u;
        float f;
    } c;
    c.u = b;
    return c.f;
}
static uint32_t alpha_to_mask_fbits(float f) {
    union {
        uint32_t u;
        float f;
    } c;
    c.f = f;
    return c.u;
}
/* A float as "0x3f000000 (0.500000000)": 9 decimals; NaN / inf / huge by name. */
static void alpha_to_mask_f(uint32_t b) {
    alpha_to_mask_x(b, 8);
    alpha_to_mask_s(" (");
    uint32_t e = (b >> 23) & 0xFF;
    if (e == 0xFF) {
        alpha_to_mask_s((b & 0x7FFFFF) ? "nan" : (b >> 31) ? "-inf" : "+inf");
    } else {
        double v = (double)alpha_to_mask_ftof(b & 0x7FFFFFFFu);
        if (b >> 31)
            alpha_to_mask_s("-");
        if (v >= 1e9) {
            alpha_to_mask_s(">=1e9");
        } else {
            unsigned long long ip = (unsigned long long)v;
            unsigned long long fr = (unsigned long long)((v - (double)ip) * 1e9 + 0.5);
            if (fr >= 1000000000ull) {
                ip++;
                fr -= 1000000000ull;
            }
            alpha_to_mask_u(ip);
            alpha_to_mask_s(".");
            char t[10];
            for (int i = 8; i >= 0; i--) {
                t[i] = (char)('0' + fr % 10);
                fr /= 10;
            }
            t[9] = 0;
            alpha_to_mask_s(t);
            if (v != 0.0 && v < 1e-6)
                alpha_to_mask_s(" tiny");
        }
    }
    alpha_to_mask_s(")");
}

/* ---- state file ---- */
/* inflight: the test running (or, | ALPHA_TO_MASK_CAPTURE, the visual capture) when the app ended;
   bad_variants: pixel shader variants that crashed or hung (ALPHA_TO_MASK_GROUP of the variant). */
#define ALPHA_TO_MASK_CAPTURE 0x10000
struct AlphaToMaskState {
    char build[16];
    int32_t next, inflight, regread_bad;
    uint64_t bad_variants;
};
static void alpha_to_mask_state_save(const struct AlphaToMaskState* st) {
    int fd = sceKernelOpen(g_alpha_to_mask_state_path, 0x601, 0x1FF);
    if (fd < 0)
        return;
    sceKernelWrite(fd, st, sizeof(*st));
    sceKernelFsync(fd);
    sceKernelClose(fd);
}
static int alpha_to_mask_state_load(struct AlphaToMaskState* st) {
    int fd = sceKernelOpen(g_alpha_to_mask_state_path, 0, 0);
    if (fd < 0)
        return 0;
    long n = sceKernelRead(fd, st, sizeof(*st));
    sceKernelClose(fd);
    if (n != (long)sizeof(*st))
        return 0;
    for (int i = 0; ALPHA_TO_MASK_BUILD[i] || st->build[i]; i++)
        if (ALPHA_TO_MASK_BUILD[i] != st->build[i])
            return 0;
    return 1;
}

static int alpha_to_mask_pop(uint32_t m) {
    int n = 0;
    for (; m; m &= m - 1)
        n++;
    return n;
}
static int alpha_to_mask_streq(const char* a, const char* b) {
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* ---- effective test fields ---- */
static uint32_t alpha_to_mask_colfmt(const struct AlphaToMaskTest* t) {
    return t->colfmt == ALPHA_TO_MASK_ZERO_COL ? 0u : t->colfmt ? t->colfmt : 9u;
}
static uint32_t alpha_to_mask_cbsm(const struct AlphaToMaskTest* t) {
    return t->cbsm == ALPHA_TO_MASK_ZERO_MASK ? 0u : t->cbsm ? t->cbsm : 0xFu;
}
static uint32_t alpha_to_mask_cbtm(const struct AlphaToMaskTest* t) {
    return t->cbtm == ALPHA_TO_MASK_ZERO_MASK ? 0u : t->cbtm ? t->cbtm : 0xFu;
}
static uint32_t alpha_to_mask_register(const struct AlphaToMaskTest* t) {
    return t->db_alpha_to_mask ? t->db_alpha_to_mask : ALPHA_TO_MASK_D0;
}
static uint32_t alpha_to_mask_aa(const struct AlphaToMaskTest* t) {
    return t->aa_mask == ALPHA_TO_MASK_AA_ZERO ? 0u : t->aa_mask ? t->aa_mask : 0xFFFFu;
}
static uint32_t alpha_to_mask_aa_pixel(const struct AlphaToMaskTest* t, uint32_t p) {
    return t->aa_quad ? (uint32_t)(t->aa_quad >> (16 * p)) & 0xFFFFu : alpha_to_mask_aa(t);
}
static uint32_t alpha_to_mask_log2(uint32_t n) {
    uint32_t l = 0;
    while ((1u << l) < n)
        l++;
    return l;
}
static uint32_t alpha_to_mask_full(uint32_t ns) {
    return ns >= 32 ? 0xFFFFFFFFu : (1u << ns) - 1u;
}
static int alpha_to_mask_has_readback(const struct AlphaToMaskTest* t) {
    return t->ns <= 8;
}

/* ---- GPU memory ---- */
struct AlphaToMaskContext {
    uint32_t* dcb;
    volatile uint32_t* fence;
    uint32_t fv;
    uint32_t* alpha;        /* A: 4096 values (x + 64 y), B at byte 0x4000, C at 0x8000 */
    uint32_t* tab;          /* shader table (gen_ps_alpha_to_mask.py) */
    uint32_t* tab_pre;      /* the same, its readback V# at out_pre */
    uint32_t* tab_iso;      /* the same, its table V# at alpha_iso (the isolation draws) */
    uint32_t* alpha_iso;    /* copy of A, B, C the isolation draws read and write */
    const uint32_t* ps_tab; /* the table the pixel shader gets */
    uint32_t*
        out; /* readback after the test draw: colour raw, depth raw at ALPHA_TO_MASK_OUT_DEPTH */
    uint32_t* out_pre; /* readback before it */
    uint32_t* zp;      /* ZPASS_DONE blocks: 0 the test draw, 1 + s isolation of sample s */
    uint32_t* regs;    /* COPY_DATA of the test draw's registers */
    void* color;       /* ns-sample colour */
    void* depth;       /* ns-sample depth */
    void* scratch;     /* the readback pass's 1-sample target */
    void* sh[ALPHA_TO_MASK_NSH];
    void* vs;
    void* vs_net; /* the textured net's vertex shader */
    void* tex;    /* the net texture: RGBA8 256 x 256, 4 levels, LINEAR_ALIGNED */
    float* verts; /* the net plane: 3 vertices (x, y, z, w, u, v, -, -) */
};
static struct AlphaToMaskContext g_alpha_to_mask_context;
static int g_alpha_to_mask_context_ok;

/* Registers the test draw sets, also read back with COPY_DATA (context index, name). The first
   run read constant garbage for the three marked unreliable while the draws used the written
   values. */
static const struct {
    uint16_t idx;
    uint8_t unreliable;
    const char* name;
} k_alpha_to_mask_rr[] = {
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
    {0x202, 0, "CB_COLOR_CONTROL"},
};
#define ALPHA_TO_MASK_NRR ((int)(sizeof(k_alpha_to_mask_rr) / sizeof(k_alpha_to_mask_rr[0])))
static uint32_t g_alpha_to_mask_written[ALPHA_TO_MASK_NRR],
    g_alpha_to_mask_wtest[ALPHA_TO_MASK_NRR]; /* last written; at the test draw */

static void alpha_to_mask_reg(struct PM4Builder* b, uint32_t idx, uint32_t v) {
    pm4_set_context_reg(b, idx, v);
    for (int i = 0; i < ALPHA_TO_MASK_NRR; i++)
        if (k_alpha_to_mask_rr[i].idx == idx)
            g_alpha_to_mask_written[i] = v;
}

/* PA_SC_AA_SAMPLE_LOCS nibble pairs (radeonsi si_state_msaa.c FILL_SREG). */
static uint32_t alpha_to_mask_locs(int s0x, int s0y, int s1x, int s1y, int s2x, int s2y, int s3x,
                                   int s3y) {
    return ((uint32_t)s0x & 0xF) | (((uint32_t)s0y & 0xF) << 4) | (((uint32_t)s1x & 0xF) << 8) |
           (((uint32_t)s1y & 0xF) << 12) | (((uint32_t)s2x & 0xF) << 16) |
           (((uint32_t)s2y & 0xF) << 20) | (((uint32_t)s3x & 0xF) << 24) |
           (((uint32_t)s3y & 0xF) << 28);
}

/* Rasterizer / DB sample state (radeonsi si_emit_msaa_config / si_state_msaa.c; the 4x pattern is
   the scene's, emit_msaa_state in main.c). */
static void alpha_to_mask_msaa(struct PM4Builder* b, uint32_t ns, uint32_t msaa_en,
                               uint32_t alpha_to_mask_num_samples, uint32_t eqaa_dis,
                               uint32_t ps_iter, uint32_t aa_lo, uint32_t aa_hi, uint32_t centre,
                               uint32_t mask_export_ns) {
    static const uint32_t max_dist[5] = {0, 4, 6, 7, 7};
    uint32_t lg = alpha_to_mask_log2(ns), locs[16] = {0}, cp[2] = {0, 0};
    if (ns == 2) {
        uint32_t l = alpha_to_mask_locs(-4, -4, 4, 4, 0, 0, 0, 0);
        locs[0] = locs[4] = locs[8] = locs[12] = l;
        cp[0] = cp[1] = 0x10101010u;
    } else if (ns == 4) {
        uint32_t l = alpha_to_mask_locs(-2, -6, 6, -2, -6, 2, 2, 6);
        locs[0] = locs[4] = locs[8] = locs[12] = l;
        cp[0] = cp[1] = 0x32103210u;
    } else if (ns == 8) {
        uint32_t l0 = alpha_to_mask_locs(-3, -5, 5, 1, -1, 3, 7, -7),
                 l1 = alpha_to_mask_locs(-7, -1, 3, 7, -5, 5, 1, -3);
        for (int i = 0; i < 4; i++) {
            locs[4 * i] = l0;
            locs[4 * i + 1] = l1;
        }
        cp[0] = cp[1] = 0x35460127u;
    } else if (ns == 16) {
        uint32_t l[4] = {alpha_to_mask_locs(-5, -2, 5, 3, -2, 6, 3, -5),
                         alpha_to_mask_locs(-4, -6, 1, 1, -6, 4, 7, -4),
                         alpha_to_mask_locs(-1, -3, 6, 7, -3, 2, 0, -7),
                         alpha_to_mask_locs(-7, -7, 2, 5, -7, 0, 4, -1)};
        for (int i = 0; i < 16; i++)
            locs[i] = l[i & 3];
        cp[0] = 0x31d0fa85u;
        cp[1] = 0xc97e64b2u;
    }
    if (centre)
        for (int i = 0; i < 16; i++)
            locs[i] = 0;
    alpha_to_mask_reg(b, CTX_AA_CONFIG, ns > 1 ? (lg | (max_dist[lg] << 13) | (lg << 20)) : 0u);
    alpha_to_mask_reg(b, CTX_MODE_CONTROL, msaa_en | ((ns > 1 || msaa_en) ? 2u : 0u));
    alpha_to_mask_reg(b, ALPHA_TO_MASK_PA_SC_MODE_CNTL_1,
                      0x06000000u | (ps_iter ? (1u << 16) : 0u));
    alpha_to_mask_reg(b, ALPHA_TO_MASK_DB_EQAA,
                      (ns > 1 ? lg : 0u) | (mask_export_ns << 8) | (ps_iter << 4) |
                          (alpha_to_mask_num_samples << 12) | (1u << 16) | (1u << 17) | (1u << 20) |
                          (eqaa_dis << 21));
    alpha_to_mask_reg(b, CTX_PA_SC_AA_MASK_X0Y0_X1Y0, aa_lo);
    alpha_to_mask_reg(b, CTX_PA_SC_AA_MASK_X0Y0_X1Y0 + 1, aa_hi);
    pm4_set_context_regs(b, CTX_PA_SC_CENTROID_PRIORITY_0, cp, 2);
    pm4_set_context_regs(b, CTX_PA_SC_AA_SAMPLE_LOCS_X0Y0_0, locs, 16);
    pm4_set_context_reg(b, CTX_DB_RENDER_OVERRIDE2, ns > 2 ? (1u << 8) : 0u);
}

/* Colour (cbf, CBF_NONE: no target) and depth (on) targets of ns <= 8 samples, 64 x 64, the
   scene's layout (main.c build_dcb: tile index 13 without FMASK / CMASK, FMASK fields = the
   colour's; depth ARRAY_1D_TILED_THIN1, PIPE_CONFIG 12). */
static void alpha_to_mask_targets(struct PM4Builder* b, struct AlphaToMaskContext* c, uint32_t ns,
                                  uint32_t cbf, uint32_t on) {
    uint32_t lg = alpha_to_mask_log2(ns);
    if (on) {
        uint32_t z = (uint32_t)((uint64_t)(uintptr_t)c->depth >> 8);
        uint32_t d[4] = {z, 0, z, 0};
        alpha_to_mask_reg(b, CTX_DB_Z_INFO, 3u | (lg << 2));
        pm4_set_context_reg(b, CTX_DB_DEPTH_INFO, (1u << 0) | (2u << 4) | (12u << 8));
        pm4_set_context_reg(b, CTX_DB_STENCIL_INFO, 0);
        pm4_set_context_regs(b, CTX_DB_Z_READ_BASE, d, 4);
        pm4_set_context_reg(b, CTX_DB_DEPTH_SIZE,
                            ((ALPHA_TO_MASK_W / 8) - 1) | (((ALPHA_TO_MASK_W / 8) - 1) << 11));
        pm4_set_context_reg(b, CTX_DB_DEPTH_SLICE, (ALPHA_TO_MASK_PX / 64) - 1);
        pm4_set_context_reg(b, CTX_DEPTH_VIEW, 0);
        pm4_set_context_reg(b, CTX_DEPTH_RENDER_OVERRIDE, 0);
    } else {
        alpha_to_mask_reg(b, CTX_DB_Z_INFO, 0);
    }
    uint32_t r[14] = {0};
    if (cbf != CBF_NONE) {
        uint32_t cb = (uint32_t)((uint64_t)(uintptr_t)c->color >> 8);
        uint32_t pt = (ALPHA_TO_MASK_W / 8) - 1, sl = (ALPHA_TO_MASK_PX / 64) - 1;
        r[0] = cb;
        r[1] = pt | (pt << 20);
        r[2] = sl;
        r[4] = k_alpha_to_mask_cbf[cbf].info;
        r[5] = 13u | (13u << 5) | (lg << 12) | (lg << 15);
        r[9] = cb;
        r[10] = sl;
    }
    pm4_set_context_regs(b, CTX_CB_COLOR0_BASE, r, 14);
    for (int i = 0; i < ALPHA_TO_MASK_NRR; i++) {
        if (k_alpha_to_mask_rr[i].idx == CTX_CB_COLOR0_INFO)
            g_alpha_to_mask_written[i] = r[4];
        if (k_alpha_to_mask_rr[i].idx == CTX_CB_COLOR0_ATTRIB)
            g_alpha_to_mask_written[i] = r[5];
    }
    pm4_emit(b, 0xC0001000u);
    pm4_emit(b, (uint32_t)ALPHA_TO_MASK_W | ((uint32_t)ALPHA_TO_MASK_W << 16));
}

/* State shared by every draw: 64 x 64 viewport and scissors, vs_full (z = zbits), one parameter,
   PERSP_CENTER + POS_X / POS_Y_FLOAT (v2, v3), + ANCILLARY (v4) for t_iter; window offset wofs
   (PA_SU_SC_MODE_CNTL.VTX_WINDOW_OFFSET_ENABLE when set). */
static void alpha_to_mask_common(struct PM4Builder* b, struct AlphaToMaskContext* c, uint32_t zbits,
                                 int v, uint32_t wofs) {
    uint32_t ena = v == V_ITER ? 0x2302u : v == V_COV ? 0x4302u : 0x302u;
    if (v == V_TEX) { /* vs_net: the V# of the plane's vertices in VS_USER_DATA_0..3 */
        uint64_t va = (uint64_t)(uintptr_t)c->vs_net;
        uint32_t vr[4] = {(uint32_t)(va >> 8), (uint32_t)(va >> 40), ALPHA_TO_MASK_VS_NET_RSRC1,
                          4u << 1};
        uint32_t vb[4];
        build_vsharp_strided(vb, c->verts, 3 * 32, 32);
        pm4_set_sh_regs(b, SH_VS_PGM_LO, vr, 4);
        pm4_set_sh_regs(b, SH_VS_USER_DATA_0, vb, 4);
    } else {
        uint64_t va = (uint64_t)(uintptr_t)c->vs;
        uint32_t vr[4] = {(uint32_t)(va >> 8), (uint32_t)(va >> 40), ALPHA_TO_MASK_VS_FULL_RSRC1,
                          1u << 1};
        pm4_set_sh_regs(b, SH_VS_PGM_LO, vr, 4);
        pm4_set_sh_reg(b, SH_VS_USER_DATA_0, zbits);
    }
    alpha_to_mask_reg(b, ALPHA_TO_MASK_PA_SC_WINDOW_OFFSET, wofs);
    pm4_set_context_reg(b, CTX_VGT_GS_ONCHIP_CNTL, VGT_GS_ONCHIP_CNTL_SAFE);
    {
        uint32_t sc[2] = {0, (uint32_t)ALPHA_TO_MASK_W | ((uint32_t)ALPHA_TO_MASK_W << 16)};
        pm4_set_context_regs(b, CTX_SCREEN_SCISSOR, sc, 2);
        pm4_set_context_regs(b, CTX_VIEWPORT_SCISSOR0, sc, 2);
        sc[0] = 1u << 31; /* WINDOW_OFFSET_DISABLE */
        pm4_set_context_regs(b, CTX_GENERIC_SCISSOR, sc, 2);
        pm4_set_context_regs(b, CTX_WINDOW_SCISSOR, sc, 2);
    }
    pm4_emit(b, pm4_type3(PM4_SET_CONTEXT_REG, 7));
    pm4_emit(b, CTX_VIEWPORT0);
    pm4_emit_f(b, ALPHA_TO_MASK_W * 0.5f);
    pm4_emit_f(b, ALPHA_TO_MASK_W * 0.5f);
    pm4_emit_f(b, ALPHA_TO_MASK_W * -0.5f);
    pm4_emit_f(b, ALPHA_TO_MASK_W * 0.5f);
    pm4_emit_f(b, 1.0f);
    pm4_emit_f(b, 0.0f);
    {
        uint32_t z[2] = {0x00000000u, 0x3F800000u};
        pm4_set_context_regs(b, CTX_VIEWPORT_ZMIN0, z, 2);
    }
    pm4_set_context_reg(b, CTX_INDEX_OFFSET, 0);
    pm4_set_context_reg(b, CTX_PS_INPUT_CNTL_0, 0);
    pm4_set_context_reg(b, CTX_VS_OUTPUT_CONFIG, 0);
    alpha_to_mask_reg(b, CTX_PS_INPUT_ENA, ena);
    pm4_set_context_reg(b, CTX_PS_INPUT_ADDR, ena);
    pm4_set_context_reg(b, CTX_NUM_INTERP, 1);
    pm4_set_context_reg(b, CTX_SHADER_POS_FORMAT, 4);
    pm4_set_context_reg(b, CTX_COLOR_CONTROL, 0x00CC0010u);
    pm4_set_context_reg(b, CTX_SPI_BARYC_CNTL, 0);
    pm4_set_context_reg(b, CTX_CLIPPER_CONTROL, 1u << 19);
    pm4_set_context_reg(b, CTX_VIEWPORT_CONTROL, 0x43F);
    pm4_set_context_reg(b, CTX_VS_OUTPUT_CONTROL, 0);
    alpha_to_mask_reg(b, ALPHA_TO_MASK_CB_BLEND0_CONTROL, 0);
    alpha_to_mask_reg(b, ALPHA_TO_MASK_PA_SU_SC_MODE_CNTL, wofs ? (1u << 16) : 0u);
    pm4_set_uconfig_reg(b, UCFG_PRIMITIVE_TYPE, 4);
    pm4_set_uconfig_reg(b, UCFG_NUM_INSTANCES, 1);
}

static void alpha_to_mask_ps_tab(struct PM4Builder* b, struct AlphaToMaskContext* c, int v,
                                 const uint32_t* tab) {
    uint64_t pa = (uint64_t)(uintptr_t)c->sh[v], t = (uint64_t)(uintptr_t)tab;
    uint32_t pr[4] = {(uint32_t)(pa >> 8), (uint32_t)(pa >> 40), k_alpha_to_mask_sh[v].rsrc1,
                      2u << 1};
    pm4_set_sh_regs(b, SH_PS_PGM_LO, pr, 4);
    uint32_t ud[2] = {(uint32_t)t, (uint32_t)(t >> 32)};
    pm4_set_sh_regs(b, SH_PS_USER_DATA_0, ud, 2);
}
static void alpha_to_mask_ps(struct PM4Builder* b, struct AlphaToMaskContext* c, int v) {
    alpha_to_mask_ps_tab(b, c, v, c->ps_tab ? c->ps_tab : c->tab);
}

/* SPI_SHADER_Z_FORMAT and DB_SHADER_CONTROL bits of a test variant (ac_get_spi_shader_z_format:
   Z + alpha 32_AR, Z + mask 32_ABGR). */
static uint32_t alpha_to_mask_zfmt(int v) {
    if (v == V_TEX)
        return 1u; /* 32_R: Z only */
    if (v == V_ALL)
        return 9u;
    return (v == V_ZA || v == V_AZ || v == V_ZAB || v == V_ZA0) ? 3u : v == V_MASK ? 9u : 0u;
}
static uint32_t alpha_to_mask_vdbsc(int v) {
    if (v == V_ZA || v == V_AZ || v == V_ZAB || v == V_ZA0 || v == V_TEX)
        return ALPHA_TO_MASK_ZEXP;
    if (v == V_MASK)
        return ALPHA_TO_MASK_ZEXP | ALPHA_TO_MASK_MASKEXP;
    if (v == V_ALL)
        return ALPHA_TO_MASK_ZEXP | ALPHA_TO_MASK_MASKEXP | ALPHA_TO_MASK_KILL;
    return (v == V_KILL || v == V_KILL2 || v == V_KILL3 || v == V_KILL_NOVM ||
            (v >= V_KP_E64 && v <= V_KP_RESTORE))
               ? ALPHA_TO_MASK_KILL
               : 0u;
}

/* The clear draw: every sample, colour 0.25 (the test's colour format; RGBA16F when the test
   has no colour target), depth 1.0. */
static void alpha_to_mask_clear_draw(struct PM4Builder* b, struct AlphaToMaskContext* c,
                                     const struct AlphaToMaskTest* t) {
    alpha_to_mask_common(b, c, 0x3F000000u, V_CLR, 0);
    alpha_to_mask_targets(b, c, t->ns, t->cbf == CBF_NONE ? CBF_F16 : t->cbf, 1);
    alpha_to_mask_msaa(b, t->ns, t->ns > 1, alpha_to_mask_log2(t->ns), 0, 0, 0xFFFFFFFFu,
                       0xFFFFFFFFu, 0, alpha_to_mask_log2(t->ns));
    alpha_to_mask_reg(b, ALPHA_TO_MASK_DB_ALPHA_TO_MASK, 0);
    alpha_to_mask_reg(b, ALPHA_TO_MASK_DB_SHADER_CONTROL, 0);
    alpha_to_mask_reg(b, ALPHA_TO_MASK_DB_COUNT_CONTROL, 0);
    pm4_set_context_reg(b, ALPHA_TO_MASK_DB_DEPTH_CLEAR, 0x3F800000u);
    alpha_to_mask_reg(b, ALPHA_TO_MASK_DB_RENDER_CONTROL, 1u); /* DEPTH_CLEAR_ENABLE */
    alpha_to_mask_reg(b, ALPHA_TO_MASK_DB_DEPTH_CONTROL, (1u << 1) | (1u << 2) | (7u << 4));
    alpha_to_mask_reg(b, CTX_Z_EXPORT_FORMAT, 0);
    alpha_to_mask_reg(b, CTX_COLOR_EXPORT_FORMAT, 9);
    alpha_to_mask_reg(b, CTX_COLOR_TARGET_MASK, 0xF);
    alpha_to_mask_reg(b, CTX_COLOR_SHADER_MASK, 0xF);
    alpha_to_mask_ps(b, c, V_CLR);
    pm4_draw_index_auto(b, 3);
}

/* The test draw's state (select: the samples PA_SC_AA_MASK keeps of the test's masks). */
static void alpha_to_mask_test_state(struct PM4Builder* b, struct AlphaToMaskContext* c,
                                     const struct AlphaToMaskTest* t, uint32_t select) {
    uint32_t aa[4];
    for (uint32_t p = 0; p < 4; p++)
        aa[p] = alpha_to_mask_aa_pixel(t, p) & select & 0xFFFFu;
    uint32_t lg = alpha_to_mask_log2(t->ns);
    int tg = alpha_to_mask_has_readback(t);
    alpha_to_mask_common(b, c, 0x3F000000u, t->ps, t->wofs);
    alpha_to_mask_targets(b, c, t->ns, tg ? t->cbf : CBF_NONE, tg && !t->nodepth);
    alpha_to_mask_msaa(b, t->ns, t->msaa_en == 2 ? (t->ns > 1) : t->msaa_en,
                       t->alpha_to_mask_num_samples == 0xFF ? lg : t->alpha_to_mask_num_samples,
                       t->eqaa_dis, t->ps_iter, aa[0] | (aa[1] << 16), aa[2] | (aa[3] << 16),
                       t->locs, t->mask_export_ns == 0xFF ? lg : t->mask_export_ns);
    alpha_to_mask_reg(b, ALPHA_TO_MASK_DB_ALPHA_TO_MASK, alpha_to_mask_register(t));
    alpha_to_mask_reg(b, ALPHA_TO_MASK_DB_SHADER_CONTROL,
                      (t->dbsc | alpha_to_mask_vdbsc(t->ps)) &
                          ~(t->no_kill_enable ? ALPHA_TO_MASK_KILL : 0u));
    /* occlusion counting (radeonsi si_emit_db_render_state, GFX7): PERFECT_ZPASS_COUNTS,
       SAMPLE_RATE log2(samples), ZPASS_ENABLE 1, SLICE_EVEN / ODD_ENABLE */
    alpha_to_mask_reg(b, ALPHA_TO_MASK_DB_COUNT_CONTROL,
                      (1u << 1) | (lg << 4) | (1u << 8) | (1u << 24) | (1u << 28));
    alpha_to_mask_reg(b, ALPHA_TO_MASK_DB_RENDER_CONTROL, 0);
    alpha_to_mask_reg(b, ALPHA_TO_MASK_DB_DEPTH_CONTROL,
                      (tg && !t->nodepth)
                          ? ((1u << 1) | (1u << 2) | ((t->zfunc ? t->zfunc - 1u : 7u) << 4))
                          : 0u);
    alpha_to_mask_reg(b, CTX_Z_EXPORT_FORMAT, alpha_to_mask_zfmt(t->ps));
    alpha_to_mask_reg(b, CTX_COLOR_EXPORT_FORMAT, alpha_to_mask_colfmt(t));
    alpha_to_mask_reg(b, CTX_COLOR_TARGET_MASK, alpha_to_mask_cbtm(t));
    alpha_to_mask_reg(b, CTX_COLOR_SHADER_MASK, alpha_to_mask_cbsm(t));
    alpha_to_mask_reg(b, ALPHA_TO_MASK_CB_BLEND0_CONTROL, t->blend);
    alpha_to_mask_reg(b, CTX_COLOR_CONTROL, t->cb_disable ? 0x00CC0000u : 0x00CC0010u);
    alpha_to_mask_ps(b, c, t->ps);
}

/* ZPASS_DONE (EVENT_WRITE, event 0x15, index 1): each RB writes its 64-bit count (bit 63 valid)
   at addr + 16 rb (radeonsi si_query.c). */
static void alpha_to_mask_zpass(struct PM4Builder* b, void* addr) {
    uint64_t a = (uint64_t)(uintptr_t)addr;
    pm4_emit(b, pm4_type3(0x46, 3));
    pm4_emit(b, 0x15u | (1u << 8));
    pm4_emit(b, (uint32_t)a);
    pm4_emit(b, (uint32_t)(a >> 32));
}
/* EVENT_WRITE with index 4 (PS_PARTIAL_FLUSH 0x10: wait for the pixel shaders, Mesa
   ac_barrier.c on GFX6-8 before the SURFACE_SYNC). */
static void alpha_to_mask_event(struct PM4Builder* b, uint32_t type, uint32_t index) {
    pm4_emit(b, pm4_type3(0x46, 1));
    pm4_emit(b, type | (index << 8));
}
/* EVENT_WRITE_EOP CACHE_FLUSH_AND_INV_TS (0x14, index 5), data 32-bit value, no interrupt (sid.h
   EOP_* fields; pm4_event_write_eop's address and data layout). */
static void alpha_to_mask_eop(struct PM4Builder* b, volatile uint32_t* addr, uint32_t v) {
    uint64_t a = (uint64_t)(uintptr_t)addr;
    pm4_emit(b, pm4_type3(0x47, 5));
    pm4_emit(b, 0x14u | (5u << 8));
    pm4_emit(b, (uint32_t)(a & 0xFFFFFFFCu));
    pm4_emit(b, ((uint32_t)(a >> 32) & 0xFFFFu) | (1u << 29));
    pm4_emit(b, v);
    pm4_emit(b, 0);
}
/* COPY_DATA register -> memory (src_sel 0, dst_sel 5, WR_CONFIRM), as ordcnt's oc_copy_reg. */
static void alpha_to_mask_copy_reg(struct PM4Builder* b, uint32_t reg, void* dst) {
    uint64_t a = (uint64_t)(uintptr_t)dst;
    pm4_emit(b, pm4_type3(0x40, 5));
    pm4_emit(b, (5u << 8) | (1u << 20));
    pm4_emit(b, reg);
    pm4_emit(b, 0);
    pm4_emit(b, (uint32_t)a);
    pm4_emit(b, (uint32_t)(a >> 32));
}

/* The readback pass: 1 sample, the scratch RGBA16F target, no depth. */
static void alpha_to_mask_readback(struct PM4Builder* b, struct AlphaToMaskContext* c,
                                   const uint32_t* tab) {
    alpha_to_mask_common(b, c, 0x3F000000u, V_RD, 0);
    {
        uint32_t s = (uint32_t)((uint64_t)(uintptr_t)c->scratch >> 8);
        uint32_t r[14] = {s,
                          (ALPHA_TO_MASK_W / 8) - 1,
                          (ALPHA_TO_MASK_PX / 64) - 1,
                          0,
                          CB_INFO_RGBA16F,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0};
        pm4_set_context_regs(b, CTX_CB_COLOR0_BASE, r, 14);
        pm4_emit(b, 0xC0001000u);
        pm4_emit(b, (uint32_t)ALPHA_TO_MASK_W | ((uint32_t)ALPHA_TO_MASK_W << 16));
    }
    pm4_set_context_reg(b, CTX_DB_Z_INFO, 0);
    emit_msaa_state(b, 1);
    pm4_set_context_reg(b, ALPHA_TO_MASK_PA_SC_MODE_CNTL_1, 0x06000000u);
    pm4_set_context_reg(b, ALPHA_TO_MASK_DB_ALPHA_TO_MASK, 0);
    pm4_set_context_reg(b, ALPHA_TO_MASK_DB_SHADER_CONTROL, 0);
    pm4_set_context_reg(b, ALPHA_TO_MASK_DB_COUNT_CONTROL, 0);
    pm4_set_context_reg(b, ALPHA_TO_MASK_DB_RENDER_CONTROL, 0);
    pm4_set_context_reg(b, ALPHA_TO_MASK_DB_DEPTH_CONTROL, 0);
    pm4_set_context_reg(b, CTX_Z_EXPORT_FORMAT, 0);
    pm4_set_context_reg(b, CTX_COLOR_EXPORT_FORMAT, 9);
    pm4_set_context_reg(b, CTX_COLOR_TARGET_MASK, 0xF);
    pm4_set_context_reg(b, CTX_COLOR_SHADER_MASK, 0xF);
    alpha_to_mask_ps_tab(b, c, V_RD, tab);
    pm4_draw_index_auto(b, 3);
}

/* Readback T#s (table 16: colour, 24: depth): TYPE 2D_MSAA (0xE) with LAST_LEVEL log2(ns), or 2D
   (9) for one sample; TILING_INDEX as the target (13 colour, 5 depth: build_tsharp_depth_msaa). */
static void alpha_to_mask_tsharps(struct AlphaToMaskContext* c, const struct AlphaToMaskTest* t) {
    uint32_t lg = alpha_to_mask_log2(t->ns), type = t->ns > 1 ? 0xEu : 9u;
    uint32_t cbf = t->cbf == CBF_NONE ? CBF_F16 : t->cbf;
    uint64_t a = (uint64_t)(uintptr_t)c->color;
    uint32_t* r = c->tab + 16;
    my_memset(r, 0, 64);
    r[0] = (uint32_t)(a >> 8);
    r[1] = (uint32_t)(a >> 40) | (k_alpha_to_mask_cbf[cbf].dfmt << 20) |
           (k_alpha_to_mask_cbf[cbf].nfmt << 26);
    r[2] = (uint32_t)(ALPHA_TO_MASK_W - 1) | ((uint32_t)(ALPHA_TO_MASK_W - 1) << 14);
    r[3] = 4u | (5u << 3) | (6u << 6) | (7u << 9) | (lg << 16) | (13u << 20) | (type << 28);
    r[4] = (uint32_t)(ALPHA_TO_MASK_W - 1) << 13;
    build_tsharp_depth_msaa(r + 8, c->depth, ALPHA_TO_MASK_W, ALPHA_TO_MASK_W);
    r[11] = (r[11] & ~((0xFu << 16) | (0xFu << 28))) | (lg << 16) | (type << 28);
}

/* One pass: clear, test draw, readback, and (iso) the isolation draws; then the end-of-pass
   synchronisation (file comment). */
static void alpha_to_mask_build(struct PM4Builder* b, struct AlphaToMaskContext* c,
                                const struct AlphaToMaskTest* t, int iso, int regread) {
    uint32_t avail = b->cap - b->off;
    int w = sceGnmDrawInitDefaultHardwareState350(b->buf + b->off, avail);
    if (w > 0)
        b->off += (uint32_t)w;
    pm4_acquire_mem(b, ALPHA_TO_MASK_COHER_ICACHE | ALPHA_TO_MASK_COHER_KCACHE |
                           ALPHA_TO_MASK_COHER_TCL1 | ALPHA_TO_MASK_COHER_TC);
    pm4_context_control(b);
    int rb = alpha_to_mask_has_readback(t);
    if (rb) {
        /* every sample before the test draw (pre), to tell which ones it changes */
        alpha_to_mask_clear_draw(b, c, t);
        pm4_acquire_mem(b, COHER_RT_TO_TEXTURE | ALPHA_TO_MASK_COHER_DB_DEST_BASE |
                               ALPHA_TO_MASK_COHER_DEST_BASE_0);
        alpha_to_mask_readback(b, c, c->tab_pre);
        alpha_to_mask_event(b, 0x10,
                            4); /* PS_PARTIAL_FLUSH: the reads finish before the targets change */
    }
    alpha_to_mask_test_state(b, c, t, 0xFFFFFFFFu);
    for (int i = 0; i < ALPHA_TO_MASK_NRR; i++)
        g_alpha_to_mask_wtest[i] = g_alpha_to_mask_written[i];
    if (regread)
        for (int i = 0; i < ALPHA_TO_MASK_NRR; i++)
            alpha_to_mask_copy_reg(b, 0xA000u + k_alpha_to_mask_rr[i].idx, c->regs + i);
    alpha_to_mask_zpass(b, c->zp);
    pm4_draw_index_auto(b, 3);
    alpha_to_mask_zpass(b, c->zp + 2);
    if (rb) {
        /* DB / CB to memory before they are read as textures (emit_post: DB_DEST_BASE_ENA and
           DEST_BASE_0_ENA, gfx6Barrier.cpp) */
        pm4_acquire_mem(b, COHER_RT_TO_TEXTURE | ALPHA_TO_MASK_COHER_DB_DEST_BASE |
                               ALPHA_TO_MASK_COHER_DEST_BASE_0);
        alpha_to_mask_readback(b, c, c->tab);
    }
    for (uint32_t s = 0; iso && s < t->ns; s++) {
        if (rb) /* the test draw sees the same targets as the first one */
            alpha_to_mask_clear_draw(b, c, t);
        c->ps_tab = c->tab_iso; /* stores of the isolation draws go to alpha_iso */
        alpha_to_mask_test_state(b, c, t, 1u << s);
        c->ps_tab = c->tab;
        alpha_to_mask_zpass(b, c->zp + (1 + s) * (ALPHA_TO_MASK_ZP_STRIDE / 4));
        pm4_draw_index_auto(b, 3);
        alpha_to_mask_zpass(b, c->zp + (1 + s) * (ALPHA_TO_MASK_ZP_STRIDE / 4) + 2);
    }
    alpha_to_mask_event(b, 0x10, 4); /* PS_PARTIAL_FLUSH */
    pm4_acquire_mem(b, ALPHA_TO_MASK_COHER_TC | ALPHA_TO_MASK_COHER_TCL1 | ALPHA_TO_MASK_COHER_CB |
                           ALPHA_TO_MASK_COHER_DB | ALPHA_TO_MASK_COHER_DEST_BASE_0 |
                           ALPHA_TO_MASK_COHER_DB_DEST_BASE);
    c->fv++;
    alpha_to_mask_eop(b, c->fence + 1, c->fv);
    alpha_to_mask_eop(b, c->fence, c->fv);
}

/* Returns 0 done, -1 fence timeout, -2 could not submit. */
static int alpha_to_mask_submit(struct AlphaToMaskContext* c, const struct AlphaToMaskTest* t,
                                int iso, int regread, uint64_t* us) {
    struct PM4Builder b;
    my_memset(c->out, 0xA5, 2 * ALPHA_TO_MASK_OUT_DEPTH * 4);
    my_memset(c->out_pre, 0xA5, 2 * ALPHA_TO_MASK_OUT_DEPTH * 4);
    my_memcpy(c->alpha_iso, c->alpha, 3 * ALPHA_TO_MASK_PX * 4);
    my_memset(c->zp, 0, ALPHA_TO_MASK_ZP_BLOCKS * ALPHA_TO_MASK_ZP_STRIDE);
    for (int i = 0; i < ALPHA_TO_MASK_NRR; i++)
        c->regs[i] = 0xDEADBEEFu;
    pm4_init(&b, c->dcb, ALPHA_TO_MASK_DCB_DW);
    alpha_to_mask_build(&b, c, t, iso, regread);
    if (b.overflow)
        return -2;
    uint64_t t0 = sceKernelGetProcessTime();
    uint32_t sz = b.off * 4;
    void* a[1] = {c->dcb};
    if (sceGnmSubmitCommandBuffers(1, a, &sz, 0, 0) != 0)
        return -2;
    sceGnmSubmitDone();
    while (*c->fence < c->fv) {
        if (sceKernelGetProcessTime() - t0 > ALPHA_TO_MASK_TIMEOUT_US)
            return -1;
        sceKernelUsleep(100);
    }
    *us = sceKernelGetProcessTime() - t0;
    return 0;
}

/* ---- the model ---- */
/* An alpha as the DB sees it: zero, full, a double in (0, 1) or the ratio num / den. */
struct AlphaValue {
    uint8_t k; /* 0 zero, 1 full, 2 double, 3 ratio */
    double d;
    uint64_t num, den;
};
static struct AlphaValue alpha_value_zero(void) {
    struct AlphaValue a = {0, 0.0, 0, 1};
    return a;
}
static struct AlphaValue alpha_value_fullv(void) {
    struct AlphaValue a = {1, 1.0, 1, 1};
    return a;
}
static struct AlphaValue alpha_value_ratio(uint64_t num, uint64_t den) {
    if (!num)
        return alpha_value_zero();
    if (num >= den)
        return alpha_value_fullv();
    struct AlphaValue a = {3, 0.0, num, den};
    return a;
}
static struct AlphaValue alpha_value_f32(uint32_t b) {
    uint32_t e = (b >> 23) & 0xFF;
    if (e == 0xFF)
        return (b & 0x7FFFFFu) || (b >> 31) ? alpha_value_zero()
                                            : alpha_value_fullv(); /* NaN / -inf / +inf */
    if ((b >> 31) || !(b & 0x7FFFFFFFu))
        return alpha_value_zero();
    double d = (double)alpha_to_mask_ftof(b);
    if (d >= 1.0)
        return alpha_value_fullv();
    struct AlphaValue a = {2, d, 0, 1};
    return a;
}
static struct AlphaValue alpha_value_half(uint32_t h) {
    uint32_t e = (h >> 10) & 0x1F, m = h & 0x3FF;
    if (e == 0x1F)
        return (m || (h & 0x8000u)) ? alpha_value_zero() : alpha_value_fullv();
    if ((h & 0x8000u) || !(h & 0x7FFFu))
        return alpha_value_zero();
    /* value = m 2^-24 (denormal) or (1024 + m) 2^(e - 25) */
    uint64_t num = e ? (1024u + m) : m;
    int sh = e ? (int)e - 25 : -24;
    if (sh >= 0)
        return alpha_value_ratio(num << sh, 1);
    return alpha_value_ratio(num, 1ull << (-sh));
}
/* v_cvt_pkrtz_f16_f32: round toward zero. */
static uint32_t alpha_to_mask_f2h_rtz(uint32_t b) {
    uint32_t s = (b >> 16) & 0x8000u, e = (b >> 23) & 0xFF, m = b & 0x7FFFFFu;
    if (e == 0xFF)
        return s | 0x7C00u | (m ? 0x200u | (m >> 13) : 0u); /* NaN: quiet, payload kept */
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
/* v_cvt_pknorm_u16_f32: clamp to [0, 1], x 65535, round to nearest even (the first hardware run's
   boundaries are all at .5 and fit this as well as round-half-up). */
static uint32_t alpha_to_mask_pknorm_u16(uint32_t b) {
    struct AlphaValue a = alpha_value_f32(b);
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
static struct AlphaValue alpha_value_raw(uint32_t v, uint32_t fmt, int h) {
    v &= 0xFFFFu;
    if (fmt == 4)
        return alpha_value_half(v);
    if (fmt == 5)
        return (h && v == 0xFFFFu) ? alpha_value_fullv() : alpha_value_ratio(v, 65536);
    if (fmt == 6) {
        int16_t s = (int16_t)v;
        if (s <= 0)
            return alpha_value_zero();
        return alpha_value_ratio((uint64_t)s, h ? 32767u : 32768u);
    }
    return alpha_value_zero();
}
static double alpha_value_value(struct AlphaValue a) {
    return a.k == 0 ? 0.0 : a.k == 1 ? 1.0 : a.k == 2 ? a.d : (double)a.num / (double)a.den;
}
/* floor(a k8) for k8 = 8 k (exact: a double times a power of two, or integer arithmetic). */
static uint32_t alpha_to_mask_floor8(struct AlphaValue a, uint32_t k8) {
    if (a.k == 0)
        return 0;
    if (a.k == 1)
        return k8;
    if (a.k == 2)
        return (uint32_t)(a.d * (double)k8);
    return (uint32_t)((a.num * k8) / a.den);
}
static uint32_t alpha_to_mask_cnt(struct AlphaValue a, uint32_t k, uint32_t off) {
    uint32_t c = (alpha_to_mask_floor8(a, 8 * k) + off) >> 3;
    return c > k ? k : c;
}
static uint32_t alpha_to_mask_fmask(struct AlphaValue a, uint32_t n, uint32_t off, uint32_t dis) {
    if (dis)
        return alpha_to_mask_full(alpha_to_mask_cnt(a, n, off));
    uint32_t m = 0;
    for (uint32_t k = n; k > 1; k >>= 1) {
        uint32_t h = alpha_to_mask_cnt(a, k, off) - alpha_to_mask_cnt(a, k >> 1, off);
        m |= alpha_to_mask_full(h) << (k >> 1);
    }
    return m | (alpha_to_mask_cnt(a, 1, off) ? 1u : 0u);
}
/* The ns-bit alpha-to-mask coverage of alpha a at quad pixel p. */
static uint32_t alpha_to_mask_amask(const struct AlphaToMaskTest* t, struct AlphaValue a,
                                    uint32_t p, int h) {
    uint32_t r = alpha_to_mask_register(t), off = 2 * ((r >> (8 + 2 * p)) & 3) + ((r >> 16) & 1);
    uint32_t lgn = t->alpha_to_mask_num_samples == 0xFF ? alpha_to_mask_log2(t->ns)
                                                        : t->alpha_to_mask_num_samples;
    if (lgn > 4 && !(t->hyp == H_CAP && h)) /* NUM_SAMPLES 5..7 act as 4 (16 samples) */
        lgn = 4;
    uint32_t n = 1u << lgn;
    if (n > t->ns && t->hyp == H_CLAMP && h)
        n = t->ns;
    uint32_t m = alpha_to_mask_fmask(a, n, off, t->eqaa_dis), out = 0;
    for (uint32_t s = 0; s < t->ns; s++)
        out |= ((m >> (s % n)) & 1u) << s;
    return out;
}
static int alpha_to_mask_fmt_alpha(uint32_t f) {
    return f >= 3 && f <= 9; /* 32_AR, FP16, UNORM16, SNORM16, UINT16, SINT16, 32_ABGR */
}
static int alpha_to_mask_exports_mrt0(int v) {
    return v != V_ZA0 && v != V_M1O;
}
/* MRT0's export enable mask (one bit per component, also when compressed). */
static uint32_t alpha_to_mask_mrt0_en(int v) {
    switch (v) {
    case V_NOA:
        return 0x7;
    case V_EN8:
    case V_C8:
        return 0x8;
    case V_EN0:
        return 0x0;
    case V_EN9:
        return 0x9;
    case V_EN1:
        return 0x1;
    case V_C3:
        return 0x3;
    case V_CC:
        return 0xC;
    case V_C4:
        return 0x4;
    default:
        return 0xF;
    }
}
static int alpha_to_mask_compressed(int v) {
    return v == V_F16 || v == V_U16 || v == V_RAW || v == V_C3 || v == V_CC || v == V_C4 ||
           v == V_C8;
}
/* MRT0's alpha is enabled by enable bit 3 or, compressed (VSRC1 = blue, alpha), bit 2. */
static int alpha_to_mask_alpha_enabled(const struct AlphaToMaskTest* t, int h) {
    uint32_t en = alpha_to_mask_mrt0_en(t->ps);
    if (alpha_to_mask_compressed(t->ps) && !(t->hyp == H_COMPR && h))
        return (en >> 2) & 1;
    return (en >> 3) & 1;
}
static int alpha_to_mask_exports_mrtz_alpha(int v) {
    return v == V_ZA || v == V_AZ || v == V_ZAB || v == V_ZA0 || v == V_ALL;
}
/* The exported sample mask as the DB applies it: MASK_EXPORT_NUM_SAMPLES (n) low bits, repeated
   over the samples when n is below the sample count (h0) or the rest cleared (h1, H_MASK_EXPORT).
 */
static uint32_t alpha_to_mask_mask_export(const struct AlphaToMaskTest* t, uint32_t mask, int h) {
    uint32_t lg = alpha_to_mask_log2(t->ns);
    uint32_t ln = t->mask_export_ns == 0xFF ? lg : t->mask_export_ns, n = 1u << ln, out = 0;
    if (n >= t->ns)
        return mask & alpha_to_mask_full(t->ns);
    if (t->hyp == H_MASK_EXPORT && h)
        return mask & alpha_to_mask_full(n);
    for (uint32_t s = 0; s < t->ns; s++)
        out |= ((mask >> (s % n)) & 1u) << s;
    return out;
}
/* Whether a kill variant drops the pixels with B & 1 (hypothesis h). */
static int alpha_to_mask_killed(const struct AlphaToMaskTest* t, int h) {
    if (t->ps == V_KILL_NOVM)
        return t->hyp == H_VM && h;
    if (t->ps == V_KP_RESTORE)
        return t->hyp == H_RESTORE && h;
    if (t->ps == V_KP_TWOEXP)
        return !(t->hyp == H_TWOEXP && h);
    if (t->ps >= V_KP_E64 && t->ps <= V_KP_SAVEEXEC)
        return 1;
    if (t->ps != V_KILL && t->ps != V_KILL2 && t->ps != V_KILL3)
        return 0;
    if (t->no_kill_enable && t->hyp == H_KILLEN)
        return !h;
    return 1;
}
/* Z test of the test draw (z 0.5) against the cleared 1.0 by DB_DEPTH_CONTROL.ZFUNC. */
static int alpha_to_mask_zpass_ok(const struct AlphaToMaskTest* t) {
    static const uint8_t pass[8] = {0, 1, 0, 1, 0, 1, 0, 1}; /* NEVER LESS EQUAL LEQUAL .. */
    return t->nodepth || !t->zfunc || pass[(t->zfunc - 1) & 7];
}
/* MRT0's alpha as exported by the variant. */
static struct AlphaValue alpha_to_mask_alpha0(const struct AlphaToMaskTest* t, uint32_t A, int h) {
    switch (t->ps) {
    case V_F16:
    case V_C3:
    case V_CC:
    case V_C4:
    case V_C8:
        return alpha_value_half(alpha_to_mask_f2h_rtz(A));
    case V_U16:
        return alpha_value_raw(alpha_to_mask_pknorm_u16(A), 5, h);
    case V_RAW:
        return alpha_value_raw(A, alpha_to_mask_colfmt(t) & 0xF, h);
    case V_ZA:
    case V_M1:
        return alpha_value_f32(t->mrt0a);
    default:
        return alpha_value_f32(A);
    }
}
/* The expected sample mask of pixel (x, y) under hypothesis h; -1: no model. */
static int64_t alpha_to_mask_expect(const struct AlphaToMaskTest* t, int x, int y, uint32_t A,
                                    uint32_t B, uint32_t C, int h) {
    if (t->hyp == H_NOMODEL)
        return -1;
    if (!alpha_to_mask_zpass_ok(t))
        return 0;
    uint32_t full = alpha_to_mask_full(t->ns), m = full, col0 = alpha_to_mask_colfmt(t) & 0xF,
             col1 = (alpha_to_mask_colfmt(t) >> 4) & 0xF, cbsm = alpha_to_mask_cbsm(t),
             r = alpha_to_mask_register(t);
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
    int gate0 = alpha_to_mask_exports_mrt0(t->ps) && alpha_to_mask_fmt_alpha(col0) && (cbsm & 8);
    int path0 = gate0 && (r & 1) && !(t->dbsc & ALPHA_TO_MASK_DISABLE_BIT);
    if (t->hyp == H_EN0 && h)
        path0 = gate0;
    int pathz = (t->dbsc & ALPHA_TO_MASK_COV2MASK) && alpha_to_mask_exports_mrtz_alpha(t->ps);
    struct AlphaValue a0 = alpha_to_mask_alpha0(t, A, h), az = alpha_value_zero();
    if (t->ps == V_ZA || t->ps == V_ZA0)
        az = alpha_value_f32(A);
    else if (t->ps == V_AZ)
        az = alpha_value_f32(t->mrtza);
    else if (t->ps == V_ZAB)
        az = alpha_value_f32(B);
    else if (t->ps == V_ALL)
        az = alpha_value_f32(C);
    if (path0 && !alpha_to_mask_alpha_enabled(t, h)) {
        if (t->hyp == H_NOALPHA && !h)
            path0 = 0;
        else
            a0 = alpha_value_zero();
    }
    if (t->hyp == H_NOMRT0 && h && !alpha_to_mask_exports_mrt0(t->ps) &&
        alpha_to_mask_fmt_alpha(col0) && (cbsm & 8) && (r & 1) &&
        !(t->dbsc & ALPHA_TO_MASK_DISABLE_BIT)) {
        path0 = 1;
        a0 = alpha_value_zero();
    }
    if (t->hyp == H_MRT1 && h) {
        path0 = (r & 1) && !(t->dbsc & ALPHA_TO_MASK_DISABLE_BIT) &&
                alpha_to_mask_fmt_alpha(col1) && (cbsm & 0x80);
        a0 = alpha_value_f32(A);
    }
    if (t->ps == V_ITER && path0) {
        uint32_t it = 0;
        for (uint32_t s = 0; s < t->ns; s++) {
            float as = alpha_to_mask_ftof(A) + (float)s * alpha_to_mask_ftof(t->iter_delta);
            struct AlphaValue a = alpha_value_f32(h ? A : alpha_to_mask_fbits(as));
            it |= alpha_to_mask_amask(t, a, p, h) & (1u << s);
        }
        m &= it;
    } else if (path0 && pathz) {
        if (t->hyp == H_COMBINE && h)
            m &= alpha_to_mask_amask(t, alpha_value_value(a0) < alpha_value_value(az) ? a0 : az, p,
                                     h);
        else
            m &= alpha_to_mask_amask(t, a0, p, h) & alpha_to_mask_amask(t, az, p, h);
    } else if (path0) {
        m &= alpha_to_mask_amask(t, a0, p, h);
    } else if (pathz) {
        m &= alpha_to_mask_amask(t, az, p, h);
    }
    if (t->ps == V_MASK)
        m &= alpha_to_mask_mask_export(t, B, h);
    if ((B & 1) && alpha_to_mask_killed(t, h))
        m = 0;
    if (t->ps == V_ALL) {
        m &= alpha_to_mask_mask_export(t, B & 0xFF, h);
        if ((B >> 8) & 1)
            m = 0;
    }
    return (int64_t)(m & alpha_to_mask_aa_pixel(t, (uint32_t)(x & 1) + 2u * (uint32_t)(y & 1)) &
                     full);
}
/* With DEPTH_BEFORE_SHADER (h0) depth and ZPASS see only the coverage, not the shader's kills. */
static int64_t alpha_to_mask_expect_depth(const struct AlphaToMaskTest* t, int x, int y, uint32_t A,
                                          uint32_t B, uint32_t C, int h) {
    if (!(t->dbsc & ALPHA_TO_MASK_DEPTH_BEFORE_SHADER) || (t->hyp == H_DBS && h) ||
        t->hyp == H_NOMODEL)
        return alpha_to_mask_expect(t, x, y, A, B, C, h);
    if (!alpha_to_mask_zpass_ok(t))
        return 0;
    return (int64_t)(alpha_to_mask_aa_pixel(t, (uint32_t)(x & 1) + 2u * (uint32_t)(y & 1)) &
                     alpha_to_mask_full(t->ns));
}
/* An MRT0 export reaches the colour target (with any enable mask): the covered samples change. */
static int alpha_to_mask_colour_written(const struct AlphaToMaskTest* t) {
    return t->cbf != CBF_NONE && !t->cb_disable && (alpha_to_mask_cbtm(t) & 1) &&
           (alpha_to_mask_cbsm(t) & 1) && (alpha_to_mask_colfmt(t) & 0xF) &&
           alpha_to_mask_exports_mrt0(t->ps);
}
/* The variants a crash or hang marks bad: all four MRTZ.a exporters together (one shader
   recompiler path), otherwise the variant alone. */
static uint64_t alpha_to_mask_group(int v) {
    uint64_t z = (1ull << V_ZA) | (1ull << V_AZ) | (1ull << V_ZAB) | (1ull << V_ZA0);
    return (z >> v) & 1 ? z : 1ull << v;
}
static int alpha_to_mask_nh(const struct AlphaToMaskTest* t) {
    return (t->hyp == H_NONE || t->hyp == H_NOMODEL) ? 1 : 2;
}

/* ---- alpha tables ---- */
static uint32_t g_alpha_to_mask_rng;
static uint32_t alpha_to_mask_rand(void) {
    uint32_t x = g_alpha_to_mask_rng ? g_alpha_to_mask_rng : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_alpha_to_mask_rng = x;
    return x;
}
static void alpha_to_mask_seed(uint32_t s) {
    g_alpha_to_mask_rng = s * 2654435761u + 0x6A09E667u;
    for (int i = 0; i < 4; i++)
        alpha_to_mask_rand();
}
/* A random alpha: [0, 1) (half the time), on the 1 / 64 grid +- 1 ulp, or any bit pattern. */
static uint32_t alpha_to_mask_rand_alpha(void) {
    uint32_t r = alpha_to_mask_rand(), k = alpha_to_mask_rand();
    switch (r & 3) {
    case 0:
    case 1:
        return alpha_to_mask_fbits((float)(k >> 8) * (1.0f / 16777216.0f));
    case 2: {
        uint32_t v = alpha_to_mask_fbits((float)(k % 65) / 64.0f);
        int d = (int)((r >> 2) % 3) - 1;
        return (v == 0 && d < 0) ? 0x80000000u : v + (uint32_t)d;
    }
    default:
        return k;
    }
}

/* Quad alphas (q = (x >> 1) + 32 (y >> 1)) and their per-pixel tables. */
static uint32_t g_alpha_to_mask_qa[ALPHA_TO_MASK_QUADS];

static void alpha_to_mask_coarse_alpha(uint32_t* al) {
    static const uint32_t sp[] = {
        0x7FC00000u, 0xFFC00000u, 0x7F800001u, 0x7FBFFFFFu, 0xFFBFFFFFu, 0x7F800000u, 0xFF800000u,
        0x80000000u, 0x00000000u, 0x00000001u, 0x007FFFFFu, 0x00800000u, 0x80000001u, 0x807FFFFFu,
        0xBF800000u, 0xBF000000u, 0xBDCCCCCDu, 0xC0000000u, 0xFF7FFFFFu, 0x3F800000u, 0x3F800001u,
        0x3F7FFFFFu, 0x3F8CCCCDu, 0x3FC00000u, 0x40000000u, 0x40800000u, 0x7F7FFFFFu, 0x2EDBE6FFu,
        0x33800000u, 0x3B800000u, 0x3B808081u, 0x37800080u};
    int n = 0;
    for (; n < (int)(sizeof(sp) / sizeof(sp[0])); n++)
        al[n] = sp[n];
    for (int k = 0; k <= 16 && n + 3 <= ALPHA_TO_MASK_SPECIALS; k++) {
        uint32_t v = alpha_to_mask_fbits((float)k / 16.0f);
        if (k)
            al[n++] = v - 1;
        al[n++] = v;
        al[n++] = v + 1;
    }
    for (int k = 1; k < 32 && n < ALPHA_TO_MASK_SPECIALS; k += 2)
        al[n++] = alpha_to_mask_fbits((float)k / 32.0f);
    for (int k = 1; n < ALPHA_TO_MASK_SPECIALS; k += 2)
        al[n++] = alpha_to_mask_fbits((float)k / 64.0f);
    for (int i = 0; i < ALPHA_TO_MASK_QUADS - ALPHA_TO_MASK_SPECIALS; i++)
        al[ALPHA_TO_MASK_SPECIALS + i] = alpha_to_mask_fbits(
            (float)i / (float)(ALPHA_TO_MASK_QUADS - ALPHA_TO_MASK_SPECIALS - 1));
}
static void alpha_to_mask_expand_quads(uint32_t* A) {
    for (int y = 0; y < ALPHA_TO_MASK_W; y++)
        for (int x = 0; x < ALPHA_TO_MASK_W; x++)
            A[x + ALPHA_TO_MASK_W * y] = g_alpha_to_mask_qa[(x >> 1) + 32 * (y >> 1)];
}
/* The net: strands every 8 pixels (alpha 1.0 on, 0.6 / 0.25 beside), fading with y to 0.45. */
static uint32_t alpha_to_mask_net_alpha(int x, int y) {
    int dx = x & 7, dy = y & 7;
    int d = dx < 8 - dx ? dx : 8 - dx;
    int e = dy < 8 - dy ? dy : 8 - dy;
    if (e < d)
        d = e;
    float base = d == 0 ? 1.0f : d == 1 ? 0.6f : d == 2 ? 0.25f : 0.0f;
    float fade = 1.0f - 0.55f * (float)y / 63.0f;
    return alpha_to_mask_fbits(base * fade);
}
/* Fill A and B for pass `pass` of test t (B from the test's seed and the pass). */
static void alpha_to_mask_fill(struct AlphaToMaskContext* c, const struct AlphaToMaskTest* t,
                               int pass) {
    uint32_t* A = c->alpha;
    uint32_t* B = c->alpha + ALPHA_TO_MASK_PX;
    alpha_to_mask_seed(t->seed * 977u + (uint32_t)pass * 131u + 7u);
    switch (t->kind) {
    case AK_COARSE:
        if (pass == 0)
            alpha_to_mask_coarse_alpha(g_alpha_to_mask_qa);
        alpha_to_mask_expand_quads(A);
        break;
    case AK_UNIFORM:
        for (int i = 0; i < ALPHA_TO_MASK_PX; i++)
            A[i] = t->uni_alpha;
        break;
    case AK_RAMP:
        for (int q = 0; q < ALPHA_TO_MASK_QUADS; q++)
            g_alpha_to_mask_qa[q] = alpha_to_mask_fbits((float)q / 1023.0f);
        alpha_to_mask_expand_quads(A);
        break;
    case AK_NET:
        for (int y = 0; y < ALPHA_TO_MASK_W; y++)
            for (int x = 0; x < ALPHA_TO_MASK_W; x++)
                A[x + ALPHA_TO_MASK_W * y] = alpha_to_mask_net_alpha(x, y);
        break;
    case AK_RANDOM:
        for (int i = 0; i < ALPHA_TO_MASK_PX; i++)
            A[i] = alpha_to_mask_rand_alpha();
        break;
    case AK_SWEEP16:
        for (int q = 0; q < ALPHA_TO_MASK_QUADS; q++)
            g_alpha_to_mask_qa[q] = (uint32_t)pass * ALPHA_TO_MASK_QUADS + (uint32_t)q;
        alpha_to_mask_expand_quads(A);
        break;
    case AK_BOUND: { /* every level boundary F / 8n, its neighbour floats, repeated */
        uint32_t lg = t->alpha_to_mask_num_samples == 0xFF ? alpha_to_mask_log2(t->ns)
                                                           : t->alpha_to_mask_num_samples,
                 n = 1u << lg, k = 0;
        uint32_t list[3 * 129 + 8];
        for (uint32_t f = 0; f <= 8 * n; f++) {
            uint32_t v = alpha_to_mask_fbits((float)f / (float)(8 * n));
            if (f)
                list[k++] = v - 1;
            list[k++] = v;
            list[k++] = f ? v + 1 : 0x80000000u;
        }
        list[k++] = 0x40000000u;
        list[k++] = 0x7FC00000u;
        list[k++] = 0xBF800000u;
        list[k++] = 0x7F800000u;
        for (int q = 0; q < ALPHA_TO_MASK_QUADS; q++)
            g_alpha_to_mask_qa[q] = list[(uint32_t)q % k];
        alpha_to_mask_expand_quads(A);
        break;
    }
    case AK_TIES: { /* v_cvt_pknorm halfway points (k + 0.5) / max and the floats beside them */
        int u16 = t->ps == V_PKNORM_U16;
        uint32_t max = u16 ? 65535u : 32767u, groups = u16 ? 16u : 8u;
        uint32_t* C2 = c->alpha + 2 * ALPHA_TO_MASK_PX;
        int d = pass / (int)groups - 1;
        for (int i = 0; i < ALPHA_TO_MASK_PX; i++) {
            uint32_t k = ((uint32_t)pass % groups) * ALPHA_TO_MASK_PX + (uint32_t)i;
            if (k > max - 1)
                k = max - 1;
            uint32_t v = alpha_to_mask_fbits((float)(((double)k + 0.5) / (double)max));
            A[i] = v + (uint32_t)d;
            C2[i] =
                u16 ? alpha_to_mask_fbits((float)(((double)(max - 1 - k) + 0.5) / (double)max)) +
                          (uint32_t)d
                    : (v + (uint32_t)d) | 0x80000000u;
            B[i] = ALPHA_TO_MASK_NOT_SHADED;
        }
        return;
    }
    case AK_TEX: /* t_tex stores the sampled alpha to B; A gets it after the pass */
        for (int i = 0; i < ALPHA_TO_MASK_PX; i++)
            A[i] = B[i] = ALPHA_TO_MASK_NOT_SHADED;
        return;
    }
    uint32_t* C = c->alpha + 2 * ALPHA_TO_MASK_PX;
    for (int i = 0; i < ALPHA_TO_MASK_PX; i++) {
        uint32_t r = alpha_to_mask_rand();
        B[i] = t->bmode == BM_ALPHA  ? alpha_to_mask_rand_alpha()
               : t->bmode == BM_MASK ? (r & 0xFFu)
               : t->bmode == BM_KILL ? (r & 1u)
               : t->bmode == BM_ALL  ? ((r & 0xFFu) | (((r >> 8) & 3u) == 0 ? 0x100u : 0u))
                                     : 0u;
        C[i] = (t->bmode == BM_ALL || t->bmode == BM_C) ? alpha_to_mask_rand_alpha() : 0u;
        if (t->bmode == BM_C)
            B[i] = ALPHA_TO_MASK_NOT_SHADED;
    }
}

/* ---- results of one pass ---- */
static uint8_t g_alpha_to_mask_cm[ALPHA_TO_MASK_PX], g_alpha_to_mask_dm[ALPHA_TO_MASK_PX],
    g_alpha_to_mask_an[ALPHA_TO_MASK_PX];
static uint32_t g_alpha_to_mask_nanom, g_alpha_to_mask_unwritten;
static uint32_t g_alpha_to_mask_anom[8][5]; /* x, y, s | plane << 8, pre raw, raw */

/* A sample's colour counts as written when its readback differs from before the test draw; its
   depth when it reads 0.5 (the test z) after 1.0 (the clear). Any other depth is unclassified. */
static void alpha_to_mask_classify(struct AlphaToMaskContext* c, const struct AlphaToMaskTest* t) {
    const volatile uint32_t* o = c->out;
    const volatile uint32_t* pre = c->out_pre;
    g_alpha_to_mask_nanom = 0;
    g_alpha_to_mask_unwritten = 0;
    for (int px = 0; px < ALPHA_TO_MASK_PX; px++) {
        uint32_t cm = 0, dm = 0, an = 0;
        for (uint32_t s = 0; s < t->ns; s++) {
            uint32_t i = (uint32_t)px * 8 + s;
            uint32_t cr = o[i], dr = o[ALPHA_TO_MASK_OUT_DEPTH + i], cp = pre[i],
                     dp = pre[ALPHA_TO_MASK_OUT_DEPTH + i];
            if (cr == ALPHA_TO_MASK_UNWRITTEN || dr == ALPHA_TO_MASK_UNWRITTEN ||
                cp == ALPHA_TO_MASK_UNWRITTEN || dp == ALPHA_TO_MASK_UNWRITTEN)
                g_alpha_to_mask_unwritten++;
            if (cr != cp)
                cm |= 1u << s;
            uint32_t bad = 0, raw = 0, praw = 0;
            if (dp != 0x3F800000u) {
                bad = 3, raw = dr, praw = dp;
            } else if (dr == 0x3F000000u) {
                dm |= 1u << s;
            } else if (dr != dp) {
                bad = 2, raw = dr, praw = dp;
            }
            if (bad) {
                an |= bad == 3 ? 4u : 2u;
                if (g_alpha_to_mask_nanom < 8) {
                    uint32_t* a = g_alpha_to_mask_anom[g_alpha_to_mask_nanom];
                    a[0] = (uint32_t)(px & 63), a[1] = (uint32_t)(px >> 6), a[2] = s | (bad << 8),
                    a[3] = praw, a[4] = raw;
                }
                g_alpha_to_mask_nanom++;
            }
        }
        g_alpha_to_mask_cm[px] = (uint8_t)cm;
        g_alpha_to_mask_dm[px] = (uint8_t)dm;
        g_alpha_to_mask_an[px] = (uint8_t)an;
    }
}

/* Totals of a test over its passes. */
struct AlphaToMaskTotals {
    uint32_t passes, retries, px, dmis[2], cmis[2], dchk, cchk, unclass, unwritten, cdiff;
    uint32_t zp_bad[2], zp_rb_bad, iso_bad[2], iso_rb_bad, zp_none, nex, not_shaded;
    uint32_t imis[2], ichk, nex_instr;
    uint32_t ex_instr[6][7]; /* x, y, A, C, GPU, h0, h1 */
    uint32_t inconsistent, repeats, nondet, nnondet_ex;
    uint32_t nondet_ex[4][4]; /* plane (0 after, 1 before), dword, first, second */
    uint64_t pass_us;
    uint32_t ex[6][9]; /* x, y, A, B, C, measured depth, measured colour, h0, h1 */
};
static struct AlphaToMaskTotals g_tot;
static uint32_t g_model_pop[2], g_model_s[2][16]; /* this pass: model popcounts */
static uint8_t g_model0[ALPHA_TO_MASK_PX];        /* this pass: h0 mask per pixel */

/* The conversion instructions (t_pk*): op(A, C) as stored, -1 for the other variants. */
static uint32_t alpha_to_mask_round(double v, int away) {
    uint32_t f = (uint32_t)v;
    double fr = v - (double)f;
    if (fr > 0.5 || (fr == 0.5 && (away || (f & 1))))
        f++;
    return f;
}
/* mode 0: f32 product, half to even; 1: exact product, half to even; 2: f32 product, half away */
static uint32_t alpha_to_mask_pknorm(uint32_t bits, int sgn, int mode) {
    float x = alpha_to_mask_ftof(bits);
    if (x != x)
        return 0;
    float lo = sgn ? -1.0f : 0.0f;
    x = x < lo ? lo : x > 1.0f ? 1.0f : x;
    float scale = sgn ? 32767.0f : 65535.0f;
    double v = mode == 1 ? (double)x * (double)scale : (double)(x * scale);
    int away = mode == 2;
    uint32_t r =
        v < 0 ? (uint32_t)-(int32_t)alpha_to_mask_round(-v, away) : alpha_to_mask_round(v, away);
    return r & 0xFFFFu;
}
static uint32_t alpha_to_mask_pkint(uint32_t v, int sgn, int truncate) {
    if (truncate)
        return v & 0xFFFFu;
    if (!sgn)
        return v > 0xFFFFu ? 0xFFFFu : v;
    int32_t s = (int32_t)v;
    s = s < -32768 ? -32768 : s > 32767 ? 32767 : s;
    return (uint32_t)s & 0xFFFFu;
}
static int64_t alpha_to_mask_instr_expect(const struct AlphaToMaskTest* t, uint32_t A, uint32_t C,
                                          int h, int px) {
    uint32_t p = (uint32_t)(px & 1) + 2u * (uint32_t)((px >> 6) & 1);
    uint32_t aa = alpha_to_mask_aa_pixel(t, p) & alpha_to_mask_full(t->ns);
    if (t->ps == V_MARK)
        return (aa || (t->hyp == H_AAPS && h)) ? 1 : ALPHA_TO_MASK_NOT_SHADED;
    if (t->ps == V_COV) {
        if (!aa && !(t->hyp == H_AAPS && h)) /* the pixel shader does not run */
            return ALPHA_TO_MASK_NOT_SHADED;
        return (t->hyp == H_COVIN && h) ? alpha_to_mask_full(t->ns) : aa;
    }
    int mode = !h ? 1 : t->hyp == H_ROUND ? 0 : t->hyp == H_ROUND2 ? 2 : 1;
    switch (t->ps) {
    case V_PKNORM_U16:
        return alpha_to_mask_pknorm(A, 0, mode) | (alpha_to_mask_pknorm(C, 0, mode) << 16);
    case V_PKNORM_I16:
        return alpha_to_mask_pknorm(A, 1, mode) | (alpha_to_mask_pknorm(C, 1, mode) << 16);
    case V_PKRTZ:
        return alpha_to_mask_f2h_rtz(A) | (alpha_to_mask_f2h_rtz(C) << 16);
    case V_PK_U16:
        return alpha_to_mask_pkint(A, 0, h) | (alpha_to_mask_pkint(C, 0, h) << 16);
    case V_PK_I16:
        return alpha_to_mask_pkint(A, 1, h) | (alpha_to_mask_pkint(C, 1, h) << 16);
    default:
        return -1;
    }
}
static void alpha_to_mask_instr_check(struct AlphaToMaskContext* c,
                                      const struct AlphaToMaskTest* t) {
    const uint32_t* A = c->alpha;
    const volatile uint32_t* B = c->alpha + ALPHA_TO_MASK_PX;
    const uint32_t* C = c->alpha + 2 * ALPHA_TO_MASK_PX;
    int nh = alpha_to_mask_nh(t);
    if (alpha_to_mask_instr_expect(t, 0, 0, 0, 0) < 0 && t->ps != V_MARK)
        return;
    for (int px = 0; px < ALPHA_TO_MASK_PX; px++) {
        int64_t e[2] = {0, 0};
        int bad0 = 0;
        g_tot.ichk++;
        for (int h = 0; h < nh; h++) {
            e[h] = alpha_to_mask_instr_expect(t, A[px], C[px], h, px);
            if ((uint32_t)e[h] != B[px]) {
                g_tot.imis[h]++;
                bad0 |= !h;
            }
        }
        if (bad0 && g_tot.nex_instr < 6) {
            uint32_t* r = g_tot.ex_instr[g_tot.nex_instr++];
            r[0] = (uint32_t)(px & 63), r[1] = (uint32_t)(px >> 6), r[2] = A[px], r[3] = C[px];
            r[4] = B[px], r[5] = (uint32_t)e[0], r[6] = nh > 1 ? (uint32_t)e[1] : 0;
        }
    }
}

/* Within a pass, every pixel with the same inputs (A, B as input, C, quad pixel, inside the window
   offset strip or not) must give the same masks and the same stored result. */
#define ALPHA_TO_MASK_CONS_SLOTS 16384u
static uint32_t g_cons_key[ALPHA_TO_MASK_CONS_SLOTS][4];
static uint64_t g_cons_m[ALPHA_TO_MASK_CONS_SLOTS];
static uint8_t g_cons_used[ALPHA_TO_MASK_CONS_SLOTS];

static int alpha_to_mask_stores_result(int v) {
    return v == V_TEX || v == V_MARK || v == V_COV || (v >= V_PKNORM_U16 && v <= V_PK_I16);
}
static void alpha_to_mask_consistency(const struct AlphaToMaskTest* t, const uint32_t* A,
                                      const uint32_t* B, const uint32_t* C) {
    int b_in =
        t->bmode == BM_ALPHA || t->bmode == BM_MASK || t->bmode == BM_KILL || t->bmode == BM_ALL;
    int result = alpha_to_mask_stores_result(t->ps);
    my_memset(g_cons_used, 0, sizeof(g_cons_used));
    for (int px = 0; px < ALPHA_TO_MASK_PX; px++) {
        int x = px & 63, y = px >> 6;
        uint32_t p = (uint32_t)(x & 1) + 2u * (uint32_t)(y & 1);
        uint32_t strip = (x < (int)(t->wofs & 0xFFFF) || y < (int)(t->wofs >> 16)) ? 1u : 0u;
        uint32_t key[4] = {A[px], b_in ? B[px] : 0u, C[px], p | (strip << 4)};
        uint32_t h = (key[0] * 2654435761u ^ key[1] * 0x85EBCA6Bu ^ key[2] * 0xC2B2AE35u ^
                      key[3] * 0x9E3779B9u) &
                     (ALPHA_TO_MASK_CONS_SLOTS - 1);
        uint64_t m = (uint64_t)g_alpha_to_mask_dm[px] | ((uint64_t)g_alpha_to_mask_cm[px] << 8) |
                     (result ? ((uint64_t)B[px] << 32) : 0u);
        while (g_cons_used[h] && (g_cons_key[h][0] != key[0] || g_cons_key[h][1] != key[1] ||
                                  g_cons_key[h][2] != key[2] || g_cons_key[h][3] != key[3]))
            h = (h + 1) & (ALPHA_TO_MASK_CONS_SLOTS - 1);
        if (!g_cons_used[h]) {
            g_cons_used[h] = 1;
            for (int k = 0; k < 4; k++)
                g_cons_key[h][k] = key[k];
            g_cons_m[h] = m;
        } else {
            g_tot.repeats++;
            if (g_cons_m[h] != m)
                g_tot.inconsistent++;
        }
    }
}

static void alpha_to_mask_check(struct AlphaToMaskContext* c, const struct AlphaToMaskTest* t) {
    const uint32_t* A = c->alpha;
    const uint32_t* B = c->alpha + ALPHA_TO_MASK_PX;
    const uint32_t* C = c->alpha + 2 * ALPHA_TO_MASK_PX;
    int nh = alpha_to_mask_nh(t), rb = alpha_to_mask_has_readback(t), dchk = rb && !t->nodepth,
        cw = alpha_to_mask_colour_written(t);
    g_model_pop[0] = g_model_pop[1] = 0;
    my_memset(g_model_s, 0, sizeof(g_model_s));
    for (int px = 0; px < ALPHA_TO_MASK_PX; px++) {
        int x = px & 63, y = px >> 6;
        int64_t e[2] = {0, 0}, ed[2] = {0, 0};
        for (int h = 0; h < nh; h++) {
            e[h] = alpha_to_mask_expect(t, x, y, A[px], B[px], C[px], h);
            ed[h] = alpha_to_mask_expect_depth(t, x, y, A[px], B[px], C[px], h);
            if (ed[h] < 0)
                continue;
            g_model_pop[h] += (uint32_t)alpha_to_mask_pop((uint32_t)ed[h]);
            for (uint32_t s = 0; s < t->ns; s++)
                g_model_s[h][s] += (uint32_t)((ed[h] >> s) & 1);
        }
        g_model0[px] = (uint8_t)(e[0] < 0 ? 0 : e[0]);
        if (!rb)
            continue;
        if (g_alpha_to_mask_an[px])
            g_tot.unclass++;
        if (cw && dchk && g_alpha_to_mask_cm[px] != g_alpha_to_mask_dm[px])
            g_tot.cdiff++;
        if (e[0] < 0)
            continue;
        g_tot.px++;
        int bad0 = 0;
        for (int h = 0; h < nh; h++) {
            uint8_t ec = cw ? (uint8_t)e[h] : 0u;
            if (t->ps == V_KILL_NOVM && !h && (B[px] & 1))
                ec = 0;
            if (dchk && g_alpha_to_mask_dm[px] != (uint8_t)ed[h]) {
                g_tot.dmis[h]++;
                if (!h)
                    bad0 = 1;
            }
            if (g_alpha_to_mask_cm[px] != ec) {
                g_tot.cmis[h]++;
                if (!h)
                    bad0 = 1;
            }
        }
        if (dchk)
            g_tot.dchk++;
        g_tot.cchk++;
        if (bad0 && g_tot.nex < 6) {
            uint32_t* r = g_tot.ex[g_tot.nex++];
            r[0] = (uint32_t)x, r[1] = (uint32_t)y, r[2] = A[px], r[3] = B[px], r[4] = C[px];
            r[5] = g_alpha_to_mask_dm[px], r[6] = g_alpha_to_mask_cm[px], r[7] = (uint32_t)e[0];
            r[8] = nh > 1 ? (uint32_t)e[1] : 0;
        }
    }
    if (rb)
        alpha_to_mask_consistency(t, A, B, C);
}

/* ZPASS_DONE: the sum over RBs whose begin and end have bit 63; -1 when none. */
static int64_t alpha_to_mask_zp_sum(struct AlphaToMaskContext* c, uint32_t blk, uint32_t* vmask) {
    const volatile uint32_t* zb = c->zp + blk * (ALPHA_TO_MASK_ZP_STRIDE / 4);
    uint64_t sum = 0;
    int valid = 0;
    *vmask = 0;
    for (int rb = 0; rb < ALPHA_TO_MASK_ZP_RBS; rb++) {
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
static int alpha_to_mask_family_size(const struct AlphaToMaskTest* t);
static void alpha_to_mask_report_header(const struct AlphaToMaskTest* t) {
    alpha_to_mask_s("### ");
    alpha_to_mask_s(t->id);
    alpha_to_mask_s(": ");
    alpha_to_mask_s(t->what);
    alpha_to_mask_s("\n");
    if (alpha_to_mask_family_size(t))
        return;
    alpha_to_mask_u(t->ns);
    alpha_to_mask_s(" samples, ");
    alpha_to_mask_s(k_alpha_to_mask_vname[t->ps]);
    alpha_to_mask_s(", colour ");
    alpha_to_mask_s(k_alpha_to_mask_cbf[t->cbf].name);
    alpha_to_mask_s(t->nodepth || !alpha_to_mask_has_readback(t) ? ", no depth target"
                                                                 : ", depth Z_32_FLOAT");
    alpha_to_mask_s(", DB_ALPHA_TO_MASK ");
    alpha_to_mask_x(alpha_to_mask_register(t), 8);
    if (t->hyp != H_NONE) {
        alpha_to_mask_s(", hypotheses h0 ");
        alpha_to_mask_s(k_alpha_to_mask_hname[t->hyp][0]);
        if (alpha_to_mask_nh(t) > 1) {
            alpha_to_mask_s(" / h1 ");
            alpha_to_mask_s(k_alpha_to_mask_hname[t->hyp][1]);
        }
    }
    alpha_to_mask_s("\n");
}

static void alpha_to_mask_report_regs(struct AlphaToMaskContext* c, int regread, int full) {
    if (!full) {
        alpha_to_mask_s("registers:");
        for (int i = 0; i < ALPHA_TO_MASK_NRR; i++) {
            uint32_t r = ((volatile uint32_t*)c->regs)[i];
            alpha_to_mask_s(" ");
            alpha_to_mask_s(k_alpha_to_mask_rr[i].name);
            alpha_to_mask_s("=");
            alpha_to_mask_x(g_alpha_to_mask_wtest[i], 8);
            if (regread && !k_alpha_to_mask_rr[i].unreliable && r != g_alpha_to_mask_wtest[i]) {
                alpha_to_mask_s(r == 0xDEADBEEFu ? "(not read)" : "(read back ");
                if (r != 0xDEADBEEFu) {
                    alpha_to_mask_x(r, 8);
                    alpha_to_mask_s(")");
                }
            }
        }
        alpha_to_mask_s("\n");
        return;
    }
    alpha_to_mask_s("\n| register | written | read back (COPY_DATA) |\n|---|---|---|\n");
    for (int i = 0; i < ALPHA_TO_MASK_NRR; i++) {
        alpha_to_mask_s("| ");
        alpha_to_mask_s(k_alpha_to_mask_rr[i].name);
        alpha_to_mask_s(" | ");
        alpha_to_mask_x(g_alpha_to_mask_wtest[i], 8);
        alpha_to_mask_s(" | ");
        if (!regread) {
            alpha_to_mask_s("not read");
        } else {
            uint32_t r = ((volatile uint32_t*)c->regs)[i];
            alpha_to_mask_x(r, 8);
            if (r == 0xDEADBEEFu)
                alpha_to_mask_s(" (not written)");
            else if (r != g_alpha_to_mask_wtest[i])
                alpha_to_mask_s(k_alpha_to_mask_rr[i].unreliable ? " differs (unreliable readback)"
                                                                 : " DIFFERS");
        }
        alpha_to_mask_s(" |\n");
    }
}

/* Every pass: the ZPASS_DONE counts against the model (h0, h1) and against the readback (depth;
   colour without a depth target). */
static void alpha_to_mask_zpass_check(struct AlphaToMaskContext* c,
                                      const struct AlphaToMaskTest* t) {
    int nh = alpha_to_mask_nh(t), rb = alpha_to_mask_has_readback(t);
    int plane = !rb ? 0 : !t->nodepth ? 1 : alpha_to_mask_colour_written(t) ? 2 : 0;
    for (uint32_t blk = 0; blk <= t->ns; blk++) {
        uint32_t vmask, rbc = 0;
        int64_t sum = alpha_to_mask_zp_sum(c, blk, &vmask);
        if (sum < 0) {
            if (!blk)
                g_tot.zp_none++;
            continue;
        }
        for (int h = 0; h < nh && t->hyp != H_NOMODEL; h++) {
            uint32_t m = blk ? g_model_s[h][blk - 1] : g_model_pop[h];
            if ((uint64_t)sum != m) {
                if (blk)
                    g_tot.iso_bad[h]++;
                else
                    g_tot.zp_bad[h]++;
            }
        }
        if (!plane)
            continue;
        for (int px = 0; px < ALPHA_TO_MASK_PX; px++) {
            uint32_t m = plane == 1 ? g_alpha_to_mask_dm[px] : g_alpha_to_mask_cm[px];
            rbc += blk ? (m >> (blk - 1)) & 1u : (uint32_t)alpha_to_mask_pop(m);
        }
        if ((uint64_t)sum != rbc) {
            if (blk)
                g_tot.iso_rb_bad++;
            else
                g_tot.zp_rb_bad++;
        }
    }
}

static void alpha_to_mask_report_zpass(struct AlphaToMaskContext* c,
                                       const struct AlphaToMaskTest* t) {
    int nh = alpha_to_mask_nh(t);
    uint64_t pop = 0, popc = 0;
    uint32_t per_s[16] = {0}, per_sc[16] = {0};
    if (alpha_to_mask_has_readback(t))
        for (int px = 0; px < ALPHA_TO_MASK_PX; px++) {
            pop += (uint64_t)alpha_to_mask_pop(g_alpha_to_mask_dm[px]);
            popc += (uint64_t)alpha_to_mask_pop(g_alpha_to_mask_cm[px]);
            for (uint32_t s = 0; s < t->ns; s++) {
                per_s[s] += (g_alpha_to_mask_dm[px] >> s) & 1;
                per_sc[s] += (g_alpha_to_mask_cm[px] >> s) & 1;
            }
        }
    alpha_to_mask_s(
        "\nZPASS_DONE (RBs with bit 63 on both; model h0 / h1; readback depth / colour):\n\n| "
        "draw | valid RBs | samples passed | model h0 | model h1 | readback depth | readback "
        "colour |\n|---|---|---|---|---|---|---|\n");
    for (uint32_t blk = 0; blk <= t->ns; blk++) {
        uint32_t vmask;
        int64_t sum = alpha_to_mask_zp_sum(c, blk, &vmask);
        uint32_t m0 = blk ? g_model_s[0][blk - 1] : g_model_pop[0];
        uint32_t m1 = blk ? g_model_s[1][blk - 1] : g_model_pop[1];
        alpha_to_mask_s("| ");
        if (blk == 0) {
            alpha_to_mask_s("test draw");
        } else {
            alpha_to_mask_s("AA_MASK 1 << ");
            alpha_to_mask_u(blk - 1);
        }
        alpha_to_mask_s(" | ");
        alpha_to_mask_u((unsigned long long)alpha_to_mask_pop(vmask));
        alpha_to_mask_s(" (");
        alpha_to_mask_x(vmask, 4);
        alpha_to_mask_s(") | ");
        if (sum < 0)
            alpha_to_mask_s("none");
        else
            alpha_to_mask_u((unsigned long long)sum);
        alpha_to_mask_s(" | ");
        if (t->hyp == H_NOMODEL)
            alpha_to_mask_s("-");
        else
            alpha_to_mask_u(m0);
        alpha_to_mask_s(" | ");
        if (nh > 1)
            alpha_to_mask_u(m1);
        else
            alpha_to_mask_s("-");
        alpha_to_mask_s(" | ");
        if (alpha_to_mask_has_readback(t)) {
            alpha_to_mask_u(blk == 0 ? pop : per_s[blk - 1]);
            alpha_to_mask_s(" | ");
            alpha_to_mask_u(blk == 0 ? popc : per_sc[blk - 1]);
        } else {
            alpha_to_mask_s("- | -");
        }
        alpha_to_mask_s(" |\n");
    }
}

static void alpha_to_mask_masks(int px, int cmp) {
    if (cmp)
        alpha_to_mask_x(g_alpha_to_mask_cm[px], 2);
    else
        alpha_to_mask_s("--");
    alpha_to_mask_s("/");
    alpha_to_mask_x(g_alpha_to_mask_dm[px], 2);
    if (g_alpha_to_mask_an[px])
        alpha_to_mask_s(" !");
}
static int alpha_to_mask_qpx(int q, int p) {
    return (2 * (q & 31) + (p & 1)) + ALPHA_TO_MASK_W * (2 * (q >> 5) + (p >> 1));
}
static uint32_t alpha_to_mask_key(int px) {
    return (uint32_t)g_alpha_to_mask_cm[px] | ((uint32_t)g_alpha_to_mask_dm[px] << 8) |
           ((uint32_t)g_alpha_to_mask_an[px] << 16);
}

/* The quad table (coarse kinds): masks per quad pixel and the model's h0, run-length over the
   linear part. */
static void alpha_to_mask_report_quads(const struct AlphaToMaskTest* t) {
    int cmp = k_alpha_to_mask_cbf[t->cbf].cmp;
    alpha_to_mask_s(
        "\ncolour / depth mask per quad pixel (p0 = x even y even, p1 = x odd, p2 = y odd, p3 = "
        "both), then the model (h0); ! = unclassified sample\n\n| slots | alpha | p0 | p1 | p2 | "
        "p3 | model p0..p3 |\n|---|---|---|---|---|---|---|\n");
    for (int q = 0; q < ALPHA_TO_MASK_QUADS;) {
        int e = q + 1;
        if (q >= ALPHA_TO_MASK_SPECIALS)
            while (e < ALPHA_TO_MASK_QUADS) {
                int same = 1;
                for (int p = 0; p < 4; p++)
                    same &= alpha_to_mask_key(alpha_to_mask_qpx(e, p)) ==
                                alpha_to_mask_key(alpha_to_mask_qpx(q, p)) &&
                            g_model0[alpha_to_mask_qpx(e, p)] == g_model0[alpha_to_mask_qpx(q, p)];
                if (!same)
                    break;
                e++;
            }
        alpha_to_mask_s("| ");
        alpha_to_mask_u((unsigned long long)q);
        if (e - 1 > q) {
            alpha_to_mask_s("..");
            alpha_to_mask_u((unsigned long long)(e - 1));
        }
        alpha_to_mask_s(" | ");
        alpha_to_mask_f(g_alpha_to_mask_qa[q]);
        if (e - 1 > q) {
            alpha_to_mask_s(" .. ");
            alpha_to_mask_f(g_alpha_to_mask_qa[e - 1]);
        }
        for (int p = 0; p < 4; p++) {
            alpha_to_mask_s(" | ");
            alpha_to_mask_masks(alpha_to_mask_qpx(q, p), cmp);
        }
        alpha_to_mask_s(" |");
        for (int p = 0; p < 4; p++) {
            alpha_to_mask_s(" ");
            alpha_to_mask_x(g_model0[alpha_to_mask_qpx(q, p)], 2);
        }
        alpha_to_mask_s(" |\n");
        q = e;
    }
}

static void alpha_to_mask_report_uniform(const struct AlphaToMaskTest* t) {
    int cmp = k_alpha_to_mask_cbf[t->cbf].cmp;
    alpha_to_mask_s("\nuniform alpha ");
    alpha_to_mask_f(t->uni_alpha);
    alpha_to_mask_s(
        ": per quad pixel p, the masks of quad 0 (model h0), and the quads that differ\n\n");
    for (int p = 0; p < 4; p++) {
        int px0 = alpha_to_mask_qpx(0, p), nd = 0;
        alpha_to_mask_s("p");
        alpha_to_mask_u((unsigned long long)p);
        alpha_to_mask_s(": ");
        alpha_to_mask_masks(px0, cmp);
        alpha_to_mask_s(" (model ");
        alpha_to_mask_x(g_model0[px0], 2);
        alpha_to_mask_s(")");
        for (int q = 1; q < ALPHA_TO_MASK_QUADS; q++)
            if (alpha_to_mask_key(alpha_to_mask_qpx(q, p)) != alpha_to_mask_key(px0)) {
                if (nd < 8) {
                    alpha_to_mask_s(", q");
                    alpha_to_mask_u((unsigned long long)q);
                    alpha_to_mask_s(" ");
                    alpha_to_mask_masks(alpha_to_mask_qpx(q, p), cmp);
                }
                nd++;
            }
        alpha_to_mask_s("; quads differing: ");
        alpha_to_mask_u((unsigned long long)nd);
        alpha_to_mask_s("\n");
    }
}

static void alpha_to_mask_report_totals(const struct AlphaToMaskTest* t) {
    int nh = alpha_to_mask_nh(t);
    alpha_to_mask_s("\npasses ");
    alpha_to_mask_u(g_tot.passes);
    alpha_to_mask_s(", repeated for unwritten readback ");
    alpha_to_mask_u(g_tot.retries);
    alpha_to_mask_s(", unwritten samples left ");
    alpha_to_mask_u(g_tot.unwritten);
    alpha_to_mask_s(", GPU time ");
    alpha_to_mask_u(g_tot.pass_us);
    alpha_to_mask_s(" us\ncross-checks: pixels with an unclassified depth sample ");
    alpha_to_mask_u(g_tot.unclass);
    alpha_to_mask_s(", colour != depth ");
    alpha_to_mask_u(g_tot.cdiff);
    alpha_to_mask_s(", ZPASS_DONE != readback (test draw / isolation draws) ");
    alpha_to_mask_u(g_tot.zp_rb_bad);
    alpha_to_mask_s(" / ");
    alpha_to_mask_u(g_tot.iso_rb_bad);
    alpha_to_mask_s(", same alpha and quad pixel with different masks ");
    alpha_to_mask_u(g_tot.inconsistent);
    alpha_to_mask_s(" of ");
    alpha_to_mask_u(g_tot.repeats);
    alpha_to_mask_s(", repeated pass differing ");
    alpha_to_mask_u(g_tot.nondet);
    alpha_to_mask_s(", draws without any count ");
    alpha_to_mask_u(g_tot.zp_none);
    if (t->ps == V_TEX) {
        alpha_to_mask_s(", pixels t_tex did not shade ");
        alpha_to_mask_u(g_tot.not_shaded);
    }
    alpha_to_mask_s("\n");
    if (t->hyp == H_NOMODEL) {
        alpha_to_mask_s("model: none for this export format (the runs below are the result)\n");
    } else {
        for (int h = 0; h < nh; h++) {
            alpha_to_mask_s("model h");
            alpha_to_mask_u((unsigned long long)h);
            alpha_to_mask_s(" (");
            alpha_to_mask_s(k_alpha_to_mask_hname[t->hyp][h]);
            alpha_to_mask_s("): depth mismatches ");
            alpha_to_mask_u(g_tot.dmis[h]);
            alpha_to_mask_s(" of ");
            alpha_to_mask_u(g_tot.dchk);
            alpha_to_mask_s(" px, colour mismatches ");
            alpha_to_mask_u(g_tot.cmis[h]);
            alpha_to_mask_s(" of ");
            alpha_to_mask_u(g_tot.cchk);
            if (g_tot.ichk) {
                alpha_to_mask_s(" px, instruction results differing ");
                alpha_to_mask_u(g_tot.imis[h]);
                alpha_to_mask_s(" of ");
                alpha_to_mask_u(g_tot.ichk);
            }
            alpha_to_mask_s(" px, ZPASS_DONE test draws / isolation draws differing ");
            alpha_to_mask_u(g_tot.zp_bad[h]);
            alpha_to_mask_s(" / ");
            alpha_to_mask_u(g_tot.iso_bad[h]);
            alpha_to_mask_s("\n");
        }
    }
    for (uint32_t i = 0; i < g_tot.nex; i++) {
        uint32_t* r = g_tot.ex[i];
        alpha_to_mask_s("  mismatch x ");
        alpha_to_mask_u(r[0]);
        alpha_to_mask_s(" y ");
        alpha_to_mask_u(r[1]);
        alpha_to_mask_s(" A ");
        alpha_to_mask_f(r[2]);
        alpha_to_mask_s(" B ");
        alpha_to_mask_x(r[3], 8);
        alpha_to_mask_s(" C ");
        alpha_to_mask_x(r[4], 8);
        alpha_to_mask_s(": depth ");
        alpha_to_mask_x(r[5], 4);
        alpha_to_mask_s(" colour ");
        alpha_to_mask_x(r[6], 4);
        alpha_to_mask_s(", model h0 ");
        alpha_to_mask_x(r[7], 4);
        if (nh > 1) {
            alpha_to_mask_s(" h1 ");
            alpha_to_mask_x(r[8], 4);
        }
        alpha_to_mask_s("\n");
    }
    for (uint32_t i = 0; i < g_tot.nex_instr; i++) {
        uint32_t* r = g_tot.ex_instr[i];
        alpha_to_mask_s("  instruction x ");
        alpha_to_mask_u(r[0]);
        alpha_to_mask_s(" y ");
        alpha_to_mask_u(r[1]);
        alpha_to_mask_s(" A ");
        alpha_to_mask_f(r[2]);
        alpha_to_mask_s(" C ");
        alpha_to_mask_f(r[3]);
        alpha_to_mask_s(": GPU ");
        alpha_to_mask_x(r[4], 8);
        alpha_to_mask_s(", model h0 ");
        alpha_to_mask_x(r[5], 8);
        if (nh > 1) {
            alpha_to_mask_s(" h1 ");
            alpha_to_mask_x(r[6], 8);
        }
        alpha_to_mask_s("\n");
    }
    for (uint32_t i = 0; i < g_tot.nnondet_ex; i++) {
        uint32_t* e = g_tot.nondet_ex[i];
        uint32_t d = e[1] % ALPHA_TO_MASK_OUT_DEPTH;
        alpha_to_mask_s("  repeated pass differs: ");
        alpha_to_mask_s(e[0] ? "before" : "after");
        alpha_to_mask_s(e[1] >= ALPHA_TO_MASK_OUT_DEPTH ? " depth x " : " colour x ");
        alpha_to_mask_u((d / 8) & 63);
        alpha_to_mask_s(" y ");
        alpha_to_mask_u((d / 8) >> 6);
        alpha_to_mask_s(" s ");
        alpha_to_mask_u(d % 8);
        alpha_to_mask_s(": ");
        alpha_to_mask_x(e[2], 8);
        alpha_to_mask_s(" then ");
        alpha_to_mask_x(e[3], 8);
        alpha_to_mask_s("\n");
    }
    for (uint32_t i = 0; i < g_alpha_to_mask_nanom && i < 8; i++) {
        uint32_t* a = g_alpha_to_mask_anom[i];
        alpha_to_mask_s("  unclassified (last pass) x ");
        alpha_to_mask_u(a[0]);
        alpha_to_mask_s(" y ");
        alpha_to_mask_u(a[1]);
        alpha_to_mask_s(" s ");
        alpha_to_mask_u(a[2] & 0xFF);
        alpha_to_mask_s((a[2] >> 8) == 3 ? " depth before the test draw " : " depth ");
        alpha_to_mask_x(a[3], 8);
        alpha_to_mask_s(" -> ");
        alpha_to_mask_x(a[4], 8);
        alpha_to_mask_s("\n");
    }
    alpha_to_mask_s("summary ");
    alpha_to_mask_s(t->id);
    alpha_to_mask_s(": ");
    if (t->hyp == H_NOMODEL) {
        alpha_to_mask_s("no model");
    } else {
        for (int h = 0; h < nh; h++) {
            if (h)
                alpha_to_mask_s(", ");
            alpha_to_mask_s("h");
            alpha_to_mask_u((unsigned long long)h);
            alpha_to_mask_s(g_tot.dmis[h] + g_tot.cmis[h] + g_tot.zp_bad[h] + g_tot.iso_bad[h] +
                                    g_tot.imis[h]
                                ? " DIFFERS"
                                : " matches");
        }
    }
    if (g_tot.zp_none)
        alpha_to_mask_s(", NO COUNTS");
    if (g_tot.zp_rb_bad || g_tot.iso_rb_bad)
        alpha_to_mask_s(", ZPASS_DONE != READBACK");
    if (g_tot.cdiff && alpha_to_mask_colour_written(t) && !t->nodepth && t->ps != V_KILL_NOVM &&
        !(t->dbsc & ALPHA_TO_MASK_DEPTH_BEFORE_SHADER))
        alpha_to_mask_s(", COLOUR != DEPTH");
    if (g_tot.unclass)
        alpha_to_mask_s(", UNCLASSIFIED");
    if (g_tot.inconsistent)
        alpha_to_mask_s(", INCONSISTENT");
    if (g_tot.nondet)
        alpha_to_mask_s(", NONDETERMINISTIC");
    if (g_tot.unwritten)
        alpha_to_mask_s(", UNWRITTEN READBACK");
    alpha_to_mask_s("\n\n");
}

/* ---- refinement (bisection on the float bit pattern, per quad pixel) ---- */
static uint32_t g_ob_bits[4][ALPHA_TO_MASK_OBMAX], g_ob_key[4][ALPHA_TO_MASK_OBMAX];
static int g_ob_n[4];
static uint32_t g_tmp_bits[ALPHA_TO_MASK_OBMAX], g_tmp_key[ALPHA_TO_MASK_OBMAX];
static uint32_t g_new_bits[ALPHA_TO_MASK_QUADS], g_new_key[ALPHA_TO_MASK_QUADS];
static uint32_t g_alpha_to_mask_conflicts;

static void alpha_to_mask_ob_add(const uint32_t* al, int nused) {
    for (int p = 0; p < 4; p++) {
        int nn = 0;
        for (int q = 0; q < nused; q++) {
            uint32_t v = al[q];
            if (v > 0x40000000u)
                continue;
            uint32_t k = alpha_to_mask_key(alpha_to_mask_qpx(q, p));
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
        while ((i < n || j < nn) && m < ALPHA_TO_MASK_OBMAX) {
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
                    g_alpha_to_mask_conflicts++;
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
static int alpha_to_mask_refine_alpha(uint32_t* al) {
    int nint = 0;
    for (int p = 0; p < 4; p++)
        for (int i = 0; i + 1 < g_ob_n[p]; i++)
            if (g_ob_key[p][i] != g_ob_key[p][i + 1] && g_ob_bits[p][i + 1] - g_ob_bits[p][i] > 1)
                nint++;
    if (!nint)
        return 0;
    int per = ALPHA_TO_MASK_QUADS / nint, n = 0;
    if (per < 1)
        per = 1;
    for (int p = 0; p < 4; p++)
        for (int i = 0; i + 1 < g_ob_n[p] && n < ALPHA_TO_MASK_QUADS; i++) {
            uint32_t lo = g_ob_bits[p][i], hi = g_ob_bits[p][i + 1];
            if (g_ob_key[p][i] == g_ob_key[p][i + 1] || hi - lo <= 1)
                continue;
            uint32_t k = (uint32_t)per;
            if (k > hi - lo - 1)
                k = hi - lo - 1;
            for (uint32_t j = 1; j <= k && n < ALPHA_TO_MASK_QUADS; j++)
                al[n++] = lo + (uint32_t)((uint64_t)(hi - lo) * j / (k + 1));
        }
    for (int q = n; q < ALPHA_TO_MASK_QUADS; q++)
        al[q] = al[0];
    return n;
}
static void alpha_to_mask_report_refined(const struct AlphaToMaskTest* t, int passes,
                                         int converged) {
    int cmp = k_alpha_to_mask_cbf[t->cbf].cmp;
    alpha_to_mask_s("\nrefined (");
    alpha_to_mask_u((unsigned long long)passes);
    alpha_to_mask_s(" passes, ");
    alpha_to_mask_s(converged ? "converged" : "NOT converged");
    alpha_to_mask_s(", same alpha with different masks: ");
    alpha_to_mask_u(g_alpha_to_mask_conflicts);
    alpha_to_mask_s(
        "): alpha runs in [0, 2] with the same colour / depth mask; \"exact\": the next run "
        "starts at the next float\n");
    for (int p = 0; p < 4; p++) {
        alpha_to_mask_s("\np");
        alpha_to_mask_u((unsigned long long)p);
        alpha_to_mask_s(":\n\n| from | to | colour/depth | boundary |\n|---|---|---|---|\n");
        for (int i = 0; i < g_ob_n[p];) {
            int e = i;
            while (e + 1 < g_ob_n[p] && g_ob_key[p][e + 1] == g_ob_key[p][i])
                e++;
            uint32_t k = g_ob_key[p][i];
            alpha_to_mask_s("| ");
            alpha_to_mask_f(g_ob_bits[p][i]);
            alpha_to_mask_s(" | ");
            alpha_to_mask_f(g_ob_bits[p][e]);
            alpha_to_mask_s(" | ");
            if (cmp)
                alpha_to_mask_x(k & 0xFF, 2);
            else
                alpha_to_mask_s("--");
            alpha_to_mask_s("/");
            alpha_to_mask_x((k >> 8) & 0xFF, 2);
            if (k >> 16)
                alpha_to_mask_s(" !");
            alpha_to_mask_s(" | ");
            alpha_to_mask_s(e + 1 >= g_ob_n[p]                           ? "end"
                            : g_ob_bits[p][e + 1] == g_ob_bits[p][e] + 1 ? "exact"
                                                                         : "open");
            alpha_to_mask_s(" |\n");
            i = e + 1;
        }
    }
}

/* ---- the 16-bit sweep: masks per raw value and quad pixel ---- */
static uint8_t g_sw[4][65536];
static void alpha_to_mask_sweep_store(int pass) {
    for (int q = 0; q < ALPHA_TO_MASK_QUADS; q++)
        for (int p = 0; p < 4; p++) {
            int px = alpha_to_mask_qpx(q, p);
            g_sw[p][pass * ALPHA_TO_MASK_QUADS + q] =
                (uint8_t)(g_alpha_to_mask_dm[px] | (g_alpha_to_mask_an[px] ? 0x80u : 0u));
        }
}
static void alpha_to_mask_report_sweep(void) {
    alpha_to_mask_s("\nruns over the raw 16-bit alpha (depth mask; 80 = unclassified):\n");
    for (int p = 0; p < 4; p++) {
        alpha_to_mask_s("\np");
        alpha_to_mask_u((unsigned long long)p);
        alpha_to_mask_s(":");
        for (uint32_t v = 0; v < 65536;) {
            uint32_t e = v;
            while (e + 1 < 65536 && g_sw[p][e + 1] == g_sw[p][v])
                e++;
            alpha_to_mask_s(" ");
            alpha_to_mask_x(v, 4);
            if (e > v) {
                alpha_to_mask_s("..");
                alpha_to_mask_x(e, 4);
            }
            alpha_to_mask_s(":");
            alpha_to_mask_x(g_sw[p][v], 2);
            v = e + 1;
        }
        alpha_to_mask_s("\n");
    }
}

/* ---- passes ---- */
static uint64_t g_alpha_to_mask_bad; /* AlphaToMaskState.bad_variants of this launch */

static void alpha_to_mask_params(struct AlphaToMaskContext* c, const struct AlphaToMaskTest* t) {
    my_memset(c->tab, 0, 64 * 4);
    c->tab[4] = t->mrt0a;
    c->tab[5] = t->mrtza;
    c->tab[8] = 0x3F000000u;
    c->tab[9] = t->colour ? t->colour : 0x3F800000u;
    c->tab[10] = t->ns;
    c->tab[11] = t->iter_delta;
    c->tab[32] = (t->raw_col & 0xFFFFu) | (t->raw_col << 16);
    c->tab[33] = t->raw_col & 0xFFFFu;
    build_tsharp_levels(c->tab + 48, c->tex, ALPHA_TO_MASK_TEX_W, ALPHA_TO_MASK_TEX_W,
                        ALPHA_TO_MASK_TEX_LEVELS);
    build_ssharp_height(c->tab + 56); /* wrap, bilinear, linear mips */
    build_vsharp(c->tab, c->alpha, 3 * ALPHA_TO_MASK_PX * 4);
    build_vsharp(c->tab + 12, c->out, 2 * ALPHA_TO_MASK_OUT_DEPTH * 4);
    alpha_to_mask_tsharps(c, t);
    my_memcpy(c->tab_pre, c->tab, 64 * 4);
    build_vsharp(c->tab_pre + 12, c->out_pre, 2 * ALPHA_TO_MASK_OUT_DEPTH * 4);
    my_memcpy(c->tab_iso, c->tab, 64 * 4);
    build_vsharp(c->tab_iso, c->alpha_iso, 3 * ALPHA_TO_MASK_PX * 4);
}

/* One pass with its checks: submit (repeated up to ALPHA_TO_MASK_RETRIES times while the readback
   has unwritten samples), classify, check against the model. Returns alpha_to_mask_submit's code.
 */
/* The first pass of a test is submitted a second time; both readbacks and counts must agree. */
static uint32_t g_rep_out[2 * ALPHA_TO_MASK_OUT_DEPTH], g_rep_pre[2 * ALPHA_TO_MASK_OUT_DEPTH];
static int64_t g_rep_zp[ALPHA_TO_MASK_ZP_BLOCKS];
static uint32_t g_rep_tab[2 * ALPHA_TO_MASK_PX];

/* One pass with its checks: submit (repeated up to ALPHA_TO_MASK_RETRIES times while the readback
   has unwritten samples), classify, check against the model and the ZPASS_DONE counts. */
static int alpha_to_mask_run_pass(struct AlphaToMaskContext* c, const struct AlphaToMaskTest* t,
                                  int regread, int repeat) {
    uint64_t us = 0;
    int r = 0, rb = alpha_to_mask_has_readback(t);
    for (int k = 0; k <= ALPHA_TO_MASK_RETRIES; k++) {
        r = alpha_to_mask_submit(c, t, 1, regread, &us);
        if (r)
            return r;
        g_tot.pass_us += us;
        if (!rb)
            break;
        alpha_to_mask_classify(c, t);
        if (!g_alpha_to_mask_unwritten)
            break;
        if (k < ALPHA_TO_MASK_RETRIES)
            g_tot.retries++;
    }
    g_tot.passes++;
    if (rb)
        g_tot.unwritten += g_alpha_to_mask_unwritten;
    if (t->ps == V_TEX) { /* the model takes the alpha t_tex sampled */
        for (int i = 0; i < ALPHA_TO_MASK_PX; i++) {
            c->alpha[i] = c->alpha[ALPHA_TO_MASK_PX + i];
            if (c->alpha[i] == ALPHA_TO_MASK_NOT_SHADED)
                g_tot.not_shaded++;
        }
    }
    alpha_to_mask_check(c, t);
    alpha_to_mask_zpass_check(c, t);
    alpha_to_mask_instr_check(c, t);
    if (!repeat)
        return 0;
    uint32_t vm;
    for (uint32_t i = 0; i < 2 * ALPHA_TO_MASK_OUT_DEPTH; i++) {
        g_rep_out[i] = c->out[i];
        g_rep_pre[i] = c->out_pre[i];
    }
    for (uint32_t blk = 0; blk <= t->ns; blk++)
        g_rep_zp[blk] = alpha_to_mask_zp_sum(c, blk, &vm);
    for (uint32_t i = 0; i < 2 * ALPHA_TO_MASK_PX; i++)
        g_rep_tab[i] = c->alpha[ALPHA_TO_MASK_PX + i];
    r = alpha_to_mask_submit(c, t, 1, 0, &us);
    if (r)
        return r;
    g_tot.pass_us += us;
    for (uint32_t i = 0; i < 2 * ALPHA_TO_MASK_OUT_DEPTH; i++)
        for (uint32_t plane = 0; plane < 2; plane++) {
            uint32_t now = plane ? c->out_pre[i] : c->out[i];
            uint32_t was = plane ? g_rep_pre[i] : g_rep_out[i];
            if (now == was)
                continue;
            g_tot.nondet++;
            if (g_tot.nnondet_ex < 4) {
                uint32_t* e = g_tot.nondet_ex[g_tot.nnondet_ex++];
                e[0] = plane, e[1] = i, e[2] = was, e[3] = now;
            }
        }
    for (uint32_t blk = 0; blk <= t->ns; blk++)
        g_tot.nondet += alpha_to_mask_zp_sum(c, blk, &vm) != g_rep_zp[blk];
    for (uint32_t i = 0; i < 2 * ALPHA_TO_MASK_PX; i++)
        g_tot.nondet += c->alpha[ALPHA_TO_MASK_PX + i] != g_rep_tab[i];
    return 0;
}

/* A random configuration k of RND. */
/* Families: one test entry runs many configurations, one line each.
   RND  48 random configurations (sample count, offsets, round, EQAA_DISABLE, NUM_SAMPLES,
        variant, PA_SC_AA_MASK, ZFUNC) with random alphas, 2 passes each
   BU   every level boundary, uniform offsets: 1x 2x 4x 8x, offsets 0..3, round 0 / 1,
        EQAA_DISABLE 0 / 1
   BP   every level boundary, a different offset per quad pixel (6 orders), round 0 / 1, 1x .. 8x
   BN   every level boundary, ALPHA_TO_MASK_NUM_SAMPLES below and above the sample count
   B16  every level boundary, 16 coverage samples (no targets: ZPASS_DONE only) */
static int alpha_to_mask_family_size(const struct AlphaToMaskTest* t) {
    if (alpha_to_mask_streq(t->id, "RND"))
        return ALPHA_TO_MASK_RND_CONFIGS;
    if (alpha_to_mask_streq(t->id, "BU"))
        return 4 * 8 + 3 * 8;
    if (alpha_to_mask_streq(t->id, "BP"))
        return 6 * 2 * 4;
    if (alpha_to_mask_streq(t->id, "BN"))
        return 10 * 2;
    if (alpha_to_mask_streq(t->id, "B16"))
        return 5 * 2 * 2;
    if (alpha_to_mask_streq(t->id, "AA"))
        return 2 + 4 + 16 + 256;
    if (alpha_to_mask_streq(t->id, "AAP"))
        return ALPHA_TO_MASK_RND_CONFIGS;
    if (alpha_to_mask_streq(t->id, "AA16"))
        return 48;
    return 0;
}
static void alpha_to_mask_family_test(struct AlphaToMaskTest* o, const struct AlphaToMaskTest* t,
                                      int k) {
    *o = *t;
    o->kind = AK_BOUND;
    o->ps = V_F32;
    o->alpha_to_mask_num_samples = 0xFF;
    if (alpha_to_mask_streq(t->id, "RND")) {
        alpha_to_mask_seed(t->seed + (uint32_t)k * 7919u);
        uint32_t r = alpha_to_mask_rand(), r2 = alpha_to_mask_rand();
        o->kind = AK_RANDOM;
        o->ns = (uint8_t)(1u << (r & 3));
        o->db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, (r >> 2) & 3, (r >> 4) & 3, (r >> 6) & 3,
                                                     (r >> 8) & 3, (r >> 10) & 1);
        o->eqaa_dis = ((r >> 11) & 7) == 0;
        o->alpha_to_mask_num_samples =
            ((r >> 14) & 3) == 0 ? (uint8_t)((r >> 16) % (alpha_to_mask_log2(o->ns) + 2)) : 0xFF;
        o->dbsc = 0;
        o->bmode = BM_NONE;
        switch (r2 % 6) {
        case 1:
            o->ps = V_ZA;
            o->dbsc = ALPHA_TO_MASK_COV2MASK;
            break;
        case 2:
            o->ps = V_ALL;
            o->dbsc = ALPHA_TO_MASK_COV2MASK;
            o->bmode = BM_ALL;
            break;
        case 3:
            o->ps = V_MASK;
            o->bmode = BM_MASK;
            break;
        case 4:
            o->ps = V_KILL;
            o->bmode = BM_KILL;
            break;
        case 5:
            o->ps = V_F16;
            o->colfmt = 4;
            break;
        }
        o->aa_mask = ((r2 >> 3) & 3) == 0 ? (((r2 >> 5) & 0xFFu) | 1u) : 0;
        o->zfunc = ((r2 >> 13) & 7) == 0 ? 5 : 0; /* GREATER: every sample fails */
        o->seed = t->seed * 31u + (uint32_t)k;
        return;
    }
    if (alpha_to_mask_streq(t->id, "BU")) {
        int nl = k < 32 ? k / 8 : 1 + (k - 32) / 8, rest = k % 8;
        uint32_t off = (uint32_t)rest >> 1, round = (uint32_t)rest & 1;
        o->ns = (uint8_t)(1u << nl);
        o->db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, off, off, off, off, round);
        o->eqaa_dis = k >= 32;
        return;
    }
    if (alpha_to_mask_streq(t->id, "BP")) {
        static const uint8_t order[6][4] = {{0, 1, 2, 3}, {1, 0, 3, 2}, {2, 3, 0, 1},
                                            {3, 2, 1, 0}, {3, 1, 0, 2}, {2, 0, 1, 3}};
        int pi = k % 6, round = (k / 6) & 1, nl = k / 12;
        const uint8_t* q = order[pi];
        o->ns = (uint8_t)(1u << nl);
        o->db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE(1, q[0], q[1], q[2], q[3], round);
        return;
    }
    if (alpha_to_mask_streq(t->id, "BN")) {
        static const uint8_t pair[10][2] = {{0, 1}, {0, 2}, {1, 2}, {0, 3}, {1, 3},
                                            {2, 3}, {2, 1}, {3, 2}, {4, 3}, {1, 0}};
        o->alpha_to_mask_num_samples = pair[k / 2][0];
        o->ns = (uint8_t)(1u << pair[k / 2][1]);
        o->db_alpha_to_mask = ALPHA_TO_MASK_D1;
        o->eqaa_dis = k & 1;
        return;
    }
    if (alpha_to_mask_streq(t->id, "AA")) { /* every PA_SC_AA_MASK value, 1x 2x 4x 8x */
        int nl = k < 2 ? 0 : k < 6 ? 1 : k < 22 ? 2 : 3;
        static const int first[4] = {0, 2, 6, 22};
        o->ns = (uint8_t)(1u << nl);
        o->kind = AK_RANDOM;
        o->ps = V_COV;
        o->bmode = BM_C;
        o->aa_mask = (uint32_t)(k - first[nl]) ? (uint32_t)(k - first[nl]) : ALPHA_TO_MASK_AA_ZERO;
        o->seed = t->seed + (uint32_t)k;
        return;
    }
    if (alpha_to_mask_streq(t->id, "AAP")) { /* random per-pixel masks with everything else */
        alpha_to_mask_seed(t->seed + (uint32_t)k * 7919u);
        uint32_t r = alpha_to_mask_rand(), r2 = alpha_to_mask_rand(), r3 = alpha_to_mask_rand();
        o->kind = AK_RANDOM;
        o->ns = (uint8_t)(1u << (r & 3));
        o->aa_quad = (uint64_t)(r2 & 0xFFFFu) | ((uint64_t)(r2 >> 16) << 16) |
                     ((uint64_t)(r3 & 0xFFFFu) << 32) | ((uint64_t)(r3 >> 16) << 48);
        o->db_alpha_to_mask = DB_ALPHA_TO_MASK_VALUE((r >> 2) & 1, (r >> 3) & 3, (r >> 5) & 3,
                                                     (r >> 7) & 3, (r >> 9) & 3, (r >> 11) & 1);
        o->dbsc = 0;
        o->bmode = BM_NONE;
        switch ((r >> 12) % 5) {
        case 0:
            o->ps = V_COV;
            o->bmode = BM_C;
            break;
        case 1:
            o->ps = V_KILL;
            o->bmode = BM_KILL;
            break;
        case 2:
            o->ps = V_MASK;
            o->bmode = BM_MASK;
            break;
        case 3:
            o->ps = V_ALL;
            o->dbsc = ALPHA_TO_MASK_COV2MASK;
            o->bmode = BM_ALL;
            break;
        default:
            o->ps = V_MARK;
            o->bmode = BM_C;
            break;
        }
        o->zfunc = ((r >> 16) & 7) == 0 ? 5 : 0;
        o->seed = t->seed * 31u + (uint32_t)k;
        return;
    }
    if (alpha_to_mask_streq(t->id, "AA16")) { /* 16 samples: walking one, walking zero, random */
        alpha_to_mask_seed(t->seed + (uint32_t)k);
        o->ns = 16;
        o->kind = AK_RANDOM;
        o->db_alpha_to_mask = ALPHA_TO_MASK_OFF0;
        uint32_t m = k < 16   ? 1u << k
                     : k < 32 ? 0xFFFFu & ~(1u << (k - 16))
                              : alpha_to_mask_rand() & 0xFFFFu;
        o->aa_mask = m ? m : ALPHA_TO_MASK_AA_ZERO;
        return;
    }
    /* B16 */
    o->ns = 16;
    o->alpha_to_mask_num_samples = (uint8_t)(k / 4);
    o->db_alpha_to_mask = (k & 1) ? ALPHA_TO_MASK_D1 : ALPHA_TO_MASK_D0;
    o->eqaa_dis = (k >> 1) & 1;
}

static void alpha_to_mask_family_line(const struct AlphaToMaskTest* o, int k) {
    alpha_to_mask_s("| ");
    alpha_to_mask_u((unsigned long long)k);
    alpha_to_mask_s(" | ");
    alpha_to_mask_u(o->ns);
    alpha_to_mask_s(" | ");
    if (o->alpha_to_mask_num_samples == 0xFF)
        alpha_to_mask_s("auto");
    else
        alpha_to_mask_u(o->alpha_to_mask_num_samples);
    alpha_to_mask_s(" | ");
    alpha_to_mask_x(alpha_to_mask_register(o), 8);
    alpha_to_mask_s(" | ");
    alpha_to_mask_u(o->eqaa_dis);
    alpha_to_mask_s(" | ");
    alpha_to_mask_s(k_alpha_to_mask_vname[o->ps]);
    if (o->aa_mask) {
        alpha_to_mask_s(", AA_MASK ");
        alpha_to_mask_x(alpha_to_mask_aa(o), 4);
    }
    if (o->aa_quad) {
        alpha_to_mask_s(", AA_MASK per pixel ");
        alpha_to_mask_x((uint32_t)(o->aa_quad >> 32), 8);
        alpha_to_mask_x((uint32_t)o->aa_quad, 8);
    }
    if (o->zfunc)
        alpha_to_mask_s(", ZFUNC GREATER");
    alpha_to_mask_s(" | ");
    alpha_to_mask_u(g_tot.dmis[0]);
    alpha_to_mask_s(" / ");
    alpha_to_mask_u(g_tot.cmis[0]);
    alpha_to_mask_s(" | ");
    alpha_to_mask_u(g_tot.zp_bad[0]);
    alpha_to_mask_s(" / ");
    alpha_to_mask_u(g_tot.iso_bad[0]);
    alpha_to_mask_s(" | ");
    alpha_to_mask_u(g_tot.zp_rb_bad);
    alpha_to_mask_s(" / ");
    alpha_to_mask_u(g_tot.iso_rb_bad);
    alpha_to_mask_s(" | ");
    alpha_to_mask_u(g_tot.cdiff);
    alpha_to_mask_s(" | ");
    alpha_to_mask_u(g_tot.unclass);
    alpha_to_mask_s(" | ");
    alpha_to_mask_u(g_tot.inconsistent);
    alpha_to_mask_s(" / ");
    alpha_to_mask_u(g_tot.repeats);
    alpha_to_mask_s(" | ");
    alpha_to_mask_u(g_tot.nondet);
    alpha_to_mask_s(" | ");
    alpha_to_mask_u(g_tot.zp_none);
    alpha_to_mask_s(" | ");
    alpha_to_mask_u(g_tot.imis[0]);
    alpha_to_mask_s(" / ");
    alpha_to_mask_u(g_tot.ichk);
    alpha_to_mask_s(" |\n");
}

/* Runs test t; returns -1 on a GPU hang. */
static int alpha_to_mask_run_test(struct AlphaToMaskContext* c, const struct AlphaToMaskTest* t,
                                  int regread, int k_index) {
    my_memset(&g_tot, 0, sizeof(g_tot));
    alpha_to_mask_report_header(t);
    int nfam = alpha_to_mask_family_size(t);
    if (nfam) {
        alpha_to_mask_s(
            "\n| # | samples | NUM_SAMPLES | DB_ALPHA_TO_MASK | EQAA_DISABLE | variant | model "
            "depth / colour mismatches | ZPASS_DONE != model: test / isolation | ZPASS_DONE != "
            "readback: test / isolation | colour != depth | unclassified | same alpha differing "
            "/ repeats | repeated pass differing | draws without counts | stored results "
            "differing |\n"
            "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
        uint32_t bad = 0, flags = 0;
        for (int k = 0; k < nfam; k++) {
            struct AlphaToMaskTest o;
            alpha_to_mask_family_test(&o, t, k);
            my_memset(&g_tot, 0, sizeof(g_tot));
            if ((g_alpha_to_mask_bad >> o.ps) & 1) {
                alpha_to_mask_s("| ");
                alpha_to_mask_u((unsigned long long)k);
                alpha_to_mask_s(" | skipped: its pixel shader variant crashed or hung earlier |\n");
                continue;
            }
            alpha_to_mask_params(c, &o);
            int npass = o.kind == AK_RANDOM ? 2 : 1;
            for (int pass = 0; pass < npass; pass++) {
                alpha_to_mask_fill(c, &o, pass);
                alpha_to_mask_flush();
                int r = alpha_to_mask_run_pass(c, &o, 0, 1);
                if (r == -1)
                    return -1;
                if (r == -2)
                    break;
            }
            alpha_to_mask_family_line(&o, k);
            bad +=
                g_tot.dmis[0] + g_tot.cmis[0] + g_tot.zp_bad[0] + g_tot.iso_bad[0] + g_tot.imis[0];
            flags += g_tot.zp_rb_bad + g_tot.iso_rb_bad + g_tot.unclass + g_tot.inconsistent +
                     g_tot.nondet + g_tot.zp_none + g_tot.unwritten;
        }
        alpha_to_mask_s("summary ");
        alpha_to_mask_s(t->id);
        alpha_to_mask_s(": ");
        alpha_to_mask_s(bad ? "DIFFERS" : "all match");
        if (flags)
            alpha_to_mask_s(", CROSS-CHECK FLAGS (see the table)");
        alpha_to_mask_s("\n\n");
        return 0;
    }
    alpha_to_mask_params(c, t);
    int npass = t->kind == AK_SWEEP16  ? 64
                : t->kind == AK_RANDOM ? 4
                : t->kind == AK_TIES   ? (t->ps == V_PKNORM_U16 ? 48 : 24)
                                       : 1;
    for (int pass = 0; pass < npass; pass++) {
        alpha_to_mask_fill(c, t, pass);
        alpha_to_mask_flush();
        int r = alpha_to_mask_run_pass(c, t, pass == 0 && regread, 1);
        if (r == -1)
            return -1;
        if (r == -2) {
            alpha_to_mask_s("NOT SUBMITTED (command buffer overflow or submit error)\n\n");
            return 0;
        }
        if (pass == 0) {
            alpha_to_mask_report_regs(c, regread, k_index == 0);
            alpha_to_mask_report_zpass(c, t);
        }
        if (t->kind == AK_SWEEP16)
            alpha_to_mask_sweep_store(pass);
    }
    int show = t->show || (alpha_to_mask_has_readback(t) && t->hyp != H_NOMODEL &&
                           g_tot.dmis[0] + g_tot.cmis[0] &&
                           (alpha_to_mask_nh(t) == 1 || g_tot.dmis[1] + g_tot.cmis[1]));
    if (t->kind == AK_UNIFORM)
        alpha_to_mask_report_uniform(t);
    else if ((t->kind == AK_COARSE || t->kind == AK_RAMP) && show)
        alpha_to_mask_report_quads(t);
    if (t->kind == AK_SWEEP16)
        alpha_to_mask_report_sweep();
    if (t->refine && t->kind == AK_COARSE) {
        g_ob_n[0] = g_ob_n[1] = g_ob_n[2] = g_ob_n[3] = 0;
        g_alpha_to_mask_conflicts = 0;
        alpha_to_mask_ob_add(g_alpha_to_mask_qa,
                             ALPHA_TO_MASK_QUADS); /* pass 0: the coarse table, still classified */
        int passes = 0, conv = 0;
        while (passes < ALPHA_TO_MASK_MAX_PASSES) {
            int n = alpha_to_mask_refine_alpha(g_alpha_to_mask_qa);
            if (!n) {
                conv = 1;
                break;
            }
            alpha_to_mask_expand_quads(c->alpha);
            alpha_to_mask_flush();
            int r = alpha_to_mask_run_pass(c, t, 0, 1);
            if (r == -1)
                return -1;
            if (r == -2)
                break;
            alpha_to_mask_ob_add(g_alpha_to_mask_qa, n);
            passes++;
        }
        alpha_to_mask_report_refined(t, passes, conv);
    }
    alpha_to_mask_report_totals(t);
    return 0;
}

/* ---- the visual pages ---- */
/* ORBIS_PAD_BUTTON_TOUCH_PAD (as OPCODE_TEST 6) */
#define ALPHA_TO_MASK_VISUAL_TOUCH_PAD 0x100000u
#define ALPHA_TO_MASK_VISUAL_ROWS 3
#define ALPHA_TO_MASK_VISUAL_PAGES                                                                 \
    ((ALPHA_TO_MASK_VIS_SLOTS + ALPHA_TO_MASK_VISUAL_ROWS - 1) / ALPHA_TO_MASK_VISUAL_ROWS)
#define ALPHA_TO_MASK_VISUAL_Y0 120 /* gen_ps_alpha_to_mask.py VIS_Y0, VIS_PITCH, VIS_H, VIS_X */
#define ALPHA_TO_MASK_VISUAL_PITCH 312
#define ALPHA_TO_MASK_VISUAL_H 256
static const int k_alpha_to_mask_visual_x[4] = {520, 792, 1064, 1336};

static uint32_t* g_alpha_to_mask_visual_tab;                          /* vis's table */
static uint32_t* g_alpha_to_mask_visual_res[ALPHA_TO_MASK_VIS_SLOTS]; /* per pixel: [7:0] GPU,
                                                                         [15:8] model (h0) */
static const struct AlphaToMaskTest* g_alpha_to_mask_visual_test[ALPHA_TO_MASK_VIS_SLOTS];
static uint32_t g_alpha_to_mask_visual_mis[ALPHA_TO_MASK_VIS_SLOTS],
    g_alpha_to_mask_visual_unwritten[ALPHA_TO_MASK_VIS_SLOTS];
static int g_alpha_to_mask_visual_ok, g_alpha_to_mask_visual_page,
    g_alpha_to_mask_visual_mx = 24, g_alpha_to_mask_visual_my = 24,
    g_alpha_to_mask_visual_ui_key = -1;

static void alpha_to_mask_visual_set_rows(void) {
    for (int r = 0; r < ALPHA_TO_MASK_VISUAL_ROWS; r++) {
        uint32_t* w = g_alpha_to_mask_visual_tab + 32 + 16 * r;
        int slot = g_alpha_to_mask_visual_page * ALPHA_TO_MASK_VISUAL_ROWS + r;
        my_memset(w, 0, 64);
        if (slot >= ALPHA_TO_MASK_VIS_SLOTS || !g_alpha_to_mask_visual_test[slot])
            continue;
        uint32_t ns = g_alpha_to_mask_visual_test[slot]->ns;
        w[0] = 1;
        w[1] = ns;
        w[2] = (uint32_t)g_alpha_to_mask_visual_mx;
        w[3] = (uint32_t)g_alpha_to_mask_visual_my;
        w[4] = ns >= 8 ? 2u : ns >= 2 ? 1u : 0u; /* sample grid: 4 x 2, 2 x 2, 2 x 1, 1 */
        w[5] = ns >= 4 ? 1u : 0u;
        w[6] = alpha_to_mask_fbits(1.0f / (float)ns);
        build_vsharp(w + 8, g_alpha_to_mask_visual_res[slot], ALPHA_TO_MASK_PX * 4);
    }
}

/* The visual captures (every launch): one pass per V test, its result next to the model. */
static void alpha_to_mask_state_save(const struct AlphaToMaskState* st);
static int alpha_to_mask_visual_capture(struct AlphaToMaskContext* c, struct AlphaToMaskState* st) {
    for (int k = 0; k < ALPHA_TO_MASK_NTESTS; k++) {
        const struct AlphaToMaskTest* t = &k_alpha_to_mask_tests[k];
        if (!t->vis || ((g_alpha_to_mask_bad >> t->ps) & 1))
            continue;
        int slot = t->vis - 1;
        st->inflight = k | ALPHA_TO_MASK_CAPTURE;
        alpha_to_mask_state_save(st);
        my_memset(&g_tot, 0, sizeof(g_tot));
        alpha_to_mask_params(c, t);
        alpha_to_mask_fill(c, t, 0);
        if (alpha_to_mask_run_pass(c, t, 0, 0) == -1)
            return -1;
        uint32_t mis = 0;
        for (int px = 0; px < ALPHA_TO_MASK_PX; px++) {
            uint32_t gpu = t->nodepth ? g_alpha_to_mask_cm[px] : g_alpha_to_mask_dm[px];
            g_alpha_to_mask_visual_res[slot][px] = gpu | ((uint32_t)g_model0[px] << 8);
            mis += gpu != g_model0[px];
        }
        g_alpha_to_mask_visual_test[slot] = t;
        g_alpha_to_mask_visual_mis[slot] = mis;
        g_alpha_to_mask_visual_unwritten[slot] = g_alpha_to_mask_unwritten;
    }
    st->inflight = -1;
    alpha_to_mask_state_save(st);
    return 0;
}

static void alpha_to_mask_visual_input(uint32_t pressed) {
    if (!g_alpha_to_mask_visual_ok)
        return;
    if (pressed & ALPHA_TO_MASK_VISUAL_TOUCH_PAD)
        g_alpha_to_mask_visual_page =
            (g_alpha_to_mask_visual_page + 1) % ALPHA_TO_MASK_VISUAL_PAGES;
    if (pressed & PAD_LEFT)
        g_alpha_to_mask_visual_mx = (g_alpha_to_mask_visual_mx + 48) & 63;
    if (pressed & PAD_RIGHT)
        g_alpha_to_mask_visual_mx = (g_alpha_to_mask_visual_mx + 16) & 63;
    if (pressed & PAD_UP)
        g_alpha_to_mask_visual_my = (g_alpha_to_mask_visual_my + 56) & 63;
    if (pressed & PAD_DOWN)
        g_alpha_to_mask_visual_my = (g_alpha_to_mask_visual_my + 8) & 63;
    if (pressed & (ALPHA_TO_MASK_VISUAL_TOUCH_PAD | PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN))
        alpha_to_mask_visual_set_rows();
}

static int alpha_to_mask_visual_cat(char* o, const char* s) {
    int n = 0;
    while (s[n]) {
        o[n] = s[n];
        n++;
    }
    return n;
}

/* Text at (x, y) wrapped at spaces to maxw pixels; returns the y of the next line. */
static int alpha_to_mask_visual_wrap(unsigned char* b, int x, int y, const char* s, int maxw,
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
static void alpha_to_mask_visual_ui(void) {
    static const unsigned char white[3] = {255, 255, 255}, grey[3] = {190, 190, 196},
                               red[3] = {255, 110, 110}, green[3] = {130, 230, 130};
    g_clock_mode = 0;
    int key = g_alpha_to_mask_visual_page * 4096 + g_alpha_to_mask_visual_mx * 64 +
              g_alpha_to_mask_visual_my;
    if (!g_ui.ok || !g_alpha_to_mask_visual_ok || g_alpha_to_mask_visual_ui_key == key)
        return;
    int nb = (g_ui.cur + 1) % UI_BUFS;
    unsigned char* b = g_ui.buf[nb];
    g_ui.drawing = 0;
    g_ui.nclip = 0;
    ui_clear(b, 0, 0, UI_W, UI_H);
    g_ui.st[nb].valid = 0;
    g_ui.dirty[nb][0] = g_ui.dirty[nb][1] = g_ui.dirty[nb][2] = g_ui.dirty[nb][3] = 0;
    char t[200], *p = t;
    p += alpha_to_mask_visual_cat(p, "Alpha-to-mask (DB_ALPHA_TO_MASK), build " ALPHA_TO_MASK_BUILD
                                     "     page ");
    p += ui_fmt_int(p, g_alpha_to_mask_visual_page + 1);
    *p++ = '/';
    p += ui_fmt_int(p, ALPHA_TO_MASK_VISUAL_PAGES);
    p += alpha_to_mask_visual_cat(p, " (touch pad)     microscope x ");
    p += ui_fmt_int(p, g_alpha_to_mask_visual_mx);
    p += alpha_to_mask_visual_cat(p, ", y ");
    p += ui_fmt_int(p, g_alpha_to_mask_visual_my);
    p += alpha_to_mask_visual_cat(p, " (d-pad)");
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
    for (int r = 0; r < ALPHA_TO_MASK_VISUAL_ROWS; r++) {
        int slot = g_alpha_to_mask_visual_page * ALPHA_TO_MASK_VISUAL_ROWS + r,
            y0 = ALPHA_TO_MASK_VISUAL_Y0 + ALPHA_TO_MASK_VISUAL_PITCH * r;
        if (slot >= ALPHA_TO_MASK_VIS_SLOTS || !g_alpha_to_mask_visual_test[slot])
            continue;
        const struct AlphaToMaskTest* v = g_alpha_to_mask_visual_test[slot];
        for (int k = 0; k < 4; k++)
            ui_text(b, k_alpha_to_mask_visual_x[k], y0 - 6, col[k], grey);
        p = t;
        p += alpha_to_mask_visual_cat(p, v->id);
        p += alpha_to_mask_visual_cat(p, "  ");
        p += alpha_to_mask_visual_cat(p, v->what);
        *p = 0;
        int yy =
            alpha_to_mask_visual_wrap(b, 40, y0 + 22, t, k_alpha_to_mask_visual_x[0] - 60, white);
        p = t;
        p += ui_fmt_int(p, v->ns);
        p += alpha_to_mask_visual_cat(p, " samples, DB_ALPHA_TO_MASK 0x");
        {
            static const char h[] = "0123456789ABCDEF";
            uint32_t a = alpha_to_mask_register(v);
            for (int i = 0; i < 8; i++)
                *p++ = h[(a >> (28 - 4 * i)) & 15];
        }
        *p = 0;
        ui_text(b, 40, yy, t, grey);
        p = t;
        p += alpha_to_mask_visual_cat(p, "pixels differing from the model: ");
        p += ui_fmt_int(p, (long)g_alpha_to_mask_visual_mis[slot]);
        p += alpha_to_mask_visual_cat(p, " of 4096");
        *p = 0;
        ui_text(b, 40, yy + 26, t, g_alpha_to_mask_visual_mis[slot] ? red : green);
        if (g_alpha_to_mask_visual_unwritten[slot])
            ui_text(b, 40, yy + 52, "readback incomplete (unwritten samples)", red);
    }
    for (int i = 0; i < 4; i++)
        g_ui.rect[i] = g_ui.pill[i] = -1e6f;
    g_ui.cur = nb;
    g_alpha_to_mask_visual_ui_key = key;
    /* ps_ui composites the UI buffer inside the panels only: vis composites the labels */
    build_tsharp(g_alpha_to_mask_visual_tab + 16, g_ui.buf[nb], UI_W, UI_H);
    build_ssharp_clamp(g_alpha_to_mask_visual_tab + 24, 0);
}

/* After the final pass: the page over the whole frame (a post pass, as OPCODE_TEST 6). */
static void alpha_to_mask_visual_pass(struct PM4Builder* b, const uint32_t* bg_v) {
    if (!g_alpha_to_mask_visual_ok)
        return;
    post_pass(b, g_frame, DISPLAY_W, DISPLAY_W, DISPLAY_H, CB_INFO_RGBA16F,
              g_alpha_to_mask_context.sh[V_VIS], ALPHA_TO_MASK_VIS_RSRC1,
              g_alpha_to_mask_visual_tab, bg_v);
}

static int alpha_to_mask_visual_init(void) {
    g_alpha_to_mask_visual_tab = (uint32_t*)gpu_alloc_typed(0x400, 0x100, MEM_TYPE_ONION);
    if (!g_alpha_to_mask_visual_tab)
        return 0;
    my_memset(g_alpha_to_mask_visual_tab, 0, 0x400);
    for (int i = 0; i < ALPHA_TO_MASK_VIS_SLOTS; i++) {
        g_alpha_to_mask_visual_res[i] =
            (uint32_t*)gpu_alloc_typed(ALPHA_TO_MASK_PX * 4, 0x100, MEM_TYPE_ONION);
        if (!g_alpha_to_mask_visual_res[i])
            return 0;
        my_memset(g_alpha_to_mask_visual_res[i], 0, ALPHA_TO_MASK_PX * 4);
    }
    /* The labels' T# and S#: ps_ui's until alpha_to_mask_visual_ui draws a page (OPCODE_TEST 6's
     * opt_init). */
    if (g_post_tab)
        my_memcpy(g_alpha_to_mask_visual_tab + 16, g_post_tab + UI_BLOCK * 32 + 36, 12 * 4);
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
static void alpha_to_mask_make_net(struct AlphaToMaskContext* c) {
    unsigned char* t = (unsigned char*)c->tex;
    for (int y = 0; y < ALPHA_TO_MASK_TEX_W; y++)
        for (int x = 0; x < ALPHA_TO_MASK_TEX_W; x++) {
            float fx = (float)(x & 31) + 0.5f, fy = (float)(y & 31) + 0.5f;
            float dx = fx < 32.0f - fx ? fx : 32.0f - fx, dy = fy < 32.0f - fy ? fy : 32.0f - fy;
            float d = dx < dy ? dx : dy;
            float a = 2.5f - d;
            a = a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a;
            unsigned char* p = t + 4 * (x + ALPHA_TO_MASK_TEX_W * y);
            p[0] = p[1] = p[2] = 255;
            p[3] = (unsigned char)(a * 255.0f + 0.5f);
        }
    unsigned char* src = t;
    for (int l = 1, w = ALPHA_TO_MASK_TEX_W / 2; l < ALPHA_TO_MASK_TEX_LEVELS; l++, w /= 2) {
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
static int alpha_to_mask_alloc(struct AlphaToMaskContext* c) {
    my_memset(c, 0, sizeof(*c));
    c->dcb = (uint32_t*)gpu_alloc_typed(ALPHA_TO_MASK_DCB_DW * 4, 0x4000, MEM_TYPE_ONION);
    c->fence = (volatile uint32_t*)gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
    c->alpha = (uint32_t*)gpu_alloc_typed(3 * ALPHA_TO_MASK_PX * 4, 0x1000, MEM_TYPE_ONION);
    c->tab = (uint32_t*)gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
    c->tab_pre = (uint32_t*)gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
    c->tab_iso = (uint32_t*)gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
    c->alpha_iso = (uint32_t*)gpu_alloc_typed(3 * ALPHA_TO_MASK_PX * 4, 0x1000, MEM_TYPE_ONION);
    c->out = (uint32_t*)gpu_alloc_typed(2 * ALPHA_TO_MASK_OUT_DEPTH * 4, 0x10000, MEM_TYPE_ONION);
    c->out_pre =
        (uint32_t*)gpu_alloc_typed(2 * ALPHA_TO_MASK_OUT_DEPTH * 4, 0x10000, MEM_TYPE_ONION);
    c->zp = (uint32_t*)gpu_alloc_typed(ALPHA_TO_MASK_ZP_BLOCKS * ALPHA_TO_MASK_ZP_STRIDE, 0x1000,
                                       MEM_TYPE_ONION);
    c->regs = (uint32_t*)gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
    c->color = gpu_alloc_typed(ALPHA_TO_MASK_PX * 8 * 8, 0x10000, MEM_TYPE_ONION);
    c->depth = gpu_alloc_typed(ALPHA_TO_MASK_PX * 4 * 8, 0x10000, MEM_TYPE_ONION);
    c->scratch = gpu_alloc_typed(ALPHA_TO_MASK_PX * 8, 0x10000, MEM_TYPE_ONION);
    c->vs = gpu_alloc_typed(sizeof(alpha_to_mask_vs_full_bin) + 256, 0x1000, MEM_TYPE_ONION);
    c->vs_net = gpu_alloc_typed(sizeof(alpha_to_mask_vs_net_bin) + 256, 0x1000, MEM_TYPE_ONION);
    c->tex =
        gpu_alloc_typed(ALPHA_TO_MASK_TEX_W * ALPHA_TO_MASK_TEX_W * 4 * 2, 0x10000, MEM_TYPE_ONION);
    c->verts = (float*)gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
    int ok = c->dcb && c->fence && c->alpha && c->tab && c->tab_pre && c->tab_iso && c->alpha_iso &&
             c->out && c->out_pre && c->zp && c->regs && c->color && c->depth && c->scratch &&
             c->vs && c->vs_net && c->tex && c->verts && !((uint64_t)(uintptr_t)c->vs >> 40) &&
             !((uint64_t)(uintptr_t)c->vs_net >> 40);
    if (ok) {
        my_memcpy(c->vs, alpha_to_mask_vs_full_bin, sizeof(alpha_to_mask_vs_full_bin));
        my_memcpy(c->vs_net, alpha_to_mask_vs_net_bin, sizeof(alpha_to_mask_vs_net_bin));
        alpha_to_mask_make_net(c);
    }
    for (int i = 0; ok && i < ALPHA_TO_MASK_NSH; i++) {
        c->sh[i] = gpu_alloc_typed(k_alpha_to_mask_sh[i].size + 256, 0x1000, MEM_TYPE_ONION);
        if (!c->sh[i] || ((uint64_t)(uintptr_t)c->sh[i] >> 40))
            ok = 0;
        else
            my_memcpy(c->sh[i], k_alpha_to_mask_sh[i].bin, k_alpha_to_mask_sh[i].size);
    }
    if (ok) {
        c->fence[0] = c->fence[1] = 0;
        c->fv = 0;
    }
    return ok;
}

static int alpha_to_mask_run(void) {
    alpha_to_mask_paths();
    struct AlphaToMaskState st;
    int resumed = alpha_to_mask_state_load(&st), tests_done = 0;
    if (!resumed) {
        my_memset(&st, 0, sizeof(st));
        for (int i = 0; ALPHA_TO_MASK_BUILD[i]; i++)
            st.build[i] = ALPHA_TO_MASK_BUILD[i];
        st.inflight = -1;
    }
    /* The app ended during a test or a capture: skip that pixel shader variant from now on (an
       emulator that cannot translate it crashes; RND's variants are its own). */
    if (st.inflight >= 0) {
        const struct AlphaToMaskTest* t = &k_alpha_to_mask_tests[st.inflight & 0xFFFF];
        if (!alpha_to_mask_streq(t->id, "RND"))
            st.bad_variants |= alpha_to_mask_group(t->ps);
        if (st.inflight & ALPHA_TO_MASK_CAPTURE) {
            trace_msg("alpha-to-mask: the app ended during a visual capture; its variant is "
                      "skipped now\n");
            st.inflight = -1;
            alpha_to_mask_state_save(&st);
        }
    }
    g_alpha_to_mask_bad = st.bad_variants;
    if (resumed && st.next >= ALPHA_TO_MASK_NTESTS && st.inflight < 0)
        tests_done = 1;
    g_alpha_to_mask_context_ok = alpha_to_mask_alloc(&g_alpha_to_mask_context);
    if (!g_alpha_to_mask_context_ok) {
        trace_msg("alpha-to-mask: allocation failed\n");
        return 0;
    }
    struct AlphaToMaskContext* c = &g_alpha_to_mask_context;
    if (!tests_done) {
        g_alpha_to_mask_fd =
            sceKernelOpen(g_alpha_to_mask_log_path, resumed ? 0x209 : 0x601, 0x1FF);
        if (g_alpha_to_mask_fd < 0) {
            trace_msg("alpha-to-mask: log open failed\n");
            return 0;
        }
        alpha_to_mask_s(resumed ? "\n## resumed at "
                                : "# Alpha-to-mask on PS4 hardware, build " ALPHA_TO_MASK_BUILD
                                  "\n\n## start at ");
        alpha_to_mask_s(st.next < ALPHA_TO_MASK_NTESTS ? k_alpha_to_mask_tests[st.next].id
                                                       : "(end)");
        alpha_to_mask_s("\n\n");
        if (st.inflight >= 0) {
            alpha_to_mask_s("### ");
            alpha_to_mask_s(k_alpha_to_mask_tests[st.inflight].id);
            alpha_to_mask_s(
                ": HUNG - the app ended before its fence (crash or GPU hang); its pixel shader "
                "variant is skipped from now on\nsummary ");
            alpha_to_mask_s(k_alpha_to_mask_tests[st.inflight].id);
            alpha_to_mask_s(": HUNG\n\n");
            if (st.inflight == 0)
                st.regread_bad = 1;
            st.next = st.inflight + 1;
            st.inflight = -1;
            alpha_to_mask_state_save(&st);
        }
        alpha_to_mask_flush();
        for (int k = st.next; k < ALPHA_TO_MASK_NTESTS; k++) {
            const struct AlphaToMaskTest* t = &k_alpha_to_mask_tests[k];
            if ((st.bad_variants >> t->ps) & 1) {
                alpha_to_mask_s("### ");
                alpha_to_mask_s(t->id);
                alpha_to_mask_s(": ");
                alpha_to_mask_s(t->what);
                alpha_to_mask_s("\nSKIPPED: its pixel shader variant ");
                alpha_to_mask_s(k_alpha_to_mask_vname[t->ps]);
                alpha_to_mask_s(" crashed or hung earlier\nsummary ");
                alpha_to_mask_s(t->id);
                alpha_to_mask_s(": SKIPPED\n\n");
                st.next = k + 1;
                alpha_to_mask_state_save(&st);
                alpha_to_mask_flush();
                continue;
            }
            st.inflight = k;
            st.next = k;
            alpha_to_mask_state_save(&st);
            if (alpha_to_mask_run_test(c, t, !st.regread_bad, k) == -1) {
                alpha_to_mask_s(
                    "TIMEOUT: no fence after 2 s - GPU hung; close the app, relaunch to "
                    "continue\nsummary ");
                alpha_to_mask_s(t->id);
                alpha_to_mask_s(": HUNG\n\n");
                if (k == 0)
                    st.regread_bad = 1;
                if (!alpha_to_mask_streq(t->id, "RND"))
                    st.bad_variants |= alpha_to_mask_group(t->ps);
                st.inflight = -1;
                st.next = k + 1;
                alpha_to_mask_state_save(&st);
                alpha_to_mask_flush();
                return -1;
            }
            st.inflight = -1;
            st.next = k + 1;
            alpha_to_mask_state_save(&st);
            alpha_to_mask_flush();
        }
        alpha_to_mask_s("## all tests done\n");
        alpha_to_mask_flush();
        sceKernelClose(g_alpha_to_mask_fd);
        g_alpha_to_mask_fd = -1;
    } else {
        trace_msg("alpha-to-mask: all tests done earlier; delete alpha_to_mask.state to run them "
                  "again\n");
    }
    if (!alpha_to_mask_visual_init()) {
        trace_msg("alpha-to-mask: visual pages unavailable (allocation)\n");
        return 0;
    }
    if (alpha_to_mask_visual_capture(c, &st) < 0)
        return -1;
    alpha_to_mask_visual_set_rows();
    g_alpha_to_mask_visual_ok = 1;
    return 0;
}
