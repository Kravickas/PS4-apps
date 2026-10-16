#pragma once
/* Tangent frame for the model shaders (vs_model / ps_model), written by the
   loaders into the vertex slots those shaders read (12 floats a vertex):
     o[3] = handedness h (+1 / -1): B = h * cross(N, T) points along +v
     o[7] = T.x, o[10] = T.y, o[11] = T.z (unit, T along +u)
   T is dP/du of the triangle projected onto each corner's normal plane
   (Gram-Schmidt). Triangles without usable UVs (none, zero UV area, or dP/du
   along the normal) get a unit vector perpendicular to N and h = +1.
   Computed from the loader's own arrays: the vertex buffer is write-combined
   GPU memory and is never read back. */

static inline float tangent_sqrt(float x) {
    if (x <= 0.0f)
        return 0.0f;
    __asm__("sqrtss %1, %0" : "=x"(x) : "x"(x));
    return x;
}

/* Unit vector perpendicular to the unit normal n. */
static inline void tangent_perp(const float n[3], float t[3]) {
    float a[3] = {1.0f, 0.0f, 0.0f};
    if (n[0] > 0.9f || n[0] < -0.9f) {
        a[0] = 0.0f;
        a[1] = 1.0f;
    }
    float d = a[0] * n[0] + a[1] * n[1] + a[2] * n[2];
    t[0] = a[0] - n[0] * d;
    t[1] = a[1] - n[1] * d;
    t[2] = a[2] - n[2] * d;
    float l = tangent_sqrt(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
    t[0] /= l;
    t[1] /= l;
    t[2] /= l;
}

/* One triangle: positions p, UVs uv (0 when the mesh has none), unit normals n
   per corner. Out: t[k] = (T.x, T.y, T.z, h) per corner. */
static inline void tangent_tri(const float p[3][3], const float uv[3][2], const float n[3][3],
                               int has_uv, float t[3][4]) {
    float e1[3], e2[3], s[3], b[3];
    for (int i = 0; i < 3; i++) {
        e1[i] = p[1][i] - p[0][i];
        e2[i] = p[2][i] - p[0][i];
    }
    float du1 = uv[1][0] - uv[0][0], dv1 = uv[1][1] - uv[0][1];
    float du2 = uv[2][0] - uv[0][0], dv2 = uv[2][1] - uv[0][1];
    float r = du1 * dv2 - du2 * dv1;
    float sg = (r < 0.0f) ? -1.0f : 1.0f; /* dP/du = s / r: only the sign of r matters */
    for (int i = 0; i < 3; i++) {
        s[i] = (e1[i] * dv2 - e2[i] * dv1) * sg;
        b[i] = (e2[i] * du1 - e1[i] * du2) * sg;
    }
    float sl = tangent_sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    for (int k = 0; k < 3; k++) {
        const float* N = n[k];
        float d = N[0] * s[0] + N[1] * s[1] + N[2] * s[2];
        float T[3] = {s[0] - N[0] * d, s[1] - N[1] * d, s[2] - N[2] * d};
        float l = tangent_sqrt(T[0] * T[0] + T[1] * T[1] + T[2] * T[2]);
        if (has_uv && r != 0.0f && sl > 0.0f && l > 1e-4f * sl) {
            T[0] /= l;
            T[1] /= l;
            T[2] /= l;
            float c0 = N[1] * T[2] - N[2] * T[1], c1 = N[2] * T[0] - N[0] * T[2],
                  c2 = N[0] * T[1] - N[1] * T[0];
            t[k][3] = (c0 * b[0] + c1 * b[1] + c2 * b[2] < 0.0f) ? -1.0f : 1.0f;
        } else {
            tangent_perp(N, T);
            t[k][3] = 1.0f;
        }
        t[k][0] = T[0];
        t[k][1] = T[1];
        t[k][2] = T[2];
    }
}
