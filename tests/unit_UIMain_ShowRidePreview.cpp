// Test library for the ride and sprite previews, MAKECOLUMNBUTTON and UIMain_ZoomToThing (unit v5013; libtpw_unit_UIMain_ShowRidePreview.dylib,
// tests/test_UIMain_ShowRidePreview.py), main app:
//   src/game/ui/ui_main.cpp
//
// The callees outside the unit are fakes that record what the real ones read; the globals of other files (gText, gThingArray) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "MainWindowCallback.h"
#include "Debug.h"
#include "InterfaceWindow.h"
#include "InterfaceFontWrapper.h"
#include "TPCamera.h"
#include "Text.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"

using namespace calllog;

// ---- globals of other files, defined here (zero at start like the Mac's bss)
alignas(16) unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");
CThingArrayEntry gThingArray[kThingArraySize];

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t Raw(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) & 0xffffffffu); }

CText& Text() { return *reinterpret_cast<CText*>(gTextStorage); }

}  // namespace

// ---- the fakes
float CalcZTrans(int32_t z) {
    ev("CalcZTrans", {z});
    uint32_t bits = static_cast<uint32_t>(ret("CalcZTrans"));
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

sRidePreviewInfo* CreateRidePreview(float x, float y, float z, float w, float h, uint32_t id) {
    ev("CreateRidePreview", {CanonBits(x), CanonBits(y), CanonBits(z), CanonBits(w), CanonBits(h), id});
    return reinterpret_cast<sRidePreviewInfo*>(static_cast<uintptr_t>(ret("CreateRidePreview")));
}

void DestroyRidePreview(sRidePreviewInfo* info) { ev("DestroyRidePreview", {Raw(info)}); }

void UIMain_DestroySpritePreview() { ev("UIMain_DestroySpritePreview", {}); }

int32_t ESP_Spawn2DCopyOfScriptAt(int32_t handle, float x, float y, float z, float scale) {
    ev("ESP_Spawn2DCopyOfScriptAt", {handle, CanonBits(x), CanonBits(y), CanonBits(z), CanonBits(scale)});
    return static_cast<int32_t>(ret("ESP_Spawn2DCopyOfScriptAt"));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition & 0xff}, {format}); }

void SetCameraPOI(float* x, float* z, float* zoom, float* yaw, float* value) {
    ev("SetCameraPOI", {CanonBits(*x), CanonBits(*z), Raw(zoom), Raw(yaw), Raw(value)});
}

// the constructor: the fake must not run the base class's
extern "C" void FakeSpriteCtor(InterfaceTextObjectSprite* self, void* data) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
extern "C" void FakeSpriteCtor(InterfaceTextObjectSprite* self, void* data) { ev("InterfaceTextObjectSprite", {newlab(self), Raw(data)}); }

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {name});
    return reinterpret_cast<InterfaceDrawObject*>(static_cast<uintptr_t>(ret("FindDrawObject")));
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), Raw(object)}); }
void InterfaceWindow::SetFontJust(int32_t h, int32_t v) { ev("SetFontJust", {lab(this), h, v}); }
void InterfaceWindow::SetFontWrap(int32_t h, int32_t v) { ev("SetFontWrap", {lab(this), h, v}); }
void InterfaceWindow::SetFont(InterfaceFont* font) {
    int64_t index = -1;
    if (font != nullptr) index = static_cast<InterfaceFontWrapper*>(font) - &Text().mFonts[0];
    ev("SetFont", {lab(this), index});
}
void InterfaceWindow::SetFontColour(int32_t r, int32_t g, int32_t b, int32_t a) { ev("SetFontColour", {lab(this), r, g, b, a}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this)}, {narrow(text).c_str()}); }
void InterfaceButton::SetTextUpColour(UI_RGBA c) {
    ev("SetTextUpColour", {lab(this), static_cast<int64_t>((uint32_t(c.mR) << 24) | (uint32_t(c.mG) << 16) | (uint32_t(c.mB) << 8) | c.mA)});
}
void InterfaceButton::SetTextDownColour(UI_RGBA c) {
    ev("SetTextDownColour", {lab(this), static_cast<int64_t>((uint32_t(c.mR) << 24) | (uint32_t(c.mG) << 16) | (uint32_t(c.mB) << 8) | c.mA)});
}

extern "C" {

uint64_t tpw_mct_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"ride", &gRidePreview},
        {"peep", &gSpritePreviewPeep},
        {"handle", &gSpritePreviewHandle},
        {"skip", &gZoomSkipThing},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_mct_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// the host's offsets: CoolRect left, InterfaceWindow help text
int32_t tpw_mct_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CoolRect, mLeft));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mHelpText));
    return n;
}

void tpw_mct_setthing(uint32_t index, uint64_t pointer) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(pointer)); }
void tpw_mct_setgroup(uint32_t group) { Text().mResolutionGroup = group; }

void tpw_mct_ride(uint64_t rect, uint32_t id, int32_t z) { UIMain_ShowRidePreview(*reinterpret_cast<CoolRect*>(rect), id, z); }
void tpw_mct_sprite(uint64_t rect, uint64_t peep, int32_t z, float scale) {
    UIMain_ShowSpritePreview(*reinterpret_cast<CoolRect*>(rect), reinterpret_cast<CPeep*>(peep), z, scale);
}
void tpw_mct_column(uint64_t window, uint64_t text, int32_t font, int32_t r, int32_t g, int32_t b, int32_t help) {
    MAKECOLUMNBUTTON(reinterpret_cast<InterfaceWindow*>(window), reinterpret_cast<char16_t*>(text), font, r, g, b, help);
}
void tpw_mct_zoom(uint32_t index) {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(index);
    UIMain_ZoomToThing(id);
}

}  // extern "C"
