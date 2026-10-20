// Unit v5547: the sideshow ("show") status panel.
//   0x101c1848 ShowStatusPanel::UpdateStatusWindow (1036 bytes): the figures of the show (gStaffRoomPanelThing) in the panel's windows
//   0x101c2508 ShowStatusPanel::ApplyChanges (132 bytes): the panel's price, chance and prize to the thing
//   0x101c261c ShowStatusPanel::applytoallcallback (168 bytes): the same for every thing of the same type
//   0x101c2a38 ShowStatusPanel::Show (80 bytes)
// The panel has the UIStatusPanelFields layout and ints at +0x24 (chance), +0x28 (prize), +0x2c (price).
#include <cstdint>

#include "InterfaceWindow.h"
#include "RideDB.h"
#include "Text.h"
#include "TbITable.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"
#include "MainWindowCallback.h"

CThing* gShowStatusThing;                  // 0x10395a54
int16_t gShowStatusScript[2] = {0, 1};     // 0x102ae3e8

namespace {

struct ShowPanelFields : UIStatusPanelFields {
    int32_t mF24;   // 0x24 the chance of winning
    int32_t mF28;   // 0x28 the prize value
    int32_t mF2C;   // 0x2c the price per use
};

ShowPanelFields* Fields(ShowStatusPanel* panel) { return reinterpret_cast<ShowPanelFields*>(panel); }

// The runtime function at 0x10237cd8: a double cut to an integer (the low 32 bits of the 64-bit cut).
// Observed: a negative value (and NaN) answers 0; a larger one saturates like an unsigned conversion.
int32_t ToInt(double value) {
    if (!(value > 0.0)) return 0;
    if (value >= 4294967295.0) return -1;
    return static_cast<int32_t>(static_cast<uint32_t>(value));
}

// (value << 10) / 100 with a signed divide (mulhw by 0x51eb851f, srawi 5, plus the sign).
int32_t Percent(uint32_t value) { return static_cast<int32_t>(value << 10) / 100; }

void CallNotify(ShowPanelFields* f, void* self) {
    typedef uint64_t (*Fn)(void*);
    Ptr32<uint32_t> entry;
    entry.mOffset = f->mVtable.get()[0x2c / 4];
    (*reinterpret_cast<Fn*>(entry.get()))(self);
}

void Apply(CObject* object, ShowPanelFields* f) {
    object->SetPricePerUse(f->mF2C);
    if (object->GetDBInfo()->Value(kRideSlot_NonWinnableSideshow).mInteger == 0) {
        object->SetChanceOfWinning(static_cast<uint8_t>(f->mF24));
        object->SetPrizeValue(f->mF28);
    }
}

}  // namespace

void ShowStatusPanel::UpdateStatusWindow() {
    ShowPanelFields* f = Fields(this);
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    object->GetDBInfo();
    if (f->mF10.mOffset != 0) {
        InterfaceWindow* parent = f->mF10.get();
        // No test for NULL on this window: the Mac reads its table at once.
        InterfaceWindow* window = parent->GetWindow(0xa09d);
        double sum1;
        object->mHistory1A4.GetSumOfLast(sum1, 0x1e);
        ShowStatusTag tagA(0x19, ToInt(sum1));
        double sum2;
        object->mHistory22C.GetSumOfLast(sum2, 0x1e);
        int32_t walked = ToInt(sum2);
        double sum3;
        object->mHistory1A4.GetSumOfLast(sum3, 0x1e);
        ShowStatusTag tagB(0x1a, ToInt(sum3) + walked);
        TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[6]);
        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1cc, &tagA, &tagB);
        window->SetClientDrawText(gUIMainConvertBuffer);

        window = f->mF10.get()->GetWindow(0xa096);
        if (window != nullptr) window->SetValue(Percent(object->GetPercentageOfWinners()));

        window = f->mF10.get()->GetWindow(0xa093);
        if (window != nullptr) {
            double sum;
            object->mHistory6C.GetSumOfLast(sum, 0x1e);
            window->SetValue(ToInt(sum));
        }

        window = f->mF10.get()->GetWindow(0xa092);
        if (window != nullptr) window->SetValue(object->GetProfitability());

        window = f->mF10.get()->GetWindow(0xa099);
        if (window != nullptr) {
            int32_t level = object->GetProspectiveExcitementLevel(f->mF28, f->mF2C, f->mF24);
            window->SetValue(Percent(static_cast<uint32_t>(level) & 0xff));
        }

        window = f->mF10.get()->GetWindow(0xa094);
        if (window != nullptr) window->SetValue(Percent(static_cast<uint32_t>(object->GetCustomerSatisfaction())));

        // The next two windows are not tested for NULL either.
        window = f->mF10.get()->GetWindow(0xa0a0);
        window->SetValue(static_cast<int32_t>(object->GetCurrentResidualValue()));

        window = f->mF10.get()->GetWindow(0xa09e);
        tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell;
        cell.mIndex = static_cast<uint16_t>(object->mBase.mCellX + (object->mBase.mCellY << 7) + 1);
        int32_t happiness = gGameWorld->GetParkAnalyser()->GetAverageHappinessAroundPoint(cell, 1);
        window->SetValue(Percent(static_cast<uint32_t>(happiness)));
    }
    int32_t state = UIMain_GetObjectState(object, 2);
    if (state != f->mF04) {
        f->mF04 = state;
        CallNotify(f, this);
    }
}

void ShowStatusPanel::ApplyChanges() { Apply(reinterpret_cast<CObject*>(gStaffRoomPanelThing), Fields(this)); }

void ShowStatusPanel::applytoallcallback(CThing* thing) {
    if (reinterpret_cast<CObject*>(thing)->mObjectType == reinterpret_cast<CObject*>(gStaffRoomPanelThing)->mObjectType)
        Apply(reinterpret_cast<CObject*>(thing), Fields(this));
}

void ShowStatusPanel::Show(CThing* thing) {
    gShowStatusThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gShowStatusScript, ShowStatusMsg, 0xa0a2, 0xa091, 0xa0a3);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xd0));
}
