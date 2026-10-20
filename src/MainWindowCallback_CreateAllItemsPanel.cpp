// Unit v5066: the all-items panel's builder and entry handler, and the all-staff panel's list functions (ui_allStaffPanel.cpp).
//   CreateAllItemsPanel 0x101983c4, AllItemsLCEntryHandler 0x10198264, FillAllStaffListCallback 0x10198690,
//   FillAllStaffList 0x101987fc, UpdateListEntries 0x10198944
#include <cstdint>

#include "AdvisorScoring.h"
#include "Debug.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "platform/arena.h"
#include "ui_controls.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "WindowScripts.inc"

// ---- globals
int16_t gAllItemsScript[] = {TPW_WINDOW_SCRIPT_102A8884};   // 0x102a8884: the image's words (generated, devtools/gen_game_tables.py)
// 0x102a8d0c: the upgrade-parent id AllItemsLCEntryHandler compares with (0 at start; the word before gAllStaffScript)
uint16_t gAllItemsNoParent = 0;
// 0x103947e8: the staff list FillAllStaffList is filling (the callback adds to it)
InterfaceListControl2* gStaffListControl = nullptr;

namespace {

constexpr int32_t kStaffTextTable = 10;   // gText + 0x574: mResourcePointers[10], the TbITable of the staff states

// The Mac's list data word (a pointer there) is an offset from the arena base here.
template <class T>
T* DataPointer(int32_t data) {
    return data != 0 ? reinterpret_cast<T*>(tpw::gArenaBase + static_cast<uint32_t>(data)) : nullptr;
}

int32_t PointerData(const void* pointer) { return pointer != nullptr ? static_cast<int32_t>(tpw::ArenaOffset(pointer)) : 0; }

// The multiply-high trick for a division by 100 (0x51eb851f), signed, the sign bit added back.
int32_t DivideBy100Signed(int32_t value) {
    int32_t quotient = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * value) >> 32) >> 5;
    return quotient + static_cast<int32_t>(static_cast<uint32_t>(quotient) >> 31);
}

// The staff state shown: the general state, a patrolling state (1) without a patrol area shows state 0.
int32_t ShownStaffState(CStaff* staff) {
    int32_t state = staff->GetGeneralStaffState();
    if (state == 1 && staff->HasPatrolArea() == 0) state = 0;
    return state;
}

}  // namespace

// 0x101983c4 (556 bytes): builds the all-items panel for a kind of item (-1: the kind the last list showed), checks the kind's button,
// fills the list through the kind's builder, sets the title and the callback.
void CreateAllItemsPanel(UIObjType type, CThing* thing) {
    int32_t kind = static_cast<int32_t>(type);
    if (kind == -1) kind = gAllItemsListType;
    UIMain_CheckPanel();
    SetLastPanel(2, 5);
    gAllItemsPanel = gGameWindow->Build(gAllItemsScript, MainWindowCallback);
    UIMain_SetPanel(gAllItemsPanel);
    InterfaceWindow* group = gAllItemsPanel->GetWindow(0x12c4b8);
    InterfaceCheckButtons* buttons = static_cast<InterfaceCheckButtons*>(group->GetWindow(0x12c4b9));
    if (kind == 2) {
        buttons->SetCurrent(0x12c4bb);
        CreateAllShowsPanel(thing);
    } else if (kind < 2) {
        if (kind == 0) {
            buttons->SetCurrent(0x12c4ba);
            CreateAllRidesPanel(thing);
        } else if (kind >= 0) {
            buttons->SetCurrent(0x12c4bc);
            CreateAllShopsPanel(thing);
        }
    } else if (kind < 4) {
        buttons->SetCurrent(0x12c4bd);
        CreateAllFeaturesPanel(thing);
    }
    if (kind == 2) {
        UIMain_SetTitleText(gAllItemsPanel, 0x12c4be, const_cast<char16_t*>(UIMain_GetString(0x50)));
    } else if (kind < 2) {
        if (kind == 0) {
            UIMain_SetTitleText(gAllItemsPanel, 0x12c4be, const_cast<char16_t*>(UIMain_GetString(0x4e)));
        } else if (kind >= 0) {
            UIMain_SetTitleText(gAllItemsPanel, 0x12c4be, const_cast<char16_t*>(UIMain_GetString(0x4f)));
        }
    } else if (kind < 4) {
        UIMain_SetTitleText(gAllItemsPanel, 0x12c4be, const_cast<char16_t*>(UIMain_GetString(0x51)));
    }
    gAllItemsPanel->GetWindow(0x12c4b8)->SetCallbackFunction(ListTemplateCallback);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xbf));
}

// 0x10198264 (284 bytes): message 0x403 (an entry is drawn; b is the entry's object): the font colour is the colour of the object's
// state; an object with an upgrade parent shows the parent's state instead (the parent found in the thing array). Then the default handler.
int32_t AllItemsLCEntryHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x403) {
        CObject* object = DataPointer<CObject>(b);
        if (object->GetUpgradeParent().mIndex != gAllItemsNoParent) {
            uint16_t parent = object->GetUpgradeParent().mIndex;
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            object = static_cast<CObject*>(gThingArray[parent].mThing);
        }
        int32_t state = UIMain_GetObjectState(object, 2);
        uint32_t colour = UIMain_GetObjectStateTextColour(state);
        window->SetFontColour(static_cast<int32_t>(colour >> 24), static_cast<int32_t>((colour >> 16) & 0xff),
                              static_cast<int32_t>((colour >> 8) & 0xff), 0xff);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x10198690 (308 bytes): UI_IterateThingList's function: adds one staff member to the list being filled (name, state text, salary, skill
// and happiness in percent). Answers 1.
int32_t FillAllStaffListCallback(CThing* thing) {
    CStaff* staff = reinterpret_cast<CStaff*>(thing);
    int32_t happiness = static_cast<int32_t>(staff->mHappiness) & 0xff;
    int32_t skill = staff->GetSkill();
    staff->GetGeneralStaffState();   // the answer is not used
    const char16_t* name = staff->GetName();
    InterfaceListControl2* list = gStaffListControl;
    int32_t state = ShownStaffState(staff);
    const unsigned char* text = static_cast<const TbITable*>(gText.mResourcePointers[kStaffTextTable])->GetConstString(static_cast<uint32_t>(state));
    UI_LCAdd item[5];
    item[0].mRaw = 0;
    item[0].mText = name;
    item[1].mRaw = 0;
    item[1].mText = UIMain_ConvertMulti(text);
    item[2].mRaw = 0;
    item[2].mInt = static_cast<int32_t>(staff->GetSalary());
    item[3].mRaw = 0;
    item[3].mInt = DivideBy100Signed(static_cast<int32_t>(static_cast<uint32_t>(skill) << 10));
    item[4].mRaw = 0;
    item[4].mInt = DivideBy100Signed(happiness << 10);
    list->AddItem(item, PointerData(thing), -1, 0);
    return 1;
}

// 0x101987fc (276 bytes): fills the staff panel's list with the staff of a type (the list is emptied first when flag is not 0).
void FillAllStaffList(StaffType type, int32_t flag) {
    int32_t kind = static_cast<int32_t>(type);
    gStaffType = kind;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gStaffPanel->GetWindow(0x321));
    if (flag != 0) list->RemoveAllItems();
    gStaffListControl = list;
    if (kind == 2) {
        UI_IterateThingList(static_cast<UIIteratorTypes>(0x10), FillAllStaffListCallback);
    } else if (kind < 2) {
        if (kind == 0) {
            UI_IterateThingList(static_cast<UIIteratorTypes>(2), FillAllStaffListCallback);
        } else if (kind >= 0) {
            UI_IterateThingList(static_cast<UIIteratorTypes>(4), FillAllStaffListCallback);
        }
    } else if (kind == 4) {
        UI_IterateThingList(static_cast<UIIteratorTypes>(0x20), FillAllStaffListCallback);
    } else if (kind < 4) {
        UI_IterateThingList(static_cast<UIIteratorTypes>(8), FillAllStaffListCallback);
    }
    list->UpdateDisplay();
}

// 0x10198944 (344 bytes): rewrites the state, salary, skill and happiness cells of every entry of the staff panel's list (last entry
// first); nothing when the panel is not open. The skill is divided as unsigned here (the callback divides signed).
void UpdateListEntries() {
    if (gStaffPanel == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gStaffPanel->GetWindow(0x321));
    for (int32_t i = list->mItemCount - 1; i >= 0; --i) {
        int32_t data = 0;
        list->GetData(i, &data);
        CStaff* staff = DataPointer<CStaff>(data);
        int32_t state = ShownStaffState(staff);
        UI_LCAdd item[5];
        item[0].mRaw = 0;
        item[0].mInt = 0;
        const unsigned char* text = static_cast<const TbITable*>(gText.mResourcePointers[kStaffTextTable])->GetConstString(static_cast<uint32_t>(state));
        item[1].mRaw = 0;
        item[1].mText = UIMain_ConvertMulti(text);
        item[2].mRaw = 0;
        item[2].mInt = static_cast<int32_t>(staff->GetSalary());
        uint32_t skill = static_cast<uint32_t>(staff->GetSkill()) << 10;
        item[3].mRaw = 0;
        item[3].mInt = static_cast<int32_t>(static_cast<uint32_t>((static_cast<uint64_t>(0x51eb851f) * skill) >> 32) >> 5);
        int32_t happiness = static_cast<int32_t>(staff->mHappiness) & 0xff;
        item[4].mRaw = 0;
        item[4].mInt = DivideBy100Signed(happiness << 10);
        list->ChangeItem(i, item, 0x1a, i == 0 ? 1 : 0, -1);
    }
}
