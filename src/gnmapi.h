#pragma once

#include <orbis/libkernel.h>

union COMPUTE_PGM_RSRC1 {
    uint32_t raw;
    struct {
        uint VGPRS : 6;
        uint SGPRS : 4;
        uint PRIORITY : 2;
        uint FLOAT_MODE : 8;
        uint PRIV : 1;
        uint DX10_CLAMP : 1;
        uint DEBUG_MODE : 1;
        uint IEEE_MODE : 1;
        uint BULKY : 1;
        uint CDBG_USER : 1;
        uint RESERVED0 : 6;
    };
};
static_assert(sizeof(COMPUTE_PGM_RSRC1) == 4);

union COMPUTE_PGM_RSRC2 {
    uint32_t raw;
    struct {
        uint SCRATCH_EN : 1;
        uint USER_SGPR : 5;
        uint TRAP_PRESENT : 1;
        uint TGID_X_EN : 1;
        uint TGID_Y_EN : 1;
        uint TGID_Z_EN : 1;
        uint TG_SIZE_EN : 1;
        uint TIDIG_COMP_CNT : 2;
        uint EXCP_EN_MSB : 2;
        uint LDS_SIZE : 9;
        uint EXCP_EN : 7;
        uint RESERVED0 : 1;
    };
};
static_assert(sizeof(COMPUTE_PGM_RSRC2) == 4);

union SPI_SHADER_PGM_RSRC1_VS {
    uint32_t raw;
    struct {
        uint VGPRS : 6;
        uint SGPRS : 4;
        uint PRIORITY : 2;
        uint FLOAT_MODE : 8;
        uint PRIV : 1;
        uint DX10_CLAMP : 1;
        uint DEBUG_MODE : 1;
        uint IEEE_MODE : 1;
        uint VGPR_COMP_CNT : 2;
        uint CU_GROUP_ENABLE : 1;
        uint CACHE_CTL : 3;
        uint CDBG_USER : 1;
        uint RESERVED0 : 1;
    };
};
static_assert(sizeof(SPI_SHADER_PGM_RSRC1_VS) == 4);

union SPI_SHADER_PGM_RSRC2_VS {
    uint32_t raw;
    struct {
        uint SCRATCH_EN : 1;
        uint USER_SGPR : 5;
        uint TRAP_PRESENT : 1;
        uint OC_LDS_EN : 1;
        uint SO_BASE0_EN : 1;
        uint SO_BASE1_EN : 1;
        uint SO_BASE2_EN : 1;
        uint SO_BASE3_EN : 1;
        uint SO_EN : 1;
        uint EXCP_EN : 9;
        uint RESERVED0 : 2;
        uint DISPATCH_DRAW_EN : 1;
        uint RESERVED1 : 7;
    };
};
static_assert(sizeof(SPI_SHADER_PGM_RSRC2_VS) == 4);

union SPI_VS_OUT_CONFIG {
    uint32_t raw;
    struct {
        uint RESERVED0 : 1;
        uint VS_EXPORT_COUNT : 5;
        uint VS_HALF_PACK : 1;
        uint RESERVED1 : 1;
        uint RESERVED2 : 5;
        uint RESERVED3 : 19;
    };
};
static_assert(sizeof(SPI_VS_OUT_CONFIG) == 4);

// SPI_SHADER_FORMAT
#define SPI_SHADER_NONE      0x0 
#define SPI_SHADER_1COMP     0x1 
#define SPI_SHADER_2COMP     0x2 
#define SPI_SHADER_4COMPRESS 0x3 
#define SPI_SHADER_4COMP     0x4 

union SPI_SHADER_POS_FORMAT {
    uint32_t raw;
    struct {
        uint POS0_EXPORT_FORMAT : 4;
        uint POS1_EXPORT_FORMAT : 4;
        uint POS2_EXPORT_FORMAT : 4;
        uint POS3_EXPORT_FORMAT : 4;
        uint RESERVED0 : 16;
    };
};
static_assert(sizeof(SPI_SHADER_POS_FORMAT) == 4);

union PA_CL_VS_OUT_CNTL {
    uint32_t raw;
    struct {
        uint CLIP_DIST_ENA_0 : 1;
        uint CLIP_DIST_ENA_1 : 1;
        uint CLIP_DIST_ENA_2 : 1;
        uint CLIP_DIST_ENA_3 : 1;
        uint CLIP_DIST_ENA_4 : 1;
        uint CLIP_DIST_ENA_5 : 1;
        uint CLIP_DIST_ENA_6 : 1;
        uint CLIP_DIST_ENA_7 : 1;
        uint CULL_DIST_ENA_0 : 1;
        uint CULL_DIST_ENA_1 : 1;
        uint CULL_DIST_ENA_2 : 1;
        uint CULL_DIST_ENA_3 : 1;
        uint CULL_DIST_ENA_4 : 1;
        uint CULL_DIST_ENA_5 : 1;
        uint CULL_DIST_ENA_6 : 1;
        uint CULL_DIST_ENA_7 : 1;
        uint USE_VTX_POINT_SIZE : 1;
        uint USE_VTX_EDGE_FLAG : 1;
        uint USE_VTX_RENDER_TARGET_INDX : 1;
        uint USE_VTX_VIEWPORT_INDX : 1;
        uint USE_VTX_KILL_FLAG : 1;
        uint VS_OUT_MISC_VEC_ENA : 1;
        uint VS_OUT_CCDIST0_VEC_ENA : 1;
        uint VS_OUT_CCDIST1_VEC_ENA : 1;
        uint VS_OUT_MISC_SIDE_BUS_ENA : 1;
        uint USE_VTX_GS_CUT_FLAG : 1;
        uint USE_VTX_LINE_WIDTH : 1;
        uint RESERVED0 : 5;
    };
};
static_assert(sizeof(PA_CL_VS_OUT_CNTL) == 4);

union SPI_SHADER_PGM_RSRC1_PS {
    uint32_t raw;
    struct {
        uint VGPRS : 6;
        uint SGPRS : 4;
        uint PRIORITY : 2;
        uint FLOAT_MODE : 8;
        uint PRIV : 1;
        uint DX10_CLAMP : 1;
        uint DEBUG_MODE : 1;
        uint IEEE_MODE : 1;
        uint CU_GROUP_DISABLE : 1;
        uint CACHE_CTL : 3;
        uint CDBG_USER : 1;
        uint RESERVED0 : 3;
    };
};
static_assert(sizeof(SPI_SHADER_PGM_RSRC1_PS) == 4);

union SPI_SHADER_PGM_RSRC2_PS {
    uint32_t raw;
    struct {
        uint SCRATCH_EN : 1;
        uint USER_SGPR : 5;
        uint TRAP_PRESENT : 1;
        uint WAVE_CNT_EN : 1;
        uint EXTRA_LDS_SIZE : 8;
        uint EXCP_EN : 9;
        uint RESERVED0 : 7;
    };
};
static_assert(sizeof(SPI_SHADER_PGM_RSRC2_PS) == 4);

union SPI_SHADER_Z_FORMAT {
    uint32_t raw;
    struct {
        uint Z_EXPORT_FORMAT : 4;
        uint RESERVED0 : 28;
    };
};
static_assert(sizeof(SPI_SHADER_Z_FORMAT) == 4);

union SPI_SHADER_COL_FORMAT {
    uint32_t raw;
    struct {
        uint COL0_EXPORT_FORMAT : 4;
        uint COL1_EXPORT_FORMAT : 4;
        uint COL2_EXPORT_FORMAT : 4;
        uint COL3_EXPORT_FORMAT : 4;
        uint COL4_EXPORT_FORMAT : 4;
        uint COL5_EXPORT_FORMAT : 4;
        uint COL6_EXPORT_FORMAT : 4;
        uint COL7_EXPORT_FORMAT : 4;
    };
};
static_assert(sizeof(SPI_SHADER_COL_FORMAT) == 4);

union SPI_PS_INPUT_ENA {
    uint32_t raw;
    struct {
        uint PERSP_SAMPLE_ENA : 1;
        uint PERSP_CENTER_ENA : 1;
        uint PERSP_CENTROID_ENA : 1;
        uint PERSP_PULL_MODEL_ENA : 1;
        uint LINEAR_SAMPLE_ENA : 1;
        uint LINEAR_CENTER_ENA : 1;
        uint LINEAR_CENTROID_ENA : 1;
        uint LINE_STIPPLE_TEX_ENA : 1;
        uint POS_X_FLOAT_ENA : 1;
        uint POS_Y_FLOAT_ENA : 1;
        uint POS_Z_FLOAT_ENA : 1;
        uint POS_W_FLOAT_ENA : 1;
        uint FRONT_FACE_ENA : 1;
        uint ANCILLARY_ENA : 1;
        uint SAMPLE_COVERAGE_ENA : 1;
        uint POS_FIXED_PT_ENA : 1;
        uint RESERVED0 : 16;
    };
};
static_assert(sizeof(SPI_PS_INPUT_ENA) == 4);

union SPI_PS_INPUT_ADDR {
    uint32_t raw;
    struct {
        uint PERSP_SAMPLE_ENA : 1;
        uint PERSP_CENTER_ENA : 1;
        uint PERSP_CENTROID_ENA : 1;
        uint PERSP_PULL_MODEL_ENA : 1;
        uint LINEAR_SAMPLE_ENA : 1;
        uint LINEAR_CENTER_ENA : 1;
        uint LINEAR_CENTROID_ENA : 1;
        uint LINE_STIPPLE_TEX_ENA : 1;
        uint POS_X_FLOAT_ENA : 1;
        uint POS_Y_FLOAT_ENA : 1;
        uint POS_Z_FLOAT_ENA : 1;
        uint POS_W_FLOAT_ENA : 1;
        uint FRONT_FACE_ENA : 1;
        uint ANCILLARY_ENA : 1;
        uint SAMPLE_COVERAGE_ENA : 1;
        uint POS_FIXED_PT_ENA : 1;
        uint RESERVED0 : 16;
    };
};
static_assert(sizeof(SPI_PS_INPUT_ADDR) == 4);

union SPI_PS_IN_CONTROL {
    uint32_t raw;
    struct {
        uint NUM_INTERP : 6;
        uint PARAM_GEN : 1;
        uint RESERVED0 : 7;
        uint BC_OPTIMIZE_DISABLE : 1;
        uint RESERVED1 : 1;
        uint RESERVED2 : 16;
    };
};
static_assert(sizeof(SPI_PS_IN_CONTROL) == 4);

union SPI_BARYC_CNTL {
    uint32_t raw;
    struct {
        uint PERSP_CENTER_CNTL : 1;
        uint RESERVED0 : 3;
        uint PERSP_CENTROID_CNTL : 1;
        uint RESERVED1 : 3;
        uint LINEAR_CENTER_CNTL : 1;
        uint RESERVED2 : 3;
        uint LINEAR_CENTROID_CNTL : 1;
        uint RESERVED3 : 3;
        uint POS_FLOAT_LOCATION : 2;
        uint RESERVED4 : 2;
        uint POS_FLOAT_ULC : 1;
        uint RESERVED5 : 3;
        uint FRONT_FACE_ALL_BITS : 1;
        uint RESERVED6 : 7;
    };
};
static_assert(sizeof(SPI_BARYC_CNTL) == 4);

union DB_SHADER_CONTROL {
    uint32_t raw;
    struct {
        uint Z_EXPORT_ENABLE : 1;
        uint STENCIL_TEST_VAL_EXPORT_ENABLE : 1;
        uint STENCIL_OP_VAL_EXPORT_ENABLE : 1;
        uint RESERVED0 : 1;
        uint Z_ORDER : 2;
        uint KILL_ENABLE : 1;
        uint COVERAGE_TO_MASK_ENABLE : 1;
        uint MASK_EXPORT_ENABLE : 1;
        uint EXEC_ON_HIER_FAIL : 1;
        uint EXEC_ON_NOOP : 1;
        uint ALPHA_TO_MASK_DISABLE : 1;
        uint DEPTH_BEFORE_SHADER : 1;
        uint CONSERVATIVE_Z_EXPORT : 2;
        uint DUAL_QUAD_DISABLE : 1;
        uint RESERVED1 : 16;
    };
};
static_assert(sizeof(DB_SHADER_CONTROL) == 4);

union CB_SHADER_MASK {
    uint32_t raw;
    struct {
        uint OUTPUT0_ENABLE : 4;
        uint OUTPUT1_ENABLE : 4;
        uint OUTPUT2_ENABLE : 4;
        uint OUTPUT3_ENABLE : 4;
        uint OUTPUT4_ENABLE : 4;
        uint OUTPUT5_ENABLE : 4;
        uint OUTPUT6_ENABLE : 4;
        uint OUTPUT7_ENABLE : 4;
    };
};
static_assert(sizeof(CB_SHADER_MASK) == 4);

namespace Gnm {

    struct CsStageRegisters {
        uint computePgmLo;
        uint computePgmHi;
        COMPUTE_PGM_RSRC1 computePgmRsrc1;
        COMPUTE_PGM_RSRC2 computePgmRsrc2;
        uint computeNumThreadX;
        uint computeNumThreadY;
        uint computeNumThreadZ;
    };
    static_assert(sizeof(CsStageRegisters) == 4 * 7);

    struct VsStageRegisters {
        uint spiShaderPgmLoVs;
        uint spiShaderPgmHiVs;
        SPI_SHADER_PGM_RSRC1_VS spiShaderPgmRsrc1Vs;
        SPI_SHADER_PGM_RSRC2_VS spiShaderPgmRsrc2Vs;
        SPI_VS_OUT_CONFIG       spiVsOutConfig;
        SPI_SHADER_POS_FORMAT   spiShaderPosFormat;
        PA_CL_VS_OUT_CNTL       paClVsOutCntl;
    };
    static_assert(sizeof(VsStageRegisters) == 4 * 7);

    struct PsStageRegisters {
        uint spiShaderPgmLoPs;
        uint spiShaderPgmHiPs;
        SPI_SHADER_PGM_RSRC1_PS spiShaderPgmRsrc1Ps;
        SPI_SHADER_PGM_RSRC2_PS spiShaderPgmRsrc2Ps;
        SPI_SHADER_Z_FORMAT     spiShaderZFormat;
        SPI_SHADER_COL_FORMAT   spiShaderColFormat;
        SPI_PS_INPUT_ENA        spiPsInputEna;
        SPI_PS_INPUT_ADDR       spiPsInputAddr;
        SPI_PS_IN_CONTROL       spiPsInControl;
        SPI_BARYC_CNTL          spiBarycCntl;
        DB_SHADER_CONTROL       dbShaderControl;
        CB_SHADER_MASK          cbShaderMask;
    };
    static_assert(sizeof(PsStageRegisters) == 4 * 12);

    extern "C" {
        uint sceGnmDrawInitDefaultHardwareState(uint* cmdBuffer, uint numDwords);                     //numDwords=0x100
        uint sceGnmDrawInitDefaultHardwareState175(uint* cmdBuffer, uint numDwords);                  //numDwords=0x100
        uint sceGnmDrawInitDefaultHardwareState200(uint* cmdBuffer, uint numDwords);                  //numDwords=0x100
        uint sceGnmDrawInitDefaultHardwareState350(uint* cmdBuffer, uint numDwords);                  //numDwords=0x100
        uint sceGnmDispatchInitDefaultHardwareState(uint* cmdBuffer, uint numDwords);                 //numDwords=0x100
        uint sceGnmDrawInitToDefaultContextState(uint* cmdBuffer, uint numDwords);                    //numDwords=0x20
        uint sceGnmDrawInitToDefaultContextState400(uint* cmdBuffer, uint numDwords);                 //numDwords=0x100
        int  sceGnmInsertWaitFlipDone(uint* cmdBuffer, uint numDwords, int hVideo, int buffer_index); //numDwords=7
        int  sceGnmSetCsShader(uint* cmdBuffer, uint numDwords, void* csRegs);                        //numDwords=25
        int  sceGnmSetVsShader(uint* cmdBuffer, uint numDwords, void* vsRegs, uint shaderModifier);   //numDwords=29
        int  sceGnmSetPsShader(uint* cmdBuffer, uint numDwords, void* psRegs);                        //numDwords=40
        int  sceGnmSetPsShader350(uint* cmdBuffer, uint numDwords, void* psRegs);                     //numDwords=40
        int  sceGnmSetEmbeddedVsShader(uint* cmdBuffer, uint numDwords, uint shaderId,
            uint shaderModifier);                                           //numDwords=29
        int  sceGnmSetEmbeddedPsShader(uint* cmdBuffer, uint numDwords, uint shaderId);               //numDwords=40

        int  sceGnmDispatchDirect(uint* cmdBuffer, uint numDwords,
            uint X, uint Y, uint Z, uint modifier); //numDwords=9

        int  sceGnmDrawIndexAuto(uint* cmdBuffer, uint numDwords,
            uint indexCount, uint modifier); //numDwords=7

        int  sceGnmDrawIndex(uint* cmdBuffer, uint numDwords,
            uint indexCount,
            void* indexAddr,
            uint modifier); //numDwords=10

        int  sceGnmDrawIndexOffset(uint* cmdBuffer, uint numDwords,
            uint indexOffset, uint indexCount,
            uint modifier); //numDwords=9

        int sceGnmSubmitCommandBuffers(
            uint count,
            void** dcbGpuAddrs,
            uint* dcbSizesInBytes,
            void** ccbGpuAddrs,
            uint* ccbSizesInBytes
        );

        int sceGnmSubmitAndFlipCommandBuffers(
            uint count,
            void** dcbGpuAddrs,
            uint* dcbSizesInBytes,
            void** ccbGpuAddrs,
            uint* ccbSizesInBytes,
            int hVideo,
            int buffer_index,
            int flipMode,
            uint64_t flipArg
        );

        int sceGnmRequestFlipAndSubmitDone(
            void* dcbGpuAddr,
            uint dcbSizesInByte,
            int hVideo,
            int buffer_index,
            int flipMode,
            uint64_t flipArg
        );

        int sceGnmSubmitDone();

        int sceGnmAddEqEvent(OrbisKernelEqueue eq, int event_id, void* udata);
        int sceGnmDeleteEqEvent(OrbisKernelEqueue eq, int event_id);

        int sceGnmMapComputeQueue(uint globalPipeId, uint queueId, void* ringBaseAddr, uint ringSizeInDW, void* readPtrAddr);
        int sceGnmMapComputeQueueWithPriority(uint globalPipeId, uint queueId, void* ringBaseAddr, uint ringSizeInDW, void* readPtrAddr, uint pipePriority);
        void sceGnmUnmapComputeQueue(int vqueueId);

        void sceGnmDingDong(uint vqueueId, uint nextStartOffsetInDw);
    }
}
