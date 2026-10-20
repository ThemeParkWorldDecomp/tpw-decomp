// FeatureStatusPanel (main app, 0x1019e374..0x1019edc4, unit v5311): the status panel of a feature (a ride's scenery piece).
// The panel has the UIStatusPanelFields layout (include/ui_main.h): +0x0c the panel's window, +0x10 the status window, +0x14 the view
// window, +0x1c the text window, +0x20 the table of virtual members (each entry the arena offset of a block whose first 8 bytes are a
// uint64_t (*)(void* panel), the host's way of a virtual call on a Mac-layout object).
#include <cstdint>

#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

// 0x1039486c (initial 0): the thing Show was given.
CThing* gFeatureStatusThing = nullptr;
// 0x102a9948 (initial word 1): the window script; the rest is not recovered.
int16_t gFeatureStatusScript[] = {0, 1};

namespace {

using PanelSlot = uint64_t (*)(void*);

UIStatusPanelFields* Fields(FeatureStatusPanel* panel) { return reinterpret_cast<UIStatusPanelFields*>(panel); }

// A virtual call through the table at +0x20: the entry at the Mac byte offset `slot`, the panel as the only argument.
uint64_t CallSlot(FeatureStatusPanel* panel, int32_t slot) {
    uint32_t entry = Fields(panel)->mVtable.get()[slot / 4];
    Ptr32<PanelSlot> block{entry};
    return (*block.get())(panel);
}

CObject* TheObject() { return reinterpret_cast<CObject*>(gStaffRoomPanelThing); }

}  // namespace

// 0x1019e728 (244 bytes)
void FeatureStatusPanel::UpdateStatusWindow() {
    CObject* object = TheObject();
    UIStatusPanelFields* fields = Fields(this);
    object->GetDBInfo();
    if (fields->mF10) {
        InterfaceWindow* countWindow = fields->mF10->GetWindow(0x1586);
        if (countWindow) {
            tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
            id.mIndex = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(object) + 0xe);
            int32_t count = gGameWorld->GetParkAnalyser()->NumberOfObjectInPark(id);
            countWindow->SetValue(count);
        }
        InterfaceWindow* valueWindow = fields->mF10->GetWindow(0x1584);
        valueWindow->SetValue(static_cast<int32_t>(object->GetCurrentResidualValue()));
    }
}

// 0x1019e85c (136 bytes)
void FeatureStatusPanel::InitWindow() {
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(this);
    UIStatusPanelFields* fields = Fields(this);
    if (base->FillObjectList(gFeatureStatusThing) < 2) {
        fields->mWindow->GetWindow(0x157c)->Show(0);
        fields->mWindow->GetWindow(0x157d)->Show(0);
    }
    base->InitEditBox(0x157f);
}

// 0x1019e958 (208 bytes)
void FeatureStatusPanel::UpdateWindow() {
    CObject* object = TheObject();
    UIStatusPanelFields* fields = Fields(this);
    uint64_t text = CallSlot(this, 0x3c);
    Ptr32<InterfaceWindow> textWindow{fields->mF1C};
    textWindow->SetText(reinterpret_cast<char16_t*>(text));
    if (*reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(object) + 0x2e) & 0x10)
        fields->mWindow->GetWindow(0x1581)->Show(1);
    else
        fields->mWindow->GetWindow(0x1581)->Show(0);
    CallSlot(this, 0x20);
    CallSlot(this, 0x1c);
}

// 0x1019ea60 (80 bytes)
void FeatureStatusPanel::RideIt() {
    TheObject()->GoFirstPerson(0);
    CallSlot(this, 0x34);
}

// 0x1019e374 (164 bytes)
void FeatureStatusPanel::UpdateViewWindow() {
    UIStatusPanelFields* fields = Fields(this);
    if (fields->mF14 && gStaffRoomPanelThing != nullptr) {
        RideInfo* info = TheObject()->GetDBInfo();
        CoolRect rect;
        fields->mF14->GetClientRect(&rect);
        UIMain_ShowRidePreview(rect, info->mRideTypeNumber, fields->mF14->mZ + 10);
    }
}

// 0x1019edc4 (76 bytes)
void FeatureStatusPanel::Show(CThing* thing) {
    gFeatureStatusThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gFeatureStatusScript, FeatureStatusMsg, 0x157e, 0x1583, 0);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x12a));
}
