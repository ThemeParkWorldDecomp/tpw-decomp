// Differential library for unit w123b: the world's thing getters (src/game/World.cpp), CParkTime::GetFunnyTime (src/game/people/ParkTime.cpp),
// CParkAnalyser::Init (src/game/park/analyser.cpp) and CChallengeHQ::Init (src/game/park/ChallengeHQ.cpp).
//
// Every callee outside those ten is a fake that records the same event as the PowerPC side's stub: the assertion and the trace, the world accessor, the two
// time stamp imports, the message centre (Subscribe, operator new, the header constructor), the analyser cell, the challenge constructor, AddChallenge,
// ClearInvalidFollowups and the balance table's Init, Load and UnInit. gMainBalance is the real storage (its table methods are fakes that ignore it).
//
// The world is NOT a block of its Mac size: only the 0x80 bytes at world + 0x1da6f0 are real memory (gTail), and the world pointer is that block's
// address minus 0x1da6f0, on both sides. The getters and the clock read nothing else of it. The thing table and the message centre pointer are the
// unit's own globals; the thing objects are small blocks. A case describes everything to the host with a few calls and then makes one call; pointers are
// reported as call-log labels so that the Mac's 32 bit words and the host's pointers compare. tpw_wg_addr gives the test the address of each block.
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "ChallengeBalance.h"
#include "Debug.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"

using namespace calllog;

namespace {

constexpr int32_t kTail = 0x1da6f0;      // the world's byte that gTail starts at
constexpr int32_t kTailSize = 0x80;
constexpr int32_t kClockAt = 0x40;       // the clock's offset inside gClockBlock (the rest is guard bytes)
constexpr int32_t kBlockSize = 0x21c48 + 0x40;

alignas(16) unsigned char gTail[2][kTailSize];
alignas(16) unsigned char gThings[8][0x20];
alignas(16) unsigned char gClockBlock[kClockAt + 0x24 + 0x40];
alignas(16) int32_t gOutCells[12];       // eight output cells and guards
alignas(16) unsigned char gBlock[kBlockSize];   // the analyser or the challenge HQ, over a guard of 0x40 bytes
alignas(16) unsigned char gCentreBlock[0x1d0];

void* gNewCentre;
void* gAllocated[8];
int32_t gAllocatedCount;
int32_t gCurrentWorld;                   // the world the getter is called on (the assertion fake mutates it)
int32_t gTable[602];                     // what Load puts into slots 1..601 of the local challenge table
int32_t gAfterLoad[20];                  // what Load puts into the main balance's twenty choices when gHaveAfter is set
int32_t gHaveAfter;

CWorld* WorldPtr(int32_t k) { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTail[k]) - kTail); }

uint32_t Word(const void* p, int32_t offset) {
    uint32_t v;
    std::memcpy(&v, static_cast<const unsigned char*>(p) + offset, 4);
    return v;
}
void SetWord(void* p, int32_t offset, uint32_t value) { std::memcpy(static_cast<unsigned char*>(p) + offset, &value, 4); }

// The challenge HQ as the fakes see it: the twenty type words and the current challenge, hashed by value (so that the two byte orders agree).
uint32_t HqState() {
    uint32_t h = 2166136261u;
    for (int32_t i = 0; i < 20; ++i) h = (h ^ Word(gBlock, 0xc + 0x30 * i)) * 16777619u;
    return (h ^ Word(gBlock, 0x3cc)) * 16777619u;
}

}  // namespace

// ---- The unit's globals (the launcher gets zero definitions) -----------------------------------------------------------------------
CThingArrayEntry gThingArray[kThingArraySize];
CMessageCentre* gMessageCentre;

// ---- The fakes ------------------------------------------------------------------------------------------------------------------------
// The condition and the text. With the queue "amut" set the assertion then changes the world: the halfword at world offset "aoff" becomes "aid", and
// "arows" rows of the thing table get another thing ("arow" and "ablk": a block number + 1, 0 for NULL), as a callback that moves things would.
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition}, {format});
    if (ret("amut", 0) != 0) {
        const int64_t offset = ret("aoff", 0);
        const uint16_t id = static_cast<uint16_t>(ret("aid", 0));
        std::memcpy(gTail[gCurrentWorld] + (offset - kTail), &id, 2);
        const int64_t rows = ret("arows", 0);
        for (int64_t k = 0; k < rows; ++k) {
            const int64_t row = ret("arow", 0);
            const int64_t block = ret("ablk", 0);
            gThingArray[row].mThing = block != 0 ? static_cast<void*>(gThings[block - 1]) : nullptr;
        }
    }
}

// The level 0 trace, with its text.
void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {level}, {format}); }

// The world accessor: records its receiver (the clock) and answers world "wsel"; with "wmut" set it first changes the answer's turn word, the clock's
// rate and the clock's start (the values "wturn", "wrate", "wstart"), as a world that moved on would.
CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {lab(this)});
    const int32_t selected = static_cast<int32_t>(ret("wsel", 0));
    if (ret("wmut", 0) != 0) {
        SetWord(gTail[selected], 0x1da70c - kTail, static_cast<uint32_t>(ret("wturn", 0)));
        SetWord(gClockBlock, kClockAt + 0x1c, static_cast<uint32_t>(ret("wrate", 0)));
        const int64_t start = ret("wstart", 0);
        std::memcpy(gClockBlock + kClockAt, &start, 8);
    }
    return *WorldPtr(selected);
}

// The sum: the receiver (the clock), its tick image and the span's, and the answer "plus".
TbTimeStamp TbTimeStamp::operator+(const TbTimeDiff& diff) const {
    ev("TimeStamp+", {lab(this), mTicks, diff.mTicks});
    return TbTimeStamp{ret("plus", 0)};
}

// The stamp's tick image and the eight output pointers as labels. Each non-NULL output, in order, takes the next value of "gtout"; with "gtmut" set the
// stamp itself is changed to "gtnew" (the caller must answer the stamp it holds AFTER this call); the TbError is "gterr".
TbError TbTimeStamp::GetTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    ev("GetTime", {mTicks, lab(a), lab(b), lab(c), lab(d), lab(e), lab(f), lab(g), lab(h)});
    for (int32_t* out : {a, b, c, d, e, f, g, h}) {
        if (out != nullptr) *out = static_cast<int32_t>(ret("gtout", 0));
    }
    if (ret("gtmut", 0) != 0) const_cast<TbTimeStamp*>(this)->mTicks = ret("gtnew", 0);
    return TbError(static_cast<int32_t>(ret("gterr", 0)));
}

// The message centre: the subscription (which may clear the global, as the real one never does, to force another allocation), the allocation and the
// 29 headers (one event for the whole block).
bool CMessageCentre::Subscribe(const CThing& thing, MsgType type) {
    ev("Subscribe", {lab(this), lab(&thing), type});
    if (ret("centreclear", 0) != 0) gMessageCentre = nullptr;
    return true;
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = std::malloc(size);
    std::memset(gNewCentre, 0xa5, size);
    gAllocated[gAllocatedCount++] = gNewCentre;
    return gNewCentre;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

// The cell: its offset in the analyser and the four numbers; the stores of the original; the answer "cret" (default 1, the original's). With "cmut" set the
// parent's five numbers change (squares per cell, grid X and Y, map X and Y) to "cs", "cgx", "cgy", "cmx", "cmy".
int32_t CParkAnalyserCell::Init(int32_t startX, int32_t startY, int32_t sizeX, int32_t sizeY) {
    const int64_t offset = reinterpret_cast<const unsigned char*>(this) - gBlock;
    ev("CellInit", {offset, startX, startY, sizeX, sizeY});
    mStartX = startX;
    mStartY = startY;
    mSizeX = sizeX;
    mSizeY = sizeY;
    if (ret("cmut", 0) != 0) {
        SetWord(gBlock, 0xc, static_cast<uint32_t>(ret("cs", 0)));
        SetWord(gBlock, 0x10, static_cast<uint32_t>(ret("cgx", 1)));
        SetWord(gBlock, 0x14, static_cast<uint32_t>(ret("cgy", 0)));
        SetWord(gBlock, 0x18, static_cast<uint32_t>(ret("cmx", 0)));
        SetWord(gBlock, 0x1c, static_cast<uint32_t>(ret("cmy", 0)));
    }
    return static_cast<int32_t>(ret("cret", 1));
}

// The local challenge table (the main balance's own Init and UnInit are ignored). Init gives the table a label; Load records the path and mode and puts the
// case's numbers into slots 1..601 (also when it answers failure) and, when the case asks for it, changes the main balance's twenty choices.
void CBalanceTable::Init() {
    if (this == &gMainBalance) return;
    ev("BalanceInit", {newlab(this), HqState()});
}
int32_t CBalanceTable::Load(const char* path, LoadMode mode) {
    ev("BalanceLoad", {lab(this), mode, HqState()}, {path});
    if (this != &gMainBalance) {
        BalanceValue* values = GetBalanceValuesStart();
        for (int32_t k = 1; k <= 601; ++k) values[k].mInteger = gTable[k];
        if (gHaveAfter != 0) {
            for (int32_t i = 0; i < 20; ++i) gMainBalance.mValues[kMainBalanceChallengesInThisLevel + i].mInteger = gAfterLoad[i];
        }
    }
    return static_cast<int32_t>(ret("load", 1));
}
void CBalanceTable::UnInit() {
    if (this == &gMainBalance) return;
    ev("BalanceUnInit", {lab(this), HqState()});
}

// The challenge: all ten arguments (the temporary's label is made on first sight, so a changed address shows) and the original's stores.
CChallenge::CChallenge(ChallengeType type, int32_t targetTime, int32_t prize, ChallengeType followup, bool checkAtEndOnly, int32_t targetVal,
                       tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> targetObj, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> targetObj2,
                       StaffType staffType, bool independent) {
    int32_t label = lab(this);
    if (label < 0) label = newlab(this);
    ev("ChallengeCtor", {label, type, targetTime, prize, followup, checkAtEndOnly, targetVal, targetObj.mIndex, targetObj2.mIndex, staffType, independent});
    mType = type;
    mTargetTime = targetTime;
    mTargetVal = targetVal;
    mTargetObj = targetObj.mIndex;
    mTargetObj2 = targetObj2.mIndex;
    mTargetStaffType = staffType;
    mPrize = prize;
    mDoubleOrNothingFollowup = followup;
    mCheckAtEndOnly = checkAtEndOnly;
    mIndependent = independent;
    mChallengeOffered = 0;
    mChallengeLost = 0;
    mChallengeDeclined = 0;
    mHaveWon = false;
    mThingIdForFollowup = 0;
}
// The HQ, the temporary and its semantic fields. With "addmut" set the HQ's slot "addslot" gets the type "addtype".
void CChallengeHQ::AddChallenge(CChallenge& c) {
    ev("AddChallenge", {lab(this), lab(&c), c.mType, c.mTargetTime, c.mTargetVal, c.mTargetObj, c.mTargetObj2, c.mTargetStaffType, c.mPrize,
                        c.mDoubleOrNothingFollowup, c.mCheckAtEndOnly, c.mIndependent});
    if (ret("addmut", 0) != 0) SetWord(this, 0xc + 0x30 * static_cast<int32_t>(ret("addslot", 0)), static_cast<uint32_t>(ret("addtype", 0)));
}
// The HQ's state; with "clrmut" set the current challenge becomes "clrval".
void CChallengeHQ::ClearInvalidFollowups() {
    ev("ClearFollowups", {lab(this), HqState()});
    if (ret("clrmut", 0) != 0) SetWord(this, 0x3cc, static_cast<uint32_t>(ret("clrval", 0)));
}

// ---- The test entry points -----------------------------------------------------------------------------------------------------------
extern "C" {

// The address of a block: 0 and 1 the world tails, 2 the clock block, 3 the output cells, 4 the analyser or HQ block, 5 the centre block, 10 + n a thing.
void* tpw_wg_addr(int32_t which) {
    if (which == 0 || which == 1) return gTail[which];
    switch (which) {
    case 2: return gClockBlock;
    case 3: return gOutCells;
    case 4: return gBlock;
    case 5: return gCentreBlock;
    default: return which >= 10 && which < 18 ? static_cast<void*>(gThings[which - 10]) : nullptr;
    }
}
void* tpw_wg_world(int32_t k) { return WorldPtr(k); }
void tpw_wg_row(int32_t row, int32_t block, uint32_t f1, uint32_t f2, uint32_t f3, uint32_t f4) {
    CThingArrayEntry& entry = gThingArray[row];
    entry.mThing = block != 0 ? static_cast<void*>(gThings[block - 1]) : nullptr;
    entry.mF04[0] = f1;
    entry.mF04[1] = f2;
    entry.mF04[2] = f3;
    entry.mF04[3] = f4;
}
// A row as the report shows it: the thing's label and the four index words.
void tpw_wg_rowinfo(int32_t row, int64_t* out) {
    const CThingArrayEntry& entry = gThingArray[row];
    out[0] = lab(entry.mThing);
    for (int32_t i = 0; i < 4; ++i) out[1 + i] = entry.mF04[i];
}

// The getters: 0 analyser const, 1 analyser, 2 lab const, 3 lab, 4 weather, 5 bank, 6 challenge HQ, each on world k; answers the label of the result.
int32_t tpw_wg_getter(int32_t op, int32_t k) {
    gCurrentWorld = k;
    CWorld* world = WorldPtr(k);
    const CWorld* constWorld = world;
    const void* result = nullptr;
    switch (op) {
    case 0: result = constWorld->GetParkAnalyser(); break;
    case 1: result = world->GetParkAnalyser(); break;
    case 2: result = constWorld->GetResearchLab(); break;
    case 3: result = world->GetResearchLab(); break;
    case 4: result = world->GetWeather(); break;
    case 5: result = world->GetBankAccount(); break;
    case 6: result = world->GetChallengeHQ(); break;
    default: break;
    }
    return lab(result);
}

// The clock: the eight outputs are given as cell numbers (-1 for NULL); answers the tick image.
int64_t tpw_wg_funny(const int32_t* select) {
    int32_t* outputs[8];
    for (int32_t i = 0; i < 8; ++i) outputs[i] = select[i] < 0 ? nullptr : &gOutCells[select[i]];
    const CParkTime* clock = reinterpret_cast<const CParkTime*>(gClockBlock + kClockAt);
    const TbTimeStamp when = clock->GetFunnyTime(outputs[0], outputs[1], outputs[2], outputs[3], outputs[4], outputs[5], outputs[6], outputs[7]);
    return when.mTicks;
}

// The analyser and the HQ: the centre global (0 or the centre block) and the allocations of the last case, then the call.
void tpw_wg_prepare(int32_t centre) {
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    gNewCentre = nullptr;
    gMessageCentre = centre != 0 ? reinterpret_cast<CMessageCentre*>(gCentreBlock) : nullptr;
}
void tpw_wg_globals(int64_t* out) {
    out[0] = lab(gMessageCentre);
    out[1] = gAllocatedCount;
}
int32_t tpw_wg_analyser() { return reinterpret_cast<CParkAnalyser*>(gBlock)->Init(); }

// The challenge table: slots 1..601 for Load (602 numbers, slot 0 ignored), the main balance's twenty choices and the count slot, and what Load changes
// the choices to (when `hasAfter` is set).
void tpw_wg_settable(const int32_t* values) { std::memcpy(gTable, values, sizeof gTable); }
void tpw_wg_setmain(const int32_t* choices, int32_t count, const int32_t* after, int32_t hasAfter) {
    for (int32_t i = 0; i < 20; ++i) gMainBalance.mValues[kMainBalanceChallengesInThisLevel + i].mInteger = choices[i];
    gMainBalance.mValues[kMainBalanceChallengesInThisLevel + 20].mInteger = count;
    std::memcpy(gAfterLoad, after, sizeof gAfterLoad);
    gHaveAfter = hasAfter;
}
void tpw_wg_mainnow(int32_t* out) {
    for (int32_t i = 0; i < 21; ++i) out[i] = gMainBalance.mValues[kMainBalanceChallengesInThisLevel + i].mInteger;
}
void tpw_wg_hq() { reinterpret_cast<CChallengeHQ*>(gBlock)->Init(); }

}  // extern "C"
