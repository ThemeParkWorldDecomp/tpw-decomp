// CalcSkirtTextureDirnFlags 0x1006eca0 (2648 bytes): for every face of a ride mesh's root node, finds the skirt cell it covers
// (through the height field's cell size) and keeps in the cell the face whose edges run most along x and z; then
// writes for each cell of the ride's footprint a texture direction flag word and the texture number into mesh->mSkirtVertices.
#include <cmath>
#include <cstdint>

#include "CBalanceTable.h"
#include "Debug.h"
#include "TPEngine.h"
#include "m3d2.h"

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
