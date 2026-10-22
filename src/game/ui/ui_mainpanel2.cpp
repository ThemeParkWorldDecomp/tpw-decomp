// ui_mainpanel2.cpp: name proven by __sinit_ui_mainpanel2_cpp in the main application.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "Game.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "keyboard.h"
#include "keytable_actions.h"
#include "LLCellSystem.h"
#include "LoansPanelMsg.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "std_deque_errorthingy.h"
#include "TbGraphicFile.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_parkStatus.h"
#include "ui_Postcard.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include "World.h"
#include <climits>
#include <cstdint>
#include <cstring>
#include <new>

// Unit v5995 (main app, ui_mainpanel2.cpp): the main panel's date display, the last-panel memory, the loans panel refresh and the
// slide-out panel's message function.
//   UI_MainPanelDateChange 0x101a6234 (416 bytes), SetLastPanel 0x101a57fc (72), UI_LoansPanelDateChange 0x101a577c (80),
//   RailsCallback 0x101a6f2c (1932).

// The callees the unit test replaces; the program's own versions are these.
namespace rails_callee {
void RectCtor(void* rect);                                   // 0x101c7448 CoolRect::CoolRect()
void RectDtor(void* rect);                                   // 0x101c74e0 CoolRect::~CoolRect() (flag -1)
void WindowSetValue(InterfaceWindow* window, int32_t value);   // the virtual SetValue (slot 0x1c of the table at +0x118)
InterfaceWindow* WindowFromWord(int32_t word);               // a 32-bit window word as a pointer (the test maps it to its window)
__attribute__((weak)) InterfaceWindow* WindowFromWord(int32_t word) {
    return SlideoutWordWindow(word);   // the panel LinkSlideoutPanel noted (include/ui_main.h)
}
__attribute__((weak)) void RectCtor(void* rect) { new (rect) CoolRect(); }
__attribute__((weak)) void RectDtor(void* rect) { static_cast<CoolRect*>(rect)->~CoolRect(); }
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
}  // namespace rails_callee

namespace {

int32_t WordFromWindow(InterfaceWindow* window) {
    return static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(window)));
}
void* PointerFromInt(int32_t value) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(value)));
}
int32_t IntFromPointer(void* value) {
    return static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value)));
}

// A CoolRect on the stack that the program builds and destroys through the constructor and destructor calls of the original.
struct RectSlot {
    alignas(16) uint8_t bytes[sizeof(CoolRect) + 8];
    CoolRect* get() { return reinterpret_cast<CoolRect*>(bytes); }
};

}  // namespace

// 0x101a57fc (72 bytes): remembers the panel last used of a kind: type 1 the lists, 2 the park panels, 3 the finance panels; others nothing.
void SetLastPanel(int32_t type, int32_t panel) {
    if (type == 2) {
        gLastPanelPark = panel;
        return;
    }
    if (type < 2) {
        if (type < 1) return;
        gLastPanelLists = panel;
        return;
    }
    if (type >= 4) return;
    gLastPanelFinance = panel;
}

// ui_mainpanel2.cpp (main app), unit v5188: GotoLastUsedPanel 0x101a587c, DoCashWin 0x101a5ca8.

int32_t gCashColourIndex;                                    // 0x103949a0
uint8_t gCashInitDone;                                       // 0x103949a4
int32_t gCashSessionCost;                                    // 0x103949a8
int32_t gCashFontCurrent = 999;                              // 0x102aad28
// 0x102aad2c: red, orange, amber, yellow, pale yellow and white, from a balance of 0 or less up to 4001 and more (read from the image).
uint8_t gCashFontColours[24] = {0xff, 0x00, 0x00, 0xff, 0xfa, 0x5a, 0x00, 0xff, 0xff, 0xaf, 0x00, 0xff,
                                0xff, 0xc8, 0x00, 0xff, 0xff, 0xff, 0x96, 0xff, 0xff, 0xff, 0xff, 0xff};

// 0x101a587c (380 bytes): opens the panel last used of the toolbar's group (1 lists, 2 park, 3 finance), unless a menu is up.
void GotoLastUsedPanel(int32_t type) {
    if (MainMenuUp() != 0) return;
    switch (type) {
        case 1:
            if (gLastPanelLists == 2) {
                CreateHireListPanel(static_cast<StaffType>(-1));
            } else if (gLastPanelLists == 1) {
                tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> selected;
                selected.mIndex = 0;   // the original stores 0 into the half of the stack word that is the argument
                CreateRideListPanel(static_cast<UIObjType>(-1), selected);
            }
            break;
        case 2:
            switch (gLastPanelPark) {
                case 3: CreateParkStatusPanel(); break;
                case 4: CreateAllStaffPanel(static_cast<StaffType>(-1), nullptr); break;
                case 5: CreateAllItemsPanel(static_cast<UIObjType>(-1), nullptr); break;
                case 6: CreateAllKidsPanel(nullptr); break;
                default: break;
            }
            break;
        case 3:
            switch (gLastPanelFinance) {
                case 7: CreateFinancePanel(); break;
                case 8: CreateLoansPanel(); break;
                case 9: CreateStaffCostsPanel(); break;
                case 10: CreateEntryPricePanel(); break;
                default: break;
            }
            break;
        default: break;
    }
}

// ui_mainpanel2.cpp (main): the message functions of three windows CreateMainPanel builds (unit w118), and the five of unit w131 (the toolbar's
// buttons, the happy bar, the cash window, the bobble window and the golden stuff window).

// 0x101a5a2c (256 bytes): the toolbar's buttons (window 0x25). A button message (0x100, a = the child's id) opens the panel of that button;
// every id but these six does nothing. Then the default handler, whose answer this is.
int32_t MainPanelButtonsMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        switch (a) {
            case 0x26: GotoLastUsedPanel(1); break;
            case 0x27:
                if (b == 1) ShowControlPanel();
                else RemoveControlPanel();
                break;
            case 0x28: GotoLastUsedPanel(2); break;
            case 0x29: gUI2DMap.CreateMapPanel(); break;
            case 0x2a: GotoLastUsedPanel(3); break;
            case 0x2b: CreateResearchPanel(); break;
            default: break;
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a5b6c (160 bytes): the toolbar (window 0x1d). Message 0x14 (the window is being destroyed) clears gMainPanel, so that the next
// CreateMainPanel builds again; everything else, and then also that one, goes to the default handler, whose answer this is. (The Mac builds a
// CoolRect on the stack that nothing reads.)
int32_t MainPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) gMainPanel = nullptr;
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a5c44 (36 bytes): the gauge window 0x1e. The default handler's answer, nothing else.
int32_t HappyGaugeCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a5ca8 (996 bytes): keeps the cash window up to date: the balance's colour and value, and the session cost of the cell modes.
void DoCashWin(InterfaceWindow* window) {
    if (static_cast<int8_t>(gCashInitDone) == 0) {
        gCashSessionCost = -999;
        gCashInitDone = 1;
    }
    int32_t balance = gGameWorldRef->GetBankAccount()->GetBalance();
    if (balance != gCashWindowWord) {
        gCashColourIndex = (balance + 999) / 1000;
        if (gCashColourIndex < 0) gCashColourIndex = 0;
        if (gCashColourIndex > 5) gCashColourIndex = 5;
        if (gCashColourIndex != gCashFontCurrent) {
            const uint8_t* colour = gCashFontColours + gCashColourIndex * 4;
            window->SetFontColour(colour[0], colour[1], colour[2], 255);
            gCashFontCurrent = gCashColourIndex;
        }
        lender_callee::WindowSetValue(window, balance < 0 ? -balance : balance);   // abs, then the virtual SetValue (slot 0x24)
        CoolRect rect54;
        CoolRect rect48;
        CoolRect rect3c;
        window->GetSizeOfClientDrawObject(&rect54);
        InterfaceWindow* w31 = window->GetWindow(0x31);
        w31->SetPosition(&rect54.mRight, nullptr);
        InterfaceWindow* w30 = window->GetWindow(0x30);
        window->GetPosition(&rect48, 1);
        w30->GetPosition(&rect3c, 1);
        int16_t x = static_cast<int16_t>(rect54.mRight - (rect3c.mRight - rect3c.mLeft));
        w30->SetPosition(&x, nullptr);
        if (balance < 0) window->GetWindow(0x32)->SetFrame(1);
        else window->GetWindow(0x32)->SetFrame(0);
        gCashWindowWord = balance;
    }
    if (gMouse == nullptr) {
        CMouse* mouse = static_cast<CMouse*>(lender_callee::OperatorNew(1));
        if (mouse != nullptr) {
            void* storage = lender_callee::OperatorNew(8);
            CMMDefault* mode = nullptr;
            if (storage != nullptr) mode = new (storage) CMMDefault();
            mouse->AssignMouseMode(mode);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
    int32_t mouseMode = gMouseMode == nullptr ? 0 : gMouseMode->GetMode();
    if (static_cast<uint32_t>(mouseMode - 3) <= 1 || (mouseMode == 8 && IsCoasterLoftMode() != 0)) {
        int32_t cost = LLCC_SystemGetSessionCost();
        if (cost != gCashSessionCost) {
            InterfaceWindow* w = window->GetWindow(0x30);
            if (w != nullptr) {
                if (cost != -999) w->Show(1);
                if (cost == -9999) {
                    w->Show(0);
                } else {
                    lender_callee::WindowSetValue(w, cost);
                    gCashSessionCost = cost;
                }
            }
        }
    } else if (gCashSessionCost != -999) {
        InterfaceWindow* w = window->GetWindow(0x30);
        if (w != nullptr) w->Show(0);
        gCashSessionCost = -999;
    }
}

// 0x101a61d0 (36 bytes): the date window 0x20. The same.
int32_t DateWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a6234 (416 bytes): the date shown in the toolbar's title window 0x20 (day, month, year since 1900 + 0x76c), and the frame of the
// window 0x31 in the cash window 0x2f: 1 when the newest deposits history value is below the newest withdrawals value.
void UI_MainPanelDateChange(int32_t day, int32_t month, int32_t year) {
    if (gMainPanel == nullptr) return;
    InterfaceWindow* title = gMainPanel->GetWindow(0x20);
    ShowStatusTag dayTag(0x10, day);
    ShowStatusTag monthTag(0x11, month);
    ShowStatusTag yearTag(0x12, year + 0x76c);
    char16_t text[0x20];
    static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(text, 0x1f, 0x1c0, &dayTag, &monthTag, &yearTag);
    title->SetClientDrawText(UIMain_ConvertMulti(reinterpret_cast<const unsigned char*>(text)));
    InterfaceWindow* cash = gGameWindow->GetWindow(0x2f);
    InterfaceWindow* gauge = cash->GetWindow(0x31);
    uint8_t* analyser = reinterpret_cast<uint8_t*>(gGameWorldRef->GetParkAnalyser());
    int32_t withdrawals;
    int32_t deposits;
    reinterpret_cast<CHistory<int, 144>*>(analyser + 0x1f5a0)->GetValue(withdrawals, 0);
    reinterpret_cast<CHistory<int, 144>*>(analyser + 0x1fc90)->GetValue(deposits, 0);
    if (deposits < withdrawals) {
        gauge->SetFrame(1);
    } else {
        gauge->SetFrame(0);
    }
}

// 0x102aad44: the animation the bobble window gets when the aerial window moves (the image's frames; the same as gTagShiftAnim).
UI_RectDelta gAerialSlide[15] = {
    {-2, 2, 2, 0}, {-2, 2, 2, 0}, {-2, 2, 2, 0},
    {-2, 2, 2, 0}, {-2, 2, 2, 0}, {-2, 2, 2, 0},
    {4, -4, -4, 0}, {4, -4, -4, 0}, {-2, 2, 2, 0},
    {-2, 2, 2, 0}, {2, -2, -2, 0}, {2, -2, -2, 0},
    {2, -2, -2, 0}, {2, -2, -2, 0}, {kAnimEnd, 0, 0, 0},
};

namespace {
// One level of HideIfUnder: a window whose right edge is above the limit is shown, any other hidden.
void Apply(InterfaceWindow* window, CoolRect& rect, int16_t limit) {
    if (window != nullptr) {
        window->GetPosition(&rect, 1);
        if (rect.mRight <= limit) window->Show(0);
        else window->Show(1);
    }
}

// The Mac passes windows through the message arguments as 32-bit words; on the host such a word is the window's offset in the arena
// (platform/arena.h; 0 is NULL). The tag windows are the exception the port fix below handles (unit v5L39b).
InterfaceWindow* AsWindow(int32_t word) {
    if (word == 0) return nullptr;
    return reinterpret_cast<InterfaceWindow*>(tpw::gArenaBase + static_cast<uint32_t>(word));
}
}  // namespace

// 0x101a68d0 (824 bytes): the aerial window's message function (window 0x2d).
int32_t AerialCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100001) {
        // a window to hang above this one: it is linked in, enabled, and this window moves up by its height
        void* extra0 = window->getExtraParam(0);
        void* extra1 = window->getExtraParam(1);
        if (a != 0) {
            InterfaceWindow* child = AsWindow(a);
            CoolRect aerial;
            CoolRect childRect;
            window->link(child, 1, 0);
            entry_callee::WindowEnable(child, 1);
            UI_PutDeferredMessage(child, 6, 1, 0, 500);
            child->GetPosition(&childRect, 1);
            int16_t width, height;
            child->GetDims(&width, &height, 1);
            height = static_cast<int16_t>(height + 10);
            window->GetPosition(&aerial, 1);
            window->AdjustPosition(0, static_cast<int16_t>(-height), 0, 0);
            window->GetPosition(&aerial, 1);
            int32_t half = (static_cast<int32_t>(aerial.mRight) - aerial.mLeft) >> 1;
            int16_t x = static_cast<int16_t>(aerial.mLeft + (half - (width >> 1)));
            child->ShiftPosition(static_cast<int16_t>(x - childRect.mLeft),
                                 static_cast<int16_t>(aerial.mTop + 8 - childRect.mTop));
            window->setExtraParam(0, extra0);
            window->setExtraParam(1, extra1);
            InterfaceWindow* bobble = window->GetWindow(0x2e);
            if (bobble != nullptr && bobble->mAnimating == 0) bobble->startAnim(gAerialSlide, 0x32);
        }
    } else if (message < 0x100001) {
        if (message == 0x14) gTagFirstWindow = nullptr;
    } else if (message == 0x100003) {
        InterfaceWindow* w = gTagFirstWindow;
        if (w != nullptr) {
            int32_t n = 1;
            for (; w != nullptr; w = w->GetNext()) {
                UI_PutDeferredMessage(w, 5, 1, 0, static_cast<uint32_t>(n * 100));
                n = n + 1;
            }
        }
    } else if (message < 0x100003) {
        if (a != 0) {
            InterfaceWindow* w = AsWindow(a);
            if ((w->GetAttr() & 1) != 0) {
                w->Show(0);
                if (b == 0) UI_PutDeferredMessage(w, 5, 1, 0, 500);
                else UI_SendMessage(w, 5, 1, 0);
            }
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
// 0x101a6c44 (692 bytes): four levels of loops, then the recursion.
void HideIfUnder(InterfaceWindow* window, int16_t limit) {
    CoolRect r0;
    Apply(window, r0, limit);
    for (InterfaceWindow* a = window->GetChild(0); a != nullptr; a = a->GetNext()) {
        CoolRect r1;
        Apply(a, r1, limit);
        for (InterfaceWindow* b = a->GetChild(0); b != nullptr; b = b->GetNext()) {
            CoolRect r2;
            Apply(b, r2, limit);
            for (InterfaceWindow* c = b->GetChild(0); c != nullptr; c = c->GetNext()) {
                CoolRect r3;
                Apply(c, r3, limit);
                for (InterfaceWindow* d = c->GetChild(0); d != nullptr; d = d->GetNext()) HideIfUnder(d, limit);
            }
        }
    }
}

// 0x101a6f2c (1932 bytes): the slide-out panel's message function (window 0x21). Message 0xa: a panel arrives (a is the window); 0xb: the
// slide-out closes (a == -1 when the panel is dropped); 0x10 with timer 0x80080: one animation step; 0x12: a drag of the panel (a is the
// x position); 0x100 with a == 0x24: the panel is removed. Every message goes on to the default handler, whose answer is returned.
int32_t RailsCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    using namespace rails_callee;
    if (message == 0x12) {
        InterfaceWindow* panel = static_cast<InterfaceWindow*>(window->getExtraParam(1));
        if (panel != nullptr) {
            RectSlot slotA;
            RectSlot slotB;
            RectCtor(slotA.bytes);
            RectCtor(slotB.bytes);
            CoolRect* previous = slotA.get();
            CoolRect* parentRect = slotB.get();
            window->GetPreviousPosition(previous);
            int16_t width;
            int16_t height;
            previous->GetDims(&width, &height);
            panel->ShiftPosition(static_cast<int16_t>(a - width), 0);
            panel->GetPosition(previous, 1);
            window->GetParent()->GetPosition(parentRect, 1);
            HideIfUnder(panel, parentRect->mRight);
            RectDtor(slotB.bytes);
            RectDtor(slotA.bytes);
        }
    } else if (message < 0x12) {
        if (message == 0xb) {
            InterfaceWindow* panel = static_cast<InterfaceWindow*>(window->getExtraParam(1));
            if (panel != nullptr && window->setAnimTimer(0x80080, 0x14) != 0) {
                RectSlot slotA;
                RectSlot slotB;
                RectCtor(slotA.bytes);
                RectCtor(slotB.bytes);
                CoolRect* tabRect = slotB.get();    // Mac 0x60
                CoolRect* ownRect = slotA.get();    // Mac 0x6c
                UI_SendMessage(panel, 0x1001d8, 0, 0);
                gSlideoutTab->GetPosition(tabRect, 1);
                window->mF130 = 10;
                window->GetPosition(ownRect, 1);
                int32_t step = (static_cast<int32_t>(ownRect->mLeft) - static_cast<int32_t>(tabRect->mLeft)) /
                               static_cast<int32_t>(window->mF130);
                window->setExtraParam(0, PointerFromInt(static_cast<int16_t>(step)));
                window->startAnimTimer(0x80080);
                if (a == -1) {
                    window->mF134 = 4;
                } else {
                    window->mF134 = 3;
                }
                InterfaceWindow* tab = window->GetWindow(0x24);
                if (tab != nullptr) WindowSetValue(tab, 0);
                SOUND_SendEvent(0, gSoundBanks[3], 0x1c, 0, 0, 0);
                RectDtor(slotB.bytes);
                RectDtor(slotA.bytes);
            }
        } else if (message < 0xb) {
            if (message >= 0xa) {
                if (static_cast<int8_t>(window->mF134) != 0) {
                    if (a != 0) window->setExtraParam(2, PointerFromInt(a));
                } else if (window->setAnimTimer(0x80080, 0x14) != 0) {
                    RectSlot slotA;
                    RectSlot slotB;
                    RectSlot slotC;
                    RectCtor(slotA.bytes);   // Mac 0x94
                    RectCtor(slotB.bytes);   // Mac 0x88
                    RectCtor(slotC.bytes);   // Mac 0x7c
                    CoolRect* mainRect = slotA.get();
                    CoolRect* newRect = slotB.get();
                    CoolRect* ownRect = slotC.get();
                    int16_t width = 0;
                    int16_t height = 0;
                    InterfaceWindow* arrival = rails_callee::WindowFromWord(a);
                    window->mF130 = 10;
                    // link 0x101ddf20 answers an int16 (its translation's linkChecked, which the other callers also use).
                    if (static_cast<int16_t>(window->linkChecked(arrival, 1, 1)) != 0) {
                        arrival->setZ(static_cast<int16_t>(arrival->mZ - 3));
                        gMainPanel->GetPosition(mainRect, 1);
                        arrival->GetPosition(newRect, 1);
                        window->GetPosition(ownRect, 1);
                        newRect->GetDims(&width, &height);
                        int32_t w = width;
                        int32_t h = height;
                        int32_t ownHeight = static_cast<int32_t>(ownRect->mBottom) - static_cast<int32_t>(ownRect->mTop);
                        int32_t top = static_cast<int16_t>(static_cast<int32_t>(ownRect->mTop) + (ownHeight / 2 - h / 2));
                        int32_t mainRight = mainRect->mRight;
                        int32_t left = static_cast<int16_t>((mainRight - w - 0x32) - (mainRight - static_cast<int32_t>(ownRect->mLeft)));
                        arrival->SetPosition(left, top, left + w, top + h);
                        arrival->Show(1);
                        HideIfUnder(arrival, ownRect->mLeft);
                        width = static_cast<int16_t>((static_cast<int32_t>(mainRect->mRight) - static_cast<int32_t>(ownRect->mLeft)) + width + 100);
                        int32_t step = static_cast<int32_t>(width) / static_cast<int32_t>(window->mF130);
                        window->setExtraParam(0, PointerFromInt(static_cast<int16_t>(step)));
                        window->setExtraParam(1, arrival);
                        window->startAnimTimer(0x80080);
                        window->mF134 = 2;
                        InterfaceWindow* tab = window->GetWindow(0x24);
                        if (tab != nullptr) WindowSetValue(tab, 1);
                        SOUND_SendEvent(0, gSoundBanks[3], 0x1c, 0, 0, 0);
                        UI_SendMessage(arrival, 0x1001d9, 0, 0);
                    }
                    RectDtor(slotC.bytes);
                    RectDtor(slotB.bytes);
                    RectDtor(slotA.bytes);
                }
            }
        } else if (message == 0x10 && a == 0x80080) {
            int16_t step = static_cast<int16_t>(IntFromPointer(window->getExtraParam(0)));
            RectSlot slot;
            RectCtor(slot.bytes);
            CoolRect* rect = slot.get();
            if (static_cast<int32_t>(window->mF130) == 0) {
                window->stopAnimTimer(0x80080);
                window->GetOriginalPosition(rect);
                InterfaceWindow* panel = static_cast<InterfaceWindow*>(window->getExtraParam(1));
                if (panel != nullptr) {
                    int32_t state = static_cast<int8_t>(window->mF134);
                    if (state == 3) {
                        panel->Show(0);
                        panel->unlink();
                        window->setExtraParam(1, nullptr);
                        window->SetPosition(rect);
                        UI_SendMessage(panel, 0x1d, 0, 0);
                        UIMain_UISound(0xc2);
                    } else if (state == 4) {
                        panel->Show(0);
                        panel->unlink();
                        window->setExtraParam(1, nullptr);
                        window->SetPosition(rect);
                        UI_PostMessage(window->GetParent(), 0xb, 0, 0);
                    } else {
                        UI_SendMessage(panel, 0x1d, 1, 0);
                    }
                }
                window->mF134 = 0;
                int32_t pending = IntFromPointer(window->getExtraParam(2));
                if (pending != 0) {
                    UI_PostMessage(window, 0xa, pending, 0);
                    window->setExtraParam(2, nullptr);
                }
            } else {
                window->mF130 = window->mF130 - 1;
                window->GetPosition(rect, 1);
                rect->mRight = static_cast<int16_t>(rect->mRight + step);
                window->SetPosition(rect);
            }
            RectDtor(slot.bytes);
        }
    } else if (message == 0x100 && a == 0x24) {
        RemoveSlideoutPanel(1);
    }
    (void)WordFromWindow;
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a76f0 (264 bytes): the happy bar (window 0x1f). Created (0x15): its animation timer 0x80083 of two seconds, started when it was made.
// The timer firing (0x10 with a = 0x80083): when the world has a park analyser (and the window is not NULL), the bar shows the analyser's
// happiness 4 as a part of 1024: 1024 * level / 100, in single precision (fmuls, then fdivs) and truncated (a NaN or too large a number
// saturates like fctiwz). Then the default handler's answer.
int32_t HappyBarMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x15) {
        if (window->setAnimTimer(0x80083, 2000) != 0) window->startAnimTimer(0x80083);
    } else if (message == 0x10 && a == 0x80083) {
        CParkAnalyser* analyser = (*gGameWorldRef).GetParkAnalyser();
        if (analyser != nullptr && window != nullptr) {
            const float level = CParkAnalyser_GetHappinessLevel(analyser, 4);
            const float scaled = (1024.0f * level) / 100.0f;
            window->SetValue(ppc_fctiwz(scaled));
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

int8_t gCashWindowGuard;       // 0x10394998
uint32_t gCashWindowFrames;    // 0x1039499c
int32_t gCashWindowWord;       // 0x103949ac

// 0x101a60c0 (208 bytes): the cash window (window 0x2f). Its frame counter (a function static, set to 0 the first time through) counts the
// on-frame messages (0x1e); the counter's value before the increment having bit 1 set (and bit 0 of the pair, "== 2") runs DoCashWin and
// starts the count again; every frame the session cost is set to 0. Created (0x15): the word at 0x103949ac and the counter go to 0.
int32_t CashWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (gCashWindowGuard == 0) {
        gCashWindowFrames = 0;
        gCashWindowGuard = 1;
    }
    if (message == 0x1e) {
        const uint32_t before = gCashWindowFrames;
        gCashWindowFrames = before + 1;
        if ((before & 2) == 2) {
            DoCashWin(window);
            gCashWindowFrames = 0;
        }
        LLCC_SystemSetSessionCost(0);
    } else if (message == 0x15) {
        gCashWindowWord = 0;
        gCashWindowFrames = 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a67f4 (160 bytes): the bobble window (window 0x2e). Message 0x10006 with a = 1 posts the aerial window the message 0x100003 (a, b = 0).
int32_t BobbleCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006 && a == 1) UI_PostMessage(gAerialWindow, 0x100003, 0, 0);
    return window->CallDefaultWindowHandler(message, a, b);
}

int8_t gGoldenSpendGuard;      // 0x10394990
int32_t gGoldenLastSpending;   // 0x10394994
int8_t gGoldenKeysGuard;       // 0x10394988
int32_t gGoldenLastKeys;       // 0x1039498c

// The master save is made the first time something wants it, and every use of the original tests for it again.
static CFSGlobalMasterSave* MasterSave() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    return gMasterSave;
}

// 0x101a6404 (948 bytes): the golden stuff window (window 0x33): the number of golden keys (children 0x36, which holds the text, and 0x34) and the spending power (0x37, with the text, and 0x35)
// of the master save, redrawn when they change. The two "last shown" values (-1 first, and
// when the window is created, 0x15) sit behind guard bytes that every call tests first. On-frame (0x1e): when the game is not the easy kind
// (type 2) the keys are looked at: a changed count (HowManyKeys is asked again for the store, for the test against 0 and for the text) hides
// both children at 0 and else shows both and puts "%d  x" in 0x36. The spending power is looked at whatever the game type: changed, it
// hides both children at 0, else shows both and puts it in 0x37 as text (the same "%d  x").
int32_t GoldenStuffMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (gGoldenSpendGuard == 0) {
        gGoldenLastSpending = -1;
        gGoldenSpendGuard = 1;
    }
    if (gGoldenKeysGuard == 0) {
        gGoldenLastKeys = -1;
        gGoldenKeysGuard = 1;
    }
    if (message == 0x1e) {
        char16_t text[8];
        if (TheGameType().mType != kGameTypeEasy) {
            if (gGoldenLastKeys != MasterSave()->HowManyKeys()) {
                gGoldenLastKeys = MasterSave()->HowManyKeys();
                if (MasterSave()->HowManyKeys() == 0) {
                    window->GetWindow(0x36)->Show(0);
                    window->GetWindow(0x34)->Show(0);
                } else {
                    window->GetWindow(0x36)->Show(1);
                    window->GetWindow(0x34)->Show(1);
                    swprintf(text, 8, u"%d  x", MasterSave()->HowManyKeys());
                    window->GetWindow(0x36)->SetText(text);
                }
            }
        }
        const int32_t spending = MasterSave()->SpendingPower();
        if (gGoldenLastSpending != spending) {
            gGoldenLastSpending = spending;
            if (spending == 0) {
                window->GetWindow(0x37)->Show(0);
                window->GetWindow(0x35)->Show(0);
            } else {
                window->GetWindow(0x37)->Show(1);
                window->GetWindow(0x35)->Show(1);
                swprintf(text, 8, u"%d  x", gGoldenLastSpending);
                window->GetWindow(0x37)->SetText(text);
            }
        }
    } else if (message == 0x15) {
        gGoldenLastSpending = -1;
        gGoldenLastKeys = -1;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// UIInterface's mouse, research button, kid and staff room events (0x101f0d14..0x101f10f8) and the error dialog's queue
// (ErrorDlgCancel 0x101f1cd0, AddUIError 0x101f1df0, the block table's destructor 0x101f2520). The inventory files them under
// ui_ErrorDisplay.cpp.

// Unit v51122: the keyboard action wrappers (each calls one UIInterface member and answers 1), context_help, and small
// ui_mainpanel2 / panel remove functions.

// 0x101a7830 (16 bytes: blr, the argument is not used; the weak copy in src/UI_pump_UIInterface_CamcorderMode.cpp yields to this one)
void MainPanel_EnableResearchButton(bool) {}

// ui_mainpanel2.cpp (main): CreateMainPanel 0x101a7878 (2,636 bytes), the game's main toolbar and the HUD windows around it (unit w118).
//
// Builds the window script gMainPanelScript under the game window (not the root window) and dresses what it made: the golden-stuff window
// at the top right (0x33), the small window above the toolbar (0x2d with 0x2e), the number window at the top left (0x2f with the cash window
// 0x30) and the toolbar itself (0x1d: gauge bar 0x1f, title 0x20, slide-out panel 0x21 with its tab, button group 0x25). gMainPanel is the
// guard (a second call builds nothing) and is stored last, only when window 0x1d exists.
//
// The listing keeps its order and its quirks: the globals are reloaded before each use, a window answer is tested only where the original
// tests it (the windows 0x37, 0x36, 0x34, 0x32, 0x1e, 0x20 and 0x25 are used unchecked), every `new` is the nothrow form whose constructor
// call is skipped on NULL, and every 16-bit sum is computed in 32 bits and cut by the store.

InterfaceWindow* gMainPanel;          // 0x103949d8
InterfaceWindow* gGoldenStuffPanel;   // 0x103949c4
InterfaceWindow* gAerialWindow;       // 0x103949d4
InterfaceWindow* gSlideoutTab;        // 0x103949c8
int32_t gMainPanelFontIndex;          // 0x103949b4
int32_t gLastPanelLists;              // 0x103949c0
int32_t gLastPanelPark;               // 0x103949bc
int32_t gLastPanelFinance;            // 0x103949b8

namespace {

// The two texts of the string pool at 0x1024e12e (+0xc and +0x16): the golden-stuff counters' first text, and the widest number the
// cash window can show (the Mac's characters are 16 bit).
char16_t kCounterText[] = u"0  x";      // 0x1024e13a
char16_t kWidestNumber[] = u"999999999";   // 0x1024e144
const char kGaugeMesh[] = "meter.wct";   // 0x1024e124

// divw: the quotient truncated towards zero; the dividend where the quotient is undefined (the emulator's answer, as in src/rsse_tick.cpp).
inline int32_t Div(int32_t a, int32_t b) {
    if (b == 0 || (a == INT32_MIN && b == -1)) return a;
    return a / b;
}

// A counter text of the golden-stuff window (0x37, and 0x36 when the game is not the easy one): its own text sprite, white, font 2, right
// aligned, showing "0  x". The listing makes it twice with the same calls.
void DressCounter(InterfaceWindow* counter) {
    InterfaceTextObjectSprite* sprite = new (std::nothrow) InterfaceTextObjectSprite(nullptr);
    counter->SetClientDrawObject(sprite);
    counter->SetFontColour(255, 255, 255, 255);
    counter->SetFont(GetFontTableEntry(2));
    counter->SetClientDrawText(nullptr);
    counter->SetFontJust(2, 1);
    counter->SetText(kCounterText);
}

}  // namespace

void CreateMainPanel() {
    if (gMainPanel != nullptr) return;
    CoolRect outer;
    gLastPanelLists = 1;
    gLastPanelPark = 3;
    gLastPanelFinance = 10;
    if (gGameWindow->Build(gMainPanelScript, nullptr) != nullptr) {
        // The golden-stuff window.
        gGoldenStuffPanel = gGameWindow->GetWindow(0x33);
        gGoldenStuffPanel->setZ(0);
        gGoldenStuffPanel->Enable(0);
        gGoldenStuffPanel->SetCallbackFunction(GoldenStuffMsg);
        InterfaceWindow* counter = gGoldenStuffPanel->GetWindow(0x37);
        counter->setZ(0);
        DressCounter(counter);
        // (the guard byte of the compiler's static: the type is read from the game options on first use)
        if (static_cast<int32_t>(TheGameType().mType) == kGameTypeEasy) {
            gGoldenStuffPanel->GetWindow(0x34)->Show(0);
        } else {
            counter = gGoldenStuffPanel->GetWindow(0x36);
            counter->setZ(0);
            DressCounter(counter);
            gGoldenStuffPanel->GetWindow(0x34)->SetFrame(3);
        }

        // The small window above the toolbar: its lower neighbour moves up to sit 8 units above the window's bottom edge.
        gAerialWindow = gGameWindow->GetWindow(0x2d);
        if (gAerialWindow != nullptr) {
            InterfaceWindow* bobble = gAerialWindow->GetWindow(0x2e);
            if (bobble != nullptr) {
                gAerialWindow->SetCallbackFunction(AerialCallback);
                CoolRect above;
                CoolRect below;
                gAerialWindow->GetPosition(&above, 1);
                bobble->GetPosition(&below, 1);
                bobble->ShiftPosition(0, static_cast<int16_t>(above.mTop - below.mBottom));
                bobble->SetCallbackFunction(BobbleCallback);
                above.mTop = static_cast<int16_t>(above.mBottom - 8);
                gAerialWindow->SetPosition(&above);
            }
        }

        // The number window: sized to the widest number the font can show, then given its text objects.
        InterfaceWindow* number = gGameWindow->GetWindow(0x2f);
        if (number != nullptr) {
            number->setZ(0);
            number->SetCallbackFunction(CashWindowCallback);
            TbRect measured = {0, 0, 0, 0};
            CoolRect numberRect;
            CoolRect imageRect;
            int32_t justification = 0;
            AdjustJustificationForHebrew(justification);
            TbRect box;
            box.left = 0;
            box.top = 0;
            box.right = 65000;
            box.bottom = 65000;
            UI_MeasureText(kWidestNumber, box, 1, static_cast<int8_t>(justification), 0, 0, &measured);
            // the height in the UI's 1536 units, plus 16 (the multiply wraps in 32 bits like the original's mulli)
            const int32_t heightProduct = static_cast<int32_t>(static_cast<uint32_t>(measured.bottom) * 0x600u);
            const int32_t textHeight = Div(heightProduct, gUIRealScreenHeight) + 0x10;
            number->GetPosition(&numberRect, 1);
            InterfaceWindow* image = number->GetWindow(0x32);
            image->GetPosition(&imageRect, 1);
            const int32_t imageHeight = imageRect.mBottom - imageRect.mTop;
            const int32_t widthProduct = static_cast<int32_t>(static_cast<uint32_t>(measured.right) << 11);
            const int32_t textWidth = Div(widthProduct, gUIRealScreenWidth);
            numberRect.mRight = static_cast<int16_t>(numberRect.mLeft + textWidth);
            numberRect.mTop = static_cast<int16_t>(imageRect.mTop + (imageHeight / 2 - textHeight / 2));
            numberRect.mBottom = static_cast<int16_t>(numberRect.mTop + textHeight);
            number->SetPosition(&numberRect);
            InterfaceNumberTextObject* numbers = new (std::nothrow) InterfaceNumberTextObject(nullptr);
            number->SetClientDrawObject(numbers);
            number->SetFont(GetFontTableEntry(1));
            number->SetFontJust(justification, 1);
            number->SetFontColour(255, 255, 255, 255);
            justification = 2;
            AdjustJustificationForHebrew(justification);
            InterfaceWindow* cash = number->GetWindow(0x30);
            if (cash != nullptr) {
                cash->setZ(0);
                InterfaceCashAndGTickTextObject* cashText = new (std::nothrow) InterfaceCashAndGTickTextObject(nullptr);
                cash->SetClientDrawObject(cashText);
                cash->SetFont(GetFontTableEntry(2));
                cash->SetFontJust(justification, 1);
                cash->SetFontColour(255, 255, 0, 255);
                cash->Show(0);
                cash->Enable(0);
            }
            InterfaceWindow* picture = number->GetWindow(0x31);
            if (picture != nullptr) picture->Enable(0);
            number->Enable(0);
        }
    }

    // The toolbar.
    InterfaceWindow* toolbar = gGameWindow->GetWindow(0x1d);
    if (toolbar != nullptr) {
        toolbar->SetCallbackFunction(MainPanelMsg);
        CoolRect toolbarRect;
        toolbar->setZ(8);
        toolbar->GetPosition(&toolbarRect, 1);   // (never used)
        InterfaceWindow* slideout = toolbar->GetWindow(0x21);
        if (slideout != nullptr) {
            InterfaceWindow* tabBar = slideout->GetWindow(0x22);
            if (tabBar != nullptr) {
                InterfaceWindow* tab = tabBar->GetWindow(0x23);
                if (tab != nullptr) {
                    gSlideoutTab = tab;
                    slideout->SetCallbackFunction(RailsCallback);
                    CoolRect slideoutRect;
                    CoolRect tabRect;
                    slideout->GetPosition(&slideoutRect, 1);
                    tab->GetPosition(&tabRect, 1);
                    slideoutRect.mRight = static_cast<int16_t>(slideoutRect.mLeft + (slideoutRect.mRight - tabRect.mLeft));
                    slideout->SetPosition(&slideoutRect);
                    slideout->SetOriginalPosition(&slideoutRect);
                    slideout->setZ(4);
                    tabBar->setZ(4);
                    gSlideoutTab->setZ(9);
                    InterfaceWindow* button = slideout->GetWindow(0x24);
                    if (button != nullptr) {
                        button->setZ(7);
                        button->Enable(0);
                    }
                }
            }
        }
        // The happy bar: the gauge window 0x1e (unchecked) and its vertical gradient.
        InterfaceWindow* gauge = toolbar->GetWindow(0x1e);
        gauge->SetCallbackFunction(HappyGaugeCallback);
        InterfaceWindow* bar = gauge->GetWindow(0x1f);
        if (bar != nullptr) {
            InterfaceDrawEngineVGradient* gradient = new (std::nothrow) InterfaceDrawEngineVGradient(const_cast<char*>(kGaugeMesh));
            bar->SetDrawObject(gradient);
            bar->SetCallbackFunction(HappyBarMsg);
            bar->Enable(0);
        }
        // The date window 0x20 (unchecked): its message function is set before its draw object.
        InterfaceWindow* date = toolbar->GetWindow(0x20);
        InterfaceTextObjectSprite* dateText = new (std::nothrow) InterfaceTextObjectSprite(nullptr);
        date->SetCallbackFunction(DateWindowCallback);
        date->SetClientDrawObject(dateText);
        gMainPanelFontIndex = 3;
        date->SetFont(GetFontTableEntry(static_cast<uint32_t>(gMainPanelFontIndex)));
        date->SetFontColour(0, 0, 0, 255);
        date->SetFontJust(1, 1);
        toolbar->GetWindow(0x25)->SetCallbackFunction(MainPanelButtonsMsg);
        gMainPanel = toolbar;
    }
}

// LinkSlideoutPanel 0x101a82ec and RemoveSlideoutPanel 0x101a83d8: the main panel's slide-out slot (child 0x21).

// 0x101a82ec (176 bytes). With the main panel and a panel given and the slot (child 0x21) found: when the slot's extra parameter 2
// is non-zero the answer is NULL; else the slot is sent 0xb (0, 0) and posted 0xa with the panel as its first argument. Every other
// path answers the panel itself. (The second argument is never read.)
void LinkSlideoutPanel(InterfaceWindow* panel, int32_t n) {
    LinkSlideoutPanelResult(panel, n);
}

// The same function with r3 kept: the answer the Mac leaves (the panel, or NULL). The header declares LinkSlideoutPanel as void because
// another unit's fake (tests/unit_CoasterEditMode.cpp) defines it so; callers ignore the answer.
InterfaceWindow* LinkSlideoutPanelResult(InterfaceWindow* panel, int32_t n) {
    (void)n;
    if (gMainPanel != nullptr && panel != nullptr) {
        InterfaceWindow* slot = gMainPanel->GetWindow(0x21);
        if (slot != nullptr) {
            if (slot->getExtraParam(2) != nullptr) return nullptr;
            UI_SendMessage(slot, 0xb, 0, 0);
            NoteSlideoutWord(panel);   // host only: the word below is the pointer's low 32 bits
            UI_PostMessage(slot, 0xa, static_cast<int32_t>(reinterpret_cast<intptr_t>(panel)), 0);
        }
    }
    return panel;
}

// 0x101a83d8 (256 bytes). Needs the game window and the main panel. The slot (child 0x21): when its byte at +0x134 (signed) is 3 or 4,
// extra parameter 2 is cleared if set; else it is sent 0xb (0, 0). Then, for a non-zero `which`, child 0x27 of child 0x25 of the main
// panel is set to state 0, redraw 1 (no NULL checks, as on the Mac).
void RemoveSlideoutPanel(int16_t which) {
    if (gGameWindow == nullptr) return;
    if (gMainPanel == nullptr) return;
    InterfaceWindow* slot = gMainPanel->GetWindow(0x21);
    if (slot != nullptr) {
        const int8_t kind = static_cast<int8_t>(slot->mF134);
        if (kind == 3 || kind == 4) {
            if (slot->getExtraParam(2) != nullptr) slot->setExtraParam(2, nullptr);
        } else {
            UI_SendMessage(slot, 0xb, 0, 0);
        }
    }
    if (which != 0) {
        InterfaceWindow* a = gMainPanel->GetWindow(0x25);
        InterfaceButton* button = static_cast<InterfaceButton*>(a->GetWindow(0x27));
        button->SetState(0, 1);
    }
}

// ui_mainpanel2.cpp (main app 0x101a8504, 0x101a86d0, 0x101a87c4; unit w88): the pause window. (HidePauseWindow and gPauseWindow are in
// ui_mainpanel2_pause.cpp.)

// 0x101a8504 (400 bytes). Message 0x14: the window is forgotten (no other message clears it). Message 0x15: the window's rectangle, with
// the animation's offsets taken off, gives the pivot: the halves of the RIGHT and BOTTOM edges (the two shorts at +8 and +10 of the
// rectangle, shifted right arithmetically), and the animation starts with 0x23. Message 0x1000b (a key): when the system key table is off
// (its enabled word is 0), the table is searched for the key (the low 16 bits of `a`) with 1 as the modifiers when bits 16..23 of `b` are 0,
// else 0; a non-zero answer (as a short) toggles the game's pause (false: not all sounds). Every message ends in the default dialog
// callback, whose answer is the answer (the Mac's decompile says void; r3 is left as the callback answered it).
int32_t PauseWindowMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) {
        gPauseWindow = nullptr;
    } else if (message == 0x15) {
        CoolRect rect;
        window->GetPosition(&rect, 1);
        window->setPivot(static_cast<int16_t>(rect.mRight >> 1), static_cast<int16_t>(rect.mBottom >> 1));
        window->startAnim(gPauseAnim, 0x23);
    } else if (message == 0x1000b) {
        if (gKeyTable->mEnabled == 0) {
            const int16_t found =
                static_cast<int16_t>(gKeyTable->Search(static_cast<int16_t>(a), ((static_cast<uint32_t>(b) >> 16) & 0xff) == 0 ? 1 : 0));
            if (found != 0) gGame.TogglePause(false);
        }
    }
    return DefaultDialogCallback(window, message, a, b);
}
// 0x101a86d0 (76 bytes): makes the pause window (Dialog with the pause script and message function, modal) when there is none. Dialog may
// answer NULL, and then there is still none.
void ShowPauseWindow() {
    if (gPauseWindow == nullptr) gPauseWindow = Dialog(gPauseDialogDescription, PauseWindowMsg, 1);
}

// ui_mainpanel2.cpp (main app, SimTheme Park 2.04b3): the pause window's global and HidePauseWindow. The window itself is made and handled
// in ui_mainpanel2_window.cpp (unit w88: ShowPauseWindow, TogglePauseWindow, PauseWindowMsg).

// 0x103949cc (the original name is unknown): the pause window, set by ShowPauseWindow and TogglePauseWindow; NULL when none is up.
InterfaceWindow* gPauseWindow;

// 0x101a8744 (88 bytes). Sends the pause window message 4 (its close) when there is one, forgets it (the pointer is cleared even
// when there was none) and has the keyboard capture decided again. UnPauseUI calls it. The Mac clears the global through the
// address it loaded before the send, which is the same word.
void HidePauseWindow() {
    if (gPauseWindow != nullptr) UI_SendMessage(gPauseWindow, 4, 0, 0);
    gPauseWindow = nullptr;
    UIMain_DecideKbCapture();
}

// 0x101a87c4 (128 bytes): makes the window when there is none, else closes it: message 4, the pointer cleared, the keyboard capture decided.
void TogglePauseWindow() {
    if (gPauseWindow == nullptr) {
        gPauseWindow = Dialog(gPauseDialogDescription, PauseWindowMsg, 1);
    } else {
        UI_SendMessage(gPauseWindow, 4, 0, 0);
        gPauseWindow = nullptr;
        UIMain_DecideKbCapture();
    }
}

// Unit v5143 (Mac file ui_mainpanel2.cpp): the bumhole window's message function, the game window toggle and SetOnOffText.

int32_t gGameWindowToggled;        // 0x10394984
// 0x101a886c (544 bytes).
int32_t bumhole(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10005) {
        if (a == 1) window->CaptureDevice();
    } else if (message < 0x10005 && message >= 0x10004) {
        if (a == 1) window->UnCaptureDevice();
    }
    Will_HandleCameraMessages(message, a, b);

    if (message == 0x1000a) {
        gKeyTableCamera->KeyDown(static_cast<int16_t>(a), static_cast<int16_t>((b >> 16) & 0xff));
    } else if (message < 0x1000a) {
        if (message == 0x10005) {
            int16_t shifts = CheckShifts();
            gKeyTableCamera->KeyDown(static_cast<int16_t>(a - 0x10), shifts);
        } else if (message < 0x10005 && message >= 0x10004) {
            int16_t shifts = CheckShifts();
            gKeyTableCamera->KeyUp(static_cast<int16_t>(a - 0x10), shifts);
        }
    } else if (message < 0x1000c) {
        int32_t mods = (b >> 16) & 0xff;
        gKeyTableCamera->KeyUp(static_cast<int16_t>(a), static_cast<int16_t>(mods));
        if (gGameWorld->mF1DA738 == 4) {
            if (a == 0x1b && mods == 0) CreateMainMenu(0);
        } else {
            int16_t found = static_cast<int16_t>(gKeyTableGame->Search(static_cast<int16_t>(a), static_cast<int16_t>(mods)));
            if (found == 4 || (a == 0x1b && mods == 0)) {
                ToggleGameWindow();
            } else {
                found = static_cast<int16_t>(gKeyTableShortcuts->Search(static_cast<int16_t>(a), static_cast<int16_t>(mods)));
                if (found == 0xf) UI_DoPostcard();
            }
        }
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a8ac0 (308 bytes).
void ToggleGameWindow() {
    if (gGameWindow == nullptr) return;
    if (gGameWindowToggled == 0) {
        if ((gFirstPersonFlags & 0x16) != 0) return;
        if (gBumholeWindow == nullptr) {
            gBumholeWindow = gUIRootWindow->CreateObject(1, 1, 0xddf22, 0, 0, 0x7ff, 0x5ff);
            if (gBumholeWindow != nullptr) {
                gBumholeWindow->SetCallbackFunction(bumhole);
                gBumholeWindow->CaptureKb(0);
            }
        }
    } else {
        if (gBumholeWindow != nullptr) delete gBumholeWindow;
        gBumholeWindow = nullptr;
    }
    gGameWindow->Show(static_cast<int16_t>(gGameWindowToggled));
    gGameWindowToggled = 1 - gGameWindowToggled;
    if (gGameWindowToggled == 0) UIMain_DecideKbCapture();
}

// 0x101a8c1c
int32_t GameWindowToggle_Hidden() { return gGameWindowToggled; }

// UI_SwitchGameWindow 0x101a8cd8 (240 bytes) and UI_SetGameWindowCursor 0x101a8c58 (80 bytes).

int32_t gGameWindowCursor;   // 0x103949b0

// 0x101a8c58: when there is a game window, sets its cursor and remembers it (sign-extended from 16 bits).
void UI_SetGameWindowCursor(int16_t cursor) {
    if (gGameWindow != nullptr) {
        gGameWindow->setCursor(cursor);
        gGameWindowCursor = cursor;
    }
}

void UI_SwitchGameWindow(int32_t first) {
    if (static_cast<int32_t>(gGameWorld->mF1DA738) == 4) return;
    if (gGameWorld->IsPlayingWinningAnim()) return;
    UI_SendMessage(gGameWindow, 6, first, 0);
    UI_SendMessage(gFirstPersonWindow, 6, 1 - first, 0);
    if (TheGameType().mType == kGameTypeOnline) {
        if (first == 0) {
            ChatGoFirstPerson();
        } else {
            ChatBackToThirdPerson();
        }
    }
    UIMain_DecideKbCapture();
}

// 0x10238800: TbDynamicStringTemplate<wchar_t>(const wchar_t*) built in place. The Mac global is 0x14 bytes; the host string is larger,
// so the storage is sized for it (nothing reads the global but this initialiser).
namespace mainpanel2_callee {
__attribute__((weak, noinline)) void WideStringCtor(void* string, const char16_t* text) {
    new (string) TbDynamicStringTemplate<char16_t>(text);
}
}  // namespace mainpanel2_callee
alignas(8) uint8_t gMainPanel2Text[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x103949e8 (0x14 bytes on the Mac)

// 0x101a8df4
const char16_t gMainPanel2Literal[] = u"gms.dat";   // 0x1024e158
void __sinit_ui_mainpanel2_cpp() {
    mainpanel2_callee::WideStringCtor(gMainPanel2Text, gMainPanel2Literal);
    __register_global_object(gMainPanel2Text, reinterpret_cast<void*>(&UIMain_DestroyWString), gMainPanel2TextNode);
}

// The main panel's and the staff panels' window scripts (main); signed 16-bit records, in image order.

// 0x102aa840, 606 words
int16_t gMainPanelScript[606] = {
    0, 1, 1, 0, 29, 0, 37, 984, 439, 1507,
    17, -1, -1,
    18, -1, -1,
    0, 1, 1, 0, 30, 0, 67, 1080, 163, 1345,
    17, 477, 0,
    18, -1, -1,
    0, 9, 3, 0, 31, 0, 85, 1100, 144, 1324,
    17, -1, -1,
    18, -1, -1,
    5,
    1, 22431, 27091,
    5,
    0, 1, 1, 0, 32, 0, 153, 1044, 404, 1121,
    17, 478, 0,
    18, -1, -1,
    3, 182, 1061, 383, 1103,
    1, 28628, 7397,
    5,
    0, 1, 1, 0, 33, 0, 331, 1069, 1006, 1495,
    17, -1, -1,
    18, -1, -1,
    0, 1, 1, 2, 34, 0, 1006, 1069, 1071, 1382,
    17, -1, -1,
    18, -1, -1,
    0, 1, 1, 2, 35, 0, 976, 1058, 1129, 1503,
    17, -1, -1,
    18, -1, -1,
    4, 4, 16, 1060, 1069, 1071, 1058, 1093, 1106, 1113, 1183, 1129, 1310, 1120, 1405, 1090, 1461, 1056, 1486, 1015, 1503, 976, 1503, 976, 1406, 981, 1392, 992, 1382, 1024, 1373, 1045, 1358, 1062, 1336,
    1, -32698, 15039,
    5,
    1, -11575, 14920,
    5,
    0, 2, 1, 0, 36, 0, 349, 1386, 429, 1466,
    17, 481, 0,
    18, -1, -1,
    1, 22160, -31654,
    5,
    1, 14092, -17361,
    5,
    4, 4, 23, 167, 1019, 258, 1014, 327, 1015, 396, 1022, 415, 1032, 428, 1057, 439, 1311, 428, 1343, 413, 1365, 389, 1383, 358, 1391, 344, 1410, 344, 1499, 255, 1507, 150, 1500, 102, 1482, 75, 1461, 48, 1409, 37, 1311, 64, 1071, 72, 1046, 84, 984, 155, 984,
    1, 1685, 7147,
    0, 1, 1, 0, 37, 0, 170, 1122, 392, 1372,
    17, -1, -1,
    18, -1, -1,
    0, 2, 1, 0, 38, 0, 170, 1122, 288, 1240,
    17, 469, 0,
    18, -1, -1,
    1, -7875, 8501,
    5,
    0, 2, 17, 0, 39, 0, 202, 1355, 320, 1474,
    17, 474, 0,
    18, -1, -1,
    1, 7524, 30690,
    5,
    0, 2, 1, 0, 40, 0, 287, 1133, 405, 1252,
    17, 470, 0,
    18, -1, -1,
    1, 29175, 5705,
    5,
    0, 2, 1, 0, 41, 0, 81, 1340, 200, 1458,
    17, 473, 0,
    18, -1, -1,
    1, 23407, 8477,
    5,
    0, 2, 1, 0, 42, 0, 156, 1241, 274, 1360,
    17, 471, 0,
    18, -1, -1,
    1, 7505, -17751,
    5,
    0, 2, 1, 0, 43, 0, 274, 1254, 392, 1372,
    17, 472, 0,
    18, 9, 0,
    1, 18992, -5280,
    5,
    0, 1, 3, 0, 44, 0, 274, 1254, 392, 1372,
    17, 494, 0,
    18, -1, -1,
    5,
    5,
    5,
    0, 1, 1, 0, 45, 0, 94, 918, 144, 984,
    17, -1, -1,
    18, -1, -1,
    0, 1, 1, 4, 46, 0, 77, 790, 161, 918,
    17, 476, 0,
    18, -1, -1,
    1, -13727, -31552,
    5,
    1, -24426, 31563,
    5,
    0, 1, 1, 0, 47, 0, 258, 60, 720, 260,
    17, -1, -1,
    18, -1, -1,
    0, 1, 1, 0, 48, 0, 458, 273, 720, 363,
    17, -1, -1,
    18, -1, -1,
    5,
    0, 6, 1, 0, 49, 0, 728, 109, 831, 211,
    17, -1, -1,
    18, -1, -1,
    1, 31193, -22873,
    5,
    0, 6, 3, 0, 50, 0, 37, 58, 242, 262,
    17, -1, -1,
    18, -1, -1,
    1, 20386, -4181,
    5,
    5,
    0, 1, 1, 0, 51, 0, 1688, 48, 1963, 278,
    17, -1, -1,
    18, -1, -1,
    0, 6, 1, 0, 52, 0, 1853, 165, 1955, 268,
    17, -1, -1,
    18, -1, -1,
    1, -13548, 7504,
    5,
    0, 6, 1, 0, 53, 0, 1853, 54, 1955, 156,
    17, -1, -1,
    18, -1, -1,
    1, -4288, 27073,
    5,
    0, 1, 1, 0, 54, 0, 1688, 171, 1829, 261,
    17, -1, -1,
    18, -1, -1,
    5,
    0, 1, 1, 0, 55, 0, 1688, 60, 1829, 150,
    17, -1, -1,
    18, -1, -1,
    5,
    5,
    5,
};

// ui_mainpanel2.cpp's data (main app, unit w88): the pause window's script (0x102aacfc) and its animation (0x102aadbc). Names invented.

int16_t gPauseDialogDescription[21] = {
    0, 1, 1, 0, 5094, 0, 174, 500, 1874, 900,   // level 0, record 0
    17, -1, -1,   // level 1, record 17
    18, -1, -1,   // level 1, record 18
    1, 17852, -22616,   // level 1, record 1
    5,   // level 1, record 5
    5,   // level 0, record 5
};

// Four frames of deltas (left, top, right, bottom), then the command "go to frame 0".
UI_RectDelta gPauseAnim[5] = {
    {-2, -6, 2, 2},
    {-2, -6, 2, 2},
    {2, 6, -2, -2},
    {2, 6, -2, -2},
    {kAnimGoto, 0, 0, 0},
};
