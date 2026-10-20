// Test entry points for the world's construction (library unit_WorldInit: World_init.cpp, Game_world.cpp, StaffDB_ctor.cpp,
// ParkTime.cpp, Arrival.cpp, CellInit.cpp, BlockSave.cpp, ExitErr.cpp for Allocate's error branch and StaffDB_dtor.cpp, which the
// world's constructor names for its unwinding path). Every callee outside those files
// is a fake that records what it is given: operator new (nothrow; it answers the test's block, or NULL when the case says so),
// TbTimeStamp::SetTime and NS_MacDoze::GetLocalTime. The globals the functions write are the unit's own, defined here.
//
// The object under test lives in gBlock (the world, or a small object followed by guard bytes); the test loads fill bytes into it
// before the call and dumps it after. State words (the world pointers, the heads of the thing lists, the save table, the grid
// registry and the exit error) are read and written in one fixed order by tpw_wi_set_state / tpw_wi_get_state; pointers are
// reported canonically (0, an offset into gBlock, a thing table index, or the raw low 32 bits) so that the Mac's 32-bit words and
// the host's pointers compare.
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"

#include "BlockSave.h"
#include "CellBase.h"
#include "ExitErr.h"
#include "Game.h"
#include "TbTimeDiff.h"
#include "World.h"
#include "platform/macdoze.h"

using namespace calllog;

namespace {

constexpr std::size_t kBlockSize = sizeof(CWorld) + 0x100;   // the world, or a small object followed by guard bytes
alignas(16) unsigned char gBlock[kBlockSize];
alignas(16) unsigned char gGameStorage[16];                  // the CGame InitWorld is called on (never read)
uint16_t gTime[8];                                           // what GetLocalTime answers

constexpr int64_t kWorldBase = 1ll << 40;
constexpr int64_t kTableBase = 2ll << 40;

// A pointer in the shape both sides report: 0, gBlock + offset, a thing table index, or the raw low 32 bits (the test's random words).
int64_t Canon(const void* pointer) {
    const uintptr_t a = reinterpret_cast<uintptr_t>(pointer);
    if (a == 0) return 0;
    const uintptr_t block = reinterpret_cast<uintptr_t>(gBlock);
    if (a >= block && a < block + kBlockSize) return kWorldBase + static_cast<int64_t>(a - block);
    const uintptr_t table = reinterpret_cast<uintptr_t>(gThingArray);
    if (a >= table && a < table + sizeof gThingArray) {
        const uintptr_t off = a - table;
        return off % sizeof(CThingArrayEntry) == 0 ? kTableBase + static_cast<int64_t>(off / sizeof(CThingArrayEntry))
                                                   : (3ll << 40) + static_cast<int64_t>(off);
    }
    return static_cast<int64_t>(a & 0xffffffffu);
}

template <class T>
T* Ptr(uint32_t word) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(word));
}

int64_t Offset(const void* object) { return static_cast<const unsigned char*>(object) - gBlock; }

}  // namespace

// ---- Globals the functions write (the launcher gets zero definitions) -----------------------------------------------------------
CWorld* gGameWorld;
CWorld* gWorld;
CMap* gWorldMap;
CThingArrayEntry gThingArray[kThingArraySize];
CThingArrayEntry* gUsedThingHead;
CThingArrayEntry* gFreeThingHead;
CBlockSave gBlockSave;
const CMapCell* gMapGridBase;
const CMapCell* gMapGridEnd;
int32_t gMapGridCellSize;
const CTrackCell* gTrackGridBase;
const CTrackCell* gTrackGridEnd;
int32_t gTrackGridCellSize;

// ---- The fakes -----------------------------------------------------------------------------------------------------------------
// Never runs (it is only named by ~CStaffDB, which the world's constructor would call if a member's constructor threw).
CStaffInfo::~CStaffInfo() { ev("CStaffInfo::~CStaffInfo", {Offset(this)}); }
// operator new with a null answer: the case's queue "newnull" says whether the call fails. CGame::InitWorld is the only caller.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = on() && ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    return fail ? nullptr : gBlock;
}
// The seventh argument is the millisecond; the stamp is reported as its offset from the block (the world, or the park time).
TbError TbTimeStamp::SetTime(int32_t year, int32_t month, int32_t day, int32_t hour, int32_t minute, int32_t second,
                             int32_t millisecond) {
    ev("SetTime", {Offset(this), year, month, day, hour, minute, second, millisecond});
    return TbError(0);
}
// Fills the time with the case's eight 16-bit values (wDayOfWeek is among them: the code never reads it).
void NS_MacDoze::GetLocalTime(_SYSTEMTIME* time) {
    ev("GetLocalTime");
    time->wYear = gTime[0];
    time->wMonth = gTime[1];
    time->wDayOfWeek = gTime[2];
    time->wDay = gTime[3];
    time->wHour = gTime[4];
    time->wMinute = gTime[5];
    time->wSecond = gTime[6];
    time->wMilliseconds = gTime[7];
}

extern "C" {

void* tpw_wi_block() { return gBlock; }
int32_t tpw_wi_block_size() { return static_cast<int32_t>(kBlockSize); }
void tpw_wi_load(const uint8_t* bytes, int32_t n) { std::memcpy(gBlock, bytes, static_cast<std::size_t>(n)); }
void tpw_wi_dump(uint8_t* out, int32_t n) { std::memcpy(out, gBlock, static_cast<std::size_t>(n)); }
void tpw_wi_set_time(const uint16_t* time) { std::memcpy(gTime, time, sizeof gTime); }

// 18 state words in the Mac's order: gGameWorld, gWorld, gWorldMap, the used head, the free head, the save table (data, size, data,
// size), the map grid (base, end, size), the track grid (base, end, size), the exit error (mError, mType, mValue); and its text.
void tpw_wi_set_state(const uint32_t* w, const uint8_t* text) {
    gGameWorld = Ptr<CWorld>(w[0]);
    gWorld = Ptr<CWorld>(w[1]);
    gWorldMap = Ptr<CMap>(w[2]);
    gUsedThingHead = Ptr<CThingArrayEntry>(w[3]);
    gFreeThingHead = Ptr<CThingArrayEntry>(w[4]);
    gBlockSave.mBlocks[0].mData = Ptr<void>(w[5]);
    gBlockSave.mBlocks[0].mSize = static_cast<int32_t>(w[6]);
    gBlockSave.mBlocks[1].mData = Ptr<void>(w[7]);
    gBlockSave.mBlocks[1].mSize = static_cast<int32_t>(w[8]);
    gMapGridBase = Ptr<const CMapCell>(w[9]);
    gMapGridEnd = Ptr<const CMapCell>(w[10]);
    gMapGridCellSize = static_cast<int32_t>(w[11]);
    gTrackGridBase = Ptr<const CTrackCell>(w[12]);
    gTrackGridEnd = Ptr<const CTrackCell>(w[13]);
    gTrackGridCellSize = static_cast<int32_t>(w[14]);
    gExitError.mError = static_cast<int32_t>(w[15]);
    gExitError.mType = static_cast<CExitError::ErrorType>(w[16]);
    gExitError.mValue = w[17];
    std::memcpy(gExitError.mText, text, sizeof gExitError.mText);
}

void tpw_wi_get_state(int64_t* out, uint8_t* text) {
    out[0] = Canon(gGameWorld);
    out[1] = Canon(gWorld);
    out[2] = Canon(gWorldMap);
    out[3] = Canon(gUsedThingHead);
    out[4] = Canon(gFreeThingHead);
    out[5] = Canon(gBlockSave.mBlocks[0].mData);
    out[6] = static_cast<uint32_t>(gBlockSave.mBlocks[0].mSize);
    out[7] = Canon(gBlockSave.mBlocks[1].mData);
    out[8] = static_cast<uint32_t>(gBlockSave.mBlocks[1].mSize);
    out[9] = Canon(gMapGridBase);
    out[10] = Canon(gMapGridEnd);
    out[11] = static_cast<uint32_t>(gMapGridCellSize);
    out[12] = Canon(gTrackGridBase);
    out[13] = Canon(gTrackGridEnd);
    out[14] = static_cast<uint32_t>(gTrackGridCellSize);
    out[15] = static_cast<uint32_t>(gExitError.mError);
    out[16] = static_cast<uint32_t>(gExitError.mType);
    out[17] = gExitError.mValue;
    std::memcpy(text, gExitError.mText, sizeof gExitError.mText);
}

// The thing table, five words an entry: thing, own index, next used, previous used, next free (the Mac's words 0 to 4).
void tpw_wi_set_table(const uint32_t* w) {
    for (int32_t i = 0; i < kThingArraySize; ++i) {
        CThingArrayEntry& entry = gThingArray[i];
        entry.mThing = Ptr<void>(w[5 * i]);
        entry.mF04[0] = w[5 * i + 1];
        entry.mF04[1] = 0xa5a5a5a5u;
        entry.mF04[2] = 0xa5a5a5a5u;
        entry.mF04[3] = 0xa5a5a5a5u;
        entry.mNextUsed = Ptr<CThingArrayEntry>(w[5 * i + 2]);
        entry.mPrevUsed = Ptr<CThingArrayEntry>(w[5 * i + 3]);
        entry.mNextFree = Ptr<CThingArrayEntry>(w[5 * i + 4]);
    }
}

void tpw_wi_get_table(int64_t* out) {
    for (int32_t i = 0; i < kThingArraySize; ++i) {
        const CThingArrayEntry& entry = gThingArray[i];
        out[5 * i] = Canon(entry.mThing);
        out[5 * i + 1] = entry.mF04[0];
        out[5 * i + 2] = Canon(entry.mNextUsed);
        out[5 * i + 3] = Canon(entry.mPrevUsed);
        out[5 * i + 4] = Canon(entry.mNextFree);
    }
}

// op: the function (the order of the test module's table). a to d are the case's arguments.
int64_t tpw_wi_call(int32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    CWorld* const world = reinterpret_cast<CWorld*>(gBlock);
    switch (op) {
    case 0: return reinterpret_cast<CGame*>(gGameStorage)->InitWorld();
    case 1: new (gBlock) CWorld; return 0;
    case 2: return world->Allocate() ? 1 : 0;
    case 3: return world->AllocateThingHeap() ? 1 : 0;
    case 4: world->InitThingHeap(); return 0;
    case 5: new (gBlock) CStaffDB; return 0;
    case 6: new (gBlock) CStaffInfo; return 0;
    case 7: new (gBlock) CParkTime; return 0;
    case 8: reinterpret_cast<CParkTime*>(gBlock)->Reset(); return 0;
    case 9: new (gBlock) CArrival; return 0;
    case 10: new (gBlock) CMapCell; return 0;
    case 11: new (gBlock) CTrackCell; return 0;
    case 12: CCellBase::RegisterMapGrid(Ptr<const CMapCell>(a), static_cast<int32_t>(b)); return 0;
    case 13: CCellBase::RegisterTrackGrid(Ptr<const CTrackCell>(a), static_cast<int32_t>(b)); return 0;
    case 14: new (gBlock) CRegionEffectCell; return 0;
    case 15: new (gBlock) ObjectControl; return 0;
    case 16:
        gBlockSave.RegisterSaveBlock(static_cast<CBlockSave::SaveBlock>(a), Ptr<void>(b), static_cast<int32_t>(c), Ptr<const char>(d));
        return 0;
    default: return -1;
    }
}

}  // extern "C"
