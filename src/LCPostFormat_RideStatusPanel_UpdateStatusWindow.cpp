// Unit v5191: the ride status panel's refresh of its figures, its buttons and its upgrade panel.
//   0x101b9ac4 RideStatusPanel::UpdateStatusWindow (936 bytes): writes the figures of the ride (gStaffRoomPanelThing) into the panel's windows and,
//                when the ride's state changed, tells the panel's table, redoes the buttons and the knobs; refreshes the upgrade list
//   0x101ba6e8 RideStatusPanel::ShowUpgradePanel (528 bytes): builds the upgrade panel (once) and fills its list
//   0x101b8cc8 RideStatusPanel::UpdateWhichButtons (992 bytes): shows, enables and sets the panel's buttons for the ride's state
#include <cstdint>

#include "History.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "RideDB.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "ppc_math.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

// 0x102ad4f0 (name invented): the window script of the upgrade panel. Only the first word (initial 0x00000001) is known.
int16_t gUpgradePanelScript[2] = {0, 1};

namespace {

// The figure windows' ids.
constexpr int32_t kIdHistory = 0x3e21;
constexpr int32_t kIdAge = 0x3e1b;
constexpr int32_t kIdExcitement = 0x3e16;
constexpr int32_t kIdReliability = 0x3e18;
constexpr int32_t kIdCondition = 0x3e19;
constexpr int32_t kIdLife = 0x3e17;
constexpr int32_t kIdResidual = 0x3e23;

// The Mac scales a figure 0 to 255 to a bar of 1000 or so: the low byte times 1024 over 100.
int32_t Scaled(uint32_t value) { return static_cast<int32_t>(((value & 0xffu) << 10) / 100u); }

// 0x10237cd8 (a runtime conversion of a double to a 32-bit integer): the low word of the truncated value.
int32_t SumToValue(double sum) { return static_cast<int32_t>(static_cast<uint32_t>(static_cast<int64_t>(sum))); }

}  // namespace

void RideStatusPanel::UpdateStatusWindow() {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    object->GetDBInfo();
    if (mStatusWindow != nullptr) {
        InterfaceWindow* window = mStatusWindow->GetWindow(kIdHistory);
        if (window != nullptr) {
            double sum;
            object->mHistory1A4.GetSumOfLast(sum, 0x1e);
            window->SetValue(SumToValue(sum));
        }
        window = mStatusWindow->GetWindow(kIdAge);
        if (window != nullptr) {
            int32_t age = object->GetObjectAge();
            int32_t negative = age < 0 ? 1 : 0;
            if (negative) age = -age;
            RideAgeTag tag(age, negative);
            char16_t text[32];
            TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[6]);
            table->FormatString(text, 0x1f, 0x1b1, &tag);
            window->SetClientDrawText(text);
        }
        window = mStatusWindow->GetWindow(kIdExcitement);
        if (window != nullptr) {
            int32_t excitement = object->GetProspectiveExcitementLevel(mSpeed, mDuration, mCapacity);
            window->SetValue(Scaled(static_cast<uint32_t>(excitement)));
        }
        window = mStatusWindow->GetWindow(kIdReliability);
        if (window != nullptr) {
            int32_t reliability = object->GetProspectiveReliability(static_cast<float>(mSpeed), static_cast<float>(mCapacity),
                                                                    static_cast<float>(mDuration));
            window->SetValue(Scaled(static_cast<uint32_t>(reliability)));
        }
        window = mStatusWindow->GetWindow(kIdCondition);
        if (window != nullptr) {
            int32_t condition = ppc_fctiwz(static_cast<double>(object->mF40));
            window->SetValue(Scaled(static_cast<uint32_t>(condition)));
        }
        window = mStatusWindow->GetWindow(kIdLife);
        if (window != nullptr) {
            int32_t life = object->GetRemainingLife();
            window->SetValue(Scaled(static_cast<uint32_t>(life)));
        }
        // No test for NULL here: the Mac reads the window's table at once.
        window = mStatusWindow->GetWindow(kIdResidual);
        uint32_t residual = object->GetCurrentResidualValue();
        window->SetValue(static_cast<int32_t>(residual));
    }
    int32_t state = UIMain_GetObjectState(object, 2);
    if (state != mState) {
        mState = state;
        mOps->Notify(this);
        UpdateWhichButtons();
        UpdateKnobsWindow();
    }
    if (mUpgradeWindow != nullptr) {
        uint16_t kind = object->mObjectType;
        InterfaceWindow* list = mUpgradeWindow->GetWindow(0x1f4d);
        FillUpgradeBox(kind, static_cast<InterfaceListControl2*>(list), 1, object);
    }
}

void RideStatusPanel::ShowUpgradePanel() {
    if (mUpgradeWindow != nullptr || mF08 != 0) return;
    mUpgradeWindow = mWindow->Build(gUpgradePanelScript, UpgradePanelMsg);
    reinterpret_cast<UIStatusPanel*>(this)->EnableNameClick(0);
    mUpgradeWindow->setZ(static_cast<int16_t>(mWindow->mZ + 0x11));
    mUpgradeWindow->setExtraParam(0, this);
    InterfaceTitleTextObject* title = new InterfaceTitleTextObject(nullptr);   // 0x19c bytes on the Mac; new never answers NULL
    mUpgradeWindow->GetWindow(0x1f4e)->SetClientDrawObject(title);
    InterfaceFont* font = GetFontTableEntry(5);
    mUpgradeWindow->GetWindow(0x1f4e)->SetFont(font);
    mUpgradeWindow->GetWindow(0x1f4e)->SetFontJust(1, 1);
    const char16_t* text = UIMain_GetString(0x1f);
    mUpgradeWindow->GetWindow(0x1f4e)->SetText(const_cast<char16_t*>(text));
    UI_TITLE_COLOUR(mUpgradeWindow->GetWindow(0x1f4e));
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(mUpgradeWindow->GetWindow(0x1f4d));
    list->SetColumnFormatsAll(ResearchListLCFormat, OnlineParkListPanelLCPostFormat);
    list->SetColumnType(0, kLCText);
    list->SetColumnType(1, kLCNumber);
    // The Mac passes the string at 0x1024e460 + 0x10: "hilight".
    list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    FillUpgradeBox(object->mObjectType, list, 0, object);
}

void RideStatusPanel::UpdateWhichButtons() {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    if (object == nullptr) return;
    RideInfo* info = object->GetDBInfo();
    if (mState == 3) {
        mWindow->GetWindow(0x3e38)->Show(0);
        mWindow->GetWindow(0x3e34)->Show(0);
        mWindow->GetWindow(0x3e2a)->Show(0);
        mWindow->GetWindow(0x3e36)->Show(0);
        mWindow->GetWindow(0x3e2c)->Show(0);
        mWindow->GetWindow(0x3e37)->Show(0);
        mWindow->GetWindow(0x3e35)->Show(0);
        return;
    }
    InterfaceWindow* window = mWindow->GetWindow(0x3e38);
    window->Show(1);
    if (mState == 0x19 || mState == 0x1a) {
        window->Enable(0);
    } else if (object->IsClosed() && !object->IsOpenable()) {
        window->Enable(0);
    } else if (!object->IsClosed() && !object->IsCloseable()) {
        window->Enable(0);
    } else {
        window->Enable(1);
        if (object->IsClosed()) {
            reinterpret_cast<InterfaceButton*>(window)->SetState(1, 1);
        } else {
            reinterpret_cast<InterfaceButton*>(window)->SetState(0, 1);
        }
    }
    mWindow->GetWindow(0x3e34)->Show(1);
    window = mWindow->GetWindow(0x3e2a);
    // Record slot 36 (+0x9c), the track type word.
    if (*reinterpret_cast<const int32_t*>(reinterpret_cast<const char*>(info) + 0x9c) == 0) {
        window->Show(0);
    } else {
        window->Show(1);
    }
    window = mWindow->GetWindow(0x3e36);
    window->Show(1);
    if (mState == 0x13 || mState == 0x15 || mState == 0x12 || mState == 0x14 || mState == 0x11) {
        reinterpret_cast<InterfaceButton*>(window)->SetState(1, 1);
    } else {
        reinterpret_cast<InterfaceButton*>(window)->SetState(0, 1);
    }
    mWindow->GetWindow(0x3e35)->Show(1);
    window = mWindow->GetWindow(0x3e2c);
    window->Show(1);
    if ((LLCC_IsRideMovable(object) & 0xff) == 0) {
        window->Enable(0);
    } else {
        window->Enable(1);
    }
    mWindow->GetWindow(0x3e37)->Show(1);
}
