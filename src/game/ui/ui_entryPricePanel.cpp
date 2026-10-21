// ui_entryPricePanel.cpp: name proven by __sinit_ui_entryPricePanel_cpp in the main application.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "Cheat.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "LoadPanelMsg.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbDynamicStringTemplate.h"
#include "TbTimeDiff.h"
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
#include <cstddef>
#include <cstdint>
#include <new>

// The entry price panel: the title window's message function, the panel's, and the panel's builder.
int16_t gEntryPriceScript[] = {TPW_WINDOW_SCRIPT_102A96A8};   // 0x102a96a8: the image's words (generated, devtools/gen_game_tables.py)
int32_t gEntryPriceClosed;               // 0x10394840: IsParkClosed, as the panel was built
uint32_t gEntryPriceFee;                 // 0x10394844: the admission fee being edited

// The virtual Enable through the table at +0x118; weak so that the test library's recording fake replaces it.
namespace entry_callee {
__attribute__((weak)) void WindowEnable(InterfaceWindow* window, int16_t on) { window->Enable(on); }
}  // namespace entry_callee

namespace {
const char* const kEntryPriceStrings = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1024e01c

void* NewDrawObject(bool cash) {
    void* memory = lender_callee::OperatorNew(0x19c);
    if (memory != nullptr) {
        if (cash) lender_callee::CashCtor(memory);
        else lender_callee::SpriteCtor(memory);
    }
    return memory;
}
}  // namespace

int32_t EntryPricePanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message != 0x1e) {
        if (message < 0x1e) {
            if (message != 0x10) {
                if (message < 0x10) {
                    if (message == 5) {
                        RemoveEntryPricePanel();
                        return 1;
                    }
                } else if (message == 0x14) {
                    gParkScreenWindow = nullptr;
                    gGameWorld->GetBankAccount()->SetAdmissionFee(gEntryPriceFee);
                    UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x12f));
                }
            }
        } else if (message == 0x800) {
            gEntryPriceFee = static_cast<uint32_t>(b);
        } else if (message < 0x800 && message == 0x100) {
            if (a == 0x4f3ac) {
                CreateLoansPanel();
                return 0;
            }
            if (a < 0x4f3ac) {
                if (a < 0) {
                    if (a > -3) {
                        RemoveEntryPricePanel();
                        return 0;
                    }
                } else if (a > 0x4f3aa) {
                    CreateFinancePanel();
                    return 0;
                }
            } else if (a == 0x4f3b1) {
                if (b == 1) gGameWorld->SetParkOpenOrClosed(0, 0);
                else gGameWorld->SetParkOpenOrClosed(1, 0);
            } else if (a < 0x4f3b1 && a < 0x4f3ae) {
                CreateStaffCostsPanel();
                return 0;
            }
        }
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

int32_t EntryPricePanelTitleMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10002) {
        UI_TITLE_COLOUR(window);
    } else if (message < 0x10002) {
        if (message > 0x10000) window->SetFontColour(0xff, 0xff, 0xff, 0xff);
    } else if (message == 0x10006 && a == 0) {
        ShowEditParkNamePanel();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

void CreateEntryPricePanel() {
    if (gParkScreenWindow != nullptr) return;
    UIMain_CheckPanel();
    SetLastPanel(3, 10);
    gParkScreenWindow = gGameWindow->Build(gEntryPriceScript, EntryPricePanelMsg);
    UIMain_SetPanel(gParkScreenWindow);
    gParkScreenWindow->GetWindow(0x4f3af)->SetCallbackFunction(EntryPricePanelTitleMsg);
    uint16_t index = gGameWorld->mGatesThing;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryPriceStrings);
    uint16_t index2 = index;
    char16_t* name = UIMain_GetObjectName(static_cast<CObject*>(gThingArray[index2].mThing));
    UIMain_SetTitleText(gParkScreenWindow, 0x4f3af, name);

    InterfaceWindow* text = gParkScreenWindow->GetWindow(0x4f3b0);
    text->SetClientDrawObject(static_cast<InterfaceDrawObject*>(NewDrawObject(false)));
    text->SetFontJust(1, 1);
    text->SetFont(GetFontTableEntry(6));
    text->SetFontColour(0, 0, 0, 0xff);
    text->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0xa0)));

    InterfaceWindow* cash = gParkScreenWindow->GetWindow(0x4f3ae);
    cash->SetClientDrawObject(static_cast<InterfaceDrawObject*>(NewDrawObject(true)));
    int32_t maximum = 10000;
    int32_t minimum = 0;
    InterfaceVaryBox* box = static_cast<InterfaceVaryBox*>(cash);
    box->SetMinMaxValues(&minimum, &maximum);
    minimum = 1;
    box->SetInc(&minimum);
    cash->SetFont(GetFontTableEntry(6));
    cash->SetFontJust(1, 1);
    cash->SetFontColour(0, 0, 0, 0xff);
    uint32_t fee = gGameWorld->GetBankAccount()->mAdmissionFee;
    gEntryPriceFee = fee;
    lender_callee::WindowSetValue(cash, static_cast<int32_t>(fee));

    InterfaceButton* button = static_cast<InterfaceButton*>(gParkScreenWindow->GetWindow(0x4f3b1));
    gEntryPriceClosed = gGameWorld->IsParkClosed();
    if (gEntryPriceClosed != 0) button->SetState(1, 1);
    else button->SetState(0, 1);
    if (gLoansGameTypeGuard == 0) {
        new (gLoansGameTypeStorage) CGameType();
        gLoansGameTypeGuard = 1;
    }
    if (reinterpret_cast<CGameType*>(gLoansGameTypeStorage)->mType == kGameTypeEasy) {
        entry_callee::WindowEnable(gParkScreenWindow->GetWindow(0x4f3ac), 0);
    }
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x12f));
}

alignas(16) uint8_t gEntryPriceStringStorage[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x1039484c
uint8_t gEntryPriceStringNode[16];   // 0x10394860 (exit record)

// 0x1019da08 (56 bytes)
void RemoveEntryPricePanel() { UI_SendMessage(gParkScreenWindow, 4, 0, 0); }

// ShowEditParkNamePanel 0x1019deac (904 bytes) and TwoEditsMsg 0x1019da6c (1,032 bytes), unit v5057 (ui_entryPricePanel.cpp on the Mac):
// the panel with two edit boxes for the two sign names of the "gates" thing (the park's name), and its message function.

// ---- the unit's own globals (the data is the Mac's; the strings the two cheat pointers lead to are not recovered)
InterfaceWindow* gEditParkNamePanel;      // 0x1039483c
InterfaceWindow* gParkScreenWindow;       // 0x10394848
int16_t gEditParkNameScript[] = {TPW_WINDOW_SCRIPT_102A9834};   // 0x102a9834: the image's words (generated, devtools/gen_game_tables.py)
char16_t gEditSignBufA[15];               // 0x10394782
char16_t gEditSignBufB[15];               // 0x10394764
static const char16_t kCheatTextA[] = u"";
static const char16_t kCheatTextB[] = u"";
const char16_t* gCheatSignA = kCheatTextA;   // 0x102a9940
const char16_t* gCheatSignB = kCheatTextB;   // 0x102a9944

// The Mac's string pool at 0x1024e01c: the assert text, and 0x43 bytes on the name of the draw object of the cursor.
static char kPoolStrings[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n\0hilight";

int32_t TwoEditsMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x802) {
        if (a == 0x3943c) {
            window->GetWindow(0x3943d)->CaptureKb(0);
        } else {
            UI_PostMessage(window, 0x100, -1, 0);
        }
    } else if (message < 0x802) {
        if (message == 0x100) {
            if (a == -1) {
                uint16_t index = gGameWorld->mGatesThing;
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kPoolStrings);
                CObject* object = static_cast<CObject*>(gThingArray[index].mThing);
                object->SetSignNameA(gEditSignBufA);
                object->SetSignNameB(gEditSignBufB);
                RegenerateRideSign(object, gEditSignBufA, gEditSignBufB);
                if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave();
                gMasterSave->SetSignNameA(gEditSignBufA);
                if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave();
                gMasterSave->SetSignNameB(gEditSignBufB);
                // The texts are scrambled with their positions before the cheat compare; the length is asked again on every pass.
                for (uint32_t i = 0; i < wcslen(gEditSignBufA); i++) gEditSignBufA[i] = static_cast<char16_t>(i ^ gEditSignBufA[i]);
                for (uint32_t i = 0; i < wcslen(gEditSignBufB); i++) gEditSignBufB[i] = static_cast<char16_t>(i ^ gEditSignBufB[i]);
                if (wcscmp(gCheatSignA, gEditSignBufA) == 0 && wcscmp(gCheatSignB, gEditSignBufB) == 0) {
                    CCheat* cheat = reinterpret_cast<CCheat*>(gGameWinCheat);
                    if (gGameWinCheatGuard == 0) {
                        new (cheat) CCheat();
                        gGameWinCheatGuard = 1;
                    }
                    cheat->SetCheatStatus(true);
                }
                UI_SendMessage(window, 4, 0, 0);
                uint16_t index2 = gGameWorld->mGatesThing;
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kPoolStrings);
                char16_t* name = UIMain_GetObjectName(static_cast<CObject*>(gThingArray[index2].mThing));
                gParkScreenWindow->GetWindow(0x4f3af)->SetText(name);
                return 0;
            }
            if (a == -2) {
                UI_SendMessage(window, 4, 0, 0);
                return 0;
            }
        } else if (message < 0x100 && message == 0x14) {
            UIMain_EnableAllKeyTables();
            gEditParkNamePanel = nullptr;
            UIMain_DecideKbCapture();
        }
    } else {
        if (message == 0x804) {
            UI_SendMessage(window, 4, 0, 0);
            return 0;
        }
        if (message < 0x804) {
            if (a == 0x3943c) window->GetWindow(0x3943d)->CaptureKb(0);
            else window->GetWindow(0x3943c)->CaptureKb(0);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

void ShowEditParkNamePanel() {
    if (gEditParkNamePanel != nullptr) return;
    int16_t z = gParkScreenWindow->mZ;
    InterfaceWindow* panel = gParkScreenWindow->Build(gEditParkNameScript, TwoEditsMsg);
    gEditParkNamePanel = panel;
    panel->setZ(static_cast<int16_t>(z + 7));

    InterfaceDrawObject* titleObject = new InterfaceTitleTextObject(nullptr);
    panel->GetWindow(0x3943e)->SetClientDrawObject(titleObject);
    InterfaceFont* titleFont = GetFontTableEntry(5);
    panel->GetWindow(0x3943e)->SetFont(titleFont);
    panel->GetWindow(0x3943e)->SetFontJust(1, 1);
    const char16_t* title = UIMain_GetString(0xc2);
    panel->GetWindow(0x3943e)->SetText(const_cast<char16_t*>(title));
    UI_TITLE_COLOUR(panel->GetWindow(0x3943e));

    uint16_t index = gGameWorld->mGatesThing;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kPoolStrings);
    CObject* object = static_cast<CObject*>(gThingArray[index].mThing);

    InterfaceEditBox* editA = static_cast<InterfaceEditBox*>(panel->GetWindow(0x3943c));
    editA->SetClientDrawObject(new InterfaceCursorPosTextObjectSprite(nullptr));
    editA->SetFont(GetFontTableEntry(7));
    editA->SetFontColour(0xff, 0xff, 0xff, 0xff);
    editA->SetStaticTextArea(gEditSignBufA, 0xe);
    editA->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(kPoolStrings + 0x43));
    object->GetSignNameA(gUIMainConvertBuffer, 0x3fff);
    editA->SetText(gUIMainConvertBuffer);
    editA->SelectAll();

    InterfaceEditBox* editB = static_cast<InterfaceEditBox*>(panel->GetWindow(0x3943d));
    editB->SetClientDrawObject(new InterfaceCursorPosTextObjectSprite(nullptr));
    editB->SetFont(GetFontTableEntry(7));
    editB->SetFontColour(0xff, 0xff, 0xff, 0xff);
    editB->SetStaticTextArea(gEditSignBufB, 0xe);
    editB->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(kPoolStrings + 0x43));
    object->GetSignNameB(gUIMainConvertBuffer, 0x3fff);
    editB->SetText(gUIMainConvertBuffer);

    UIMain_DisableAllKeyTables();
    panel->GetWindow(0x3943c)->CaptureKb(0);
}

// 0x1019e260 (64 bytes): builds the global string from the text 0x2a bytes into the cheat text.
void __sinit_ui_entryPricePanel_cpp() {
    TbDynamicStringTemplate<char16_t>* string = reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(gEntryPriceStringStorage);
    new (string) TbDynamicStringTemplate<char16_t>(
        reinterpret_cast<const char16_t*>(reinterpret_cast<const uint8_t*>(gCheatSignA) + 0x2a));
    __register_global_object(string, reinterpret_cast<void*>(&UIMain_DestroyWString), gEntryPriceStringNode);
}

// Park window scripts (main); signed 16-bit records, in image order.

// 0x102a93cc, 54 words
int16_t gCashPanelScript[54] = {
    0, 1, 1, 0, 7000, 0, 474, 1159, 941, 1313,
    0, 2, 1, 0, 7001, 0, 788, 1159, 941, 1313,
    1, -6253, 15046,
    5,
    0, 2, 1, 0, 7002, 0, 474, 1159, 627, 1313,
    1, 26366, -4550,
    5,
    0, 2, 1, 0, 7003, 0, 632, 1159, 785, 1313,
    1, -20670, -5025,
    5,
    5,
    5,
};

// 0x102a9438, 235 words
int16_t gCoasterPanelScript[235] = {
    0, 1, 1, 0, 23, 0, 859, 916, 1534, 1190,
    17, -1, -1,
    18, -1, -1,
    0, 8, 17, 0, 24, 0, 1037, 916, 1407, 1190,
    17, -1, -1,
    18, -1, -1,
    0, 2, 17, 0, 25, 0, 1170, 1065, 1272, 1168,
    17, 569, 0,
    18, -1, -1,
    1, 7320, 11509,
    5,
    0, 2, 17, 0, 26, 0, 1170, 944, 1272, 1046,
    17, 297, 0,
    18, -1, -1,
    1, 5761, 3505,
    5,
    0, 2, 17, 0, 27, 0, 1057, 944, 1159, 1046,
    17, 296, 0,
    18, -1, -1,
    1, 439, 3785,
    5,
    0, 2, 17, 0, 28, 0, 1057, 1065, 1159, 1168,
    17, 300, 0,
    18, -1, -1,
    1, -21803, -4807,
    5,
    0, 2, 17, 0, 29, 0, 1282, 1065, 1385, 1168,
    17, 303, 0,
    18, -1, -1,
    1, -12947, 21058,
    5,
    0, 2, 17, 0, 30, 0, 1282, 944, 1385, 1046,
    17, 302, 0,
    18, -1, -1,
    1, 19820, -14023,
    5,
    1, -12040, -17669,
    0, 2, 17, 0, 31, 0, 877, 944, 979, 1046,
    17, 564, 0,
    18, -1, -1,
    1, -1351, -3753,
    5,
    0, 2, 17, 0, 32, 0, 877, 1065, 979, 1168,
    17, 565, 0,
    18, -1, -1,
    5,
    0, 2, 17, 0, 33, 0, 1432, 944, 1534, 1046,
    17, 304, 0,
    18, -1, -1,
    1, -10857, 9167,
    5,
    5,
    0, 1, 3, 0, 34, 0, 859, 916, 1003, 1190,
    17, -1, -1,
    18, -1, -1,
    1, -12040, -17669,
    5,
    5,
    5,
};

// 0x102a9634, 58 words
int16_t gControlPanelScript[58] = {
    0, 1, 1, 0, 98, 0, 466, 1316, 785, 1469,
    17, -1, -1,
    18, -1, -1,
    0, 2, 1, 0, 99, 0, 474, 1316, 627, 1469,
    17, 479, 0,
    18, -1, -1,
    1, 24279, -24659,
    5,
    0, 2, 1, 0, 100, 0, 632, 1316, 785, 1469,
    17, 480, 0,
    18, -1, -1,
    1, -4573, 21302,
    5,
    5,
    5,
};
