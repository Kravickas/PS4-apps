#pragma once
/* Model loaders (OBJ / STL / PLY). Implemented in loaders.c, which the Makefile
   builds with -O2: parsing a multi-GB text OBJ is CPU-bound. */
#include <stdint.h>

/* Model vertex buffer: [identity 64][BG sun 16][BG quad 6 x 48][MVP 64][sun 16]
   then vertices at OBJ_DATA_OFF, 12 floats (48 bytes) each. */
#define OBJ_STRIDE 12
#define OBJ_BG_VERTS 6
#define OBJ_IDENT_SIZE 64
#define OBJ_BG_SUN_SIZE 16
#define OBJ_BG_SIZE (OBJ_BG_VERTS * OBJ_STRIDE * 4)
#define OBJ_MVP_OFF (OBJ_IDENT_SIZE + OBJ_BG_SUN_SIZE + OBJ_BG_SIZE)
#define OBJ_MVP_SIZE 64
#define OBJ_SUN_SIZE 16
#define OBJ_DATA_OFF (OBJ_MVP_OFF + OBJ_MVP_SIZE + OBJ_SUN_SIZE)

typedef struct {
    void* vb_base;
    float* verts;
    uint32_t* ib_base;
    int num_verts, num_tris, num_indices, indexed;
    unsigned long vb_size, ib_size;
} ObjMesh;

typedef void (*obj_progress_fn)(float frac, const char* msg, void* ud); /* frac 0..1 */

int obj_load_file(const char* path, void* (*alloc_fn)(unsigned long, unsigned long), ObjMesh* out,
                  obj_progress_fn progress, void* progress_ud);
int stl_load_binary(const char* path, void* (*alloc_fn)(unsigned long, unsigned long), ObjMesh* out,
                    obj_progress_fn cb, void* cb_ud);
int ply_load_file(const char* path, void* (*alloc_fn)(unsigned long, unsigned long), ObjMesh* out,
                  obj_progress_fn cb, void* cb_ud);
