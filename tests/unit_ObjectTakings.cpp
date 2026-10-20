// Test library for a ride's takings, costs, prizes and satisfaction (unit w198; libtpw_unit_ObjectTakings.dylib, tests/test_ObjectTakings.py), main app:
//   src/Object_takings.cpp   the 29 CObject members; real under them: src/ParkAnalyser_history.cpp (the history sums and averages) and
//   src/WorldSavvy.cpp (GetWorld).
//
// The callees outside the unit are fakes that record the same event as the PowerPC stubs and answer from the test's queues: GAME_ASSERTMSG, CTRACE, the
// ride database's GetRideInfo (answers one block the module fills), the world's three accessors, the bank account's deposit and withdrawal, the
// challenge counter, the map's two regional effectors, the two script calls, the world's random number, the game type's constructor, CParkTime::GetFunnyTime,
// TbTimeStamp::operator- and the string table's GetConstString. The worlds, the map, the bank account, the challenge HQ, the table and the script are small
// blocks that are only ever labelled; the analyser is a zeroed arena block of which only two history words are written.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "FS_Online.h"
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "ParkTime.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbFontRenderParams.h"
#include "TbITable.h"
#include "TbTextResourceManager.h"
#include "TbTimeDiff.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

namespace {
alignas(16) unsigned char gRideBlock[sizeof(RideInfo)];   // the one ride record GetRideInfo answers (host layout: 8 byte slots)
RSSE_SCRIPT* gScript;
CBankAccount* gBank;
CParkAnalyser* gAnalyser;
CChallengeHQ* gHQ;
int32_t gCtorType;   // what the game type's constructor makes
RideInfo& Ride() { return *reinterpret_cast<RideInfo*>(gRideBlock); }
}  // namespace

// ---- globals of other files, defined here
CWorld* gWorld;
CWorld* gGameWorld;
CMap* gWorldMap;
CRideDatabase gRideDatabase;
CText gText;
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {static_cast<int64_t>(level)}, {format}); }

CGameType::CGameType() {
    ev("CGameType", {1});
    mType = static_cast<GameType>(gCtorType);
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return &Ride();
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return gBank;
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return gAnalyser;
}

CChallengeHQ* CWorld::GetChallengeHQ() {
    ev("GetChallengeHQ", {lab(this)});
    return gHQ;
}

uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("rand"));
}

void CBankAccount::MakeDeposit(uint32_t amount) { ev("MakeDeposit", {lab(this), static_cast<int64_t>(amount)}); }

void CBankAccount::MakeWithdrawal(uint32_t amount) { ev("MakeWithdrawal", {lab(this), static_cast<int64_t>(amount)}); }

void CChallengeHQ::AddToCounter(ChallengeType type, int32_t amount, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj,
                                tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) {
    ev("AddToCounter", {lab(this), static_cast<int32_t>(type), amount, obj.mIndex, thing.mIndex});
}

void CMap::AddRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("AddRegionalEffector", {lab(this), static_cast<int32_t>(effector), cell.mIndex});
}

void CMap::RemoveRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("RemoveRegionalEffector", {lab(this), static_cast<int32_t>(effector), cell.mIndex});
}

RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script) {
    ev("RSSE_GetScriptPointer", {script});
    return gScript;
}

void RSSE_Setvar(RSSE_SCRIPT* script, int32_t index, int32_t value) { ev("RSSE_Setvar", {lab(script), index, value}); }

TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    ev("GetFunnyTime", {lab(this), lab(a), lab(b), lab(c), lab(d), lab(e), lab(f), lab(g)}, {});
    ev("GetFunnyTime.8", {lab(h)});
    return TbTimeStamp{ret("ft")};
}

TbTimeDiff TbTimeStamp::operator-(const TbTimeStamp& other) const {
    ev("Stamp-", {lab(this), mTicks, lab(&other), other.mTicks});
    TbTimeDiff diff;
    diff.mTicks = ret("minus");
    return diff;
}

const unsigned char* TbITable::GetConstString(uint32_t id) const {
    ev("GetConstString", {lab(this), static_cast<int64_t>(id)});
    return reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(ret("name")));
}

extern "C" {

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_otk_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_otk_free(uint64_t block) { tpw::ArenaFree(reinterpret_cast<void*>(static_cast<uintptr_t>(block))); }

// The address of the ride database global (the label of GetRideInfo's `this`).
uint64_t tpw_otk_ridedb() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gRideDatabase)); }

// The offsets the module relies on (HOST_LAYOUT).
int32_t tpw_otk_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mHistory1F100));
    out[n++] = static_cast<int64_t>(sizeof(CHistory<int, 144>));
    out[n++] = static_cast<int64_t>(sizeof(CHistory<uint32_t, 30>));
    out[n++] = static_cast<int64_t>(sizeof(CObject));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBase) + offsetof(CThing, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBase) + offsetof(CThing, mCellY));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBuilt));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF20));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF40));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF44));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF4C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF5A));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF60));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory6C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistoryF4));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory1A4));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory22C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory2B4));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory33C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF17C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF180));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mCostOfGoods));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF188));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mChanceOfNotLoosing));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mPricePerUse));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF194));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mState));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF19C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF1A0));
    return n;
}

// The pointers the code reaches, and what the game type's constructor makes. The blocks are only labelled, except the analyser.
void tpw_otk_env(uint64_t world, uint64_t gameWorld, uint64_t map, uint64_t table, uint64_t bank, uint64_t analyser, uint64_t hq, uint64_t script,
                 int32_t ctorType) {
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world));
    gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(gameWorld));
    gWorldMap = reinterpret_cast<CMap*>(static_cast<uintptr_t>(map));
    gText.mResourcePointers[4] = reinterpret_cast<void*>(static_cast<uintptr_t>(table));
    gBank = reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(bank));
    gAnalyser = reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser));
    gHQ = reinterpret_cast<CChallengeHQ*>(static_cast<uintptr_t>(hq));
    gScript = reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(script));
    gCtorType = ctorType;
}

// The ride record: the balance slots 0 to 159 (integers) and the UI type.
void tpw_otk_ride(const int32_t* values, int32_t uiType) {
    RideInfo& ride = Ride();
    for (int32_t i = 0; i < kRideSlot_SlotCount; i++) ride.mBalance.mValues[i].mInteger = values[i];
    ride.mUIType = uiType;
}

// The game type object (a function-local static, constructed by the first use): set before a call, read after it.
void tpw_otk_mode(int32_t type) { TheGameType().mType = static_cast<GameType>(type); }
int32_t tpw_otk_type() { return static_cast<int32_t>(TheGameType().mType); }

// ---- the functions, one wrapper each (the object is the first argument)
CObject* Ob(uint64_t object) { return reinterpret_cast<CObject*>(static_cast<uintptr_t>(object)); }

int32_t tpw_otk_age(uint64_t o) { return Ob(o)->GetObjectAge(); }
int32_t tpw_otk_life(uint64_t o) { return Ob(o)->GetRemainingLife(); }
void tpw_otk_walked(uint64_t o) { Ob(o)->WalkedAway(); }
void tpw_otk_customer(uint64_t o) { Ob(o)->HadCustomer(); }
void tpw_otk_takings(uint64_t o, uint32_t amount) { Ob(o)->RecordNewTakings(amount); }
void tpw_otk_cost(uint64_t o, uint32_t amount) { Ob(o)->RecordACost(amount); }
void tpw_otk_winner(uint64_t o) { Ob(o)->HadAWinner(); }
uint32_t tpw_otk_price(uint64_t o) { return Ob(o)->GetPricePerUse(); }
uint32_t tpw_otk_prize(uint64_t o) { return Ob(o)->GetPrizeValue(); }
void tpw_otk_setprize(uint64_t o, int32_t value) { Ob(o)->SetPrizeValue(value); }
int32_t tpw_otk_prospective(uint64_t o, float quality, float ingredient) { return Ob(o)->GetProspectiveCostOfGoods(quality, ingredient); }
int32_t tpw_otk_goods(uint64_t o) { return Ob(o)->GetCostOfGoods(); }
int32_t tpw_otk_revenue(uint64_t o) { return Ob(o)->GetTotalRevenue(); }
int32_t tpw_otk_profit(uint64_t o) { return Ob(o)->GetProfitability(); }
void tpw_otk_express(uint64_t o, int32_t value) { Ob(o)->ExpressSatisfaction(value); }
int32_t tpw_otk_satisfaction(uint64_t o) { return Ob(o)->GetCustomerSatisfaction(); }
uint32_t tpw_otk_pctwin(uint64_t o) { return Ob(o)->GetPercentageOfWinners(); }
uint32_t tpw_otk_quality(uint64_t o) { return Ob(o)->GetProductQuality(); }
void tpw_otk_setquality(uint64_t o, uint32_t value) { Ob(o)->SetProductQuality(static_cast<uint8_t>(value)); }
uint32_t tpw_otk_chance(uint64_t o) { return Ob(o)->GetChanceOfWinning(); }
void tpw_otk_setchance(uint64_t o, uint32_t value) { Ob(o)->SetChanceOfWinning(static_cast<uint8_t>(value)); }
void tpw_otk_setingredient(uint64_t o, int32_t value) { Ob(o)->SetAmountOfSpecialIngredient(value); }
uint32_t tpw_otk_ingredient(uint64_t o) { return Ob(o)->GetAmountOfSpecialIngredient(); }
uint64_t tpw_otk_ingname(uint64_t o) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Ob(o)->GetSpecialIngredientName())); }
int32_t tpw_otk_resipct(uint64_t o) { return Ob(o)->GetResidualValuePercentage(); }
uint32_t tpw_otk_resival(uint64_t o) { return Ob(o)->GetCurrentResidualValue(); }
void tpw_otk_toilet(uint64_t o, uint32_t amount) { Ob(o)->PersonUsedToilet(amount); }
int32_t tpw_otk_win(uint64_t o, uint64_t person) { return Ob(o)->WillIWin(reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))) ? 1 : 0; }
void tpw_otk_setprice(uint64_t o, int32_t value) { Ob(o)->SetPricePerUse(value); }

}  // extern "C"
