// mask_readback_test.cpp
//
// Drop-in test for the existing IMAGE_STORE_MIP harness (same file/headers as
// hello_world/main.cpp). It isolates the wave-mask-as-value readback that
// shadPS4 currently mishandles (the operation underneath v_cmp_lg_u64 on the
// RE3/RE4 mask-vs-VCC idiom, and homebrew V_MOV_B32 vN, sM).
//
// One wave of 64 active lanes (NumThreadX = 64; v0 = lane id, since
// v_thread_cnt defaults to 1 -> TIDIG_COMP_CNT = 0):
//   1. v_cmp_lt_u32  s[8:9], v0, 48   -> SGPR-pair wave mask, bit n = (lane n < 48)
//   2. v_cmp_lt_u32  vcc,    v0, 48   -> same mask written to VCC
//   3. v_mov_b32 v4, s8 / v5, s9      -> read the SGPR mask back AS A VALUE
//   4. v_mov_b32 v6, vcc_lo / v7, vcc_hi -> read VCC back AS A VALUE
//   5. store v4..v7 -> dst[0..3]
//
// Predicate is (lane < 48), not (lane < 32), on purpose: it makes the high
// dword reveal the wave width as well as exercising the readback.
//
//   real PS4 (wave64, lanes 0-47 pass):   lo = FFFFFFFF, hi = 0000FFFF
//   a wave32 host (only lanes 0-31 exist): lo = FFFFFFFF, hi = 00000000
//
// So expected dst on real PS4:
//   dst[0] = 0xFFFFFFFF  (sgpr mask lo)
//   dst[1] = 0x0000FFFF  (sgpr mask hi)  <- nonzero high word == wave64
//   dst[2] = 0xFFFFFFFF  (vcc lo)
//   dst[3] = 0x0000FFFF  (vcc hi)
//
// shadPS4 today: v_mov_b32 reads the *packed* SSA var (plain s8 / VccLoTag),
// which the v_cmp never wrote (it wrote ThreadBitScalar{8} / VccFlagTag), so
// the values come back undefined/garbage rather than the mask. Both the SGPR
// path (dst[0..1]) and the VCC path (dst[2..3]) are exercised because they
// resolve through different code (GetScalarRegister vs GetVccLo).
//
// On a subgroup-64 host (AMD / Steam Deck) the ballot-materialization fix makes
// these match the PS4 values exactly. On a subgroup-32 host (NVIDIA) the high
// word reads back 0x00000000 because the wave is genuinely 32 lanes there
// (the 64-lane PS4 wave is split across two subgroups) -- which this test now
// makes visible rather than hiding.

static t_cs_shader_test make_cs_mask_readback_test(t_linear_alloc* linear_dmem,
                                                   CmdBuf* cmd_buf) {

    t_cs_shader_test t = {};
    t.cmd_buf = cmd_buf;

    auto cb_instruction = [](ShaderBuilder* shader) {
        // bit n = (lane n < 48) -> 0x0000FFFF_FFFFFFFF on a 64-lane wave
        shader->VOP3c_OP(VOP3_CMP_LT_U32, MAKE_SGPR(8),
                         MAKE_VGPR(0), MAKE_IMM_INT(48));     // s[8:9]
        shader->VOP3c_OP(VOP3_CMP_LT_U32, MAKE_SGPR(VCC_LO),
                         MAKE_VGPR(0), MAKE_IMM_INT(48));     // vcc

        // read both masks back as plain 32-bit values (the mishandled op)
        shader->V_MOV_B32(MAKE_VGPR(4), MAKE_SGPR(8));        // sgpr mask lo
        shader->V_MOV_B32(MAKE_VGPR(5), MAKE_SGPR(9));        // sgpr mask hi
        shader->V_MOV_B32(MAKE_VGPR(6), MAKE_SGPR(VCC_LO));   // vcc lo
        shader->V_MOV_B32(MAKE_VGPR(7), MAKE_SGPR(VCC_HI));   // vcc hi
    };

    // load_op = 0: skip the prologue load (it would clobber v0 = lane id);
    // keep the store epilogue, which writes v4..v7 to the dst V#.
    auto shader = make_cs_shader(0, MUBUF_BUFFER_STORE_FORMAT_XYZW, cb_instruction);

    // one full wave; v_thread_cnt defaults to 1 so v0 = thread id x (lane id).
    shader->NumThreadX = 64;
    shader->NumThreadY = 1;
    shader->NumThreadZ = 1;
    shader->sgpr_count = 10;   // s0-3 src V#, s4-7 dst V#, s8-9 mask pair

    t.shader_ptr = linear_dmem->alloc(shader->GetByteSize(), 256);
    t.regs = shader->ExportCs(t.shader_ptr);
    delete shader;

    t.src = (uint*)linear_dmem->alloc(16 * 4, 4);
    t.dst = (uint*)linear_dmem->alloc(16 * 4, 4);

    for (int i = 0; i < 16; i++) {
        t.src[i] = 0;
        t.dst[i] = 0xCDCDCDCD;   // poison so an untouched dst is obvious
    }

    t.src_sel.x = DSEL_R; t.src_sel.y = DSEL_G; t.src_sel.z = DSEL_B; t.src_sel.w = DSEL_A;
    t.dst_sel = t.src_sel;

    // V# layout matches make_cs_shader: src at user-data 0..3, dst at 4..7.
    VSharpResource4 src_vsharp = {};
    VSharpResource4 dst_vsharp = {};

    src_vsharp.setVMemoryType(VMemoryTypeRO);
    src_vsharp.setVMemoryPtrs(t.src, 0, sizeof(uint) * 4, 1);
    src_vsharp.setChannelOrder(t.src_sel.x, t.src_sel.y, t.src_sel.z, t.src_sel.w);
    src_vsharp.nfmt = BUF_NUM_FORMAT_UINT;
    src_vsharp.dfmt = BUF_DATA_FORMAT_32_32_32_32;

    dst_vsharp.setVMemoryType(VMemoryTypeSC);
    dst_vsharp.setVMemoryPtrs(t.dst, 0, sizeof(uint) * 4, 1);
    dst_vsharp.setChannelOrder(t.dst_sel.x, t.dst_sel.y, t.dst_sel.z, t.dst_sel.w);
    dst_vsharp.nfmt = BUF_NUM_FORMAT_UINT;
    dst_vsharp.dfmt = BUF_DATA_FORMAT_32_32_32_32;

    cmd_buf->SetCsShader(&t.regs);
    cmd_buf->SetUserDataCsRegs(0, 4, &src_vsharp);
    cmd_buf->SetUserDataCsRegs(4, 4, &dst_vsharp);
    cmd_buf->DispatchDirect(1, 1, 1);

    auto on_after = [](t_cs_shader_test* t) {
        const uint exp[4] = { 0xFFFFFFFF, 0x0000FFFF, 0xFFFFFFFF, 0x0000FFFF };
        const char* lbl[4] = { "sgpr_mask_lo", "sgpr_mask_hi", "vcc_lo", "vcc_hi" };
        printf("[mask_readback] wave=64, predicate (lane < 48)\n");
        for (int i = 0; i < 4; i++) {
            printf("  %-12s = %08x  expected %08x -> %s\n",
                   lbl[i], t->dst[i], exp[i], PASS[t->dst[i] == exp[i]]);
        }
        printf("  (hi word 0000ffff = 64-lane wave; 00000000 = 32-lane wave)\n");
    };

    test_after_action.push_back({ on_after, t });
    return t;
}

// ---- invocation -------------------------------------------------------------
// In main(), alongside the other make_cs_*_test() calls (before the submit /
// do_test_after_action()), add:
//
//     make_cs_mask_readback_test(&linear_dmem, cmd_buf);
//
// Then build with the usual OpenOrbis + create-fself flow. Run on the PS4 to
// capture ground truth, and on shadPS4 (current vs patched) to compare.
