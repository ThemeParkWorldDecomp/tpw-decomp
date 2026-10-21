// Test library for the park name panel (unit v5057; libtpw_unit_ShowEditParkNamePanel.dylib, tests/test_ShowEditParkNamePanel.py), main app:
//   src/game/ui/ui_entryPricePanel.cpp   ShowEditParkNamePanel, TwoEditsMsg and the unit's globals (the panel, the two text areas, the cheat texts)
//
// The callees outside the unit are fakes that record what the real ones read, under the same event names as the module's PowerPC stubs.
// A window is identified by the tag the test puts in its id (mId); a thing by the word at its start. The windows are blocks of
// the arena with a hand-made vtable (every slot answers like SetText), so the virtual call in the panel code lands in the fake.
// The globals of other files the unit reads are defined here (zero at start like the Mac's bss).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"

#include "Cheat.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
CFSGlobalMasterSave* gMasterSave;
int8_t gGameWinCheatGuard;
alignas(4) uint8_t gGameWinCheat[12];
char16_t gUIMainConvertBuffer[0x4000];

namespace {

template <class T>
int64_t I(T v) {
    return static_cast<int64_t>(v);
}
int64_t P(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }
int64_t wid(const InterfaceWindow* w) { return w ? w->mId : 0; }
int64_t objTag(const void* p) { return p ? *static_cast<const uint32_t*>(p) : 0; }

int64_t whichBuf(const void* p) {
    if (p == gEditSignBufA) return 1;
    if (p == gEditSignBufB) return 2;
    if (p == gUIMainConvertBuffer) return 3;
    return 0;
}

void evText(const char* name, int64_t first, const char16_t* t) {
    int64_t n = -1, c[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    if (t != nullptr) {
        n = 0;
        while (n < 9 && t[n] != 0) n++;
        for (int i = 0; i < 8 && i < n; i++) c[i] = t[i];
    }
    ev(name, {first, n, c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]});
}

struct Child {
    int32_t parent;
    int32_t id;
    InterfaceWindow* window;
};
Child gChildren[16];
int32_t gChildCount = 0;
InterfaceWindow* gPanelBlock = nullptr;
const char16_t* gSignA = nullptr;     // what GetSignNameA writes
const char16_t* gSignB = nullptr;
const char16_t* gStringBlock = nullptr;   // what UIMain_GetString answers
char16_t* gNameBlock = nullptr;           // what UIMain_GetObjectName answers

void copyText(char16_t* to, const char16_t* from) {
    size_t i = 0;
    while (from[i] != 0) {
        to[i] = from[i];
        i++;
    }
    to[i] = 0;
}

int32_t fakeSetText(InterfaceWindow* self, char16_t* text) {
    evText("SetText", wid(self), text);
    return 1;
}
void* gVtable[40];

}  // namespace

// ---- the fakes
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {wid(this), I(description == gEditParkNameScript), I(callback == TwoEditsMsg)});
    return gPanelBlock;
}
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {wid(this), I(z)}); }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {wid(this), I(id)});
    for (int32_t i = 0; i < gChildCount; i++)
        if (gChildren[i].parent == this->mId && gChildren[i].id == id) return gChildren[i].window;
    return nullptr;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {wid(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {wid(this), P(font)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {wid(this), I(horizontal), I(vertical)}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {wid(this), I(red), I(green), I(blue), I(alpha)});
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {wid(this), I(on)}); }
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {wid(this), I(message), I(a), I(b)});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

int32_t InterfaceEditBox::SetStaticTextArea(char16_t* text, int32_t capacity) {
    ev("SetStaticTextArea", {wid(this), whichBuf(text), I(capacity)});
    return 0;
}
void InterfaceEditBox::SetCursorDrawObject(InterfaceDrawObject* object) { ev("SetCursorDrawObject", {wid(this), P(object)}); }
void InterfaceEditBox::SelectAll() { ev("SelectAll", {wid(this)}); }

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    int64_t n = 0, c[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    while (n < 9 && name[n] != 0) n++;
    for (int i = 0; i < 8 && i < n; i++) c[i] = static_cast<uint8_t>(name[i]);
    ev("FindDrawObject", {n, c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]});
    return reinterpret_cast<InterfaceDrawObject*>(static_cast<uintptr_t>(ret("FindDrawObject")));
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {I(index)});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(0x1000 + index * 16));
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {I(id)});
    return gStringBlock;
}
void UI_TITLE_COLOUR(InterfaceWindow* window) { ev("UI_TITLE_COLOUR", {wid(window)}); }
void GAME_ASSERTMSG(int32_t condition, const char*, ...) { ev("GAME_ASSERTMSG", {I(condition & 0xff)}); }
void UIMain_DisableAllKeyTables() { ev("UIMain_DisableAllKeyTables", {}); }
void UIMain_EnableAllKeyTables() { ev("UIMain_EnableAllKeyTables", {}); }
void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture", {}); }
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {wid(window), I(message), I(a), I(b)});
    return 0;
}
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {wid(window), I(message), I(a), I(b)});
    return 0;
}
char16_t* UIMain_GetObjectName(CObject* object) {
    ev("UIMain_GetObjectName", {objTag(object)});
    return gNameBlock;
}
void RegenerateRideSign(void* object, const char16_t* signA, const char16_t* signB) {
    ev("RegenerateRideSign", {objTag(object), whichBuf(signA), whichBuf(signB)});
}
int32_t wcscmp(const char16_t* a, const char16_t* b) {
    while (*a != 0 && *a == *b) {
        a++;
        b++;
    }
    return static_cast<int32_t>(*a) - static_cast<int32_t>(*b);
}

void CObject::GetSignNameA(char16_t* buffer, uint32_t size) {
    ev("GetSignNameA", {objTag(this), whichBuf(buffer), I(size)});
    copyText(buffer, gSignA);
}
void CObject::GetSignNameB(char16_t* buffer, uint32_t size) {
    ev("GetSignNameB", {objTag(this), whichBuf(buffer), I(size)});
    copyText(buffer, gSignB);
}
void CObject::SetSignNameA(const char16_t* text) { evText("SetSignNameA", objTag(this), text); }
void CObject::SetSignNameB(const char16_t* text) { evText("SetSignNameB", objTag(this), text); }

void CFSGlobalMasterSave::SetSignNameA(const char16_t* text) { evText("MasterSetSignNameA", lab(this), text); }
void CFSGlobalMasterSave::SetSignNameB(const char16_t* text) { evText("MasterSetSignNameB", lab(this), text); }
CCheat::CCheat() { ev("CCheat", {newlab(this)}); }
void CCheat::SetCheatStatus(bool cheating) { ev("SetCheatStatus", {lab(this), I(cheating)}); }

// The constructors of classes whose bases are not in this library: the complete-object symbols, made by hand.
extern "C" void tpw_v5057_titlector(void* self, void* data) { ev("InterfaceTitleTextObject", {newlab(self), P(data)}); }
extern "C" void tpw_v5057_cursorctor(void* self, void* data) {
    ev("InterfaceCursorPosTextObjectSprite", {newlab(self), P(data)});
}
__asm__(
    ".globl __ZN24InterfaceTitleTextObjectC1EPv\n"
    ".p2align 2\n"
    "__ZN24InterfaceTitleTextObjectC1EPv:\n"
    "    b _tpw_v5057_titlector\n"
    ".globl __ZN34InterfaceCursorPosTextObjectSpriteC1EPv\n"
    ".p2align 2\n"
    "__ZN34InterfaceCursorPosTextObjectSpriteC1EPv:\n"
    "    b _tpw_v5057_cursorctor\n");
extern "C" void tpw_v5057_masterctor(void* self) { ev("CFSGlobalMasterSave", {newlab(self)}); }
__asm__(
    ".globl __ZN19CFSGlobalMasterSaveC1Ev\n"
    ".p2align 2\n"
    "__ZN19CFSGlobalMasterSaveC1Ev:\n"
    "    b _tpw_v5057_masterctor\n");

extern "C" {

uint64_t tpw_v5057_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"bufA", gEditSignBufA},          {"bufB", gEditSignBufB},          {"conv", gUIMainConvertBuffer},
        {"panel", &gEditParkNamePanel},   {"screen", &gParkScreenWindow},   {"world", &gGameWorld},
        {"thing", gThingArray},           {"master", &gMasterSave},         {"guard", &gGameWinCheatGuard},
        {"cheatA", &gCheatSignA},         {"cheatB", &gCheatSignB},         {"script", gEditParkNameScript},
        {"vtbl", gVtable},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v5057_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Clears the tables of the previous case and makes the vtable.
void tpw_v5057_reset() {
    gChildCount = 0;
    gPanelBlock = nullptr;
    gSignA = gSignB = gStringBlock = nullptr;
    gNameBlock = nullptr;
    for (void*& slot : gVtable) slot = reinterpret_cast<void*>(&fakeSetText);
}
void tpw_v5057_child(int32_t parent, int32_t id, uint64_t window) {
    gChildren[gChildCount++] = {parent, id, reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window))};
}
// which: 0 the panel Build answers, 1 and 2 the texts GetSignNameA/B write, 3 the text GetString answers, 4 the one GetObjectName answers.
void tpw_v5057_block(int32_t which, uint64_t block) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(block));
    switch (which) {
        case 0: gPanelBlock = static_cast<InterfaceWindow*>(p); break;
        case 1: gSignA = static_cast<char16_t*>(p); break;
        case 2: gSignB = static_cast<char16_t*>(p); break;
        case 3: gStringBlock = static_cast<char16_t*>(p); break;
        default: gNameBlock = static_cast<char16_t*>(p); break;
    }
}

// mId, mZ, sizeof(InterfaceWindow), sizeof(CThingArrayEntry), offsetof(CWorld, mGatesThing)
int32_t tpw_v5057_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mId));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mGatesThing));
    return n;
}

void tpw_v5057_show() { ShowEditParkNamePanel(); }
int32_t tpw_v5057_two(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return TwoEditsMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
