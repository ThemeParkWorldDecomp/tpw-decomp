// ui_upgradeStatus.cpp: the upgrade status panel's window handler (UpgradeStatusMsg, 0x101c63b4) and ui_staffStatus.cpp's static
// initialiser (__sinit_ui_staffStatus_cpp, 0x101c5bec). Unit v5548.
#include <cstdint>

#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

int32_t gStaffStatusF40;   // 0x10395b40
int32_t gStaffStatusF44;   // 0x10395b44

// The staff panel is built with the UIStatusPanel constructor, then its table pointer (+0x20) gets the Mac's table at 0x102af264 (the table
// is not recovered, so the word is kept as the Mac address) and its mode word (+0) -1.
void __sinit_ui_staffStatus_cpp() {
    gStaffStatusF44 = 0;
    gStaffStatusF40 = 0x10000;
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(&gStaffStatusPanel);
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(&gStaffStatusPanel));
    fields->mVtable.mOffset = 0x102af264;
    fields->mMode = -1;
}

int32_t UpgradeStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    UpgradeStatusPanel* panel = &gUpgradeStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(panel);
    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message < 0x15) {
        if (message == 0x10) {
            if (static_cast<uint32_t>(a) == 0x80080u) {
                CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
                object->GetDBInfo();
                if (fields->mF10.mOffset != 0) {
                    InterfaceWindow* countWindow = fields->mF10->GetWindow(0x5210);
                    if (countWindow != nullptr) {
                        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
                        id.mIndex = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(object) + 0xe);
                        int32_t count = gGameWorld->GetParkAnalyser()->NumberOfObjectInPark(id);
                        lender_callee::WindowSetValue(countWindow, count);
                    }
                    InterfaceWindow* valueWindow = fields->mF10->GetWindow(0x520e);
                    lender_callee::WindowSetValue(valueWindow, static_cast<int32_t>(object->GetCurrentResidualValue()));
                }
            }
        } else if (message < 0x10) {
            if (message == 5) {
                panel->Remove();
                return 1;
            }
        } else if (message >= 0x14) {
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x12d));
        }
    } else if (message == 0x100) {
        if (a == 0x520b) {
            base->GetNextObject();
        } else if (a < 0x520b) {
            if (a == 0x5209) {
                base->EraseObject();
                return 0;
            }
            if (a < 0x5209) {
                if (a == -1) {
                    panel->Remove();
                    return 0;
                }
            } else {
                base->GetPreviousObject();
            }
        } else {
            if (a == 0x5214) {
                base->Move();
                return 0;
            }
            if (a < 0x5214 && a >= 0x5213) {
                CreateAllItemsPanel(kUIObjRide, nullptr);
                return 0;
            }
        }
    } else if (message < 0x100 && message == 0x1e) {
        UIMain_TickRidePreview();
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}
