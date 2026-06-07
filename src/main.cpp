#include <sstream>

#include <functional>
#include <vector>

#include <stdint.h>
#include <sys/types.h>
#include <orbis/_types/kernel.h>
#include <orbis/_types/pthread.h>
#include <orbis/_types/errors.h>
#include <orbis/_types/user.h>
#include <orbis/_types/video.h>

#include <orbis/libkernel.h>
//#include <orbis/VideoOut.h>

typedef struct {
    uint64_t count;
    uint64_t processTime;
    uint64_t tscTime;
    int64_t  flipArg;
    uint64_t submitTsc;
    uint64_t reserved0;
    int32_t  numGpuFlipPending;
    int32_t  numFlipPending;
    int32_t  currentBuffer;
    uint32_t reserved1;
} OrbisVideoOutFlipStatus2;

extern "C" {

    // int sceKernelAllocateMainDirectMemory(size_t len, size_t align, int32_t mtype, off_t* p_out); //fixed header
    //int sceKernelMapDirectMemory(void** addr, size_t len, int prot, int flags, off_t dstart, size_t align);
    //int sceKernelReleaseDirectMemory(off_t dstart, size_t len);
    //int sceKernelIsNeoMode(void);
    //int sceKernelUsleep(uint microsec);

    //int sceKernelVirtualQuery(const void*, int32_t, OrbisKernelVirtualQueryInfo*, size_t);

    //int sceKernelCreateEqueue(OrbisKernelEqueue* eq, const char* name);
    //int sceKernelDeleteEqueue(OrbisKernelEqueue eq);
    //int sceKernelWaitEqueue(OrbisKernelEqueue eq, OrbisKernelEvent* ev, int num, int* p_out, OrbisKernelUseconds* p_timeo);
    ////////


    int sceVideoOutOpen(OrbisUserServiceUserId userId, int busType, int index, const void* param);
    int sceVideoOutClose(int hVideo);

    void sceVideoOutSetBufferAttribute(OrbisVideoOutBufferAttribute* attr, uint pixelFormat, uint tilingMode, uint aspectRatio, uint width, uint height, uint pitchInPixel);
    int sceVideoOutRegisterBuffers(int hVideo, int startIndex, void* const* addrs, int buf_nums, const OrbisVideoOutBufferAttribute* attrs);
    int sceVideoOutUnregisterBuffers(int hVideo, int attr_index);
    int sceVideoOutSubmitChangeBufferAttribute(int hVideo, int buf_index, OrbisVideoOutBufferAttribute* attr);
    int sceVideoOutSubmitFlip(int hVideo, int buf_index, int flipMode, int64_t flipArg);
    int sceVideoOutSetFlipRate(int hVideo, int fliprate);
    int sceVideoOutAddFlipEvent(OrbisKernelEqueue eq, int hVideo, void* udata);
    int sceVideoOutDeleteFlipEvent(OrbisKernelEqueue eq, int hVideo);
    int sceVideoOutGetFlipStatus(int hVideo, OrbisVideoOutFlipStatus2* status);
    int sceVideoOutIsFlipPending(int hVideo);
    int sceVideoOutGetResolutionStatus(int hVideo, OrbisVideoOutResolutionStatus* status);

    int sceVideoOutGetBufferLabelAddress(int hVideo, uint64_t** ptr);



    ////////

}

#define FRAME_WIDTH     1920
#define FRAME_HEIGHT    1080

#define PIXEL_FORMAT_ARGB8 0x80000000
#define PIXEL_FORMAT_ABGR8 0x80002200

#define FLIP_VSYNC 1
#define FLIP_HSYNC 2
#define FLIP_MODE_WINDOW 3
#define FLIP_MODE_VSYNC_MULTI 4
#define FLIP_MODE_VSYNC_MULTI_2 5
#define FLIP_MODE_WINDOW_2 6


#define TILING_MODE_TILE 0
#define TILING_MODE_LINEAR 1

#define WB_ONION 0
#define WC_GARLIC 3
#define WB_GARLIC 10

#define VM_PROT_GPU_READ 0x10
#define VM_PROT_GPU_WRITE 0x20
#define VM_PROT_GPU_ALL 0x30

#define MAP_FIXED 0x0010

// Some OpenOrbis stub-generated libkernel.h headers type the 4th parameter of
// sceKernelAllocateMainDirectMemory as off_t by value. The real libkernel ABI
// takes off_t* and writes the allocated physical offset through it, which is
// what every call site below relies on. Call through the correct prototype so
// the offset is returned to the caller regardless of how the header declares it.
#define sceKernelAllocateMainDirectMemory(len, align, mtype, pout)                  \
    (((int32_t (*)(size_t, size_t, int32_t, off_t*))sceKernelAllocateMainDirectMemory)( \
        (len), (align), (mtype), (pout)))

#include "frame.h"
#include "vsharp.h"
#include "tsharp.h"
#include "render_target.h"
#include "cmdbuf.h"
#include "compute_queue.h"
#include "shader_builder.h"

struct t_dmem_pages {
    off_t dmem_ofs;
    off_t dmem_size;
    void* dmem_ptr;

    int Alloc(uintptr_t size, int memoryType) {

        size = (size + 16 * 1024 - 1) & ~(16 * 1024 - 1);

        this->dmem_size = size;

        int ret;

        ret = sceKernelAllocateMainDirectMemory(
            size,
            16 * 1024,
            memoryType,
            &this->dmem_ofs);
        
        if (ret != 0) return ret;

        ret = sceKernelMapDirectMemory(
            &this->dmem_ptr,
            size,
            VM_PROT_READ | VM_PROT_WRITE | VM_PROT_GPU_ALL,
            0,
            this->dmem_ofs,
            16 * 1024);

        if (ret != 0) {

            sceKernelReleaseDirectMemory(this->dmem_ofs, size);
            this->dmem_ofs = 0;
            this->dmem_size = 0;

            return ret;
        };

        return 0;
    };

    int Free() {
        if (dmem_ptr != NULL) sceKernelReleaseDirectMemory(this->dmem_ofs, this->dmem_size);
    };

};


struct t_linear_alloc {
    off_t dmem_ofs;
    void* dmem_ptr;
    void* dmem_cur;

    void init(uintptr_t size) {

        int ret;

        ret = sceKernelAllocateMainDirectMemory(
            size,
            16 * 1024,
            WB_ONION,
            &this->dmem_ofs);
        printf("sceKernelAllocateMainDirectMemory->%d %d\n", ret, dmem_ofs);

        ret = sceKernelMapDirectMemory(
            &this->dmem_ptr,
            size,
            VM_PROT_READ | VM_PROT_WRITE | VM_PROT_GPU_ALL,
            0,
            this->dmem_ofs,
            16 * 1024);

        printf("sceKernelMapDirectMemory->%d\n", ret);

        this->dmem_cur = this->dmem_ptr;

    };

    void* alloc(uintptr_t size, uintptr_t align) {
        this->dmem_cur = (void*)(((uintptr_t)this->dmem_cur + (align - 1)) & (~(align - 1)));
        void* ret = this->dmem_cur;
        this->dmem_cur = (void*)((uintptr_t)this->dmem_cur + size);
        return ret;
    };

};


void PatchShaderPtr(void* regs, void* shader) {
    ((uint*)regs)[0] = (uintptr_t)shader >> 8;
    ((uint*)regs)[1] = (uintptr_t)shader >> 8 >> 32;
}

const Gnm::VsStageRegisters const_simple_vs_shader_regs = {
    0x00000000, //spiShaderPgmLoVs;
    0x00000000, //spiShaderPgmHiVs;
    {.raw = 0x002C0002}, //spiShaderPgmRsrc1Vs;
    {.raw = 0x00000008}, //spiShaderPgmRsrc2Vs; //SCRATCH_EN:bit1; USER_SGPR:bit5;
    {.raw = 0x00000000}, //spiVsOutConfig;
    {.raw = 0x00000004}, //spiShaderPosFormat;
    {.raw = 0x00000000}  //paClVsOutCntl;
};

const Gnm::PsStageRegisters const_simple_ps_shader_regs = {
    0x00000000, //spiShaderPgmLoPs;
    0x00000000, //spiShaderPgmHiPs;
    {.raw = 0x002C0000}, //spiShaderPgmRsrc1Ps;
    {.raw = 0x00000000}, //spiShaderPgmRsrc2Ps; //SCRATCH_EN:bit1; USER_SGPR:bit5; //NEW:0x00000008
    {.raw = 0x00000000}, //spiShaderZFormat;
    {.raw = 0x00000004}, //spiShaderColFormat;
    {.raw = 0x00000002}, //spiPsInputEna;
    {.raw = 0x00000002}, //spiPsInputAddr;
    {.raw = 0x00000001}, //spiPsInControl;
    {.raw = 0x00000000}, //spiBarycCntl;
    {.raw = 0x00000010}, //dbShaderControl;
    {.raw = 0x0000000F}, //cbShaderMask;
};

//mmSPI_SHADER_USER_DATA_VS_0 : = 0x00A00214 [fetch program]
//mmSPI_SHADER_USER_DATA_VS_1 : = 0x00000002
//mmSPI_SHADER_USER_DATA_VS_2 : = 0x00A001E4 [buffer with two V#]
//mmSPI_SHADER_USER_DATA_VS_3 : = 0x00000002
const uint simple_fetch[] = {
    0xC0820300, //S_LOAD_DWORDX4 s[4:7], s[2:3], 0x00
    0xBF8C007F, //S_WAITCNT lgkmcnt(0) 
    0xE00C2000, //BUFFER_LOAD_FORMAT_XYZW v[4:7], v0, s[4:7], 0, [0] IDXEN
    0x80010400,
    0xC0820304, //S_LOAD_DWORDX4 s[4:7], s[2:3], 0x04
    0xBF8C007F, //S_WAITCNT lgkmcnt(0) 
    0xE00C2000, //BUFFER_LOAD_FORMAT_XYZW v[8:11], v0, s[4:7], 0, [0] IDXEN
    0x80010800,
    0xBF8C0000, //S_WAITCNT lgkmcnt(0) expcnt(0) vmcnt(0)
    0xBE802000  //S_SETPC_B64 s[0:1]
};

const uint simple_vs_shader[] = {
    0xBEEB03FF, //S_MOV_B32 VCC_HI, #0x00000008
    0x00000008, 
    0xBE802100, //S_SWAPPC_B64 s[0:1], s[0:1] 
    0xF80008CF, //EXP pos0, v4, v5, v6, v7 done
    0x07060504,
    0xF800020F, //EXP param0, v8, v9, v10, v11
    0x0B0A0908,
    0xBF810000, //S_ENDPGM
    0x000AAA39,
    0x00080000,
    0x5B000000,
    0x00010301,
    0x0000D0D0,
    0x00000014,
    0x00000012,
    0x00020017,
    0x00000302,
    0x046D611C,
    0x5362724F, //"OrbS"
    0x07726468, //"hdr", version[8]
    0x00002045, //[8] length[24]
    0x050C0202, //chunkUsageBaseOffsetInDW[8] numInputUsageSlots[8]
    0x00EFEF6F, //shaderHash0
    0x00000000, //shaderHash1
    0x5F1F362F  //crc32
};

const uint simple_ps_shader[] = {
    0xBEEB03FF, //S_MOV_B32 VCC_HI, #0x00000008
    0x00000008, 

    //NEW
    //0x7E000280, //V_MOV_B32 v0, 0
    //0xE00C2000, //BUFFER_LOAD_FORMAT_XYZW v[0:3], v0, s[0:3], 0,[0] IDXEN
    //0x80000000,
    //0xBF8C0000, //S_WAITCNT lgkmcnt(0) expcnt(0) vmcnt(0)
    //0x5E000300, //V_CVT_PKRTZ_F16_F32 v0, v0, v1
    //0x5E040702, //V_CVT_PKRTZ_F16_F32 v2, v2, v3
    //0xF8001C0F, //EXP mrt0, v0, v2 compr vm done
    //0x00000200,
    //NEW

    // 
    0xBEFC0300, //S_MOV_B32 M0, s0
    0xC80C0000, //V_INTERP_P1_F32 v3, v0, attr0.x
    0xC80D0001, //V_INTERP_P2_F32 v3, v1, attr0.x
    0xC8080100, //V_INTERP_P1_F32 v2, v0, attr0.y
    0xC8090101, //V_INTERP_P2_F32 v2, v1, attr0.y
    0x5E040503, //V_CVT_PKRTZ_F16_F32 v2, v3, v2
    0xC80C0200, //V_INTERP_P1_F32 v3, v0, attr0.z
    0xC8000300, //V_INTERP_P1_F32 v0, v0, attr0.w
    0xC80D0201, //V_INTERP_P2_F32 v3, v1, attr0.z
    0xC8010301, //V_INTERP_P2_F32 v0, v1, attr0.w
    0x5E000103, //V_CVT_PKRTZ_F16_F32 v0, v3, v0
    0xF8001C0F, //EXP mrt0, v2, v0 compr vm done
    0x00000002,
    //

    0xBF810000, //S_ENDPGM
    0x00000302,
    0x046D611C,
    0x5362724F, //"OrbS"
    0x07726468, //"hdr", version[8]
    0x00004041, //[8] length[24]
    0x05080002, //chunkUsageBaseOffsetInDW[8] numInputUsageSlots[8]
    0x48BDAEE3, //shaderHash0
    0x00000000, //shaderHash1
    0x46B3C0B6  //crc32
};

struct t_dce_data {
    uint64_t time : 12;
    uint64_t counter : 4;
    uint64_t flip_arg : 48;
};

static_assert(sizeof(t_dce_data) == 8);

int hVideo=0;

const char* PASS[2] = { "[Test FAILED!]", "[Test passed!]" };

uint F32ToF10(float f) {
    uint i = *reinterpret_cast<uint*>(&f);

    uint t1 = i & 0x7fffffff; // Non-sign bits
    uint t3 = i & 0xff800000; // Exponent + sign

    t1 = t1 >> 18; // Align mantissa on MSB

    t1 = t1 - 0xE00; // Adjust bias

    if (t3 < 0x38800000) t1 = 0;     // Flush-to-zero
    if (t3 > 0x47000000) t1 = 0x3FF; // Clamp-to-max 

    return t1;
}

uint F32ToF11(float f) {
    uint i = *reinterpret_cast<uint*>(&f);

    uint t1 = i & 0x7fffffff; // Non-sign bits
    uint t3 = i & 0xff800000; // Exponent + sign

    //    [S|E|M]
    //F32 [1|8|23]
    //F16 [1|5|10] -> 13 -> 0x1C000 -> 0x7BFF -> 0x47000000
    //F11 [0|6|5]  -> 18 -> 0x1C00  -> 0x7FF  -> 0x87000000
    //F10 [0|5|5]  -> 18 -> 0xE00   -> 0x3FF  -> 0x47000000

    t1 = t1 >> 18; // Align mantissa on MSB
    
    t1 = t1 - 0x1C00; // Adjust bias

    if (t3 < 0x38800000) t1 = 0;     // Flush-to-zero
    if (t3 > 0x87000000) t1 = 0x7FF; // Clamp-to-max 

    return t1;
}

static ShaderBuilder* make_cs_shader(uint load_op, uint store_op, std::function<void(ShaderBuilder*)> cb_instruction) {

    auto shader = new ShaderBuilder();

    shader->user_count = (4 * 2); //two V# (4 byte align)
    shader->sgpr_count = (4 * 2);
    shader->vgpr_count = 4 + 4;

    if (load_op != 0) {
        shader->MUBUF_OP(load_op,
            MAKE_VGPR(0),    //dst
            MAKE_VGPR(0),    //addr
            MAKE_SGPR(0),    //V#
            0,               //offset
            MAKE_IMM_INT(0), //soffset
            BUF_NONE
        );

        shader->S_WAITCNT(0, 0, 0);
    };

    cb_instruction(shader);
    
    if (store_op != 0) {

        shader->V_MOV_B32(MAKE_VGPR(0), MAKE_IMM_INT(0));
        shader->V_MOV_B32(MAKE_VGPR(1), MAKE_IMM_INT(0));

        shader->MUBUF_OP(store_op,
            MAKE_VGPR(4),    //src
            MAKE_VGPR(0),    //addr
            MAKE_SGPR(4),    //V#
            0,               //offset
            MAKE_IMM_INT(0), //soffset
            BUF_OFFEN | BUF_IDXEN
        );
    };
    
    shader->S_ENDPGM();

    return shader;
};

static ShaderBuilder* make_cs_shader(std::function<void(ShaderBuilder*)> cb_instruction) {

    return make_cs_shader(MUBUF_BUFFER_LOAD_FORMAT_XYZW, MUBUF_BUFFER_STORE_FORMAT_XYZW, cb_instruction);

};

struct t_swizzle {
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint8_t w;
};

struct t_cs_shader_test {
  
    CmdBuf* cmd_buf;

    void* shader_ptr;

    Gnm::CsStageRegisters regs;

    uint* src;
    uint* dst;

    //uint* dst2[4];

    t_swizzle src_sel;
    t_swizzle dst_sel;

};

typedef std::function<void(t_cs_shader_test*)> t_after_cb;

struct t_after_action {
    t_after_cb cb;
    t_cs_shader_test data;
};

std::vector<t_after_action> test_after_action{};

static void do_test_after_action() {

    for (auto& element : test_after_action) {

        element.cb(&element.data); //call

    };

};

static t_cs_shader_test make_cs_buf_test(t_linear_alloc* linear_dmem,
                                         CmdBuf* cmd_buf,
                                         std::function<void(ShaderBuilder*)> cb_instruction,
                                         t_after_cb on_before,t_after_cb on_after) {

    t_cs_shader_test t = {};

    t.cmd_buf = cmd_buf;

    auto shader = make_cs_shader(cb_instruction);

    t.shader_ptr = linear_dmem->alloc(shader->GetByteSize(), 256);

    t.regs = shader->ExportCs(t.shader_ptr);

    //bulder not need anymore
    delete shader;

    //in, out data
    t.src = (uint*)linear_dmem->alloc(16 * 4, 4);
    t.dst = (uint*)linear_dmem->alloc(16 * 4, 4);

    t.src_sel.x = DSEL_R;
    t.src_sel.y = DSEL_G;
    t.src_sel.z = DSEL_B;
    t.src_sel.w = DSEL_A;

    t.dst_sel = t.src_sel;

    //
    on_before(&t);

    //They are passed directly to the registers, so it's just static
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

    test_after_action.push_back({ on_after, t });

    return t;
};

static t_cs_shader_test make_cs_custom_test(t_linear_alloc* linear_dmem,
                                            CmdBuf* cmd_buf,
                                            std::function<void(ShaderBuilder*)> cb_instruction,
                                            t_after_cb on_before, t_after_cb on_after) {

    t_cs_shader_test t = {};

    t.cmd_buf = cmd_buf;

    auto shader = new ShaderBuilder();

    cb_instruction(shader);

    shader->S_ENDPGM();

    t.shader_ptr = linear_dmem->alloc(shader->GetByteSize(), 256);

    t.regs = shader->ExportCs(t.shader_ptr);

    //bulder not need anymore
    delete shader;

    //
    on_before(&t);

    test_after_action.push_back({ on_after, t });

    return t;
};


//MUBUF_BUFFER_STORE_FORMAT_XYZW

static t_cs_shader_test make_cs_mask_readback_test(t_linear_alloc* linear_dmem,
                                                   CmdBuf* cmd_buf) {

    // Isolates the wave-mask-as-value readback (the op underneath
    // v_cmp_lg_u64 on the RE3/RE4 mask-vs-VCC idiom, and V_MOV_B32 vN, sM).
    // One wave of 64 active lanes (v0 = lane id). Predicate (lane < 48) is
    // chosen so the high dword reveals the wave width too:
    //   real PS4 wave64 : lo=FFFFFFFF hi=0000FFFF
    //   a wave32 host   : lo=FFFFFFFF hi=00000000
    // shadPS4 today returns garbage for all four (reads the packed SSA var the
    // v_cmp never wrote). The SGPR path (dst[0..1]) and the VCC path
    // (dst[2..3]) resolve through different code, so both are exercised.

    t_cs_shader_test t = {};
    t.cmd_buf = cmd_buf;

    auto cb_instruction = [](ShaderBuilder* shader) {
        shader->VOP3c_OP(VOP3_CMP_LT_U32, MAKE_SGPR(8),
                         MAKE_VGPR(0), MAKE_IMM_INT(48));     // s[8:9]
        shader->VOP3c_OP(VOP3_CMP_LT_U32, MAKE_SGPR(VCC_LO),
                         MAKE_VGPR(0), MAKE_IMM_INT(48));     // vcc

        shader->V_MOV_B32(MAKE_VGPR(4), MAKE_SGPR(8));        // sgpr mask lo
        shader->V_MOV_B32(MAKE_VGPR(5), MAKE_SGPR(9));        // sgpr mask hi
        shader->V_MOV_B32(MAKE_VGPR(6), MAKE_SGPR(VCC_LO));   // vcc lo
        shader->V_MOV_B32(MAKE_VGPR(7), MAKE_SGPR(VCC_HI));   // vcc hi
    };

    // load_op = 0: skip the prologue load (would clobber v0 = lane id);
    // keep the store epilogue (writes v4..v7 to the dst V#).
    auto shader = make_cs_shader(0, MUBUF_BUFFER_STORE_FORMAT_XYZW, cb_instruction);

    shader->NumThreadX = 64;   // one full wave; v_thread_cnt defaults to 1 -> v0 = lane id
    shader->NumThreadY = 1;
    shader->NumThreadZ = 1;
    shader->sgpr_count = 10;    // s0-3 src V#, s4-7 dst V#, s8-9 mask pair

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

static int filter_sel(int i, int store_count) {
    switch (i) {
    case 4:
    case 5:
    case 6:
    case 7:if ((i - 4) >= store_count) i = 0;
        break;
    };
    return i;
};

static t_swizzle filter_dst_sel(t_swizzle dst, int store_count) {

    t_swizzle result = {};

    result.x = filter_sel(dst.x, store_count);
    result.y = filter_sel(dst.y, store_count);
    result.z = filter_sel(dst.z, store_count);
    result.w = filter_sel(dst.w, store_count);

    return result;
};

//

static int fix_cst_sel(int i) {
    switch (i) {
    case 2:return 1;
    case 3:return 0;
    default:
        return i;
    };
};

static int convert_sel(int i) {
    switch (i) {
    case 2:return 1;
    case 3:return 0;
    case 4:
    case 5:
    case 6:
    case 7:return (11 - i);
    default:
        return i;
    };
};

static int filter_no_cst(int i) {
    switch (i) {
    case 4:
    case 5:
    case 6:
    case 7:return i;
    default:
        return 0;
    };
};

static t_swizzle _get_reverse_dst_sel_1(t_swizzle dst, int store_count) {

    t_swizzle result = {};

    switch (store_count) {
    case 1: {

        switch (dst.x) {
        case 2:result.x = 1;
            break;
        case 3:result.x = 3; //special value:0x3f800001
            break;
        case 4:
        case 5:
        case 6:
        case 7:result.x = 4;
            break;
        default:
            result.x = dst.x;
            break;
        };

        break;
    };
    case 2:
    case 3: 
    case 4:
    {

        switch (dst.x) {
        case 0:
        case 1:
        case 2:
        case 3:result.x = 7;
            break;
        case 4:result.x = 4;
            break;
        case 5:
        case 6:
        case 7:result.x = 7;
            break;
        default:
            result.x = dst.x;
            break;
        };

        result.x = filter_sel(result.x, store_count);

        break;
    };
    };

    return result;
};

static t_swizzle _get_reverse_dst_sel_2(t_swizzle dst, int store_count) {

    dst.x = filter_no_cst(dst.x);
    dst.y = filter_no_cst(dst.y);
    dst.z = filter_no_cst(dst.z);
    dst.w = filter_no_cst(dst.w);

    t_swizzle result = {};

    result.x = 5;
    result.y = 4;

    if ((dst.x != 4) && (dst.z != 0)) {
        result.x  = 7;
        result.y  = 4;
    }
    else
        if ((dst.y == 0) || ((dst.x != 0) && (dst.z != 0))) {
            result.x = 4;
            result.y = 7;
        }
        else
            if ((dst.x == 4) && (dst.z == 0)) {
                result.x = 4;
                result.y = 5;
            };

    result.x = filter_sel(result.x, store_count);
    result.y = filter_sel(result.y, store_count);

    return result;
};

static t_swizzle get_reverse_dst_sel(t_swizzle dst, int elem_count, int store_count) {

    t_swizzle result = {};

    switch (elem_count) {
    case 1:
        result = _get_reverse_dst_sel_1(dst, store_count);

        break;
    case 2:
        result = _get_reverse_dst_sel_2(dst, store_count);

        break;
    case 3:
        result.x = fix_cst_sel(dst.x);
        result.y = fix_cst_sel(dst.y);
        result.z = fix_cst_sel(dst.z);

        result = filter_dst_sel(result, store_count);

        break;
    case 4:
        if (dst.y == 5) {
            result.x = fix_cst_sel(dst.x);
            result.y = fix_cst_sel(dst.y);
            result.z = fix_cst_sel(dst.z);
            result.w = fix_cst_sel(dst.w);
        }
        else
        {
            result.x = convert_sel(dst.w);
            result.y = convert_sel(dst.z);
            result.z = convert_sel(dst.y);
            result.w = convert_sel(dst.x);
        };

        result = filter_dst_sel(result, store_count);

        break;
    };

    return result;
};

typedef uint t_vector4u[4];

const t_vector4u init_vector_8 = {
  8,8,8,8
};

struct t_per_dst_sel {
    t_vector4u per_store_count[4]; //X, XY, XYZ, XYZW
};

#define full_sel_bits (4*3)
#define max_sel_count (1 << full_sel_bits)

struct t_per_elem_cnt {
    t_per_dst_sel per_dst_sel[max_sel_count];
};

struct t_all_elem {
    t_per_elem_cnt per_elem_cnt[4];
};

//const BufFlags used_flags = BUF_OFFEN | BUF_IDXEN;
//const BufFlags used_flags = BUF_IDXEN;
const BufFlags used_flags = BUF_OFFEN;

static t_cs_shader_test make_cs_lg_u64_test(t_linear_alloc* linear_dmem,
                                            CmdBuf* cmd_buf) {

    // End-to-end test of the actual RE3/RE4 instruction: v_cmp_lg_u64 comparing
    // two wave masks -- mask A in an SGPR pair vs mask B moved from VCC into
    // VGPRs, exactly as the games do. One wave of 64 active lanes (v0 = lane id).
    //   mask A = (lane < 48) -> 0x0000FFFF_FFFFFFFF
    //   mask B = (lane < 40) -> 0x000000FF_FFFFFFFF
    // A != B, so v_cmp_lg_u64 sets VCC true on every active lane. v_cndmask reads
    // that result per-lane (NOT a mask readback) into a plain 1/0 value.
    //
    // NOTE: this dispatch executes v_cmp_lg_u64 with a VGPR src1, so UNFIXED
    // shadPS4 asserts/crashes (vector_alu.cpp). Run it only with the
    // V_CMPX_LG_U64 / V_CMP_U64 fix applied.
    //
    // Expected dst on real PS4:
    //   dst[0] = 0x00000001  compare result (A != B -> true)
    //   dst[1] = 0x0000FFFF  mask A hi, read back as a value (SGPR readback)
    //   dst[2] = 0x000000FF  mask B hi, read back as a value (VCC readback)
    //   dst[3] = 0x00000000  unused
    // shadPS4 with the crash fix but no ballot materialization: dst[1]/dst[2]
    // come back garbage (Bug B), and dst[0] is unreliable because v_cmp_lg_u64
    // is fed those garbage inputs.

    t_cs_shader_test t = {};
    t.cmd_buf = cmd_buf;

    auto cb_instruction = [](ShaderBuilder* shader) {
        // mask A -> s[8:9]
        shader->VOP3c_OP(VOP3_CMP_LT_U32, MAKE_SGPR(8),
                         MAKE_VGPR(0), MAKE_IMM_INT(48));
        // mask B -> vcc, then moved into v[2:3] (the VCC->VGPR readback)
        shader->VOP3c_OP(VOP3_CMP_LT_U32, MAKE_SGPR(VCC_LO),
                         MAKE_VGPR(0), MAKE_IMM_INT(40));
        shader->V_MOV_B32(MAKE_VGPR(2), MAKE_SGPR(VCC_LO));
        shader->V_MOV_B32(MAKE_VGPR(3), MAKE_SGPR(VCC_HI));

        // the actual instruction: vcc = (s[8:9] != v[2:3]) per lane
        shader->VOP3c_OP(VOP3_CMP_LG_U64, MAKE_SGPR(VCC_LO),
                         MAKE_SGPR(8), MAKE_VGPR(2));

        // observe the per-lane result without a mask readback: v4 = vcc ? 1 : 0
        shader->V_MOV_B32(MAKE_VGPR(9), MAKE_IMM_INT(1));
        shader->VOP2_OP(VOP2_CNDMASK_B32, MAKE_VGPR(4),
                        MAKE_IMM_INT(0), MAKE_VGPR(9));

        // diagnostics: the two input masks' high words, read back as values
        shader->V_MOV_B32(MAKE_VGPR(5), MAKE_SGPR(9));     // mask A hi (SGPR readback)
        shader->V_MOV_B32(MAKE_VGPR(6), MAKE_VGPR(3));     // mask B hi (already in v3)
        shader->V_MOV_B32(MAKE_VGPR(7), MAKE_IMM_INT(0));  // unused
    };

    auto shader = make_cs_shader(0, MUBUF_BUFFER_STORE_FORMAT_XYZW, cb_instruction);

    shader->NumThreadX = 64;
    shader->NumThreadY = 1;
    shader->NumThreadZ = 1;
    shader->vgpr_count = 10;   // v0..v9
    shader->sgpr_count = 10;   // s0-3 src V#, s4-7 dst V#, s8-9 mask A

    t.shader_ptr = linear_dmem->alloc(shader->GetByteSize(), 256);
    t.regs = shader->ExportCs(t.shader_ptr);
    delete shader;

    t.src = (uint*)linear_dmem->alloc(16 * 4, 4);
    t.dst = (uint*)linear_dmem->alloc(16 * 4, 4);
    for (int i = 0; i < 16; i++) { t.src[i] = 0; t.dst[i] = 0xCDCDCDCD; }

    t.src_sel.x = DSEL_R; t.src_sel.y = DSEL_G; t.src_sel.z = DSEL_B; t.src_sel.w = DSEL_A;
    t.dst_sel = t.src_sel;

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
        const uint exp[4] = { 0x00000001, 0x0000FFFF, 0x000000FF, 0x00000000 };
        const char* lbl[4] = { "cmp(A!=B)", "maskA_hi", "maskB_hi", "unused" };
        printf("[v_cmp_lg_u64] A=(lane<48) B=(lane<40), wave=64\n");
        for (int i = 0; i < 4; i++) {
            printf("  %-10s = %08x  expected %08x -> %s\n",
                   lbl[i], t->dst[i], exp[i], PASS[t->dst[i] == exp[i]]);
        }
    };

    test_after_action.push_back({ on_after, t });
    return t;
}

static t_cs_shader_test make_cs_test_dst_sel(t_linear_alloc* linear_dmem,CmdBuf* cmd_buf) {

    t_cs_shader_test t = {};

    auto shader = make_cs_shader(0,
                                 0,
        [](ShaderBuilder* shader) {

            shader->user_count = 16;
            shader->sgpr_count = 16;

            //shader->sgpr_count = 17;
            //shader->s_tgid_x = 1;
            //shader->v_thread_cnt = 1;
            //shader->NumThreadX = 64;

        shader->V_MOV_B32(MAKE_VGPR(4), MAKE_IMM_INT(DSEL_R));
        shader->V_MOV_B32(MAKE_VGPR(5), MAKE_IMM_INT(DSEL_G));
        shader->V_MOV_B32(MAKE_VGPR(6), MAKE_IMM_INT(DSEL_B));
        shader->V_MOV_B32(MAKE_VGPR(7), MAKE_IMM_INT(DSEL_A));

        //S_LSHL_B32 s16, s16, 6    //s16 = (s_tgid_x << 6) = (s_tgid_x * 64)
        //shader->SOP2_OP(SOP2_LSHL_B32, MAKE_SGPR(16), MAKE_SGPR(16), MAKE_IMM_INT(6));

        //V_ADD_I32   v0,  s16, v0  //v0 = (s16 + v_thread_id_x)
        //shader->VOP2_OP(VOP2_ADD_I32, MAKE_VGPR(0), MAKE_SGPR(16), MAKE_VGPR(0));

        shader->V_MOV_B32(MAKE_VGPR(0), MAKE_IMM_INT(0));
        shader->V_MOV_B32(MAKE_VGPR(1), MAKE_IMM_INT(0));

        //shader->S_WAITCNT(0, 0, 0);

        const uint buf_store_fmt[] = {
          MUBUF_BUFFER_STORE_FORMAT_X,
          MUBUF_BUFFER_STORE_FORMAT_XY,
          MUBUF_BUFFER_STORE_FORMAT_XYZ,
          MUBUF_BUFFER_STORE_FORMAT_XYZW
        };

        for (int store_cnt = 0; store_cnt < 4; store_cnt++) {

            if ((used_flags & BUF_OFFEN) != 0) {
                shader->V_MOV_B32(MAKE_VGPR(0), MAKE_IMM_INT(store_cnt * sizeof(t_vector4u)));
            }
            else
            if ((used_flags & BUF_IDXEN) != 0) {
                shader->V_MOV_B32(MAKE_VGPR(0), MAKE_IMM_INT(store_cnt));
            };
            
            //s[0..3]
            shader->MUBUF_OP(buf_store_fmt[store_cnt],
                MAKE_VGPR(4),    //src
                MAKE_VGPR(0),    //addr
                MAKE_SGPR(0),    //V#
                0,               //offset
                MAKE_IMM_INT(0), //soffset
                used_flags
            );

            //s[4..7]
            shader->MUBUF_OP(buf_store_fmt[store_cnt],
                MAKE_VGPR(4),    //src
                MAKE_VGPR(0),    //addr
                MAKE_SGPR(4),    //V#
                0,               //offset
                MAKE_IMM_INT(0), //soffset
                used_flags
            );

            //s[8..11]
            shader->MUBUF_OP(buf_store_fmt[store_cnt],
                MAKE_VGPR(4),    //src
                MAKE_VGPR(0),    //addr
                MAKE_SGPR(8),    //V#
                0,               //offset
                MAKE_IMM_INT(0), //soffset
                used_flags
            );

            //s[12..15]
            shader->MUBUF_OP(buf_store_fmt[store_cnt],
                MAKE_VGPR(4),    //src
                MAKE_VGPR(0),    //addr
                MAKE_SGPR(12),   //V#
                0,               //offset
                MAKE_IMM_INT(0), //soffset
                used_flags
            );

        };

        shader->S_WAITCNT(0, 0, 0);

     });


    t.shader_ptr = linear_dmem->alloc(shader->GetByteSize(), 256);

    t.regs = shader->ExportCs(t.shader_ptr);

    //bulder not need anymore
    delete shader;

    printf("alloc elements=%d\n", sizeof(t_all_elem));

    t_all_elem* elements = (t_all_elem*)linear_dmem->alloc(sizeof(t_all_elem), 4);

    t.dst = (uint*)elements; //save to

    for (int elem_cnt = 0; elem_cnt < 4; elem_cnt++)
        for (int i = 0; i < max_sel_count; i++)
            for (int store_cnt = 0; store_cnt < 4; store_cnt++) {

                memcpy(&elements->per_elem_cnt[elem_cnt].per_dst_sel[i].per_store_count[store_cnt], &init_vector_8, sizeof(init_vector_8));

            };

    //They are passed directly to the registers, so it's just static
    VSharpResource4 dst_vsharp = {};

    dst_vsharp.setVMemoryType(VMemoryTypeSC);
    dst_vsharp.nfmt = BUF_NUM_FORMAT_UINT;
    
    cmd_buf->SetCsShader(&t.regs);

    for (int i = 0; i < max_sel_count; i++) {

        //printf("[%d]\n", i);

        dst_vsharp.dst_sel_x = (i >> (3 * 0)) & 7;
        dst_vsharp.dst_sel_y = (i >> (3 * 1)) & 7;
        dst_vsharp.dst_sel_z = (i >> (3 * 2)) & 7;
        dst_vsharp.dst_sel_w = (i >> (3 * 3)) & 7;

        const uint buf_data_format[] = {
            BUF_DATA_FORMAT_32,
            BUF_DATA_FORMAT_32_32,
            BUF_DATA_FORMAT_32_32_32,
            BUF_DATA_FORMAT_32_32_32_32
        };

        for (int elem_cnt = 0; elem_cnt < 4; elem_cnt++) {

            void* dst = &elements->per_elem_cnt[elem_cnt].per_dst_sel[i];

            dst_vsharp.setVMemoryPtrs(dst, 0, sizeof(t_vector4u), 4);
            dst_vsharp.dfmt = buf_data_format[elem_cnt];
            cmd_buf->SetUserDataCsRegs(elem_cnt * 4, 4, &dst_vsharp);

        };

        cmd_buf->DispatchDirect(1, 1, 1);

    };


    test_after_action.push_back({ 
        
        [](t_cs_shader_test* test) {

            const char* _0123RGBA = "0123RGBA";

            t_all_elem* elements = (t_all_elem*)test->dst; //load from 

            for (int elem_cnt = 0; elem_cnt < 4; elem_cnt++)
                for (int store_cnt = 0; store_cnt < 4; store_cnt++) {

                    printf("store[s:%d->e:%d]\n", store_cnt + 1, elem_cnt + 1);

                    for (int i = 0; i < max_sel_count; i++) {

                        uint* dst = (uint*)(&elements->per_elem_cnt[elem_cnt].per_dst_sel[i].per_store_count[store_cnt]);

                        int w;

                        w = 0;
                        if (elem_cnt >= 0) w |= (dst[0] << (3 * 3));
                        if (elem_cnt >= 1) w |= (dst[1] << (3 * 2));
                        if (elem_cnt >= 2) w |= (dst[2] << (3 * 1));
                        if (elem_cnt >= 3) w |= (dst[3] << (3 * 0));

                        int z;

                        t_swizzle s = {};

                        s.x = (i >> (3 * 0)) & 7;
                        s.y = (i >> (3 * 1)) & 7;
                        s.z = (i >> (3 * 2)) & 7;
                        s.w = (i >> (3 * 3)) & 7;

                        s = get_reverse_dst_sel(s, elem_cnt + 1, store_cnt + 1);

                        z = 0;

                        bool valid;

                        if (((used_flags & BUF_IDXEN) != 0) && (elem_cnt == 0) && (store_cnt == 0) && (s.x == 3))
                        {
                            z = 0x3f800001; //special case, HW bug?

                            valid = (dst[0] == z);
                        }
                        else {

                            if (s.x == 3) s.x = 0;

                            z |= (s.x << (3 * 3));
                            z |= (s.y << (3 * 2));
                            z |= (s.z << (3 * 1));
                            z |= (s.w << (3 * 0));

                            valid = (w == z);
                        };

                        if (!valid)
                            printf("dst_sel[%d->%d] [%c%c%c%c] = %01x %01x %01x %01x (%04o) == %04o -> %s\n",

                                store_cnt + 1,
                                elem_cnt + 1,

                                _0123RGBA[(i >> (3 * 0)) & 7],
                                _0123RGBA[(i >> (3 * 1)) & 7],
                                _0123RGBA[(i >> (3 * 2)) & 7],
                                _0123RGBA[(i >> (3 * 3)) & 7],

                                dst[0],
                                dst[1],
                                dst[2],
                                dst[3],

                                w,
                                z,
                                PASS[valid]

                            );


                    }; //for


                }; //for


            }, t });

    return t;


};

/////////////////////////////////////////////LOAD

static int _get_load_dst_sel_1(int i) {
    switch (i) {
    case 2:return 1;
    case 3:return 4;
    case 4:
    case 5:
    case 6:
    case 7:return 4;
    };
    return i;
};

static int _get_load_dst_sel_2(int i) {
    switch (i) {
    case 2:return 1;
    case 3:return 0;
    case 6:return 4;
    case 7:return 5;
    };
    return i;
};

static int _get_load_dst_sel_3(int i) {
    switch (i) {
    case 2:return 1;
    case 3:return 0;
    case 7:return 4;
    };
    return i;
};

static int _get_load_dst_sel_4(int i) {
    switch (i) {
    case 2:return 1;
    case 3:return 0;
    };
    return i;
};

static t_swizzle get_load_dst_sel(t_swizzle dst, int elem_count) {

    t_swizzle result = {};

    switch (elem_count) {
    case 1:

        result.x = _get_load_dst_sel_1(dst.x);
        result.y = _get_load_dst_sel_1(dst.y);
        result.z = _get_load_dst_sel_1(dst.z);
        result.w = _get_load_dst_sel_1(dst.w);

        break;
    case 2:
   
        result.x = _get_load_dst_sel_2(dst.x);
        result.y = _get_load_dst_sel_2(dst.y);
        result.z = _get_load_dst_sel_2(dst.z);
        result.w = _get_load_dst_sel_2(dst.w);

        break;
    case 3:

        result.x = _get_load_dst_sel_3(dst.x);
        result.y = _get_load_dst_sel_3(dst.y);
        result.z = _get_load_dst_sel_3(dst.z);
        result.w = _get_load_dst_sel_3(dst.w);

        break;
    case 4:

        result.x = _get_load_dst_sel_4(dst.x);
        result.y = _get_load_dst_sel_4(dst.y);
        result.z = _get_load_dst_sel_4(dst.z);
        result.w = _get_load_dst_sel_4(dst.w);

        break;
    };

    return result;
};

static t_cs_shader_test make_cs_test_dst_sel_load(t_linear_alloc* linear_dmem, CmdBuf* cmd_buf) {

    t_cs_shader_test t = {};

    auto shader = make_cs_shader(0,
        0,
        [](ShaderBuilder* shader) {

            shader->user_count = 16;
            shader->sgpr_count = 16;
            shader->vgpr_count = 20;

            shader->V_MOV_B32(MAKE_VGPR(0), MAKE_IMM_INT(0));
            shader->V_MOV_B32(MAKE_VGPR(1), MAKE_IMM_INT(0));

            for (int i = 4; i < 20; i++) {
                shader->V_MOV_B32(MAKE_VGPR(i), MAKE_IMM_INT(9));
            };

            const uint buf_load_fmt[] = {
              MUBUF_BUFFER_LOAD_FORMAT_X,
              MUBUF_BUFFER_LOAD_FORMAT_XY,
              MUBUF_BUFFER_LOAD_FORMAT_XYZ,
              MUBUF_BUFFER_LOAD_FORMAT_XYZW
            };

            for (int load_cnt = 0; load_cnt < 4; load_cnt++) {

                //s[0..3]
                shader->MUBUF_OP(buf_load_fmt[load_cnt],
                    MAKE_VGPR(4 + load_cnt * 4), //src   //4..7 
                    MAKE_VGPR(0),                //addr
                    MAKE_SGPR(0),                //V#
                    0,                           //offset
                    MAKE_IMM_INT(0),             //soffset
                    used_flags
                );

            };

            shader->S_WAITCNT(0, 0, 0);

            for (int load_cnt = 0; load_cnt < 4; load_cnt++) {

                if ((used_flags & BUF_OFFEN) != 0) {
                    shader->V_MOV_B32(MAKE_VGPR(0), MAKE_IMM_INT(load_cnt * sizeof(t_vector4u)));
                }
                else
                if ((used_flags & BUF_IDXEN) != 0) {
                    shader->V_MOV_B32(MAKE_VGPR(0), MAKE_IMM_INT(load_cnt));
                };

                //s[4..7]
                shader->MUBUF_OP(MUBUF_BUFFER_STORE_FORMAT_XYZW,
                    MAKE_VGPR(4 + load_cnt * 4),    //src
                    MAKE_VGPR(0),                   //addr
                    MAKE_SGPR(4),                   //V#
                    0,                              //offset
                    MAKE_IMM_INT(0),                //soffset
                    used_flags
                );

            };

            shader->S_WAITCNT(0, 0, 0);

        });


    t.shader_ptr = linear_dmem->alloc(shader->GetByteSize(), 256);

    t.regs = shader->ExportCs(t.shader_ptr);

    //bulder not need anymore
    delete shader;

    t.src = (uint*)linear_dmem->alloc(4 * 4, 4);

    t.src[0] = DSEL_R;
    t.src[1] = DSEL_G;
    t.src[2] = DSEL_B;
    t.src[3] = DSEL_A;

    printf("alloc elements=%d\n", sizeof(t_all_elem));

    t_all_elem* elements = (t_all_elem*)linear_dmem->alloc(sizeof(t_all_elem), 4);

    t.dst = (uint*)elements; //save to

    for (int elem_cnt = 0; elem_cnt < 4; elem_cnt++)
        for (int i = 0; i < max_sel_count; i++)
            for (int load_cnt = 0; load_cnt < 4; load_cnt++) {

                memcpy(&elements->per_elem_cnt[elem_cnt].per_dst_sel[i].per_store_count[load_cnt], &init_vector_8, sizeof(init_vector_8));

            };

    //They are passed directly to the registers, so it's just static
    VSharpResource4 src_vsharp = {};
    VSharpResource4 dst_vsharp = {};

    src_vsharp.setVMemoryType(VMemoryTypeRO);
    src_vsharp.setVMemoryPtrs(t.src, 0, sizeof(uint) * 4, 1);
    src_vsharp.nfmt = BUF_NUM_FORMAT_UINT;
    src_vsharp.dfmt = BUF_DATA_FORMAT_32_32_32_32;
        
    dst_vsharp.setVMemoryType(VMemoryTypeSC);
    dst_vsharp.setChannelOrder(DSEL_R, DSEL_G, DSEL_B, DSEL_A);
    dst_vsharp.nfmt = BUF_NUM_FORMAT_UINT;
    dst_vsharp.dfmt = BUF_DATA_FORMAT_32_32_32_32;

    cmd_buf->SetCsShader(&t.regs);

    for (int i = 0; i < max_sel_count; i++) {

        //printf("[%d]\n", i);

        src_vsharp.dst_sel_x = (i >> (3 * 0)) & 7;
        src_vsharp.dst_sel_y = (i >> (3 * 1)) & 7;
        src_vsharp.dst_sel_z = (i >> (3 * 2)) & 7;
        src_vsharp.dst_sel_w = (i >> (3 * 3)) & 7;

        const uint buf_data_format[] = {
            BUF_DATA_FORMAT_32,
            BUF_DATA_FORMAT_32_32,
            BUF_DATA_FORMAT_32_32_32,
            BUF_DATA_FORMAT_32_32_32_32
        };

        for (int elem_cnt = 0; elem_cnt < 4; elem_cnt++) {

            //printf("[%d %d]\n", elem_cnt, i);

            void* dst = &elements->per_elem_cnt[elem_cnt].per_dst_sel[i];

            src_vsharp.dfmt = buf_data_format[elem_cnt];
            cmd_buf->SetUserDataCsRegs(0, 4, &src_vsharp);

            dst_vsharp.setVMemoryPtrs(dst, 0, sizeof(t_vector4u), 4);
            cmd_buf->SetUserDataCsRegs(4, 4, &dst_vsharp);

            cmd_buf->DispatchDirect(1, 1, 1);
            
        };

    };


    test_after_action.push_back({

        [](t_cs_shader_test* test) {

            const char* _0123RGBA = "0123RGBA";

            t_all_elem* elements = (t_all_elem*)test->dst; //load from 

            for (int elem_cnt = 0; elem_cnt < 4; elem_cnt++)
                for (int load_cnt = 0; load_cnt < 4; load_cnt++) {

                    printf("load[s:%d->e:%d]\n", load_cnt + 1, elem_cnt + 1);

                    for (int i = 0; i < max_sel_count; i++) {

                        uint* dst = (uint*)(&elements->per_elem_cnt[elem_cnt].per_dst_sel[i].per_store_count[load_cnt]);

                        int w;

                        w = 0;
                        if (load_cnt >= 0) w |= (dst[0] << (3 * 3));
                        if (load_cnt >= 1) w |= (dst[1] << (3 * 2));
                        if (load_cnt >= 2) w |= (dst[2] << (3 * 1));
                        if (load_cnt >= 3) w |= (dst[3] << (3 * 0));

                        int z;

                        t_swizzle s = {};

                        if (load_cnt >= 0) s.x = (i >> (3 * 0)) & 7;
                        if (load_cnt >= 1) s.y = (i >> (3 * 1)) & 7;
                        if (load_cnt >= 2) s.z = (i >> (3 * 2)) & 7;
                        if (load_cnt >= 3) s.w = (i >> (3 * 3)) & 7;

                        s = get_load_dst_sel(s, elem_cnt + 1);

                        z = 0;

                        bool valid;

                        z |= (s.x << (3 * 3));
                        z |= (s.y << (3 * 2));
                        z |= (s.z << (3 * 1));
                        z |= (s.w << (3 * 0));

                        valid = (w == z);

                        if (!valid)
                            printf("dst_sel[%d->%d] [%c%c%c%c] = %01x %01x %01x %01x (%04o) == %04o -> %s\n",

                                load_cnt + 1,
                                elem_cnt + 1,

                                _0123RGBA[(i >> (3 * 0)) & 7],
                                _0123RGBA[(i >> (3 * 1)) & 7],
                                _0123RGBA[(i >> (3 * 2)) & 7],
                                _0123RGBA[(i >> (3 * 3)) & 7],

                                dst[0],
                                dst[1],
                                dst[2],
                                dst[3],

                                w,
                                z,
                                PASS[valid]

                            );


                    }; //for


                }; //for


            }, t });

    printf("[4]\n");

    return t;


};

//

int main(void)
{
    // No buffering
    setvbuf(stdout, NULL, _IONBF, 0);

    int neo_mode = sceKernelIsNeoMode();
    int ret;

    hVideo = sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN, ORBIS_VIDEO_OUT_BUS_MAIN, 0, 0);

    printf("sceVideoOutOpen->%d\n", hVideo);

    uint64_t* labels = {};

    ret = sceVideoOutGetBufferLabelAddress(hVideo, &labels);
    printf("sceVideoOutGetBufferLabelAddress->%d %s\n", ret, PASS[ret == 16]);

    printf("labels->%p %s\n", labels,  PASS[labels != NULL]);

    void* labels_page = (void*)(((uintptr_t)labels) & (~0x3FFF));

    printf("labels_page->%p\n", labels_page);

    OrbisKernelVirtualQueryInfo info = {};

    ret = sceKernelVirtualQuery(labels_page, 0, &info, sizeof(info));

    // printf("sceKernelVirtualQuery  =%d\n", ret);
    // printf("  info.start           =%p\n", info.start_addr);
    // printf("  info.end             =%p\n", info.end_addr);
    // printf("  info.protection      =%x\n", info.prot);
    // printf("  info.memoryType      =%d\n", info.mtype);
    // printf("  info.isFlexibleMemory=%d\n", info.isFlexibleMemory);
    // printf("  info.isDirectMemory  =%d\n", info.isDirectMemory);
    // printf("  info.isStack         =%d\n", info.isStack);
    // printf("  info.isPooledMemory  =%d\n", info.isPooledMemory);
    // printf("  info.isCommitted     =%d\n", info.isCommitted);
    // printf("  info.name            =%s\n", info.name);

    ret = sceVideoOutSetFlipRate(hVideo, 0);
    printf("sceVideoOutSetFlipRate->%d %s\n", ret, PASS[ret == 0]);

    ///////////

    OrbisKernelEqueue dce_eq = {};

    ret = sceKernelCreateEqueue(&dce_eq, "dce_eq");
    printf("sceKernelCreateEqueue->%d %s\n", ret, PASS[ret == 0]);

    ret = sceVideoOutAddFlipEvent(dce_eq, hVideo, &dce_eq);
    printf("sceVideoOutAddFlipEvent->%d %s\n", ret, PASS[ret == 0]);

    //////////

    auto frame = new Frame(FRAME_WIDTH, FRAME_HEIGHT, neo_mode);

    ret = frame->alloc();
    printf("alloc()->%d\n", ret);

    OrbisVideoOutBufferAttribute attr = frame->get_attr();

    /*
    {
     
        void* dmem_ptr = {};

        int offset = 1;

        while (offset < (16 * 1024)) {

            dmem_ptr = (void*)((uintptr_t)frame->dmem_ptr + offset);

            ret = sceVideoOutRegisterBuffers(hVideo, 2, &dmem_ptr, 1, &attr);
            printf("sceVideoOutRegisterBuffers[+%d]->%d\n", offset, ret);

            sceVideoOutUnregisterBuffers(hVideo, ret);

            offset = offset << 1;
        }

       
    }
    */


    printf("[sceVideoOutRegisterBuffers]\n");
    ret = sceVideoOutRegisterBuffers(hVideo, 0, &frame->dmem_ptr, 1, &attr);
    printf("sceVideoOutRegisterBuffers->%d %s\n", ret, PASS[ret == 0]);

    ret = sceVideoOutRegisterBuffers(hVideo, 1, &frame->dmem_ptr, 1, &attr);
    printf("sceVideoOutRegisterBuffers->%d %s\n", ret, PASS[ret == 1]);

    ret = sceVideoOutRegisterBuffers(hVideo, 2, &frame->dmem_ptr, 1, &attr);
    printf("sceVideoOutRegisterBuffers->%d %s\n", ret, PASS[ret == 2]);


    //ret = sceVideoOutSubmitChangeBufferAttribute(hVideo, 2, &attr);
    //printf("sceVideoOutSubmitChangeBufferAttribute(2)->%d %s\n", ret, PASS[ret == -2144796662]); //ERROR_INVALID_INDEX 
    //
    //ret = sceVideoOutSubmitChangeBufferAttribute(hVideo, 0, &attr);
    //printf("sceVideoOutSubmitChangeBufferAttribute(0)->%d %s\n", ret, PASS[ret == 0]);
    //
    //ret = sceVideoOutSubmitChangeBufferAttribute(hVideo, 0, &attr);
    //printf("sceVideoOutSubmitChangeBufferAttribute(0)->%d %s\n", ret, PASS[ret == -2144796663]); //ERROR_RESOURCE_BUSY 
    //
    //ret = sceVideoOutSubmitChangeBufferAttribute(hVideo, 1, &attr);
    //printf("sceVideoOutSubmitChangeBufferAttribute(1)->%d %s\n", ret, PASS[ret == -2144796663]); //ERROR_RESOURCE_BUSY

    memset(frame->dmem_ptr, 0xFF, frame->size);

    OrbisVideoOutFlipStatus2 FlipStatus;

    if (labels != NULL) {
        printf("[before submit] labels[0]->%d %s\n", labels[0], PASS[labels[0] == 0]);
    }

    ret = sceVideoOutSubmitFlip(hVideo, 0, FLIP_VSYNC, 0x100);
    printf("sceVideoOutSubmitFlip->%d %s\n", ret, PASS[ret == 0]);

    ret = sceVideoOutSubmitFlip(hVideo, -1, FLIP_VSYNC, 0x200);
    printf("sceVideoOutSubmitFlip->%d %s\n", ret, PASS[ret == 0]);

    FlipStatus = {};
    sceVideoOutGetFlipStatus(hVideo, &FlipStatus);

    printf("[after submit] FlipStatus-> count:%d numGpuFlipPending:%d numFlipPending:%d\n",
        FlipStatus.count, FlipStatus.numGpuFlipPending, FlipStatus.numFlipPending);

    if (labels != NULL) {
        printf("[after submit] labels[0]->%d labels[1]->%d\n", labels[0], labels[1]);

        while (FlipStatus.numFlipPending != 0) {
            sceKernelUsleep(1000);

            FlipStatus = {};
            sceVideoOutGetFlipStatus(hVideo, &FlipStatus);

            printf("[after submit] FlipStatus-> count:%d numGpuFlipPending:%d numFlipPending:%d\n",
                FlipStatus.count, FlipStatus.numGpuFlipPending, FlipStatus.numFlipPending);

            printf("[after submit] labels[0]->%d labels[1]->%d\n", labels[0], labels[1]);
        };
    }

    printf("[after submit] FlipStatus-> count:%d %s\n", FlipStatus.count, PASS[FlipStatus.count == 1]);
    printf("[after submit] FlipStatus-> numGpuFlipPending:%d %s\n", FlipStatus.numGpuFlipPending, PASS[FlipStatus.numGpuFlipPending == 0]);
    printf("[after submit] FlipStatus-> numFlipPending:%d %s\n", FlipStatus.numFlipPending, PASS[FlipStatus.numFlipPending == 0]);

    OrbisKernelEvent event = {};
    int event_count = {};
    t_dce_data dce_data;

    ret = sceKernelWaitEqueue(dce_eq, &event, 1, &event_count, NULL);
    printf("sceKernelWaitEqueue->%d %s\n", ret, PASS[ret == 0]);

    //0x%016llX

    printf("[kevent]\n");
    printf(" ident =0x%016llX %s\n", event.ident, PASS[event.ident == 0x0006000000000000]);
    printf(" filter=%d %s\n", event.filter, PASS[event.filter == -13]);
    printf(" data  =0x%016llX\n", event.data);

    dce_data = *reinterpret_cast<t_dce_data*>(&event.data);

    printf("  time    =%d\n", dce_data.time);
    printf("  counter =%d %s\n", dce_data.counter, PASS[dce_data.counter == 1]);
    printf("  flip_arg=0x%016llX %s\n", dce_data.flip_arg, PASS[dce_data.flip_arg == 0x100]);

    printf(" udata =0x%016llX %s\n", event.udata, PASS[event.udata == &dce_eq]);

    //////////////////
    RENDER_TARGET rt = {};

    rt.BASE = (uint64_t)frame->dmem_ptr >> 8; //BASE_256B
    rt.PITCH.TILE_MAX = (frame->pad_width / 8) - 1;
    rt.SLICE.TILE_MAX = (frame->pad_height * frame->pad_width / 64) - 1;

    rt.INFO.FORMAT = COLOR_8_8_8_8;
    rt.INFO.NUMBER_TYPE = NUMBER_SRGB;
    rt.INFO.COMP_SWAP = SWAP_ALT; //BGRA
    rt.INFO.BLEND_CLAMP = 1;
    rt.INFO.ALT_TILE_MODE = (neo_mode != 0);

    rt.ATTRIB.TILE_MODE_INDEX = 0xA;

    rt.CLEAR_WORD = 0xFF0000;

    rt.hint.width = frame->width;
    rt.hint.height = frame->height;
    /////////////////


    t_linear_alloc linear_dmem = {};

    linear_dmem.init(16*1024*1024);

    //vs shader (256 byte aligned)
    void* ptr_vs_shader = linear_dmem.alloc(sizeof(simple_vs_shader), 256);

    //fetch shader (4 byte align)
    void* ptr_vs_fetch = linear_dmem.alloc(sizeof(simple_fetch), 4);

    //ps shader (256 byte aligned)
    void* ptr_ps_shader = linear_dmem.alloc(sizeof(simple_ps_shader), 256);

    memcpy(ptr_vs_shader, &simple_vs_shader, sizeof(simple_vs_shader));
    memcpy(ptr_vs_fetch, &simple_fetch, sizeof(simple_fetch));
    memcpy(ptr_ps_shader, &simple_ps_shader, sizeof(simple_ps_shader));

    //two V# (4 byte align)
    void* ptr_two_vsharp = linear_dmem.alloc(sizeof(VSharpResource4) * 2, 4);

    VSharpResource4* ptr_pos_vsharp = ((VSharpResource4*)ptr_two_vsharp) + 0;
    VSharpResource4* ptr_clr_vsharp = ((VSharpResource4*)ptr_two_vsharp) + 1;

    printf("ptr_pos_vsharp->0x%016llX\n", ptr_pos_vsharp);
    printf("ptr_clr_vsharp->0x%016llX\n", ptr_clr_vsharp);

    Gnm::VsStageRegisters simple_vs_shader_regs = const_simple_vs_shader_regs;
    Gnm::PsStageRegisters simple_ps_shader_regs = const_simple_ps_shader_regs;

    PatchShaderPtr(&simple_vs_shader_regs, ptr_vs_shader);
    PatchShaderPtr(&simple_ps_shader_regs, ptr_ps_shader);

    struct vec2 {
        float x;
        float y;
    };

    struct vec3 {
        float x;
        float y;
        float z;
    };

    struct vec4b {
        uint8_t x;
        uint8_t y;
        uint8_t z;
        uint8_t w;
    };

    struct Vertex {
        vec2 pos; //BUF_DATA_FORMAT_32_32     BUF_NUM_FORMAT_FLOAT
        vec3 clr; //BUF_DATA_FORMAT_32_32_32  BUF_NUM_FORMAT_FLOAT
    };

    struct Vertex2 {
        vec2 pos; //BUF_DATA_FORMAT_32_32     BUF_NUM_FORMAT_FLOAT
        vec4b clr;
    };

    const Vertex vertices[] = {
        {{0.0f, 0.5f}, {1.0f, 0.0f, 0.0f}},  //R 0
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, //G 1
        {{-0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},//B 2
        //
        //
        {{0.75f, 0.5f}, {1.0f, 1.0f, 1.0f}}, //W 3
        {{0.0f, 0.5f}, {1.0f, 0.0f, 0.0f}} //R 4
    };

    const Vertex2 vertices2[] = {
        {{0.0f, 0.5f}, {255, 0, 0, 1}},  //R 0
        {{0.5f, -0.5f}, {0, 255, 0, 1}}, //G 1
        {{-0.5f, -0.5f}, {0, 0, 255, 1}},//B 2
        //
        //
        {{0.75f, 0.5f}, {255, 255, 255, 1}}, //W 3
        {{0.0f, 0.5f},  {255, 0, 0, 1}} //R 4
    };

    const short vindex[] = {
        0,0,0,
        1,0,3,  //index_offset
        0,3,2   //index_offset and vertex_offset
    };

    //vertices (4 byte align)
    void* ptr_vertices = linear_dmem.alloc(sizeof(vertices), 4);

    memcpy(ptr_vertices, &vertices, sizeof(vertices));

    //vindex (4 byte align)
    void* ptr_vindex = linear_dmem.alloc(sizeof(vindex), 4);

    memcpy(ptr_vindex, &vindex, sizeof(vindex));

    ptr_pos_vsharp->setVMemoryType(VMemoryTypeRO);
    ptr_pos_vsharp->setVMemoryPtrs((void*)(((ulong)ptr_vertices)+1), offsetof(Vertex, pos), sizeof(Vertex), 5);
    ptr_pos_vsharp->setChannelOrder(DSEL_R, DSEL_G, DSEL_0, DSEL_1);
    ptr_pos_vsharp->nfmt = BUF_NUM_FORMAT_FLOAT;
    ptr_pos_vsharp->dfmt = BUF_DATA_FORMAT_32_32;

    ptr_clr_vsharp->setVMemoryType(VMemoryTypeRO);
    ptr_clr_vsharp->setVMemoryPtrs(ptr_vertices, offsetof(Vertex, clr), sizeof(Vertex), 5);
    ptr_clr_vsharp->setChannelOrder(DSEL_R, DSEL_G, DSEL_B, DSEL_0);
    ptr_clr_vsharp->nfmt = BUF_NUM_FORMAT_FLOAT;
    ptr_clr_vsharp->dfmt = BUF_DATA_FORMAT_32_32_32;

    //
    //ptr_clr_vsharp->setVMemoryPtrs(ptr_vertices, offsetof(Vertex2, clr), sizeof(Vertex2), 5);
    //ptr_clr_vsharp->setChannelOrder(DSEL_1, DSEL_G, DSEL_B, DSEL_A);
    //ptr_clr_vsharp->dfmt = BUF_DATA_FORMAT_RESERVED;
    //
  
    //ptr_clr_vsharp->nfmt = BUF_NUM_FORMAT_UNORM;
    //ptr_clr_vsharp->dfmt = BUF_DATA_FORMAT_8_8_8_8;
  
    //BUF_DATA_FORMAT_RESERVED 0x0f

    //test V# (4 byte align)
    VSharpResource4* ptr_test_vsharp = (VSharpResource4*)linear_dmem.alloc(sizeof(VSharpResource4), 4);

    uint* ptr_test = (uint*)linear_dmem.alloc(4, 4);

    //{1.0f, 0.0f, 0.0f}

    ptr_test[0] = F32ToF11(1.0f) << 10 | F32ToF10(0.5f);
    
    ptr_test_vsharp->setVMemoryType(VMemoryTypeRO);
    ptr_test_vsharp->setVMemoryPtrs(ptr_test, 0, 4, 1);
    ptr_test_vsharp->setChannelOrder(DSEL_R, DSEL_G, DSEL_B, DSEL_0);
    ptr_test_vsharp->nfmt = BUF_NUM_FORMAT_FLOAT;
    ptr_test_vsharp->dfmt = BUF_DATA_FORMAT_11_11_10 /*BUF_DATA_FORMAT_10_11_11*/;

    //10_11_11
    //      RR  GG  BB
    //high [10][11][11] low

    //11_11_10
    //      RR  GG  BB
    //high [11][11][10] low

    auto cmd_buf = new CmdBuf(16 * 1024 * 1024, neo_mode);

    ret = cmd_buf->alloc();
    printf("alloc()->%d\n", ret);

    cmd_buf->reset();

    cmd_buf->InitDefault();

    cmd_buf->SetRenderTarget(0, &rt);

    cmd_buf->setRenderTargetMask(0xF);

    cmd_buf->setViewport2D(0, 0, 0, frame->width, frame->height, 0.0, 1.0);

    cmd_buf->setViewportTransformControl(PA_CL_VTE_CNTL::getDefault()); //init default

    cmd_buf->setScreenScissor(0, 0, frame->width, frame->height);

    cmd_buf->setWindowOffset(100, 0);
    cmd_buf->setGenericScissor(0, 100, frame->width, frame->height, 1);

    cmd_buf->setWindowScissor(0, 0, frame->width - 100, frame->height, 0);

    cmd_buf->setScanModeControl(0, 1);
    cmd_buf->setViewportScissor(0, 0, 0, frame->width, frame->height - 100, 0);

    //60*16=960
    //32*16=512
    cmd_buf->setHardwareScreenOffset(60, 32);

    cmd_buf->setGuardBands(32.0, 60.0, 1.0, 1.0);

    //draw black rect
    cmd_buf->SetEmbeddedVsShader();
    cmd_buf->SetEmbeddedPsShader();

    cmd_buf->setPrimitiveType(PRIM_RECT_LIST);

    //cmd_buf->NopOverflow(); // test PM4 packed overflow

    cmd_buf->DrawIndexAuto(3); //rect_list 
    //draw black rect

    //wait to draw
    cmd_buf->waitForGraphicsWrites(0, 1, 0,
        CacheActionWBAndInvL1andL2,
        ExtCacheActionFlushAndInvCbCache,
        StallCBParserDisable);

    ///// draw triangle
    cmd_buf->SetVsShader(&simple_vs_shader_regs);
    cmd_buf->SetPsShader(&simple_ps_shader_regs);

    //mmSPI_PS_INPUT_CNTL_0 0xA191 
    cmd_buf->SetCtxReg(0xA191, 0);

    cmd_buf->SetUserDataVsPtr(0, ptr_vs_fetch);
    cmd_buf->SetUserDataVsPtr(2, ptr_two_vsharp);
   
    cmd_buf->SetUserDataPsRegs(0, 4, ptr_test_vsharp);

    cmd_buf->setPrimitiveType(PRIM_TRI_LIST);

    cmd_buf->DrawIndexAuto(3);
    ///// draw triangle

    //wait to draw
    cmd_buf->waitForGraphicsWrites(0, 1, 0,
        CacheActionWBAndInvL1andL2,
        ExtCacheActionFlushAndInvCbCache,
        StallCBParserDisable);

    cmd_buf->setIndexSize(IndexSize16, 0);
    //cmd_buf->setIndexBuffer(ptr_vindex);

    off_t  dmem_ofs_index ={};
    short* dmem_big_index ={};

    const int big_index_len = 64 * 1024;
    //const int big_index_len2 = (12) * 1024;

    const int big_index_len2 = (64) * 1024;

    //INDEX CACHE SIZE = 12-13KB!
    //dcbSizesInBytes->2380


    ret = sceKernelAllocateMainDirectMemory(
        (size_t)big_index_len,
        (size_t)16 * 1024,
        WB_ONION,
        &dmem_ofs_index);
    printf("sceKernelAllocateMainDirectMemory->%d %d\n", ret, dmem_ofs_index);

    ret = sceKernelMapDirectMemory(
        (void**)&dmem_big_index,
        big_index_len/2,
        VM_PROT_READ | VM_PROT_WRITE | VM_PROT_GPU_ALL,
        0,
        dmem_ofs_index + big_index_len / 2, //revers map
        16 * 1024);

    printf("sceKernelMapDirectMemory->%d 0x%016llX\n", ret, dmem_big_index);

    short* dmem_big_index2 = dmem_big_index + (big_index_len / 2 / 2);

    ret = sceKernelMapDirectMemory(
        (void**)&dmem_big_index2,
        big_index_len / 2,
        VM_PROT_READ | VM_PROT_WRITE | VM_PROT_GPU_ALL,
        MAP_FIXED,
        dmem_ofs_index, //revers map
        16 * 1024);

    printf("sceKernelMapDirectMemory->%d 0x%016llX\n", ret, dmem_big_index2);

    uint* wait_addr = (uint*)linear_dmem.alloc(4, 4);

    //It looks like nop packets are not cached in pfp
    //cmd_buf->Nop(4 * 1024 / 4);

    cmd_buf->waitOnAddress(wait_addr, 0xFFFFFFFF, WAIT_REG_MEM_FUNC_EQUAL, 1);

    //cmd_buf->waitOnAddressAndStallCommandBufferParser(wait_addr, 0xFFFFFFFF, WAIT_REG_MEM_FUNC_EQUAL, 1);
  
    int last_id = {};

    last_id = (big_index_len / 2 / 3) - 1;

    dmem_big_index[last_id * 3 + 0] = 1;
    dmem_big_index[last_id * 3 + 1] = 1;
    dmem_big_index[last_id * 3 + 2] = 1;

    //cmd_buf->DrawIndex(3, &dmem_big_index[last_id * 3 + 0]);

    last_id = (big_index_len2 / 2 / 3) - 1;

    dmem_big_index[last_id * 3 + 0] = 1;
    dmem_big_index[last_id * 3 + 1] = 4;
    dmem_big_index[last_id * 3 + 2] = 3;

    ///cmd_buf->DrawIndex((big_index_len - big_index_len2) / 2, dmem_big_index + (big_index_len2 / 2));

    //wait to draw
    //cmd_buf->waitForGraphicsWrites(0, 1, 0,
    //    CacheActionWBAndInvL1andL2,
    //    ExtCacheActionFlushAndInvCbCache,
    //    StallCBParserDisable);

    //PFP caches part of the index taking into account the offset
    cmd_buf->setIndexBuffer(dmem_big_index);
    cmd_buf->DrawIndexOffset((last_id * 3 + 3) - (12 * 1024 / 2 / 3 * 3), (12 * 1024 / 2 / 3 * 3));

    ///cmd_buf->DrawIndex(last_id * 3 + 3, dmem_big_index);

    //cmd_buf->setIndexOffset(1);  //vertex_offset -> index_id + 1

    /*
    //cmd_buf->DrawIndexOffset(6, 3);

    //cursed draw

    //drawInitiator 0->drawindexed 2->drawauto

    uint* cmdBuffer = cmd_buf->curr_ptr;

    cmdBuffer[0] = PM4_HEADER_BUILD(6, IT_DRAW_INDEX_2);
    cmdBuffer[1] = 3; //maxSize

    ((void**)&cmdBuffer[2])[0] = (void*)((uintptr_t)ptr_vindex + 6*2); //indexAddr

    cmdBuffer[4] = 3; //indexCount
    cmdBuffer[5] = 0; //drawInitiator

    //cmdBuffer[0] = PM4_HEADER_BUILD(3, IT_DRAW_INDEX_AUTO);
    //cmdBuffer[1] = 3; //indexCount
    //cmdBuffer[2] = 2; //drawInitiator 0->drawindexed 2->drawauto

    cmd_buf->curr_ptr = cmdBuffer + 6;
    */

    auto cs_shader = new ShaderBuilder();

    cs_shader->user_count = (4 * 2); //two V# (4 byte align)
    cs_shader->sgpr_count = (4 * 2);
    cs_shader->vgpr_count = 4;

    cs_shader->MUBUF_OP(MUBUF_BUFFER_LOAD_FORMAT_XYZW,
                        MAKE_VGPR(0),    //dst
                        MAKE_VGPR(0),    //addr
                        MAKE_SGPR(0),    //V#
                        0,               //offset
                        MAKE_IMM_INT(0), //soffset
                        BUF_NONE
                       );
  
    cs_shader->S_WAITCNT(0,0,0);

    cs_shader->MUBUF_OP(MUBUF_BUFFER_STORE_FORMAT_X,
        MAKE_VGPR(0),    //src
        MAKE_VGPR(0),    //addr
        MAKE_SGPR(4),    //V#
        0,               //offset
        MAKE_IMM_INT(0), //soffset
        BUF_NONE
    );

    cs_shader->S_ENDPGM();

    //

    //two V# (4 byte align)
    void* ptr_two_vsharp_cs = linear_dmem.alloc(sizeof(VSharpResource4) * 2, 4);

    printf("ptr_two_vsharp_cs->0x%016llX\n", ptr_two_vsharp_cs);

    uint* cs_src_data = (uint*)linear_dmem.alloc(16 * 4, 4);

    uint* cs_dst_data_10_11_11 = (uint*)linear_dmem.alloc(16 * 4, 4);
    uint* cs_dst_data_11_11_10 = (uint*)linear_dmem.alloc(16 * 4, 4);

    //uint* cs_src_data_reserved = (uint*)linear_dmem.alloc(256 * 4, 4);
    //uint* cs_dst_data_reserved = (uint*)linear_dmem.alloc(256 * 4, 4);

    //for (int r = 0; r < 256; r++) {
    //    cs_src_data_reserved[r] = r;
    //    cs_dst_data_reserved[r] = 0;
    //};

    void* cs_shader_ptr = linear_dmem.alloc(cs_shader->GetByteSize(), 256);

    printf("cs_shader_ptr->0x%016llX %d\n", cs_shader_ptr, cs_shader->GetByteSize());

    auto cs_regs = cs_shader->ExportCs(cs_shader_ptr);

    //

    VSharpResource4* ptr_read_vsharp  = ((VSharpResource4*)ptr_two_vsharp_cs) + 0;
    VSharpResource4* ptr_write_vsharp = ((VSharpResource4*)ptr_two_vsharp_cs) + 1;

    ptr_read_vsharp->setVMemoryType(VMemoryTypeRO);
    ptr_read_vsharp->setVMemoryPtrs(cs_src_data, 0, sizeof(float), 1);
    ptr_read_vsharp->setChannelOrder(DSEL_R, DSEL_G, DSEL_B, DSEL_A);
    ptr_read_vsharp->nfmt = BUF_NUM_FORMAT_FLOAT;
    ptr_read_vsharp->dfmt = BUF_DATA_FORMAT_32;

    ptr_write_vsharp->setVMemoryType(VMemoryTypeSC);
    ptr_write_vsharp->setVMemoryPtrs(cs_dst_data_10_11_11, 0, sizeof(float), 1);
    ptr_write_vsharp->setChannelOrder(DSEL_R, DSEL_G, DSEL_B, DSEL_A);
    ptr_write_vsharp->nfmt = BUF_NUM_FORMAT_FLOAT;
    ptr_write_vsharp->dfmt = BUF_DATA_FORMAT_10_11_11;

//#define BUF_DATA_FORMAT_10_11_11 0x06
//#define BUF_DATA_FORMAT_11_11_10 0x07

    const uint test_10[] = {
     0x00000000,
     0x000001E0,
     0x00000292,
     0x00000000,
     0x000003FF,
     0x000003FF,
     0x000003E0,
     0x00000000,
     0x000003DF,
     0x00000000,
     0x00000000,
     0x00000173,
     0x000001A6,
     0x000001C0,
     0x000001CC,
     0x000001D9
    };

    const uint test_11[] = {
     0x00000000,
     0x000003C0,
     0x00000524,
     0x00000000,
     0x000007FF,
     0x000007FF,
     0x000007C0,
     0x00000000,
     0x000007BF,
     0x00000000,
     0x00000000,
     0x000002E6,
     0x0000034C,
     0x00000380,
     0x00000399,
     0x000003B3
    };

    ((float*)cs_src_data)[0] = 0.0;
    ((float*)cs_src_data)[1] = 1.0;
    ((float*)cs_src_data)[2] = 50.0;
    ((float*)cs_src_data)[3] = -1.0;
    (        cs_src_data)[4] = 0x7FC00000; //  NAN (0.0f / 0.0f); 0x7FC00000
    (        cs_src_data)[5] = 0xFFC00000; // -NAN (0.0f / 0.0f); 0xFFC00000
    ((float*)cs_src_data)[6] = (1.0f / 0.0f);
    ((float*)cs_src_data)[7] = (-1.0f / 0.0f);
    ((float*)cs_src_data)[8] = 5.555555555E+37;
    ((float*)cs_src_data)[9] = 5.555555555E-37;
    ((float*)cs_src_data)[10] = 5.555555555E-20;
    ((float*)cs_src_data)[11] = 0.1;
    ((float*)cs_src_data)[12] = 0.3;
    ((float*)cs_src_data)[13] = 0.5;
    ((float*)cs_src_data)[14] = 0.7;
    ((float*)cs_src_data)[15] = 0.9;

    const int cs_src_data_count = 16;

    for (int r = 0; r < cs_src_data_count; r++) {
        cs_dst_data_10_11_11[r] = 0;
        cs_dst_data_11_11_10[r] = 0;
    };

    cmd_buf->SetCsShader(&cs_regs);

    //10_11_11 test

    ptr_read_vsharp->nfmt = BUF_NUM_FORMAT_FLOAT;
    ptr_read_vsharp->dfmt = BUF_DATA_FORMAT_32;

    ptr_write_vsharp->nfmt = BUF_NUM_FORMAT_FLOAT;
    ptr_write_vsharp->dfmt = BUF_DATA_FORMAT_10_11_11;

    for (int r = 0; r < cs_src_data_count; r++) {
        ptr_read_vsharp ->setVMemoryPtrs(&cs_src_data[r], 0, sizeof(float), 1);
        ptr_write_vsharp->setVMemoryPtrs(&cs_dst_data_10_11_11[r], 0, sizeof(float), 1);
        cmd_buf->SetUserDataCsRegs(0, 4 * 2, ptr_two_vsharp_cs);
        cmd_buf->DispatchDirect(1, 1, 1);
    };

    //11_11_10 test

    ptr_read_vsharp->nfmt = BUF_NUM_FORMAT_FLOAT;
    ptr_read_vsharp->dfmt = BUF_DATA_FORMAT_32;

    ptr_write_vsharp->nfmt = BUF_NUM_FORMAT_FLOAT;
    ptr_write_vsharp->dfmt = BUF_DATA_FORMAT_11_11_10;

    for (int r = 0; r < cs_src_data_count; r++) {
        ptr_read_vsharp->setVMemoryPtrs(&cs_src_data[r], 0, sizeof(float), 1);
        ptr_write_vsharp->setVMemoryPtrs(&cs_dst_data_11_11_10[r], 0, sizeof(float), 1);
        cmd_buf->SetUserDataCsRegs(0, 4 * 2, ptr_two_vsharp_cs);
        cmd_buf->DispatchDirect(1, 1, 1);
    };

    //undef test

    //ptr_read_vsharp->nfmt = BUF_NUM_FORMAT_FLOAT;
    //ptr_read_vsharp->dfmt = BUF_DATA_FORMAT_RESERVED;

    //ptr_write_vsharp->nfmt = BUF_NUM_FORMAT_FLOAT;
    //ptr_write_vsharp->dfmt = BUF_DATA_FORMAT_32;

    //for (int r = 0; r < 256; r++) {

    //    ptr_read_vsharp ->setVMemoryPtrs(&cs_src_data_reserved[r], 0, sizeof(float), 1);
    //    ptr_write_vsharp->setVMemoryPtrs(&cs_dst_data_reserved[r], 0, sizeof(float), 1);
    //    cmd_buf->SetUserDataCsRegs(0, 4 * 2, ptr_two_vsharp_cs);
    //    cmd_buf->DispatchDirect(1, 1, 1);
    //};

    //V_SAD_U32 v4, v0, v1, abs(v2)
    //V_SAD_U32 v5, v0, v1, -v3
    //V_SAD_U32 v6, v0, v1, -abs(v2)

#if 0  // IMAGE_STORE_MIP sweeps off: not needed for the mask test and
       // they emit GCN3+/PS4-Pro opcodes (F16 VOP2 50/51, V_ADD_NC_U16
       // 771, V_ADD_NC_I16 781, V_ADD3_U32 877, V_SUB_CO_U32 784) that
       // base PS4 (GCN2/Sea Islands) and shadPS4 do not implement.
    make_cs_buf_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            //absdiff(src0,src1) + src2

            shader->VOP3a_OP(VOP3_SAD_U32,
                MAKE_VGPR(4),
                MAKE_VGPR(0), MAKE_VGPR(1), MAKE_VGPR(2) | V3F_ABS
            );

            shader->VOP3a_OP(VOP3_SAD_U32,
                MAKE_VGPR(5),
                MAKE_VGPR(0), MAKE_VGPR(1), MAKE_VGPR(3) | V3F_NEG
            );

            shader->VOP3a_OP(VOP3_SAD_U32,
                MAKE_VGPR(6),
                MAKE_VGPR(0), MAKE_VGPR(1), MAKE_VGPR(2) | V3F_ABS | V3F_NEG 
            );


        },
        [](t_cs_shader_test* test) {

            //absdiff(src0,src1) + src2
            test->src[0] = 1;
            test->src[1] = 1;
            test->src[2] = 0x80FFFFFF;
            test->src[3] = 0x00FFFFFF;

            test->dst[0] = 0;
            test->dst[1] = 0;
            test->dst[2] = 0;
            //

        },
        [](t_cs_shader_test* test) {
            printf("V_SAD_U32 v4, v0, v1, abs(v2)  = 0x%08llX %s\n", test->dst[0], PASS[test->dst[0] == 0x00FFFFFF]);
            printf("V_SAD_U32 v5, v0, v1, -v3      = 0x%08llX %s\n", test->dst[1], PASS[test->dst[1] == 0x80FFFFFF]);
            printf("V_SAD_U32 v6, v0, v1, -abs(v2) = 0x%08llX %s\n", test->dst[2], PASS[test->dst[2] == 0x80FFFFFF]);
        }
    );

    //V_MUL_I32_I24 v4, v0, v1
    //V_MAD_I32_I24 v5, v0, v1, v2
    //V_MAD_I32_I24 v6, v0, abs(v1), v2
    //V_MAD_I32_I24 v7, v0, v1, -v3
   
    make_cs_buf_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            //(vsrc0[23:0].s * vsrc1[23:0].s) 
            shader->VOP3a_OP(VOP3_MUL_I32_I24,
                MAKE_VGPR(4),
                MAKE_VGPR(0), MAKE_VGPR(1), 0 
                );

            //vsrc0[23:0].i * vsrc1[23:0].i + vsrc2.i  
            shader->VOP3a_OP(VOP3_MAD_I32_I24,
                MAKE_VGPR(5),
                MAKE_VGPR(0), MAKE_VGPR(1), MAKE_VGPR(2)
            );

            //vsrc0[23:0].i * vsrc1[23:0].i + vsrc2.i 
            shader->VOP3a_OP(VOP3_MAD_I32_I24,
                MAKE_VGPR(6),
                MAKE_VGPR(0), MAKE_VGPR(1) | V3F_ABS, MAKE_VGPR(2)
            );

            //vsrc0[23:0].i * vsrc1[23:0].i + vsrc2.i 
            shader->VOP3a_OP(VOP3_MAD_I32_I24,
                MAKE_VGPR(7),
                MAKE_VGPR(0), MAKE_VGPR(1), MAKE_VGPR(3) | V3F_NEG
            );


        },
        [](t_cs_shader_test* test) {

             
            test->src[0] = 1;
            test->src[1] = 0x01800000;
            test->src[2] = 0;
            test->src[3] = 0x80000000;

            test->dst[0] = 0;
            test->dst[1] = 0;
            test->dst[2] = 0;
            test->dst[3] = 0;
            //


        },
        [](t_cs_shader_test* test) {
            printf("V_MUL_I32_I24 v4, v0, v1          = 0x%08llX %s\n", test->dst[0], PASS[test->dst[0] == 0xFF800000]);
            printf("V_MAD_I32_I24 v4, v0, v1, v2      = 0x%08llX %s\n", test->dst[1], PASS[test->dst[1] == 0xFF800000]);
            printf("V_MAD_I32_I24 v5, v0, abs(v1), v2 = 0x%08llX %s\n", test->dst[2], PASS[test->dst[2] == 0xFF800000]);
            printf("V_MAD_I32_I24 v7, v0, v1, -v3     = 0x%08llX %s\n", test->dst[3], PASS[test->dst[3] == 0xFF800000]);
        }
    );

    //V_CMP_EQ_I32 s[0:1], v0, v1
    //V_MOV_B32 v4 ,s0
    
    //V_CMP_EQ_I32 s[0:1], v2, abs(v3)
    //V_MOV_B32 v5 ,s0

    make_cs_buf_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            shader->VOP3c_OP(VOP3_CMP_EQ_I32,
                MAKE_SGPR(0), //s[0:1]
                MAKE_VGPR(0), MAKE_VGPR(1)
                );
            shader->V_MOV_B32(MAKE_VGPR(4), MAKE_SGPR(0));

            shader->VOP3c_OP(VOP3_CMP_EQ_I32,
                MAKE_SGPR(0), //s[0:1]
                MAKE_VGPR(2), MAKE_VGPR(3) | V3F_ABS
                );
            shader->V_MOV_B32(MAKE_VGPR(5), MAKE_SGPR(0));
        },
        [](t_cs_shader_test* test) {

            //vsrc0 == vsrc1 
            test->src[0] = 0x80000000;
            test->src[1] = 0x80000000;
            test->src[2] = 0;
            test->src[3] = 0x80000000;

            test->dst[0] = 0;
            test->dst[1] = 0;
            //

        },
        [](t_cs_shader_test* test) {
            printf("V_CMP_EQ_I32 s[0:1], v0, v1      = 0x%08llX %s\n", test->dst[0], PASS[test->dst[0] == 0x1]);
            printf("V_CMP_EQ_I32 s[0:1], v2, abs(v3) = 0x%08llX %s\n", test->dst[1], PASS[test->dst[1] == 0x1]);
        }
    );

    //vdst = smask[thread_id:] ? vsrc1 : vsrc0  
    //V_CNDMASK_B32 v4, v0, v1,       EXEC
    //V_CNDMASK_B32 v5, v0, abs(v2),  EXEC 
    //V_CNDMASK_B32 v6, v0, -v2,      EXEC 
    //V_CNDMASK_B32 v7, v0, -abs(v3), EXEC 

    make_cs_buf_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            //vdst = smask[thread_id:] ? vsrc1 : vsrc0 
            shader->VOP3a_OP(VOP3_CNDMASK_B32,
                MAKE_VGPR(4),
                MAKE_VGPR(0), MAKE_VGPR(1), EXEC_LO
                );

            shader->VOP3a_OP(VOP3_CNDMASK_B32,
                MAKE_VGPR(5),
                MAKE_VGPR(0), MAKE_VGPR(2) | V3F_ABS, EXEC_LO
            );

            shader->VOP3a_OP(VOP3_CNDMASK_B32,
                MAKE_VGPR(6),
                MAKE_VGPR(0), MAKE_VGPR(2) | V3F_NEG, EXEC_LO
            );

            shader->VOP3a_OP(VOP3_CNDMASK_B32,
                MAKE_VGPR(7),
                MAKE_VGPR(0), MAKE_VGPR(3) | V3F_ABS | V3F_NEG, EXEC_LO
            );


        },
        [](t_cs_shader_test* test) {


            test->src[0] = 1;
            test->src[1] = 2;
            test->src[2] = 0x80000003;
            test->src[3] = 0x80000004;

            test->dst[0] = 0;
            test->dst[1] = 0;
            test->dst[2] = 0;
            test->dst[3] = 0;
            //


        },
        [](t_cs_shader_test* test) {
            printf("V_CNDMASK_B32 v4, v0, v1,       EXEC = 0x%08llX %s\n", test->dst[0], PASS[test->dst[0] == 0x00000002]);
            printf("V_CNDMASK_B32 v5, v0, abs(v2),  EXEC = 0x%08llX %s\n", test->dst[1], PASS[test->dst[1] == 0x00000003]);
            printf("V_CNDMASK_B32 v6, v0, -v2,      EXEC = 0x%08llX %s\n", test->dst[2], PASS[test->dst[2] == 0x00000003]);
            printf("V_CNDMASK_B32 v7, v0, -abs(v3), EXEC = 0x%08llX %s\n", test->dst[3], PASS[test->dst[3] == 0x80000004]);
        }
    );

    //V_MAD_U64_U32 v[4:5], v0, v0, v[2:3]
    //V_MAD_U64_U32 v[6:7], v0, v1, 0

    make_cs_buf_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            shader->sgpr_count = (4 * 2) + 4;

            shader->S_MOV_B64(MAKE_SGPR(8), EXEC_LO);

            shader->S_MOV_B32(MAKE_SGPR(0), MAKE_IMM_INT(1));
            //
            //shader->V_MOV_B32(MAKE_VGPR(0), MAKE_IMM_INT(1));
            //shader->V_MOV_B32(MAKE_VGPR(4), MAKE_IMM_INT(4));
            //shader->V_MOV_B32(MAKE_VGPR(5), MAKE_IMM_INT(5));

            shader->S_WAITCNT(0, 0, 0);

            //vdst.du = vsrc0.u * vsrc1.u + vsrc2.du; VCC = carry & EXEC
            shader->VOP3b_OP(VOP3_MAD_U64_U32,
                MAKE_VGPR(4), VCC_LO,
                MAKE_VGPR(0) | V3F_NEG, MAKE_VGPR(1), MAKE_IMM_INT(0)
                );

            shader->S_WAITCNT(0, 0, 0);
   
            //vdst.du = vsrc0.u * vsrc1.u + vsrc2.du; VCC = carry & EXEC
            shader->VOP3b_OP(VOP3_MAD_U64_U32,
                MAKE_VGPR(6), VCC_LO,
                MAKE_VGPR(0) | V3F_NEG, MAKE_VGPR(1), MAKE_VGPR(2) | V3F_NEG
            );


           shader->S_WAITCNT(0, 0, 0);
           //shader->S_NOP();
           //shader->S_NOP();
           //shader->S_NOP();
           //shader->S_NOP();

           //shader->S_MOV_B32(VCC_LO, MAKE_IMM_INT(1));

           shader->S_MOV_B64(MAKE_SGPR(10), EXEC_LO);

           shader->S_MOV_B64(EXEC_LO, MAKE_SGPR(8));

           //shader->V_MOV_B32(MAKE_VGPR(4), MAKE_VGPR(0));
           //shader->V_MOV_B32(MAKE_VGPR(5), MAKE_VGPR(1));

           //shader->V_MOV_B32(MAKE_VGPR(6), MAKE_SGPR(10));
           //shader->V_MOV_B32(MAKE_VGPR(7), MAKE_SGPR(11));

           //shader->V_MOV_B32(MAKE_VGPR(6), MAKE_IMM_INT(6));

        },
        [](t_cs_shader_test* test) {


            test->src[0] = 0x80000001;

            test->src[1] = 1;

            test->src[2] = 0;
            test->src[3] = 0;

            test->dst[0] = 0;
            test->dst[1] = 0;
            test->dst[2] = 0;
            test->dst[3] = 0;
            //


        },
        [](t_cs_shader_test* test) {
            printf("V_MAD_U64_U32 v[4:5], vcc, -v0, v1, 0       = 0x%08llX %s\n", test->dst[0], PASS[test->dst[0] == 0x00000001]);
            printf("                                              0x%08llX %s\n", test->dst[1], PASS[test->dst[1] == 0x00000000]);
            printf("V_MAD_U64_U32 v[6:7], vcc, -v0, v1, -v[2:3] = 0x%08llX %s\n", test->dst[2], PASS[test->dst[2] == 0x00000001]);
            printf("                                              0x%08llX %s\n", test->dst[3], PASS[test->dst[3] == 0x80000000]);
        }
    );

    //V_CVT_

    make_cs_buf_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            //uint->float
            shader->VOP3a_OP(VOP3_CVT_F32_U32,
                MAKE_VGPR(4) | V3F_CLAMP,
                MAKE_VGPR(1), 0, 0
                );

            shader->VOP3a_OP(VOP3_CVT_F32_U32,
                MAKE_VGPR(5) | V3F_MUL2,
                MAKE_VGPR(0), 0, 0
            );

            shader->VOP3a_OP(VOP3_CVT_F32_U32,
                MAKE_VGPR(6),
                MAKE_VGPR(2) | V3F_ABS, 0, 0
            );

            shader->VOP3a_OP(VOP3_CVT_F32_U32,
                MAKE_VGPR(7),
                MAKE_VGPR(3), 0, 0
            );

            //float->uint
            shader->VOP3a_OP(VOP3_CVT_U32_F32,
                MAKE_VGPR(7) | V3F_CLAMP | V3F_MUL2,
                MAKE_VGPR(7), 0, 0
            );


        },
        [](t_cs_shader_test* test) {


            test->src[0] = 1;
            test->src[1] = 2;
            test->src[2] = 0x80000003;
            test->src[3] = 4;

            test->dst[0] = 0;
            test->dst[1] = 0;
            test->dst[2] = 0;
            test->dst[3] = 0;
            //


        },
        [](t_cs_shader_test* test) {
            printf("VOP3_CVT_0 = 0x%08llX %s\n", test->dst[0], PASS[test->dst[0] == 0x3F800000]);
            printf("VOP3_CVT_1 = 0x%08llX %s\n", test->dst[1], PASS[test->dst[1] == 0x40000000]);
            printf("VOP3_CVT_2 = 0x%08llX %s\n", test->dst[2], PASS[test->dst[2] == 0x40400000]);
            printf("VOP3_CVT_3 = 0x%08llX %s\n", test->dst[3], PASS[test->dst[3] == 0x00000004]);
        }
    );

    make_cs_buf_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            shader->VOP2_OP(51, // sub_f16
                MAKE_VGPR(4),
                MAKE_VGPR(0), MAKE_VGPR(1)
                );

            shader->VOP2_OP(50, // add_f16
                MAKE_VGPR(5),
                MAKE_VGPR(1), MAKE_VGPR(1)
            );

            shader->VOP3a_OP(306,
                MAKE_VGPR(6) | V3F_CLAMP,
                MAKE_VGPR(1), MAKE_VGPR(1), 0
            );

            shader->VOP3a_OP(306,
                MAKE_VGPR(7),
                MAKE_VGPR(1) | V3F_NEG, MAKE_VGPR(1) | V3F_NEG, 0
            );


        },
        [](t_cs_shader_test* test) {


            test->src[0] = 0;
            test->src[1] = 0x3c00; // 1.0
            test->src[2] = 0;
            test->src[3] = 0;

            test->dst[0] = 0;
            test->dst[1] = 0;
            test->dst[2] = 0;
            test->dst[3] = 0;
            //


        },
        [](t_cs_shader_test* test) {
            printf("V_SUB_F16 v4, 0.0, 1.0 = 0x%08llX %s\n", test->dst[0], PASS[test->dst[0] == 0xbc00]);
            printf("V_ADD_F16 v5, 1.0, 1.0 = 0x%08llX %s\n", test->dst[1], PASS[test->dst[1] == 0x4000]);
            printf("V_ADD_F16 v6, 1.0, 1.0 clamp = 0x%08llX %s\n", test->dst[2], PASS[test->dst[2] == 0x3c00]);
            printf("V_ADD_F16 v7, -(1.0), -(1.0) = 0x%08llX %s\n", test->dst[3], PASS[test->dst[3] == 0xc000]);
        }
    );

    make_cs_buf_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            shader->V_MOV_B32(MAKE_VGPR(4), MAKE_IMM_INT(0));
            shader->VOP3a_OP(771, // V_ADD_NC_U16
                MAKE_VGPR(4),
                MAKE_VGPR(0), MAKE_VGPR(1), 0
            );

            shader->V_MOV_B32(MAKE_VGPR(5), MAKE_IMM_INT(0));
            shader->VOP3a_OP(771, // V_ADD_NC_U16
                MAKE_VGPR(5) | V3F_CLAMP,
                MAKE_VGPR(0), MAKE_VGPR(1), 0
            );

            shader->V_MOV_B32(MAKE_VGPR(6), MAKE_IMM_INT(0));
            shader->VOP3a_OP(781, // V_ADD_NC_I16
                MAKE_VGPR(6),
                MAKE_VGPR(0), MAKE_VGPR(2), 0
            );

            shader->V_MOV_B32(MAKE_VGPR(7), MAKE_IMM_INT(0));
            shader->VOP3a_OP(781, // V_ADD_NC_I16
                MAKE_VGPR(7) | V3F_CLAMP,
                MAKE_VGPR(2), MAKE_VGPR(2), 0
            );

        },
        [](t_cs_shader_test* test) {

            test->src[0] = 1;
            test->src[1] = 2;
            test->src[2] = 0xFFFF;
            test->src[3] = 0;

            test->dst[0] = 0;
            test->dst[1] = 0;
            test->dst[2] = 0;
            test->dst[3] = 0;
            //

        },
        [](t_cs_shader_test* test) {
            printf("V_ADD_NC_U16 v4, #1, #2 = 0x%08llX %s\n", test->dst[0], PASS[test->dst[0] == 3]);
            printf("V_ADD_NC_U16 v5, #1, #2 clmp = 0x%08llX %s\n", test->dst[1], PASS[test->dst[1] == 3]);
            printf("V_ADD_NC_I16 v6, #1, #-1 = 0x%08llX %s\n", test->dst[2], PASS[test->dst[2] == 0]);
            printf("V_ADD_NC_I16 v7, #-1, #-1 clmp = 0x%08llX %s\n", test->dst[3], PASS[test->dst[3] == 0x0000FFFE]);
        }
    );

    make_cs_buf_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            shader->VOP3a_OP(877, // V_ADD3_U32
                MAKE_VGPR(4),
                MAKE_VGPR(0), MAKE_VGPR(1), MAKE_VGPR(1)
            );

            shader->VOP3a_OP(877, // V_ADD3_U32
                MAKE_VGPR(5),
                MAKE_VGPR(2), MAKE_VGPR(2), MAKE_VGPR(2)
            );

            shader->VOP3a_OP(877, // V_ADD3_U32
                MAKE_VGPR(6) | V3F_CLAMP,
                MAKE_VGPR(2), MAKE_VGPR(2), MAKE_VGPR(2)
            );

            shader->VOP3a_OP(877, // V_ADD3_U32
                MAKE_VGPR(7) | V3F_CLAMP,
                MAKE_VGPR(1) | V3F_NEG, MAKE_VGPR(1), MAKE_VGPR(1)
            );

        },
        [](t_cs_shader_test* test) {

            test->src[0] = 1;
            test->src[1] = 2;
            test->src[2] = 2000000000;
            test->src[3] = 0;

            test->dst[0] = 0;
            test->dst[1] = 0;
            test->dst[2] = 0;
            test->dst[3] = 0;
            //

        },
        [](t_cs_shader_test* test) {
            printf("V_ADD3_U32 v4, #1, #2, #2                                 = 0x%08llX %s\n", test->dst[0], PASS[test->dst[0] == 0x00000005]);
            printf("V_ADD3_U32 v5, #2000000000, #2000000000, #2000000000      = 0x%08llX %s\n", test->dst[1], PASS[test->dst[1] == 0x65A0BC00]);
            printf("V_ADD3_U32 v6, #2000000000, #2000000000, #2000000000 clmp = 0x%08llX %s\n", test->dst[2], PASS[test->dst[2] == 0x65A0BC00]);
            printf("V_ADD3_U32 v7, neg(#2), #2, #2 clmp                       = 0x%08llX %s\n", test->dst[3], PASS[test->dst[3] == 0x80000006]);
        }
    );

    make_cs_buf_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            shader->VOP3a_OP(784, // V_SUB_U32
                MAKE_VGPR(4),
                MAKE_VGPR(0), MAKE_VGPR(1), 0
            );

            shader->VOP3a_OP(784, // V_SUB_U32
                MAKE_VGPR(5),
                MAKE_VGPR(3), MAKE_VGPR(2), 0
            );

            shader->VOP3a_OP(784, // V_SUB_U32
                MAKE_VGPR(6) | V3F_CLAMP,
                MAKE_VGPR(2), MAKE_VGPR(3), 0
            );

            shader->VOP3a_OP(784, // V_SUB_U32
                MAKE_VGPR(7) | V3F_CLAMP,
                MAKE_VGPR(1) | V3F_NEG, MAKE_VGPR(0), 0
            );

        },
        [](t_cs_shader_test* test) {

            test->src[0] = 1;
            test->src[1] = 2;
            test->src[2] = 2000000000;
            test->src[3] = 4000000000;

            test->dst[0] = 0;
            test->dst[1] = 0;
            test->dst[2] = 0;
            test->dst[3] = 0;
            //

        },
        [](t_cs_shader_test* test) {
            printf("V_SUB_U32 v4, #1, #2                        = 0x%08llX %s\n", test->dst[0], PASS[test->dst[0] == 0xFFFFFFFF]);
            printf("V_SUB_U32 v5, #4000000000, #2000000000      = 0x%08llX %s\n", test->dst[1], PASS[test->dst[1] == 0x77359400]);
            printf("V_SUB_U32 v6, #2000000000, #4000000000 clmp = 0x%08llX %s\n", test->dst[2], PASS[test->dst[2] == 0x00000000]);
            printf("V_SUB_U32 v7, neg(#2), #2 clmp              = 0x%08llX %s\n", test->dst[3], PASS[test->dst[3] == 0x80000001]);
        }
    );

    make_cs_buf_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            shader->V_MOV_B32(MAKE_VGPR(4), MAKE_IMM_INT(0));
            shader->VOP3a_OP(771, // V_ADD_NC_U16
                MAKE_VGPR(4),
                MAKE_VGPR(0), MAKE_VGPR(1), 0
            );

            shader->V_MOV_B32(MAKE_VGPR(5), MAKE_IMM_INT(0));
            shader->VOP3a_OP(771, // V_ADD_NC_U16
                MAKE_VGPR(5) | V3F_CLAMP,
                MAKE_VGPR(0), MAKE_VGPR(1), 0
            );

            shader->V_MOV_B32(MAKE_VGPR(6), MAKE_IMM_INT(0));
            shader->VOP3a_OP(781, // V_ADD_NC_I16
                MAKE_VGPR(6),
                MAKE_VGPR(2), MAKE_VGPR(1), 0
            );

            shader->V_MOV_B32(MAKE_VGPR(7), MAKE_IMM_INT(0));
            shader->VOP3a_OP(781, // V_ADD_NC_I16
                MAKE_VGPR(7) | V3F_CLAMP,
                MAKE_VGPR(2), MAKE_VGPR(1), 0
            );

        },
        [](t_cs_shader_test* test) {

            test->src[0] = 65000;
            test->src[1] = 4096;
            test->src[2] = 32000;
            test->src[3] = 0;

            test->dst[0] = 0;
            test->dst[1] = 0;
            test->dst[2] = 0;
            test->dst[3] = 0;
            //

        },
        [](t_cs_shader_test* test) {
            printf("V_ADD_NC_U16 v4, #65000, #4096      = 0x%08llX %s\n", test->dst[0], PASS[test->dst[0] == 0x00000DE8]);
            printf("V_ADD_NC_U16 v5, #65000, #4096 clmp = 0x%08llX %s\n", test->dst[1], PASS[test->dst[1] == 0x0000FFFF]);
            printf("V_ADD_NC_I16 v6, #32000, #4096      = 0x%08llX %s\n", test->dst[2], PASS[test->dst[2] == 0x00008D00]);
            printf("V_ADD_NC_I16 v7, #32000, #4096 clmp = 0x%08llX %s\n", test->dst[3], PASS[test->dst[3] == 0x00007FFF]);
        }
    );

    //V_MUL_LEGACY_F32 v4, v0, v1
    //V_MUL_F32        v5, v0, v1
    //V_MIN_LEGACY_F32 v6, v0, v1
    //V_MIN_F32        v7, v0, v1

    make_cs_buf_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            shader->VOP2_OP(VOP2_MUL_LEGACY_F32,
                MAKE_VGPR(4),
                MAKE_VGPR(0), MAKE_VGPR(1)
            );

            shader->VOP2_OP(VOP2_MUL_F32,
                MAKE_VGPR(5),
                MAKE_VGPR(0), MAKE_VGPR(1)
            );

            shader->VOP2_OP(VOP2_MIN_LEGACY_F32,
                MAKE_VGPR(6),
                MAKE_VGPR(0), MAKE_VGPR(1)
            );

            shader->VOP2_OP(VOP2_MIN_F32,
                MAKE_VGPR(7),
                MAKE_VGPR(0), MAKE_VGPR(1)
            );

            shader->VOP2_OP(VOP2_MIN_LEGACY_F32,
                MAKE_VGPR(8),
                MAKE_VGPR(1), MAKE_VGPR(0)
            );

        },
        [](t_cs_shader_test* test) {

            test->src[0] = 0;
            test->src[1] = 0x7FC00000; //NAN

            test->dst[0] = 0;
            test->dst[1] = 0;
            test->dst[2] = 0;
            test->dst[3] = 0;
            test->dst[4] = 0;
            //

        },
        [](t_cs_shader_test* test) {
            printf("V_MUL_LEGACY_F32 v4, v0, v1 = 0x%08llX %s\n", test->dst[0], PASS[test->dst[0] == 0x00000000]);
            printf("V_MUL_F32        v5, v0, v1 = 0x%08llX %s\n", test->dst[1], PASS[test->dst[1] == 0x7FC00000]);
            printf("V_MIN_LEGACY_F32 v6, v0, v1 = 0x%08llX %s\n", test->dst[2], PASS[test->dst[2] == 0x7FC00000]);
            printf("V_MIN_F32        v7, v0, v1 = 0x%08llX %s\n", test->dst[3], PASS[test->dst[3] == 0x00000000]);
            printf("V_MIN_LEGACY_F32 v8, v1, v0 = 0x%08llX %s\n", test->dst[4], PASS[test->dst[4] == 0x00000000]);
        }
    );

    make_cs_buf_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            shader->VOP2_OP(VOP2_MAX_LEGACY_F32,
                MAKE_VGPR(4),
                MAKE_VGPR(0), MAKE_VGPR(1)
            );

            shader->VOP2_OP(VOP2_MAX_F32,
                MAKE_VGPR(5),
                MAKE_VGPR(0), MAKE_VGPR(1)
            );

            shader->VOP2_OP(VOP2_MAX_LEGACY_F32,
                MAKE_VGPR(6),
                MAKE_VGPR(1), MAKE_VGPR(0)
            );

            shader->VOP2_OP(VOP2_MAX_LEGACY_F32,
                MAKE_VGPR(7),
                MAKE_VGPR(1), MAKE_VGPR(2)
            );

        },
        [](t_cs_shader_test* test) {

            test->src[0] = 0;
            test->src[1] = 0x7FC00000; //NAN
            test->src[2] = 0x7FFFFFFF; //another NAN

            test->dst[0] = 0;
            test->dst[1] = 0;
            test->dst[2] = 0;
            test->dst[3] = 0;
            test->dst[4] = 0;
            //

        },
        [](t_cs_shader_test* test) {
            printf("V_MAX_LEGACY_F32 v4, v0, v1 = 0x%08llX %s\n", test->dst[0], PASS[test->dst[0] == 0x7FC00000]);
            printf("V_MAX_F32        v5, v0, v1 = 0x%08llX %s\n", test->dst[1], PASS[test->dst[1] == 0x00000000]);
            printf("V_MAX_LEGACY_F32 v6, v1, v0 = 0x%08llX %s\n", test->dst[2], PASS[test->dst[2] == 0x00000000]);
            printf("V_MAX_LEGACY_F32 v7, v1, v2 = 0x%08llX %s\n", test->dst[3], PASS[test->dst[3] == 0x7FFFFFFF]);
        }
    );

    //img

    TSharpResource8 img_sharp = {};

    t_dmem_pages img_data = {};
    img_data.Alloc(128 * 1024, WC_GARLIC);

    printf("img_data = 0x%08llX .. 0x%08llX\n", (ulong)img_data.dmem_ptr, (ulong)img_data.dmem_ptr + 128 * 1024);

    //kTileModeThin_2dThin = $0000000E;
    img_sharp.Init2D(IMG_DATA_FORMAT_32_32_32_32, IMG_NUM_FORMAT_UINT, 0xE, 8, 8, 4);
    img_sharp.setVMemoryPtr(img_data.dmem_ptr);

//write IMAGE_STORE

    make_cs_custom_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            shader->user_count = (4 * 2); //T# + V# 
            shader->sgpr_count = 8;
            shader->vgpr_count = 6;

            //color
            shader->V_MOV_B32(MAKE_VGPR(0), MAKE_SGPR(4));
            shader->V_MOV_B32(MAKE_VGPR(1), MAKE_SGPR(5));
            shader->V_MOV_B32(MAKE_VGPR(2), MAKE_SGPR(6));
            shader->V_MOV_B32(MAKE_VGPR(3), MAKE_SGPR(7));

            //pos
            shader->V_MOV_B32(MAKE_VGPR(4), MAKE_IMM_INT(0));
            shader->V_MOV_B32(MAKE_VGPR(5), MAKE_IMM_INT(0));

            //store
            shader->MIMG_OP(MIMG_IMAGE_STORE,
                MAKE_VGPR(0), //color [0..3]
                MAKE_VGPR(4), //pos
                MAKE_SGPR(0), //T# 0..3 (R128)
                MAKE_SGPR(0), //S# (ignored)
                0b1111,       //color selector
                MIMG_R128 | MIMG_GLC
            );

        },
        [&img_sharp](t_cs_shader_test* test) {

            test->cmd_buf->SetCsShader(&test->regs);

            img_sharp.base_level = 0;
            test->cmd_buf->SetUserDataCsRegs(0, 4, (void*)&img_sharp);
  
            test->cmd_buf->SetUserDataCs(4, 1);
            test->cmd_buf->SetUserDataCs(5, 2);
            test->cmd_buf->SetUserDataCs(6, 3);
            test->cmd_buf->SetUserDataCs(7, 4);

            test->cmd_buf->DispatchDirect(1, 1, 1);

            img_sharp.base_level = 1;
            test->cmd_buf->SetUserDataCsRegs(0, 4, (void*)&img_sharp);

            test->cmd_buf->SetUserDataCs(4, 5);
            test->cmd_buf->SetUserDataCs(5, 6);
            test->cmd_buf->SetUserDataCs(6, 7);
            test->cmd_buf->SetUserDataCs(7, 8);

            test->cmd_buf->DispatchDirect(1, 1, 1);
        },
        [](t_cs_shader_test* test) {
            //
        }
    );

//write IMAGE_STORE_MIP

    make_cs_custom_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            shader->user_count = (4 * 2); //T# + V# 
            shader->sgpr_count = 8;
            shader->vgpr_count = 7;

            //color
            shader->V_MOV_B32(MAKE_VGPR(0), MAKE_SGPR(4));
            shader->V_MOV_B32(MAKE_VGPR(1), MAKE_SGPR(5));
            shader->V_MOV_B32(MAKE_VGPR(2), MAKE_SGPR(6));
            shader->V_MOV_B32(MAKE_VGPR(3), MAKE_SGPR(7));

            //pos
            shader->V_MOV_B32(MAKE_VGPR(4), MAKE_IMM_INT(0));
            shader->V_MOV_B32(MAKE_VGPR(5), MAKE_IMM_INT(0));
            shader->V_MOV_B32(MAKE_VGPR(6), MAKE_IMM_INT(1)); //INDEX

            //store
            shader->MIMG_OP(MIMG_IMAGE_STORE_MIP,
                MAKE_VGPR(0), //color [0..3]
                MAKE_VGPR(4), //pos
                MAKE_SGPR(0), //T# 0..3 (R128)
                MAKE_SGPR(0), //S# (ignored)
                0b1111,       //color selector
                MIMG_R128 | MIMG_GLC
            );

        },
        [&img_sharp](t_cs_shader_test* test) {

            test->cmd_buf->SetCsShader(&test->regs);

            img_sharp.base_level = 2 - 1;
            test->cmd_buf->SetUserDataCsRegs(0, 4, (void*)&img_sharp);
  
            test->cmd_buf->SetUserDataCs(4, 9);
            test->cmd_buf->SetUserDataCs(5, 10);
            test->cmd_buf->SetUserDataCs(6, 11);
            test->cmd_buf->SetUserDataCs(7, 12);

            test->cmd_buf->DispatchDirect(1, 1, 1);

            img_sharp.base_level = 3 - 1;
            test->cmd_buf->SetUserDataCsRegs(0, 4, (void*)&img_sharp);

            test->cmd_buf->SetUserDataCs(4, 13);
            test->cmd_buf->SetUserDataCs(5, 14);
            test->cmd_buf->SetUserDataCs(6, 15);
            test->cmd_buf->SetUserDataCs(7, 16);

            test->cmd_buf->DispatchDirect(1, 1, 1);
        },
        [](t_cs_shader_test* test) {
            //
        }
    );
  
//read

    make_cs_custom_test(&linear_dmem, cmd_buf,

        [](ShaderBuilder* shader) {

            shader->user_count = (4 * 2); //T# + V# 
            shader->sgpr_count = 8;
            shader->vgpr_count = 6;

            //color init
            shader->V_MOV_B32(MAKE_VGPR(0), MAKE_IMM_INT(0));
            shader->V_MOV_B32(MAKE_VGPR(1), MAKE_IMM_INT(0));
            shader->V_MOV_B32(MAKE_VGPR(2), MAKE_IMM_INT(0));
            shader->V_MOV_B32(MAKE_VGPR(3), MAKE_IMM_INT(0));

            //pos
            shader->V_MOV_B32(MAKE_VGPR(4), MAKE_IMM_INT(0));
            shader->V_MOV_B32(MAKE_VGPR(5), MAKE_IMM_INT(0));

            //load
            shader->MIMG_OP(MIMG_IMAGE_LOAD,
                            MAKE_VGPR(0), //color [0..3]
                            MAKE_VGPR(4), //pos
                            MAKE_SGPR(0), //T# 0..3 (R128)
                            MAKE_SGPR(0), //S# (ignored)
                            0b1111,       //color selector
                            MIMG_R128 | MIMG_GLC
            );

            shader->S_WAITCNT(0, 0, 0);

            //save
            shader->MUBUF_OP(MUBUF_BUFFER_STORE_FORMAT_XYZW,
                MAKE_VGPR(0),    //src
                MAKE_VGPR(4),    //addr
                MAKE_SGPR(4),    //V#
                0,               //offset
                MAKE_IMM_INT(0), //soffset
                BUF_OFFEN | BUF_IDXEN
            );


        },
        [&img_sharp, &linear_dmem](t_cs_shader_test* test) {

            test->dst = (uint*)linear_dmem.alloc(16 * 4, 4);

            VSharpResource4 dst_vsharp = {};
            dst_vsharp.setVMemoryType(VMemoryTypeSC);
            dst_vsharp.setVMemoryPtrs(test->dst, 0, sizeof(uint) * 4, 1);
            dst_vsharp.setChannelOrder(DSEL_R, DSEL_G, DSEL_B, DSEL_A);
            dst_vsharp.nfmt = BUF_NUM_FORMAT_UINT;
            dst_vsharp.dfmt = BUF_DATA_FORMAT_32_32_32_32;

            //

            test->cmd_buf->SetCsShader(&test->regs);

            for (int r = 0; r < 4; r++) {

                dst_vsharp.setVMemoryPtrs(test->dst, r * 4 * 4, sizeof(uint) * 4, 1);
                img_sharp.base_level = r;

                test->cmd_buf->SetUserDataCsRegs(0, 4, (void*)&img_sharp);
                test->cmd_buf->SetUserDataCsRegs(4, 4, (void*)&dst_vsharp);

                test->cmd_buf->DispatchDirect(1, 1, 1);

            };

        },
        [](t_cs_shader_test* test) {
            printf("[base_level = 0]\n");
            printf("TEXTURE = 0x%08llX %s\n", test->dst[ 0], PASS[test->dst[ 0] == 1]);
            printf("TEXTURE = 0x%08llX %s\n", test->dst[ 1], PASS[test->dst[ 1] == 2]);
            printf("TEXTURE = 0x%08llX %s\n", test->dst[ 2], PASS[test->dst[ 2] == 3]);
            printf("TEXTURE = 0x%08llX %s\n", test->dst[ 3], PASS[test->dst[ 3] == 4]);
            //                                                            
            printf("[base_level = 1]\n");                                 
            printf("TEXTURE = 0x%08llX %s\n", test->dst[ 4], PASS[test->dst[ 4] == 5]);
            printf("TEXTURE = 0x%08llX %s\n", test->dst[ 5], PASS[test->dst[ 5] == 6]);
            printf("TEXTURE = 0x%08llX %s\n", test->dst[ 6], PASS[test->dst[ 6] == 7]);
            printf("TEXTURE = 0x%08llX %s\n", test->dst[ 7], PASS[test->dst[ 7] == 8]);
            //                                                            
            printf("[base_level = 2]\n");                                 
            printf("TEXTURE = 0x%08llX %s\n", test->dst[ 8], PASS[test->dst[ 8] == 9]);
            printf("TEXTURE = 0x%08llX %s\n", test->dst[ 9], PASS[test->dst[ 9] == 10]);
            printf("TEXTURE = 0x%08llX %s\n", test->dst[10], PASS[test->dst[10] == 11]);
            printf("TEXTURE = 0x%08llX %s\n", test->dst[11], PASS[test->dst[11] == 12]);
            //
            printf("[base_level = 3]\n");
            printf("TEXTURE = 0x%08llX %s\n", test->dst[12], PASS[test->dst[12] == 13]);
            printf("TEXTURE = 0x%08llX %s\n", test->dst[13], PASS[test->dst[13] == 14]);
            printf("TEXTURE = 0x%08llX %s\n", test->dst[14], PASS[test->dst[14] == 15]);
            printf("TEXTURE = 0x%08llX %s\n", test->dst[15], PASS[test->dst[15] == 16]);
        }
    );

#endif  // IMAGE_STORE_MIP sweeps

    //DST SELECTOR
    //make_cs_test_dst_sel(&linear_dmem, cmd_buf);

    ///SRC SELECTOR
    //make_cs_test_dst_sel_load(&linear_dmem, cmd_buf);

    make_cs_mask_readback_test(&linear_dmem, cmd_buf);

    make_cs_lg_u64_test(&linear_dmem, cmd_buf);

    cmd_buf->prepareFlip();

    ///////////

    void* dcbGpuAddrs = cmd_buf->dmem_ptr;
    uint  dcbSizesInBytes = cmd_buf->get_stream_size();

    printf("dcbSizesInBytes->%d\n", dcbSizesInBytes);

    ret = Gnm::sceGnmSubmitAndFlipCommandBuffers(
        1,
        &dcbGpuAddrs,
        &dcbSizesInBytes,
        NULL,
        NULL,
        hVideo,
        2,
        FLIP_VSYNC,
        0x103
    );
    printf("sceGnmSubmitAndFlipCommandBuffers->%d %s\n", ret, PASS[ret == 0]);

    printf("[after SubmitAndFlip] labels[0]->%d labels[1]->%d labels[2]->%d\n", labels[0], labels[1], labels[2]);

    //break the index, but everything should be fine since PFP has already cached the values
    ((short*)ptr_vindex)[6] = 0xF;

    dmem_big_index[last_id * 3 + 2] = 0xF;

    //resume execution of ME
    wait_addr[0] = 1;

    event = {};
    event_count = {};

    ret = sceKernelWaitEqueue(dce_eq, &event, 1, &event_count, NULL);
    printf("sceKernelWaitEqueue->%d %s\n", ret, PASS[ret == 0]);

    //0x%016llX

    printf("[kevent]\n");
    printf(" ident =0x%016llX %s\n", event.ident, PASS[event.ident == 0x0006000000000000]);
    printf(" filter=%d %s\n", event.filter, PASS[event.filter == -13]);
    printf(" data  =0x%016llX\n", event.data);

    dce_data = *reinterpret_cast<t_dce_data*>(&event.data);

    printf("  time    =%d\n", dce_data.time);
    printf("  counter =%d %s\n", dce_data.counter, PASS[dce_data.counter == 1]);
    printf("  flip_arg=0x%016llX %s\n", dce_data.flip_arg, PASS[dce_data.flip_arg == 0x103]);

    printf(" udata =0x%016llX %s\n", event.udata, PASS[event.udata == &dce_eq]);

    Gnm::sceGnmSubmitDone();
    sceKernelUsleep(1000);

    /*
    for (int r = 0; r < cs_src_data_count; r++) {
        printf("cs_src_data[%d] =0x%08llX %f\n", r, (cs_src_data)[r], ((float*)cs_src_data)[r]);
    };

    for (int r = 0; r < cs_src_data_count; r++) {
        printf("cs_dst_data_10_11_11[%d] = 0x%08llX == 0x%08llX %s\n", r, cs_dst_data_10_11_11[r], test_11[r], PASS[cs_dst_data_10_11_11[r] == test_11[r]]);
    };

    for (int r = 0; r < cs_src_data_count; r++) {
        printf("cs_dst_data_11_11_10[%d] = 0x%08llX == 0x%08llX %s\n", r, cs_dst_data_11_11_10[r], test_10[r], PASS[cs_dst_data_11_11_10[r] == test_10[r]]);
    };

    for (int r = 0; r < 256; r++) {

        printf("cs_dst_data_reserved[%d] = 0x%08llX %f\n", r, (cs_dst_data_reserved)[r], ((float*)cs_dst_data_reserved)[r]);

    };
    */

    do_test_after_action();

    while (true) {
        sceKernelUsleep(1000);
        Gnm::sceGnmSubmitDone();
    };

    printf("[after done] labels[0]->%d labels[1]->%d labels[2]->%d\n", labels[0], labels[1], labels[2]);
    //[after done] labels[0]->1 labels[1]->0

    {

        OrbisKernelEqueue gc_eq = {};

        ret = sceKernelCreateEqueue(&gc_eq, "gc_eq");

        Gnm::sceGnmAddEqEvent(gc_eq, 0x40, (void*)0x1111111111111111);
        Gnm::sceGnmAddEqEvent(gc_eq, 0x00, (void*)0x1111111111111111);
        Gnm::sceGnmAddEqEvent(gc_eq, 0x01, (void*)0x1111111111111111);
        Gnm::sceGnmAddEqEvent(gc_eq, 0x02, (void*)0x1111111111111111);
        Gnm::sceGnmAddEqEvent(gc_eq, 0x03, (void*)0x1111111111111111);
        Gnm::sceGnmAddEqEvent(gc_eq, 0x04, (void*)0x1111111111111111);
        Gnm::sceGnmAddEqEvent(gc_eq, 0x05, (void*)0x1111111111111111);
        Gnm::sceGnmAddEqEvent(gc_eq, 0x06, (void*)0x1111111111111111);

        ulong* dstGpuAddr = (ulong*)linear_dmem.alloc(8,4);

        for (int srcSel = 0; srcSel <= 4; srcSel++)
            for (int intSel = 0; intSel <= 3; intSel++) {

                *dstGpuAddr = 0;

                cmd_buf->reset();

                cmd_buf->writeAtEndOfPipeWithInterrupt(
                    EopFlushCbDbCaches,
                    EventWriteDestMemory,
                    dstGpuAddr,
                    srcSel,
                    0x2222222222222222,
                    CacheActionNone,
                    CachePolicyLru,
                    intSel);

                void* dcbGpuAddrs = cmd_buf->dmem_ptr;
                uint  dcbSizesInBytes = cmd_buf->get_stream_size();

                ret = Gnm::sceGnmSubmitCommandBuffers(
                    1,
                    &dcbGpuAddrs,
                    &dcbSizesInBytes,
                    NULL,
                    NULL
                );
                printf("sceGnmSubmitCommandBuffers->%d %s\n", ret, PASS[ret == 0]);

                Gnm::sceGnmSubmitDone();

                OrbisKernelEvent event = {};
                int event_count = {};
                OrbisKernelUseconds usec = 1000 * 1000;

                ret = sceKernelWaitEqueue(gc_eq, &event, 1, &event_count, &usec);

                printf("EOP_TEST[srcSel:%d;intSel:%d]\n", srcSel, intSel);

                if ((intSel == 1) || (intSel == 2)) {
                    printf(" err=%d %s\n", ret, PASS[ret == 0]);
                }
                else {
                    printf(" err=%d %s\n", ret, PASS[ret == -2147352516]);
                };

                if ((srcSel == 0) || (intSel == 1)) {
                    printf(" dstGpuAddr=0x%016llX %s\n", *dstGpuAddr, PASS[*dstGpuAddr == 0]);
                }
                else
                    if (srcSel == 1) {
                        printf(" dstGpuAddr=0x%016llX %s\n", *dstGpuAddr, PASS[*dstGpuAddr == 0x0000000022222222]);
                    }
                    else
                        if (srcSel == 2) {
                            printf(" dstGpuAddr=0x%016llX %s\n", *dstGpuAddr, PASS[*dstGpuAddr == 0x2222222222222222]);
                        }
                        else
                        {
                            printf(" dstGpuAddr=0x%016llX %s\n", *dstGpuAddr, PASS[*dstGpuAddr != 0]);
                        };

                printf(" ident =0x%016llX %s\n", event.ident , PASS[(ret != 0) || (event.ident  == 0x0000000000000040)]);
                printf(" filter=%d %s\n"       , event.filter, PASS[(ret != 0) || (event.filter == -14)]);
                printf(" data  =0x%016llX\n"   , event.data);
                printf(" udata =0x%016llX %s\n", event.udata , PASS[(ret != 0) || (event.udata == (void*)0x1111111111111111)]);

            };

        ///flip

        for (int p = 0; p <= 3; p++)
        {
            *dstGpuAddr = 0;

            cmd_buf->reset();
            cmd_buf->InitDefault();

            switch (p) {
            case 0:
                cmd_buf->prepareFlip();
                break;
            case 1:
                cmd_buf->prepareFlip(dstGpuAddr, 0x22222222);
                break;
            case 2:
                cmd_buf->prepareFlipWithEopInterrupt(EopFlushCbDbCaches, CacheActionNone);
                break;
            case 3:
                cmd_buf->prepareFlipWithEopInterrupt(EopFlushCbDbCaches, dstGpuAddr, 0x22222222, CacheActionNone);
                break;
            };

            void* dcbGpuAddrs = cmd_buf->dmem_ptr;
            uint  dcbSizesInBytes = cmd_buf->get_stream_size();

            ret = Gnm::sceGnmSubmitAndFlipCommandBuffers(
                1,
                &dcbGpuAddrs,
                &dcbSizesInBytes,
                NULL,
                NULL,
                hVideo,
                2,
                FLIP_VSYNC,
                0x103
            );
            printf("sceGnmSubmitAndFlipCommandBuffers->%d %s\n", ret, PASS[ret == 0]);

            Gnm::sceGnmSubmitDone();

            OrbisKernelEvent event = {};
            int event_count = {};
            OrbisKernelUseconds usec = 1000 * 1000;

            ret = sceKernelWaitEqueue(gc_eq, &event, 1, &event_count, &usec);

            printf("SubmitAndFlip[%d]\n", p);
  
            if ((p == 2) || (p == 3)) {
                printf(" err=%d %s\n", ret, PASS[ret == 0]);
            }
            else {
                printf(" err=%d %s\n", ret, PASS[ret == -2147352516]);
            };

            if ((p == 0) || (p == 2)) {
                printf(" dstGpuAddr=0x%016llX %s\n", *dstGpuAddr, PASS[*dstGpuAddr == 0]);
            }
            else
            {
                printf(" dstGpuAddr=0x%016llX %s\n", *dstGpuAddr, PASS[*dstGpuAddr == 0x22222222]);
            };

            printf(" ident =0x%016llX %s\n", event.ident, PASS[(ret != 0) || (event.ident == 0x0000000000000040)]);
            printf(" filter=%d %s\n", event.filter, PASS[(ret != 0) || (event.filter == -14)]);
            printf(" data  =0x%016llX\n", event.data);
            printf(" udata =0x%016llX %s\n", event.udata, PASS[(ret != 0) || (event.udata == (void*)0x1111111111111111)]);


        };

        auto compute_queue = new ComputeQueue(0, 0);
        ret = compute_queue->Map(1 << 8, linear_dmem.alloc(8, 4));

        printf("compute_queue->Map():%d\n", ret);

        for (int srcSel = 0; srcSel <= 3; srcSel++)
            for (int intSel = 0; intSel <= 3; intSel++) {

                *dstGpuAddr = 0;

                cmd_buf->reset();

                cmd_buf->writeReleaseMemEventWithInterrupt(
                    ReleaseMemEventFlushCbDbCaches,
                    EventWriteDestMemory,
                    dstGpuAddr,
                    srcSel,
                    0x2222222222222222,
                    CacheActionNone,
                    CachePolicyLru,
                    intSel);

                compute_queue->callCommandBuffer(cmd_buf);

                compute_queue->DingDong();

                OrbisKernelEvent event = {};
                int event_count = {};
                OrbisKernelUseconds usec = 1000 * 1000;

                ret = sceKernelWaitEqueue(gc_eq, &event, 1, &event_count, &usec);

                printf("RELEASE_TEST[srcSel:%d;intSel:%d]\n", srcSel, intSel);

                if ((intSel == 1) || (intSel == 2)) {
                    printf(" err=%d %s\n", ret, PASS[ret == 0]);
                }
                else {
                    printf(" err=%d %s\n", ret, PASS[ret == -2147352516]);
                };

                if (srcSel == 0) {
                    printf(" dstGpuAddr=0x%016llX %s\n", *dstGpuAddr, PASS[*dstGpuAddr == 0]);
                }
                else
                    if (srcSel == 1) {
                        printf(" dstGpuAddr=0x%016llX %s\n", *dstGpuAddr, PASS[*dstGpuAddr == 0x0000000022222222]);
                    }
                    else
                        if (srcSel == 2) {
                            printf(" dstGpuAddr=0x%016llX %s\n", *dstGpuAddr, PASS[*dstGpuAddr == 0x2222222222222222]);
                        }
                        else
                        {
                            printf(" dstGpuAddr=0x%016llX %s\n", *dstGpuAddr, PASS[*dstGpuAddr != 0]);
                        };

                printf(" ident =0x%016llX %s\n", event.ident, PASS[(ret != 0) || (event.ident == 0x00)]);
                printf(" filter=%d %s\n"       , event.filter, PASS[(ret != 0) || (event.filter == -14)]);
                printf(" data  =0x%016llX\n"   , event.data);
                printf(" udata =0x%016llX %s\n", event.udata, PASS[(ret != 0) || (event.udata == (void*)0x1111111111111111)]);

            };


    }; //end EOP/RELEASE test




    while (true) {
        sceKernelUsleep(1000);
        Gnm::sceGnmSubmitDone();
    };

    for (;;) {}
}
