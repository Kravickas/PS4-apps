#pragma once
#include <stdint.h>
#include <string.h>

// ============================================================================
// PM4 Packet Types and Opcodes for AMD GCN (PS4 Liverpool GPU)
// ============================================================================

// PM4 Type 3 header: [31:30]=type(3), [15:8]=opcode, [29:16]=count-1, [7:0]=predicate
static inline uint32_t PM4_HDR(uint32_t opcode, uint32_t count) {
    // type3 = 3 << 30, count is N-1 dwords after header
    return (3u << 30) | ((count - 1u) << 16) | ((opcode & 0xFF) << 8);
}

// Type 2 NOP padding
static inline uint32_t PM4_TYPE2_NOP() {
    return 0x80000000u;
}

// PM4 Opcodes
enum PM4Opcode : uint32_t {
    IT_NOP                    = 0x10,
    IT_SET_BASE               = 0x11,
    IT_INDEX_BASE             = 0x12,
    IT_CLEAR_STATE            = 0x12,
    IT_INDEX_BUFFER_SIZE      = 0x13,
    IT_DISPATCH_DIRECT        = 0x15,
    IT_DISPATCH_INDIRECT      = 0x16,
    IT_SET_PREDICATION        = 0x20,
    IT_COND_EXEC              = 0x22,
    IT_INDEX_TYPE             = 0x2A,
    IT_DRAW_INDEX_2           = 0x27,
    IT_CONTEXT_CONTROL        = 0x28,
    IT_DRAW_INDEX_AUTO        = 0x2D,
    IT_NUM_INSTANCES          = 0x2F,
    IT_INDIRECT_BUFFER        = 0x3F,
    IT_STRMOUT_BUFFER_UPDATE  = 0x34,
    IT_MEM_SEMAPHORE          = 0x39,
    IT_WRITE_DATA             = 0x37,
    IT_EVENT_WRITE            = 0x46,
    IT_EVENT_WRITE_EOP        = 0x47,
    IT_EVENT_WRITE_EOS        = 0x48,
    IT_RELEASE_MEM            = 0x49,
    IT_DMA_DATA               = 0x50,
    IT_ACQUIRE_MEM            = 0x58,
    IT_REWIND                 = 0x59,
    IT_SET_CONFIG_REG         = 0x68,
    IT_SET_CONTEXT_REG        = 0x69,
    IT_SET_SH_REG             = 0x76,
    IT_SET_UCONFIG_REG        = 0x79,
    IT_SET_QUEUE_REG          = 0x78,
    IT_INCREMENT_DE_COUNTER   = 0x85,
    IT_WAIT_ON_CE_COUNTER     = 0x86,
    IT_INCREMENT_CE_COUNTER   = 0x84,
    IT_WAIT_ON_DE_COUNTER_DIFF= 0x88,
    IT_WRITE_CONST_RAM        = 0x81,
    IT_DUMP_CONST_RAM         = 0x83,
    IT_INDIRECT_BUFFER_CONST  = 0x43,
    IT_WAIT_REG_MEM           = 0x3C,
    IT_COPY_DATA              = 0x40,
    IT_PFP_SYNC_ME            = 0x42,
    IT_DISPATCH_DIRECT_MEC    = 0x15,
};

// ============================================================================
// PM4 Packet Emitters — append dwords to a command buffer
// ============================================================================

struct CmdBuffer {
    uint32_t* buf;
    uint32_t  offset;     // current write position in dwords
    uint32_t  capacity;   // max dwords

    void init(uint32_t* storage, uint32_t cap_dwords) {
        buf = storage;
        offset = 0;
        capacity = cap_dwords;
        memset(buf, 0, cap_dwords * sizeof(uint32_t));
    }

    void emit(uint32_t dw) {
        if (offset < capacity) buf[offset++] = dw;
    }

    uint32_t sizeBytes() const { return offset * sizeof(uint32_t); }
    uint32_t sizeDwords() const { return offset; }
};

// ---------------------------------------------------------------------------
// NOP with optional payload
// ---------------------------------------------------------------------------
static inline void pm4_nop(CmdBuffer& cb, uint32_t count = 1) {
    cb.emit(PM4_HDR(IT_NOP, count));
    for (uint32_t i = 0; i < count; i++) cb.emit(0); // count payload dwords
}

// ---------------------------------------------------------------------------
// WRITE_DATA — write immediate data to a memory address
//   dst_sel: 2 = memory sync, 5 = memory async
// ---------------------------------------------------------------------------
static inline void pm4_write_data(CmdBuffer& cb, void* address, const uint32_t* data, uint32_t num_dwords) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    uint32_t count = 2 + num_dwords; // header body: dst_sel, addr_lo, addr_hi, data...
    cb.emit(PM4_HDR(IT_WRITE_DATA, count + 1));
    // dw1: dst_sel=5 (memory async), wr_confirm=1
    cb.emit((5u << 8) | (1u << 20));
    cb.emit((uint32_t)(addr & 0xFFFFFFFF));
    cb.emit((uint32_t)(addr >> 32));
    for (uint32_t i = 0; i < num_dwords; i++) cb.emit(data[i]);
}

// Convenience: write a single u32
static inline void pm4_write_data_u32(CmdBuffer& cb, volatile uint32_t* address, uint32_t value) {
    pm4_write_data(cb, (void*)address, &value, 1);
}

// Convenience: write a single u64
static inline void pm4_write_data_u64(CmdBuffer& cb, volatile uint64_t* address, uint64_t value) {
    uint32_t data[2] = { (uint32_t)(value), (uint32_t)(value >> 32) };
    pm4_write_data(cb, (void*)address, data, 2);
}

// ---------------------------------------------------------------------------
// EVENT_WRITE_EOP — end-of-pipe event with fence write
//   Writes `data` to `address` after all prior work completes (on real HW).
//   data_sel: 1=32bit low, 2=64bit, 3=gpu_clock64
//   int_sel:  0=none, 2=irq_when_write_confirm
// ---------------------------------------------------------------------------
static inline void pm4_event_write_eop(CmdBuffer& cb, void* address, uint64_t data,
                                        uint32_t data_sel = 2, uint32_t int_sel = 0) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    // event_type = CACHE_FLUSH_AND_INV_TS_EVENT (0x14), event_index = 5 (EOP timestamp)
    uint32_t dw1 = (0x14u) | (5u << 8);
    uint32_t dw2 = (data_sel << 29) | (int_sel << 24);
    cb.emit(PM4_HDR(IT_EVENT_WRITE_EOP, 5));
    cb.emit(dw1);                              // event control
    cb.emit((uint32_t)(addr & 0xFFFFFFFFu));   // address lo
    cb.emit((uint32_t)(addr >> 32) | dw2);     // address hi + data/int sel
    cb.emit((uint32_t)(data));                  // data lo
    cb.emit((uint32_t)(data >> 32));            // data hi
}

// ---------------------------------------------------------------------------
// RELEASE_MEM — compute-queue fence write
// ---------------------------------------------------------------------------
static inline void pm4_release_mem(CmdBuffer& cb, void* address, uint64_t data,
                                    uint32_t data_sel = 2, uint32_t int_sel = 0) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    uint32_t dw1 = (0x14u) | (5u << 8); // CACHE_FLUSH_AND_INV_TS_EVENT, event_index=5
    uint32_t dw2 = (data_sel << 29) | (int_sel << 24);
    cb.emit(PM4_HDR(IT_RELEASE_MEM, 6));
    cb.emit(dw1);
    cb.emit(dw2);
    cb.emit((uint32_t)(addr & 0xFFFFFFFFu));
    cb.emit((uint32_t)(addr >> 32));
    cb.emit((uint32_t)(data));
    cb.emit((uint32_t)(data >> 32));
}

// ---------------------------------------------------------------------------
// WAIT_REG_MEM — poll memory until condition is met
//   func: 3=equal, 5=greater_equal, 6=greater
//   mem_space: 1=memory
// ---------------------------------------------------------------------------
static inline void pm4_wait_reg_mem(CmdBuffer& cb, volatile void* address,
                                     uint32_t ref, uint32_t mask, uint32_t func = 5) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    cb.emit(PM4_HDR(IT_WAIT_REG_MEM, 6));
    // func | mem_space=1 (memory) | engine=0 (ME)
    cb.emit(func | (1u << 4));
    cb.emit((uint32_t)(addr & 0xFFFFFFFFu));
    cb.emit((uint32_t)(addr >> 32));
    cb.emit(ref);
    cb.emit(mask);
    cb.emit(10); // poll interval
}

// ---------------------------------------------------------------------------
// MEM_SEMAPHORE — signal or wait on a memory-based semaphore
//   Signal: increments *address
//   Wait:   spins until *address > 0, then decrements
// ---------------------------------------------------------------------------
static inline void pm4_mem_semaphore_signal(CmdBuffer& cb, volatile void* address) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    cb.emit(PM4_HDR(IT_MEM_SEMAPHORE, 2));
    cb.emit((uint32_t)((addr >> 3) << 3));           // addr_lo [31:3]
    uint32_t dw2 = ((uint32_t)(addr >> 32) & 0xFF);  // addr_hi
    dw2 |= (6u << 29);  // sem_sel = SignalSemaphore
    dw2 |= (0u << 20);  // signal_type = Increment
    cb.emit(dw2);
}

static inline void pm4_mem_semaphore_wait(CmdBuffer& cb, volatile void* address) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    cb.emit(PM4_HDR(IT_MEM_SEMAPHORE, 2));
    cb.emit((uint32_t)((addr >> 3) << 3));
    uint32_t dw2 = ((uint32_t)(addr >> 32) & 0xFF);
    dw2 |= (7u << 29);  // sem_sel = WaitSemaphore
    cb.emit(dw2);
}

// ---------------------------------------------------------------------------
// DMA_DATA — GPU-side memory fill or copy
//   src_sel: 0=data (immediate), 1=memory, 3=memory_l2
//   dst_sel: 0=memory, 1=gds, 3=memory_l2
// ---------------------------------------------------------------------------
static inline void pm4_dma_data_fill(CmdBuffer& cb, void* dst, uint32_t value, uint32_t num_bytes) {
    uint64_t daddr = (uint64_t)(uintptr_t)dst;
    cb.emit(PM4_HDR(IT_DMA_DATA, 6));
    // dw1: src_sel=2 (Data/immediate), dst_sel=0 (memory), cp_sync=1
    cb.emit((2u << 29) | (0u << 20) | (1u << 31));
    cb.emit(value);                               // src_addr_lo = data value
    cb.emit(0);                                   // src_addr_hi (unused for Data)
    cb.emit((uint32_t)(daddr & 0xFFFFFFFF));      // dst_addr_lo
    cb.emit((uint32_t)(daddr >> 32));              // dst_addr_hi
    cb.emit(num_bytes);                           // command: byte count [20:0]
}

static inline void pm4_dma_data_copy(CmdBuffer& cb, void* dst, void* src, uint32_t num_bytes) {
    uint64_t saddr = (uint64_t)(uintptr_t)src;
    uint64_t daddr = (uint64_t)(uintptr_t)dst;
    cb.emit(PM4_HDR(IT_DMA_DATA, 6));
    // dw1: src_sel=3 (MemoryUsingL2), dst_sel=3 (MemoryUsingL2), cp_sync=1
    cb.emit((3u << 29) | (3u << 20) | (1u << 31));
    cb.emit((uint32_t)(saddr & 0xFFFFFFFF));   // src_addr_lo
    cb.emit((uint32_t)(saddr >> 32));           // src_addr_hi
    cb.emit((uint32_t)(daddr & 0xFFFFFFFF));    // dst_addr_lo
    cb.emit((uint32_t)(daddr >> 32));           // dst_addr_hi
    cb.emit(num_bytes);                         // command: byte count [20:0]
}

// ---------------------------------------------------------------------------
// ACQUIRE_MEM — invalidate GPU caches (should be barrier on real HW)
// ---------------------------------------------------------------------------
static inline void pm4_acquire_mem(CmdBuffer& cb) {
    cb.emit(PM4_HDR(IT_ACQUIRE_MEM, 6));
    cb.emit(0x02800000); // cp_coher_cntl: TC|SH action ena
    cb.emit(0xFFFFFFFF); // cp_coher_size_lo
    cb.emit(0x000000FF); // cp_coher_size_hi
    cb.emit(0);          // cp_coher_base_lo
    cb.emit(0);          // cp_coher_base_hi
    cb.emit(10);         // poll interval
}

// ---------------------------------------------------------------------------
// PFP_SYNC_ME — PFP catches up to ME (CpSync in shadps4)
// ---------------------------------------------------------------------------
static inline void pm4_pfp_sync_me(CmdBuffer& cb) {
    cb.emit(PM4_HDR(IT_PFP_SYNC_ME, 1));
    cb.emit(0);
}

// ---------------------------------------------------------------------------
// WRITE_CONST_RAM — CE writes to constant heap
// ---------------------------------------------------------------------------
static inline void pm4_write_const_ram(CmdBuffer& cb, uint32_t offset_bytes,
                                        const uint32_t* data, uint32_t num_dwords) {
    cb.emit(PM4_HDR(IT_WRITE_CONST_RAM, num_dwords + 1));
    cb.emit(offset_bytes);
    for (uint32_t i = 0; i < num_dwords; i++) cb.emit(data[i]);
}

// ---------------------------------------------------------------------------
// DUMP_CONST_RAM — CE dumps constant heap to memory
// ---------------------------------------------------------------------------
static inline void pm4_dump_const_ram(CmdBuffer& cb, void* address,
                                       uint32_t offset_bytes, uint32_t num_dwords) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    cb.emit(PM4_HDR(IT_DUMP_CONST_RAM, 4));  // 4 body dwords: offset, size, addr_lo, addr_hi
    cb.emit(offset_bytes);
    cb.emit(num_dwords);
    cb.emit((uint32_t)(addr & 0xFFFFFFFF));
    cb.emit((uint32_t)(addr >> 32));
}

// ---------------------------------------------------------------------------
// INCREMENT_CE_COUNTER / INCREMENT_DE_COUNTER / WAIT_ON_CE_COUNTER
// ---------------------------------------------------------------------------
static inline void pm4_increment_ce_counter(CmdBuffer& cb) {
    cb.emit(PM4_HDR(IT_INCREMENT_CE_COUNTER, 1));
    cb.emit(0);
}
static inline void pm4_increment_de_counter(CmdBuffer& cb) {
    cb.emit(PM4_HDR(IT_INCREMENT_DE_COUNTER, 1));
    cb.emit(0);
}
static inline void pm4_wait_on_ce_counter(CmdBuffer& cb) {
    cb.emit(PM4_HDR(IT_WAIT_ON_CE_COUNTER, 1));
    cb.emit(0);
}

// ---------------------------------------------------------------------------
// CONTEXT_CONTROL — needed at start of DCB
// ---------------------------------------------------------------------------
static inline void pm4_context_control(CmdBuffer& cb) {
    cb.emit(PM4_HDR(IT_CONTEXT_CONTROL, 2));
    cb.emit(0x80000000);  // LOAD_ENABLE
    cb.emit(0x80000000);  // SHADOW_ENABLE
}

// ---------------------------------------------------------------------------
// EVENT_WRITE_EOS — end-of-shader event with fence or GDS store
//   command: 2 = SignalFence, 1 = GdsStore
// ---------------------------------------------------------------------------
static inline void pm4_event_write_eos_fence(CmdBuffer& cb, void* address, uint32_t data) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    uint32_t dw1 = (0x04u) | (4u << 8); // CS_DONE, event_index=4
    uint32_t cmd_info = ((uint32_t)(addr >> 32) & 0xFFFF) | (2u << 29); // command=SignalFence
    cb.emit(PM4_HDR(IT_EVENT_WRITE_EOS, 4));
    cb.emit(dw1);
    cb.emit((uint32_t)(addr & 0xFFFFFFFFu));
    cb.emit(cmd_info);
    cb.emit(data);
}

// ---------------------------------------------------------------------------
// COND_EXEC — conditionally skip N dwords if *address == 0
// ---------------------------------------------------------------------------
static inline void pm4_cond_exec(CmdBuffer& cb, volatile void* bool_address, uint32_t exec_count_dw) {
    uint64_t addr = (uint64_t)(uintptr_t)bool_address;
    cb.emit(PM4_HDR(IT_COND_EXEC, 3));
    cb.emit((uint32_t)((addr >> 2) << 2));          // bool_addr_lo [31:2]
    cb.emit((uint32_t)((addr >> 32) & 0xFFFF));     // bool_addr_hi [15:0]
    cb.emit(exec_count_dw & 0x3FFF);                // exec_count [13:0]
}

// ---------------------------------------------------------------------------
// INDIRECT_BUFFER — jump to secondary command buffer
// ---------------------------------------------------------------------------
static inline void pm4_indirect_buffer(CmdBuffer& cb, void* ib_address, uint32_t ib_size_dw) {
    uint64_t addr = (uint64_t)(uintptr_t)ib_address;
    cb.emit(PM4_HDR(IT_INDIRECT_BUFFER, 3));
    cb.emit((uint32_t)(addr & 0xFFFFFFFF));
    cb.emit((uint32_t)(addr >> 32) & 0xFFFF);
    cb.emit(ib_size_dw & 0xFFFFF);  // ib_size [19:0]
}

// ---------------------------------------------------------------------------
// EVENT_WRITE — ZpassDone for occlusion queries
// ---------------------------------------------------------------------------
static inline void pm4_event_write_zpass(CmdBuffer& cb, void* address) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    // event_type=PixelPipeStatDump(0x16), event_index=ZpassDone(1)
    cb.emit(PM4_HDR(IT_EVENT_WRITE, 3));
    cb.emit((0x16u) | (1u << 8));
    cb.emit((uint32_t)(addr & 0xFFFFFFFF));
    cb.emit((uint32_t)(addr >> 32));
}
