// The ride status panel's upgrade list (unit v5074), main app ("SimTheme Park"):
//   FillUpgradeBox   0x101ba128 (880 bytes): fills a list control with the rides that can be built as an upgrade
//   UpgradePanelMsg  0x101ba4e0 (460 bytes): the upgrade panel's message handler
#include <cstdint>

#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "ObjectControl.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

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
