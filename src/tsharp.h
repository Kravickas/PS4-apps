#pragma once

#define VMemoryTypePV 0x60 // < Private         
#define VMemoryTypeGC 0x6D // < GPU Coherent    
#define VMemoryTypeSC 0x6E // < System Coherent 
#define VMemoryTypeUC 0x6F // < Uncached        
#define VMemoryTypeRO 0x10 // < Read Only    

#define DSEL_0 0
#define DSEL_1 1
#define DSEL_R 4
#define DSEL_G 5
#define DSEL_B 6
#define DSEL_A 7

// SQ_RSRC_IMG_TYPE
#define SQ_RSRC_IMG_1D            0x8
#define SQ_RSRC_IMG_2D            0x9
#define SQ_RSRC_IMG_3D            0xa
#define SQ_RSRC_IMG_CUBE          0xb
#define SQ_RSRC_IMG_1D_ARRAY      0xc
#define SQ_RSRC_IMG_2D_ARRAY      0xd
#define SQ_RSRC_IMG_2D_MSAA       0xe
#define SQ_RSRC_IMG_2D_MSAA_ARRAY 0xf

// IMG_DATA_FORMAT
#define IMG_DATA_FORMAT_INVALID           0x00
#define IMG_DATA_FORMAT_8                 0x01
#define IMG_DATA_FORMAT_16                0x02
#define IMG_DATA_FORMAT_8_8               0x03
#define IMG_DATA_FORMAT_32                0x04
#define IMG_DATA_FORMAT_16_16             0x05
#define IMG_DATA_FORMAT_10_11_11          0x06
#define IMG_DATA_FORMAT_11_11_10          0x07
#define IMG_DATA_FORMAT_10_10_10_2        0x08
#define IMG_DATA_FORMAT_2_10_10_10        0x09
#define IMG_DATA_FORMAT_8_8_8_8           0x0a
#define IMG_DATA_FORMAT_32_32             0x0b
#define IMG_DATA_FORMAT_16_16_16_16       0x0c
#define IMG_DATA_FORMAT_32_32_32          0x0d
#define IMG_DATA_FORMAT_32_32_32_32       0x0e
#define IMG_DATA_FORMAT_RESERVED_15       0x0f
#define IMG_DATA_FORMAT_5_6_5             0x10
#define IMG_DATA_FORMAT_1_5_5_5           0x11
#define IMG_DATA_FORMAT_5_5_5_1           0x12
#define IMG_DATA_FORMAT_4_4_4_4           0x13
#define IMG_DATA_FORMAT_8_24              0x14
#define IMG_DATA_FORMAT_24_8              0x15
#define IMG_DATA_FORMAT_X24_8_32          0x16
#define IMG_DATA_FORMAT_RESERVED_23       0x17
#define IMG_DATA_FORMAT_RESERVED_24       0x18
#define IMG_DATA_FORMAT_ETC2_RGB          0x18
#define IMG_DATA_FORMAT_RESERVED_25       0x19
#define IMG_DATA_FORMAT_ETC2_RGBA         0x19
#define IMG_DATA_FORMAT_RESERVED_26       0x1a
#define IMG_DATA_FORMAT_ETC2_R            0x1a
#define IMG_DATA_FORMAT_RESERVED_27       0x1b
#define IMG_DATA_FORMAT_ETC2_RG           0x1b
#define IMG_DATA_FORMAT_RESERVED_28       0x1c
#define IMG_DATA_FORMAT_ETC2_RGBA1        0x1c
#define IMG_DATA_FORMAT_RESERVED_29       0x1d
#define IMG_DATA_FORMAT_RESERVED_30       0x1e
#define IMG_DATA_FORMAT_RESERVED_31       0x1f
#define IMG_DATA_FORMAT_GB_GR             0x20
#define IMG_DATA_FORMAT_BG_RG             0x21
#define IMG_DATA_FORMAT_5_9_9_9           0x22
#define IMG_DATA_FORMAT_BC1               0x23
#define IMG_DATA_FORMAT_BC2               0x24
#define IMG_DATA_FORMAT_BC3               0x25
#define IMG_DATA_FORMAT_BC4               0x26
#define IMG_DATA_FORMAT_BC5               0x27
#define IMG_DATA_FORMAT_BC6               0x28
#define IMG_DATA_FORMAT_BC7               0x29
#define IMG_DATA_FORMAT_RESERVED_42       0x2a
#define IMG_DATA_FORMAT_RESERVED_43       0x2b
#define IMG_DATA_FORMAT_FMASK8_S2_F1      0x2c
#define IMG_DATA_FORMAT_FMASK8_S4_F1      0x2d
#define IMG_DATA_FORMAT_FMASK8_S8_F1      0x2e
#define IMG_DATA_FORMAT_FMASK8_S2_F2      0x2f
#define IMG_DATA_FORMAT_FMASK8_S4_F2      0x30
#define IMG_DATA_FORMAT_FMASK8_S4_F4      0x31
#define IMG_DATA_FORMAT_FMASK16_S16_F1    0x32
#define IMG_DATA_FORMAT_FMASK16_S8_F2     0x33
#define IMG_DATA_FORMAT_FMASK32_S16_F2    0x34
#define IMG_DATA_FORMAT_FMASK32_S8_F4     0x35
#define IMG_DATA_FORMAT_FMASK32_S8_F8     0x36
#define IMG_DATA_FORMAT_FMASK64_S16_F4    0x37
#define IMG_DATA_FORMAT_FMASK64_S16_F8    0x38
#define IMG_DATA_FORMAT_4_4               0x39
#define IMG_DATA_FORMAT_6_5_5             0x3a
#define IMG_DATA_FORMAT_1                 0x3b
#define IMG_DATA_FORMAT_1_REVERSED        0x3c
#define IMG_DATA_FORMAT_32_AS_8           0x3d
#define IMG_DATA_FORMAT_32_AS_8_8         0x3e
#define IMG_DATA_FORMAT_32_AS_32_32_32_32 0x3f
// IMG_NUM_FORMAT
#define IMG_NUM_FORMAT_UNORM       0x0
#define IMG_NUM_FORMAT_SNORM       0x1
#define IMG_NUM_FORMAT_USCALED     0x2
#define IMG_NUM_FORMAT_SSCALED     0x3
#define IMG_NUM_FORMAT_UINT        0x4
#define IMG_NUM_FORMAT_SINT        0x5
#define IMG_NUM_FORMAT_RESERVED_6  0x6
#define IMG_NUM_FORMAT_FLOAT       0x7
#define IMG_NUM_FORMAT_RESERVED_8  0x8
#define IMG_NUM_FORMAT_SRGB        0x9
#define IMG_NUM_FORMAT_RESERVED_10 0xa
#define IMG_NUM_FORMAT_RESERVED_11 0xb
#define IMG_NUM_FORMAT_RESERVED_12 0xc
#define IMG_NUM_FORMAT_RESERVED_13 0xd
#define IMG_NUM_FORMAT_RESERVED_14 0xe
#define IMG_NUM_FORMAT_RESERVED_15 0xf

struct TSharpResource8 {
    uint base : 32;
    uint unused : 6;
    uint mtype_L2 : 2;
    uint min_lod : 12; //fixed point 4.8 minimum LOD (0.0..15.0)
    uint dfmt : 6;     //texture data format; num components, num bits
    uint nfmt : 4;     //texture numeric format; value conversion
    uint mtype_L1L : 2;
    //64
    uint width : 14;      //texture width (0..16383)
    uint height : 14;     //texture height (0..16383)
    uint perf_mod : 3;    //0=0/16, 1=2/16, 2=5/16, 3=7/16, 4=9/16, 5=11/16, 6=14/16, 7=16/16
    uint interlaced : 1;  //texture is interlaced
    //32
    uint dst_sel_x : 3;  //Destination channel select:
    uint dst_sel_y : 3;  //0=0, 1=1, 2=0, 3=1, 4=R, 5=G, 6=B, 7=A
    uint dst_sel_z : 3;
    uint dst_sel_w : 3;
    uint base_level : 4;  //first mip level (0..15)
    uint last_level : 4;  //last mip level (0..15); for msaa, number of samples
    uint tiling_idx : 5;  //index into lookup table of surface tiling settings
    uint pow2pad : 1;     //memory footprint is padded to power of 2 dimensions
    uint mtype_L1M : 1;
    uint reserved : 1;
    uint _type : 4;  //values [8..15] are 1D, 2D, 3D, Cube, 1D array, 2D array, 2D MSAA, 2D MSAA array; 0 is V#, 1-7 reserved
    //32
    uint depth : 13; //3D texture depth (0..8192)
    uint pitch : 14; //texture pitch in texels (0..16383); defaults to width
    uint reserved2 : 5;
    //32
    uint base_array : 13; //first array index (0..16383)
    uint last_array : 13; //texture height (0..16383)
    uint reserved3 : 6;
    //64
    uint min_lod_warn : 12;    //min mip level to trigger LWE (LOD warn enable); unsigned fixed point 4.8
    uint counter_bank_id : 8;  //PRT counter ID
    uint LOD_hdw_cnt_en : 1;   //PRT hardware counter enable
    //NEO mode only
    uint compression_en : 1;   //Indicates whether the texture is bandwidth-compressed by using a metadata buffer to avoid reading redundant cache lines.
    uint alpha_is_on_msb : 1;  //Specifies that DCC compression is to consider the alpha channel to be in the most significant bits of each texel.
    uint color_transform : 1;  //This governs whether the red and blue channels are de-correlated from the green channel.
    uint alt_tile_mode : 1;    //This indicates that the surface is tiled for NEO mode, and is incompatible with base mode.
    //
    uint reserved4 : 39;

    void Init2D(int dfmt,int nfmt,int tiling_idx,int width,int height,int mips = 1) {

        this->dfmt = dfmt;
        this->nfmt = nfmt;
        this->tiling_idx = tiling_idx;

        this->width  = width - 1;
        this->height = height - 1;
        this->last_level = mips - 1;
        
        if (mips > 1) this->pow2pad = 1;

        this->perf_mod = 7;

        this->_type = SQ_RSRC_IMG_2D;

        setChannelOrder(DSEL_R, DSEL_G, DSEL_B, DSEL_A);

        setVMemoryType(VMemoryTypePV);

    };

    int getVMemoryType() {
        return (this->mtype_L1M << 4) |
            (this->mtype_L1L << 2) |
            (this->mtype_L2);
    };

    void setVMemoryType(int m) {
        this->mtype_L1M = (m >> 4);
        this->mtype_L1L = (m >> 2);
        this->mtype_L2  = m;
    };

    void setVMemoryPtr(void* ptr) {
        this->base = ((uintptr_t)ptr) >> 8;
    };

    void setChannelOrder(uint x, uint y, uint z, uint w) {
        this->dst_sel_x = x;
        this->dst_sel_y = y;
        this->dst_sel_z = z;
        this->dst_sel_w = w;
    };


};
                              
