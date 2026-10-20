// Test library for HACKRestoreCoasterQueues, NormaliseVectorXZ, DisableCoasterCarDrawSafe and IncCoasterConst1 (unit v51023;
// libtpw_unit_HACKRestoreCoasterQueues.dylib, tests/test_HACKRestoreCoasterQueues.py), main app:
//   src/LoadCoasters_HACKRestoreCoasterQueues.cpp
//
// The callees are recording fakes; the globals of other files are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "analyser.h"
#include "m3d2.h"
#include "rsse.h"
#include "ui_iterators.h"
#include "keytable_actions.h"

using namespace calllog;

tag_sCoasterInstance* gCoasterInstances;
tag_sCoasterInstance* gCoasterEditInstance;
tag_sRideAnimSet* gRideAnimSets[64];
uint64_t gIterHead;

namespace {
uint32_t U32At(const void* base, size_t off) { return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(base) + off); }
uint16_t U16At(const void* base, size_t off) { return *reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(base) + off); }
}  // namespace

// ---- the fakes (marker words: mesh +0x80, set +0x300, same on both sides)
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    uint16_t id = U16At(this, 0);
    ev("GetThingId", {id});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = id;
    return result;
}

RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script) {
    ev("GetScript", {static_cast<uint32_t>(script)});
    return reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(ret("Script") & 0xffffffffu));
}

int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index) {
    ev("Getvar", {static_cast<uint32_t>(reinterpret_cast<uintptr_t>(script)), static_cast<uint32_t>(index)});
    return static_cast<int32_t>(ret(index == 0 ? "Var0" : "Var1"));
}

CThing* UI_IterateThingNext(UIIteratorTypes type, CThing* thing) {
    uint64_t next = thing == nullptr ? gIterHead : *reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(thing) + 0x2f0);
    ev("Iter", {static_cast<uint32_t>(type), thing == nullptr ? 0xffffffffu : U16At(thing, 0)});
    return reinterpret_cast<CThing*>(static_cast<uintptr_t>(next));
}

void HACKCoasterReturnLostPeep(tag_sCoasterInstance* instance, uint32_t peep) {
    ev("ReturnLostPeep", {instance->mEntranceID, peep});
}

void DisableMeshDraw(tag_sMesh* mesh) { ev("DisableMeshDraw", {U32At(mesh, 0x80)}); }

void DisableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) {
    ev("DisableMeshDraw2", {U32At(mesh, 0x80), static_cast<uint32_t>((from.mF00 << 16) | from.mF02), static_cast<uint32_t>((to.mF00 << 16) | to.mF02)});
}

int32_t FindNthDataNodeIndex(tag_sRideAnimSet* set, uint32_t kind, uint32_t n) {
    ev("FindNth", {U32At(set, 0x300), kind, n});
    return static_cast<int32_t>(ret("Find"));
}

void DisableRideHead(tag_sMesh* mesh, uint32_t index) { ev("DisableRideHead", {U32At(mesh, 0x80), index}); }

void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) {
    ev("FlagGenerateTrack", {instance->mEntranceID, static_cast<uint32_t>(flag)});
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    double a = va_arg(args, double);
    double b = va_arg(args, double);
    va_end(args);
    uint64_t abits, bbits;
    std::memcpy(&abits, &a, 8);
    std::memcpy(&bbits, &b, 8);
    uint32_t sum = 0, len = 0;
    for (const char* p = format; *p; ++p) {
        sum += static_cast<uint8_t>(*p);
        ++len;
    }
    ev("CTRACE", {level, sum, len, static_cast<uint32_t>(abits >> 32), static_cast<uint32_t>(abits), static_cast<uint32_t>(bbits >> 32),
                  static_cast<uint32_t>(bbits)});
}

#define OFF(T, M) static_cast<int64_t>(__builtin_offsetof(T, M))

extern "C" {

uint64_t tpw_hrcq_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gCoasterInstances", &gCoasterInstances},
        {"gCoasterEditInstance", &gCoasterEditInstance},
        {"gRideAnimSets", &gRideAnimSets[0]},
        {"gIterHead", &gIterHead},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_hrcq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets, in the order of the module's LAYOUT_NAMES.
int32_t tpw_hrcq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = OFF(tag_sCoasterInstance, mEntranceID);
    out[n++] = OFF(tag_sCoasterInstance, mNext);
    out[n++] = OFF(tag_sCoasterInstance, mScriptID);
    out[n++] = OFF(tag_sCoasterInstance, mCObject);
    out[n++] = OFF(tag_sCoasterInstance, mTrainCount);
    out[n++] = OFF(tag_sCoasterInstance, mTrains);
    out[n++] = OFF(tag_sCoasterInstance, mType);
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = OFF(sTrainInstance, mCarsInUse);
    out[n++] = OFF(sTrainInstance, mCars);
    out[n++] = static_cast<int64_t>(sizeof(sTrainInstance));
    out[n++] = OFF(sCarInstance, mCurrentA);
    out[n++] = OFF(sCarInstance, mCountA);
    out[n++] = OFF(sCarInstance, mCurrentB);
    out[n++] = OFF(sCarInstance, mCountB);
    out[n++] = static_cast<int64_t>(sizeof(sCarInstance));
    out[n++] = OFF(tag_sRideAnimSet, mFlags);
    out[n++] = OFF(tag_sRideAnimSet, mHeads);
    out[n++] = OFF(tag_sRideAnimSet, mFC0);
    out[n++] = OFF(tag_sRideAnimSet, mFC4);
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = OFF(tag_sMesh, mF28);
    out[n++] = OFF(sEmbeddedSet, mRecords);
    out[n++] = OFF(sEmbeddedRecord, mFlags);
    out[n++] = static_cast<int64_t>(sizeof(sEmbeddedRecord));
    return n;
}

void tpw_hrcq_hack(uint32_t instance) { HACKRestoreCoasterQueues(instance); }
void tpw_hrcq_norm(uint64_t point) { NormaliseVectorXZ(*reinterpret_cast<s3DPoint*>(static_cast<uintptr_t>(point))); }
void tpw_hrcq_disable(uint32_t ride) { DisableCoasterCarDrawSafe(ride); }
int32_t tpw_hrcq_inc() { return IncCoasterConst1(); }

}  // extern "C"
