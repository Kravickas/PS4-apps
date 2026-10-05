/* Generated with the pass headers: the OPCODE_TEST 5 (ps_cvt) or 6 (ps_mod) passes, one binary (own
   hash) a float mode, and their PGM_RSRC1 without FLOAT_MODE (each pass adds k_opt5_float_mode[m]
   << 12). */
#pragma once
#if OPCODE_TEST == 6
#define OPT5_PASS_RSRC1 0xC4u
static const uint32_t* const k_opt5_pass[20] = {
    ps_mod_0_binary,  ps_mod_1_binary,  ps_mod_2_binary,  ps_mod_3_binary,  ps_mod_4_binary,
    ps_mod_5_binary,  ps_mod_6_binary,  ps_mod_7_binary,  ps_mod_8_binary,  ps_mod_9_binary,
    ps_mod_10_binary, ps_mod_11_binary, ps_mod_12_binary, ps_mod_13_binary, ps_mod_14_binary,
    ps_mod_15_binary, ps_mod_16_binary, ps_mod_17_binary, ps_mod_18_binary, ps_mod_19_binary};
static const uint32_t k_opt5_pass_size[20] = {
    sizeof(ps_mod_0_binary),  sizeof(ps_mod_1_binary),  sizeof(ps_mod_2_binary),
    sizeof(ps_mod_3_binary),  sizeof(ps_mod_4_binary),  sizeof(ps_mod_5_binary),
    sizeof(ps_mod_6_binary),  sizeof(ps_mod_7_binary),  sizeof(ps_mod_8_binary),
    sizeof(ps_mod_9_binary),  sizeof(ps_mod_10_binary), sizeof(ps_mod_11_binary),
    sizeof(ps_mod_12_binary), sizeof(ps_mod_13_binary), sizeof(ps_mod_14_binary),
    sizeof(ps_mod_15_binary), sizeof(ps_mod_16_binary), sizeof(ps_mod_17_binary),
    sizeof(ps_mod_18_binary), sizeof(ps_mod_19_binary)};
#else
#define OPT5_PASS_RSRC1 0xC4u
static const uint32_t* const k_opt5_pass[5] = {ps_cvt_0_binary, ps_cvt_1_binary, ps_cvt_2_binary,
                                               ps_cvt_3_binary, ps_cvt_4_binary};
static const uint32_t k_opt5_pass_size[5] = {sizeof(ps_cvt_0_binary), sizeof(ps_cvt_1_binary),
                                             sizeof(ps_cvt_2_binary), sizeof(ps_cvt_3_binary),
                                             sizeof(ps_cvt_4_binary)};
#endif
