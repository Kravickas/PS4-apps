/* TRIANGLE_TEST: red_prig's "OpenOrbis Gnm Triangle" test app (IMAGE_STORE_MIP_v3, BREW00091)
 * rebuilt on ShadCube4, with probes for each thing its frame depends on. Results: ShadCube4
 * triangle.log in the trace log's folder (the first of trace_init's folders that takes a file).
 *
 * Steps, in this order (one state entry each; see the end of this comment):
 *   MEM      the app's memory sequence, run at the start of main() before ShadCube4 maps anything:
 *            posix mmap 16 KiB (MAP_ANON | MAP_PRIVATE), the 1920 x 1080 frame (0x7F8000 bytes,
 *            64 KiB aligned, WC_GARLIC, filled with 0xFF), 64 KiB of WB_ONION of which the second
 *            half is mapped at an address the kernel picks (16 KiB aligned), then the first half
 *            MAP_FIXED right after it. Logged: every address, the VirtualQuery of the fixed range
 *            before the fixed map, whether it lands on the frame, and the frame's first dwords
 *            through the frame's address before / after. The fixed map is only made when every
 *            page of its range is free or the frame's (never over ShadCube4's own memory).
 *            Before all of it: every mapped range from 0x200000000 up (VirtualQuery, find next),
 *            with its protection, type, flags and name.
 *   F_*      vertex fetch of a V# whose base is 0..3 bytes past a dword: per data format, base
 *            offsets 0..3, strides 20 / 22 / 21, 4 vertices, UINT. tri_vs_fetch stores what the
 *            fetch shader returned (shadPS4: vertex input) and what its own buffer_load_format of
 *            the same V# returned (shadPS4: buffer load). The data bytes are their own addresses
 *            (byte a = a), so every component shows the address it was read from. Per component:
 *            A read at the model address, E at the exact address (when that is not the model's),
 *            c the dst_sel constant, r a component past the format that reads the format's
 *            component (its index modulo the component count), z one that reads 0, o one that
 *            reads 1, u not written, X anything else. fetch==direct: both paths returned the same
 *            dwords (on the PS4 they are the same instruction). Model (the PS4, triangle-3: every
 *            case of both paths): the vertex's byte address (base + index * stride) with the bits
 *            below the smaller of the element size and 4 cleared (8.1.6 Alignment, Southern
 *            Islands ISA: "For Dword or larger reads or writes, the two LSBs of the byte-address
 *            are ignored"; RDNA ISA, Alignment: 1-byte formats 1, 2-byte formats 2, 4-byte and
 *            larger 4), components past the format r.
 *   F_RP     the app's two V#s exactly: position 32_32 FLOAT at vertices + 1 (dst X, Y, 0, 1),
 *            colour 32_32_32 FLOAT (dst X, Y, Z, 0), stride 20, 5 vertices.
 *   MC       components past the format per number format: 8, 16, 8_8, 32, 16_16, 32_32,
 *            32_32_32 with UNORM / SNORM / UINT / SINT / FLOAT (the valid ones), dst X Y Z W,
 *            base 0, stride 20. In-format components v (the value, not judged), past-format ones
 *            r / z / o / X as in F, judged on vertices 1..3 (vertex 0's bytes are 0).
 *   RAW      raw loads of a V# (stride 0) whose base is 0..3 bytes past a dword, at byte offset
 *            5 * vertex (tri_vs_raw): buffer_load_dword / ushort / ubyte / dwordx2 and
 *            s_buffer_load_dword at dword offset 1 and SGPR byte offset 6. The byte address each
 *            read came from (from the bytes it returned) against the exact one. Model, judged for
 *            dword and dwordx2 only (8.1.6): the two LSBs cleared.
 *   S_*      scissors and window offset on a 256 x 128 linear RGBA8 target: a clear draw, then one
 *            triangle over the viewport (tri_vs_full / tri_ps_cov) with the case's state. The
 *            pixel shader marks each pixel it runs for in a coverage map (POS_X / POS_Y), so
 *            every case is read twice: the target (CPU) and the map. Rectangles (TL inclusive,
 *            BR exclusive): screen 0,0-256,128, window 8,4-200,100, generic 24,12-240,120,
 *            viewport 4,20-230,110. Model (PA_SC_*_SCISSOR_TL.WINDOW_OFFSET_DISABLE, Sea Islands
 *            register reference: "If set, <scissor> is not offset by the WINDOW_OFFSET register
 *            values"; screen scissor "NOT affected by WINDOW_OFFSET"; PA_SU_SC_MODE_CNTL.
 *            VTX_WINDOW_OFFSET_ENABLE: "Enables addition of PA_SC_WINDOW_OFFSET values to vertex
 *            data"): every scissor and the viewport's pixels moved by the offset where enabled,
 *            intersected, clamped to the target.
 *              S_W / S_G / S_V  one restrictive scissor (the others open and not offset): its
 *                               WINDOW_OFFSET_DISABLE 0 / 1, offset (16, 8) / (-16, -8),
 *                               VTX_WINDOW_OFFSET_ENABLE 0 / 1
 *              S_A              all three restrictive, every WINDOW_OFFSET_DISABLE combination
 *                               (bits: window, generic, viewport), offset (16, 8), vertex
 *                               offset off and on
 *              S_RP             the app's state scaled to 256 x 128 (offset 16, 0; generic
 *                               0,16-256,128 offset; window 0,0-240,128 not offset; viewport
 *                               0,0-256,112 not offset), its PA_SU_HARDWARE_SCREEN_OFFSET 60, 32
 *                               and guard band (32, 60, 1, 1); also without them, and S_A 000
 *                               with them
 *   I_*      index fetch against a CPU write made after the submit: a WRITE_DATA marker, then
 *            WAIT_REG_MEM (memory, equal 1) on a label, then a point-list draw of `count` 16-bit
 *            indices from `offset` (tri_vs_index marks each index value it receives). Before the
 *            submit index i = i; once the marker shows the CP has reached the wait (and after an
 *            optional delay) the CPU writes 32768 + i to every index of the draw, then the label.
 * Logged per case: how many of the draw's indices were seen with the old value, the new one, both,
 * neither, and the runs. Cases: WAIT_REG_MEM ENGINE ME / PFP, DRAW_INDEX_OFFSET_2
 * (sceGnmDrawIndexOffset, INDEX_BASE) / DRAW_INDEX_2, ranges 26622+6144 (the app's), 32763+3,
 * 24574+8192, 16382+16384, 0+6144, delay 0 / 2 ms; I_RP: the app's case, only the last index
 * changed; I_P*: ME, DRAW_INDEX_OFFSET_2, start addresses 0x8000 / 0x8800 / 0x8FFE (16-bit) and
 * 0x4000 / 0x3FFC (32-bit indices); I_2DRAW: two draws after the wait (0+1024, then
 * 16384+12288). Page model (the PS4, triangle-3: every ME case old up to the same boundary): with
 * the wait in the ME the indices from the draw's start up to the end of the 16 KiB from the 4 KiB
 * page of its start are read before the wait ends (old), the rest after (new); with the wait in
 * the PFP all new. RP       the app's frame: its command buffer
 * (sceGnmDrawInitDefaultHardwareState350, its render target - here linear, the display buffer fb 0
 * -, viewport, scissors, window offset, hardware screen offset, guard band, the embedded rect
 * shaders, its triangle shaders and fetch shader byte for byte, the reversed-halves index buffer,
 * WAIT_REG_MEM and sceGnmDrawIndexOffset, the index changed after the submit) into fb 0, which was
 *            filled with 0xFF. Logged: the black and the coloured pixels' bounding boxes, rows and
 *            columns through them, and samples; then fb 0 is shown for TRIANGLE_SHOW_S seconds.
 *            The colours: its CB format (8_8_8_8 SRGB, COMP_SWAP ALT) on ShadCube4's A8R8G8B8
 *            display shows red as red (its own app used A8B8G8R8, which shows it blue).
 *
 * After a crash or a GPU hang close the app and launch it again: the step that was running is
 * logged as CRASHED / HUNG and skipped. TRIANGLE_BUILD changes restart the run (log truncated).
 * When all steps are done the run does not repeat until triangle.state is deleted; the RP frame
 * is still drawn and shown on every launch (unless it crashed). Needs, in shadPS4, the buffer and
 * linear image readbacks (the CPU reads GPU results). */
#include "ps_triangle.h"

#define TRIANGLE_BUILD "triangle-4"
#define TRIANGLE_TIMEOUT_US 2000000u
#define TRIANGLE_SHOW_S 10
#define TRIANGLE_DCB_DW 0x4000u
#define TRIANGLE_LOG_BUF 0x10000u
#define TRIANGLE_UNWRITTEN 0xA5A5A5A5u
#define TRIANGLE_RT_W 256
#define TRIANGLE_RT_H 128
#define TRIANGLE_RT_PX (TRIANGLE_RT_W * TRIANGLE_RT_H)
#define TRIANGLE_OPEN 16384
#define TRIANGLE_INDICES 32768u
#define TRIANGLE_NEW 32768u

extern void* mmap(void* addr, unsigned long len, int prot, int flags, int fd, long offset);
extern int munmap(void* addr, unsigned long len);
extern int sceKernelVirtualQuery(const void* addr, int flags, void* info, unsigned long size);
extern int sceKernelAllocateMainDirectMemory(unsigned long len, unsigned long align, int type,
                                             long* phys);
extern int sceKernelIsNeoMode(void);
extern int sceGnmDrawIndexAuto(uint32_t* cmd, uint32_t dw, uint32_t count, uint32_t modifier);
extern int sceGnmDrawIndexOffset(uint32_t* cmd, uint32_t dw, uint32_t offset, uint32_t count,
                                 uint32_t modifier);
extern int sceGnmSetVsShader(uint32_t* cmd, uint32_t dw, const void* regs, uint32_t modifier);
extern int sceGnmSetPsShader350(uint32_t* cmd, uint32_t dw, const void* regs);
extern int sceGnmSetEmbeddedVsShader(uint32_t* cmd, uint32_t dw, uint32_t id, uint32_t modifier);
extern int sceGnmSetEmbeddedPsShader(uint32_t* cmd, uint32_t dw, uint32_t id);

/* ---- log ---- */
static int g_tri_fd = -1;
static char g_tri_buf[TRIANGLE_LOG_BUF];
static unsigned g_tri_len;
static void tri_flush(void) {
    if (g_tri_fd >= 0 && g_tri_len) {
        sceKernelWrite(g_tri_fd, g_tri_buf, g_tri_len);
        sceKernelFsync(g_tri_fd);
    }
    g_tri_len = 0;
}
static void tri_s(const char* s) {
    while (*s) {
        if (g_tri_len >= TRIANGLE_LOG_BUF - 1)
            tri_flush();
        g_tri_buf[g_tri_len++] = *s++;
    }
}
static void tri_u(unsigned long long v) {
    char t[24];
    t[lg_u64(t, v)] = 0;
    tri_s(t);
}
static void tri_i(long long v) {
    char t[24];
    t[lg_i64(t, v)] = 0;
    tri_s(t);
}
static void tri_x(unsigned long long v, int digits) {
    static const char h[] = "0123456789abcdef";
    char t[20];
    int n = 0;
    t[n++] = '0';
    t[n++] = 'x';
    for (int i = digits - 1; i >= 0; i--)
        t[n++] = h[(v >> (i * 4)) & 0xf];
    t[n] = 0;
    tri_s(t);
}
static void tri_rect(const int* r) {
    if (r[0] >= r[2] || r[1] >= r[3]) {
        tri_s("empty");
        return;
    }
    tri_i(r[0]);
    tri_s(",");
    tri_i(r[1]);
    tri_s("-");
    tri_i(r[2]);
    tri_s(",");
    tri_i(r[3]);
}

/* ---- files: the first of trace_init's folders where a file can be created (shadPS4 has no
   /user/data; its trace goes to /data/ShadCube4/) ---- */
static char g_tri_log_path[96], g_tri_state_path[96], g_tri_mem_path[96];
static void tri_join(char* out, const char* dir, const char* name) {
    int n = 0;
    while (*dir && n < 94)
        out[n++] = *dir++;
    while (*name && n < 95)
        out[n++] = *name++;
    out[n] = 0;
}
static int tri_paths(void) {
    static const char* const dirs[] = {"/user/data/ShadCube4/", DATA_DIR_NEW, DATA_DIR_OLD, "",
                                       "/mnt/sandbox/SHAD00004/data/"};
    if (g_tri_state_path[0])
        return 1;
    sceKernelMkdir("/user/data/ShadCube4", 0777);
    for (int i = 0; i < (int)(sizeof(dirs) / sizeof(dirs[0])); i++) {
        char probe[96];
        tri_join(probe, dirs[i], "triangle.dir");
        int fd = sceKernelOpen(probe, 0x601, 0x1FF);
        if (fd < 0)
            continue;
        sceKernelClose(fd);
        tri_join(g_tri_log_path, dirs[i], "ShadCube4 triangle.log");
        tri_join(g_tri_state_path, dirs[i], "triangle.state");
        tri_join(g_tri_mem_path, dirs[i], "triangle.mem");
        return 1;
    }
    return 0;
}

/* ---- state ---- */
struct TriangleState {
    char build[32];
    int32_t next, inflight;
    int32_t mem; /* 0 not run, 1 running, 2 done (results in triangle.mem) */
    int32_t rp_bad;
};
static void tri_state_save(const struct TriangleState* st) {
    int fd = sceKernelOpen(g_tri_state_path, 0x601, 0x1FF);
    if (fd < 0)
        return;
    sceKernelWrite(fd, st, sizeof(*st));
    sceKernelFsync(fd);
    sceKernelClose(fd);
}
static int tri_streq(const char* a, const char* b) {
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}
static void tri_state_load(struct TriangleState* st) {
    my_memset(st, 0, sizeof(*st));
    int fd = sceKernelOpen(g_tri_state_path, 0, 0);
    long n = -1;
    if (fd >= 0) {
        n = sceKernelRead(fd, st, sizeof(*st));
        sceKernelClose(fd);
    }
    st->build[sizeof(st->build) - 1] = 0;
    if (n != (long)sizeof(*st) || !tri_streq(st->build, TRIANGLE_BUILD)) {
        my_memset(st, 0, sizeof(*st));
        for (int i = 0; TRIANGLE_BUILD[i]; i++)
            st->build[i] = TRIANGLE_BUILD[i];
        st->inflight = -1;
    }
}

/* ==== MEM: the app's memory sequence, at the start of main() ====================================
 */
#define TRI_VQ_MAX 24
#define TRI_VQ_END 0x204000000ull
struct TriangleMem {
    char build[32];
    int32_t r_mmap, r_fa, r_fm, r_ia, r_im, r_fix, fix_made;
    uint64_t a_mmap, f_phys, f_va, i_phys, i_va, target, fix_va;
    int32_t q_ret[2];
    uint64_t q_start[2], q_end[2], q_off[2];
    uint32_t q_prot[2], q_type[2];
    char q_class[2]; /* f free, F the frame, o other */
    uint32_t f_before[2][4], f_after[2][4], i_val[4];
    int32_t vq_n, vq_ret;
    uint64_t vq_start[TRI_VQ_MAX], vq_end[TRI_VQ_MAX];
    uint32_t vq_prot[TRI_VQ_MAX], vq_type[TRI_VQ_MAX], vq_flags[TRI_VQ_MAX];
    char vq_name[TRI_VQ_MAX][32];
};
static struct TriangleMem g_tri_mem;
/* Every mapped range from 0x200000000 up to TRI_VQ_END (VirtualQuery, SCE_KERNEL_VQ_FIND_NEXT). */
static void tri_mem_walk(void) {
    struct TriangleMem* m = &g_tri_mem;
    uint64_t a = 0x200000000ull;
    while (m->vq_n < TRI_VQ_MAX) {
        uint8_t info[0x48];
        my_memset(info, 0, sizeof(info));
        m->vq_ret = sceKernelVirtualQuery((const void*)(uintptr_t)a, 1, info, sizeof(info));
        if (m->vq_ret != 0)
            break;
        uint64_t start, end;
        my_memcpy(&start, info + 0x00, 8);
        my_memcpy(&end, info + 0x08, 8);
        if (start >= TRI_VQ_END || end <= a)
            break;
        int k = m->vq_n++;
        m->vq_start[k] = start;
        m->vq_end[k] = end;
        my_memcpy(&m->vq_prot[k], info + 0x18, 4);
        my_memcpy(&m->vq_type[k], info + 0x1C, 4);
        m->vq_flags[k] = info[0x20];
        my_memcpy(m->vq_name[k], info + 0x21, 31);
        m->vq_name[k][31] = 0;
        a = end;
    }
}
static void tri_mem_query(int k, uint64_t a) {
    struct TriangleMem* m = &g_tri_mem;
    uint8_t info[0x48];
    my_memset(info, 0, sizeof(info));
    m->q_ret[k] = sceKernelVirtualQuery((const void*)(uintptr_t)a, 0, info, sizeof(info));
    my_memcpy(&m->q_start[k], info + 0x00, 8);
    my_memcpy(&m->q_end[k], info + 0x08, 8);
    my_memcpy(&m->q_off[k], info + 0x10, 8);
    my_memcpy(&m->q_prot[k], info + 0x18, 4);
    my_memcpy(&m->q_type[k], info + 0x1C, 4);
    if (m->q_ret[k] != 0)
        m->q_class[k] = 'f';
    else if (m->q_start[k] >= m->f_va && m->q_end[k] <= m->f_va + 0x7F8000ull)
        m->q_class[k] = 'F';
    else
        m->q_class[k] = 'o';
}
static void triangle_mem_probe(void) {
    struct TriangleState st;
    if (!tri_paths())
        return; /* no folder takes a file: nothing could be logged */
    tri_state_load(&st);
    if (st.mem != 0)
        return; /* done, or it crashed (1): the log step reports it */
    st.mem = 1;
    tri_state_save(&st);
    struct TriangleMem* m = &g_tri_mem;
    my_memset(m, 0, sizeof(*m));
    for (int i = 0; TRIANGLE_BUILD[i]; i++)
        m->build[i] = TRIANGLE_BUILD[i];
    tri_mem_walk();
    void* a = mmap(0, 0x4000, 3, 0x1002, -1, 0);
    m->a_mmap = (uint64_t)(uintptr_t)a;
    m->r_mmap = (a == (void*)-1) ? -1 : 0;
    long fp = 0, ip = 0;
    void *f = 0, *iv = 0;
    m->r_fa = sceKernelAllocateMainDirectMemory(0x7F8000, 0x10000, 3, &fp);
    if (m->r_fa == 0)
        m->r_fm = sceKernelMapDirectMemory(&f, 0x7F8000, 0x33, 0, fp, 0x10000);
    m->f_phys = (uint64_t)fp;
    m->f_va = (uint64_t)(uintptr_t)f;
    m->r_ia = sceKernelAllocateMainDirectMemory(0x10000, 0x4000, 0, &ip);
    m->i_phys = (uint64_t)ip;
    if (m->r_fa == 0 && m->r_fm == 0) {
        my_memset(f, 0xFF, 0x7F8000);
        for (int k = 0; k < 4; k++) {
            m->f_before[0][k] = ((volatile uint32_t*)f)[k];
            m->f_before[1][k] = ((volatile uint32_t*)f)[0x1000 + k];
        }
    }
    if (m->r_ia == 0) {
        m->r_im = sceKernelMapDirectMemory(&iv, 0x8000, 0x33, 0, ip + 0x8000, 0x4000);
        m->i_va = (uint64_t)(uintptr_t)iv;
    }
    if (m->r_ia == 0 && m->r_im == 0 && m->r_fa == 0 && m->r_fm == 0) {
        my_memset(iv, 0, 0x8000);
        m->target = m->i_va + 0x8000;
        tri_mem_query(0, m->target);
        tri_mem_query(1, m->target + 0x4000);
        if (m->q_class[0] != 'o' && m->q_class[1] != 'o') {
            void* t = (void*)(uintptr_t)m->target;
            m->r_fix = sceKernelMapDirectMemory(&t, 0x8000, 0x33, 0x10, ip, 0x4000);
            m->fix_va = (uint64_t)(uintptr_t)t;
            m->fix_made = m->r_fix == 0;
            if (m->fix_made) {
                ((volatile uint32_t*)t)[0] = 0x11223344u;
                ((volatile uint32_t*)t)[0x1000] = 0x55667788u;
            }
        }
        for (int k = 0; k < 4; k++) {
            m->f_after[0][k] = ((volatile uint32_t*)f)[k];
            m->f_after[1][k] = ((volatile uint32_t*)f)[0x1000 + k];
            m->i_val[k] = ((volatile uint32_t*)iv)[k];
        }
    }
    if (m->fix_made)
        sceKernelMunmap((void*)(uintptr_t)m->fix_va, 0x8000);
    if (iv)
        sceKernelMunmap(iv, 0x8000);
    if (f)
        sceKernelMunmap(f, 0x7F8000);
    if (m->r_mmap == 0)
        munmap(a, 0x4000);
    if (m->r_fa == 0)
        sceKernelReleaseDirectMemory(fp, 0x7F8000);
    if (m->r_ia == 0)
        sceKernelReleaseDirectMemory(ip, 0x10000);
    int fd = sceKernelOpen(g_tri_mem_path, 0x601, 0x1FF);
    if (fd >= 0) {
        sceKernelWrite(fd, m, sizeof(*m));
        sceKernelFsync(fd);
        sceKernelClose(fd);
    }
    st.mem = 2;
    tri_state_save(&st);
}
static void tri_kv(const char* k, unsigned long long v) {
    tri_s(k);
    tri_s("=");
    tri_x(v, v >> 32 ? 12 : 8);
    tri_s(" ");
}
static void tri_kd(const char* k, long long v) {
    tri_s(k);
    tri_s("=");
    tri_i(v);
    tri_s(" ");
}
static void tri_mem_report(int mem_state) {
    tri_s("### MEM: the app's memory sequence at the start of main()\n");
    if (mem_state == 1) {
        tri_s("CRASHED: the app ended during it\nsummary MEM: CRASHED\n\n");
        return;
    }
    struct TriangleMem* m = &g_tri_mem;
    int fd = sceKernelOpen(g_tri_mem_path, 0, 0);
    long n = -1;
    if (fd >= 0) {
        n = sceKernelRead(fd, m, sizeof(*m));
        sceKernelClose(fd);
    }
    m->build[sizeof(m->build) - 1] = 0;
    if (n != (long)sizeof(*m) || !tri_streq(m->build, TRIANGLE_BUILD)) {
        tri_s("no results file\nsummary MEM: MISSING\n\n");
        return;
    }
    tri_s("mapped from 0x200000000 before it (find next):");
    tri_kd(" ranges", m->vq_n);
    tri_kd("last_ret", m->vq_ret);
    tri_s("\n");
    for (int k = 0; k < m->vq_n && k < TRI_VQ_MAX; k++) {
        tri_s("  ");
        tri_kv("start", m->vq_start[k]);
        tri_kv("end", m->vq_end[k]);
        tri_kv("prot", m->vq_prot[k]);
        tri_kv("type", m->vq_type[k]);
        tri_kv("flags", m->vq_flags[k]);
        tri_s("name=\"");
        m->vq_name[k][31] = 0;
        tri_s(m->vq_name[k]);
        tri_s("\"\n");
    }
    tri_kd("mmap_ret", m->r_mmap);
    tri_kv("mmap", m->a_mmap);
    tri_s("\n");
    tri_kd("frame_alloc", m->r_fa);
    tri_kv("phys", m->f_phys);
    tri_kd("map", m->r_fm);
    tri_kv("va", m->f_va);
    tri_kv("end", m->f_va + 0x7F8000ull);
    tri_s("\n");
    tri_kd("index_alloc", m->r_ia);
    tri_kv("phys", m->i_phys);
    tri_kd("map_second_half", m->r_im);
    tri_kv("va", m->i_va);
    tri_s("\n");
    tri_kv("fixed_target", m->target);
    tri_s("\n");
    for (int k = 0; k < 2; k++) {
        tri_s("  query ");
        tri_kv("at", m->target + 0x4000ull * (uint64_t)k);
        tri_kd("ret", m->q_ret[k]);
        tri_kv("start", m->q_start[k]);
        tri_kv("end", m->q_end[k]);
        tri_kv("offset", m->q_off[k]);
        tri_kv("prot", m->q_prot[k]);
        tri_kv("type", m->q_type[k]);
        tri_s("class=");
        char c[2] = {m->q_class[k], 0};
        tri_s(c);
        tri_s("\n");
    }
    tri_kd("fixed_made", m->fix_made);
    tri_kd("fixed_ret", m->r_fix);
    tri_kv("fixed_va", m->fix_va);
    tri_s("\n");
    tri_s("relative: mmap-0x200000000=");
    tri_x(m->a_mmap - 0x200000000ull, 8);
    tri_s(" frame-mmap=");
    tri_x(m->f_va - m->a_mmap, 8);
    tri_s(" index-mmap=");
    tri_x(m->i_va - m->a_mmap, 8);
    tri_s(" target_overlaps_frame=");
    int ov = m->target < m->f_va + 0x7F8000ull && m->target + 0x8000ull > m->f_va;
    tri_u((unsigned)ov);
    tri_s("\n");
    for (int k = 0; k < 2; k++) {
        tri_s(k ? "frame+0x4000 before " : "frame+0 before ");
        for (int j = 0; j < 4; j++) {
            tri_x(m->f_before[k][j], 8);
            tri_s(" ");
        }
        tri_s(" after ");
        for (int j = 0; j < 4; j++) {
            tri_x(m->f_after[k][j], 8);
            tri_s(" ");
        }
        tri_s("\n");
    }
    tri_s("index first half (through its own address) ");
    for (int j = 0; j < 4; j++) {
        tri_x(m->i_val[j], 8);
        tri_s(" ");
    }
    tri_s("\nsummary MEM: ");
    tri_s(!m->fix_made ? "NO FIXED MAP"
          : ov         ? "FIXED MAP OVER THE FRAME"
                       : "FIXED MAP NOT OVER THE FRAME");
    tri_s("\n\n");
}

/* ==== GPU context ==============================================================================
 */
struct TriangleContext {
    uint32_t* dcb;
    volatile uint32_t* fence; /* [0] [1] EOP fences, [16] the WAIT_REG_MEM label */
    uint32_t fv;
    uint8_t* data;   /* 256 bytes, byte a = a */
    uint8_t* rpdata; /* the app's 5 vertices (pos vec2, clr vec3) */
    uint32_t* tab;   /* V# tables / shader tables, 256 bytes each */
    uint32_t* out;   /* fetch results: 512 bytes per case */
    uint32_t* rt;    /* 256 x 128 RGBA8 linear */
    uint32_t* map;   /* coverage map, 256 x 128 */
    uint32_t* scratch_map;
    uint16_t* ib;    /* 32768 indices */
    uint32_t* flags; /* 65536 */
    void *vs_fetch, *vs_full, *vs_index, *vs_raw, *ps_cov, *fetch;
    /* RP */
    void *rp_vs, *rp_ps, *rp_fetch;
    uint32_t* rp_vsharps; /* pos, clr */
    uint32_t* rp_test_vsharp;
    uint32_t* rp_test;
    uint8_t* rp_vertices;
    uint16_t* rp_vindex;
    uint16_t* rp_index; /* 64 KiB, the halves mapped in reverse */
    volatile uint32_t* rp_wait;
};
static struct TriangleContext g_tri;

static void* tri_alloc(unsigned long size, unsigned long align) {
    return gpu_alloc_typed(size, align, MEM_TYPE_ONION);
}
/* 64 KiB: one VA range of our own, then its halves remapped in reverse (the app's
   dmem_big_index: VA + 0 -> physical + 32 KiB, VA + 32 KiB -> physical + 0). */
static uint16_t* tri_alloc_reversed(void) {
    long phys = 0;
    void* va = 0;
    if (sceKernelAllocateDirectMemory(0, 0x600000000L, 0x10000, 0x4000, 0, (unsigned long*)&phys))
        return 0;
    if (sceKernelMapDirectMemory(&va, 0x10000, 0x33, 0, phys, 0x4000))
        return 0;
    void* lo = va;
    void* hi = (uint8_t*)va + 0x8000;
    if (sceKernelMapDirectMemory(&lo, 0x8000, 0x33, 0x10, phys + 0x8000, 0x4000) ||
        sceKernelMapDirectMemory(&hi, 0x8000, 0x33, 0x10, phys, 0x4000))
        return 0;
    my_memset(va, 0, 0x10000);
    return (uint16_t*)va;
}

/* The app's shaders, byte for byte (its main.cpp), and its stage registers. */
static const uint32_t k_rp_fetch[] = {0xC0820300, 0xBF8C007F, 0xE00C2000, 0x80010400, 0xC0820304,
                                      0xBF8C007F, 0xE00C2000, 0x80010800, 0xBF8C0000, 0xBE802000};
static const uint32_t k_rp_vs[] = {0xBEEB03FF, 0x00000008, 0xBE802100, 0xF80008CF, 0x07060504,
                                   0xF800020F, 0x0B0A0908, 0xBF810000, 0x000AAA39, 0x00080000,
                                   0x5B000000, 0x00010301, 0x0000D0D0, 0x00000014, 0x00000012,
                                   0x00020017, 0x00000302, 0x046D611C, 0x5362724F, 0x07726468,
                                   0x00002045, 0x050C0202, 0x00EFEF6F, 0x00000000, 0x5F1F362F};
static const uint32_t k_rp_ps[] = {0xBEEB03FF, 0x00000008, 0xBEFC0300, 0xC80C0000, 0xC80D0001,
                                   0xC8080100, 0xC8090101, 0x5E040503, 0xC80C0200, 0xC8000300,
                                   0xC80D0201, 0xC8010301, 0x5E000103, 0xF8001C0F, 0x00000002,
                                   0xBF810000, 0x00000302, 0x046D611C, 0x5362724F, 0x07726468,
                                   0x00004041, 0x05080002, 0x48BDAEE3, 0x00000000, 0x46B3C0B6};
static const float k_rp_vertices[5][5] = {{0.0f, 0.5f, 1.0f, 0.0f, 0.0f},
                                          {0.5f, -0.5f, 0.0f, 1.0f, 0.0f},
                                          {-0.5f, -0.5f, 0.0f, 0.0f, 1.0f},
                                          {0.75f, 0.5f, 1.0f, 1.0f, 1.0f},
                                          {0.0f, 0.5f, 1.0f, 0.0f, 0.0f}};
static const short k_rp_vindex[9] = {0, 0, 0, 1, 0, 3, 0, 3, 2};

static int triangle_alloc(struct TriangleContext* c) {
    my_memset(c, 0, sizeof(*c));
    c->dcb = (uint32_t*)tri_alloc(TRIANGLE_DCB_DW * 4, 0x4000);
    c->fence = (volatile uint32_t*)tri_alloc(0x1000, 0x1000);
    c->data = (uint8_t*)tri_alloc(0x1000, 0x1000);
    c->rpdata = (uint8_t*)tri_alloc(0x1000, 0x1000);
    c->tab = (uint32_t*)tri_alloc(0x10000, 0x1000);
    c->out = (uint32_t*)tri_alloc(0x10000, 0x1000);
    c->rt = (uint32_t*)tri_alloc(TRIANGLE_RT_PX * 4, 0x10000);
    c->map = (uint32_t*)tri_alloc(TRIANGLE_RT_PX * 4, 0x10000);
    c->scratch_map = (uint32_t*)tri_alloc(TRIANGLE_RT_PX * 4, 0x10000);
    c->ib = (uint16_t*)tri_alloc(TRIANGLE_INDICES * 2, 0x10000);
    c->flags = (uint32_t*)tri_alloc(65536 * 4, 0x10000);
    c->vs_fetch = tri_alloc(sizeof(tri_vs_fetch_bin), 0x1000);
    c->vs_full = tri_alloc(sizeof(tri_vs_full_bin), 0x1000);
    c->vs_index = tri_alloc(sizeof(tri_vs_index_bin), 0x1000);
    c->vs_raw = tri_alloc(sizeof(tri_vs_raw_bin), 0x1000);
    c->ps_cov = tri_alloc(sizeof(tri_ps_cov_bin), 0x1000);
    c->fetch = tri_alloc(sizeof(tri_fetch_bin), 0x1000);
    c->rp_vs = tri_alloc(sizeof(k_rp_vs), 0x1000);
    c->rp_ps = tri_alloc(sizeof(k_rp_ps), 0x1000);
    c->rp_fetch = tri_alloc(sizeof(k_rp_fetch), 0x1000);
    c->rp_vsharps = (uint32_t*)tri_alloc(0x1000, 0x1000);
    c->rp_test_vsharp = c->rp_vsharps + 16;
    c->rp_test = c->rp_vsharps + 32;
    c->rp_vertices = (uint8_t*)tri_alloc(0x1000, 0x1000);
    c->rp_vindex = (uint16_t*)tri_alloc(0x1000, 0x1000);
    c->rp_wait = (volatile uint32_t*)tri_alloc(0x1000, 0x1000);
    c->rp_index = tri_alloc_reversed();
    if (!(c->dcb && c->fence && c->data && c->rpdata && c->tab && c->out && c->rt && c->map &&
          c->scratch_map && c->ib && c->flags && c->vs_fetch && c->vs_full && c->vs_index &&
          c->vs_raw && c->ps_cov && c->fetch && c->rp_vs && c->rp_ps && c->rp_fetch &&
          c->rp_vsharps && c->rp_vertices && c->rp_vindex && c->rp_wait && c->rp_index))
        return 0;
    my_memcpy(c->vs_fetch, tri_vs_fetch_bin, sizeof(tri_vs_fetch_bin));
    my_memcpy(c->vs_full, tri_vs_full_bin, sizeof(tri_vs_full_bin));
    my_memcpy(c->vs_index, tri_vs_index_bin, sizeof(tri_vs_index_bin));
    my_memcpy(c->vs_raw, tri_vs_raw_bin, sizeof(tri_vs_raw_bin));
    my_memcpy(c->ps_cov, tri_ps_cov_bin, sizeof(tri_ps_cov_bin));
    my_memcpy(c->fetch, tri_fetch_bin, sizeof(tri_fetch_bin));
    my_memcpy(c->rp_vs, k_rp_vs, sizeof(k_rp_vs));
    my_memcpy(c->rp_ps, k_rp_ps, sizeof(k_rp_ps));
    my_memcpy(c->rp_fetch, k_rp_fetch, sizeof(k_rp_fetch));
    for (int i = 0; i < 256; i++)
        c->data[i] = (uint8_t)i;
    my_memcpy(c->rpdata, k_rp_vertices, sizeof(k_rp_vertices));
    c->fence[0] = c->fence[1] = 0;
    return 1;
}

/* V#: base, stride, records, dst_sel (x | y << 3 | z << 6 | w << 9), number and data format. */
static void tri_vsharp(uint32_t* v, uint64_t base, uint32_t stride, uint32_t records, uint32_t dst,
                       uint32_t nfmt, uint32_t dfmt) {
    v[0] = (uint32_t)base;
    v[1] = ((uint32_t)(base >> 32) & 0xFFFFu) | ((stride & 0x3FFFu) << 16);
    v[2] = records;
    v[3] = (dst & 0xFFFu) | (nfmt << 12) | (dfmt << 15);
}
#define TRI_DST_XYZW (4u | (5u << 3) | (6u << 6) | (7u << 9))
#define TRI_DST_XY01 (4u | (5u << 3) | (0u << 6) | (1u << 9))
#define TRI_DST_XYZ0 (4u | (5u << 3) | (6u << 6) | (0u << 9))

/* ---- command buffer pieces ---- */
static void tri_begin(struct PM4Builder* b, struct TriangleContext* c) {
    pm4_init(b, c->dcb, TRIANGLE_DCB_DW);
    int w = sceGnmDrawInitDefaultHardwareState350(b->buf + b->off, b->cap - b->off);
    if (w > 0)
        b->off += (uint32_t)w;
    pm4_acquire_mem(b, COHER_SH_ICACHE_ENA | COHER_SH_KCACHE_ENA | COHER_TCL1_ACTION_ENA |
                           COHER_TC_ACTION_ENA);
    pm4_context_control(b);
}
static void tri_eop(struct PM4Builder* b, volatile uint32_t* addr, uint32_t v) {
    uint64_t a = (uint64_t)(uintptr_t)addr;
    pm4_emit(b, pm4_type3(0x47, 5));
    pm4_emit(b, 0x14u | (5u << 8)); /* CACHE_FLUSH_AND_INV_TS, EVENT_INDEX 5 */
    pm4_emit(b, (uint32_t)(a & 0xFFFFFFFCu));
    pm4_emit(b, ((uint32_t)(a >> 32) & 0xFFFFu) | (1u << 29));
    pm4_emit(b, v);
    pm4_emit(b, 0);
}
/* End: PS_PARTIAL_FLUSH, caches to memory, two EOPs (as alpha-to-mask: GFX7 needs two for all
   engines to be idle). */
static void tri_end(struct PM4Builder* b, struct TriangleContext* c) {
    pm4_emit(b, pm4_type3(0x46, 1));
    pm4_emit(b, 0x10u | (4u << 8));
    pm4_acquire_mem(b, COHER_TC_ACTION_ENA | COHER_TCL1_ACTION_ENA | COHER_CB_ACTION_ENA |
                           COHER_DB_ACTION_ENA | COHER_CB_DEST_BASE_ENA | (1u << 14));
    c->fv++;
    tri_eop(b, c->fence + 1, c->fv);
    tri_eop(b, c->fence, c->fv);
}
/* Submits and waits: 0 done, -1 fence timeout, -2 not submitted. */
static int tri_kick(struct TriangleContext* c, struct PM4Builder* b) {
    if (b->overflow)
        return -2;
    uint32_t sz = b->off * 4;
    void* a[1] = {c->dcb};
    if (sceGnmSubmitCommandBuffers(1, a, &sz, 0, 0) != 0)
        return -2;
    sceGnmSubmitDone();
    return 0;
}
static int tri_wait(struct TriangleContext* c) {
    uint64_t t0 = sceKernelGetProcessTime();
    while (*c->fence < c->fv) {
        if (sceKernelGetProcessTime() - t0 > TRIANGLE_TIMEOUT_US)
            return -1;
        sceKernelUsleep(100);
    }
    return 0;
}

/* Shared state of the S and I draws: 256 x 128 viewport (y down), one parameter, the PS inputs
   PERSP_CENTER + POS_X / POS_Y_FLOAT, linear RGBA8 UNORM target, no depth. */
#define TRI_CB_INFO ((0xAu << 2) | (1u << 7) | (1u << 15))
static void tri_common(struct PM4Builder* b, struct TriangleContext* c) {
    pm4_set_context_reg(b, CTX_VGT_GS_ONCHIP_CNTL, VGT_GS_ONCHIP_CNTL_SAFE);
    pm4_emit(b, pm4_type3(PM4_SET_CONTEXT_REG, 7));
    pm4_emit(b, CTX_VIEWPORT0);
    pm4_emit_f(b, TRIANGLE_RT_W * 0.5f);
    pm4_emit_f(b, TRIANGLE_RT_W * 0.5f);
    pm4_emit_f(b, TRIANGLE_RT_H * -0.5f);
    pm4_emit_f(b, TRIANGLE_RT_H * 0.5f);
    pm4_emit_f(b, 1.0f);
    pm4_emit_f(b, 0.0f);
    {
        uint32_t z[2] = {0x00000000u, 0x3F800000u};
        pm4_set_context_regs(b, CTX_VIEWPORT_ZMIN0, z, 2);
    }
    pm4_set_context_reg(b, CTX_INDEX_OFFSET, 0);
    pm4_set_context_reg(b, CTX_PS_INPUT_CNTL_0, 0);
    pm4_set_context_reg(b, CTX_VS_OUTPUT_CONFIG, 0);
    pm4_set_context_reg(b, CTX_PS_INPUT_ENA, 0x302);
    pm4_set_context_reg(b, CTX_PS_INPUT_ADDR, 0x302);
    pm4_set_context_reg(b, CTX_NUM_INTERP, 1);
    pm4_set_context_reg(b, CTX_SHADER_POS_FORMAT, 4);
    pm4_set_context_reg(b, CTX_COLOR_CONTROL, 0x00CC0010u);
    pm4_set_context_reg(b, CTX_SPI_BARYC_CNTL, 0);
    pm4_set_context_reg(b, CTX_CLIPPER_CONTROL, 1u << 19);
    pm4_set_context_reg(b, CTX_VIEWPORT_CONTROL, 0x43Fu);
    pm4_set_context_reg(b, CTX_VS_OUTPUT_CONTROL, 0);
    pm4_set_context_reg(b, CTX_BLEND_CONTROL0, 0);
    pm4_set_context_reg(b, CTX_DEPTH_CONTROL, 0);
    pm4_set_context_reg(b, CTX_DB_Z_INFO, 0);
    pm4_set_context_reg(b, CTX_DB_SHADER_CONTROL, 0);
    pm4_set_context_reg(b, CTX_Z_EXPORT_FORMAT, 0);
    pm4_set_context_reg(b, CTX_COLOR_EXPORT_FORMAT, 9);
    pm4_set_context_reg(b, CTX_COLOR_TARGET_MASK, 0xF);
    pm4_set_context_reg(b, CTX_COLOR_SHADER_MASK, 0xF);
    emit_msaa_state(b, 1);
    {
        uint32_t cb = (uint32_t)((uint64_t)(uintptr_t)c->rt >> 8);
        uint32_t r[14] = {cb,
                          (TRIANGLE_RT_W / 8) - 1,
                          (TRIANGLE_RT_PX / 64) - 1,
                          0,
                          TRI_CB_INFO,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0};
        pm4_set_context_regs(b, CTX_CB_COLOR0_BASE, r, 14);
        pm4_emit(b, 0xC0001000u);
        pm4_emit(b, (uint32_t)TRIANGLE_RT_W | ((uint32_t)TRIANGLE_RT_H << 16));
    }
    pm4_set_uconfig_reg(b, UCFG_NUM_INSTANCES, 1);
    (void)c;
}
static void tri_vs(struct PM4Builder* b, void* code, uint32_t rsrc1, uint32_t user_sgprs) {
    uint64_t a = (uint64_t)(uintptr_t)code;
    uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), rsrc1, user_sgprs << 1};
    pm4_set_sh_regs(b, SH_VS_PGM_LO, r, 4);
}
static void tri_ps(struct PM4Builder* b, struct TriangleContext* c, const uint32_t* tab) {
    uint64_t a = (uint64_t)(uintptr_t)c->ps_cov, t = (uint64_t)(uintptr_t)tab;
    uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), TRI_PS_COV_RSRC1, 2u << 1};
    pm4_set_sh_regs(b, SH_PS_PGM_LO, r, 4);
    uint32_t ud[2] = {(uint32_t)t, (uint32_t)(t >> 32)};
    pm4_set_sh_regs(b, SH_PS_USER_DATA_0, ud, 2);
}
static void tri_ptr_ud(struct PM4Builder* b, uint32_t reg, const void* p) {
    uint64_t a = (uint64_t)(uintptr_t)p;
    uint32_t ud[2] = {(uint32_t)a, (uint32_t)(a >> 32)};
    pm4_set_sh_regs(b, reg, ud, 2);
}
static void tri_scissor(struct PM4Builder* b, uint32_t reg, int l, int t, int r, int bt, int dis) {
    uint32_t v[2] = {((uint32_t)l & 0x7FFFu) | (((uint32_t)t & 0x7FFFu) << 16) |
                         ((uint32_t)(dis != 0) << 31),
                     ((uint32_t)r & 0x7FFFu) | (((uint32_t)bt & 0x7FFFu) << 16)};
    pm4_set_context_regs(b, reg, v, 2);
}
/* PS table: map V#, map pitch, value, colour. */
static void tri_cov_table(uint32_t* t, uint32_t* map, uint32_t pitch, uint32_t px, uint32_t value,
                          float r, float g, float bl, float a) {
    tri_vsharp(t, (uint64_t)(uintptr_t)map, 0, px * 4, 0, 4, 4);
    t[4] = pitch;
    t[5] = value;
    float col[4] = {r, g, bl, a};
    my_memcpy(t + 6, col, 16);
}

/* ==== F: vertex fetch alignment =================================================================
 */
static const struct {
    uint8_t dfmt, ncomp, csize;
    const char* name;
} k_tri_fmt[] = {
    {1, 1, 1, "8"},         {2, 1, 2, "16"},
    {3, 2, 1, "8_8"},       {4, 1, 4, "32"},
    {5, 2, 2, "16_16"},     {10, 4, 1, "8_8_8_8"},
    {11, 2, 4, "32_32"},    {12, 4, 2, "16_16_16_16"},
    {13, 3, 4, "32_32_32"}, {14, 4, 4, "32_32_32_32"},
};
#define TRI_NFMT ((int)(sizeof(k_tri_fmt) / sizeof(k_tri_fmt[0])))
static const uint8_t k_tri_strides[3] = {20, 22, 21};

struct TriFetchCase {
    uint64_t base;
    const uint8_t* mem; /* the bytes at offset 0 of the data */
    uint32_t off, stride, nvert, dst, nfmt, dfmt, ncomp, csize;
};
static uint32_t tri_read(const uint8_t* mem, uint32_t a, uint32_t size) {
    uint32_t v = 0;
    for (uint32_t i = 0; i < size; i++)
        v |= (uint32_t)mem[a + i] << (8 * i);
    return v;
}
/* Past-format component c of a vertex's results vtx (doc at the top). */
static char tri_classify_past(const struct TriFetchCase* f, uint32_t c, const uint32_t* vtx) {
    uint32_t sel = (f->dst >> (3 * c)) & 7u;
    uint32_t r = (sel - 4) % f->ncomp;
    if (((f->dst >> (3 * r)) & 7u) == 4 + r && vtx[c] == vtx[r])
        return 'r';
    if (vtx[c] == 0)
        return 'z';
    return (vtx[c] == 1 || vtx[c] == 0x3F800000u) ? 'o' : 'X';
}
/* One letter per component (doc at the top). */
static char tri_classify(const struct TriFetchCase* f, uint32_t k, uint32_t c,
                         const uint32_t* vtx) {
    uint32_t v = vtx[c];
    if (v == TRIANGLE_UNWRITTEN)
        return 'u';
    uint32_t sel = (f->dst >> (3 * c)) & 7u;
    if (sel == 0 || sel == 1) {
        uint32_t one = f->nfmt == 7 ? 0x3F800000u : 1u;
        return v == (sel ? one : 0u) ? 'c' : 'X';
    }
    if (sel < 4)
        return 'X';
    uint32_t comp = sel - 4;
    if (comp >= f->ncomp)
        return tri_classify_past(f, c, vtx);
    uint32_t a = f->off + k * f->stride;
    uint32_t elem = f->ncomp * f->csize;
    uint32_t align = elem < 4 ? elem : 4;
    uint32_t m = tri_read(f->mem, (a & ~(align - 1)) + comp * f->csize, f->csize);
    uint32_t e = tri_read(f->mem, a + comp * f->csize, f->csize);
    if (v == m)
        return 'A';
    return v == e ? 'E' : 'X';
}
static void tri_fetch_build(struct PM4Builder* b, struct TriangleContext* c,
                            const struct TriFetchCase* f, uint32_t* tab, uint32_t* out) {
    tri_vsharp(tab, f->base + f->off, f->stride, f->nvert, f->dst, f->nfmt, f->dfmt);
    tri_vsharp(tab + 4, (uint64_t)(uintptr_t)out, 0, 512, 0, 4, 4);
    tri_vs(b, c->vs_fetch, TRI_VS_FETCH_RSRC1, 4);
    tri_ptr_ud(b, SH_VS_USER_DATA_0, c->fetch);
    tri_ptr_ud(b, SH_VS_USER_DATA_0 + 2, tab);
    pm4_draw_index_auto(b, f->nvert);
}
/* Returns 0 / -1 timeout / -2 not submitted. */
static int tri_fetch_group(struct TriangleContext* c, int g) {
    struct TriFetchCase f[12];
    int n = 0;
    if (g < TRI_NFMT) {
        for (int s = 0; s < 3; s++)
            for (uint32_t off = 0; off < 4; off++, n++) {
                f[n].base = (uint64_t)(uintptr_t)c->data;
                f[n].mem = c->data;
                f[n].off = off;
                f[n].stride = k_tri_strides[s];
                f[n].nvert = 4;
                f[n].dst = TRI_DST_XYZW;
                f[n].nfmt = 4;
                f[n].dfmt = k_tri_fmt[g].dfmt;
                f[n].ncomp = k_tri_fmt[g].ncomp;
                f[n].csize = k_tri_fmt[g].csize;
            }
    } else {
        /* the app's V#s */
        f[0] = (struct TriFetchCase){
            (uint64_t)(uintptr_t)c->rpdata, c->rpdata, 1, 20, 5, TRI_DST_XY01, 7, 11, 2, 4};
        f[1] = (struct TriFetchCase){
            (uint64_t)(uintptr_t)c->rpdata, c->rpdata, 8, 20, 5, TRI_DST_XYZ0, 7, 13, 3, 4};
        n = 2;
    }
    for (int i = 0; i < n * 128; i++)
        c->out[i] = TRIANGLE_UNWRITTEN;
    struct PM4Builder b;
    tri_begin(&b, c);
    tri_common(&b, c);
    {
        uint64_t a = (uint64_t)(uintptr_t)c->ps_cov;
        uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), TRI_PS_COV_RSRC1, 2u << 1};
        pm4_set_sh_regs(&b, SH_PS_PGM_LO, r, 4);
        tri_cov_table(c->tab + 0x3F00, c->scratch_map, TRIANGLE_RT_W, TRIANGLE_RT_PX, 0, 0, 0, 0,
                      0);
        tri_ptr_ud(&b, SH_PS_USER_DATA_0, c->tab + 0x3F00);
    }
    pm4_set_context_reg(&b, CTX_WINDOW_OFFSET, 0);
    tri_scissor(&b, CTX_SCREEN_SCISSOR, 0, 0, TRIANGLE_RT_W, TRIANGLE_RT_H, 0);
    tri_scissor(&b, CTX_WINDOW_SCISSOR, 0, 0, TRIANGLE_OPEN, TRIANGLE_OPEN, 1);
    tri_scissor(&b, CTX_GENERIC_SCISSOR, 0, 0, TRIANGLE_OPEN, TRIANGLE_OPEN, 1);
    pm4_set_context_reg(&b, CTX_POLYGON_CONTROL, 0);
    pm4_set_uconfig_reg(&b, UCFG_PRIMITIVE_TYPE, 1); /* point list */
    for (int i = 0; i < n; i++)
        tri_fetch_build(&b, c, &f[i], c->tab + i * 64, c->out + i * 128);
    tri_end(&b, c);
    int r = tri_kick(c, &b);
    if (r == 0)
        r = tri_wait(c);
    if (r < 0)
        return r;
    for (int i = 0; i < n; i++) {
        const struct TriFetchCase* p = &f[i];
        const uint32_t* o = c->out + i * 128;
        int ok[2] = {1, 1};
        tri_s(g < TRI_NFMT ? "F_" : "F_RP ");
        tri_s(g < TRI_NFMT ? k_tri_fmt[g].name
                           : (i ? "colour 32_32_32 FLOAT" : "position 32_32 FLOAT"));
        tri_s(" base+");
        tri_u(p->off);
        tri_s(" stride ");
        tri_u(p->stride);
        tri_s(" align ");
        tri_u(p->ncomp * p->csize < 4 ? p->ncomp * p->csize : 4);
        tri_s("\n");
        for (int path = 0; path < 2; path++) {
            tri_s(path ? "  direct " : "  fetch  ");
            for (uint32_t k = 0; k < p->nvert; k++) {
                char t[5];
                for (uint32_t cc = 0; cc < 4; cc++) {
                    t[cc] = tri_classify(p, k, cc, o + k * 8 + path * 4);
                    if (t[cc] != 'A' && t[cc] != 'c' && t[cc] != 'r')
                        ok[path] = 0;
                }
                t[4] = 0;
                tri_s(t);
                tri_s(" ");
            }
            tri_s(" raw");
            for (uint32_t k = 0; k < p->nvert; k++)
                for (uint32_t cc = 0; cc < 4; cc++) {
                    tri_s(" ");
                    tri_x(o[k * 8 + path * 4 + cc], 8);
                }
            tri_s("\n");
        }
        int same = 1;
        for (uint32_t k = 0; k < p->nvert; k++)
            for (uint32_t cc = 0; cc < 4; cc++)
                same &= o[k * 8 + cc] == o[k * 8 + 4 + cc];
        tri_s("  summary fetch ");
        tri_s(ok[0] ? "MODEL" : "DIFF");
        tri_s(" direct ");
        tri_s(ok[1] ? "MODEL" : "DIFF");
        tri_s(same ? " fetch==direct" : " fetch!=direct");
        tri_s("\n");
    }
    tri_s("\n");
    return 0;
}

/* ==== MC: components past the format, per number format =======================================
 */
static const struct {
    uint8_t fmt; /* index into k_tri_fmt */
    uint8_t nfmt;
} k_tri_mc[] = {
    {0, 0}, {0, 1}, {0, 4}, {0, 5},         /* 8 */
    {1, 0}, {1, 1}, {1, 4}, {1, 5}, {1, 7}, /* 16 */
    {2, 0}, {2, 1}, {2, 4}, {2, 5},         /* 8_8 */
    {3, 4}, {3, 5}, {3, 7},                 /* 32 */
    {4, 0}, {4, 1}, {4, 4}, {4, 5}, {4, 7}, /* 16_16 */
    {6, 4}, {6, 5}, {6, 7},                 /* 32_32 */
    {8, 4}, {8, 5}, {8, 7},                 /* 32_32_32 */
};
#define TRI_NMC ((int)(sizeof(k_tri_mc) / sizeof(k_tri_mc[0])))
static const char* const k_tri_nfmt_name[8] = {"UNORM", "SNORM", "USCALED",  "SSCALED",
                                               "UINT",  "SINT",  "SNORM_NZ", "FLOAT"};
static int tri_mc_step(struct TriangleContext* c) {
    struct TriFetchCase f[TRI_NMC];
    for (int i = 0; i < TRI_NMC; i++) {
        const int g = k_tri_mc[i].fmt;
        f[i] = (struct TriFetchCase){(uint64_t)(uintptr_t)c->data,
                                     c->data,
                                     0,
                                     20,
                                     4,
                                     TRI_DST_XYZW,
                                     k_tri_mc[i].nfmt,
                                     k_tri_fmt[g].dfmt,
                                     k_tri_fmt[g].ncomp,
                                     k_tri_fmt[g].csize};
    }
    for (int i = 0; i < TRI_NMC * 128; i++)
        c->out[i] = TRIANGLE_UNWRITTEN;
    struct PM4Builder b;
    tri_begin(&b, c);
    tri_common(&b, c);
    {
        uint64_t a = (uint64_t)(uintptr_t)c->ps_cov;
        uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), TRI_PS_COV_RSRC1, 2u << 1};
        pm4_set_sh_regs(&b, SH_PS_PGM_LO, r, 4);
        tri_cov_table(c->tab + 0x3F00, c->scratch_map, TRIANGLE_RT_W, TRIANGLE_RT_PX, 0, 0, 0, 0,
                      0);
        tri_ptr_ud(&b, SH_PS_USER_DATA_0, c->tab + 0x3F00);
    }
    pm4_set_context_reg(&b, CTX_WINDOW_OFFSET, 0);
    tri_scissor(&b, CTX_SCREEN_SCISSOR, 0, 0, TRIANGLE_RT_W, TRIANGLE_RT_H, 0);
    tri_scissor(&b, CTX_WINDOW_SCISSOR, 0, 0, TRIANGLE_OPEN, TRIANGLE_OPEN, 1);
    tri_scissor(&b, CTX_GENERIC_SCISSOR, 0, 0, TRIANGLE_OPEN, TRIANGLE_OPEN, 1);
    pm4_set_context_reg(&b, CTX_POLYGON_CONTROL, 0);
    pm4_set_uconfig_reg(&b, UCFG_PRIMITIVE_TYPE, 1); /* point list */
    for (int i = 0; i < TRI_NMC; i++)
        tri_fetch_build(&b, c, &f[i], c->tab + i * 64, c->out + i * 128);
    tri_end(&b, c);
    int r = tri_kick(c, &b);
    if (r == 0)
        r = tri_wait(c);
    if (r < 0)
        return r;
    tri_s("### MC: components past the format, per number format (base 0, stride 20, dst X Y Z W)"
          "\n\n");
    for (int i = 0; i < TRI_NMC; i++) {
        const struct TriFetchCase* p = &f[i];
        const uint32_t* o = c->out + i * 128;
        tri_s("MC_");
        tri_s(k_tri_fmt[k_tri_mc[i].fmt].name);
        tri_s(" ");
        tri_s(k_tri_nfmt_name[p->nfmt & 7]);
        tri_s("\n");
        char sum[4] = {0, 0, 0, 0};
        for (int path = 0; path < 2; path++) {
            tri_s(path ? "  direct " : "  fetch  ");
            for (uint32_t k = 0; k < p->nvert; k++) {
                char t[5];
                const uint32_t* v = o + k * 8 + path * 4;
                for (uint32_t cc = 0; cc < 4; cc++) {
                    t[cc] = v[cc] == TRIANGLE_UNWRITTEN ? 'u'
                            : cc < p->ncomp             ? 'v'
                                                        : tri_classify_past(p, cc, v);
                    if (path != 0 || k == 0 || cc < p->ncomp)
                        continue;
                    char* q = &sum[cc - p->ncomp];
                    *q = *q == 0 ? t[cc] : *q == t[cc] ? *q : '?';
                }
                t[4] = 0;
                tri_s(t);
                tri_s(" ");
            }
            tri_s(" raw");
            for (uint32_t k = 0; k < p->nvert; k++)
                for (uint32_t cc = 0; cc < 4; cc++) {
                    tri_s(" ");
                    tri_x(o[k * 8 + path * 4 + cc], 8);
                }
            tri_s("\n");
        }
        int same = 1;
        for (uint32_t k = 0; k < p->nvert; k++)
            for (uint32_t cc = 0; cc < 4; cc++)
                same &= o[k * 8 + cc] == o[k * 8 + 4 + cc];
        tri_s("  summary past-format components (fetch, vertices 1..3) ");
        tri_s(sum);
        tri_s(same ? " fetch==direct" : " fetch!=direct");
        tri_s("\n");
    }
    tri_s("\n");
    return 0;
}

/* ==== RAW: raw buffer loads with a V# base 0..3 bytes past a dword ==============================
 */
/* The byte address a value of size bytes came from (data byte a = a), or -1. */
static int tri_decode(uint32_t v, uint32_t size) {
    uint32_t a = v & 0xFFu;
    for (uint32_t i = 1; i < size; i++)
        if (((v >> (8 * i)) & 0xFFu) != ((a + i) & 0xFFu))
            return -1;
    if (size < 4 && (v >> (8 * size)) != 0)
        return -1;
    return (int)a;
}
static int tri_raw_step(struct TriangleContext* c) {
    for (int i = 0; i < 4 * 128; i++)
        c->out[i] = TRIANGLE_UNWRITTEN;
    struct PM4Builder b;
    tri_begin(&b, c);
    tri_common(&b, c);
    {
        uint64_t a = (uint64_t)(uintptr_t)c->ps_cov;
        uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), TRI_PS_COV_RSRC1, 2u << 1};
        pm4_set_sh_regs(&b, SH_PS_PGM_LO, r, 4);
        tri_cov_table(c->tab + 0x3F00, c->scratch_map, TRIANGLE_RT_W, TRIANGLE_RT_PX, 0, 0, 0, 0,
                      0);
        tri_ptr_ud(&b, SH_PS_USER_DATA_0, c->tab + 0x3F00);
    }
    pm4_set_context_reg(&b, CTX_WINDOW_OFFSET, 0);
    tri_scissor(&b, CTX_SCREEN_SCISSOR, 0, 0, TRIANGLE_RT_W, TRIANGLE_RT_H, 0);
    tri_scissor(&b, CTX_WINDOW_SCISSOR, 0, 0, TRIANGLE_OPEN, TRIANGLE_OPEN, 1);
    tri_scissor(&b, CTX_GENERIC_SCISSOR, 0, 0, TRIANGLE_OPEN, TRIANGLE_OPEN, 1);
    pm4_set_context_reg(&b, CTX_POLYGON_CONTROL, 0);
    pm4_set_uconfig_reg(&b, UCFG_PRIMITIVE_TYPE, 1); /* point list */
    for (uint32_t off = 0; off < 4; off++) {
        uint32_t* t = c->tab + off * 64;
        tri_vsharp(t, (uint64_t)(uintptr_t)c->data + off, 0, 256, 0, 4, 4);
        tri_vsharp(t + 4, (uint64_t)(uintptr_t)(c->out + off * 128), 0, 512, 0, 4, 4);
        tri_vs(&b, c->vs_raw, TRI_VS_RAW_RSRC1, 2);
        tri_ptr_ud(&b, SH_VS_USER_DATA_0, t);
        pm4_draw_index_auto(&b, 4);
    }
    tri_end(&b, c);
    int r = tri_kick(c, &b);
    if (r == 0)
        r = tri_wait(c);
    if (r < 0)
        return r;
    static const struct {
        const char* name;
        uint8_t dw, size, judged;
        uint8_t imm; /* s_buffer: byte offset of the load, 0 for the vector loads */
    } k_loads[7] = {{"dword", 0, 4, 1, 0},          {"ushort", 1, 2, 0, 0},
                    {"ubyte", 2, 1, 0, 0},          {"dwordx2.lo", 4, 4, 1, 0},
                    {"dwordx2.hi", 5, 4, 1, 4},     {"s_buffer imm 1", 3, 4, 0, 4},
                    {"s_buffer sgpr 6", 6, 4, 0, 6}};
    tri_s("### RAW: raw loads with a V# base 0..3 bytes past a dword (offset 5 * vertex)\n\n");
    for (uint32_t off = 0; off < 4; off++) {
        const uint32_t* o = c->out + off * 128;
        tri_s("RAW base+");
        tri_u(off);
        tri_s("\n");
        int ok = 1;
        for (int l = 0; l < 7; l++) {
            const int scalar = l >= 5;
            tri_s("  ");
            tri_s(k_loads[l].name);
            tri_s(": exact");
            for (uint32_t k = 0; k < 4; k++) {
                tri_s(" ");
                uint32_t e = off + (scalar ? 0 : 5 * k) + k_loads[l].imm;
                tri_u(e);
            }
            tri_s("  read");
            for (uint32_t k = 0; k < 4; k++) {
                uint32_t v = o[k * 8 + k_loads[l].dw];
                int a = tri_decode(v, k_loads[l].size);
                tri_s(" ");
                if (a < 0)
                    tri_x(v, 8);
                else
                    tri_u((unsigned)a);
                if (k_loads[l].judged) {
                    uint32_t e = off + 5 * k;
                    uint32_t m = (e & ~3u) + k_loads[l].imm;
                    if (a != (int)m)
                        ok = 0;
                }
            }
            tri_s(k_loads[l].judged ? "  (model: LSBs cleared)\n" : "\n");
        }
        tri_s("  summary dword/dwordx2 ");
        tri_s(ok ? "MODEL" : "DIFF");
        tri_s("\n");
    }
    tri_s("\n");
    return 0;
}

/* ==== S: scissors and window offset =============================================================
 */
struct TriScissorCase {
    char id[16];
    int16_t ox, oy;
    uint8_t which; /* 1 window, 2 generic, 4 viewport restrictive; 8: the app's scaled state */
    uint8_t dis;   /* WINDOW_OFFSET_DISABLE: 1 window, 2 generic, 4 viewport */
    uint8_t vtx, hwso;
};
#define TRI_NS 43
static struct TriScissorCase g_tri_sc[TRI_NS];
static void tri_scissor_cases(void) {
    int n = 0;
    static const char wn[3] = {'W', 'G', 'V'};
    for (int w = 0; w < 3; w++)
        for (int d = 0; d < 2; d++)
            for (int o = 0; o < 2; o++)
                for (int v = 0; v < 2; v++, n++) {
                    struct TriScissorCase* s = &g_tri_sc[n];
                    my_memset(s, 0, sizeof(*s));
                    s->id[0] = 'S';
                    s->id[1] = '_';
                    s->id[2] = wn[w];
                    s->id[3] = '_';
                    s->id[4] = d ? 'd' : 'o';
                    s->id[5] = o ? 'n' : 'p';
                    s->id[6] = v ? 'v' : '-';
                    s->ox = o ? -16 : 16;
                    s->oy = o ? -8 : 8;
                    s->which = (uint8_t)(1u << w);
                    s->dis = (uint8_t)((d ? (1u << w) : 0u) | (7u & ~(1u << w)));
                    s->vtx = (uint8_t)v;
                }
    for (int v = 0; v < 2; v++)
        for (int bits = 0; bits < 8; bits++, n++) {
            struct TriScissorCase* s = &g_tri_sc[n];
            my_memset(s, 0, sizeof(*s));
            s->id[0] = 'S';
            s->id[1] = '_';
            s->id[2] = 'A';
            s->id[3] = '_';
            s->id[4] = (char)('0' + (bits & 1));
            s->id[5] = (char)('0' + ((bits >> 1) & 1));
            s->id[6] = (char)('0' + ((bits >> 2) & 1));
            s->id[7] = v ? 'v' : '-';
            s->ox = 16;
            s->oy = 8;
            s->which = 7;
            s->dis = (uint8_t)bits;
            s->vtx = (uint8_t)v;
        }
    static const char* rp[3] = {"S_RP", "S_RP_nohw", "S_A_000_hw"};
    for (int k = 0; k < 3; k++, n++) {
        struct TriScissorCase* s = &g_tri_sc[n];
        my_memset(s, 0, sizeof(*s));
        for (int i = 0; rp[k][i]; i++)
            s->id[i] = rp[k][i];
        if (k < 2) {
            s->ox = 16;
            s->oy = 0;
            s->which = 8;
            s->dis = 1 | 4; /* window and viewport not offset, generic offset */
        } else {
            s->ox = 16;
            s->oy = 8;
            s->which = 7;
        }
        s->hwso = k != 1;
    }
}
static const int k_tri_win[4] = {8, 4, 200, 100}, k_tri_gen[4] = {24, 12, 240, 120},
                 k_tri_vp[4] = {4, 20, 230, 110};
static void tri_rects(const struct TriScissorCase* s, int win[4], int gen[4], int vp[4],
                      int* vp_en) {
    static const int open[4] = {0, 0, TRIANGLE_OPEN, TRIANGLE_OPEN};
    if (s->which == 8) {
        static const int w8[4] = {0, 0, 240, 128}, g8[4] = {0, 16, 256, 128},
                         v8[4] = {0, 0, 256, 112};
        for (int i = 0; i < 4; i++)
            win[i] = w8[i], gen[i] = g8[i], vp[i] = v8[i];
        *vp_en = 1;
        return;
    }
    for (int i = 0; i < 4; i++) {
        win[i] = (s->which & 1) ? k_tri_win[i] : open[i];
        gen[i] = (s->which & 2) ? k_tri_gen[i] : open[i];
        vp[i] = (s->which & 4) ? k_tri_vp[i] : open[i];
    }
    *vp_en = (s->which & 4) != 0;
}
static void tri_scissor_model(const struct TriScissorCase* s, int m[4]) {
    int win[4], gen[4], vp[4], vp_en;
    tri_rects(s, win, gen, vp, &vp_en);
    int r[4] = {0, 0, TRIANGLE_RT_W, TRIANGLE_RT_H};
    const int* rs[3] = {win, gen, vp};
    for (int k = 0; k < 3; k++) {
        if (k == 2 && !vp_en)
            continue;
        int dx = (s->dis >> k) & 1 ? 0 : s->ox, dy = (s->dis >> k) & 1 ? 0 : s->oy;
        int q[4] = {rs[k][0] + dx, rs[k][1] + dy, rs[k][2] + dx, rs[k][3] + dy};
        for (int i = 0; i < 2; i++)
            r[i] = r[i] > q[i] ? r[i] : q[i];
        for (int i = 2; i < 4; i++)
            r[i] = r[i] < q[i] ? r[i] : q[i];
    }
    if (s->vtx) { /* the viewport's pixels move */
        int q[4] = {s->ox, s->oy, TRIANGLE_RT_W + s->ox, TRIANGLE_RT_H + s->oy};
        for (int i = 0; i < 2; i++)
            r[i] = r[i] > q[i] ? r[i] : q[i];
        for (int i = 2; i < 4; i++)
            r[i] = r[i] < q[i] ? r[i] : q[i];
    }
    for (int i = 0; i < 4; i++)
        m[i] = r[i] < 0 ? 0 : r[i];
}
/* Bounding box and count of the pixels equal to v. */
static uint32_t tri_bbox(const uint32_t* px, uint32_t v, int r[4]) {
    uint32_t n = 0;
    r[0] = TRIANGLE_RT_W;
    r[1] = TRIANGLE_RT_H;
    r[2] = r[3] = 0;
    for (int y = 0; y < TRIANGLE_RT_H; y++)
        for (int x = 0; x < TRIANGLE_RT_W; x++)
            if (px[y * TRIANGLE_RT_W + x] == v) {
                n++;
                if (x < r[0])
                    r[0] = x;
                if (y < r[1])
                    r[1] = y;
                if (x + 1 > r[2])
                    r[2] = x + 1;
                if (y + 1 > r[3])
                    r[3] = y + 1;
            }
    return n;
}
static int tri_scissor_case(struct TriangleContext* c, int k) {
    const struct TriScissorCase* s = &g_tri_sc[k];
    int win[4], gen[4], vp[4], vp_en;
    tri_rects(s, win, gen, vp, &vp_en);
    my_memset(c->map, 0, TRIANGLE_RT_PX * 4);
    uint32_t* tclr = c->tab + 0x3E00;
    uint32_t* ttst = c->tab + 0x3D00;
    uint32_t value = 0x100u + (uint32_t)k;
    tri_cov_table(tclr, c->scratch_map, TRIANGLE_RT_W, TRIANGLE_RT_PX, 0, 0, 0, 0, 0);
    tri_cov_table(ttst, c->map, TRIANGLE_RT_W, TRIANGLE_RT_PX, value, 1, 1, 1, 1);
    struct PM4Builder b;
    tri_begin(&b, c);
    tri_common(&b, c);
    tri_vs(&b, c->vs_full, TRI_VS_FULL_RSRC1, 0);
    pm4_set_uconfig_reg(&b, UCFG_PRIMITIVE_TYPE, 4);
    /* clear draw: no offset, open scissors */
    pm4_set_context_reg(&b, CTX_WINDOW_OFFSET, 0);
    pm4_set_context_reg(&b, 0x08D, 0); /* PA_SU_HARDWARE_SCREEN_OFFSET */
    {
        const uint32_t gb[4] = {0x3F800000u, 0x3F800000u, 0x3F800000u, 0x3F800000u};
        pm4_set_context_regs(&b, 0x2FA, gb, 4);
    }
    tri_scissor(&b, CTX_SCREEN_SCISSOR, 0, 0, TRIANGLE_RT_W, TRIANGLE_RT_H, 0);
    tri_scissor(&b, CTX_WINDOW_SCISSOR, 0, 0, TRIANGLE_OPEN, TRIANGLE_OPEN, 1);
    tri_scissor(&b, CTX_GENERIC_SCISSOR, 0, 0, TRIANGLE_OPEN, TRIANGLE_OPEN, 1);
    tri_scissor(&b, CTX_VIEWPORT_SCISSOR0, 0, 0, TRIANGLE_OPEN, TRIANGLE_OPEN, 1);
    pm4_set_context_reg(&b, CTX_MODE_CONTROL, 0);
    pm4_set_context_reg(&b, CTX_POLYGON_CONTROL, 0);
    tri_ps(&b, c, tclr);
    pm4_draw_index_auto(&b, 3);
    /* the case */
    pm4_set_context_reg(&b, CTX_WINDOW_OFFSET,
                        ((uint32_t)(uint16_t)s->oy << 16) | (uint32_t)(uint16_t)s->ox);
    if (s->hwso) {
        pm4_set_context_reg(&b, 0x08D, (32u << 16) | 60u);
        /* PA_CL_GB_VERT_CLIP_ADJ 60, VERT_DISC 1, HORZ_CLIP 32, HORZ_DISC 1 (the app's) */
        const uint32_t gb[4] = {0x42700000u, 0x3F800000u, 0x42000000u, 0x3F800000u};
        pm4_set_context_regs(&b, 0x2FA, gb, 4);
    }
    tri_scissor(&b, CTX_WINDOW_SCISSOR, win[0], win[1], win[2], win[3], s->dis & 1);
    tri_scissor(&b, CTX_GENERIC_SCISSOR, gen[0], gen[1], gen[2], gen[3], s->dis & 2);
    tri_scissor(&b, CTX_VIEWPORT_SCISSOR0, vp[0], vp[1], vp[2], vp[3], s->dis & 4);
    pm4_set_context_reg(&b, CTX_MODE_CONTROL, vp_en ? 2u : 0u);
    pm4_set_context_reg(&b, CTX_POLYGON_CONTROL, s->vtx ? (1u << 16) : 0u);
    tri_ps(&b, c, ttst);
    pm4_draw_index_auto(&b, 3);
    tri_end(&b, c);
    int r = tri_kick(c, &b);
    if (r == 0)
        r = tri_wait(c);
    if (r < 0)
        return r;
    int m[4], rt[4], mp[4];
    tri_scissor_model(s, m);
    uint32_t nrt = tri_bbox(c->rt, 0xFFFFFFFFu, rt);
    uint32_t nmp = tri_bbox(c->map, value, mp);
    uint32_t nm = (m[0] < m[2] && m[1] < m[3]) ? (uint32_t)((m[2] - m[0]) * (m[3] - m[1])) : 0;
    int rt_ok = nrt == nm &&
                (nm == 0 || (rt[0] == m[0] && rt[1] == m[1] && rt[2] == m[2] && rt[3] == m[3]));
    int mp_ok = nmp == nm &&
                (nm == 0 || (mp[0] == m[0] && mp[1] == m[1] && mp[2] == m[2] && mp[3] == m[3]));
    tri_s(s->id);
    tri_s(": offset ");
    tri_i(s->ox);
    tri_s(",");
    tri_i(s->oy);
    tri_s(" disable w/g/v ");
    tri_u(s->dis & 1);
    tri_u((s->dis >> 1) & 1);
    tri_u((s->dis >> 2) & 1);
    tri_s(" vport_scissor ");
    tri_u((unsigned)vp_en);
    tri_s(" vtx_offset ");
    tri_u(s->vtx);
    tri_s(" hw_screen_offset ");
    tri_u(s->hwso);
    tri_s("\n  window ");
    tri_rect(win);
    tri_s(" generic ");
    tri_rect(gen);
    tri_s(" viewport ");
    tri_rect(vp);
    tri_s("\n  model ");
    tri_rect(m);
    tri_s(" (");
    tri_u(nm);
    tri_s(" px)  target ");
    if (nrt)
        tri_rect(rt);
    else
        tri_s("none");
    tri_s(" (");
    tri_u(nrt);
    tri_s(" px)  map ");
    if (nmp)
        tri_rect(mp);
    else
        tri_s("none");
    tri_s(" (");
    tri_u(nmp);
    tri_s(" px)\n  summary target ");
    tri_s(rt_ok ? "MODEL" : "DIFF");
    tri_s(" map ");
    tri_s(mp_ok ? "MODEL" : "DIFF");
    tri_s("\n\n");
    return 0;
}

/* ==== I: index fetch against a write after the submit ===========================================
 */
struct TriIndexCase {
    char id[24];
    uint8_t pfp, packet, only_last, isize;
    uint32_t offset, count, delay_us;
    uint32_t offset2, count2; /* a second draw after the first, count2 0: none */
};
#define TRI_NI 47
static struct TriIndexCase g_tri_ic[TRI_NI];
static void tri_index_cases(void) {
    static const uint32_t rg[5][2] = {
        {26622, 6144}, {32763, 3}, {24574, 8192}, {16382, 16384}, {0, 6144}};
    int n = 0;
    for (int e = 0; e < 2; e++)
        for (int p = 0; p < 2; p++)
            for (int r = 0; r < 5; r++)
                for (int d = 0; d < 2; d++, n++) {
                    struct TriIndexCase* s = &g_tri_ic[n];
                    my_memset(s, 0, sizeof(*s));
                    int i = 0;
                    const char* a = e ? "I_PFP_" : "I_ME_";
                    while (*a)
                        s->id[i++] = *a++;
                    a = p ? "IDX2_" : "OFS2_";
                    while (*a)
                        s->id[i++] = *a++;
                    s->id[i++] = (char)('0' + r);
                    s->id[i++] = d ? 'd' : '-';
                    s->pfp = (uint8_t)e;
                    s->packet = (uint8_t)p;
                    s->offset = rg[r][0];
                    s->count = rg[r][1];
                    s->delay_us = d ? 2000u : 0u;
                    s->isize = 2;
                }
    static const struct {
        const char* id;
        uint8_t isize;
        uint32_t offset, count, offset2, count2;
    } k_more[7] = {{"I_RP", 2, 26622, 6144, 0, 0},        {"I_P16_8000", 2, 16384, 16384, 0, 0},
                   {"I_P16_8800", 2, 17408, 12288, 0, 0}, {"I_P16_8FFE", 2, 18431, 12288, 0, 0},
                   {"I_P32_4000", 4, 4096, 8192, 0, 0},   {"I_P32_3FFC", 4, 4095, 8192, 0, 0},
                   {"I_2DRAW", 2, 0, 1024, 16384, 12288}};
    for (int m = 0; m < 7; m++, n++) {
        struct TriIndexCase* s = &g_tri_ic[n];
        my_memset(s, 0, sizeof(*s));
        const char* a = k_more[m].id;
        for (int i = 0; *a; i++)
            s->id[i] = *a++;
        s->isize = k_more[m].isize;
        s->offset = k_more[m].offset;
        s->count = k_more[m].count;
        s->offset2 = k_more[m].offset2;
        s->count2 = k_more[m].count2;
        s->only_last = m == 0;
    }
}
/* Page model (doc at the top): how many of count indices from index offset are old. */
static uint32_t tri_page_model(const struct TriIndexCase* s, uint32_t offset, uint32_t count) {
    if (s->pfp)
        return 0;
    uint32_t start = offset * s->isize;
    uint32_t end = (start & ~0xFFFu) + 0x4000u;
    uint32_t n = (end - start) / s->isize;
    return n < count ? n : count;
}
static void tri_wait_reg_mem(struct PM4Builder* b, volatile uint32_t* addr, int pfp) {
    uint64_t a = (uint64_t)(uintptr_t)addr;
    pm4_emit(b, pm4_type3(0x3C, 6));
    pm4_emit(b, (1u << 4) | 3u | (pfp ? 0x100u : 0u)); /* memory, equal, ENGINE */
    pm4_emit(b, (uint32_t)a & 0xFFFFFFFCu);
    pm4_emit(b, (uint32_t)(a >> 32) & 0xFFFFu);
    pm4_emit(b, 1);
    pm4_emit(b, 0xFFFFFFFFu);
    pm4_emit(b, 10);
}
static void tri_index_base(struct PM4Builder* b, const void* p) {
    uint64_t a = (uint64_t)(uintptr_t)p;
    pm4_emit(b, pm4_type3(0x26, 2));
    pm4_emit(b, (uint32_t)(a & 0xFFFFFFFEu));
    pm4_emit(b, (uint32_t)(a >> 32));
}
static uint32_t g_tri_gnm_ofs[9];
/* Counts and logs one draw's indices; returns 1 when they match the page model. */
static int tri_index_report(struct TriangleContext* c, const struct TriIndexCase* s,
                            uint32_t offset, uint32_t count) {
    uint32_t n_old = 0, n_new = 0, n_both = 0, n_none = 0;
    int first_new = -1, last_old = -1;
    tri_s("  runs");
    int runs = 0, prev = -1, run0 = 0;
    for (uint32_t j = 0; j <= count; j++) {
        int st = -2;
        if (j < count) {
            uint32_t i = offset + j;
            int o = c->flags[i] != 0, nw = c->flags[TRIANGLE_NEW + i] != 0;
            if (s->only_last && i != 32765)
                nw = 0;
            st = o | (nw << 1);
            n_old += st == 1;
            n_new += st == 2;
            n_both += st == 3;
            n_none += st == 0;
            if ((st & 2) && first_new < 0)
                first_new = (int)j;
            if (st & 1)
                last_old = (int)j;
        }
        if (st != prev) {
            if (prev >= 0 && runs < 16) {
                static const char* nm[4] = {"none", "old", "new", "both"};
                tri_s(" [");
                tri_u((unsigned)run0);
                tri_s("..");
                tri_u(j);
                tri_s(") ");
                tri_s(nm[prev]);
                runs++;
            }
            prev = st;
            run0 = (int)j;
        }
    }
    tri_s("\n  old ");
    tri_u(n_old);
    tri_s(" new ");
    tri_u(n_new);
    tri_s(" both ");
    tri_u(n_both);
    tri_s(" none ");
    tri_u(n_none);
    tri_s(" first_new ");
    tri_i(first_new);
    tri_s(" last_old ");
    tri_i(last_old);
    uint32_t pm = tri_page_model(s, offset, count);
    if (s->only_last)
        pm = count;
    tri_s("\n  page model old ");
    tri_u(pm);
    tri_s("\n  summary ");
    tri_s(n_none ? "MISSING" : n_new == 0 ? "ALL OLD" : n_old == 0 ? "ALL NEW" : "SPLIT");
    int model =
        n_none == 0 && n_both == 0 && n_old == pm && n_new == (s->only_last ? 0 : count - pm);
    tri_s(model ? " MODEL" : " DIFF");
    tri_s("\n");
    return model;
}
static int tri_index_case(struct TriangleContext* c, int k) {
    const struct TriIndexCase* s = &g_tri_ic[k];
    uint32_t* ib32 = (uint32_t*)c->ib;
    if (s->isize == 4) {
        for (uint32_t i = 0; i < TRIANGLE_INDICES / 2; i++)
            ib32[i] = i;
    } else {
        for (uint32_t i = 0; i < TRIANGLE_INDICES; i++)
            c->ib[i] = (uint16_t)i;
    }
    my_memset(c->flags, 0, 65536 * 4);
    c->fence[16] = 0;
    c->fence[32] = 0;
    uint32_t* t = c->tab + 0x3C00;
    tri_vsharp(t, (uint64_t)(uintptr_t)c->flags, 0, 65536 * 4, 0, 4, 4);
    struct PM4Builder b;
    tri_begin(&b, c);
    tri_common(&b, c);
    pm4_set_context_reg(&b, CTX_WINDOW_OFFSET, 0);
    tri_scissor(&b, CTX_SCREEN_SCISSOR, 0, 0, TRIANGLE_RT_W, TRIANGLE_RT_H, 0);
    tri_scissor(&b, CTX_WINDOW_SCISSOR, 0, 0, TRIANGLE_OPEN, TRIANGLE_OPEN, 1);
    tri_scissor(&b, CTX_GENERIC_SCISSOR, 0, 0, TRIANGLE_OPEN, TRIANGLE_OPEN, 1);
    pm4_set_context_reg(&b, CTX_POLYGON_CONTROL, 0);
    tri_vs(&b, c->vs_index, TRI_VS_INDEX_RSRC1, 2);
    tri_ptr_ud(&b, SH_VS_USER_DATA_0, t);
    tri_ps(&b, c, c->tab + 0x3F00);
    tri_cov_table(c->tab + 0x3F00, c->scratch_map, TRIANGLE_RT_W, TRIANGLE_RT_PX, 0, 0, 0, 0, 0);
    pm4_set_uconfig_reg(&b, UCFG_PRIMITIVE_TYPE, 1);
    pm4_emit(&b, pm4_type3(0x2A, 1)); /* INDEX_TYPE: the app's setIndexSize(16, 0), or 32-bit */
    pm4_emit(&b, (1u << 10) | (s->isize == 4 ? 1u : 0u));
    pm4_write_data_dword(&b, c->fence + 32, 1); /* the CP has reached the wait */
    tri_wait_reg_mem(&b, c->fence + 16, s->pfp);
    if (s->packet == 0) {
        tri_index_base(&b, c->ib);
        if (pm4_have_space(&b, 9)) {
            sceGnmDrawIndexOffset(b.buf + b.off, 9, s->offset, s->count, 0);
            for (int i = 0; i < 9; i++)
                g_tri_gnm_ofs[i] = b.buf[b.off + i];
            b.off += 9;
        } else {
            b.overflow++;
        }
        if (s->count2) {
            if (pm4_have_space(&b, 9)) {
                sceGnmDrawIndexOffset(b.buf + b.off, 9, s->offset2, s->count2, 0);
                b.off += 9;
            } else {
                b.overflow++;
            }
        }
    } else {
        pm4_draw_index_2(&b, s->count, (uint64_t)(uintptr_t)(c->ib + s->offset), s->count);
    }
    tri_end(&b, c);
    int r = tri_kick(c, &b);
    if (r < 0)
        return r;
    uint64_t t0 = sceKernelGetProcessTime();
    while (c->fence[32] != 1) {
        if (sceKernelGetProcessTime() - t0 > TRIANGLE_TIMEOUT_US) {
            c->fence[16] = 1; /* release it anyway */
            tri_wait(c);
            return -1;
        }
    }
    uint64_t tr = sceKernelGetProcessTime();
    if (s->delay_us)
        sceKernelUsleep(s->delay_us);
    uint64_t t1 = sceKernelGetProcessTime();
    if (s->only_last) {
        c->ib[32765] = (uint16_t)(TRIANGLE_NEW + 32765);
    } else if (s->isize == 4) {
        for (uint32_t i = s->offset; i < s->offset + s->count; i++)
            ib32[i] = TRIANGLE_NEW + i;
    } else {
        for (uint32_t i = s->offset; i < s->offset + s->count; i++)
            c->ib[i] = (uint16_t)(TRIANGLE_NEW + i);
        for (uint32_t i = s->offset2; i < s->offset2 + s->count2; i++)
            c->ib[i] = (uint16_t)(TRIANGLE_NEW + i);
    }
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    c->fence[16] = 1;
    uint64_t t2 = sceKernelGetProcessTime();
    r = tri_wait(c);
    if (r < 0)
        return r;
    tri_s(s->id);
    tri_s(": engine ");
    tri_s(s->pfp ? "PFP" : "ME");
    tri_s(s->packet ? " DRAW_INDEX_2" : " DRAW_INDEX_OFFSET_2");
    tri_s(s->isize == 4 ? " 32-bit" : " 16-bit");
    tri_s(" offset ");
    tri_u(s->offset);
    tri_s(" (byte ");
    tri_x(s->offset * s->isize, 4);
    tri_s(") count ");
    tri_u(s->count);
    tri_s(s->only_last ? " (last index changed)" : "");
    tri_s("\n  submit->wait reached ");
    tri_u(tr - t0);
    tri_s(" us, delay ");
    tri_u(t1 - tr);
    tri_s(" us, writes ");
    tri_u(t2 - t1);
    tri_s(" us\n");
    if (s->packet == 0) {
        tri_s("  sceGnmDrawIndexOffset dwords");
        for (int i = 0; i < 9; i++) {
            tri_s(" ");
            tri_x(g_tri_gnm_ofs[i], 8);
        }
        tri_s("\n");
    }
    tri_index_report(c, s, s->offset, s->count);
    if (s->count2) {
        tri_s("  second draw offset ");
        tri_u(s->offset2);
        tri_s(" (byte ");
        tri_x(s->offset2 * s->isize, 4);
        tri_s(") count ");
        tri_u(s->count2);
        tri_s(" (page model: as if prefetched like the first)\n");
        tri_index_report(c, s, s->offset2, s->count2);
    }
    tri_s("\n");
    return 0;
}

/* ==== RP: the app's frame ======================================================================
 */
static int g_tri_neo;
static void tri_ctx(struct PM4Builder* b, uint32_t reg, uint32_t v) {
    pm4_set_context_reg(b, reg - 0xA000u, v);
}
static int tri_gnm(struct PM4Builder* b, uint32_t dw) {
    if (!pm4_have_space(b, dw)) {
        b->overflow++;
        return 0;
    }
    return 1;
}
/* The app's command buffer (main.cpp 1666-1813) with its render target at fb. */
static void tri_rp_build(struct PM4Builder* b, struct TriangleContext* c, void* fb) {
    pm4_init(b, c->dcb, TRIANGLE_DCB_DW);
    {
        int w = sceGnmDrawInitDefaultHardwareState350(b->buf + b->off, 0x100);
        if (w > 0)
            b->off += (uint32_t)w;
    }
    {
        uint32_t base = (uint32_t)((uint64_t)(uintptr_t)fb >> 8);
        uint32_t r[14] = {base,
                          (DISPLAY_W / 8) - 1,
                          (DISPLAY_W * DISPLAY_H / 64) - 1,
                          0,
                          CB_INFO_DISPLAY_SRGB,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0xFF0000u,
                          0,
                          0};
        pm4_set_context_regs(b, CTX_CB_COLOR0_BASE, r, 14);
        pm4_emit(b, 0xC0001000u);
        pm4_emit(b, (uint32_t)DISPLAY_W | ((uint32_t)DISPLAY_H << 16));
    }
    tri_ctx(b, 0xA08E, 0xF);
    {
        float sc[3] = {DISPLAY_W * 0.5f, (0.0f - DISPLAY_H) * 0.5f, 0.5f};
        float of[3] = {DISPLAY_W * 0.5f, DISPLAY_H * 0.5f, 0.5f};
        uint32_t z[2] = {0x00000000u, 0x3F800000u};
        pm4_set_context_regs(b, 0xB4, z, 2);
        pm4_emit(b, pm4_type3(PM4_SET_CONTEXT_REG, 7));
        pm4_emit(b, 0x10F);
        pm4_emit_f(b, sc[0]);
        pm4_emit_f(b, of[0]);
        pm4_emit_f(b, sc[1]);
        pm4_emit_f(b, of[1]);
        pm4_emit_f(b, sc[2]);
        pm4_emit_f(b, of[2]);
    }
    tri_ctx(b, 0xA206, 0x43F);
    tri_scissor(b, CTX_SCREEN_SCISSOR, 0, 0, DISPLAY_W, DISPLAY_H, 0);
    tri_ctx(b, 0xA080, 100u);
    tri_scissor(b, CTX_GENERIC_SCISSOR, 0, 100, DISPLAY_W, DISPLAY_H, 0);
    tri_scissor(b, CTX_WINDOW_SCISSOR, 0, 0, DISPLAY_W - 100, DISPLAY_H, 1);
    tri_ctx(b, 0xA292, 2);
    tri_scissor(b, CTX_VIEWPORT_SCISSOR0, 0, 0, DISPLAY_W, DISPLAY_H - 100, 1);
    tri_ctx(b, 0xA08D, (32u << 16) | 60u);
    {
        const uint32_t gb[4] = {0x42700000u, 0x3F800000u, 0x42000000u, 0x3F800000u};
        pm4_set_context_regs(b, 0x2FA, gb, 4);
    }
    if (tri_gnm(b, 29 + 40)) {
        sceGnmSetEmbeddedVsShader(b->buf + b->off, 29, 0, 0);
        b->off += 29;
        sceGnmSetEmbeddedPsShader(b->buf + b->off, 40, 0);
        b->off += 40;
    }
    pm4_emit(b, pm4_type3(PM4_SET_UCONFIG_REG, 2));
    pm4_emit(b, ((uint32_t)(g_tri_neo == 1) << 28) | 0x242u);
    pm4_emit(b, 17); /* RECT_LIST */
    if (tri_gnm(b, 7)) {
        sceGnmDrawIndexAuto(b->buf + b->off, 7, 3, 0);
        b->off += 7;
    }
    /* waitForGraphicsWrites(0, 1, 0, WB_INV_L1_L2, FLUSH_INV_CB, stall disabled) */
    pm4_emit(b, 0xC0055800u);
    pm4_emit(b, (1u << 31) | (0x02000000u & 0x7FFFFFFFu) | ((0x38u & 0x30u) << 18) |
                    ((0x38u & 0xBu) << 15));
    pm4_emit(b, 1);
    pm4_emit(b, 0);
    pm4_emit(b, 0);
    pm4_emit(b, 0);
    pm4_emit(b, 10);
    {
        uint64_t v = (uint64_t)(uintptr_t)c->rp_vs, p = (uint64_t)(uintptr_t)c->rp_ps;
        uint32_t vs[7] = {
            (uint32_t)(v >> 8), (uint32_t)(v >> 40), 0x002C0002u, 0x00000008u, 0, 4, 0};
        uint32_t ps[12] = {
            (uint32_t)(p >> 8), (uint32_t)(p >> 40), 0x002C0000u, 0, 0, 4, 2, 2, 1, 0, 0x10u, 0xF};
        if (tri_gnm(b, 29 + 40)) {
            sceGnmSetVsShader(b->buf + b->off, 29, vs, 0);
            b->off += 29;
            sceGnmSetPsShader350(b->buf + b->off, 40, ps);
            b->off += 40;
        }
    }
    tri_ctx(b, 0xA191, 0);
    tri_ptr_ud(b, SH_VS_USER_DATA_0, c->rp_fetch);
    tri_ptr_ud(b, SH_VS_USER_DATA_0 + 2, c->rp_vsharps);
    pm4_set_sh_regs(b, SH_PS_USER_DATA_0, c->rp_test_vsharp, 4);
    pm4_emit(b, pm4_type3(PM4_SET_UCONFIG_REG, 2));
    pm4_emit(b, ((uint32_t)(g_tri_neo == 1) << 28) | 0x242u);
    pm4_emit(b, 4);
    if (tri_gnm(b, 7)) {
        sceGnmDrawIndexAuto(b->buf + b->off, 7, 3, 0);
        b->off += 7;
    }
    pm4_emit(b, 0xC0055800u);
    pm4_emit(b, (1u << 31) | (0x02000000u & 0x7FFFFFFFu) | ((0x38u & 0x30u) << 18) |
                    ((0x38u & 0xBu) << 15));
    pm4_emit(b, 1);
    pm4_emit(b, 0);
    pm4_emit(b, 0);
    pm4_emit(b, 0);
    pm4_emit(b, 10);
    pm4_emit(b, pm4_type3(0x2A, 1));
    pm4_emit(b, 1u << 10);
    tri_wait_reg_mem(b, c->rp_wait, 0);
    tri_index_base(b, c->rp_index);
    if (tri_gnm(b, 9)) {
        sceGnmDrawIndexOffset(b->buf + b->off, 9, 32766 - 6144, 6144, 0);
        b->off += 9;
    }
    tri_end(b, c);
}
static void tri_rp_setup(struct TriangleContext* c) {
    my_memcpy(c->rp_vertices, k_rp_vertices, sizeof(k_rp_vertices));
    my_memcpy(c->rp_vindex, k_rp_vindex, sizeof(k_rp_vindex));
    uint64_t v = (uint64_t)(uintptr_t)c->rp_vertices;
    tri_vsharp(c->rp_vsharps, v + 1, 20, 5, TRI_DST_XY01, 7, 11);
    tri_vsharp(c->rp_vsharps + 4, v + 8, 20, 5, TRI_DST_XYZ0, 7, 13);
    /* its PS V# (11_11_10 FLOAT, one record); the pixel shader does not read it */
    c->rp_test[0] = 0;
    tri_vsharp(c->rp_test_vsharp, (uint64_t)(uintptr_t)c->rp_test, 4, 1, TRI_DST_XYZ0, 7, 7);
    my_memset(c->rp_index, 0, 0x10000);
    c->rp_index[10921 * 3 + 0] = 1;
    c->rp_index[10921 * 3 + 1] = 4;
    c->rp_index[10921 * 3 + 2] = 3;
    *c->rp_wait = 0;
}
/* Draws the frame into fb; 0 done, -1 timeout, -2 not submitted. */
static int tri_rp_draw(struct TriangleContext* c, void* fb) {
    uint32_t* p = (uint32_t*)fb;
    for (long i = 0; i < (long)DISPLAY_W * DISPLAY_H; i++)
        p[i] = 0xFFFFFFFFu;
    tri_rp_setup(c);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    struct PM4Builder b;
    tri_rp_build(&b, c, fb);
    int r = tri_kick(c, &b);
    if (r < 0)
        return r;
    c->rp_vindex[6] = 0xF;
    c->rp_index[10921 * 3 + 2] = 0xF;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    *c->rp_wait = 1;
    return tri_wait(c);
}
static int tri_is_black(uint32_t v) {
    return (v & 0x00FFFFFFu) == 0;
}
static int tri_is_colour(uint32_t v) {
    uint32_t r = (v >> 16) & 0xFF, g = (v >> 8) & 0xFF, bl = v & 0xFF;
    return !(r == g && g == bl);
}
static void tri_rp_report(const uint32_t* p) {
    int bb[4] = {DISPLAY_W, DISPLAY_H, 0, 0}, cb[4] = {DISPLAY_W, DISPLAY_H, 0, 0};
    uint32_t nw = 0, nb = 0, nc = 0, no = 0;
    for (int y = 0; y < DISPLAY_H; y++)
        for (int x = 0; x < DISPLAY_W; x++) {
            uint32_t v = p[(long)y * DISPLAY_W + x];
            int* r = 0;
            if (v == 0xFFFFFFFFu)
                nw++;
            else if (tri_is_black(v))
                nb++, r = bb;
            else if (tri_is_colour(v))
                nc++, r = cb;
            else
                no++;
            if (r) {
                if (x < r[0])
                    r[0] = x;
                if (y < r[1])
                    r[1] = y;
                if (x + 1 > r[2])
                    r[2] = x + 1;
                if (y + 1 > r[3])
                    r[3] = y + 1;
            }
        }
    tri_s("pixels: white ");
    tri_u(nw);
    tri_s(" black ");
    tri_u(nb);
    tri_s(" coloured ");
    tri_u(nc);
    tri_s(" other ");
    tri_u(no);
    tri_s("\nblack bbox ");
    tri_rect(bb);
    tri_s(" (model 100,100-1820,980)\ncoloured bbox ");
    tri_rect(cb);
    tri_s("\n");
    static const int rows[3] = {300, 630, 900}, cols[3] = {600, 960, 1360};
    for (int k = 0; k < 3; k++) {
        int y = rows[k], first = -1, last = -1, cf = -1, cl = -1;
        for (int x = 0; x < DISPLAY_W; x++) {
            uint32_t v = p[(long)y * DISPLAY_W + x];
            if (v != 0xFFFFFFFFu) {
                if (first < 0)
                    first = x;
                last = x;
            }
            if (tri_is_colour(v)) {
                if (cf < 0)
                    cf = x;
                cl = x;
            }
        }
        tri_s("row ");
        tri_u((unsigned)y);
        tri_s(": not white ");
        tri_i(first);
        tri_s("..");
        tri_i(last);
        tri_s(" coloured ");
        tri_i(cf);
        tri_s("..");
        tri_i(cl);
        tri_s("\n");
    }
    for (int k = 0; k < 3; k++) {
        int x = cols[k], first = -1, last = -1, cf = -1, cl = -1;
        for (int y = 0; y < DISPLAY_H; y++) {
            uint32_t v = p[(long)y * DISPLAY_W + x];
            if (v != 0xFFFFFFFFu) {
                if (first < 0)
                    first = y;
                last = y;
            }
            if (tri_is_colour(v)) {
                if (cf < 0)
                    cf = y;
                cl = y;
            }
        }
        tri_s("column ");
        tri_u((unsigned)x);
        tri_s(": not white ");
        tri_i(first);
        tri_s("..");
        tri_i(last);
        tri_s(" coloured ");
        tri_i(cf);
        tri_s("..");
        tri_i(cl);
        tri_s("\n");
    }
    /* triangle 1 centroid (960, 630), triangle 2 (1360, 450), the rect's corners, outside */
    static const int sp[9][2] = {{960, 630},  {1360, 450}, {100, 100},   {99, 99},   {1819, 979},
                                 {1820, 980}, {10, 10},    {1000, 1050}, {1900, 500}};
    tri_s("samples");
    for (int k = 0; k < 9; k++) {
        tri_s(" (");
        tri_u((unsigned)sp[k][0]);
        tri_s(",");
        tri_u((unsigned)sp[k][1]);
        tri_s(")=");
        tri_x(p[(long)sp[k][1] * DISPLAY_W + sp[k][0]], 8);
    }
    tri_s("\n");
}
static void tri_show(int video) {
    sceVideoOutSubmitFlip(video, 0, 1, 0);
    for (int i = 0; i < TRIANGLE_SHOW_S * 10; i++)
        sceKernelUsleep(100000);
}

/* ==== run ======================================================================================
 */
#define TRI_STEP_MEM 0
#define TRI_STEP_F0 1
#define TRI_STEP_MC (TRI_STEP_F0 + TRI_NFMT + 1)
#define TRI_STEP_RAW (TRI_STEP_MC + 1)
#define TRI_STEP_S0 (TRI_STEP_RAW + 1)
#define TRI_STEP_I0 (TRI_STEP_S0 + TRI_NS)
#define TRI_STEP_RP (TRI_STEP_I0 + TRI_NI)
#define TRI_STEPS (TRI_STEP_RP + 1)
static void tri_step_name(int k) {
    if (k == TRI_STEP_MEM)
        tri_s("MEM");
    else if (k == TRI_STEP_MC)
        tri_s("MC");
    else if (k == TRI_STEP_RAW)
        tri_s("RAW");
    else if (k < TRI_STEP_MC) {
        tri_s("F_");
        tri_s(k - TRI_STEP_F0 < TRI_NFMT ? k_tri_fmt[k - TRI_STEP_F0].name : "RP");
    } else if (k < TRI_STEP_I0)
        tri_s(g_tri_sc[k - TRI_STEP_S0].id);
    else if (k < TRI_STEP_RP)
        tri_s(g_tri_ic[k - TRI_STEP_I0].id);
    else
        tri_s("RP");
}
/* Returns 0, or -1 when the GPU hung (the caller stops the app). */
static int triangle_run(int video, void* fb0) {
    struct TriangleState st;
    if (!tri_paths()) {
        trace_msg("triangle: no folder takes a file (log / state)\n");
        return 0;
    }
    tri_state_load(&st);
    tri_scissor_cases();
    tri_index_cases();
    g_tri_neo = sceKernelIsNeoMode();
    struct TriangleContext* c = &g_tri;
    if (!triangle_alloc(c)) {
        trace_msg("triangle: allocation failed\n");
        return 0;
    }
    int fresh = st.next == 0 && st.inflight < 0;
    if (st.next < TRI_STEPS || st.inflight >= 0) {
        g_tri_fd = sceKernelOpen(g_tri_log_path, fresh ? 0x601 : 0x209, 0x1FF);
        if (g_tri_fd < 0) {
            trace_msg("triangle: log open failed\n");
            return 0;
        }
        tri_s(
            fresh
                ? "# OpenOrbis Gnm Triangle (red_prig) rebuilt on ShadCube4, build " TRIANGLE_BUILD
                  "\n\n"
                : "\n## resumed\n\n");
        if (st.inflight >= 0) {
            tri_s("### ");
            tri_step_name(st.inflight);
            tri_s(": CRASHED or HUNG - the app ended before its fence; skipped\nsummary ");
            tri_step_name(st.inflight);
            tri_s(": CRASHED\n\n");
            if (st.inflight == TRI_STEP_RP)
                st.rp_bad = 1;
            st.next = st.inflight + 1;
            st.inflight = -1;
            tri_state_save(&st);
        }
        tri_flush();
        for (int k = st.next; k < TRI_STEPS; k++) {
            st.inflight = k;
            st.next = k;
            tri_state_save(&st);
            int r = 0;
            if (k == TRI_STEP_MEM) {
                tri_mem_report(st.mem);
            } else if (k == TRI_STEP_MC) {
                r = tri_mc_step(c);
            } else if (k == TRI_STEP_RAW) {
                r = tri_raw_step(c);
            } else if (k < TRI_STEP_MC) {
                if (k == TRI_STEP_F0)
                    tri_s("### F: vertex fetch with a V# base 0..3 bytes past a dword\n\n");
                r = tri_fetch_group(c, k - TRI_STEP_F0);
            } else if (k < TRI_STEP_I0) {
                if (k == TRI_STEP_S0)
                    tri_s("### S: scissors and window offset (256 x 128)\n\n");
                r = tri_scissor_case(c, k - TRI_STEP_S0);
            } else if (k < TRI_STEP_RP) {
                if (k == TRI_STEP_I0)
                    tri_s("### I: index fetch against a CPU write after the submit\n\n");
                r = tri_index_case(c, k - TRI_STEP_I0);
            } else {
                tri_s("### RP: the app's frame in fb 0\n");
                r = tri_rp_draw(c, fb0);
                if (r == 0)
                    tri_rp_report((const uint32_t*)fb0);
            }
            if (r < 0) {
                tri_s(r == -1 ? "TIMEOUT: no fence after 2 s - GPU hung; close the app and launch "
                                "it again\nsummary "
                              : "NOT SUBMITTED\nsummary ");
                tri_step_name(k);
                tri_s(r == -1 ? ": HUNG\n\n" : ": NOT SUBMITTED\n\n");
                if (k == TRI_STEP_RP)
                    st.rp_bad = 1;
                st.inflight = -1;
                st.next = k + 1;
                tri_state_save(&st);
                tri_flush();
                if (r == -1)
                    return -1;
                continue;
            }
            st.inflight = -1;
            st.next = k + 1;
            tri_state_save(&st);
            tri_flush();
            if (k == TRI_STEP_RP)
                tri_show(video);
        }
        tri_s("## all steps done\n");
        tri_flush();
        sceKernelClose(g_tri_fd);
        g_tri_fd = -1;
        return 0;
    }
    trace_msg("triangle: all steps done earlier; delete triangle.state to run them again\n");
    if (st.rp_bad)
        return 0;
    st.inflight = TRI_STEP_RP;
    tri_state_save(&st);
    int r = tri_rp_draw(c, fb0);
    st.inflight = -1;
    if (r == -1) {
        st.rp_bad = 1;
        tri_state_save(&st);
        return -1;
    }
    tri_state_save(&st);
    if (r == 0)
        tri_show(video);
    return 0;
}
