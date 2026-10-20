#include <cstdint>
#include <cstring>
#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "UIInterface.h"
#include "ui_main.h"

// 0x10057064
void SetCoasterButtonState(uint32_t state, uint32_t arg) {
    uint32_t bit = 1;
    while (arg != 0) {
        uint32_t hit = arg & bit;
        if (hit != 0) {
            if ((hit & 0x1ff) != 0) {
                DisableCurrentPylonEditState();
                gCoasterEditDown &= ~hit;
            }
            arg &= ~bit;
        }
        bit <<= 1;
    }
    bit = 1;
    while (state != 0) {
        if ((state & bit) != 0) {
            HandleCoasterButtonDown(state & bit);
            state &= ~bit;
        }
        bit <<= 1;
    }
}
