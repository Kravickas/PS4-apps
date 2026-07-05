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

/* --- texture sampler PS: loads T#(desc[0..7]) + sampler(desc[8..11]) from the
       user-data desc pointer (s0:1), samples M at a FIXED texel (uv=0.5,0.5),
       exports it to MRT0. Assembled+verified with llvm-mc (gfx7/hawaii). --- */
static const uint32_t ps_samp_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000000, 0xC0C40100, 0xC0820108,
    0x7E0002F0, 0x7E0202F0, 0xBF8C007F, 0xF0800F00,
    0x00220400, 0xBF8C0F70, 0xF800080F, 0x07060504,
    0xBF810000, 0x5362724F, 0x00726468, 0x00003400,
    0x00000000, 0xDEADBEEF, 0xCAFE00C7, 0x00000000,
};

static int   g_log_fd         = -1;
static void *g_vs_fulltri_gpu = 0;
static void *g_ps_grad_gpu    = 0;
static void *g_ps_samp_gpu    = 0;

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

/* RGBA8 2D T# (data_format 10) for sampling M as a texture. */
static void build_tsharp(uint32_t *t, void *tex, int w, int h) {
    uint64_t a=(uint64_t)(uintptr_t)tex;
    my_memset(t,0,32);
    t[0]=(uint32_t)(a>>8); t[1]=(uint32_t)(a>>40)|(10u<<20);
    t[2]=(uint32_t)(w-1)|((uint32_t)(h-1)<<14);
    t[3]=4u|(5u<<3)|(6u<<6)|(7u<<9)|(8u<<20)|(9u<<28);
    t[4]=(uint32_t)(w-1)<<13;
}

/* Bilinear sampler S#. */
static void build_ssharp(uint32_t *s) {
    my_memset(s,0,16);
    s[1] = (0xF00u << 12);
    s[2] = (1u << 20) | (1u << 22);
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

/* Sample M (via the desc table T#/sampler) into a horizontal band
   [sx, sx+sw] x [0,H] of the display fb. Uses ps_samp (fixed-texel sample). */
static uint32_t build_coh_sample(struct PM4Builder *b, void *fb, void *desc,
                                 uint32_t sx, uint32_t sw,
                                 volatile uint32_t *fence, uint32_t fv) {
    pm4_init_default_hw_state(b);
    pm4_context_control(b);
    { uint64_t a=(uint64_t)(uintptr_t)g_vs_fulltri_gpu;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),1u,0u};
      pm4_set_sh_regs(b,SH_VS_PGM_LO,r,4); }
    /* Scissor to the band; loadOp=LOAD keeps the other half from the prior pass. */
    { uint32_t s[2]={(sx&0x7FFF)|(0u<<16),((sx+sw)&0x7FFF)|((DISPLAY_H&0x7FFF)<<16)};
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
    pm4_set_context_reg(b,CTX_DEPTH_RENDER_CONTROL,0);
    pm4_set_context_reg(b,CTX_DB_Z_INFO,0);
    pm4_set_context_reg(b,CTX_DB_STENCIL_INFO,0);
    pm4_set_context_reg(b,CTX_DEPTH_CONTROL,0);
    pm4_set_context_reg(b,CTX_POLYGON_CONTROL,0);
    { uint32_t c=(uint32_t)((uint64_t)(uintptr_t)fb>>8);
      uint32_t r[14]={c,(DISPLAY_W/8)-1,(DISPLAY_W*DISPLAY_H/64)-1,0,
        0x09A8u,0,0,0,0,0,0,0,0,0};
      pm4_set_context_regs(b,CTX_CB_COLOR0_BASE,r,14);
      pm4_emit(b,0xC0001000u); pm4_emit(b,DISPLAY_W|(DISPLAY_H<<16)); }
    pm4_set_context_reg(b,CTX_COLOR_TARGET_MASK,0xF);
    pm4_set_context_reg(b,CTX_COLOR_SHADER_MASK,0xF);
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0,0);
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONFIG,0);
    pm4_set_context_reg(b,CTX_PS_INPUT_ENA,0x302);
    pm4_set_context_reg(b,CTX_PS_INPUT_ADDR,0x302);
    pm4_set_context_reg(b,CTX_NUM_INTERP,0);
    /* desc pointer -> PS user_data s0:1 (full byte address for s_load base). */
    { uint64_t d=(uint64_t)(uintptr_t)desc;
      uint32_t ud[2]={(uint32_t)d,(uint32_t)(d>>32)};
      pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2); }
    /* PS = sampler: RSRC1 16vgpr/16sgpr (0x43), RSRC2 USER_SGPR=2 (2<<1). */
    { uint64_t a=(uint64_t)(uintptr_t)g_ps_samp_gpu;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),0x43u,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4); }
    pm4_set_uconfig_reg(b,UCFG_PRIMITIVE_TYPE,4);
    pm4_set_uconfig_reg(b,UCFG_NUM_INSTANCES,1);
    pm4_draw_index_auto(b,3);
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

/* Two-pass GPU texture-sample coherence test. Writes the result INTO fb and
   flips it; the SCREEN is the verdict:
     LEFT half  = sample of M after fill A  (always color A)
     RIGHT half = sample of the SAME texel after a non-render fill B
        split  (left A, right B) -> GPU fetch saw the non-render write (coherent)
        uniform(both A)          -> fetch returned a stale cached texel (not coherent)
   Returns 3 on alloc failure, else 0. */
static int run_sample_coherence_test(uint32_t *dcb, volatile uint32_t *fence,
                                     int video, void *fb){
    unsigned long mbytes=(unsigned long)DISPLAY_W*DISPLAY_H*4;
    void *M=gpu_alloc(mbytes,0x100000);
    void *desc=gpu_alloc(64,256);
    struct PM4Builder pm4;
    const uint32_t A=0xFF0000FFu, B=0xFF00FF00u;   /* two clearly-different colors */

    trace_msg("SAMP start\n");
    if(!M||!desc){ trace_msg("SAMP alloc failed\n"); return 3; }
    build_tsharp((uint32_t*)desc, M, DISPLAY_W, DISPLAY_H);
    build_ssharp((uint32_t*)desc + 8);

    volatile uint32_t *mp=(volatile uint32_t*)M;
    volatile uint32_t *fp=(volatile uint32_t*)fb;
    const unsigned long cM =(unsigned long)(DISPLAY_H/2)*DISPLAY_W + DISPLAY_W/2;
    const unsigned long cL =(unsigned long)(DISPLAY_H/2)*DISPLAY_W + DISPLAY_W/4;
    const unsigned long cR =(unsigned long)(DISPLAY_H/2)*DISPLAY_W + 3*(DISPLAY_W/4);

    /* 1: fill M = A (non-render write). */
    *fence=0; pm4_init(&pm4,dcb,DCB_SIZE/4);
    coh_submit(dcb,build_coh_fill(&pm4,M,mbytes,A,fence,1),fence,1);
    coh_log("M.afterA=", mp[cM]);            /* expect A=0xFF0000FF: confirms fill works */
    /* 2: sample M -> LEFT half (warms the texture cache with A). */
    *fence=0; pm4_init(&pm4,dcb,DCB_SIZE/4);
    coh_submit(dcb,build_coh_sample(&pm4,fb,desc,0,DISPLAY_W/2,fence,2),fence,2);
    coh_log("fb.L=", fp[cL]);                /* 0 -> sample/bind broken; A -> sampler works */
    /* 3: fill M = B (the non-render write under test). */
    *fence=0; pm4_init(&pm4,dcb,DCB_SIZE/4);
    coh_submit(dcb,build_coh_fill(&pm4,M,mbytes,B,fence,3),fence,3);
    coh_log("M.afterB=", mp[cM]);            /* expect B=0xFF00FF00 */
    /* 4: sample the SAME texel -> RIGHT half; end with prepare_flip and flip. */
    *fence=0; pm4_init(&pm4,dcb,DCB_SIZE/4);
    build_coh_sample(&pm4,fb,desc,DISPLAY_W/2,DISPLAY_W/2,fence,4);
    pm4_prepare_flip(&pm4);                             /* MUST be last 64 dwords */
    {
        const uint32_t *a[1]={dcb}; uint32_t s[1]={pm4.off*4};
        sceGnmSubmitCommandBuffers(1,(void**)a,s,0,0);
        sceGnmSubmitDone();
        for(int w=0; w<200000 && *fence!=4; w++) sceKernelUsleep(50);
        sceKernelUsleep(2000);
        coh_log("fb.R=", fp[cR]);            /* B -> coherent (write reflected); A -> stale */
        sceVideoOutSubmitFlip(video,0,1,0);
        sceKernelUsleep(16000);
    }
    trace_msg("SAMP done: read fb.L/fb.R from log. fb.L=0 means bind broke, not coherence.\n");
    return 0;
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
    void *psg=gpu_alloc(sizeof(ps_grad_binary),256);
    if(!psg) return 1; my_memcpy(psg,ps_grad_binary,sizeof(ps_grad_binary)); g_ps_grad_gpu=psg;
    void *pss=gpu_alloc(sizeof(ps_samp_binary),256);
    if(!pss) return 1; my_memcpy(pss,ps_samp_binary,sizeof(ps_samp_binary)); g_ps_samp_gpu=pss;

    uint32_t *dcb=(uint32_t*)gpu_alloc(DCB_SIZE,0x10000);
    volatile uint32_t *fence=(volatile uint32_t*)gpu_alloc(0x1000,0x1000);
    if(!dcb||!fence) return 1;

    trace_init();

    /* Runs 4 passes into fb[0] and flips it. The SCREEN is the verdict:
       split (left/right different) = coherent; uniform = stale texture fetch. */
    run_sample_coherence_test(dcb, fence, video, fb[0]);

    for(;;) sceKernelUsleep(1000000);
    return 0;
}
