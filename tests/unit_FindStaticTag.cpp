// Test library for FindStaticTag and AddTag (unit v5249; libtpw_unit_FindStaticTag.dylib, tests/test_FindStaticTag.py), main app:
//   src/game/ui/ui_tags.cpp
//
// AddATag and ShowTagPanel are recording fakes; gStaticTags is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"

using namespace calllog;

sStaticTag gStaticTags[10];

InterfaceWindow* AddATag(CTagParams& params) {
    ev("AddATag", {static_cast<int64_t>(params.mShow)});
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(ret("AddATag")));
}

void ShowTagPanel(InterfaceWindow* window) {
    ev("ShowTagPanel", {lab(window)});
}

extern "C" {

void tpw_ft_set(int32_t index, uint64_t window) {
    gStaticTags[index].mWindow = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window));
}

int32_t tpw_ft_find(uint64_t window) {
    return FindStaticTag(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)));
}

uint32_t tpw_ft_add(uint32_t show) {
    CTagParams params;
    std::memset(&params, 0, sizeof params);
    params.mShow = static_cast<uint8_t>(show);
    return AddTag(params) ? 1u : 0u;
}

}  // extern "C"
