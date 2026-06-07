#pragma once

#include <orbis/libkernel.h>

class Frame
{
public:
    int width;
    int height;
    int pitch;
    int pad_width;
    int pad_height;
    off_t size;
    off_t dmem_ofs;
    void* dmem_ptr;
public:
    Frame(int w, int h, int neo_mode)
    {
        this->width = w;
        this->height = h;

        this->pitch = (width + 127) / 128;
        this->pad_width = this->pitch * 128;

        if (neo_mode != 0)
        {
            this->pad_height = ((height + 127) & (~127));
        }
        else
        {
            this->pad_height = ((height + 63) & (~63));
        }

        this->size = this->pad_width * this->pad_height * 4;
    };

    int alloc() {

        int r;

        off_t aligned_size = (this->size + 16 * 1024 - 1) & ~(16 * 1024 - 1);

        this->dmem_ofs = 0;

        r = sceKernelAllocateMainDirectMemory(
            aligned_size,
            64 * 1024,      //VideoOut need 64KB align
            WC_GARLIC,
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
            64 * 1024); //VideoOut need 64KB align

        printf("sceKernelMapDirectMemory->%d\n", r);

        if (r != 0) {

            sceKernelReleaseDirectMemory(this->dmem_ofs, aligned_size);
            this->dmem_ofs = 0;

            return r;
        }

        return 0;
    };

    OrbisVideoOutBufferAttribute get_attr() {
        return { (int)PIXEL_FORMAT_ABGR8,
                 TILING_MODE_TILE,
                 0,
                 (uint)this->width,
                 (uint)this->height,
                 (uint)this->width,
        };
    };

};

