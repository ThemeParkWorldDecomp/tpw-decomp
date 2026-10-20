// ui_StatusPanel.cpp (main app), unit v5090: GetNextObject 0x10192c28, FillObjectList 0x10192a90, EraseObject 0x10193138,
// OpenWindow 0x101935a0, Move 0x101938d8 of UIStatusPanel.
#include <cstdint>

#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "ThingReceivers.h"
#include "TPSettings.h"
#include "ui_ErrorDisplay.h"
#include "ui_iterators.h"
#include "ui_main.h"

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
void CallSlot(UIStatusPanel* panel, uint32_t offset) {
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

void UIStatusPanel::GetNextObject() {
    CallSlot(this, 0x44);
    CThing* next = nullptr;
    const int32_t type = IteratorTypeForMode(Fields(this)->mMode);
    if (type != 0) next = UI_WrapIterateThingNext(static_cast<UIIteratorTypes>(type), gStaffRoomPanelThing);
    if (next != nullptr && next != gStaffRoomPanelThing) {
        gStaffRoomPanelThing = next;
        CallSlot(this, 0xc);
    }
}

int32_t UIStatusPanel::FillObjectList(CThing* thing) {
    gStatusPanelListCount = 0;
    const int32_t type = IteratorTypeForMode(Fields(this)->mMode);
    if (type != 0) UI_IterateThingList(static_cast<UIIteratorTypes>(type), StatusPanelIteratorCallback);
    gStaffRoomPanelThing = thing;
    return gStatusPanelListCount;
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
    CallSlot(this, 0x34);
}

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
    CallSlot(this, 0x8);
    f->mF14 = f->mWindow->GetWindow(windowA);
    if (f->mF14.mOffset != 0) {
        if (windowC != 0) {
            f->mF18 = f->mF14->GetWindow(windowC);
            f->mF18->Enable(0);
        }
        f->mF14->SetCallbackFunction(ViewWindowCallback);
        f->mF14->setExtraParam(6, this);
        CallSlot(this, 0x18);
    }
    f->mF10 = f->mWindow->GetWindow(windowB);
    if (f->mF10.mOffset != 0) CallSlot(this, 0x14);
    CallSlot(this, 0xc);
    f->mWindow->setExtraParam(6, this);
}

void UIStatusPanel::Move() {
    CThing* thing = gStaffRoomPanelThing;
    CObject* object = reinterpret_cast<CObject*>(thing);
    UI_SetSelectedObject(object);
    LLCM_SetCurrentObject(true);
    const uint16_t id = CellId(gStaffRoomPanelThing);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(object);
    RideInfo* info = object->GetDBInfo();
    if (*reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(info) + 0x40) != 0) {
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
    CallSlot(this, 0x34);
}
