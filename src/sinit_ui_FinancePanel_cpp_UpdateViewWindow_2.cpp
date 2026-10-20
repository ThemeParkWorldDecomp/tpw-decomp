// Unit v5984: the hire panel's functions in ui_FinancePanel.cpp / ui_mainpanel2.cpp's neighbourhood:
//   UI_FinancePanelPanelDateChange 0x101a0cf4, FillHireList 0x101a0fc8, UpdateViewWindow 0x101a10f4, HirePanelMsg 0x101a1314,
//   HireListLCFormat (LCFormat) 0x101a1798.
#include <cstdint>

#include "FinanceGraphPanelMsg.h"
#include "FillLenderList.h"
#include "InterfaceFont.h"
#include "LbPalette.h"
#include "LoadPanelMsg.h"
#include "StaffDB.h"
#include "TbITable.h"
#include "Text.h"
#include "TbVTag.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ESP_sprites.h"
#include "MainWindowCallback.h"
#include "ui_controls.h"
#include "ui_main.h"

int32_t gHireListFormat;   // 0x10394908

// 0x101a29b0 (56 bytes): stand-in, nothing translated yet (the test library defines its own).
__attribute__((weak)) void RemoveHireListPanel() {}

// 0x101a0cf4 (144 bytes): redraws the finance graph with the new range (nothing when the panel is closed). The argument is unused.
void UI_FinancePanelPanelDateChange(int32_t month) {
    (void)month;
    if (gFinanceGraphPanel == nullptr) return;
    int32_t high = GetMaxYValue();
    int32_t low = GetMinYValue();
    gFinanceGrapher.SetYRange(low, high, 10);
    InterfaceWindow* window = gFinanceGraphPanel->GetWindow(0x12272);
    DrawMeGraph(window, gFinanceGrapher, reinterpret_cast<void (*)(CUIGrapher&)>(gFinanceKissMyLines));
}

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
