// RideStatusPanel::Condemned (0x101bc2f0) and RideStatusPanel::InitStatusWindow (0x101b9340), main app.
#include <cstdint>
#include "History.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "RideDB.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "ppc_math.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "World.h"
#include "Debug.h"
#include "FillLenderList.h"

namespace {

// new, then the constructor (the Mac tests the answer of new for NULL, which never happens).
template <class F>
InterfaceDrawObject* Make(uint32_t size, F construct) {
    void* memory = lender_callee::OperatorNew(size);
    construct(memory);
    return static_cast<InterfaceDrawObject*>(memory);
}

InterfaceWindow* TextWindow(InterfaceWindow* root, int32_t id, int32_t string) {
    InterfaceWindow* w = root->GetWindow(id);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::SpriteCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    w->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(string)));
    return w;
}

void Gradient(InterfaceWindow* root, int32_t id) {
    InterfaceWindow* w = root->GetWindow(id);
    UIMain_IndentClient(w, 0, 10);
    // 0x1024e460: "ridestatbar.wct"
    w->SetClientDrawObject(Make(0x124, [](void* m) { vsp_callee::GradientCtor(m, const_cast<char*>("ridestatbar.wct")); }));
    lender_callee::WindowSetValue(w, 0);
}

}  // namespace

void RideStatusPanel::Condemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    if (mWindow == nullptr) return;
    // 0x1024e487
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    void* thing = gThingArray[id.mIndex].mThing;
    if (thing != nullptr && thing == static_cast<void*>(gStaffRoomPanelThing)) UIStatusPanel_CallSlot(this, 0x1c);
}

void RideStatusPanel::InitStatusWindow() {
    if (mStatusWindow == nullptr) return;
    InterfaceWindow* root = mStatusWindow;
    TextWindow(root, 0x3e20, 0x11);
    TextWindow(root, 0x3e1a, 0x12);
    TextWindow(root, 0x3e1c, 0x13);
    TextWindow(root, 0x3e1e, 0x14);
    TextWindow(root, 0x3e1f, 0x15);
    TextWindow(root, 0x3e1d, 0x16);
    TextWindow(root, 0x3e22, 0x17);

    InterfaceWindow* w = root->GetWindow(0x3e21);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::NumberCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    lender_callee::WindowSetValue(w, 0);

    w = root->GetWindow(0x3e1b);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::SpriteCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    w->SetClientDrawText(nullptr);

    Gradient(root, 0x3e16);
    Gradient(root, 0x3e18);
    Gradient(root, 0x3e19);
    Gradient(root, 0x3e17);

    w = root->GetWindow(0x3e23);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::CashCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    lender_callee::WindowSetValue(w, 0);
}
