#pragma once

#include <orbis/libkernel.h>
#include "gnmapi.h"
#include "cmdbuf.h"


class ComputeQueue
{
public:
    int  vqueueId;
    uint globalPipeId;
    uint queueId;
    uint ringSizeInDW;
    off_t dmem_ofs;
    void* ringBaseAddr;
    ulong* readPtrAddr;
    uint* writePtrAddr;
public:
    ComputeQueue(uint globalPipeId, uint queueId)
    {
        this->globalPipeId = globalPipeId;
        this->queueId = queueId;
    };

    ~ComputeQueue() {
        off_t aligned_size = ((ringSizeInDW * 4) + 16 * 1024 - 1) & ~(16 * 1024 - 1);
        if (ringBaseAddr != NULL) sceKernelReleaseDirectMemory(this->dmem_ofs, aligned_size);
        if (vqueueId != 0) Gnm::sceGnmUnmapComputeQueue(vqueueId);
    }

    int Map(uint ringSizeInDW, void* readPtrAddr) {

        //1 << 8 ... 1 << 30

        //next power of two
        ringSizeInDW--;
        ringSizeInDW |= ringSizeInDW >> 1;
        ringSizeInDW |= ringSizeInDW >> 2;
        ringSizeInDW |= ringSizeInDW >> 4;
        ringSizeInDW |= ringSizeInDW >> 8;
        ringSizeInDW |= ringSizeInDW >> 16;
        ringSizeInDW++;

        //
        int r;

        off_t aligned_size = ((ringSizeInDW * 4) + 16 * 1024 - 1) & ~(16 * 1024 - 1);

        this->dmem_ofs = 0;

        r = sceKernelAllocateMainDirectMemory(
            (size_t)aligned_size,
            (size_t)16 * 1024,
            WB_ONION,
            &this->dmem_ofs);

        //printf("sceKernelAllocateMainDirectMemory->%d %d\n", r, this->dmem_ofs);

        if (r != 0) return r;

        void* ringBaseAddr = NULL;

        r = sceKernelMapDirectMemory(
            &ringBaseAddr,
            aligned_size,
            VM_PROT_READ | VM_PROT_WRITE | VM_PROT_GPU_ALL,
            0,
            this->dmem_ofs,
            16 * 1024);

        //printf("sceKernelMapDirectMemory->%d\n", r);

        if (r != 0) {

            sceKernelReleaseDirectMemory(this->dmem_ofs, aligned_size);
            this->dmem_ofs = 0;

            return r;
        }

        int vqueueId = Gnm::sceGnmMapComputeQueue(this->globalPipeId, this->queueId, ringBaseAddr, ringSizeInDW, readPtrAddr);

        if (vqueueId < 0) {
            sceKernelReleaseDirectMemory(this->dmem_ofs, aligned_size);
            return vqueueId;
        };

        this->ringBaseAddr = ringBaseAddr;
        this->writePtrAddr = (uint*)ringBaseAddr;
        this->readPtrAddr = (ulong*)readPtrAddr;
        this->ringSizeInDW = ringSizeInDW;
        this->vqueueId = vqueueId;

        return 0;
    };

    void incWritePtr(uint SizeInDW) {

        uint pos = this->writePtrAddr - (uint*)this->ringBaseAddr;

        pos = (pos + SizeInDW) & (this->ringSizeInDW - 1);

        this->writePtrAddr = (uint*)this->ringBaseAddr + pos;
    };

    void callCommandBuffer(void* cbBaseAddr, ulong cbSizeInDW) {

        uint* cmds = this->writePtrAddr;

        cmds[0] = 0xc0023f02;
        cmds[1] = (uint)(ulong)cbBaseAddr;
        cmds[2] = (uint)((ulong)cbBaseAddr >> 32) & 0xffff;
        cmds[3] = (uint)cbSizeInDW & 0xfffff | 0x1800000;

        incWritePtr(4);
    };

    void callCommandBuffer(CmdBuf* cmd_buf) {

        void* addr = cmd_buf->dmem_ptr;
        uint  size = cmd_buf->get_stream_size() ;

        callCommandBuffer(addr, size >> 2);
    };

    void DingDong() {

        uint pos = this->writePtrAddr - (uint*)this->ringBaseAddr;

        Gnm::sceGnmDingDong(this->vqueueId, pos);

    };


}; //ComputeQueue

