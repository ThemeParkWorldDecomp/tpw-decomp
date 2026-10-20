#include <cstddef>
#include <cstdint>
#include <new>
#include "Debug.h"
#include "FillLenderList.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "AdvisorScoring.h"
#include "FinanceGraphPanelMsg.h"
#include "LoadPanelMsg.h"
#include "LoansPanelMsg.h"
#include "TbDynamicStringTemplate.h"
#include "UI_pump.h"

// Unit v51109: small stand-ins of the panels (removers, static initialisers, counters).
void RemoveAllKidsPanel();
void __sinit_ui_allKidsPanel_cpp();
void __sinit_ui_StatusPanel_cpp();
void __sinit_ui_allStaffPanel_cpp();
void __sinit_ui_textmanager_cpp();
void __sinit_ui_entryPricePanel_cpp();

// Globals no header names (the Mac's addresses in the comments; names invented).
uint32_t gStatusPanelWord47a8;   // 0x103947a8, set to 0x10000 by __sinit_ui_StatusPanel_cpp
uint32_t gStatusPanelWord47ac;   // 0x103947ac, cleared by it
uint32_t gAllKidsWord47cc;       // 0x103947cc, set to 0x10000 by __sinit_ui_allKidsPanel_cpp
uint32_t gAllKidsWord47d0;       // 0x103947d0, cleared by it
uint32_t gAllStaffWord47f4;      // 0x103947f4, set to 0x10000 by __sinit_ui_allStaffPanel_cpp
uint32_t gAllStaffWord47f8;      // 0x103947f8, cleared by it
uint8_t gCoolRectDtor[8];        // stands for the destructor word 0x102680a0
uint8_t gCoolRectNode[16];       // 0x1038cd68 (exit record of gUIQueryRect)
alignas(16) uint8_t gEntryPriceStringStorage[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x1039484c
uint8_t gEntryPriceStringNode[16];   // 0x10394860 (exit record)

namespace {

// The history at a byte offset of the analyser (the offsets are the Mac's).
CHistory<int, 144>* HistoryAt(void* analyser, uint32_t offset) {
    return reinterpret_cast<CHistory<int, 144>*>(static_cast<uint8_t*>(analyser) + offset);
}

// The window a 32-bit global holds (the panel globals typed as int in the headers).
InterfaceWindow* WindowOf(int32_t word) {
    Ptr32<InterfaceWindow> pointer;
    pointer.mOffset = static_cast<uint32_t>(word);
    return pointer.get();
}

}  // namespace

// 0x10195b40 (48 bytes): the all-rides list's row: the thing goes to the list with no upgrade ride. Answers 1.
int32_t FillAllRidesListCallback(CThing* thing) {
    AddItemToAllRidesList(thing, gAllItemsList, nullptr);
    return 1;
}

// 0x1019592c (28 bytes)
void __sinit_ui_allKidsPanel_cpp() {
    gAllKidsWord47d0 = 0;
    gAllKidsWord47cc = 0x10000;
}

// 0x101956a4 (56 bytes)
void RemoveAllKidsPanel() { UI_SendMessage(gAllKidsMsgWindow, 4, 0, 0); }

// 0x101946ac (28 bytes)
void __sinit_ui_StatusPanel_cpp() {
    gStatusPanelWord47ac = 0;
    gStatusPanelWord47a8 = 0x10000;
}

// 0x1019862c (56 bytes)
void RemoveAllItemsPanel() { UI_SendMessage(gAllItemsPanel, 4, 0, 0); }

// 0x10192a3c (24 bytes): counts the thing. Answers 1.
int32_t StatusPanelIteratorCallback(CThing* thing) {
    (void)thing;
    gStatusPanelListCount = gStatusPanelListCount + 1;
    return 1;
}

// 0x10199e20 (56 bytes)
void RemoveAllStaffPanel() { UI_SendMessage(gStaffPanel, 4, 0, 0); }

// 0x10199e84 (28 bytes)
void __sinit_ui_allStaffPanel_cpp() {
    gAllStaffWord47f8 = 0;
    gAllStaffWord47f4 = 0x10000;
}

// 0x101902e8 (56 bytes)
void __sinit_ui_textmanager_cpp() {
    new (&gUIQueryRect) CoolRect();
    __register_global_object(&gUIQueryRect, gCoolRectDtor, gCoolRectNode);
}

// 0x1019cd00 (64 bytes): both the coaster panel and the edit mode must be set.
void CoasterButtonsDown(uint32_t mask) {
    if (gCoasterPanel != nullptr && gCoasterEditMode != 0) WhichCheckButtonsAreDown(mask);
}

// 0x1019d28c (60 bytes)
void ShowControlPanel() {
    LinkSlideoutPanel(gControlPanel, 0);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x131));
}

// 0x1019d2f0 (52 bytes)
void RemoveControlPanel() {
    RemoveSlideoutPanel(0);
    UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x131));
}

// 0x1019da08 (56 bytes)
void RemoveEntryPricePanel() { UI_SendMessage(gParkScreenWindow, 4, 0, 0); }

// 0x1019e260 (64 bytes): builds the global string from the text 0x2a bytes into the cheat text.
void __sinit_ui_entryPricePanel_cpp() {
    TbDynamicStringTemplate<char16_t>* string = reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(gEntryPriceStringStorage);
    new (string) TbDynamicStringTemplate<char16_t>(
        reinterpret_cast<const char16_t*>(reinterpret_cast<const uint8_t*>(gCheatSignA) + 0x2a));
    __register_global_object(string, reinterpret_cast<void*>(&UIMain_DestroyWString), gEntryPriceStringNode);
}

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

// 0x101a0c8c (56 bytes)
void RemoveFinancePanelPanel() { UI_SendMessage(gFinanceGraphPanel, 4, 0, 0); }

// 0x101a29b0 (56 bytes)
void RemoveHireListPanel() { UI_SendMessage(gHirePanelWindow, 4, 0, 0); }

// 0x101a3934 (56 bytes): the global is typed as an int in the header; it holds the window.
void RemoveLoadPanel() { UI_SendMessage(WindowOf(gLoadPanelOpen), 4, 0, 0); }

// 0x101a3be4 (56 bytes): the same for the save panel.
void RemoveSavePanel() { UI_SendMessage(WindowOf(gSavePanelOpen), 4, 0, 0); }

// 0x101a3c44 (104 bytes): pays the loan off (the answer is not used), then lists the lenders again. Answers 1.
int32_t PaybackLoanOK(void* parameter) {
    gGameWorld->GetBankAccount()->PayOffLoan(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(parameter)));
    FillLenderList(static_cast<InterfaceListControl2*>(gLoansPanel->GetWindow(0x12c4bd)));
    return 1;
}
