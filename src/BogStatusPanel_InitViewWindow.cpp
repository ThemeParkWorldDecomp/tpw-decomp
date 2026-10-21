// BogStatusPanel (main app): the bog status panel's window functions. The panel has the UIStatusPanelFields layout (ui_main.h).
#include <cstdint>
#include <cstring>

#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_main.h"
#include "WindowScripts.inc"

CThing* gBogStatusThing = nullptr;                 // 0x103947fc
int16_t gBogStatusScript[] = {TPW_WINDOW_SCRIPT_102A908C};   // 0x102a908c: the image's words (generated, devtools/gen_game_tables.py)

namespace {

// The host's way of the Mac's virtual call through a table of TVector pointers: the table holds Ptr32 words, each the arena offset of a
// block whose first 8 bytes are the function (see UIStatusPanelFields).
using Slot = uint64_t (*)(void*, uint64_t);

Slot SlotOf(Ptr32<uint32_t> table, uint32_t byteOffset) {
    Ptr32<Slot> entry;
    entry.mOffset = table.get()[byteOffset / 4];
    return *entry.get();
}

}  // namespace

// 0x10199ed0 (236 bytes): with a view window (+0x18): one deeper than it was by 11, hidden, a new text sprite as its client draw
// object, justification 1, 1, font table entry 8, black opaque, no text. The new never answers NULL on the Mac but the test is kept.
void BogStatusPanel::InitViewWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    InterfaceWindow* window = fields->mF18.get();
    if (window != nullptr) {
        window->setZ(static_cast<int16_t>(window->mZ + 0xb));
        fields->mF18.get()->Show(0);
        void* sprite = lender_callee::OperatorNew(0x19c);
        if (sprite != nullptr) lender_callee::SpriteCtor(sprite);
        fields->mF18.get()->SetClientDrawObject(static_cast<InterfaceDrawObject*>(sprite));
        fields->mF18.get()->SetFontJust(1, 1);
        InterfaceFont* font = GetFontTableEntry(8);
        fields->mF18.get()->SetFont(font);
        fields->mF18.get()->SetFontColour(0, 0, 0, 0xff);
        fields->mF18.get()->SetClientDrawText(nullptr);
    }
}

// 0x1019a050 (164 bytes): with the window at +0x14 and a thing in gStaffRoomPanelThing: the ride preview of the ride record's word at
// +0x78c, in the window's client rectangle, one in front of the window.
void BogStatusPanel::UpdateViewWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    if (fields->mF14.mOffset != 0 && gStaffRoomPanelThing != nullptr) {
        RideInfo* info = reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GetDBInfo();
        CoolRect rect;
        fields->mF14.get()->GetClientRect(&rect);
        InterfaceWindow* window = fields->mF14.get();
        // The record's ride type number (+0x78c) by name: the host's record is laid out wider (unit v5L31).
        UIMain_ShowRidePreview(rect, info->mRideTypeNumber, static_cast<int32_t>(window->mZ) + 1);
    }
}

// 0x1019a7c8 (152 bytes): fills the object list for the thing Show was given; with fewer than 2 objects the two buttons 0x15bb6 and
// 0x15bb7 of the panel's window are hidden (GetWindow's answer is not checked); then the edit box 0x15bc1.
void BogStatusPanel::InitWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(this);
    if (base->FillObjectList(gBogStatusThing) < 2) {
        fields->mWindow.get()->GetWindow(0x15bb6)->Show(0);
        fields->mWindow.get()->GetWindow(0x15bb7)->Show(0);
    }
    base->InitEditBox(0x15bc1);
}

// 0x1019a8cc (120 bytes): the panel's table slot 0x3c answers the text, which goes to the window at +0x1c through that window's
// virtual SetText (slot 0x2c of its table at +0x118); then the panel's slots 0x20 and 0x1c.
void BogStatusPanel::UpdateWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    uint64_t text = SlotOf(fields->mVtable, 0x3c)(this, 0);
    Ptr32<InterfaceWindow> textWindow;
    textWindow.mOffset = fields->mF1C;
    textWindow.get()->SetText(reinterpret_cast<char16_t*>(text));
    SlotOf(fields->mVtable, 0x20)(this, 0);
    SlotOf(fields->mVtable, 0x1c)(this, 0);
}

// 0x1019ac5c (140 bytes): keeps the thing, opens the window and gives the advisor's word: 0xd5 when the thing's float at +0x40 (cut to
// an integer, low byte) is below 0x19, else 0x12b.
void BogStatusPanel::Show(CThing* thing) {
    gBogStatusThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gBogStatusScript, BogStatusMsg, 0x15bb8, 0x15bba, 0x15bb9);
    float value = *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(thing) + 0x40);
    int32_t cut;
    if (value != value || value < -2147483648.0f) cut = INT32_MIN;
    else if (value >= 2147483648.0f) cut = 0x7fffffff;
    else cut = static_cast<int32_t>(value);
    if ((static_cast<uint32_t>(cut) & 0xff) < 0x19)
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xd5));
    else
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x12b));
}
