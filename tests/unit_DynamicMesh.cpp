// Test library for the dynamic cell meshes (unit w112b; libtpw_unit_DynamicMesh.dylib, tests/test_DynamicMesh.py), main app:
//   src/DynamicMesh.cpp   CDynamicMesh (constructor, destructor, SetBoundingBox, DC_CreateSquareFace, DC_SetVertex), DMCreateMesh, the
//                         two DMEnableMeshRender, CorrectDTexturePaths, LLCC_SystemGetMode
//
// Every callee outside the unit is a fake that records the same event on both sides: UseSmallTextures, CGame::GetCurrentThemeName,
// SetMatrixIdentity, MReleaseAllocF (it hands out zeroed arena blocks and numbers them), CreateMeshFromM3D2Header,
// CreateEngineMesh, the three EnableMeshDraw, DMDisableMeshRender, DMFreeMesh, FindFilePath, SetNewMeshPaths, OverrideTexturePaths
// and DetermineHFieldHeightQuick. The libc calls are the host's own.
//
// The test module (Python) builds every world on both sides from one description and reads both sides back with the same code, so this
// file holds only: the fakes, the globals the unit reads (defined here because their owners are not in the library), a few allocation
// helpers that hand the module raw memory (native, or arena for what a Ptr32 field points to), the offsets of the native records, and
// one thin entry point per function that takes plain pointers and the bit patterns of floats.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "DynamicMesh.h"
#include "Game.h"
#include "GameSys.h"
#include "LLCellSystem.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "analyser.h"
#include "calllog.h"
#include "fake_strings.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files, defined here
CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
int32_t gLLCC480C;
int32_t gDynamicCellF9BD4;
float gDynamicCellF9DA0;
sDTextureEntry gDynamicCellTextureEntries[20];

namespace {

// The blocks MReleaseAllocF handed out in this case, in order (a pointer into block k is told as ((k + 1) << 32) | offset), the
// memory the module asked for, and the mesh records the fake of CreateMeshFromM3D2Header made.
struct Block {
    uint8_t* address;
    size_t size;
};
std::vector<Block> gBlocks;
std::vector<void*> gNative;
std::vector<void*> gArena;
std::vector<void*> gMeshes;
const int32_t kMeshLabel = 7;   // the label the module gives the mesh record of the case

int64_t Tag(const void* pointer) {
    if (pointer == nullptr) return 0;
    const uint8_t* address = static_cast<const uint8_t*>(pointer);
    for (size_t k = 0; k < gBlocks.size(); ++k)
        if (address >= gBlocks[k].address && address < gBlocks[k].address + gBlocks[k].size)
            return (static_cast<int64_t>(k + 1) << 32) | static_cast<int64_t>(address - gBlocks[k].address);
    return lab(pointer);
}

uint64_t Pack(const void* pointer) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)); }

float FromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

// The search paths FindFilePath's fake can answer (by the value in the case's queue "find"; 0 is NULL).
const char* const kSearchPaths[] = {"", "SearchPath1:", "Other:SearchPath2:", "S3"};

}  // namespace

// ---- the fakes
bool UseSmallTextures() {
    ev("UseSmallTextures");
    return (ret("small", 0) & 0xff) != 0;
}

const TbDynamicStringTemplate<char>& CGame::GetCurrentThemeName() {
    ev("GetCurrentThemeName", {lab(this)});
    return mCurrentThemeName;
}

void SetMatrixIdentity(sMatrix* matrix) { ev("SetMatrixIdentity", {Tag(matrix)}); }

void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {size, flags});
    void* block = tpw::ArenaAllocZeroed(size);
    gArena.push_back(block);
    gBlocks.push_back({static_cast<uint8_t*>(block), size});
    return block;
}

tag_sMesh* CreateMeshFromM3D2Header(tag_sM3D2Header* header) {
    ev("CreateMeshFromM3D2Header", {Tag(header)});
    tag_sMesh* mesh = static_cast<tag_sMesh*>(std::calloc(1, sizeof(tag_sMesh)));
    mesh->mHeader = header;
    gMeshes.push_back(mesh);
    tpw_cl_label(kMeshLabel, mesh);
    return mesh;
}

void CreateEngineMesh(tag_sMesh* mesh) { ev("CreateEngineMesh", {lab(mesh)}); }

void EnableMeshDraw(tag_sMesh* mesh) { ev("EnableMeshDraw", {lab(mesh)}); }
void EnableMeshDraw(tag_sMesh* mesh, sMatrix* matrix, int32_t flags) { ev("EnableMeshDrawMatrix", {lab(mesh), Tag(matrix), flags}); }
void EnableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) {
    ev("EnableMeshDrawXZ", {lab(mesh), (static_cast<int64_t>(from.mF00) << 16) | from.mF02, (static_cast<int64_t>(to.mF00) << 16) | to.mF02});
}

// The set's flags word at the call, so that a clear or set done before the call shows.
int32_t DMDisableMeshRender(tag_sDMeshSet* set) {
    ev("DMDisableMeshRender", {lab(set), set->mF00});
    return 1;
}
int32_t DMFreeMesh(tag_sDMeshSet* set) {
    ev("DMFreeMesh", {lab(set), set->mF00});
    return 1;
}

// From the case's queue "find": 0 answers NULL, n the n-th search path text.
char* FindFilePath(const char* name, uint32_t flags, int32_t* result) {
    ev("FindFilePath", {flags, result != nullptr}, {name});
    const int64_t which = ret("find", 0);
    return which == 0 ? nullptr : const_cast<char*>(kSearchPaths[which & 3]);
}

void SetNewMeshPaths(char* root, char* folder, char* textureFolder, char* sharedTextureFolder) {
    // An event holds three texts: the root and the folder, then the two texture folders.
    ev("SetNewMeshPaths", {}, {root, folder});
    ev("SetNewMeshPaths.textures", {}, {textureFolder, sharedTextureFolder});
}

void OverrideTexturePaths(tag_sM3D2Header* header, int32_t checkFiles, int32_t unused) {
    ev("OverrideTexturePaths", {Tag(header), checkFiles, unused});
}

// From the case's queue "height" (the answer's bit pattern; the default is 0.0f).
float DetermineHFieldHeightQuick(float x, float z) {
    ev("DetermineHFieldHeightQuick", {fbits(x), fbits(z)});
    return FromBits(static_cast<uint32_t>(ret("height", 0)));
}

extern "C" {

// ---- memory for the module
// Frees everything the last case made.
void tpw_dm_reset() {
    for (void* block : gNative) std::free(block);
    gNative.clear();
    for (void* block : gArena) tpw::ArenaFree(block);
    gArena.clear();
    for (void* mesh : gMeshes) std::free(mesh);
    gMeshes.clear();
    gBlocks.clear();
}

// A block of `size` bytes filled with `fill` (native memory, or arena memory for what a Ptr32 field points to); a label puts it in the call
// log's table.
uint64_t tpw_dm_alloc(uint64_t size, uint32_t fill, int32_t label, int32_t arena) {
    void* block;
    if (arena != 0) {
        block = tpw::ArenaAllocZeroed(size);
        gArena.push_back(block);
    } else {
        block = std::malloc(size != 0 ? size : 1);
        gNative.push_back(block);
    }
    std::memset(block, static_cast<int>(fill), size);
    if (label > 0) tpw_cl_label(label, block);
    return Pack(block);
}

uint64_t tpw_dm_arena_base() { return Pack(tpw::ArenaBase()); }

// Block k of the case (address, size), 0 when there is none.
int32_t tpw_dm_block(int32_t k, uint64_t* out) {
    if (k < 0 || static_cast<size_t>(k) >= gBlocks.size()) return 0;
    out[0] = Pack(gBlocks[k].address);
    out[1] = gBlocks[k].size;
    return 1;
}

// The address of one of the globals the unit reads: 0 gLLCC480C, 1 gLLCCDB50, 2 gLLCCDB54, 3 gDynamicCellF9BC0, 4 gDynamicCellF9BC4,
// 5 gDynamicCellF9BD4, 6 gDynamicCellF9DA0, 7 gDynamicCellUV, 8 gTPGlobals, 9 gDynamicCellTexturePath, 10 gDynamicCellTextureEntries, 11 gGame.
uint64_t tpw_dm_global(int32_t which) {
    switch (which) {
    case 0: return Pack(&gLLCC480C);
    case 1: return Pack(&gLLCCDB50);
    case 2: return Pack(&gLLCCDB54);
    case 3: return Pack(&gDynamicCellF9BC0);
    case 4: return Pack(&gDynamicCellF9BC4);
    case 5: return Pack(&gDynamicCellF9BD4);
    case 6: return Pack(&gDynamicCellF9DA0);
    case 7: return Pack(gDynamicCellUV);
    case 8: return Pack(&gTPGlobals);
    case 9: return Pack(gDynamicCellTexturePath);
    case 10: return Pack(gDynamicCellTextureEntries);
    case 11: return Pack(&gGame);
    }
    return 0;
}

// The theme name CGame::GetCurrentThemeName answers.
void tpw_dm_set_theme(const char* name) { gGame.mCurrentThemeName = name; }

// The offsets and sizes of the native records, in the order the module lists them (see LAYOUT in the module).
int32_t tpw_dm_layout(int64_t* out) {
    int32_t n = 0;
#define OFF(type, member) out[n++] = static_cast<int64_t>(__builtin_offsetof(type, member))
    OFF(tag_sDMeshSet, mF00); OFF(tag_sDMeshSet, mCount); OFF(tag_sDMeshSet, mF06); OFF(tag_sDMeshSet, mEntries);
    OFF(tag_sDMeshSet, mMatrix); OFF(tag_sDMeshSet, mVertexSets); OFF(tag_sDMeshSet, mPoints); OFF(tag_sDMeshSet, mUVSets);
    OFF(tag_sDMeshSet, mFaces); OFF(tag_sDMeshSet, mBlock20); OFF(tag_sDMeshSet, mMesh); OFF(tag_sDMeshSet, mTextureCount);
    OFF(tag_sDMeshSet, mVertexCount); OFF(tag_sDMeshSet, mFaceSetCount); OFF(tag_sDMeshSet, mFaceCount); OFF(tag_sDMeshSet, mUVCount);
    OFF(tag_sDMeshSet, mPointCount); OFF(tag_sDMeshSet, mAllocation); OFF(tag_sDMeshSet, mF38); OFF(tag_sDMeshSet, mShared);
    out[n++] = sizeof(tag_sDMeshSet);
    OFF(sDTextureSet, mFlags); OFF(sDTextureSet, mCount); OFF(sDTextureSet, mFolder); OFF(sDTextureSet, mSharedFolder);
    OFF(sDTextureSet, mEntries); OFF(sDTextureSet, mInfos); OFF(sDTextureSet, mTextures);
    out[n++] = static_cast<int64_t>(__builtin_offsetof(sDTextureSet, mUse)) + static_cast<int64_t>(__builtin_offsetof(sDMeshShared, mInstances));
    OFF(sDTextureSet, mUse);
    out[n++] = sizeof(sDTextureSet);
    OFF(sDTextureEntry, mFlags); OFF(sDTextureEntry, mName);
    out[n++] = sizeof(sDTextureEntry);
    OFF(sDMeshEntry, mFlags); OFF(sDMeshEntry, mFaceCount); OFF(sDMeshEntry, mVertexCount); OFF(sDMeshEntry, mUVCount);
    OFF(sDMeshEntry, mFaceSetCount); OFF(sDMeshEntry, mPointCount); OFF(sDMeshEntry, mF0E); OFF(sDMeshEntry, mBox);
    OFF(sDMeshEntry, mMatrix); OFF(sDMeshEntry, mFaceSets);
    out[n++] = sizeof(sDMeshEntry);
    OFF(CDynamicMesh, mType); OFF(CDynamicMesh, mTextures); OFF(CDynamicMesh, mItem); OFF(CDynamicMesh, mMeshSet);
    OFF(CDynamicMesh, mFaceCursor); OFF(CDynamicMesh, mFaceCount); OFF(CDynamicMesh, mVertexCount); OFF(CDynamicMesh, mMaxSquares);
    out[n++] = sizeof(CDynamicMesh);
    OFF(tag_sMesh, mHeader);
    out[n++] = sizeof(tag_sMesh);
#undef OFF
    return n;
}

// ---- the functions
int32_t tpw_dm_get_mode() { return LLCC_SystemGetMode(); }

void tpw_dm_bounding_box(CDynamicMesh* mesh, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f) {
    mesh->SetBoundingBox(FromBits(a), FromBits(b), FromBits(c), FromBits(d), FromBits(e), FromBits(f));
}

int32_t tpw_dm_enable(tag_sDMeshSet* set) { return DMEnableMeshRender(set); }

int32_t tpw_dm_enable_xz(tag_sDMeshSet* set, uint32_t from, uint32_t to) {
    return DMEnableMeshRender(set, HFieldXZ{static_cast<uint16_t>(from >> 16), static_cast<uint16_t>(from)},
                              HFieldXZ{static_cast<uint16_t>(to >> 16), static_cast<uint16_t>(to)});
}

// mode 1: the Mac's positive flag (delete, which frees the object), 0: the destructor alone.
void tpw_dm_destroy(CDynamicMesh* mesh, int32_t mode) {
    if (mode == 1) {
        for (size_t i = 0; i < gNative.size(); ++i)
            if (gNative[i] == mesh) gNative.erase(gNative.begin() + static_cast<std::ptrdiff_t>(i));
        delete mesh;
    } else {
        mesh->~CDynamicMesh();
    }
}

void tpw_dm_correct(sDTextureSet* textures, tag_sDMeshSet* set) { CorrectDTexturePaths(textures, set); }

void tpw_dm_vertex(CDynamicMesh* mesh, uint32_t x, uint32_t y, uint32_t z, uint32_t u, uint32_t v, int32_t follow, s3DPoint* point) {
    mesh->DC_SetVertex(FromBits(x), FromBits(y), FromBits(z), FromBits(u), FromBits(v), follow, point);
}

void tpw_dm_square(CDynamicMesh* mesh, uint32_t x, uint32_t y, uint32_t z, uint32_t side, uint32_t rotation, int32_t texture, uint32_t f4,
                   int32_t large, uint32_t u10, int32_t keep) {
    mesh->DC_CreateSquareFace(FromBits(x), FromBits(y), FromBits(z), static_cast<uint8_t>(side), static_cast<uint8_t>(rotation), texture,
                              FromBits(f4), large, static_cast<uint8_t>(u10), keep);
}

int32_t tpw_dm_create(tag_sDMeshSet* set, sDTextureSet* textures, const char* name, uint32_t flags) {
    return DMCreateMesh(set, textures, name, flags);
}

// The constructor over `memory` (the module fills it with 0xa5 first and gives it a label).
void tpw_dm_construct(void* memory, uint32_t sizeA, uint32_t sizeB, uint32_t* ok, const char* name, int32_t type, uint32_t a, uint32_t b,
                      uint32_t c, uint32_t d, uint32_t e, uint32_t f) {
    new (memory) CDynamicMesh(static_cast<uint16_t>(sizeA), static_cast<uint16_t>(sizeB), ok, name, static_cast<DYN_Type>(type), FromBits(a),
                              FromBits(b), FromBits(c), FromBits(d), FromBits(e), FromBits(f));
}

}  // extern "C"
