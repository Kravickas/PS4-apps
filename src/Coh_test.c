// SPDX-License-Identifier: GPL-2.0-or-later
// PS4 GPU->memory coherence test (standalone). Build with OpenOrbis.
// Verdict is shown as a full-screen color:
//   GREEN  pass         : DMA-filled 0 stayed in the untouched center -> RT reflects a non-render GPU write
//   RED    fail         : center != 0 after the fill -> not reflected
//   BLUE   inconclusive : fill/readback broken (sanity gate) -> ignore pass/fail
//   MAGENTA alloc failed
// Raw hex also written to /data/trace.log.
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
int  sceKernelAllocateDirectMemory(int64_t, int64_t, uint64_t, uint64_t, int, long*);
int  sceKernelMapDirectMemory(void**, uint64_t, int, int, long, uint64_t);
int  sceKernelOpen(const char*, int, int);
long sceKernelWrite(int, const void*, unsigned long);
int  sceKernelFsync(int);
int  sceKernelUsleep(unsigned int);
int  sceVideoOutOpen(int, int, int, const void*);
int  sceVideoOutSetFlipRate(int, int);
void sceVideoOutSetBufferAttribute(void*, unsigned int, unsigned int, unsigned int,
                                   unsigned int, unsigned int, unsigned int);
int  sceVideoOutRegisterBuffers(int, int, void* const*, int, const void*);
int  sceVideoOutSubmitFlip(int, int, unsigned int, long);
int  sceGnmSubmitCommandBuffers(unsigned int, void**, unsigned int*, void**, unsigned int*);
int  sceGnmSubmitDone(void);
#ifdef __cplusplus
}
#endif

#define DISPLAY_W       1920
#define DISPLAY_H       1080
#define NUM_FRAMES      2
#define DCB_SIZE        0x20000
#define PROT_CPU_RW     0x03
#define PROT_GPU_RW     0x30
#define MEM_TYPE_FLEX   0x03

/* --- gradient PS (reads position, RSRC per RT_TEST) --- */
static const uint32_t ps_grad_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000000, 0x7E0A02FF, 0x3A005ADF,
    0x10000B02, 0x7E0A02FF, 0x3A2FF2E5, 0x10020B03,
    0x7E0C0280, 0x7E0E02F2, 0xF800080F, 0x07060100,
    0xBF810000, 0x5362724F, 0x00726468, 0x00003400,
    0x00000000, 0xDEADBEEF, 0xCAFE0E03, 0x00000000,
};

/* --- fullscreen-triangle VS (position only, no vertex fetch) --- */
static const uint32_t vs_fulltri_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000000, 0x34020081, 0x36020282,
    0x36040082, 0x7E020D01, 0x7E040D02, 0x7E0602F4,
    0x10020701, 0xD2080001, 0x0001E501, 0x10040702,
    0xD2080002, 0x0001E502, 0x7E060280, 0x7E0802F2,
    0xF80008CF, 0x04030201, 0xBF810000, 0x5362724F,
    0x00726468, 0x00004C00, 0x00000000, 0xDEADBEEF,
    0xCAFE0E02, 0x00000000,
};

static int   g_log_fd         = -1;
static void *g_vs_fulltri_gpu = 0;
static void *g_ps_grad_gpu    = 0;

#include "pm4.h"

static void my_memset(void *d, int v, unsigned long n) {
    unsigned char *p = (unsigned char *)d;
    for (unsigned long i = 0; i < n; i++) p[i] = (unsigned char)v;
}
static void my_memcpy(void *dst, const void *src, unsigned long n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (unsigned long i = 0; i < n; i++) d[i] = s[i];
}

static unsigned long lg_len(const char *s){ unsigned long n=0; while(s[n]) n++; return n; }
static int lg_hex(char *o, unsigned long long v){
    const char *h="0123456789abcdef"; char t[16]; int i=0,j=0;
    o[j++]='0'; o[j++]='x';
    if(v==0){ o[j++]='0'; return j; }
    while(v){ t[i++]=h[v&0xf]; v>>=4; }
    while(i) o[j++]=t[--i];
    return j;
}

/* trace_init: open the log once (WRONLY|CREAT|TRUNC = 0x601), trying several
   paths. Keeps the fd open for the whole run. */
static void trace_init(void){
    const char *paths[] = { "/data/trace.log", "/data/CUBETST00/trace.log",
                            "trace.log", "/mnt/sandbox/BREW00001/data/trace.log", 0 };
    for (int i=0; paths[i]; i++){
        int fd = sceKernelOpen(paths[i], 0x601, 0x1FF);
        if (fd >= 0){ g_log_fd = fd; return; }
    }
}
/* trace_line: write one already-built line and fsync immediately. */
static void trace_line(const char *buf, unsigned long n){
    if (g_log_fd < 0) return;
    sceKernelWrite(g_log_fd, buf, n);
    sceKernelFsync(g_log_fd);
}
/* trace_msg: write a plain string + fsync. */
static void trace_msg(const char *s){ trace_line(s, lg_len(s)); }

static void *gpu_alloc(unsigned long size, unsigned long align) {
    long phys = 0; void *addr = 0;
    size = (size + 0x3FFF) & ~0x3FFFUL;
    if (align < 0x4000) align = 0x4000;
    if (sceKernelAllocateDirectMemory(0, 0x600000000ULL, size, align, MEM_TYPE_FLEX, &phys)) return 0;
    if (sceKernelMapDirectMemory(&addr, size, PROT_CPU_RW | PROT_GPU_RW, 0, phys, align)) return 0;
    my_memset(addr, 0, size);
    return addr;
}

static uint32_t build_coh_render(struct PM4Builder *b, void *color, void *ps_gpu,
                                 uint32_t sc_w, uint32_t sc_h,
                                 volatile uint32_t *fence, uint32_t fv) {
    pm4_init_default_hw_state(b);
    pm4_context_control(b);
    /* VS = fulltri (position only): RSRC1=1, RSRC2=0. */
    { uint64_t a=(uint64_t)(uintptr_t)g_vs_fulltri_gpu;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),1u,0u};
      pm4_set_sh_regs(b,SH_VS_PGM_LO,r,4); }
    /* Scissors clip coverage to sc_w x sc_h at origin; rest stays memory. */
    { uint32_t s[2]={0,(sc_w&0x7FFF)|((sc_h&0x7FFF)<<16)};
      pm4_set_context_regs(b,CTX_SCREEN_SCISSOR,s,2);
      pm4_set_context_regs(b,CTX_GENERIC_SCISSOR,s,2);
      pm4_set_context_regs(b,CTX_VIEWPORT_SCISSOR0,s,2);
      s[0]=(1u<<31);
      pm4_set_context_regs(b,CTX_WINDOW_SCISSOR,s,2); }
    pm4_emit(b,pm4_type3(PM4_SET_CONTEXT_REG,7));
    pm4_emit(b,CTX_VIEWPORT0);
    pm4_emit_f(b,(float)DISPLAY_W*0.5f); pm4_emit_f(b,(float)DISPLAY_W*0.5f);
    pm4_emit_f(b,(float)DISPLAY_H*-0.5f); pm4_emit_f(b,(float)DISPLAY_H*0.5f);
    pm4_emit_f(b,1.0f); pm4_emit_f(b,0.0f);
    pm4_set_context_reg(b,CTX_INDEX_OFFSET,0);
    /* Depth fully disabled (SIMPLE_DRAW). */
    pm4_set_context_reg(b,CTX_DEPTH_RENDER_CONTROL,0);
    pm4_set_context_reg(b,CTX_DB_Z_INFO,0);
    pm4_set_context_reg(b,CTX_DB_STENCIL_INFO,0);
    pm4_set_context_reg(b,CTX_DEPTH_CONTROL,0);
    pm4_set_context_reg(b,CTX_POLYGON_CONTROL,0);
    /* CB_COLOR0 -> target (linear BGRA8, info=0x09A8), 14 regs. */
    { uint32_t c=(uint32_t)((uint64_t)(uintptr_t)color>>8);
      uint32_t r[14]={c,(DISPLAY_W/8)-1,(DISPLAY_W*DISPLAY_H/64)-1,0,
        0x09A8u,0,0,0,0,0,0,0,0,0};
      pm4_set_context_regs(b,CTX_CB_COLOR0_BASE,r,14);
      pm4_emit(b,0xC0001000u); pm4_emit(b,DISPLAY_W|(DISPLAY_H<<16)); }
    pm4_set_context_reg(b,CTX_COLOR_TARGET_MASK,0xF);
    pm4_set_context_reg(b,CTX_COLOR_SHADER_MASK,0xF);
    /* PS inputs: fulltri exports position; gradient PS reads pos (0x302). */
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0,0);
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONFIG,0);
    pm4_set_context_reg(b,CTX_PS_INPUT_ENA,0x302);
    pm4_set_context_reg(b,CTX_PS_INPUT_ADDR,0x302);
    pm4_set_context_reg(b,CTX_NUM_INTERP,0);
    /* PS = gradient: RSRC1=(0<<6)|2, RSRC2=(2<<1). */
    { uint64_t a=(uint64_t)(uintptr_t)ps_gpu;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),(0u<<6)|2u,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4); }
    pm4_set_uconfig_reg(b,UCFG_PRIMITIVE_TYPE,4);   /* trilist */
    pm4_set_uconfig_reg(b,UCFG_NUM_INSTANCES,1);
    pm4_draw_index_auto(b,3);
    pm4_event_write_eop_flush(b,fence,fv);
    return b->off*4;
}

static uint32_t build_coh_fill(struct PM4Builder *b, void *dst, unsigned long bytes,
                               uint32_t value, volatile uint32_t *fence, uint32_t fv) {
    pm4_init_default_hw_state(b);
    const unsigned long CHUNK=0x100000;   /* DMA_DATA byte count is 21-bit */
    unsigned long off=0;
    while(off<bytes){ unsigned long n=bytes-off; if(n>CHUNK)n=CHUNK;
        pm4_dma_fill(b,(char*)dst+off,(uint32_t)n,value); off+=n; }
    pm4_event_write_eop_flush(b,fence,fv);
    return b->off*4;
}

static void coh_submit(uint32_t *dcb, uint32_t sz, volatile uint32_t *fence, uint32_t fv){
    const uint32_t *a[1]={dcb}; uint32_t s[1]={sz};
    sceGnmSubmitCommandBuffers(1,(void**)a,s,0,0);
    sceGnmSubmitDone();
    for(int w=0; w<200000 && *fence!=fv; w++) sceKernelUsleep(50);
    sceKernelUsleep(2000);   /* let any in-flight L2 writeback drain before CPU read */
}

static void coh_log(const char *label, uint32_t v){
    char o[80]; unsigned n=0; const char *p=label;
    while(*p) o[n++]=*p++;
    n+=(unsigned)lg_hex(o+n,(unsigned long long)v);
    o[n++]='\n'; trace_line(o,n);
}

/* Verdict code (also drives the on-screen color in main):
 *   0 PASS         center stayed 0 after the fill -> RT reflects the GPU write
 *   1 FAIL         center !=0 after the fill      -> write NOT reflected
 *   2 INCONCLUSIVE render/fill/readback broken (s1==0 or s2!=0) -> test invalid
 *   3 ALLOC        target allocation failed
 */
static int run_coherence_test(uint32_t *dcb, volatile uint32_t *fence){
    unsigned long bytes=(unsigned long)DISPLAY_W*DISPLAY_H*4;
    void *M=gpu_alloc(bytes,0x100000);
    struct PM4Builder pm4;
    volatile uint32_t *bg=(volatile uint32_t*)M;
    unsigned long ci=(unsigned long)(DISPLAY_H/2)*DISPLAY_W + DISPLAY_W/2; /* center bg */
    unsigned long co=(unsigned long)10*DISPLAY_W + 10;                     /* inside corner */
    uint32_t s1, s2, fin;

    trace_msg("COH start\n");
    if(!M){ trace_msg("COH alloc failed\n"); return 3; }

    /* Stage 1: establish nonzero — fulltri gradient over the whole target. */
    *fence=0; pm4_init(&pm4,dcb,DCB_SIZE/4);
    coh_submit(dcb,build_coh_render(&pm4,M,g_ps_grad_gpu,DISPLAY_W,DISPLAY_H,fence,1),fence,1);
    s1=bg[ci]; coh_log("COH s1 center=",s1);

    /* Stage 2: NON-render GPU write — CP DMA fill the target memory with 0. */
    *fence=0; pm4_init(&pm4,dcb,DCB_SIZE/4);
    coh_submit(dcb,build_coh_fill(&pm4,M,bytes,0u,fence,2),fence,2);
    s2=bg[ci]; coh_log("COH s2 after-fill=",s2);

    /* Stage 3: re-render, drawing ONLY a 64x64 corner; center untouched. */
    *fence=0; pm4_init(&pm4,dcb,DCB_SIZE/4);
    coh_submit(dcb,build_coh_render(&pm4,M,g_ps_grad_gpu,64,64,fence,3),fence,3);
    fin=bg[ci]; coh_log("COH final center=",fin); coh_log("COH final corner=",bg[co]);

    /* Two sanity gates before any verdict:
       - s1 MUST be nonzero: stage 1 rendered a gradient over the whole target,
         so the center must read back nonzero. If it's 0 the render never reached
         memory (or didn't draw) and the whole test is meaningless.
       - s2 MUST be 0: the fill wrote 0 everywhere, so the center must read 0.
       Only if both hold does final-center 0-vs-nonzero mean anything. */
    if(s1==0u){ trace_msg("COH VERDICT: INCONCLUSIVE (render/readback broken, s1=0)\n"); return 2; }
    if(s2!=0u){ trace_msg("COH VERDICT: INCONCLUSIVE (fill/readback broken, s2!=0)\n"); return 2; }
    if(fin==0u){ trace_msg("COH VERDICT: PASS (RT reflects the GPU fill)\n"); return 0; }
    trace_msg("COH VERDICT: FAIL (fill not reflected)\n"); return 1;
}

int main(void){
    int video = sceVideoOutOpen(0,0,0,0);
    if (video < 0) return 1;
    sceVideoOutSetFlipRate(video,0);

    unsigned long fb_size = (unsigned long)DISPLAY_W*DISPLAY_H*4;
    void *fb[NUM_FRAMES];
    for (int i=0;i<NUM_FRAMES;i++){ fb[i]=gpu_alloc(fb_size,0x100000); if(!fb[i]) return 1; }

    unsigned char buf_attr[48]; my_memset(buf_attr,0,48);
    sceVideoOutSetBufferAttribute(buf_attr,0x80000000,1,0,DISPLAY_W,DISPLAY_H,DISPLAY_W);
    sceVideoOutRegisterBuffers(video,0,fb,NUM_FRAMES,buf_attr);

    void *vs=gpu_alloc(sizeof(vs_fulltri_binary),256);
    if(!vs) return 1; my_memcpy(vs,vs_fulltri_binary,sizeof(vs_fulltri_binary)); g_vs_fulltri_gpu=vs;
    void *ps=gpu_alloc(sizeof(ps_grad_binary),256);
    if(!ps) return 1; my_memcpy(ps,ps_grad_binary,sizeof(ps_grad_binary)); g_ps_grad_gpu=ps;

    uint32_t *dcb=(uint32_t*)gpu_alloc(DCB_SIZE,0x10000);
    volatile uint32_t *fence=(volatile uint32_t*)gpu_alloc(0x1000,0x1000);
    if(!dcb||!fence) return 1;

    trace_init();

    /* 0 PASS=green  1 FAIL=red  2 INCONCLUSIVE=blue  3 ALLOC=magenta  (BGRA8) */
    static const uint32_t coh_col[4]={0xFF00FF00u,0xFFFF0000u,0xFF0000FFu,0xFFFF00FFu};
    struct PM4Builder pm4;
    int rc=run_coherence_test(dcb, fence);

    /* Paint the verdict into fb[0] and flip with the PROPER handshake: the DCB
       MUST end with pm4_prepare_flip (last 64 dwords) or the display/buffer-label
       state desyncs and corrupts other flip consumers (debug overlay, system UI).
       Same submit sequence the cube app uses. */
    *fence=0;
    pm4_init(&pm4,dcb,DCB_SIZE/4);
    build_coh_fill(&pm4,fb[0],fb_size,coh_col[rc],fence,9);   /* dma_fill + EOP */
    pm4_prepare_flip(&pm4);                                    /* last 64 dwords */
    {
        const uint32_t *a[1]={dcb}; uint32_t s[1]={pm4.off*4};
        sceGnmSubmitCommandBuffers(1,(void**)a,s,0,0);
        sceGnmSubmitDone();
        for(int w=0; w<10000 && *fence!=9; w++) sceKernelUsleep(100);
        sceVideoOutSubmitFlip(video,0,1,0);
        sceKernelUsleep(16000);
    }

    for(;;) sceKernelUsleep(1000000);
    return 0;
}
