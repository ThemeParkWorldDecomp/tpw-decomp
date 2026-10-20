// CreateHireListPanel 0x101a1c88 (3316 bytes, ui_mainpanel2.cpp's hire panel): builds the hire panel (or, when it is up already, only
// switches its staff type), fills the cash line, the staff type list with its column buttons and the finance summary below it.
#include <algorithm>
#include <new>

#include "LoadPanelMsg.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "analyser.h"
#include "ui_main.h"

#include "MainWindowCallback.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "WindowScripts.inc"

// ---- the unit's own data (names invented; the addresses are the Mac's)
int16_t gHirePanelScript[] = {TPW_WINDOW_SCRIPT_102A9EE8};   // 0x102a9ee8: the image's words (generated, devtools/gen_game_tables.py)
int32_t gHireColumnColour = 8;          // 0x102aa320
int32_t gHireSortColumn = 1;            // 0x102aa324
int32_t gHireListWord10 = 0;            // 0x10394910
int32_t gHireListWord14 = 0;            // 0x10394914
int32_t gHireListWithdrawals = 0;       // 0x10394918
int32_t gHireListDeposits = 0;          // 0x1039491c
// 0x1024e0c3: "hilight", then at +8 "ridestatbar.wct"
char gHireListStrings[24] = {'h', 'i', 'l', 'i', 'g', 'h', 't', 0, 'r', 'i', 'd', 'e', 's', 't', 'a', 't', 'b', 'a', 'r', '.', 'w', 'c', 't', 0};

// The hire list's row maker and the panel's message function: not translated yet (stand-ins).
__attribute__((weak)) InterfaceWindow* HireListLCFormat() { return nullptr; }   // 0x101a1798
__attribute__((weak)) int32_t HirePanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }   // 0x101a1314

static_assert(sizeof(InterfaceTextObjectSprite) <= 0x400 && sizeof(InterfaceDrawEngineHGradient) <= 0x400, "the allocation below is 0x400 at least");

namespace hirelist_callee {
__attribute__((weak)) int32_t Labs(int32_t value) { return value < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(value)) : value; }
// The Mac allocates the sizes of its objects; the host's are larger (InterfaceTextObjectSprite 0x250 bytes for the Mac's 0x19c), so the block is
// at least 0x400 bytes.
__attribute__((weak)) void* OperatorNew(uint32_t size) { return ::operator new(std::max<size_t>(size, 0x400)); }
__attribute__((weak)) void TitleCtor(void* mem) { new (mem) InterfaceTitleTextObject(nullptr); }
__attribute__((weak)) void SpriteCtor(void* mem) { new (mem) InterfaceTextObjectSprite(nullptr); }
__attribute__((weak)) void CashCtor(void* mem) { new (mem) InterfaceCashTextObject(nullptr); }
__attribute__((weak)) void GradientCtor(void* mem, const char* mesh) { new (mem) InterfaceDrawEngineHGradient(const_cast<char*>(mesh)); }
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, char16_t* text) { window->SetText(text); }
}  // namespace hirelist_callee

namespace {

using namespace hirelist_callee;

InterfaceDrawObject* NewTitle() {
    void* mem = OperatorNew(0x19c);
    if (mem != nullptr) TitleCtor(mem);
    return static_cast<InterfaceDrawObject*>(static_cast<InterfaceTitleTextObject*>(mem));
}

InterfaceDrawObject* NewSprite() {
    void* mem = OperatorNew(0x19c);
    if (mem != nullptr) SpriteCtor(mem);
    return static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(mem));
}

InterfaceDrawObject* NewCash() {
    void* mem = OperatorNew(0x19c);
    if (mem != nullptr) CashCtor(mem);
    return static_cast<InterfaceDrawObject*>(static_cast<InterfaceCashTextObject*>(mem));
}

// The check button of the staff type; the types outside 0..4 select nothing.
void SelectTypeButton(InterfaceCheckButtons* buttons, int32_t type) {
    switch (type) {
    case 0: buttons->SetCurrent(0x2490); break;
    case 1: buttons->SetCurrent(0x2493); break;
    case 2: buttons->SetCurrent(0x2492); break;
    case 3: buttons->SetCurrent(0x2494); break;
    case 4: buttons->SetCurrent(0x2491); break;
    default: break;
    }
}

// The panel's title for the staff type (the strings 0x8b..0x8f).
void SetTypeTitle(int32_t type) {
    int32_t string;
    switch (type) {
    case 0: string = 0x8b; break;
    case 1: string = 0x8c; break;
    case 2: string = 0x8d; break;
    case 3: string = 0x8e; break;
    case 4: string = 0x8f; break;
    default: return;
    }
    UIMain_SetTitleText(gHirePanelWindow, 0x2480, const_cast<char16_t*>(UIMain_GetString(string)));
}

// A text row of the summary: a text sprite with font 6 and the string.
InterfaceWindow* TextRow(InterfaceWindow* parent, int32_t id, bool red, int32_t string) {
    InterfaceWindow* row = parent->GetWindow(id);
    row->SetClientDrawObject(NewSprite());
    if (red) row->SetFontColour(0xff, 0, 0, 0xff);
    row->SetFont(GetFontTableEntry(6));
    row->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(string)));
    return row;
}

// A cash row of the summary: a cash object, right justified, font 6; the value is set by the caller.
InterfaceWindow* CashRow(InterfaceWindow* parent, int32_t id) {
    InterfaceWindow* row = parent->GetWindow(id);
    row->SetClientDrawObject(NewCash());
    row->SetFontJust(2, 1);
    row->SetFont(GetFontTableEntry(6));
    return row;
}

}  // namespace

void CreateHireListPanel(StaffType type) {
    int32_t staff = static_cast<int32_t>(type);
    UIMain_CheckPanel();
    SetLastPanel(1, 2);
    if (gHirePanelWindow != nullptr && staff != -1 && staff != gHirePanelType) {
        // The panel is up: only the type changes (no NULL checks, as the original).
        gHirePanelType = staff;
        InterfaceWindow* list = gHirePanelWindow->GetWindow(0x248e);
        InterfaceCheckButtons* buttons = static_cast<InterfaceCheckButtons*>(list->GetWindow(0x248f));
        SelectTypeButton(buttons, gHirePanelType);
        SetTypeTitle(gHirePanelType);
        FillHireList(static_cast<StaffType>(gHirePanelType), 1);
        return;
    }
    InterfaceWindow* built = gGameWindow->Build(gHirePanelScript, HirePanelMsg);
    if (built == nullptr) return;
    if (staff == -1) staff = gHirePanelType;
    gHirePanelType = staff;
    gHirePanelWindow = built;
    UIMain_SetPanel(gHirePanelWindow);

    // the cash line
    InterfaceWindow* cash = gHirePanelWindow->GetWindow(0x2496);
    TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[6]);
    int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
    if (balance < 0) {
        TbIntegerVTag tag(0xd, hirelist_callee::Labs(balance));
        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1cb, static_cast<TbVTag*>(&tag), nullptr);
    } else {
        TbIntegerVTag tag(0xd, balance);
        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1ca, static_cast<TbVTag*>(&tag), nullptr);
    }
    cash->SetDrawObject(NewTitle());
    cash->SetFontJust(2, 1);
    cash->SetFont(GetFontTableEntry(5));
    cash->SetFontColour(0xff, 0xff, 0xff, 0xff);
    WindowSetText(cash, gUIMainConvertBuffer);

    // the list
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gHirePanelWindow->GetWindow(0x248e));
    if (list != nullptr) {
        list->SetColumnFormatsAll(HireListLCFormat, HirePanelLCPostFormat);
        list->SetColumnType(0, kLCText);
        list->SetColumnType(1, kLCNumber);
        list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(gHireListStrings));
        list->SetCallbackFunction(HirePanelLCHandler);
        list->SetSort(gHireSortColumn, false);
        InterfaceWindow* column = list->GetWindow(0x10);
        MAKECOLUMNBUTTON(column, const_cast<char16_t*>(UIMain_GetString(0x90)), gHireColumnColour, 0xff, 0xef, 0, 0x9f);
        column = list->GetWindow(0x11);
        MAKECOLUMNBUTTON(column, const_cast<char16_t*>(UIMain_GetString(0x91)), gHireColumnColour, 0xff, 0xef, 0, 0xa0);
        SetTypeTitle(gHirePanelType);
        FillHireList(static_cast<StaffType>(gHirePanelType), 0);
        InterfaceCheckButtons* buttons = static_cast<InterfaceCheckButtons*>(list->GetWindow(0x248f));
        if (buttons != nullptr) SelectTypeButton(buttons, gHirePanelType);
    }

    // the header of the list
    InterfaceWindow* header = gHirePanelWindow->GetWindow(0x2481);
    if (header != nullptr) {
        InterfaceWindow* label = header->GetWindow(0x2483);
        label->SetClientDrawObject(NewSprite());
        label->SetFont(GetFontTableEntry(6));
        label->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x92)));
        InterfaceWindow* bar = header->GetWindow(0x2482);
        UIMain_IndentClient(bar, 0, 10);
        void* mem = OperatorNew(0x124);
        if (mem != nullptr) GradientCtor(mem, gHireListStrings + 8);
        bar->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceDrawEngineHGradient*>(mem)));
        WindowSetValue(bar, 0);
    }

    // the summary
    InterfaceWindow* summary = gHirePanelWindow->GetWindow(0x2485);
    if (summary != nullptr) {
        CParkAnalyser* analyser = gGameWorld->GetParkAnalyser();
        TextRow(summary, 0x2488, false, 0x165);
        TextRow(summary, 0x248d, true, 0x166);
        TextRow(summary, 0x248a, true, 0x167);
        TextRow(summary, 0x2486, false, 0x169);
        gHireListDeposits = 0;
        gHireListWord10 = 0;
        gHireListWithdrawals = 0;
        analyser->mHistory1F100[5].GetValue(gHireListDeposits, 0);
        InterfaceWindow* row = CashRow(summary, 0x2489);
        WindowSetValue(row, gHireListDeposits);
        analyser->mHistory1F100[3].GetValue(gHireListWord10, 0);
        row = CashRow(summary, 0x248c);
        WindowSetValue(row, gHireListWord10);
        row->SetFontColour(0xff, 0, 0, 0xff);
        analyser->mHistory1F100[2].GetValue(gHireListWithdrawals, 0);
        gHireListWord14 = gHireListWithdrawals - gHireListWord10;
        row = CashRow(summary, 0x248b);
        WindowSetValue(row, gHireListWord14);
        row->SetFontColour(0xff, 0, 0, 0xff);
        row = CashRow(summary, 0x2487);
        WindowSetValue(row, gHireListDeposits - gHireListWithdrawals);
    }
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xbd));
}
