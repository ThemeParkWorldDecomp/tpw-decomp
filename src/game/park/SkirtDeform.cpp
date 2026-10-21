// SkirtDeform.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "CBalanceTable.h"
#include "Debug.h"
#include "LevelSystems.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "tb_matrix.h"
#include "tb_mesh.h"
#include "ThingReceivers.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ui_main.h"
#include <cmath>
#include <cstdint>
#include <cstring>

// gameplay_leaves.cpp (main, unit v5L07): small leaves a park run reaches, from different Mac files: CellIsRide 0x1006d718
// (the inventory's Debug.cpp range), LocatorByeByeKid 0x101bf98c (ui_staffPanel.cpp), CObject::IsCoaster 0x10110cd8 (Object.cpp) and CAdvisor::FinalDestroy 0x1000a0ec (Advisor.cpp).

// 0x1006d718 (40 bytes): a cell mode is a ride's when any of its low four bits is set (the Mac tests 4, then 9, then 0xa).
int32_t CellIsRide(CELL_MODES mode) {
    const int32_t bits = static_cast<int32_t>(mode);
    if ((bits & 4) != 0 || (bits & 9) != 0 || (bits & 0xa) != 0) return 1;
    return 0;
}

// 0x1006d76c: marks the edges of a ride shape (width columns, height rows) for the skirt: a cell is solid when its value has a bit of
// 0xf; a solid and an empty neighbour (outside the shape counts as empty) set the bit of that side in gSkirtCellFlags: 4 below,
// 2 left, 1 above, 8 right.
void DecideRideShapeDeformEdges(RideShape* shape, int32_t width, int32_t height) {
    std::memset(gSkirtCellFlags, 0, 0x640);
    gSkirtColumns = static_cast<uint32_t>(width);
    gSkirtRows = static_cast<uint32_t>(height);
    auto solid = [&](int32_t row, int32_t col) {
        uint32_t v = static_cast<uint32_t>(shape->mCells[row][col].mValue);
        return (v & 4) != 0 || (v & 9) != 0 || (v & 0xa) != 0;
    };
    for (int32_t row = 0; row < height; ++row) {
        for (int32_t col = 0; col < width; ++col) {
            bool self = solid(row, col);
            bool other = row < height - 1 ? solid(row + 1, col) : false;
            if ((self || other) && self != other) gSkirtCellFlags[row][col] |= 4;
            other = col > 0 ? solid(row, col - 1) : false;
            if ((self || other) && self != other) gSkirtCellFlags[row][col] |= 2;
            other = row > 0 ? solid(row - 1, col) : false;
            if ((self || other) && self != other) gSkirtCellFlags[row][col] |= 1;
            other = col < width - 1 ? solid(row, col + 1) : false;
            if ((self || other) && self != other) gSkirtCellFlags[row][col] |= 8;
        }
    }
}

// Debug.cpp range of the main app (SimTheme Park 2.04b3), unit v5221: DoOrSetupSkirtDeform (0x1006daa4, 3604 bytes).
//
// Walks a node hierarchy `b` (with a parallel hierarchy `a` supplying the original vertex positions) against a height field. Two modes,
// by `setup`: 0 = find the vertices that lie on the edge of the field's playable area and list their index (a pass that only
// selects); non-zero = move the listed vertices so their height follows the field (and widen the node's box).

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
    CombineMatricesNoWDest2nd(&b->mMatrix, &matrix);
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

// Deforms the skirt of a mesh against a height field: once for the mesh's own list (the block of the mesh's parent mesh), then once for
// every node of the header that has a texture record (its list is the word at texture + 4). Does nothing without a height-field header
// or without a height field in it. Afterwards flags the first node 0x10000 and the header 0x40000.
void DoMeshSkirtDeform(tag_sMesh* mesh, tag_sM3D2Header* header, float y, uint32_t a) {
    tag_sM3D2Header* meshHeader = mesh->mHeader;
    tag_sM3D2Header* self = meshHeader->mSelf.get();
    if (header == nullptr) return;
    if (header->mHField.get() == nullptr) return;

    sMatrix matrix;
    SetMatrixIdentity(&matrix);
    uint16_t* list = static_cast<uint16_t*>(mesh->mF08->mBlock14);
    if ((meshHeader->mFlags & 0x100) != 0) a |= 1;
    DoOrSetupSkirtDeform(self->mF78.get(), meshHeader->mF78.get(), &list, matrix, header->mHField.get(), 1, nullptr, y, nullptr, nullptr,
                         a, nullptr, 0);

    tag_sNode* node = meshHeader->mNodes.get();
    for (uint32_t n = meshHeader->mNodeCount; n != 0; --n, ++node) {
        if (node->mF98.get() == nullptr) continue;
        list = reinterpret_cast<uint16_t*>(node->mF98->mTexture.get());
        DoOrSetupSkirtDeform(self->mF78.get(), meshHeader->mF78.get(), &list, matrix, header->mHField.get(), 1, nullptr, y, nullptr,
                             nullptr, a, node, 1);
    }
    meshHeader->mF78->mFlags |= 0x10000;
    meshHeader->mFlags |= 0x40000;
}

// Debug.cpp range of the main app (SimTheme Park 2.04b3), unit w133: SetBaseBlendFlags (0x1006ebec, 116 bytes).

// Sets (on != 0) or clears bit 0x800 of the first halfword of the texture record of every face set of the node, unless the node has
// flag 0x200. The original answers the last texture record (or the mesh) in r3; the callers drop it.
void SetBaseBlendFlags(tag_sMesh* mesh, tag_sNode* node, int32_t on) {
    static_cast<void>(mesh);
    if ((node->mFlags & 0x200) != 0) return;
    sFaceSet* set = node->mFaceSets.get();
    if (on != 0) {
        for (uint32_t n = node->mFaceSetCount; n != 0; --n) {
            sTexture* texture = set->mTexture.get();
            ++set;
            texture->mF00 = static_cast<uint16_t>(texture->mF00 | 0x800);
        }
        return;
    }
    for (uint32_t n = node->mFaceSetCount; n != 0; --n) {
        sTexture* texture = set->mTexture.get();
        ++set;
        texture->mF00 = static_cast<uint16_t>(texture->mF00 & 0xf7ff);
    }
}

// CalcSkirtTextureDirnFlags 0x1006eca0 (2648 bytes): for every face of a ride mesh's root node, finds the skirt cell it covers
// (through the height field's cell size) and keeps in the cell the face whose edges run most along x and z; then
// writes for each cell of the ride's footprint a texture direction flag word and the texture number into mesh->mSkirtVertices.

// 0x102d1fa8 and 0x102d1fac (invented names): the cell last looked at, and the 20 x 20 table of cells.
SkirtCell* gSkirtCurrentCell;     // 0x102d1fa8
SkirtCell gSkirtCells[20][20];    // 0x102d1fac

namespace {

// The glue at 0x10237cd8: a double to an integer, truncated.
int32_t FpToInt(double value) { return static_cast<int32_t>(static_cast<int64_t>(value)); }

const char kSkirtErrorFormat[] = "Could not generate skirt flags for ride %s (root node position %.3f, %.3f, %.3f)\n";
const char kSkirtCountFormat[] = "Has %d flipped and %d non-flipped skirt textures\n";

}  // namespace

void CalcSkirtTextureDirnFlags(tag_sRideAnimSet* set, RideShape* shape, int32_t width, int32_t height, int32_t force) {
    const uint8_t* blend = static_cast<const uint8_t*>(LLCC_GetRideBlendInfo(shape));
    tag_sMesh* mesh = set->mHeads;
    tag_sM3D2Header* header = mesh->mHeader;
    tag_sNode* node = mesh->mRootNode;
    if (node == nullptr) node = header->mF78.get();
    const uint16_t* indices = node->mF94.get();
    mesh->mSkirtVertices = MReleaseAllocF(static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * 4, 1);
    int32_t flipped = 0;
    int32_t notFlipped = 0;
    int32_t reported = 0;
    GetNodeWorldVertPosnsIntoGeomBuffer(node);

    for (uint32_t row = 0; row < static_cast<uint32_t>(height); ++row) {
        for (uint32_t col = 0; col < static_cast<uint32_t>(width); ++col) {
            SkirtCell* cell = reinterpret_cast<SkirtCell*>(reinterpret_cast<char*>(gSkirtCells) + row * 0x690 + col * 0x54);
            cell->mBestX = 0.0f;
            cell->mBestZ = 0.0f;
            cell->mFaceSet = -1;
            cell->mFace = -1;
        }
    }

    uint32_t dirXIndex = 0;
    uint32_t dirZIndex = 0;
    const s3DPoint* dirX = nullptr;
    const s3DPoint* dirZ = nullptr;
    sFaceSet* faceSet = node->mFaceSets.get();
    for (uint32_t setNo = 0; setNo < node->mFaceSetCount; ++setNo) {
        sFace* face = node->mFaces.get() + faceSet->mFirstFace;
        for (uint32_t faceNo = 0; faceNo < faceSet->mFaceCount; ++faceNo) {
            char* geom = static_cast<char*>(gScratchGeomBuffer);
            const s3DPoint* pts[3];
            pts[0] = reinterpret_cast<const s3DPoint*>(geom + indices[face->mF02[0]] * 0xc);
            pts[1] = reinterpret_cast<const s3DPoint*>(geom + indices[face->mF02[1]] * 0xc);
            pts[2] = reinterpret_cast<const s3DPoint*>(geom + indices[face->mF02[2]] * 0xc);
            const sHField* field = gHeightfieldHeader->mHField.get();
            float sumX = pts[0]->x + pts[1]->x;
            sumX = sumX + pts[2]->x;
            float sumZ = pts[0]->z + pts[1]->z;
            sumZ = sumZ + pts[2]->z;
            float fx = (sumX / 3.0f) / field->mF08[2];
            float fz = (sumZ / 3.0f) / field->mF08[3];
            int32_t cx = FpToInt(fx);
            int32_t cz = FpToInt(fz);
            SkirtCell* cell = reinterpret_cast<SkirtCell*>(reinterpret_cast<char*>(gSkirtCells) + cz * 0x690 + cx * 0x54);
            gSkirtCurrentCell = cell;
            float bestX = 0.0f;
            float bestZ = 0.0f;
            sTexture* texture = faceSet->mTexture.get();
            if (force != 0 || blend == nullptr || blend[(static_cast<uint32_t>(cx) << 4) + static_cast<uint32_t>(cz)] != 0)
                texture->mF00 = static_cast<uint16_t>(texture->mF00 | 0x800);
            else
                texture->mF00 = static_cast<uint16_t>(texture->mF00 & 0xf7ff);

            s3DPoint edges[3];
            for (uint32_t k = 0; k < 3; ++k) {
                const s3DPoint* from = pts[k];
                const s3DPoint* to = pts[(k + 1) % 3];
                s3DPoint* e = &edges[k];
                *e = *to;
                e->x = e->x - from->x;
                e->y = e->y - from->y;
                e->z = e->z - from->z;
                double sum = (static_cast<double>(e->x) * static_cast<double>(e->x)) + static_cast<double>(e->y * e->y);
                sum = static_cast<double>(e->z * e->z) + sum;
                float length = static_cast<float>(std::sqrt(sum));
                e->x = e->x / length;
                e->y = e->y / length;
                e->z = e->z / length;
                float ax = static_cast<float>(std::fabs(static_cast<double>(e->x)));
                float az = static_cast<float>(std::fabs(static_cast<double>(e->z)));
                if (!(ax <= bestX)) {
                    bestX = ax;
                    dirX = e;
                    dirXIndex = k;
                }
                if (!(az <= bestZ)) {
                    bestZ = az;
                    dirZ = e;
                    dirZIndex = k;
                }
            }
            if (!(bestX <= 0.9f) && !(bestZ <= 0.9f)) {
                cell = gSkirtCurrentCell;
                if (bestX > cell->mBestX || !(bestZ <= cell->mBestZ)) {
                    cell->mBestX = bestX;
                    gSkirtCurrentCell->mBestZ = bestZ;
                    gSkirtCurrentCell->mFaceSet = static_cast<int32_t>(setNo);
                    gSkirtCurrentCell->mFace = static_cast<int32_t>(faceNo);
                    gSkirtCurrentCell->mCorner[0] = *pts[0];
                    gSkirtCurrentCell->mCorner[1] = *pts[1];
                    gSkirtCurrentCell->mCorner[2] = *pts[2];
                    gSkirtCurrentCell->mDirX = *dirX;
                    gSkirtCurrentCell->mDirZ = *dirZ;
                    gSkirtCurrentCell->mIndexX = dirXIndex;
                    gSkirtCurrentCell->mIndexZ = dirZIndex;
                }
            }
            ++face;
        }
        ++faceSet;
    }

    StripRideWrapping(set, node, 0.02f, const_cast<uint8_t*>(blend), static_cast<s3DPoint*>(gScratchGeomBuffer));

    for (uint32_t row = 0; row < static_cast<uint32_t>(height); ++row) {
        for (uint32_t col = 0; col < static_cast<uint32_t>(width); ++col) {
            uint16_t* out = reinterpret_cast<uint16_t*>(static_cast<char*>(mesh->mSkirtVertices) + (row * static_cast<uint32_t>(width) + col) * 4);
            out[0] = 1;
            out[1] = 10000;
            if (force == 0 && blend != nullptr && blend[(col << 4) + row] == 0) continue;
            if (force == 0 && shape->mCells[row][col].mValue == 0) continue;
            SkirtCell* cell = reinterpret_cast<SkirtCell*>(reinterpret_cast<char*>(gSkirtCells) + row * 0x690 + col * 0x54);
            gSkirtCurrentCell = cell;
            if (cell->mFaceSet == -1) {
                if (reported == 0) {
                    tag_sM3D2Header* h = mesh->mHeader;
                    const char* root = reinterpret_cast<const char*>(h->mF78.get());
                    double px = static_cast<double>(*reinterpret_cast<const float*>(root + 0x40));
                    double py = static_cast<double>(*reinterpret_cast<const float*>(root + 0x44));
                    double pz = static_cast<double>(*reinterpret_cast<const float*>(root + 0x48));
                    ERRORBox(kSkirtErrorFormat, h->mName, px, py, pz);
                    reported = 1;
                }
                continue;
            }
            sFaceSet* fs = node->mFaceSets.get() + static_cast<uint32_t>(cell->mFaceSet);
            uint32_t faceIndex = static_cast<uint32_t>(fs->mFirstFace) + static_cast<uint32_t>(cell->mFace);
            sFace* skirtFace = node->mFaces.get() + faceIndex;
            if (cell->mBestX <= 0.9f) continue;
            if (cell->mBestZ <= 0.9f) continue;

            uint32_t a, b, c, d;
            if (!(cell->mDirX.x <= 0.0f)) {
                a = cell->mIndexX;
                b = (cell->mIndexX + 1) % 3;
            } else {
                b = cell->mIndexX;
                a = (cell->mIndexX + 1) % 3;
            }
            if (!(cell->mDirZ.z <= 0.0f)) {
                c = cell->mIndexZ;
                d = (cell->mIndexZ + 1) % 3;
            } else {
                d = cell->mIndexZ;
                c = (cell->mIndexZ + 1) % 3;
            }
            uint32_t idx[4] = {0, 0, 0, 0};
            int32_t pos;
            if (a == c) {
                idx[0] = a;
                pos = 2;
                idx[1] = d;
                idx[3] = b;
            } else if (d == a) {
                idx[0] = c;
                pos = 3;
                idx[1] = d;
                idx[2] = b;
            } else if (b == d) {
                idx[1] = a;
                pos = 0;
                idx[2] = b;
                idx[3] = c;
            } else {
                idx[0] = a;
                pos = 1;
                idx[2] = d;
                idx[3] = b;
            }
            float uv[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
            for (uint32_t k = 0; k < 4; ++k) {
                if (k == static_cast<uint32_t>(pos)) continue;
                uint32_t u = skirtFace->mF02[idx[k]];
                const char* uvs = reinterpret_cast<const char*>(node->mUVs.get()) + ((u << 3) & 0xffffffe0u) + ((u << 2) & 0xc);
                uv[k][0] = *reinterpret_cast<const float*>(uvs);
                uv[k][1] = *reinterpret_cast<const float*>(uvs + 0x10);
            }
            switch (pos) {
                case 0:
                    uv[0][0] = uv[1][0] + (uv[3][0] - uv[2][0]);
                    uv[0][1] = uv[1][1] + (uv[3][1] - uv[2][1]);
                    break;
                case 1:
                    uv[1][0] = uv[0][0] + (uv[2][0] - uv[3][0]);
                    uv[1][1] = uv[0][1] + (uv[2][1] - uv[3][1]);
                    break;
                case 2:
                    uv[2][0] = uv[1][0] + (uv[3][0] - uv[0][0]);
                    uv[2][1] = uv[1][1] + (uv[3][1] - uv[0][1]);
                    break;
                case 3:
                    uv[3][0] = uv[0][0] + (uv[2][0] - uv[1][0]);
                    uv[3][1] = uv[0][1] + (uv[2][1] - uv[1][1]);
                    break;
                default:
                    break;
            }
            float minU = uv[0][0];
            float minV = uv[0][1];
            for (uint32_t k = 1; k < 4; ++k) {
                if (!(uv[k][0] >= minU)) minU = uv[k][0];
                if (!(uv[k][1] >= minV)) minV = uv[k][1];
            }
            uint32_t corner = 0;
            float best = 3.4028234663852886e+38f;
            for (uint32_t k = 0; k < 4; ++k) {
                float du = uv[k][0] - minU;
                float dv = uv[k][1] - minV;
                float dist = (du * du) + (dv * dv);
                if (!(dist >= best)) {
                    corner = k;
                    best = dist;
                }
            }
            uint32_t flags = 0x80;
            if (corner == 2)
                flags |= 0x10;
            else if (corner == 1)
                flags |= 8;
            else if (corner == 3)
                flags |= 0x20;
            uint32_t next = (corner + 1) & 3;
            float du = static_cast<float>(std::fabs(static_cast<double>(uv[corner][0]) - static_cast<double>(uv[next][0])));
            float dv = static_cast<float>(std::fabs(static_cast<double>(uv[corner][1]) - static_cast<double>(uv[next][1])));
            if (!(dv >= du)) {
                flags |= 0x40;
                ++flipped;
            } else {
                ++notFlipped;
            }
            out[0] = static_cast<uint16_t>(flags);
            int32_t number = static_cast<int32_t>(fs->mTexture.mOffset - header->mTextures.mOffset) / 8;
            out[1] = static_cast<uint16_t>(number);
        }
    }

    CTRACE(5, kSkirtCountFormat, flipped, notFlipped);
    mesh->mF00 = mesh->mF00 | 0x40;
}

// 0x1006f748 (1080 bytes): the skirt vertices of a ride's mesh (RideDestroy.cpp range).

namespace {

// Copies the deform's list into a block: a count word, the Mac offset of the data (block + 8), then the data.
void StoreNodeList(tag_sNode* node, int32_t count, const uint16_t* list) {
    uint32_t* block = static_cast<uint32_t*>(MReleaseAllocF(static_cast<uint32_t>(count) * 2 + 8, 0));
    node->mF98.mOffset = tpw::ArenaOffset(block);
    uint8_t* data = reinterpret_cast<uint8_t*>(block) + 8;
    block[1] = tpw::ArenaOffset(data);
    block[0] = static_cast<uint32_t>(count);
    if (data != nullptr && count != 0) std::memcpy(data, list, static_cast<size_t>(count) * 2);
}

}  // namespace

// The third argument is the ride's shape (null: no edges are decided), 4th and 5th its size. Unused local at 0x2d8 (a copy of
// 12 bytes of a global) is not kept.
// The list buffer is 256 entries (-0x288) with no bound: a longer list runs on into the matrix above it (-0x88, 64 bytes,
// dead once the first DoOrSetupSkirtDeform has its copy) and the unused copy. The jungle's incagod lists 275 entries.
void CalculateSkirtVertices(tag_sRideAnimSet* set, tag_sM3D2Header* header, void* a, int32_t w, int32_t h) {
    tag_sMesh* mesh = set->mHeads;
    tag_sM3D2Header* mh = mesh->mHeader;
    tag_sNode* bestNode = nullptr;
    int32_t bestCount = 0;
    int32_t count = 0;
    uint16_t buffer[256];
    uint16_t* list = nullptr;
    sMatrix matrix;

    if ((mh->mFlags & 0x80) == 0 && a != nullptr) {
        DecideRideShapeDeformEdges(static_cast<RideShape*>(a), w, h);
    } else {
        std::memset(gSkirtCellFlags, 0, 0x640);
        gSkirtColumns = 0;
        gSkirtRows = 0;
    }

    if (header != nullptr && header->mHField.mOffset != 0) {
        SetMatrixIdentity(&matrix);
        list = buffer;
        count = 0;
        bestNode = nullptr;
        bestCount = 0;
        uint32_t flags = (mh->mFlags & 0x100) ? 1 : 0;
        DoOrSetupSkirtDeform(mh->mF78.get(), mh->mF78.get(), &list, matrix, header->mHField.get(), 0, &count, 0.0f, &bestNode,
                             &bestCount, flags, nullptr, 0);
        if (bestNode != nullptr) bestNode->mFlags |= 0x4000;
        mesh->mRootNode = bestNode;
    }

    uint16_t countHalf = static_cast<uint16_t>(count);
    std::memcpy(mesh->mF0C, &countHalf, 2);
    if (count > 0) {
        mesh->mBlock14 = MReleaseAllocF(static_cast<uint32_t>(count) * 2, 1);
        if (mesh->mBlock14 != nullptr && count != 0) std::memcpy(mesh->mBlock14, buffer, static_cast<size_t>(count) * 2);
    }

    tag_sNode* node = mh->mNodes.get();
    for (uint32_t n = mh->mNodeCount; n != 0; --n, ++node) {
        if ((node->mFlags & 0x40) != 0 && node != mesh->mRootNode) {
            count = 0;
            list = buffer;
            SetMatrixIdentity(&matrix);
            DoOrSetupSkirtDeform(mh->mF78.get(), mh->mF78.get(), &list, matrix, header->mHField.get(), 0, &count, 0.0f, &bestNode,
                                 &bestCount, 0, node, 1);
            if (count > 0) StoreNodeList(node, count, buffer);
        }
    }

    if (a != nullptr) {
        uint32_t flags = mesh->mHeader->mFlags;
        if ((flags & 8) == 0) {
            ERRORBox("Ride %s has not been exported with 'has procedural base' selected\nFull reexport recommended\n",
                     mesh->mHeader->mName);
        } else if (mesh->mRootNode != nullptr || (flags & 0x80) != 0) {
            CalcSkirtTextureDirnFlags(set, static_cast<RideShape*>(a), w, h, static_cast<int32_t>(flags & 0x80));
        }
    }
}

// Ride and mesh teardown (main app, unit w105): the destroyers the lobby's leaving reaches. DestroyRideType 0x1006a768 (608 bytes),
// DestroyRideAnimSet 0x1006a5e8 (324), DestroyRideAnimSetInstance 0x10063338 (368), DestroyMesh 0x10064de0 (308), DestroyMeshHeader
// 0x10064d7c (40), DestroyRideHeightMap 0x1005e9f4 (60), DestroySkirtVertices 0x1006fbd8 (68, Debug.cpp on the Mac), DMFreeMesh
// 0x10030ae8 (240) and the two DMDisableMeshRender 0x100309c8 (68) and 0x10030a48 (84) (TPAdvisor.cpp on the Mac).
// (DestroyMeshInstance 0x100631c4 is in MeshDestroy.cpp.)

// 0x1006fbd8.
void DestroySkirtVertices(tag_sRideAnimSet* set) {
    MReleaseFreeF(set->mHeads->mSkirtVertices);
    set->mHeads->mSkirtVertices = nullptr;
}
