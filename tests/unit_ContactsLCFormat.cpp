// Test library for the online contacts and messages list functions (unit v5279; libtpw_unit_ContactsLCFormat.dylib,
// tests/test_ContactsLCFormat.py), main app:
//   src/game/ui/ui_onlinePanel.cpp   the seven functions and the four globals it defines
//
// The callees outside the unit are recording fakes that answer from the test's queues; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FillLenderList.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "image_copies.h"

// The tables as loaded, for the image test (unit v5L39).
TPW_IMAGE_COPY(msgcol, gMessagesLCColours);
TPW_IMAGE_COPY(concol, gContactsLCColours);

using namespace calllog;

// ---- globals of other files, defined here
int16_t gUIRealScreenHeight;
InterfaceWindow* gOnlinePanel;

namespace {
alignas(16) uint8_t gDummyPanel[64];
alignas(16) uint8_t gDummyA[64];
alignas(16) uint8_t gDummyB[64];
alignas(16) uint8_t gDummyWindow[64];
alignas(16) uint8_t gDummyFont[64];
alignas(16) char16_t gText1[8];
alignas(16) char16_t gText2[8];

int64_t P(const void* p) { return lab(p); }
}  // namespace

// ---- the fakes
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(gDummyFont);
}

int32_t InterfaceFont::GetHeight() {
    ev("GetHeight", {P(this)});
    return static_cast<int32_t>(ret("GetHeight"));
}

void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {P(this), P(reinterpret_cast<const void*>(callback))});
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {P(this), P(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {P(this), P(font)}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {P(this), red, green, blue, alpha});
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {P(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {P(this), id});
    if (id == 0xc3123) return reinterpret_cast<InterfaceWindow*>(gDummyA);
    if (id == 0xc3124) return reinterpret_cast<InterfaceWindow*>(gDummyB);
    return nullptr;
}

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    ev("AddItem", {P(this), P(item->mText), data, position, update});
    return 0;
}
int32_t InterfaceListControl2::FindString(char16_t* text, int32_t column) {
    ev("FindString", {P(this), P(text), column});
    return static_cast<int32_t>(ret("FindString"));
}
int32_t InterfaceListControl2::RemoveItem(int32_t position) {
    ev("RemoveItem", {P(this), position});
    return 0;
}
void InterfaceListControl2::SetData(int32_t position, int32_t data) { ev("SetData", {P(this), position, data}); }
void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {P(this)}); }

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("OperatorNew", {static_cast<int64_t>(size)});
    if (ret("OperatorNewNull") != 0) return nullptr;
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {newlab(mem)}); }
void WindowCtor(void* mem, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                int16_t bottom) {
    ev("WindowCtor", {newlab(mem), P(parent), attributes, id, left, top, right, bottom});
}
}  // namespace lender_callee

extern "C" {

uint64_t tpw_cl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"fontIdx", &gContactsFontIndex},
        {"msgcol", gMessagesLCColours},
        {"concol", gContactsLCColours},
        {"busy", &gOnlineContactsBusy},
        {"screenH", &gUIRealScreenHeight},
        {"winP", gDummyPanel},
        {"winA", gDummyA},
        {"winB", gDummyB},
        {"winW", gDummyWindow},
        {"font", gDummyFont},
        {"text1", gText1},
        {"text2", gText2},
        {"cb", reinterpret_cast<const void*>(&ContactsLCEntryHandler)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cl_layout(int64_t*) { return 0; }

void tpw_cl_setpanel(int32_t on) { gOnlinePanel = on ? reinterpret_cast<InterfaceWindow*>(gDummyPanel) : nullptr; }

int64_t tpw_cl_fmt() { return lab(ContactsLCFormat()); }
void tpw_cl_post(uint64_t window) { ContactsLCPostFormat(reinterpret_cast<InterfaceWindow*>(window)); }
int32_t tpw_cl_conh(uint64_t window, int32_t m, int32_t a, int32_t b) {
    return ContactsLCEntryHandler(reinterpret_cast<InterfaceWindow*>(window), m, a, b);
}
int32_t tpw_cl_msgh(uint64_t window, int32_t m, int32_t a, int32_t b) {
    return MessagesLCEntryHandler(reinterpret_cast<InterfaceWindow*>(window), m, a, b);
}
void tpw_cl_add(uint64_t text, int32_t data) { _OnlineAddContact(reinterpret_cast<char16_t*>(text), data); }
void tpw_cl_rem(uint64_t text) { _OnlineRemoveContact(reinterpret_cast<char16_t*>(text)); }
void tpw_cl_upd(uint64_t text, int32_t data) { _OnlineUpdateContact(reinterpret_cast<char16_t*>(text), data); }

}  // extern "C"
