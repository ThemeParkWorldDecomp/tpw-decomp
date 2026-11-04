// Test library for the spawn and loan key actions (unit v5028; libtpw_unit_spawn_handyman.dylib, tests/test_spawn_handyman.py), main app:
//   src/game/keyboard.cpp
//
// The callees (the two operator news, the staff and person constructors, the CWorld calls, ApplyForLoan, CTRACE) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"
#include "intrface.h"
#include "keytable_actions.h"
#include "online.h"

using namespace calllog;

// ---- globals of other files, defined here
CIntrface gIntrface;
CWorld* gGameWorld;

static const uint32_t kBank = 0xAC0000;

// ---- the fakes
// The game heap's operator new (the arena), which the actions use.
namespace keyaction_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
}  // namespace keyaction_callee

// The online code's operator new: the host heap, as the real one (platform/online_dispatch.cpp). A person or staff member made with it
// is outside the arena, and the staff and visitor lists, which keep its address in 32 bits, stop the game (BMCTV's crash).
void* online_callee::OperatorNew(uint32_t size) {
    ev("online new", {static_cast<int64_t>(size)});
    return ::operator new(size);
}

typedef tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CellTag;

// A thing's address must fit the 32-bit pointer fields that keep it; one that does not is an extra event.
static void ev_thing(const char* name, const void* self, CellTag cell) {
    ev(name, {newlab(self), static_cast<int64_t>(cell.mIndex)});
    if (!tpw::InArena(self)) ev("outside the arena", {}, {name});
}

CHandyman::CHandyman(CellTag cell) { ev_thing("CHandyman", this, cell); }
CEntertainer::CEntertainer(CellTag cell) { ev_thing("CEntertainer", this, cell); }
CGuard::CGuard(CellTag cell) { ev_thing("CGuard", this, cell); }
CMechanic::CMechanic(CellTag cell) { ev_thing("CMechanic", this, cell); }
CResearcher::CResearcher(CellTag cell) { ev_thing("CResearcher", this, cell); }
CPerson::CPerson(CellTag cell) { ev_thing("CPerson", this, cell); }

bool CWorld::IsPlayingWinningAnim() {
    ev("IsPlayingWinningAnim", {lab(this)});
    return ret("IsPlayingWinningAnim") != 0;
}
void CWorld::EndWinningSequence() { ev("EndWinningSequence", {lab(this)}); }
void CWorld::StartWinningSequence() { ev("StartWinningSequence", {lab(this)}); }
int32_t CWorld::ParkGates_GetStatus() const {
    ev("ParkGates_GetStatus", {lab(this)});
    return static_cast<int32_t>(ret("ParkGates_GetStatus"));
}
void CWorld::SetParkOpenOrClosed(int32_t open, int32_t gateFlag) {
    ev("SetParkOpenOrClosed", {lab(this), open, gateFlag});
}
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(kBank));
}
bool CBankAccount::ApplyForLoan(uint32_t loan) {
    ev("ApplyForLoan", {lab(this), static_cast<int64_t>(loan)});
    return ret("ApplyForLoan") != 0;
}
void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {static_cast<int64_t>(level)}, {format}); }

extern "C" {

uint64_t tpw_sh_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gIntrface", &gIntrface},
        {"gGameWorld", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sh_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sh_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CIntrface, mF24));
    return n;
}

int32_t tpw_sh_spawn_person() { return spawn_person(); }
int32_t tpw_sh_spawn_five_persons() { return spawn_five_persons(); }
int32_t tpw_sh_start_winning_sequence() { return start_winning_sequence(); }
int32_t tpw_sh_spawn_entertainer() { return spawn_entertainer(); }
int32_t tpw_sh_spawn_handyman() { return spawn_handyman(); }
int32_t tpw_sh_spawn_bag_o_staff() { return spawn_bag_o_staff(); }
int32_t tpw_sh_apply_for_loan_1() { return apply_for_loan_1(); }
int32_t tpw_sh_apply_for_loan_2() { return apply_for_loan_2(); }

}  // extern "C"
