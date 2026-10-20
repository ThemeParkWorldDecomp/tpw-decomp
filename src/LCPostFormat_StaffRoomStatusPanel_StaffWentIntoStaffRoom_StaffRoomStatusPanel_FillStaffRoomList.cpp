// Unit v5669: the staff room panel's list fill and window set-up (ui_staffRoomStatus.cpp, main app):
//   FillStaffRoomList 0x101c31d4, InitStatusWindow 0x101c2e88, InitWindow 0x101c36f0, UpdateViewSprite 0x101c2b88.
#include <cstdint>
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ESP_sprites.h"
#include "Ptr32.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "analyser.h"
#include "ppc_math.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_iterators.h"
#include "ui_main.h"

int32_t gStaffRoomColumnColour = 8;               // 0x102aed64 (name invented)
InterfaceListControl2* gStaffRoomList = nullptr;  // 0x10395a94 (name invented)
uint32_t gStaffRoomSpawnBlock[1] = {0x102669d8};  // 0x1029ff70 (name invented; data: only its address is used here)

// Untranslated neighbours, weak stand-ins so the program links (the test library defines the real fakes).
__attribute__((weak)) int32_t StaffRoomListFiller(CThing*) { return 0; }       // 0x101c30fc
__attribute__((weak)) InterfaceWindow* StaffRoomLCFormat() { return nullptr; }   // 0x101c3360

namespace {

StaffRoomPanelFields* PanelFields(StaffRoomStatusPanel* panel) { return reinterpret_cast<StaffRoomPanelFields*>(panel); }

int32_t WordAt(const void* base, int32_t offset) {
    return *reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(base) + offset);
}

}  // namespace

// 0x101c31d4: lists the staff of the type (the filler adds each into the list at gStaffRoomList), then the panel's virtual at +0x28 of the
// table at +0x20 runs. A type outside 0 to 4 lists nothing.
void StaffRoomStatusPanel::FillStaffRoomList(StaffType type, int32_t flag) {
    StaffRoomPanelFields* f = PanelFields(this);
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(f->mWindow.get()->GetWindow(0x1e7b));
    if (flag != 0) list->RemoveAllItems();
    gStaffRoomList = list;
    gStaffRoomId = gStaffRoomPanelThing->GetThingId();
    gStaffRoomStaffType = type;
    int32_t t = static_cast<int32_t>(type);
    if (t == 2) {
        UI_IterateThingList(kUIIterEntertainers, StaffRoomListFiller);
    } else if (t < 2) {
        if (t == 0) {
            UI_IterateThingList(kUIIterHandymen, StaffRoomListFiller);
        } else if (t >= 0) {
            UI_IterateThingList(kUIIterMechanics, StaffRoomListFiller);
        }
    } else if (t == 4) {
        UI_IterateThingList(kUIIterResearchers, StaffRoomListFiller);
    } else if (t < 4) {
        UI_IterateThingList(kUIIterGuards, StaffRoomListFiller);
    }
    list->UpdateDisplay();
    uint32_t slot = f->mTable.get()[0x28 / 4];
    uint64_t (*fn)(void*) = *reinterpret_cast<uint64_t (**)(void*)>(tpw::gArenaBase + slot);
    fn(this);
}

// 0x101c2e88: the two figures of the status window get their draw objects and the font 6; the first a text, the second a cash value (0).
void StaffRoomStatusPanel::InitStatusWindow() {
    StaffRoomPanelFields* f = PanelFields(this);
    InterfaceWindow* window = f->mStatusWindow.get()->GetWindow(0x1e8b);
    void* object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::SpriteCtor(object);
    window->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(object));
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x38)));
    window = f->mStatusWindow.get()->GetWindow(0x1e8a);
    object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::CashCtor(object);
    window->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(object));
    window->SetFont(GetFontTableEntry(6));
    lender_callee::WindowSetValue(window, 0);
}

// 0x101c36f0: fills the object list, hides the two arrows when fewer than 2 objects, sets the list up (columns, selection bar, handler,
// sort, column buttons), fills it, checks the button of the staff type and starts the edit box.
void StaffRoomStatusPanel::InitWindow() {
    StaffRoomPanelFields* f = PanelFields(this);
    int32_t count = FillObjectList(gStaffRoomShowThing);
    if (count < 2) {
        f->mWindow.get()->GetWindow(0x1e85)->Show(0);
        f->mWindow.get()->GetWindow(0x1e86)->Show(0);
    }
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(f->mWindow.get()->GetWindow(0x1e7b));
    list->SetColumnFormatsAll(StaffRoomLCFormat, LCPostFormat);
    list->SetColumnType(0, kLCText);
    list->SetColumnType(1, kLCNumber);
    list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    list->SetCallbackFunction(StaffRoomPanelLCHandler);
    list->SetSort(gStaffRoomMsg406, false);
    InterfaceWindow* button = list->GetWindow(0x10);
    MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x39)), gStaffRoomColumnColour, 0xff, 0xef, 0, 0x43);
    button = list->GetWindow(0x11);
    MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x3a)), gStaffRoomColumnColour, 0xff, 0xef, 0, 0x44);
    FillStaffRoomList(gStaffRoomStaffType, 0);
    InterfaceCheckButtons* checks = static_cast<InterfaceCheckButtons*>(list->GetWindow(0x1e7c));
    int32_t t = static_cast<int32_t>(gStaffRoomStaffType);
    if (t == 2) {
        checks->SetCurrent(0x1e7f);
    } else if (t < 2) {
        if (t == 0) {
            checks->SetCurrent(0x1e7d);
        } else if (t >= 0) {
            checks->SetCurrent(0x1e80);
        }
    } else if (t == 4) {
        checks->SetCurrent(0x1e7e);
    } else if (t < 4) {
        checks->SetCurrent(0x1e81);
    }
    InitEditBox(0x1e82);
}

// 0x101c2b88: puts the staff view script at the middle of the view window (x and y from the client rectangle, z from the window's Z);
// when the staff's two words (+0x24, +0x20) differ from the remembered ones the old script is killed and a new one spawned.
void StaffRoomStatusPanel::UpdateViewSprite(CStaff* staff) {
    StaffRoomPanelFields* f = PanelFields(this);
    CoolRect rect;
    InterfaceWindow* view = f->mWindow.get()->GetWindow(0x1e84);
    view->GetClientRect(&rect);
    int32_t left = rect.mLeft, top = rect.mTop, right = rect.mRight, bottom = rect.mBottom;
    int32_t x = left + ((right - left) >> 1) - 0x3f8;
    int32_t y = 0x300 - (bottom - ((bottom - top) >> 2));
    float fy = static_cast<float>(y) / 768.0f;
    float fx = static_cast<float>(x) * 0.0009765625f;
    float fz = CalcZTrans(static_cast<int32_t>(view->mZ) + 1);
    int32_t sprite = WordAt(staff, 0x24);
    int32_t a = WordAt(staff, 0x20);
    if (sprite != f->mF28 || a != f->mF2c) {
        f->mF28 = sprite;
        f->mF2c = a;
        if (f->mScript != 0) SCR_KillScript(f->mScript);
        f->mScript = ESP_SpawnScript(gStaffRoomSpawnBlock, static_cast<ESP_Sprites>(sprite), a, static_cast<ACTS>(1), fx, fy, fz);
        SCR_SetFloatVariable(f->mScript, 8, 8.0f);
    }
}
