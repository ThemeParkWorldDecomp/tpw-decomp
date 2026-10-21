// Test library for CUI2DMapSprite::UpdateVisibilityAndColour (unit v5652; libtpw_unit_CUI2DMapSprite_UpdateVisibilityAndColour.dylib), main app:
//   src/game/ui/ui_2dmap.cpp
//
// Fakes: CEZSprite::GetSpriteDetails (logs the instance, answers a block the test allocates) and UI2DMReturnScaleColour (logs the index and
// the offsets of the three colours inside the table, answers a colour word from the queue "Scale").
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "DrawRenderClasses.h"
#include "InterfaceWindow.h"
#include "tb_ezsprite.h"
#include "ui_main.h"

using namespace calllog;

CUI2DMapColourEntry gUI2DMapColours[23];
CEZSprite gei_EZSprite;

CEZSprite::CEZSprite() {}
CEZSprite::~CEZSprite() {}

static void* gDet = nullptr;

void* CEZSprite::GetSpriteDetails(void* instance) {
    ev("GetSpriteDetails", {lab(instance)});
    return gDet;
}

bfl::draw::CColour UI2DMReturnScaleColour(int32_t index, const bfl::draw::CColour& a, const bfl::draw::CColour& b,
                                          const bfl::draw::CColour& c) {
    const char* base = reinterpret_cast<const char*>(gUI2DMapColours);
    ev("Scale", {index, reinterpret_cast<const char*>(&a) - base, reinterpret_cast<const char*>(&b) - base,
                 reinterpret_cast<const char*>(&c) - base});
    uint32_t w = static_cast<uint32_t>(ret("Scale"));
    bfl::draw::CColour r{};
    r.mRed = w >> 24;
    r.mGreen = w >> 16;
    r.mBlue = w >> 8;
    r.mAlpha = w;
    return r;
}

extern "C" {

uint64_t tpw_uvc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"colours", &gUI2DMapColours},
        {"det", &gDet},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_uvc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_uvc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mKind));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mThing));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mMode));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mMatch));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mSprite));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mVisible));
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMapSprite));
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMapColourEntry));
    return n;
}

void tpw_uvc_run(uint64_t object, int32_t mask, int32_t second, int32_t third) {
    reinterpret_cast<CUI2DMapSprite*>(static_cast<uintptr_t>(object))->UpdateVisibilityAndColour(mask, second, third);
}

}  // extern "C"
