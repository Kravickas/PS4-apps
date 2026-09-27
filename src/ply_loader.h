#include "tangent.h"
/* ply_loader.h — ASCII PLY loader
 * Supports: element vertex N (x,y,z + optional nx,ny,nz)
 *           element face M (property list uchar int vertex_index/vertex_indices)
 */

int ply_load_file(const char* path, void* (*alloc_fn)(unsigned long, unsigned long), ObjMesh* out,
                  void (*cb)(float, const char*, void*), void* cb_ud) {
    int fd = sceKernelOpen(path, 0, 0);
    if (fd < 0) return -1;

    /* Read header (ASCII, line by line) */
    char hdr[8192]; int hdr_len = 0;
    int r = sceKernelRead(fd, hdr, sizeof(hdr)-1);
    if (r <= 0) { sceKernelClose(fd); return -2; }
    hdr[r] = 0;
    sceKernelClose(fd);

    /* Parse header */
    int nv = 0, nf = 0, is_binary = 0;
    int has_nx = 0; /* has vertex normals */
    int prop_count = 0; /* properties per vertex */
    int nx_idx = -1, ny_idx = -1, nz_idx = -1;
    int in_vertex = 0, in_face = 0;
    int header_bytes = 0;

    const char *lp = hdr;
    while (*lp) {
        const char *le = lp;
        while (*le && *le != '\n' && *le != '\r') le++;

        int ll = le - lp;
        if (ll >= 10 && lp[0]=='e' && lp[1]=='n' && lp[2]=='d' && lp[3]=='_') {
            header_bytes = (le - hdr);
            while (header_bytes < r && (hdr[header_bytes]=='\n'||hdr[header_bytes]=='\r')) header_bytes++;
            break;
        }
        if (ll >= 6 && lp[0]=='f' && lp[1]=='o' && lp[2]=='r' && lp[3]=='m' && lp[4]=='a' && lp[5]=='t') {
            if (ll > 11 && lp[7]=='b') is_binary = 1; /* binary_little_endian or binary_big_endian */
        }
        if (ll >= 14 && lp[0]=='e' && lp[1]=='l' && lp[2]=='e' && lp[3]=='m') {
            /* element vertex N or element face N */
            const char *ep = lp + 8; /* skip "element " */
            if (ep[0]=='v') {
                in_vertex = 1; in_face = 0; prop_count = 0;
                while (ep < le && *ep != ' ') ep++; ep++;
                nv = obj_atoi(&ep, le);
            } else if (ep[0]=='f') {
                in_vertex = 0; in_face = 1;
                while (ep < le && *ep != ' ') ep++; ep++;
                nf = obj_atoi(&ep, le);
            } else { in_vertex = 0; in_face = 0; }
        }
        if (ll >= 8 && lp[0]=='p' && lp[1]=='r' && lp[2]=='o' && in_vertex) {
            /* property float x/y/z/nx/ny/nz */
            const char *pp = lp;
            /* Find last word (property name) */
            const char *name = le - 1;
            while (name > lp && *name != ' ' && *name != '\t') name--;
            name++;
            if (name[0]=='n' && name[1]=='x') { has_nx = 1; nx_idx = prop_count; }
            if (name[0]=='n' && name[1]=='y') { ny_idx = prop_count; }
            if (name[0]=='n' && name[1]=='z') { nz_idx = prop_count; }
            prop_count++;
        }

        while (*le == '\n' || *le == '\r') le++;
        lp = le;
    }

    if (nv == 0 || nf == 0 || is_binary) {
        /* Binary PLY not supported yet */
        return is_binary ? -10 : -3;
    }
    if (cb)
        cb(0.0f, "Reading PLY", cb_ud);

    /* Re-read file from after header */
    fd = sceKernelOpen(path, 0, 0);
    sceKernelLseek(fd, header_bytes, 0);

    /* Alloc positions */
    unsigned long pos_sz = (unsigned long)nv * 4;
    long px_ph, py_ph, pz_ph;
    float *px = (float*)obj_talloc(pos_sz, &px_ph);
    float *py = (float*)obj_talloc(pos_sz, &py_ph);
    float *pz = (float*)obj_talloc(pos_sz, &pz_ph);
    if (!px||!py||!pz) { sceKernelClose(fd); return -4; }

    /* Read vertices */
    char line[4096];
    int vi = 0;
    /* Simple line reader */
    char rbuf[65536]; int rbuf_len = 0;
    int phase = 0; /* 0=verts, 1=faces */
    int fi = 0;

    /* Count triangles in faces (fan triangulation) */
    /* First pass: read vertices, count face triangles */
    int nf_tri = 0;

    /* Read all data into memory (for simplicity with ASCII PLY) */
    long fsize = sceKernelLseek(fd, 0, 2);
    sceKernelLseek(fd, header_bytes, 0);
    long data_size = fsize - header_bytes;
    long data_ph;
    char *data = (char*)obj_talloc(data_size + 1, &data_ph);
    if (!data) { sceKernelClose(fd); return -5; }

    long total = 0;
    while (total < data_size) {
        int rd = sceKernelRead(fd, data + total, data_size - total > 65536 ? 65536 : data_size - total);
        if (rd <= 0) break;
        total += rd;
    }
    data[total] = 0;
    sceKernelClose(fd);

    if (cb)
        cb(1.0f / 3.0f, "Parsing vertices", cb_ud);

    /* Parse vertices */
    const char *dp = data, *de = data + total;
    for (vi = 0; vi < nv && dp < de; vi++) {
        float props[16]; int pi = 0;
        while (dp < de && pi < prop_count && pi < 16) {
            while (dp < de && (*dp==' '||*dp=='\t')) dp++;
            if (dp >= de || *dp == '\n' || *dp == '\r') break;
            props[pi++] = obj_atof(&dp, de);
        }
        while (dp < de && *dp != '\n') dp++;
        if (dp < de) dp++;
        px[vi] = pi > 0 ? props[0] : 0;
        py[vi] = pi > 1 ? props[1] : 0;
        pz[vi] = pi > 2 ? props[2] : 0;
    }

    /* Count face triangles */
    const char *face_start = dp;
    const char *tp = dp;
    for (fi = 0; fi < nf && tp < de; fi++) {
        while (tp < de && (*tp==' '||*tp=='\t')) tp++;
        int fc = 0;
        if (tp < de) { fc = obj_atoi(&tp, de); }
        if (fc >= 3) nf_tri += fc - 2;
        while (tp < de && *tp != '\n') tp++;
        if (tp < de) tp++;
    }

    if (cb)
        cb(2.0f / 3.0f, "Emitting triangles", cb_ud);

    /* Alloc VB */
    unsigned int nverts_out = nf_tri * 3;
    unsigned long vb_size = OBJ_DATA_OFF + (unsigned long)nverts_out * OBJ_STRIDE * 4;
    void *vb = alloc_fn(vb_size, 0x1000);
    if (!vb) { obj_tfree(data, data_ph, data_size+1); return -6; }

    /* Auto-scale */
    float minx=1e30f,miny=1e30f,minz=1e30f,maxx=-1e30f,maxy=-1e30f,maxz=-1e30f;
    for (int i=0;i<vi;i++){
        if(px[i]<minx)minx=px[i]; if(py[i]<miny)miny=py[i]; if(pz[i]<minz)minz=pz[i];
        if(px[i]>maxx)maxx=px[i]; if(py[i]>maxy)maxy=py[i]; if(pz[i]>maxz)maxz=pz[i];
    }
    float ccx=(minx+maxx)*0.5f, ccy=(miny+maxy)*0.5f, ccz=(minz+maxz)*0.5f;
    float ddx=maxx-minx, ddy=maxy-miny, ddz=maxz-minz;
    float maxd=ddx>ddy?ddx:ddy; if(ddz>maxd)maxd=ddz;
    float sc = maxd > 1e-6f ? 1.8f / maxd : 1.0f;
    for (int i=0;i<vi;i++){px[i]=(px[i]-ccx)*sc;py[i]=(py[i]-ccy)*sc;pz[i]=(pz[i]-ccz)*sc;}

    /* Compute smooth normals */
    long snx_ph, sny_ph, snz_ph;
    float *snx=(float*)obj_talloc(pos_sz,&snx_ph);
    float *sny=(float*)obj_talloc(pos_sz,&sny_ph);
    float *snz=(float*)obj_talloc(pos_sz,&snz_ph);
    if(snx&&sny&&snz){
        for(int i=0;i<vi;i++){snx[i]=0;sny[i]=0;snz[i]=0;}
        /* Accumulate face normals */
        dp = face_start;
        for (fi=0;fi<nf&&dp<de;fi++){
            while(dp<de&&(*dp==' '||*dp=='\t'))dp++;
            int fc=obj_atoi(&dp,de);
            int fv[64]; int fvc=0;
            for(int i=0;i<fc&&i<64;i++){
                while(dp<de&&(*dp==' '||*dp=='\t'))dp++;
                fv[fvc++]=obj_atoi(&dp,de);
            }
            while(dp<de&&*dp!='\n')dp++;
            if(dp<de)dp++;
            if(fvc>=3&&fv[0]>=0&&fv[0]<vi&&fv[1]>=0&&fv[1]<vi&&fv[2]>=0&&fv[2]<vi){
                float e1x=px[fv[1]]-px[fv[0]],e1y=py[fv[1]]-py[fv[0]],e1z=pz[fv[1]]-pz[fv[0]];
                float e2x=px[fv[2]]-px[fv[0]],e2y=py[fv[2]]-py[fv[0]],e2z=pz[fv[2]]-pz[fv[0]];
                float fnx=e1y*e2z-e1z*e2y,fny=e1z*e2x-e1x*e2z,fnz=e1x*e2y-e1y*e2x;
                float fl=obj_sqrtf(fnx*fnx+fny*fny+fnz*fnz);
                if(fl>1e-6f){fnx/=fl;fny/=fl;fnz/=fl;}
                for(int i=0;i<fvc;i++){
                    if(fv[i]>=0&&fv[i]<vi){snx[fv[i]]+=fnx;sny[fv[i]]+=fny;snz[fv[i]]+=fnz;}
                }
            }
        }
        for(int i=0;i<vi;i++){
            float l=obj_sqrtf(snx[i]*snx[i]+sny[i]*sny[i]+snz[i]*snz[i]);
            if(l>1e-6f){snx[i]/=l;sny[i]/=l;snz[i]/=l;}
        }
    }

    /* Emit triangles */
    float *verts = (float*)((char*)vb + OBJ_DATA_OFF);
    int nout = 0;
    dp = face_start;
    for (fi=0;fi<nf&&dp<de;fi++){
        while(dp<de&&(*dp==' '||*dp=='\t'))dp++;
        int fc=obj_atoi(&dp,de);
        int fv[64]; int fvc=0;
        for(int i=0;i<fc&&i<64;i++){
            while(dp<de&&(*dp==' '||*dp=='\t'))dp++;
            fv[fvc++]=obj_atoi(&dp,de);
        }
        while(dp<de&&*dp!='\n')dp++;
        if(dp<de)dp++;
        if(fvc<3)continue;
        for(int t=0;t<fvc-2;t++){
            int idx[3]={fv[0],fv[t+1],fv[t+2]};
            for(int k=0;k<3;k++){
                int ii=idx[k]; if(ii<0||ii>=vi)continue;
                float*o=verts+nout*OBJ_STRIDE;
                o[0]=px[ii];o[1]=py[ii];o[2]=pz[ii];o[3]=1;
                if(snx){o[4]=snx[ii];o[5]=sny[ii];o[6]=snz[ii];}
                else{o[4]=0;o[5]=1;o[6]=0;}
                /* No UVs: tangent perpendicular to the normal (tangent.h). */
                {
                    float tn[3], tg[3];
                    tn[0] = snx ? snx[ii] : 0.0f;
                    tn[1] = snx ? sny[ii] : 1.0f;
                    tn[2] = snx ? snz[ii] : 0.0f;
                    float tl = tn[0] * tn[0] + tn[1] * tn[1] + tn[2] * tn[2];
                    if (tl > 1e-12f)
                        tangent_perp(tn, tg);
                    else {
                        tg[0] = 1.0f;
                        tg[1] = 0.0f;
                        tg[2] = 0.0f;
                    }
                    o[7] = tg[0];
                    o[10] = tg[1];
                    o[11] = tg[2];
                }
                o[8] = 0.8f;
                o[9] = 0.8f;
                nout++;
            }
        }
    }

    /* Cleanup */
    if(snx){obj_tfree(snx,snx_ph,pos_sz);obj_tfree(sny,sny_ph,pos_sz);obj_tfree(snz,snz_ph,pos_sz);}
    obj_tfree(px,px_ph,pos_sz);obj_tfree(py,py_ph,pos_sz);obj_tfree(pz,pz_ph,pos_sz);
    obj_tfree(data,data_ph,data_size+1);

    /* Identity matrix */
    float *id=(float*)vb;
    for(int i=0;i<16;i++)id[i]=0; id[0]=1;id[5]=1;id[10]=1;id[15]=1;

    out->vb_base=vb; out->verts=verts; out->ib_base=0;
    out->num_verts=nout; out->num_tris=nout/3;
    out->vb_size=vb_size; out->indexed=0;
    return 0;
}
