// ui_FinancePanel.cpp: name proven by __sinit_ui_FinancePanel_cpp in the main application.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_Online.h"
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
#include "ui_iterators.h"
#include "ui_main.h"
#include "UI_pump.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <new>

// Unit v5904: FeatureStatusMsg 0x1019eae4, __sinit_ui_FeatureStatus_cpp 0x1019eea0, GetMaxYValue 0x1019ef1c, KissMyLines 0x1019f25c.

namespace {

// The Mac's offsets of the eight histories in the park analyser, by selection bit (bit 0 first).
const uint32_t kFinanceHistoryOffset[8] = {0x1f100, 0x1f350, 0x1fc90, 0x1fee0, 0x20130, 0x20380, 0x1f5a0, 0x1f7f0};

CHistory<int32_t, 144>* FinanceHistory(uint8_t* analyser, int bit) {
    return reinterpret_cast<CHistory<int32_t, 144>*>(analyser + kFinanceHistoryOffset[bit]);
}

}  // namespace

// 0x1019ef1c (380 bytes): the largest value of the selected histories.
int32_t GetMaxYValue() {
    int32_t value = 0;
    uint8_t* analyser = reinterpret_cast<uint8_t*>(gGameWorld->GetParkAnalyser());
    for (int bit = 0; bit < 8; bit++)
        if (gFinanceGraphFlags & (1u << bit)) MaxAnalyserValue(FinanceHistory(analyser, bit), value, gFinanceGraphMode);
    return value;
}

namespace {

// The history at a byte offset of the analyser (the offsets are the Mac's).
CHistory<int, 144>* HistoryAt(void* analyser, uint32_t offset) {
    return reinterpret_cast<CHistory<int, 144>*>(static_cast<uint8_t*>(analyser) + offset);
}

}  // namespace

// 0x1019f0bc (380 bytes): the smallest value over the selected series (bit n of the flags); the series are at fixed
// offsets of the analyser, in the order of the bits. Starts from 0 (the answer is 0 or lower than 0 of nothing is lower).
int32_t GetMinYValue() {
    static const uint32_t kOffsets[8] = {0x1f100, 0x1f350, 0x1fc90, 0x1fee0, 0x20130, 0x20380, 0x1f5a0, 0x1f7f0};
    int32_t value = 0;
    void* analyser = gGameWorld->GetParkAnalyser();
    for (int bit = 0; bit < 8; bit++)
        if (gFinanceGraphFlags & (1u << bit)) MinAnalyserValue(HistoryAt(analyser, kOffsets[bit]), value, gFinanceGraphMode);
    return value;
}

// 0x1019f25c (424 bytes): a line for each selected history; bit 0 takes the last colour of gFinancePanelColours.
void KissMyLines(CUIGrapher& grapher) {
    uint8_t* analyser = reinterpret_cast<uint8_t*>(gGameWorld->GetParkAnalyser());
    for (int bit = 0; bit < 8; bit++)
        if (gFinanceGraphFlags & (1u << bit))
            GraphLine(grapher, FinanceHistory(analyser, bit), *reinterpret_cast<TbColour*>(gFinancePanelColours[7 - bit]), 1,
                      gFinanceGraphMode);
}

// Unit v51004: AllStaffPanelMsg 0x10198ac4 (596), UIStatusPanel::RequestMaintenance 0x101932a0 (88), FinancePanelPanelMsg 0x1019f434 (368).

// 0x1019f434: the finance panel's messages.
int32_t FinancePanelPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    switch (message) {
    case 0x15:
        UIMain_SetTitleText(window, 0x12271, const_cast<char16_t*>(UIMain_GetString(0xa1)));
        break;
    case 5:
        RemoveFinancePanelPanel();
        return 1;
    case 0x14:
        gFinanceGraphPanel = nullptr;
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xc5));
        break;
    case 0x100:
        if (a == 0x1227f) { CreateLoansPanel(); return 0; }
        if (a == 0x12281) { CreateEntryPricePanel(); return 0; }
        if (a == 0x12280) { CreateStaffCostsPanel(); return 0; }
        if (a < 0 && a >= -2) { RemoveFinancePanelPanel(); return 0; }
        break;
    default:
        break;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

// FinanceGraphPanelMsg 0x1019f5e4: the message handler of the finance graph panel.

uint32_t gFinanceGraphFlags = 1;
CUIGrapher gFinanceGrapher;
int32_t gFinanceGraphMode = 0;
InterfaceWindow* gFinanceGraphPanel = nullptr;

namespace {

void Redraw(InterfaceWindow* window) {
    int32_t maxY = GetMaxYValue();
    int32_t minY = GetMinYValue();
    gFinanceGrapher.SetYRange(minY, maxY, 10);
    DrawMeGraph(window, gFinanceGrapher, KissMyLines);
}

void Release(InterfaceWindow* parent, int32_t id) {
    static_cast<InterfaceButton*>(parent->GetWindow(id))->SetState(0, 1);
}

// The buttons of the first group (0x12277, 0x1227b): bits 0..1; the second group clears bits 2..7.
void ToggleBit(InterfaceWindow* window, int32_t down, uint32_t bit, uint32_t groupMask, bool secondGroup,
               const int32_t* ids, int n, int32_t firstId) {
    uint32_t& flags = gFinanceGraphFlags;
    if (down == 1) {
        if ((flags & groupMask) != 0) {
            InterfaceWindow* panel = gFinanceGraphPanel->GetWindow(0x12272);
            if (secondGroup) {
                for (int i = 0; i < n; i++) Release(panel, ids[i]);
                flags &= 0xffffff03u;
            } else {
                Release(panel, firstId);
                Release(panel, 0x1227b);
                flags &= 0xfffffffcu;
            }
        }
        flags |= bit;
    } else {
        flags &= ~bit;
    }
    Redraw(window);
}

}  // namespace

int32_t FinanceGraphPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    CoolRect rect;
    static const int32_t kSecond[6] = {0x12279, 0x12278, 0x1227c, 0x1227d, 0x1227a, 0x1227e};
    static const int32_t kSecondB[6] = {0x12279, 0x12278, 0x1227c, 0x1227d, 0x1227a, 0x1227e};
    (void)kSecondB;

    if (message == 0x100) {
        if (a == 0x1227a) {
            ToggleBit(window, b, 0x40, 3, false, nullptr, 0, 0x12277);
        } else if (a < 0x1227a) {
            if (a == 0x12277) {
                ToggleBit(window, b, 1, 0xfc, true, kSecond, 6, 0);
            } else if (a < 0x12277) {
                if (a == -2) {
                    RemoveFinancePanelPanel();
                    return 0;
                }
            } else if (a < 0x12279) {
                ToggleBit(window, b, 8, 3, false, nullptr, 0, 0x12277);
            } else {
                ToggleBit(window, b, 4, 3, false, nullptr, 0, 0x12277);
            }
        } else if (a == 0x1227d) {
            ToggleBit(window, b, 0x20, 3, false, nullptr, 0, 0x12277);
        } else if (a < 0x1227d) {
            if (a < 0x1227c) {
                ToggleBit(window, b, 2, 0xfc, true, kSecond, 6, 0);
            } else {
                ToggleBit(window, b, 0x10, 3, false, nullptr, 0, 0x12277);
            }
        } else if (a < 0x1227f) {
            ToggleBit(window, b, 0x80, 3, false, nullptr, 0, 0x12277);
        }
    } else if (message < 0x100) {
        if (message == 0x15) {
            window->GetClientRect(&rect);
            int32_t maxY = GetMaxYValue();
            int32_t minY = GetMinYValue();
            SetupGraph(gFinanceGrapher, rect, minY, maxY, 10, 0, 0xb, 0xc);
            DrawMeGraph(window, gFinanceGrapher, KissMyLines);
        }
    } else if (message < 0x102) {
        bool hit = false;
        if (a == 0x12275) {
            hit = true;
            gFinanceGraphMode = 0;
        } else if (a < 0x12275) {
            if (a > 0x12273) {
                hit = true;
                gFinanceGraphMode = 2;
            }
        } else if (a < 0x12277) {
            hit = true;
            gFinanceGraphMode = 1;
        }
        if (hit && b == 1) {
            int32_t mode = gFinanceGraphMode;
            if (mode == 1) {
                gFinanceGrapher.SetXRange(0, 0x23, 0x24);
            } else if (mode < 1) {
                if (mode >= 0) gFinanceGrapher.SetXRange(0, 0xb, 0xc);
            } else if (mode < 3) {
                gFinanceGrapher.SetXRange(0, 0x8f, 0x90);
            }
            Redraw(window);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// CreateFinancePanel 0x101a001c (3140 bytes, unit v51026): builds the finance panel under the game window, sets the label texts and the
// starting state of the graph buttons.

int16_t gFinanceScript[] = {TPW_WINDOW_SCRIPT_102A9BEC};   // 0x102a9bec: the image's words (generated, devtools/gen_game_tables.py)

namespace {

// The label set-up the original repeats for every text window: a text sprite as the client draw object, centred, font 0xb, black,
// no text of its own; then the string through the virtual SetText (slot 0x2c of the table at +0x118).
void SetupLabel(InterfaceWindow* window, int32_t stringId) {
    void* mem = parkstatus_callee::OperatorNew(0x19c);
    if (mem != nullptr) parkstatus_callee::SpriteCtor(mem);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(mem));
    window->SetFontJust(1, 1);
    window->SetFont(GetFontTableEntry(0xb));
    window->SetFontColour(0, 0, 0, 0xff);
    window->SetClientDrawText(nullptr);
    parkstatus_callee::WindowSetText(window, UIMain_GetString(stringId));
}

void SetButton(InterfaceWindow* panel, uint32_t bit, int32_t id) {
    if ((gFinanceGraphFlags & bit) != 0) static_cast<InterfaceButton*>(panel->GetWindow(id))->SetState(1, 1);
}

}  // namespace

void CreateFinancePanel() {
    if (gFinanceGraphPanel != nullptr) return;
    UIMain_CheckPanel();
    SetLastPanel(3, 7);
    InterfaceWindow* built = gGameWindow->Build(gFinanceScript, FinancePanelPanelMsg);
    if (built == nullptr) return;
    gFinanceGraphPanel = built;
    UIMain_SetPanel(gFinanceGraphPanel);
    InterfaceWindow* panel = gFinanceGraphPanel->GetWindow(0x12272);
    if (panel != nullptr) {
        void* mem = parkstatus_callee::OperatorNew(0xdc);
        if (mem != nullptr) parkstatus_callee::DrawSpriteCtor(mem);
        panel->SetClientDrawObject(static_cast<InterfaceDrawObject*>(mem));
        panel->SetCallbackFunction(FinanceGraphPanelMsg);
        InterfaceWindow* group = panel->GetWindow(0x12273);
        if (group != nullptr) {
            SetupLabel(group->GetWindow(0x12275), 0x16a);
            SetupLabel(group->GetWindow(0x12276), 0x16b);
            SetupLabel(group->GetWindow(0x12274), 0x16c);
            int32_t mode = gFinanceGraphMode;
            InterfaceCheckButtons* checks = static_cast<InterfaceCheckButtons*>(group);
            if (mode == 1) {
                checks->SetCurrent(0x12276);
            } else if (mode < 1) {
                if (mode >= 0) checks->SetCurrent(0x12275);
            } else if (mode < 3) {
                checks->SetCurrent(0x12274);
            }
        }
        SetupLabel(panel->GetWindow(0x12277), 0xa2);
        SetupLabel(panel->GetWindow(0x1227b), 0xa3);
        SetupLabel(panel->GetWindow(0x12279), 0xa4);
        SetupLabel(panel->GetWindow(0x12278), 0xa5);
        SetupLabel(panel->GetWindow(0x1227c), 0xa6);
        SetupLabel(panel->GetWindow(0x1227d), 0xa7);
        SetupLabel(panel->GetWindow(0x1227a), 0xa8);
        SetupLabel(panel->GetWindow(0x1227e), 0xa9);
        SetButton(panel, 1, 0x12277);
        SetButton(panel, 2, 0x1227b);
        SetButton(panel, 4, 0x12279);
        SetButton(panel, 8, 0x12278);
        SetButton(panel, 0x10, 0x1227c);
        SetButton(panel, 0x20, 0x1227d);
        SetButton(panel, 0x40, 0x1227a);
        SetButton(panel, 0x80, 0x1227e);
        if (gLoansGameTypeGuard == 0) {
            new (gLoansGameTypeStorage) CGameType();
            gLoansGameTypeGuard = 1;
        }
        if (reinterpret_cast<CGameType*>(gLoansGameTypeStorage)->mType == kGameTypeEasy) {
            entry_callee::WindowEnable(gFinanceGraphPanel->GetWindow(0x1227f), 0);
        }
    }
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xc5));
}

// 0x101a0c8c (56 bytes)
void RemoveFinancePanelPanel() { UI_SendMessage(gFinanceGraphPanel, 4, 0, 0); }

// Unit v5984: the hire panel's functions in ui_FinancePanel.cpp / ui_mainpanel2.cpp's neighbourhood:
//   UI_FinancePanelPanelDateChange 0x101a0cf4, FillHireList 0x101a0fc8, UpdateViewWindow 0x101a10f4, HirePanelMsg 0x101a1314,
//   HireListLCFormat (LCFormat) 0x101a1798.

// 0x101a0cf4 (144 bytes): redraws the finance graph with the new range (nothing when the panel is closed). The argument is unused.
void UI_FinancePanelPanelDateChange(int32_t month) {
    (void)month;
    if (gFinanceGraphPanel == nullptr) return;
    int32_t high = GetMaxYValue();
    int32_t low = GetMinYValue();
    gFinanceGrapher.SetYRange(low, high, 10);
    InterfaceWindow* window = gFinanceGraphPanel->GetWindow(0x12272);
    DrawMeGraph(window, gFinanceGrapher, KissMyLines);
}

// __sinit_ui_FinancePanel_cpp 0x101a0dbc: the finance panel's static initialiser.

static void SetColour(uint8_t* c, uint8_t r, uint8_t g, uint8_t b) {
    c[1] = r;
    c[2] = g;
    c[3] = b;
    c[0] = 0xff;
    c[4] = LbPalette_FindColour(&_lbGlobalPalette, r, g, b);
}

void __sinit_ui_FinancePanel_cpp() {
    SetColour(gFinancePanelColours[7], 0xff, 0xff, 0);
    SetColour(gFinancePanelColours[6], 0xff, 0, 0xff);
    SetColour(gFinancePanelColours[5], 0xff, 0, 0);
    SetColour(gFinancePanelColours[4], 0, 0xbd, 0xec);
    SetColour(gFinancePanelColours[3], 0xff, 0x90, 0);
    SetColour(gFinancePanelColours[2], 0x90, 0xd3, 0);
    SetColour(gFinancePanelColours[1], 0xff, 0x75, 0x75);
    SetColour(gFinancePanelColours[0], 0x95, 0x58, 0xd2);
    CUIGrapher_Construct(&gFinanceGrapher);
}
