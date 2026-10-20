#include <cstdint>
#include <cstring>
#include "DrawRenderClasses.h"
#include "InterfaceWindow.h"
#include "ppc_math.h"
#include "tb_ezsprite.h"
#include "ui_main.h"

namespace {
void SetFlag(CEZSprite& e, void* sprite, int32_t visible) {
    uint8_t* d = static_cast<uint8_t*>(e.GetSpriteDetails(sprite));
    uint32_t f = *reinterpret_cast<uint32_t*>(d + 0x10);
    f = visible == 0 ? (f | 1u) : (f & 0xfffffffeu);
    *reinterpret_cast<uint32_t*>(d + 0x10) = f;
}
}  // namespace

// 0x10231134
void CUI2DMapSprite::UpdateVisibilityAndColour(int32_t mask, int32_t second, int32_t third) {
    const uint8_t* table = reinterpret_cast<const uint8_t*>(gUI2DMapColours);
    if ((mMode == 1 && (static_cast<uint32_t>(mask) & static_cast<uint32_t>(mMatch)) != 0) ||
        (mMode == 2 && second == mMatch) || (mMode == 3 && third == mMatch)) {
        if (mVisible != 1) {
            mVisible = 1;
            SetFlag(gei_EZSprite, mSprite, mVisible);
        }
        uint8_t col[0x24];
        std::memcpy(col, table + 0x24, 0x24);
        switch (mKind) {
        case 0: {
            uint8_t* t = static_cast<uint8_t*>(mThing);
            if (t && t[2] == 1) {
                float v = *reinterpret_cast<float*>(t + 0x19c);
                int32_t idx = ppc_fctiwz(v);
                bfl::draw::CColour r = UI2DMReturnScaleColour(
                    static_cast<int32_t>(static_cast<uint32_t>(idx) & 0xff),
                    *reinterpret_cast<const bfl::draw::CColour*>(table + 0x264),
                    *reinterpret_cast<const bfl::draw::CColour*>(table + 0x288),
                    *reinterpret_cast<const bfl::draw::CColour*>(table + 0x2ac));
                std::memcpy(col, &r, 0x24);
            }
            break;
        }
        case 1: std::memcpy(col, table + 0x2d0, 0x24); break;
        case 3: std::memcpy(col, table + 0x2f4, 0x24); break;
        case 4: {
            uint8_t* t = static_cast<uint8_t*>(mThing);
            if (t && t[2] == 6) {
                float v = *reinterpret_cast<float*>(t + 0x1f4);
                int32_t idx = ppc_fctiwz(v);
                bfl::draw::CColour r = UI2DMReturnScaleColour(
                    static_cast<int32_t>(static_cast<uint32_t>(idx) & 0xff),
                    *reinterpret_cast<const bfl::draw::CColour*>(table + 0x264),
                    *reinterpret_cast<const bfl::draw::CColour*>(table + 0x288),
                    *reinterpret_cast<const bfl::draw::CColour*>(table + 0x2ac));
                std::memcpy(col, &r, 0x24);
            }
            break;
        }
        case 5: std::memcpy(col, table + 0x318, 0x24); break;
        default: break;
        }
        uint32_t v = (static_cast<uint32_t>(col[1]) << 16) | (static_cast<uint32_t>(col[2]) << 24) |
                     (static_cast<uint32_t>(col[0]) << 8) | col[3];
        uint8_t* d = static_cast<uint8_t*>(gei_EZSprite.GetSpriteDetails(mSprite));
        *reinterpret_cast<uint32_t*>(d + 0x24) = v;
    } else if (mVisible != 0) {
        mVisible = 0;
        SetFlag(gei_EZSprite, mSprite, mVisible);
    }
}
