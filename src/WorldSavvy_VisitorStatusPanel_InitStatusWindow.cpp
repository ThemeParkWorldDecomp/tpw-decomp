#include "WorldSavvy.h"

#include <new>

#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

// The real constructors by their symbols, weak so that the test library (which fakes both callees and does not link them) builds.
extern "C" void VspGradientCtorSym(void*, void*) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv") __attribute__((weak));

namespace vsp_callee {
__attribute__((weak)) void GradientCtor(void* mem, void* data) { VspGradientCtorSym(mem, data); }
// Stand-in: InterfaceVisitorTimeTextObject (0x1018edf4) is not declared anywhere; the sprite constructor stands for it.
__attribute__((weak)) void TimeCtor(void* mem) { lender_callee::SpriteCtor(mem); }
}  // namespace vsp_callee

namespace {

// operator new, then (when it answered) the constructor; as the Mac: the object may be NULL, it is given to the window anyway.
template <typename Ctor>
InterfaceDrawObject* Make(uint32_t size, Ctor ctor) {
    void* mem = lender_callee::OperatorNew(size);
    if (mem) ctor(mem);
    return static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(mem));
}

InterfaceWindow* Text(InterfaceWindow* parent, int32_t id, int32_t stringId) {
    InterfaceWindow* w = parent->GetWindow(id);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::SpriteCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    w->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(stringId)));
    return w;
}

}  // namespace

void VisitorStatusPanel::InitStatusWindow() {
    if (mWindow == nullptr) return;
    InterfaceWindow* root = mWindow;
    Text(root, 0x372c, 0x46);
    Text(root, 0x3725, 0x47);
    Text(root, 0x3726, 0x48);
    Text(root, 0x3727, 0x4b);
    Text(root, 0x3728, 0x49);
    Text(root, 0x3729, 0x4a);
    Text(root, 0x372a, 0x4c);
    Text(root, 0x372b, 0x4d);

    InterfaceWindow* w = root->GetWindow(0x3724);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::CashCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    lender_callee::WindowSetValue(w, 0x29a);

    w = root->GetWindow(0x372d);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { vsp_callee::TimeCtor(m); }));
    w->SetFont(GetFontTableEntry(6));

    static const int32_t kNumbers[] = {0x372e, 0x372f, 0x3730, 0x3731};
    for (int32_t id : kNumbers) {
        w = root->GetWindow(id);
        w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::NumberCtor(m); }));
        w->SetFont(GetFontTableEntry(6));
        lender_callee::WindowSetValue(w, 0);
    }

    w = root->GetWindow(0x3732);
    if (w) {
        UIMain_IndentClient(w, 0, 10);
        // 0x10247de3: "happygrad.wct"
        w->SetClientDrawObject(Make(0x124, [](void* m) { vsp_callee::GradientCtor(m, const_cast<char*>("happygrad.wct")); }));
    }

    w = root->GetWindow(0x3733);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::SpriteCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    w->SetClientDrawText(nullptr);
}
