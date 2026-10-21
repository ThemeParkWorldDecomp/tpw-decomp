// Unit v5291: the staff room panel's member functions (StaffWentIntoStaffRoom, UpdateWindow, StaffLeftStaffRoom, Show,
// UpdateStatusWindow, DeInitViewWindow).
#include <cstdint>

#include "InterfaceWindow.h"
#include "Ptr32.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "ppc_math.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "WindowScripts.inc"

// ---- globals nobody else defines
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> gStaffRoomId;   // 0x10395a90
CThing* gStaffRoomShowThing = nullptr;                           // 0x10395a88
int16_t gStaffRoomScript[] = {TPW_WINDOW_SCRIPT_102AE9C8};   // 0x102ae9c8: the image's words (generated, devtools/gen_game_tables.py)

namespace {

typedef uint64_t (*PanelSlot)(void*);

StaffRoomPanelFields* Fields(StaffRoomStatusPanel* panel) { return reinterpret_cast<StaffRoomPanelFields*>(panel); }

// The Mac calls the TVector in the panel's table at the byte offset, with the panel; answers r3.
uint64_t CallSlot(StaffRoomPanelFields* f, uint32_t byteOffset, void* panel) {
    Ptr32<PanelSlot> block;
    block.mOffset = f->mTable.get()[byteOffset / 4];
    return (*block.get())(panel);
}

int32_t DataOf(const CThing* thing) { return thing != nullptr ? static_cast<int32_t>(tpw::ArenaOffset(thing)) : 0; }

}  // namespace

// 0x101c3a80: adds the staff member to the list when it rests in the staff room and is of the listed type.
void StaffRoomStatusPanel::StaffWentIntoStaffRoom(CThing* thing) {
    StaffRoomPanelFields* f = Fields(this);
    if (f->mWindow.get() == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(f->mWindow.get()->GetWindow(0x1e7b));
    CStaff* staff = reinterpret_cast<CStaff*>(thing);
    if (!staff->IsRestingInStaffRoom(gStaffRoomId)) return;
    if (gStaffRoomStaffType != staff->GetStaffType()) return;
    UI_LCAdd item[2];
    item[0].mRaw = 0;
    item[0].mText = staff->GetName();
    float value = *reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(thing) + 0x1f8);
    uint32_t low = (static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(value))) & 0xffu) << 10;
    item[1].mRaw = 0;
    item[1].mInt = static_cast<int32_t>(low) / 100;
    list->AddItem(item, DataOf(thing), -1, 1);
}

// 0x101c39b8: slot 0x3c's text goes to the text window, then slots 0x20, the list refill and slot 0x1c.
void StaffRoomStatusPanel::UpdateWindow() {
    StaffRoomPanelFields* f = Fields(this);
    uint64_t text = CallSlot(f, 0x3c, this);
    f->mTextWindow.get()->SetText(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text)));
    CallSlot(f, 0x20, this);
    FillStaffRoomList(gStaffRoomStaffType, 1);
    CallSlot(f, 0x1c, this);
}

// 0x101c3bbc: takes the staff member out of the list when it is there.
void StaffRoomStatusPanel::StaffLeftStaffRoom(CThing* thing) {
    StaffRoomPanelFields* f = Fields(this);
    if (f->mWindow.get() == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(f->mWindow.get()->GetWindow(0x1e7b));
    int32_t position = list->FindData(DataOf(thing));
    if (position >= 0) list->RemoveItem(position);
}

// 0x101c4014
void StaffRoomStatusPanel::Show(CThing* thing) {
    gStaffRoomShowThing = thing;
    OpenWindow(gStaffRoomScript, StaffRoomStatusMsg, 0x1e84, 0x1e89, 0);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x12c));
}

// 0x101c3054: the residual value of the panel's thing goes to the control 0x1e8a.
void StaffRoomStatusPanel::UpdateStatusWindow() {
    StaffRoomPanelFields* f = Fields(this);
    uint32_t value = reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GetCurrentResidualValue();
    InterfaceWindow* control = f->mStatusWindow.get()->GetWindow(0x1e8a);
    control->SetValue(static_cast<int32_t>(value));
}

// 0x101c2db4: kills the staff view script.
void StaffRoomStatusPanel::DeInitViewWindow() {
    StaffRoomPanelFields* f = Fields(this);
    if (f->mScript != 0) {
        SCR_KillScript(f->mScript);
        f->mScript = 0;
        f->mF28 = -1;
        f->mF2c = -1;
    }
}
