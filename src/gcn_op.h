#pragma once

 //SOP1
#define SOP1_MOV_B32            0x03
#define SOP1_MOV_B64            0x04
#define SOP1_CMOV_B32           0x05
#define SOP1_CMOV_B64           0x06
#define SOP1_NOT_B32            0x07
#define SOP1_NOT_B64            0x08
#define SOP1_WQM_B32            0x09
#define SOP1_WQM_B64            0x0A
#define SOP1_BREV_B32           0x0B
#define SOP1_BREV_B64           0x0C
#define SOP1_BCNT0_I32_B32      0x0D
#define SOP1_BCNT0_I32_B64      0x0E
#define SOP1_BCNT1_I32_B32      0x0F
#define SOP1_BCNT1_I32_B64      0x10
#define SOP1_FF0_I32_B32        0x11
#define SOP1_FF0_I32_B64        0x12
#define SOP1_FF1_I32_B32        0x13
#define SOP1_FF1_I32_B64        0x14
#define SOP1_FLBIT_I32_B32      0x15
#define SOP1_FLBIT_I32_B64      0x16
#define SOP1_FLBIT_I32          0x17
#define SOP1_FLBIT_I32_I64      0x18
#define SOP1_SEXT_I32_I8        0x19
#define SOP1_SEXT_I32_I16       0x1A
#define SOP1_BITSET0_B32        0x1B
#define SOP1_BITSET0_B64        0x1C
#define SOP1_BITSET1_B32        0x1D
#define SOP1_BITSET1_B64        0x1E
#define SOP1_GETPC_B64          0x1F
#define SOP1_SETPC_B64          0x20
#define SOP1_SWAPPC_B64         0x21

#define SOP1_AND_SAVEEXEC_B64   0x24
#define SOP1_OR_SAVEEXEC_B64    0x25
#define SOP1_XOR_SAVEEXEC_B64   0x26
#define SOP1_ANDN2_SAVEEXEC_B64 0x27
#define SOP1_ORN2_SAVEEXEC_B64  0x28
#define SOP1_NAND_SAVEEXEC_B64  0x29
#define SOP1_NOR_SAVEEXEC_B64   0x2A
#define SOP1_XNOR_SAVEEXEC_B64  0x2B
#define SOP1_QUADMASK_B32       0x2C
#define SOP1_QUADMASK_B64       0x2D
#define SOP1_MOVRELS_B32        0x2E
#define SOP1_MOVRELS_B64        0x2F
#define SOP1_MOVRELD_B32        0x30
#define SOP1_MOVRELD_B64        0x31
#define SOP1_CBRANCH_JOIN       0x32

#define SOP1_ABS_I32            0x34


 //SOP2
#define SOP2_ADD_U32        0x00
#define SOP2_SUB_U32        0x01
#define SOP2_ADD_I32        0x02
#define SOP2_SUB_I32        0x03
#define SOP2_ADDC_U32       0x04
#define SOP2_SUBB_U32       0x05
#define SOP2_MIN_I32        0x06
#define SOP2_MIN_U32        0x07
#define SOP2_MAX_I32        0x08
#define SOP2_MAX_U32        0x09
#define SOP2_CSELECT_B32    0x0A
#define SOP2_CSELECT_B64    0x0B

#define SOP2_AND_B32        0x0E
#define SOP2_AND_B64        0x0F
#define SOP2_OR_B32         0x10
#define SOP2_OR_B64         0x11
#define SOP2_XOR_B32        0x12
#define SOP2_XOR_B64        0x13
#define SOP2_ANDN2_B32      0x14
#define SOP2_ANDN2_B64      0x15
#define SOP2_ORN2_B32       0x16
#define SOP2_ORN2_B64       0x17
#define SOP2_NAND_B32       0x18
#define SOP2_NAND_B64       0x19
#define SOP2_NOR_B32        0x1A
#define SOP2_NOR_B64        0x1B
#define SOP2_XNOR_B32       0x1C
#define SOP2_XNOR_B64       0x1D
#define SOP2_LSHL_B32       0x1E
#define SOP2_LSHL_B64       0x1F
#define SOP2_LSHR_B32       0x20
#define SOP2_LSHR_B64       0x21
#define SOP2_ASHR_I32       0x22
#define SOP2_ASHR_I64       0x23
#define SOP2_BFM_B32        0x24
#define SOP2_BFM_B64        0x25
#define SOP2_MUL_I32        0x26
#define SOP2_BFE_U32        0x27
#define SOP2_BFE_I32        0x28
#define SOP2_BFE_U64        0x29
#define SOP2_BFE_I64        0x2A
#define SOP2_CBRANCH_G_FORK 0x2B
#define SOP2_ABSDIFF_I32    0x2C


 //SOPP
#define SOPP_NOP            0x00
#define SOPP_ENDPGM         0x01
#define SOPP_BRANCH         0x02

#define SOPP_CBRANCH_SCC0   0x04
#define SOPP_CBRANCH_SCC1   0x05
#define SOPP_CBRANCH_VCCZ   0x06
#define SOPP_CBRANCH_VCCNZ  0x07
#define SOPP_CBRANCH_EXECZ  0x08
#define SOPP_CBRANCH_EXECNZ 0x09
#define SOPP_BARRIER        0x0A

#define SOPP_WAITCNT        0x0C

#define SOPP_SLEEP          0x0E
#define SOPP_SETPRIO        0x0F
#define SOPP_SENDMSG        0x10

#define SOPP_ICACHE_INV     0x13
#define SOPP_INCPERFLEVEL   0x14
#define SOPP_DECPERFLEVEL   0x15
#define SOPP_TTRACEDATA     0x16

 //SOPC
#define SOPC_CMP_EQ_I32  0x00
#define SOPC_CMP_LG_I32  0x01
#define SOPC_CMP_GT_I32  0x02
#define SOPC_CMP_GE_I32  0x03
#define SOPC_CMP_LT_I32  0x04
#define SOPC_CMP_LE_I32  0x05
#define SOPC_CMP_EQ_U32  0x06
#define SOPC_CMP_LG_U32  0x07
#define SOPC_CMP_GT_U32  0x08
#define SOPC_CMP_GE_U32  0x09
#define SOPC_CMP_LT_U32  0x0A
#define SOPC_CMP_LE_U32  0x0B
#define SOPC_BITCMP0_B32 0x0C
#define SOPC_BITCMP1_B32 0x0D
#define SOPC_BITCMP0_B64 0x0E
#define SOPC_BITCMP1_B64 0x0F
#define SOPC_SETVSKIP    0x10

 //SOPK
#define SOPK_MOVK_I32         0x00
#define SOPK_MOVK_HI_I32      0x01
#define SOPK_CMOVK_I32        0x02
#define SOPK_CMPK_EQ_I32      0x03
#define SOPK_CMPK_LG_I32      0x04
#define SOPK_CMPK_GT_I32      0x05
#define SOPK_CMPK_GE_I32      0x06
#define SOPK_CMPK_LT_I32      0x07
#define SOPK_CMPK_LE_I32      0x08
#define SOPK_CMPK_EQ_U32      0x09
#define SOPK_CMPK_LG_U32      0x0A
#define SOPK_CMPK_GT_U32      0x0B
#define SOPK_CMPK_GE_U32      0x0C
#define SOPK_CMPK_LT_U32      0x0D
#define SOPK_CMPK_LE_U32      0x0E
#define SOPK_ADDK_I32         0x0F
#define SOPK_MULK_I32         0x10
#define SOPK_CBRANCH_I_FORK   0x11
#define SOPK_GETREG_B32       0x12
#define SOPK_SETREG_B32       0x13
#define SOPK_SETREG_IMM32_B32 0x15

 //VOPC
#define VOPC_CMP_F_F32      0x00
#define VOPC_CMP_LT_F32     0x01
#define VOPC_CMP_EQ_F32     0x02
#define VOPC_CMP_LE_F32     0x03
#define VOPC_CMP_GT_F32     0x04
#define VOPC_CMP_LG_F32     0x05
#define VOPC_CMP_GE_F32     0x06
#define VOPC_CMP_O_F32      0x07
#define VOPC_CMP_U_F32      0x08
#define VOPC_CMP_NGE_F32    0x09
#define VOPC_CMP_NLG_F32    0x0A
#define VOPC_CMP_NGT_F32    0x0B
#define VOPC_CMP_NLE_F32    0x0C
#define VOPC_CMP_NEQ_F32    0x0D
#define VOPC_CMP_NLT_F32    0x0E
#define VOPC_CMP_T_F32      0x0F

#define VOPC_CMPX_F_F32     0x10
#define VOPC_CMPX_LT_F32    0x11
#define VOPC_CMPX_EQ_F32    0x12
#define VOPC_CMPX_LE_F32    0x13
#define VOPC_CMPX_GT_F32    0x14
#define VOPC_CMPX_LG_F32    0x15
#define VOPC_CMPX_GE_F32    0x16
#define VOPC_CMPX_O_F32     0x17
#define VOPC_CMPX_U_F32     0x18
#define VOPC_CMPX_NGE_F32   0x19
#define VOPC_CMPX_NLG_F32   0x1A
#define VOPC_CMPX_NGT_F32   0x1B
#define VOPC_CMPX_NLE_F32   0x1C
#define VOPC_CMPX_NEQ_F32   0x1D
#define VOPC_CMPX_NLT_F32   0x1E
#define VOPC_CMPX_T_F32     0x1F

#define VOPC_CMP_F_F64      0x20
#define VOPC_CMP_LT_F64     0x21
#define VOPC_CMP_EQ_F64     0x22
#define VOPC_CMP_LE_F64     0x23
#define VOPC_CMP_GT_F64     0x24
#define VOPC_CMP_LG_F64     0x25
#define VOPC_CMP_GE_F64     0x26
#define VOPC_CMP_O_F64      0x27
#define VOPC_CMP_U_F64      0x28
#define VOPC_CMP_NGE_F64    0x29
#define VOPC_CMP_NLG_F64    0x2A
#define VOPC_CMP_NGT_F64    0x2B
#define VOPC_CMP_NLE_F64    0x2C
#define VOPC_CMP_NEQ_F64    0x2D
#define VOPC_CMP_NLT_F64    0x2E
#define VOPC_CMP_T_F64      0x2F

#define VOPC_CMPX_F_F64     0x30
#define VOPC_CMPX_LT_F64    0x31
#define VOPC_CMPX_EQ_F64    0x32
#define VOPC_CMPX_LE_F64    0x33
#define VOPC_CMPX_GT_F64    0x34
#define VOPC_CMPX_LG_F64    0x35
#define VOPC_CMPX_GE_F64    0x36
#define VOPC_CMPX_O_F64     0x37
#define VOPC_CMPX_U_F64     0x38
#define VOPC_CMPX_NGE_F64   0x39
#define VOPC_CMPX_NLG_F64   0x3A
#define VOPC_CMPX_NGT_F64   0x3B
#define VOPC_CMPX_NLE_F64   0x3C
#define VOPC_CMPX_NEQ_F64   0x3D
#define VOPC_CMPX_NLT_F64   0x3E
#define VOPC_CMPX_T_F64     0x3F

#define VOPC_CMPS_F_F32     0x40
#define VOPC_CMPS_LT_F32    0x41
#define VOPC_CMPS_EQ_F32    0x42
#define VOPC_CMPS_LE_F32    0x43
#define VOPC_CMPS_GT_F32    0x44
#define VOPC_CMPS_LG_F32    0x45
#define VOPC_CMPS_GE_F32    0x46
#define VOPC_CMPS_O_F32     0x47
#define VOPC_CMPS_U_F32     0x48
#define VOPC_CMPS_NGE_F32   0x49
#define VOPC_CMPS_NLG_F32   0x4A
#define VOPC_CMPS_NGT_F32   0x4B
#define VOPC_CMPS_NLE_F32   0x4C
#define VOPC_CMPS_NEQ_F32   0x4D
#define VOPC_CMPS_NLT_F32   0x4E
#define VOPC_CMPS_T_F32     0x4F

#define VOPC_CMPSX_F_F32    0x50
#define VOPC_CMPSX_LT_F32   0x51
#define VOPC_CMPSX_EQ_F32   0x52
#define VOPC_CMPSX_LE_F32   0x53
#define VOPC_CMPSX_GT_F32   0x54
#define VOPC_CMPSX_LG_F32   0x55
#define VOPC_CMPSX_GE_F32   0x56
#define VOPC_CMPSX_O_F32    0x57
#define VOPC_CMPSX_U_F32    0x58
#define VOPC_CMPSX_NGE_F32  0x59
#define VOPC_CMPSX_NLG_F32  0x5A
#define VOPC_CMPSX_NGT_F32  0x5B
#define VOPC_CMPSX_NLE_F32  0x5C
#define VOPC_CMPSX_NEQ_F32  0x5D
#define VOPC_CMPSX_NLT_F32  0x5E
#define VOPC_CMPSX_T_F32    0x5F

#define VOPC_CMPS_F_F64     0x60
#define VOPC_CMPS_LT_F64    0x61
#define VOPC_CMPS_EQ_F64    0x62
#define VOPC_CMPS_LE_F64    0x63
#define VOPC_CMPS_GT_F64    0x64
#define VOPC_CMPS_LG_F64    0x65
#define VOPC_CMPS_GE_F64    0x66
#define VOPC_CMPS_O_F64     0x67
#define VOPC_CMPS_U_F64     0x68
#define VOPC_CMPS_NGE_F64   0x69
#define VOPC_CMPS_NLG_F64   0x6A
#define VOPC_CMPS_NGT_F64   0x6B
#define VOPC_CMPS_NLE_F64   0x6C
#define VOPC_CMPS_NEQ_F64   0x6D
#define VOPC_CMPS_NLT_F64   0x6E
#define VOPC_CMPS_T_F64     0x6F

#define VOPC_CMPSX_F_F64    0x70
#define VOPC_CMPSX_LT_F64   0x71
#define VOPC_CMPSX_EQ_F64   0x72
#define VOPC_CMPSX_LE_F64   0x73
#define VOPC_CMPSX_GT_F64   0x74
#define VOPC_CMPSX_LG_F64   0x75
#define VOPC_CMPSX_GE_F64   0x76
#define VOPC_CMPSX_O_F64    0x77
#define VOPC_CMPSX_U_F64    0x78
#define VOPC_CMPSX_NGE_F64  0x79
#define VOPC_CMPSX_NLG_F64  0x7A
#define VOPC_CMPSX_NGT_F64  0x7B
#define VOPC_CMPSX_NLE_F64  0x7C
#define VOPC_CMPSX_NEQ_F64  0x7D
#define VOPC_CMPSX_NLT_F64  0x7E
#define VOPC_CMPSX_T_F64    0x7F

#define VOPC_CMP_F_I32      0x80
#define VOPC_CMP_LT_I32     0x81
#define VOPC_CMP_EQ_I32     0x82
#define VOPC_CMP_LE_I32     0x83
#define VOPC_CMP_GT_I32     0x84
#define VOPC_CMP_LG_I32     0x85
#define VOPC_CMP_GE_I32     0x86
#define VOPC_CMP_T_I32      0x87

#define VOPC_CMPX_F_I32     0x90
#define VOPC_CMPX_LT_I32    0x91
#define VOPC_CMPX_EQ_I32    0x92
#define VOPC_CMPX_LE_I32    0x93
#define VOPC_CMPX_GT_I32    0x94
#define VOPC_CMPX_LG_I32    0x95
#define VOPC_CMPX_GE_I32    0x96
#define VOPC_CMPX_T_I32     0x97

#define VOPC_CMP_F_I64      0xA0
#define VOPC_CMP_LT_I64     0xA1
#define VOPC_CMP_EQ_I64     0xA2
#define VOPC_CMP_LE_I64     0xA3
#define VOPC_CMP_GT_I64     0xA4
#define VOPC_CMP_LG_I64     0xA5
#define VOPC_CMP_GE_I64     0xA6
#define VOPC_CMP_T_I64      0xA7

#define VOPC_CMPX_F_I64     0xB0
#define VOPC_CMPX_LT_I64    0xB1
#define VOPC_CMPX_EQ_I64    0xB2
#define VOPC_CMPX_LE_I64    0xB3
#define VOPC_CMPX_GT_I64    0xB4
#define VOPC_CMPX_LG_I64    0xB5
#define VOPC_CMPX_GE_I64    0xB6
#define VOPC_CMPX_T_I64     0xB7

#define VOPC_CMP_F_U32      0xC0
#define VOPC_CMP_LT_U32     0xC1
#define VOPC_CMP_EQ_U32     0xC2
#define VOPC_CMP_LE_U32     0xC3
#define VOPC_CMP_GT_U32     0xC4
#define VOPC_CMP_LG_U32     0xC5
#define VOPC_CMP_GE_U32     0xC6
#define VOPC_CMP_T_U32      0xC7

#define VOPC_CMPX_F_U32     0xD0
#define VOPC_CMPX_LT_U32    0xD1
#define VOPC_CMPX_EQ_U32    0xD2
#define VOPC_CMPX_LE_U32    0xD3
#define VOPC_CMPX_GT_U32    0xD4
#define VOPC_CMPX_LG_U32    0xD5
#define VOPC_CMPX_GE_U32    0xD6
#define VOPC_CMPX_T_U32     0xD7

#define VOPC_CMP_F_U64      0xE0
#define VOPC_CMP_LT_U64     0xE1
#define VOPC_CMP_EQ_U64     0xE2
#define VOPC_CMP_LE_U64     0xE3
#define VOPC_CMP_GT_U64     0xE4
#define VOPC_CMP_LG_U64     0xE5
#define VOPC_CMP_GE_U64     0xE6
#define VOPC_CMP_T_U64      0xE7

#define VOPC_CMPX_F_U64     0xF0
#define VOPC_CMPX_LT_U64    0xF1
#define VOPC_CMPX_EQ_U64    0xF2
#define VOPC_CMPX_LE_U64    0xF3
#define VOPC_CMPX_GT_U64    0xF4
#define VOPC_CMPX_LG_U64    0xF5
#define VOPC_CMPX_GE_U64    0xF6
#define VOPC_CMPX_T_U64     0xF7

#define VOPC_CMP_CLASS_F32  0x88
#define VOPC_CMPX_CLASS_F32 0x98
#define VOPC_CMP_CLASS_F64  0xA8
#define VOPC_CMPX_CLASS_F64 0xB8

 //VOP1
#define VOP1_NOP               0x00
#define VOP1_MOV_B32           0x01
#define VOP1_READFIRSTLANE_B32 0x02
#define VOP1_CVT_I32_F64       0x03
#define VOP1_CVT_F64_I32       0x04
#define VOP1_CVT_F32_I32       0x05
#define VOP1_CVT_F32_U32       0x06
#define VOP1_CVT_U32_F32       0x07
#define VOP1_CVT_I32_F32       0x08
#define VOP1_MOV_FED_B32       0x09
#define VOP1_CVT_F16_F32       0x0A
#define VOP1_CVT_F32_F16       0x0B
#define VOP1_CVT_RPI_I32_F32   0x0C
#define VOP1_CVT_FLR_I32_F32   0x0D
#define VOP1_CVT_OFF_F32_I4    0x0E
#define VOP1_CVT_F32_F64       0x0F
#define VOP1_CVT_F64_F32       0x10
#define VOP1_CVT_F32_UBYTE0    0x11
#define VOP1_CVT_F32_UBYTE1    0x12
#define VOP1_CVT_F32_UBYTE2    0x13
#define VOP1_CVT_F32_UBYTE3    0x14
#define VOP1_CVT_U32_F64       0x15
#define VOP1_CVT_F64_U32       0x16
#define VOP1_TRUNC_F64         0x17
#define VOP1_CEIL_F64          0x18
#define VOP1_RNDNE_F64         0x19
#define VOP1_FLOOR_F64         0x1A
#define VOP1_FRACT_F32         0x20
#define VOP1_TRUNC_F32         0x21
#define VOP1_CEIL_F32          0x22
#define VOP1_RNDNE_F32         0x23
#define VOP1_FLOOR_F32         0x24
#define VOP1_EXP_F32           0x25
#define VOP1_LOG_CLAMP_F32     0x26
#define VOP1_LOG_F32           0x27
#define VOP1_RCP_CLAMP_F32     0x28
#define VOP1_RCP_LEGACY_F32    0x29
#define VOP1_RCP_F32           0x2A
#define VOP1_RCP_IFLAG_F32     0x2B
#define VOP1_RSQ_CLAMP_F32     0x2C
#define VOP1_RSQ_LEGACY_F32    0x2D
#define VOP1_RSQ_F32           0x2E
#define VOP1_RCP_F64           0x2F
#define VOP1_RCP_CLAMP_F64     0x30
#define VOP1_RSQ_F64           0x31
#define VOP1_RSQ_CLAMP_F64     0x32
#define VOP1_SQRT_F32          0x33
#define VOP1_SQRT_F64          0x34
#define VOP1_SIN_F32           0x35
#define VOP1_COS_F32           0x36
#define VOP1_NOT_B32           0x37
#define VOP1_BFREV_B32         0x38
#define VOP1_FFBH_U32          0x39
#define VOP1_FFBL_B32          0x3A
#define VOP1_FFBH_I32          0x3B
#define VOP1_FREXP_EXP_I32_F64 0x3C
#define VOP1_FREXP_MANT_F64    0x3D
#define VOP1_FRACT_F64         0x3E
#define VOP1_FREXP_EXP_I32_F32 0x3F
#define VOP1_FREXP_MANT_F32    0x40
#define VOP1_CLREXCP           0x41
#define VOP1_MOVRELD_B32       0x42
#define VOP1_MOVRELS_B32       0x43
#define VOP1_MOVRELSD_B32      0x44

 //VOP2
#define VOP2_CNDMASK_B32        0x00
#define VOP2_READLANE_B32       0x01
#define VOP2_WRITELANE_B32      0x02
#define VOP2_ADD_F32            0x03
#define VOP2_SUB_F32            0x04
#define VOP2_SUBREV_F32         0x05
#define VOP2_MAC_LEGACY_F32     0x06
#define VOP2_MUL_LEGACY_F32     0x07
#define VOP2_MUL_F32            0x08
#define VOP2_MUL_I32_I24        0x09
#define VOP2_MUL_HI_I32_I24     0x0A
#define VOP2_MUL_U32_U24        0x0B
#define VOP2_MUL_HI_U32_U24     0x0C
#define VOP2_MIN_LEGACY_F32     0x0D
#define VOP2_MAX_LEGACY_F32     0x0E
#define VOP2_MIN_F32            0x0F
#define VOP2_MAX_F32            0x10
#define VOP2_MIN_I32            0x11
#define VOP2_MAX_I32            0x12
#define VOP2_MIN_U32            0x13
#define VOP2_MAX_U32            0x14
#define VOP2_LSHR_B32           0x15
#define VOP2_LSHRREV_B32        0x16
#define VOP2_ASHR_I32           0x17
#define VOP2_ASHRREV_I32        0x18
#define VOP2_LSHL_B32           0x19
#define VOP2_LSHLREV_B32        0x1A
#define VOP2_AND_B32            0x1B
#define VOP2_OR_B32             0x1C
#define VOP2_XOR_B32            0x1D
#define VOP2_BFM_B32            0x1E
#define VOP2_MAC_F32            0x1F
#define VOP2_MADMK_F32          0x20
#define VOP2_MADAK_F32          0x21
#define VOP2_BCNT_U32_B32       0x22
#define VOP2_MBCNT_LO_U32_B32   0x23
#define VOP2_MBCNT_HI_U32_B32   0x24
#define VOP2_ADD_I32            0x25
#define VOP2_SUB_I32            0x26
#define VOP2_SUBREV_I32         0x27
#define VOP2_ADDC_U32           0x28
#define VOP2_SUBB_U32           0x29
#define VOP2_SUBBREV_U32        0x2A
#define VOP2_LDEXP_F32          0x2B
#define VOP2_CVT_PKACCUM_U8_F32 0x2C
#define VOP2_CVT_PKNORM_I16_F32 0x2D
#define VOP2_CVT_PKNORM_U16_F32 0x2E
#define VOP2_CVT_PKRTZ_F16_F32  0x2F
#define VOP2_CVT_PK_U16_U32     0x30
#define VOP2_CVT_PK_I16_I32     0x31

 //VOP3c
#define VOP3_CMP_F_F32      VOPC_CMP_F_F32
#define VOP3_CMP_LT_F32     VOPC_CMP_LT_F32
#define VOP3_CMP_EQ_F32     VOPC_CMP_EQ_F32
#define VOP3_CMP_LE_F32     VOPC_CMP_LE_F32
#define VOP3_CMP_GT_F32     VOPC_CMP_GT_F32
#define VOP3_CMP_LG_F32     VOPC_CMP_LG_F32
#define VOP3_CMP_GE_F32     VOPC_CMP_GE_F32
#define VOP3_CMP_O_F32      VOPC_CMP_O_F32
#define VOP3_CMP_U_F32      VOPC_CMP_U_F32
#define VOP3_CMP_NGE_F32    VOPC_CMP_NGE_F32
#define VOP3_CMP_NLG_F32    VOPC_CMP_NLG_F32
#define VOP3_CMP_NGT_F32    VOPC_CMP_NGT_F32
#define VOP3_CMP_NLE_F32    VOPC_CMP_NLE_F32
#define VOP3_CMP_NEQ_F32    VOPC_CMP_NEQ_F32
#define VOP3_CMP_NLT_F32    VOPC_CMP_NLT_F32
#define VOP3_CMP_T_F32      VOPC_CMP_T_F32

#define VOP3_CMPX_F_F32     VOPC_CMPX_F_F32
#define VOP3_CMPX_LT_F32    VOPC_CMPX_LT_F32
#define VOP3_CMPX_EQ_F32    VOPC_CMPX_EQ_F32
#define VOP3_CMPX_LE_F32    VOPC_CMPX_LE_F32
#define VOP3_CMPX_GT_F32    VOPC_CMPX_GT_F32
#define VOP3_CMPX_LG_F32    VOPC_CMPX_LG_F32
#define VOP3_CMPX_GE_F32    VOPC_CMPX_GE_F32
#define VOP3_CMPX_O_F32     VOPC_CMPX_O_F32
#define VOP3_CMPX_U_F32     VOPC_CMPX_U_F32
#define VOP3_CMPX_NGE_F32   VOPC_CMPX_NGE_F32
#define VOP3_CMPX_NLG_F32   VOPC_CMPX_NLG_F32
#define VOP3_CMPX_NGT_F32   VOPC_CMPX_NGT_F32
#define VOP3_CMPX_NLE_F32   VOPC_CMPX_NLE_F32
#define VOP3_CMPX_NEQ_F32   VOPC_CMPX_NEQ_F32
#define VOP3_CMPX_NLT_F32   VOPC_CMPX_NLT_F32
#define VOP3_CMPX_T_F32     VOPC_CMPX_T_F32

#define VOP3_CMP_F_F64      VOPC_CMP_F_F64
#define VOP3_CMP_LT_F64     VOPC_CMP_LT_F64
#define VOP3_CMP_EQ_F64     VOPC_CMP_EQ_F64
#define VOP3_CMP_LE_F64     VOPC_CMP_LE_F64
#define VOP3_CMP_GT_F64     VOPC_CMP_GT_F64
#define VOP3_CMP_LG_F64     VOPC_CMP_LG_F64
#define VOP3_CMP_GE_F64     VOPC_CMP_GE_F64
#define VOP3_CMP_O_F64      VOPC_CMP_O_F64
#define VOP3_CMP_U_F64      VOPC_CMP_U_F64
#define VOP3_CMP_NGE_F64    VOPC_CMP_NGE_F64
#define VOP3_CMP_NLG_F64    VOPC_CMP_NLG_F64
#define VOP3_CMP_NGT_F64    VOPC_CMP_NGT_F64
#define VOP3_CMP_NLE_F64    VOPC_CMP_NLE_F64
#define VOP3_CMP_NEQ_F64    VOPC_CMP_NEQ_F64
#define VOP3_CMP_NLT_F64    VOPC_CMP_NLT_F64
#define VOP3_CMP_T_F64      VOPC_CMP_T_F64

#define VOP3_CMPX_F_F64     VOPC_CMPX_F_F64
#define VOP3_CMPX_LT_F64    VOPC_CMPX_LT_F64
#define VOP3_CMPX_EQ_F64    VOPC_CMPX_EQ_F64
#define VOP3_CMPX_LE_F64    VOPC_CMPX_LE_F64
#define VOP3_CMPX_GT_F64    VOPC_CMPX_GT_F64
#define VOP3_CMPX_LG_F64    VOPC_CMPX_LG_F64
#define VOP3_CMPX_GE_F64    VOPC_CMPX_GE_F64
#define VOP3_CMPX_O_F64     VOPC_CMPX_O_F64
#define VOP3_CMPX_U_F64     VOPC_CMPX_U_F64
#define VOP3_CMPX_NGE_F64   VOPC_CMPX_NGE_F64
#define VOP3_CMPX_NLG_F64   VOPC_CMPX_NLG_F64
#define VOP3_CMPX_NGT_F64   VOPC_CMPX_NGT_F64
#define VOP3_CMPX_NLE_F64   VOPC_CMPX_NLE_F64
#define VOP3_CMPX_NEQ_F64   VOPC_CMPX_NEQ_F64
#define VOP3_CMPX_NLT_F64   VOPC_CMPX_NLT_F64
#define VOP3_CMPX_T_F64     VOPC_CMPX_T_F64

#define VOP3_CMPS_F_F32     VOPC_CMPS_F_F32
#define VOP3_CMPS_LT_F32    VOPC_CMPS_LT_F32
#define VOP3_CMPS_EQ_F32    VOPC_CMPS_EQ_F32
#define VOP3_CMPS_LE_F32    VOPC_CMPS_LE_F32
#define VOP3_CMPS_GT_F32    VOPC_CMPS_GT_F32
#define VOP3_CMPS_LG_F32    VOPC_CMPS_LG_F32
#define VOP3_CMPS_GE_F32    VOPC_CMPS_GE_F32
#define VOP3_CMPS_O_F32     VOPC_CMPS_O_F32
#define VOP3_CMPS_U_F32     VOPC_CMPS_U_F32
#define VOP3_CMPS_NGE_F32   VOPC_CMPS_NGE_F32
#define VOP3_CMPS_NLG_F32   VOPC_CMPS_NLG_F32
#define VOP3_CMPS_NGT_F32   VOPC_CMPS_NGT_F32
#define VOP3_CMPS_NLE_F32   VOPC_CMPS_NLE_F32
#define VOP3_CMPS_NEQ_F32   VOPC_CMPS_NEQ_F32
#define VOP3_CMPS_NLT_F32   VOPC_CMPS_NLT_F32
#define VOP3_CMPS_T_F32     VOPC_CMPS_T_F32

#define VOP3_CMPSX_F_F32    VOPC_CMPSX_F_F32
#define VOP3_CMPSX_LT_F32   VOPC_CMPSX_LT_F32
#define VOP3_CMPSX_EQ_F32   VOPC_CMPSX_EQ_F32
#define VOP3_CMPSX_LE_F32   VOPC_CMPSX_LE_F32
#define VOP3_CMPSX_GT_F32   VOPC_CMPSX_GT_F32
#define VOP3_CMPSX_LG_F32   VOPC_CMPSX_LG_F32
#define VOP3_CMPSX_GE_F32   VOPC_CMPSX_GE_F32
#define VOP3_CMPSX_O_F32    VOPC_CMPSX_O_F32
#define VOP3_CMPSX_U_F32    VOPC_CMPSX_U_F32
#define VOP3_CMPSX_NGE_F32  VOPC_CMPSX_NGE_F32
#define VOP3_CMPSX_NLG_F32  VOPC_CMPSX_NLG_F32
#define VOP3_CMPSX_NGT_F32  VOPC_CMPSX_NGT_F32
#define VOP3_CMPSX_NLE_F32  VOPC_CMPSX_NLE_F32
#define VOP3_CMPSX_NEQ_F32  VOPC_CMPSX_NEQ_F32
#define VOP3_CMPSX_NLT_F32  VOPC_CMPSX_NLT_F32
#define VOP3_CMPSX_T_F32    VOPC_CMPSX_T_F32

#define VOP3_CMPS_F_F64     VOPC_CMPS_F_F64
#define VOP3_CMPS_LT_F64    VOPC_CMPS_LT_F64
#define VOP3_CMPS_EQ_F64    VOPC_CMPS_EQ_F64
#define VOP3_CMPS_LE_F64    VOPC_CMPS_LE_F64
#define VOP3_CMPS_GT_F64    VOPC_CMPS_GT_F64
#define VOP3_CMPS_LG_F64    VOPC_CMPS_LG_F64
#define VOP3_CMPS_GE_F64    VOPC_CMPS_GE_F64
#define VOP3_CMPS_O_F64     VOPC_CMPS_O_F64
#define VOP3_CMPS_U_F64     VOPC_CMPS_U_F64
#define VOP3_CMPS_NGE_F64   VOPC_CMPS_NGE_F64
#define VOP3_CMPS_NLG_F64   VOPC_CMPS_NLG_F64
#define VOP3_CMPS_NGT_F64   VOPC_CMPS_NGT_F64
#define VOP3_CMPS_NLE_F64   VOPC_CMPS_NLE_F64
#define VOP3_CMPS_NEQ_F64   VOPC_CMPS_NEQ_F64
#define VOP3_CMPS_NLT_F64   VOPC_CMPS_NLT_F64
#define VOP3_CMPS_T_F64     VOPC_CMPS_T_F64

#define VOP3_CMPSX_F_F64    VOPC_CMPSX_F_F64
#define VOP3_CMPSX_LT_F64   VOPC_CMPSX_LT_F64
#define VOP3_CMPSX_EQ_F64   VOPC_CMPSX_EQ_F64
#define VOP3_CMPSX_LE_F64   VOPC_CMPSX_LE_F64
#define VOP3_CMPSX_GT_F64   VOPC_CMPSX_GT_F64
#define VOP3_CMPSX_LG_F64   VOPC_CMPSX_LG_F64
#define VOP3_CMPSX_GE_F64   VOPC_CMPSX_GE_F64
#define VOP3_CMPSX_O_F64    VOPC_CMPSX_O_F64
#define VOP3_CMPSX_U_F64    VOPC_CMPSX_U_F64
#define VOP3_CMPSX_NGE_F64  VOPC_CMPSX_NGE_F64
#define VOP3_CMPSX_NLG_F64  VOPC_CMPSX_NLG_F64
#define VOP3_CMPSX_NGT_F64  VOPC_CMPSX_NGT_F64
#define VOP3_CMPSX_NLE_F64  VOPC_CMPSX_NLE_F64
#define VOP3_CMPSX_NEQ_F64  VOPC_CMPSX_NEQ_F64
#define VOP3_CMPSX_NLT_F64  VOPC_CMPSX_NLT_F64
#define VOP3_CMPSX_T_F64    VOPC_CMPSX_T_F64

#define VOP3_CMP_F_I32      VOPC_CMP_F_I32
#define VOP3_CMP_LT_I32     VOPC_CMP_LT_I32
#define VOP3_CMP_EQ_I32     VOPC_CMP_EQ_I32
#define VOP3_CMP_LE_I32     VOPC_CMP_LE_I32
#define VOP3_CMP_GT_I32     VOPC_CMP_GT_I32
#define VOP3_CMP_LG_I32     VOPC_CMP_LG_I32
#define VOP3_CMP_GE_I32     VOPC_CMP_GE_I32
#define VOP3_CMP_T_I32      VOPC_CMP_T_I32

#define VOP3_CMPX_F_I32     VOPC_CMPX_F_I32
#define VOP3_CMPX_LT_I32    VOPC_CMPX_LT_I32
#define VOP3_CMPX_EQ_I32    VOPC_CMPX_EQ_I32
#define VOP3_CMPX_LE_I32    VOPC_CMPX_LE_I32
#define VOP3_CMPX_GT_I32    VOPC_CMPX_GT_I32
#define VOP3_CMPX_LG_I32    VOPC_CMPX_LG_I32
#define VOP3_CMPX_GE_I32    VOPC_CMPX_GE_I32
#define VOP3_CMPX_T_I32     VOPC_CMPX_T_I32

#define VOP3_CMP_F_I64      VOPC_CMP_F_I64
#define VOP3_CMP_LT_I64     VOPC_CMP_LT_I64
#define VOP3_CMP_EQ_I64     VOPC_CMP_EQ_I64
#define VOP3_CMP_LE_I64     VOPC_CMP_LE_I64
#define VOP3_CMP_GT_I64     VOPC_CMP_GT_I64
#define VOP3_CMP_LG_I64     VOPC_CMP_LG_I64
#define VOP3_CMP_GE_I64     VOPC_CMP_GE_I64
#define VOP3_CMP_T_I64      VOPC_CMP_T_I64

#define VOP3_CMPX_F_I64     VOPC_CMPX_F_I64
#define VOP3_CMPX_LT_I64    VOPC_CMPX_LT_I64
#define VOP3_CMPX_EQ_I64    VOPC_CMPX_EQ_I64
#define VOP3_CMPX_LE_I64    VOPC_CMPX_LE_I64
#define VOP3_CMPX_GT_I64    VOPC_CMPX_GT_I64
#define VOP3_CMPX_LG_I64    VOPC_CMPX_LG_I64
#define VOP3_CMPX_GE_I64    VOPC_CMPX_GE_I64
#define VOP3_CMPX_T_I64     VOPC_CMPX_T_I64

#define VOP3_CMP_F_U32      VOPC_CMP_F_U32
#define VOP3_CMP_LT_U32     VOPC_CMP_LT_U32
#define VOP3_CMP_EQ_U32     VOPC_CMP_EQ_U32
#define VOP3_CMP_LE_U32     VOPC_CMP_LE_U32
#define VOP3_CMP_GT_U32     VOPC_CMP_GT_U32
#define VOP3_CMP_LG_U32     VOPC_CMP_LG_U32
#define VOP3_CMP_GE_U32     VOPC_CMP_GE_U32
#define VOP3_CMP_T_U32      VOPC_CMP_T_U32

#define VOP3_CMPX_F_U32     VOPC_CMPX_F_U32
#define VOP3_CMPX_LT_U32    VOPC_CMPX_LT_U32
#define VOP3_CMPX_EQ_U32    VOPC_CMPX_EQ_U32
#define VOP3_CMPX_LE_U32    VOPC_CMPX_LE_U32
#define VOP3_CMPX_GT_U32    VOPC_CMPX_GT_U32
#define VOP3_CMPX_LG_U32    VOPC_CMPX_LG_U32
#define VOP3_CMPX_GE_U32    VOPC_CMPX_GE_U32
#define VOP3_CMPX_T_U32     VOPC_CMPX_T_U32

#define VOP3_CMP_F_U64      VOPC_CMP_F_U64
#define VOP3_CMP_LT_U64     VOPC_CMP_LT_U64
#define VOP3_CMP_EQ_U64     VOPC_CMP_EQ_U64
#define VOP3_CMP_LE_U64     VOPC_CMP_LE_U64
#define VOP3_CMP_GT_U64     VOPC_CMP_GT_U64
#define VOP3_CMP_LG_U64     VOPC_CMP_LG_U64
#define VOP3_CMP_GE_U64     VOPC_CMP_GE_U64
#define VOP3_CMP_T_U64      VOPC_CMP_T_U64

#define VOP3_CMPX_F_U64     VOPC_CMPX_F_U64
#define VOP3_CMPX_LT_U64    VOPC_CMPX_LT_U64
#define VOP3_CMPX_EQ_U64    VOPC_CMPX_EQ_U64
#define VOP3_CMPX_LE_U64    VOPC_CMPX_LE_U64
#define VOP3_CMPX_GT_U64    VOPC_CMPX_GT_U64
#define VOP3_CMPX_LG_U64    VOPC_CMPX_LG_U64
#define VOP3_CMPX_GE_U64    VOPC_CMPX_GE_U64
#define VOP3_CMPX_T_U64     VOPC_CMPX_T_U64

#define VOP3_CMP_CLASS_F32  VOPC_CMP_CLASS_F32
#define VOP3_CMPX_CLASS_F32 VOPC_CMPX_CLASS_F32
#define VOP3_CMP_CLASS_F64  VOPC_CMP_CLASS_F64
#define VOP3_CMPX_CLASS_F64 VOPC_CMPX_CLASS_F64

 //VOP3a

#define VOP3_CNDMASK_B32        (256+VOP2_CNDMASK_B32   )
#define VOP3_READLANE_B32       (256+VOP2_READLANE_B32  )
#define VOP3_WRITELANE_B32      (256+VOP2_WRITELANE_B32 )
#define VOP3_ADD_F32            (256+VOP2_ADD_F32       )
#define VOP3_SUB_F32            (256+VOP2_SUB_F32       )
#define VOP3_SUBREV_F32         (256+VOP2_SUBREV_F32    )
#define VOP3_MAC_LEGACY_F32     (256+VOP2_MAC_LEGACY_F32)
#define VOP3_MUL_LEGACY_F32     (256+VOP2_MUL_LEGACY_F32)
#define VOP3_MUL_F32            (256+VOP2_MUL_F32       )
#define VOP3_MUL_I32_I24        (256+VOP2_MUL_I32_I24   )
#define VOP3_MUL_HI_I32_I24     (256+VOP2_MUL_HI_I32_I24)
#define VOP3_MUL_U32_U24        (256+VOP2_MUL_U32_U24   )
#define VOP3_MUL_HI_U32_U24     (256+VOP2_MUL_HI_U32_U24)
#define VOP3_MIN_LEGACY_F32     (256+VOP2_MIN_LEGACY_F32)
#define VOP3_MAX_LEGACY_F32     (256+VOP2_MAX_LEGACY_F32)
#define VOP3_MIN_F32            (256+VOP2_MIN_F32       )
#define VOP3_MAX_F32            (256+VOP2_MAX_F32       )
#define VOP3_MIN_I32            (256+VOP2_MIN_I32       )
#define VOP3_MAX_I32            (256+VOP2_MAX_I32       )
#define VOP3_MIN_U32            (256+VOP2_MIN_U32       )
#define VOP3_MAX_U32            (256+VOP2_MAX_U32       )
#define VOP3_LSHR_B32           (256+VOP2_LSHR_B32      )
#define VOP3_LSHRREV_B32        (256+VOP2_LSHRREV_B32   )
#define VOP3_ASHR_I32           (256+VOP2_ASHR_I32      )
#define VOP3_ASHRREV_I32        (256+VOP2_ASHRREV_I32   )
#define VOP3_LSHL_B32           (256+VOP2_LSHL_B32      )
#define VOP3_LSHLREV_B32        (256+VOP2_LSHLREV_B32   )
#define VOP3_AND_B32            (256+VOP2_AND_B32       )
#define VOP3_OR_B32             (256+VOP2_OR_B32        )
#define VOP3_XOR_B32            (256+VOP2_XOR_B32       )
#define VOP3_BFM_B32            (256+VOP2_BFM_B32       )
#define VOP3_MAC_F32            (256+VOP2_MAC_F32       )

#define VOP3_BCNT_U32_B32       (256+VOP2_BCNT_U32_B32    )
#define VOP3_MBCNT_LO_U32_B32   (256+VOP2_MBCNT_LO_U32_B32)
#define VOP3_MBCNT_HI_U32_B32   (256+VOP2_MBCNT_HI_U32_B32)

#define VOP3_LDEXP_F32          (256+VOP2_LDEXP_F32         )
#define VOP3_CVT_PKACCUM_U8_F32 (256+VOP2_CVT_PKACCUM_U8_F32)
#define VOP3_CVT_PKNORM_I16_F32 (256+VOP2_CVT_PKNORM_I16_F32)
#define VOP3_CVT_PKNORM_U16_F32 (256+VOP2_CVT_PKNORM_U16_F32)
#define VOP3_CVT_PKRTZ_F16_F32  (256+VOP2_CVT_PKRTZ_F16_F32 )
#define VOP3_CVT_PK_U16_U32     (256+VOP2_CVT_PK_U16_U32    )
#define VOP3_CVT_PK_I16_I32     (256+VOP2_CVT_PK_I16_I32    )

#define VOP3_MAD_LEGACY_F32  0x140
#define VOP3_MAD_F32         0x141
#define VOP3_MAD_I32_I24     0x142
#define VOP3_MAD_U32_U24     0x143
#define VOP3_CUBEID_F32      0x144
#define VOP3_CUBESC_F32      0x145
#define VOP3_CUBETC_F32      0x146
#define VOP3_CUBEMA_F32      0x147
#define VOP3_BFE_U32         0x148
#define VOP3_BFE_I32         0x149
#define VOP3_BFI_B32         0x14A
#define VOP3_FMA_F32         0x14B
#define VOP3_FMA_F64         0x14C
#define VOP3_LERP_U8         0x14D
#define VOP3_ALIGNBIT_B32    0x14E
#define VOP3_ALIGNBYTE_B32   0x14F
#define VOP3_MULLIT_F32      0x150
#define VOP3_MIN3_F32        0x151
#define VOP3_MIN3_I32        0x152
#define VOP3_MIN3_U32        0x153
#define VOP3_MAX3_F32        0x154
#define VOP3_MAX3_I32        0x155
#define VOP3_MAX3_U32        0x156
#define VOP3_MED3_F32        0x157
#define VOP3_MED3_I32        0x158
#define VOP3_MED3_U32        0x159
#define VOP3_SAD_U8          0x15A
#define VOP3_SAD_HI_U8       0x15B
#define VOP3_SAD_U16         0x15C
#define VOP3_SAD_U32         0x15D
#define VOP3_CVT_PK_U8_F32   0x15E
#define VOP3_DIV_FIXUP_F32   0x15F
#define VOP3_DIV_FIXUP_F64   0x160
#define VOP3_LSHL_B64        0x161
#define VOP3_LSHR_B64        0x162
#define VOP3_ASHR_I64        0x163
#define VOP3_ADD_F64         0x164
#define VOP3_MUL_F64         0x165
#define VOP3_MIN_F64         0x166
#define VOP3_MAX_F64         0x167
#define VOP3_LDEXP_F64       0x168
#define VOP3_MUL_LO_U32      0x169
#define VOP3_MUL_HI_U32      0x16A
#define VOP3_MUL_LO_I32      0x16B
#define VOP3_MUL_HI_I32      0x16C

#define VOP3_DIV_FMAS_F32    0x16F
#define VOP3_DIV_FMAS_F64    0x170
#define VOP3_MSAD_U8         0x171
#define VOP3_QSAD_PK_U16_U8  0x172
#define VOP3_MQSAD_PK_U16_U8 0x173
#define VOP3_TRIG_PREOP_F64  0x174
#define VOP3_MQSAD_U32_U8    0x175
#define VOP3_MAD_U64_U32     0x176
#define VOP3_MAD_I64_I32     0x177

#define VOP3_NOP               (384+VOP1_NOP              )
#define VOP3_MOV_B32           (384+VOP1_MOV_B32          )
#define VOP3_READFIRSTLANE_B32 (384+VOP1_READFIRSTLANE_B32)
#define VOP3_CVT_I32_F64       (384+VOP1_CVT_I32_F64      )
#define VOP3_CVT_F64_I32       (384+VOP1_CVT_F64_I32      )
#define VOP3_CVT_F32_I32       (384+VOP1_CVT_F32_I32      )
#define VOP3_CVT_F32_U32       (384+VOP1_CVT_F32_U32      )
#define VOP3_CVT_U32_F32       (384+VOP1_CVT_U32_F32      )
#define VOP3_CVT_I32_F32       (384+VOP1_CVT_I32_F32      )
#define VOP3_MOV_FED_B32       (384+VOP1_MOV_FED_B32      )
#define VOP3_CVT_F16_F32       (384+VOP1_CVT_F16_F32      )
#define VOP3_CVT_F32_F16       (384+VOP1_CVT_F32_F16      )
#define VOP3_CVT_RPI_I32_F32   (384+VOP1_CVT_RPI_I32_F32  )
#define VOP3_CVT_FLR_I32_F32   (384+VOP1_CVT_FLR_I32_F32  )
#define VOP3_CVT_OFF_F32_I4    (384+VOP1_CVT_OFF_F32_I4   )
#define VOP3_CVT_F32_F64       (384+VOP1_CVT_F32_F64      )
#define VOP3_CVT_F64_F32       (384+VOP1_CVT_F64_F32      )
#define VOP3_CVT_F32_UBYTE0    (384+VOP1_CVT_F32_UBYTE0   )
#define VOP3_CVT_F32_UBYTE1    (384+VOP1_CVT_F32_UBYTE1   )
#define VOP3_CVT_F32_UBYTE2    (384+VOP1_CVT_F32_UBYTE2   )
#define VOP3_CVT_F32_UBYTE3    (384+VOP1_CVT_F32_UBYTE3   )
#define VOP3_CVT_U32_F64       (384+VOP1_CVT_U32_F64      )
#define VOP3_CVT_F64_U32       (384+VOP1_CVT_F64_U32      )
#define VOP3_TRUNC_F64         (384+VOP1_TRUNC_F64        )
#define VOP3_CEIL_F64          (384+VOP1_CEIL_F64         )
#define VOP3_RNDNE_F64         (384+VOP1_RNDNE_F64        )
#define VOP3_FLOOR_F64         (384+VOP1_FLOOR_F64        )
#define VOP3_FRACT_F32         (384+VOP1_FRACT_F32        )
#define VOP3_TRUNC_F32         (384+VOP1_TRUNC_F32        )
#define VOP3_CEIL_F32          (384+VOP1_CEIL_F32         )
#define VOP3_RNDNE_F32         (384+VOP1_RNDNE_F32        )
#define VOP3_FLOOR_F32         (384+VOP1_FLOOR_F32        )
#define VOP3_EXP_F32           (384+VOP1_EXP_F32          )
#define VOP3_LOG_CLAMP_F32     (384+VOP1_LOG_CLAMP_F32    )
#define VOP3_LOG_F32           (384+VOP1_LOG_F32          )
#define VOP3_RCP_CLAMP_F32     (384+VOP1_RCP_CLAMP_F32    )
#define VOP3_RCP_LEGACY_F32    (384+VOP1_RCP_LEGACY_F32   )
#define VOP3_RCP_F32           (384+VOP1_RCP_F32          )
#define VOP3_RCP_IFLAG_F32     (384+VOP1_RCP_IFLAG_F32    )
#define VOP3_RSQ_CLAMP_F32     (384+VOP1_RSQ_CLAMP_F32    )
#define VOP3_RSQ_LEGACY_F32    (384+VOP1_RSQ_LEGACY_F32   )
#define VOP3_RSQ_F32           (384+VOP1_RSQ_F32          )
#define VOP3_RCP_F64           (384+VOP1_RCP_F64          )
#define VOP3_RCP_CLAMP_F64     (384+VOP1_RCP_CLAMP_F64    )
#define VOP3_RSQ_F64           (384+VOP1_RSQ_F64          )
#define VOP3_RSQ_CLAMP_F64     (384+VOP1_RSQ_CLAMP_F64    )
#define VOP3_SQRT_F32          (384+VOP1_SQRT_F32         )
#define VOP3_SQRT_F64          (384+VOP1_SQRT_F64         )
#define VOP3_SIN_F32           (384+VOP1_SIN_F32          )
#define VOP3_COS_F32           (384+VOP1_COS_F32          )
#define VOP3_NOT_B32           (384+VOP1_NOT_B32          )
#define VOP3_BFREV_B32         (384+VOP1_BFREV_B32        )
#define VOP3_FFBH_U32          (384+VOP1_FFBH_U32         )
#define VOP3_FFBL_B32          (384+VOP1_FFBL_B32         )
#define VOP3_FFBH_I32          (384+VOP1_FFBH_I32         )
#define VOP3_FREXP_EXP_I32_F64 (384+VOP1_FREXP_EXP_I32_F64)
#define VOP3_FREXP_MANT_F64    (384+VOP1_FREXP_MANT_F64   )
#define VOP3_FRACT_F64         (384+VOP1_FRACT_F64        )
#define VOP3_FREXP_EXP_I32_F32 (384+VOP1_FREXP_EXP_I32_F32)
#define VOP3_FREXP_MANT_F32    (384+VOP1_FREXP_MANT_F32   )
#define VOP3_CLREXCP           (384+VOP1_CLREXCP          )
#define VOP3_MOVRELD_B32       (384+VOP1_MOVRELD_B32      )
#define VOP3_MOVRELS_B32       (384+VOP1_MOVRELS_B32      )
#define VOP3_MOVRELSD_B32      (384+VOP1_MOVRELSD_B32     )

 //VOP3b
#define VOP3_DIV_SCALE_F32   0x16D
#define VOP3_DIV_SCALE_F64   0x16E

 //SMRD
#define SMRD_LOAD_DWORD           0x00
#define SMRD_LOAD_DWORDX2         0x01
#define SMRD_LOAD_DWORDX4         0x02
#define SMRD_LOAD_DWORDX8         0x03
#define SMRD_LOAD_DWORDX16        0x04

#define SMRD_BUFFER_LOAD_DWORD    0x08
#define SMRD_BUFFER_LOAD_DWORDX2  0x09
#define SMRD_BUFFER_LOAD_DWORDX4  0x0A
#define SMRD_BUFFER_LOAD_DWORDX8  0x0B
#define SMRD_BUFFER_LOAD_DWORDX16 0x0C

#define SMRD_MEMTIME              0x1E
#define SMRD_DCACHE_INV           0x1F

 //VINTRP
#define VINTRP_INTERP_P1_F32  0
#define VINTRP_INTERP_P2_F32  1
#define VINTRP_INTERP_MOV_F32 2

 //MUBUF
#define MUBUF_BUFFER_LOAD_FORMAT_X      0x00
#define MUBUF_BUFFER_LOAD_FORMAT_XY     0x01
#define MUBUF_BUFFER_LOAD_FORMAT_XYZ    0x02
#define MUBUF_BUFFER_LOAD_FORMAT_XYZW   0x03
#define MUBUF_BUFFER_STORE_FORMAT_X     0x04
#define MUBUF_BUFFER_STORE_FORMAT_XY    0x05
#define MUBUF_BUFFER_STORE_FORMAT_XYZ   0x06
#define MUBUF_BUFFER_STORE_FORMAT_XYZW  0x07

#define MUBUF_BUFFER_LOAD_UBYTE         0x08
#define MUBUF_BUFFER_LOAD_SBYTE         0x09
#define MUBUF_BUFFER_LOAD_USHORT        0x0A
#define MUBUF_BUFFER_LOAD_SSHORT        0x0B
#define MUBUF_BUFFER_LOAD_DWORD         0x0C
#define MUBUF_BUFFER_LOAD_DWORDX2       0x0D
#define MUBUF_BUFFER_LOAD_DWORDX4       0x0E
#define MUBUF_BUFFER_LOAD_DWORDX3       0x0F

#define MUBUF_BUFFER_STORE_BYTE         0x18
#define MUBUF_BUFFER_STORE_SHORT        0x1A
#define MUBUF_BUFFER_STORE_DWORD        0x1C
#define MUBUF_BUFFER_STORE_DWORDX2      0x1D
#define MUBUF_BUFFER_STORE_DWORDX4      0x1E
#define MUBUF_BUFFER_STORE_DWORDX3      0x1F

#define MUBUF_BUFFER_ATOMIC_SWAP        0x30
#define MUBUF_BUFFER_ATOMIC_CMPSWAP     0x31
#define MUBUF_BUFFER_ATOMIC_ADD         0x32
#define MUBUF_BUFFER_ATOMIC_SUB         0x33
#define MUBUF_BUFFER_ATOMIC_SMIN        0x35
#define MUBUF_BUFFER_ATOMIC_UMIN        0x36
#define MUBUF_BUFFER_ATOMIC_SMAX        0x37
#define MUBUF_BUFFER_ATOMIC_UMAX        0x38
#define MUBUF_BUFFER_ATOMIC_AND         0x39
#define MUBUF_BUFFER_ATOMIC_OR          0x3a
#define MUBUF_BUFFER_ATOMIC_XOR         0x3b
#define MUBUF_BUFFER_ATOMIC_INC         0x3c
#define MUBUF_BUFFER_ATOMIC_DEC         0x3d
#define MUBUF_BUFFER_ATOMIC_FCMPSWAP    0x3e
#define MUBUF_BUFFER_ATOMIC_FMIN        0x3f
#define MUBUF_BUFFER_ATOMIC_FMAX        0x40

#define MUBUF_BUFFER_ATOMIC_SWAP_X2     0x50
#define MUBUF_BUFFER_ATOMIC_CMPSWAP_X2  0x51
#define MUBUF_BUFFER_ATOMIC_ADD_X2      0x52
#define MUBUF_BUFFER_ATOMIC_SUB_X2      0x53
#define MUBUF_BUFFER_ATOMIC_SMIN_X2     0x55
#define MUBUF_BUFFER_ATOMIC_UMIN_X2     0x56
#define MUBUF_BUFFER_ATOMIC_SMAX_X2     0x57
#define MUBUF_BUFFER_ATOMIC_UMAX_X2     0x58
#define MUBUF_BUFFER_ATOMIC_AND_X2      0x59
#define MUBUF_BUFFER_ATOMIC_OR_X2       0x5a
#define MUBUF_BUFFER_ATOMIC_XOR_X2      0x5b
#define MUBUF_BUFFER_ATOMIC_INC_X2      0x5c
#define MUBUF_BUFFER_ATOMIC_DEC_X2      0x5d
#define MUBUF_BUFFER_ATOMIC_FCMPSWAP_X2 0x5e
#define MUBUF_BUFFER_ATOMIC_FMIN_X2     0x5f
#define MUBUF_BUFFER_ATOMIC_FMAX_X2     0x60

#define MUBUF_BUFFER_WBINVL1            0x71

 //MTBUF
#define MTBUF_TBUFFER_LOAD_FORMAT_X     0x00
#define MTBUF_TBUFFER_LOAD_FORMAT_XY    0x01
#define MTBUF_TBUFFER_LOAD_FORMAT_XYZ   0x02
#define MTBUF_TBUFFER_LOAD_FORMAT_XYZW  0x03

#define MTBUF_TBUFFER_STORE_FORMAT_X    0x04
#define MTBUF_TBUFFER_STORE_FORMAT_XY   0x05
#define MTBUF_TBUFFER_STORE_FORMAT_XYZ  0x06
#define MTBUF_TBUFFER_STORE_FORMAT_XYZW 0x07

 //MIMG
#define MIMG_IMAGE_LOAD             0x00
#define MIMG_IMAGE_LOAD_MIP         0x01
#define MIMG_IMAGE_LOAD_PCK         0x02
#define MIMG_IMAGE_LOAD_PCK_SGN     0x03
#define MIMG_IMAGE_LOAD_MIP_PCK     0x04
#define MIMG_IMAGE_LOAD_MIP_PCK_SGN 0x05

#define MIMG_IMAGE_STORE            0x08
#define MIMG_IMAGE_STORE_MIP        0x09
#define MIMG_IMAGE_STORE_PCK        0x0a
#define MIMG_IMAGE_STORE_MIP_PCK    0x0b

#define MIMG_IMAGE_GET_RESINFO      0x0e

#define MIMG_IMAGE_ATOMIC_SWAP      0x0f
#define MIMG_IMAGE_ATOMIC_CMPSWAP   0x10
#define MIMG_IMAGE_ATOMIC_ADD       0x11
#define MIMG_IMAGE_ATOMIC_SUB       0x12
#define MIMG_IMAGE_ATOMIC_SMIN      0x14
#define MIMG_IMAGE_ATOMIC_UMIN      0x15
#define MIMG_IMAGE_ATOMIC_SMAX      0x16
#define MIMG_IMAGE_ATOMIC_UMAX      0x17
#define MIMG_IMAGE_ATOMIC_AND       0x18
#define MIMG_IMAGE_ATOMIC_OR        0x19
#define MIMG_IMAGE_ATOMIC_XOR       0x1a
#define MIMG_IMAGE_ATOMIC_INC       0x1b
#define MIMG_IMAGE_ATOMIC_DEC       0x1c
#define MIMG_IMAGE_ATOMIC_FCMPSWAP  0x1d
#define MIMG_IMAGE_ATOMIC_FMIN      0x1e
#define MIMG_IMAGE_ATOMIC_FMAX      0x1f

#define MIMG_IMAGE_SAMPLE           0x20
#define MIMG_IMAGE_SAMPLE_CL        0x21
#define MIMG_IMAGE_SAMPLE_D         0x22
#define MIMG_IMAGE_SAMPLE_D_CL      0x23
#define MIMG_IMAGE_SAMPLE_L         0x24
#define MIMG_IMAGE_SAMPLE_B         0x25
#define MIMG_IMAGE_SAMPLE_B_CL      0x26
#define MIMG_IMAGE_SAMPLE_LZ        0x27
#define MIMG_IMAGE_SAMPLE_C         0x28
#define MIMG_IMAGE_SAMPLE_C_CL      0x29
#define MIMG_IMAGE_SAMPLE_C_D       0x2a
#define MIMG_IMAGE_SAMPLE_C_D_CL    0x2b
#define MIMG_IMAGE_SAMPLE_C_L       0x2c
#define MIMG_IMAGE_SAMPLE_C_B       0x2d
#define MIMG_IMAGE_SAMPLE_C_B_CL    0x2e
#define MIMG_IMAGE_SAMPLE_C_LZ      0x2f
#define MIMG_IMAGE_SAMPLE_O         0x30
#define MIMG_IMAGE_SAMPLE_CL_O      0x31
#define MIMG_IMAGE_SAMPLE_D_O       0x32
#define MIMG_IMAGE_SAMPLE_D_CL_O    0x33
#define MIMG_IMAGE_SAMPLE_L_O       0x34
#define MIMG_IMAGE_SAMPLE_B_O       0x35
#define MIMG_IMAGE_SAMPLE_B_CL_O    0x36
#define MIMG_IMAGE_SAMPLE_LZ_O      0x37
#define MIMG_IMAGE_SAMPLE_C_O       0x38
#define MIMG_IMAGE_SAMPLE_C_CL_O    0x39
#define MIMG_IMAGE_SAMPLE_C_D_O     0x3a
#define MIMG_IMAGE_SAMPLE_C_D_CL_O  0x3b
#define MIMG_IMAGE_SAMPLE_C_L_O     0x3c
#define MIMG_IMAGE_SAMPLE_C_B_O     0x3d
#define MIMG_IMAGE_SAMPLE_C_B_CL_O  0x3e
#define MIMG_IMAGE_SAMPLE_C_LZ_O    0x3f

#define MIMG_IMAGE_GATHER4          0x40
#define MIMG_IMAGE_GATHER4_CL       0x41
#define MIMG_IMAGE_GATHER4_L        0x44
#define MIMG_IMAGE_GATHER4_B        0x45
#define MIMG_IMAGE_GATHER4_B_CL     0x46
#define MIMG_IMAGE_GATHER4_LZ       0x47
#define MIMG_IMAGE_GATHER4_C        0x48
#define MIMG_IMAGE_GATHER4_C_CL     0x49
#define MIMG_IMAGE_GATHER4_C_L      0x4c
#define MIMG_IMAGE_GATHER4_C_B      0x4d
#define MIMG_IMAGE_GATHER4_C_B_CL   0x4e
#define MIMG_IMAGE_GATHER4_C_LZ     0x4f
#define MIMG_IMAGE_GATHER4_O        0x50
#define MIMG_IMAGE_GATHER4_CL_O     0x51
#define MIMG_IMAGE_GATHER4_L_O      0x54
#define MIMG_IMAGE_GATHER4_B_O      0x55
#define MIMG_IMAGE_GATHER4_B_CL_O   0x56
#define MIMG_IMAGE_GATHER4_LZ_O     0x57
#define MIMG_IMAGE_GATHER4_C_O      0x58
#define MIMG_IMAGE_GATHER4_C_CL_O   0x59
#define MIMG_IMAGE_GATHER4_C_L_O    0x5c
#define MIMG_IMAGE_GATHER4_C_B_O    0x5d
#define MIMG_IMAGE_GATHER4_C_B_CL_O 0x5e
#define MIMG_IMAGE_GATHER4_C_LZ_O   0x5f

#define MIMG_IMAGE_GET_LOD          0x60

#define MIMG_IMAGE_SAMPLE_CD        0x68
#define MIMG_IMAGE_SAMPLE_CD_CL     0x69
#define MIMG_IMAGE_SAMPLE_C_CD      0x6a
#define MIMG_IMAGE_SAMPLE_C_CD_CL   0x6b
#define MIMG_IMAGE_SAMPLE_CD_O      0x6c
#define MIMG_IMAGE_SAMPLE_CD_CL_O   0x6d
#define MIMG_IMAGE_SAMPLE_C_CD_O    0x6e
#define MIMG_IMAGE_SAMPLE_C_CD_CL_O 0x6f


 //DS
#define DS_DS_ADD_U32             0x00
#define DS_DS_SUB_U32             0x01
#define DS_DS_RSUB_U32            0x02
#define DS_DS_INC_U32             0x03
#define DS_DS_DEC_U32             0x04
#define DS_DS_MIN_I32             0x05
#define DS_DS_MAX_I32             0x06
#define DS_DS_MIN_U32             0x07
#define DS_DS_MAX_U32             0x08
#define DS_DS_AND_B32             0x09
#define DS_DS_OR_B32              0x0A
#define DS_DS_XOR_B32             0x0B
#define DS_DS_MSKOR_B32           0x0C
#define DS_DS_WRITE_B32           0x0D
#define DS_DS_WRITE2_B32          0x0E
#define DS_DS_WRITE2ST64_B32      0x0F
#define DS_DS_CMPST_B32           0x10
#define DS_DS_CMPST_F32           0x11
#define DS_DS_MIN_F32             0x12
#define DS_DS_MAX_F32             0x13
#define DS_DS_NOP                 0x14
#define DS_DS_GWS_INIT            0x19
#define DS_DS_GWS_SEMA_V          0x1A
#define DS_DS_GWS_SEMA_BR         0x1B
#define DS_DS_GWS_SEMA_P          0x1C
#define DS_DS_GWS_BARRIER         0x1D
#define DS_DS_WRITE_B8            0x1E
#define DS_DS_WRITE_B16           0x1F
#define DS_DS_ADD_RTN_U32         0x20
#define DS_DS_SUB_RTN_U32         0x21
#define DS_DS_RSUB_RTN_U32        0x22
#define DS_DS_INC_RTN_U32         0x23
#define DS_DS_DEC_RTN_U32         0x24
#define DS_DS_MIN_RTN_I32         0x25
#define DS_DS_MAX_RTN_I32         0x26
#define DS_DS_MIN_RTN_U32         0x27
#define DS_DS_MAX_RTN_U32         0x28
#define DS_DS_AND_RTN_B32         0x29
#define DS_DS_OR_RTN_B32          0x2A
#define DS_DS_XOR_RTN_B32         0x2B
#define DS_DS_MSKOR_RTN_B32       0x2C
#define DS_DS_WRXCHG_RTN_B32      0x2D
#define DS_DS_WRXCHG2_RTN_B32     0x2E
#define DS_DS_WRXCHG2ST64_RTN_B32 0x2F
#define DS_DS_CMPST_RTN_B32       0x30
#define DS_DS_CMPST_RTN_F32       0x31
#define DS_DS_MIN_RTN_F32         0x32
#define DS_DS_MAX_RTN_F32         0x33
#define DS_DS_WRAP_RTN_B32        0x34
#define DS_DS_SWIZZLE_B32         0x35
#define DS_DS_READ_B32            0x36
#define DS_DS_READ2_B32           0x37
#define DS_DS_READ2ST64_B32       0x38
#define DS_DS_READ_I8             0x39
#define DS_DS_READ_U8             0x3A
#define DS_DS_READ_I16            0x3B
#define DS_DS_READ_U16            0x3C
#define DS_DS_CONSUME             0x3D
#define DS_DS_APPEND              0x3E
#define DS_DS_ORDERED_COUNT       0x3F
#define DS_DS_ADD_U64             0x40
#define DS_DS_SUB_U64             0x41
#define DS_DS_RSUB_U64            0x42
#define DS_DS_INC_U64             0x43
#define DS_DS_DEC_U64             0x44
#define DS_DS_MIN_I64             0x45
#define DS_DS_MAX_I64             0x46
#define DS_DS_MIN_U64             0x47
#define DS_DS_MAX_U64             0x48
#define DS_DS_OR_B64              0x4A
#define DS_DS_XOR_B64             0x4B
#define DS_DS_MSKOR_B64           0x4C
#define DS_DS_WRITE_B64           0x4D
#define DS_DS_WRITE2_B64          0x4E
#define DS_DS_WRITE2ST64_B64      0x4F
#define DS_DS_CMPST_B64           0x50
#define DS_DS_CMPST_F64           0x51
#define DS_DS_MIN_F64             0x52
#define DS_DS_MAX_F64             0x53
#define DS_DS_ADD_RTN_U64         0x60
#define DS_DS_SUB_RTN_U64         0x61
#define DS_DS_RSUB_RTN_U64        0x62
#define DS_DS_INC_RTN_U64         0x63
#define DS_DS_DEC_RTN_U64         0x64
#define DS_DS_MIN_RTN_I64         0x65
#define DS_DS_MAX_RTN_I64         0x66
#define DS_DS_MIN_RTN_U64         0x67
#define DS_DS_MAX_RTN_U64         0x68
#define DS_DS_AND_RTN_B64         0x69
#define DS_DS_OR_RTN_B64          0x6A
#define DS_DS_XOR_RTN_B64         0x6B
#define DS_DS_MSKOR_RTN_B64       0x6C
#define DS_DS_WRXCHG_RTN_B64      0x6D
#define DS_DS_WRXCHG2_RTN_B64     0x6E
#define DS_DS_WRXCHG2ST64_RTN_B64 0x6F
#define DS_DS_CMPST_RTN_B64       0x70
#define DS_DS_CMPST_RTN_F64       0x71
#define DS_DS_MIN_RTN_F64         0x72
#define DS_DS_MAX_RTN_F64         0x73
#define DS_DS_READ_B64            0x76
#define DS_DS_READ2_B64           0x77
#define DS_DS_READ2ST64_B64       0x78
#define DS_DS_CONDXCHG32_RTN_B64  0x7E
#define DS_DS_ADD_SRC2_U32        0x80
#define DS_DS_SUB_SRC2_U32        0x81
#define DS_DS_RSUB_SRC2_U32       0x82
#define DS_DS_INC_SRC2_U32B       0x83
#define DS_DS_DEC_SRC2_U32        0x84
#define DS_DS_MIN_SRC2_I32        0x85
#define DS_DS_MAX_SRC2_I32        0x86
#define DS_DS_MIN_SRC2_U32        0x87
#define DS_DS_MAX_SRC2_U32        0x88
#define DS_DS_AND_SRC2_B32B       0x89
#define DS_DS_OR_SRC2_B32         0x8A
#define DS_DS_XOR_SRC2_B32        0x8B
#define DS_DS_WRITE_SRC2_B32      0x8C
#define DS_DS_MIN_SRC2_F32        0x92
#define DS_DS_MAX_SRC2_F32        0x93
#define DS_DS_ADD_SRC2_U64        0xC0
#define DS_DS_SUB_SRC2_U64        0xC1
#define DS_DS_RSUB_SRC2_U64       0xC2
#define DS_DS_INC_SRC2_U64        0xC3
#define DS_DS_DEC_SRC2_U64        0xC4
#define DS_DS_MIN_SRC2_I64        0xC5
#define DS_DS_MAX_SRC2_I64        0xC6
#define DS_DS_MIN_SRC2_U64        0xC7
#define DS_DS_MAX_SRC2_U64        0xC8
#define DS_DS_AND_SRC2_B64        0xC9
#define DS_DS_OR_SRC2_B64         0xCA
#define DS_DS_XOR_SRC2_B64        0xCB
#define DS_DS_MIN_SRC2_F64        0xD2
#define DS_DS_MAX_SRC2_F64        0xD3

