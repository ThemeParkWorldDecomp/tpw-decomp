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
#include <climits>
#include <cstdint>
#include <new>

#include "FS_Online.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

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
