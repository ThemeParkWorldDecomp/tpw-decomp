// Test library for the keyboard cheat actions (unit v5267; libtpw_unit_WeatherUpLots.dylib, tests/test_WeatherUpLots.py), main app:
//   src/ui_Postcard_panel_spawn_handyman_WeatherUpLots.cpp
//
// The callees outside the unit are recording fakes; the globals of other files are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Cheat.h"
#include "Debug.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "keytable_actions.h"
#include "tb_resource_display.h"
#include "ui_main.h"

using namespace calllog;

CWorld* gGameWorld;
CDisplay::CDisplay() {}   // fake: the unit only uses mFlags
CDisplay gei_Display;
alignas(4) uint8_t gGameWinCheat[12];
int8_t gGameWinCheatGuard;

extern int8_t gChatterToggleGuard;
extern int32_t gChatterToggleOn;

namespace {
CWeather* gWeatherBlock;
CBankAccount* gBankBlock;
}  // namespace

CWeather* CWorld::GetWeather() {
    ev("GetWeather", {lab(this)});
    return gWeatherBlock;
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return gBankBlock;
}

void CWeather::SetQuality(int32_t quality) { ev("SetQuality", {lab(this), quality}); }

void CBankAccount::MakeDeposit(uint32_t amount) { ev("MakeDeposit", {lab(this), amount}); }

CCheat::CCheat() { ev("CCheat", {lab(this)}); }

void CCheat::SetCheatStatus(bool cheating) { ev("SetCheatStatus", {lab(this), cheating ? 1 : 0}); }

void SND_SetChatterOnOff(int32_t on) { ev("SND_SetChatterOnOff", {on}); }

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const char* arg = va_arg(args, const char*);
    va_end(args);
    ev("CTRACE", {level}, {format, arg});
}

extern "C" {

uint64_t tpw_wl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world_ptr", &gGameWorld},
        {"display", &gei_Display},
        {"cheat", gGameWinCheat},
        {"cheat_guard", &gGameWinCheatGuard},
        {"chatter_guard", &gChatterToggleGuard},
        {"chatter_on", &gChatterToggleOn},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_wl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_wl_set(uint64_t weather, uint64_t bank) {
    gWeatherBlock = reinterpret_cast<CWeather*>(static_cast<uintptr_t>(weather));
    gBankBlock = reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(bank));
}

int32_t tpw_wl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWeather, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(CWeather, mF64));
    out[n++] = static_cast<int64_t>(offsetof(CDisplay, mFlags));
    out[n++] = static_cast<int64_t>(sizeof(CCheat));
    return n;
}

int32_t tpw_wl_call(int32_t op) {
    switch (op) {
        case 0: return WeatherUpLots();
        case 1: return WeatherDown();
        case 2: return WeatherDownLots();
        case 3: return WeatherUp();
        case 4: return DeactivateWeather();
        case 5: return ChatterToggle();
        case 6: return RealTimeLightingMasterToggle();
        default: return start_cheating();
    }
}

}  // extern "C"
