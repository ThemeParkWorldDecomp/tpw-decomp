// Test entry points for the world's turn (library unit_WorldTurn: src/World_turn.cpp, unit w121): CWorld::DoGameTurn, DoOnlineGameTurn,
// UpdateThingsInEngine, UpdateBumperCarsInEngine, AddThingToThingList, _CThingArrayEntry::FreeMe, DeleteAllThings, DeletePendingThings,
// GetNextUsedThing, GetUsedThingHead, IsParkClosed and ParkGates_GetStatus.
//
// Every callee outside those twelve is a fake that records the same event as the PowerPC side's stub: the asserts and traces, the thing's
// virtuals (GetThingId, Process, KillMe), the people (UpdatePositionInEngine, IsInsidePark), the bumper cars, the park analyser, the macro
// AI, the action recorder, the gates' script variables, the pending list's RemoveHead and the game type constructor. operator new and
// delete are the call log's (a labelled block that is deleted records "delete"); this file keeps every freed block until the next case so
// that the test can read what a freed thing held and no address is reused.
//
// The world is raw memory of the Mac's size over the fill 0xa5 (only four of its words matter), the thing table and its heads are the
// unit's own globals, the thing objects are blocks of 0x80 bytes. A case is described to the host by a few calls (begin, thing, table,
// world, pending, fakes) and then one call of the function; pointers are reported as 0, a block number or a table row so that the Mac's
// 32 bit words and the host's pointers compare.
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
bool KeepFreed(void*, bool) { return true; }
}  // namespace
#define TPW_RELEASE_OBSERVER(p, array) KeepFreed(p, array)
#include "calllog.cpp"

#include "ActionRecorder.h"
#include "Debug.h"
#include "FS_Online.h"
#include "TPAdvisor.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldThings.h"
#include "analyser.h"
#include "rsse.h"

using namespace calllog;

namespace {

constexpr int32_t kMaxBlocks = 64;
constexpr std::size_t kBlockSize = 0x80;
constexpr int32_t kLabelWorld = 1, kLabelAnalyser = 2, kLabelScript = 3, kLabelRecorder = 4, kLabelList = 5, kLabelBlock = 10;

alignas(16) unsigned char gWorldBytes[sizeof(CWorld)];
alignas(16) unsigned char gAnalyserBytes[0x40];
alignas(16) unsigned char gScriptBytes[0x40];
void* gBlocks[kMaxBlocks];       // the thing objects of the case (operator new, freed at the next begin)
int32_t gBlockCount;
uint16_t gQueue[256];            // the ids RemoveHead answers, in order
int32_t gQueueLen, gQueuePos;
int32_t gAnalyserPresent, gScriptPresent;
int32_t gCtorType;

CThingArrayEntry gInitial[kThingArraySize];   // the table as the case started (the report lists the rows that changed)

CWorld* World() { return reinterpret_cast<CWorld*>(gWorldBytes); }

// The pointer as the call log sees it, whatever the compiler knows about `this`: a thing may be NULL here.
template <class T>
T* Launder(T* pointer) {
    __asm__ volatile("" : "+r"(pointer));
    return pointer;
}

// A pointer in the shape both sides report: 0, a block number + 1, or -1 for any other address.
int64_t BlockOf(const void* pointer) {
    if (pointer == nullptr) return 0;
    for (int32_t i = 0; i < gBlockCount; ++i)
        if (gBlocks[i] == pointer) return i + 1;
    return -1;
}

// A table entry pointer as a row + 1 (0 for NULL, -1 for an address outside the table).
int64_t RowOf(const CThingArrayEntry* entry) {
    if (entry == nullptr) return 0;
    if (entry < gThingArray || entry >= gThingArray + kThingArraySize) return -1;
    return (entry - gThingArray) + 1;
}

const CThingArrayEntry* EntryOf(uint32_t rowPlusOne) { return rowPlusOne == 0 ? nullptr : &gThingArray[rowPlusOne - 1]; }

}  // namespace

// ---- The unit's globals (the launcher gets zero definitions) -----------------------------------------------------------------------
CThingArrayEntry gThingArray[kThingArraySize];
CThingArrayEntry* gUsedThingHead;
CThingArrayEntry* gFreeThingHead;
TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>> gPendingDeleteList;
CActionRecorder gActionRecorder;

// ---- The fakes ------------------------------------------------------------------------------------------------------------------------
// The condition and the text, and the world's state word (DeleteAllThings sets it to 2 and then 1).
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition, World()->mF1DA738}, {format});
}

// The level, the first variadic integer when the text takes one (a %d), and the text.
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

// The thing's own id; a NULL thing answers 0 (the Mac would read address 0).
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    const CThing* self = Launder(this);
    ev("GetThingId", {lab(self)});
    return {self != nullptr ? self->mThingId : uint16_t(0)};
}

void CThing::Process() { ev("Process", {lab(this), World()->mTurn}); }

// Mode 0 of the queue "killmode" does nothing; mode 1 is the real one: the thing is dead, its kind 2, and its id is queued first.
void CThing::KillMe() {
    ev("KillMe", {lab(this), World()->mF1DA738});
    if (ret("killmode", 0) == 1) {
        mDead = 1;
        mKind = 2;
        for (int32_t i = gQueueLen; i > gQueuePos; --i) gQueue[i] = gQueue[i - 1];
        gQueue[gQueuePos] = mThingId;
        ++gQueueLen;
        ++gPendingDeleteList.mCount;
    }
}

void CPeep::UpdatePositionInEngine(float step) { ev("UpdatePositionInEngine", {lab(this), fbits(step)}); }

// Answers the queue "inside" (only zero means outside).
int32_t CPeep::IsInsidePark() const {
    ev("IsInsidePark", {lab(this)});
    return static_cast<int32_t>(ret("inside", 1));
}

// The ride word is the whole 32 bit word of the thing.
void BMP_DrawCarsForRide(int32_t ride, float step) { ev("BMP_DrawCarsForRide", {static_cast<uint32_t>(ride), fbits(step)}); }

int32_t CParkAnalyser::NumberOfVisitorsInPark() const {
    ev("NumberOfVisitorsInPark", {lab(Launder(this))});
    return static_cast<int32_t>(ret("visitors", 0));
}

// The analyser is a labelled block or NULL (the stand-in the launcher has today).
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return gAnalyserPresent ? reinterpret_cast<CParkAnalyser*>(gAnalyserBytes) : nullptr;
}

void CMacroAI::Update() { ev("CMacroAI::Update", {lab(this), World()->mTurn}); }

void CActionRecorder::DoTick() { ev("DoTick", {lab(this)}); }

void CWorld::ParkGates_Close(int32_t flag) { ev("ParkGates_Close", {lab(this), flag}); }

RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script) {
    ev("RSSE_GetScriptPointer", {static_cast<uint32_t>(script)});
    return gScriptPresent ? reinterpret_cast<RSSE_SCRIPT*>(gScriptBytes) : nullptr;
}

// Answers the queue "var".
int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index) {
    ev("RSSE_Getvar", {lab(script), index});
    return static_cast<int32_t>(ret("var", 1));
}

// The Mac's RemoveHead counts the list down and answers the id at its head; here the ids come from the case's queue.
template <>
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>
TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>::RemoveHead() {
    ev("RemoveHead", {lab(this), mCount});
    --mCount;
    return {gQueuePos < gQueueLen ? gQueue[gQueuePos++] : uint16_t(0xffff)};
}

// Makes the type the case says; records nothing (the host's object is made once).
CGameType::CGameType() : mType(static_cast<GameType>(gCtorType)) {}

extern "C" {

// Frees the previous case's blocks and makes `count` new ones over the fill 0xa5, with their labels.
void tpw_wt_begin(int32_t count) {
    for (int32_t i = 0; i < gBlockCount; ++i) std::free(gBlocks[i]);
    gBlockCount = count;
    for (int32_t i = 0; i < count; ++i) {
        gBlocks[i] = ::operator new(kBlockSize);
        std::memset(gBlocks[i], 0xa5, kBlockSize);
    }
    gQueueLen = gQueuePos = 0;
    gAnalyserPresent = gScriptPresent = 0;
    std::memset(gAnalyserBytes, 0xa5, sizeof gAnalyserBytes);
    std::memset(gScriptBytes, 0xa5, sizeof gScriptBytes);
}

// The object behind a label: 1 the world, 2 the analyser, 3 the script, 4 the action recorder, 5 the pending list, 10 + k block k.
void* tpw_wt_ptr(int32_t label) {
    switch (label) {
    case kLabelWorld: return gWorldBytes;
    case kLabelAnalyser: return gAnalyserBytes;
    case kLabelScript: return gScriptBytes;
    case kLabelRecorder: return &gActionRecorder;
    case kLabelList: return &gPendingDeleteList;
    default: return label >= kLabelBlock && label < kLabelBlock + gBlockCount ? gBlocks[label - kLabelBlock] : nullptr;
    }
}

// The fields of thing block k: the id (+0), the kind (+2), the dead flag (+3) and the two words at +0x20 and +0x24.
void tpw_wt_thing(int32_t k, uint32_t id, uint32_t kind, uint32_t dead, uint32_t w20, uint32_t w24) {
    unsigned char* block = static_cast<unsigned char*>(gBlocks[k]);
    const uint16_t id16 = static_cast<uint16_t>(id);
    std::memcpy(block, &id16, 2);
    block[2] = static_cast<unsigned char>(kind);
    block[3] = static_cast<unsigned char>(dead);
    std::memcpy(block + 0x20, &w20, 4);
    std::memcpy(block + 0x24, &w24, 4);
}

// The report of thing block k: id, kind, dead, the two words and 1 when every other byte still holds the fill.
void tpw_wt_get_thing(int32_t k, uint32_t* out) {
    const unsigned char* block = static_cast<const unsigned char*>(gBlocks[k]);
    uint16_t id;
    std::memcpy(&id, block, 2);
    out[0] = id;
    out[1] = block[2];
    out[2] = block[3];
    std::memcpy(&out[3], block + 0x20, 4);
    std::memcpy(&out[4], block + 0x24, 4);
    bool fill = true;
    for (std::size_t i = 4; i < kBlockSize; ++i)
        if ((i < 0x20 || i >= 0x28) && block[i] != 0xa5) fill = false;
    out[5] = fill;
}

// The table: every row default (no thing, its own index, no links), then `n` rows of six words (row, thing block + 1, index word, next
// row + 1, previous row + 1, next free row + 1), then the two heads (row + 1, 0 for NULL).
void tpw_wt_table(const uint32_t* rows, int32_t n, uint32_t usedHead, uint32_t freeHead) {
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

// The rows that differ from the table the case started with: the count, then six values a row (the row, the thing block + 1 or -1 for
// another address, the index word, the three links as row + 1), then the two heads as row + 1.
void tpw_wt_get_table(int64_t* out) {
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

// The world: fill 0xa5 everywhere, then the four words the functions use.
void tpw_wt_world(uint32_t turn, uint32_t closed, uint32_t gates, uint32_t state) {
    std::memset(gWorldBytes, 0xa5, sizeof gWorldBytes);
    World()->mTurn = turn;
    World()->mF1DA710 = closed;
    World()->mGatesThing = static_cast<uint16_t>(gates);
    World()->mF1DA738 = static_cast<int32_t>(state);
}

// The four words, and 1 when every other byte of the world still holds the fill.
void tpw_wt_get_world(uint32_t* out) {
    out[0] = World()->mTurn;
    out[1] = World()->mF1DA710;
    out[2] = World()->mGatesThing;
    out[3] = static_cast<uint32_t>(World()->mF1DA738);
    World()->mTurn = World()->mF1DA710 = 0xa5a5a5a5u;
    World()->mGatesThing = 0xa5a5;
    World()->mF1DA738 = static_cast<int32_t>(0xa5a5a5a5u);
    bool fill = true;
    for (std::size_t i = 0; i < sizeof gWorldBytes; ++i)
        if (gWorldBytes[i] != 0xa5) fill = false;
    out[4] = fill;
}

// The pending list: its count word and the ids RemoveHead answers.
void tpw_wt_pending(int32_t count, const uint16_t* ids, int32_t n) {
    gPendingDeleteList.mCount = count;
    gQueueLen = n;
    gQueuePos = 0;
    for (int32_t i = 0; i < n; ++i) gQueue[i] = ids[i];
}

// The count, the position in the queue, the queue's length and its ids from the position on.
void tpw_wt_get_pending(int64_t* out) {
    out[0] = gPendingDeleteList.mCount;
    out[1] = gQueueLen - gQueuePos;
    for (int32_t i = gQueuePos; i < gQueueLen; ++i) out[2 + i - gQueuePos] = gQueue[i];
}

void tpw_wt_fakes(int32_t analyser, int32_t script) {
    gAnalyserPresent = analyser;
    gScriptPresent = script;
}

// The game type object: the case's type, and the type the constructor fake makes (used when the Mac's guard byte is 0).
void tpw_wt_gametype(int32_t type, int32_t ctorType) {
    gCtorType = ctorType;
    TheGameType().mType = static_cast<GameType>(type);
}

// One function (op, in the order of the test module's table). `world` 0 passes a NULL world to the leaves that do not read it. The
// ThingId answers are returned as a plain integer; `thing` is a thing block + 1 (0 for NULL) and `row` an entry; `bits` a float's bits.
int64_t tpw_wt_call(int32_t op, int32_t world, int32_t thing, int32_t row, uint32_t bits) {
    CWorld* const w = world ? World() : nullptr;
    CThing* const t = thing == 0 ? nullptr : static_cast<CThing*>(gBlocks[thing - 1]);
    float step;
    std::memcpy(&step, &bits, 4);
    switch (op) {
    case 0: World()->DoGameTurn(); return 0;
    case 1: World()->DoOnlineGameTurn(); return 0;
    case 2: World()->UpdateThingsInEngine(step); return 0;
    case 3: World()->UpdateBumperCarsInEngine(step); return 0;
    case 4: return w->AddThingToThingList(t).mIndex;
    case 5: gThingArray[row].FreeMe(); return 0;
    case 6: World()->DeleteAllThings(); return 0;
    case 7: World()->DeletePendingThings(); return 0;
    case 8: return w->GetNextUsedThing(t).mIndex;
    case 9: return w->GetUsedThingHead().mIndex;
    case 10: return static_cast<uint32_t>(World()->IsParkClosed());
    case 11: return static_cast<uint32_t>(World()->ParkGates_GetStatus());
    default: return -1;
    }
}

}  // extern "C"
