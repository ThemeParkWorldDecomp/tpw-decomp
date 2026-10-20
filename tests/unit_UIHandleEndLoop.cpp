// Test library for UIHandleEndLoop, UIHandleMidLoop and SetPylonType (unit v5707; libtpw_unit_UIHandleEndLoop.dylib, tests/test_UIHandleEndLoop.py), main app:
//   src/LayoutSetRotatePylon_DeleteLastTrackBrace_UIHandleEndLoop.cpp
//
// The callees outside the unit are fakes recording their arguments; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "TPCoaster.h"
#include "UIInterface.h"
#include "ui_main.h"

using namespace calllog;

tag_sCoasterInstance* gCoasterEditInstance;
uint32_t gCoasterEditEnabled;
uint32_t gCoasterEditDown;
uint32_t gCoasterWord16DC;
uint32_t gCoasterWord1790;
uint32_t gCoasterInputFlags;
CActionRecorder gActionRecorder;
UIInterface gUIInterface;

void GetCoasterEditAvailableButtons(uint32_t* out) { ev("GetCoasterEditAvailableButtons", {lab(out)}); }
void SetCoasterButtonState(uint32_t state, uint32_t arg) { ev("SetCoasterButtonState", {state, arg}); }
void UIInterface::ChangeCoasterButtonsEnabled(uint32_t mask) { ev("ChangeCoasterButtonsEnabled", {lab(this), mask}); }
void UIInterface::ChangeCoasterButtonsDown(uint32_t mask) { ev("ChangeCoasterButtonsDown", {lab(this), mask}); }
void CActionRecorder::RecordAction(RecordedAction action) { ev("RecordAction", {lab(this), static_cast<int64_t>(action)}); }
void SetPotentialPylonArea() { ev("SetPotentialPylonArea", {}); }

extern "C" {

uint64_t tpw_u7_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"edit", &gCoasterEditInstance},
        {"enabled", &gCoasterEditEnabled},
        {"down", &gCoasterEditDown},
        {"w16dc", &gCoasterWord16DC},
        {"w1790", &gCoasterWord1790},
        {"input", &gCoasterInputFlags},
        {"rec", &gActionRecorder},
        {"ui", &gUIInterface},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_u7_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_u7_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBlock3C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBraces));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mPreviewBrace));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFlags));
    return n;
}

void tpw_u7_end() { UIHandleEndLoop(); }
void tpw_u7_mid() { UIHandleMidLoop(); }
void tpw_u7_pylon(uint32_t type) { SetPylonType(type); }

}  // extern "C"
