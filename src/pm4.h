#pragma once
#include <stdint.h>

/* PKT3 header: type=3, count=N body DWORDs - 1, opcode */
#define PM4_HDR(op, n) (0xC0000000u | (((n)-1u) << 16) | ((op) << 8))

#define PM4_SET_SH_REG       0x76
#define PM4_DISPATCH_DIRECT   0x15
#define PM4_RELEASE_MEM       0x49
#define PM4_ACQUIRE_MEM       0x58
#define PM4_NOP               0x10

/* SH register base for compute */
#define SH_BASE               0x2C00
#define SH(r)                 ((r) - SH_BASE)

#define mmCOMPUTE_NUM_THREAD_X            0x2E07
#define mmCOMPUTE_NUM_THREAD_Y            0x2E08
#define mmCOMPUTE_NUM_THREAD_Z            0x2E09
#define mmCOMPUTE_PGM_LO                  0x2E0C
#define mmCOMPUTE_PGM_HI                  0x2E0D
#define mmCOMPUTE_PGM_RSRC1               0x2E12
#define mmCOMPUTE_PGM_RSRC2               0x2E13
#define mmCOMPUTE_RESOURCE_LIMITS         0x2E15
#define mmCOMPUTE_STATIC_THREAD_MGMT_SE0  0x2E16
#define mmCOMPUTE_STATIC_THREAD_MGMT_SE1  0x2E17
#define mmCOMPUTE_USER_DATA_0             0x2E40

struct PM4Builder {
    uint32_t* buf;
    uint32_t  off;

    void emit(uint32_t v) { buf[off++] = v; }

    void set_sh_reg(uint32_t reg, uint32_t val) {
        emit(PM4_HDR(PM4_SET_SH_REG, 2));
        emit(SH(reg));
        emit(val);
    }

    void set_sh_reg2(uint32_t reg, uint32_t v0, uint32_t v1) {
        emit(PM4_HDR(PM4_SET_SH_REG, 3));
        emit(SH(reg));
        emit(v0);
        emit(v1);
    }

    void set_sh_reg3(uint32_t reg, uint32_t v0, uint32_t v1, uint32_t v2) {
        emit(PM4_HDR(PM4_SET_SH_REG, 4));
        emit(SH(reg));
        emit(v0);
        emit(v1);
        emit(v2);
    }

    void dispatch(uint32_t x, uint32_t y, uint32_t z) {
        emit(PM4_HDR(PM4_DISPATCH_DIRECT, 4));
        emit(x);
        emit(y);
        emit(z);
        emit(1); /* initiator: COMPUTE_SHADER_EN */
    }

    void release_mem(uint64_t addr, uint32_t value) {
        emit(PM4_HDR(PM4_RELEASE_MEM, 6));
        emit(0x528);                             /* event_type=0x28(BOP_TS), event_index=5(EOP) */
        emit((1u << 29));                        /* data_sel=1(Data32Low), int_sel=0(None) */
        emit((uint32_t)(addr & 0xFFFFFFFF));
        emit((uint32_t)(addr >> 32));
        emit(value);
        emit(0);
    }

    void nop(uint32_t n) {
        for (uint32_t i = 0; i < n; i++) {
            emit(PM4_HDR(PM4_NOP, 1));
            emit(0);
        }
    }

    uint32_t sizeBytes() const { return off * 4; }
};
