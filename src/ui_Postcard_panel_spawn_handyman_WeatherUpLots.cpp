// Keyboard cheat actions: the weather keys, chatter, real time lighting and start_cheating.
#include <cstdint>

#include "Cheat.h"
#include "Debug.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "keytable_actions.h"
#include "tb_resource_display.h"
#include "ui_main.h"

// 0x1037f240 / 0x1037f244 (BSS): the guard byte and the value of ChatterToggle's function-static
int8_t gChatterToggleGuard;
int32_t gChatterToggleOn;

// 0x1014d6bc (108 bytes)
int32_t WeatherUpLots() {
    gGameWorld->GetWeather()->mF64 = 1;
    int32_t quality = static_cast<int32_t>(gGameWorld->GetWeather()->mF0C);
    gGameWorld->GetWeather()->SetQuality(quality + 10);
    return 1;
}

// 0x1014d62c (108 bytes)
int32_t WeatherDown() {
    gGameWorld->GetWeather()->mF64 = 1;
    int32_t quality = static_cast<int32_t>(gGameWorld->GetWeather()->mF0C);
    gGameWorld->GetWeather()->SetQuality(quality - 1);
    return 1;
}

// 0x1014d74c (108 bytes)
int32_t WeatherDownLots() {
    gGameWorld->GetWeather()->mF64 = 1;
    int32_t quality = static_cast<int32_t>(gGameWorld->GetWeather()->mF0C);
    gGameWorld->GetWeather()->SetQuality(quality - 10);
    return 1;
}

// 0x1014d5a0 (108 bytes)
int32_t WeatherUp() {
    gGameWorld->GetWeather()->mF64 = 1;
    int32_t quality = static_cast<int32_t>(gGameWorld->GetWeather()->mF0C);
    gGameWorld->GetWeather()->SetQuality(quality + 1);
    return 1;
}

// 0x1014d83c (88 bytes)
int32_t DeactivateWeather() {
    gGameWorld->GetWeather()->mF64 = 1;
    gGameWorld->GetWeather()->SetQuality(100);
    return 1;
}

// 0x1014d51c (96 bytes)
int32_t ChatterToggle() {
    if (gChatterToggleGuard == 0) {
        gChatterToggleOn = 1;
        gChatterToggleGuard = 1;
    }
    SND_SetChatterOnOff(gChatterToggleOn);
    gChatterToggleOn = 1 - gChatterToggleOn;
    return 1;
}

// 0x1014d8bc (96 bytes)
int32_t RealTimeLightingMasterToggle() {
    gei_Display.mFlags ^= 0x800;
    const char* state = (gei_Display.mFlags & 0x800) != 0 ? "dis" : "en";
    CTRACE(0x499602d2, "Real time lighting %sabled", state);
    return 1;
}

// 0x1014d470 (132 bytes)
int32_t start_cheating() {
    CCheat* cheat = reinterpret_cast<CCheat*>(gGameWinCheat);
    if (gGameWinCheatGuard == 0) {
        new (cheat) CCheat();
        gGameWinCheatGuard = 1;
    }
    cheat->SetCheatStatus(true);
    gGameWorld->GetBankAccount()->MakeDeposit(1);
    return 1;
}
