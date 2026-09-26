#pragma once
/*
 * obj_loader.h — 3-pass streaming OBJ loader
 *
 * NEVER loads the full 2.1GB file. Streams 64MB chunks.
 * Pass 1: count verts + faces
 * Pass 2: read positions (162MB)
 * Pass 3: emit triangulated faces to VB (2.6GB)
 * Peak: 162MB + 2.6GB + 64MB = 2.82GB
 */

#include "loaders.h"

#define OBJ_CHUNK (64 * 1024 * 1024)

static void *obj_talloc(unsigned long size, long *phys) {
    unsigned long a = 0x4000;
    size = (size + a - 1) & ~(a - 1); if (size < a) size = a;
    unsigned long ph = 0;   /* out-param is unsigned long* */
    if (sceKernelAllocateDirectMemory(0, 0x600000000ULL, size, a, 3, &ph) != 0) return 0;
    void *addr = 0;
    if (sceKernelMapDirectMemory(&addr, size, 3, 0, ph, a) != 0) return 0;
    *phys = ph; return addr;
}
static void obj_tfree(void *addr, long phys, unsigned long size) {
    unsigned long a = 0x4000;
    size = (size + a - 1) & ~(a - 1); if (size < a) size = a;
    if (addr) sceKernelMunmap(addr, size);
    if (phys >= 0) sceKernelReleaseDirectMemory(phys, size);
}

/* Digits accumulate in a uint64 (first 19 significant), then one scale by an
   exact power of ten in double. Faster than per-digit float math, and exact to
   float precision (the old v*10 / f*=0.1 float loop lost digits). */
static const double obj_pow10[23] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                     1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                     1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
static float obj_atof(const char** pp, const char* end) {
    const char* p = *pp;
    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    int neg = 0;
    if (p < end && (*p == '-' || *p == '+')) {
        neg = (*p == '-');
        p++;
    }
    uint64_t m = 0;
    int nd = 0, e10 = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        if (nd < 19) {
            m = m * 10 + (uint64_t)(*p - '0');
            if (m)
                nd++;
        } else {
            e10++;
        }
        p++;
    }
    if (p < end && *p == '.') {
        p++;
        while (p < end && *p >= '0' && *p <= '9') {
            if (nd < 19) {
                m = m * 10 + (uint64_t)(*p - '0');
                if (m)
                    nd++;
                e10--;
            }
            p++;
        }
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
        p++;
        int es = 1, ex = 0;
        if (p < end && (*p == '-' || *p == '+')) {
            es = (*p == '-') ? -1 : 1;
            p++;
        }
        while (p < end && *p >= '0' && *p <= '9') {
            if (ex < 10000)
                ex = ex * 10 + (*p - '0');
            p++;
        }
        e10 += es * ex;
    }
    double v = (double)m;
    while (e10 < -22 && v != 0.0) {
        v /= 1e22;
        e10 += 22;
    }
    while (e10 > 22) {
        v *= 1e22;
        e10 -= 22;
    }
    if (e10 < 0)
        v /= obj_pow10[-e10];
    else if (e10 > 0)
        v *= obj_pow10[e10];
    *pp = p;
    return (float)(neg ? -v : v);
}
static int obj_atoi(const char **pp, const char *end) {
    const char *p=*pp;while(p<end&&(*p==' '||*p=='\t'))p++;
    int s=1;if(p<end&&*p=='-'){s=-1;p++;}int v=0;
    while(p<end&&*p>='0'&&*p<='9'){v=v*10+(*p-'0');p++;}*pp=p;return s*v;
}
static const char *obj_memchr(const char *s, char c, int n) {
    for(int i=0;i<n;i++) if(s[i]==c) return s+i;
    return 0;
}

/* sqrtss: exact, one instruction. The old 8-step Newton from r = x was ~2x off
   for small x (e.g. 1e-6), which skewed per-face normal weights. */
static inline float obj_sqrtf(float x) {
    if (x <= 0)
        return 0;
    __asm__("sqrtss %1, %0" : "=x"(x) : "x"(x));
    return x;
}

/* MTL material table */
#define MTL_MAX 32
#define MTL_NAME_LEN 48
typedef struct { char name[MTL_NAME_LEN]; float r, g, b; } MtlEntry;
typedef struct { MtlEntry m[MTL_MAX]; int n; } MtlTable;

static int mtl_name_eq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (*a == 0 || *a == '\n' || *a == '\r') && (*b == 0 || *b == '\n' || *b == '\r');
}

static void mtl_load(const char *obj_path, MtlTable *t) {
    t->n = 0;
    /* Build MTL path: try .obj → .mtl */
    char mtl_path[256];
    int i = 0;
    while (obj_path[i] && i < 250) { mtl_path[i] = obj_path[i]; i++; }
    mtl_path[i] = 0;
    /* Replace .obj → .mtl */
    if (i > 4 && mtl_path[i-4] == '.') {
        mtl_path[i-3] = 'm'; mtl_path[i-2] = 't'; mtl_path[i-1] = 'l';
    }

    int fd = sceKernelOpen(mtl_path, 0, 0);
    if (fd < 0) return;

    /* Read entire MTL (typically <4KB) */
    char buf[4096];
    int len = sceKernelRead(fd, buf, sizeof(buf)-1);
    sceKernelClose(fd);
    if (len <= 0) return;
    buf[len] = 0;

    int cur = -1;
    const char *lp = buf;
    while (lp < buf + len) {
        const char *le = lp;
        while (le < buf + len && *le != '\n') le++;

        if (lp[0]=='n' && lp[1]=='e' && lp[2]=='w' && lp[3]=='m' && t->n < MTL_MAX) {
            cur = t->n++;
            const char *nm = lp + 7;
            int j = 0;
            while (j < MTL_NAME_LEN-1 && nm+j < le && nm[j] != '\r' && nm[j] != '\n')
                { t->m[cur].name[j] = nm[j]; j++; }
            t->m[cur].name[j] = 0;
            t->m[cur].r = 0.8f; t->m[cur].g = 0.8f; t->m[cur].b = 0.8f;
        } else if (lp[0]=='K' && lp[1]=='d' && lp[2]==' ' && cur >= 0) {
            const char *p = lp + 3;
            t->m[cur].r = obj_atof(&p, le);
            t->m[cur].g = obj_atof(&p, le);
            t->m[cur].b = obj_atof(&p, le);
        }

        lp = (le < buf + len) ? le + 1 : le;
    }
}

static int mtl_find(const MtlTable *t, const char *name) {
    for (int i = 0; i < t->n; i++)
        if (mtl_name_eq(t->m[i].name, name)) return i;
    return -1;
}

static int obj_emit_face(const char *p, const char *end,
                         const float *px, const float *py, const float *pz,
                         const float *nnx, const float *nny, const float *nnz,
                         int nv, int nn, float *out) {
    int fv[64],fn[64],fvc=0;
    while(p<end&&*p!='\n'&&*p!='\r'&&fvc<64){
        while(p<end&&(*p==' '||*p=='\t'))p++;
        if(p>=end||*p=='\n'||*p=='\r')break;
        int vi=obj_atoi(&p,end);
        int ni=0;
        if(p<end&&*p=='/'){p++;if(p<end&&*p!='/')obj_atoi(&p,end);if(p<end&&*p=='/'){p++;ni=obj_atoi(&p,end);}}
        if(vi<0)vi=nv+vi+1; vi--;
        if(ni<0)ni=nn+ni+1; ni--;
        if(vi>=0&&vi<nv){fv[fvc]=vi;fn[fvc]=ni;fvc++;}
    }
    if(fvc<3)return 0;
    float e1x=px[fv[1]]-px[fv[0]],e1y=py[fv[1]]-py[fv[0]],e1z=pz[fv[1]]-pz[fv[0]];
    float e2x=px[fv[2]]-px[fv[0]],e2y=py[fv[2]]-py[fv[0]],e2z=pz[fv[2]]-pz[fv[0]];
    float nx=e1y*e2z-e1z*e2y,ny=e1z*e2x-e1x*e2z,nz=e1x*e2y-e1y*e2x;
    float nl=obj_sqrtf(nx*nx+ny*ny+nz*nz);if(nl>1e-4f){nx/=nl;ny/=nl;nz/=nl;}
    /* Store normalized face normal — PS shader computes lighting dynamically */
    int n=0;
    for(int t=0;t<fvc-2;t++){
        int idx[3]={fv[0],fv[t+1],fv[t+2]};
        int nidx[3]={fn[0],fn[t+1],fn[t+2]};
        for(int k=0;k<3;k++){
            float*o=out+n*OBJ_STRIDE;int ii=idx[k];
            o[0]=px[ii];o[1]=py[ii];o[2]=pz[ii];o[3]=1;
            /* Use per-vertex normal (vn) if available, else face normal */
            float vnx,vny,vnz;
            if(nnx && nidx[k]>=0 && nidx[k]<nn){
                vnx=nnx[nidx[k]]; vny=nny[nidx[k]]; vnz=nnz[nidx[k]];
            } else {
                vnx=nx; vny=ny; vnz=nz;
            }
            o[4]=vnx;o[5]=vny;o[6]=vnz;o[7]=0; /* normal for per-pixel Phong */
            n++;}}
    return n;
}

/* Stream a file pass: calls line_fn for each complete line.
 * line_fn(line_start, line_end, user_data). Returns 0 to continue. */
typedef int (*obj_line_fn)(const char*, const char*, void*);
/* Per-chunk progress for one streaming pass: reports
   (pass + bytes_done / fsize) / npass after every chunk. fn may be 0. */
typedef struct {
    void (*fn)(float frac, const char* msg, void* ud);
    void* ud;
    const char* msg;
    int pass, npass;
    unsigned long fsize;
} ObjProg;

/* First '\n' in [p, e), 8 bytes per step: x = word ^ 0x0A.. has a zero byte
   exactly where the word has '\n' (the classic exact has-zero test), then the
   byte loop finds its position. */
static inline const char* obj_find_nl(const char* p, const char* e) {
    while (p < e && ((uintptr_t)p & 7)) {
        if (*p == '\n')
            return p;
        p++;
    }
    while (e - p >= 8) {
        uint64_t x = *(const uint64_t*)p ^ 0x0A0A0A0A0A0A0A0AULL;
        if ((x - 0x0101010101010101ULL) & ~x & 0x8080808080808080ULL)
            break;
        p += 8;
    }
    while (p < e) {
        if (*p == '\n')
            return p;
        p++;
    }
    return 0;
}

/* Reads land at OBJ_HEAD in each buffer; the partial last line of the previous
   buffer is copied in front of them (longest carried line = OBJ_HEAD). */
#define OBJ_HEAD 65536

typedef struct {
    int fd;
    char* buf[2];
    long len[2];
    int full[2];
    void* mtx;
    void* cv;
} ObjReader;

/* Reader thread: fills buffer k whenever the parser has released it. Stops
   after the read that returns less than a full chunk (EOF or error), which is
   also where the parser stops, so neither waits on the other at exit. */
static void* obj_reader_main(void* arg) {
    ObjReader* rd = (ObjReader*)arg;
    for (int k = 0;; k ^= 1) {
        scePthreadMutexLock(&rd->mtx);
        while (rd->full[k])
            scePthreadCondWait(&rd->cv, &rd->mtx);
        scePthreadMutexUnlock(&rd->mtx);
        long r = sceKernelRead(rd->fd, rd->buf[k] + OBJ_HEAD, OBJ_CHUNK);
        scePthreadMutexLock(&rd->mtx);
        rd->len[k] = r;
        rd->full[k] = 1;
        scePthreadCondSignal(&rd->cv);
        scePthreadMutexUnlock(&rd->mtx);
        if (r < OBJ_CHUNK)
            break;
    }
    return 0;
}

/* One pass over the file: every pass reads the whole file, so a reader thread
   fills one buffer while this thread parses the other and disk reads overlap
   parsing. Falls back to synchronous reads if the thread cannot be created. */
static void obj_stream_pass(const char* path, obj_line_fn fn, void* ud, const ObjProg* pg) {
    int fd = sceKernelOpen(path, 0, 0);
    if (fd < 0)
        return;
    unsigned long bsz = OBJ_HEAD + OBJ_CHUNK + 1;
    long ph[2] = {-1, -1};
    ObjReader rd;
    rd.fd = fd;
    rd.buf[0] = (char*)obj_talloc(bsz, &ph[0]);
    rd.buf[1] = rd.buf[0] ? (char*)obj_talloc(bsz, &ph[1]) : 0;
    rd.len[0] = rd.len[1] = 0;
    rd.full[0] = rd.full[1] = 0;
    rd.mtx = 0;
    rd.cv = 0;
    if (!rd.buf[0] || !rd.buf[1]) {
        if (rd.buf[0])
            obj_tfree(rd.buf[0], ph[0], bsz);
        sceKernelClose(fd);
        return;
    }
    void* thr = 0;
    int threaded = scePthreadMutexInit(&rd.mtx, 0, "obj_reader") == 0;
    if (threaded && scePthreadCondInit(&rd.cv, 0, "obj_reader") != 0) {
        scePthreadMutexDestroy(&rd.mtx);
        threaded = 0;
    }
    if (threaded && scePthreadCreate(&thr, 0, obj_reader_main, &rd, "obj_reader") != 0) {
        scePthreadCondDestroy(&rd.cv);
        scePthreadMutexDestroy(&rd.mtx);
        threaded = 0;
    }
    const char* lp = 0; /* partial last line carried from the previous buffer */
    int left = 0;
    unsigned long done = 0;
    for (int k = 0, first = 1;; k ^= 1, first = 0) {
        long r;
        if (threaded) {
            scePthreadMutexLock(&rd.mtx);
            while (!rd.full[k])
                scePthreadCondWait(&rd.cv, &rd.mtx);
            r = rd.len[k];
            scePthreadMutexUnlock(&rd.mtx);
        } else {
            r = sceKernelRead(fd, rd.buf[k] + OBJ_HEAD, OBJ_CHUNK);
        }
        if (r < 0)
            r = 0;
        char* ck = rd.buf[k] + OBJ_HEAD - left;
        for (int i = 0; i < left; i++)
            ck[i] = lp[i];
        if (threaded && !first) { /* leftover copied: hand the previous buffer back */
            scePthreadMutexLock(&rd.mtx);
            rd.full[k ^ 1] = 0;
            scePthreadCondSignal(&rd.cv);
            scePthreadMutexUnlock(&rd.mtx);
        }
        long clen = left + r;
        ck[clen] = 0;
        const char *cp = ck, *ce = ck + clen;
        left = 0;
        while (cp < ce) {
            const char* eol = obj_find_nl(cp, ce);
            if (!eol) {
                left = (int)(ce - cp);
                if (left > OBJ_HEAD)
                    left = OBJ_HEAD;
                lp = cp;
                break;
            }
            fn(cp, eol, ud);
            cp = eol + 1;
        }
        done += (unsigned long)r;
        if (pg && pg->fn && pg->fsize) {
            float f = (float)done / (float)pg->fsize;
            if (f > 1.0f)
                f = 1.0f;
            pg->fn(((float)pg->pass + f) / (float)pg->npass, pg->msg, pg->ud);
        }
        if (r < OBJ_CHUNK) { /* EOF: the last line may have no '\n' */
            if (left > 0) {
                char* t = (char*)lp;
                t[left] = '\n';
                fn(t, t + left, ud);
            }
            break;
        }
    }
    if (threaded) {
        scePthreadJoin(thr, 0);
        scePthreadCondDestroy(&rd.cv);
        scePthreadMutexDestroy(&rd.mtx);
    }
    sceKernelClose(fd);
    obj_tfree(rd.buf[0], ph[0], bsz);
    obj_tfree(rd.buf[1], ph[1], bsz);
}

/* Pass 1 context: count */
typedef struct { int nv; int nn; int nt; int nf_tri; } P1Ctx;
static int p1_line(const char *s, const char *e, void *ud) {
    P1Ctx *c = (P1Ctx*)ud;
    if (*s == 'v' && s+1 < e && s[1] == ' ') c->nv++;
    else if (*s == 'v' && s+1 < e && s[1] == 't' && s+2 < e && s[2] == ' ') c->nt++;
    else if (*s == 'f' && s+1 < e && (s[1]==' '||s[1]=='\t')) {
        const char *p = s+2; int fvc = 0;
        while (p<e) { while(p<e&&(*p==' '||*p=='\t'))p++; if(p<e&&*p!='\n'&&*p!='\r'){fvc++;while(p<e&&*p!=' '&&*p!='\t'&&*p!='\n'&&*p!='\r')p++;} else break; }
        if (fvc >= 3) c->nf_tri += fvc - 2;
    }
    return 0;
}

/* Pass 2 context: read positions */
typedef struct { float *px, *py, *pz, *nnx, *nny, *nnz, *tu, *tv; int vi, ni, ti, maxv, maxn, maxt; } P2Ctx;
static int p2_line(const char *s, const char *e, void *ud) {
    P2Ctx *c = (P2Ctx*)ud;
    if (*s == 'v' && s+1 < e && s[1] == 't' && s+2 < e && s[2] == ' ' && c->tu && c->ti < c->maxt) {
        s += 3;
        c->tu[c->ti] = obj_atof(&s, e);
        c->tv[c->ti] = obj_atof(&s, e);
        c->ti++;
    } else if (*s == 'v' && s+1 < e && s[1] == ' ' && c->vi < c->maxv) {
        s += 2;
        c->px[c->vi] = obj_atof(&s, e);
        c->py[c->vi] = obj_atof(&s, e);
        c->pz[c->vi] = obj_atof(&s, e);
        c->vi++;
    } else if (*s == 'v' && s+2 < e && s[1] == 'n' && s[2] == ' ' && c->nnx && c->ni < c->maxn) {
        s += 3;
        c->nnx[c->ni] = obj_atof(&s, e);
        c->nny[c->ni] = obj_atof(&s, e);
        c->nnz[c->ni] = obj_atof(&s, e);
        c->ni++;
    }
    return 0;
}

/* Pass 3 context: emit faces */
typedef struct { const float *px, *py, *pz, *snx, *sny, *snz, *tu, *tv; int nv, nt; float *out; int nout; float cr,cg,cb; const MtlTable *mtl; } P3Ctx;
static int p3_emit_smooth_uv(const char *p, const char *end,
                          const float *px, const float *py, const float *pz,
                          const float *snx, const float *sny, const float *snz,
                          const float *tu, const float *tv, int nv, int nt, float *out) {
    int fv[64], ft[64], fvc = 0;
    while(p<end&&*p!='\n'&&*p!='\r'&&fvc<64){
        while(p<end&&(*p==' '||*p=='\t'))p++;
        if(p>=end||*p=='\n'||*p=='\r')break;
        int vi=obj_atoi(&p,end);
        int ti=-1;
        if(p<end&&*p=='/'){p++;if(p<end&&*p!='/')ti=obj_atoi(&p,end);if(p<end&&*p=='/'){p++;obj_atoi(&p,end);}}
        if(vi<0)vi=nv+vi+1; vi--;
        if(ti>0)ti--; else if(ti==0)ti=-1;
        if(vi>=0&&vi<nv){fv[fvc]=vi;ft[fvc]=ti;fvc++;}
    }
    if(fvc<3)return 0;
    int n=0;
    for(int t=0;t<fvc-2;t++){
        int idx[3]={fv[0],fv[t+1],fv[t+2]};
        int tidx[3]={ft[0],ft[t+1],ft[t+2]};
        for(int k=0;k<3;k++){
            float*o=out+n*OBJ_STRIDE; int ii=idx[k]; int ti=tidx[k];
            o[0]=px[ii];o[1]=py[ii];o[2]=pz[ii];o[3]=1;
            o[4]=snx[ii];o[5]=sny[ii];o[6]=snz[ii];o[7]=0;
            o[8]=(ti>=0&&ti<nt)?tu[ti]:0;
            o[9]=(ti>=0&&ti<nt)?tv[ti]:0;
            o[10]=0; o[11]=0;
            n++;
        }
    }
    return n;
}

static int p3_line(const char *s, const char *e, void *ud) {
    P3Ctx *c = (P3Ctx*)ud;
    if (s[0]=='u' && s[1]=='s' && s[2]=='e' && s[5]=='l' && s[6]==' ') {
        /* usemtl name */
        const char *nm = s + 7;
        int mi = mtl_find(c->mtl, nm);
        if (mi >= 0) { c->cr = c->mtl->m[mi].r; c->cg = c->mtl->m[mi].g; c->cb = c->mtl->m[mi].b; }
    }
    if (*s == 'f' && s+1 < e && (s[1]==' '||s[1]=='\t')) {
        int base = c->nout;
        c->nout += p3_emit_smooth_uv(s+2, e, c->px, c->py, c->pz, c->snx, c->sny, c->snz, c->tu, c->tv, c->nv, c->nt, c->out + c->nout * OBJ_STRIDE);
        /* UVs stored per-vertex from vt indices */
    }
    return 0;
}

/* ================================================================ */
/* Progress callback: called between passes with (pass 1-3, detail string, user_data) */

/* Smooth normal accumulation callback */
typedef struct { const float *px,*py,*pz; float *snx,*sny,*snz; int nv; } SNCtx;
static int sn_line(const char *s, const char *e, void *ud) {
    SNCtx *c = (SNCtx*)ud;
    if (s[0]!='f' || (s[1]!=' ' && s[1]!='\t')) return 0;
    const char *fp=s+2;
    int afv[64],afvc=0;
    while(fp<e&&afvc<64){
        while(fp<e&&(*fp==' '||*fp=='\t'))fp++;
        if(fp>=e||*fp=='\n'||*fp=='\r')break;
        int avi=obj_atoi(&fp,e);
        if(fp<e&&*fp=='/'){fp++;if(fp<e&&*fp!='/')obj_atoi(&fp,e);if(fp<e&&*fp=='/'){fp++;obj_atoi(&fp,e);}}
        if(avi<0)avi=c->nv+avi+1; avi--;
        if(avi>=0&&avi<c->nv)afv[afvc++]=avi;
    }
    if(afvc<3)return 0;
    float e1x=c->px[afv[1]]-c->px[afv[0]],e1y=c->py[afv[1]]-c->py[afv[0]],e1z=c->pz[afv[1]]-c->pz[afv[0]];
    float e2x=c->px[afv[2]]-c->px[afv[0]],e2y=c->py[afv[2]]-c->py[afv[0]],e2z=c->pz[afv[2]]-c->pz[afv[0]];
    float fnx=e1y*e2z-e1z*e2y,fny=e1z*e2x-e1x*e2z,fnz=e1x*e2y-e1y*e2x;
    float fl=obj_sqrtf(fnx*fnx+fny*fny+fnz*fnz);
    if(fl<1e-6f)return 0; /* skip degenerate faces */
    fnx/=fl;fny/=fl;fnz/=fl; /* normalize: equal weight per face */
    for(int i=0;i<afvc;i++){c->snx[afv[i]]+=fnx;c->sny[afv[i]]+=fny;c->snz[afv[i]]+=fnz;}
    return 0;
}

int obj_load_file(const char* path, void* (*alloc_fn)(unsigned long, unsigned long), ObjMesh* out,
                  obj_progress_fn progress, void* progress_ud) {
    out->vb_base=0;out->ib_base=0;out->verts=0;
    out->num_verts=0;out->num_tris=0;out->num_indices=0;
    out->indexed=0;out->vb_size=0;out->ib_size=0;

    /* progress() renders a vblank-paced frame and the caller probes a list of
       candidate paths, so a MISS must not cost a frame: check the file exists
       (and get its size) with a plain open first. On a hit, show the first
       frame immediately - pass 1 over a 2 GB file used to run before the first
       frame - and report per 64 MB chunk in each of the 4 streaming passes. */
    int efd = sceKernelOpen(path, 0, 0);
    if (efd < 0)
        return -1;
    long fsz = sceKernelLseek(efd, 0, 2); /* SEEK_END */
    sceKernelClose(efd);
    ObjProg pg = {progress, progress_ud, "Counting", 0, 4, fsz > 0 ? (unsigned long)fsz : 0};
    if (progress)
        progress(0.0f, "Counting", progress_ud);

    /* ==== PASS 1: count ==== */
    P1Ctx p1 = {0, 0, 0, 0};
    obj_stream_pass(path, p1_line, &p1, &pg);
    if (p1.nv == 0 || p1.nf_tri == 0) return -4;

    /* ==== Alloc positions (162MB for 13.5M verts) ==== */
    unsigned long pos_sz = (unsigned long)p1.nv * 4;
    long px_ph, py_ph, pz_ph;
    float *px = (float*)obj_talloc(pos_sz, &px_ph);
    float *py = (float*)obj_talloc(pos_sz, &py_ph);
    float *pz = (float*)obj_talloc(pos_sz, &pz_ph);
    if (!px || !py || !pz) return -5;

    /* Vertex normals (vn) for smooth shading */
    int nn = p1.nn;
    unsigned long nrm_sz = nn > 0 ? (unsigned long)nn * 4 : 0;
    long nnx_ph = -1, nny_ph = -1, nnz_ph = -1;
    float *nnx = 0, *nny = 0, *nnz = 0;
    if (nn > 0) {
        nnx = (float*)obj_talloc(nrm_sz, &nnx_ph);
        nny = (float*)obj_talloc(nrm_sz, &nny_ph);
        nnz = (float*)obj_talloc(nrm_sz, &nnz_ph);
    }


    /* Alloc UVs */
    int nti = p1.nt > 0 ? p1.nt : 1;
    long tu_ph = 0, tv_ph = 0;
    float *tu = (float*)obj_talloc(nti * 4, &tu_ph);
    float *tv = (float*)obj_talloc(nti * 4, &tv_ph);
    if (!tu || !tv) { obj_tfree(px,px_ph,pos_sz); obj_tfree(py,py_ph,pos_sz); obj_tfree(pz,pz_ph,pos_sz); return -8; }

    /* ==== PASS 2: read positions + UVs ==== */
    P2Ctx p2 = { px, py, pz, nnx, nny, nnz, tu, tv, 0, 0, 0, p1.nv, nn, nti };
    pg.pass = 1;
    pg.msg = "Reading positions";
    obj_stream_pass(path, p2_line, &p2, &pg);

    /* Auto-scale */
    int vi = p2.vi;
    float mnx=px[0],mxx=px[0],mny=py[0],mxy=py[0],mnz=pz[0],mxz=pz[0];
    for(int i=1;i<vi;i++){if(px[i]<mnx)mnx=px[i];if(px[i]>mxx)mxx=px[i];if(py[i]<mny)mny=py[i];if(py[i]>mxy)mxy=py[i];if(pz[i]<mnz)mnz=pz[i];if(pz[i]>mxz)mxz=pz[i];}
    float cx=(mnx+mxx)*.5f,cy=(mny+mxy)*.5f,cz=(mnz+mxz)*.5f;
    float rng=mxx-mnx;if(mxy-mny>rng)rng=mxy-mny;if(mxz-mnz>rng)rng=mxz-mnz;
    float sc=(rng>1e-4f)?1.8f/rng:1;
    for(int i=0;i<vi;i++){px[i]=(px[i]-cx)*sc;py[i]=(py[i]-cy)*sc;pz[i]=(pz[i]-cz)*sc;}

    /* ==== Alloc VB (2.6GB for 27M tris) ==== */
    /* Memory: 162MB pos + 2.6GB VB = 2.76GB. Fits! */
    unsigned long vb_size = OBJ_DATA_OFF + (unsigned long)p1.nf_tri * 3 * OBJ_STRIDE * 4;
    void *vb = alloc_fn(vb_size, 0x1000);
    if (!vb) {
        obj_tfree(tu, tu_ph, nti*4); obj_tfree(tv, tv_ph, nti*4);
    obj_tfree(px, px_ph, pos_sz);
        obj_tfree(py, py_ph, pos_sz);
        obj_tfree(pz, pz_ph, pos_sz);
        return -6;
    }
    float *verts = (float*)((char*)vb + OBJ_DATA_OFF);


    /* ==== Compute smooth vertex normals ==== */
    unsigned long snrm_sz = (unsigned long)vi * 4;
    long snx_ph=-1, sny_ph=-1, snz_ph=-1;
    float *snx = (float*)obj_talloc(snrm_sz, &snx_ph);
    float *sny = (float*)obj_talloc(snrm_sz, &sny_ph);
    float *snz = (float*)obj_talloc(snrm_sz, &snz_ph);
    if (!snx||!sny||!snz) return -7;
    for(int i=0;i<vi;i++){snx[i]=0;sny[i]=0;snz[i]=0;}
    /* Pass 3a: accumulate via obj_stream_pass */
    { SNCtx snctx = { px, py, pz, snx, sny, snz, vi };
        pg.pass = 2;
        pg.msg = "Smoothing normals";
        obj_stream_pass(path, sn_line, &snctx, &pg);
    }
    for(int i=0;i<vi;i++){
        float l=obj_sqrtf(snx[i]*snx[i]+sny[i]*sny[i]+snz[i]*snz[i]);
        if(l>1e-6f){snx[i]/=l;sny[i]/=l;snz[i]/=l;}
    }

    /* ==== PASS 3: emit faces ==== */
    /* Load MTL */
    MtlTable mtl_table;
    mtl_load(path, &mtl_table);
    /* Hardcoded Controller materials as fallback */
    if (mtl_table.n == 0) {
        struct { const char *n; float r,g,b; } hc[] = {
            {"Metal bits",0.70f,0.74f,0.74f}, {"Shiny black",0.02f,0.02f,0.02f},
            {"Blue",0.08f,0.48f,0.94f}, {"Green",0.04f,0.96f,0.16f},
            {"Red",0.96f,0.10f,0.04f}, {"Yellow",0.96f,0.81f,0.04f},
            {"Logo black",0.02f,0.02f,0.02f}, {"Shiny white",1.0f,1.0f,1.0f},
            {"Analogs",1.0f,1.0f,1.0f}, {"Analogs bump",1.0f,1.0f,1.0f},
            {"Dull white",1.0f,1.0f,1.0f},
            {"Front buttons transparent",0.8f,0.8f,0.8f},
            {"Front button inserts",0.02f,0.02f,0.02f}, {0,0,0,0}
        };
        for (int hi=0; hc[hi].n && mtl_table.n < MTL_MAX; hi++) {
            int mi = mtl_table.n++;
            const char *s = hc[hi].n; int j=0;
            while (s[j] && j < MTL_NAME_LEN-1) { mtl_table.m[mi].name[j] = s[j]; j++; }
            mtl_table.m[mi].name[j] = 0;
            mtl_table.m[mi].r = hc[hi].r;
            mtl_table.m[mi].g = hc[hi].g;
            mtl_table.m[mi].b = hc[hi].b;
        }
    }
    P3Ctx p3 = { px, py, pz, snx, sny, snz, tu, tv, vi, nti, verts, 0, 0.8f, 0.8f, 0.8f, &mtl_table };
    pg.pass = 3;
    pg.msg = "Building triangles";
    obj_stream_pass(path, p3_line, &p3, &pg);

    /* Free positions */
    obj_tfree(tu, tu_ph, nti*4); obj_tfree(tv, tv_ph, nti*4);
    obj_tfree(px, px_ph, pos_sz);
    obj_tfree(py, py_ph, pos_sz);
    obj_tfree(pz, pz_ph, pos_sz);
    if (nnx) obj_tfree(nnx, nnx_ph, nrm_sz);
    if (nny) obj_tfree(nny, nny_ph, nrm_sz);
    if (nnz) obj_tfree(nnz, nnz_ph, nrm_sz);
    obj_tfree(snx, snx_ph, snrm_sz);
    obj_tfree(sny, sny_ph, snrm_sz);
    obj_tfree(snz, snz_ph, snrm_sz);

    if (progress)
        progress(1.0f, "Done", progress_ud);

    out->vb_base = vb; out->verts = verts; out->ib_base = 0;
    out->num_verts = p3.nout; out->num_tris = p3.nout / 3;
    out->num_indices = 0; out->indexed = 0;
    out->vb_size = vb_size; out->ib_size = 0;
    return 0;
}
