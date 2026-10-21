// Test library for the finance panel static initialiser (unit v5377; libtpw_unit_sinit_ui_FinancePanel_cpp.dylib):
//   src/game/ui/ui_FinancePanel.cpp   __sinit_ui_FinancePanel_cpp, gFinancePanelColours is defined here.
//
// LbPalette_FindColour and the CUIGrapher constructor are recording fakes; the globals are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FinanceGraphPanelMsg.h"
#include "LbPalette.h"

using namespace calllog;

struct TbPalette {
    uint8_t pad[4];
};
TbPalette _lbGlobalPalette;
uint8_t gFinancePanelColours[8][5];
CUIGrapher gFinanceGrapher;

uint8_t LbPalette_FindColour(const TbPalette* palette, uint8_t red, uint8_t green, uint8_t blue) {
    ev("FindColour", {lab(const_cast<TbPalette*>(palette)), red, green, blue});
    return static_cast<uint8_t>(ret("FindColour"));
}

void CUIGrapher_Construct(CUIGrapher* self) { ev("CUIGrapher", {lab(self)}); }

extern "C" {

uint64_t tpw_fp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"pal", &_lbGlobalPalette},
        {"colours", gFinancePanelColours},
        {"grapher", &gFinanceGrapher},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gFinancePanelColours));
    return n;
}

void tpw_fp_run() { __sinit_ui_FinancePanel_cpp(); }

}  // extern "C"
