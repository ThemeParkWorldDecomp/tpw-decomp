// CPerson balloon, happiness and catch helpers (unit v5293).
#include <cstdint>

#include "ThingReceivers.h"
#include "Debug.h"
#include "analyser.h"
#include "MainBalance.h"

uint32_t gBalloonPopTarget = 0x102669d8;   // 0x1029fd84

namespace {
template <typename T>
T* FieldAt(void* base, uint32_t offset) {
    return reinterpret_cast<T*>(reinterpret_cast<char*>(base) + offset);
}

// The balance rows at +0x18, +0x1c, +0x20 of gMainBalance, low byte only.
uint32_t BalanceByte(uint32_t offset) {
    return *FieldAt<uint32_t>(&gMainBalance, offset) & 0xff;
}

uint32_t AmountOf(int32_t amount) {
    switch (amount) {
        case 0: return BalanceByte(0x18);
        case 1: return BalanceByte(0x1c);
        case 2: return BalanceByte(0x20);
        default: break;
    }
    GAME_ASSERTMSG(0, "get_happiness_amount - unknown default");
    return 0;
}
}  // namespace

// 0x1011b004
void CPerson::LosingMods(CObject*) {
    CTRACE(2, "Person lost this sideshow :( \n");
    LowerHappiness(static_cast<Amount>(1));
}

// 0x1011b084
void CPerson::DrawBalloon(float x, float y, float z, int32_t mode) {
    int32_t* script = FieldAt<int32_t>(this, 0x210);
    if (*script != 0) {
        SCR_SetScriptPosition(*script, *FieldAt<float>(this, 0x218), y, *FieldAt<float>(this, 0x21c), mode);
        *FieldAt<float>(this, 0x218) = x;
        *FieldAt<float>(this, 0x21c) = z;
    }
}

// 0x1011b11c
void CPerson::MakeBalloonPop() {
    int32_t* script = FieldAt<int32_t>(this, 0x210);
    if (*script != 0) {
        ESP_ForceGoto(*script, &gBalloonPopTarget);
        *script = 0;
    }
}

// 0x1011b198
uint32_t CPerson::GetHappinessAmount(Amount amount) const {
    return AmountOf(amount);
}

// 0x1011b258
void CPerson::RaiseHappiness(Amount amount) {
    uint32_t value = AmountOf(amount) & 0xff;
    float* happiness = FieldAt<float>(this, 0x19c);
    float h = *happiness;
    float sum = static_cast<float>(value) + h;
    if (!(sum > 100.0f)) {   // ble / bge are taken for NaN
        if (!(sum < 0.0f))
            *happiness = h + static_cast<float>(value);
        else
            *happiness = 0.0f;
    } else {
        *happiness = 100.0f;
    }
}

// 0x1011b384
void CPerson::LowerHappiness(Amount amount) {
    uint32_t value = AmountOf(amount) & 0xff;
    float* happiness = FieldAt<float>(this, 0x19c);
    float h = *happiness;
    float diff = -static_cast<float>(value) + h;
    if (!(diff > 100.0f)) {
        if (!(diff < 0.0f))
            *happiness = h - static_cast<float>(value);
        else
            *happiness = 0.0f;
    } else {
        *happiness = 100.0f;
    }
}

// 0x1011b508
void CPerson::GetBagged() {
    int32_t* script = FieldAt<int32_t>(this, 0x210);
    if (*script != 0) {
        SCR_KillScript(*script);
        *script = 0;
    }
    FieldAt<CDelayedScript>(this, 0xc)->KillSpriteScript();
    SetState(static_cast<PersonState>(0x11));
}

// 0x1011b700
int32_t CPerson::IsCatchable() const {
    int32_t state = *FieldAt<int32_t>(const_cast<CPerson*>(this), 0x220);
    if (state == 6 || state == 7 || state == 8 || state == 10 || state == 9) return 1;
    CTRACE(2, "Not catchable because of state %d\n", state);
    return 0;
}
