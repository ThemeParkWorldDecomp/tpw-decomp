// Test library for OverrideTag 0x10191d7c and TagDlgCallback 0x10191628 (unit v5531; libtpw_unit_OverrideTag.dylib,
// tests/test_OverrideTag.py), main app:
//   src/ui_StatusPanel_FindStaticTag_OverrideTag.cpp   OverrideTag, TagDlgCallback, gTagPanelPending, gTagSlideAnim
//
// Every callee outside the unit is a fake that records what the real one reads and answers from the test's queues. The windows are
// plain blocks of the arena with a hand-made vtable (Enable and SetText record; the Mac's slots 0x1c and 0x2c). The globals of other
// files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "InterfaceWindow.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "analyser.h"
#include "image_copies.h"

// The tables as loaded, for the image test (unit v5L39).
TPW_IMAGE_COPY(slide, gTagSlideAnim);
TPW_IMAGE_COPY(shift, gTagShiftAnim);

using namespace calllog;

// ---- globals of other files, defined here
InterfaceWindow* gTagPanel;
InterfaceWindow* gTagPanelSource;
InterfaceWindow* gTagFirstWindow;
InterfaceWindow* gAerialWindow;

namespace {

InterfaceWindow* gNodes[8];
char16_t* gBufs[4];
char16_t* gTxt;
CTagParams* gParams;
int32_t gPtrIndex = 0;                              // the getExtraParam index that answers a buffer
InterfaceDrawObject* const kDrawObject = reinterpret_cast<InterfaceDrawObject*>(uintptr_t(0x7000a000));

InterfaceWindow* NodeOf(int64_t k) { return (k >= 1 && k <= 6) ? gNodes[k] : nullptr; }

int64_t Lab(const void* p) { return lab(const_cast<void*>(p)); }

// The label of the window whose address, cut to the Mac's 32 bits, is `a` (the arena is wider than 32 bits on the host).
int64_t LabLow(int32_t a) {
    if (a == 0) return 0;
    for (int k = 1; k <= 6; ++k)
        if (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gNodes[k])) == static_cast<uint32_t>(a)) return Lab(gNodes[k]);
    return -1;
}

int16_t FakeEnable(InterfaceWindow* self, int16_t on) {
    ev("Enable", {Lab(self), on});
    return 0;
}
int32_t FakeSetText(InterfaceWindow* self, char16_t* text) {
    ev("SetText", {Lab(self), Lab(text)});
    return 0;
}
void FakeOther() { ev("OTHER", {}); }

void* gVtable[32];

int SlotOf(void* member_ptr_bytes) {
    intptr_t words[2];
    std::memcpy(words, member_ptr_bytes, sizeof(words));
    return static_cast<int>(words[0] / 8);   // arm64: the offset itself, the virtual flag is in the adjustment word
}

}  // namespace

// ---- the fakes
void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("GEP", {Lab(this), index});
    char key[16];
    std::snprintf(key, sizeof(key), "GEP%d", index);
    uint32_t value = static_cast<uint32_t>(ret(key));
    if (index == gPtrIndex) return (value >= 1 && value <= 3) ? static_cast<void*>(gBufs[value]) : nullptr;
    return reinterpret_cast<void*>(static_cast<uintptr_t>(value));
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GW", {Lab(this), id});
    char key[16];
    std::snprintf(key, sizeof(key), "GW%d", id - 0x10d2e);
    return NodeOf(ret(key));
}
InterfaceWindow* InterfaceWindow::GetNext() {
    ev("GN", {Lab(this)});
    char key[16];
    std::snprintf(key, sizeof(key), "GN%d", static_cast<int>(Lab(this)));
    return NodeOf(ret(key));
}
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SCDT", {Lab(this), Lab(text)}); }
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SDO", {Lab(this), Lab(object)}); }
void InterfaceWindow::setZ(int16_t z) { ev("SZ", {Lab(this), z}); }
int32_t InterfaceWindow::startAnim(UI_RectDelta* anim, int32_t ticks) {
    ev("SA", {Lab(this), Lab(anim), ticks});
    return 0;
}
bool InterfaceWindow::stopAnim() {
    ev("STA", {Lab(this)});
    return false;
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CDW", {Lab(this), message, a, b});
    return static_cast<int32_t>(ret("CD"));
}
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    int64_t sum = 0;
    for (int i = 0; name[i] != 0; ++i) sum += (i + 1) * static_cast<int64_t>(static_cast<unsigned char>(name[i]));
    ev("FDO", {sum});
    return kDrawObject;
}
void SetButton(InterfaceWindow* window, TagButtonAction action) { ev("SB", {Lab(window), static_cast<int32_t>(action)}); }
int32_t DoButton(TagButtonAction action, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id, int32_t extra) {
    ev("DB", {static_cast<int32_t>(action), id.mIndex, extra});
    return static_cast<int32_t>(ret("DB"));
}
void RemoveSlideoutPanel(int16_t which) { ev("RSP", {which}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CAM", {static_cast<int32_t>(message)}); }
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("PM", {Lab(window), message, LabLow(a), b});
    return 0;
}
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("SM", {Lab(window), message, LabLow(a), b});
    return 0;
}
char16_t* wcsncpy(char16_t* dst, const char16_t* src, uint32_t count) {
    ev("wcsncpy", {Lab(dst), Lab(src), static_cast<int64_t>(count)});
    uint32_t i = 0;
    for (; i < count && src[i] != 0; ++i) dst[i] = src[i];
    for (; i < count; ++i) dst[i] = 0;
    return dst;
}

extern "C" {

uint64_t tpw_ot_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gTagPanel", &gTagPanel},         {"gTagPanelSource", &gTagPanelSource}, {"gTagPanelPending", &gTagPanelPending},
        {"gTagFirstWindow", &gTagFirstWindow}, {"gAerialWindow", &gAerialWindow},   {"anim", gTagSlideAnim},
        {"dobj", kDrawObject},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// Makes a fresh world: six windows, three destination buffers, a text, a parameter block; every window has the fake vtable.
void tpw_ot_world() {
    static bool once = false;
    if (!once) {
        once = true;
        void* fake = reinterpret_cast<void*>(&FakeOther);
        for (void*& slot : gVtable) slot = fake;
        int16_t (InterfaceWindow::*enable)(int16_t) = &InterfaceWindow::Enable;
        int32_t (InterfaceWindow::*settext)(char16_t*) = &InterfaceWindow::SetText;
        gVtable[SlotOf(&enable)] = reinterpret_cast<void*>(&FakeEnable);
        gVtable[SlotOf(&settext)] = reinterpret_cast<void*>(&FakeSetText);
    }
    for (int k = 1; k <= 6; ++k) {
        gNodes[k] = static_cast<InterfaceWindow*>(tpw::ArenaAllocZeroed(sizeof(InterfaceWindow)));
        *reinterpret_cast<void**>(gNodes[k]) = &gVtable[0];
    }
    for (int k = 1; k <= 3; ++k) gBufs[k] = static_cast<char16_t*>(tpw::ArenaAllocZeroed(0x200));
    gTxt = static_cast<char16_t*>(tpw::ArenaAllocZeroed(0x200));
    gParams = static_cast<CTagParams*>(tpw::ArenaAllocZeroed(sizeof(CTagParams)));
}

// kind: 0 window k, 1 buffer k, 2 text, 3 parameters
uint64_t tpw_ot_obj(int32_t kind, int32_t k) {
    void* p = nullptr;
    if (kind == 0) p = gNodes[k];
    if (kind == 1) p = gBufs[k];
    if (kind == 2) p = gTxt;
    if (kind == 3) p = gParams;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

int32_t tpw_ot_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CTagParams, mF4));
    out[n++] = static_cast<int64_t>(offsetof(CTagParams, mText));
    out[n++] = static_cast<int64_t>(offsetof(CTagParams, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(CTagParams, mF20));
    out[n++] = static_cast<int64_t>(sizeof(CTagParams));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow));
    int16_t (InterfaceWindow::*enable)(int16_t) = &InterfaceWindow::Enable;
    int32_t (InterfaceWindow::*settext)(char16_t*) = &InterfaceWindow::SetText;
    intptr_t w[2];
    std::memcpy(w, &enable, 16);
    out[n++] = w[0];
    out[n++] = w[1];
    std::memcpy(w, &settext, 16);
    out[n++] = w[0];
    return n;
}

uint32_t tpw_ot_override() {
    gPtrIndex = 2;
    return OverrideTag(*gParams) ? 1u : 0u;
}

uint32_t tpw_ot_dlg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    gPtrIndex = 1;
    return static_cast<uint32_t>(TagDlgCallback(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b));
}

}  // extern "C"
