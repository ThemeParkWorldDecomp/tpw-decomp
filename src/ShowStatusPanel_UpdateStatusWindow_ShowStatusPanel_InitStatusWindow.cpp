#include <algorithm>
#include <cstdint>
#include <new>
#include "InterfaceWindow.h"
#include "RideDB.h"
#include "Text.h"
#include "TbITable.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"
#include "MainWindowCallback.h"

// 0x1024e668: "ridestatbar.wct", then at +0x10 "happygrad.wct".
const char gStatusMeshNames[] = "ridestatbar.wct\0happygrad.wct";

// The real versions (the host cannot call the Mac's new plus constructor); the test library has strong fakes.
static_assert(sizeof(InterfaceTextObjectSprite) <= 0x400 && sizeof(InterfaceDrawEngineHGradient) <= 0x400, "the allocation below is 0x400 at least");

namespace statusinit_callee {
// The Mac allocates the sizes of its objects; the host's are larger (InterfaceTextObjectSprite 0x250 bytes for the Mac's 0x19c), so the block is
// at least 0x400 bytes.
__attribute__((weak)) void* OperatorNew(uint32_t size) { return ::operator new(std::max<size_t>(size, 0x400)); }
__attribute__((weak)) void SpriteCtor(void* mem) { new (mem) InterfaceTextObjectSprite(nullptr); }
__attribute__((weak)) void CashCtor(void* mem) { new (mem) InterfaceCashTextObject(nullptr); }
__attribute__((weak)) void GradientCtor(void* mem, const char* meshName) {
    new (mem) InterfaceDrawEngineHGradient(const_cast<char*>(meshName));
}
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
}  // namespace statusinit_callee

namespace {

using namespace statusinit_callee;

// A window id's text draw object, the font 6 and the string (0: no text is given, the text is NULL).
InterfaceWindow* TextChild(InterfaceWindow* parent, int32_t id, int32_t stringId, bool hasText) {
    InterfaceWindow* window = parent->GetWindow(id);
    void* object = OperatorNew(0x19c);
    if (object != nullptr) SpriteCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(hasText ? const_cast<char16_t*>(UIMain_GetString(stringId)) : nullptr);
    return window;
}

void CashChild(InterfaceWindow* parent, int32_t id) {
    InterfaceWindow* window = parent->GetWindow(id);
    void* object = OperatorNew(0x19c);
    if (object != nullptr) CashCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    window->SetFont(GetFontTableEntry(6));
    WindowSetValue(window, 0);
}

void GradientChild(InterfaceWindow* parent, int32_t id, const char* mesh, int32_t value) {
    InterfaceWindow* window = parent->GetWindow(id);
    UIMain_IndentClient(window, 0, 10);
    void* object = OperatorNew(0x124);
    if (object != nullptr) GradientCtor(object, mesh);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    WindowSetValue(window, value);
}

}  // namespace

// 0x101c0f80 (2128 bytes)
void ShowStatusPanel::InitStatusWindow() {
    const char* mesh = gStatusMeshNames;
    InterfaceWindow* parent = reinterpret_cast<UIStatusPanelFields*>(this)->mF10.get();
    if (parent == nullptr) return;
    TextChild(parent, 0xa09c, 0x29, true);
    TextChild(parent, 0xa095, 0x2a, true);
    TextChild(parent, 0xa09a, 0x2b, true);
    TextChild(parent, 0xa098, 0x2c, true);
    TextChild(parent, 0xa09b, 0x2d, true);
    TextChild(parent, 0xa097, 0x2e, true);
    TextChild(parent, 0xa0a1, 0x2f, true);
    TextChild(parent, 0xa09f, 0x30, true);
    TextChild(parent, 0xa09d, 0, false);
    GradientChild(parent, 0xa096, mesh, 0x32);
    CashChild(parent, 0xa093);
    CashChild(parent, 0xa092);
    GradientChild(parent, 0xa099, mesh, 100);
    GradientChild(parent, 0xa094, mesh, 0x32);
    CashChild(parent, 0xa0a0);
    GradientChild(parent, 0xa09e, mesh + 0x10, 100);
}
