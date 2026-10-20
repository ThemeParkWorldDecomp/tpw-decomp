// Unit v5241: the ride list panel's info window set-up (InitInfoWindow 0x101b6500) and refresh (UpdateInfoWindow 0x101b6a5c).
#include <cstdint>

#include "InterfaceWindow.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "World.h"
#include "analyser.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

namespace {

const char16_t kIntegerFormat[] = u"%d";            // 0x1024e43c
const char kGradientName[] = "ridestatbar.wct";     // 0x1024e41a

// value * 1024 / 100 as the listing does it (slwi 10, then the multiply-high division by 100).
int32_t Scaled(int32_t value) { return static_cast<int32_t>(static_cast<uint32_t>(value) << 10) / 100; }

// The ride record's balance slots at Mac offsets 0x13c, 0x19c and 0x1b0 (0x0c + 4 * slot).
int32_t SlotAt(RideInfo* ride, int32_t slot) { return ride->Value(static_cast<RideBalanceSlot>(slot)).mInteger; }

void SetUpText(InterfaceWindow* window, int32_t stringId) {
    InterfaceTextObjectSprite* object = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(object);
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(stringId)));
}

void SetUpBar(InterfaceWindow* panel, int32_t id, int32_t value) {
    InterfaceWindow* window = panel->GetWindow(id);
    UIMain_IndentClient(window, 0, 10);
    InterfaceDrawEngineHGradient* object = new InterfaceDrawEngineHGradient(const_cast<char*>(kGradientName));
    window->SetClientDrawObject(object);
    window->SetValue(value);
}

}  // namespace

void InitInfoWindow() {
    InterfaceWindow* panel = gRideListPanel->GetWindow(0x1ed);
    if (panel == nullptr) return;
    SetUpText(panel->GetWindow(0x1f4), 0x7d);
    SetUpText(panel->GetWindow(0x1f3), 0x7e);
    SetUpText(panel->GetWindow(0x1f1), 0x7f);
    SetUpText(panel->GetWindow(0x1f6), 0x80);
    SetUpText(panel->GetWindow(0x1f7), 0x81);
    SetUpText(panel->GetWindow(0x1f5), 0x88);
    SetUpText(panel->GetWindow(0x1f2), 0x88);
    SetUpBar(panel, 0x1ee, 500);
    SetUpBar(panel, 0x1ef, 200);
    SetUpBar(panel, 0x1f0, 100);
}

void UpdateInfoWindow(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id) {
    char16_t text[16];     // r1+0x3c
    InterfaceWindow* panel = gRideListPanel->GetWindow(0x1ed);
    if (panel == nullptr) return;
    int16_t key = static_cast<int16_t>(id.mIndex);
    if (key < 0) return;
    RideInfo* ride = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{id.mIndex});
    int32_t count = gGameWorld->GetParkAnalyser()->NumberOfObjectInPark(id);
    if (count != 0) {
        swprintf(text, 10, kIntegerFormat, count);
    } else {
        swprintf(text, 10, UIMain_GetString(0x88));
    }
    InterfaceWindow* countWindow = panel->GetWindow(0x1f5);
    if (countWindow != nullptr) countWindow->SetClientDrawText(text);
    switch (gRideListType) {
        case 0: {
            InterfaceWindow* window = panel->GetWindow(0x1ee);
            if (window != nullptr) window->SetValue(Scaled(SlotAt(ride, 76)));
            window = panel->GetWindow(0x1f2);
            if (window != nullptr) {
                swprintf(gUIMainConvertBuffer, 0x3fff, kIntegerFormat, SlotAt(ride, 100));
                window->SetText(gUIMainConvertBuffer);
            }
            window = panel->GetWindow(0x1ef);
            if (window != nullptr) window->SetValue(Scaled(SlotAt(ride, 105)));
            break;
        }
        case 2: {
            InterfaceWindow* window = panel->GetWindow(0x1ee);
            if (window != nullptr) window->SetValue(Scaled(SlotAt(ride, 76)));
            break;
        }
        default:
            break;
    }
}
