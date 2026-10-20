// Differential library for unit w124: the things' turns (CWeather::UpdateTurn src/Weather_turn.cpp, CAdvisor::UpdateTurn src/Advisor_turn.cpp,
// CChallengeHQ::UpdateTurn src/ChallengeHQ_turn.cpp, CParkAnalyser::UpdateTurn and CParkAnalyserCell::Init src/ParkAnalyser_turn.cpp,
// SubscriptionTree::insert_one src/msl_tree_insert.cpp) and the nine companions of src/ThingMessages.cpp.
//
// Every callee outside those is a fake that records the same event as the PowerPC side's stub. The worlds are not blocks of their Mac size: only the 0x80
// bytes at world + 0x1da6f0 are real memory (gTail), and a world pointer is that block's address minus 0x1da6f0 on both sides. The object under test lives
// in gObj over a guard of 0x40 bytes on each side, so a stray store shows. Objects are reported as call-log labels (see test_ThingTurns.py).
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "MainBalance.h"
#include "MslSubscriptionTree.h"
#include "TPSound.h"
#include "TbTimeDiff.h"
#include "ThingMessages.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "calllog.h"
#include "tb_rain.h"

using namespace calllog;

namespace {

constexpr int32_t kTail = 0x1da6f0;
constexpr int32_t kTailSize = 0x80;
constexpr int32_t kObjAt = 0x40;
constexpr int32_t kObjSize = 0x1700 + 0x80;

alignas(16) unsigned char gTail[2][kTailSize];
alignas(16) unsigned char gObj[kObjAt + kObjSize];
alignas(16) unsigned char gCentre[0x1d0];
alignas(16) unsigned char gRain[0x100];
alignas(16) unsigned char gChal[4][0x100];      // 0x40 into each: a CChallenge
alignas(16) unsigned char gTree[0x40];
alignas(16) unsigned char gNodes[8][0x60];      // host nodes (node_base 0x18 bytes + the value)
alignas(16) unsigned char gNewNode[0x60];
alignas(16) unsigned char gKey[0x10];
alignas(16) unsigned char gMsg[0x80];
CAdvisorScoring gScoringObj;
uint8_t gGuard;                 // the Mac's guard byte at 0x102b4100
void* gNewCentre;
void* gAllocated[8];
int32_t gAllocatedCount;
int32_t gWorldSel, gGameSel;

CWorld* WorldPtr(int32_t k) { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTail[k]) - kTail); }
CWeather* Weather() { return reinterpret_cast<CWeather*>(gObj + kObjAt); }
CChallengeHQ* Hq() { return reinterpret_cast<CChallengeHQ*>(gObj + kObjAt); }
CAdvisor* Advisor() { return reinterpret_cast<CAdvisor*>(gObj + kObjAt); }
CChallenge* Chal(int32_t k) { return reinterpret_cast<CChallenge*>(gChal[k] + kObjAt); }

uint32_t Bits(float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    return v;
}
void WriteBits(float* f, uint32_t v) { std::memcpy(f, &v, 4); }

// The worlds the globals name after a call, as the queues "swapg" and "swapw" say (-1 or an exhausted queue: no change; the last value repeats).
void Swap() {
    const int64_t g = ret("swapg", -1);
    if (g >= 0) { gGameSel = static_cast<int32_t>(g); gGameWorld = WorldPtr(gGameSel); }
    const int64_t w = ret("swapw", -1);
    if (w >= 0) { gWorldSel = static_cast<int32_t>(w); gWorld = WorldPtr(gWorldSel); }
}

}  // namespace

// ---- The unit's globals ---------------------------------------------------------------------------------------------------------------
CWorld* gWorld;
CWorld* gGameWorld;
CMessageCentre* gMessageCentre;
CRain* gei_Rain;
int32_t gSoundBanks[8];

void CBalanceTable::Init() {}
void CBalanceTable::UnInit() {}

// ---- The advisor's scoring: a function-local static on the Mac, guarded by a byte -------------------------------------------------------
CAdvisorScoring& TheScoring() {
    if (static_cast<int8_t>(gGuard) == 0) {
        ev("Scoring.Ctor", {lab(&gScoringObj)});
        gGuard = 1;
    }
    return gScoringObj;
}
CAdvisorScoring::CAdvisorScoring() {}
CAdvisorScoring::~CAdvisorScoring() {}
const BalanceDescription* CAdvisorBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CAdvisorBalance::GetBalanceValuesStart() { return mValues; }
AdvisorMessage CAdvisorScoring::GetHighestScoringFromNext(int32_t count) {
    ev("Highest", {lab(this), count});
    return static_cast<AdvisorMessage>(ret("best"));
}
int32_t CAdvisorScoring::GetScoreFor(AdvisorMessage message) {
    ev("score", {lab(this), message});
    return static_cast<int32_t>(ret("score"));
}
int32_t CAdvisorScoring::GetNumResponsesFor(AdvisorMessage message) const {
    ev("num", {lab(this), message});
    return static_cast<int32_t>(ret("num"));
}
int32_t CAdvisorScoring::GetCyclingFor(AdvisorMessage message) const {
    ev("cycling", {lab(this), message});
    return static_cast<int32_t>(ret("cycling"));
}

// ---- The world, the clock ------------------------------------------------------------------------------------------------------------
CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {lab(this)});
    return *gWorld;
}
uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    const uint32_t value = static_cast<uint32_t>(ret("rand"));
    Swap();
    return value;
}
TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    ev("GetFunnyTime", {lab(this), lab(a), lab(b), lab(c), lab(d), lab(e), lab(f), lab(g)}, {});
    ev("GetFunnyTime.8", {lab(h)});
    const int64_t ticks = ret("ft");
    Swap();
    return TbTimeStamp{ticks};
}
TbError TbTimeDiff::Set(int32_t days, int32_t hours, int32_t minutes, int32_t seconds, int32_t millis) {
    ev("DiffSet", {days, hours, minutes, seconds, millis});
    mTicks = static_cast<int64_t>(days) * 864000000000LL;
    return TbError(0);
}
bool TbTimeStamp::operator>(const TbTimeStamp& other) const {
    ev("Stamp>", {lab(this), mTicks, lab(&other), other.mTicks});
    return ret("gt") != 0;
}
TbTimeDiff TbTimeStamp::operator-(const TbTimeStamp& other) const {
    ev("Stamp-", {lab(this), mTicks, lab(&other), other.mTicks});
    TbTimeDiff diff;
    diff.mTicks = ret("minus");
    return diff;
}
TbTimeStamp TbTimeStamp::operator+(const TbTimeDiff& diff) const {
    ev("Stamp+", {lab(this), mTicks, diff.mTicks});
    return TbTimeStamp{ret("plus")};
}

// ---- The weather's callees -------------------------------------------------------------------------------------------------------------
uint32_t CWeather::FindNewQuality() const {
    ev("FindNewQuality", {lab(this)});
    return static_cast<uint32_t>(ret("fnq"));
}
void CWeather::SetPrecipitationType(PrecipitationType type) { ev("SetPrecipitationType", {lab(this), static_cast<int32_t>(type)}); }
void CWeather::FindLightningCoords(D3DVECTOR& start, D3DVECTOR& end, int32_t strikesLeft) {
    ev("FindLightningCoords", {lab(this), strikesLeft});
    WriteBits(&start.x, static_cast<uint32_t>(ret("fl")));
    WriteBits(&start.y, static_cast<uint32_t>(ret("fl")));
    WriteBits(&start.z, static_cast<uint32_t>(ret("fl")));
    WriteBits(&end.x, static_cast<uint32_t>(ret("fl")));
    WriteBits(&end.y, static_cast<uint32_t>(ret("fl")));
    WriteBits(&end.z, static_cast<uint32_t>(ret("fl")));
}
void CWeatherForecast::Invalidate() {
    ev("Invalidate", {lab(this)});
    if (ret("invclear", 1) != 0) mF0C = 0;
}
void CRain::SetRaindrops(int32_t count) { ev("SetRaindrops", {lab(this), count}); }
int32_t CRain::AddLightning(D3DVECTOR start, D3DVECTOR end, int32_t a, int32_t b) {
    ev("AddLightning", {lab(this), Bits(start.x), Bits(start.y), Bits(start.z), Bits(end.x), Bits(end.y), Bits(end.z), a, b});
    return 0;
}
void SND_SetRainLevel(int32_t level) { ev("SND_SetRainLevel", {level}); }
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return 0;
}
void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {level}, {format}); }

// ---- The advisor's callees ------------------------------------------------------------------------------------------------------------
CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                                 int32_t responseNo, bool overrideOnlyOnce, int32_t score) {
    ev("RequestCtor", {message, thingId.mIndex, secondary, responseNo, overrideOnlyOnce, score});
    mThingId = thingId.mIndex;
    mSecondary = secondary;
    mResponseNo = responseNo;
    mMessage = message;
    mScore = score;
    mValid = 1;
    mOverrideOnlyOnce = overrideOnlyOnce;
    mScoreOverridden = 0;
}
int32_t CAdvisor::QueueRequest(const CAdvisorRequest& request) {
    ev("QueueRequest", {lab(this), request.mThingId, request.mSecondary, request.mResponseNo, request.mMessage, request.mScore, request.mValid,
                        request.mOverrideOnlyOnce, request.mScoreOverridden});
    return static_cast<int32_t>(ret("queued"));
}
bool CAdvisor::IsPlaying() {
    ev("IsPlaying", {lab(this)});
    return (ret("playing") & 0xff) != 0;
}
void CAdvisor::SetPlayingFor(uint32_t length) { ev("SetPlayingFor", {lab(this), length}); }
int32_t CAdvisor::FindHighestScoringSlotInQueue() {
    ev("FindHighest", {lab(this)});
    const int64_t threshold = ret("thmut", -1);
    if (threshold != -1) gScoringObj.mBalance.mValues[kAdvisorBalanceMinScore].mInteger = static_cast<int32_t>(threshold);
    return static_cast<int32_t>(ret("slot"));
}
int32_t CAdvisorRequest::Play(uint32_t* length, int32_t responseNo) {
    ev("Play", {lab(this), responseNo});
    *length = static_cast<uint32_t>(ret("len"));
    return static_cast<int32_t>(ret("play"));
}
int32_t CAdvisorRequest::PlayWithoutScore(uint32_t* length, int32_t responseNo) {
    ev("PlayWithoutScore", {lab(this), responseNo});
    *length = static_cast<uint32_t>(ret("len"));
    return static_cast<int32_t>(ret("play"));
}

// ---- The challenge hall's callees ------------------------------------------------------------------------------------------------------
bool CChallengeHQ::IsSystemOn() {
    ev("IsSystemOn", {lab(this)});
    return (ret("sys") & 0xff) != 0;
}
CChallenge* CChallengeHQ::GetCurrentChallenge() {
    ev("GetCurrent", {lab(this)});
    const int64_t which = ret("cur");
    Swap();
    return which == 0 ? nullptr : Chal(static_cast<int32_t>(which) - 1);
}
void CChallengeHQ::OfferNextChallenge(bool force) { ev("OfferNext", {lab(this), force}); }
void CChallengeHQ::EndCurrentChallenge() { ev("EndCurrent", {lab(this)}); }
void CChallengeHQ::WonCurrentChallenge() { ev("WonCurrent", {lab(this)}); }
bool CChallenge::DidThisWinIt(int32_t& value, bool atEnd) {
    ev("DidWin", {lab(this), lab(&value), value, atEnd});
    if (ret("winmut", 0) != 0) value = static_cast<int32_t>(ret("winval"));
    return (ret("won") & 0xff) != 0;
}
int32_t CChallenge::GetActiveId() const {
    ev("GetActiveId", {lab(this)});
    return static_cast<int32_t>(ret("aid"));
}

// ---- The message centre ------------------------------------------------------------------------------------------------------------------
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    const int32_t type = message.GetType();
    if (type == 1) {
        const auto& m = static_cast<const CMsgWeather&>(message);
        ev("PostMessage", {lab(this), type, message.mFlags, m.mQuality});
    } else if (type == 2) {
        const auto& m = static_cast<const CMsgWeatherWarning&>(message);
        ev("PostMessage", {lab(this), type, message.mFlags, m.mQuality, m.mDays});
    } else {
        const auto& m = static_cast<const CMsgChallenge&>(message);
        ev("PostMessage", {lab(this), type, message.mFlags, m.mTagId, m.mMessage, m.mFlag, m.mPrize, m.mTargetTime, m.mTargetVal, m.mDays, m.mValue,
                           m.mObj});
        ev("PostMessage.2", {m.mObj2, m.mStaff});
    }
    return static_cast<int32_t>(ret("post"));
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

// ---- The tree's node insertion -----------------------------------------------------------------------------------------------------------
SubscriptionTree::node_base* SubscriptionTree::insert_node_at(node_base* parent, bool left, bool leftmost, const CSubscription& key) {
    ev("insert_node_at", {lab(this), lab(parent), left, leftmost, lab(&key), key.mThingId});
    const int64_t which = ret("ins");
    if (which == 0) return nullptr;
    if (which == 100) return reinterpret_cast<node_base*>(gNewNode);
    return reinterpret_cast<node_base*>(gNodes[which - 1]);
}

// ---- The test entry points ---------------------------------------------------------------------------------------------------------------
extern "C" {

// 0, 1 the world tails; 2 the object block (the object at +0x40); 3 the centre block; 4 the rain block; 5 + k a challenge block (the object at +0x40);
// 10 the tree; 11 + k a node; 19 the new node; 20 the key; 21 the scoring object.
void* tpw_tt_addr(int32_t which) {
    if (which == 0 || which == 1) return gTail[which];
    if (which >= 5 && which < 9) return gChal[which - 5];
    if (which >= 11 && which < 19) return gNodes[which - 11];
    switch (which) {
    case 2: return gObj;
    case 3: return gCentre;
    case 4: return gRain;
    case 10: return gTree;
    case 19: return gNewNode;
    case 20: return gKey;
    case 22: return &reinterpret_cast<SubscriptionTree*>(gTree)->mRoot;
    case 21: return &gScoringObj;
    default: return nullptr;
    }
}
void* tpw_tt_world(int32_t k) { return WorldPtr(k); }

// The globals before a case: the worlds named by index, the centre block (0 for none), the rain block, the first sound bank, the guard byte and
// the scoring threshold; the allocations of the last case are freed.
void tpw_tt_prepare(int32_t world, int32_t game, int32_t centre, int32_t rain, int32_t bank, int32_t guard, int32_t threshold) {
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    gNewCentre = nullptr;
    gWorldSel = world;
    gGameSel = game;
    gWorld = WorldPtr(world);
    gGameWorld = WorldPtr(game);
    gMessageCentre = centre != 0 ? reinterpret_cast<CMessageCentre*>(gCentre) : nullptr;
    gei_Rain = rain != 0 ? reinterpret_cast<CRain*>(gRain) : nullptr;
    gSoundBanks[0] = bank;
    gGuard = static_cast<uint8_t>(guard);
    gScoringObj.mBalance.mValues[kAdvisorBalanceMinScore].mInteger = threshold;
}
void tpw_tt_balance(int32_t slot, int32_t value) { gMainBalance.mValues[slot].mInteger = value; }
// What is left after a call: the label of the centre global, the allocations, the world indices, the guard byte and the scoring threshold.
void tpw_tt_globals(int64_t* out) {
    out[0] = lab(gMessageCentre);
    out[1] = gAllocatedCount;
    out[2] = gWorldSel;
    out[3] = gGameSel;
    out[4] = gGuard;
    out[5] = gScoringObj.mBalance.mValues[kAdvisorBalanceMinScore].mInteger;
}
void tpw_tt_run(int32_t which) {
    switch (which) {
    case 0: Weather()->UpdateTurn(); break;
    case 1: Advisor()->UpdateTurn(); break;
    case 2: Hq()->UpdateTurn(); break;
    default: reinterpret_cast<CParkAnalyser*>(gObj + kObjAt)->UpdateTurn(); break;
    }
}
int32_t tpw_tt_cell(int32_t a, int32_t b, int32_t c, int32_t d) { return reinterpret_cast<CParkAnalyserCell*>(gObj + kObjAt)->Init(a, b, c, d); }

// The tree: node k (1 up) has the children left[k-1] and right[k-1] (node numbers, 0 none) and the value values[k-1]; root is a node number (0 none).
void tpw_tt_set_tree(int32_t root, const int32_t* left, const int32_t* right, const uint32_t* values, int32_t n, uint32_t count) {
    using node = SubscriptionTree::node;
    std::memset(gTree, 0xa5, sizeof gTree);
    std::memset(gNodes, 0xa5, sizeof gNodes);
    auto at = [](int32_t k) -> SubscriptionTree::node_base* { return k == 0 ? nullptr : reinterpret_cast<SubscriptionTree::node_base*>(gNodes[k - 1]); };
    for (int32_t i = 0; i < n; ++i) {
        node* x = reinterpret_cast<node*>(gNodes[i]);
        x->mLeft = at(left[i]);
        x->mRight = at(right[i]);
        x->mValue.mThingId = static_cast<uint16_t>(values[i]);
    }
    SubscriptionTree* tree = reinterpret_cast<SubscriptionTree*>(gTree);
    tree->mCount = count;
    tree->mRoot = at(root);
}

// insert_one on the tree block with the key block's first halfword; out = the label of the node, the inserted byte.
void tpw_tt_insert(int64_t* out) {
    SubscriptionTree* tree = reinterpret_cast<SubscriptionTree*>(gTree);
    const CSubscription* key = reinterpret_cast<const CSubscription*>(gKey);
    const SubscriptionInsertResult result = tree->insert_one(*key);
    out[0] = lab(result.mNode);
    out[1] = result.mInserted;
}

// The companions. kind 0 weather, 1 warning, 2 challenge. out of the constructor: the Mac-ordered payload words, the flags and the type; a byte map
// of what is intact is returned in `intact`.
void tpw_tt_ctor(int32_t kind, const int64_t* args, int64_t* out) {
    std::memset(gMsg, 0xa5, sizeof gMsg);
    unsigned char* begin = gMsg;
    CMsg* base = nullptr;
    unsigned char* written[16][2];
    int32_t nwritten = 0;
    auto note = [&](const void* p, size_t n) { written[nwritten][0] = reinterpret_cast<unsigned char*>(const_cast<void*>(p)); written[nwritten++][1] = reinterpret_cast<unsigned char*>(const_cast<void*>(p)) + n; };
    if (kind == 0) {
        auto* m = new (begin) CMsgWeather(static_cast<int32_t>(args[0]));
        base = m;
        note(&m->mFlags, 4); note(&m->mQuality, 4);
        out[2] = m->mQuality;
    } else if (kind == 1) {
        auto* m = new (begin) CMsgWeatherWarning(static_cast<int32_t>(args[0]), static_cast<int32_t>(args[1]));
        base = m;
        note(&m->mFlags, 4); note(&m->mQuality, 4); note(&m->mDays, 4);
        out[2] = m->mQuality;
        out[3] = m->mDays;
    } else {
        auto* m = new (begin) CMsgChallenge(static_cast<int32_t>(args[0]), static_cast<AdvisorMessage>(args[1]), args[2] != 0, static_cast<int32_t>(args[3]),
                                            static_cast<int32_t>(args[4]), static_cast<int32_t>(args[5]), static_cast<int32_t>(args[6]),
                                            static_cast<int32_t>(args[7]), tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(args[8])},
                                            tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(args[9])}, static_cast<StaffType>(args[10]));
        base = m;
        note(&m->mFlags, 4); note(&m->mTagId, 4); note(&m->mMessage, 4); note(&m->mFlag, 1); note(&m->mPrize, 4); note(&m->mTargetTime, 4);
        note(&m->mTargetVal, 4); note(&m->mDays, 4); note(&m->mValue, 4); note(&m->mObj, 2); note(&m->mObj2, 2); note(&m->mStaff, 4);
        out[2] = m->mTagId; out[3] = m->mMessage; out[4] = m->mFlag; out[5] = m->mPrize; out[6] = m->mTargetTime; out[7] = m->mTargetVal;
        out[8] = m->mDays; out[9] = m->mValue; out[10] = m->mObj; out[11] = m->mObj2; out[12] = m->mStaff;
    }
    out[0] = base->mFlags;
    out[1] = base->GetType();
    bool intact = true;
    note(begin, sizeof(void*));   // the vtable
    for (size_t i = 0; i < sizeof gMsg; ++i) {
        bool member = false;
        for (int32_t k = 0; k < nwritten; ++k) member = member || (begin + i >= written[k][0] && begin + i < written[k][1]);
        if (!member && gMsg[i] != 0xa5) intact = false;
    }
    out[13] = intact ? 1 : 0;
}
// GetType of a message holding the given flags (the answer must not depend on them).
int32_t tpw_tt_gettype(int32_t kind, uint32_t flags) {
    std::memset(gMsg, 0xa5, sizeof gMsg);
    CMsg* m;
    if (kind == 0) m = new (gMsg) CMsgWeather(7);
    else if (kind == 1) m = new (gMsg) CMsgWeatherWarning(7, 8);
    else m = new (gMsg) CMsgChallenge(1, kAdvisorNoMessage, false, 0, 0, 0, 0, 0, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{0},
                                      tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{0}, static_cast<StaffType>(0));
    m->mFlags = flags;
    return m->GetType();
}
// The destructor with the Mac's delete flag: a delete-expression for a positive (signed 16 bit) flag, else an explicit call; out = the flags left behind.
void tpw_tt_dtor(int32_t kind, int32_t flag, int32_t present, uint32_t* out) {
    out[0] = 0;
    if (present == 0) return;
    CMsg* m;
    if (static_cast<int16_t>(flag) > 0) {
        if (kind == 0) m = new CMsgWeather(5);
        else if (kind == 1) m = new CMsgWeatherWarning(5, 6);
        else m = new CMsgChallenge(1, kAdvisorNoMessage, false, 0, 0, 0, 0, 0, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{0},
                                   tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{0}, static_cast<StaffType>(0));
        --tpw_cl_heap[0];
        tpw_cl_label(3, m);
        delete m;
        return;
    }
    std::memset(gMsg, 0xa5, sizeof gMsg);
    if (kind == 0) { auto* w = new (gMsg) CMsgWeather(5); w->mFlags = 0x11223344; w->~CMsgWeather(); std::memcpy(&out[0], &w->mFlags, 4); }
    else if (kind == 1) { auto* w = new (gMsg) CMsgWeatherWarning(5, 6); w->mFlags = 0x11223344; w->~CMsgWeatherWarning(); std::memcpy(&out[0], &w->mFlags, 4); }
    else {
        auto* w = new (gMsg) CMsgChallenge(1, kAdvisorNoMessage, false, 0, 0, 0, 0, 0, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{0},
                                           tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{0}, static_cast<StaffType>(0));
        w->mFlags = 0x11223344;
        w->~CMsgChallenge();
        std::memcpy(&out[0], &w->mFlags, 4);
    }
}

}  // extern "C"
