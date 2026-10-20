// Debug.cpp range of the main app (SimTheme Park 2.04b3), unit v5221: DoOrSetupSkirtDeform (0x1006daa4, 3604 bytes).
//
// Walks a node hierarchy `b` (with a parallel hierarchy `a` supplying the original vertex positions) against a height field. Two modes,
// by `setup`: 0 = find the vertices that lie on the edge of the field's playable area and list their index (a pass that only
// selects); non-zero = move the listed vertices so their height follows the field (and widen the node's box).
#include <cmath>
#include <cstdint>

#include "Debug.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"

namespace {

// (float)(unsigned)n, as the original's magic-number conversion computes it (fsubs in single precision).
float UnsignedToFloat(uint32_t n) { return static_cast<float>(static_cast<double>(n)); }

bool Near(float frac, float tolerance) {
    return static_cast<float>(std::fabs(static_cast<double>(frac - 0.0f))) < tolerance ||
           static_cast<float>(std::fabs(static_cast<double>(frac - 1.0f))) < tolerance;
}

}  // namespace

void DoOrSetupSkirtDeform(tag_sNode* a, tag_sNode* b, uint16_t** list, sMatrix matrix, sHField* hf, int32_t setup, int32_t* count,
                          float lift, tag_sNode** bestNode, int32_t* bestCount, uint32_t flags, tag_sNode* only, int32_t mode) {
    float tolerance = 0.01f;       // f28
    float yTolerance = 0.01f;      // f27
    if (mode != 0) {
        tolerance = 0.1f;
        yTolerance = 100.0f;
    }
    CombineMatricesNoWDest2nd(reinterpret_cast<s3DAxis*>(&b->mMatrix), &matrix);
    const uint32_t nodeFlags = b->mFlags;
    const bool plain = (nodeFlags & 0x20000) == 0;   // r25
    bool ok;
    if (plain)
        ok = (flags & 2) == 0 && (setup == 0 || (nodeFlags & 0x1000) != 0);
    else
        ok = (flags & 4) == 0;
    if (ok && (nodeFlags & 0x200) == 0 && (only == nullptr || only == b)) {
        sMatrix inverse;
        if (setup != 0) InvertMatrix(&inverse, &matrix);
        int32_t hits = 0;
        uint32_t cell = 0;       // r20: column
        uint32_t row = 0;        // r23
        uint32_t i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0;
        float frac0 = 0.0f, frac1 = 0.0f;   // f30, f29
        for (int32_t v = 0; v < static_cast<int32_t>(b->mVertexCount); ++v) {
            bool take;
            if (plain) {
                if (setup == 0)
                    take = true;
                else
                    take = (b->mFlags & 0x1000) != 0 && static_cast<uint32_t>(v) == **list;
            } else {
                take = true;
            }
            if (take) {
                const uint32_t quadBase = static_cast<uint32_t>(v >> 2) * 0x30;   // (v >> 2) * 0x30
                const uint32_t lane = static_cast<uint32_t>(v) & 3;
                float* src = reinterpret_cast<float*>(reinterpret_cast<char*>(a->mVertices.get()) + quadBase) + lane;
                float point[3];
                point[0] = src[0];
                point[1] = src[4];
                point[2] = src[8];
                VectorThroughLMatrixNoW(&matrix, reinterpret_cast<s3DPoint*>(point));
                uint32_t bits = 0;               // r30
                bool nearX = false, nearZ = false;   // r19, r18
                int32_t steps = mode != 0 ? 2 : 1;   // r22
                uint32_t hIndex = 0;             // r28
                const uint32_t width = hf->mF18[0];
                const float* heights = hf->mHeights.get();
                for (int32_t axis = 0; axis < 2; ++axis) {
                    float t;
                    double ip;
                    float ipf;
                    float frac;
                    if (axis == 0) {
                        t = (point[0] - hf->mF08[0]) / hf->mF08[2];
                        if (!(t >= 0.0f)) t = 0.0f;
                        const double fr = std::modf(static_cast<double>(t), &ip);
                        frac0 = static_cast<float>(fr);
                        ipf = static_cast<float>(ip);
                        frac = frac0;
                        i13 = cw_cvt_fp2unsigned(static_cast<double>(t));
                        i14 = cw_cvt_fp2unsigned(static_cast<double>(ipf));
                        const uint32_t up = cw_cvt_fp2unsigned(static_cast<double>(ipf)) + 1;
                        i16 = up;
                        i15 = cw_cvt_fp2unsigned(static_cast<double>(ipf));
                        i17 = up;
                    } else {
                        t = (point[2] - hf->mF08[1]) / hf->mF08[3];
                        if (!(t >= 0.0f)) t = 0.0f;
                        const double fr = std::modf(static_cast<double>(t), &ip);
                        frac1 = static_cast<float>(fr);
                        ipf = static_cast<float>(ip);
                        frac = frac1;
                        i13 += cw_cvt_fp2unsigned(static_cast<double>(t)) * width;
                        const uint32_t stride = width + 1;
                        const uint32_t step = cw_cvt_fp2unsigned(static_cast<double>(ipf)) * stride;
                        i14 += step;
                        i16 += step;
                        const uint32_t step2 = cw_cvt_fp2unsigned(static_cast<double>(1.0f + ipf)) * stride;
                        i15 += step2;
                        i17 += step2;
                    }
                    if (plain) {
                        if (frac >= 0.5f) {
                            if (axis == 0) {
                                bits |= 4;
                                const float up = 1.0f + ipf;
                                hIndex = cw_cvt_fp2unsigned(static_cast<double>(up));
                                cell = cw_cvt_fp2unsigned(static_cast<double>(ipf));
                                if (up < 0.0f) return;
                                if (up >= UnsignedToFloat(hf->mF18[0] + 1)) return;
                            } else {
                                bits |= 2;
                                const float up = 1.0f + ipf;
                                hIndex += cw_cvt_fp2unsigned(static_cast<double>(up)) * (hf->mF18[0] + 1);
                                row = cw_cvt_fp2unsigned(static_cast<double>(ipf));
                                if (up < 0.0f) return;
                                if (up >= UnsignedToFloat(hf->mF18[1] + 1)) return;
                            }
                        } else if (axis == 0) {
                            bits |= 1;
                            hIndex = cw_cvt_fp2unsigned(static_cast<double>(ipf));
                            cell = cw_cvt_fp2unsigned(static_cast<double>(ipf));
                            if (ipf < 0.0f) return;
                            if (ipf >= UnsignedToFloat(hf->mF18[0] + 1)) return;
                        } else {
                            bits |= 8;
                            hIndex += cw_cvt_fp2unsigned(static_cast<double>(ipf)) * (hf->mF18[0] + 1);
                            row = cw_cvt_fp2unsigned(static_cast<double>(ipf));
                            if (ipf < 0.0f) return;
                            if (ipf >= UnsignedToFloat(hf->mF18[1] + 1)) return;
                        }
                    }
                    if (setup == 0 && plain) {
                        if (axis == 0) {
                            const uint32_t w = gSkirtColumns;
                            if (cell >= w) {
                                if ((bits & 1) != 0) {
                                    bits &= ~1u;
                                    bits |= 4;
                                }
                                cell = w - 1;
                            }
                        } else {
                            const uint32_t h = gSkirtRows;
                            if (row >= h) {
                                if ((bits & 8) != 0) {
                                    bits &= ~8u;
                                    bits |= 2;
                                }
                                row = h - 1;
                            }
                        }
                    }
                    if (setup != 0) {
                        if (plain) {
                            if (Near(frac, tolerance)) {
                                const float origin = axis == 0 ? hf->mF08[0] : hf->mF08[1];
                                const float scale = axis == 0 ? hf->mF08[2] : hf->mF08[3];
                                const float whole = frac >= 0.5f ? 1.0f + ipf : ipf;
                                const float result = ppc_fmadds(whole, scale, origin);
                                if (axis == 0) {
                                    nearX = true;
                                    point[0] = result;
                                } else {
                                    nearZ = true;
                                    point[2] = result;
                                }
                            }
                        }
                    } else if (Near(frac, tolerance)) {
                        if (axis == 0)
                            nearX = true;
                        else
                            nearZ = true;
                    } else if (steps != 0) {
                        --steps;
                    }
                }
                bool hit = false;
                if (setup == 0) {
                    if (plain && steps != 0) {
                        if (static_cast<float>(std::fabs(static_cast<double>(point[1] - 0.0f))) < yTolerance) {
                            if ((flags & 1) != 0) {
                                hit = true;
                            } else {
                                const uint32_t word = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(gSkirtCellFlags) + row * 0x50 + cell * 4);
                                if (nearX && (bits & 1) != 0 && (word & 2) != 0) hit = true;
                                if (nearZ && (bits & 2) != 0 && (word & 4) != 0) hit = true;
                                if (nearX && (bits & 4) != 0 && (word & 8) != 0) hit = true;
                                if (nearZ && (bits & 8) != 0 && (word & 1) != 0) hit = true;
                            }
                        }
                        if (hit) {
                            b->mFlags |= 0x1000;
                            *count = *count + 1;
                            **list = static_cast<uint16_t>(v);
                            *list = *list + 1;
                            hits = hits + 1;
                        }
                    }
                } else if (plain) {
                    if (mode == 0) {
                        point[1] = heights[hIndex];
                    } else {
                        float result;
                        if (nearX) {
                            if ((bits & 2) != 0) {
                                const float f3 = 1.0f - frac1;
                                const float lowerH = heights[static_cast<int32_t>(hIndex - (hf->mF18[0] + 1))];
                                result = ppc_fmadds(1.0f - f3, heights[hIndex], f3 * lowerH);
                            } else {
                                const float upperH = heights[hf->mF18[0] + hIndex + 1];
                                result = ppc_fmadds(1.0f - frac1, heights[hIndex], frac1 * upperH);
                            }
                        } else if ((bits & 4) != 0) {
                            const float f3 = 1.0f - frac0;
                            const float lowerH = heights[static_cast<int32_t>(hIndex - 1)];
                            result = ppc_fmadds(1.0f - f3, heights[hIndex], f3 * lowerH);
                        } else {
                            const float nextH = heights[hIndex + 1];
                            result = ppc_fmadds(1.0f - frac0, heights[hIndex], frac0 * nextH);
                        }
                        point[1] = point[1] + (result - lift);
                    }
                } else {
                    const uint16_t quadFlags = *reinterpret_cast<const uint16_t*>(reinterpret_cast<const char*>(hf->mQuads.get()) + i13 * 4);
                    bool upperA, upperB;
                    if ((quadFlags & 4) != 0) {
                        upperA = upperB = frac0 >= 1.0f - frac1;
                    } else if (frac0 >= frac1) {
                        upperA = true;
                        upperB = false;
                    } else {
                        upperA = false;
                        upperB = true;
                    }
                    float slope;
                    if (upperA)
                        slope = heights[i17] - heights[i16];
                    else
                        slope = heights[i15] - heights[i14];
                    float result;
                    if (upperB) {
                        const float f4 = 1.0f - frac0;
                        const float f2 = frac0 * heights[i17];
                        const float inner = ppc_fmadds(f4, heights[i15], f2);
                        result = ppc_fmadds(1.0f - frac1, -slope, inner);
                    } else {
                        const float f0 = frac0 * heights[i16];
                        const float inner = ppc_fmadds(1.0f - frac0, heights[i14], f0);
                        result = ppc_fmadds(frac1, slope, inner);
                    }
                    point[1] = point[1] + (result - lift);
                }
                if (bestCount != nullptr && hits > *bestCount) {
                    *bestCount = hits;
                    *bestNode = b;
                }
                if (setup != 0) {
                    VectorThroughLMatrixNoW(&inverse, reinterpret_cast<s3DPoint*>(point));
                    float* dst = reinterpret_cast<float*>(reinterpret_cast<char*>(b->mVertices.get()) + quadBase) + lane;
                    dst[0] = point[0];
                    dst[4] = point[1];
                    dst[8] = point[2];
                    float* lo = b->mBox.mMin;
                    float* hi = b->mBox.mMax;
                    if (point[0] < lo[0])
                        lo[0] = point[0];
                    else if (point[0] > hi[0])
                        hi[0] = point[0];
                    if (point[1] < lo[1])
                        lo[1] = point[1];
                    else if (point[1] > hi[1])
                        hi[1] = point[1];
                    if (point[2] < lo[2])
                        lo[2] = point[2];
                    else if (point[2] > hi[2])
                        hi[2] = point[2];
                    if ((b->mFlags & 0x1000) != 0) *list = *list + 1;
                }
            }
        }
    }
    tag_sNode* p = a->mF0C.get();
    for (tag_sNode* q = b->mF0C.get(); p != nullptr && q != nullptr; q = q->mF08.get()) {
        DoOrSetupSkirtDeform(p, q, list, matrix, hf, setup, count, lift, bestNode, bestCount, flags, only, mode);
        p = p->mF08.get();
    }
}
