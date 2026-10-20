// Recording harness for the original UI focus-state routines.
#include "UI_Base.h"
#include "calllog.h"

namespace { int gRoot; }

void activateUI(InterfaceWindow* window, int32_t active) {
    calllog::ev("activateUI", {calllog::lab(window), active, gUIActive, gUIButtonFlags});
}

extern "C" {
void tpw_ui_focus_seed(int32_t root, int32_t active, int32_t buttons) {
    gUIRootWindow = root != 0 ? reinterpret_cast<InterfaceWindow*>(&gRoot) : nullptr;
    gUIActive = active;
    gUIButtonFlags = static_cast<int16_t>(buttons);
    tpw_cl_label(1, &gRoot);
}
int32_t tpw_ui_focus_op(int32_t op, int64_t* state) {
    int32_t result = 0;
    if (op == 1) UI_Activate();
    else if (op == 2) UI_DeActivate();
    else if (op == 3) result = UI_GetButtonFlags();
    state[0] = gUIRootWindow != nullptr;
    state[1] = gUIActive;
    state[2] = gUIButtonFlags;
    return result;
}
}
