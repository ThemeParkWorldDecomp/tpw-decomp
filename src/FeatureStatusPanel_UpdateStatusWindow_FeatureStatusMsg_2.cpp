// Unit v5904: FeatureStatusMsg 0x1019eae4, __sinit_ui_FeatureStatus_cpp 0x1019eea0, GetMaxYValue 0x1019ef1c, KissMyLines 0x1019f25c.
#include <cstdint>
#include "FinanceGraphPanelMsg.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

namespace {

// The Mac's offsets of the eight histories in the park analyser, by selection bit (bit 0 first).
const uint32_t kFinanceHistoryOffset[8] = {0x1f100, 0x1f350, 0x1fc90, 0x1fee0, 0x20130, 0x20380, 0x1f5a0, 0x1f7f0};

CHistory<int32_t, 144>* FinanceHistory(uint8_t* analyser, int bit) {
    return reinterpret_cast<CHistory<int32_t, 144>*>(analyser + kFinanceHistoryOffset[bit]);
}

// The panel's table at +0x20: each slot holds the arena offset of a block whose first 8 bytes are the function (the host's virtual call).
void CallFeaturePanelSlot(UIStatusPanelFields* fields, uint32_t slot, void* self) {
    Ptr32<uint32_t> entry;
    entry.mOffset = fields->mVtable.get()[slot / 4];
    typedef uint64_t (*Fn)(void*);
    (*reinterpret_cast<Fn*>(entry.get()))(self);
}

}  // namespace

// 0x1019eae4 (676 bytes): the feature status panel's window handler. Everything that does not return falls to DefaultStatusPanelMsg.
int32_t FeatureStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    FeatureStatusPanel* panel = &gFeatureStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    UIStatusPanelFields* f = reinterpret_cast<UIStatusPanelFields*>(panel);
    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message == 0x10) {
        if (a == 0x80080) {
            CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
            object->GetDBInfo();
            if (f->mF10.mOffset != 0) {
                InterfaceWindow* countWindow = f->mF10->GetWindow(0x1586);
                if (countWindow != nullptr) {
                    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
                    id.mIndex = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(object) + 0xe);
                    int32_t count = gGameWorld->GetParkAnalyser()->NumberOfObjectInPark(id);
                    countWindow->SetValue(count);
                }
                InterfaceWindow* valueWindow = f->mF10->GetWindow(0x1584);
                // No test for NULL here: the Mac reads the window's table at once.
                valueWindow->SetValue(static_cast<int32_t>(object->GetCurrentResidualValue()));
            }
        }
    } else if (message == 5) {
        panel->Remove();
        return 1;
    } else if (message == 0x14) {
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x12a));
    } else if (message == 0x1e) {
        UIMain_TickRidePreview();
    } else if (message == 0x100) {
        if (a == -1) {
            panel->Remove();
            return 0;
        } else if (a == 0x157b) {
            base->EraseObject();
            return 0;
        } else if (a == 0x157c) {
            base->GetPreviousObject();
        } else if (a == 0x157d) {
            base->GetNextObject();
        } else if (a == 0x1580) {
            CreateAllItemsPanel(kUIObjFeature, nullptr);
            return 0;
        } else if (a == 0x1581) {
            reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GoFirstPerson(0);
            CallFeaturePanelSlot(f, 0x34, panel);
            return 0;
        } else if (a == 0x1582) {
            base->Move();
            return 0;
        }
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}

// 0x1019eea0 (76 bytes): gFeatureStatusPanel's constructor, table 0x102a9ba0 at +0x20, mode 3.
void __sinit_ui_FeatureStatus_cpp() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(&gFeatureStatusPanel);
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(&gFeatureStatusPanel));
    fields->mVtable.mOffset = 0x102a9ba0;
    fields->mMode = 3;
}

// 0x1019ef1c (380 bytes): the largest value of the selected histories.
int32_t GetMaxYValue() {
    int32_t value = 0;
    uint8_t* analyser = reinterpret_cast<uint8_t*>(gGameWorld->GetParkAnalyser());
    for (int bit = 0; bit < 8; bit++)
        if (gFinanceGraphFlags & (1u << bit)) MaxAnalyserValue(FinanceHistory(analyser, bit), value, gFinanceGraphMode);
    return value;
}

// 0x1019f25c (424 bytes): a line for each selected history; bit 0 takes the last colour of gFinancePanelColours.
void KissMyLines(CUIGrapher& grapher) {
    uint8_t* analyser = reinterpret_cast<uint8_t*>(gGameWorld->GetParkAnalyser());
    for (int bit = 0; bit < 8; bit++)
        if (gFinanceGraphFlags & (1u << bit))
            GraphLine(grapher, FinanceHistory(analyser, bit), *reinterpret_cast<TbColour*>(gFinancePanelColours[7 - bit]), 1,
                      gFinanceGraphMode);
}
