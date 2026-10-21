// Test library for UIMenu::Init and UIMenu::AddItem (unit v5163; libtpw_unit_UIMenu_AddItem.dylib, tests/test_UIMenu_AddItem.py), main app:
//   src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/InterfaceWindow.cpp, src/game/ui/UIMenu.cpp, src/game/ui/UI_Base.cpp   UIMenu::Init, UIMenu::AddItem, the handler handles
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the weak
// menu_callee functions of the src file are replaced by the fakes here. The two screen size globals are defined here, zero at start.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "ui_menu.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
int16_t gUIRealScreenHeight;   // 0x10395bba
int16_t gUIRealScreenWidth;    // 0x10395bbc

namespace {

void* gNewWin = nullptr;   // what CreateObject answers when the case says so

int32_t Cb1(InterfaceWindow*, int32_t, int32_t, int32_t) { return 1; }
int32_t Cb2(InterfaceWindow*, int32_t, int32_t, int32_t) { return 2; }
int32_t Cb3(InterfaceWindow*, int32_t, int32_t, int32_t) { return 3; }
using Handler = int32_t (*)(InterfaceWindow*, int32_t, int32_t, int32_t);

Handler HandlerOf(int32_t index) {
    switch (index) {
        case 1: return Cb1;
        case 2: return Cb2;
        case 3: return Cb3;
        default: return nullptr;
    }
}

int64_t IndexOf(Handler handler) {
    if (handler == nullptr) return 0;
    if (handler == Cb1) return 1;
    if (handler == Cb2) return 2;
    if (handler == Cb3) return 3;
    return 9;
}

// A pointer-sized value that is a small number or an object: small numbers as they are, objects as labels.
int64_t Pv(const void* p) {
    uintptr_t v = reinterpret_cast<uintptr_t>(p);
    return v < 0x100000u ? static_cast<int64_t>(v) : static_cast<int64_t>(lab(const_cast<void*>(p)));
}

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub
void UI_MeasureText(const char16_t* text, const TbRect& rect, int8_t font, int8_t horizontal, int8_t vertical, int32_t wrap,
                    TbRect* out) {
    ev("MeasureText", {lab(const_cast<char16_t*>(text)), rect.left, rect.top, rect.right, rect.bottom, font, horizontal, vertical, wrap});
    out->left = static_cast<int32_t>(ret("mt0"));
    out->top = static_cast<int32_t>(ret("mt1"));
    out->right = static_cast<int32_t>(ret("mt2"));
    out->bottom = static_cast<int32_t>(ret("mt3"));
}

int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), subtract});
    rect->mLeft = static_cast<int16_t>(ret("gp0"));
    rect->mTop = static_cast<int16_t>(ret("gp1"));
    rect->mRight = static_cast<int16_t>(ret("gp2"));
    rect->mBottom = static_cast<int16_t>(ret("gp3"));
    return 0;
}

InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return ret("create") != 0 ? static_cast<InterfaceWindow*>(gNewWin) : nullptr;
}

void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallback", {lab(this), IndexOf(callback)});
}

int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, Pv(value)});
    return 0;
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), Pv(object)}); }

void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), Pv(object)}); }

void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), Pv(font)}); }

void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }

void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(ret("font")));
}

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {std::strcmp(name, "lolight") == 0 ? 1 : 0});
    return reinterpret_cast<InterfaceDrawObject*>(static_cast<uintptr_t>(ret("find")));
}

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {size});
    return tpw::ArenaAllocZeroed(size);
}

void WindowCtor(void* mem, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                int16_t bottom) {
    ev("WindowCtor", {newlab(mem), lab(parent), attributes, id, left, top, right, bottom});
}
}  // namespace lender_callee

namespace menu_callee {
void RectCtor(void* mem) {
    ev("RectCtor", {0});
    std::memset(mem, 0, sizeof(CoolRect));
}

void RectDtor(void*) { ev("RectDtor", {-1}); }

void TitleTextCtor(void* mem) { ev("TitleCtor", {newlab(mem), 0}); }

void* GetAndCopy(const char16_t* text) {
    ev("GetAndCopy", {lab(const_cast<char16_t*>(text))});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("copy")));
}

void WindowSetText(InterfaceWindow* window, char16_t* text) { ev("SetText", {lab(window), lab(text)}); }

int32_t Rand() {
    ev("rand", {0});
    return static_cast<int32_t>(ret("rand"));
}
}  // namespace menu_callee

// The src file's weak default TitleTextCtor still names the real constructor (its vtable lives elsewhere): a symbol of that name that is
// never called here, so the library links.
extern "C" void tpw_ua_unused_title_ctor(void*) __asm__("__ZN24InterfaceTitleTextObjectC1EPv");
extern "C" void tpw_ua_unused_title_ctor(void*) {}
extern "C" void tpw_ua_unused_rect_ctor(void*) __asm__("__ZN8CoolRectC1Ev");
extern "C" void tpw_ua_unused_rect_ctor(void*) {}
extern "C" void tpw_ua_unused_rect_dtor(void*) __asm__("__ZN8CoolRectD1Ev");
extern "C" void tpw_ua_unused_rect_dtor(void*) {}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_ua_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"width", &gUIRealScreenWidth},
        {"height", &gUIRealScreenHeight},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_ua_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_ua_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIMenu, mF00));
    out[n++] = static_cast<int64_t>(offsetof(UIMenu, mF04));
    out[n++] = static_cast<int64_t>(offsetof(UIMenu, mF08));
    out[n++] = static_cast<int64_t>(offsetof(UIMenu, mShown));
    out[n++] = static_cast<int64_t>(offsetof(UIMenu, mWindow));
    out[n++] = static_cast<int64_t>(sizeof(UIMenu));
    return n;
}

// The handle the host's word at 0 holds for the fake handler number 0 to 3, and the number of a handle.
uint32_t tpw_ua_handle(int32_t index) { return UIMenuHandlerHandle(HandlerOf(index)); }
int32_t tpw_ua_handle_index(uint32_t handle) { return static_cast<int32_t>(IndexOf(UIMenuHandlerFromHandle(handle))); }

static void Final(UIMenu* menu) {
    ev("Final", {IndexOf(UIMenuHandlerFromHandle(menu->mF00)), menu->mF04, lab(menu->mWindow)});
}

// ---- the functions, one wrapper each with C types only
void tpw_ua_init(uint64_t menu, int32_t handler, int32_t itemHandler) {
    UIMenu* m = reinterpret_cast<UIMenu*>(static_cast<uintptr_t>(menu));
    m->Init(HandlerOf(handler), HandlerOf(itemHandler));
    Final(m);
}

uint32_t tpw_ua_additem(uint64_t menu, uint64_t newwin, uint64_t text, int32_t position, int32_t id, int32_t a, int32_t b, int32_t c,
                        int32_t d) {
    UIMenu* m = reinterpret_cast<UIMenu*>(static_cast<uintptr_t>(menu));
    gNewWin = reinterpret_cast<void*>(static_cast<uintptr_t>(newwin));
    uint32_t answer = static_cast<uint32_t>(m->AddItem(position, reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(text)), id, a, b, c, d));
    Final(m);
    return answer;
}

}  // extern "C"
