/* The CLEAR_STATE dump (test CSD): every context register of AMD's Sea Islands clear state is
 * set to a fill pattern, read back with COPY_DATA, then CLEAR_STATE runs and every register is
 * read back again. Three fill patterns, each submitted twice: a register CLEAR_STATE sets reads
 * the same value after it in all six runs; one it leaves alone reads the fill pattern again.
 * COPY_DATA writes in order (WR_CONFIRM), so after a GPU hang the first result still holding
 * CLEAR_STATE_UNWRITTEN is the read that hung; that register is skipped from then on (skip).
 * Needs alpha_to_mask_copy_reg and the alpha_to_mask_s / _x / _u log helpers. */
#pragma once
#include "clear_state_regs.h"

#define CLEAR_STATE_RUNS 6
#define CLEAR_STATE_UNWRITTEN 0xC5C5C5C5u
#define CLEAR_STATE_SKIP_WORDS ((CLEAR_STATE_NREGS + 31) / 32)
static const uint32_t k_clear_state_fill[3] = {0x00000000u, 0xFFFFFFFFu, 0x5A5A5A5Au};

static uint32_t clear_state_fill(int run) {
    return k_clear_state_fill[run % 3];
}
static int clear_state_skipped(const uint32_t* skip, int i) {
    return (skip[i / 32] >> (i % 32)) & 1;
}

/* One run: before and after hold CLEAR_STATE_NREGS dwords each. */
static void clear_state_dump_build(struct PM4Builder* b, uint32_t fill, uint32_t* before,
                                   uint32_t* after, const uint32_t* skip) {
    pm4_context_control(b);
    for (int i = 0; i < CLEAR_STATE_NREGS; i++)
        if (!clear_state_skipped(skip, i))
            pm4_set_context_reg(b, k_clear_state_regs[i].idx, fill);
    for (int i = 0; i < CLEAR_STATE_NREGS; i++)
        if (!clear_state_skipped(skip, i))
            alpha_to_mask_copy_reg(b, 0xA000u + k_clear_state_regs[i].idx, before + i);
    pm4_emit(b, pm4_type3(0x12, 1)); /* CLEAR_STATE */
    pm4_emit(b, 0);
    for (int i = 0; i < CLEAR_STATE_NREGS; i++)
        if (!clear_state_skipped(skip, i))
            alpha_to_mask_copy_reg(b, 0xA000u + k_clear_state_regs[i].idx, after + i);
}

/* After a hang: the register whose read did not complete (0 .. NREGS - 1 before CLEAR_STATE,
   NREGS .. 2 NREGS - 1 after it), -1 if every read completed. */
static int clear_state_hang_position(const uint32_t* before, const uint32_t* after,
                                     const uint32_t* skip) {
    for (int i = 0; i < CLEAR_STATE_NREGS; i++)
        if (!clear_state_skipped(skip, i) && before[i] == CLEAR_STATE_UNWRITTEN)
            return i;
    for (int i = 0; i < CLEAR_STATE_NREGS; i++)
        if (!clear_state_skipped(skip, i) && after[i] == CLEAR_STATE_UNWRITTEN)
            return CLEAR_STATE_NREGS + i;
    return -1;
}

enum { CS_SET, CS_KEPT, CS_MIXED };

/* res: CLEAR_STATE_RUNS blocks of before / after. Returns how many readable registers CLEAR_STATE
   sets to a value other than AMD's; *shad counts those other than shadPS4's defaults,
   *unreadable and *other the registers without one value. */
static int clear_state_dump_report(const uint32_t* res, const uint32_t* skip, int* shad,
                                   int* unreadable, int* other) {
    int differ = 0;
    *shad = *unreadable = *other = 0;
    alpha_to_mask_s("\n| register | name | after CLEAR_STATE | AMD clear state | shadPS4 defaults "
                    "| note |\n|---|---|---|---|---|---|\n");
    for (int i = 0; i < CLEAR_STATE_NREGS; i++) {
        int readable = !clear_state_skipped(skip, i), kind = CS_SET;
        uint32_t v = res[CLEAR_STATE_NREGS + i];
        for (int r = 0; r < CLEAR_STATE_RUNS; r++) {
            const uint32_t* blk = res + (uint32_t)r * 2 * CLEAR_STATE_NREGS;
            if (blk[i] != clear_state_fill(r))
                readable = 0;
            if (blk[CLEAR_STATE_NREGS + i] != v)
                kind = CS_MIXED;
        }
        if (kind == CS_MIXED) {
            kind = CS_KEPT;
            for (int r = 0; r < CLEAR_STATE_RUNS; r++)
                if (res[(uint32_t)r * 2 * CLEAR_STATE_NREGS + CLEAR_STATE_NREGS + i] !=
                    clear_state_fill(r))
                    kind = CS_MIXED;
        }
        uint16_t idx = k_clear_state_regs[i].idx;
        int watch = idx == 0x083 || idx == 0x08E || idx == 0x08F || idx == 0x2AA || idx == 0x30E ||
                    idx == 0x30F;
        if (!readable) {
            (*unreadable)++;
        } else if (kind != CS_SET) {
            (*other)++;
        } else {
            differ += v != k_clear_state_regs[i].amd;
            *shad += v != k_clear_state_regs[i].shad;
        }
        if (!watch && readable && kind == CS_SET && v == k_clear_state_regs[i].amd &&
            v == k_clear_state_regs[i].shad)
            continue;
        alpha_to_mask_s("| ");
        alpha_to_mask_x(0xA000u + idx, 4);
        alpha_to_mask_s(" | ");
        alpha_to_mask_s(k_clear_state_regs[i].name);
        alpha_to_mask_s(" | ");
        if (!readable)
            alpha_to_mask_s("-");
        else if (kind == CS_SET)
            alpha_to_mask_x(v, 8);
        else
            alpha_to_mask_s(kind == CS_KEPT ? "not set (keeps the fill)" : "differs per run");
        alpha_to_mask_s(" | ");
        alpha_to_mask_x(k_clear_state_regs[i].amd, 8);
        alpha_to_mask_s(" | ");
        alpha_to_mask_x(k_clear_state_regs[i].shad, 8);
        alpha_to_mask_s(" | ");
        if (clear_state_skipped(skip, i)) {
            alpha_to_mask_s("skipped: hung the GPU");
        } else if (!readable) {
            alpha_to_mask_s("COPY_DATA does not read back the fill");
        } else if (kind == CS_SET) {
            if (v != k_clear_state_regs[i].amd)
                alpha_to_mask_s("not AMD's value ");
            if (v != k_clear_state_regs[i].shad)
                alpha_to_mask_s("not shadPS4's default");
        }
        alpha_to_mask_s(" |\n");
    }
    alpha_to_mask_s("\nper run (fill: before CLEAR_STATE / after), registers where a run reads "
                    "differently:\n");
    for (int i = 0; i < CLEAR_STATE_NREGS; i++) {
        int same = res[i] == clear_state_fill(0);
        for (int r = 1; r < CLEAR_STATE_RUNS; r++) {
            const uint32_t* blk = res + (uint32_t)r * 2 * CLEAR_STATE_NREGS;
            if (blk[CLEAR_STATE_NREGS + i] != res[CLEAR_STATE_NREGS + i] ||
                blk[i] != clear_state_fill(r))
                same = 0;
        }
        if (same || clear_state_skipped(skip, i))
            continue;
        alpha_to_mask_x(0xA000u + k_clear_state_regs[i].idx, 4);
        alpha_to_mask_s(" ");
        alpha_to_mask_s(k_clear_state_regs[i].name);
        alpha_to_mask_s(":");
        for (int r = 0; r < CLEAR_STATE_RUNS; r++) {
            const uint32_t* blk = res + (uint32_t)r * 2 * CLEAR_STATE_NREGS;
            alpha_to_mask_s(" ");
            alpha_to_mask_x(clear_state_fill(r), 8);
            alpha_to_mask_s(": ");
            alpha_to_mask_x(blk[i], 8);
            alpha_to_mask_s(" / ");
            alpha_to_mask_x(blk[CLEAR_STATE_NREGS + i], 8);
        }
        alpha_to_mask_s("\n");
    }
    alpha_to_mask_s("\nall registers after CLEAR_STATE (run 1):\n");
    for (int i = 0; i < CLEAR_STATE_NREGS; i++) {
        alpha_to_mask_x(0xA000u + k_clear_state_regs[i].idx, 4);
        alpha_to_mask_s(" ");
        if (clear_state_skipped(skip, i))
            alpha_to_mask_s("skipped   ");
        else
            alpha_to_mask_x(res[CLEAR_STATE_NREGS + i], 8);
        alpha_to_mask_s(" ");
        alpha_to_mask_s(k_clear_state_regs[i].name);
        alpha_to_mask_s("\n");
    }
    return differ;
}
