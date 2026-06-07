#pragma once

#include <nmmintrin.h> 
#include "gnmapi.h"
#include "gcn_op.h"

struct ShaderBinaryInfo {
    char signature[7];
    uint version : 8;

    uint pssl_or_cg : 1;
    uint cached : 1;
    uint m_type : 4;
    uint source_type : 2;
    uint length : 24;

    uint chunkUsageBaseOffsetInDW : 8;
    uint numInputUsageSlots : 8;
    uint isSrt : 1;
    uint isSrtUsedInfoValid : 1;
    uint isExtendedUsageInfo : 1;
    uint SourceHashType : 2;
    uint reserved2 : 3;
    uint footer_version : 8;

    uint shaderHash0;
    uint shaderHash1;
    uint crc32;
};
static_assert(sizeof(ShaderBinaryInfo) == 28);

uint32_t calc_crc32(void* data, ulong len) {
    uint32_t crc = 0xFFFFFFFF;

    while (len >= 8) {
        crc = _mm_crc32_u64(crc, *reinterpret_cast<const uint64_t*>(data));
        data = (void*)((ulong)(data) + 8);
        len -= 8;
    };

    while (len--) {
        crc = _mm_crc32_u8(crc, *reinterpret_cast<const uint8_t*>(data));
        data = (void*)((ulong)(data) + 1);
    };

    return ~crc;
};


uint MAKE_SGPR(uint id) {
    return id;
};

#define VCC_LO     106
#define VCC_HI     107
#define M0         124
#define EXEC_LO    126
#define EXEC_HI    127
#define VCCZ       251
#define EXECZ      252
#define SCC        253
#define LDS_DIRECT 254
#define LITERAL    255

uint MAKE_IMM_INT(int value) {
    if ((value >= 0) && (value <= 64)) {
        return value + 128;
    };
    if ((value >= -16) && (value <= -1)) {
        return (-value) + 192;
    };
    return 128;
};

int MAKE_IMM_FLOAT(float value) {
    if (value == 0.5) {
        return 240;
    };
    if (value == -0.5) {
        return 241;
    };
    if (value == 1.0) {
        return 242;
    };
    if (value == -1.0) {
        return 243;
    };
    if (value == 2.0) {
        return 244;
    };
    if (value == -2.0) {
        return 245;
    };
    if (value == 4.0) {
        return 246;
    };
    if (value == -4.0) {
        return 247;
    };
    return 128;
};

uint MAKE_VGPR(uint id) {
    return id + 256;
};

#define H_SOP1   0b101111101 //9
#define H_SOPC   0b101111110 //9
#define H_SOPP   0b101111111 //9
                 
#define H_VOP1   0b0111111   //7
#define H_VOPC   0b0111110   //7
                             
#define H_VOP3   0b110100    //6
#define H_DS     0b110110    //6
#define H_MUBUF  0b111000    //6
#define H_MTBUF  0b111010    //6
#define H_EXP    0b111110    //6
#define H_VINTRP 0b110010    //6
#define H_MIMG   0b111100    //6
                             
#define H_SMRD   0b11000     //5
                             
#define H_SOPK   0b1011      //4
                             
#define H_SOP2   0b10        //2
                             
#define H_VOP2   0b0         //1 

struct  TSOP1 {
    uint SSRC : 8;
    uint OP : 8;
    uint SDST : 7;
    uint ENCODE : 9;
};

struct TSOP2 {
    uint SSRC0 : 8;
    uint SSRC1 : 8;
    uint SDST : 7;
    uint OP : 7;
    uint ENCODE : 2;
};

struct TSOPP {
    uint SIMM : 16;
    uint OP : 7;
    uint ENCODE : 9;
};

struct Twaitcnt_simm {
    uint vmcnt : 4;
    uint expcnt : 3;
    uint reserved1 : 1;
    uint lgkmcnt : 4;
    uint reserved2 : 4;
};

struct TVOP1 {
    uint SRC0 : 9;
    uint OP : 8;
    uint VDST : 8;
    uint ENCODE : 7;
};

struct TVOP2 {
    uint SRC0 : 9;
    uint VSRC1 : 8;
    uint VDST : 8;
    uint OP : 6;
    uint ENCODE : 1;
};

struct TVOP3a {
    uint VDST : 8;
    uint ABS : 3;
    uint CLAMP : 1;
    uint OP_SEL : 4;
    uint OPM : 1;
    uint OP : 9;
    uint ENCODE : 6;

    uint SRC0 : 9;
    uint SRC1 : 9;
    uint SRC2 : 9;
    uint OMOD : 2;
    uint NEG : 3;
};

static_assert(sizeof(TVOP3a) == sizeof(uint64_t));

struct TVOP3b {
    uint VDST : 8;
    uint SDST : 7;
    uint reserved : 2;
    uint OP : 9;
    uint ENCODE : 6;

    uint SRC0 : 9;
    uint SRC1 : 9;
    uint SRC2 : 9;
    uint OMOD : 2;
    uint NEG : 3;
};

typedef enum {
    //input (per src)
    V3F_ABS = 1 << 31,
    V3F_NEG = 1 << 30,
    //output (dst only)
    V3F_CLAMP = 1 << 31,
    V3F_MUL2 = 1 << 29,
    V3F_MUL4 = 2 << 29,
    V3F_DIV2 = 3 << 29
} Vop3Flags;

static Vop3Flags operator|(Vop3Flags a, Vop3Flags b) {
    return static_cast<Vop3Flags>(
        static_cast<uint>(a) | static_cast<uint>(b)
        );
};

static uint operator|(uint a, Vop3Flags b) {
    return static_cast<Vop3Flags>(
        static_cast<uint>(a) | static_cast<uint>(b)
        );
};

struct TMUBUF {
    uint OFFSET : 12;
    uint OFFEN : 1;
    uint IDXEN : 1;
    uint GLC : 1;
    uint reserved1 : 1;
    uint LDS : 1;
    uint reserved2 : 1;
    uint OP : 7;
    uint reserved3 : 1;
    uint ENCODE : 6;

    uint VADDR : 8;
    uint VDATA : 8;
    uint SRSRC : 5;
    uint reserved4 : 1;
    uint SLC : 1;
    uint TFE : 1;
    uint SOFFSET : 8;
};

struct TMTBUF {
    uint OFFSET : 12;
    uint OFFEN : 1;
    uint IDXEN : 1;
    uint GLC : 1;
    uint reserved1 : 1;
    uint OP : 3;
    uint DFMT : 4;
    uint NFMT : 3;
    uint ENCODE : 6;

    uint VADDR : 8;
    uint VDATA : 8;
    uint SRSRC : 5;
    uint reserved4 : 1;
    uint SLC : 1;
    uint TFE : 1;
    uint SOFFSET : 8;
};

typedef enum {
    BUF_NONE = 0,
    BUF_OFFEN = 1 << 0,
    BUF_IDXEN = 1 << 1,
    BUF_GLC = 1 << 2,
    BUF_LDS = 1 << 3,
    BUF_SLC = 1 << 4,
    BUF_TFE = 1 << 5
} BufFlags;

BufFlags operator|(BufFlags a, BufFlags b) {
    return static_cast<BufFlags>(
        static_cast<uint>(a) | static_cast<uint>(b)
        );
};

struct TEXP {
    uint EN : 4;
    uint TGT : 6;
    uint COMPR : 1;
    uint DONE : 1;
    uint VM : 1;
    uint reserved : 13;
    uint ENCODING : 6;

    uint VSRC0 : 8;
    uint VSRC1 : 8;
    uint VSRC2 : 8;
    uint VSRC3 : 8;
};

struct TVINTRP {
    uint VSRC : 8;
    uint ATTRCHAN : 2;
    uint ATTR : 6;
    uint OP : 2;
    uint VDST : 8;
    uint ENCODING : 6;
};

struct TMIMG {
    uint reserved1 : 8;
    uint DMASK : 4;
    uint UNRM : 1;
    uint GLC : 1;
    uint DA : 1;
    uint R128 : 1;
    uint TFE : 1;
    uint LWE : 1;
    uint OP : 7;
    uint SLC : 1;
    uint ENCODE : 6;

    uint VADDR : 8;
    uint VDATA : 8;
    uint SRSRC : 5;
    uint SSAMP : 5;
    uint reserved2 : 6;
};

typedef enum {
    MIMG_NONE = 0,
    MIMG_UNRM = 1 << 0,
    MIMG_GLC  = 1 << 1,
    MIMG_DA   = 1 << 2,
    MIMG_R128 = 1 << 3,
    MIMG_TFE  = 1 << 4,
    MIMG_LWE  = 1 << 5,
    MIMG_SLC  = 1 << 6
} MimgFlags;

MimgFlags operator|(MimgFlags a, MimgFlags b) {
    return static_cast<MimgFlags>(
        static_cast<uint>(a) | static_cast<uint>(b)
        );
};


class ShaderBuilder
{
public:

    std::vector<uint> body{};

    uint vgpr_count;
    uint sgpr_count;
    uint user_count;
    uint lds_size;
    uint dx10_clamp;

    uint NumThreadX;
    uint NumThreadY;
    uint NumThreadZ;

    uint s_tgid_x;
    uint s_tgid_y;
    uint s_tgid_z;

    uint v_thread_cnt;

public:
    ShaderBuilder()
    {
        vgpr_count = 1;
        sgpr_count = 1;
        user_count = 1;
        lds_size = 0;
        dx10_clamp = 1;

        NumThreadX = 1;
        NumThreadY = 1;
        NumThreadZ = 1;

        s_tgid_x = 0;
        s_tgid_y = 0;
        s_tgid_z = 0;

        v_thread_cnt = 1;
    };

    uint GetByteSize()
    {
        uint base_size = body.size() * 4;
        uint align_size = (base_size + 7) & (~7); //align

        return 8 + align_size + sizeof(ShaderBinaryInfo); //header + body + footer
    };

    void CopyBody(void* buffer, uint size) {

        if (size < 8) return;

        uint base_size = body.size() * 4;
        uint align_size = (base_size + 7) & (~7); //align

        ((uint*)buffer)[0] = 0xBEEB03FF;       //s_mov_b32
        ((uint*)buffer)[1] = align_size >> 3;  //data

        buffer = (void*)((ulong)buffer + 8);
        size = size - 8;

        uint diff = base_size;
        if (size < base_size) diff = size;

        std::memcpy(buffer, body.data(), diff); //body

        buffer = (void*)((ulong)buffer + diff);
        size = size - diff;

        if (align_size > diff) {
            diff = align_size - diff;
            if (size < diff) return;
            buffer = (void*)((ulong)buffer + diff);
            size = size - diff;
        };

        uint32_t crc32 = calc_crc32(body.data(), base_size);

        ShaderBinaryInfo info = {};
        info.signature[0] = 'O';
        info.signature[1] = 'r';
        info.signature[2] = 'b';
        info.signature[3] = 'S';
        info.signature[4] = 'h';
        info.signature[5] = 'd';
        info.signature[6] = 'r';
        info.version = 7;
        info.pssl_or_cg = 1;
        info.source_type = 1;
        info.length = base_size + 8;
        info.SourceHashType = 1;
        info.footer_version = 4;
        info.shaderHash0 = crc32; //???
        info.shaderHash1 = crc32; //???
        info.crc32 = crc32;       //???

        if (size < sizeof(ShaderBinaryInfo)) return;

        std::memcpy(buffer, &info, sizeof(ShaderBinaryInfo)); //footer

    };

    Gnm::CsStageRegisters ExportCs(void* shader) {

        CopyBody(shader, GetByteSize());

        Gnm::CsStageRegisters regs = {};

        ((uint*)&regs)[0] = (uintptr_t)shader >> 8;
        ((uint*)&regs)[1] = (uintptr_t)shader >> 8 >> 32;

        regs.computeNumThreadX = NumThreadX;
        regs.computeNumThreadY = NumThreadY;
        regs.computeNumThreadZ = NumThreadZ;
           
        int const vcc_pair = 2;

        regs.computePgmRsrc1.VGPRS = (vgpr_count + 3) / 4 - 1;
        regs.computePgmRsrc1.SGPRS = (sgpr_count + vcc_pair + 7) / 8 - 1;
        regs.computePgmRsrc1.FLOAT_MODE = 192;
        regs.computePgmRsrc1.DX10_CLAMP = dx10_clamp;
           
        if (user_count <= 16) {
            regs.computePgmRsrc2.USER_SGPR = user_count;
        }
        else {
            regs.computePgmRsrc2.USER_SGPR = 16;
        };
       
        regs.computePgmRsrc2.LDS_SIZE = (lds_size + 511) / 512;

        regs.computePgmRsrc2.TGID_X_EN = (s_tgid_x != 0);
        regs.computePgmRsrc2.TGID_Y_EN = (s_tgid_y != 0);
        regs.computePgmRsrc2.TGID_Z_EN = (s_tgid_z != 0);

        if ((v_thread_cnt >= 1) && (v_thread_cnt <= 3)) {
            regs.computePgmRsrc2.TIDIG_COMP_CNT = v_thread_cnt - 1;
        };

        return regs;
    };

    void S_ENDPGM() {
        TSOPP cmd = { .ENCODE = H_SOPP };

        cmd.OP = SOPP_ENDPGM;

        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
    };

    void S_NOP() {
        TSOPP cmd = { .ENCODE = H_SOPP };

        cmd.OP = SOPP_NOP;

        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
    };

    void S_BARRIER() {
        TSOPP cmd = { .ENCODE = H_SOPP };

        cmd.OP = SOPP_BARRIER;

        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
    };

    void S_WAITCNT(uint lgkmcnt, uint expcnt, uint vmcnt) {

        TSOPP cmd = { .ENCODE = H_SOPP };

        cmd.OP = SOPP_WAITCNT;

        Twaitcnt_simm bits = {};
        bits.vmcnt = vmcnt;
        bits.expcnt = expcnt;
        bits.lgkmcnt = lgkmcnt;

        cmd.SIMM = *reinterpret_cast<uint16_t*>(&bits);

        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
    };

    void S_MOV_B32(uint sdst, uint ssrc) {

        TSOP1 cmd = { .ENCODE = H_SOP1 };

        cmd.OP = SOP1_MOV_B32;

        cmd.SDST = sdst;
        cmd.SSRC = ssrc;

        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
    };

    void S_MOV_B64(uint sdst, uint ssrc) {

        TSOP1 cmd = { .ENCODE = H_SOP1 };

        cmd.OP = SOP1_MOV_B64;

        cmd.SDST = sdst;
        cmd.SSRC = ssrc;

        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
    };

    void SOP2_OP(uint op,
                 uint sdst,
                 uint ssrc0, uint ssrc1) {

        TSOP2 cmd = { .ENCODE = H_SOP2 };

        cmd.OP = op;

        cmd.SDST = sdst;
        cmd.SSRC0 = ssrc0;
        cmd.SSRC1 = ssrc1;
        
        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
    };

    void V_MOV_B32(uint vdst, uint ssrc) {

        TVOP1 cmd = { .ENCODE = H_VOP1 };

        cmd.OP = VOP1_MOV_B32;

        cmd.VDST = vdst - 256;
        cmd.SRC0 = ssrc;

        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);

    };

    void V_READFIRSTLANE_B32(uint sdst,uint vsrc) {

        TVOP1 cmd = { .ENCODE = H_VOP1 };

        cmd.OP = VOP1_READFIRSTLANE_B32;

        cmd.VDST = sdst;
        cmd.SRC0 = vsrc;

        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
    };

    void VOP2_OP(uint op,
                 uint vdst,
                 uint src0, uint vsrc1
                ) {

        TVOP2 cmd = { .ENCODE = H_VOP2 };

        cmd.OP = op;

        cmd.VDST  = vdst - 256;
        cmd.SRC0  = src0;
        cmd.VSRC1 = vsrc1 - 256;
   
        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
    };

    void VOP3a_OP(uint op,
                  uint vdst,
                  uint src0, uint src1, uint src2
                 ) {

        TVOP3a cmd = { .ENCODE = H_VOP3 };
        cmd.OP_SEL = 0;

        cmd.OP = op & 0x1FF;
        cmd.OPM = op >> 9;

        cmd.VDST = (vdst & 0x1FF) - 256;

        cmd.SRC0 = src0;
        cmd.SRC1 = src1;
        cmd.SRC2 = src2;

        cmd.ABS   = (((src0 & V3F_ABS) != 0) << 0) | (((src1 & V3F_ABS) != 0) << 1) | (((src2 & V3F_ABS) != 0) << 2);
        cmd.CLAMP = (vdst & V3F_CLAMP) != 0;
            
        cmd.OMOD  = vdst >> 29;
        cmd.NEG   = (((src0 & V3F_NEG) != 0) << 0) | (((src1 & V3F_NEG) != 0) << 1) | (((src2 & V3F_NEG) != 0) << 2);

        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
        body.push_back(reinterpret_cast<uint*>(&cmd)[1]);
    };

    void VOP3b_OP(uint op,
                  uint vdst,
                  uint sdst,
                  uint src0, uint src1, uint src2
    ) {

        TVOP3b cmd = { .ENCODE = H_VOP3 };
        cmd.reserved = 0;

        cmd.OP = op;

        cmd.VDST = (vdst & 0x1FF) - 256;
        cmd.SDST = sdst;

        cmd.SRC0 = src0;
        cmd.SRC1 = src1;
        cmd.SRC2 = src2;

        cmd.OMOD = vdst >> 29;
        cmd.NEG = (((src0 & V3F_NEG) != 0) << 0) | (((src1 & V3F_NEG) != 0) << 1) | (((src2 & V3F_NEG) != 0) << 2);

        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
        body.push_back(reinterpret_cast<uint*>(&cmd)[1]);
    };

    void VOP3c_OP(uint op,
                  uint sdst,
                  uint src0, uint src1
    ) {

        TVOP3a cmd = { .ENCODE = H_VOP3 };

        cmd.OP = op;

        cmd.VDST = sdst;

        cmd.SRC0 = src0;
        cmd.SRC1 = src1;
        cmd.SRC2 = 0;

        cmd.ABS   = (((src0 & V3F_ABS) != 0) << 0) | (((src1 & V3F_ABS) != 0) << 1);
        cmd.CLAMP = (sdst & V3F_CLAMP) != 0;

        cmd.OMOD = sdst >> 29;
        cmd.NEG  = (((src0 & V3F_NEG) != 0) << 0) | (((src1 & V3F_NEG) != 0) << 1);

        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
        body.push_back(reinterpret_cast<uint*>(&cmd)[1]);
    };


    void MUBUF_OP(uint op,
        uint vdata,
        uint vaddr,
        uint svsharp,
        uint imm_offset,
        uint soffset,
        BufFlags flags
    ) {

        TMUBUF cmd = { .ENCODE = H_MUBUF };

        cmd.OP = op;

        cmd.VDATA = (vdata - 256);
        cmd.VADDR = (vaddr - 256);
        cmd.SRSRC = (svsharp / 4);
        cmd.OFFSET = imm_offset;
        cmd.SOFFSET = soffset;

        cmd.OFFEN = (flags & BUF_OFFEN) != 0;
        cmd.IDXEN = (flags & BUF_IDXEN) != 0;
        cmd.GLC = (flags & BUF_GLC) != 0;
        cmd.LDS = (flags & BUF_LDS) != 0;
        cmd.SLC = (flags & BUF_SLC) != 0;
        cmd.TFE = (flags & BUF_TFE) != 0;

        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
        body.push_back(reinterpret_cast<uint*>(&cmd)[1]);
    };

    void MIMG_OP(uint op,
        uint vdata,
        uint vaddr,
        uint stsharp,
        uint sssharp,
        uint dmask,
        MimgFlags flags
    ) {

        TMIMG cmd = { .ENCODE = H_MIMG };

        cmd.OP = op;

        cmd.VDATA = (vdata - 256);
        cmd.VADDR = (vaddr - 256);
        cmd.SRSRC = (stsharp / 4);
        cmd.SSAMP = (sssharp / 4);

        cmd.DMASK = dmask;

        cmd.GLC = (flags & BUF_GLC) != 0;

        cmd.UNRM = (flags & MIMG_UNRM) != 0;
        cmd.GLC  = (flags & MIMG_GLC ) != 0;
        cmd.DA   = (flags & MIMG_DA  ) != 0;
        cmd.R128 = (flags & MIMG_R128) != 0;
        cmd.TFE  = (flags & MIMG_TFE ) != 0;
        cmd.LWE  = (flags & MIMG_LWE ) != 0;
        cmd.SLC  = (flags & MIMG_SLC ) != 0;

        body.push_back(reinterpret_cast<uint*>(&cmd)[0]);
        body.push_back(reinterpret_cast<uint*>(&cmd)[1]);
    };


};

