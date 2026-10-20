#include "keytable_actions.h"
#include "Advisor.h"
#include "LLCellSystem.h"
#include "intrface.h"
#include "Mouse.h"
#include "MapCell.h"
#include "World.h"
#include "TPSound.h"

// 0x1014c010: the "backtrack" key. Makes the mouse if there is none; when the mouse mode answers 0 or 1 and the
// cell system is in mode 0, plays a sound at the selected cell, clicks it in mode 0x32, otherwise (mode 3) backtracks.
int32_t backtrack() {
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse;   // 1 byte on the Mac; the original tests the result
        if (mouse != nullptr) {
            CMMDefault* mode = new CMMDefault;
            mouse->AssignMouseMode(mode);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
    uint32_t mouseMode = (gMouseMode == nullptr) ? 0 : static_cast<uint32_t>(gMouseMode->GetMode());
    if (LLCC_SystemIsMode(0) != 0 && mouseMode <= 1) {
        const uint16_t id = gIntrface.mF24;
        const uint32_t x = static_cast<uint32_t>(id - 1) & 0x7f;
        const uint32_t y = static_cast<uint32_t>(id - 1) >> 7;
        LLCC_SystemGetMode();
        const CMapCell* cell = reinterpret_cast<const CMapCell*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8 +
                                                                  static_cast<int32_t>(id - 1) * 0x44);
        if (cell->IsPath()) {
            const float fx = static_cast<float>(static_cast<int32_t>(x));
            const float fy = static_cast<float>(static_cast<int32_t>(y));
            SOUND_SendEvent(0, gSoundBanks[3], 0x5f, static_cast<int32_t>(10.0f * fx), 0, static_cast<int32_t>(10.0f * fy));
            LLCC_SystemSetMode(static_cast<CELL_MODES>(0x32), true);
            LLCM_LeftMouseButtonUp(static_cast<int32_t>(x), static_cast<int32_t>(y), 0, 0);
            LLCC_SystemSetMode(static_cast<CELL_MODES>(0), true);
        }
    } else if (mouseMode == 3) {
        LLCC_SystemBacktrack(nullptr, true);
    }
    return 1;
}
