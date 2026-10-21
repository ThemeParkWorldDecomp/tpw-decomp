// Test library for the dynamic cell meshes in the frame (unit w120; libtpw_unit_DynamicCells.dylib, tests/test_DynamicCells.py), main app:
//   src/game/park/DynamicCell.cpp   DC_TickDynamicCellMeshes, CDynamicMesh::DrawAll, CDynamicMesh::DC_CreatePolyFace (and the three arrays it defines)
//   src/game/park/LLCellMouse.cpp   LLCCM_CreateQMesh, LLCCM_CreateKTrackMesh, LLCCM_CreateWTrackMesh, LLCCM_CreateHoardingMesh
//
// Every callee outside the two files is a fake that records the same event on both sides: CClock::GetPauseFlag, CVariableTime::GetTimeRate,
// CDynamicMesh::DC_CreateSquareFace and DC_SetVertex (both advance the mesh's counters by the amounts the case's queues say), HDZ_RotatePointZ
// (rewrites the corner rows and may touch the two scroll lags), DC_AnimateWater, DC_DrawWater (may move the water's start position),
// DC_AnimateFlag, DC_DrawFlag, DC_ResetSquareFaceListPtr, sort (may replace the keys), CWorld::GetWeather (answers one of eight case-made
// blocks), InstanceRide, RestoreRideInstance, SetCurRideAnim and CTRACE. Floats are recorded as bit patterns with every NaN made one
// value. A pointer into a registered region is recorded as (label << 32) + offset, any other object by its label.
//
// The test module (Python) builds every world on both sides from one description, so this file holds the fakes, the globals the unit reads
// (their owners are not in the library), raw memory for the module, the offsets of the mesh object, and one entry point per function.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "CellBase.h"
#include "Clock.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "LLCellSystem.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "m3d2.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files, defined here (zero at start like the Mac's bss, as the unit's functions expect)
float gDynamicCellUV[10][3];
float gDynamicCellF9BC0;
float gDynamicCellF9BC4;
int32_t gLLCCDB50;
int32_t gLLCCDB54;
int32_t gDynamicCellF9D94;
float gDynamicCellF9D98;
float gDynamicCellF9D9C;
float gDynamicCellF9DA0;
int32_t gDynamicCellFBDBC;
uint8_t gDynamicCellFlag = 1;
CDynamicMesh* gBlueprintMesh;
CDynamicMesh* gWaterMesh;
CDynamicMesh* gFlagsMesh;
s3DPoint gWaterStartPos;
float gCameraF5BF4;
CVariableTime::CVariableTime() {}
CFixedTime::CFixedTime() {}
CClock gClock;
CWorld* gGameWorld;
sLLCMMesh gLLCMQMeshes[8];
sLLCMMesh gLLCMKTrackMeshes[20];
sLLCMMesh gLLCMWTrackMeshes[25];
sLLCMMesh gLLCMHoardingMeshes[28];
int32_t gLLCMQSize[2];
int32_t gLLCMTrackSize[2];
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances];

namespace {

// Regions of memory the module registers: a pointer into one is recorded as (label << 32) + (its offset from the region's base, which
// may lie below the registered start: the sort call passes the key array less one word).
struct Region {
    const uint8_t* base;
    size_t size;
    int32_t label;
};
std::vector<Region> gRegions;
std::vector<void*> gBlocks;
CWeather* gWeather[8];

int64_t Tag(const void* pointer) {
    if (pointer == nullptr) return 0;
    const uint8_t* address = static_cast<const uint8_t*>(pointer);
    for (const Region& region : gRegions)
        if (address >= region.base && address < region.base + region.size)
            return (static_cast<int64_t>(region.label) << 32) + static_cast<int64_t>(address - region.base);
    return lab(pointer);
}

uint32_t Fb(float value) {
    const uint32_t bits = fbits(value);
    return (bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0 ? 0x7fc00000u : bits;
}

float FromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

// The queue value -1 means "leave alone" for the fakes that may write.
constexpr int64_t kLeave = -1;

}  // namespace

// ---- the fakes
int32_t CClock::GetPauseFlag() {
    ev("GetPauseFlag", {lab(this)});
    return static_cast<int32_t>(ret("pause", 0));
}

// The queue "rate" holds the answer's 64-bit pattern.
double CVariableTime::GetTimeRate() {
    ev("GetTimeRate", {lab(this)});
    const int64_t bits = ret("rate", 0);
    double value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

// Both mesh fakes record the arguments and then the counters they were called with, and advance the counters by the case's amounts (the
// square's queues are "sqv" vertices, "sqf" the face count and "sqc" the cursor, in faces).
void CDynamicMesh::DC_CreateSquareFace(float x, float y, float z, uint8_t side, uint8_t rotation, int32_t texture, float unused1, int32_t large,
                                       uint8_t unused2, int32_t keepPosition) {
    ev("DC_CreateSquareFace", {lab(this), Fb(x), Fb(y), Fb(z), side, rotation, texture, Fb(unused1), large, unused2, keepPosition});
    ev("SquareState", {mVertexCount, mFaceCount, Tag(mFaceCursor)});
    const uint32_t vertices = static_cast<uint32_t>(ret("sqv", 4));
    const uint32_t faces = static_cast<uint32_t>(ret("sqf", 2));
    const int64_t cursor = ret("sqc", 2);
    mVertexCount = mVertexCount + vertices;
    mFaceCount = mFaceCount + faces;
    mFaceCursor = mFaceCursor + cursor;
}

void CDynamicMesh::DC_SetVertex(float x, float y, float z, float u, float v, int32_t followTerrain, s3DPoint* point) {
    ev("DC_SetVertex", {lab(this), Fb(x), Fb(y), Fb(z), Fb(u), Fb(v), followTerrain, Tag(point), mVertexCount});
    mVertexCount = mVertexCount + static_cast<uint32_t>(ret("vtx", 1));
}

// Rewrites the x and y of rows 5 to 8 from the queue "rot" (eight values, -1 leaves one alone) and may set the two scroll lags.
void HDZ_RotatePointZ(s3DPoint* points, float angle) {
    ev("HDZ_RotatePointZ", {Tag(points), Fb(angle)});
    float* rows = reinterpret_cast<float*>(points);
    for (int32_t k = 0; k < 8; ++k) {
        const int64_t bits = ret("rot", kLeave);
        if (bits != kLeave) rows[(k / 2) * 3 + (k % 2)] = FromBits(static_cast<uint32_t>(bits));
    }
    const int64_t lagZ = ret("rotbc0", kLeave);
    if (lagZ != kLeave) gDynamicCellF9BC0 = FromBits(static_cast<uint32_t>(lagZ));
    const int64_t lagX = ret("rotbc4", kLeave);
    if (lagX != kLeave) gDynamicCellF9BC4 = FromBits(static_cast<uint32_t>(lagX));
}

void DC_AnimateWater(int32_t a, int32_t b, float c, float d) { ev("DC_AnimateWater", {a, b, Fb(c), Fb(d)}); }

// May move the water's start position (queues "wsx", "wsy", "wsz", one value a call, -1 leaves it).
void DC_DrawWater(float x, float y, float z, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f) {
    ev("DC_DrawWater", {Fb(x), Fb(y), Fb(z), a, b, c, d, e, f});
    const int64_t wx = ret("wsx", kLeave);
    const int64_t wy = ret("wsy", kLeave);
    const int64_t wz = ret("wsz", kLeave);
    if (wx != kLeave) gWaterStartPos.x = FromBits(static_cast<uint32_t>(wx));
    if (wy != kLeave) gWaterStartPos.y = FromBits(static_cast<uint32_t>(wy));
    if (wz != kLeave) gWaterStartPos.z = FromBits(static_cast<uint32_t>(wz));
}

void DC_AnimateFlag(int32_t a, int32_t b, float c, float d) { ev("DC_AnimateFlag", {a, b, Fb(c), Fb(d)}); }

void DC_DrawFlag(float x, float y, float z, float w, int32_t a, int32_t b, int32_t texture, float angle) {
    ev("DC_DrawFlag", {Fb(x), Fb(y), Fb(z), Fb(w), a, b, texture, Fb(angle)});
}

// (With the record count, which the real one clears.)
void DC_ResetSquareFaceListPtr() { ev("DC_ResetSquareFaceListPtr", {gDynamicCellFBDBC}); }

// May replace the keys (queue "sortk": the new words from key 1 on, ended by -1).
void sort(uint32_t* keysMinusOne, uint32_t count) {
    ev("sort", {Tag(keysMinusOne), count});
    for (uint32_t n = 1;; ++n) {
        const int64_t word = ret("sortk", kLeave);
        if (word == kLeave) break;
        keysMinusOne[n] = static_cast<uint32_t>(word);
    }
}

// Answers the block the queue "weather" names (0 to 7).
CWeather* CWorld::GetWeather() {
    ev("GetWeather", {lab(this)});
    return gWeather[ret("weather", 0) & 7];
}

// The ride parameters live on the caller's stack, so the fakes record their contents.
static void RecordParams(const sFixedRideParams* params) {
    ev("RideParams", {params->mX, params->mZ, params->mRotation, Tag(params->mF0C)});
}

uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c, int32_t d, int32_t e, uint32_t f,
                      int32_t g) {
    ev("InstanceRide", {type, flags, a, b, Tag(c), d, e, f, g});
    RecordParams(params);
    return static_cast<uint32_t>(ret("inst", 1));
}

uint32_t RestoreRideInstance(uint32_t instance, uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, void* c, int32_t d, int32_t e,
                             int32_t f) {
    ev("RestoreRideInstance", {instance, type, flags, a, Tag(c), d, e, f});
    RecordParams(params);
    return static_cast<uint32_t>(ret("restore", 0));
}

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), a, anim, loop, Fb(speed), b});
}

void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {level}, {format}); }

extern "C" {

// ---- memory for the module
void tpw_dc_reset() {
    for (void* block : gBlocks) std::free(block);
    gBlocks.clear();
    gRegions.clear();
    for (CWeather*& block : gWeather) block = nullptr;
}

// A block of `size` bytes filled with `fill`; a label puts it in the call log's table.
uint64_t tpw_dc_alloc(uint64_t size, uint32_t fill, int32_t label) {
    void* block = std::malloc(size != 0 ? size : 1);
    gBlocks.push_back(block);
    std::memset(block, static_cast<int>(fill), size);
    if (label > 0) tpw_cl_label(label, block);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}

// Pointers into [pointer - below, pointer + size) are recorded as (label << 32) + their offset from `pointer`.
void tpw_dc_region(int32_t label, uint64_t pointer, uint64_t size, uint64_t below) {
    gRegions.push_back({reinterpret_cast<const uint8_t*>(pointer) - below, static_cast<size_t>(size + below), label});
}

void tpw_dc_weather(int32_t k, uint64_t block) { gWeather[k & 7] = reinterpret_cast<CWeather*>(block); }

uint64_t tpw_dc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"uv", gDynamicCellUV}, {"bc0", &gDynamicCellF9BC0}, {"bc4", &gDynamicCellF9BC4}, {"db50", &gLLCCDB50}, {"db54", &gLLCCDB54},
        {"d94", &gDynamicCellF9D94}, {"d98", &gDynamicCellF9D98}, {"d9c", &gDynamicCellF9D9C}, {"da0", &gDynamicCellF9DA0},
        {"fbdbc", &gDynamicCellFBDBC}, {"flag", &gDynamicCellFlag}, {"bp", &gBlueprintMesh}, {"water", &gWaterMesh}, {"flags", &gFlagsMesh},
        {"ws", &gWaterStartPos}, {"cam", &gCameraF5BF4}, {"clock", &gClock}, {"world", &gGameWorld}, {"ridesets", gRideAnimSets},
        {"qmesh", gLLCMQMeshes}, {"kmesh", gLLCMKTrackMeshes}, {"wmesh", gLLCMWTrackMeshes}, {"hmesh", gLLCMHoardingMeshes},
        {"qsize", gLLCMQSize}, {"tsize", gLLCMTrackSize}, {"faces", gDynamicFaces}, {"keys", gDynamicFaceKeys}, {"spots", gDynamicFlagSpots},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The offsets and sizes the module needs, in the order of LAYOUT in the module: the mesh object's members, its size, the sFaceSet-free
// records' sizes, then the mesh table entry's members.
int32_t tpw_dc_layout(int64_t* out) {
    int32_t n = 0;
    auto off = [](size_t a, size_t b) { return static_cast<int64_t>(a + b); };
    out[n++] = static_cast<int64_t>(__builtin_offsetof(CDynamicMesh, mType));
    out[n++] = off(__builtin_offsetof(CDynamicMesh, mItem), __builtin_offsetof(sDMeshEntry, mFaceCount));
    out[n++] = off(__builtin_offsetof(CDynamicMesh, mItem), __builtin_offsetof(sDMeshEntry, mVertexCount));
    out[n++] = off(__builtin_offsetof(CDynamicMesh, mItem), __builtin_offsetof(sDMeshEntry, mUVCount));
    out[n++] = off(__builtin_offsetof(CDynamicMesh, mItem), __builtin_offsetof(sDMeshEntry, mBox));
    out[n++] = off(__builtin_offsetof(CDynamicMesh, mItem), __builtin_offsetof(sDMeshEntry, mFaceSets));
    out[n++] = off(__builtin_offsetof(CDynamicMesh, mMeshSet), __builtin_offsetof(tag_sDMeshSet, mFaces));
    out[n++] = static_cast<int64_t>(__builtin_offsetof(CDynamicMesh, mFaceCursor));
    out[n++] = static_cast<int64_t>(__builtin_offsetof(CDynamicMesh, mFaceCount));
    out[n++] = static_cast<int64_t>(__builtin_offsetof(CDynamicMesh, mVertexCount));
    out[n++] = static_cast<int64_t>(__builtin_offsetof(CDynamicMesh, mMaxSquares));
    out[n++] = static_cast<int64_t>(sizeof(CDynamicMesh));
    out[n++] = static_cast<int64_t>(__builtin_offsetof(sLLCMMesh, mRideType));
    out[n++] = static_cast<int64_t>(__builtin_offsetof(sLLCMMesh, mTimeControls));
    out[n++] = static_cast<int64_t>(sizeof(sLLCMMesh));
    return n;
}

// ---- the functions
void tpw_dc_tick() { DC_TickDynamicCellMeshes(); }

void tpw_dc_drawall(CDynamicMesh* mesh) { mesh->DrawAll(); }

void tpw_dc_poly(CDynamicMesh* mesh, s3DPoint* positions, int32_t texture, s3DPoint* uvs, s3DPoint* points) {
    mesh->DC_CreatePolyFace(positions, texture, uvs, points);
}

// which: 0 Q, 1 K, 2 W, 3 Hoarding.
uint32_t tpw_dc_instance(int32_t which, int32_t x, int32_t y, uint32_t kind, uint32_t index, uint32_t rotation, int32_t restore) {
    const CCellBase::CELL_IndexType tile{kind, index, rotation};
    switch (which) {
    case 0: return LLCCM_CreateQMesh(x, y, tile, restore);
    case 1: return LLCCM_CreateKTrackMesh(x, y, tile, restore);
    case 2: return LLCCM_CreateWTrackMesh(x, y, tile, restore);
    case 3: return LLCCM_CreateHoardingMesh(x, y, tile, restore);
    }
    return 0;
}

}  // extern "C"
