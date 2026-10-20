// Test library for LLCM_LeftMouseButtonDown (unit v5089; libtpw_unit_LLCM_LeftMouseButtonDown.dylib, tests/test_LLCM_LeftMouseButtonDown.py), main app:
//   src/LLCM_LeftMouseButtonDown.cpp   LLCM_LeftMouseButtonDown and the two bytes gLLCCDB34, gLLCCE090
//
// The callees outside the unit are recording fakes; the globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "CellBase.h"
#include "DynamicMesh.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "World.h"
#include "ui_main.h"

using namespace calllog;

int32_t gLLCC9F14;
int32_t gLLCCC214;
int32_t gLLCCE0DC;
int32_t gLLCCDB50;
int32_t gLLCCDB54;
CWorld* gGameWorld;
CActionRecorder gActionRecorder;

// ---- the fakes
void LLCM_MouseMoved(int32_t x, int32_t y, uint16_t flag) { ev("MouseMoved", {x, y, flag}); }

int32_t LLCM_GetIllegalPlacement() {
    ev("GetIllegalPlacement", {});
    return static_cast<int32_t>(ret("Illegal"));
}

void CActionRecorder::RecordAction(RecordedAction action, uint8_t a, uint8_t b) { ev("Rec2", {action, a, b}); }

void CActionRecorder::RecordAction(RecordedAction action, uint8_t a, uint8_t b, uint16_t c) { ev("Rec3", {action, a, b, c}); }

int32_t LLCC_SystemIsMode(int32_t mode) {
    ev("IsMode", {mode});
    return static_cast<int32_t>(ret("IsMode", mode, 0));
}

void LLCM_SetCurrentObject(bool flag) { ev("SetCurrentObject", {flag}); }

int32_t LLCC_SystemAddToClickStack(int32_t x, int32_t y) {
    ev("AddToClickStack", {x, y});
    return 1;
}

void LLCC_DrawLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    ev("DrawLine", {mode, x, y, *endX, *endY});
    int32_t nx = static_cast<int32_t>(ret("DrawX"));
    int32_t ny = static_cast<int32_t>(ret("DrawY"));
    if (nx != 999) *endX = nx;
    if (ny != 999) *endY = ny;
}

uint32_t DoMovePylon(uint32_t x, uint32_t z) {
    ev("DoMovePylon", {x, z});
    return static_cast<uint32_t>(ret("DoMovePylon"));
}

void CoasterAcceptLeftMouseClick(int32_t down) { ev("CoasterAcceptLeftMouseClick", {down}); }

extern "C" {

uint64_t tpw_llb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"rec", &gActionRecorder.mRecording},
        {"c214", &gLLCCC214},
        {"e0dc", &gLLCCE0DC},
        {"db34", &gLLCCDB34},
        {"db50", &gLLCCDB50},
        {"db54", &gLLCCDB54},
        {"e090", &gLLCCE090},
        {"f14", &gLLCC9F14},
        {"world", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_llb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_llb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mRecording));
    return n;
}

void tpw_llb_down(int32_t x, int32_t y, uint32_t shift, int32_t flag) {
    LLCM_LeftMouseButtonDown(x, y, static_cast<uint16_t>(shift), flag);
}

}  // extern "C"
