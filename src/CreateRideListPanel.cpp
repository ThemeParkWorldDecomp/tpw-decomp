// CreateRideListPanel 0x101b8434 (1936 bytes, unit v5010): the ride list panel. When the panel is up it is only refilled and the list
// and the type buttons are put right; otherwise it is built from the script, its title, picture, text and list are set up, and the
// advisor is told (message 0xbc).
#include <cstdint>

#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "WindowScripts.inc"

// The globals no other file names (BSS, and the data the code reads).
int32_t gRideListType = 0;              // 0x10395994
InterfaceWindow* gRideListPanel = nullptr;   // 0x10395998
uint16_t gRideListNoneA = 0;            // 0x102aceec
uint16_t gRideListNoneB = 0;            // 0x102aceee
int32_t gRideListButtonFont = 8;        // 0x102acedc
int32_t gRideListSortColumn = 1;        // 0x102acee4
int16_t gRideListScript[] = {TPW_WINDOW_SCRIPT_102ACA9C};   // 0x102aca9c: the image's words (generated, devtools/gen_game_tables.py)
int32_t gRideListF90 = 0;               // 0x10395990
int16_t gRideListF8C = 0;               // 0x1039598c
int16_t gRideListF8A = 0;               // 0x1039598a
int16_t gRideListF88 = 0;               // 0x10395988

namespace {

// The type buttons (window 0x1f9): the check button of the type is made current (type 2 0x1fa, 0 0x1fb, 1 0x1fd, 3 0x1fc, others none).
void SetTypeButton(InterfaceWindow* buttons, int32_t type) {
    if (buttons == nullptr) return;
    InterfaceCheckButtons* check = static_cast<InterfaceCheckButtons*>(buttons);
    if (type == 2) {
        check->SetCurrent(0x1fa);
    } else if (type < 2) {
        if (type == 0) check->SetCurrent(0x1fb);
        else if (type >= 0) check->SetCurrent(0x1fd);
    } else if (type < 4) {
        check->SetCurrent(0x1fc);
    }
}

// The list shows the entry with the selected id (a 16-bit value, compared as unsigned) unless it is the "none" id.
void SelectEntry(InterfaceListControl2* list, uint16_t selected, uint16_t none) {
    if (selected == none) return;
    int32_t position = list->FindData(static_cast<int32_t>(selected));
    if (position >= 0) list->SetCurrent(position);
}

}  // namespace

void CreateRideListPanel(UIObjType type, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> selectedTag) {
    int32_t kind = static_cast<int32_t>(type);
    const uint16_t selected = selectedTag.mIndex;
    SetLastPanel(1, 1);
    if (gRideListPanel != nullptr) {
        if (kind == -1) kind = gRideListType;
        FillRideList(static_cast<UIObjType>(kind), 1);
        InterfaceWindow* window = gRideListPanel->GetWindow(0x1f8);
        if (window != nullptr) {
            InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window);
            SetTypeButton(list->GetWindow(0x1f9), kind);
            SelectEntry(list, selected, gRideListNoneA);
        }
        return;
    }

    RemoveSlideoutPanel(1);
    UIMain_CheckPanel();
    gRideListF90 = 0;
    gRideListF8C = 0;
    gRideListF8A = 0;
    gRideListF88 = 0;
    gRideListPanel = gGameWindow->Build(gRideListScript, RidePanelMsg);
    if (gRideListPanel != nullptr) {
        UIMain_SetPanel(gRideListPanel);
        InterfaceWindow* title = gRideListPanel->GetWindow(0x200);
        int32_t balance = static_cast<CWorld*>(gGameWorld)->GetBankAccount()->GetBalance();
        TbITable* strings = static_cast<TbITable*>(gText.mResourcePointers[6]);
        if (balance < 0) {
            TbIntegerVTag tag(0xd, static_cast<int32_t>(0u - static_cast<uint32_t>(balance)));   // labs
            strings->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1cb, &tag);
        } else {
            TbIntegerVTag tag(0xd, balance);
            strings->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1ca, &tag);
        }
        InterfaceTitleTextObject* titleObject = new InterfaceTitleTextObject(nullptr);
        title->SetDrawObject(titleObject);
        title->SetFontJust(2, 1);
        title->SetFont(GetFontTableEntry(5));
        title->SetFontColour(0xff, 0xff, 0xff, 0xff);
        title->SetText(gUIMainConvertBuffer);

        InterfaceWindow* picture = gRideListPanel->GetWindow(0x1ea)->GetWindow(0x1eb);
        InterfaceDrawObjectSprite* sprite = new InterfaceDrawObjectSprite(nullptr);
        picture->SetClientDrawObject(sprite);
        picture->setZ(static_cast<int16_t>(picture->mZ + 0xc));

        InterfaceWindow* label = gRideListPanel->GetWindow(0x1ea)->GetWindow(0x1ec);
        InterfaceTextObjectSprite* textObject = new InterfaceTextObjectSprite(nullptr);
        label->SetClientDrawObject(textObject);
        label->SetFontJust(1, 1);
        label->SetFontWrap(1, 1);
        label->SetFont(GetFontTableEntry(6));
        label->SetFontColour(0xff, 0xff, 0xff, 0xff);
        label->SetClientDrawText(nullptr);
        label->setZ(static_cast<int16_t>(label->mZ + 0xc));

        InterfaceWindow* window = gRideListPanel->GetWindow(0x1f8);
        if (window != nullptr) {
            InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window);
            list->SetCallbackFunction(RideListLCHandler);
            list->SetColumnFormatsAll(RideListLCFormat, RideListLCPostFormat);
            list->SetColumnType(0, kLCText);
            list->SetColumnType(1, kLCNumber);
            list->SetColumnType(2, kLCNumber);
            list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
            list->SetSort(gRideListSortColumn, false);
            InterfaceWindow* button = list->GetWindow(0x10);
            char16_t* text = const_cast<char16_t*>(UIMain_GetString(0x7b));
            MAKECOLUMNBUTTON(button, text, gRideListButtonFont, 0xff, 0xef, 0, 0x90);
            button = list->GetWindow(0x11);
            text = const_cast<char16_t*>(UIMain_GetString(0x7c));
            MAKECOLUMNBUTTON(button, text, gRideListButtonFont, 0xff, 0xef, 0, 0x91);
            button = list->GetWindow(0x12);
            text = const_cast<char16_t*>(UIMain_GetString(0x8a));
            MAKECOLUMNBUTTON(button, text, gRideListButtonFont, 0xff, 0xef, 0, 0x92);
            if (kind == -1) kind = gRideListType;
            SetTypeButton(list->GetWindow(0x1f9), kind);
            FillRideList(static_cast<UIObjType>(kind), 0);
            int32_t titleString = 0;
            if (kind == 2) titleString = 0x79;
            else if (kind < 2) {
                if (kind == 0) titleString = 0x77;
                else if (kind >= 0) titleString = 0x78;
            } else if (kind < 4) {
                titleString = 0x7a;
            }
            if (titleString != 0)
                UIMain_SetTitleText(gRideListPanel, 0x1fe, const_cast<char16_t*>(UIMain_GetString(titleString)));
            SelectEntry(list, selected, gRideListNoneB);
        }
    }
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xbc));
}
