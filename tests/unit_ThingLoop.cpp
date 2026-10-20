// Unit w123a: recording fakes for every callee outside the seven entry points.
#include <cstdint>
#include <cstring>
#include <new>
#include "calllog.h"
#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "Debug.h"
#include "FS_Online.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "World.h"

using namespace calllog;

uint32_t ThingLoopWorldRand(CWorld*) __asm__("__ZN6CWorld7GetRandEv");
void ThingLoopWorldPending(CWorld*, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>)
    __asm__("__ZN6CWorld22AddToPendingDeleteListE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE");
int32_t ThingLoopSeason(const CParkTime*) __asm__("__ZNK9CParkTime9GetSeasonEv");

CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gWorld;
CWorld* gGameWorld;
CMessageCentre* gMessageCentre;
uint16_t gThingLoopEmptyCellId;

namespace {
alignas(16) uint8_t gBlock[0x1730];
alignas(16) uint8_t gWorlds[2][0x2e0];
alignas(16) uint8_t gCentre[0x1f0];   // one padded header permits the signed -1 case
alignas(16) uint8_t gFresh[0x1d0];
alignas(16) uint8_t gCell[0x84];
alignas(16) uint8_t gThings[5][0x4c];
void* gNewCentre;
int32_t gOp;
int32_t gCtorType;
int32_t gPostKind;
int32_t gFinalId;
int32_t gAssertHeads[2];
int32_t gAssertIndex;
CThingArrayEntry gInitialTable[kThingArraySize];
void* gDeathVptr;
const CMsgDeath* gPostedMessage;
bool gPoisonMessage;

CThing* Thing() { return reinterpret_cast<CThing*>(gBlock); }
CMapCell* Cell() { return reinterpret_cast<CMapCell*>(gCell); }
CMessageCentre* Existing() { return reinterpret_cast<CMessageCentre*>(gCentre + 0x10); }
uint32_t Snap(const CThing* thing) {
    return (uint32_t(thing->mThingId) << 16) | (uint32_t(thing->mKind) << 8) | thing->mDead;
}
// Snapshot all four advisor words at each insertion to check stores follow every call.
uint32_t Word(const void* p, int32_t offset) {
    uint32_t v;
    std::memcpy(&v, static_cast<const uint8_t*>(p) + offset, 4);
    return v;
}
int32_t CentreLabel() { return lab(gMessageCentre); }
// A retained post pointer checks the inlined death-vptr reset before the switch.
void DeathState() {
    if (!gPoisonMessage) return;
    void* current;
    std::memcpy(&current, static_cast<const void*>(gPostedMessage), sizeof current);
    ev("DeathRestored", {current == gDeathVptr, gPostedMessage->mFlags, gPostedMessage->mThingId});
}
void Final(const char* name, void* p) {
    DeathState();
    ev(name, {lab(p), Snap(static_cast<CThing*>(p))});
    if (gFinalId >= 0) static_cast<CThing*>(p)->mThingId = static_cast<uint16_t>(gFinalId);
}
void Turn(const char* name, void* p) { ev(name, {lab(p), Snap(static_cast<CThing*>(p))}); }
void ChangeWorld() {
    const int32_t next = static_cast<int32_t>(ret("worldnext", -1));
    if (next >= 0) gGameWorld = reinterpret_cast<CWorld*>(gWorlds[next]);
}
}

// Balance lifetime is isolated from file loading; only the weather slots are used.
void CBalanceTable::Init() {}
CMainBalance::~CMainBalance() {}
const BalanceDescription* CMainBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CMainBalance::GetBalanceValuesStart() { return mValues; }
CMainBalance gMainBalance;

void GAME_ASSERTMSG(int32_t condition, const char* text, ...) {
    if (gOp == 1) DeathState();
    ev("GAME_ASSERTMSG", {condition, Snap(Thing()), Cell()->mWho}, {text});
    if (gOp == 2 && gAssertIndex < 2) {
        const int32_t head = gAssertHeads[gAssertIndex++];
        if (head >= 0) Cell()->mWho = static_cast<uint16_t>(head);
    }
    if (gOp == 1 && gFinalId >= 0) Thing()->mThingId = static_cast<uint16_t>(gFinalId);
}
void CTRACE(uint32_t level, const char* text, ...) { ev("CTRACE", {level}, {text}); }
CGameType::CGameType() {
    ev("CGameType", {1});
    mType = static_cast<GameType>(gCtorType);
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    const uint16_t answer = static_cast<uint16_t>(ret("id", mThingId));
    ev("GetThingId", {lab(this), Snap(this), answer});
    return {answer};
}
// The canonical death constructor/type are test support, not production translations.
CMsgDeath::CMsgDeath(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    ev("CMsgDeath", {id.mIndex});
    mFlags = 0;
    mThingId = id.mIndex;
    std::memcpy(&gDeathVptr, static_cast<const void*>(this), sizeof gDeathVptr);
}
int32_t CMsgDeath::GetType() const { return 0x1b; }
// Unit w123c: the destructor is out of line in the production class; this library keeps its own empty one (the stack message of KillMe).
CMsgDeath::~CMsgDeath() {}
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    const auto& death = static_cast<const CMsgDeath&>(message);
    ev("PostMessage", {lab(this), 27, death.mFlags, death.mThingId, Snap(Thing())});
    gPostedMessage = &death;
    gPoisonMessage = ret("poisonmsg", 0) != 0;
    if (gPoisonMessage) {
        auto* writable = const_cast<CMsgDeath*>(&death);
        std::memset(static_cast<void*>(writable), 0, sizeof(void*));
        writable->mFlags = 0x87654321;
        writable->mThingId = 0x8001;
    }
    if (gPostKind >= 0) Thing()->mKind = static_cast<uint8_t>(gPostKind);
    return static_cast<int32_t>(ret("post", 1));
}
void ThingLoopWorldPending(CWorld* world, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    DeathState();
    ev("Pending", {lab(world), id.mIndex, Snap(Thing())});
}
uint32_t ThingLoopWorldRand(CWorld* world) {
    const uint32_t answer = static_cast<uint32_t>(ret("rand", 0));
    ev("GetRand", {lab(world), answer});
    ChangeWorld();
    return answer;
}
int32_t ThingLoopSeason(const CParkTime* time) {
    const int32_t label = time == reinterpret_cast<CParkTime*>(gWorlds[0] + 0x2a0) ? 1 :
                          time == reinterpret_cast<CParkTime*>(gWorlds[1] + 0x2a0) ? 4 : -1;
    const int32_t answer = static_cast<int32_t>(ret("season", 0));
    ev("GetSeason", {label, 0x2a0, answer});
    ChangeWorld();
    return answer;
}
SubscriptionInsertResult SubscriptionTree::insert_one(const CSubscription& key) {
    const intptr_t base = reinterpret_cast<intptr_t>(gMessageCentre);
    const int64_t slot = (reinterpret_cast<intptr_t>(this) - base) / 16;
    const uint8_t inserted = static_cast<uint8_t>(ret("inserted", 0));
    const int64_t node = ret("node", 0);
    ev("insert_one", {CentreLabel(), slot, key.mThingId, node, inserted,
                      Snap(Thing()), Word(gBlock, 0x10), Word(gBlock, 0xd4), Word(gBlock, 0xdc), Word(gBlock, 0xd8)});
    if (ret("clearcentre", 0) != 0) gMessageCentre = nullptr;
    // The independent iterator is never dereferenced by Subscribe.
    return {reinterpret_cast<node_base*>(static_cast<uintptr_t>(node)), inserted};
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    std::memset(gFresh, 0xa5, sizeof gFresh);
    gNewCentre = gFresh;
    return gFresh;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 16, 29});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

extern "C" {
// A fixed set of labelled storage blocks, with guard bytes included in the dumps.
void* tpw_tl_pointer(int32_t which) {
    switch (which) {
    case 0: return gBlock;
    case 1: return gWorlds[0];
    case 2: return Existing();
    case 3: return gCell;
    case 4: return gWorlds[1];
    default: return gThings[which - 10];
    }
}
void tpw_tl_copy(int32_t which, uint8_t* data, int32_t n, int32_t load) {
    void* p = tpw_tl_pointer(which);
    if (load) std::memcpy(p, data, n);
    else std::memcpy(data, p, n);
}
void tpw_tl_config(int32_t op, int32_t existing, int32_t type, int32_t postkind, int32_t finalid,
                   int32_t head0, int32_t head1, int32_t sentinel, int32_t gameworld) {
    gOp = op;
    gCtorType = type;
    gPostKind = postkind;
    gFinalId = finalid;
    gAssertHeads[0] = head0;
    gAssertHeads[1] = head1;
    gAssertIndex = 0;
    gThingLoopEmptyCellId = static_cast<uint16_t>(sentinel);
    gWorld = reinterpret_cast<CWorld*>(gWorlds[0]);
    gGameWorld = reinterpret_cast<CWorld*>(gWorlds[gameworld]);
    gMessageCentre = existing ? Existing() : nullptr;
    gNewCentre = nullptr;
    gPoisonMessage = false;
    std::memset(gCentre, 0xa5, sizeof gCentre);
    std::memset(gThingArray, 0xa5, sizeof gThingArray);
    gThingArray[0].mThing = nullptr;
    std::memset(gFresh, 0xa5, sizeof gFresh);
}
void tpw_tl_row(int32_t row, int32_t block) { gThingArray[row].mThing = block < 0 ? nullptr : gThings[block]; }
void tpw_tl_balance(uint32_t* values, int32_t load) {
    for (int32_t i = 0; i < 504; ++i) {
        if (load) {
            gMainBalance.mValues[i].mBits = 0xa5a5a5a500000000ull | values[i];
        } else values[i] = static_cast<uint32_t>(gMainBalance.mValues[i].mInteger);
    }
}
int32_t tpw_tl_balance_guard() {
    for (int32_t i = 0; i < 504; ++i) {
        if ((gMainBalance.mValues[i].mBits >> 32) != 0xa5a5a5a5u) return i;
    }
    return -1;
}
void tpw_tl_mode(int32_t type) { TheGameType().mType = static_cast<GameType>(type); }
int32_t tpw_tl_type() { return TheGameType().mType; }
int32_t tpw_tl_centre() { return CentreLabel(); }
int32_t tpw_tl_gameworld() { return lab(gGameWorld); }
int32_t tpw_tl_table_guard() {
    return std::memcmp(gInitialTable, gThingArray, sizeof gThingArray) == 0 ? -1 : 0;
}
void tpw_tl_snapshot_table() { std::memcpy(gInitialTable, gThingArray, sizeof gThingArray); }
int32_t tpw_tl_centre_guard() {
    for (uint8_t value : gCentre) {
        if (value != 0xa5) return 0;
    }
    return 1;
}
int32_t tpw_tl_fresh_guard() {
    for (uint8_t value : gFresh) {
        if (value != 0xa5) return 0;
    }
    return 1;
}
uint32_t tpw_tl_call(int32_t op, int32_t argument) {
    switch (op) {
    case 0: Thing()->Process(); break;
    case 1: Thing()->KillMe(); break;
    case 2: Cell()->InsertThing({static_cast<uint16_t>(argument)}); break;
    case 3: return gMessageCentre->Subscribe(*Thing(), static_cast<MsgType>(argument));
    case 4: return reinterpret_cast<CWeather*>(gBlock)->FindNewQuality();
    case 5: return reinterpret_cast<CAdvisor*>(gBlock)->Init();
    case 6: reinterpret_cast<CTagSystem*>(gBlock)->Init(); break;
    }
    return 0;
}
}

// Each direct callee logs its logical this and the thing state.
void CPerson::UpdateTurn() { Turn("CPerson::UpdateTurn", this); }
void CPerson::ModelState() { Turn("CPerson::ModelState", this); }
void CPerson::FinalDestroy() { Final("CPerson::FinalDestroy", this); }
void CObject::UpdateTurn() { Turn("CObject::UpdateTurn", this); }
void CObject::ModelState() { Turn("CObject::ModelState", this); }
void CObject::FinalDestroy() { Final("CObject::FinalDestroy", this); }
void CMechanic::UpdateTurn() { Turn("CMechanic::UpdateTurn", this); }
void CMechanic::ModelState() { Turn("CMechanic::ModelState", this); }
void CMechanic::FinalDestroy() { Final("CMechanic::FinalDestroy", this); }
void CHandyman::UpdateTurn() { Turn("CHandyman::UpdateTurn", this); }
void CHandyman::ModelState() { Turn("CHandyman::ModelState", this); }
void CHandyman::FinalDestroy() { Final("CHandyman::FinalDestroy", this); }
void CEntertainer::UpdateTurn() { Turn("CEntertainer::UpdateTurn", this); }
void CEntertainer::ModelState() { Turn("CEntertainer::ModelState", this); }
void CEntertainer::FinalDestroy() { Final("CEntertainer::FinalDestroy", this); }
void CGuard::UpdateTurn() { Turn("CGuard::UpdateTurn", this); }
void CGuard::ModelState() { Turn("CGuard::ModelState", this); }
void CGuard::FinalDestroy() { Final("CGuard::FinalDestroy", this); }
void CResearcher::UpdateTurn() { Turn("CResearcher::UpdateTurn", this); }
void CResearcher::ModelState() { Turn("CResearcher::ModelState", this); }
void CResearcher::FinalDestroy() { Final("CResearcher::FinalDestroy", this); }
void CStaffHQ::FinalDestroy() { Final("CStaffHQ::FinalDestroy", this); }
void CMechanicHQ::FinalDestroy() { Final("CMechanicHQ::FinalDestroy", this); }
void CAdvisor::UpdateTurn() { Turn("CAdvisor::UpdateTurn", this); }
void CAdvisor::FinalDestroy() { Final("CAdvisor::FinalDestroy", this); }
void CTagSystem::FinalDestroy() { Final("CTagSystem::FinalDestroy", this); }
void CParkAnalyser::UpdateTurn() { Turn("CParkAnalyser::UpdateTurn", this); }
void CParkAnalyser::FinalDestroy() { Final("CParkAnalyser::FinalDestroy", this); }
void CResearchLab::FinalDestroy() { Final("CResearchLab::FinalDestroy", this); }
void CWeather::UpdateTurn() { Turn("CWeather::UpdateTurn", this); }
void CWeather::FinalDestroy() { Final("CWeather::FinalDestroy", this); }
void CBankAccount::FinalDestroy() { Final("CBankAccount::FinalDestroy", this); }
void CUIMsgReceiver::FinalDestroy() { Final("CUIMsgReceiver::FinalDestroy", this); }
void COnlinePerson::UpdateTurn() { Turn("COnlinePerson::UpdateTurn", this); }
void COnlinePerson::ModelState() { Turn("COnlinePerson::ModelState", this); }
void COnlinePerson::FinalDestroy() { Final("COnlinePerson::FinalDestroy", this); }
void CChallengeHQ::UpdateTurn() { Turn("CChallengeHQ::UpdateTurn", this); }
void CChallengeHQ::FinalDestroy() { Final("CChallengeHQ::FinalDestroy", this); }
