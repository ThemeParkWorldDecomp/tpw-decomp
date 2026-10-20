// MatrixPowerPC.cpp (decompiled from the engine library of SimTheme Park 2.04b3,
// CMatrixPowerPC at 0x100418ac..0x10042ba8; original file name unknown).
//
// The original loops are unrolled by two or four; the plain loops here do the same work
// in the same order. Loop counts are unsigned, as the original's (a negative count runs
// 2^32 - |count| times there too). ppc_* helpers mark fused instructions (ppc_math.h).
#include "MatrixPowerPC.h"

#include <cstdint>

#include "platform/interfacelib.h"
#include "ppc_math.h"
#include "tb_resource_display.h"

namespace {
const unsigned char kCodeMissing[] = "\x0c" "code missing";   // Pascal string for DebugStr
}

CMatrixPowerPC::~CMatrixPowerPC() {
}

void CMatrixPowerPC::Transform(sMatrix* m, void* out, sVertexPosnSet* in, int32_t outStride, int32_t first,
                               int32_t count) {
    uint32_t lane = static_cast<uint32_t>(first) & 3;
    const uint8_t* block = reinterpret_cast<const uint8_t*>(in) + (first >> 2) * 0x30;
    uint8_t* o = static_cast<uint8_t*>(out);
    for (uint32_t n = static_cast<uint32_t>(count); n != 0; n--) {
        const float* p = reinterpret_cast<const float*>(block + lane * 4);
        float y = p[4], x = p[0], z = p[8];   // lanes of the y, x and z arrays
        lane += 1;
        block += (static_cast<int32_t>(lane) >> 2) * 0x30;
        lane &= 3;
        float* d = reinterpret_cast<float*>(o);
        d[0] = m->m[3][0] + ppc_fmadds(z, m->m[2][0], ppc_fmadds(x, m->m[0][0], y * m->m[1][0]));
        d[1] = m->m[3][1] + ppc_fmadds(z, m->m[2][1], ppc_fmadds(x, m->m[0][1], y * m->m[1][1]));
        d[2] = m->m[3][2] + ppc_fmadds(z, m->m[2][2], ppc_fmadds(x, m->m[0][2], y * m->m[1][2]));
        d[3] = m->m[3][3] + ppc_fmadds(z, m->m[2][3], ppc_fmadds(x, m->m[0][3], y * m->m[1][3]));
        o += outStride;
    }
}

void CMatrixPowerPC::Transform(sMatrix* m, void* out, void* in, int32_t outStride, int32_t inStride, int32_t count,
                               int32_t type) {
    uint8_t* o = static_cast<uint8_t*>(out);
    const uint8_t* s = static_cast<const uint8_t*>(in);
    uint32_t n = static_cast<uint32_t>(count);
    if (type == 2) {
        // xyz -> xyz with translation
        for (; n != 0; n--) {
            const float* v = reinterpret_cast<const float*>(s);
            float y = v[1], x = v[0], z = v[2];
            s += inStride;
            float* d = reinterpret_cast<float*>(o);
            d[0] = m->m[3][0] + ppc_fmadds(z, m->m[2][0], ppc_fmadds(x, m->m[0][0], y * m->m[1][0]));
            d[1] = m->m[3][1] + ppc_fmadds(z, m->m[2][1], ppc_fmadds(x, m->m[0][1], y * m->m[1][1]));
            d[2] = m->m[3][2] + ppc_fmadds(z, m->m[2][2], ppc_fmadds(x, m->m[0][2], y * m->m[1][2]));
            o += outStride;
        }
    } else if (type == 1) {
        // xyzw -> xyzw
        for (; n != 0; n--) {
            const float* v = reinterpret_cast<const float*>(s);
            float y = v[1], x = v[0], z = v[2], w = v[3];
            s += inStride;
            float* d = reinterpret_cast<float*>(o);
            d[0] = ppc_fmadds(w, m->m[3][0], ppc_fmadds(z, m->m[2][0], ppc_fmadds(x, m->m[0][0], y * m->m[1][0])));
            d[1] = ppc_fmadds(w, m->m[3][1], ppc_fmadds(z, m->m[2][1], ppc_fmadds(x, m->m[0][1], y * m->m[1][1])));
            d[2] = ppc_fmadds(w, m->m[3][2], ppc_fmadds(z, m->m[2][2], ppc_fmadds(x, m->m[0][2], y * m->m[1][2])));
            d[3] = ppc_fmadds(w, m->m[3][3], ppc_fmadds(z, m->m[2][3], ppc_fmadds(x, m->m[0][3], y * m->m[1][3])));
            o += outStride;
        }
    } else if (type == 3) {
        // xyz -> xyz, rotation only
        for (; n != 0; n--) {
            const float* v = reinterpret_cast<const float*>(s);
            float y = v[1], x = v[0], z = v[2];
            s += inStride;
            float* d = reinterpret_cast<float*>(o);
            d[0] = ppc_fmadds(z, m->m[2][0], ppc_fmadds(x, m->m[0][0], y * m->m[1][0]));
            d[1] = ppc_fmadds(z, m->m[2][1], ppc_fmadds(x, m->m[0][1], y * m->m[1][1]));
            d[2] = ppc_fmadds(z, m->m[2][2], ppc_fmadds(x, m->m[0][2], y * m->m[1][2]));
            o += outStride;
        }
    } else {
        // type 0 and anything outside 1..3: xyz -> xyzw with translation
        for (; n != 0; n--) {
            const float* v = reinterpret_cast<const float*>(s);
            float y = v[1], x = v[0], z = v[2];
            s += inStride;
            float* d = reinterpret_cast<float*>(o);
            d[0] = m->m[3][0] + ppc_fmadds(z, m->m[2][0], ppc_fmadds(x, m->m[0][0], y * m->m[1][0]));
            d[1] = m->m[3][1] + ppc_fmadds(z, m->m[2][1], ppc_fmadds(x, m->m[0][1], y * m->m[1][1]));
            d[2] = m->m[3][2] + ppc_fmadds(z, m->m[2][2], ppc_fmadds(x, m->m[0][2], y * m->m[1][2]));
            d[3] = m->m[3][3] + ppc_fmadds(z, m->m[2][3], ppc_fmadds(x, m->m[0][3], y * m->m[1][3]));
            o += outStride;
        }
    }
}

float CMatrixPowerPC::Project(void* verts, int32_t stride, int32_t count, int32_t type) {
    float sum = 0.0f;
    uint8_t* p = static_cast<uint8_t*>(verts);
    uint32_t n = static_cast<uint32_t>(count);
    if (type == 2) {
        for (; n != 0; n--) {
            float* v = reinterpret_cast<float*>(p);
            v[1] = -v[1];
            float z = v[2] * gei_Display.mZ60;
            v[2] = z;
            sum = sum + z;
            v[2] = static_cast<float>(ppc_fmsub(2.0, static_cast<double>(v[2]), 1.0));
            p += stride;
        }
    } else if (type == 1) {
        for (; n != 0; n--) {
            float* v = reinterpret_cast<float*>(p);
            v[1] = -v[1];
            float z = gei_Display.mZ7C * v[3];
            v[2] = z;
            sum = sum + z;
            float w = v[3];
            float q = v[2] / w;
            v[2] = static_cast<float>(static_cast<double>(w) * ppc_fmsub(2.0, static_cast<double>(q), 1.0));
            p += stride;
        }
    } else {
        // the depth constants are read once, before the loop
        float split = gei_Display.mZ68;
        float nearW = gei_Display.mZ78;
        float nearZW = gei_Display.mZ74;
        float farW = gei_Display.mZ70;
        float farZ = gei_Display.mZ6C;
        for (; n != 0; n--) {
            float* v = reinterpret_cast<float*>(p);
            v[1] = -v[1];
            float z = v[2];
            float r;
            if (z < split) {
                float w = v[3];
                r = ppc_fmadds(nearW, w, nearZW * (z * w));
            } else {
                float t = z * farZ;
                float w = v[3];
                r = ppc_fmadds(farW, w, t);
            }
            sum = sum + r;
            v[2] = r;
            float w = v[3];
            float q = v[2] / w;
            v[2] = static_cast<float>(static_cast<double>(w) * ppc_fmsub(2.0, static_cast<double>(q), 1.0));
            p += stride;
        }
    }
    return sum;
}

void CMatrixPowerPC::Light(SFVF_TLCTEX* verts, tag_sNode* node, sFaceSet* faces, int32_t count) {
    DebugStr(kCodeMissing);
}

void CMatrixPowerPC::TransformUV(STMCoordinateSet* out, sUVSet* in, int32_t a, int32_t b, int32_t c) {
    DebugStr(kCodeMissing);
}

void CMatrixPowerPC::MultMatrix(const float* a, const float* b, float* out) {
    // Accumulates in place in `out`, re-reading it at every step, so an `out` that aliases
    // `a` or `b` gives the original's results.
    for (int32_t j = 0; j < 4; j++) {
        for (int32_t i = 0; i < 4; i++) {
            float* o = &out[i * 4 + j];
            *o = 0.0f;
            for (int32_t k = 0; k < 4; k++) *o = ppc_fmadds(a[i * 4 + k], b[k * 4 + j], *o);
        }
    }
}
