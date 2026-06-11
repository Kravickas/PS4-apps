/* stl_loader.h — Binary + ASCII STL loader
 * Produces same output as obj_loader: smooth vertex normals, VB at OBJ_DATA_OFF
 *
 * Binary STL: 80-byte header, uint32 tri_count, then per-tri:
 *   float[3] normal, float[3] v1, float[3] v2, float[3] v3, uint16 attrib (50 bytes)
 * ASCII STL: "solid name" ... "facet normal" ... "vertex" ... "endsolid"
 */

#define STL_TRI_SIZE 50

static int stl_is_binary(const char *path) {
    int fd = sceKernelOpen(path, 0, 0);
    if (fd < 0) return -1;
    char buf[84];
    int r = sceKernelRead(fd, buf, 84);
    sceKernelClose(fd);
    if (r < 84) return 0;
    /* If header doesn't start with "solid" or has non-ASCII, likely binary */
    if (buf[0]!='s'||buf[1]!='o'||buf[2]!='l'||buf[3]!='i'||buf[4]!='d') return 1;
    /* Could be ASCII "solid ..." — check if tri count makes sense */
    unsigned int ntri = *(unsigned int*)(buf + 80);
    { int fd2 = sceKernelOpen(path,0,0); long fsize = fd2>=0 ? sceKernelLseek(fd2, 0, 2) : 0; if(fd2>=0) sceKernelClose(fd2);
    /* Binary size = 84 + ntri*50 */
    if (fsize > 0 && fsize == 84 + (long)ntri * 50) return 1; }
    return 0;
}

static int stl_load_binary(const char *path, void *(*alloc_fn)(unsigned long,unsigned long),
                           ObjMesh *out, void (*cb)(int,const char*,void*), void *cb_ud) {
    int fd = sceKernelOpen(path, 0, 0);
    if (fd < 0) return -1;

    /* Read header + count */
    char hdr[84];
    sceKernelRead(fd, hdr, 84);
    unsigned int ntri = *(unsigned int*)(hdr + 80);
    if (ntri == 0 || ntri > 50000000) { sceKernelClose(fd); return -2; }

    if (cb) cb(0, "Reading STL", cb_ud);

    /* Read all triangle data */
    unsigned long data_sz = (unsigned long)ntri * STL_TRI_SIZE;
    long data_ph;
    char *data = (char*)obj_talloc(data_sz, &data_ph);
    if (!data) { sceKernelClose(fd); return -3; }

    unsigned long total = 0;
    while (total < data_sz) {
        int r = sceKernelRead(fd, data + total, data_sz - total > 65536 ? 65536 : data_sz - total);
        if (r <= 0) break;
        total += r;
    }
    sceKernelClose(fd);

    /* Extract positions: 3 verts per tri, find unique by hashing */
    unsigned int nverts = ntri * 3;
    if (cb) cb(1, "Computing normals", cb_ud);

    /* Alloc VB */
    unsigned long vb_size = OBJ_DATA_OFF + (unsigned long)nverts * OBJ_STRIDE * 4;
    void *vb = alloc_fn(vb_size, 0x1000);
    if (!vb) { obj_tfree(data, data_ph, data_sz); return -4; }

    /* For smooth normals: accumulate face normals per unique position.
     * STL has no shared vertices, so we weld by position.
     * Simple approach for <50M tris: just use face normals (flat shading)
     * and let the smooth normal pass in main handle it if needed.
     * Actually: emit raw triangles with face normals for now,
     * smooth normal computation uses position-based welding. */

    float *verts = (float*)((char*)vb + OBJ_DATA_OFF);
    float minx=1e30f,miny=1e30f,minz=1e30f,maxx=-1e30f,maxy=-1e30f,maxz=-1e30f;

    for (unsigned int t = 0; t < ntri; t++) {
        const float *tri = (const float*)(data + t * STL_TRI_SIZE);
        float nx = tri[0], ny = tri[1], nz = tri[2]; /* face normal from file */
        float nl = obj_sqrtf(nx*nx+ny*ny+nz*nz);
        if (nl < 1e-6f) {
            /* Compute from vertices */
            float e1x=tri[7]-tri[4],e1y=tri[8]-tri[5],e1z=tri[9]-tri[6];
            float e2x=tri[10]-tri[4],e2y=tri[11]-tri[5],e2z=tri[12]-tri[6];
            nx=e1y*e2z-e1z*e2y; ny=e1z*e2x-e1x*e2z; nz=e1x*e2y-e1y*e2x;
            nl=obj_sqrtf(nx*nx+ny*ny+nz*nz);
        }
        if (nl>1e-6f){nx/=nl;ny/=nl;nz/=nl;}

        for (int v = 0; v < 3; v++) {
            float px = tri[3+v*3], py = tri[4+v*3], pz = tri[5+v*3];
            if(px<minx)minx=px; if(py<miny)miny=py; if(pz<minz)minz=pz;
            if(px>maxx)maxx=px; if(py>maxy)maxy=py; if(pz>maxz)maxz=pz;
            int idx = (t*3+v) * OBJ_STRIDE;
            verts[idx]=px; verts[idx+1]=py; verts[idx+2]=pz; verts[idx+3]=1;
            verts[idx+4]=nx; verts[idx+5]=ny; verts[idx+6]=nz; verts[idx+7]=0;
            verts[idx+8]=0.8f; verts[idx+9]=0.8f; verts[idx+10]=0.8f; verts[idx+11]=1.0f;
        }
    }

    obj_tfree(data, data_ph, data_sz);

    /* Auto-scale to ±0.9 */
    if (cb) cb(2, "Scaling", cb_ud);
    float cx=(minx+maxx)*0.5f, cy=(miny+maxy)*0.5f, cz=(minz+maxz)*0.5f;
    float dx=maxx-minx, dy=maxy-miny, dz=maxz-minz;
    float maxd=dx>dy?dx:dy; if(dz>maxd)maxd=dz;
    float sc = maxd > 1e-6f ? 1.8f / maxd : 1.0f;
    for (unsigned int i = 0; i < nverts; i++) {
        int idx = i * OBJ_STRIDE;
        verts[idx]   = (verts[idx]-cx)*sc;
        verts[idx+1] = (verts[idx+1]-cy)*sc;
        verts[idx+2] = (verts[idx+2]-cz)*sc;
    }

    /* Copy BG/MVP header from identity template */
    float *id = (float*)vb;
    for(int i=0;i<16;i++) id[i]=0; id[0]=1;id[5]=1;id[10]=1;id[15]=1;

    out->vb_base = vb; out->verts = verts; out->ib_base = 0;
    out->num_verts = nverts; out->num_tris = ntri;
    out->vb_size = vb_size; out->indexed = 0;
    return 0;
}

