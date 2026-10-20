// Unit v5995 (main app, ui_mainpanel2.cpp): the main panel's date display, the last-panel memory, the loans panel refresh and the
// slide-out panel's message function.
//   UI_MainPanelDateChange 0x101a6234 (416 bytes), SetLastPanel 0x101a57fc (72), UI_LoansPanelDateChange 0x101a577c (80),
//   RailsCallback 0x101a6f2c (1932).
#include <cstdint>
#include <new>
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "LLCellSystem.h"
#include "UI_Base.h"
#include "World.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "ui_main.h"
#include "FillLenderList.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "LoansPanelMsg.h"
#include "TPSound.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "analyser.h"

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

// 0x101a577c (80 bytes): refills the lender list of the loans panel while it shows the loans taken (mode 1). The month is unused.
void UI_LoansPanelDateChange(int32_t month) {
    (void)month;
    if (gLoansPanel != nullptr && gLenderListMode == 1) {
        InterfaceWindow* list = gLoansPanel->GetWindow(0x12c4bd);
        FillLenderList(static_cast<InterfaceListControl2*>(list));
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
