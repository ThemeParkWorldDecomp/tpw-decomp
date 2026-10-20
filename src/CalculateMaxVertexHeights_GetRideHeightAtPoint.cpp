// Ride height lookups by polygon search: DoNodePolySearch 0x1005cce4 (708 B), GetHeightFromPolySearch 0x1005d000 (356 B),
// GetRideHeightAtPoint 0x1005d198 (316 B).
//
// The ride context and the nodes are read by Mac offset (32-bit layout, pointers as 32-bit words in 32-bit addressable memory).
#include <cstdint>
#include <cstring>

#include "TPEngine.h"
#include "LevelSystems.h"
#include "m3d2.h"
#include "Ptr32.h"
#include "ppc_math.h"
#include "rsse.h"

struct sEmbeddedDataNodeParams;

namespace MathLib {
int __fpclassifyf(float x);  // glue 0x10239268
}

// 0x102c1e10: the ride context. +0x04 the world (+0xa0 float height offset, +0x46 / +0x48 u16, +0x70 faces, +0x78 box (+0x44 float),
// +0x7c u32 flags per node), +0x10 int rotation, +0x28 embedded data (+0 flags, +4 params array), +0x2c node, +0x30 s3DPoint array.
Ptr32<uint8_t> gRideCtxRef;  // 0x102c1e10
static inline uint8_t* RideCtx() { return gRideCtxRef.get(); }
// 0x102c1e14: bit 0 height field, bit 3 flat, bit 1 and bit 2 polygon search variants.
uint32_t gRideFlags;  // 0x102c1e14

float DoNodePolySearch(tag_sNode* node, sEmbeddedDataNodeParams* params, float x, float z, s3DPoint* pt, int flag);
float GetHeightFromPolySearch(float x, float z, int a, int b);

namespace {

inline uint32_t U32(const uint8_t* b, size_t o) {
    uint32_t v;
    std::memcpy(&v, b + o, 4);
    return v;
}
inline uint32_t U16(const uint8_t* b, size_t o) {
    uint16_t v;
    std::memcpy(&v, b + o, 2);
    return v;
}
inline float F32(const uint8_t* b, size_t o) {
    float v;
    std::memcpy(&v, b + o, 4);
    return v;
}
inline uint8_t* Ptr(const uint8_t* b, size_t o) {
    Ptr32<uint8_t> p;
    p.mOffset = U32(b, o);
    return p.get();
}

}  // namespace

// 0x1005cce4 (708 bytes).
float DoNodePolySearch(tag_sNode* nodeP, sEmbeddedDataNodeParams* paramsP, float x, float z, s3DPoint* ptP, int flag) {
    const uint8_t* node = reinterpret_cast<const uint8_t*>(nodeP);
    const uint8_t* params = reinterpret_cast<const uint8_t*>(paramsP);
    const uint8_t* pt = reinterpret_cast<const uint8_t*>(ptP);
    float best = -100.0f;
    const uint8_t* set = Ptr(node, 0x6c);
    for (uint32_t sets = U16(node, 0x5a); sets != 0; --sets, set += 0x10) {
        const uint8_t* face = Ptr(node, 0x70) + U16(set, 4) * 8;
        const uint8_t* table = static_cast<const uint8_t*>(gPolySearchTable);
        if (params != nullptr) table += U16(params, 0x10) * 0xc;
        for (uint32_t n = U16(set, 6); n != 0; --n, face += 8) {
            if ((U16(face, 0) & 1) == 0) continue;
            const uint8_t* nrm;
            if (pt != nullptr) {
                nrm = pt;
                pt += 0xc;
            } else {
                nrm = Ptr(node, 0x64) + (U16(face, 0) >> 1) * 0xc;
            }
            const uint8_t* idx = Ptr(node, 0x94);
            float loc[12];
            const uint8_t* v[3];
            for (int k = 0; k < 3; ++k) {
                v[k] = table + U16(idx, U16(face, 2 + 2 * k) * 2) * 0xc;
                std::memcpy(&loc[3 + 3 * k], v[k], 12);
            }
            bool skip = false;
            for (int k = 3; k != 0; --k) {
                int m = k % 3;
                float a = loc[m * 3 + 3];
                float f8 = loc[k * 3];
                float f7 = loc[k * 3 + 2];
                float f4 = -a + f8;
                float f6 = loc[m * 3 + 5] - f7;
                float f5 = f4 * f7;
                f4 = z * f4;
                f5 = -ppc_fmadds(f6, f8, f5);
                f4 = ppc_fmadds(x, f6, f4);
                f4 = f5 + f4;
                if (f4 < 0.0f) {
                    skip = true;
                    break;
                }
            }
            if (skip) continue;
            loc[1] = F32(nrm, 4);
            switch (static_cast<int32_t>(U32(RideCtx(), 0x10))) {
            case 0xb4:
                loc[0] = -F32(nrm, 0);
                loc[2] = -F32(nrm, 8);
                break;
            case 0x5a:
                loc[0] = F32(nrm, 8);
                loc[2] = -F32(nrm, 0);
                break;
            case 0x10e:
                loc[0] = -F32(nrm, 8);
                loc[2] = F32(nrm, 0);
                break;
            default:
                loc[0] = F32(nrm, 0);
                loc[1] = F32(nrm, 4);
                loc[2] = F32(nrm, 8);
                break;
            }
            float f9 = loc[1];
            float f4 = f9 * loc[4];
            float f3 = z * loc[2];
            f4 = ppc_fmadds(loc[0], loc[3], f4);
            f3 = ppc_fmadds(x, loc[0], f3);
            f4 = -ppc_fmadds(loc[2], loc[5], f4);
            f3 = f4 + f3;
            f3 = -f3;
            f3 = f3 / f9;
            if (f3 > best) best = f3;
            if (flag != 0) return best;
        }
    }
    return best;
}

// 0x1005d000 (356 bytes).
float GetHeightFromPolySearch(float x, float z, int a, int b) {
    const uint8_t* ctx = RideCtx();
    const uint8_t* world = Ptr(ctx, 4);
    const uint8_t* embed = Ptr(ctx, 0x28);
    float best = -100.0f;
    if (b == 0 && U32(ctx, 0x2c) != 0) {
        best = DoNodePolySearch(reinterpret_cast<tag_sNode*>(Ptr(ctx, 0x2c)), nullptr, x, z,
                                reinterpret_cast<s3DPoint*>(Ptr(ctx, 0x30)), 1);
        if (best == -100.0f) best = F32(Ptr(world, 0x78), 0x44);
    }
    if (a == 0 && embed != nullptr && (U32(embed, 0) & 8) != 0) {
        const uint8_t* params = Ptr(RideCtx(), 0x28);
        params = Ptr(params, 4);
        uint8_t* node = Ptr(world, 0x70) + U16(world, 0x46) * 0xa0;
        const uint8_t* flags = Ptr(world, 0x7c);
        for (int32_t n = static_cast<int32_t>(U16(world, 0x48)); n != 0; --n) {
            uint32_t f = U32(flags, 0);
            if ((f & 2) != 0) {
                float r = DoNodePolySearch(reinterpret_cast<tag_sNode*>(node),
                                           reinterpret_cast<sEmbeddedDataNodeParams*>(const_cast<uint8_t*>(params)), x, z,
                                           nullptr, static_cast<int>(f & 4));
                if (r > best) {
                    if ((U32(flags, 0) & 4) != 0) return r;
                    best = r;
                }
            }
            params += 0x14;
            flags += 0x14;
            node += 0xa0;
        }
    }
    return best;
}

// 0x1005d198 (316 bytes).
void GetRideHeightAtPoint(float x, float z, float& y) {
    uint8_t* world = Ptr(RideCtx(), 4);
    float* offset = reinterpret_cast<float*>(world + 0xa0);
    if (MathLib::__fpclassifyf(*offset) == 1) *offset = 0.0f;
    uint32_t flags = gRideFlags;
    if ((flags & 1) != 0) {
        float h = DetermineHFieldHeightQuick(x, z);
        y = *reinterpret_cast<float*>(Ptr(RideCtx(), 4) + 0xa0) + h;
    } else if ((flags & 8) != 0) {
        y = *reinterpret_cast<float*>(Ptr(RideCtx(), 4) + 0xa0);
    } else if ((flags & 2) != 0) {
        float h = GetHeightFromPolySearch(x, z, 0, 1);
        y = *reinterpret_cast<float*>(Ptr(RideCtx(), 4) + 0xa0) + h;
    } else if ((flags & 4) != 0) {
        float h = GetHeightFromPolySearch(x, z, 1, 0);
        y = *reinterpret_cast<float*>(Ptr(RideCtx(), 4) + 0xa0) + h;
    } else {
        y = 0.0f;
    }
}
