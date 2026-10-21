// ui_staffRoomStatus.cpp: name proven by __sinit_ui_staffRoomStatus_cpp in the main application.

#include "analyser.h"
#include "ESP_sprites.h"
#include "FillLenderList.h"
#include "helpers/ui_staffRoomStatusHelpers.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "WindowScripts.inc"
#include <cstdint>
#include <new>

// The staff, show and upgrade status panels' view-window functions and the small free functions around them (unit v51051), main app.

// ---- StaffRoomStatusPanel
void StaffRoomStatusPanel::InitViewWindow() {   // 0x101c2d60
    mScript = 0;
    mF28 = -1;
    mF2c = -1;
}

// Unit v5291: the staff room panel's member functions (StaffWentIntoStaffRoom, UpdateWindow, StaffLeftStaffRoom, Show,
// UpdateStatusWindow, DeInitViewWindow).

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

void StaffRoomStatusPanel::UpdateViewWindow() {}     // 0x101c2e44
void StaffRoomStatusPanel::DeInitStatusWindow() {}   // 0x101c3010

// 0x101c3054: the residual value of the panel's thing goes to the control 0x1e8a.
void StaffRoomStatusPanel::UpdateStatusWindow() {
    StaffRoomPanelFields* f = Fields(this);
    uint32_t value = reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GetCurrentResidualValue();
    InterfaceWindow* control = f->mStatusWindow.get()->GetWindow(0x1e8a);
    control->SetValue(static_cast<int32_t>(value));
}

// Unit v5669: the staff room panel's list fill and window set-up (ui_staffRoomStatus.cpp, main app):
//   FillStaffRoomList 0x101c31d4, InitStatusWindow 0x101c2e88, InitWindow 0x101c36f0, UpdateViewSprite 0x101c2b88.

int32_t gStaffRoomColumnColour = 8;               // 0x102aed64 (name invented)
InterfaceListControl2* gStaffRoomList = nullptr;  // 0x10395a94 (name invented)
uint32_t gStaffRoomSpawnBlock[1] = {0x102669d8};  // 0x1029ff70 (name invented; data: only its address is used here)

namespace {

StaffRoomPanelFields* PanelFields(StaffRoomStatusPanel* panel) { return reinterpret_cast<StaffRoomPanelFields*>(panel); }

int32_t WordAt(const void* base, int32_t offset) {
    return *reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(base) + offset);
}

}  // namespace

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

// ui_staffRoomStatus.cpp / ui_showStatus.cpp / ui_staffStatus.cpp pieces (main app, UI):
//   0x101c2b10 __sinit_ui_showStatus_cpp, 0x101c30fc StaffRoomListFiller, 0x101c3360 StaffRoomLCFormat (the file's LCFormat),
//   0x101c40f0 __sinit_ui_staffRoomStatus_cpp, 0x101c4ec0 StaffStatusViewWindowCallback, 0x101c5878 StaffStatusPanel::ShowEditNamePanel.

int32_t gStaffRoomListFormat;                          // 0x10395a8c
int32_t gStaffRoomValueA = 0;                          // 0x10395acc
int32_t gStaffRoomValueB = 0;                          // 0x10395ad0
uint32_t* gStaffRoomPanelTable;                        // 0x102aed88 (a block made by the static initialiser: a Ptr32 field cannot hold the data address)
// The callees the host cannot call as the Mac does (new plus constructor, the virtuals through __ptr_glue): weak, faked by the test library.
namespace staffroom_callee {
__attribute__((weak)) InterfaceWindow* NewRowWindow(int16_t bottom) {
    return new InterfaceWindow(nullptr, 0, 0, 0, 0, 100, bottom);   // 0x138 bytes of the game heap on the Mac; the host window is larger (arena)
}
__attribute__((weak)) int16_t FontHeight(InterfaceFont* font) { return static_cast<int16_t>(font->GetHeight()); }
}  // namespace staffroom_callee

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

// Unit v5004: the staff room panel's list control formatter and message functions (main app, ui_staffRoomStatus.cpp / ui_onlineParkListPanel.cpp):
//   LCPostFormat 0x101c346c, StaffRoomPanelLCHandler 0x101c35a4, StaffRoomStatusMsg 0x101c3c68.

// 0x101c346c: gives the list control a text sprite as client draw object with the table font, then its first child (indented) a horizontal
// gradient ("ridestatbar.wct") and a SetValue(0) (the virtual at the table's +0x24).
void LCPostFormat(InterfaceWindow* window) {
    InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    window->SetFont(GetFontTableEntry(gLCFontIndex));
    window->SetClientDrawText(nullptr);
    InterfaceWindow* child = window->GetChild(0);
    UIMain_IndentClient(child, 0, 10);
    InterfaceDrawEngineHGradient* gradient = new InterfaceDrawEngineHGradient(const_cast<char*>("ridestatbar.wct"));
    child->SetClientDrawObject(gradient);
    child->SetValue(0);
}

// 0x101c35a4: message 0x101 with b == 1 fills the staff room list for the type the control number (a) names.
int32_t StaffRoomPanelLCHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    StaffRoomStatusPanel* panel = &gStaffRoomStatusPanel;
    if (message == 0x101 && b == 1) {
        switch (a) {
            case 0x1e7d: panel->FillStaffRoomList(static_cast<StaffType>(0), 1); break;
            case 0x1e7e: panel->FillStaffRoomList(static_cast<StaffType>(4), 1); break;
            case 0x1e7f: panel->FillStaffRoomList(static_cast<StaffType>(2), 1); break;
            case 0x1e80: panel->FillStaffRoomList(static_cast<StaffType>(1), 1); break;
            case 0x1e81: panel->FillStaffRoomList(static_cast<StaffType>(3), 1); break;
            default: break;
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
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

void StaffRoomStatusPanel::DeInitWindow() {}         // 0x101c3978

// 0x101c39b8: slot 0x3c's text goes to the text window, then slots 0x20, the list refill and slot 0x1c.
void StaffRoomStatusPanel::UpdateWindow() {
    StaffRoomPanelFields* f = Fields(this);
    uint64_t text = CallSlot(f, 0x3c, this);
    f->mTextWindow.get()->SetText(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text)));
    CallSlot(f, 0x20, this);
    FillStaffRoomList(gStaffRoomStaffType, 1);
    CallSlot(f, 0x1c, this);
}

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

// 0x101c3bbc: takes the staff member out of the list when it is there.
void StaffRoomStatusPanel::StaffLeftStaffRoom(CThing* thing) {
    StaffRoomPanelFields* f = Fields(this);
    if (f->mWindow.get() == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(f->mWindow.get()->GetWindow(0x1e7b));
    int32_t position = list->FindData(DataOf(thing));
    if (position >= 0) list->RemoveItem(position);
}

// 0x101c3c68: the staff room status panel's message function.
int32_t StaffRoomStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    StaffRoomStatusPanel* panel = &gStaffRoomStatusPanel;
    switch (message) {
        case 5:
            panel->Remove();
            return 1;
        case 0x10:
            if (a == 0x80080) {
                // Rewrites column 1 of every row: the staff member's float at +0x1f8, cut to a byte times 1024 over 100.
                InterfaceListControl2* list = static_cast<InterfaceListControl2*>(panel->mWindow->GetWindow(0x1e7b));
                for (int32_t i = list->mItemCount - 1; i >= 0; i--) {
                    int32_t data = 0;
                    list->GetData(i, &data);
                    // The data word is a 32-bit pointer (an offset in the arena, Ptr32) to the object whose float sits at +0x1f8.
                    Ptr32<uint8_t> object;
                    object.mOffset = static_cast<uint32_t>(data);
                    float value = *reinterpret_cast<float*>(object.get() + 0x1f8);
                    int32_t whole = static_cast<int32_t>(value);
                    UI_LCAdd item[2];
                    item[0].mRaw = 0;   // the original leaves this word as it was on the stack; column 0 is not rewritten
                    item[1].mRaw = 0;
                    item[1].mInt = static_cast<int32_t>((static_cast<uint32_t>(whole) & 0xffu) << 10) / 100;
                    list->ChangeItem(i, item, 2, i == 0, -1);
                }
            }
            break;
        case 0x14:
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(300));
            break;
        case 0x15:
            if (window->setAnimTimer(0x80080, 2000) != 0) window->startAnimTimer(0x80080);
            break;
        case 0x100:
            switch (a) {
                case -1:
                    panel->Remove();
                    return 0;
                case 0x1e7a:
                    panel->EraseObject();
                    return 0;
                case 0x1e83:
                    CreateAllStaffPanel(gStaffRoomStaffType, nullptr);
                    return 0;
                case 0x1e85:
                    panel->GetPreviousObject();
                    break;
                case 0x1e86:
                    panel->GetNextObject();
                    break;
                case 0x1e87:
                    panel->Move();
                    return 0;
                case 0x1e88:
                    CreateAllItemsPanel(static_cast<UIObjType>(3), gStaffRoomPanelThing);
                    return 0;
                default:
                    break;
            }
            break;
        case 0x400: {
            InterfaceWindow* found = window->GetWindow(a);
            if (found != nullptr && b != -1) {
                int32_t data = 0;
                static_cast<InterfaceListControl2*>(found)->GetData(b, &data);
                Ptr32<CThing> thing;
                thing.mOffset = static_cast<uint32_t>(data);
                gStaffStatusPanel.Show(thing.get());
                return 0;
            }
            return 0;
        }
        case 0x401: {
            InterfaceWindow* found = window->GetWindow(a);
            if (found != nullptr) {
                if (b == -1) {
                    if (panel->mScript != 0) {
                        SCR_KillScript(panel->mScript);
                        panel->mScript = 0;
                        panel->mF28 = -1;
                        panel->mF2c = -1;
                    }
                } else {
                    int32_t data = 0;
                    static_cast<InterfaceListControl2*>(found)->GetData(b, &data);
                    Ptr32<CStaff> staff;
                    staff.mOffset = static_cast<uint32_t>(data);
                    panel->UpdateViewSprite(staff.get());
                }
            }
            break;
        }
        case 0x406:
            gStaffRoomMsg406 = b;
            break;
        default:
            break;
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}

// 0x101c4014
void StaffRoomStatusPanel::Show(CThing* thing) {
    gStaffRoomShowThing = thing;
    OpenWindow(gStaffRoomScript, StaffRoomStatusMsg, 0x1e84, 0x1e89, 0);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x12c));
}

void StaffRoomStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }   // 0x101c4098

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
