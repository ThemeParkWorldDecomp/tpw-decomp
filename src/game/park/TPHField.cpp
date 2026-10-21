// TPHField.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#define TPW_GLOBE_INIT_REAL 1
#include "ActionRecorder.h"
#include "analyser.h"
#include "Bitmap.h"
#include "CBalanceTable.h"
#include "CellBase.h"
#include "Clock.h"
#include "Debug.h"
#include "email.h"
#include "ExitErr.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_System.h"
#include "Game.h"
#include "GameSys.h"
#include "helpers/HLSpriteHelpers.h"
#include "helpers/M3D2LoadHelpers.h"
#include "HFieldTextures.h"
#include "intrface.h"
#include "keytable_actions.h"
#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "m3d2.h"
#include "Main.h"
#include "MainWindowCallback.h"
#include "MapCell.h"
#include "MapWho.h"
#include "Mouse.h"
#include "ObjectParts.h"
#include "online.h"
#include "OptionsBalance.h"
#include "platform/arena.h"
#include "platform/indirectx.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "Quickload.h"
#include "RideDB.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "std_exception_what.h"
#include "tb_ddraw.h"
#include "tb_hfield.h"
#include "tb_init.h"
#include "tb_loaders.h"
#include "tb_matrix.h"
#include "tb_mesh.h"
#include "tb_rain.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_sky.h"
#include "tb_texturecache.h"
#include "TbDynamicStringTemplate.h"
#include "ThingReceivers.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPDialog.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPMovie.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "TrackCell.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "Wavelet.h"
#include "World.h"
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

// The engine start-up glue of the main app (0x10059c70..0x10068d90 in part; the inventory
// files the range under TPRide.cpp, and the original file name is unknown): the options
// table and the options dialog turned into engine flags, the window, the OpenGL context and
// the engine's start, the engine's Mac* hooks, the per-frame engine calls, the memory hooks,
// the procedural texturing switches and the scratch geometry buffer.
//
// The rest of the range (heightfield, ride height maps, hoardings, instancing, meshes,
// quickload) is not translated yet.

// 0x1005b734: no heap is made; any non-NULL value does.
int32_t HNewHeap(uint32_t a, uint32_t b, uint32_t c, sHeapInfo** heap) {
    *heap = reinterpret_cast<sHeapInfo*>(1);
    return 1;
}

// 0x1005b774
int32_t HFreeHeap(sHeapInfo* heap) { return 1; }

// The map reset (main app of SimTheme Park 2.04b3; the inventory files LLCC_ResetMap under LLCellSystem.cpp, HFieldCellStatus under
// TPRide.cpp, RemoveNeighbour and ModifyDirection under LLCellMouse.cpp and the cell resets under unrelated names; unit w112c):
// LLCC_ResetMap puts every cell of the map and of the track layer back to its start state (the cell's place comes from its id, the
// start state from the height field and, for the map cells, the attribute map).

// 0x1005b7a8 (112 bytes). The Mac's arithmetic is 32 bit: the quad's address is the table plus ((x + y * width) << 2) wrapped in
// 32 bits, and only the first half word of the quad is read.
int32_t HFieldCellStatus(int32_t x, int32_t y) {
    tag_sM3D2Header* const header = gHeightfieldHeader;
    if (header == nullptr) return 1;
    const sHField* const field = header->mHField;
    if (field == nullptr) return 1;
    const uint32_t width = field->mF18[0];
    if (static_cast<uint32_t>(x) >= width) return 0;
    if (static_cast<uint32_t>(y) >= field->mF18[1]) return 0;
    const uint32_t offset = (static_cast<uint32_t>(x) + static_cast<uint32_t>(y) * width) << 2;
    const sHFieldQuad* const quad =
        reinterpret_cast<const sHFieldQuad*>(reinterpret_cast<const uint8_t*>(field->mQuads.get()) + offset);
    return (quad->mF00 & 1) != 0 ? 0 : 1;
}

// Unit v5967: AbortMovePylon 0x10057df4, SingleHFieldTextureChange 0x1005b840, PlacePylon (4 arguments) 0x10052a30,
// IsCoasterUseable 0x10052954, IsTrackValid 0x100528d8, IsTrackClosed (void*) 0x10052850, DecCoasterConst2 0x10052628.

// 0x1005b840
void SingleHFieldTextureChange(int32_t x, int32_t y, const CCellBase::CELL_IndexType* tile) {
    tag_sM3D2Header* header = gHeightfieldHeader;
    if (header != nullptr && (header->mFlags & 2)) {
        SetHeightfieldTexture(header, &gExtraTextureTable, x, y, static_cast<int32_t>(tile->mKind), static_cast<int32_t>(tile->mIndex),
                              static_cast<int32_t>(tile->mRotation));
        TPForce3DRefresh(1);
    }
}

// TPRide.cpp (main app 0x1005b8fc, unit w114): the level's procedural texture set.

// Loads the procedural textures from "<mesh folder>base.lnd" (just "base.lnd" when GetMeshPath answers NULL) with the engine's PT_Load
// (its answer is dropped), then, reading gGameOptions after PT_Load, makes the quads' orientation flags match the textures being on
// (bit 0x4000: CorrectOrientationFlagsForDiffProceduralMode(1)) or turns the procedural textures off (PT_Disable). The Mac's sprintf
// into the 1024 byte buffer is unbounded; this one is cut at the buffer.
void LoadProceduralTextureSet(int32_t mode) {
    char path[1024];
    const char* folder = GetMeshPath();
    if (folder != nullptr) std::snprintf(path, sizeof path, "%sbase.lnd", folder);
    else std::snprintf(path, sizeof path, "base.lnd");
    PT_Load(path, mode);
    if ((gGameOptions & 0x4000) != 0) CorrectOrientationFlagsForDiffProceduralMode(1);
    else PT_Disable();
}

// The level height field's texture table and flatness (main app of SimTheme Park 2.04b3).
// See include/HFieldTextures.h for what the table is and where each function lies in the
// inventory.
//
// Quirks of the original that are kept:
//  - The table's byte count (mBytes) is only ever added to: a second ParseLevelTCT asks for
//    the first one's bytes as well.
//  - InterceptAndInsertM3D2Textures reuses the flag that its search sets when it finds a
//    texture of the mesh in the table, in the loop that makes the mesh's new records: if the
//    LAST texture of the mesh was found, only the first category gets records, and the
//    places that the map gives to the other categories' textures point past the new arrays.
//  - GetString (TPAdvisor.cpp) may read one byte past the end of the text.
//  - FreeHFieldTextureTable leaves the table's other pointers (the categories' records, the
//    new textures) as they were: they point into the blocks it frees.

namespace {

// The cross product of the two edges that leave one corner of a triangle of the field: the
// edges (ten, dy1, 0) and (0, dy2, ten), with the cell's size ten (+10 or -10) and the steps
// in height dy1 and dy2, each divided by its length (a double sum, one square root, rounded
// to single). The result is not a unit vector: its length is the sine of the angle between
// the edges. The coordinates come from double products and fused multiply-subtracts, each
// rounded to single, in the order the original computes them.
struct sNormal {
    float x, y, z;
};

sNormal TriangleNormal(float ten, float dy1, float dy2) {
    const float zero = 0.0f;
    // (float)(zero * zero) is 0 and the sums are added in double.
    const double sum1 = static_cast<float>(zero * zero) + ppc_fmadd(ten, ten, static_cast<float>(dy1 * dy1));
    const float length1 = static_cast<float>(std::sqrt(sum1));
    const float edge1Z = zero / length1;
    const float edge1Y = dy1 / length1;
    const float edge1X = ten / length1;
    const double sum2 = static_cast<float>(ten * ten) + ppc_fmadd(zero, zero, static_cast<float>(dy2 * dy2));
    const float length2 = static_cast<float>(std::sqrt(sum2));
    const float edge2Y = dy2 / length2;
    const float edge2X = zero / length2;
    const float edge2Z = ten / length2;
    sNormal normal;
    normal.x = static_cast<float>(ppc_fmsub(edge2Y, edge1Z, static_cast<double>(edge2Z) * edge1Y));
    normal.y = static_cast<float>(ppc_fmsub(edge2Z, edge1X, static_cast<double>(edge2X) * edge1Z));
    normal.z = static_cast<float>(ppc_fmsub(edge2X, edge1Y, static_cast<double>(edge2Y) * edge1X));
    return normal;
}

}  // namespace

void DetermineHFieldFlatness(tag_sM3D2Header* header) {
    sHField* field = header->mHField;
    for (uint32_t y = 0; y < field->mF18[1]; ++y) {
        for (uint32_t x = 0; x < field->mF18[0]; ++x) {
            const float* corner = field->mHeights.get() + (x + y * (field->mF18[0] + 1));
            const float* below = corner + field->mF18[0] + 1;
            const float cell = 10.0f;
            // The triangle at the cell's first corner, then the one at the opposite corner.
            const sNormal first = TriangleNormal(cell, corner[1] - corner[0], below[0] - corner[0]);
            const sNormal second = TriangleNormal(-cell, below[0] - below[1], corner[1] - below[1]);
            float dot = first.y * second.y;
            dot = ppc_fmadds(first.x, second.x, dot);
            dot = ppc_fmadds(first.z, second.z, dot);
            // The two normals of a cell that steps along one axis only are parallel and of length 1,
            // so their dot product is 1; a cell that slopes along both axes has shorter normals.
            // Those cells are marked (0x800 in the quad's first word).
            if (dot < 0.9999f) field->mQuads[x + y * field->mF18[0]].mF00 |= 0x800;
        }
    }
}

tag_sRideAnimSet* gHeightfieldRideSet;
tag_sM3D2Header* gHeightfieldHeader;
sHFieldQuad* gHeightfieldQuads;
sHField* gHeightfieldCopy;

int32_t LoadLevelHeightfield(int32_t useLevel, int32_t mode) {
    char folder[1024], shared[1024];
    std::sprintf(folder, "%s:Terrain", useLevel ? gTPFileInfo.GetLevelPath().mData : get_theme_path());
    BuildSharedTexturePath(shared);
    SetupDirectoryForMeshLoad(const_cast<char*>("base.md2"), folder, shared);
    LoadRideAnimSet(const_cast<char*>("Base"), 8, &gHeightfieldRideSet, nullptr, 1);
    if (gHeightfieldRideSet == nullptr) LoadRideAnimSet(const_cast<char*>("TestBase"), 8, &gHeightfieldRideSet, nullptr, 1);
    if (gHeightfieldRideSet == nullptr) { gHeightfieldHeader = nullptr; gHeightfieldQuads = nullptr; return 0; }
    gHeightfieldHeader = gHeightfieldRideSet->mHeads->mHeader;
    gHeightfieldQuads = gHeightfieldHeader->mHField->mQuads;
    if (gHeightfieldHeader != nullptr) {
        BuildHFieldTextureTable(gHeightfieldHeader);
        SetupHFieldNodeGamuts(gHeightfieldHeader);
        DetermineHFieldFlatness(gHeightfieldHeader);
        SaveHFieldCopy(gHeightfieldHeader, &gHeightfieldCopy);
        if ((gGameOptions & 0x2000) != 0 && useLevel == 0) LoadProceduralTextureSet(mode);
        CreateMeshTypeEngineMesh(gHeightfieldRideSet->mHeads);
        CreateEngineMesh(gHeightfieldRideSet->mHeads);
        if ((gHeightfieldRideSet->mFlags & 0x4000) != 0)
            EnableMeshDraw(gHeightfieldRideSet->mHeads, gHeightfieldRideSet->mFC0, gHeightfieldRideSet->mFC4);
        else EnableMeshDraw(gHeightfieldRideSet->mHeads);
    }
    return 1;
}

// The park's second-stage systems that LevelInit2 and LevelDeInit call (main application, unit w111): the attribute map, the poly
// search table, the heightmap system's words, the level lights' teardown, the highlight colour buffers, the coaster types and the
// data error handler. Each function keeps the original's order of calls and stores.

// Globals (names invented; the Mac addresses are in the headers).
void* gPolySearchTable;
float gHeightmapScaleA;
float gHeightmapScaleB;
// 0x1005be44 (124 bytes). The Mac formats into a 1,032 byte buffer with sprintf; the host bounds the write. The world's map is at
// +0x2d8 (plain arithmetic: the Mac does not test the world).
bool LoadLevelAttributeMap() {
    char buffer[1032];
    char* themePath = get_theme_path();
    std::snprintf(buffer, sizeof buffer, "%s:Terrain:Base.map", themePath);
    const char* path = FindFilePath(buffer, 1, nullptr);
    return gGameWorld->GetMap()->LoadAttribMap(path) != 0;
}

void SaveHFieldCopy(tag_sM3D2Header* header, sHField** copy) {
    if (header == nullptr || header->mHField == nullptr) { *copy = nullptr; return; }
    sHField* source = header->mHField;
    const uint32_t heightBytes = source->mHeightCount * sizeof(float);
    const uint32_t quadBytes = source->mQuadCount * sizeof(sHFieldQuad);
    auto* field = static_cast<sHField*>(MReleaseAllocF(sizeof(sHField) + heightBytes + quadBytes, 1));
    if (field != nullptr) std::memcpy(field, source, sizeof(sHField));
    field->mHeights = reinterpret_cast<float*>(field + 1);
    if (source->mHeights != nullptr && source->mHeightCount != 0) std::memcpy(field->mHeights.get(), source->mHeights.get(), heightBytes);
    field->mQuads = reinterpret_cast<sHFieldQuad*>(reinterpret_cast<uint8_t*>(field + 1) + heightBytes);
    if (source->mQuads != nullptr && source->mQuadCount != 0) std::memcpy(field->mQuads.get(), source->mQuads.get(), quadBytes);
    *copy = field;
}

// 0x1005c050: puts the saved quad table back. The byte count is mQuadCount * 4 of the header's own field.
void RestoreHFieldDataFromCopy(tag_sM3D2Header* header, sHField* copy) {
    if (header == nullptr) return;
    sHField* field = header->mHField.get();
    if (field == nullptr) return;
    void* dest = field->mQuads.get();
    if (dest == nullptr) return;
    // The original reads copy->mQuads without a NULL check on copy; so does this.
    void* src = copy->mQuads.get();
    if (src == nullptr) return;
    uint32_t count = field->mQuadCount;
    if (count == 0) return;
    std::memcpy(dest, src, count << 2);
}

void DestroyLevelHeightfield() {
    if (gHeightfieldCopy != nullptr) { MReleaseFreeF(gHeightfieldCopy); gHeightfieldCopy = nullptr; }
    if (gHeightfieldRideSet != nullptr) { DestroyRideAnimSet(gHeightfieldRideSet, 0); gHeightfieldRideSet = nullptr; }
    FreeHFieldTextureTable();
}

// Unit v5258: GetNodePositionSimple, SetRideSpritePosnFocus, GetMeshPtrAtHFieldCell (TPRide.cpp).

void SetRideSpritePosnFocus(tag_sMesh* mesh);   // include/rsse.h
extern Ptr32<uint8_t> gRideCtxRef;              // 0x102c1e10 (src/CalculateMaxVertexHeights_GetRideHeightAtPoint.cpp)
extern uint32_t gRideFlags;                     // 0x102c1e14

// 0x1005c1b0 (112 bytes).
tag_sRideAnimSet* GetMeshPtrAtHFieldCell(uint32_t x, uint32_t y) {
    sHField* field = gHeightfieldHeader->mHField;
    if (x >= field->mF18[0]) x = field->mF18[0] - 1;
    if (y >= field->mF18[1]) y = field->mF18[1] - 1;
    int32_t id = GetRideIDAtCell(static_cast<int32_t>(x), static_cast<int32_t>(y));
    if (id == 0) return nullptr;
    return gRideAnimSets[id];
}

// 0x1005c250
void GetHFieldMaxVertexIndices(uint32_t* width, uint32_t* height) {
    *width = gHeightfieldHeader->mHField->mF18[0];
    *height = gHeightfieldHeader->mHField->mF18[1];
}

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

// 0x1005c7e8 (60 bytes): 12 bytes for each of gPolySearchCount, zeroed (flag 1).
void SetupForPolySearch() { gPolySearchTable = MReleaseAllocF(gPolySearchCount * 0xc, 1); }

// 0x1005c850 (60 bytes).
void SetdownFromPolySearch() {
    MReleaseFreeF(gPolySearchTable);
    gPolySearchTable = nullptr;
}

// 0x1005c8b8 (140 bytes).
void GetNodePositionSimple(tag_sNode* node, s3DPoint* out, int32_t) {
    if (node->mF04) {
        out->x = node->mMatrix.m[3][0];
        out->y = node->mMatrix.m[3][1];
        out->z = node->mMatrix.m[3][2];
    } else {
        out->x = 0.0f;
        out->y = 0.0f;
        out->z = 0.0f;
    }
    for (tag_sNode* p = node->mF04; p != nullptr; p = p->mF04) {
        if (p->mF04) {
            out->x = out->x + p->mMatrix.m[3][0];
            out->y = out->y + p->mMatrix.m[3][1];
            out->z = out->z + p->mMatrix.m[3][2];
        }
    }
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

// 0x1005cc10 (156 bytes).
void SetRideSpritePosnFocus(tag_sMesh* mesh) {
    gRideCtxRef = reinterpret_cast<uint8_t*>(mesh);
    if (mesh->mF28 != nullptr && (*static_cast<uint32_t*>(mesh->mF28) & 8) != 0) {
        gRideFlags = 2;
        SetupMeshForPolySearchSimple(mesh, 0);
    } else if ((mesh->mHeader->mFlags & 0x80) != 0) {
        gRideFlags = 1;
    } else if (0.0f != mesh->mHeader->mFA0) {
        gRideFlags = 1;
    } else {
        gRideFlags = 4;
        SetupMeshForPolySearchSimple(mesh, 1);
    }
}

// Ride height lookups by polygon search: DoNodePolySearch 0x1005cce4 (708 B), GetHeightFromPolySearch 0x1005d000 (356 B),
// GetRideHeightAtPoint 0x1005d198 (316 B).
//
// The ride context and the nodes are read by Mac offset (32-bit layout, pointers as 32-bit words in 32-bit addressable memory).

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

// CalculateMaxVertexHeights 0x1005d304 (1984 bytes, TPRide.cpp): moves a node's vertices through the node's matrix, keeps them
// inside the ride's height map, widens the map's bounds, rasterises every face into the map keeping the highest value per
// cell, then does the same for the node's children.

int32_t gRideHeightStride;                 // 0x102c2144
float gRideHeightLimitZ;                   // 0x102c2148
float gRideHeightLimitX;                   // 0x102c214c
int32_t gRideHeightScan[50][4];            // 0x102c1e20

namespace {

// The error report shared by the clamps: remember the node, raise the flag.
inline void Report(tag_sNode* node, tag_sNode** badNode, int32_t* flag) {
    if (badNode != nullptr) *badNode = node;
    if (*flag == 0) *flag = 1;
}

// Clamps a coordinate into [0, limit): a value below -1 or above limit + 1 is an error (0 is the "ok" answer).
inline void ClampToLimit(float& value, const float& limit, tag_sNode* node, tag_sNode** badNode, int32_t* flag, int32_t& ok) {
    if (value < 0.0f) {
        if (value < -1.0f) {
            Report(node, badNode, flag);
            ok = 0;
        }
        value = 0.0f;
    } else if (value >= limit) {
        if (value > 1.0f + limit) {
            Report(node, badNode, flag);
            ok = 0;
        }
        value = limit - 0.001f;
    }
}

}  // namespace

int32_t CalculateMaxVertexHeights(tag_sNode* node, tag_sM3D2Header* header, sMatrix matrix, sRideHeightMap* map,
                                  tag_sNode** badNode, tag_sAnimHeader* anim, int32_t unused, int32_t* flag) {
    CombineMatricesNoWDest2nd(&node->mMatrix, &matrix);
    int32_t ok = 1;
    if ((node->mFlags & 0x200) == 0 && (node->mFlags & 0x8010) == 0) {
        uint8_t* vertices = reinterpret_cast<uint8_t*>(node->mVertices.get());
        for (uint32_t i = 0, count = node->mVertexCount; count != 0; count--, i++) {
            float* block = reinterpret_cast<float*>(vertices + (i >> 2) * 0x30 + (i & 3) * 4);   // x, then y at +0x10, z at +0x20
            s3DPoint point;
            point.x = block[0];
            point.y = block[4];
            point.z = block[8];
            VectorThroughLMatrixNoW(&matrix, &point);
            if (point.x < map->mMin[0]) map->mMin[0] = point.x;
            if (point.y < map->mMin[1]) map->mMin[1] = point.y;
            if (point.z < map->mMin[2]) map->mMin[2] = point.z;
            if (point.x > map->mMax[0]) map->mMax[0] = point.x;
            if (point.y > map->mMax[1]) map->mMax[1] = point.y;
            if (point.z > map->mMax[2]) map->mMax[2] = point.z;
            ClampToLimit(point.x, gRideHeightLimitX, node, badNode, flag, ok);
            ClampToLimit(point.z, gRideHeightLimitZ, node, badNode, flag, ok);
            if (point.y < 0.0f) point.y = 0.0f;
            if (point.y >= 100.0f) {
                if (*flag == 0) *flag = 1;
                point.y = 99.999f;
            }
            block[0] = point.x;
            block[4] = point.y;
            block[8] = point.z;
        }

        const sFaceSet* faceSet = node->mFaceSets.get();
        for (uint32_t s = node->mFaceSetCount; s != 0; s--, faceSet++) {
            const sFace* face = node->mFaces.get() + faceSet->mFirstFace;
            for (uint32_t f = faceSet->mFaceCount; f != 0; f--, face++) {
                int32_t col[3], row[3], height[3];   // 0x9c, 0x84, 0x90
                for (int32_t k = 2; k >= 0; k--) {
                    const uint16_t index = node->mF94.get()[face->mF02[k]];
                    const float* b = reinterpret_cast<const float*>(vertices + (index >> 2) * 0x30 + (index & 3) * 4);
                    const float y = b[4];
                    float a = b[0] / 10.0f;
                    a = 5.0f * a;
                    col[k] = ppc_fctiwz(static_cast<double>(a));
                    float c = b[8] / 10.0f;
                    c = 5.0f * c;
                    row[k] = ppc_fctiwz(static_cast<double>(c));
                    float h = 255.0f * y;
                    h = h / 100.0f;
                    height[k] = ppc_fctiwz(static_cast<double>(h));
                }
                int32_t lo = row[0];
                if (row[1] < lo) lo = row[1];
                int32_t hi = row[0];
                if (row[1] > row[0]) hi = row[1];
                if (row[2] < lo) lo = row[2];
                if (row[2] > hi) hi = row[2];
                const int32_t lines = hi - lo + 1;
                for (int32_t n = 0; n < lines; n++) {
                    gRideHeightScan[n][0] = 1000000;
                    gRideHeightScan[n][1] = -1000000;
                }
                for (int32_t k = 2; k >= 0; k--) {
                    const int32_t nx = (k + 1) % 3;
                    const int32_t c0 = col[k], h0 = height[k];
                    const int32_t c1 = col[nx], h1 = height[nx];
                    const int32_t dRow = row[nx] - row[k];
                    const int32_t dCol = c1 - c0;
                    const int32_t dHeight = h1 - h0;
                    const int32_t half = dRow / 2;
                    int32_t step = 1;
                    int32_t span = dRow;
                    if (dRow < 0) {
                        step = -1;
                        span = -dRow;
                    }
                    int32_t* e = gRideHeightScan[row[k] - lo];
                    if (c0 < e[0]) {
                        e[0] = c0;
                        e[2] = h0;
                    }
                    if (c0 > e[1]) {
                        e[1] = c0;
                        e[3] = h0;
                    }
                    int32_t* e2 = e + dRow * 4;
                    if (c1 < e2[0]) {
                        e2[0] = c1;
                        e2[2] = h1;
                    }
                    if (c1 > e2[1]) {
                        e2[1] = c1;
                        e2[3] = h1;
                    }
                    for (int32_t i = 1; i < span; i++) {
                        e += step * 4;
                        const int32_t cv = c0 + (half + dCol * i) / span;
                        const int32_t hv = h0 + (dHeight * i) / span;
                        if (cv < e[0]) {
                            e[0] = cv;
                            e[2] = hv;
                        }
                        if (cv > e[1]) {
                            e[1] = cv;
                            e[3] = hv;
                        }
                    }
                }
                const int32_t* e = gRideHeightScan[0];
                for (int32_t n = 0; n < lines; n++, e += 4) {
                    const int32_t r = n + lo;
                    const int32_t c0 = e[0], c1 = e[1], h0 = e[2], h1 = e[3];
                    const int32_t width = c1 - c0;
                    const int32_t dHeight = h1 - h0;
                    uint8_t* cell = map->mCells.get() + (c0 + gRideHeightStride * r);
                    if (h0 > *cell) *cell = static_cast<uint8_t>(h0);
                    cell = map->mCells.get() + (c1 + gRideHeightStride * r);
                    if (h1 > *cell) *cell = static_cast<uint8_t>(h1);
                    for (int32_t j = 1; j < width; j++) {
                        const int32_t hv = h0 + (dHeight * j) / width;
                        cell = map->mCells.get() + (c0 + gRideHeightStride * r + j);
                        if (hv > *cell) *cell = static_cast<uint8_t>(hv);
                    }
                }
            }
        }
    }
    for (tag_sNode* child = node->mF0C.get(); child != nullptr; child = child->mF08.get()) {
        if (CalculateMaxVertexHeights(child, header, matrix, map, badNode, anim, unused, flag) == 0) ok = 0;
    }
    return ok;
}

// GenerateRideHeightMap 0x1005db4c (1892 bytes, TPRide.cpp): the height map of a ride. It instances the ride's animation set,
// takes the vertices' highest point per cell of a five-by-five finer grid (CalculateMaxVertexHeights) either once, or, when the
// set has animations other than the two first kinds, once per 30 ms step of every animation, then reduces each cell to the
// highest of its 25 finer cells. Answers the map, or NULL when the set cannot be instanced.

// 0x102c2140: the second word of the pair the heights code reads, the map's depth in finer cells (height times five).
int32_t gRideHeightRows;

namespace {

// The map's word at 0x08 holds two halves: the width at 0x08 and the depth at 0x0a, in the byte order of the machine.
uint16_t* MapHalves(sRideHeightMap* map) { return reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(map) + 8); }

void SetLimits(sRideHeightMap* map) {
    const uint16_t* halves = MapHalves(map);
    gRideHeightLimitX = 10.0f * static_cast<float>(halves[0]);
    gRideHeightLimitZ = 10.0f * static_cast<float>(halves[1]);
    gRideHeightStride = static_cast<int32_t>(halves[0]) * 5;
    gRideHeightRows = static_cast<int32_t>(halves[1]) * 5;
}

}  // namespace

sRideHeightMap* GenerateRideHeightMap(tag_sRideAnimSet* set, uint32_t w, uint32_t h, RideShape* shape, int32_t flag) {
    tag_sRideAnimSet* inst = nullptr;
    if (InstanceRideAnimSet(set, nullptr, 0x211f, &inst, 0) == 0) return nullptr;
    InitRideAnimSet(inst, 1, 0, 0);
    tag_sM3D2Header* instHeader = inst->mHeads->mHeader;   // read again after the instance is destroyed

    sRideHeightMap* map = static_cast<sRideHeightMap*>(MReleaseAllocF(0x30, 1));
    map->mF00 = 0xab1e0003u;
    map->mF04 = 0x640005u;
    MapHalves(map)[0] = static_cast<uint16_t>(w);
    MapHalves(map)[1] = static_cast<uint16_t>(h);
    const uint32_t cellCount = w * h;
    const uint32_t fineBytes = w * 5 * h * 5;   // five by five finer cells per cell, a byte each
    map->mCells = static_cast<uint8_t*>(MReleaseAllocF(fineBytes + cellCount * 2, 1));
    map->mF10 = map->mCells.mOffset + fineBytes;
    map->mF14 = map->mF10 + cellCount;

    int32_t flagWord = 0;
    tag_sNode* badNode = nullptr;
    bool animated = false;
    if (inst->mAnimationCount != 0) {
        for (uint32_t kind = 0; kind < 12; kind++) {
            if (kind != 0 && kind != 1 && inst->mGroups[kind].mCount != 0) animated = true;
        }
    }

    if (animated) {
        map->mMin[0] = 3.4028234663852886e+38f;
        map->mMin[1] = 3.4028234663852886e+38f;
        map->mMin[2] = 3.4028234663852886e+38f;
        map->mMax[0] = 1.1754943508222875e-38f;
        map->mMax[1] = 1.1754943508222875e-38f;
        map->mMax[2] = 1.1754943508222875e-38f;
        for (uint32_t i = 0; i < inst->mAnimationCount; i++) {
            tag_sAnimHeader* anim = inst->mAnimations[i];
            if (anim == nullptr) continue;
            if (inst->mGroups[0].mEntries != nullptr && anim == inst->mGroups[0].mEntries->mAnimation) continue;
            if (inst->mGroups[1].mEntries != nullptr && anim == inst->mGroups[1].mEntries->mAnimation) continue;

            uint32_t time = 0;
            uint32_t kind, index, unused;
            sAnimTimeControl control;
            GetAnimStats(inst, i, &kind, &index, &unused);
            control.mF04 = 0xc;
            control.mF08 = 0;
            control.mF0C = 1.0f;
            SetRideAnimTime(time);
            ResetAnimTimeStats(&control, anim, 1, 0.0f);
            GetAnimTimeStats(&control, &time);   // the original passes a third argument, 1, which the function ignores
            SetNewAnim(inst, kind, index, &control, 0, 1.0f);
            while (GetAnimRemainingMilliseconds(&control) >= 0.0f) {
                RATick(&control, inst->mHeads, anim);
                const int32_t steps = ppc_fctiwz(control.mF20);
                tag_sMesh* mesh = inst->mHeads;
                SetLimits(map);
                sMatrix matrix;
                SetMatrixIdentity(&matrix);
                CalculateMaxVertexHeights(mesh->mHeader->mF78, mesh->mHeader, matrix, map, &badNode, anim, steps, &flagWord);
                QuickCopyUnmaskedMeshVertices(inst->mHeads->mF08, inst->mHeads, 1);
                tag_sNode* node = instHeader->mNodes;
                for (uint32_t n = instHeader->mNodeCount; n != 0; n--) {
                    node->mFlags &= 0xff7fffffu;
                    node++;
                }
                time += 0x1e;
                SetRideAnimTime(time);
                GetAnimTimeStats(&control, &time);
            }
            ResetRideAnimationSingleChannel(inst, inst->mGroups[control.mF04].mEntries[control.mF08].mAnimation, nullptr);
        }
    } else {
        tag_sM3D2Header* original = set->mHeads->mHeader;
        std::memcpy(map->mMin, original->mBox.mMin, sizeof(map->mMin));
        std::memcpy(map->mMax, original->mBox.mMax, sizeof(map->mMax));
        tag_sMesh* mesh = inst->mHeads;
        SetLimits(map);
        sMatrix matrix;
        SetMatrixIdentity(&matrix);
        CalculateMaxVertexHeights(mesh->mHeader->mF78, mesh->mHeader, matrix, map, &badNode, nullptr, 0, &flagWord);
    }
    DestroyRideAnimSetInstance(inst, nullptr, 0);

    uint8_t* fine = map->mCells;
    Ptr32<uint8_t> heights;
    heights.mOffset = map->mF10;
    Ptr32<uint8_t> solid;
    solid.mOffset = map->mF14;
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint8_t best = 0;
            const uint32_t base = x * 5 + w * (y * 0x19);
            for (uint32_t row = 0; row < 5; row++) {
                for (uint32_t col = 0; col < 5; col++) {
                    const uint8_t value = fine[base + (col + w * (row * 5))];
                    if (value > best) best = value;
                }
            }
            if (flag != 0)
                heights[x + y * w] = static_cast<uint8_t>(ppc_fctiwz((255.0f * instHeader->mBox.mMax[1]) / 100.0f));
            else
                heights[x + y * w] = best;
            if (shape == nullptr || shape->mCells[y][x].mValue != 0)
                solid[x + y * w] = 1;
            else
                solid[x + y * w] = 0;
        }
    }
    return map;
}

// Unit v5127: LoadFileIntoMemory 0x1005e300, LoadRideHeightMap 0x1005e40c, GetRideHeightMap 0x1005e8e8.

// 0x1005e300: the whole file in a block; mode "rb" (the TOC string at 0x1023d2b0) when binary, else "rb" + 3 = "b".
char* LoadFileIntoMemory(char* path, int32_t binary) {
    void* block = nullptr;
    uint32_t read = 0;
    bfl::FileSystem::CFile* file;
    // The TOC string is "rb" followed by "r": the pool holds "rb\0r\0", so "rb" + 3 is the text "r".
    static const char kModes[] = "rb\0r";
    if (binary == 0)
        file = TPFile_Open(path, kModes + 3);
    else
        file = TPFile_Open(path, kModes);
    if (file != nullptr) {
        uint32_t size = TPFile_Size(file);
        block = MReleaseAllocF(size, 1);
        if (block != nullptr) read = TPFile_Read(block, size, 1, file);
        TPFile_Close(file);
    }
    if (read == 0) {
        MReleaseFreeF(block);
        block = nullptr;
    }
    return static_cast<char*>(block);
}

// 0x1005e40c: loads and byte-swaps a height map file, checks its magic and turns the offsets into pointers.
sRideHeightMap* LoadRideHeightMap(tag_sRideAnimSet* set, uint32_t w, uint32_t h, char* path) {
    sRideHeightMap* map = reinterpret_cast<sRideHeightMap*>(LoadFileIntoMemory(path, 1));
    if (map != nullptr) {
        uint8_t* bytes = reinterpret_cast<uint8_t*>(map);
        USwapInplace32ULong_1005e658(map->mF00);
        USwapInplace32ULong_1005e658(map->mF04);
        USwapInplace16UShort_1005e610(*reinterpret_cast<uint16_t*>(bytes + 8));
        USwapInplace16UShort_1005e610(*reinterpret_cast<uint16_t*>(bytes + 10));
        uint32_t* words = reinterpret_cast<uint32_t*>(bytes);
        words[3] = USwap32UCharPtr_1005e5b4(words[3]);
        words[4] = USwap32UCharPtr_1005e5b4(words[4]);
        words[5] = USwap32UCharPtr_1005e5b4(words[5]);
        reinterpret_cast<sAlignedBox*>(bytes + 0x18)->Byteswap();
        if (map->mF00 == 0xab1e0003 && map->mF04 == 0x640005) {
            // The Mac adds the block's address; here the three words become arena offsets (Ptr32's convention, as
            // GenerateRideHeightMap fills them), so the block's own arena offset is added.
            uint32_t base = tpw::ArenaOffset(map);
            words[3] += base;
            words[4] += base;
            words[5] += base;
        } else {
            MReleaseFreeF(map);
            map = nullptr;
        }
    }
    return map;
}

// The mesh file loader (engine library of SimTheme Park 2.04b3, 0x100483f4..0x1004b9c8 and
// AlignTo64Bit at 0x10039078; the original file's name is not in the binary, the inventory
// places the range after tb_printf_opengl.cpp).
//
// LoadM3D2Header reads a whole .md2 file into one block and makes it usable in place.
// The file is little-endian and holds offsets from its start where the structures of
// include/m3d2.h have pointers. The original does two things to every record: it swaps
// each field to the Mac's byte order, and it adds the block's address to each offset that
// is not 0. Here the swaps go through SamsUtilities' block swappers, which do nothing on a
// little-endian host, and an offset becomes a pointer with Ptr32::Relocate.
//
// Two bit fields are the exception to "swap each field": the Mac's compiler numbers the
// bits of a bit field from the other end, so the original REBUILDS those words instead of
// swapping them, and the engine's code then reads the rebuilt layout. They are rebuilt
// here on every host, from the file's little-endian value, into the value the Mac has:
//   a face's first word: index (15 bits) and flag (1 bit) become index << 1 | flag;
//   a compressed vertex: three 10-bit fields a, b, c (from bit 0 up) become
//   a << 22 | b << 12 | c << 2.

void sAlignedBox::Byteswap() { SwapWords32(this, 6); }

namespace {
// The stored pointer words are arena offsets on the host (Ptr32); 0 is null.
inline void* PtrOf(uint32_t offset) { return offset != 0 ? tpw::gArenaBase + offset : nullptr; }
}  // namespace

// 0x1005e6a0: writes the height map to `path` as one block: the 0x30-byte header, then the cells (25 bytes per
// cell), then the two byte planes (1 byte per cell each); the three pointers are stored as offsets from the block start.
// Answers 1 when fwrite wrote its one item, else 0 (also when the file cannot be opened).
void SaveRideHeightMap(sRideHeightMap* map, char* path) {
    const uint8_t* src = reinterpret_cast<const uint8_t*>(map);
    uint32_t count = static_cast<uint32_t>(*reinterpret_cast<const uint16_t*>(src + 8)) *
                     static_cast<uint32_t>(*reinterpret_cast<const uint16_t*>(src + 10));
    uint32_t cellBytes = count * 0x19;
    uint32_t total = cellBytes + count * 2 + 0x30;
    uint8_t* block = static_cast<uint8_t*>(MReleaseAllocF(total, 0));
    uint32_t* head = reinterpret_cast<uint32_t*>(block);
    const uint32_t* from = reinterpret_cast<const uint32_t*>(src);
    for (int i = 0; i < 12; ++i) head[i] = from[i];

    uint8_t* cursor = block + 0x30;
    head[3] = tpw::ArenaOffset(cursor);
    cursor += cellBytes;
    if (PtrOf(head[3]) != nullptr && PtrOf(from[3]) != nullptr && cellBytes != 0)
        std::memcpy(PtrOf(head[3]), PtrOf(from[3]), cellBytes);
    head[4] = tpw::ArenaOffset(cursor);
    cursor += count;
    if (PtrOf(head[4]) != nullptr && PtrOf(from[4]) != nullptr && count != 0)
        std::memcpy(PtrOf(head[4]), PtrOf(from[4]), count);
    head[5] = tpw::ArenaOffset(cursor);
    if (PtrOf(head[5]) != nullptr && PtrOf(from[5]) != nullptr && count != 0)
        std::memcpy(PtrOf(head[5]), PtrOf(from[5]), count);

    uint32_t base = tpw::ArenaOffset(block);
    head[3] -= base;
    head[4] -= base;
    head[5] -= base;

    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        CTRACE(0, "Could not open file '%s' for writing\n", path);
        MReleaseFreeF(block);
        return;
    }
    std::fwrite(block, total, 1, file);
    std::fclose(file);
    MReleaseFreeF(block);
}

// 0x1005e8e8: the stored map when it matches (w, h), else a generated one that is saved; its box goes to the mesh header.
void GetRideHeightMap(tag_sRideAnimSet* set, uint32_t w, uint32_t h, char* path, void* a, int32_t b) {
    sRideHeightMap* map = LoadRideHeightMap(set, w, h, path);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(map);
    if (map == nullptr || w != *reinterpret_cast<const uint16_t*>(bytes + 8) ||
        h != *reinterpret_cast<const uint16_t*>(bytes + 10)) {
        map = GenerateRideHeightMap(set, w, h, static_cast<RideShape*>(a), b);
        if (map != nullptr) SaveRideHeightMap(map, path);
    }
    // No NULL check on map, as in the original.
    uint32_t* dest = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(set->mHeads->mHeader) + 0x80);
    const uint32_t* src = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(map) + 0x18);
    for (int i = 0; i < 6; ++i) dest[i] = src[i];
    set->mHeightMap = map;
}

// Ride and mesh teardown (main app, unit w105): the destroyers the lobby's leaving reaches. DestroyRideType 0x1006a768 (608 bytes),
// DestroyRideAnimSet 0x1006a5e8 (324), DestroyRideAnimSetInstance 0x10063338 (368), DestroyMesh 0x10064de0 (308), DestroyMeshHeader
// 0x10064d7c (40), DestroyRideHeightMap 0x1005e9f4 (60), DestroySkirtVertices 0x1006fbd8 (68, Debug.cpp on the Mac), DMFreeMesh
// 0x10030ae8 (240) and the two DMDisableMeshRender 0x100309c8 (68) and 0x10030a48 (84) (TPAdvisor.cpp on the Mac).
// (DestroyMeshInstance 0x100631c4 is in MeshDestroy.cpp.)

// 0x1005e9f4.
void DestroyRideHeightMap(tag_sRideAnimSet* set) {
    MReleaseFreeF(set->mHeightMap);
    set->mHeightMap = nullptr;
}

// TPRide.cpp (main app of SimTheme Park 2.04b3): the two terrain-height lookups, unit w140: DetermineHFieldHeight (0x1005ea70, 776 bytes)
// and DetermineHFieldHeightQuick (0x1005eda8, 676 bytes).
//
// The height field is gHeightfieldHeader->mHField (include/m3d2.h sHField): the cell sizes mF08[2] (x) and mF08[3] (z), the origin
// mF08[0] and mF08[1], the counts mF18[0] (width) and mF18[1] (depth), (width + 1) * (depth + 1) heights and one sHFieldQuad per cell.
// Both lookups pick the cell, the fraction inside it and the diagonal the cell is cut along, then interpolate the corner heights.
// Every float is a single in the listing's order; the fused multiply-adds are ppc_fmadds (4 in the full lookup, 10 in the quick one).
// The Mac reads the quad's flags at quads + index * 4, which is sHFieldQuad::mF00.

namespace {
// An unsigned count as the Mac converts it (the magic 2^52 trick): the exact value rounded to a single.
float CountToFloat(uint32_t n) { return static_cast<float>(n); }

// x and z must lie inside the field: 0 <= x < 10 * width and 0 <= z < 10 * depth (the cells are 10 units wide in the field's own scale).
bool InsideField(const sHField* hf, float x, float z) {
    const float zero = 0.0f;
    if (x < zero) return false;
    if (z < zero) return false;
    const float ten = 10.0f;
    if (x >= ten * CountToFloat(hf->mF18[0])) return false;
    if (z >= ten * CountToFloat(hf->mF18[1])) return false;
    return true;
}
}  // namespace

// The terrain height at (x, z): 0.0f outside the field (and where the cell fraction falls outside it); with flag 0 a cell whose flag
// word has bit 1 answers FLT_MAX (the largest float: no ground there). The fraction (x - origin) / cell size picks the cell with the
// unsigned truncation __cvt_fp2unsigned. Bit 4 of the cell's flags is the cell's other diagonal.
float DetermineHFieldHeight(float x, float z, int32_t flag) {
    const sHField* const hf = gHeightfieldHeader->mHField.get();
    if (!InsideField(hf, x, z)) return 0.0f;
    const float width = CountToFloat(hf->mF18[0]);
    const float depth = CountToFloat(hf->mF18[1]);
    const float fx = (x - hf->mF08[0]) / hf->mF08[2];
    if (fx < 0.0f || !(fx <= width)) return 0.0f;
    double whole = 0.0;
    const float fracX = static_cast<float>(std::modf(static_cast<double>(fx), &whole));
    const uint32_t ix = cw_cvt_fp2unsigned(static_cast<double>(fx));
    const float fz = (z - hf->mF08[1]) / hf->mF08[3];
    if (fz < 0.0f || !(fz <= depth)) return 0.0f;
    const float fracZ = static_cast<float>(std::modf(static_cast<double>(fz), &whole));
    const float wholeZ = static_cast<float>(whole);
    const uint32_t iz = cw_cvt_fp2unsigned(static_cast<double>(wholeZ));
    const uint32_t cell = ix + iz * hf->mF18[0];
    const uint16_t quad = hf->mQuads.get()[static_cast<int32_t>(cell)].mF00;
    if (flag == 0 && (quad & 1) != 0) return FLT_MAX;
    const uint32_t row = hf->mF18[0] + 1;
    const int32_t v00 = static_cast<int32_t>(ix + iz * row);
    const int32_t v10 = static_cast<int32_t>(ix + 1 + iz * row);
    const int32_t v01 = static_cast<int32_t>(ix + (iz + 1) * row);
    const int32_t v11 = static_cast<int32_t>(ix + 1 + (iz + 1) * row);
    // slopeFar: the slope is taken along the far edge (v11 - v10), else the near one (v01 - v00); upper: the far corner's form.
    bool slopeFar;
    bool upper;
    if ((quad & 4) != 0) {
        if (fracX >= 1.0f - fracZ) {
            slopeFar = true;
            upper = true;
        } else {
            slopeFar = false;
            upper = false;
        }
    } else if (fracX >= fracZ) {
        slopeFar = true;
        upper = false;
    } else {
        slopeFar = false;
        upper = true;
    }
    const float* const h = hf->mHeights.get();
    float slope;
    if (slopeFar) slope = h[v11] - h[v10];
    else slope = h[v01] - h[v00];
    if (upper) {
        const float base = fracX * h[v11];
        const float lower = ppc_fmadds(1.0f - fracX, h[v01], base);
        return ppc_fmadds(1.0f - fracZ, -slope, lower);
    }
    const float base = fracX * h[v10];
    const float lower = ppc_fmadds(1.0f - fracX, h[v00], base);
    return ppc_fmadds(fracZ, slope, lower);
}

// The quick lookup: no origin, no flag word test for "no ground", the cell index is the fctiwz truncation (a signed word), and a cell
// without bit 0x800 is flat along its first diagonal whatever its bit 4 says (the full lookup never reads 0x800).
float DetermineHFieldHeightQuick(float x, float z) {
    const sHField* const hf = gHeightfieldHeader->mHField.get();
    if (!InsideField(hf, x, z)) return 0.0f;
    double wholeX = 0.0;
    double wholeZ = 0.0;
    const float fracX = static_cast<float>(std::modf(static_cast<double>(x / hf->mF08[2]), &wholeX));
    const int32_t ix = ppc_fctiwz(wholeX);
    const float fracZ = static_cast<float>(std::modf(static_cast<double>(z / hf->mF08[3]), &wholeZ));
    const int32_t iz = ppc_fctiwz(wholeZ);
    const int32_t width = static_cast<int32_t>(hf->mF18[0]);
    const int32_t cell = static_cast<int32_t>(static_cast<uint32_t>(ix) + static_cast<uint32_t>(iz) * static_cast<uint32_t>(width));
    const uint16_t quad = hf->mQuads.get()[cell].mF00;
    const int32_t v00 = static_cast<int32_t>(static_cast<uint32_t>(ix) + static_cast<uint32_t>(iz) * static_cast<uint32_t>(width + 1));
    const int32_t v10 = v00 + 1;
    const int32_t v01 = v00 + width + 1;
    const int32_t v11 = v00 + width + 2;
    const float* const h = hf->mHeights.get();
    if ((quad & 0x800) == 0) {
        const float base = fracX * h[v10];
        const float lower = ppc_fmadds(1.0f - fracX, h[v00], base);
        return ppc_fmadds(fracZ, h[v01] - h[v00], lower);
    }
    if ((quad & 4) != 0) {
        const float limit = 1.0f - fracZ;
        if (fracX >= limit) {
            const float base = fracX * h[v11];
            const float lower = ppc_fmadds(1.0f - fracX, h[v01], base);
            return ppc_fmadds(limit, -(h[v11] - h[v10]), lower);
        }
        const float base = fracX * h[v10];
        const float lower = ppc_fmadds(1.0f - fracX, h[v00], base);
        return ppc_fmadds(fracZ, h[v01] - h[v00], lower);
    }
    if (fracX >= fracZ) {
        const float base = fracX * h[v10];
        const float lower = ppc_fmadds(1.0f - fracX, h[v00], base);
        return ppc_fmadds(fracZ, h[v11] - h[v10], lower);
    }
    const float base = fracX * h[v11];
    const float lower = ppc_fmadds(1.0f - fracX, h[v01], base);
    return ppc_fmadds(1.0f - fracZ, -(h[v01] - h[v00]), lower);
}

// Unit v5901: the AI-world cell helpers of the level code (Destroy3DObjectFromAIWorld, Assign3DObjectToAIWorld,
// GetWorldGSquareHeight, DetermineInnerBaseInsets, GetRideIDAtCell).

// 0x1029cf10 (name invented): the halfword a thing id is compared with (0 at start).
uint16_t gLevelNoThingId = 0;

namespace {

const char kThingZeroMsg[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1023d2de

// The cell of (x, y) as the Mac computes it: world + 0x2d8 + (id16 - 1) * 0x44.
CMapCell* AICell(int32_t x, int32_t y) {
    uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
    int32_t index = static_cast<int32_t>(id) - 1;
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8 + index * 0x44);
}

void ClampXY(int32_t& x, int32_t& y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= gHeightfieldWidth) x = gHeightfieldWidth - 1;
    if (y >= gHeightfieldHeight) y = gHeightfieldHeight - 1;
}

// fctiwz: truncates toward zero, saturating (0x80000000 for NaN)
int32_t Trunc(float v) {
    if (v != v) return INT32_MIN;
    if (v >= 2147483648.0f) return INT32_MAX;
    if (v <= -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(v);
}

// The ride id of a cell with no track mesh: the thing of the cell's column override, when it is a ride (kind 3).
uint32_t RideOfOverride(int32_t x, int32_t y) {
    uint16_t id;
    LLCR_OverRide(&id, x, y);
    if (id != gLevelNoThingId) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMsg);
        const uint8_t* thing = static_cast<const uint8_t*>(gThingArray[id].mThing);
        if (thing != nullptr && thing[2] == 3) return *reinterpret_cast<const uint32_t*>(thing + 0x1c);
    }
    return 0;
}

}  // namespace

// 0x1005f080 (184 bytes)
int32_t GetRideIDAtCell(int32_t x, int32_t y) {
    uint32_t ride = LLCC_GetTrackMeshAt(x, y);
    if (ride == 0) ride = RideOfOverride(x, y);
    return static_cast<int32_t>(ride);
}
// 0x1005f160 (620 bytes)
float GetWorldGSquareHeight(int32_t x, int32_t y, uint32_t* extra) {
    uint32_t ride;
    if (extra != nullptr && *extra != 0) {
        ride = *extra;
    } else {
        ride = LLCC_GetTrackMeshAt(x, y);
        if (ride == 0) ride = RideOfOverride(x, y);
    }
    float fx = 10.0f * static_cast<float>(x);
    float fz = 10.0f * static_cast<float>(y);
    if (ride != 0) {
        if (extra != nullptr) *extra = ride;
        tag_sRideAnimSet* set = gRideAnimSets[ride];
        if (set != nullptr) {
            const uint8_t* block = static_cast<const uint8_t*>(set->mHeightMap);
            if (block != nullptr) {
                tag_sMesh* mesh = set->mHeads;
                int32_t rotation = mesh->mF10;
                const uint8_t* node = reinterpret_cast<const uint8_t*>(mesh->mHeader->mF78.get());
                float nx, nz, ny;
                std::memcpy(&nx, node + 0x40, 4);
                std::memcpy(&ny, node + 0x44, 4);
                std::memcpy(&nz, node + 0x48, 4);
                float u = fx - nx;
                float v = fz - nz;
                float a = u, b = v;
                if (rotation == 0xb4) {
                    a = -u - 10.0f;
                    b = -v - 10.0f;
                } else if (rotation < 0xb4) {
                    if (rotation == 0x5a) {
                        a = -v - 10.0f;
                        b = u;
                    }
                } else if (rotation == 0x10e) {
                    a = v;
                    b = -u - 10.0f;
                }
                int32_t row = Trunc(b / 10.0f);
                int32_t col = Trunc(a / 10.0f);
                uint16_t width = *reinterpret_cast<const uint16_t*>(block + 8);
                // The heights (+0x10): a 32-bit word of the map image, an arena offset on the host like sRideHeightMap's other two.
                Ptr32<const uint8_t> data;
                data.mOffset = *reinterpret_cast<const uint32_t*>(block + 0x10);
                int32_t index = col + static_cast<int32_t>(static_cast<uint32_t>(row) * width);
                float byte = static_cast<float>(static_cast<double>(data[index]));
                float scaled = 100.0f * byte;
                return scaled / 255.0f + ny;
            }
        }
    }
    if (extra == nullptr) return DetermineHFieldHeightQuick(fx, fz);
    *extra = 0;
    return 0.0f;
}

// 0x1005f3fc (268 bytes)
void Destroy3DObjectFromAIWorld(uint32_t a, uint32_t b) {
    int32_t x = static_cast<int32_t>(a);
    int32_t y = static_cast<int32_t>(b);
    ClampXY(x, y);
    if (CMap::IsXYValid(x, y)) {
        CMapCell* cell = AICell(x, y);
        if (cell != nullptr) {
            CTrackCell* track = cell->GetTrackCellEquiv();
            cell->Delete(0, 0);
            track->Delete(0, 0, true);
        }
    }
}

// 0x1005f53c (384 bytes); the instance is not read.
void Assign3DObjectToAIWorld(void* instance, uint32_t a, uint32_t b, uint32_t c, int32_t d) {
    (void)instance;
    int32_t x = static_cast<int32_t>(a);
    int32_t y = static_cast<int32_t>(b);
    ClampXY(x, y);
    if (CMap::IsXYValid(x, y)) {
        CMapCell* cell = AICell(x, y);
        if (cell != nullptr) {
            CTrackCell* track = cell->GetTrackCellEquiv();
            uint16_t saved = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(cell) + 0x10);
            cell->Delete(0, 0);
            cell->Modify(static_cast<CELL_MODES>(0x15), 0, 0);
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id;
            id.mIndex = saved;
            cell->ModifyParentID(id);
            *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(track) + 4) = c;
            if (d != 0)
                cell->SetFlag(static_cast<uint16_t>(cell->GetFlag() | 0x100));
            else
                cell->SetFlag(static_cast<uint16_t>(cell->GetFlag() & 0xfeff));
        }
    }
}

// 0x1005f6f4 (24 bytes): 0.02f (the constant 0x3ca3d70a) into both words, the one at 0x102c1e0c first.
void SetupHeightmapSystem() {
    const float scale = 0.02f;
    gHeightmapScaleA = scale;
    gHeightmapScaleB = scale;
}

// 0x1005f738 (316 bytes): for each of the four corners, the half vector to the second nearest point (in x and z).
// With fewer than two points the zero answer goes to insets[0] every time (the original never advances its output).
void DetermineInnerBaseInsets(s2DPoint* corners, s3DPoint* points, int32_t count, s2DPoint* insets) {
    for (int32_t i = 0; i < 4; ++i) {
        float best = 3.4028234663852886e+38f;
        float second = best;
        int32_t bestIndex = -1;
        int32_t secondIndex = -1;
        for (int32_t j = 0; j < count; ++j) {
            float dx = points[j].x - corners[i].x;
            float dz = points[j].z - corners[i].y;
            float dzz = dz * dz;
            double sum = static_cast<double>(dx) * static_cast<double>(dx) + static_cast<double>(dzz);
            float d = static_cast<float>(std::sqrt(sum));
            if (d < best) {
                second = best;
                secondIndex = bestIndex;
                best = d;
                bestIndex = j;
            } else if (d < second) {
                second = d;
                secondIndex = j;
            }
        }
        if (secondIndex == -1) {
            insets[0].x = 0.0f;
            insets[0].y = 0.0f;
        } else {
            insets[i].x = (points[secondIndex].x - corners[i].x) * 0.5f;
            insets[i].y = (points[secondIndex].z - corners[i].y) * 0.5f;
        }
    }
}

// Unit v51119: the world globe's camera steps and list registration (CLBScreenComponentOnlineGlobe), and the small
// destructors and byte swaps the neighbouring addresses hold.

namespace {
// The word at (pointer word + off): the Mac's 32-bit pointers are offsets from the arena's base on the host.
inline uint32_t& W(uint32_t base, uint32_t off) {
    return *reinterpret_cast<uint32_t*>(tpw::ArenaBase() + static_cast<uintptr_t>(base) + off);
}
}  // namespace

// 0x1006427c: the Mac object is 0x74 bytes: +0 pointer to the CLight part's vtable record, +4 the class's vtable, +8 the shared
// CMapLinkBase, +0x10 the CLight part. Every pointer is a 32-bit word.
CLightPoint* CLightPoint_DtorMac(CLightPoint* self, int16_t flag) {
    if (self == nullptr) return self;
    uint32_t me = tpw::ArenaOffset(self);
    uint32_t vt2 = A(gCLightParallelVt2);
    uint32_t vt3 = A(gCLightParallelVt3);
    uint32_t vt1 = A(gCLightPointVt);
    W(me, 4) = vt1;
    W(W(me, 0), 0x5c) = vt1 + 0x24;
    W(W(W(me, 0), 0), 0) = vt1 + 0xc;
    uint32_t q = W(W(me, 0), 0);
    W(q, 4) = (me + 8) - q;
    uint32_t p = W(me, 0);
    W(p, 0x60) = (me + 0x10) - p;
    if (flag != 0) {
        if (me + 0x10 != 0) {
            W(me, 0x6c) = vt3;
            W(W(me, 0x10), 0) = vt3 + 0x18;
            uint32_t r = W(me, 0x10);
            W(r, 4) = (me + 0x70) - r;
        }
        if (me + 8 != 0) W(me, 8) = vt2;
    }
    if (flag > 0) OstreamDelete(self);
    return self;
}

// Unit v51125: small leaves of the main app (see include/std_exception_what.h).

// ---- the callees outside the unit: weak defaults (the real calls) that the unit's test library replaces with recording fakes.
#define TPW_WEAK __attribute__((weak))
// ---- the byte swaps 0x1005e5b4, 0x1005e610, 0x1005e658. Their one caller, LoadRideHeightMap, swaps a .hmp's little-endian words to the
// Mac's order: the swaps are real only where the host's order differs from the file's (or when a test asks, TPW_SWAP_FILE_WORDS), like
// platform/sams_swaps.cpp's; a little-endian host's real swaps turned every .hmp's magic and rejected it (unit v5L44).
#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
uint32_t USwap32UCharPtr_1005e5b4(uint32_t word) { return __builtin_bswap32(word); }
void USwapInplace16UShort_1005e610(uint16_t& half) { half = __builtin_bswap16(half); }
void USwapInplace32ULong_1005e658(uint32_t& word) { word = __builtin_bswap32(word); }
#else
uint32_t USwap32UCharPtr_1005e5b4(uint32_t word) { return word; }
void USwapInplace16UShort_1005e610(uint16_t&) {}
void USwapInplace32ULong_1005e658(uint32_t&) {}
#endif
