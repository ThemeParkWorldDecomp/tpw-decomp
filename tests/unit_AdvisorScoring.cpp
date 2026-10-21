// Test library for the advisor's message scoring (libtpw_unit_AdvisorScoring.dylib; see tests/test_AdvisorScoring.py): src/game/advisor/Advisor.cpp, src/game/advisor/AdvisorScoring.cpp (the
// constructor, the destructor and the ten queries of CAdvisorScoring), src/game/advisor/Advisor.cpp and the two generated tables, all real.
//
// Fakes that record through calllog: CBalanceTable::Init, Load and UnInit (the balance reader has its own test, test_BalanceTable), CTRACE,
// and the three scorers the table's pointers can name (_ADVISOR_NO_SCORE, _ADVISOR_WELCOME, _ADVISOR_OPEN_PARK). CGameType's constructor is
// a fake that records nothing and starts the type at gCtorType (the function-local object of TheGameType is made only once on the host, so
// the test sets its type before every call: see test_AdvisorScoring.py).
//
// The Python module sets the world with tpw_as_reset, tpw_as_patch (a field of a record), tpw_as_values (the balance table's slots) and
// tpw_as_build, calls one function with tpw_as_call and reads the object's words with tpw_as_dump.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "AdvisorScoring.h"
#include "Debug.h"
#include "FS_Online.h"
#include "calllog.h"

using namespace calllog;

// ---- fakes
void CBalanceTable::Init() { ev("Balance.Init", {newlab(this)}); }
void CBalanceTable::UnInit() { ev("Balance.UnInit", {lab(this)}); }
int32_t CBalanceTable::Load(const char* path, LoadMode mode) {
    ev("Balance.Load", {lab(this), mode}, {path});
    return static_cast<int32_t>(ret("Balance.Load"));
}

void CTRACE(uint32_t level, const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    ev("CTRACE", {level}, {text});
}

namespace {
int32_t gCtorType;
}
CGameType::CGameType() : mType(static_cast<GameType>(gCtorType)) {}

// The scorers: record the call (with the object they were called on) and answer the queue "Scorer".
int32_t CAdvisorScoring::_ADVISOR_NO_SCORE() {
    ev("ADVISOR_NO_SCORE", {lab(this)});
    return static_cast<int32_t>(ret("Scorer"));
}
int32_t CAdvisorScoring::_ADVISOR_WELCOME() {
    ev("ADVISOR_WELCOME", {lab(this)});
    return static_cast<int32_t>(ret("Scorer"));
}
int32_t CAdvisorScoring::_ADVISOR_OPEN_PARK() {
    ev("ADVISOR_OPEN_PARK", {lab(this)});
    return static_cast<int32_t>(ret("Scorer"));
}

namespace {

constexpr int kLabelObject = 2;
constexpr int kLabelBalance = 3;

AdvisorMessageRecord gPristine[kAdvisorMessageCount + 2];
bool gHavePristine;

CAdvisorScoring* gThis;                 // the object under test
alignas(CAdvisorScoring) unsigned char gBlock[sizeof(CAdvisorScoring)];   // the block the constructor test builds in
CAdvisorScoring* gInBlock;

AdvisorScorer ScorerOf(int index) {
    switch (index) {
    case 0: return &CAdvisorScoring::_ADVISOR_NO_SCORE;
    case 1: return &CAdvisorScoring::_ADVISOR_WELCOME;
    default: return &CAdvisorScoring::_ADVISOR_OPEN_PARK;
    }
}

}  // namespace

extern "C" {

// A fresh world: the table as the file has it, every record's scorer i % 3, the game type object at `type` (what the Mac's object holds if it was
// made already, else what the constructor makes: the module passes the one that will be seen), no object.
void tpw_as_reset(int32_t type, int32_t ctorType) {
    if (!gHavePristine) {
        std::memcpy(gPristine, gAdvisorMessageBlock, sizeof gPristine);
        gHavePristine = true;
    }
    std::memcpy(gAdvisorMessageBlock, gPristine, sizeof gPristine);
    for (int i = 0; i < kAdvisorMessageCount; ++i) gAdvisorMessages[i].mScorer = ScorerOf(i % 3);
    gCtorType = ctorType;
    TheGameType().mType = static_cast<GameType>(type);
    gThis = nullptr;
    gInBlock = nullptr;
}

// Sets a field of a record: 0 mSkipped, 1 mMessage, 2 mCategory, 3 mMode, 7 mMaxDuplicates, 8 mFirstResponse, 9 mNumResponses, 10 mCycling, 11 mWord2c
// (the Mac's word numbers), 12 the scorer (0, 1, 2).
void tpw_as_patch(int32_t index, int32_t field, int64_t value) {
    AdvisorMessageRecord& r = gAdvisorMessages[index];
    const int32_t v = static_cast<int32_t>(value);
    switch (field) {
    case 0: r.mSkipped = v; break;
    case 1: r.mMessage = v; break;
    case 2: r.mCategory = v; break;
    case 3: r.mMode = v; break;
    case 7: r.mMaxDuplicates = v; break;
    case 8: r.mFirstResponse = v; break;
    case 9: r.mNumResponses = v; break;
    case 10: r.mCycling = v; break;
    case 11: r.mWord2c = v; break;
    case 12: r.mScorer = ScorerOf(v); break;
    }
}

// Builds the object under test (with the log off: the constructor runs) and fills its balance slots; label 2 is the object, 3 its table.
void tpw_as_build(const int64_t* values) {
    void* memory = ::operator new(sizeof(CAdvisorScoring));
    std::memset(memory, 0xa5, sizeof(CAdvisorScoring));   // (as the PowerPC side's memory is: the words the constructor leaves show)
    gThis = new (memory) CAdvisorScoring;
    for (int i = 0; i < kAdvisorBalanceSlots; ++i) gThis->mBalance.mValues[i].mInteger = static_cast<int32_t>(values[i]);
    gThis->mCursor = static_cast<int32_t>(values[kAdvisorBalanceSlots]);
    tpw_cl_label(kLabelObject, gThis);
    tpw_cl_label(kLabelBalance, &gThis->mBalance);
}

// Makes the block the constructor test builds in: filled with 0xa5, as the PowerPC side's memory is.
void tpw_as_block() { std::memset(gBlock, 0xa5, sizeof gBlock); }

// The calls. a[0]: 0 GetScoreFor, 1 GetNumResponsesFor, 2 GetMaxDuplicatesInQueueFor, 3 GetMinTimeFor, 4 SayOnlyOnce, 5 GetCyclingFor,
// 6 GetMessageCategory, 7 HasThisBeenKilledTooManyTimes(message, a[2]), 8 IsThisValidInTheCurrentMode, 9 IsThisATutorialMessage,
// 10 the constructor (in the block), 11 the destructor in place, 12 delete.
int64_t tpw_as_call(const int64_t* a) {
    const auto message = static_cast<AdvisorMessage>(a[1]);
    switch (a[0]) {
    case 0: return gThis->GetScoreFor(message);
    case 1: return gThis->GetNumResponsesFor(message);
    case 2: return gThis->GetMaxDuplicatesInQueueFor(message);
    case 3: return gThis->GetMinTimeFor(message);
    case 4: return gThis->SayOnlyOnce(message);
    case 5: return gThis->GetCyclingFor(message);
    case 6: return gThis->GetMessageCategory(message);
    case 7: return gThis->HasThisBeenKilledTooManyTimes(message, static_cast<int32_t>(a[2]));
    case 8: return gThis->IsThisValidInTheCurrentMode(message);
    case 9: return gThis->IsThisATutorialMessage(message);
    case 10:
        gInBlock = new (gBlock) CAdvisorScoring;
        return 0;
    case 11:
        gThis->~CAdvisorScoring();
        return 0;
    case 12:
        delete gThis;
        gThis = nullptr;
        return 0;
    }
    return -999;
}

// The words the calls can change: the object's three first words and the first four balance slots (of the object under test, or of the block's).
void tpw_as_dump(int64_t* out) {
    const CAdvisorScoring* p = gInBlock != nullptr ? gInBlock : gThis;
    int n = 0;
    if (p != nullptr) {
        out[1 + n++] = static_cast<uint32_t>(p->mCursor);
        out[1 + n++] = p->mWord4;
        out[1 + n++] = p->mWord8;
        for (int i = 0; i < 4; ++i) out[1 + n++] = static_cast<uint32_t>(p->mBalance.mValues[i].mInteger);
    }
    out[0] = n;
}

// CAdvisorBalance::GetBalanceDescription(row): the row as numbers (type, the 32 name bytes, the two integers, the two floats' bits, the array count,
// the flags), 39 in all.
void tpw_as_desc(int32_t row, int64_t* out) {
    static CAdvisorBalance balance;
    const BalanceDescription* d = balance.GetBalanceDescription(row);
    int n = 0;
    out[n++] = d->mType;
    for (int i = 0; i < 32; ++i) out[n++] = static_cast<uint8_t>(d->mName[i]);
    out[n++] = d->mMinInteger;
    out[n++] = d->mMaxInteger;
    uint32_t bits;
    std::memcpy(&bits, &d->mMinFloat, 4);
    out[n++] = bits;
    std::memcpy(&bits, &d->mMaxFloat, 4);
    out[n++] = bits;
    out[n++] = d->mArrayCount;
    out[n++] = d->mFlags;
}

// CAdvisorBalance::GetBalanceValuesStart: 1 when it answers the first value slot.
int64_t tpw_as_values_start() {
    static CAdvisorBalance balance;
    return balance.GetBalanceValuesStart() == &balance.mValues[0];
}

void tpw_as_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) out[i] = tpw_cl_heap[i];
}

}  // extern "C"
