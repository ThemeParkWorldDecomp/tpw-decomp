// Test library for LoadCoasters (unit v5372; libtpw_unit_LoadCoasters.dylib, tests/test_LoadCoasters.py), main app:
//   src/LoadCoasters.cpp   LoadCoasters
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; LbFile_Read hands out
// the case's file bytes. The globals of other files the unit reads, defined here, are zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
uint32_t gPylonStackSkip;
void* gCoasterCurrentData;
tag_sCoasterInstance* gCoasterEditInstance;
tag_sCoasterInstance* gCoasterInstances;
void* gObjectTable[0x100];
uint32_t gObjectTableLowest;
uint32_t gObjectTableFirstFree;
uint32_t gObjectTableCount;
tag_sRideAnimSet* gRideAnimSets[64];
CActionRecorder gActionRecorder;

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

const uint8_t* gFileData = nullptr;
uint32_t gFileLen = 0;
uint32_t gFilePos = 0;

}  // namespace

// ---- the swaps (TPCoaster_trains.cpp): no-ops on a little-endian host
namespace TPCoasterSwap {
void USwapInplace16UShort(uint16_t&) {}
void USwapInplace32Int(int32_t&) {}
void USwapInplace32ULong(uint32_t&) {}
void USwapInplaceFloat(float&) {}
}  // namespace TPCoasterSwap

// ---- the fakes
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), static_cast<int64_t>(length)});
    uint32_t n = gFileLen - gFilePos;
    if (n > length) n = length;
    if (n) std::memcpy(buffer, gFileData + gFilePos, n);
    gFilePos += n;
    *transferred = n;
    return TbError(0);
}

namespace SamsUtilities {
void UDebug(const char* text, ...) { ev("UDebug", {}, {text}); }
}  // namespace SamsUtilities

void InstanceCoasterTrains(tag_sCoasterType* type, tag_sCoasterInstance* instance, int32_t flag) {
    ev("InstanceCoasterTrains", {lab(type), lab(instance), flag});
}
void PlaceEntrancePylons(tag_sCoasterInstance* instance, uint32_t x, uint32_t z, float height, int32_t flag) {
    ev("PlaceEntrancePylons", {lab(instance), x, z, CanonBits(height), flag});
}
uint32_t AllocateObjectInstanceToTable(uint32_t& count, uint32_t& firstFree, uint32_t& lowest, uint32_t flags, void** table, void* object,
                                       uint32_t extra) {
    ev("AllocateObjectInstanceToTable", {count, firstFree, lowest, flags, table == gObjectTable ? 1 : 0, lab(object), extra});
    return static_cast<uint32_t>(ret("Alloc"));
}
int32_t SetPlaceLoop() {
    ev("SetPlaceLoop", {});
    return 1;
}
int32_t PlacePylon(uint32_t x, uint32_t z, int32_t flag, tag_sPylon** outPylon, tag_sTrackBrace** outBrace, uint8_t level, uint32_t extra) {
    ev("PlacePylon", {x, z, flag, level, extra});
    *outPylon = reinterpret_cast<tag_sPylon*>(static_cast<uintptr_t>(ret("PlacePylonP")));
    *outBrace = reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(ret("PlacePylonB")));
    return 1;
}
void SetLoftPylon(float value, uint32_t id, int32_t a, int32_t b) { ev("SetLoftPylon", {CanonBits(value), id, a, b}); }
void SetRotatePylon(float value, uint32_t id, int32_t setFlag, int32_t b) { ev("SetRotatePylon", {CanonBits(value), id, setFlag, b}); }
void SetWobblePylon(float x, float z, uint32_t id, int32_t setFlags, int32_t b) {
    ev("SetWobblePylon", {CanonBits(x), CanonBits(z), id, setFlags, b});
}
void RecalcTrackParams(tag_sCoasterInstance* instance) { ev("RecalcTrackParams", {lab(instance)}); }
void UpdatePylonStackMeshes(sPylonControl* control, tag_sPylon* pylon, int32_t a, int32_t b) {
    ev("UpdatePylonStackMeshes", {lab(control), lab(pylon), a, b});
}
void PlaceEndPylons(tag_sCoasterInstance* instance, float height) { ev("PlaceEndPylons", {lab(instance), CanonBits(height)}); }
void SetCoasterEditState(uint32_t state) { ev("SetCoasterEditState", {state}); }
void SetCoasterState(tag_sCoasterInstance* instance, uint32_t state) { ev("SetCoasterState", {lab(instance), state}); }
tag_sTrackBrace* GetTrackBrace(sCoasterTrackParams* params, uint32_t id) {
    const void* instance = reinterpret_cast<const uint8_t*>(params) - offsetof(tag_sCoasterInstance, mBlock3C);
    ev("GetTrackBrace", {lab(instance), id});
    return reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(ret("GetTrackBrace")));
}
int32_t LoadTrainsAndCars(void* file, tag_sCoasterInstance* instance) {
    ev("LoadTrainsAndCars", {lab(file), lab(instance)});
    return 1;
}
void SetTrainCapacity(tag_sCoasterInstance* instance, uint32_t a, uint32_t b, uint32_t c) { ev("SetTrainCapacity", {lab(instance), a, b, c}); }
void ClearCoasterButtons() { ev("ClearCoasterButtons", {}); }
void FlashEndPylons(tag_sCoasterInstance* instance, uint32_t mode) { ev("FlashEndPylons", {lab(instance), mode}); }

extern "C" {

uint64_t tpw_LC_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"skip", &gPylonStackSkip},
        {"cur", &gCoasterCurrentData},
        {"edit", &gCoasterEditInstance},
        {"list", &gCoasterInstances},
        {"table", gObjectTable},
        {"lowest", &gObjectTableLowest},
        {"first", &gObjectTableFirstFree},
        {"count", &gObjectTableCount},
        {"sets", gRideAnimSets},
        {"rec", &gActionRecorder.mRecording},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_LC_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_LC_arenabase() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// The host offsets of the Mac members the module writes, in the order of the module's table.
int32_t tpw_LC_layout(int64_t* out) {
    int32_t n = 0;
#define O(type, member) out[n++] = static_cast<int64_t>(offsetof(type, member))
    // instance
    O(tag_sCoasterInstance, mFlags);
    O(tag_sCoasterInstance, mType);
    O(tag_sCoasterInstance, mState);
    O(tag_sCoasterInstance, mEntranceID);
    O(tag_sCoasterInstance, mObjectInstanceID);
    O(tag_sCoasterInstance, mNext);
    O(tag_sCoasterInstance, mF2C);
    O(tag_sCoasterInstance, mBlock3C);
    O(tag_sCoasterInstance, mPylons);
    O(tag_sCoasterInstance, mLastBrace);
    O(tag_sCoasterInstance, mBraces);
    O(tag_sCoasterInstance, mPreviewBrace);
    O(tag_sCoasterInstance, mBrace64);
    O(tag_sCoasterInstance, mFF0);
    O(tag_sCoasterInstance, mTrainCount);
    O(tag_sCoasterInstance, mF140);
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    // brace
    O(tag_sTrackBrace, mNumber);
    O(tag_sTrackBrace, mFlags);
    O(tag_sTrackBrace, mPrev);
    O(tag_sTrackBrace, mFB0);
    O(tag_sTrackBrace, mFB4);
    O(tag_sTrackBrace, mF70);
    O(tag_sTrackBrace, mF90);
    O(tag_sTrackBrace, mF74);
    O(tag_sTrackBrace, mData);
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    // pylon list node
    O(sCoasterPylon, mFlags);
    O(sCoasterPylon, mControl);
    O(sCoasterPylon, mNext);
    O(sCoasterPylon, mCount);
    O(sCoasterPylon, mBraces);
    out[n++] = static_cast<int64_t>(sizeof(sCoasterPylon));
    // pylon data
    O(sTrackBraceData, mF18Table);
    out[n++] = static_cast<int64_t>(sizeof(sTrackBraceData));
    // type
    O(tag_sCoasterType, mF2F4);
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterType));
    // ride set, mesh, header
    O(tag_sRideAnimSet, mHeads);
    O(tag_sMesh, mHeader);
    O(tag_sM3D2Header, mF78);
#undef O
    return n;
}

void tpw_LC_setfile(uint64_t data, uint32_t length) {
    gFileData = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(data));
    gFileLen = length;
    gFilePos = 0;
}

int32_t tpw_LC_run(uint64_t file) { return LoadCoasters(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }

}  // extern "C"
