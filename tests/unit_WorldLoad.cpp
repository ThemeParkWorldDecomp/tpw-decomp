// Test library for the world's load (unit w207; libtpw_unit_WorldLoad.dylib, tests/test_WorldLoad.py), main app:
//   src/game/Thing.cpp, src/game/World.cpp, src/game/rides/rsse.cpp       CWorld::Load, LOADVAR<CWorld::WorldState>, LOADVAR<CThing::ThingModel>, CThing::ForceThingId
//   src/game/ControlManager.cpp, src/game/Object.cpp, src/game/advisor/Advisor.cpp, src/game/park/BankAccount.cpp, src/game/park/analyser.cpp, src/game/rides/FlyingRide.cpp, src/game/save/FS_MasterSave.cpp  the basic LOADVAR instances (REAL on both sides: the file is the fake)
//
// The fake file: LbFile_Read serves the bytes the case gives (little-endian, as the game's files are) from a cursor and answers from its queues
// ("LbFile_Read" the result, "LbFile_Read:xfer" the transferred count). Every other callee of Load is a fake that records what the real one
// reads: the twenty Serialise functions (the world's three parts as offsets from the world, the things as the label of their block), InitThingHeap
// and ReconstructFreeList (the table stays as the case set it), the stand-ins CThoughts::CThoughts, Pilot::Pilot and CThing::CThing (the label of
// the block they build in and the offset in it), the two sprite switches, GAME_ASSERTMSG and the serialise log. Load makes the things with the
// plain operator new: the call log's operator new fills each block with 0xa5, logs its size and gives it a label (100 and up, in order), and this
// file keeps the blocks of the case so that the module can read them back. The world is one static block of the Mac's size over the fill 0xa5,
// the thing table and its heads are the unit's own globals, and two small static blocks stand for the things earlier rows pointed at.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#pragma clang diagnostic ignored "-Winvalid-offsetof"   // tpw_wld_layout takes member offsets of CWorld, which is not a standard-layout class

namespace {
void ObserveAllocation(std::size_t size, bool array, void* block);
}  // namespace
#define TPW_CALLLOG_EVENTS 20000
#define TPW_CALLLOG_LABELS 512
#define TPW_CALLLOG_QUEUE_VALUES 16500
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#include "calllog.cpp"
#include "platform/arena.h"

#include "AdvisorQueue.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_MasterSave.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// The unit makes its things on the game heap (the Mac's operator new), which the host takes from the arena (unit v5L31). Here the arena is
// the call log's operator new and delete, so a thing's block is made, filled, labelled and freed as the Mac's is.
namespace tpw {
void* ArenaAlloc(size_t size) { return ::operator new(size); }
bool InArena(const void* address) { return address != nullptr; }
void ArenaFree(void* block) { ::operator delete(block); }
}  // namespace tpw

// ---- globals of other files the unit reads or writes, defined here (the program's definitions are in files this library does not compile)
CSaveLoad gSaveLoad;
CThingArrayEntry gThingArray[kThingArraySize];
CThingArrayEntry* gUsedThingHead;
CThingArrayEntry* gFreeThingHead;

namespace {

constexpr int32_t kMaxAllocations = 64;
constexpr std::size_t kStaleSize = 0x20;
constexpr int64_t kLabelStale = 10;   // the two static blocks are labelled 10 and 11 (the module's LABEL_STALE)

alignas(16) unsigned char gWorldBytes[sizeof(CWorld)];
alignas(16) unsigned char gStale[2][kStaleSize];
char gFileHandle;   // the fake file's address (a label for the log)

// The file the case serves and the read position.
std::vector<uint8_t> gFileData;
std::size_t gCursor;

// The blocks the call log's operator new made during the case: base, size and label.
struct Allocation {
    unsigned char* base;
    std::size_t size;
    int64_t label;
};
Allocation gAllocs[kMaxAllocations];
int32_t gAllocCount;

// The table as the case set it, for the list of rows the call changed.
CThingArrayEntry gSnapTable[kThingArraySize];

CWorld* World() { return reinterpret_cast<CWorld*>(gWorldBytes); }

template <class T>
T* Launder(T* pointer) {
    __asm__ volatile("" : "+r"(pointer));
    return pointer;
}

// A world member's offset from the world, else 0x40000000 plus the label of the object (the things' blocks).
int64_t Where(const void* p) {
    const uintptr_t a = reinterpret_cast<uintptr_t>(p);
    const uintptr_t b = reinterpret_cast<uintptr_t>(gWorldBytes);
    if (a >= b && a - b < sizeof(CWorld)) return static_cast<int64_t>(a - b);
    return 0x40000000 + lab(p);
}

// The label of the block of this case that holds p (-1 when none does) and p's offset in it (0 when none does).
int64_t BlockLabel(const void* p) {
    const unsigned char* a = static_cast<const unsigned char*>(p);
    for (int32_t k = 0; k < gAllocCount; ++k)
        if (a >= gAllocs[k].base && a < gAllocs[k].base + gAllocs[k].size) return gAllocs[k].label;
    return -1;
}
int64_t BlockOffset(const void* p) {
    const unsigned char* a = static_cast<const unsigned char*>(p);
    for (int32_t k = 0; k < gAllocCount; ++k)
        if (a >= gAllocs[k].base && a < gAllocs[k].base + gAllocs[k].size) return static_cast<int64_t>(a - gAllocs[k].base);
    return 0;
}

// What a table row points at: 0 for NULL, the stale blocks' labels, the label of a block this case made, else -1.
int64_t ThingLabel(const void* p) {
    if (p == nullptr) return 0;
    for (int32_t k = 0; k < 2; ++k)
        if (p == gStale[k]) return kLabelStale + k;
    for (int32_t k = 0; k < gAllocCount; ++k)
        if (p == gAllocs[k].base) return gAllocs[k].label;
    return -1;
}

// A table pointer as the row plus one (0 for NULL, -1 for anything outside the table).
int64_t RowPlusOne(const CThingArrayEntry* p) {
    if (p == nullptr) return 0;
    if (p < gThingArray || p >= gThingArray + kThingArraySize) return -1;
    return static_cast<int64_t>(p - gThingArray) + 1;
}

// The call log's operator new calls this after it made a block: during a case the block is filled with 0xa5, logged and kept.
void ObserveAllocation(std::size_t size, bool, void* block) {
    if (!on()) return;
    std::memset(block, 0xa5, size);
    const int64_t label = newlab(block);
    ev("new", {static_cast<int64_t>(size), label});
    if (gAllocCount < kMaxAllocations) gAllocs[gAllocCount++] = {static_cast<unsigned char*>(block), size, label};
}

}  // namespace

// ---- the fakes
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("CSaveLoad::WriteSerialiseLog", {lab(this), size, state}, {name});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// The file: the bytes from the cursor. The count the call reports is what it took, unless the queue forces another one; the buffer gets the
// bytes (zero filled behind the file's end) only when the count is the length and the result is not negative.
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), length});
    const std::size_t available = gCursor < gFileData.size() ? gFileData.size() - gCursor : 0;
    const uint32_t taken = static_cast<uint32_t>(available < length ? available : length);
    uint32_t count = taken;
    const int64_t forced = ret("LbFile_Read:xfer", -1);
    if (forced >= 0) count = static_cast<uint32_t>(forced);
    *transferred = count;
    const int32_t result = static_cast<int32_t>(ret("LbFile_Read"));
    if (count == length && result >= 0) {
        std::memset(buffer, 0, length);
        if (taken != 0) std::memcpy(buffer, gFileData.data() + gCursor, taken);
    }
    gCursor += taken;
    return TbError(result);
}

// Serialise_basic.cpp holds the SAVEVAR instances as well and they call this; Load never reaches it.
TbError LbFile_Write(void* file, const void*, uint32_t, uint32_t* transferred) {
    ev("LbFile_Write", {lab(file)});
    *transferred = 0;
    return TbError(-1);
}

// The table is reset by the module's case (the fake leaves it as the case set it); the free list is not relinked either.
void CWorld::InitThingHeap() { ev("CWorld::InitThingHeap", {Where(this)}); }
void CWorld::ReconstructFreeList() { ev("CWorld::ReconstructFreeList", {Where(this)}); }

void ESP_DisableSpriteDraw() { ev("ESP_DisableSpriteDraw"); }
void ESP_EnableSpriteDraw() { ev("ESP_EnableSpriteDraw"); }

// The parts Load builds in place: the stand-ins write nothing and log where they were called (the block's label and the offset in the block).
CThoughts::CThoughts() { ev("CThoughts::CThoughts", {BlockLabel(this), BlockOffset(this)}); }
CThing::CThing(const CSerialise&) { ev("CThing::CThing", {BlockLabel(this), BlockOffset(this)}); }

// Pilot::Pilot is written by hand under the constructor's own symbol: a compiled `Pilot::Pilot() {}` would first run Vehicle's implicit constructor, which
// zeroes the Vector2d members (mPos, mVelocity, mForward, mSide, mF38, mF40, mDestination, mWaypoints), while the Mac's stand-in (a bare return) leaves
// the bytes as they were.
extern "C" void tpw_wld_pilot_ctor(Pilot* self) __asm__("__ZN5PilotC1Ev");
extern "C" void tpw_wld_pilot_ctor(Pilot* self) { ev("Pilot::Pilot", {BlockLabel(self), BlockOffset(self)}); }

// The twenty Serialise functions Load reaches (the first three are the world's parts, the rest the things').
#define TPW_SERIALISE_FAKE(Class)                                                                          \
    int32_t Class::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {                 \
        ev(#Class "::Serialise", {Where(this), lab(file), save, version, unused});                        \
        return static_cast<int32_t>(ret(#Class "::Serialise"));                                           \
    }
TPW_SERIALISE_FAKE(CControlManager)
TPW_SERIALISE_FAKE(CMacroAI)
TPW_SERIALISE_FAKE(CMap)
TPW_SERIALISE_FAKE(CPerson)
TPW_SERIALISE_FAKE(CObject)
TPW_SERIALISE_FAKE(CMechanic)
TPW_SERIALISE_FAKE(CHandyman)
TPW_SERIALISE_FAKE(CEntertainer)
TPW_SERIALISE_FAKE(CGuard)
TPW_SERIALISE_FAKE(CResearcher)
TPW_SERIALISE_FAKE(CStaffHQ)
TPW_SERIALISE_FAKE(CMechanicHQ)
TPW_SERIALISE_FAKE(CAdvisor)
TPW_SERIALISE_FAKE(CTagSystem)
TPW_SERIALISE_FAKE(CParkAnalyser)
TPW_SERIALISE_FAKE(CResearchLab)
TPW_SERIALISE_FAKE(CWeather)
TPW_SERIALISE_FAKE(CBankAccount)
TPW_SERIALISE_FAKE(CUIMsgReceiver)
TPW_SERIALISE_FAKE(CChallengeHQ)

extern "C" {

// The host address of an object by the module's name for it (the labels' keys).
uint64_t tpw_wld_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"file", &gFileHandle},
        {"gSaveLoad", &gSaveLoad},
        {"stale0", gStale[0]},
        {"stale1", gStale[1]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The offsets and sizes the module relies on (HOST_LAYOUT): the world's size, the 27 members of its tail, the map and control manager
// offsets, the thing kind's offset and the table's size.
int32_t tpw_wld_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CWorld));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mStartTime));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA710));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mPersonCount));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mStaffHQThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMechanicHQThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mAdvisorThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTagSystemThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkAnalyserThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mResearchLabThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mWeatherThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mBankAccountThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mUIMsgReceiverThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mArrivalVehicleThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mBusThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mSeaplaneThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mFerryThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mGatesThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mLightsThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mChallengeHQThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA738));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA73C));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA73E));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA740));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA742));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA744));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA746));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(World()->GetMap()) - reinterpret_cast<uintptr_t>(World()));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(World()->GetControlManager()) - reinterpret_cast<uintptr_t>(World()));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    out[n++] = static_cast<int64_t>(kThingArraySize);
    return n;
}

// ---- the world, the table, the file
uint64_t tpw_wld_world() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gWorldBytes)); }

// One row of the table: the thing (0 none, 1 or 2 one of the stale blocks), the index word, and the next used, previous used and next free
// rows (row plus one, 0 for none). Every row is whole: nothing in it is left from an earlier case (tpw_wld_reset).
void tpw_wld_row(int32_t row, int32_t thing, uint32_t index, int32_t nextPlusOne, int32_t prevPlusOne, int32_t freePlusOne) {
    CThingArrayEntry& entry = gThingArray[row];
    entry.mThing = thing != 0 ? static_cast<void*>(gStale[thing - 1]) : nullptr;
    entry.mF04[0] = index;
    entry.mF04[1] = entry.mF04[2] = entry.mF04[3] = 0;
    entry.mNextUsed = nextPlusOne != 0 ? &gThingArray[nextPlusOne - 1] : nullptr;
    entry.mPrevUsed = prevPlusOne != 0 ? &gThingArray[prevPlusOne - 1] : nullptr;
    entry.mNextFree = freePlusOne != 0 ? &gThingArray[freePlusOne - 1] : nullptr;
}

// The state every case starts from: the blocks of the last case freed, the world and the stale blocks over the fill 0xa5, the table in its
// default (no thing, the row's own index, no links), the heads NULL and no file.
void tpw_wld_reset() {
    for (int32_t k = 0; k < gAllocCount; ++k) std::free(gAllocs[k].base);
    gAllocCount = 0;
    std::memset(gWorldBytes, 0xa5, sizeof(gWorldBytes));
    std::memset(gStale, 0xa5, sizeof(gStale));
    for (int32_t row = 0; row < kThingArraySize; ++row) tpw_wld_row(row, 0, static_cast<uint32_t>(row), 0, 0, 0);
    gUsedThingHead = nullptr;
    gFreeThingHead = nullptr;
    gFileData.clear();
    gCursor = 0;
}

void tpw_wld_heads(int32_t usedPlusOne, int32_t freePlusOne) {
    gUsedThingHead = usedPlusOne != 0 ? &gThingArray[usedPlusOne - 1] : nullptr;
    gFreeThingHead = freePlusOne != 0 ? &gThingArray[freePlusOne - 1] : nullptr;
}
void tpw_wld_get_heads(int64_t* out) {
    out[0] = RowPlusOne(gUsedThingHead);
    out[1] = RowPlusOne(gFreeThingHead);
}

void tpw_wld_set_file(const uint8_t* data, int32_t size) {
    gFileData.assign(data, data + size);
    gCursor = 0;
}

// The snapshot of the table, and the rows that differ from it: six values for each (row, thing label, index word, next used, previous used and
// next free as row plus one). Answers how many rows differ; the first `capacity` are written.
void tpw_wld_snapshot() { std::memcpy(gSnapTable, gThingArray, sizeof(gThingArray)); }
int32_t tpw_wld_changed(int64_t* out, int32_t capacity) {
    int32_t n = 0;
    for (int32_t row = 0; row < kThingArraySize; ++row) {
        if (std::memcmp(&gThingArray[row], &gSnapTable[row], sizeof(CThingArrayEntry)) == 0) continue;
        if (n < capacity) {
            const CThingArrayEntry& entry = gThingArray[row];
            out[6 * n + 0] = row;
            out[6 * n + 1] = ThingLabel(entry.mThing);
            out[6 * n + 2] = entry.mF04[0];
            out[6 * n + 3] = RowPlusOne(entry.mNextUsed);
            out[6 * n + 4] = RowPlusOne(entry.mPrevUsed);
            out[6 * n + 5] = RowPlusOne(entry.mNextFree);
        }
        ++n;
    }
    return n;
}

// The blocks Load made (base, size, label), in the order of the allocations.
int32_t tpw_wld_alloc_count() { return gAllocCount; }
void tpw_wld_alloc_info(int32_t k, uint64_t* out) {
    out[0] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gAllocs[k].base));
    out[1] = static_cast<uint64_t>(gAllocs[k].size);
    out[2] = static_cast<uint64_t>(gAllocs[k].label);
}

// ---- the functions, one wrapper each with C types only
int32_t tpw_wld_load() { return Launder(World())->Load(&gFileHandle); }

// LOADVAR of the world state (0) or the thing model (1): the value is read from and written back to *value.
int32_t tpw_wld_loadvar(int32_t which, uint32_t* value, const char* name, int32_t noLog) {
    if (which == 0) {
        CWorld::WorldState copy = static_cast<CWorld::WorldState>(*value);
        const int32_t answer = LOADVAR<CWorld::WorldState>(&gFileHandle, &copy, name, noLog);
        *value = static_cast<uint32_t>(copy);
        return answer;
    }
    CThing::ThingModel copy = static_cast<CThing::ThingModel>(*value);
    const int32_t answer = LOADVAR<CThing::ThingModel>(&gFileHandle, &copy, name, noLog);
    *value = static_cast<uint32_t>(copy);
    return answer;
}

// CThing::ForceThingId on the block at `block` (one of the stale blocks).
void tpw_wld_forceid(uint64_t block, uint32_t id) {
    CThing* const thing = reinterpret_cast<CThing*>(static_cast<uintptr_t>(block));
    thing->ForceThingId(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(id)});
}

}  // extern "C"
