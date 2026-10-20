// Test library for the pylon edit helpers (unit v5893; libtpw_unit_DeletePylon.dylib, tests/test_DeletePylon.py), main app:
//   src/DisableCurrentPylonEditState_SetCoasterEditState_DeletePylon.cpp   RemoveTrackBraceFromPylon, DeletePylon, MoveBrace
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of other
// files the unit reads are defined here (zero at start, like the Mac's bss).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"

using namespace calllog;

// ---- globals of other files, defined here
tag_sRideAnimSet* gRideAnimSets[8];
tag_sM3D2Header* gHeightfieldHeader;
int32_t gHeightfieldWidth;
int32_t gHeightfieldHeight;
tag_sPylon* gPylonMoveOrigData;
float gPylonMoveHeight;

namespace {

sPylonRecord* gFakeNewPylon;
sFixedRideParams gFakeParams;

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float FloatOf(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, sizeof f);
    return f;
}

int64_t I(uint32_t v) { return static_cast<int64_t>(v); }
int64_t I(int32_t v) { return static_cast<int64_t>(v); }
int64_t P(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
void Destroy3DObjectFromAIWorld(uint32_t a, uint32_t b) { ev("D3D", {I(a), I(b)}); }

void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}

void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) { ev("FlagGenerateTrack", {lab(instance), I(flag)}); }

void Assign3DObjectToAIWorld(void* instance, uint32_t a, uint32_t b, uint32_t c, int32_t d) {
    ev("Assign3D", {lab(instance), I(a), I(b), I(c), I(d)});
}

void UpdatePylonStackStructures(sPylonControl* control, tag_sPylon* pylon) { ev("UpdStruct", {lab(control), lab(pylon)}); }

void UpdatePylonStackMeshes(sPylonControl* control, tag_sPylon* pylon, int32_t a, int32_t b) {
    ev("UpdMeshes", {lab(control), lab(pylon), I(a), I(b)});
}

sFixedRideParams* GetTempFixedRideParams(int32_t a, int32_t b, int32_t x, int32_t z) {
    ev("GetTempParams", {I(a), I(b), I(x), I(z)});
    gFakeParams.mF0C = reinterpret_cast<void*>(static_cast<uintptr_t>(0xabc0));
    return &gFakeParams;
}

void MoveRide(tag_sRideAnimSet* set, int32_t x, int32_t z, void* params, int32_t a, int32_t b, uint32_t c) {
    ev("MoveRide", {lab(set), I(x), I(z), P(params), I(a), I(b), I(c)});
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t value = va_arg(args, int32_t);
    va_end(args);
    int64_t sum = 0;
    for (int i = 0; format[i] != 0; ++i) sum += static_cast<int64_t>(i + 1) * static_cast<uint8_t>(format[i]);
    ev("CTRACE", {I(level), I(value), sum});
}

int32_t DetermineLinkLevel(float height, sLinkPylon* pylon, uint32_t* level, float* out, int32_t quiet) {
    ev("DetermineLinkLevel", {CanonBits(height), lab(pylon), I(quiet)});
    *level = static_cast<uint32_t>(ret("DLLlevel"));
    *out = FloatOf(ret("DLLout"));
    return static_cast<int32_t>(ret("DLL"));
}

void InsertBraceIntoPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, sLinkPylon* pylon, uint32_t level, float height, float* out,
                          int32_t a, int32_t b, int32_t c, int32_t d) {
    ev("InsertBrace", {lab(instance), lab(brace), lab(pylon), I(level), CanonBits(height), P(out), I(a), I(b), I(c), I(d)});
}

sPylonRecord* NewPylon(uint32_t x, uint32_t z, uint32_t flags, int32_t unused, sPylonControl* control, s3DPoint* point) {
    ev("NewPylon", {I(x), I(z), I(flags), I(unused), lab(control), lab(point)});
    return gFakeNewPylon;
}

void FlagCollisionUpdateTrackAroundBrace(tag_sTrackBrace* brace) { ev("FlagCollision", {lab(brace)}); }

void DoMeshSkirtDeform(tag_sMesh* mesh, tag_sM3D2Header* header, float y, uint32_t a) {
    ev("SkirtDeform", {lab(mesh), lab(header), CanonBits(y), I(a)});
}

int32_t SquishPylonBracesIfRequired(tag_sCoasterInstance* instance, sPylonControl* control, sLinkPylon* pylon) {
    ev("Squish", {lab(instance), lab(control), lab(pylon)});
    return static_cast<int32_t>(ret("SQ"));
}

float CalculateBraceHeight(tag_sTrackBrace* brace) {
    ev("CalcBraceHeight", {lab(brace)});
    return FloatOf(ret("CBH"));
}

float GetIntervalForHeight(float height, sPylonControl* control) {
    ev("GetInterval", {CanonBits(height), lab(control)});
    return FloatOf(ret("GIH"));
}

void SetupBraceAnimFrameData(sCoasterType* type, tag_sTrackBrace* brace) { ev("SetupBrace", {lab(type), lab(brace)}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_pyl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"sets", &gRideAnimSets[0]},          {"hfhdr", &gHeightfieldHeader},   {"width", &gHeightfieldWidth},
        {"height", &gHeightfieldHeight},      {"orig", &gPylonMoveOrigData},    {"moveheight", &gPylonMoveHeight},
        {"newpylon", &gFakeNewPylon},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pyl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_pyl_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// The host offsets of the fields the module writes by Mac offset, in the module's LAYOUT order.
int32_t tpw_pyl_layout(int64_t* out) {
    int32_t n = 0;
#define OFS(T, M) out[n++] = static_cast<int64_t>(offsetof(T, M))
    OFS(tag_sCoasterInstance, mType);
    OFS(tag_sCoasterInstance, mF40);
    OFS(tag_sCoasterInstance, mPylons);
    OFS(tag_sCoasterInstance, mPylonTail);
    OFS(sPylonRecord, mNumber);
    OFS(sPylonRecord, mFlags);
    OFS(sPylonRecord, mIndex);
    OFS(sPylonRecord, mCellX);
    OFS(sPylonRecord, mCellZ);
    OFS(sPylonRecord, mControl);
    OFS(sPylonRecord, mX);
    OFS(sPylonRecord, mY);
    OFS(sPylonRecord, mZ);
    OFS(sPylonRecord, mNext);
    OFS(sPylonRecord, mPrev);
    OFS(sPylonRecord, mCount);
    OFS(sPylonRecord, mBraces);
    OFS(tag_sTrackBrace, mNumber);
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mF00) + 4);
    OFS(tag_sTrackBrace, mFlags);
    OFS(tag_sTrackBrace, mWord70);
    OFS(tag_sTrackBrace, mFE8w);
    OFS(tag_sTrackBrace, mData);
    OFS(sPylonControl, mFB4);
    OFS(sPylonControl, mFB8);
    OFS(sPylonControl, mFC4);
    OFS(tag_sRideAnimSet, mHeads);
    OFS(tag_sMesh, mHeader);
    OFS(tag_sM3D2Header, mFlags);
    OFS(tag_sM3D2Header, mHField);
    OFS(tag_sM3D2Header, mF78);
    OFS(sHField, mF18);
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mMatrix) + 4 * 13);
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    out[n++] = static_cast<int64_t>(sizeof(sPylonControl));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(sizeof(tag_sM3D2Header));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
#undef OFS
    return n;
}

// ---- the functions, one wrapper each with C types only
void tpw_pyl_rem(uint64_t instance, uint64_t brace) {
    RemoveTrackBraceFromPylon(reinterpret_cast<tag_sCoasterInstance*>(instance), reinterpret_cast<tag_sTrackBrace*>(brace));
}

void tpw_pyl_del(uint64_t instance, uint64_t pylon, int32_t flag) {
    DeletePylon(reinterpret_cast<tag_sCoasterInstance*>(instance), reinterpret_cast<tag_sPylon*>(pylon), flag);
}

void tpw_pyl_move(uint64_t instance, uint64_t brace, uint64_t pylon, uint32_t x, uint32_t z, uint64_t a, uint64_t b, int32_t c, int32_t d) {
    MoveBrace(reinterpret_cast<tag_sCoasterInstance*>(instance), reinterpret_cast<tag_sTrackBrace*>(brace), reinterpret_cast<tag_sPylon*>(pylon), x, z,
              reinterpret_cast<float*>(a), reinterpret_cast<float*>(b), c, d);
}

}  // extern "C"
