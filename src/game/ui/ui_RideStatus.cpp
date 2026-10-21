// ui_RideStatus.cpp: name proven by __sinit_ui_RideStatus_cpp in the main application.

#include "Debug.h"
#include "email.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "helpers/ui_RideStatusHelpers.h"
#include "History.h"
#include "InterfaceFont.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "ObjectControl.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPEngine.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_Postcard.h"
#include "WindowScripts.inc"
#include "World.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>
#include <string>

// Unit v5191: the ride status panel's refresh of its figures, its buttons and its upgrade panel.
//   0x101b9ac4 RideStatusPanel::UpdateStatusWindow (936 bytes): writes the figures of the ride (gStaffRoomPanelThing) into the panel's windows and,
//                when the ride's state changed, tells the panel's table, redoes the buttons and the knobs; refreshes the upgrade list
//   0x101ba6e8 RideStatusPanel::ShowUpgradePanel (528 bytes): builds the upgrade panel (once) and fills its list
//   0x101b8cc8 RideStatusPanel::UpdateWhichButtons (992 bytes): shows, enables and sets the panel's buttons for the ride's state

// 0x102ad4f0 (name invented): the window script of the upgrade panel.
int16_t gUpgradePanelScript[] = {TPW_WINDOW_SCRIPT_102AD4F0};   // 0x102ad4f0: the image's words (generated, devtools/gen_game_tables.py)

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
    // Record slot 36 (the Mac's +0x9c), the track type word, by name: the host's record is laid out wider (a vtable and 8-byte slots).
    if (info->Value(kRideSlot_WhichTrackType).mInteger == 0) {
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

// Unit v51121: the small functions of ui_ridepanel.cpp (RideStatusPanel, ShopStatusPanel), ui_StaffCostsPanel.cpp, the research panel and
// the tank panel message function.

void RideStatusPanel::InitViewWindow() { InitView(this); }            // 0x101b90e4
void RideStatusPanel::DeInitViewWindow() { UIMain_DestroyRidePreview(); }   // 0x101b9208
void RideStatusPanel::UpdateViewWindow() { ShowPreview(this); }       // 0x101b9264

// RideStatusPanel::Condemned (0x101bc2f0) and RideStatusPanel::InitStatusWindow (0x101b9340), main app.

namespace {

// new, then the constructor (the Mac tests the answer of new for NULL, which never happens).
template <class F>
InterfaceDrawObject* Make(uint32_t size, F construct) {
    void* memory = lender_callee::OperatorNew(size);
    construct(memory);
    return static_cast<InterfaceDrawObject*>(memory);
}

InterfaceWindow* TextWindow(InterfaceWindow* root, int32_t id, int32_t string) {
    InterfaceWindow* w = root->GetWindow(id);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::SpriteCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    w->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(string)));
    return w;
}

void Gradient(InterfaceWindow* root, int32_t id) {
    InterfaceWindow* w = root->GetWindow(id);
    UIMain_IndentClient(w, 0, 10);
    // 0x1024e460: "ridestatbar.wct"
    w->SetClientDrawObject(Make(0x124, [](void* m) { vsp_callee::GradientCtor(m, const_cast<char*>("ridestatbar.wct")); }));
    lender_callee::WindowSetValue(w, 0);
}

}  // namespace

void RideStatusPanel::InitStatusWindow() {
    if (mStatusWindow == nullptr) return;
    InterfaceWindow* root = mStatusWindow;
    TextWindow(root, 0x3e20, 0x11);
    TextWindow(root, 0x3e1a, 0x12);
    TextWindow(root, 0x3e1c, 0x13);
    TextWindow(root, 0x3e1e, 0x14);
    TextWindow(root, 0x3e1f, 0x15);
    TextWindow(root, 0x3e1d, 0x16);
    TextWindow(root, 0x3e22, 0x17);

    InterfaceWindow* w = root->GetWindow(0x3e21);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::NumberCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    lender_callee::WindowSetValue(w, 0);

    w = root->GetWindow(0x3e1b);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::SpriteCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    w->SetClientDrawText(nullptr);

    Gradient(root, 0x3e16);
    Gradient(root, 0x3e18);
    Gradient(root, 0x3e19);
    Gradient(root, 0x3e17);

    w = root->GetWindow(0x3e23);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::CashCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    lender_callee::WindowSetValue(w, 0);
}

void RideStatusPanel::DeInitStatusWindow() {}                         // 0x101b9a84

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
        UIStatusPanel_CallSlot(this, 0x2c);
        UpdateWhichButtons();
        UpdateKnobsWindow();
    }
    if (mUpgradeWindow != nullptr) {
        uint16_t kind = object->mObjectType;
        InterfaceWindow* list = mUpgradeWindow->GetWindow(0x1f4d);
        FillUpgradeBox(kind, static_cast<InterfaceListControl2*>(list), 1, object);
    }
}

// The ride status panel's upgrade list (unit v5074), main app ("SimTheme Park"):
//   FillUpgradeBox   0x101ba128 (880 bytes): fills a list control with the rides that can be built as an upgrade
//   UpgradePanelMsg  0x101ba4e0 (460 bytes): the upgrade panel's message handler

uint32_t gParkListFontIndex = 7;   // 0x102ad5dc

// The online park list's row format (LCPostFormat of ui_onlineParkListPanel.cpp, 0x101b9fb8, 316 bytes): the row gets a text draw object, the font
// and white text; its first child a cash text object, the font, right/middle justification and (the virtual at vtable offset 0x24) SetValue(0).
void OnlineParkListPanelLCPostFormat(InterfaceWindow* window) {
    InterfaceTextObjectSprite* text = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(text);
    window->SetFont(GetFontTableEntry(gParkListFontIndex));
    window->SetFontColour(0xff, 0xff, 0xff, 0xff);
    InterfaceWindow* child = window->GetChild(0);
    InterfaceNullCashTextObject* cash = new InterfaceNullCashTextObject(nullptr);
    child->SetClientDrawObject(cash);
    child->SetFont(GetFontTableEntry(gParkListFontIndex));
    child->SetFontJust(2, 1);
    child->SetValue(0);
}

namespace {

// 0x1024e55e: the delimiter string of wcstok, one newline (the word there is 0x000a0000: the halfword at the odd address).
const char16_t kLineDelimiter[] = u"\n";

constexpr int32_t kNeedsUpgradeData = -0x32c7;   // the data word of the "this ride needs an upgrade" line
constexpr int32_t kNoUpgradeData = -0x1ed2;      // the data word of the lines of the text shown when nothing is on offer

}  // namespace

// The upgrade list: `kind` is the value of the ride record word at +0x284 the rides must have (0x9e'th balance value); with `clear` the list is emptied first.
// Rides of that kind, not failed to load and with a researched control are listed (data: the ride type, second column: the upgrade cost), unless
// the game type is the easy game, which lists nothing. Then the object's own upgrade: when it wants one, the "needs an upgrade" line, else the next level
// (if the control has it). When nothing was listed the explanation (interface string 0x1a, 0x1b or 0x1c) is split into lines.
void FillUpgradeBox(int32_t kind, InterfaceListControl2* list, int32_t clear, CObject* object) {
    if (clear != 0) list->RemoveAllItems();
    RideInfo* ride = gRideDatabase.GetFirstRide();
    uint32_t level = object->mF4C;
    int32_t count = 0;
    if (TheGameType().mType != kGameTypeEasy) {
        for (; ride != nullptr; ride = gRideDatabase.GetNextRide()) {
            if (ride->mLoadFailed != 0) continue;
            if (ride->Value(static_cast<RideBalanceSlot>((0x284 - 0xc) / 4)).mInteger != kind) continue;
            ObjectControl* control = gGameWorld->GetControlManager()->GetObjectControl(
                tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{ride->mId}, 0);
            if (control->mAvailable == 0) continue;
            gRideDatabase.GetTypeName(ride->mId, gUIMainConvertBuffer, 0x3fff);
            UI_LCAdd item[2] = {};
            item[0].mText = gUIMainConvertBuffer;
            item[1].mInt = control->mUpgradeCost;
            list->AddItem(item, ride->mId, -1, 0);
            count++;
        }
        RideInfo* info = object->GetDBInfo();
        if (object->RequiresUpgrade() != 0) {
            TbIntegerVTag tag(0, static_cast<int32_t>(level) + 2);
            static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1e, &tag);
            UI_LCAdd item[2] = {};
            item[0].mText = gUIMainConvertBuffer;
            list->AddItem(item, kNeedsUpgradeData, -1, 0);
            count = 1;
        } else if (static_cast<int32_t>(level) < 2) {
            level++;
            ObjectControl* control = gGameWorld->GetControlManager()->GetObjectControl(
                tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{object->mObjectType}, 0);
            if (static_cast<int32_t>(level) <= control->mUpgradesAvailable) {
                TbIntegerVTag tag(0, static_cast<int32_t>(level) + 1);
                static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1d, &tag);
                UI_LCAdd item[2] = {};
                item[0].mText = gUIMainConvertBuffer;
                // the upgrade cost of the new level: the record's slot 107 of that level (0x40 bytes each)
                item[1].mInt = info->Value(static_cast<RideBalanceSlot>((0x1b8 - 0xc) / 4 + 16 * level)).mInteger;
                list->AddItem(item, -static_cast<int32_t>(level), -1, 0);
                count++;
            } else {
                level--;
            }
        }
    }
    if (count == 0) {
        if (TheGameType().mType == kGameTypeEasy) UIMain_GetString(0x1b);
        else if (level == 2) UIMain_GetString(0x1c);
        else UIMain_GetString(0x1a);
        UI_LCAdd item[2] = {};
        char16_t* token = wcstok(gUIMainConvertBuffer, kLineDelimiter, nullptr);
        while (token != nullptr) {
            item[0].mText = token;
            list->AddItem(item, kNoUpgradeData, -1, 0);
            token = wcstok(nullptr, kLineDelimiter, nullptr);
        }
    }
    list->UpdateDisplay();
}

// The upgrade panel's messages: 0x14 (a close notice) tells the panel to go away and is then handled by default; 0x100 with -2 posts message 4;
// 0x400 (a list line chosen: a is the list's id, b the position) buys the upgrade the line names; everything else is the default handler's.
int32_t UpgradePanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    RideStatusPanel* panel = &gRideStatusPanel;
    if (message == 0x100) {
        if (a == -2) {
            UI_SendMessage(window, 4, 0, 0);
            return 0;
        }
    } else if (message < 0x100) {
        if (message != 0x15 && message < 0x15 && message > 0x13) {
            static_cast<RideStatusPanel*>(window->getExtraParam(0))->UpgradePanelByeBye();
        }
    } else if (message == 0x400) {
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
        if (list != nullptr && b != -1) {
            int32_t data;
            list->GetData(b, &data);
            if (data != kNoUpgradeData) {
                if (data == kNeedsUpgradeData) {
                    panel->Upgrade(0, true);
                    UI_SendMessage(window, 4, 0, 0);
                    panel->UpdateStatusWindow();
                    return 0;
                }
                if (panel->Upgrade(data, false) != 0) {
                    if (data > 0) {
                        panel->Remove();
                        return 0;
                    }
                }
                UI_SendMessage(window, 4, 0, 0);
                panel->UpdateStatusWindow();
                return 0;
            }
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
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

// 0x101ba930.
void RideStatusPanel::UpgradePanelByeBye() {
    mUpgradeWindow = nullptr;
    reinterpret_cast<UIStatusPanel*>(this)->EnableNameClick(1);
}

// Unit v51011: RideStatusPanel::InitWindow 0x101ba99c, UpdateWindow 0x101baaac, InitKnobsWindow 0x101bab6c, UpdateKnobsTitles 0x101baf28.

// The Mac's string table at 0x1024e460: "hilight" at +0x10, "smallguage.wct" at +0x18 (the gradients' mesh).
const char gRideStatusMeshNames[0x28] = {0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
                                         'h', 'i', 'l', 'i', 'g', 'h', 't', 0,   's', 'm', 'a', 'l', 'l', 'g', 'u', 'a',
                                         'g', 'e', '.', 'w', 'c', 't', 0,   0};

// 0x101ba99c: the object list (the two arrows go when there is less than a second object), the knobs, the window, the edit box.
void RideStatusPanel::InitWindow() {
    int32_t count = reinterpret_cast<UIStatusPanel*>(this)->FillObjectList(gRideStatusShowThing);
    if (count < 2) {
        mWindow->GetWindow(0x3e26)->Show(0);
        mWindow->GetWindow(0x3e27)->Show(0);
    }
    InitKnobsWindow();
    mWindow->Show(1);
    reinterpret_cast<UIStatusPanel*>(this)->InitEditBox(0x3e28);
}

void RideStatusPanel::DeInitWindow() {}                               // 0x101baa74

// 0x101baaac: the name, the two ops-table calls (slots at +0x20 and +0x1c, the panel as argument), the knobs, the buttons.
void RideStatusPanel::UpdateWindow() {
    parkstatus_callee::WindowSetText(mNameWindow, UIMain_GetObjectName(reinterpret_cast<CObject*>(gStaffRoomPanelThing)));
    UIStatusPanel_CallSlot(this, 0x20);
    UIStatusPanel_CallSlot(this, 0x1c);
    UpdateKnobsWindow();
    UpdateWhichButtons();
}

// 0x101bab6c: the knobs' gradients and texts. The stack CoolRect is built and destroyed whether or not there is a window.
void RideStatusPanel::InitKnobsWindow() {
    alignas(8) uint8_t rect[0x20];
    kidlist_callee::RectCtor(rect);
    if (mWindow != nullptr) {
        const char* mesh = gRideStatusMeshNames + 0x18;
        // the capacity bar (0x3e2d) and the speed bar (0x3e30): range 0 to 0x78, a gradient on the marker (0x3e2e) that starts at 0
        for (int32_t id : {0x3e2d, 0x3e30}) {
            InterfaceWindow* bar = mWindow->GetWindow(id);
            static_cast<InterfaceScrollBar*>(bar)->SetMinMax(0, 0x78);
            void* gradient = parkstatus_callee::OperatorNew(0x124);
            if (gradient != nullptr) parkstatus_callee::GradientCtor(gradient, mesh);
            bar->GetWindow(0x3e2e)->SetClientDrawObject(static_cast<InterfaceDrawObject*>(gradient));
            parkstatus_callee::WindowSetValue(bar->GetWindow(0x3e2e), 0);
        }
        // the duration bar
        static_cast<InterfaceScrollBar*>(mWindow->GetWindow(0x3e2f))->SetMinMax(0, 10);
        // the titles under the knobs
        for (int32_t id : {0x3e31, 0x3e33, 0x3e32}) {
            InterfaceWindow* title = mWindow->GetWindow(id);
            void* sprite = parkstatus_callee::OperatorNew(0x19c);
            if (sprite != nullptr) parkstatus_callee::SpriteCtor(sprite);
            title->SetClientDrawObject(static_cast<InterfaceDrawObject*>(sprite));
            title->SetFontJust(1, 1);
            title->SetFont(GetFontTableEntry(6));
            title->SetFontColour(0, 0, 0, 0xff);
            title->SetClientDrawText(nullptr);
        }
    }
    kidlist_callee::RectDtor(rect);
}

// 0x101baf28: bit 1 the speed title (0x3e33), bit 2 the capacity title (0x3e31), bit 4 the duration title (0x3e32). Each formats a string
// of the text table (gText + 0x564) into the shared buffer with a one-number tag (the value without its sign, a flag for negative).
void RideStatusPanel::UpdateKnobsTitles(int32_t which) {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[6]);
    char16_t* text = gUIMainConvertBuffer;
    auto format = [&](uint32_t string, uint32_t tagId, int32_t value) {
        int32_t negative = value < 0 ? 1 : 0;
        RideKnobTag tag(tagId, negative ? -value : value, negative);
        table->FormatString(text, 0x3fff, string, &tag);
    };
    if (which & 1) {
        InterfaceWindow* window = mWindow->GetWindow(0x3e33);
        format(0x1aa, 5, mSpeed);
        window->SetClientDrawText(text);
    }
    if (which & 2) {
        InterfaceWindow* window = mWindow->GetWindow(0x3e31);
        if (object->GetDBInfo()->mBalance.mValues[36].mInteger == 3) {
            format(0x1cf, 6, mCapacity);
        } else if (object->GetDBInfo()->mBalance.mValues[36].mInteger == 2) {
            format(0x1cd, 6, mCapacity);
        } else {
            format(0x1ab, 6, mCapacity);
        }
        window->SetClientDrawText(text);
    }
    if (which & 4) {
        InterfaceWindow* window = mWindow->GetWindow(0x3e32);
        int32_t unit = object->GetDBInfo()->mBalance.mValues[25].mInteger;
        if (unit == 2) {
            window->Show(1);
            format(0x1ae, 7, mDuration);
        } else if (unit < 2) {
            if (unit == 0) {
                window->Show(0);
            } else if (unit >= 0) {
                window->Show(1);
                format(mDuration == 1 ? 0x1ac : 0x1ad, 7, mDuration);
            }
        } else if (unit == 4) {
            window->Show(1);
            format(0x1b0, 7, mDuration);
        } else if (unit < 4) {
            window->Show(1);
            format(0x1af, 7, mDuration);
        }
        window->SetClientDrawText(text);
    }
}

// Unit v5914: the ride status panel's knobs and buttons.
//   0x101bbb88 RideStatusPanel::Upgrade (448 bytes): pays for an upgrade (flag 1: the ride's own, asked of the object at once; a negative `what`: the
//              ride's upgrade of that level; else builds the object type `what`, when the bank balance covers its control's upgrade cost)
//   0x101bbacc RideStatusPanel::ApplyChanges (136 bytes)
//   0x101bb9b8 RideStatusPanel::DoDial (228 bytes)
//   0x101bb71c RideStatusPanel::Edit (624 bytes)
//   0x101bc0bc RideStatusPanel::Show (176 bytes)
//   0x101bb494 RideStatusPanel::UpdateKnobsWindow (588 bytes)

// 0x1039599c (bss) and 0x102acef0 (the window script).
CThing* gRideStatusShowThing;
int16_t gRideStatusScript[] = {TPW_WINDOW_SCRIPT_102ACEF0};   // 0x102acef0: the image's words (generated, devtools/gen_game_tables.py)

namespace allitems_post_callee {
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
}  // namespace allitems_post_callee

namespace {

// The ride record's value of a slot (the Mac's offset is 0xc + 4 * slot).
int32_t RideValue(RideInfo* info, int32_t slot) { return info->Value(static_cast<RideBalanceSlot>(slot)).mInteger; }

// gMouse is made on first use (the same code is inlined everywhere in the original).
void EnsureMouse() {
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse;   // 1 byte on the Mac; new never answers NULL
        CMMDefault* mode = new CMMDefault();
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
}

}  // namespace

// 0x101bb494.
void RideStatusPanel::UpdateKnobsWindow() {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    RideInfo* info = object->GetDBInfo();
    if (mWindow == nullptr) return;
    InterfaceWindow* window = mWindow->GetWindow(0x3e30);
    if (window != nullptr) {   // the speed bar
        int32_t low = RideValue(info, 74);
        int32_t high = RideValue(info, 75);
        static_cast<InterfaceScrollBar*>(window)->SetMinMax(low, high);
        allitems_post_callee::WindowSetValue(window, static_cast<int32_t>(object->mF54));
        uint8_t level = object->mF4C;
        InterfaceWindow* marker = window->GetWindow(0x3e2e);
        allitems_post_callee::WindowSetValue(marker, (RideValue(info, 104 + 16 * level) << 10) / (high - low));
    }
    window = mWindow->GetWindow(0x3e2f);
    if (window != nullptr) {   // the duration bar
        int32_t unit = RideValue(info, 25);
        *reinterpret_cast<int32_t*>(mF28) = unit;
        window->Show(unit == 0 ? 0 : 1);
        static_cast<InterfaceScrollBar*>(window)->SetMinMax(RideValue(info, 72), RideValue(info, 73));
        allitems_post_callee::WindowSetValue(window, object->mF58);
    }
    window = mWindow->GetWindow(0x3e2d);
    if (window != nullptr) {   // the capacity bar
        int32_t low = RideValue(info, 70);
        int32_t high = RideValue(info, 71);
        if (low == high) {
            window->Show(0);
            mWindow->GetWindow(0x3e31)->Show(0);
        } else {
            window->Show(1);
            mWindow->GetWindow(0x3e31)->Show(1);
            uint8_t level = object->mF4C;
            InterfaceWindow* marker = window->GetWindow(0x3e2e);
            allitems_post_callee::WindowSetValue(marker, (RideValue(info, 100 + 16 * level) << 10) / (high - low));
        }
        static_cast<InterfaceScrollBar*>(window)->SetMinMax(low, high);
        allitems_post_callee::WindowSetValue(window, object->mF59);
    }
    UpdateKnobsTitles(7);
}
// 0x101bb71c.
void RideStatusPanel::Edit(int32_t on) {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    UI_SetSelectedObject(object);
    LLCM_SetCurrentObject(true);
    if (on == 0) {
        CMMPathDraw* draw = new CMMPathDraw(static_cast<CELL_MODES>(0x14));
        EnsureMouse();
        gMouse->AssignMouseMode(draw);
        LLCM_LeftMouseButtonUp(0, 0, 0, 0);
        UIStatusPanel_CallSlot(this, 0x34);
        return;
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type = {object->mObjectType};
    RideInfo* info = gRideDatabase.GetRideInfo(type);
    int32_t track = RideValue(info, 36);
    int32_t mode;
    if (track == 2) {
        mode = 0x13;
    } else if (track < 2) {
        mode = track >= 1 ? 0xe : 0;
    } else if (track < 4) {
        CoasterEditMode(object);
        UIStatusPanel_CallSlot(this, 0x34);
        return;
    } else {
        mode = 0;
    }
    CMMPathDraw* draw = new CMMPathDraw(static_cast<CELL_MODES>(mode));
    LLCC_SystemSetRideTrack(RideValue(info, 36));
    EnsureMouse();
    gMouse->AssignMouseMode(draw);
    LLCM_LeftMouseButtonUp(0, 0, 0, 0);
    UIStatusPanel_CallSlot(this, 0x34);
}

// 0x101bb9b8. The Mac's table slot at +0x1c (UpdateStatusWindow) after each knob.
void RideStatusPanel::DoDial(int32_t a, int32_t b) {
    reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GetDBInfo();
    if (a == 0x3e2f) {
        mDuration = b;
        UIStatusPanel_CallSlot(this, 0x1c);
        UpdateKnobsTitles(4);
    } else if (a < 0x3e2f) {
        if (a == 0x3e2d) {
            mCapacity = b;
            UIStatusPanel_CallSlot(this, 0x1c);
            UpdateKnobsTitles(2);
        }
    } else if (a < 0x3e31) {
        mSpeed = b;
        UIStatusPanel_CallSlot(this, 0x1c);
        UpdateKnobsTitles(1);
    }
}

// 0x101bbacc.
void RideStatusPanel::ApplyChanges() {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    object->SetOperatingSpeed(static_cast<uint32_t>(mSpeed));
    object->SetOperatingCapacity(static_cast<uint8_t>(mCapacity));
    if (RideValue(object->GetDBInfo(), 25) != 0) object->SetOperatingDuration(static_cast<uint8_t>(mDuration));
}

// 0x101bbb88. `flag` is the Mac's byte, compared with 1.
int32_t RideStatusPanel::Upgrade(int32_t what, bool flag) {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    CBankAccount* account = gGameWorld->GetBankAccount();
    int32_t balance = account->GetBalance();
    if (flag) {
        object->RequestUpgrade(0);
        return 1;
    }
    if (what < 0) {
        RideInfo* info = object->GetDBInfo();
        // the record's CostOfUpgrade of level -what (slot 107, 16 slots a level)
        if (balance > RideValue(info, 107 + 16 * (-what))) {
            object->RequestUpgrade(1);
            return 0;
        }
        return 0;
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type = {static_cast<uint16_t>(what)};
    ObjectControl* control = gGameWorld->GetControlManager()->GetObjectControl(type, 0);
    if (balance >= control->mUpgradeCost) {
        CMMPathDraw* draw = new CMMPathDraw(static_cast<CELL_MODES>(4), type);
        EnsureMouse();
        gMouse->AssignMouseMode(draw);
        UI_SetSelectedObject(object);
        LLCM_SetCurrentObject(true);
        return 1;
    }
    UIMain_NotEnoughDough();
    return 0;
}

// 0x101bbd78 (780 bytes): the handler of the ride status panel's window. Messages not handled here go to DefaultStatusPanelMsg.
int32_t RideStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    RideStatusPanel* panel = &gRideStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    if (message != 0x1d) {
        if (message < 0x1d) {
            if (message == 0x10) {
                if (a == 0x80080) panel->UpdateStatusWindow();
            } else if (message < 0x10) {
                if (message == 5) {
                    panel->Remove();
                    return 1;
                }
            } else if (message == 0x15) {
                if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
            } else if (message < 0x15 && message >= 0x14) {
                UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x129));
                UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xcf));
            }
        } else if (message == 0x100) {
            if (a == 0x3e2b) {
                base->EraseObject();
                return 0;
            }
            if (a < 0x3e2b) {
                if (a == 0x3e27) {
                    base->GetNextObject();
                } else if (a < 0x3e27) {
                    if (a >= 0) {
                        if (a >= 0x3e26) base->GetPreviousObject();
                    } else if (a >= -2) {
                        panel->Remove();
                        return 0;
                    }
                } else {
                    if (a == 0x3e29) {
                        CreateAllItemsPanel(kUIObjRide, gStaffRoomPanelThing);
                        return 0;
                    }
                    if (a > 0x3e29) {
                        panel->Edit(1);
                        return 0;
                    }
                }
            } else {
                if (a == 0x3e36) {
                    if (b == 1) base->RequestMaintenance(false);
                    else base->RequestMaintenance(true);
                    return 0;
                }
                if (a < 0x3e36) {
                    if (a == 0x3e34) {
                        panel->Edit(0);
                        return 0;
                    }
                    if (a > 0x3e34) {
                        panel->ShowUpgradePanel();
                        return 0;
                    }
                    if (a < 0x3e2d) {
                        base->Move();
                        return 0;
                    }
                } else if (a == 0x3e38) {
                    if (b == 1) base->CloseObject(1);
                    else base->CloseObject(0);
                    panel->UpdateStatusWindow();
                } else if (a < 0x3e38) {
                    reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GoFirstPerson(0);
                    UIStatusPanel_CallSlot(panel, 0x34);
                    return 0;
                }
            }
        } else if (message < 0x100) {
            if (message < 0x1f) UIMain_TickRidePreview();
        } else if (message == 0x800) {
            panel->DoDial(a, b);
        }
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}

// 0x101bc0bc.
void RideStatusPanel::Show(CThing* thing) {
    gRideStatusShowThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gRideStatusScript, RideStatusMsg, 0x3e24, 0x3e15, 0x3e25);
    mUpgradeWindow = nullptr;
    // the condition as an int, its low byte: 1 to 0x14 is the "needs attention" message
    uint32_t condition = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(reinterpret_cast<CObject*>(thing)->mF40))) & 0xffu;
    if (condition <= 0x14 && condition != 0) {
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x129));
        return;
    }
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xcf));
}

void RideStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }   // 0x101bc1a0

// 0x101bc1f4: the body of Condemned.
void RideStatusPanel::BreakDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    if (mWindow == nullptr) return;
    // 0x1024e487
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    void* thing = gThingArray[id.mIndex].mThing;
    if (thing != nullptr && thing == static_cast<void*>(gStaffRoomPanelThing)) UIStatusPanel_CallSlot(this, 0x1c);
}

void RideStatusPanel::Condemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    if (mWindow == nullptr) return;
    // 0x1024e487
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    void* thing = gThingArray[id.mIndex].mThing;
    if (thing != nullptr && thing == static_cast<void*>(gStaffRoomPanelThing)) UIStatusPanel_CallSlot(this, 0x1c);
}

// ui_RideStatus.cpp static initialiser (0x101bc3ec, 80 bytes).

// UIStatusPanel::UIStatusPanel (0x101929e8) is not translated yet: an empty weak stand-in; the unit test
// defines its own recording fake.
__attribute__((weak)) void UIStatusPanel_Construct(UIStatusPanel*) {}

// 0x101bc3ec (80 bytes)
void __sinit_ui_RideStatus_cpp() {
    RideStatusPanel* panel = &gRideStatusPanel;
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(panel));
    panel->mVtable.set(RideStatusPanelTable());
    panel->mF00[0] = 0; panel->mF00[1] = 0; panel->mF00[2] = 0; panel->mF00[3] = 0;
    panel->mF28[0] = 0; panel->mF28[1] = 0; panel->mF28[2] = 0; panel->mF28[3] = 0;
}

namespace postcard_callee {
// The Mac allocates the sizes of its objects; the host's are larger (InterfaceTextObjectSprite 0x250 bytes for the Mac's 0x19c), so the block is
// at least 0x400 bytes.
__attribute__((weak)) void* OperatorNew(uint32_t size) { return ::operator new(std::max<size_t>(size, 0x400)); }
}  // namespace postcard_callee

// 0x103959a0 (name invented): the research list's row height, worked out by the first ResearchListLCFormat (0 until then).
int32_t gResearchRowHeight = 0;

// The Mac calls a virtual through the font entry's table here (__ptr_glue, r3 = the font); the class is not recovered, so the height
// getter stands in. The test library replaces this function with a recording fake.
__attribute__((weak)) int16_t PostcardFontVirtual(InterfaceFont* font) { return static_cast<int16_t>(font->GetHeight()); }

// 0x101b9ea8: LCFormat of the research list file: the row height is worked out once.
InterfaceWindow* ResearchListLCFormat() {
    if (gResearchRowHeight == 0) {
        int32_t height = PostcardFontVirtual(GetFontTableEntry(gParkListFontIndex));
        gResearchRowHeight = height * 0x600 / gUIRealScreenHeight + 6;   // QUIRK: no check for a zero screen height
    }
    void* memory = postcard_callee::OperatorNew(0x138);
    InterfaceWindow* window = static_cast<InterfaceWindow*>(memory);
    if (memory != nullptr) new (memory) InterfaceWindow(nullptr, 1, 0, 0, 0, 100, static_cast<int16_t>(gResearchRowHeight));
    if (window != nullptr) window->CreateObject(1, 1, 1, 100, 0, 200, static_cast<int16_t>(gResearchRowHeight));
    return window;
}
