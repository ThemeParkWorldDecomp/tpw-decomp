// LLCM_LeftMouseButtonDown 0x10084a00 (624 bytes): the left button goes down on a cell.
#include <cstdint>

#include "ActionRecorder.h"
#include "CellBase.h"
#include "DynamicMesh.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "World.h"
#include "ui_main.h"

uint8_t gLLCCDB34;   // 0x102ddb34
uint8_t gLLCCE090;   // 0x102de090

void LLCM_LeftMouseButtonDown(int32_t x, int32_t y, uint16_t shift, int32_t flag) {
    gLLCCDB34 = 1;
    if (gActionRecorder.mRecording != 0) {
        gLLCCC214 = 0;
        if (gLLCCE0DC == 0) LLCM_MouseMoved(x, y, shift);
    }
    if (flag != 0) {
        x = gLLCCDB54;
        y = gLLCCDB50;
    }
    if (LLCM_GetIllegalPlacement() == 0) {
        if (shift == 0)
            gActionRecorder.RecordAction(static_cast<RecordedAction>(0xf), static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        else
            gActionRecorder.RecordAction(static_cast<RecordedAction>(0x28), static_cast<uint8_t>(x), static_cast<uint8_t>(y), shift);
    }
    if (LLCC_SystemIsMode(0x38) == 0 && LLCM_GetIllegalPlacement() != 0) return;
    if (LLCC_SystemIsMode(0) != 0) return;
    if (LLCC_SystemIsMode(0x16) == 0) return;
    LLCM_SetCurrentObject(false);
    if (gLLCCE090 == 0) {
        CoasterAcceptLeftMouseClick(1);
        return;
    }
    LLCC_SystemAddToClickStack(x, y);
    LLCC_DrawLine(static_cast<CELL_MODES>(0x87), x, y, &x, &y);
    LLCC_DrawLine(static_cast<CELL_MODES>(0x15), x, y, &x, &y);
    LLCC_DrawLine(static_cast<CELL_MODES>(0x83), x, y, &x, &y);
    LLCC_DrawLine(static_cast<CELL_MODES>(0x81), x, y, &x, &y);
    const uint16_t index = static_cast<uint16_t>(x + (y << 7) + 1);
    uint8_t* world = reinterpret_cast<uint8_t*>(gGameWorld);
    const uint32_t result = DoMovePylon(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
    *reinterpret_cast<uint32_t*>(world + 0x2d8 + (static_cast<int32_t>(index) - 1) * 0x28 + 0x110000 + 4) = result;
    gLLCC9F14 = -1;
    gLLCCE090 = 0;
}
