// ui_allKidsPanel.cpp: name proven by __sinit_ui_allKidsPanel_cpp in the main application.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "helpers/ui_allItemsPanelHelpers.h"
#include "History.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "LoadPanelMsg.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbDynamicStringTemplate.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "UI_pump.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// ui_allKidsPanel.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: the selection words' accessors,
// UI_GetSelectedPeep (0x101946f8), UI_GetSelectedObject (0x10194730), UI_SetSelectedObject (0x10194768), 12 bytes each, and
// UI_CheckSelected (0x101947e4, 44 bytes). Unit v5L16: UI_SetSelectedPeep (0x101947a8, 12 bytes).

CThing* UI_GetSelectedPeep() { return static_cast<CThing*>(gUISelectedPeep); }

void* UI_GetSelectedObject() { return gUISelectedObject; }

void UI_SetSelectedObject(void* object) { gUISelectedObject = object; }

void UI_SetSelectedPeep(CPeep* peep) { gUISelectedPeep = peep; }

// Takes the interface's three pointer words as the selection.
void UI_CheckSelected() {
    gUISelectedThing = gIntrface.mPointedThing;
    gUISelectedPeep = gIntrface.mPointedStaffOrGuest;
    gUISelectedObject = gIntrface.mThingUnderPointer;
}

// ui_allKidsPanel.cpp (main): UI_ClearSelected 0x10194838 (32 B).

void* gUISelectedThing;    // 0x103947b8
void* gUISelectedPeep;     // 0x103947b4
void* gUISelectedObject;   // 0x103947b0
void UI_ClearSelected() {
    gUISelectedThing = nullptr;
    gUISelectedPeep = nullptr;
    gUISelectedObject = nullptr;
}

// The all-kids panel (unit v5078): FillAllKidsListCallback 0x10194880, UpdateListEntries 0x101949cc (the all-kids one; the
// all-staff twin of the same name is at 0x10198944), AllKidsPanelMsg 0x10194b4c, LCFormat 0x10194df0.

void RemoveAllKidsPanel();   // 0x101956a4 (56 bytes, untranslated)

// Globals of this panel (the Mac's addresses in the comments).
InterfaceWindow* gAllKidsMsgWindow;                // 0x103947c8  the panel window
InterfaceListControl2* gAllKidsListControl;        // 0x103947c4  the list in it
int32_t gAllKidsListFormat;                        // 0x103947bc  the row height LCFormat keeps
int32_t gAllKidsFontIndex = 7;                     // 0x102a8878
int32_t gAllKidsSelectedIndex = 1;                 // 0x102a8880

namespace {

// A list entry's data word is a pointer on the Mac; on the host it is the arena offset (Ptr32.h).
int32_t DataOf_2(const CThing* thing) { return thing != nullptr ? static_cast<int32_t>(tpw::ArenaOffset(thing)) : 0; }
CThing* ThingOf(int32_t data) {
    Ptr32<CThing> pointer;
    pointer.mOffset = static_cast<uint32_t>(data);
    return pointer.get();
}

uint32_t Word(const void* base, uint32_t offset) {
    uint32_t v;
    std::memcpy(&v, static_cast<const uint8_t*>(base) + offset, 4);
    return v;
}

// The six cells one person gives the list (the stack block at 0x38 of FillAllKidsListCallback, 0x44 of UpdateListEntries).
void MakeCells(CThing* thing, UI_LCAdd* cells, uint32_t turnShift) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(thing);
    CPerson* person = reinterpret_cast<CPerson*>(thing);
    for (int i = 0; i < 6; ++i) cells[i].mRaw = 0;
    cells[0].mInt = static_cast<int32_t>(Word(p, 0x1d8));
    cells[1].mInt = static_cast<int32_t>(Word(p, 0x1a0));
    uint32_t turns = person->GetTimeInPark();
    CParkTime* parkTime = &gGameWorld->mParkTime;
    TbTimeDiff span = parkTime->GetFunnyTimeForGameTurns(turns << turnShift);   // the update asks for four times as many turns (slwi 2)
    cells[2].mInt = static_cast<int32_t>(span.mTicks / 0x861c46800LL);   // 0x10237e1c: 64-bit division by 36,000,000,000
    cells[3].mInt = static_cast<int32_t>(Word(p, 0x1c4));
    int32_t lives = static_cast<int32_t>(Word(p, 0x30));
    cells[4].mInt = lives != 0 ? lives - 1 : 999;
    float level;
    std::memcpy(&level, p + 0x19c, 4);
    int32_t whole = ppc_fctiwz(static_cast<double>(level));
    cells[5].mInt = static_cast<int32_t>((static_cast<uint32_t>(whole) & 0xffu) << 10) / 100;
}

}  // namespace

// 0x10194880 (276 bytes): adds the person to the list.
int32_t FillAllKidsListCallback(CThing* thing) {
    UI_LCAdd cells[6];
    MakeCells(thing, cells, 0);
    gAllKidsListControl->AddItem(cells, DataOf_2(thing), -1, 0);
    return 1;
}

namespace AllKids {

// 0x101949cc (344 bytes): refreshes every row of the list from its person (last row first).
void UpdateListEntries() {
    if (gAllKidsMsgWindow == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gAllKidsMsgWindow->GetWindow(0x1e497));
    for (int32_t position = list->mItemCount - 1; position >= 0; --position) {
        int32_t data;
        list->GetData(position, &data);
        CThing* thing = ThingOf(data);
        UI_LCAdd cells[6];
        MakeCells(thing, cells, 2);
        list->ChangeItem(position, cells, 0x3f, position == 0, -1);
    }
}

}  // namespace AllKids

// 0x10194b4c (616 bytes): the panel's message handler.
int32_t AllKidsPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        switch (a) {
        case -2:
            RemoveAllKidsPanel();
            return 0;
        case 0x1e499:
            CreateParkStatusPanel();
            return 0;
        case 0x1e49a:
            CreateAllStaffPanel(static_cast<StaffType>(-1), nullptr);
            return 0;
        case 0x1e49b:
            CreateAllItemsPanel(static_cast<UIObjType>(-1), nullptr);
            return 0;
        default:
            break;
        }
    } else if (message == 5) {
        RemoveAllKidsPanel();
        return 1;
    } else if (message == 0x10) {
        if (a == 0x80083) AllKids::UpdateListEntries();
    } else if (message == 0x14) {
        gAllKidsMsgWindow = nullptr;
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x11e));
    } else if (message == 0x15) {
        UIMain_SetTitleText(window, 0x1e498, const_cast<char16_t*>(UIMain_GetString(0x70)));
    } else if (message == 0x400) {
        InterfaceWindow* list = window->GetWindow(a);
        if (list != nullptr && b != -1) {
            int32_t data = 0;
            static_cast<InterfaceListControl2*>(list)->GetData(b, &data);
            gVisitorStatusPanel.Show(ThingOf(data));
        }
        return 0;
    } else if (message == 0x402) {
        InterfaceWindow* list = window->GetWindow(a);
        if (list != nullptr && b != -1) {
            int32_t data = 0;
            static_cast<InterfaceListControl2*>(list)->GetData(b, &data);
            CThing* thing = ThingOf(data);
            UIMain_ZoomToThing(thing->GetThingId());
            RemoveAllKidsPanel();
            return 0;
        }
    } else if (message == 0x406) {
        gAllKidsSelectedIndex = b;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

// 0x10194df0 (408 bytes): makes the list-format window (the row height is always 0x38 in the end).
InterfaceWindow* LCFormat() {
    if (gAllKidsListFormat == 0) {
        // The Mac calls a virtual through the font entry's table here (__ptr_glue); the class is not recovered, so the height getter stands in.
        InterfaceFont* font = GetFontTableEntry(static_cast<uint32_t>(gAllKidsFontIndex));
        int32_t height = static_cast<int16_t>(font->GetHeight());
        int32_t screen = gUIRealScreenHeight;
        gAllKidsListFormat = screen != 0 ? height * 0x600 / screen + 6 : height * 0x600;
    }
    gAllKidsListFormat = 0x38;
    void* memory = ::operator new(sizeof(InterfaceWindow));   // 0x138 bytes on the Mac; the host window is larger
    InterfaceWindow* window = new (memory) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, static_cast<int16_t>(gAllKidsListFormat));
    window->CreateObject(1, 1, 3, 0, 0x2c, 0, 0x2c);
    window->CreateObject(1, 1, 0x3e7, 0, 0, 0, 0);
    window->CreateObject(1, 0x11, 4, 0, 0, 0, 0);
    window->CreateObject(6, 1, 4, 0, 0, 0, 0);
    window->CreateObject(9, 0x11, 5, 0, 0, 0, 0);
    return window;
}

// Unit v5219: the all-kids panel's creation and its list rows, and the rides / sideshows list rows (ui_allKidsPanel.cpp, ui_allStaffPanel.cpp):
//   CreateAllKidsPanel 0x101952bc, AllKidsPanel_KidAdded 0x101957b8, AddItemToAllRidesList 0x10195978,
//   FillAllSideshowsListCallback 0x10195dc8, UpdateRideListEntries 0x1019600c.
//
// The cells of a row are one word each on the Mac (a stack block); a person or a ride is reached by the Mac's offsets.
// Two runtime helpers are called as the listing does (no source names them): 0x10237cd8 turns the sum of a history (a double) into a
// word (the listing keeps r3, the HIGH word of a 64-bit conversion), 0x10237e1c divides a 64-bit span (r3:r4) by r5:r6 and the
// listing keeps the low word.

// ---- globals first named here (no header, source or import names them)
int16_t gAllKidsScript[] = {TPW_WINDOW_SCRIPT_102A868C};   // 0x102a868c: the image's words (generated, devtools/gen_game_tables.py)
int32_t gAllKidsColumnFont = 8;         // 0x102a887c the font number of the column buttons
int32_t gAllKidsCounter = 0;            // 0x103947c0 cleared before the list is filled
namespace {

template <class T>
T& At(void* base, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

}  // namespace

// 0x101952bc (952 bytes): opens the all-kids panel, fills its list with the visitors, selects `thing` when it is in it and labels the column buttons.
void CreateAllKidsPanel(CThing* thing) {
    UIMain_CheckPanel();
    SetLastPanel(2, 6);
    InterfaceWindow* built = gGameWindow->Build(gAllKidsScript, AllKidsPanelMsg);
    if (built == nullptr) return;
    gAllKidsMsgWindow = built;
    UIMain_SetPanel(gAllKidsMsgWindow);
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gAllKidsMsgWindow->GetWindow(0x1e497));
    if (list != nullptr) {
        list->SetColumnFormatsAll(LCFormat, AllKidsListLCPostFormat);
        for (int16_t column = 0; column < 6; ++column) list->SetColumnType(column, kLCNumber);
        list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
        list->SetSort(gAllKidsSelectedIndex, false);
        InterfaceListControl2* filled = static_cast<InterfaceListControl2*>(gAllKidsMsgWindow->GetWindow(0x1e497));
        gAllKidsListControl = filled;
        gAllKidsCounter = 0;
        UI_IterateThingList(kUIIterVisitors, FillAllKidsListCallback);
        filled->UpdateDisplay();
        if (thing != nullptr) {
            int32_t position = list->FindData(DataOf(thing));
            if (position >= 0) list->SetCurrent(position);
        }
        for (int32_t i = 0; i < 6; ++i) {
            InterfaceWindow* button = list->GetWindow(0x10 + i);
            char16_t* text = const_cast<char16_t*>(UIMain_GetString(0x71 + i));
            MAKECOLUMNBUTTON(button, text, gAllKidsColumnFont, 0xff, 0xef, 0, 0x81 + i);
        }
    }
    if (gAllKidsMsgWindow->setAnimTimer(0x80083, 2000) != 0) gAllKidsMsgWindow->startAnimTimer(0x80083);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x11e));
}

uint32_t gAllKidsWord47cc;       // 0x103947cc, set to 0x10000 by __sinit_ui_allKidsPanel_cpp
uint32_t gAllKidsWord47d0;       // 0x103947d0, cleared by it
// 0x101956a4 (56 bytes)
void RemoveAllKidsPanel() { UI_SendMessage(gAllKidsMsgWindow, 4, 0, 0); }

// Unit v5952: FillAllRidesListCallback_Upgrades 0x10195ba8, FillAllShopsListCallback 0x10195cb0, FillAllFeaturesListCallback 0x10195f24,
// AllKidsPanel_KidGone 0x10195708 and the kids list's attach function 0x10194fa8 (AllKidsListLCPostFormat; the Mac name is LCPostFormat).

namespace {

char kKidNames[] = "ti_hungry\0happygrad.wct";   // 0x1024de14, the second name at +10

}  // namespace

namespace kidlist_callee {
__attribute__((weak)) void VisitorNumCtor(void* mem) { lender_callee::SpriteCtor(mem); }
// The stack rectangle is never read by the function: its constructor and destructor have no effect that is observable here.
__attribute__((weak)) void RectCtor(void*) {}
__attribute__((weak)) void RectDtor(void*) {}
}  // namespace kidlist_callee

// 0x10195708 (124 bytes): takes the kid's row out of the kids list (when the panel is open and the row is there) and counts it off.
void AllKidsPanel_KidGone(CThing* thing) {
    if (gAllKidsMsgWindow == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gAllKidsMsgWindow->GetWindow(0x1e497));
    int32_t position = list->FindData(DataOf(thing));
    if (position < 0) return;
    list->RemoveItem(position);
    gAllKidsCounter = gAllKidsCounter - 1;
}

// 0x10194fa8 (736 bytes): the kids list row's columns: a visitor number, cash, a visitor time, a number, then two pictures (the second a
// gradient). The objects come from the allocator, which is checked for NULL like the Mac does.
void AllKidsListLCPostFormat(InterfaceWindow* window) {
    using namespace allitems_post_callee;
    alignas(16) unsigned char rect[sizeof(CoolRect) + 16];
    kidlist_callee::RectCtor(rect);
    void* memory = OperatorNew(0x19c);
    if (memory != nullptr) kidlist_callee::VisitorNumCtor(memory);
    window->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(memory));
    window->SetFont(GetFontTableEntry(static_cast<uint32_t>(gAllKidsFontIndex)));
    window->SetFontJust(1, 1);
    InterfaceWindow* child = window->GetChild(0);
    memory = OperatorNew(0x19c);
    if (memory != nullptr) CashCtor(memory);
    child->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(memory));
    child->SetFont(GetFontTableEntry(static_cast<uint32_t>(gAllKidsFontIndex)));
    WindowSetValue(child, 0);
    child->SetFontJust(2, 1);
    child = child->GetNext();
    memory = OperatorNew(0x19c);
    if (memory != nullptr) vsp_callee::TimeCtor(memory);
    child->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(memory));
    child->SetFont(GetFontTableEntry(static_cast<uint32_t>(gAllKidsFontIndex)));
    child->SetFontJust(1, 1);
    child = child->GetNext();
    memory = OperatorNew(0x19c);
    if (memory != nullptr) NumberCtor(memory);
    child->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(memory));
    child->SetFont(GetFontTableEntry(static_cast<uint32_t>(gAllKidsFontIndex)));
    WindowSetValue(child, 0);
    child = child->GetNext();
    UIMain_IndentClient(child, 4, 0);
    child->SetClientDrawObject(DrawObjectLibrary::FindDrawObject(kKidNames));
    child = child->GetNext();
    UIMain_IndentClient(child, 0, 8);
    memory = OperatorNew(0x124);
    if (memory != nullptr) GradientCtor(memory, kKidNames + 10);
    child->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(memory));
    kidlist_callee::RectDtor(rect);
}

// 0x101957b8 (320 bytes): a kid has come into the park: adds his row to the all-kids list when the panel is open.
void AllKidsPanel_KidAdded(CThing* thing) {
    if (gAllKidsMsgWindow == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gAllKidsMsgWindow->GetWindow(0x1e497));
    gAllKidsListControl = list;
    UI_LCAdd cells[6];
    cells[0] = Number(At<int32_t>(thing, 0x1d8));
    cells[1] = Number(At<int32_t>(thing, 0x1a0));
    uint32_t turns = reinterpret_cast<CPerson*>(thing)->GetTimeInPark();
    const CParkTime* parkTime = reinterpret_cast<const CParkTime*>(reinterpret_cast<const uint8_t*>(gGameWorld) + 0x2a0);
    TbTimeDiff span = parkTime->GetFunnyTimeForGameTurns(turns);
    cells[2] = Number(static_cast<int32_t>(span.mTicks / 0x861c46800LL));   // 0x10237e1c: hours (a signed 64-bit divide), the low word
    cells[3] = Number(At<int32_t>(thing, 0x1c4));
    int32_t stay = At<int32_t>(thing, 0x30);
    cells[4] = Number(stay == 0 ? 999 : stay - 1);
    cells[5] = Number(PercentOfByte(static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(At<float>(thing, 0x19c))))));
    list->AddItem(cells, DataOf(thing), -1, 0);
    list->UpdateDisplay();
}

// 0x1019592c (28 bytes)
void __sinit_ui_allKidsPanel_cpp() {
    gAllKidsWord47d0 = 0;
    gAllKidsWord47cc = 0x10000;
}
