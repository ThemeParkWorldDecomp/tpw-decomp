// Differential library for unit w129: the macro AI (src/MacroAI.cpp, src/Arrival_update.cpp, src/ParkTime_update.cpp, src/StaffDB_update.cpp,
// src/GoldenTicket_check.cpp, src/Game_iterate.cpp, src/ControlManager_setup.cpp, src/WorldSavvy.cpp, src/ParkAnalyser_gates.cpp, src/DateMessages.cpp).
//
// Every callee outside the unit is a fake that records the same event as the PowerPC side's stub: the funny time, the time stamp operators, the message
// centre post and its lazy allocation, the UI (DateChange, StaffDBChange, three panels), the staff database's helpers, the arrivals' people, the world's
// dice and IsParkClosed, the game type, the three golden ticket checks, the frame step and the control check. gMainBalance is storage defined here.
//
// The world is NOT a block of its Mac size: the head (the first 0x300 bytes: staff database, park time, golden ticket control, arrivals) is one block
// (gHead, with a guard of 0x40 bytes on both sides) and the tails (the 0x80 bytes at world + 0x1da6f0, which hold the turn) are two blocks; a world
// pointer is a tail's address minus 0x1da6f0, on both sides. gWorld and gGameWorld can name different tails.
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Arrival.h"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "Clock.h"
#include "DateMessages.h"
#include "FS_Online.h"
#include "Game.h"
#include "MainBalance.h"
#include "ObjectParts.h"
#include "ParkTime.h"
#include "StaffDB.h"
#include "UIInterface.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "calllog.h"
#include "ui_main.h"

using namespace calllog;

namespace {

constexpr int32_t kTail = 0x1da6f0;
constexpr int32_t kTailSize = 0x80;
constexpr int32_t kGuard = 0x40;
constexpr int32_t kHeadSize = 0x300;
constexpr int32_t kControlSize = 0x12cc;

alignas(16) unsigned char gTail[2][kTailSize];
alignas(16) unsigned char gHead[kGuard + kHeadSize + kGuard];
alignas(16) unsigned char gControl[kGuard + kControlSize + kGuard];
alignas(16) unsigned char gGameBlock[0x80];
alignas(16) unsigned char gCentreBlock[0x1d0];
alignas(16) unsigned char gSpare[0x40];
alignas(16) unsigned char gMsg[0x40];

void* gNewCentre;
void* gAllocated[8];
int32_t gAllocatedCount;
int32_t gCtorType;

CWorld* WorldPtr(int32_t k) { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTail[k]) - kTail); }
unsigned char* Head() { return gHead + kGuard; }
CStaffDB* Staff() { return reinterpret_cast<CStaffDB*>(Head()); }
CParkTime* Park() { return reinterpret_cast<CParkTime*>(Head() + 0x2a0); }
CArrival* Arrival() { return reinterpret_cast<CArrival*>(Head() + 0x2c4); }
CGoldenTicketControl* Golden() { return reinterpret_cast<CGoldenTicketControl*>(Head() + 0x2c0); }

uint32_t Hash(const unsigned char* p, int32_t n) {
    uint32_t h = 2166136261u;
    for (int32_t i = 0; i < n; ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}

}  // namespace

// ---- The unit's globals --------------------------------------------------------------------------------------------------------------
CWorld* gWorld;
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;
CMessageCentre* gMessageCentre;
UIInterface gUIInterface;
CVariableTime::CVariableTime() {}   // the clock's members are built but never read here
CFixedTime::CFixedTime() {}
CClock gClock;
uint32_t gTPWFlags;

void CBalanceTable::Init() {}
CMainBalance::~CMainBalance() {}
const BalanceDescription* CMainBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CMainBalance::GetBalanceValuesStart() { return mValues; }
CMainBalance gMainBalance;

// ---- The fakes ------------------------------------------------------------------------------------------------------------------------
CGameType::CGameType() {
    ev("CGameType", {1});
    mType = static_cast<GameType>(gCtorType);
}
void CGoldenTicketControl::CheckForLocal() { ev("CheckForLocal", {lab(this)}); }
void CGoldenTicketControl::CheckForGlobal() { ev("CheckForGlobal", {lab(this)}); }
void CGoldenTicketControl::CheckForSecret() { ev("CheckForSecret", {lab(this)}); }
void CArrival::CreatePeople() { ev("CreatePeople", {lab(this)}); }

// The funny time: the receiver and the eight output pointers as labels; each non-NULL output, in order, takes the next value of "ftout"; the answer is "ftret".
TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    ev("GetFunnyTime", {lab(this), lab(a), lab(b), lab(c), lab(d), lab(e), lab(f), lab(g), lab(h)});
    for (int32_t* out : {a, b, c, d, e, f, g, h}) {
        if (out != nullptr) *out = static_cast<int32_t>(ret("ftout", 0));
    }
    return TbTimeStamp{ret("ftret", 0)};
}

TbTimeStamp TbTimeStamp::operator+(const TbTimeDiff& diff) const {
    ev("TimeStamp+", {lab(this), mTicks, diff.mTicks});
    return TbTimeStamp{ret("plus", 0)};
}
TbTimeDiff TbTimeStamp::operator-(const TbTimeStamp& other) const {
    ev("TimeStamp-", {lab(this), mTicks, lab(&other), other.mTicks});
    return TbTimeDiff{ret("minus", 0)};
}
// The stamp's tick image and the eight outputs as labels; with "gtmut" set the stamp itself changes to "gtnew" (the caller must answer the stamp it holds
// AFTER this call); the TbError is "gterr".
TbError TbTimeStamp::GetTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    ev("GetTime", {mTicks, lab(a), lab(b), lab(c), lab(d), lab(e), lab(f), lab(g), lab(h)});
    if (ret("gtmut", 0) != 0) const_cast<TbTimeStamp*>(this)->mTicks = ret("gtnew", 0);
    return TbError(static_cast<int32_t>(ret("gterr", 0)));
}

// The posted message: the centre, the message's type and its flags word. With "pmut" set the post changes the park date's day and month to "pday" and "pmon".
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    ev("PostMessage", {lab(this), message.GetType(), message.mFlags});
    if (ret("pmut", 0) != 0) {
        gParkDate.mDay = static_cast<int32_t>(ret("pday", 0));
        gParkDate.mMonth = static_cast<int32_t>(ret("pmon", 0));
    }
    return 1;
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

// The UI: the date block by value (the nine words), the staff slot and the three panels' month.
void UIInterface::DateChange(ParkDate date) {
    ev("DateChange", {lab(this), date.mSecond, date.mMinute, date.mHour, date.mDay, date.mMonth, date.mYear, date.mDayOfWeek, date.mDayOfYear, date.mIsDst});
}
void UIInterface::StaffDBChange(int32_t slot) { ev("StaffDBChange", {lab(this), slot}); }
void UI_ParkStatusPanelDateChange(int32_t month) { ev("ParkStatusPanelDateChange", {month}); }
void UI_FinancePanelPanelDateChange(int32_t month) { ev("FinancePanelDateChange", {month}); }
void UI_LoansPanelDateChange(int32_t month) { ev("LoansPanelDateChange", {month}); }

// The staff database's helpers: the receiver (the database) and the arguments; the answers come from queues ("numstaff", "nst" consumed in call order,
// "ffs"). Generate records the record's offset in the head, the category and the slot.
int32_t CStaffDB::GetNumStaff() {
    ev("GetNumStaff", {lab(this)});
    return static_cast<int32_t>(ret("numstaff", 0));
}
int32_t CStaffDB::GetNumStaffOfType(int32_t type) {
    ev("GetNumStaffOfType", {lab(this), type});
    return static_cast<int32_t>(ret("nst", 0));
}
uint32_t CStaffDB::GetFirstFreeSlot() {
    ev("GetFirstFreeSlot", {lab(this)});
    return static_cast<uint32_t>(ret("ffs", 0));
}
void CStaffDB::CountStaffTypes() { ev("CountStaffTypes", {lab(this)}); }
void CStaffDB::EnsureMinimumAmountInPool() { ev("EnsureMinimumAmountInPool", {lab(this)}); }
void CStaffDB::GenerateOpeningStaffPool() { ev("GenerateOpeningStaffPool", {lab(this)}); }
void CStaffInfo::Generate(int32_t category, int32_t slot) {
    ev("Generate", {static_cast<int64_t>(reinterpret_cast<unsigned char*>(this) - Head()), category, slot});
}

// The dice: the receiver (the world) and the answer "rand"; with "rstop" set the dice change the stop flag "rstopk" of the staff database to "rstopv".
uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    if (ret("rstop", 0) != 0) Staff()->mStopProducing[ret("rstopk", 0)] = static_cast<uint8_t>(ret("rstopv", 0));
    return static_cast<uint32_t>(ret("rand", 0));
}
int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {lab(this)});
    return static_cast<int32_t>(ret("closed", 0));
}

// The frame step: the receiver; with "nfmut" set it changes gTPWFlags to "nfflags" and the game's restart word to "nfrestart".
void CFixableTime::NextFrame() {
    ev("NextFrame", {lab(this)});
    if (ret("nfmut", 0) != 0) {
        gTPWFlags = static_cast<uint32_t>(ret("nfflags", 0));
        const int32_t restart = static_cast<int32_t>(ret("nfrestart", 0));
        reinterpret_cast<CGame*>(gGameBlock)->mRestart = restart;
    }
}

// The control check: the receiver and a hash of the 0x12cc bytes of the table (zeros once the clear is done) and of the next two bytes.
void CControlManager::CheckAllControlsInitialised() {
    ev("CheckAllControlsInitialised", {lab(this), Hash(reinterpret_cast<unsigned char*>(this), kControlSize), reinterpret_cast<unsigned char*>(this)[kControlSize],
                                       reinterpret_cast<unsigned char*>(this)[kControlSize + 1]});
}

// ---- The test entry points -----------------------------------------------------------------------------------------------------------
extern "C" {

// The address of a block: 0 and 1 the tails, 2 the head base (the world head is at +0x40), 3 the control table base (the table is at +0x40),
// 4 the game block, 5 the centre block, 6 the park date, 7 the UI interface, 8 the game clock, 9 the message block.
void* tpw_ma_addr(int32_t which) {
    switch (which) {
    case 0: case 1: return gTail[which];
    case 2: return gHead;
    case 3: return gControl;
    case 4: return gGameBlock;
    case 5: return gCentreBlock;
    case 6: return &gParkDate;
    case 7: return &gUIInterface;
    case 8: return &gClock;
    case 9: return gMsg;
    default: return nullptr;
    }
}
void* tpw_ma_world(int32_t k) { return WorldPtr(k); }
// The world gWorld names, the world gGameWorld names, the type CGameType's constructor sets.
void tpw_ma_config(int32_t wsel, int32_t gsel, int32_t ctortype) {
    gWorld = WorldPtr(wsel);
    gGameWorld = WorldPtr(gsel);
    gCtorType = ctortype;
}
// The centre global (0 or the centre block) and the allocations of the last case are released.
void tpw_ma_prepare(int32_t centre) {
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    gNewCentre = nullptr;
    gMessageCentre = centre != 0 ? reinterpret_cast<CMessageCentre*>(gCentreBlock) : nullptr;
}
void tpw_ma_globals(int64_t* out) {
    out[0] = lab(gMessageCentre);
    out[1] = gAllocatedCount;
    out[2] = gTPWFlags;
    out[3] = gGameIterateFlag;
}
void tpw_ma_date(uint32_t* words, int32_t set) {
    if (set != 0) std::memcpy(&gParkDate, words, sizeof gParkDate);
    else std::memcpy(words, &gParkDate, sizeof gParkDate);
}
void tpw_ma_flags(uint32_t flags, int32_t iterflag) {
    gTPWFlags = flags;
    gGameIterateFlag = iterflag;
}
// The balance's staff slots: five targets (158 to 162), the interval (175) and the maximum (176).
void tpw_ma_balance(const int32_t* values) {
    for (int32_t k = 0; k < 5; ++k) gMainBalance.mValues[kMainBalanceStaffTarget + k].mInteger = values[k];
    gMainBalance.mValues[kMainBalanceStaffTopUpInterval].mInteger = values[5];
    gMainBalance.mValues[kMainBalanceStaffTopUpMax].mInteger = values[6];
}
void tpw_ma_mode(int32_t type) { TheGameType().mType = static_cast<GameType>(type); }
int32_t tpw_ma_type() { return TheGameType().mType; }

void tpw_ma_macro(int32_t op) {
    CMacroAI* macro = reinterpret_cast<CMacroAI*>(Head());
    if (op == 0) macro->Init();
    else macro->Update();
}
void tpw_ma_arrival(int32_t op) {
    if (op == 0) Arrival()->Init();
    else if (op == 1) Arrival()->Update();
    else Arrival()->UpdateArrivalRate();
}
void tpw_ma_parktime(int32_t op) {
    if (op == 0) Park()->Init();
    else Park()->Update();
}
int64_t tpw_ma_since() { return Park()->GetFunnyTimeSinceSessionStart().mTicks; }
void tpw_ma_staff(int32_t op) {
    if (op == 0) Staff()->Init();
    else Staff()->Update();
}
void tpw_ma_tickets() { Golden()->CheckForTickets(); }
// The game's restart word lives at the host layout's offset of the member (the Mac's 0x18): set it (set != 0) and answer it.
int32_t tpw_ma_restart(int32_t set, int32_t value) {
    CGame* game = reinterpret_cast<CGame*>(gGameBlock);
    if (set != 0) game->mRestart = value;
    return game->mRestart;
}
void tpw_ma_iterate() { reinterpret_cast<CGame*>(gGameBlock)->Iterate(); }
void tpw_ma_setup() { reinterpret_cast<CControlManager*>(gControl + kGuard)->SetupInitialControls(); }
int32_t tpw_ma_getworld() { return lab(&reinterpret_cast<const CWorldSavvy*>(gSpare)->GetWorld()); }
int32_t tpw_ma_gates() { return reinterpret_cast<const CParkAnalyser*>(gSpare)->GatesOpen() ? 1 : 0; }

// The companions. kind 0 CMsgEndOfDay, 1 CMsgEndOfMonth, 2 CMsgEndOfYear. out of the constructor: the flags, the type, the vptr key check (1) and whether
// every byte past the object's two words is intact.
void tpw_ma_ctor(int32_t kind, int64_t* out) {
    std::memset(gMsg, 0xa5, sizeof gMsg);
    CMsg* m;
    if (kind == 0) m = new (gMsg) CMsgEndOfDay();
    else if (kind == 1) m = new (gMsg) CMsgEndOfMonth();
    else m = new (gMsg) CMsgEndOfYear();
    out[0] = m->mFlags;
    out[1] = m->GetType();
    bool intact = true;
    for (size_t i = 2 * sizeof(void*); i < sizeof gMsg; ++i) intact = intact && gMsg[i] == 0xa5;
    out[2] = intact ? 1 : 0;
}
int32_t tpw_ma_gettype(int32_t kind, uint32_t flags) {
    std::memset(gMsg, 0xa5, sizeof gMsg);
    CMsg* m;
    if (kind == 0) m = new (gMsg) CMsgEndOfDay();
    else if (kind == 1) m = new (gMsg) CMsgEndOfMonth();
    else m = new (gMsg) CMsgEndOfYear();
    m->mFlags = flags;
    return m->GetType();
}
// The destructor with the Mac's delete flag: a delete-expression for a positive (signed 16 bit) flag, else an explicit call; out = the flags left behind.
void tpw_ma_dtor(int32_t kind, int32_t flag, int32_t present, uint32_t* out) {
    out[0] = 0;
    if (present == 0) return;
    CMsg* m;
    if (static_cast<int16_t>(flag) > 0) {
        if (kind == 0) m = new CMsgEndOfDay();
        else if (kind == 1) m = new CMsgEndOfMonth();
        else m = new CMsgEndOfYear();
        --tpw_cl_heap[0];
        tpw_cl_label(3, m);
        delete m;
        return;
    }
    std::memset(gMsg, 0xa5, sizeof gMsg);
    if (kind == 0) { auto* w = new (gMsg) CMsgEndOfDay(); w->mFlags = 0x11223344; w->~CMsgEndOfDay(); std::memcpy(&out[0], &w->mFlags, 4); }
    else if (kind == 1) { auto* w = new (gMsg) CMsgEndOfMonth(); w->mFlags = 0x11223344; w->~CMsgEndOfMonth(); std::memcpy(&out[0], &w->mFlags, 4); }
    else { auto* w = new (gMsg) CMsgEndOfYear(); w->mFlags = 0x11223344; w->~CMsgEndOfYear(); std::memcpy(&out[0], &w->mFlags, 4); }
}

}  // extern "C"
