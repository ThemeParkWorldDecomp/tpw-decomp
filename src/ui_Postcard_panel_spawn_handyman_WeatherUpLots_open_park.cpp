// keyboard.cpp key actions, main app 0x1014d364 .. 0x1014e73c (unit v51108): each answers 1.
#include <cstdint>
#include <new>
#include "Cheat.h"
#include "Debug.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "keytable_actions.h"
#include "tb_resource_display.h"
#include "ui_main.h"
#include "intrface.h"
#include "Clock.h"
#include "CVariableTime.h"
#include "UIInterface.h"
#include "GameSys.h"
#include "platform/arena.h"

// The hud and movie toggles (names invented).
int32_t gKeyHudToggle = 0;      // 0x1037f238
int32_t gKeyMovieToggle = 0;    // 0x1037f23c

namespace keyaction_callee {
// The runtime's operator new (0x102384b8), the game heap: the arena on the host (the staff it makes are things, whose addresses the
// lists keep in 32 bits); weak so the test library can fake it.
__attribute__((weak)) void* OperatorNew(uint32_t size) { return tpw::ArenaAlloc(size); }
}  // namespace keyaction_callee

namespace {
typedef tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> StaffCell;

template <class T>
void MakeKeyStaff(uint32_t size) {
    StaffCell cell;
    cell.mIndex = gIntrface.mF24;
    void* memory = keyaction_callee::OperatorNew(size);
    if (memory) new (memory) T(cell);
}
}  // namespace

// 0x1014d364 (48 bytes)
int32_t normal_speed_time() {
    reinterpret_cast<CVariableTime*>(&gClock)->SetTimeRate(1.0);
    return 1;
}

// 0x1014d3bc (56 bytes)
int32_t open_park() {
    gGameWorld->SetParkOpenOrClosed(1, 0);
    return 1;
}

// 0x1014d414 (56 bytes)
int32_t close_park() {
    gGameWorld->SetParkOpenOrClosed(0, 0);
    return 1;
}

// 0x1014d7e0 (56 bytes)
int32_t WeatherReset() {
    gGameWorld->GetWeather()->mF64 = 0;
    return 1;
}

// 0x1014d950 (28 bytes)
int32_t FPSDisplayToggle() {
    gIntrface.mShowFPS = (gIntrface.mShowFPS == 0);
    return 1;
}

// 0x1014d994 (56 bytes)
int32_t complete_all_research() {
    gGameWorld->GetResearchLab()->CheatCompleteAll();
    return 1;
}

// 0x1014d9f8 (56 bytes)
int32_t uncomplete_all_research() {
    gGameWorld->GetResearchLab()->CheatCompleteOnlyPaidFor();
    return 1;
}

// 0x1014da60 (56 bytes)
int32_t create_arrival_vehicle() {
    gGameWorld->ArrivalVehicle_DoNextLeg(1);
    return 1;
}

// 0x1014dd7c (48 bytes)
int32_t populate_park() {
    gGameWorld->PopulatePark();
    return 1;
}

// 0x1014de5c (48 bytes)
int32_t start_bankruptcy_sequence() {
    gGameWorld->StartBankruptcySequence();
    return 1;
}

// 0x1014dfbc (88 bytes)
int32_t spawn_mechanic() {
    MakeKeyStaff<CMechanic>(0x218);
    return 1;
}

// 0x1014e03c (88 bytes)
int32_t spawn_researcher() {
    MakeKeyStaff<CResearcher>(0x214);
    return 1;
}

// 0x1014e0bc (88 bytes)
int32_t spawn_guard() {
    MakeKeyStaff<CGuard>(0x214);
    return 1;
}

// 0x1014e4b8 (60 bytes)
int32_t get_doshed_up() {
    gGameWorld->GetBankAccount()->MakeDeposit(10000);
    return 1;
}

// 0x1014e518 (40 bytes)
int32_t snapshot() {
    GAMESYSTEM_Screenshot();
    return 1;
}

// 0x1014e560 (60 bytes)
int32_t continuous_snapshot() {
    gKeyMovieToggle = 1 - gKeyMovieToggle;
    GAMESYSTEM_ScreenshotMovie(gKeyMovieToggle);
    return 1;
}

// 0x1014e5c8 (44 bytes)
int32_t togglegamewindow() {
    gUIInterface.SwitchGameWindow();
    return 1;
}

// 0x1014e61c (60 bytes)
int32_t togglehud() {
    gKeyHudToggle = 1 - gKeyHudToggle;
    GAMESYSTEM_SetHudState(gKeyHudToggle);
    return 1;
}

// 0x1014e6c8 (44 bytes)
int32_t mainmenu() {
    gUIInterface.ShowGameMainMenu();
    return 1;
}

// 0x1014e714 (44 bytes)
int32_t Screen_BuyObject() {
    gUIInterface.Screen_BuyObject();
    return 1;
}
