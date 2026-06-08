#pragma once

#include <orbis/libkernel.h>
#include "gnmapi.h"
#include "render_target.h"


#define IT_NOP               0x10
#define IT_CLEAR_STATE       0x12
#define IT_INDEX_BUFFER_SIZE 0x13
#define IT_INDEX_BASE        0x26
#define IT_DRAW_INDEX_2      0x27
#define IT_INDEX_TYPE        0x2a
#define IT_DRAW_INDEX_AUTO   0x2d
#define IT_WAIT_REG_MEM      0x3c
#define IT_DMA_DATA          0x50
#define IT_DUMP_CONST_RAM    0x83
#define IT_WRITE_DATA        0x37
#define IT_COPY_DATA         0x40
#define IT_PFP_SYNC_ME       0x42
#define IT_SET_CONFIG_REG    0x68
#define IT_SET_CONTEXT_REG   0x69
#define IT_SET_SH_REG        0x76
#define IT_SET_UCONFIG_REG   0x79


#define PM4_HEADER_BUILD(lenDw, op) ( ((uint32_t)((((uint16_t)(lenDw)-2) << 16) | 0xC0000000)) | ((uint8_t)(op)) << 8 )

struct PM4_TYPE_3_HEADER {
    uint predicate : 1;
    uint shaderType : 1;
    uint reserved : 6;
    uint opcode : 8;
    uint count : 14;
    uint type : 2;
};

union PA_CL_VTE_CNTL {
    uint32_t raw;

    struct {
        uint VPORT_X_SCALE_ENA : 1;
        uint VPORT_X_OFFSET_ENA : 1;
        uint VPORT_Y_SCALE_ENA : 1;
        uint VPORT_Y_OFFSET_ENA : 1;
        uint VPORT_Z_SCALE_ENA : 1;
        uint VPORT_Z_OFFSET_ENA : 1;
        uint RESERVED0 : 2;
        uint VTX_XY_FMT : 1;
        uint VTX_Z_FMT : 1;
        uint VTX_W0_FMT : 1;
        uint PERFCOUNTER_REF : 1;
        uint RESERVED1 : 20;
    };

    static PA_CL_VTE_CNTL getDefault() {
        return { .raw = 0x43F };
    }
};
static_assert(sizeof(PA_CL_VTE_CNTL) == 4);

class CmdBuf
{
public:
    off_t size;
    off_t dmem_ofs;
    void* dmem_ptr;
    uint* curr_ptr;
    int   neo_mode;
public:
    CmdBuf(off_t size,int neo_mode)
    {
        this->size = size;
        this->neo_mode = neo_mode;
    };

    uint get_stream_size() {
        return ((uint64_t)curr_ptr - (uint64_t)dmem_ptr);
    };

    int alloc() {

        int r;

        off_t aligned_size = (this->size + 16 * 1024 - 1) & ~(16 * 1024 - 1);

        this->dmem_ofs = 0;

        r = sceKernelAllocateMainDirectMemory(
            (size_t)aligned_size,
            (size_t)16 * 1024,
            WB_ONION,
            &this->dmem_ofs);

        printf("sceKernelAllocateMainDirectMemory->%d %d\n", r, this->dmem_ofs);

        if (r != 0) return r;

        this->dmem_ptr = 0;

        r = sceKernelMapDirectMemory(
            &this->dmem_ptr,
            aligned_size,
            VM_PROT_READ | VM_PROT_WRITE | VM_PROT_GPU_ALL,
            0,
            this->dmem_ofs,
            16 * 1024);

        printf("sceKernelMapDirectMemory->%d\n", r);

        if (r != 0) {

            sceKernelReleaseDirectMemory(this->dmem_ofs, aligned_size);
            this->dmem_ofs = 0;

            return r;
        }

        this->curr_ptr = (uint*)this->dmem_ptr;

        return 0;
    };

    void reset()
    {
        this->curr_ptr = (uint*)this->dmem_ptr;
    };

    void InitDefault() {
        int i = Gnm::sceGnmDrawInitDefaultHardwareState350(this->curr_ptr, 0x100);
        this->curr_ptr = this->curr_ptr + i;
    };

    void* Nop(uint count_dw)
    {
        if (count_dw < 2) count_dw = 2;

        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(count_dw, IT_NOP);

        this->curr_ptr = cmds + count_dw;

        return &cmds[1];
    };

    void NopOverflow()
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD((0x3FFF + 2), IT_NOP);

        this->curr_ptr = cmds + 1;
    }

    void SetEmbeddedVsShader() {
        Gnm::sceGnmSetEmbeddedVsShader(this->curr_ptr, 29, 0, 0);
        this->curr_ptr = this->curr_ptr + 29;
    };

    void SetEmbeddedPsShader() {
        Gnm::sceGnmSetEmbeddedPsShader(this->curr_ptr, 40, 0);
        this->curr_ptr = this->curr_ptr + 40;
    };

    void SetCsShader(Gnm::CsStageRegisters* csRegs)
    {
        Gnm::sceGnmSetCsShader(this->curr_ptr, 25, csRegs);
        this->curr_ptr = this->curr_ptr + 25;
    };

    void SetVsShader(Gnm::VsStageRegisters* vsRegs)
    {
        Gnm::sceGnmSetVsShader(this->curr_ptr, 29, vsRegs, 0);
        this->curr_ptr = this->curr_ptr + 29;
    };

    void SetPsShader(Gnm::PsStageRegisters* psRegs)
    {
        Gnm::sceGnmSetPsShader350(this->curr_ptr, 40, psRegs);
        this->curr_ptr = this->curr_ptr + 40;
    };

    void SetCfgReg(uint reg_offset, uint reg_data)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(3, IT_SET_CONFIG_REG);
        cmds[1] = reg_offset - 0x2000;
        cmds[2] = reg_data;

        this->curr_ptr = cmds + 3;
    };

    void SetCtxReg(uint reg_offset, uint reg_data)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(3, IT_SET_CONTEXT_REG);
        cmds[1] = reg_offset - 0xA000;
        cmds[2] = reg_data;

        this->curr_ptr = cmds + 3;
    };

    void SetCtxRegs(uint reg_offset, uint reg_count, void* reg_data)
    {
        uint* cmds = this->curr_ptr;
        uint  len = 2 + reg_count;

        cmds[0] = PM4_HEADER_BUILD(len, IT_SET_CONTEXT_REG);
        cmds[1] = reg_offset - 0xA000;

        memcpy(&cmds[2], reg_data, reg_count * sizeof(uint));

        this->curr_ptr = cmds + len;
    };

    void SetShReg(uint reg_offset, uint reg_data)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(3, IT_SET_SH_REG);
        cmds[1] = reg_offset - 0x2C00;
        cmds[2] = reg_data;

        this->curr_ptr = cmds + 3;
    };

    void SetShRegs(uint reg_offset, uint reg_count, void* reg_data)
    {
        uint* cmds = this->curr_ptr;
        uint  len = 2 + reg_count;

        cmds[0] = PM4_HEADER_BUILD(len, IT_SET_SH_REG);
        cmds[1] = reg_offset - 0x2C00;

        memcpy(&cmds[2], reg_data, reg_count * sizeof(uint));

        this->curr_ptr = cmds + len;
    };

    void SetUserDataVs(uint index, uint data) {
        // mmSPI_SHADER_USER_DATA_VS_0 0x2C4C
        SetShReg(0x2C4C + index, data);
    };

    void SetUserDataVsPtr(uint index, void* data) {
        // mmSPI_SHADER_USER_DATA_VS_0 0x2C4C
        SetShRegs(0x2C4C + index, 2, &data);
    };

    void SetUserDataVsRegs(uint index, uint count, void* data) {
        // mmSPI_SHADER_USER_DATA_VS_0 0x2C4C 
        SetShRegs(0x2C4C + index, count, data);
    };

    void SetUserDataPs(uint index, uint data) {
        // mmSPI_SHADER_USER_DATA_PS_0 0x2C0C 
        SetShReg(0x2C0C + index, data);
    };

    void SetUserDataPsPtr(uint index, void* data) {
        // mmSPI_SHADER_USER_DATA_PS_0 0x2C0C 
        SetShRegs(0x2C0C + index, 2, &data);
    };

    void SetUserDataPsRegs(uint index,uint count, void* data) {
        // mmSPI_SHADER_USER_DATA_PS_0 0x2C0C 
        SetShRegs(0x2C0C + index, count, data);
    };

    void SetUserDataCs(uint index, uint data) {
        //  mmCOMPUTE_USER_DATA_0  0x2E40;   
        SetShReg(0x2E40 + index, data);
    };

    void SetUserDataCsRegs(uint index, uint count, void* data) {
        //  mmCOMPUTE_USER_DATA_0  0x2E40;   
        SetShRegs(0x2E40 + index, count, data);
    };

    void SetRenderTarget(uint index, RENDER_TARGET* data)
    {
        SetCtxRegs(0xA318 + index * 15, 14, (void*)data);

        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(2, IT_NOP);
        cmds[1] = *reinterpret_cast<uint*>(&data->hint);

        this->curr_ptr = cmds + 2;
    };

    void setRenderTargetMask(uint mask)
    {
        //mmCB_TARGET_MASK
        SetCtxReg(0xA08E, mask);
    };

#define WAIT_REG_MEM_FUNC_ALWAYS        0
#define WAIT_REG_MEM_FUNC_LESS          1
#define WAIT_REG_MEM_FUNC_LESS_EQUAL    2
#define WAIT_REG_MEM_FUNC_EQUAL         3
#define WAIT_REG_MEM_FUNC_NOT_EQUAL     4
#define WAIT_REG_MEM_FUNC_GREATER_EQUAL 5
#define WAIT_REG_MEM_FUNC_GREATER       6

    void waitOnAddress(void* gpuAddr, uint mask, int compareFunc, uint refValue)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(7, IT_WAIT_REG_MEM);
        cmds[1] = (uint)(1 << 4) | compareFunc & 7;
        cmds[2] = (uint)(uint64_t)gpuAddr & 0xfffffffc;
        cmds[3] = (uint)((uint64_t)gpuAddr >> 32) & 0xffff;
        cmds[4] = refValue;
        cmds[5] = mask;
        cmds[6] = 10;

        this->curr_ptr = cmds + 7;
    };

    void waitOnAddressAndStallCommandBufferParser(void* gpuAddr, uint mask, int compareFunc, uint refValue)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(7, IT_WAIT_REG_MEM);
        cmds[1] = (uint)(1 << 4) | compareFunc & 7 |  0x100;
        cmds[2] = (uint)(uint64_t)gpuAddr & 0xfffffffc;
        cmds[3] = (uint)((uint64_t)gpuAddr >> 32) & 0xffff;
        cmds[4] = refValue;
        cmds[5] = mask;
        cmds[6] = 10;

        this->curr_ptr = cmds + 7;
    }



#define CacheActionNone 0x00
#define CacheActionWBAndInvL1andL2 0x38
#define CacheActionWBAndInvL2Vol 0x3B
#define CacheActionInvL2Vol 0x33
#define CacheActionInvL1 0x10

#define ExtCacheActionFlushAndInvCbCache 0x02000000
#define ExtCacheActionFlushAndInvDbCache 0x04000000
#define ExtCacheActionInvKCache 0x08000000
#define ExtCacheActionInvICache 0x20000000

#define StallCBParserEnable  0
#define StallCBParserDisable 1

    void waitForGraphicsWrites(
        uint baseAddr256,
        uint sizeIn256,
        uint targetMask,
        int cacheAction,
        uint extendedCacheMask,
        int commandBufferStallMode)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = 0xc0055800;
        cmds[1] = commandBufferStallMode << 31 |
            (extendedCacheMask | targetMask) & 0x7fffffff |
            (cacheAction & 0x30) << 18 | (cacheAction & 0xb) << 15;

        cmds[2] = sizeIn256;
        cmds[3] = 0;
        cmds[4] = baseAddr256;
        cmds[5] = 0;
        cmds[6] = 10;

        this->curr_ptr = cmds + 7;
    };

    void flushShaderCachesAndWait(
        int cacheAction,
        uint extendedCacheMask)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = 0xc0055800;
        cmds[1] = (extendedCacheMask & 0x79ffffff) | (cacheAction & 0x30) << 18 | (cacheAction & 0xb) << 15;
        cmds[2] = 0;
        cmds[3] = 0;
        cmds[4] = 0;
        cmds[5] = 0;
        cmds[6] = 10;

        this->curr_ptr = cmds + 7;
    };


#define EopFlushCbDbCaches              0x04
#define EopFlushAndInvalidateCbDbCaches 0x14
#define EopCbDbReadsDone                0x28
#define EopCsDone                       0x28

#define EventWriteDestMemory       0x00
#define EventWriteDestTcL2         0x01
#define EventWriteDestTcL2Volatile 0x11

#define EventWriteSource32BitsImmediate     0x1
#define EventWriteSource64BitsImmediate     0x2
#define EventWriteSourceGlobalClockCounter  0x3
#define EventWriteSourceGpuCoreClockCounter 0x4

#define CachePolicyLru    0x0
#define CachePolicyStream 0x1
#define CachePolicyBypass 0x2

    void writeAtEndOfPipe(int eventType,
        int dstSelector, void* dstGpuAddr,
        int srcSelector, ulong immValue, int cacheAction,
        int cachePolicy)
    {
        ulong mask;

        uint* cmds = this->curr_ptr;

        cmds[0] = 0xc0044700;

        mask = 0xfffffffffc;
        if (srcSelector != 1) {
            mask = 0xfffffffff8;
        };

        cmds[1] = (cachePolicy & 3) * 0x2000000 + 0x500 +
                  ((dstSelector & 0x10) << 23 | eventType & 0x3f |
                  (cacheAction & 0x3f) << 12);

        cmds[2] = (int)(mask & (ulong)dstGpuAddr);

        cmds[3] = srcSelector << 29 | (dstSelector & 1) << 16 |
                  (uint)((mask & (ulong)dstGpuAddr) >> 32);

        *(ulong*)(cmds + 4) = immValue;

        this->curr_ptr = cmds + 6;
    };

    void writeAtEndOfPipeWithInterrupt(int eventType,
        int dstSelector, void* dstGpuAddr,
        int srcSelector, ulong immValue, int cacheAction,
        int cachePolicy,
        int intSel = 2)
    {
        ulong mask;

        uint* cmds = this->curr_ptr;

        cmds[0] = 0xc0044700;

        mask = 0xfffffffffc;
        if (srcSelector != 1) {
            mask = 0xfffffffff8;
        };

        cmds[1] = (cachePolicy & 3) * 0x2000000 + 0x500 +
                  ((dstSelector & 0x10) << 23 | eventType & 0x3f |
                  (cacheAction & 0x3f) << 12);

        cmds[2] = (int)(mask & (ulong)dstGpuAddr);

        cmds[3] = (int)((mask & (ulong)dstGpuAddr) >> 32) + ((intSel & 3) << 24) +
                  (srcSelector << 29 | (dstSelector & 1) << 16);

        *(ulong*)(cmds + 4) = immValue;

        this->curr_ptr = cmds + 6;
    };

    void writeAtEndOfShader(int eventType, void* dstGpuAddr, uint immValue)
    {
        uint header;
        uint addressLo;
        uint addressHi;
        uint eventMask;

        uint* cmds = this->curr_ptr;

        addressLo = (uint)((ulong)dstGpuAddr & 0xfffffffffc);
        addressHi = (uint)(((ulong)dstGpuAddr & 0xfffffffffc) >> 32) | 0x40000000;

        header = (eventType != 0x30) + 0xc0034800 + (uint)(eventType != 0x30);

        eventMask = eventType & 0x3f | 0x600;

        cmds[0] = header;
        cmds[1] = eventMask;
        cmds[2] = addressLo;
        cmds[3] = addressHi;
        cmds[4] = immValue;

        this->curr_ptr = cmds + 5;
    };

#define ReleaseMemEventCsDone                       0x2F
#define ReleaseMemEventFlushCbDbCaches              0x04
#define ReleaseMemEventFlushAndInvalidateCbDbCaches 0x14
#define ReleaseMemEventCbDbReadsDone                0x28
#define ReleaseMemEventFlushAndInvalidateDbCache    0x2B
#define ReleaseMemEventFlushAndInvalidateCbCache    0x2D

    void writeReleaseMemEvent(
        int eventType,
        int dstSelector, void* dstGpuAddr,
        int srcSelector, ulong immValue, int cacheAction,
        int writePolicy)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = 0xc0054902;

        cmds[1] = (writePolicy & 3) * 0x2000000 + 0x500 +
                  ((cacheAction & 0x3f) << 12 |
                  (dstSelector & 0x10) << 23 |
                  eventType & 0x3f | (uint)(eventType == 0x2f) << 8);

        cmds[2] = ((dstSelector & 1) << 16) + (3 << 24) + ((srcSelector & 3) << 29);

        *(void**)(cmds + 3) = dstGpuAddr;
        *(ulong*)(cmds + 5) = immValue;

        this->curr_ptr = cmds + 7;
    };

    void writeReleaseMemEventWithInterrupt(
        int eventType,
        int dstSelector, void* dstGpuAddr,
        int srcSelector, ulong immValue, int cacheAction,
        int writePolicy,
        int intSel = 2)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = 0xc0054902;

        cmds[1] = (writePolicy & 3) * 0x2000000 + 0x500 +
                  ((cacheAction & 0x3f) << 12 |
                  (dstSelector & 0x10) << 23 |
                  eventType & 0x3f | (uint)(eventType == 0x2f) << 8);

        cmds[2] = ((dstSelector & 1) << 16) + ((intSel & 3) << 24) + ((srcSelector & 3) << 29);

        *(void**)(cmds + 3) = dstGpuAddr;
        *(ulong*)(cmds + 5) = immValue;

        this->curr_ptr = cmds + 7;
    };

    void setViewport(
        uint viewportId,
        float dmin,
        float dmax,
        void* scale,  //float[3]
        void* offset  //float[3]
    )
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(4, IT_SET_CONTEXT_REG);
        cmds[1] = viewportId * 2 + 0xb4; //PA_SC_VPORT_ZMIN_MAX 0xA0B4
        cmds[2] = *reinterpret_cast<uint*>(&dmin);
        cmds[3] = *reinterpret_cast<uint*>(&dmax);

        cmds[4] = PM4_HEADER_BUILD(8, IT_SET_CONTEXT_REG);
        cmds[5] = viewportId * 6 + 0x10f; //PA_CL_VPORT_SCALE_OFFSET 0xA10F
        cmds[6] = ((uint*)scale)[0];
        cmds[7] = ((uint*)offset)[0];
        cmds[8] = ((uint*)scale)[1];
        cmds[9] = ((uint*)offset)[1];
        cmds[10] = ((uint*)scale)[2];
        cmds[11] = ((uint*)offset)[2];

        this->curr_ptr = cmds + 12;
    };

    void setViewport2D(
        uint viewportId,
        int left,
        int top,
        int right,
        int bottom,
        float dmin,
        float dmax
    )
    {
        float offset[3];
        float scale[3];

        scale[0] = (float)(right - left) * 0.5;
        scale[1] = (float)(top - bottom) * 0.5;
        scale[2] = 0.5;
        offset[0] = (float)left + (float)(right - left) * 0.5;
        offset[1] = (float)top + (float)(bottom - top) * 0.5;
        offset[2] = 0.5;

        setViewport(viewportId, dmin, dmax, &scale, &offset);
    };

    void setViewportTransformControl(PA_CL_VTE_CNTL data)
    {
        //mmPA_CL_VTE_CNTL
        SetCtxReg(0xA206, data.raw);
    };

    void setScreenScissor(
        uint left,
        uint top,
        uint right,
        uint bottom)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(4, IT_SET_CONTEXT_REG);
        cmds[1] = 0xc;
        cmds[2] = (top & 0xffff) << 16 | left & 0xffff;
        cmds[3] = (bottom & 0xffff) << 16 | right & 0xffff;

        this->curr_ptr = cmds + 4;
    };

    void setGenericScissor(
        uint left,
        uint top,
        uint right,
        uint bottom,
        int windowOffsetEnable)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(4, IT_SET_CONTEXT_REG);
        cmds[1] = 0x90;
        cmds[2] = (uint)(windowOffsetEnable == 0) << 31 | (top & 0x7fff) << 16 | left & 0x7fff;
        cmds[3] = (bottom & 0x7fff) << 16 | right & 0x7fff;

        this->curr_ptr = cmds + 4;
    };

    void setWindowScissor(
        uint left,
        uint top,
        uint right,
        uint bottom,
        int windowOffsetEnable)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = 0xc0026900;
        cmds[1] = 0x81;
        cmds[2] = (uint)(windowOffsetEnable == 0) << 31 | (top & 0x7fff) << 16 | left & 0x7fff;
        cmds[3] = (bottom & 0x7fff) << 16 | right & 0x7fff;

        this->curr_ptr = cmds + 4;
    };

    void setViewportScissor(
        uint viewportId,
        uint left,
        uint top,
        uint right,
        uint bottom,
        int windowOffsetEnable)

    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(4, IT_SET_CONTEXT_REG);
        cmds[1] = viewportId * 2 + 0x94;
        cmds[2] = (uint)(windowOffsetEnable == 0) << 31 | (top & 0x7fff) << 16 | left & 0x7fff;
        cmds[3] = (bottom & 0x7fff) << 16 | right & 0x7fff;

        this->curr_ptr = cmds + 4;
    };

    void setWindowOffset(short offsetX, short offsetY)
    {
        //mmPA_SC_WINDOW_OFFSET 0xA080
        SetCtxReg(0xA080, (offsetY & 0xffff) << 16 | offsetX & 0xffff);
    }

    void setScanModeControl(int msaa, int viewportScissor)
    {
        //mmPA_SC_MODE_CNTL_0 0xA292 
        SetCtxReg(0xA292, (msaa & 1) + (viewportScissor & 1) * 2);
    };

    void setHardwareScreenOffset(uint offsetX, uint offsetY)
    {
        //mmPA_SU_HARDWARE_SCREEN_OFFSET 0xA08D; 
        SetCtxReg(0xA08d, (offsetY & 0x1fc) << 16 | offsetX & 0x1fc);
    };

    void setGuardBands(
        float horzClip,
        float vertClip,
        float horzDiscard,
        float vertDiscard)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(6, IT_SET_CONTEXT_REG);
        cmds[1] = 0x2fa;
        cmds[2] = *reinterpret_cast<uint*>(&vertClip);
        cmds[3] = *reinterpret_cast<uint*>(&vertDiscard);
        cmds[4] = *reinterpret_cast<uint*>(&horzClip);
        cmds[5] = *reinterpret_cast<uint*>(&horzDiscard);

        this->curr_ptr = cmds + 6;
    };

#define PRIM_TRI_LIST  4
#define PRIM_TRI_FAN   5
#define PRIM_TRI_STRIP 6
#define PRIM_RECT_LIST 17

    void setPrimitiveType(int primType)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(3, IT_SET_UCONFIG_REG);
        cmds[1] = (uint)(this->neo_mode == 1) << 28 | 0x242;
        cmds[2] = primType & 0x3f;

        this->curr_ptr = cmds + 3;
    };

    void setIndexBuffer(void* indexAddr)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(3, IT_INDEX_BASE);

        ((uintptr_t*)&cmds[1])[0] = (uintptr_t)indexAddr & 0xfffffffffe;

        this->curr_ptr = cmds + 3;
    };

    void setIndexCount(uint indexCount)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(2, IT_INDEX_BUFFER_SIZE);
        cmds[1] = indexCount;

        this->curr_ptr = cmds + 2;
    };

    void setIndexOffset(uint offset)
    {
        //mmVGT_INDX_OFFSET 0xA102;
        SetCfgReg(0xA102, offset);
    };

 #define IndexSize16 0
 #define IndexSize32 1

    void setIndexSize(int indexSize, int cachePolicy)
    {
        uint* cmds = this->curr_ptr;

        uint i = (cachePolicy == 2) ? 0 : (cachePolicy & 3) << 6;

        cmds[0] = PM4_HEADER_BUILD(2, IT_INDEX_TYPE);
        cmds[1] = (uint)(cachePolicy != 2) << 10 | (indexSize & 0xfffffb3f) | i;

        this->curr_ptr = cmds + 2;
    };

    void DrawIndexAuto(int indexCount)
    {
        uint* cmds = this->curr_ptr;

        Gnm::sceGnmDrawIndexAuto(cmds, 7, indexCount, 0);

        this->curr_ptr = cmds + 7;
    };

    void DrawIndex(uint indexCount,void* indexAddr)
    {
        uint* cmds = this->curr_ptr;

        Gnm::sceGnmDrawIndex(cmds, 10, indexCount, indexAddr, 0);

        this->curr_ptr = cmds + 10;
    };

    void DrawIndexOffset(uint indexOffset,uint indexCount)
    {
        uint* cmds = this->curr_ptr;

        Gnm::sceGnmDrawIndexOffset(cmds, 9, indexOffset, indexCount, 0);

        this->curr_ptr = cmds + 9;
    };


    void DispatchDirect(uint X, uint Y, uint Z)
    {
        uint* cmds = this->curr_ptr;

        Gnm::sceGnmDispatchDirect(cmds, 9, X, Y, Z, 0);

        this->curr_ptr = cmds + 9;
    };

    void waitUntilSafeForRendering(uint hVideo, uint buffer_index) {
        uint* cmds = this->curr_ptr;

        Gnm::sceGnmInsertWaitFlipDone(cmds, 7, hVideo, buffer_index);

        this->curr_ptr = cmds + 7;
    };

    void prepareFlip()
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(64, IT_NOP);
        cmds[1] = 0x68750777;

        this->curr_ptr = cmds + 64;
    };

    void prepareFlip(void* labelAddr, uint value)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(64, IT_NOP);
        cmds[1] = 0x68750778;
        ((void**)&cmds[2])[0] = labelAddr;
        cmds[4] = value;

        this->curr_ptr = cmds + 64;
    };

    void prepareFlipWithEopInterrupt(int eventType, int cacheAction)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(64, IT_NOP);
        cmds[1] = 0x68750780;
        cmds[5] = eventType;
        cmds[6] = cacheAction;

        this->curr_ptr = cmds + 64;
    };

    void prepareFlipWithEopInterrupt(int eventType, void* labelAddr, uint value, int cacheAction)
    {
        uint* cmds = this->curr_ptr;

        cmds[0] = PM4_HEADER_BUILD(64, IT_NOP);
        cmds[1] = 0x68750781;
        ((void**)&cmds[2])[0] = labelAddr;
        cmds[4] = value;
        cmds[5] = eventType;
        cmds[6] = cacheAction;

        this->curr_ptr = cmds + 64;
    };

};
