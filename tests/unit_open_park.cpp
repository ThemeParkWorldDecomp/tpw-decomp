// Test library for the keyboard cheat actions (unit v51108; libtpw_unit_open_park.dylib, tests/test_open_park.py), main app:
//   src/ui_Postcard_panel_spawn_handyman_WeatherUpLots_open_park.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read. Objects the fakes hand back are dummy blocks
// (labels 3 research lab, 4 bank account, 5 world); only the weather block is real memory (the unit writes a byte of it).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Clock.h"
#include "CVariableTime.h"
#include "GameSys.h"
#include "ThingReceivers.h"
#include "UIInterface.h"
#include "World.h"
#include "intrface.h"
#include "keytable_actions.h"

using namespace calllog;

#include "CFixedTime.h"
CFixedTime::CFixedTime() : mLastClock(0), mMilliseconds(0) {}   // gClock's member; never read here

CVariableTime::CVariableTime() {}   // reached through gClock's members; never read here

// ---- globals of other files, defined here
CIntrface gIntrface;
CClock gClock;
UIInterface gUIInterface;
CWorld* gGameWorld;

namespace {
uint8_t gWorldObject[64];
uint8_t gLabObject[64];
uint8_t gBankObject[64];
CWeather* gFakeWeather;
}  // namespace

// ---- the fakes
namespace keyaction_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    if (ret("newnull")) return nullptr;
    return tpw::ArenaAllocZeroed(size);
}
}  // namespace keyaction_callee

void CWorld::SetParkOpenOrClosed(int32_t open, int32_t gateFlag) { ev("SetParkOpenOrClosed", {lab(this), open, gateFlag}); }
CWeather* CWorld::GetWeather() {
    ev("GetWeather", {lab(this)});
    return gFakeWeather;
}
CResearchLab* CWorld::GetResearchLab() {
    ev("GetResearchLab", {lab(this)});
    return reinterpret_cast<CResearchLab*>(gLabObject);
}
void CResearchLab::CheatCompleteAll() { ev("CheatCompleteAll", {lab(this)}); }
void CResearchLab::CheatCompleteOnlyPaidFor() { ev("CheatCompleteOnlyPaidFor", {lab(this)}); }
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::ArrivalVehicle_DoNextLeg(int32_t leg) {
    ev("ArrivalVehicle_DoNextLeg", {lab(this), leg});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> t;
    t.mIndex = 0;
    return t;
}
void CWorld::PopulatePark() { ev("PopulatePark", {lab(this)}); }
void CWorld::StartBankruptcySequence() { ev("StartBankruptcySequence", {lab(this)}); }
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(gBankObject);
}
void CBankAccount::MakeDeposit(uint32_t amount) { ev("MakeDeposit", {lab(this), amount}); }
void CVariableTime::SetTimeRate(double rate) {
    int64_t bits;
    std::memcpy(&bits, &rate, 8);
    ev("SetTimeRate", {lab(this), bits});
}
CMechanic::CMechanic(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) { ev("CMechanic", {newlab(this), cell.mIndex}); }
CResearcher::CResearcher(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) { ev("CResearcher", {newlab(this), cell.mIndex}); }
CGuard::CGuard(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) { ev("CGuard", {newlab(this), cell.mIndex}); }
void GAMESYSTEM_Screenshot() { ev("Screenshot", {}); }
void GAMESYSTEM_ScreenshotMovie(int32_t on) { ev("ScreenshotMovie", {on}); }
void GAMESYSTEM_SetHudState(int32_t state) { ev("SetHudState", {state}); }
void UIInterface::SwitchGameWindow() { ev("SwitchGameWindow", {lab(this)}); }
void UIInterface::ShowGameMainMenu() { ev("ShowGameMainMenu", {lab(this)}); }
void UIInterface::Screen_BuyObject() { ev("Screen_BuyObject", {lab(this)}); }

extern "C" {

uint64_t tpw_ka_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"intr", &gIntrface}, {"clock", &gClock}, {"ui", &gUIInterface}, {"world", &gGameWorld},
        {"worldobj", gWorldObject}, {"lab", gLabObject}, {"bank", gBankObject},
        {"hud", &gKeyHudToggle}, {"snap", &gKeyMovieToggle},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ka_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_ka_set_weather(uint64_t p) { gFakeWeather = reinterpret_cast<CWeather*>(static_cast<uintptr_t>(p)); }

// The host offsets of the words the module writes by name: CIntrface::mF24, mShowFPS, CWeather::mF64.
int32_t tpw_ka_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CIntrface, mF24));
    out[n++] = static_cast<int64_t>(offsetof(CIntrface, mShowFPS));
    out[n++] = static_cast<int64_t>(offsetof(CWeather, mF64));
    return n;
}

#define KA(name) uint32_t tpw_ka_##name() { return static_cast<uint32_t>(name()); }
KA(open_park)
KA(normal_speed_time)
KA(close_park)
KA(WeatherReset)
KA(FPSDisplayToggle)
KA(complete_all_research)
KA(uncomplete_all_research)
KA(create_arrival_vehicle)
KA(populate_park)
KA(start_bankruptcy_sequence)
KA(spawn_mechanic)
KA(spawn_researcher)
KA(spawn_guard)
KA(get_doshed_up)
KA(snapshot)
KA(continuous_snapshot)
KA(togglegamewindow)
KA(togglehud)
KA(mainmenu)
KA(Screen_BuyObject)

}  // extern "C"
