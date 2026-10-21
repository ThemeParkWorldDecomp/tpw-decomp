// ui_staffRoomStatus.cpp / ui_showStatus.cpp / ui_staffStatus.cpp pieces (main app, UI):
//   0x101c2b10 __sinit_ui_showStatus_cpp, 0x101c30fc StaffRoomListFiller, 0x101c3360 StaffRoomLCFormat (the file's LCFormat),
//   0x101c40f0 __sinit_ui_staffRoomStatus_cpp, 0x101c4ec0 StaffStatusViewWindowCallback, 0x101c5878 StaffStatusPanel::ShowEditNamePanel.
#include <cstdint>
#include <new>
#include "platform/arena.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "Ptr32.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "ppc_math.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "WindowScripts.inc"

int32_t gStaffRoomListFormat;                          // 0x10395a8c
int32_t gStaffRoomValueA = 0;                          // 0x10395acc
int32_t gStaffRoomValueB = 0;                          // 0x10395ad0
uint32_t* gStaffRoomPanelTable;                        // 0x102aed88 (a block made by the static initialiser: a Ptr32 field cannot hold the data address)
uint32_t* gShowStatusTable;                            // 0x102ae980 (the same)

static uint32_t* MakeTable(uint32_t*& table, uint32_t* (*make)()) {
    if (table == nullptr) table = make();
    return table;
}
int16_t gStaffEditNameScript[] = {TPW_WINDOW_SCRIPT_102AF16C};   // 0x102af16c: the image's words (generated, devtools/gen_game_tables.py)

// The callees the host cannot call as the Mac does (new plus constructor, the virtuals through __ptr_glue): weak, faked by the test library.
namespace staffroom_callee {
__attribute__((weak)) InterfaceWindow* NewRowWindow(int16_t bottom) {
    return new InterfaceWindow(nullptr, 0, 0, 0, 0, 100, bottom);   // 0x138 bytes of the game heap on the Mac; the host window is larger (arena)
}
__attribute__((weak)) int16_t FontHeight(InterfaceFont* font) { return static_cast<int16_t>(font->GetHeight()); }
__attribute__((weak)) InterfaceDrawObject* NewTitleText() { return new InterfaceTitleTextObject(nullptr); }
__attribute__((weak)) InterfaceDrawObject* NewCursorText() { return new InterfaceCursorPosTextObjectSprite(nullptr); }
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, const char16_t* text) { window->SetText(const_cast<char16_t*>(text)); }
}  // namespace staffroom_callee

// 0x101c2b10
void __sinit_ui_showStatus_cpp() {
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(&gShowStatusPanel));
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(&gShowStatusPanel);
    fields->mVtable.set(MakeTable(gShowStatusTable, ShowStatusPanelTable));
    fields->mMode = 2;
}

// 0x101c40f0
void __sinit_ui_staffRoomStatus_cpp() {
    gStaffRoomValueB = 0;
    gStaffRoomValueA = 0x10000;
    UIStatusPanel_Construct(&gStaffRoomStatusPanel);
    StaffRoomPanelFields* fields = reinterpret_cast<StaffRoomPanelFields*>(&gStaffRoomStatusPanel);
    fields->mTable.set(MakeTable(gStaffRoomPanelTable, StaffRoomStatusPanelTable));
    fields->mF00 = 0x3eb;
    fields->mScript = 0;
    fields->mF2c = -1;
    fields->mF28 = -1;
}

// 0x101c30fc: adds a resting staff member (name, energy as a percentage of 100 in 1/1024 steps) to gStaffRoomList
int32_t StaffRoomListFiller(CThing* thing) {
    CStaff* staff = reinterpret_cast<CStaff*>(thing);
    if (staff->IsRestingInStaffRoom(gStaffRoomId)) {
        UI_LCAdd item[2] = {};
        item[0].mText = staff->GetName();
        int32_t energy = static_cast<int32_t>(*reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(thing) + 0x1f8));
        item[1].mInt = static_cast<int32_t>((static_cast<uint32_t>(energy) & 0xff) << 10) / 100;
        gStaffRoomList->AddItem(item, static_cast<int32_t>(reinterpret_cast<intptr_t>(thing)), -1, 0);
    }
    return 1;
}

// 0x101c3360: the list's row maker
InterfaceWindow* StaffRoomLCFormat() {
    if (gStaffRoomListFormat == 0) {
        InterfaceFont* font = GetFontTableEntry(gLCFontIndex);
        int32_t height = staffroom_callee::FontHeight(font);
        gStaffRoomListFormat = height * 0x600 / static_cast<int32_t>(gUIRealScreenHeight) + 6;
    }
    InterfaceWindow* window = staffroom_callee::NewRowWindow(static_cast<int16_t>(gStaffRoomListFormat));
    if (window != nullptr) window->CreateObject(9, 0x11, 1, 100, 0, 200, 0x32);
    return window;
}

// 0x101c4ec0
int32_t StaffStatusViewWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) reinterpret_cast<UIStatusPanel*>(&gStaffStatusPanel)->ZoomTo();
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101c5878
void StaffStatusPanel::ShowEditNamePanel() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    Ptr32<InterfaceWindow>& windowField = *reinterpret_cast<Ptr32<InterfaceWindow>*>(self + 8);
    Ptr32<InterfaceWindow>& parentField = *reinterpret_cast<Ptr32<InterfaceWindow>*>(self + 0xc);
    if (windowField.mOffset != 0) return;
    CThing* thing = gStaffRoomPanelThing;
    if (thing == nullptr) return;
    uint8_t type = reinterpret_cast<uint8_t*>(thing)[2];
    if (!(type == 4 || type == 5 || type == 6 || type == 8 || type == 7)) return;
    CStaff* staff = reinterpret_cast<CStaff*>(thing);

    InterfaceWindow* parent = parentField.get();
    int16_t z = parent->mZ;   // the Mac's +0xdc, by name: the host window is laid out differently
    InterfaceWindow* window = parent->Build(gStaffEditNameScript, OneEditMsg);
    windowField.set(window);
    window->setZ(static_cast<int16_t>(z + 10));
    window->setExtraParam(0, thing);
    window->setExtraParam(1, this);

    InterfaceDrawObject* titleObject = staffroom_callee::NewTitleText();
    window->GetWindow(0x5daa)->SetClientDrawObject(titleObject);
    InterfaceFont* titleFont = GetFontTableEntry(5);
    window->GetWindow(0x5daa)->SetFont(titleFont);
    window->GetWindow(0x5daa)->SetFontJust(1, 1);
    const char16_t* title = UIMain_GetString(0xc3);
    staffroom_callee::WindowSetText(window->GetWindow(0x5daa), title);
    UI_TITLE_COLOUR(window->GetWindow(0x5daa));

    InterfaceEditBox* edit = static_cast<InterfaceEditBox*>(window->GetWindow(0x5dab));
    InterfaceDrawObject* cursor = staffroom_callee::NewCursorText();
    edit->SetClientDrawObject(cursor);
    edit->SetFont(GetFontTableEntry(7));
    edit->SetFontColour(0xff, 0xff, 0xff, 0xff);
    edit->SetStaticTextArea(gStaffEditName, 0x20);
    edit->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    staffroom_callee::WindowSetText(edit, staff->GetName());
    edit->SelectAll();
    UIMain_DisableAllKeyTables();
    edit->CaptureKb(0);
}
