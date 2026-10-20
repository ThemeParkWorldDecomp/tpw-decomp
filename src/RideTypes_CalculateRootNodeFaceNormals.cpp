// Ride mesh helpers of the TPRide file: face normals of the root node, the blended base textures,
// the node's world vertex positions (0x100694c8, 0x10069770, 0x10068dec).
#include <cmath>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"
#include "tb_mesh.h"

// 0x10237cd8: a glue import taking a float in f1 and answering an int in r3 (the float to int step).
int32_t FloatToIntGlue(float value);

namespace {

uint16_t Half(const uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

}  // namespace

// Works out the normal of every face of the root node's face sets, turned by `rotation` (0, 90,
// 180, 270 degrees; any other value leaves it as is). A face whose normal's y is below 0.999
// (or is NaN) gets its flag bit 0 set and its normal appended to the scratch geometry buffer;
// `result` counts them.
void CalculateRootNodeFaceNormals(tag_sNode* node, int32_t rotation, int32_t& result) {
    result = 0;
    const uint8_t* set = reinterpret_cast<const uint8_t*>(node->mFaceSets.get());    float* out = static_cast<float*>(gScratchGeomBuffer);
    for (uint32_t sets = node->mFaceSetCount; sets != 0; --sets, set += 0x10) {
        uint8_t* face = reinterpret_cast<uint8_t*>(node->mFaces.get()) + static_cast<uint32_t>(Half(set + 4)) * 8;
        const uint16_t* map = node->mF94.get();
        const uint8_t* verts = reinterpret_cast<const uint8_t*>(node->mVertices.get());
        for (uint32_t n = Half(set + 6); n != 0; --n, face += 8) {
            uint32_t ia = map[Half(face + 2)];
            uint32_t ib = map[Half(face + 4)];
            uint32_t ic = map[Half(face + 6)];
            const float* pa = reinterpret_cast<const float*>(verts + (ia >> 2) * 0x30 + (ia & 3) * 4);
            const float* pb = reinterpret_cast<const float*>(verts + (ib >> 2) * 0x30 + (ib & 3) * 4);
            const float* pc = reinterpret_cast<const float*>(verts + (ic >> 2) * 0x30 + (ic & 3) * 4);
            float x0 = pa[0], y0 = pa[4], z0 = pa[8];
            float f3 = x0 - pb[0];
            float f5 = z0 - pb[8];
            float f4 = y0 - pb[4];
            float f6 = x0 - pc[0];
            float f7 = y0 - pc[4];
            float f8 = z0 - pc[8];
            float t0 = f4 * f6;
            float t2 = f5 * f7;
            float t1 = f3 * f8;
            float nz = ppc_fmsubs(f3, f7, t0);
            float nx = ppc_fmsubs(f4, f8, t2);
            float ny = ppc_fmsubs(f5, f6, t1);
            float sy = ny * ny;
            float sz = nz * nz;
            double sum = ppc_fmadd(static_cast<double>(nx), static_cast<double>(nx), static_cast<double>(sy));
            double total = static_cast<double>(sz) + sum;
            float len = static_cast<float>(std::sqrt(total));
            nx = nx / len;
            ny = ny / len;
            nz = nz / len;
            if (rotation == 0xb4) {
                nx = nx * -1.0f;
                nz = nz * -1.0f;
            } else if (rotation == 0x5a) {
                float old = nx;
                nx = nz;
                nz = -old;
            } else if (rotation == 0x10e) {
                float old = nx;
                nx = -nz;
                nz = old;
            }
            if (ny < 0.999f) {   // fcmpo then bge: a NaN skips the face
                // Bit 0 of the face's 16-bit index-and-flag word. The Mac's stb to byte 1 is the low byte of
                // its big-endian word; on the host the word is native, so the flag goes on the word itself.
                uint16_t word;
                std::memcpy(&word, face, 2);
                word = static_cast<uint16_t>(word | 1);
                std::memcpy(face, &word, 2);
                out[0] = nx;
                out[1] = ny;
                out[2] = nz;
                out += 3;
                ++result;
            }
        }
    }
}

// Copies the blend flags of the ride mesh's skirt records onto the height field's cells under the
// ride, turned by the mesh's rotation (mF10), then asks the engine mesh for its base.
// With a rotation other than 0, 90, 180, 270 the original uses stale registers; not reproduced.
void GetBlendedBaseTextures(tag_sRideAnimSet* set) {
    tag_sMesh* mesh = set->mHeads;
    sHField* field = gHeightfieldHeader->mHField.get();
    tag_sNode* root = mesh->mHeader->mF78.get();
    const uint8_t* map = static_cast<const uint8_t*>(set->mSelf->mHeightMap);
    int32_t rotation = mesh->mF10;
    const uint8_t* src = static_cast<const uint8_t*>(mesh->mSkirtVertices);

    int32_t baseX = FloatToIntGlue(root->mMatrix.m[3][0] / field->mF08[2]);
    int32_t baseZ = FloatToIntGlue(root->mMatrix.m[3][2] / field->mF08[3]);
    if (rotation == 0xb4) {
        baseX -= 1;
        baseZ -= 1;
    } else if (rotation == 0x5a) {
        baseZ -= 1;
    } else if (rotation == 0x10e) {
        baseX -= 1;
    }
    int32_t stride = static_cast<int32_t>(field->mF18[0]);
    uint8_t* cells = reinterpret_cast<uint8_t*>(field->mQuads.get());
    int32_t x = 0, z = 0;
    for (uint32_t i = 0; i < Half(map + 0xa); ++i) {
        for (uint32_t j = 0; j < Half(map + 8); ++j) {
            int32_t ii = static_cast<int32_t>(i), jj = static_cast<int32_t>(j);
            if (rotation == 0) {
                x = baseX + jj;
                z = baseZ + ii;
            } else if (rotation == 0x5a) {
                x = baseX + ii;
                z = baseZ - jj;
            } else if (rotation == 0xb4) {
                x = baseX - jj;
                z = baseZ - ii;
            } else if (rotation == 0x10e) {
                x = baseX - ii;
                z = baseZ + jj;
            }
            uint32_t flags = Half(src);
            if ((flags & 0x80) != 0) {
                uint32_t value = flags & 0x40;
                uint8_t* cell = cells + static_cast<int32_t>((x + z * stride) * 4);
                uint32_t dir = flags & 0x38;
                if (rotation == 0) {
                    value |= dir;
                } else if (rotation == 0x5a) {
                    if (dir == 0) value |= 8;
                    else if (dir == 8) value |= 0x10;
                    else if (dir == 0x10) value |= 0x20;
                } else if (rotation == 0xb4) {
                    if (dir == 0) value |= 0x10;
                    else if (dir == 8) value |= 0x20;
                    else if (dir == 0x20) value |= 8;
                } else if (rotation == 0x10e) {
                    if (dir == 0) value |= 0x20;
                    else if (dir == 0x10) value |= 8;
                    else if (dir == 0x20) value |= 0x10;
                }
                uint16_t old = Half(cell);
                uint16_t merged = static_cast<uint16_t>(value | 0x281 | (old & 6));
                std::memcpy(cell, &merged, 2);
                std::memcpy(cell + 2, src + 2, 2);
            }
            src += 4;
        }
    }
    if (mesh->mEngineMesh->GenerateRideBase(set->mFC0.mF02, set->mFC0.mF00, set->mFC4.mF02, set->mFC4.mF00) == 0)
        ERRORBox("Could not blend base for ride %s, engine function returned 0\n", mesh->mHeader->mName);
}

// Puts the node's vertex positions, moved by the node's matrix combined with those of its
// descendants (the chain of mF04 links), into the scratch geometry buffer: x, y, z per vertex.
void GetNodeWorldVertPosnsIntoGeomBuffer(tag_sNode* node) {
    sMatrix m;
    std::memcpy(&m, &node->mMatrix, sizeof m);
    for (tag_sNode* child = node->mF04.get(); child != nullptr; child = child->mF04.get())
        CombineMatricesNoWDest1st(&m, &child->mMatrix);
    float* out = static_cast<float*>(gScratchGeomBuffer);
    const uint8_t* verts = reinterpret_cast<const uint8_t*>(node->mVertices.get());
    for (uint32_t i = 0; i < node->mVertexCount; ++i) {
        const float* p = reinterpret_cast<const float*>(verts + (i >> 2) * 0x30 + (i & 3) * 4);
        float x = p[0], y = p[4], z = p[8];
        out[0] = x;
        out[1] = y;
        out[2] = z;
        float ax = ppc_fmadds(x, m.m[0][0], y * m.m[1][0]);
        ax = ppc_fmadds(z, m.m[2][0], ax);
        float ay = ppc_fmadds(x, m.m[0][1], y * m.m[1][1]);
        ay = ppc_fmadds(z, m.m[2][1], ay);
        float az = ppc_fmadds(x, m.m[0][2], y * m.m[1][2]);
        az = ppc_fmadds(z, m.m[2][2], az);
        out[0] = m.m[3][0] + ax;
        out[1] = m.m[3][1] + ay;
        out[2] = m.m[3][2] + az;
        out += 3;
    }
}
