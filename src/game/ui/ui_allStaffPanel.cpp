// ui_allStaffPanel.cpp: name proven by __sinit_ui_allStaffPanel_cpp in the main application.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_Online.h"
#include "helpers/ui_allStaffPanelHelpers.h"
#include "History.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "LoadPanelMsg.h"
#include "LoansPanelMsg.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "UI_pump.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <new>

// Unit v5066: the all-items panel's builder and entry handler, and the all-staff panel's list functions (ui_allStaffPanel.cpp).
//   CreateAllItemsPanel 0x101983c4, AllItemsLCEntryHandler 0x10198264, FillAllStaffListCallback 0x10198690,
//   FillAllStaffList 0x101987fc, UpdateListEntries 0x10198944

// 0x103947e8: the staff list FillAllStaffList is filling (the callback adds to it)
InterfaceListControl2* gStaffListControl = nullptr;

namespace {

constexpr int32_t kStaffTextTable = 10;   // gText + 0x574: mResourcePointers[10], the TbITable of the staff states

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

// Unit v51004: AllStaffPanelMsg 0x10198ac4 (596), UIStatusPanel::RequestMaintenance 0x101932a0 (88), FinancePanelPanelMsg 0x1019f434 (368).

namespace {

// The list's data word: the staff member's address on the Mac, its arena offset here (FillAllStaffListCallback's PointerData; unit
// v5L31: the raw word read as an address sent StaffStatusPanel::Show a pointer to nothing).
CThing* DataThing(int32_t data) {
    return data != 0 ? reinterpret_cast<CThing*>(tpw::gArenaBase + static_cast<uint32_t>(data)) : nullptr;
}

}  // namespace

// 0x10198ac4: the all-staff panel's messages. Bug kept: message 0x402 asks the thing id of the list's data pointer.
int32_t AllStaffPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    switch (message) {
    case 0x100:
        switch (a) {
        case -2: RemoveAllStaffPanel(); return 0;
        case 0x32e: CreateParkStatusPanel(); return 0;
        case 0x32f: CreateAllKidsPanel(nullptr); return 0;
        case 0x330: CreateAllItemsPanel(static_cast<UIObjType>(-1), nullptr); return 0;
        default: break;
        }
        break;
    case 5:
        RemoveAllStaffPanel();
        return 1;
    case 0x10:
        if (static_cast<uint32_t>(a) - 0x80000u == 0x83u) UpdateListEntries();
        break;
    case 0x14:
        gStaffPanel = nullptr;
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x11d));
        break;
    case 0x15:
        UIMain_SetTitleText(window, 0x328, const_cast<char16_t*>(UIMain_GetString(100)));
        break;
    case 0x400: {
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
        if (list != nullptr && b != -1) {
            int32_t data;
            list->GetData(b, &data);
            gStaffStatusPanel.Show(DataThing(data));
        }
        return 0;
    }
    case 0x402: {
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
        if (list != nullptr && b != -1) {
            int32_t data;
            list->GetData(b, &data);
            CThing* thing = DataThing(data);
            tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id = thing->GetThingId();
            UIMain_ZoomToThing(id);
            RemoveAllStaffPanel();
            return 0;
        }
        break;
    }
    case 0x406:
        gStaffSort = b;
        break;
    default:
        break;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

// The all-staff panel's row maker AllStaffLCFormat (LCFormat 0x101991e8), its attach function AllStaffLCPostFormat (LCPostFormat
// 0x10199370) and the panel's button handler AllStaffPanelLCHandler 0x10198d54.

int32_t gStaffListFormat;           // 0x103947e4
uint32_t gStaffListFont = 7;        // 0x102a9080

namespace {
// 0x1024df23 and 0x1024df33: the two gradient file names, 16 bytes apart.
const char kGradientNames[] = "ridestatbar.wct\0happygrad.wct";
}  // namespace

// 0x101991e8 (360 bytes): the row window of the list, 4 controls; the row height (kept in a global) is the font's height scaled by
// 0x600 / the real screen height, plus 6.
InterfaceWindow* AllStaffLCFormat() {
    if (gStaffListFormat == 0) {
        InterfaceFont* font = GetFontTableEntry(gStaffListFont);
        int32_t height = static_cast<int16_t>(font->GetHeight());
        gStaffListFormat = height * 0x600 / gUIRealScreenHeight + 6;
    }
    InterfaceWindow* window = new InterfaceWindow(nullptr, 0, 0, 0, 0, 0, static_cast<int16_t>(gStaffListFormat));
    window->CreateObject(1, 1, 999, 0, 0, 0, 0);
    window->CreateObject(1, 1, 999, 0, 0, 0, 0);
    window->CreateObject(9, 0x11, 4, 0, 0, 0, 0);
    window->CreateObject(9, 0x11, 5, 0, 0, 0, 0);
    return window;
}

// 0x10199370 (652 bytes): gives the row window and its four children their draw objects (two text sprites, a cash text, two gradients).
// The slot +0x24 of the window's vtable is SetValue; the CoolRect is made and destroyed without use.
void AllStaffLCPostFormat(InterfaceWindow* window) {
    CoolRect unused;
    window->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    window->SetFont(GetFontTableEntry(gStaffListFont));
    window->SetClientDrawText(nullptr);

    InterfaceWindow* child = window->GetChild(0);
    child->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    child->SetFont(GetFontTableEntry(gStaffListFont));
    child->SetClientDrawText(nullptr);

    child = child->GetNext();
    child->SetClientDrawObject(new InterfaceCashTextObject(nullptr));
    child->SetFontJust(2, 1);
    child->SetFont(GetFontTableEntry(gStaffListFont));
    child->SetValue(0);

    child = child->GetNext();
    UIMain_IndentClient(child, 0, 8);
    child->SetClientDrawObject(new InterfaceDrawEngineHGradient(const_cast<char*>(kGradientNames)));
    child->SetValue(500);

    child = child->GetNext();
    UIMain_IndentClient(child, 0, 8);
    child->SetClientDrawObject(new InterfaceDrawEngineHGradient(const_cast<char*>(kGradientNames + 16)));
}

// 0x10198d54 (1,104 bytes): a click (message 0x101, b == 1) on one of the five staff buttons 0x323 to 0x327; any other message falls to the default handler.
int32_t AllStaffPanelLCHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x101 && b == 1) {
        switch (a) {
            case 0x323: ShowStaffKind(0, 0x6b, 5); break;
            case 0x324: ShowStaffKind(4, 0x6f, 8); break;
            case 0x325: ShowStaffKind(2, 0x6e, 6); break;
            case 0x326: ShowStaffKind(1, 0x6c, 4); break;
            case 0x327: ShowStaffKind(3, 0x6d, 7); break;
            default: break;
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// Unit v5019: the all-staff panel (ui_allStaffPanel.cpp), CreateAllStaffPanel 0x10199630 (1972 bytes).

// ---- globals (no other file names them; names invented)
int32_t gStaffColumnFont = 8;          // 0x102a9084 the font number of the column buttons
int32_t gStaffSort = 1;                // 0x102a9088 the sort column of the staff list
InterfaceWindow* gStaffPanel = nullptr;   // 0x103947f0 the open all-staff panel
int32_t gStaffType = 0;                // 0x103947ec the kind of staff the panel lists

// 0x10199630 (1972 bytes): builds the all-staff panel from its script, sets up the list (columns, sort, selection bar, the kind of
// staff, the current entry), the kind check buttons, the texts and the happiness bars, starts the refresh timer and says the advice.
// QUIRK: the model for the happiness average (r30) is only set for the kinds 0 to 4; for another kind the original passes whatever
// its caller left in r30 (here 0).
void CreateAllStaffPanel(StaffType type, CThing* thing) {
    UIMain_CheckPanel();
    SetLastPanel(2, 4);
    InterfaceWindow* built = gGameWindow->Build(gAllStaffScript, AllStaffPanelMsg);
    if (built == nullptr) return;
    gStaffPanel = built;
    UIMain_SetPanel(gStaffPanel);
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gStaffPanel->GetWindow(0x321));
    if (list != nullptr) {
        list->SetColumnFormatsAll(AllStaffLCFormat, AllStaffLCPostFormat);
        list->SetColumnType(0, kLCText);
        list->SetColumnType(1, kLCText);
        list->SetColumnType(2, kLCNumber);
        list->SetColumnType(3, kLCNumber);
        list->SetColumnType(4, kLCNumber);
        list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
        list->SetCallbackFunction(AllStaffPanelLCHandler);
        list->SetSort(gStaffSort, false);
        InterfaceWindow* button = list->GetWindow(0x10);
        MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x65)), gStaffColumnFont, 0xff, 0xef, 0, 0x77);
        button = list->GetWindow(0x11);
        MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x66)), gStaffColumnFont, 0xff, 0xef, 0, 0x78);
        button = list->GetWindow(0x12);
        MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x67)), gStaffColumnFont, 0xff, 0xef, 0, 0x79);
        button = list->GetWindow(0x13);
        MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x68)), gStaffColumnFont, 0xff, 0xef, 0, 0x7a);
        button = list->GetWindow(0x14);
        MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x69)), gStaffColumnFont, 0xff, 0xef, 0, 0x7b);
        int32_t kind = static_cast<int32_t>(type);
        if (kind == -1) kind = gStaffType;
        gStaffType = kind;
        FillAllStaffList(static_cast<StaffType>(gStaffType), 0);
        if (thing != nullptr) {
            int32_t position = list->FindData(static_cast<int32_t>(reinterpret_cast<intptr_t>(thing)));
            if (position >= 0) list->SetCurrent(position);
        }
        InterfaceCheckButtons* checks = static_cast<InterfaceCheckButtons*>(list->GetWindow(0x322));
        if (checks != nullptr) {
            int32_t now = gStaffType;
            if (now == 2) {
                checks->SetCurrent(0x325);
            } else if (now < 2) {
                if (now == 0) {
                    checks->SetCurrent(0x323);
                } else if (now >= 0) {
                    checks->SetCurrent(0x326);
                }
            } else if (now == 4) {
                checks->SetCurrent(0x324);
            } else if (now < 4) {
                checks->SetCurrent(0x327);
            }
        }
    }
    InterfaceWindow* box = gStaffPanel->GetWindow(0x329);
    InterfaceWindow* sub = box->GetWindow(0x32a);
    InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
    sub->SetClientDrawObject(sprite);
    sub->SetFont(GetFontTableEntry(6));
    sub->SetClientDrawText(nullptr);
    sub = box->GetWindow(0x32d);
    UIMain_IndentClient(sub, 0, 8);
    InterfaceDrawEngineHGradient* gradient = new InterfaceDrawEngineHGradient(const_cast<char*>("happygrad.wct"));
    sub->SetClientDrawObject(gradient);
    sub = box->GetWindow(0x32b);
    sprite = new InterfaceTextObjectSprite(nullptr);
    sub->SetClientDrawObject(sprite);
    sub->SetFont(GetFontTableEntry(6));
    sub->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x6a)));
    sub = box->GetWindow(0x32c);
    UIMain_IndentClient(sub, 0, 8);
    gradient = new InterfaceDrawEngineHGradient(const_cast<char*>("happygrad.wct"));
    sub->SetClientDrawObject(gradient);
    float level = gGameWorld->GetParkAnalyser()->GetStaffHappinessLevel(0);
    float scaled = 1024.0f * level;
    float percent = scaled / 100.0f;
    sub->SetValue(ppc_fctiwz(static_cast<double>(percent)));

    int32_t now = gStaffType;
    box = gStaffPanel->GetWindow(0x329);
    sub = box->GetWindow(0x32a);
    if (now == 2) {
        sub->SetText(const_cast<char16_t*>(UIMain_GetString(0x6e)));
    } else if (now < 2) {
        if (now == 0) {
            sub->SetText(const_cast<char16_t*>(UIMain_GetString(0x6b)));
        } else if (now >= 0) {
            sub->SetText(const_cast<char16_t*>(UIMain_GetString(0x6c)));
        }
    } else if (now == 4) {
        sub->SetText(const_cast<char16_t*>(UIMain_GetString(0x6f)));
    } else if (now < 4) {
        sub->SetText(const_cast<char16_t*>(UIMain_GetString(0x6d)));
    }

    now = gStaffType;
    box = gStaffPanel->GetWindow(0x329);
    sub = box->GetWindow(0x32d);
    int32_t model = 0;
    if (now == 2) {
        model = 6;
    } else if (now < 2) {
        if (now == 0) {
            model = 5;
        } else if (now >= 0) {
            model = 4;
        }
    } else if (now == 4) {
        model = 8;
    } else if (now < 4) {
        model = 7;
    }
    int32_t average = gGameWorld->GetParkAnalyser()->AverageHappinessOfStaffType(static_cast<CThing::ThingModel>(model));
    // the average times 1024, divided by 100 by the multiply-high trick (0x51eb851f)
    int32_t shifted = static_cast<int32_t>(static_cast<uint32_t>(average) << 10);
    int32_t quotient = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * shifted) >> 32) >> 5;
    quotient += static_cast<int32_t>(static_cast<uint32_t>(quotient) >> 31);
    sub->SetValue(quotient);
    if (gStaffPanel->setAnimTimer(0x80083, 2000) != 0) gStaffPanel->startAnimTimer(0x80083);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x11d));
}

uint32_t gAllStaffWord47f4;      // 0x103947f4, set to 0x10000 by __sinit_ui_allStaffPanel_cpp
uint32_t gAllStaffWord47f8;      // 0x103947f8, cleared by it
// 0x10199e20 (56 bytes)
void RemoveAllStaffPanel() { UI_SendMessage(gStaffPanel, 4, 0, 0); }

// 0x10199e84 (28 bytes)
void __sinit_ui_allStaffPanel_cpp() {
    gAllStaffWord47f8 = 0;
    gAllStaffWord47f4 = 0x10000;
}

// The all-staff panel's window script (ui_allStaffPanel.cpp, data at 0x102a8d0e, 441 words), which CreateAllStaffPanel
// (src/CreateAllStaffPanel.cpp) builds. A file of its own because the unit's test defines the array with its fakes; the words are the
// image's, generated at configure time (devtools/gen_game_tables.py, WindowScripts.inc).

int16_t gAllStaffScript[] = {TPW_WINDOW_SCRIPT_102A8D0E};   // 0x102a8d0e

// Window scripts that InterfaceWindow::Build reads and that no translated file defined (unit v5L42): the launcher had all-zero
// stand-ins for them, so opening the panel failed the lpNew assertion in ProcessLevel. A file of their own because each building
// function's unit test defines its array with its fakes; the words are the image's, generated at configure time
// (devtools/gen_game_tables.py, WindowScripts.inc).

int16_t gAllShowsScript[] = {TPW_WINDOW_SCRIPT_102A8B42};          // 0x102a8b42, CreateAllShowsPanel 0x10196ed0
