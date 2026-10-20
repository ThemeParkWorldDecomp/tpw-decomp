#include <cstdint>
#include <new>
#include "Cheat.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "WindowScripts.inc"

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
