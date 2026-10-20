// Test library for the staff member, part 2 (unit w190; libtpw_unit_StaffQueries.dylib, tests/test_StaffQueries.py), main app:
//   src/Staff_queries.cpp   the 19 CStaff members of staff.cpp from SackMe to GetSalary (the list is in the file's first comment)
//   src/WorldSavvy.cpp      CWorldSavvy::GetWorld (answers *gWorld), which GetEmploymentDuration calls
//
// The callees outside them are fakes that record the same events as the PowerPC stubs: the CPeep members, CThing::GetThingId and KillMe, the staff
// member's SetState, the delayed script's SpawnSpriteScript, the world's getters, the bank account, the two headquarters, the mechanic's ChooseNextAction,
// the mouse and its default mode (the nothrow operator new logs the Mac's sizes), the park time, the trace and the assertion. TbTimeStamp::operator- is a
// plain subtraction (no event). The globals of other files are defined here: gWorld, gMouse, its two words, the current mouse mode, and the storage of the
// balance table gMainBalance (raw bytes the test fills).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "Debug.h"
#include "MainBalance.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

#ifdef __APPLE__
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");
#else
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("gMainBalance");
#endif
alignas(16) unsigned char gBalanceStorage[sizeof(CMainBalance)] = {};

// ---- globals of other files
CWorld* gWorld;
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

const char* gBase;                      // the staff member under test (offsets in the logs are from it)
CParkAnalyser* gAnalyser = nullptr;     // the analyser the world answers (shifted so that the two histories' totals lie in the case's block)
bool gQuiet = false;                    // the test's own mouse mode is made without a log entry


int32_t Off(const void* p) { return static_cast<int32_t>(reinterpret_cast<const char*>(p) - gBase); }

// The Mac's size of an object the host made: the sizes the fake operator new logs (the mouse is 1 byte, its default mode 8 on the Mac).
int64_t MacSize(std::size_t size) {
    if (size == sizeof(CMouse)) return 1;
    if (size == sizeof(CMMDefault)) return 8;
    return static_cast<int64_t>(size);
}

// The number of "%d" in a format: the trace and the assertion log that many integer arguments.
int32_t CountInts(const char* format) {
    int32_t n = 0;
    for (const char* p = format; (p = std::strstr(p, "%d")) != nullptr; p += 2) ++n;
    return n;
}

// The mode the test puts in gMouseMode: its GetMode answers the queue "GetMode" (the Mac's side is a fake function in a fake vtable).
class TestMode : public CMouseMode {
public:
    int32_t GetMode() override {
        ev("GetMode", {lab(this)});
        return static_cast<int32_t>(ret("GetMode", 0));
    }
};

}  // namespace

// ---- the nothrow operator new (the mouse has no constructor call: it is labelled here and on the PowerPC side)
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = on() && ret("newnull", 0) != 0;
    ev("new", {MacSize(size), fail});
    if (fail) return nullptr;
    void* block = ::operator new(size);
    if (size == sizeof(CMouse)) newlab(block);
    return block;
}

// ---- the mouse and its default mode
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }
CMMDefault::CMMDefault() {
    if (!gQuiet) ev("CMMDefault.ctor", {newlab(this)});
}
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

// ---- the thing, the CPeep members and the staff member's SetState
ThingId CThing::GetThingId() const {
    ev("CThing::GetThingId", {lab(this)});
    return {static_cast<uint16_t>(ret("CThing::GetThingId", 0))};
}
void CThing::KillMe() { ev("CThing::KillMe", {lab(this)}); }
int32_t CPeep::IsInsidePark() const {
    ev("CPeep::IsInsidePark", {lab(this)});
    return static_cast<int32_t>(ret("CPeep::IsInsidePark", 0));
}
void CPeep::SpawnParticleEffect(int32_t effect) { ev("CPeep::SpawnParticleEffect", {lab(this), effect}); }
void CPeep::Teleport(MapPos x, MapPos y) {
    ev("CPeep::Teleport", {lab(this), (x.mCell << 8) | x.mOffset, (y.mCell << 8) | y.mOffset});
}
void CStaff::SetState(StaffState state) { ev("CStaff::SetState", {lab(this), static_cast<int32_t>(state)}); }
void CDelayedScript::SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index) {
    ev("CDelayedScript::SpawnSpriteScript", {Off(this), static_cast<int32_t>(script), set, index});
}

// ---- the world, the bank account, the analyser, the two headquarters and the mechanic
CStaffHQ* CWorld::GetStaffHQ() {
    ev("CWorld::GetStaffHQ", {});
    return reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(0x6000));
}
CMechanicHQ* CWorld::GetMechanicHQ() {
    ev("CWorld::GetMechanicHQ", {});
    return reinterpret_cast<CMechanicHQ*>(static_cast<uintptr_t>(0x7000));
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("CWorld::GetParkAnalyser", {});
    return gAnalyser;
}
CBankAccount* CWorld::GetBankAccount() {
    ev("CWorld::GetBankAccount", {});
    return reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(0x5000));
}
void CBankAccount::MakeWithdrawal(uint32_t amount) { ev("CBankAccount::MakeWithdrawal", {lab(this), amount}); }
int32_t CStaffHQ::IsStriking(StaffType type) {
    ev("CStaffHQ::IsStriking", {lab(this), static_cast<int32_t>(type)});
    return static_cast<int32_t>(ret("CStaffHQ::IsStriking", 0));
}
void CStaffHQ::PickUpStaff(ThingId staff) { ev("CStaffHQ::PickUpStaff", {lab(this), staff.mIndex}); }
void CMechanicHQ::ReassignJobsAfterDrop(CMechanic& mechanic, MapId cell) { ev("CMechanicHQ::ReassignJobsAfterDrop", {lab(this), lab(&mechanic), cell.mIndex}); }
void CMechanic::ChooseNextAction() { ev("CMechanic::ChooseNextAction", {lab(this)}); }

// ---- the park time and the time stamp subtraction (a plain subtraction, no event)
TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    ev("GetFunnyTime", {static_cast<int64_t>(reinterpret_cast<const char*>(this) - reinterpret_cast<const char*>(gWorld)), a != nullptr, b != nullptr, c != nullptr,
                        d != nullptr, e != nullptr, f != nullptr, g != nullptr, h != nullptr});
    return TbTimeStamp{ret("funny", 0)};
}
TbTimeDiff TbTimeStamp::operator-(const TbTimeStamp& other) const {
    TbTimeDiff span;
    span.mTicks = static_cast<int64_t>(static_cast<uint64_t>(mTicks) - static_cast<uint64_t>(other.mTicks));
    return span;
}

// ---- the runtime: the "%d" arguments are logged after the condition (assertion) or the level (trace)
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    int64_t v[4] = {condition, 0, 0, 0};
    const int32_t n = CountInts(format);
    va_list args;
    va_start(args, format);
    for (int32_t i = 0; i < n && i < 3; ++i) v[1 + i] = va_arg(args, uint32_t);
    va_end(args);
    switch (n) {
    case 0: ev("GAME_ASSERTMSG", {v[0]}, {format}); break;
    case 1: ev("GAME_ASSERTMSG", {v[0], v[1]}, {format}); break;
    default: ev("GAME_ASSERTMSG", {v[0], v[1], v[2]}, {format}); break;
    }
}
void CTRACE(uint32_t level, const char* format, ...) {
    int64_t v[3] = {level, 0, 0};
    const int32_t n = CountInts(format);
    va_list args;
    va_start(args, format);
    for (int32_t i = 0; i < n && i < 2; ++i) v[1 + i] = va_arg(args, uint32_t);
    va_end(args);
    switch (n) {
    case 0: ev("CTRACE", {v[0]}, {format}); break;
    default: ev("CTRACE", {v[0], v[1]}, {format}); break;
    }
}

extern "C" {

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_stq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
// The address of the balance table's first value (slot 0 at the Mac's 0x102b4568 + 8).
uint64_t tpw_stq_balance() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gMainBalance.mValues[0])); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_stq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CStaff));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    out[n++] = static_cast<int64_t>(offsetof(CMainBalance, mValues));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<CParkAnalyser*>(0)->mHistory1F100[3].mTemp));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<CParkAnalyser*>(0)->mHistory1F100[4].mTemp));
    out[n++] = static_cast<int64_t>(offsetof(CMechanic, mObjectToRepair));
    out[n++] = static_cast<int64_t>(offsetof(CHandyman, mToiletToClean));
    out[n++] = static_cast<int64_t>(sizeof(CMouse));
    out[n++] = static_cast<int64_t>(sizeof(CMMDefault));
    return n;
}

// The state a case sets before the call: the staff member under test, the world (any block), the analyser's block (the pointer is shifted back so that
// the history 3's total lies at its start; history 4's is 0x250 further), the mouse (0 or a block), the two words of the mouse, whether gMouseMode holds
// a test mode (its address is answered), and the mode's block.
uint64_t tpw_stq_setup(uint64_t staff, uint64_t world, uint64_t analyser, uint64_t mouse, int32_t word1, int32_t word2, int32_t withMode, uint64_t modeBlock) {
    gBase = reinterpret_cast<const char*>(staff);
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world));
    const uintptr_t total = reinterpret_cast<uintptr_t>(&reinterpret_cast<CParkAnalyser*>(0)->mHistory1F100[3].mTemp);
    gAnalyser = reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser) - total);
    gMouse = reinterpret_cast<CMouse*>(static_cast<uintptr_t>(mouse));
    gMouseWord1E0 = word1;
    gMouseWord1E4 = word2;
    gMouseMode = nullptr;
    if (withMode != 0) {
        gQuiet = true;
        gMouseMode = new (reinterpret_cast<void*>(static_cast<uintptr_t>(modeBlock))) TestMode();
        gQuiet = false;
    }
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gMouseMode));
}
uint64_t tpw_stq_analyser() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gAnalyser)); }
int64_t tpw_stq_mouse() { return static_cast<int64_t>(lab(gMouse) >= 0 ? lab(gMouse) : ((1ll << 33) | (reinterpret_cast<uintptr_t>(gMouse) & 0xffffffffu))); }
void tpw_stq_mousewords(int32_t* out) {
    out[0] = gMouseWord1E0;
    out[1] = gMouseWord1E4;
}

// ---- the functions, one wrapper each with C types only (a ThingId or MapId is passed as its halfword)
static CStaff* S(uint64_t block) { return reinterpret_cast<CStaff*>(static_cast<uintptr_t>(block)); }
void tpw_stq_sack(uint64_t b) { S(b)->SackMe(); }
void tpw_stq_train(uint64_t b, int32_t amount) { S(b)->TrainMe(amount); }
int32_t tpw_stq_duration(uint64_t b) { return S(b)->GetEmploymentDuration(); }
int32_t tpw_stq_canpick(uint64_t b) { return S(b)->CanBePickedUp(); }
void tpw_stq_pickme(uint64_t b) { S(b)->PickMeUp(); }
void tpw_stq_dropme(uint64_t b, uint32_t cell) { S(b)->DropMe(MapId{static_cast<uint16_t>(cell)}); }
void tpw_stq_pointed(uint64_t b) { S(b)->BeingPointedAt(); }
int32_t tpw_stq_stafftype(uint64_t b) { return static_cast<int32_t>(S(b)->GetStaffType()); }
int32_t tpw_stq_genstate(uint64_t b) { return S(b)->GetGeneralStaffState(); }
int32_t tpw_stq_skill(uint64_t b) { return S(b)->GetSkill(); }
int32_t tpw_stq_striking(uint64_t b) { return S(b)->IsStriking(); }
int32_t tpw_stq_allocride(uint64_t b, uint32_t id) { return S(b)->IsAllocatedToRide(ThingId{static_cast<uint16_t>(id)}); }
int32_t tpw_stq_working(uint64_t b, uint32_t id) { return S(b)->IsWorkingOnAllocatedObject(ThingId{static_cast<uint16_t>(id)}); }
int32_t tpw_stq_resting(uint64_t b, uint32_t id) { return S(b)->IsRestingInStaffRoom(ThingId{static_cast<uint16_t>(id)}); }
int32_t tpw_stq_tired(uint64_t b) { return S(b)->TooTiredToWork(); }
void tpw_stq_setname(uint64_t b, uint64_t name) { S(b)->SetName(reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(name))); }
int64_t tpw_stq_getname(uint64_t b) { return static_cast<int64_t>(reinterpret_cast<const char*>(S(b)->GetName()) - reinterpret_cast<const char*>(S(b))); }
uint32_t tpw_stq_salary(uint64_t b) { return S(b)->GetSalary(); }
uint32_t tpw_stq_ssalary(int32_t type, uint32_t grade) { return CStaff::GetSalary(static_cast<StaffType>(type), static_cast<uint8_t>(grade)); }

}  // extern "C"
