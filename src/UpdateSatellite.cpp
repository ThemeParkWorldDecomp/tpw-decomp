// Unit v5193: UpdateSatellite 0x1022b8d0 and UI2DMReturnScaleColour 0x1022bb44 (ui_worldLobbyController.cpp, ui_2dmap.cpp).
#include <cstdint>

#include "DrawRenderClasses.h"
#include "InterfaceWindow.h"

InterfaceWindow* gUISatelliteParent;   // 0x10396fec

void UpdateSatellite(int32_t which, int32_t value) {
    if (gUISatelliteParent == nullptr) return;
    InterfaceWindow* panel = gUISatelliteParent->GetWindow(0x16007);
    int32_t id;
    switch (which) {
        case 0: id = 0x16008; break;
        case 1: id = 0x16009; break;
        case 2: id = 0x1600a; break;
        case 3: id = 0x1600b; break;
        case 4: id = 0x1600c; break;
        default: return;
    }
    // The Mac calls slot 7 of the vtable at +0x118 with the window itself as this.
    panel->GetWindow(id)->SetValue(value);
}

bfl::draw::CColour UI2DMReturnScaleColour(int32_t percent, const bfl::draw::CColour& a, const bfl::draw::CColour& b,
                                          const bfl::draw::CColour& c) {
    using bfl::draw::CColour;
    if (percent < 0) percent = 0;
    else if (percent > 100) percent = 100;
    const CColour* from;
    const CColour* to;
    int32_t steps;
    if (percent < 50) {
        from = &a;
        to = &b;
        steps = percent;
    } else {
        from = &b;
        to = &c;
        steps = percent - 50;
    }
    int32_t scaled = static_cast<int32_t>(static_cast<uint32_t>(steps) << 16);
    int32_t t = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * scaled) >> 32) >> 4;   // mulhw, srawi
    t += static_cast<int32_t>(static_cast<uint32_t>(t) >> 31);
    CColour result = *from;
    result.mRed = static_cast<uint8_t>(from->mRed + ((t * (static_cast<int32_t>(to->mRed) - from->mRed)) >> 16));
    result.mGreen = static_cast<uint8_t>(from->mGreen + ((t * (static_cast<int32_t>(to->mGreen) - from->mGreen)) >> 16));
    result.mBlue = static_cast<uint8_t>(from->mBlue + ((t * (static_cast<int32_t>(to->mBlue) - from->mBlue)) >> 16));
    result.mAlpha = 0xff;
    return result;
}
