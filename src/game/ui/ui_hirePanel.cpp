// ui_hirePanel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_LoadSave.h"
#include "Game.h"
#include "History.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "LbPalette.h"
#include "LoadPanelMsg.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "StaffDB.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "UI_pump.h"
#include "WindowScripts.inc"
#include "World.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>

// Unit v5984: the hire panel's functions in ui_FinancePanel.cpp / ui_mainpanel2.cpp's neighbourhood:
//   UI_FinancePanelPanelDateChange 0x101a0cf4, FillHireList 0x101a0fc8, UpdateViewWindow 0x101a10f4, HirePanelMsg 0x101a1314,
//   HireListLCFormat (LCFormat) 0x101a1798.

int32_t gHireListFormat;   // 0x10394908

// 0x101a0fc8 (252 bytes): fills the hire list (window 0x248e) with the staff of `type`: name and salary, the slot as the entry's data.
void FillHireList(StaffType type, int32_t flag) {
    if (gHirePanelWindow == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gHirePanelWindow->GetWindow(0x248e));
    if (list == nullptr) return;
    gHirePanelType = static_cast<int32_t>(type);
    if (flag != 0) list->RemoveAllItems();
    for (uint32_t slot = 0; slot < 0x20; ++slot) {
        CStaffInfo* staff = gGameWorld->mStaffDB.GetStaff(slot);
        if (staff != nullptr && static_cast<int32_t>(type) == static_cast<int32_t>(staff->mType)) {
            char16_t name[40];
            staff->GetName(name, 0x27);
            UI_LCAdd item[2] = {};
            item[0].mText = name;
            item[1].mInt = static_cast<int32_t>(staff->GetSalary());
            list->AddItem(item, static_cast<int32_t>(slot), -1, 0);
        }
    }
    list->UpdateDisplay();
}

// 0x101a10f4 (504 bytes): when the staff member of `slot` differs (type or sub type) from the one shown, replaces the preview script in the
// hire panel's view window 0x2484. QUIRK: a type below 0 or above 4 leaves the sprite register as the caller had it (0 here).
void UpdateViewWindow(int32_t slot) {
    CoolRect rect;
    InterfaceWindow* window = gHirePanelWindow->GetWindow(0x2484);
    window->GetClientRect(&rect);
    int32_t left = rect.mLeft, top = rect.mTop, right = rect.mRight, bottom = rect.mBottom;
    int32_t xi = left + ((right - left) >> 1) - 0x3f8;
    int32_t yi = 0x300 - (bottom - ((bottom - top) >> 2));
    float y = static_cast<float>(yi) / 768.0f;
    float x = static_cast<float>(xi) * 0.0009765625f;
    float z = CalcZTrans(static_cast<int32_t>(window->mZ) + 1);
    CStaffInfo* staff = gGameWorld->mStaffDB.GetStaff(static_cast<uint32_t>(slot));
    if (staff != nullptr) {
        int32_t type = static_cast<int32_t>(staff->mType);
        int32_t sub = static_cast<int32_t>(staff->mSubType);
        if (type != gHireListSelA || sub != gHireListSelB) {
            gHireListSelA = type;
            gHireListSelB = sub;
            if (gHireListScript != 0) SCR_KillScript(gHireListScript);
            int32_t sprite = 0;
            if (type == 2) sprite = 4;
            else if (type == 0) sprite = 5;
            else if (type == 1) sprite = 6;
            else if (type == 3) sprite = 7;
            else if (type == 4) sprite = 8;
            gHireListScript = ESP_SpawnScript(gStaffRoomSpawnBlock, static_cast<ESP_Sprites>(sprite), sub, static_cast<ACTS>(1), x, y, z);
            SCR_SetFloatVariable(gHireListScript, 8, 8.0f);
        }
    }
}

// CreateHireListPanel 0x101a1c88 (3316 bytes, ui_mainpanel2.cpp's hire panel): builds the hire panel (or, when it is up already, only
// switches its staff type), fills the cash line, the staff type list with its column buttons and the finance summary below it.

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

// 0x101a1314 (1100 bytes): the hire panel's message function.
int32_t HirePanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        if (a == -2) {
            RemoveHireListPanel();
            return 0;
        }
        if (a == 0x2495) {
            tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> none;
            none.mIndex = 0;
            CreateRideListPanel(static_cast<UIObjType>(-1), none);
            return 0;
        }
    } else if (message < 0x100) {
        if (message == 0x10) {
            if (a == 0x80080) {
                InterfaceWindow* cash = window->GetWindow(0x2496);
                int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
                TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[6]);
                if (balance < 0) {
                    TbIntegerVTag tag(0xd, hirelist_callee::Labs(balance));
                    table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1cb, &tag);
                } else {
                    TbIntegerVTag tag(0xd, balance);
                    table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1ca, &tag);
                }
                hirelist_callee::WindowSetText(cash, gUIMainConvertBuffer);
            }
        } else if (message < 0x10) {
            if (message == 5) {
                RemoveHireListPanel();
                return 1;
            }
        } else if (message == 0x15) {
            if (window->setAnimTimer(0x80080, 1000) != 0) window->startAnimTimer(0x80080);
        } else if (message < 0x15 && message > 0x13) {
            gHirePanelWindow = nullptr;
            if (gHireListScript != 0) SCR_KillScript(gHireListScript);
            gHireListScript = 0;
            gHireListSelA = -1;
            gHireListSelB = -1;
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xbd));
        }
    } else if (message == 0x401) {
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
        if (list != nullptr && b != -1) {
            int32_t slot;
            list->GetData(b, &slot);
            CStaffInfo* staff = gGameWorld->mStaffDB.GetStaff(static_cast<uint32_t>(slot));
            InterfaceWindow* header = gHirePanelWindow->GetWindow(0x2481);
            if (header != nullptr && staff != nullptr) {
                uint32_t grade = staff->mPayGrade;
                InterfaceWindow* bar = header->GetWindow(0x2482);
                hirelist_callee::WindowSetValue(bar, static_cast<int32_t>((grade << 10) & 0x3fc00) / 5);
            }
            CStaffInfo* again = gGameWorld->mStaffDB.GetStaff(static_cast<uint32_t>(slot));
            InterfaceWindow* summary = gHirePanelWindow->GetWindow(0x2485);
            if (summary != nullptr && again != nullptr) {
                int32_t salary = static_cast<int32_t>(again->GetSalary());
                InterfaceWindow* total = summary->GetWindow(0x248c);
                hirelist_callee::WindowSetValue(total, gHireListWord10 + salary);
                InterfaceWindow* left = summary->GetWindow(0x2487);
                hirelist_callee::WindowSetValue(left, gHireListDeposits - (gHireListWord14 + (gHireListWord10 + salary)));
            }
            UpdateViewWindow(slot);
        }
    } else if (message < 0x401) {
        if (message > 0x3ff) {
            InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
            if (list != nullptr && b != -1) {
                int32_t slot;
                list->GetData(b, &slot);
                gGameWorld->mStaffDB.HireStaff(static_cast<uint32_t>(slot));
                RemoveHireListPanel();
                UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x135));
                return 0;
            }
        }
    } else if (message == 0x406) {
        gHireSortColumn = b;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

// The hire panel's list functions (ui_mainpanel2.cpp): HirePanelLCPostFormat 0x101a18a4 (the row attach function) and
// HirePanelLCHandler 0x101a19f8 (the message function of the staff type buttons). Unit v5554.
// Note: src/LCPostFormat.cpp is the LCPostFormat at 0x101c346c, a different function.

uint32_t gHireLCFontIndex = 7;       // 0x102aa31c (name invented): the font table index of the hire list rows
int32_t gHireListScript = 0;         // 0x1039490c (name invented): the script handle the handler kills
int32_t gHireListSelA = -1;          // 0x102aa328 (name invented): reset to -1 by the handler
int32_t gHireListSelB = -1;          // 0x102aa32c (name invented): reset to -1 by the handler

void HirePanelLCPostFormat(InterfaceWindow* window) {
    using namespace lender_callee;
    void* object = OperatorNew(0x19c);
    if (object != nullptr) SpriteCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    window->SetFont(GetFontTableEntry(gHireLCFontIndex));
    InterfaceWindow* child = window->GetChild(0);

    object = OperatorNew(0x19c);
    if (object != nullptr) CashCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    child->SetFont(GetFontTableEntry(gHireLCFontIndex));
    child->SetFontJust(2, 1);
    WindowSetValue(child, 0);
}

static void HireSelect(int32_t type, int32_t stringId) {
    FillHireList(static_cast<StaffType>(type), 1);
    UIMain_SetTitleText(gHirePanelWindow, 0x2480, const_cast<char16_t*>(UIMain_GetString(stringId)));
    if (gHireListScript != 0) SCR_KillScript(gHireListScript);
    gHireListScript = 0;
    gHireListSelA = -1;
    gHireListSelB = -1;
}

int32_t HirePanelLCHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x101 && b == 1) {
        switch (a) {
            case 0x2490: HireSelect(0, 0x8b); break;
            case 0x2491: HireSelect(4, 0x8f); break;
            case 0x2492: HireSelect(2, 0x8d); break;
            case 0x2493: HireSelect(1, 0x8c); break;
            case 0x2494: HireSelect(3, 0x8e); break;
            default: break;
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

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

// 0x101a29b0 (56 bytes)
void RemoveHireListPanel() { UI_SendMessage(gHirePanelWindow, 4, 0, 0); }

// 0x101a1798 (236 bytes): LCFormat of the hire list: a window 100 wide and gHireListFormat high (worked out once from the font) with one
// child (kind 1, id 1, 100 to 200 by 0 to 0x32). QUIRK: no check for a zero screen height on the Mac (the emulator answers the dividend).
InterfaceWindow* HireListLCFormat() {
    if (gHireListFormat == 0) {
        InterfaceFont* font = GetFontTableEntry(gHireLCFontIndex);
        // The Mac calls slot 0xc of the table at the font entry's +0x10 (__ptr_glue); the class is not recovered, so the height getter stands in.
        int32_t height = static_cast<int16_t>(font->GetHeight());
        int32_t screen = gUIRealScreenHeight;
        int32_t product = height * 0x600;
        gHireListFormat = (screen != 0 ? product / screen : product) + 6;
    }
    InterfaceWindow* window = static_cast<InterfaceWindow*>(lender_callee::OperatorNew(0x138));
    if (window != nullptr) {
        lender_callee::WindowCtor(window, nullptr, 0, 0, 0, 0, 100, static_cast<int16_t>(gHireListFormat));
        window->CreateObject(1, 1, 1, 100, 0, 200, 0x32);
    }
    return window;
}

// ui_mainpanel2.cpp (main app), unit v5183: LoadPanelMsg 0x101a2bdc, UI_HirePanelStateChange 0x101a2a14, SavePanelMsg 0x101a2df4.

// Original names unknown.
int32_t gHirePanelType;             // 0x10394920
InterfaceWindow* gHirePanelWindow;  // 0x10394924
static void HireEntry(InterfaceListControl2* list, CStaffDB* db, int slot) {
    CStaffInfo* staff = db->GetStaff(static_cast<uint32_t>(slot));
    char16_t name[40];
    LCWord item[2];
    staff->GetName(name, 0x28);
    item[0].mText = name;
    item[1].mInt = static_cast<int32_t>(staff->GetSalary());
    list->AddItem(item, slot, -1, 0);
}

void UI_HirePanelStateChange(int slot) {
    if (gHirePanelWindow == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gHirePanelWindow->GetWindow(0x248e));
    int32_t position = list->FindData(slot);
    CStaffInfo* staff = reinterpret_cast<CStaffDB*>(gGameWorld)->GetStaff(static_cast<uint32_t>(slot));
    if (position > -1) {
        if (staff == nullptr) {
            list->RemoveItem(position);
            return;
        }
        list->RemoveItem(position);
        HireEntry(list, reinterpret_cast<CStaffDB*>(gGameWorld), slot);
        return;
    }
    if (staff == nullptr) return;
    if (gHirePanelType != static_cast<int32_t>(staff->mType)) return;
    HireEntry(list, reinterpret_cast<CStaffDB*>(gGameWorld), slot);
    list->UpdateDisplay();
}
