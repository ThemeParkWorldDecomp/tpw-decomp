// Test library for the tag panel functions (unit v5565; libtpw_unit_ShowTagPanel.dylib, tests/test_ShowTagPanel.py), main app:
//   src/ui_StatusPanel_SetButton_ShowTagPanel.cpp   ShowTagPanel, TagCallback
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The windows
// are zeroed blocks whose first word is a fake vtable (slot 6 is Enable); the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "analyser.h"
#include "rsse.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
int32_t gTagCount;
InterfaceWindow* gTagPanel;
InterfaceWindow* gTagPanelPending;
InterfaceWindow* gTagPanelSource;
sStaticTag gStaticTags[10];
InterfaceWindow* gTagFirstWindow;
InterfaceWindow* gAerialWindow;
UI_RectDelta gTagShiftAnim[1];

namespace {

InterfaceWindow* gObjs[16];
void* gVt[32];

int64_t R(const char* base, int n = -9999) {
    char key[48];
    if (n == -9999) std::snprintf(key, sizeof key, "%s", base);
    else std::snprintf(key, sizeof key, "%s%d", base, n);
    return ret(key);
}

InterfaceWindow* Obj(int64_t index) { return (index > 0 && index < 16) ? gObjs[index] : nullptr; }

int16_t FakeEnable(InterfaceWindow* self, int16_t on) {
    ev("Enable", {lab(self), on});
    return 1;
}

}  // namespace

// ---- CoolRegion / CoolRect: the vtable of the local rectangles needs every virtual
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRect::CoolRect() { ev("CoolRect", {}); }
CoolRect::~CoolRect() { ev("~CoolRect", {}); }

// ---- the fakes
void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {lab(this), index});
    return reinterpret_cast<void*>(static_cast<intptr_t>(static_cast<int32_t>(R("gep", index))));
}
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, static_cast<int32_t>(reinterpret_cast<intptr_t>(value))});
    return 1;
}
int32_t InterfaceWindow::stopAnimTimer(int32_t id) { ev("stopAnimTimer", {lab(this), id}); return 0; }  // int32_t since v51111
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), subtract});
    rect->mLeft = static_cast<int16_t>(R("posl"));
    rect->mTop = static_cast<int16_t>(R("post"));
    rect->mRight = static_cast<int16_t>(R("posr"));
    rect->mBottom = static_cast<int16_t>(R("posb"));
    return 1;
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 1;
}
int32_t InterfaceWindow::GetDims(int16_t* width, int16_t* height, int16_t adjust) {
    ev("GetDims", {lab(this), width ? 1 : 0, height ? 1 : 0, adjust});
    if (height) *height = static_cast<int16_t>(R("dims"));
    return 1;
}
void InterfaceWindow::AdjustPosition(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    ev("AdjustPosition", {lab(this), left, top, right, bottom});
}
InterfaceWindow* InterfaceWindow::GetNext() {
    int64_t l = lab(this);
    ev("GetNext", {l});
    return Obj(R("next", static_cast<int>(l)));
}
int32_t InterfaceWindow::ShiftPosition(int16_t dx, int16_t dy) {
    ev("ShiftPosition", {lab(this), dx, dy});
    return 1;
}
int32_t InterfaceWindow::startAnim(UI_RectDelta* anim, int32_t ticks) {
    ev("startAnim", {lab(this), anim == gTagShiftAnim ? 1 : 2, ticks});
    return 1;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return Obj(R("getwin"));
}
void InterfaceWindow::unlink() { ev("unlink", {lab(this)}); }
int16_t InterfaceWindow::GetVisibility() {
    ev("GetVisibility", {lab(this)});
    return static_cast<int16_t>(R("vis"));
}
InterfaceWindow* InterfaceWindow::GetParent() {
    ev("GetParent", {lab(this)});
    return Obj(R("parent"));
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(R("default"));
}

int32_t PAR_GetEmitterStatus(uint32_t handle) {
    ev("PAR_GetEmitterStatus", {static_cast<int32_t>(handle)});
    return static_cast<int32_t>(R("status"));
}
void PAR_KillEmitter(int32_t handle) { ev("PAR_KillEmitter", {handle}); }
void PAR_SetEmitterPos(int32_t handle, int32_t x, int32_t y, int32_t z) { ev("PAR_SetEmitterPos", {handle, x, y, z}); }
void PAR_SetEmitterMask(int32_t handle, int32_t mask) { ev("PAR_SetEmitterMask", {handle, mask}); }
uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return static_cast<uint32_t>(R("init"));
}
void LinkSlideoutPanel(InterfaceWindow* panel, int32_t n) { ev("LinkSlideoutPanel", {lab(panel), n}); }
void RemoveSlideoutPanel(int16_t which) { ev("RemoveSlideoutPanel", {which}); }
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("UIMain_AdvisorMessage", {static_cast<int32_t>(message)}); }
void UIMain_UISound(int32_t code) { ev("UIMain_UISound", {code}); }
void UIMain_MidWindowParticles(int32_t id, InterfaceWindow* window) { ev("UIMain_MidWindowParticles", {id, lab(window)}); }
int32_t FindStaticTag(InterfaceWindow* window) {
    ev("FindStaticTag", {lab(window)});
    return static_cast<int32_t>(R("fst"));
}
int32_t DoButton(TagButtonAction action, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id, int32_t extra) {
    ev("DoButton", {static_cast<int32_t>(action), id.mIndex, extra});
    return 0;
}
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    // `a` is a window pointer cut to 32 bits: find the window it was
    int64_t label = -1;
    for (int k = 1; k < 16; k++)
        if (gObjs[k] && static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gObjs[k])) == static_cast<uint32_t>(a)) label = lab(gObjs[k]);
    ev("UI_PostMessage", {lab(window), message, label, b});
    return 0;
}

extern "C" {

uint64_t tpw_stp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    gVt[6] = reinterpret_cast<void*>(&FakeEnable);
    const Entry entries[] = {
        {"count", &gTagCount},   {"panel", &gTagPanel},   {"pending", &gTagPanelPending}, {"source", &gTagPanelSource},
        {"tags", gStaticTags},   {"first", &gTagFirstWindow}, {"aerial", &gAerialWindow},  {"anim", gTagShiftAnim},
        {"objs", gObjs},         {"vt", gVt},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_stp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_stp_layout(int64_t* out) {
    alignas(16) static char buffer[2048];
    InterfaceWindow* w = reinterpret_cast<InterfaceWindow*>(buffer);
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow));
    out[n++] = reinterpret_cast<char*>(&w->mF134) - buffer;
    out[n++] = reinterpret_cast<char*>(&w->mAnimating) - buffer;
    out[n++] = static_cast<int64_t>(sizeof(sStaticTag));
    out[n++] = static_cast<int64_t>(offsetof(sStaticTag, mActive));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    return n;
}

void tpw_stp_show(uint64_t window) { ShowTagPanel(reinterpret_cast<InterfaceWindow*>(window)); }
int32_t tpw_stp_callback(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return TagCallback(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}

}  // extern "C"
