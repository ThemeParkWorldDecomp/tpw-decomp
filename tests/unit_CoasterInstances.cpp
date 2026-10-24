// Test library for the coaster instances (unit w186; libtpw_unit_CoasterInstances.dylib, tests/test_CoasterInstances.py), main app:
//   src/game/rides/CoasterPylon.cpp, src/game/rides/CoasterType.cpp, src/game/rides/TPCoaster.cpp   SetCurrentEditCoaster, the pylon interval three, FlagGenerateTrack, XZAngleBetweenPylons, the pylon area
//                                 words, DestroyEndPylons, EndTrack, SetCoasterState, IsCoasterType, IsCoaster, the finders and setters,
//                                 InstanceCoaster, ui_GetNumCoasterTracks (and gCoasterInstances, gCoasterInstanceCount, the four words)
//   src/game/rides/CoasterType.cpp, src/game/rides/TPCoaster.cpp        ~CCoasterBalance
//
// Every callee outside the unit is a fake that records its arguments (floats as bits, objects as labels) and answers from the test's queues;
// the ones that run while a loop of the unit runs can also change the world (the edit coaster, the trains' base and count, a brace's next)
// to show which words the original reads again. The globals of other files the unit reads are defined here, zero at start like the Mac's bss.
// `grep -n "<<FILL" tests/unit_CoasterInstances.cpp` prints nothing.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "CoasterBalance.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

// ---- globals of other files, defined here (the program's definitions are in files this library does not compile)
tag_sCoasterInstance* gCoasterEditInstance;
tag_sCoasterType* gCoasterTypes[kMaxCoasterTypes];
uint32_t gCoasterTypeCount;
uint32_t gObjectTableCount;
uint32_t gObjectTableFirstFree;
uint32_t gObjectTableLowest;
void* gObjectTable[0x100];
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances + 1];
tag_sRideAnimSet* gRideTypes[kMaxRideTypes + 1];
CActionRecorder gActionRecorder;

namespace {

void SwitchEdit(const char* key) {
    if (ret(key) != 0 && gCoasterEditInstance != nullptr) gCoasterEditInstance = gCoasterEditInstance->mNext;
}

// The trains' fakes: the word the original reads, then (by the case) another base or another count, to show which words are read again.
void TrainFake(const char* name, const char* swap, const char* count, tag_sCoasterInstance* instance, sTrainInstance* train, int64_t extra) {
    const uint32_t word0 = train->mFlags;
    ev(name, {lab(instance), lab(train), extra, instance->mState, fbits(instance->mF2C), word0, instance->mFlags});
    if (ret(swap) != 0) instance->mTrains = reinterpret_cast<sTrainInstance*>(instance->mUVOffsets);
    const int64_t newCount = ret(count, -1);
    if (newCount >= 0) instance->mTrainCount = static_cast<int32_t>(newCount);
}

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
void RecalcTrackParams(tag_sCoasterInstance* instance) {
    ev("RecalcTrackParams", {lab(instance), instance->mFlags, lab(gCoasterEditInstance)});
    SwitchEdit("Edit:Recalc");
}
void DeleteTrackBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace) {
    ev("DeleteTrackBrace", {lab(instance), lab(brace), instance->mBlock3C});
    // The brace is destroyed: its next word is lost (the case's spare brace stands in for what a freed block holds).
    if (ret("Poison") != 0) brace->mNext = static_cast<tag_sTrackBrace*>(instance->mCObject);
}
void SetCoasterEditState(uint32_t state) {
    ev("SetCoasterEditState", {state, lab(gCoasterEditInstance)});
    SwitchEdit("Edit:State");
}
void CoasterPanelEndTrack() {
    ev("CoasterPanelEndTrack", {lab(gCoasterEditInstance)});
    SwitchEdit("Edit:Panel");
}
void PlaceEndPylons(tag_sCoasterInstance* instance, float height) {
    ev("PlaceEndPylons", {lab(instance), fbits(height), lab(gCoasterEditInstance)});
    SwitchEdit("Edit:Place");
}
void KickPeepsFromEntranceQueue(tag_sCoasterInstance* instance) { ev("KickPeepsFromEntranceQueue", {lab(instance), instance->mState}); }
void UnloadTrain(tag_sCoasterInstance* instance, sTrainInstance* train, int32_t flag) {
    TrainFake("UnloadTrain", "Swap:Unload", "Count:Unload", instance, train, flag);
}
void ActivateCoasterTrain(tag_sCoasterInstance* instance, sTrainInstance* train) {
    TrainFake("ActivateCoasterTrain", "Swap:Activate", "Count:Activate", instance, train, 0);
}
void DeactivateCoasterTrain(tag_sCoasterInstance* instance, sTrainInstance* train) {
    TrainFake("DeactivateCoasterTrain", "Swap:Deactivate", "Count:Deactivate", instance, train, 0);
}
void FlashEndPylons(tag_sCoasterInstance* instance, uint32_t mode) { ev("FlashEndPylons", {lab(instance), mode, instance->mFlags}); }
void SetTrainCapacity(tag_sCoasterInstance* instance, uint32_t a, uint32_t b, uint32_t c) {
    ev("SetTrainCapacity", {lab(instance), a, b, c});
}
void CoasterSetSpeed(tag_sCoasterInstance* instance, uint32_t speed, int32_t flag) {
    ev("CoasterSetSpeed", {lab(instance), speed, flag, instance->mState, lab(gCoasterEditInstance)});
}
void PlaceEntrancePylons(tag_sCoasterInstance* instance, uint32_t x, uint32_t z, float height, int32_t flag) {
    ev("PlaceEntrancePylons", {lab(instance), x, z, fbits(height), flag});
}
void InstanceCoasterTrains(tag_sCoasterType* type, tag_sCoasterInstance* instance, int32_t flag) {
    ev("InstanceCoasterTrains", {lab(type), lab(instance), flag});
}
void RestoreCoasterSounds(tag_sCoasterInstance* instance) { ev("RestoreCoasterSounds", {lab(instance)}); }
uint32_t AllocateObjectInstanceToTable(uint32_t& count, uint32_t& firstFree, uint32_t& lowest, uint32_t flags, void** table, void* object,
                                       uint32_t extra) {
    const int64_t places = (&count == &gObjectTableCount ? 1 : 0) | (&firstFree == &gObjectTableFirstFree ? 2 : 0) |
                           (&lowest == &gObjectTableLowest ? 4 : 0) | (table == gObjectTable ? 8 : 0);
    ev("AllocateObjectInstanceToTable", {places, flags, lab(object), extra});
    return static_cast<uint32_t>(ret("Alloc", 7));
}
void AllocateQueue(sWraparoundQueue* queue, uint32_t capacity) {
    // The queue is told by its place in the newest instance (gCoasterInstances' head): 0 for the first queue, 1 for the second.
    const int64_t which = queue == &gCoasterInstances->mQueueB0 ? 0 : queue == &gCoasterInstances->mQueueD0 ? 1 : -1;
    ev("AllocateQueue", {lab(gCoasterInstances), which, capacity});
}
// The requested (host) size is told as the Mac's for the instance record.
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {size == sizeof(tag_sCoasterInstance) ? 0x14c : size, flags});
    void* block = tpw::ArenaAllocZeroed(size);
    newlab(block);
    return block;
}
void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}
void CBalanceTable::Init() {}
void CBalanceTable::UnInit() { ev("CBalanceTable::UnInit", {lab(this)}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_ci_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"instances", &gCoasterInstances},
        {"count", &gCoasterInstanceCount},
        {"edit", &gCoasterEditInstance},
        {"typecount", &gCoasterTypeCount},
        {"types", gCoasterTypes},
        {"rideanim", gRideAnimSets},
        {"ridetypes", gRideTypes},
        {"w689C", &gCoasterWord689C},
        {"w6898", &gCoasterWord6898},
        {"w6894", &gCoasterWord6894},
        {"w6890", &gCoasterWord6890},
        {"recording", &gActionRecorder.mRecording},
        {"otcount", &gObjectTableCount},
        {"otfree", &gObjectTableFirstFree},
        {"otlowest", &gObjectTableLowest},
        {"otable", gObjectTable},
        {"arenabase", reinterpret_cast<const void*>(tpw::gArenaBase)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_ci_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

#define OFF(T, m) static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<T*>(0x1000)->m) - 0x1000)

// The offsets and sizes the module relies on (HOST_LAYOUT), struct by struct in the module's order.
int32_t tpw_ci_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = OFF(tag_sCoasterInstance, mFlags);
    out[n++] = OFF(tag_sCoasterInstance, mType);
    out[n++] = OFF(tag_sCoasterInstance, mState);
    out[n++] = OFF(tag_sCoasterInstance, mTypeID);
    out[n++] = OFF(tag_sCoasterInstance, mEntranceID);
    out[n++] = OFF(tag_sCoasterInstance, mObjectInstanceID);
    out[n++] = OFF(tag_sCoasterInstance, mNext);
    out[n++] = OFF(tag_sCoasterInstance, mX);
    out[n++] = OFF(tag_sCoasterInstance, mZ);
    out[n++] = OFF(tag_sCoasterInstance, mScriptID);
    out[n++] = OFF(tag_sCoasterInstance, mCObject);
    out[n++] = OFF(tag_sCoasterInstance, mF2C);
    out[n++] = OFF(tag_sCoasterInstance, mBlock3C);
    out[n++] = OFF(tag_sCoasterInstance, mBraces);
    out[n++] = OFF(tag_sCoasterInstance, mFAC);
    out[n++] = OFF(tag_sCoasterInstance, mTrainCount);
    out[n++] = OFF(tag_sCoasterInstance, mF118);
    out[n++] = OFF(tag_sCoasterInstance, mF11C);
    out[n++] = OFF(tag_sCoasterInstance, mTrains);
    out[n++] = OFF(tag_sCoasterInstance, mUVOffsets);
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = OFF(tag_sTrackBrace, mFlags);
    out[n++] = OFF(tag_sTrackBrace, mNext);
    out[n++] = OFF(tag_sTrackBrace, mData);
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    out[n++] = OFF(sTrackBraceData, mFlags);
    out[n++] = static_cast<int64_t>(sizeof(sTrackBraceData));
    out[n++] = OFF(tag_sCoasterType, mFlags);
    out[n++] = OFF(tag_sCoasterType, mRideTypeNumber);
    out[n++] = OFF(tag_sCoasterType, mTextureCount);
    out[n++] = OFF(tag_sCoasterType, mFA4);
    out[n++] = OFF(tag_sCoasterType, mF2F4);
    out[n++] = OFF(tag_sCoasterType, mF2F8);
    out[n++] = OFF(tag_sCoasterType, mF318);
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterType));
    out[n++] = OFF(tag_sRideAnimSet, mFlags);
    out[n++] = OFF(tag_sRideAnimSet, mHeads);
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = OFF(tag_sMesh, mHeader);
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    out[n++] = OFF(tag_sM3D2Header, mF78);
    out[n++] = static_cast<int64_t>(sizeof(tag_sM3D2Header));
    out[n++] = OFF(tag_sNode, mMatrix.m[3][1]);
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    out[n++] = OFF(sPylonControl, mFBC);
    out[n++] = OFF(sPylonControl, mFC0);
    out[n++] = static_cast<int64_t>(sizeof(sPylonControl));
    out[n++] = OFF(sTrainInstance, mFlags);
    out[n++] = static_cast<int64_t>(sizeof(sTrainInstance));
    out[n++] = static_cast<int64_t>(sizeof(CCoasterBalance));
    return n;
}

// 1 when the unit is built with the port fixes on (unit coaster64): sTrackBraceData is then the host pylon record's view, so its size is
// that record's.
int32_t tpw_ci_portfix() {
    return 0;
}

// ---- the functions, one wrapper each with C types only (floats as bits out, pointers as uint64_t)
void tpw_ci_SetCurrentEditCoaster(uint64_t instance) { SetCurrentEditCoaster(reinterpret_cast<tag_sCoasterInstance*>(instance)); }
uint32_t tpw_ci_Interval(int32_t which, float value, uint64_t pylon) {
    auto* p = reinterpret_cast<sPylonControl*>(pylon);
    return fbits(which == 0 ? GetIntervalForHeight(value, p) : which == 1 ? GetIntervalForHeightDelta(value, p) : GetHeightForInterval(value, p));
}
void tpw_ci_FlagGenerateTrack(uint64_t instance, int32_t flag) { FlagGenerateTrack(reinterpret_cast<tag_sCoasterInstance*>(instance), flag); }
void tpw_ci_FlagCollision(uint64_t brace) { FlagCollisionUpdateTrackAroundBrace(reinterpret_cast<tag_sTrackBrace*>(brace)); }
uint32_t tpw_ci_XZAngle(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f) {
    return fbits(XZAngleBetweenPylons(a, b, c, d, e, f));
}
// The four answers go to four words of the caller's block.
void tpw_ci_Potential(uint64_t block) {
    auto* w = reinterpret_cast<uint32_t*>(block);
    GetPotentialPylonPositionArea(w, w + 1, w + 2, w + 3);
}
void tpw_ci_DestroyEndPylons(uint64_t instance) { DestroyEndPylons(reinterpret_cast<tag_sCoasterInstance*>(instance)); }
void tpw_ci_EndTrack(int32_t flag) { EndTrack(flag); }
void tpw_ci_SetCoasterState(uint64_t instance, uint32_t state) { SetCoasterState(reinterpret_cast<tag_sCoasterInstance*>(instance), state); }
int32_t tpw_ci_IsCoasterType(uint32_t type) { return IsCoasterType(type); }
int32_t tpw_ci_IsCoaster(uint32_t instance) { return IsCoaster(instance); }
uint64_t tpw_ci_Find(uint32_t entrance) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(FindCoasterInstanceFromEntranceID(entrance))); }
void tpw_ci_SetCObject(uint32_t entrance, uint64_t object) { SetCoasterCObject(entrance, reinterpret_cast<void*>(object)); }
void tpw_ci_SetScriptID(uint32_t entrance, int32_t id) { SetCoasterScriptID(entrance, id); }
int32_t tpw_ci_NumTracks() { return ui_GetNumCoasterTracks(); }
uint32_t tpw_ci_InstanceCoaster(uint32_t type, uint32_t entrance, uint32_t x, uint32_t z, int32_t flag, uint64_t object) {
    return InstanceCoaster(type, entrance, x, z, flag, reinterpret_cast<void*>(object));
}

// ---- CCoasterBalance: a heap block (label 7) deleted by the destructor call when the flag's low half is positive
void* tpw_ci_balance_new() {
    void* p = ::operator new(sizeof(CCoasterBalance));
    std::memset(p, 0xa5, sizeof(CCoasterBalance));
    tpw_cl_label(7, p);
    return p;
}
// The answer is the object's label (0 for NULL, 7 for the block), taken before the delete forgets it.
int32_t tpw_ci_balance_dtor(void* object, int32_t flag) {
    auto* o = static_cast<CCoasterBalance*>(object);
    const int32_t label = lab(o);
    if (o != nullptr) {
        if (static_cast<int16_t>(flag) > 0) delete o;
        else o->~CCoasterBalance();
    }
    return label;
}

}  // extern "C"
