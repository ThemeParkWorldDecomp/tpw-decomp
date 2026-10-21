#include <cmath>
#include <cstdio>
#include "LevelInit.h"
#include "LevelSystems.h"
#include "LLCellSystem.h"
#include "Light.h"
#include "Main.h"
#include "MapCell.h"
#include "MapWho.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "World.h"
#include "m3d2.h"
#include "rsse.h"
#include "tb_resource_system.h"
#include "platform/samsutils.h"

int32_t gParkDarkeningFlag;   // 0x102c1e04 (name invented): cleared at the end of RecalculateParkDarkening

// 0x1005c2b0 (676 bytes): the matrix (by value, like the Mac) is combined with the node's own; unless the node has flag 0x200 each
// of its normals is scaled to length 1 (0.225 where the height field cell under its vertex has quad bit 0x400); then the children.
void UpdateHFieldDarkeningNormals(tag_sNode* node, sMatrix matrix) {
    CombineMatricesNoWDest2nd(&node->mMatrix, &matrix);
    if ((node->mFlags & 0x200) == 0) {
        const uint16_t* map = node->mF94.get();
        const sHField* field = gHeightfieldHeader->mHField.get();
        const uint32_t width = field->mF18[0];
        const uint32_t height = field->mF18[1];
        GetNodeWorldVertPosnsIntoGeomBuffer(node, &matrix);
        const uint32_t count = node->mF5E;
        float* normal = reinterpret_cast<float*>(node->mPoints.get());
        for (uint32_t i = 0; i < count; ++i) {
            const float ten = 10.0f;
            const float* vert = reinterpret_cast<const float*>(static_cast<const uint8_t*>(gScratchGeomBuffer) + map[i] * 12u);
            const float fx = vert[0] / ten;
            const uint32_t cellX = static_cast<uint32_t>(FloatToCell(static_cast<double>(fx)));
            const uint32_t cellZ = static_cast<uint32_t>(FloatToCell(static_cast<double>(vert[2] / ten)));
            const uint32_t cellX2 = static_cast<uint32_t>(FloatToCell(static_cast<double>(fx)));
            if (cellX2 < width && cellZ < height) {
                const uint16_t quad = gHeightfieldQuads[cellX + cellZ * width].mF00;
                const float target = (quad & 0x400) != 0 ? 0.225f : 1.0f;   // 0x1029cf0c: 0x3e666666
                const float yy = normal[1] * normal[1];
                const float zz = normal[2] * normal[2];
                const double sum = static_cast<double>(normal[0]) * static_cast<double>(normal[0]) + static_cast<double>(yy);
                const float len = static_cast<float>(std::sqrt(static_cast<double>(zz) + sum));
                const float scale = target / len;
                normal[0] = normal[0] * scale;
                normal[1] = normal[1] * scale;
                normal[2] = normal[2] * scale;
            }
            normal += 3;
        }
    }
    for (tag_sNode* child = node->mF0C.get(); child != nullptr; child = child->mF08.get())
        UpdateHFieldDarkeningNormals(child, matrix);
}

// 0x1005c59c (540 bytes): every cell of the height field gets quad bit 0x400 when the map cell has flag 0x40 and not 0x80, then
// the normals are redone from the root node with an identity matrix and the lights are flagged.
void RecalculateParkDarkening() {
    uint16_t* quad = reinterpret_cast<uint16_t*>(gHeightfieldHeader->mHField->mQuads.get());
    for (uint32_t row = 0; row < gHeightfieldHeader->mHField->mF18[1]; ++row) {
        for (uint32_t col = 0; col < gHeightfieldHeader->mHField->mF18[0]; ++col) {
            int32_t x = static_cast<int32_t>(col);
            int32_t y = static_cast<int32_t>(row);
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (x >= gHeightfieldWidth) x = gHeightfieldWidth - 1;
            if (y >= gHeightfieldHeight) y = gHeightfieldHeight - 1;
            const uint16_t index = static_cast<uint16_t>(x + (y * 128 + 1));
            CMapCell* cell = reinterpret_cast<CMapCell*>(reinterpret_cast<uint8_t*>(gGameWorldRef) + 0x2d8 +
                                                         (static_cast<int32_t>(index) - 1) * 0x44);
            if (cell->IsFlag(0x40) != 0 && cell->IsFlag(0x80) == 0)
                *quad |= 0x400;
            else
                *quad &= 0xfbff;
            quad += 2;
        }
    }
    sMatrix identity;
    SetMatrixIdentity(&identity);
    UpdateHFieldDarkeningNormals(gHeightfieldHeader->mF78.get(), identity);
    gei_System.UpdateLights();
    gParkDarkeningFlag = 0;
}

// 0x1005c988 (584 bytes): writes into gPolySearchTable the vertices of the root node (pass 0) and then, unless flag is nonzero, of
// every node whose embedded entry has flag 2 (pass 1), moved by the node's position and the root's matrix row 3 and turned by the
// mesh's rotation (0x5a, 0xb4, 0x10e). A mini node is read as a full node (the original does).
void SetupMeshForPolySearchSimple(tag_sMesh* mesh, int32_t flag) {
    tag_sM3D2Header* header = mesh->mHeader;
    const int32_t passes = flag != 0 ? 1 : 2;
    const tag_sNode* origin = header->mF78.get();
    const float originX = origin->mMatrix.m[3][0];
    const float originY = origin->mMatrix.m[3][1];
    const float originZ = origin->mMatrix.m[3][2];
    int32_t total = 0;
    tag_sNode* node = nullptr;
    sEmbeddedData* embedded = nullptr;
    sEmbeddedRecord* record = nullptr;
    for (int32_t pass = 0; pass < passes; ++pass) {
        uint32_t count;
        if (pass == 0) {
            node = mesh->mRootNode;
            count = 1;
        } else {
            count = header->mEmbeddedCount;
            if (count != 0) {
                embedded = header->mEmbedded.get();
                record = static_cast<sEmbeddedSet*>(mesh->mF28)->mRecords;
            }
        }
        for (uint32_t i = 0; i < count; ++i) {
            if (pass == 1) {
                const uint32_t n = static_cast<uint32_t>(header->mF46) + i;
                if (n < header->mNodeCount)
                    node = reinterpret_cast<tag_sNode*>(reinterpret_cast<uint8_t*>(header->mNodes.get()) + n * 0xa0u);
                else
                    node = reinterpret_cast<tag_sNode*>(reinterpret_cast<uint8_t*>(header->mMiniNodes.get()) +
                                                        (n - header->mNodeCount) * 0x58u);
            }
            if ((node != nullptr && pass == 0) || (pass == 1 && (embedded->mFlags & 2) != 0)) {
                s3DPoint pos;
                GetNodePositionSimple(node, &pos, mesh->mF10);
                float* out = static_cast<float*>(gPolySearchTable) + total * 3;
                if (pass == 1) *reinterpret_cast<uint16_t*>(&record->mF10) = static_cast<uint16_t>(total);
                const uint8_t* verts = reinterpret_cast<const uint8_t*>(node->mVertices.get());
                for (uint32_t j = 0; j < node->mVertexCount; ++j) {
                    const float* base = reinterpret_cast<const float*>(verts + (j >> 2) * 0x30u);
                    const uint32_t lane = j & 3;
                    const float px = base[lane] + pos.x;
                    const float py = base[4 + lane] + pos.y;
                    const float pz = base[8 + lane] + pos.z;
                    out[1] = originY + py;
                    switch (mesh->mF10) {
                        case 0xb4:
                            out[0] = originX - px;
                            out[2] = originZ - pz;
                            break;
                        case 0x5a:
                            out[0] = originX + pz;
                            out[2] = originZ - px;
                            break;
                        case 0x10e:
                            out[0] = originX - pz;
                            out[2] = originZ + px;
                            break;
                        default:
                            out[0] = originX + px;
                            out[2] = originZ + pz;
                            break;
                    }
                    out += 3;
                }
                total += node->mVertexCount;
            }
            if (pass == 1) {
                ++record;
                ++embedded;
            }
        }
    }
}
