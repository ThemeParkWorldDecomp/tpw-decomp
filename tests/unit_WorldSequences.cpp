// Test entry points for the world's sequences (library unit_WorldSequences: src/game/World.cpp, unit w194): CWorld::PopulatePark,
// StartingPopulationForOnline, StartBankruptcySequence, StartWinningSequence, IsPlayingWinningAnim, EndWinningSequence, GetPrevUsedThing,
// SeedRand, ReconstructFreeList, GetNewPersonIndex, FlagItemAsSelected, ConvertDLRidesUsedToIndices, RestoreEngineObjects,
// DeleteEngineObjects and the pending list's RemoveHead and AddHead.
//
// Every callee outside those sixteen is a fake that records the same event as the PowerPC side's stub: the asserts and traces (the condition
// and the format; the by-value string argument of the winning assertion is not logged), the thing virtuals (GetThingId, KillMe, the ride's
// RequestBreakdown and UpdateEngine, the sprite script's KillSpriteScript), the people's constructors, CObject's constructor, the map
// (IsXYValid, the entrance and path queries, the entrance and booth positions), the park analyser, the gates (status and SetParkOpenOrClosed),
// DeletePendingThings, the ride database (first, next, record by id), _wcsicmp, the two windows, DestroyRideInstance, the sprite switch and
// the first person ride stop. The strings are functional (fake_strings.h). operator new logs a block of 0x100 bytes or more (the list nodes of
// 12 bytes here and 10 on the Mac are not compared); operator delete is the call log's and this file keeps every freed block.
//
// The world is raw memory of the Mac's size over the fill 0xa5 (only a few words matter), the thing table and its heads are the unit's own
// globals, the thing objects are blocks of 0x80 bytes. A case is described to the host by a few calls and then one call of the function;
// pointers are reported as 0, a block number or a table row so that the Mac's 32 bit words and the host's pointers compare.
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
bool KeepFreed(void*, bool) { return true; }
void observe_allocation(std::size_t size, bool array, void* block);
}  // namespace
#define TPW_RELEASE_OBSERVER(p, array) KeepFreed(p, array)
#define TPW_ALLOCATION_OBSERVER observe_allocation
#include "calllog.cpp"
#include "platform/arena.h"

#include "Debug.h"
#include "ESP_sprites.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "UIInterface.h"
#include "World.h"
#include "WorldThings.h"
#include "analyser.h"
#include "platform/macdoze.h"

using namespace calllog;

// The unit makes its things on the game heap (the Mac's operator new), which the host takes from the arena (unit v5L31). Here the arena is
// the call log's operator new and delete, so a thing's block is made, filled, labelled and freed as the Mac's is.
namespace tpw {
void* ArenaAlloc(size_t size) { return ::operator new(size); }
bool InArena(const void* address) { return address != nullptr; }
void ArenaFree(void* block) { ::operator delete(block); }
}  // namespace tpw

#define TPW_FAKE_STRINGS_JOIN_WITHOUT_NEW
#include "fake_strings.h"

namespace {

constexpr int32_t kMaxBlocks = 64;
constexpr std::size_t kBlockSize = 0x80;
constexpr int32_t kLabelWorld = 1, kLabelAnalyser = 2, kLabelList = 5, kLabelRides = 6, kLabelUI = 7, kLabelBlock = 10;
constexpr int32_t kMaxRides = 32;

alignas(16) unsigned char gWorldBytes[sizeof(CWorld)];
alignas(16) unsigned char gAnalyserBytes[0x40];
void* gBlocks[kMaxBlocks];       // the thing objects of the case
int32_t gBlockCount;
alignas(16) unsigned char gRideBytes[kMaxRides][sizeof(RideInfo)];
int32_t gRideCount, gRideCursor;
int32_t gValidCount, gEntranceCount, gPathCount;
uint32_t gValid[64], gEntrance[64], gPath[64];   // the 1 based map cells that answer yes
uint32_t gEntrancePos[2], gBoothPos[2];
uint32_t gRidesInPark;

CThingArrayEntry gInitial[kThingArraySize];   // the table as the case started

CWorld* World() { return reinterpret_cast<CWorld*>(gWorldBytes); }
RideInfo* Ride(int32_t k) { return reinterpret_cast<RideInfo*>(gRideBytes[k]); }

template <class T>
T* Launder(T* pointer) {
    __asm__ volatile("" : "+r"(pointer));
    return pointer;
}

int64_t BlockOf(const void* pointer) {
    if (pointer == nullptr) return 0;
    for (int32_t i = 0; i < gBlockCount; ++i)
        if (gBlocks[i] == pointer) return i + 1;
    return -1;
}

int64_t RowOf(const CThingArrayEntry* entry) {
    if (entry == nullptr) return 0;
    if (entry < gThingArray || entry >= gThingArray + kThingArraySize) return -1;
    return (entry - gThingArray) + 1;
}

const CThingArrayEntry* EntryOf(uint32_t rowPlusOne) { return rowPlusOne == 0 ? nullptr : &gThingArray[rowPlusOne - 1]; }

bool In(const uint32_t* set, int32_t n, uint32_t cell) {
    for (int32_t i = 0; i < n; ++i)
        if (set[i] == cell) return true;
    return false;
}

uint32_t CellOf(const CMapCell* cell) {
    return static_cast<uint32_t>(cell - World()->mMapCells) + 1;
}

void observe_allocation(std::size_t size, bool, void* block) {
    if (!on()) return;
    std::memset(block, 0xa5, size);
    if (size >= 0x100) ev("new", {static_cast<int64_t>(size)});
}

}  // namespace

// ---- The unit's globals (the launcher gets zero definitions) -----------------------------------------------------------------------
CThingArrayEntry gThingArray[kThingArraySize];
CThingArrayEntry* gUsedThingHead;
CThingArrayEntry* gFreeThingHead;
TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>> gPendingDeleteList;
CRideDatabase gRideDatabase;
UIInterface gUIInterface;
uint8_t gParkRequiredRides[0x20];

// ---- The fakes ------------------------------------------------------------------------------------------------------------------------
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// The level, the first variadic integer when the text takes a %d, and the text.
void CTRACE(uint32_t level, const char* format, ...) {
    int64_t argument = 0;
    if (std::strstr(format, "%d") != nullptr) {
        va_list list;
        va_start(list, format);
        argument = static_cast<uint32_t>(va_arg(list, int));
        va_end(list);
    }
    ev("CTRACE", {level, argument}, {format});
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    const CThing* self = Launder(this);
    ev("GetThingId", {lab(self)});
    return {self != nullptr ? self->mThingId : uint16_t(0)};
}

void CThing::KillMe() { ev("KillMe", {lab(this)}); }

void CObject::RequestBreakdown() { ev("RequestBreakdown", {lab(this)}); }
void CObject::UpdateEngine(int32_t flag) { ev("UpdateEngine", {lab(this), flag}); }
// The sprite script is the three words at +0xc of the thing: the label is the thing's.
void CDelayedScript::KillSpriteScript() { ev("KillSpriteScript", {lab(reinterpret_cast<const unsigned char*>(this) - 0xc)}); }
void DestroyRideInstance(uint32_t instance) { ev("DestroyRideInstance", {instance}); }
void StopRidingRideIn1stPerson() { ev("StopRidingRideIn1stPerson"); }
void ESP_DisableSpriteDraw() { ev("ESP_DisableSpriteDraw"); }
void UIInterface::Bankrupt(CObject* gates) { ev("Bankrupt", {lab(this), lab(gates)}); }
void UIInterface::Congratulations(CObject* feature) { ev("Congratulations", {lab(this), lab(feature)}); }

// The object's constructor writes the thing's own id (the queue "id").
CObject::CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> map, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t a,
                 int32_t b) {
    ev("CObject", {newlab(this), map.mIndex, type.mIndex, a, b});
    mBase.mThingId = static_cast<uint16_t>(ret("id", 0x1234));
}

#define PERSON_CTOR(cls)                                                                \
    cls::cls(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {                     \
        ev(#cls, {newlab(this), cell.mIndex});                                          \
    }
PERSON_CTOR(CPerson)
PERSON_CTOR(CMechanic)
PERSON_CTOR(CHandyman)
PERSON_CTOR(CEntertainer)

// The map. The valid, entrance and path cells are the case's lists; IsXYValid is not logged (it runs 16384 times).
int32_t CMap::IsXYValid(int32_t x, int32_t y) { return In(gValid, gValidCount, static_cast<uint32_t>(x + y * 0x80 + 1)) ? 1 : 0; }
int32_t CMapCell::IsEntrance() const {
    ev("IsEntrance", {CellOf(this)});
    return In(gEntrance, gEntranceCount, CellOf(this)) ? 1 : 0;
}
int32_t CMapCell::IsPath() const {
    ev("IsPath", {CellOf(this)});
    return In(gPath, gPathCount, CellOf(this)) ? 1 : 0;
}
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetParkEntrancePos(int32_t which) const {
    ev("GetParkEntrancePos", {which});
    return {static_cast<uint16_t>(gEntrancePos[which & 1])};
}
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetTicketBoothPos(int32_t which) const {
    ev("GetTicketBoothPos", {which});
    return {static_cast<uint16_t>(gBoothPos[which & 1])};
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(gAnalyserBytes);
}
int32_t CParkAnalyser::NumberOfObjectTypeInPark(UIObjType type, int32_t level) const {
    ev("NumberOfObjectTypeInPark", {lab(Launder(this)), type, level});
    return static_cast<int32_t>(gRidesInPark);
}

// The gates: the status answers the queue "gates" (default 1, open); the other two only log.
int32_t CWorld::ParkGates_GetStatus() const {
    ev("ParkGates_GetStatus", {lab(this)});
    return static_cast<int32_t>(ret("gates", 1));
}
void CWorld::SetParkOpenOrClosed(int32_t open, int32_t gateFlag) { ev("SetParkOpenOrClosed", {lab(this), open, gateFlag}); }
void CWorld::DeletePendingThings() { ev("DeletePendingThings", {lab(this)}); }

// The ride database: the case's rides in order; GetRideInfo finds the ride of the id (the first when none has it).
RideInfo* CRideDatabase::GetFirstRide() {
    ev("GetFirstRide", {lab(this)});
    gRideCursor = 0;
    return gRideCount > 0 ? Ride(0) : nullptr;
}
RideInfo* CRideDatabase::GetNextRide() {
    ev("GetNextRide", {lab(this)});
    ++gRideCursor;
    return gRideCursor < gRideCount ? Ride(gRideCursor) : nullptr;
}
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    for (int32_t i = 0; i < gRideCount; ++i)
        if (Ride(i)->mId == id.mIndex) return Ride(i);
    return Ride(0);
}

// The comparison ignores ASCII case (the Mac's is the same for the texts the cases use).
int32_t NS_MacDoze::_wcsicmp(const char16_t* a, const char16_t* b) {
    const std::string first = narrow(a);
    const std::string second = narrow(b);
    ev("wcsicmp", {}, {first.c_str(), second.c_str()});
    for (std::size_t i = 0;; ++i) {
        char16_t x = a[i];
        char16_t y = b[i];
        if (x >= 'A' && x <= 'Z') x = static_cast<char16_t>(x + 32);
        if (y >= 'A' && y <= 'Z') y = static_cast<char16_t>(y + 32);
        if (x != y) return 1;
        if (x == 0) return 0;
    }
}

extern "C" {

void tpw_wsq_begin(int32_t count) {
    for (int32_t i = 0; i < gBlockCount; ++i) std::free(gBlocks[i]);
    gBlockCount = count;
    for (int32_t i = 0; i < count; ++i) {
        gBlocks[i] = ::operator new(kBlockSize);
        std::memset(gBlocks[i], 0xa5, kBlockSize);
    }
    gRideCount = gRideCursor = 0;
    gValidCount = gEntranceCount = gPathCount = 0;
    gEntrancePos[0] = gEntrancePos[1] = gBoothPos[0] = gBoothPos[1] = 0;
    gRidesInPark = 0;
    std::memset(gAnalyserBytes, 0xa5, sizeof gAnalyserBytes);
}

// 1 the world, 2 the analyser, 5 the pending list, 6 the ride database, 7 the UI interface, 10 + k block k.
void* tpw_wsq_ptr(int32_t label) {
    switch (label) {
    case kLabelWorld: return gWorldBytes;
    case kLabelAnalyser: return gAnalyserBytes;
    case kLabelList: return &gPendingDeleteList;
    case kLabelRides: return &gRideDatabase;
    case kLabelUI: return &gUIInterface;
    default: return label >= kLabelBlock && label < kLabelBlock + gBlockCount ? gBlocks[label - kLabelBlock] : nullptr;
    }
}

// Thing block k: the id (+0), the kind (+2), the dead flag (+3), the words at +0xc and +0x1c and the halfword at +0x2e.
void tpw_wsq_thing(int32_t k, uint32_t id, uint32_t kind, uint32_t dead, uint32_t w0c, uint32_t w1c, uint32_t h2e) {
    unsigned char* block = static_cast<unsigned char*>(gBlocks[k]);
    const uint16_t id16 = static_cast<uint16_t>(id);
    const uint16_t flags = static_cast<uint16_t>(h2e);
    std::memcpy(block, &id16, 2);
    block[2] = static_cast<unsigned char>(kind);
    block[3] = static_cast<unsigned char>(dead);
    std::memcpy(block + 0xc, &w0c, 4);
    std::memcpy(block + 0x1c, &w1c, 4);
    std::memcpy(block + 0x2e, &flags, 2);
}

// The report of block k: id, kind, dead, the two words, the halfword and 1 when every other byte still holds the fill.
void tpw_wsq_get_thing(int32_t k, uint32_t* out) {
    const unsigned char* block = static_cast<const unsigned char*>(gBlocks[k]);
    uint16_t id, flags;
    std::memcpy(&id, block, 2);
    std::memcpy(&flags, block + 0x2e, 2);
    out[0] = id;
    out[1] = block[2];
    out[2] = block[3];
    std::memcpy(&out[3], block + 0xc, 4);
    std::memcpy(&out[4], block + 0x1c, 4);
    out[5] = flags;
    bool fill = true;
    for (std::size_t i = 4; i < kBlockSize; ++i)
        if ((i < 0xc || i >= 0x10) && (i < 0x1c || i >= 0x20) && (i < 0x2e || i >= 0x30) && block[i] != 0xa5) fill = false;
    out[6] = fill;
}

// The table (as unit w121's): every row default, then `n` rows of six words (row, thing block + 1, index word, next row + 1, previous
// row + 1, next free row + 1), then the two heads (row + 1, 0 for NULL).
void tpw_wsq_table(const uint32_t* rows, int32_t n, uint32_t usedHead, uint32_t freeHead) {
    for (int32_t i = 0; i < kThingArraySize; ++i) {
        CThingArrayEntry& entry = gThingArray[i];
        entry.mThing = nullptr;
        entry.mF04[0] = static_cast<uint32_t>(i);
        entry.mF04[1] = entry.mF04[2] = entry.mF04[3] = 0xa5a5a5a5u;
        entry.mNextUsed = entry.mPrevUsed = entry.mNextFree = nullptr;
    }
    for (int32_t j = 0; j < n; ++j) {
        const uint32_t* w = rows + 6 * j;
        CThingArrayEntry& entry = gThingArray[w[0]];
        entry.mThing = w[1] == 0 ? nullptr : gBlocks[w[1] - 1];
        entry.mF04[0] = w[2];
        entry.mNextUsed = const_cast<CThingArrayEntry*>(EntryOf(w[3]));
        entry.mPrevUsed = const_cast<CThingArrayEntry*>(EntryOf(w[4]));
        entry.mNextFree = const_cast<CThingArrayEntry*>(EntryOf(w[5]));
    }
    gUsedThingHead = const_cast<CThingArrayEntry*>(EntryOf(usedHead));
    gFreeThingHead = const_cast<CThingArrayEntry*>(EntryOf(freeHead));
    std::memcpy(gInitial, gThingArray, sizeof gInitial);
}

// The rows that differ from the start: the count, six values a row, then the two heads as row + 1.
void tpw_wsq_get_table(int64_t* out) {
    int64_t n = 0;
    for (int32_t i = 0; i < kThingArraySize; ++i) {
        const CThingArrayEntry& entry = gThingArray[i];
        const CThingArrayEntry& before = gInitial[i];
        if (entry.mThing == before.mThing && entry.mF04[0] == before.mF04[0] && entry.mNextUsed == before.mNextUsed &&
            entry.mPrevUsed == before.mPrevUsed && entry.mNextFree == before.mNextFree)
            continue;
        int64_t* row = out + 1 + 6 * n++;
        row[0] = i;
        row[1] = BlockOf(entry.mThing);
        row[2] = entry.mF04[0];
        row[3] = RowOf(entry.mNextUsed);
        row[4] = RowOf(entry.mPrevUsed);
        row[5] = RowOf(entry.mNextFree);
    }
    out[0] = n;
    out[1 + 6 * n] = RowOf(gUsedThingHead);
    out[2 + 6 * n] = RowOf(gFreeThingHead);
}

// The world: fill 0xa5, then the start time (+0x1da708), the person count (+0x1da714), the gates id (+0x1da732) and the state word.
void tpw_wsq_world(uint32_t start, uint32_t persons, uint32_t gates, uint32_t state) {
    std::memset(gWorldBytes, 0xa5, sizeof gWorldBytes);
    World()->mStartTime = start;
    World()->mPersonCount = persons;
    World()->mGatesThing = static_cast<uint16_t>(gates);
    World()->mF1DA738 = static_cast<int32_t>(state);
}

// The four words, and 1 when every other byte of the world still holds the fill.
void tpw_wsq_get_world(uint32_t* out) {
    out[0] = World()->mStartTime;
    out[1] = World()->mPersonCount;
    out[2] = World()->mGatesThing;
    out[3] = static_cast<uint32_t>(World()->mF1DA738);
    World()->mStartTime = World()->mPersonCount = 0xa5a5a5a5u;
    World()->mGatesThing = 0xa5a5;
    World()->mF1DA738 = static_cast<int32_t>(0xa5a5a5a5u);
    bool fill = true;
    for (std::size_t i = 0; i < sizeof gWorldBytes; ++i)
        if (gWorldBytes[i] != 0xa5) fill = false;
    out[4] = fill;
}

// Ride k: its id, downloaded ride number, folder (narrow text) and item name (UTF-16 from 16 bit units).
void tpw_wsq_ride(int32_t k, uint32_t id, int32_t number, const char* folder, const uint16_t* name, int32_t nameLength) {
    std::memset(gRideBytes[k], 0, sizeof gRideBytes[k]);
    Ride(k)->mId = static_cast<uint16_t>(id);
    Ride(k)->Value(kRideSlot_DownloadedRideNumber).mInteger = number;
    std::strncpy(Ride(k)->mFolder, folder, sizeof Ride(k)->mFolder - 1);
    for (int32_t i = 0; i < nameLength && i < 0x3f; ++i) Ride(k)->mItemName[i] = static_cast<char16_t>(name[i]);
    if (k >= gRideCount) gRideCount = k + 1;
}

// The globals of the case: the winning thing id, the bitmap, the "End" feature's name.
void tpw_wsq_globals(uint32_t winning, const uint8_t* bitmap, const uint16_t* name, int32_t nameLength) {
    gWinningAnimThing = static_cast<uint16_t>(winning);
    std::memcpy(gParkRequiredRides, bitmap, 0x20);
    char16_t text[0x40];
    for (int32_t i = 0; i < nameLength && i < 0x3f; ++i) text[i] = static_cast<char16_t>(name[i]);
    text[nameLength] = 0;
    gWorldEndName = text;
}

// The map: the cells (1 based) that are valid, entrances and paths, the two entrance cells and the two booth cells, the analyser's ride count.
void tpw_wsq_map(const uint32_t* valid, int32_t nv, const uint32_t* entrance, int32_t ne, const uint32_t* path, int32_t np,
                 const uint32_t* positions, uint32_t rides) {
    gValidCount = nv;
    gEntranceCount = ne;
    gPathCount = np;
    std::memcpy(gValid, valid, nv * 4);
    std::memcpy(gEntrance, entrance, ne * 4);
    std::memcpy(gPath, path, np * 4);
    gEntrancePos[0] = positions[0];
    gEntrancePos[1] = positions[1];
    gBoothPos[0] = positions[2];
    gBoothPos[1] = positions[3];
    gRidesInPark = rides;
}

void tpw_wsq_get_globals(uint32_t* out) {
    out[0] = gWinningAnimThing;
    std::memcpy(out + 1, gParkRequiredRides, 0x20);
}

// The pending list: `n` ids (head first) in 12 byte nodes, the count word as given.
void tpw_wsq_list(const uint16_t* ids, int32_t n, int32_t count) {
    gPendingDeleteList.mHead = gPendingDeleteList.mTail = nullptr;
    for (int32_t i = n - 1; i >= 0; --i) {
        auto* node = new TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>::CNode(
            tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{ids[i]});
        node->mNext = gPendingDeleteList.mHead;
        if (gPendingDeleteList.mHead != nullptr) gPendingDeleteList.mHead->mPrev = node;
        else gPendingDeleteList.mTail = node;
        gPendingDeleteList.mHead = node;
    }
    gPendingDeleteList.mCount = count;
}

// The list as the report wants it: [count, head is NULL, tail is NULL, forward length, forward ids..., backward length, backward ids...].
void tpw_wsq_get_list(int64_t* out) {
    out[0] = gPendingDeleteList.mCount;
    out[1] = gPendingDeleteList.mHead == nullptr;
    out[2] = gPendingDeleteList.mTail == nullptr;
    int64_t n = 0;
    for (auto* node = gPendingDeleteList.mHead; node != nullptr && n < 64; node = node->mNext) out[4 + n++] = node->mData.mIndex;
    out[3] = n;
    int64_t m = 0;
    for (auto* node = gPendingDeleteList.mTail; node != nullptr && m < 64; node = node->mPrev) out[5 + n + m++] = node->mData.mIndex;
    out[4 + n] = m;
}

// One function (op, in the order of the test module's table). `thing` is a block + 1 (0 for NULL); `a` an id, seed or the like. The answer
// is the function's result as a plain integer. ConvertDLRidesUsedToIndices fills `out` (0x210 bytes).
int64_t tpw_wsq_call(int32_t op, int32_t thing, uint32_t a, uint8_t* out) {
    CWorld* const w = World();
    CThing* const t = thing == 0 ? nullptr : static_cast<CThing*>(gBlocks[thing - 1]);
    switch (op) {
    case 0: w->PopulatePark(); return 0;
    case 1: w->StartingPopulationForOnline(); return 0;
    case 2: w->StartBankruptcySequence(); return 0;
    case 3: w->StartWinningSequence(); return 0;
    case 4: return w->IsPlayingWinningAnim() ? 1 : 0;
    case 5: w->EndWinningSequence(); return 0;
    case 6: return w->GetPrevUsedThing(t).mIndex;
    case 7: w->SeedRand(a); return 0;
    case 8: w->ReconstructFreeList(); return 0;
    case 9: return static_cast<uint32_t>(w->GetNewPersonIndex());
    case 10: w->FlagItemAsSelected(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(a)}); return 0;
    case 11: return static_cast<uint32_t>(w->ConvertDLRidesUsedToIndices(reinterpret_cast<uint16_t*>(out)));
    case 12: {
        const auto id = gPendingDeleteList.RemoveHead();
        return id.mIndex;
    }
    case 13: gPendingDeleteList.AddHead(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(a)}); return 0;
    case 14: w->RestoreEngineObjects(); return 0;
    case 15: w->DeleteEngineObjects(); return 0;
    default: return -1;
    }
}

}  // extern "C"
