// ui_StatusPanel.cpp: name proven by __sinit_ui_StatusPanel_cpp in the main application.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "LoadPanelMsg.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "online.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "UI_pump.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <new>

// Globals no header names (the Mac's addresses in the comments; names invented).
uint32_t gStatusPanelWord47a8;   // 0x103947a8, set to 0x10000 by __sinit_ui_StatusPanel_cpp
uint32_t gStatusPanelWord47ac;   // 0x103947ac, cleared by it
// 0x10192a3c (24 bytes): counts the thing. Answers 1.
int32_t StatusPanelIteratorCallback(CThing* thing) {
    (void)thing;
    gStatusPanelListCount = gStatusPanelListCount + 1;
    return 1;
}

// ui_StatusPanel.cpp (main app), unit v5090: GetNextObject 0x10192c28, FillObjectList 0x10192a90, EraseObject 0x10193138,
// OpenWindow 0x101935a0, Move 0x101938d8 of UIStatusPanel.

int32_t gStatusPanelListCount;       // 0x103947a0
uint8_t gStatusPanelClickStackDir;   // 0x102e47f0
CObject* gStatusPanelMoveObject;     // 0x102e47f4

namespace {

// The list the panel's mode shows, 0 for a mode that shows none (the original then calls nothing).
int32_t IteratorTypeForMode(int32_t mode) {
    switch (mode) {
        case -2: return 0x40;
        case -1: return 0x1;
        case 0: return 0x80;
        case 1: return 0x200;
        case 2: return 0x100;
        case 3: return 0x800;
        case 0x3ea: return 0x1000;
        case 0x3eb: return 0x8000;
        case 0x3ec: return 0x4000;
        default: return 0;
    }
}

UIStatusPanelFields* Fields(UIStatusPanel* panel) { return reinterpret_cast<UIStatusPanelFields*>(panel); }

// The panel's virtual function at the Mac's table offset `offset` (see UIStatusPanelFields).
void CallSlot_3(UIStatusPanel* panel, uint32_t offset) {
    const uint32_t* table = Fields(panel)->mVtable.get();
    const Ptr32<void (*)(UIStatusPanel*)> entry = {table[offset / 4]};
    (*entry.get())(panel);
}

uint16_t CellId(const CThing* thing) {
    return static_cast<uint16_t>(thing->mCellX + (thing->mCellY << 7) + 1);
}

// The map cell click that follows the object's selection: the cell's x and y from its id (the id is not checked for 0).
void ClickCell(uint16_t id) {
    const int32_t index = static_cast<int32_t>(id) - 1;
    LLCM_LeftMouseButtonUp(index & 0x7f, static_cast<int32_t>(static_cast<uint32_t>(index) >> 7), 0, 0);
}

}  // namespace

int32_t UIStatusPanel::FillObjectList(CThing* thing) {
    gStatusPanelListCount = 0;
    const int32_t type = IteratorTypeForMode(Fields(this)->mMode);
    if (type != 0) UI_IterateThingList(static_cast<UIIteratorTypes>(type), StatusPanelIteratorCallback);
    gStaffRoomPanelThing = thing;
    return gStatusPanelListCount;
}

void UIStatusPanel::GetNextObject() {
    CallSlot_3(this, 0x44);
    CThing* next = nullptr;
    const int32_t type = IteratorTypeForMode(Fields(this)->mMode);
    if (type != 0) next = UI_WrapIterateThingNext(static_cast<UIIteratorTypes>(type), gStaffRoomPanelThing);
    if (next != nullptr && next != gStaffRoomPanelThing) {
        gStaffRoomPanelThing = next;
        CallSlot_3(this, 0xc);
    }
}

// Unit v51114: the status panel's small members (EnableNameClick, EditNamePanelGone, GetPreviousObject, ObjectGone, CloseObject,
// ZoomTo, CloseWindow, GetName, the empty base versions and the constructor), the SamsUtilities byte swaps next to them and the
// rank text object's destructor.

namespace {

// The Mac layout of a status panel is read by offset (the class has no members).
uint8_t* Bytes(UIStatusPanel* panel) { return reinterpret_cast<uint8_t*>(panel); }

// A virtual call through the table at +0x20 on the Mac-layout object: the entry at `offset` of the table is the arena offset of a block
// whose first 8 bytes are the function (the host's way of a __ptr_glue call). r3 is the panel; r4 stays what it was (`thing` where the
// caller leaves it).
void CallSlot_2(UIStatusPanel* panel, uint32_t offset, CThing* thing = nullptr) {
    uint8_t* table = reinterpret_cast<Ptr32<uint8_t>*>(Bytes(panel) + 0x20)->get();
    uint32_t entry = *reinterpret_cast<uint32_t*>(table + offset);
    auto function = *reinterpret_cast<void (**)(UIStatusPanel*, CThing*)>(tpw::gArenaBase + entry);
    function(panel, thing);
}

// The iterator type of a panel mode, 0 for none (the compare tree of GetPreviousObject).
int32_t PreviousIteratorType(int32_t mode) {
    switch (mode) {
        case -2: return kUIIterVisitors;
        case -1: return kUIIterStaff;
        case 0: return kUIIterRides;
        case 1: return kUIIterShops;
        case 2: return kUIIterSideshows;
        case 3: return kUIIterPlainFeatures;
        case 0x3ea: return kUIIterBogs;
        case 0x3eb: return kUIIterStaffRooms;
        case 0x3ec: return kUIIterNonTrackUpgrades;
        default: return 0;
    }
}

}  // namespace

// 0x101929e8: the table, the thing cleared and the windows cleared.
void UIStatusPanel_Construct(UIStatusPanel* self) {
    uint8_t* bytes = Bytes(self);
    reinterpret_cast<Ptr32<uint32_t>*>(bytes + 0x20)->set(UIStatusPanelTable());
    gStaffRoomPanelThing = nullptr;
    *reinterpret_cast<uint32_t*>(bytes + 0xc) = 0;
    *reinterpret_cast<uint32_t*>(bytes + 0x14) = 0;
    *reinterpret_cast<uint32_t*>(bytes + 0x10) = 0;
    *reinterpret_cast<uint32_t*>(bytes + 0x18) = 0;
}

// 0x10192de0: slot 0x44, then the previous thing of the mode's kind (wrapping); a different one becomes the panel's thing and slot 0xc runs.
void UIStatusPanel::GetPreviousObject() {
    CallSlot_2(this, 0x44);
    CThing* previous = nullptr;
    const int32_t type = PreviousIteratorType(*reinterpret_cast<int32_t*>(Bytes(this)));
    if (type != 0) previous = UI_WrapIterateThingPrevious(static_cast<UIIteratorTypes>(type), gStaffRoomPanelThing);
    if (previous != nullptr && previous != gStaffRoomPanelThing) {
        gStaffRoomPanelThing = previous;
        CallSlot_2(this, 0xc);
    }
}
// 0x10192f9c: the thing that goes is the panel's: slot 0x34 (r4 is still the thing).
void UIStatusPanel::ObjectGone(CThing* thing) {
    if (thing == gStaffRoomPanelThing) CallSlot_2(this, 0x34, thing);
}

// 0x10193010: a flag closes the panel's object, none reopens it.
void UIStatusPanel::CloseObject(int32_t flag) {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    if (flag != 0) {
        object->CloseObject();
    } else {
        object->Reopen();
    }
}

// ShowVersionNumberTag 0x101928dc and delme 0x10193084 (ui_StatusPanel.cpp).

// The confirm handler of the dialog: selects the object, then clicks on its cell.
int32_t delme(void* thing) {
    const uint8_t* bytes = static_cast<const uint8_t*>(thing);
    uint16_t cell = static_cast<uint16_t>(bytes[4] + (static_cast<uint32_t>(bytes[6]) << 7) + 1);
    UI_SetSelectedObject(thing);
    LLCM_SetCurrentObject(true);
    LLCC_SystemSetMode(0x33, true);
    uint32_t v = static_cast<uint32_t>(cell) - 1;
    LLCM_LeftMouseButtonUp(static_cast<int32_t>(v & 0x7f), static_cast<int32_t>(v >> 7), 0, 0);
    return 1;
}

void UIStatusPanel::EraseObject() {
    // the original tests the whole byte (lbz, cmplwi 0), not a bool
    if (*reinterpret_cast<const uint8_t*>(&gPlayerSettings.mCurrent.mConfirmDeleteOn) != 0) {
        dlgboxparams params;
        params.mText = UIMain_GetString(0x18c);
        params.mRed = 0xff;
        params.mGreen = 0xff;
        params.mBlue = 0xff;
        params.mValue8 = 4;
        // the Mac stores the thing's address in the word; the host stores its arena offset
        params.mValueC = gStaffRoomPanelThing != nullptr ? static_cast<int32_t>(tpw::ArenaOffset(gStaffRoomPanelThing)) : 0;
        UI_DialogBox(params, static_cast<dlgbutton>(2), nullptr, static_cast<dlgbutton>(1), delme, static_cast<dlgbutton>(0), nullptr,
                     false);
    } else {
        CThing* thing = gStaffRoomPanelThing;
        const uint16_t id = CellId(thing);
        UI_SetSelectedObject(thing);
        LLCM_SetCurrentObject(true);
        LLCC_SystemSetMode(static_cast<CELL_MODES>(0x33), true);
        ClickCell(id);
    }
    gStaffRoomPanelThing = nullptr;
    CallSlot_3(this, 0x34);
}

// Unit v51004: AllStaffPanelMsg 0x10198ac4 (596), UIStatusPanel::RequestMaintenance 0x101932a0 (88), FinancePanelPanelMsg 0x1019f434 (368).

// 0x101932a0: flags the panel's object for maintenance (on) or takes the flag off, then the panel's table slot at Mac +0x1c.
void UIStatusPanel::RequestMaintenance(bool on) {
    reinterpret_cast<CObject*>(gStaffRoomPanelThing)->RequestMaintenance(on ? 0 : 1);
    UIStatusPanel_CallSlot(this, 0x1c);
}

// 0x10193330
void UIStatusPanel::ZoomTo() { UIMain_ZoomToThing(gStaffRoomPanelThing->GetThingId()); }

void UIStatusPanel::InitWindow() {}

void UIStatusPanel::UpdateWindow() {}
void UIStatusPanel::DeInitWindow() {}
void UIStatusPanel::InitStatusWindow() {}
void UIStatusPanel::InitViewWindow() {}
void UIStatusPanel::UpdateStatusWindow() {}
void UIStatusPanel::UpdateViewWindow() {}
void UIStatusPanel::DeInitStatusWindow() {}
void UIStatusPanel::DeInitViewWindow() {}

void UIStatusPanel::OpenWindow(int16_t* description, InterfaceWindowCallback callback, int32_t windowA, int32_t windowB,
                               int32_t windowC) {
    UIStatusPanelFields* f = Fields(this);
    if (f->mWindow.mOffset != 0) return;
    f->mF14.mOffset = 0;
    f->mF10.mOffset = 0;
    f->mF18.mOffset = 0;
    f->mF04 = 0;
    UIMain_CheckPanel();
    InterfaceWindow* built = gGameWindow->Build(description, callback);
    if (built == nullptr) return;
    f->mWindow = built;
    UIMain_SetPanel(built);
    CallSlot_3(this, 0x8);
    f->mF14 = f->mWindow->GetWindow(windowA);
    if (f->mF14.mOffset != 0) {
        if (windowC != 0) {
            f->mF18 = f->mF14->GetWindow(windowC);
            f->mF18->Enable(0);
        }
        f->mF14->SetCallbackFunction(ViewWindowCallback);
        f->mF14->setExtraParam(6, this);
        CallSlot_3(this, 0x18);
    }
    f->mF10 = f->mWindow->GetWindow(windowB);
    if (f->mF10.mOffset != 0) CallSlot_3(this, 0x14);
    CallSlot_3(this, 0xc);
    f->mWindow->setExtraParam(6, this);
}

// 0x10193774: tells the panel's window (+0xc) message 4.
void UIStatusPanel::CloseWindow() {
    UI_SendMessage(reinterpret_cast<Ptr32<InterfaceWindow>*>(Bytes(this) + 0xc)->get(), 4, 0, 0);
}

void UIStatusPanel::Show(CThing*) {}
void UIStatusPanel::Remove() {}
void UIStatusPanel::ChangeName(char16_t*) {}
// 0x1019387c
char16_t* UIStatusPanel::GetName() { return UIMain_GetObjectName(reinterpret_cast<CObject*>(gStaffRoomPanelThing)); }

void UIStatusPanel::Move() {
    CThing* thing = gStaffRoomPanelThing;
    CObject* object = reinterpret_cast<CObject*>(thing);
    UI_SetSelectedObject(object);
    LLCM_SetCurrentObject(true);
    const uint16_t id = CellId(gStaffRoomPanelThing);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(object);
    RideInfo* info = object->GetDBInfo();
    if (info->Value(kRideSlot_HasQueue).mInteger != 0) {   // the Mac's +0x40 (unit v5L31: the host's balance slots are wider)
        gStatusPanelClickStackDir = static_cast<uint8_t>(LLCC_SystemConstructQueueClickStack(object));
        LLCC_SystemMakeClickStackCopy();
    }
    LLCC_SystemSetMode(static_cast<CELL_MODES>(0x33), true);
    ClickCell(id);
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> pathId;
    pathId.mIndex = *reinterpret_cast<const uint16_t*>(bytes + 0xe);
    CMMPathDraw* path = new CMMPathDraw(static_cast<CELL_MODES>(0x3b), pathId);
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse;
        CMMDefault* normal = new CMMDefault;
        mouse->AssignMouseMode(normal);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
    gMouse->AssignMouseMode(path);
    gStatusPanelMoveObject = object;
    LLCC_SystemSetMode(static_cast<CELL_MODES>(0x3b), true);
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> rideId;
    rideId.mIndex = *reinterpret_cast<const uint16_t*>(bytes + 0xe);
    LLCC_SystemSetRide(rideId, 0);
    LLCC_SetRideRotation(*reinterpret_cast<const int32_t*>(bytes + 0x10), true);
    gStaffRoomPanelThing = nullptr;
    CallSlot_3(this, 0x34);
}

// Unit v5910: 0x10193af4 UIStatusPanel::DisplayStatusText, 0x10193c78 DefaultStatusPanelMsg, 0x10193dcc UIStatusPanel::InitEditBox,
// 0x10193fac the edit-name panel's handler (TwoEditsMsg on the Mac, TwoEditsMsgStatus here: the park-name handler already has the plain name).

// 0x1024de12: an empty string (the word there starts 0x0000).
char16_t gStatusPanelEmptyText[1] = {0};

namespace {

UIStatusPanelFields* Fields_2(void* panel) { return reinterpret_cast<UIStatusPanelFields*>(panel); }

// __ptr_glue call of the panel's second table (the pointer at +0x20), slot at byte offset `slot`.
void CallStatusSlot(void* panel, uint32_t slot) {
    Ptr32<uint32_t> entry;
    entry.mOffset = Fields_2(panel)->mVtable.get()[slot / 4];
    typedef void (*Fn)(void*);
    (*reinterpret_cast<Fn*>(entry.get()))(panel);
}

}  // namespace

// 0x10193af4 (128 bytes)
void UIStatusPanel::DisplayStatusText() {
    UIStatusPanelFields* f = Fields_2(this);
    if (gStaffRoomPanelThing != nullptr) {
        InterfaceWindow* window = f->mF18.get();
        if (window == nullptr) return;
        if (f->mF04 == 0) window->Show(0);
        else window->Show(1);
        UIMain_SetObjectStateText(f->mF18.get(), f->mF04);
    }
}

namespace {
// The virtual call: table pointer at +0x20, each entry the arena offset of a block whose first bytes are the function.
void CallPanelSlot(void* panel, uint32_t slot) {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(panel);
    Ptr32<void (*)(void*)> entry;
    entry.mOffset = fields->mVtable.get()[slot / 4];
    (*entry.get())(panel);
}
}  // namespace

// 0x10193bac (140 bytes): message 0x10006 zooms to the thing and calls slot 0x34 of the panel in extra parameter 6.
int32_t ViewWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        void* panel = window->getExtraParam(6);
        tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id = gStaffRoomPanelThing->GetThingId();
        UIMain_ZoomToThing(id);
        CallPanelSlot(panel, 0x34);
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x10193c78 (276 bytes)
int32_t DefaultStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    void* panel = window->getExtraParam(6);
    if (message == 0x1000a) {
        if (a == 0x2800) static_cast<UIStatusPanel*>(panel)->GetNextObject();
        else if (a == 0x2600) static_cast<UIStatusPanel*>(panel)->GetPreviousObject();
    } else if (message == 0x14) {
        if (gStaffRoomPanelThing != nullptr) CallStatusSlot(panel, 0x44);
        CallStatusSlot(panel, 0x24);
        CallStatusSlot(panel, 0x28);
        CallStatusSlot(panel, 0x10);
        Fields_2(panel)->mWindow.mOffset = 0;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

// 0x10193dcc (124 bytes)
void UIStatusPanel::InitEditBox(int32_t id) {
    UIStatusPanelFields* f = Fields_2(this);
    InterfaceWindow* edit = f->mWindow.get()->GetWindow(id);
    Ptr32<InterfaceWindow> stored;
    stored.set(edit);
    f->mF1C = stored.mOffset;
    edit->SetCallbackFunction(StatusPanelTitleMsg);
    edit->setExtraParam(0, this);
    UIMain_SetTitleText(f->mWindow.get(), id, gStatusPanelEmptyText);
}

// 0x10193fac (548 bytes)
int32_t TwoEditsMsgStatus(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x802) {
        if (a == 0x3943c) {
            window->GetWindow(0x3943d)->CaptureKb(0);
        } else {
            UI_PostMessage(window, 0x100, -1, 0);
        }
    } else if (message < 0x802) {
        if (message == 0x100) {
            if (a == -1) {
                CObject* object = static_cast<CObject*>(window->getExtraParam(0));
                object->SetSignNameA(gEditSignBufA);
                object->SetSignNameB(gEditSignBufB);
                RegenerateRideSign(object, gEditSignBufA, gEditSignBufB);
                CallStatusSlot(window->getExtraParam(1), 0xc);
                UI_SendMessage(window, 4, 0, 0);
                return 0;
            }
            if (a == -2) {
                UI_SendMessage(window, 4, 0, 0);
                return 0;
            }
        } else if (message < 0x100 && message == 0x14) {
            UIMain_EnableAllKeyTables();
            static_cast<UIStatusPanel*>(window->getExtraParam(1))->EditNamePanelGone();
            UIMain_DecideKbCapture();
        }
    } else {
        if (message == 0x804) {
            UI_SendMessage(window, 4, 0, 0);
            return 0;
        }
        if (message < 0x804) {
            if (a == 0x3943c) window->GetWindow(0x3943d)->CaptureKb(0);
            else window->GetWindow(0x3943c)->CaptureKb(0);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x10193e7c (240 bytes): 0x10001 white font, 0x10002 the title colour, 0x10006 with a == 0 slot 0x40 of the panel in
// extra parameter 0; then the default handler answers.
int32_t StatusPanelTitleMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10002) {
        UI_TITLE_COLOUR(window);
    } else if (message < 0x10002) {
        if (message > 0x10000) window->SetFontColour(0xff, 0xff, 0xff, 0xff);
    } else if (message == 0x10006 && a == 0) {
        void* panel = window->getExtraParam(0);
        CallPanelSlot(panel, 0x40);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// UIStatusPanel::ShowEditNamePanel 0x10194208 (900 bytes): opens the two-edit-box name panel for the sign object the status panel shows.

int16_t gEditNamePanelScript[] = {TPW_WINDOW_SCRIPT_102A8530};   // 0x102a8530: the image's words (generated, devtools/gen_game_tables.py)

void UIStatusPanel::ShowEditNamePanel() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    Ptr32<InterfaceWindow>& windowField = *reinterpret_cast<Ptr32<InterfaceWindow>*>(self + 8);
    Ptr32<InterfaceWindow>& parentField = *reinterpret_cast<Ptr32<InterfaceWindow>*>(self + 0xc);
    if (windowField.mOffset != 0) return;
    CThing* thing = gStaffRoomPanelThing;
    if (thing == nullptr) return;
    if (reinterpret_cast<uint8_t*>(thing)[2] != 3) return;
    CObject* object = reinterpret_cast<CObject*>(thing);

    InterfaceWindow* parent = parentField.get();
    int16_t z = parent->mZ;   // the Mac's +0xdc, by name: the host window is laid out differently
    InterfaceWindow* window = parent->Build(gEditNamePanelScript, TwoEditsMsg);
    windowField.set(window);
    window->setZ(static_cast<int16_t>(z + 0x11));
    window->setExtraParam(0, thing);
    window->setExtraParam(1, this);

    InterfaceTitleTextObject* titleObject = new InterfaceTitleTextObject(nullptr);
    window->GetWindow(0x3943e)->SetClientDrawObject(titleObject);
    InterfaceFont* titleFont = GetFontTableEntry(5);
    window->GetWindow(0x3943e)->SetFont(titleFont);
    window->GetWindow(0x3943e)->SetFontJust(1, 1);
    const char16_t* title = UIMain_GetString(0xc2);
    window->GetWindow(0x3943e)->SetText(const_cast<char16_t*>(title));
    UI_TITLE_COLOUR(window->GetWindow(0x3943e));

    InterfaceEditBox* editA = static_cast<InterfaceEditBox*>(window->GetWindow(0x3943c));
    editA->SetClientDrawObject(new InterfaceCursorPosTextObjectSprite(nullptr));
    editA->SetFont(GetFontTableEntry(7));
    editA->SetFontColour(0xff, 0xff, 0xff, 0xff);
    editA->SetStaticTextArea(gEditSignBufA, 0xe);
    editA->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    object->GetSignNameA(gUIMainConvertBuffer, 0x3fff);
    editA->SetText(gUIMainConvertBuffer);
    editA->SelectAll();

    InterfaceEditBox* editB = static_cast<InterfaceEditBox*>(window->GetWindow(0x3943d));
    editB->SetClientDrawObject(new InterfaceCursorPosTextObjectSprite(nullptr));
    editB->SetFont(GetFontTableEntry(7));
    editB->SetFontColour(0xff, 0xff, 0xff, 0xff);
    editB->SetStaticTextArea(gEditSignBufB, 0xe);
    editB->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    object->GetSignNameB(gUIMainConvertBuffer, 0x3fff);
    editB->SetText(gUIMainConvertBuffer);
    UIMain_DisableAllKeyTables();
    window->GetWindow(0x3943c)->CaptureKb(0);
}

// 0x101945c4: forgets the edit-name window.
void UIStatusPanel::EditNamePanelGone() { *reinterpret_cast<uint32_t*>(Bytes(this) + 8) = 0; }

// 0x10194608: the panel's window (+0x1c) is enabled or disabled (its virtual at Mac vtable +0x1c; the argument is a halfword).
void UIStatusPanel::EnableNameClick(int32_t enable) {
    InterfaceWindow* window = reinterpret_cast<Ptr32<InterfaceWindow>*>(Bytes(this) + 0x1c)->get();
    window->Enable(static_cast<int16_t>(enable));
}

// 0x10194674 and its twins: nothing.
void UIStatusPanel::ApplyChanges() {}

// 0x101946ac (28 bytes)
void __sinit_ui_StatusPanel_cpp() {
    gStatusPanelWord47ac = 0;
    gStatusPanelWord47a8 = 0x10000;
}

// ui_RideStatus.cpp static initialiser (0x101bc3ec, 80 bytes).

// Unit v5L31: the status panels' tables of virtual slots, the ones their constructors store at +0x20 (the Mac's vtables 0x102a8644 and
// those of the nine panel classes). A Ptr32 cannot name the Mac's data, so each table is a block of the arena (UIStatusPanel_MakeTable),
// made on first use and kept. Slot by slot they are the Mac's: the class's own member where the Mac's table has one, the base's otherwise.

namespace {

template <class P, void (P::*F)()>
uint64_t Plain(void* panel, const void*) {
    (static_cast<P*>(panel)->*F)();
    return 0;
}
template <class P, void (P::*F)(CThing*)>
uint64_t WithThing(void* panel, const void* thing) {
    (static_cast<P*>(panel)->*F)(static_cast<CThing*>(const_cast<void*>(thing)));
    return 0;
}
template <class P, void (P::*F)(char16_t*)>
uint64_t WithName(void* panel, const void* name) {
    (static_cast<P*>(panel)->*F)(static_cast<char16_t*>(const_cast<void*>(name)));
    return 0;
}
template <class P, class R, R (P::*F)()>
uint64_t Answer(void* panel, const void*) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>((static_cast<P*>(panel)->*F)()));
}

constexpr int32_t kSlots = 0x48 / 4;   // the header words and the 16 slots of UIStatusPanel (+0x08 to +0x44)

struct Slots {
    UIStatusPanelSlot s[kSlots + 1] = {};   // one more for FeatureStatusPanel::RideIt (+0x48)
    UIStatusPanelSlot& operator[](uint32_t offset) { return s[offset / 4]; }
};

// The base's table (0x102a8644).
Slots BaseSlots() {
    Slots t;
    t[0x08] = Plain<UIStatusPanel, &UIStatusPanel::InitWindow>;
    t[0x0c] = Plain<UIStatusPanel, &UIStatusPanel::UpdateWindow>;
    t[0x10] = Plain<UIStatusPanel, &UIStatusPanel::DeInitWindow>;
    t[0x14] = Plain<UIStatusPanel, &UIStatusPanel::InitStatusWindow>;
    t[0x18] = Plain<UIStatusPanel, &UIStatusPanel::InitViewWindow>;
    t[0x1c] = Plain<UIStatusPanel, &UIStatusPanel::UpdateStatusWindow>;
    t[0x20] = Plain<UIStatusPanel, &UIStatusPanel::UpdateViewWindow>;
    t[0x24] = Plain<UIStatusPanel, &UIStatusPanel::DeInitStatusWindow>;
    t[0x28] = Plain<UIStatusPanel, &UIStatusPanel::DeInitViewWindow>;
    t[0x2c] = Plain<UIStatusPanel, &UIStatusPanel::DisplayStatusText>;
    t[0x30] = WithThing<UIStatusPanel, &UIStatusPanel::Show>;
    t[0x34] = Plain<UIStatusPanel, &UIStatusPanel::Remove>;
    t[0x38] = WithName<UIStatusPanel, &UIStatusPanel::ChangeName>;
    t[0x3c] = Answer<UIStatusPanel, char16_t*, &UIStatusPanel::GetName>;
    t[0x40] = Plain<UIStatusPanel, &UIStatusPanel::ShowEditNamePanel>;
    t[0x44] = Plain<UIStatusPanel, &UIStatusPanel::ApplyChanges>;
    return t;
}

// Every panel class has its own +0x08 to +0x28, Show (+0x30) and Remove (+0x34); +0x2c to +0x44 are the base's unless it overrides them.
template <class P>
Slots OwnSlots() {
    Slots t = BaseSlots();
    t[0x08] = Plain<P, &P::InitWindow>;
    t[0x0c] = Plain<P, &P::UpdateWindow>;
    t[0x10] = Plain<P, &P::DeInitWindow>;
    t[0x14] = Plain<P, &P::InitStatusWindow>;
    t[0x18] = Plain<P, &P::InitViewWindow>;
    t[0x1c] = Plain<P, &P::UpdateStatusWindow>;
    t[0x20] = Plain<P, &P::UpdateViewWindow>;
    t[0x24] = Plain<P, &P::DeInitStatusWindow>;
    t[0x28] = Plain<P, &P::DeInitViewWindow>;
    t[0x30] = WithThing<P, &P::Show>;
    t[0x34] = Plain<P, &P::Remove>;
    return t;
}

uint32_t* Make(const Slots& t) { return UIStatusPanel_MakeTable(t.s, kSlots + 1); }

}  // namespace

uint32_t* UIStatusPanelTable() {   // 0x102a8644 (name invented)
    static uint32_t* table = Make(BaseSlots());
    return table;
}

uint32_t* RideStatusPanelTable() {   // 0x102ad5fc
    static uint32_t* table = [] {
        Slots t = OwnSlots<RideStatusPanel>();
        t[0x44] = Plain<RideStatusPanel, &RideStatusPanel::ApplyChanges>;
        return Make(t);
    }();
    return table;
}

uint32_t* ShopStatusPanelTable() {   // 0x102adbd8
    static uint32_t* table = [] {
        Slots t = OwnSlots<ShopStatusPanel>();
        t[0x44] = Plain<ShopStatusPanel, &ShopStatusPanel::ApplyChanges>;
        return Make(t);
    }();
    return table;
}

uint32_t* ShowStatusPanelTable() {   // 0x102ae980
    static uint32_t* table = [] {
        Slots t = OwnSlots<ShowStatusPanel>();
        t[0x44] = Plain<ShowStatusPanel, &ShowStatusPanel::ApplyChanges>;
        return Make(t);
    }();
    return table;
}

uint32_t* StaffStatusPanelTable() {   // 0x102af264
    static uint32_t* table = [] {
        Slots t = OwnSlots<StaffStatusPanel>();
        t[0x38] = WithName<StaffStatusPanel, &StaffStatusPanel::ChangeName>;
        t[0x3c] = Answer<StaffStatusPanel, const char16_t*, &StaffStatusPanel::GetName>;
        t[0x40] = Plain<StaffStatusPanel, &StaffStatusPanel::ShowEditNamePanel>;
        return Make(t);
    }();
    return table;
}

uint32_t* StaffRoomStatusPanelTable() {   // 0x102aed88
    static uint32_t* table = Make(OwnSlots<StaffRoomStatusPanel>());
    return table;
}

uint32_t* UpgradeStatusPanelTable() {   // 0x102af4dc
    static uint32_t* table = Make(OwnSlots<UpgradeStatusPanel>());
    return table;
}

uint32_t* FeatureStatusPanelTable() {   // 0x102a9ba0
    static uint32_t* table = [] {
        Slots t = OwnSlots<FeatureStatusPanel>();
        t[0x48] = Plain<FeatureStatusPanel, &FeatureStatusPanel::RideIt>;
        return Make(t);
    }();
    return table;
}

uint32_t* BogStatusPanelTable() {   // 0x102a9384
    static uint32_t* table = [] {
        Slots t = OwnSlots<BogStatusPanel>();
        t[0x44] = Plain<BogStatusPanel, &BogStatusPanel::ApplyChanges>;
        return Make(t);
    }();
    return table;
}

uint32_t* VisitorStatusPanelTable() {   // 0x102a4b5c
    static uint32_t* table = Make(OwnSlots<VisitorStatusPanel>());
    return table;
}

// Park window scripts (main); signed 16-bit records, in image order.

// 0x102a82a0, 137 words
int16_t gTagPanelScript[137] = {
    0, 1, 1, 0, 3373, 1, 458, 1101, 1073, 1442,
    17, -1, -1,
    18, -1, -1,
    0, 2, 1, 0, 3374, 1, 486, 1325, 589, 1428,
    17, -1, -1,
    18, -1, -1,
    1, 31966, -22132,
    5,
    0, 2, 1, 0, 3375, 1, 486, 1218, 589, 1320,
    17, -1, -1,
    18, -1, -1,
    5,
    0, 2, 1, 0, 3376, 1, 486, 1110, 589, 1213,
    17, -1, -1,
    18, -1, -1,
    5,
    0, 1, 3, 0, 3377, 1, 458, 1101, 623, 1442,
    17, -1, -1,
    18, -1, -1,
    1, -17131, -17811,
    5,
    0, 1, 1, 0, 3378, 1, 623, 1101, 835, 1442,
    17, -1, -1,
    18, -1, -1,
    1, -17178, -17811,
    3, 609, 1118, 1056, 1422,
    5,
    0, 1, 1, 0, 3379, 1, 835, 1101, 1073, 1442,
    17, -1, -1,
    18, -1, -1,
    1, -18165, -17811,
    5,
    5,
    5,
};
